/*-------------------------------------------------------------------------
 *
 * dependency.c
 *	  Routines to support inter-object dependencies.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/catalog/dependency.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_init_privs.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_publication_rel.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/policy.h"
#include "commands/proclang.h"
#include "commands/publicationcmds.h"
#include "commands/schemacmds.h"
#include "commands/seclabel.h"
#include "commands/sequence.h"
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteRemove.h"
#include "storage/lmgr.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/*
 * Deletion processing requires additional state for each ObjectAddress that
 * it's planning to delete.  For simplicity and code-sharing we make the
 * ObjectAddresses code support arrays with or without this extra state.
 */
typedef struct
{
  int flags;              /* bitmask, see bit definitions below */
  ObjectAddress dependee; /* object whose deletion forced this one */
} ObjectAddressExtra;

/* ObjectAddressExtra flag bits */
#define DEPFLAG_ORIGINAL 0x0001  /* an original deletion target */
#define DEPFLAG_NORMAL 0x0002    /* reached via normal dependency */
#define DEPFLAG_AUTO 0x0004      /* reached via auto dependency */
#define DEPFLAG_INTERNAL 0x0008  /* reached via internal dependency */
#define DEPFLAG_PARTITION 0x0010 /* reached via partition dependency */
#define DEPFLAG_EXTENSION 0x0020 /* reached via extension dependency */
#define DEPFLAG_REVERSE 0x0040   /* reverse internal/extension link */
#define DEPFLAG_IS_PART 0x0080   /* has a partition dependency */
#define DEPFLAG_SUBOBJECT 0x0100 /* subobject of another deletable object */

/* expansible list of ObjectAddresses */
struct ObjectAddresses
{
  ObjectAddress *refs;        /* => palloc'd array */
  ObjectAddressExtra *extras; /* => palloc'd array, or NULL if not used */
  int numrefs;                /* current number of references */
  int maxrefs;                /* current size of palloc'd array(s) */
};

/* typedef ObjectAddresses appears in dependency.h */

/* threaded list of ObjectAddresses, for recursion detection */
typedef struct ObjectAddressStack
{
  const ObjectAddress *object;     /* object being visited */
  int flags;                       /* its current flag bits */
  struct ObjectAddressStack *next; /* next outer stack level */
} ObjectAddressStack;

/* temporary storage in findDependentObjects */
typedef struct
{
  ObjectAddress obj; /* object to be deleted --- MUST BE FIRST */
  int subflags;      /* flags to pass down when recursing to obj */
} ObjectAddressAndFlags;

/* for find_expr_references_walker */
typedef struct
{
  ObjectAddresses *addrs; /* addresses being accumulated */
  List *rtables;          /* list of rangetables to resolve Vars */
} find_expr_references_context;

/*
 * This constant table maps ObjectClasses to the corresponding catalog OIDs.
 * See also getObjectClass().
 */
static const Oid object_classes[] = {
    RelationRelationId,              /* OCLASS_CLASS */
    ProcedureRelationId,             /* OCLASS_PROC */
    TypeRelationId,                  /* OCLASS_TYPE */
    CastRelationId,                  /* OCLASS_CAST */
    CollationRelationId,             /* OCLASS_COLLATION */
    ConstraintRelationId,            /* OCLASS_CONSTRAINT */
    ConversionRelationId,            /* OCLASS_CONVERSION */
    AttrDefaultRelationId,           /* OCLASS_DEFAULT */
    LanguageRelationId,              /* OCLASS_LANGUAGE */
    LargeObjectRelationId,           /* OCLASS_LARGEOBJECT */
    OperatorRelationId,              /* OCLASS_OPERATOR */
    OperatorClassRelationId,         /* OCLASS_OPCLASS */
    OperatorFamilyRelationId,        /* OCLASS_OPFAMILY */
    AccessMethodRelationId,          /* OCLASS_AM */
    AccessMethodOperatorRelationId,  /* OCLASS_AMOP */
    AccessMethodProcedureRelationId, /* OCLASS_AMPROC */
    RewriteRelationId,               /* OCLASS_REWRITE */
    TriggerRelationId,               /* OCLASS_TRIGGER */
    NamespaceRelationId,             /* OCLASS_SCHEMA */
    StatisticExtRelationId,          /* OCLASS_STATISTIC_EXT */
    TSParserRelationId,              /* OCLASS_TSPARSER */
    TSDictionaryRelationId,          /* OCLASS_TSDICT */
    TSTemplateRelationId,            /* OCLASS_TSTEMPLATE */
    TSConfigRelationId,              /* OCLASS_TSCONFIG */
    AuthIdRelationId,                /* OCLASS_ROLE */
    DatabaseRelationId,              /* OCLASS_DATABASE */
    TableSpaceRelationId,            /* OCLASS_TBLSPACE */
    ForeignDataWrapperRelationId,    /* OCLASS_FDW */
    ForeignServerRelationId,         /* OCLASS_FOREIGN_SERVER */
    UserMappingRelationId,           /* OCLASS_USER_MAPPING */
    DefaultAclRelationId,            /* OCLASS_DEFACL */
    ExtensionRelationId,             /* OCLASS_EXTENSION */
    EventTriggerRelationId,          /* OCLASS_EVENT_TRIGGER */
    PolicyRelationId,                /* OCLASS_POLICY */
    PublicationRelationId,           /* OCLASS_PUBLICATION */
    PublicationRelRelationId,        /* OCLASS_PUBLICATION_REL */
    SubscriptionRelationId,          /* OCLASS_SUBSCRIPTION */
    TransformRelationId              /* OCLASS_TRANSFORM */
};

