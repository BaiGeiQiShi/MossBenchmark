/*-------------------------------------------------------------------------
 *
 * indexcmds.c
 *	  POSTGRES define and remove index code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/indexcmds.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/progress.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "partitioning/partdesc.h"
#include "pgstat.h"
#include "rewrite/rewriteManip.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/pg_rusage.h"
#include "utils/regproc.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/* non-export function prototypes */
static void
CheckPredicate(Expr *predicate);
static void
ComputeIndexAttrs(IndexInfo *indexInfo, Oid *typeOidP, Oid *collationOidP, Oid *classOidP, int16 *colOptionP, List *attList, List *exclusionOpNames, Oid relId, const char *accessMethodName, Oid accessMethodId, bool amcanorder, bool isconstraint, Oid ddl_userid, int ddl_sec_context, int *ddl_save_nestlevel);
static char *
ChooseIndexName(const char *tabname, Oid namespaceId, List *colnames, List *exclusionOpNames, bool primary, bool isconstraint);
static char *
ChooseIndexNameAddition(List *colnames);
static List *
ChooseIndexColumnNames(List *indexElems);
static void
RangeVarCallbackForReindexIndex(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg);
static bool
ReindexRelationConcurrently(Oid relationOid, int options);
static void
ReindexPartitionedIndex(Relation parentIdx);
static void
update_relispartition(Oid relationId, bool newval);

/*
 * callback argument type for RangeVarCallbackForReindexIndex()
 */
struct ReindexIndexCallbackState
{
  bool concurrent;      /* flag from statement */
  Oid locked_table_oid; /* tracks previously locked table */
};

/*
 * CheckIndexCompatible
 *		Determine whether an existing index definition is compatible
 *with a prospective index definition, such that the existing index storage
 *		could become the storage of the new index, avoiding a rebuild.
 *
 * 'heapRelation': the relation the index would apply to.
 * 'accessMethodName': name of the AM to use.
 * 'attributeList': a list of IndexElem specifying columns and expressions
 *		to index on.
 * 'exclusionOpNames': list of names of exclusion-constraint operators,
 *		or NIL if not an exclusion constraint.
 *
 * This is tailored to the needs of ALTER TABLE ALTER TYPE, which recreates
 * any indexes that depended on a changing column from their pg_get_indexdef
 * or pg_get_constraintdef definitions.  We omit some of the sanity checks of
 * DefineIndex.  We assume that the old and new indexes have the same number
 * of columns and that if one has an expression column or predicate, both do.
 * Errors arising from the attribute list still apply.
 *
 * Most column type changes that can skip a table rewrite do not invalidate
 * indexes.  We acknowledge this when all operator classes, collations and
 * exclusion operators match.  Though we could further permit intra-opfamily
 * changes for btree and hash indexes, that adds subtle complexity with no
 * concrete benefit for core types. Note, that INCLUDE columns aren't
 * checked by this function, for them it's enough that table rewrite is
 * skipped.
 *
 * When a comparison or exclusion operator has a polymorphic input type, the
 * actual input types must also match.  This defends against the possibility
 * that operators could vary behavior in response to get_fn_expr_argtype().
 * At present, this hazard is theoretical: check_exclusion_constraint() and
 * all core index access methods decline to set fn_expr for such calls.
 *
 * We do not yet implement a test to verify compatibility of expression
 * columns or predicates, so assume any such index is incompatible.
 */
bool
CheckIndexCompatible(Oid oldId, const char *accessMethodName, List *attributeList, List *exclusionOpNames)
{














































































































































}

