/*-------------------------------------------------------------------------
 *
 * heap.c
 *	  code to create and destroy POSTGRES heap relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/heap.c
 *
 *
 * INTERFACE ROUTINES
 *		heap_create()			- Create an uncataloged heap
 *relation heap_create_with_catalog() - Create a cataloged relation
 *		heap_drop_with_catalog() - Removes named relation from catalogs
 *
 * NOTES
 *	  this code taken from access/heap/create.c, which contains
 *	  the old heap_create_with_catalog, amcreate, and amdestroy.
 *	  those routines will soon call these routines using the function
 *	  manager,
 *	  just like the poorly named "NewXXX" routines do.  The
 *	  "New" routines are all going to die soon, once and for all!
 *		-cim 1/13/91
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/objectaccess.h"
#include "catalog/partition.h"
#include "catalog/pg_am.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_partitioned_table.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_subscription_rel.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "commands/tablecmds.h"
#include "commands/typecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/* Potentially set by pg_upgrade_support functions */
Oid binary_upgrade_next_heap_pg_class_oid = InvalidOid;
Oid binary_upgrade_next_toast_pg_class_oid = InvalidOid;

static void
AddNewRelationTuple(Relation pg_class_desc, Relation new_rel_desc, Oid new_rel_oid, Oid new_type_oid, Oid reloftype, Oid relowner, char relkind, TransactionId relfrozenxid, TransactionId relminmxid, Datum relacl, Datum reloptions);
static ObjectAddress
AddNewRelationType(const char *typeName, Oid typeNamespace, Oid new_rel_oid, char new_rel_kind, Oid ownerid, Oid new_row_type, Oid new_array_type);
static void
RelationRemoveInheritance(Oid relid);
static Oid
StoreRelCheck(Relation rel, const char *ccname, Node *expr, bool is_validated, bool is_local, int inhcount, bool is_no_inherit, bool is_internal);
static void
StoreConstraints(Relation rel, List *cooked_constraints, bool is_internal);
static bool
MergeWithExistingConstraint(Relation rel, const char *ccname, Node *expr, bool allow_merge, bool is_local, bool is_initially_valid, bool is_no_inherit);
static void
SetRelationNumChecks(Relation rel, int numchecks);
static Node *
cookConstraint(ParseState *pstate, Node *raw_constraint, char *relname);
static List *
insert_ordered_unique_oid(List *list, Oid datum);

/* ----------------------------------------------------------------
 *				XXX UGLY HARD CODED BADNESS FOLLOWS XXX
 *
 *		these should all be moved to someplace in the lib/catalog
 *		module, if not obliterated first.
 * ----------------------------------------------------------------
 */

/*
 * Note:
 *		Should the system special case these attributes in the future?
 *		Advantage:	consume much less space in the ATTRIBUTE
 *relation. Disadvantage:  special cases will be all over the place.
 */

/*
 * The initializers below do not include trailing variable length fields,
 * but that's OK - we're never going to reference anything beyond the
 * fixed-size portion of the structure anyway.
 */