static void
findDependentObjects(const ObjectAddress *object, int objflags, int flags, ObjectAddressStack *stack, ObjectAddresses *targetObjects, const ObjectAddresses *pendingObjects, Relation *depRel);
static void
reportDependentObjects(const ObjectAddresses *targetObjects, DropBehavior behavior, int flags, const ObjectAddress *origObject);
static void
deleteOneObject(const ObjectAddress *object, Relation *depRel, int32 flags);
static void
doDeletion(const ObjectAddress *object, int flags);
static bool
find_expr_references_walker(Node *node, find_expr_references_context *context);
static void
eliminate_duplicate_dependencies(ObjectAddresses *addrs);
static int
object_address_comparator(const void *a, const void *b);
static void
add_object_address(ObjectClass oclass, Oid objectId, int32 subId, ObjectAddresses *addrs);
static void
add_exact_object_address_extra(const ObjectAddress *object, const ObjectAddressExtra *extra, ObjectAddresses *addrs);
static bool
object_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddresses *addrs);
static bool
stack_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddressStack *stack);
static void
DeleteInitPrivs(const ObjectAddress *object);

/*
 * Go through the objects given running the final actions on them, and execute
 * the actual deletion.
 */
static void
deleteObjectsInList(ObjectAddresses *targetObjects, Relation *depRel, int flags)
{


















































}

/*
 * performDeletion: attempt to drop the specified object.  If CASCADE
 * behavior is specified, also drop any dependent objects (recursively).
 * If RESTRICT behavior is specified, error out if there are any dependent
 * objects, except for those that should be implicitly dropped anyway
 * according to the dependency type.
 *
 * This is the outer control routine for all forms of DROP that drop objects
 * that can participate in dependencies.  Note that performMultipleDeletions
 * is a variant on the same theme; if you change anything here you'll likely
 * need to fix that too.
 *
 * Bits in the flags argument can include:
 *
 * PERFORM_DELETION_INTERNAL: indicates that the drop operation is not the
 * direct result of a user-initiated action.  For example, when a temporary
 * schema is cleaned out so that a new backend can use it, or when a column
 * default is dropped as an intermediate step while adding a new one, that's
 * an internal operation.  On the other hand, when we drop something because
 * the user issued a DROP statement against it, that's not internal. Currently
 * this suppresses calling event triggers and making some permissions checks.
 *
 * PERFORM_DELETION_CONCURRENTLY: perform the drop concurrently.  This does
 * not currently work for anything except dropping indexes; don't set it for
 * other object types or you may get strange results.
 *
 * PERFORM_DELETION_QUIETLY: reduce message level from NOTICE to DEBUG2.
 *
 * PERFORM_DELETION_SKIP_ORIGINAL: do not delete the specified object(s),
 * but only what depends on it/them.
 *
 * PERFORM_DELETION_SKIP_EXTENSIONS: do not delete extensions, even when
 * deleting objects that are part of an extension.  This should generally
 * be used only when dropping temporary objects.
 *
 * PERFORM_DELETION_CONCURRENT_LOCK: perform the drop normally but with a lock
 * as if it were concurrent.  This is used by REINDEX CONCURRENTLY.
 *
 */
void
performDeletion(const ObjectAddress *object, DropBehavior behavior, int flags)
{





































}

/*
 * performMultipleDeletions: Similar to performDeletion, but act on multiple
 * objects at once.
 *
 * The main difference from issuing multiple performDeletion calls is that the
 * list of objects that would be implicitly dropped, for each object to be
 * dropped, is the union of the implicit-object list for all objects.  This
 * makes each check be more relaxed.
 */
void
performMultipleDeletions(const ObjectAddresses *objects, DropBehavior behavior, int flags)
{























































}