/*
 * WaitForOlderSnapshots
 *
 * Wait for transactions that might have an older snapshot than the given xmin
 * limit, because it might not contain tuples deleted just before it has
 * been taken. Obtain a list of VXIDs of such transactions, and wait for them
 * individually. This is used when building an index concurrently.
 *
 * We can exclude any running transactions that have xmin > the xmin given;
 * their oldest snapshot must be newer than our xmin limit.
 * We can also exclude any transactions that have xmin = zero, since they
 * evidently have no live snapshot at all (and any one they might be in
 * process of taking is certainly newer than ours).  Transactions in other
 * DBs can be ignored too, since they'll never even be able to see the
 * index being worked on.
 *
 * We can also exclude autovacuum processes and processes running manual
 * lazy VACUUMs, because they won't be fazed by missing index entries
 * either.  (Manual ANALYZEs, however, can't be excluded because they
 * might be within transactions that are going to do arbitrary operations
 * later.)
 *
 * Also, GetCurrentVirtualXIDs never reports our own vxid, so we need not
 * check for that.
 *
 * If a process goes idle-in-transaction with xmin zero, we do not need to
 * wait for it anymore, per the above argument.  We do not have the
 * infrastructure right now to stop waiting if that happens, but we can at
 * least avoid the folly of waiting when it is idle at the time we would
 * begin to wait.  We do this by repeatedly rechecking the output of
 * GetCurrentVirtualXIDs.  If, during any iteration, a particular vxid
 * doesn't show up in the output, we know we can forget about it.
 */
static void
WaitForOlderSnapshots(TransactionId limitXmin, bool progress)
{



































































}

/*
 * DefineIndex
 *		Creates a new index.
 *
 * This function manages the current userid according to the needs of pg_dump.
 * Recreating old-database catalog entries in new-database is fine, regardless
 * of which users would have permission to recreate those entries now.  That's
 * just preservation of state.  Running opaque expressions, like calling a
 * function named in a catalog entry or evaluating a pg_node_tree in a catalog
 * entry, as anyone other than the object owner, is not fine.  To adhere to
 * those principles and to remain fail-safe, use the table owner userid for
 * most ACL checks.  Use the original userid for ACL checks reached without
 * traversing opaque expressions.  (pg_dump can predict such ACL checks from
 * catalogs.)  Overall, this is a mess.  Future DDL development should
 * consider offering one DDL command for catalog setup and a separate DDL
 * command for steps that run opaque expressions.
 *
 * 'relationId': the OID of the heap relation on which the index is to be
 *		created
 * 'stmt': IndexStmt describing the properties of the new index.
 * 'indexRelationId': normally InvalidOid, but during bootstrap can be
 *		nonzero to specify a preselected OID for the index.
 * 'parentIndexId': the OID of the parent index; InvalidOid if not the child
 *		of a partitioned index.
 * 'parentConstraintId': the OID of the parent constraint; InvalidOid if not
 *		the child of a constraint (only used when recursing)
 * 'is_alter_table': this is due to an ALTER rather than a CREATE operation.
 * 'check_rights': check for CREATE rights in namespace and tablespace.  (This
 *		should be true except when ALTER is deleting/recreating an
 *index.) 'check_not_in_use': check for table not already in use in current
 *session. This should be true unless caller is holding the table open, in which
 *		case the caller had better have checked it earlier.
 * 'skip_build': make the catalog entries but don't create the index files
 * 'quiet': suppress the NOTICE chatter ordinarily provided for constraints.
 *
 * Returns the object address of the created index.
 */
ObjectAddress
DefineIndex(Oid relationId, IndexStmt *stmt, Oid indexRelationId, Oid parentIndexId, Oid parentConstraintId, bool is_alter_table, bool check_rights, bool check_not_in_use, bool skip_build, bool quiet)
{











































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































}

/*
 * CheckMutability
 *		Test whether given expression is mutable
 */
static bool
CheckMutability(Expr *expr)
{

















}

/*
 * CheckPredicate
 *		Checks that the given partial-index predicate is valid.
 *
 * This used to also constrain the form of the predicate to forms that
 * indxpath.c could do something with.  However, that seems overly
 * restrictive.  One useful application of partial indexes is to apply
 * a UNIQUE constraint across a subset of a table, and in that scenario
 * any evaluable predicate will work.  So accept any predicate here
 * (except ones requiring a plan), and let indxpath.c fend for itself.
 */