static const FormData_pg_attribute a1 = {
    .attname = {"ctid"},
    .atttypid = TIDOID,
    .attlen = sizeof(ItemPointerData),
    .attnum = SelfItemPointerAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = false,
    .attstorage = 'p',
    .attalign = 's',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a2 = {
    .attname = {"xmin"},
    .atttypid = XIDOID,
    .attlen = sizeof(TransactionId),
    .attnum = MinTransactionIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a3 = {
    .attname = {"cmin"},
    .atttypid = CIDOID,
    .attlen = sizeof(CommandId),
    .attnum = MinCommandIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a4 = {
    .attname = {"xmax"},
    .atttypid = XIDOID,
    .attlen = sizeof(TransactionId),
    .attnum = MaxTransactionIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a5 = {
    .attname = {"cmax"},
    .atttypid = CIDOID,
    .attlen = sizeof(CommandId),
    .attnum = MaxCommandIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

/*
 * We decided to call this attribute "tableoid" rather than say
 * "classoid" on the basis that in the future there may be more than one
 * table of a particular class/type. In any case table is still the word
 * used in SQL.
 */
static const FormData_pg_attribute a6 = {
    .attname = {"tableoid"},
    .atttypid = OIDOID,
    .attlen = sizeof(Oid),
    .attnum = TableOidAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute *SysAtt[] = {&a1, &a2, &a3, &a4, &a5, &a6};

/*
 * This function returns a Form_pg_attribute pointer for a system attribute.
 * Note that we elog if the presented attno is invalid, which would only
 * happen if there's a problem upstream.
 */
const FormData_pg_attribute *
SystemAttributeDefinition(AttrNumber attno)
{





}

/*
 * If the given name is a system attribute name, return a Form_pg_attribute
 * pointer for a prototype definition.  If not, return NULL.
 */
const FormData_pg_attribute *
SystemAttributeByName(const char *attname)
{













}

/* ----------------------------------------------------------------
 *				XXX END OF UGLY HARD CODED BADNESS XXX
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 *		heap_create		- Create an uncataloged heap relation
 *
 *		Note API change: the caller must now always provide the OID
 *		to use for the relation.  The relfilenode may (and, normally,
 *		should) be left unspecified.
 *
 *		rel->rd_rel is initialized by RelationBuildLocalRelation,
 *		and is mostly zeroes at return.
 * ----------------------------------------------------------------
 */
Relation
heap_create(const char *relname, Oid relnamespace, Oid reltablespace, Oid relid, Oid relfilenode, Oid accessmtd, TupleDesc tupDesc, char relkind, char relpersistence, bool shared_relation, bool mapped_relation, bool allow_system_table_mods, TransactionId *relfrozenxid, MultiXactId *relminmxid)
{


































































































































}

/* ----------------------------------------------------------------
 *		heap_create_with_catalog		- Create a cataloged
 *relation
 *
 *		this is done in multiple steps:
 *
 *		1) CheckAttributeNamesTypes() is used to make certain the tuple
 *		   descriptor contains a valid set of attribute names and types
 *
 *		2) pg_class is opened and get_relname_relid()
 *		   performs a scan to ensure that no relation with the
 *		   same name already exists.
 *
 *		3) heap_create() is called to create the new relation on disk.
 *
 *		4) TypeCreate() is called to define a new type corresponding
 *		   to the new relation.
 *
 *		5) AddNewRelationTuple() is called to register the
 *		   relation in pg_class.
 *
 *		6) AddNewAttributeTuples() is called to register the
 *		   new relation's schema in pg_attribute.
 *
 *		7) StoreConstraints is called ()		- vadim 08/22/97
 *
 *		8) the relations are closed and the new relation's oid
 *		   is returned.
 *
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		CheckAttributeNamesTypes
 *
 *		this is used to make certain the tuple descriptor contains a
 *		valid set of attribute names and datatypes.  a problem simply
 *		generates ereport(ERROR) which aborts the current transaction.
 *
 *		relkind is the relkind of the relation to be created.
 *		flags controls which datatypes are allowed, cf
 *CheckAttributeType.
 * --------------------------------
 */
void
CheckAttributeNamesTypes(TupleDesc tupdesc, char relkind, int flags)
{



















































}

/* --------------------------------
 *		CheckAttributeType
 *
 *		Verify that the proposed datatype of an attribute is legal.
 *		This is needed mainly because there are types (and pseudo-types)
 *		in the catalogs that we do not support as elements of real
 *tuples. We also check some other properties required of a table column.
 *
 * If the attribute is being proposed for addition to an existing table or
 * composite type, pass a one-element list of the rowtype OID as
 * containing_rowtypes.  When checking a to-be-created rowtype, it's
 * sufficient to pass NIL, because there could not be any recursive reference
 * to a not-yet-existing rowtype.
 *
 * flags is a bitmask controlling which datatypes we allow.  For the most
 * part, pseudo-types are disallowed as attribute types, but there are some
 * exceptions: ANYARRAYOID, RECORDOID, and RECORDARRAYOID can be allowed
 * in some cases.  (This works because values of those type classes are
 * self-identifying to some extent.  However, RECORDOID and RECORDARRAYOID
 * are reliably identifiable only within a session, since the identity info
 * may use a typmod that is only locally assigned.  The caller is expected
 * to know whether these cases are safe.)
 *
 * flags can also control the phrasing of the error messages.  If
 * CHKATYPE_IS_PARTKEY is specified, "attname" should be a partition key
 * column number as text, not a real column name.
 * --------------------------------
 */
void
CheckAttributeType(const char *attname, Oid atttypid, Oid attcollation, List *containing_rowtypes, int flags)
{









































































































}

/*
 * InsertPgAttributeTuple
 *		Construct and insert a new tuple in pg_attribute.
 *
 * Caller has already opened and locked pg_attribute.  new_attribute is the
 * attribute to insert.  attcacheoff is always initialized to -1, attacl and
 * attoptions are always initialized to NULL.
 *
 * indstate is the index state for CatalogTupleInsertWithInfo.  It can be
 * passed as NULL, in which case we'll fetch the necessary info.  (Don't do
 * this when inserting multiple attributes, because it's a tad more
 * expensive.)
 */
void
InsertPgAttributeTuple(Relation pg_attribute_rel, Form_pg_attribute new_attribute, CatalogIndexState indstate)
{

















































}

/* --------------------------------
 *		AddNewAttributeTuples
 *
 *		this registers the new relation's schema by adding
 *		tuples to pg_attribute.
 * --------------------------------
 */
static void
AddNewAttributeTuples(Oid new_rel_oid, TupleDesc tupdesc, char relkind)
{









































































}

/* --------------------------------
 *		InsertPgClassTuple
 *
 *		Construct and insert a new tuple in pg_class.
 *
 * Caller has already opened and locked pg_class.
 * Tuple data is taken from new_rel_desc->rd_rel, except for the
 * variable-width fields which are not present in a cached reldesc.
 * relacl and reloptions are passed in Datum form (to avoid having
 * to reference the data types in heap.h).  Pass (Datum) 0 to set them
 * to NULL.
 * --------------------------------
 */
void
InsertPgClassTuple(Relation pg_class_desc, Relation new_rel_desc, Oid new_rel_oid, Datum relacl, Datum reloptions)
{

































































}

/* --------------------------------
 *		AddNewRelationTuple
 *
 *		this registers the new relation in the catalogs by
 *		adding a tuple to pg_class.
 * --------------------------------
 */
static void
AddNewRelationTuple(Relation pg_class_desc, Relation new_rel_desc, Oid new_rel_oid, Oid new_type_oid, Oid reloftype, Oid relowner, char relkind, TransactionId relfrozenxid, TransactionId relminmxid, Datum relacl, Datum reloptions)
{














































}

/* --------------------------------
 *		AddNewRelationType -
 *
 *		define a composite type corresponding to the new relation
 * --------------------------------
 */
static ObjectAddress
AddNewRelationType(const char *typeName, Oid typeNamespace, Oid new_rel_oid, char new_rel_kind, Oid ownerid, Oid new_row_type, Oid new_array_type)
{































}

/* --------------------------------
 *		heap_create_with_catalog
 *
 *		creates a new cataloged relation.  see comments above.
 *
 * Arguments:
 *	relname: name to give to new rel
 *	relnamespace: OID of namespace it goes in
 *	reltablespace: OID of tablespace it goes in
 *	relid: OID to assign to new rel, or InvalidOid to select a new OID
 *	reltypeid: OID to assign to rel's rowtype, or InvalidOid to select one
 *	reloftypeid: if a typed table, OID of underlying type; else InvalidOid
 *	ownerid: OID of new rel's owner
 *	tupdesc: tuple descriptor (source of column definitions)
 *	cooked_constraints: list of precooked check constraints and defaults
 *	relkind: relkind for new rel
 *	relpersistence: rel's persistence status (permanent, temp, or unlogged)
 *	shared_relation: true if it's to be a shared relation
 *	mapped_relation: true if the relation will use the relfilenode map
 *	oncommit: ON COMMIT marking (only relevant if it's a temp table)
 *	reloptions: reloptions in Datum form, or (Datum) 0 if none
 *	use_user_acl: true if should look for user-defined default permissions;
 *		if false, relacl is always set NULL
 *	allow_system_table_mods: true to allow creation in system namespaces
 *	is_internal: is this a system-generated catalog?
 *
 * Output parameters:
 *	typaddress: if not null, gets the object address of the new pg_type
 *entry
 *
 * Returns the OID of the new relation
 * --------------------------------
 */
Oid
heap_create_with_catalog(const char *relname, Oid relnamespace, Oid reltablespace, Oid relid, Oid reltypeid, Oid reloftypeid, Oid ownerid, Oid accessmtd, TupleDesc tupdesc, List *cooked_constraints, char relkind, char relpersistence, bool shared_relation, bool mapped_relation, OnCommitAction oncommit, Datum reloptions, bool use_user_acl, bool allow_system_table_mods, bool is_internal, Oid relrewrite, ObjectAddress *typaddress)
{











































































































































































































































































































}

/*
 *		RelationRemoveInheritance
 *
 * Formerly, this routine checked for child relations and aborted the
 * deletion if any were found.  Now we rely on the dependency mechanism
 * to check for or delete child relations.  By the time we get here,
 * there are no children and we need only remove any pg_inherits rows
 * linking this relation to its parent(s).
 */
static void
RelationRemoveInheritance(Oid relid)
{


















}

/*
 *		DeleteRelationTuple
 *
 * Remove pg_class row for the given relid.
 *
 * Note: this is shared by relation deletion and index deletion.  It's
 * not intended for use anyplace else.
 */
void
DeleteRelationTuple(Oid relid)
{


















}

/*
 *		DeleteAttributeTuples
 *
 * Remove pg_attribute rows for the given relid.
 *
 * Note: this is shared by relation deletion and index deletion.  It's
 * not intended for use anyplace else.
 */
void
DeleteAttributeTuples(Oid relid)
{






















}

/*
 *		DeleteSystemAttributeTuples
 *
 * Remove pg_attribute rows for system columns of the given relid.
 *
 * Note: this is only used when converting a table to a view.  Views don't
 * have system columns, so we should remove them from pg_attribute.
 */
void
DeleteSystemAttributeTuples(Oid relid)
{























}

/*
 *		RemoveAttributeById
 *
 * This is the guts of ALTER TABLE DROP COLUMN: actually mark the attribute
 * deleted in pg_attribute.  We also remove pg_statistic entries for it.
 * (Everything else needed, such as getting rid of any pg_attrdef entry,
 * is handled by dependency.c.)
 */
void
RemoveAttributeById(Oid relid, AttrNumber attnum)
{




































































































}

/*
 *		RemoveAttrDefault
 *
 * If the specified relation/attribute has a default, remove it.
 * (If no default, raise error if complain is true, else return quietly.)
 */
void
RemoveAttrDefault(Oid relid, AttrNumber attnum, DropBehavior behavior, bool complain, bool internal)
{



































}

/*
 *		RemoveAttrDefaultById
 *
 * Remove a pg_attrdef entry specified by OID.  This is the guts of
 * attribute-default removal.  Note it should be called via performDeletion,
 * not directly.
 */
void
RemoveAttrDefaultById(Oid attrdefId)
{
























































}

/*
 * heap_drop_with_catalog	- removes specified relation from catalogs
 *
 * Note that this routine is not responsible for dropping objects that are
 * linked to the pg_class entry via dependencies (for example, indexes and
 * constraints).  Those are deleted by the dependency-tracing logic in
 * dependency.c before control gets here.  In general, therefore, this routine
 * should never be called directly; go through performDeletion() instead.
 */
void
heap_drop_with_catalog(Oid relid)
{









































































































































































}

/*
 * RelationClearMissing
 *
 * Set atthasmissing and attmissingval to false/null for all attributes
 * where they are currently set. This can be safely and usefully done if
 * the table is rewritten (e.g. by VACUUM FULL or CLUSTER) where we know there
 * are no rows left with less than a full complement of attributes.
 *
 * The caller must have an AccessExclusive lock on the relation.
 */
void
RelationClearMissing(Relation rel)
{




















































}

/*
 * SetAttrMissing
 *
 * Set the missing value of a single attribute. This should only be used by
 * binary upgrade. Takes an AccessExclusive lock on the relation owning the
 * attribute.
 */
void
SetAttrMissing(Oid relid, char *attname, char *value)
{















































}

/*
 * Store a default expression for column attnum of relation rel.
 *
 * Returns the OID of the new pg_attrdef tuple.
 *
 * add_column_mode must be true if we are storing the default for a new
 * attribute, and false if it's for an already existing attribute. The reason
 * for this is that the missing value must never be updated after it is set,
 * which can only be when a column is added to the table. Otherwise we would
 * in effect be changing existing tuples.
 */
Oid
StoreAttrDefault(Relation rel, AttrNumber attnum, Node *expr, bool is_internal, bool add_column_mode)
{





























































































































































}

/*
 * Store a check-constraint expression for the given relation.
 *
 * Caller is responsible for updating the count of constraints
 * in the pg_class entry for the relation.
 *
 * The OID of the new constraint is returned.
 */
static Oid
StoreRelCheck(Relation rel, const char *ccname, Node *expr, bool is_validated, bool is_local, int inhcount, bool is_no_inherit, bool is_internal)
{























































































}

/*
 * Store defaults and constraints (passed as a list of CookedConstraint).
 *
 * Each CookedConstraint struct is modified to store the new catalog tuple OID.
 *
 * NOTE: only pre-cooked expressions will be passed this way, which is to
 * say expressions inherited from an existing relation.  Newly parsed
 * expressions can be added later, by direct calls to StoreAttrDefault
 * and StoreRelCheck (see AddRelationNewConstraints()).
 */
static void
StoreConstraints(Relation rel, List *cooked_constraints, bool is_internal)
{





































}

/*
 * AddRelationNewConstraints
 *
 * Add new column default expressions and/or constraint check expressions
 * to an existing relation.  This is defined to do both for efficiency in
 * DefineRelation, but of course you can do just one or the other by passing
 * empty lists.
 *
 * rel: relation to be modified
 * newColDefaults: list of RawColumnDefault structures
 * newConstraints: list of Constraint nodes
 * allow_merge: true if check constraints may be merged with existing ones
 * is_local: true if definition is local, false if it's inherited
 * is_internal: true if result of some internal process, not a user request
 *
 * All entries in newColDefaults will be processed.  Entries in newConstraints
 * will be processed only if they are CONSTR_CHECK type.
 *
 * Returns a list of CookedConstraint nodes that shows the cooked form of
 * the default and constraint expressions added to the relation.
 *
 * NB: caller should have opened rel with AccessExclusiveLock, and should
 * hold that lock till end of transaction.  Also, we assume the caller has
 * done a CommandCounterIncrement if necessary to make the relation's catalog
 * tuples visible.
 */
List *
AddRelationNewConstraints(Relation rel, List *newColDefaults, List *newConstraints, bool allow_merge, bool is_local, bool is_internal, const char *queryString)
{




























































































































































































































}

/*
 * Check for a pre-existing check constraint that conflicts with a proposed
 * new one, and either adjust its conislocal/coninhcount settings or throw
 * error as needed.
 *
 * Returns true if merged (constraint is a duplicate), or false if it's
 * got a so-far-unique name, or throws error if conflict.
 *
 * XXX See MergeConstraintsIntoExisting too if you change this code.
 */
static bool
MergeWithExistingConstraint(Relation rel, const char *ccname, Node *expr, bool allow_merge, bool is_local, bool is_initially_valid, bool is_no_inherit)
{



























































































































}

/*
 * Update the count of constraints in the relation's pg_class tuple.
 *
 * Caller had better hold exclusive lock on the relation.
 *
 * An important side effect is that a SI update message will be sent out for
 * the pg_class tuple, which will force other backends to rebuild their
 * relcache entries for the rel.  Also, this backend will rebuild its
 * own relcache entry at the next CommandCounterIncrement.
 */
static void
SetRelationNumChecks(Relation rel, int numchecks)
{


























}

/*
 * Check for references to generated columns
 */
static bool
check_nested_generated_walker(Node *node, void *context)
{





































}

static void
check_nested_generated(ParseState *pstate, Node *node)
{

}

/*
 * Take a raw default and convert it to a cooked format ready for
 * storage.
 *
 * Parse state should be set up to recognize any vars that might appear
 * in the expression.  (Even though we plan to reject vars, it's more
 * user-friendly to give the correct error message than "unknown var".)
 *
 * If atttypid is not InvalidOid, coerce the expression to the specified
 * type (and typmod atttypmod).   attname is only needed in this case:
 * it is used in the error message, if any.
 */
Node *
cookDefault(ParseState *pstate, Node *raw_default, Oid atttypid, int32 atttypmod, const char *attname, char attgenerated)
{

















































}

/*
 * Take a raw CHECK constraint expression and convert it to a cooked format
 * ready for storage.
 *
 * Parse state must be set up to recognize any vars that might appear
 * in the expression.
 */
static Node *
cookConstraint(ParseState *pstate, Node *raw_constraint, char *relname)
{



























}

/*
 * CopyStatistics --- copy entries in pg_statistic from one rel to another
 */
void
CopyStatistics(Oid fromrelid, Oid torelid)
{






























}

/*
 * RemoveStatistics --- remove entries in pg_statistic for a rel or column
 *
 * If attnum is zero, remove all entries for rel; else remove only the one(s)
 * for that column.
 */
void
RemoveStatistics(Oid relid, AttrNumber attnum)
{































}

/*
 * RelationTruncateIndexes - truncate all indexes associated
 * with the heap relation to zero tuples.
 *
 * The routine will truncate and then reconstruct the indexes on
 * the specified relation.  Caller must hold exclusive lock on rel.
 */
static void
RelationTruncateIndexes(Relation heapRelation)
{


































}

/*
 *	 heap_truncate
 *
 *	 This routine deletes all data within all the specified relations.
 *
 * This is not transaction-safe!  There is another, transaction-safe
 * implementation in commands/tablecmds.c.  We now use this only for
 * ON COMMIT truncation of temporary tables, where it doesn't matter.
 */
void
heap_truncate(List *relids)
{



























}

/*
 *	 heap_truncate_one_rel
 *
 *	 This routine deletes all data within the specified relation.
 *
 * This is not transaction-safe, because the truncation is done immediately
 * and cannot be rolled back later.  Caller is responsible for having
 * checked permissions etc, and must have obtained AccessExclusiveLock.
 */
void
heap_truncate_one_rel(Relation rel)
{




























}

/*
 * heap_truncate_check_FKs
 *		Check for foreign keys referencing a list of relations that
 *		are to be truncated, and raise error if there are any
 *
 * We disallow such FKs (except self-referential ones) since the whole point
 * of TRUNCATE is to not scan the individual rows to be thrown away.
 *
 * This is split out so it can be shared by both implementations of truncate.
 * Caller should already hold a suitable lock on the relations.
 *
 * tempTables is only used to select an appropriate error message.
 */
void
heap_truncate_check_FKs(List *relations, bool tempTables)
{










































































}

/*
 * heap_truncate_find_FKs
 *		Find relations having foreign keys referencing any of the given
 *rels
 *
 * Input and result are both lists of relation OIDs.  The result contains
 * no duplicates, does *not* include any rels that were already in the input
 * list, and is sorted in OID order.  (The last property is enforced mainly
 * to guarantee consistent behavior in the regression tests; we don't want
 * behavior to change depending on chance locations of rows in pg_constraint.)
 *
 * Note: caller should already have appropriate lock on all rels mentioned
 * in relationIds.  Since adding or dropping an FK requires exclusive lock
 * on both rels, this ensures that the answer will be stable.
 */
List *
heap_truncate_find_FKs(List *relationIds)
{
























































































































}

/*
 * insert_ordered_unique_oid
 *		Insert a new Oid into a sorted list of Oids, preserving
 *ordering, and eliminating duplicates
 *
 * Building the ordered list this way is O(N^2), but with a pretty small
 * constant, so for the number of entries we expect it will probably be
 * faster than trying to apply qsort().  It seems unlikely someone would be
 * trying to truncate a table with thousands of dependent tables ...
 */
static List *
insert_ordered_unique_oid(List *list, Oid datum)
{

































}

/*
 * StorePartitionKey
 *		Store information about the partition key rel into the catalog
 */
void
StorePartitionKey(Relation rel, char strategy, int16 partnatts, AttrNumber *partattrs, List *partexprs, Oid *partopclass, Oid *partcollation)
{























































































































}

/*
 *	RemovePartitionKeyByRelId
 *		Remove pg_partitioned_table entry for a relation
 */
void
RemovePartitionKeyByRelId(Oid relid)
{















}

/*
 * StorePartitionBound
 *		Update pg_class tuple of rel to store the partition bound and
 *set relispartition to true
 *
 * If this is the default partition, also update the default partition OID in
 * pg_partitioned_table.
 *
 * Also, invalidate the parent's relcache, so that the next rebuild will load
 * the new partition's info into its partition descriptor.  If there is a
 * default partition, we must invalidate its relcache entry as well.
 */
void
StorePartitionBound(Relation rel, Relation parent, PartitionBoundSpec *bound)
{

































































}