/*
 * findDependentObjects - find all objects that depend on 'object'
 *
 * For every object that depends on the starting object, acquire a deletion
 * lock on the object, add it to targetObjects (if not already there),
 * and recursively find objects that depend on it.  An object's dependencies
 * will be placed into targetObjects before the object itself; this means
 * that the finished list's order represents a safe deletion order.
 *
 * The caller must already have a deletion lock on 'object' itself,
 * but must not have added it to targetObjects.  (Note: there are corner
 * cases where we won't add the object either, and will also release the
 * caller-taken lock.  This is a bit ugly, but the API is set up this way
 * to allow easy rechecking of an object's liveness after we lock it.  See
 * notes within the function.)
 *
 * When dropping a whole object (subId = 0), we find dependencies for
 * its sub-objects too.
 *
 *	object: the object to add to targetObjects and find dependencies on
 *	objflags: flags to be ORed into the object's targetObjects entry
 *	flags: PERFORM_DELETION_xxx flags for the deletion operation as a whole
 *	stack: list of objects being visited in current recursion; topmost item
 *			is the object that we recursed from (NULL for external
 *callers) targetObjects: list of objects that are scheduled to be deleted
 *	pendingObjects: list of other objects slated for destruction, but
 *			not necessarily in targetObjects yet (can be NULL if
 *none) *depRel: already opened pg_depend relation
 *
 * Note: objflags describes the reason for visiting this particular object
 * at this time, and is not passed down when recursing.  The flags argument
 * is passed down, since it describes what we're doing overall.
 */
static void
findDependentObjects(const ObjectAddress *object, int objflags, int flags, ObjectAddressStack *stack, ObjectAddresses *targetObjects, const ObjectAddresses *pendingObjects, Relation *depRel)
{

























































































































































































































































































































































































































































































































}

/*
 * reportDependentObjects - report about dependencies, and fail if RESTRICT
 *
 * Tell the user about dependent objects that we are going to delete
 * (or would need to delete, but are prevented by RESTRICT mode);
 * then error out if there are any and it's not CASCADE mode.
 *
 *	targetObjects: list of objects that are scheduled to be deleted
 *	behavior: RESTRICT or CASCADE
 *	flags: other flags for the deletion operation
 *	origObject: base object of deletion, or NULL if not available
 *		(the latter case occurs in DROP OWNED)
 */
static void
reportDependentObjects(const ObjectAddresses *targetObjects, DropBehavior behavior, int flags, const ObjectAddress *origObject)
{






























































































































































































}

/*
 * deleteOneObject: delete a single object for performDeletion.
 *
 * *depRel is the already-open pg_depend relation.
 */
static void
deleteOneObject(const ObjectAddress *object, Relation *depRel, int flags)
{

























































































}

/*
 * doDeletion: actually delete a single object
 */
static void
doDeletion(const ObjectAddress *object, int flags)
{





























































































































































































}

/*
 * AcquireDeletionLock - acquire a suitable lock for deleting an object
 *
 * Accepts the same flags as performDeletion (though currently only
 * PERFORM_DELETION_CONCURRENTLY does anything).
 *
 * We use LockRelation for relations, LockDatabaseObject for everything
 * else.  Shared-across-databases objects are not currently supported
 * because no caller cares, but could be modified to use LockSharedObject.
 */
void
AcquireDeletionLock(const ObjectAddress *object, int flags)
{






















}

/*
 * ReleaseDeletionLock - release an object deletion lock
 *
 * Companion to AcquireDeletionLock.
 */
void
ReleaseDeletionLock(const ObjectAddress *object)
{









}

/*
 * recordDependencyOnExpr - find expression dependencies
 *
 * This is used to find the dependencies of rules, constraint expressions,
 * etc.
 *
 * Given an expression or query in node-tree form, find all the objects
 * it refers to (tables, columns, operators, functions, etc).  Record
 * a dependency of the specified type from the given depender object
 * to each object mentioned in the expression.
 *
 * rtable is the rangetable to be used to interpret Vars with varlevelsup=0.
 * It can be NIL if no such variables are expected.
 */
void
recordDependencyOnExpr(const ObjectAddress *depender, Node *expr, List *rtable, DependencyType behavior)
{

















}

/*
 * recordDependencyOnSingleRelExpr - find expression dependencies
 *
 * As above, but only one relation is expected to be referenced (with
 * varno = 1 and varlevelsup = 0).  Pass the relation OID instead of a
 * range table.  An additional frammish is that dependencies on that
 * relation's component columns will be marked with 'self_behavior',
 * whereas 'behavior' is used for everything else; also, if 'reverse_self'
 * is true, those dependencies are reversed so that the columns are made
 * to depend on the table not vice versa.
 *
 * NOTE: the caller should ensure that a whole-table dependency on the
 * specified relation is created separately, if one is needed.  In particular,
 * a whole-row Var "relation.*" will not cause this routine to emit any
 * dependency item.  This is appropriate behavior for subexpressions of an
 * ordinary query, so other cases need to cope as necessary.
 */