static void
CheckPredicate(Expr *predicate)
{













}

/*
 * Compute per-index-column information, including indexed column numbers
 * or index expressions, opclasses, and indoptions. Note, all output vectors
 * should be allocated for all columns, including "including" ones.
 *
 * If the caller switched to the table owner, ddl_userid is the role for ACL
 * checks reached without traversing opaque expressions.  Otherwise, it's
 * InvalidOid, and other ddl_* arguments are undefined.
 */
static void
ComputeIndexAttrs(IndexInfo *indexInfo, Oid *typeOidP, Oid *collationOidP, Oid *classOidP, int16 *colOptionP, List *attList, /* list of IndexElem's */
    List *exclusionOpNames, Oid relId, const char *accessMethodName, Oid accessMethodId, bool amcanorder, bool isconstraint, Oid ddl_userid, int ddl_sec_context, int *ddl_save_nestlevel)
{





































































































































































































































































































































}

/*
 * Resolve possibly-defaulted operator class specification
 *
 * Note: This is used to resolve operator class specification in index and
 * partition key definitions.
 */
Oid
ResolveOpClass(List *opclass, Oid attrType, const char *accessMethodName, Oid accessMethodId)
{

























































































}

/*
 * GetDefaultOpClass
 *
 * Given the OIDs of a datatype and an access method, find the default
 * operator class, if any.  Returns InvalidOid if there is none.
 */
Oid
GetDefaultOpClass(Oid type_id, Oid am_id)
{














































































}

/*
 *	makeObjectName()
 *
 *	Create a name for an implicitly created index, sequence, constraint,
 *	extended statistics, etc.
 *
 *	The parameters are typically: the original table name, the original
 *field name, and a "type" string (such as "seq" or "pkey").    The field name
 *	and/or type can be NULL if not relevant.
 *
 *	The result is a palloc'd string.
 *
 *	The basic result we want is "name1_name2_label", omitting "_name2" or
 *	"_label" when those parameters are NULL.  However, we must generate
 *	a name with less than NAMEDATALEN characters!  So, we truncate one or
 *	both names if necessary to make a short-enough string.  The label part
 *	is never truncated (so it had better be reasonably short).
 *
 *	The caller is responsible for checking uniqueness of the generated
 *	name and retrying as needed; retrying will be done by altering the
 *	"label" string (which is why we never truncate that part).
 */
char *
makeObjectName(const char *name1, const char *name2, const char *label)
{





































































}

/*
 * Select a nonconflicting name for a new relation.  This is ordinarily
 * used to choose index names (which is why it's here) but it can also
 * be used for sequences, or any autogenerated relation kind.
 *
 * name1, name2, and label are used the same way as for makeObjectName(),
 * except that the label can't be NULL; digits will be appended to the label
 * if needed to create a name that is unique within the specified namespace.
 *
 * If isconstraint is true, we also avoid choosing a name matching any
 * existing constraint in the same namespace.  (This is stricter than what
 * Postgres itself requires, but the SQL standard says that constraint names
 * should be unique within schemas, so we follow that for autogenerated
 * constraint names.)
 *
 * Note: it is theoretically possible to get a collision anyway, if someone
 * else chooses the same name concurrently.  This is fairly unlikely to be
 * a problem in practice, especially if one is holding an exclusive lock on
 * the relation identified by name1.  However, if choosing multiple names
 * within a single command, you'd better create the new object and do
 * CommandCounterIncrement before choosing the next one!
 *
 * Returns a palloc'd string.
 */
char *
ChooseRelationName(const char *name1, const char *name2, const char *label, Oid namespaceid, bool isconstraint)
{

























}

/*
 * Select the name to be used for an index.
 *
 * The argument list is pretty ad-hoc :-(
 */
static char *
ChooseIndexName(const char *tabname, Oid namespaceId, List *colnames, List *exclusionOpNames, bool primary, bool isconstraint)
{





















}