void
recordDependencyOnSingleRelExpr(const ObjectAddress *depender, Node *expr, Oid relId, DependencyType behavior, DependencyType self_behavior, bool reverse_self)
{












































































}

/*
 * Recursively search an expression tree for object references.
 *
 * Note: we avoid creating references to columns of tables that participate
 * in an SQL JOIN construct, but are not actually used anywhere in the query.
 * To do so, we do not scan the joinaliasvars list of a join RTE while
 * scanning the query rangetable, but instead scan each individual entry
 * of the alias list when we find a reference to it.
 *
 * Note: in many cases we do not need to create dependencies on the datatypes
 * involved in an expression, because we'll have an indirect dependency via
 * some other object.  For instance Var nodes depend on a column which depends
 * on the datatype, and OpExpr nodes depend on the operator which depends on
 * the datatype.  However we do need a type dependency if there is no such
 * indirect dependency, as for example in Const and CoerceToDomain nodes.
 *
 * Similarly, we don't need to create dependencies on collations except where
 * the collation is being freshly introduced to the expression.
 */
static bool
find_expr_references_walker(Node *node, find_expr_references_context *context)
{





















































































































































































































































































































































































































































































































































}

/*
 * Given an array of dependency references, eliminate any duplicates.
 */
static void
eliminate_duplicate_dependencies(ObjectAddresses *addrs)
{





















































}

/*
 * qsort comparator for ObjectAddress items
 */
static int
object_address_comparator(const void *a, const void *b)
{














































}

/*
 * Routines for handling an expansible array of ObjectAddress items.
 *
 * new_object_addresses: create a new ObjectAddresses array.
 */
ObjectAddresses *
new_object_addresses(void)
{










}

/*
 * Add an entry to an ObjectAddresses array.
 *
 * It is convenient to specify the class by ObjectClass rather than directly
 * by catalog OID.
 */
static void
add_object_address(ObjectClass oclass, Oid objectId, int32 subId, ObjectAddresses *addrs)
{




















}

/*
 * Add an entry to an ObjectAddresses array.
 *
 * As above, but specify entry exactly.
 */
void
add_exact_object_address(const ObjectAddress *object, ObjectAddresses *addrs)
{













}

/*
 * Add an entry to an ObjectAddresses array.
 *
 * As above, but specify entry exactly and provide some "extra" data too.
 */
static void
add_exact_object_address_extra(const ObjectAddress *object, const ObjectAddressExtra *extra, ObjectAddresses *addrs)
{






















}

/*
 * Test whether an object is present in an ObjectAddresses array.
 *
 * We return "true" if object is a subobject of something in the array, too.
 */
bool
object_address_present(const ObjectAddress *object, const ObjectAddresses *addrs)
{
















}

/*
 * As above, except that if the object is present then also OR the given
 * flags into its associated extra data (which must exist).
 */
static bool
object_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddresses *addrs)
{
































































}

/*
 * Similar to above, except we search an ObjectAddressStack.
 */
static bool
stack_address_present_add_flags(const ObjectAddress *object, int flags, ObjectAddressStack *stack)
{








































}

/*
 * Record multiple dependencies from an ObjectAddresses array, after first
 * removing any duplicates.
 */
void
record_object_address_dependencies(const ObjectAddress *depender, ObjectAddresses *referenced, DependencyType behavior)
{


}

/*
 * Sort the items in an ObjectAddresses array.
 *
 * The major sort key is OID-descending, so that newer objects will be listed
 * first in most cases.  This is primarily useful for ensuring stable outputs
 * from regression tests; it's not recommended if the order of the objects is
 * determined by user input, such as the order of targets in a DROP command.
 */
void
sort_object_addresses(ObjectAddresses *addrs)
{




}

/*
 * Clean up when done with an ObjectAddresses array.
 */
void
free_object_addresses(ObjectAddresses *addrs)
{






}

/*
 * Determine the class of a given object identified by objectAddress.
 *
 * This function is essentially the reverse mapping for the object_classes[]
 * table.  We implement it as a function because the OIDs aren't consecutive.
 */
ObjectClass
getObjectClass(const ObjectAddress *object)
{































































































































}

/*
 * delete initial ACL for extension objects
 */
static void
DeleteInitPrivs(const ObjectAddress *object)
{





















}