/*
 * Generate "name2" for a new index given the list of column names for it
 * (as produced by ChooseIndexColumnNames).  This will be passed to
 * ChooseRelationName along with the parent table name and a suitable label.
 *
 * We know that less than NAMEDATALEN characters will actually be used,
 * so we can truncate the result once we've generated that many.
 *
 * XXX See also ChooseForeignKeyConstraintNameAddition and
 * ChooseExtendedStatisticNameAddition.
 */
static char *
ChooseIndexNameAddition(List *colnames)
{


























}

/*
 * Select the actual names to be used for the columns of an index, given the
 * list of IndexElems for the columns.  This is mostly about ensuring the
 * names are unique so we don't get a conflicting-attribute-names error.
 *
 * Returns a List of plain strings (char *, not String nodes).
 */
static List *
ChooseIndexColumnNames(List *indexElems)
{


























































}

/*
 * ReindexIndex
 *		Recreate a specific index.
 */
void
ReindexIndex(RangeVar *indexRelation, int options, bool concurrent)
{










































}

/*
 * Check permissions on table before acquiring relation lock; also lock
 * the heap before the RangeVarGetRelidExtended takes the index lock, to avoid
 * deadlocks.
 */
static void
RangeVarCallbackForReindexIndex(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg)
{
































































}

/*
 * ReindexTable
 *		Recreate all indexes of a table (and of its toast table, if any)
 */
Oid
ReindexTable(RangeVar *relation, int options, bool concurrent)
{
































}

/*
 * ReindexMultipleTables
 *		Recreate indexes of tables selected by objectName/objectKind.
 *
 * To reduce the probability of deadlocks, each table is reindexed in a
 * separate transaction, so we can release the lock on it right away.
 * That means this must not be called within a user transaction block!
 */
void
ReindexMultipleTables(const char *objectName, ReindexObjectType objectKind, int options, bool concurrent)
{






































































































































































































}

/*
 * ReindexRelationConcurrently - process REINDEX CONCURRENTLY for given
 * relation OID
 *
 * 'relationOid' can either belong to an index, a table or a materialized
 * view.  For tables and materialized views, all its indexes will be rebuilt,
 * excluding invalid indexes and any indexes used in exclusion constraints,
 * but including its associated toast table indexes.  For indexes, the index
 * itself will be rebuilt.  If 'relationOid' belongs to a partitioned table
 * then we issue a warning to mention these are not yet supported.
 *
 * The locks taken on parent tables and involved indexes are kept until the
 * transaction is committed, at which point a session lock is taken on each
 * relation.  Both of these protect against concurrent schema changes.
 *
 * Returns true if any indexes have been rebuilt (including toast table's
 * indexes, when relevant), otherwise returns false.
 *
 * NOTE: This cannot be used on temporary relations.  A concurrent build would
 * cause issues with ON COMMIT actions triggered by the transactions of the
 * concurrent build.  Temporary relations are not subject to concurrent
 * concerns, so there's no need for the more complicated concurrent build,
 * anyway, and a non-concurrent reindex is more efficient.
 */
static bool
ReindexRelationConcurrently(Oid relationOid, int options)
{




















































































































































































































































































































































































































































































































































































































































































}

/*
 *	ReindexPartitionedIndex
 *		Reindex each child of the given partitioned index.
 *
 * Not yet implemented.
 */
static void
ReindexPartitionedIndex(Relation parentIdx)
{

}

/*
 * Insert or delete an appropriate pg_inherits tuple to make the given index
 * be a partition of the indicated parent index.
 *
 * This also corrects the pg_depend information for the affected index.
 */
void
IndexSetParentIndex(Relation partitionIdx, Oid parentOid)
{










































































































}

/*
 * Subroutine of IndexSetParentIndex to update the relispartition flag of the
 * given index to the given value.
 */
static void
update_relispartition(Oid relationId, bool newval)
{














}