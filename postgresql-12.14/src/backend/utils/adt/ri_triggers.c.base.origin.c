/*-------------------------------------------------------------------------
 *
 * ri_triggers.c
 *
 *	Generic trigger procedures for referential integrity constraint
 *	checks.
 *
 *	Note about memory management: the private hashtables kept here live
 *	across query and transaction boundaries, in fact they live as long as
 *	the backend does.  This works because the hashtable structures
 *	themselves are allocated by dynahash.c in its permanent DynaHashCxt,
 *	and the SPI plans they point to are saved using SPI_keepplan().
 *	There is not currently any provision for throwing away a
 *no-longer-needed plan --- consider improving this someday.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * src/backend/utils/adt/ri_triggers.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "lib/ilist.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/*
 * Local definitions
 */

#define RI_MAX_NUMKEYS INDEX_MAX_KEYS

#define RI_INIT_CONSTRAINTHASHSIZE 64
#define RI_INIT_QUERYHASHSIZE (RI_INIT_CONSTRAINTHASHSIZE * 4)

#define RI_KEYS_ALL_NULL 0
#define RI_KEYS_SOME_NULL 1
#define RI_KEYS_NONE_NULL 2

/* RI query type codes */
/* these queries are executed against the PK (referenced) table: */
#define RI_PLAN_CHECK_LOOKUPPK 1
#define RI_PLAN_CHECK_LOOKUPPK_FROM_PK 2
#define RI_PLAN_LAST_ON_PK RI_PLAN_CHECK_LOOKUPPK_FROM_PK
/* these queries are executed against the FK (referencing) table: */
#define RI_PLAN_CASCADE_DEL_DODELETE 3
#define RI_PLAN_CASCADE_UPD_DOUPDATE 4
#define RI_PLAN_RESTRICT_CHECKREF 5
#define RI_PLAN_SETNULL_DOUPDATE 6
#define RI_PLAN_SETDEFAULT_DOUPDATE 7

#define MAX_QUOTED_NAME_LEN (NAMEDATALEN * 2 + 3)
#define MAX_QUOTED_REL_NAME_LEN (MAX_QUOTED_NAME_LEN * 2)

#define RIAttName(rel, attnum) NameStr(*attnumAttName(rel, attnum))
#define RIAttType(rel, attnum) attnumTypeId(rel, attnum)
#define RIAttCollation(rel, attnum) attnumCollationId(rel, attnum)

#define RI_TRIGTYPE_INSERT 1
#define RI_TRIGTYPE_UPDATE 2
#define RI_TRIGTYPE_DELETE 3

/*
 * RI_ConstraintInfo
 *
 * Information extracted from an FK pg_constraint entry.  This is cached in
 * ri_constraint_cache.
 */
typedef struct RI_ConstraintInfo
{
  Oid constraint_id;                /* OID of pg_constraint entry (hash key) */
  bool valid;                       /* successfully initialized? */
  uint32 oidHashValue;              /* hash value of pg_constraint OID */
  NameData conname;                 /* name of the FK constraint */
  Oid pk_relid;                     /* referenced relation */
  Oid fk_relid;                     /* referencing relation */
  char confupdtype;                 /* foreign key's ON UPDATE action */
  char confdeltype;                 /* foreign key's ON DELETE action */
  char confmatchtype;               /* foreign key's match type */
  int nkeys;                        /* number of key columns */
  int16 pk_attnums[RI_MAX_NUMKEYS]; /* attnums of referenced cols */
  int16 fk_attnums[RI_MAX_NUMKEYS]; /* attnums of referencing cols */
  Oid pf_eq_oprs[RI_MAX_NUMKEYS];   /* equality operators (PK = FK) */
  Oid pp_eq_oprs[RI_MAX_NUMKEYS];   /* equality operators (PK = PK) */
  Oid ff_eq_oprs[RI_MAX_NUMKEYS];   /* equality operators (FK = FK) */
  dlist_node valid_link;            /* Link in list of valid entries */
} RI_ConstraintInfo;

/*
 * RI_QueryKey
 *
 * The key identifying a prepared SPI plan in our query hashtable
 */
typedef struct RI_QueryKey
{
  Oid constr_id;        /* OID of pg_constraint entry */
  int32 constr_queryno; /* query type ID, see RI_PLAN_XXX above */
} RI_QueryKey;

/*
 * RI_QueryHashEntry
 */
typedef struct RI_QueryHashEntry
{
  RI_QueryKey key;
  SPIPlanPtr plan;
} RI_QueryHashEntry;

/*
 * RI_CompareKey
 *
 * The key identifying an entry showing how to compare two values
 */
typedef struct RI_CompareKey
{
  Oid eq_opr; /* the equality operator to apply */
  Oid typeid; /* the data type to apply it to */
} RI_CompareKey;

/*
 * RI_CompareHashEntry
 */
typedef struct RI_CompareHashEntry
{
  RI_CompareKey key;
  bool valid;               /* successfully initialized? */
  FmgrInfo eq_opr_finfo;    /* call info for equality fn */
  FmgrInfo cast_func_finfo; /* in case we must coerce input */
} RI_CompareHashEntry;

/*
 * Local data
 */
static HTAB *ri_constraint_cache = NULL;
static HTAB *ri_query_cache = NULL;
static HTAB *ri_compare_cache = NULL;
static dlist_head ri_constraint_cache_valid_list;
static int ri_constraint_cache_valid_count = 0;

/*
 * Local function prototypes
 */
static bool
ri_Check_Pk_Match(Relation pk_rel, Relation fk_rel, TupleTableSlot *oldslot, const RI_ConstraintInfo *riinfo);
static Datum
ri_restrict(TriggerData *trigdata, bool is_no_action);
static Datum
ri_set(TriggerData *trigdata, bool is_set_null);
static void
quoteOneName(char *buffer, const char *name);
static void
quoteRelationName(char *buffer, Relation rel);
static void
ri_GenerateQual(StringInfo buf, const char *sep, const char *leftop, Oid leftoptype, Oid opoid, const char *rightop, Oid rightoptype);
static void
ri_GenerateQualCollation(StringInfo buf, Oid collation);
static int
ri_NullCheck(TupleDesc tupdesc, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk);
static void
ri_BuildQueryKey(RI_QueryKey *key, const RI_ConstraintInfo *riinfo, int32 constr_queryno);
static bool
ri_KeysEqual(Relation rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, const RI_ConstraintInfo *riinfo, bool rel_is_pk);
static bool
ri_AttributesEqual(Oid eq_opr, Oid typeid, Datum oldvalue, Datum newvalue);

static void
ri_InitHashTables(void);
static void
InvalidateConstraintCacheCallBack(Datum arg, int cacheid, uint32 hashvalue);
static SPIPlanPtr
ri_FetchPreparedPlan(RI_QueryKey *key);
static void
ri_HashPreparedPlan(RI_QueryKey *key, SPIPlanPtr plan);
static RI_CompareHashEntry *
ri_HashCompareOp(Oid eq_opr, Oid typeid);

static void
ri_CheckTrigger(FunctionCallInfo fcinfo, const char *funcname, int tgkind);
static const RI_ConstraintInfo *
ri_FetchConstraintInfo(Trigger *trigger, Relation trig_rel, bool rel_is_pk);
static const RI_ConstraintInfo *
ri_LoadConstraintInfo(Oid constraintOid);
static SPIPlanPtr
ri_PlanCheck(const char *querystr, int nargs, Oid *argtypes, RI_QueryKey *qkey, Relation fk_rel, Relation pk_rel, bool cache_plan);
static bool
ri_PerformCheck(const RI_ConstraintInfo *riinfo, RI_QueryKey *qkey, SPIPlanPtr qplan, Relation fk_rel, Relation pk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, bool detectNewRows, int expect_OK);
static void
ri_ExtractValues(Relation rel, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk, Datum *vals, char *nulls);
static void
ri_ReportViolation(const RI_ConstraintInfo *riinfo, Relation pk_rel, Relation fk_rel, TupleTableSlot *violatorslot, TupleDesc tupdesc, int queryno, bool partgone) pg_attribute_noreturn();

/*
 * RI_FKey_check -
 *
 * Check foreign key existence (combined for INSERT and UPDATE).
 */
static Datum
RI_FKey_check(TriggerData *trigdata)
{

































































































































































}

/*
 * RI_FKey_check_ins -
 *
 * Check foreign key existence at insert event on FK table.
 */
Datum
RI_FKey_check_ins(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_check_upd -
 *
 * Check foreign key existence at update event on FK table.
 */
Datum
RI_FKey_check_upd(PG_FUNCTION_ARGS)
{





}

/*
 * ri_Check_Pk_Match
 *
 * Check to see if another PK row has been created that provides the same
 * key values as the "oldslot" that's been modified or deleted in our trigger
 * event.  Returns true if a match is found in the PK table.
 *
 * We assume the caller checked that the oldslot contains no NULL key values,
 * since otherwise a match is impossible.
 */
static bool
ri_Check_Pk_Match(Relation pk_rel, Relation fk_rel, TupleTableSlot *oldslot, const RI_ConstraintInfo *riinfo)
{





































































}

/*
 * RI_FKey_noaction_del -
 *
 * Give an error and roll back the current transaction if the
 * delete has resulted in a violation of the given referential
 * integrity constraint.
 */
Datum
RI_FKey_noaction_del(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_restrict_del -
 *
 * Restrict delete from PK table to rows unreferenced by foreign key.
 *
 * The SQL standard intends that this referential action occur exactly when
 * the delete is performed, rather than after.  This appears to be
 * the only difference between "NO ACTION" and "RESTRICT".  In Postgres
 * we still implement this as an AFTER trigger, but it's non-deferrable.
 */
Datum
RI_FKey_restrict_del(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_noaction_upd -
 *
 * Give an error and roll back the current transaction if the
 * update has resulted in a violation of the given referential
 * integrity constraint.
 */
Datum
RI_FKey_noaction_upd(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_restrict_upd -
 *
 * Restrict update of PK to rows unreferenced by foreign key.
 *
 * The SQL standard intends that this referential action occur exactly when
 * the update is performed, rather than after.  This appears to be
 * the only difference between "NO ACTION" and "RESTRICT".  In Postgres
 * we still implement this as an AFTER trigger, but it's non-deferrable.
 */
Datum
RI_FKey_restrict_upd(PG_FUNCTION_ARGS)
{





}

/*
 * ri_restrict -
 *
 * Common code for ON DELETE RESTRICT, ON DELETE NO ACTION,
 * ON UPDATE RESTRICT, and ON UPDATE NO ACTION.
 */
static Datum
ri_restrict(TriggerData *trigdata, bool is_no_action)
{






































































































}

/*
 * RI_FKey_cascade_del -
 *
 * Cascaded delete foreign key references at delete event on PK table.
 */
Datum
RI_FKey_cascade_del(PG_FUNCTION_ARGS)
{


























































































}

/*
 * RI_FKey_cascade_upd -
 *
 * Cascaded update foreign key references at update event on PK table.
 */
Datum
RI_FKey_cascade_upd(PG_FUNCTION_ARGS)
{







































































































}

/*
 * RI_FKey_setnull_del -
 *
 * Set foreign key references to NULL values at delete event on PK table.
 */
Datum
RI_FKey_setnull_del(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_setnull_upd -
 *
 * Set foreign key references to NULL at update event on PK table.
 */
Datum
RI_FKey_setnull_upd(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_setdefault_del -
 *
 * Set foreign key references to defaults at delete event on PK table.
 */
Datum
RI_FKey_setdefault_del(PG_FUNCTION_ARGS)
{





}

/*
 * RI_FKey_setdefault_upd -
 *
 * Set foreign key references to defaults at update event on PK table.
 */
Datum
RI_FKey_setdefault_upd(PG_FUNCTION_ARGS)
{





}

/*
 * ri_set -
 *
 * Common code for ON DELETE SET NULL, ON DELETE SET DEFAULT, ON UPDATE SET
 * NULL, and ON UPDATE SET DEFAULT.
 */
static Datum
ri_set(TriggerData *trigdata, bool is_set_null)
{




















































































































}

/*
 * RI_FKey_pk_upd_check_required -
 *
 * Check if we really need to fire the RI trigger for an update or delete to a
 * PK relation.  This is called by the AFTER trigger queue manager to see if it
 * can skip queuing an instance of an RI trigger.  Returns true if the trigger
 * must be fired, false if we can prove the constraint will still be satisfied.
 *
 * newslot will be NULL if this is called for a delete.
 */
bool
RI_FKey_pk_upd_check_required(Trigger *trigger, Relation pk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot)
{





















}

/*
 * RI_FKey_fk_upd_check_required -
 *
 * Check if we really need to fire the RI trigger for an update to an FK
 * relation.  This is called by the AFTER trigger queue manager to see if
 * it can skip queuing an instance of an RI trigger.  Returns true if the
 * trigger must be fired, false if we can prove the constraint will still
 * be satisfied.
 */
bool
RI_FKey_fk_upd_check_required(Trigger *trigger, Relation fk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot)
{




















































































}

/*
 * RI_Initial_Check -
 *
 * Check an entire table for non-matching values using a single query.
 * This is not a trigger procedure, but is called during ALTER TABLE
 * ADD FOREIGN KEY to validate the initial table contents.
 *
 * We expect that the caller has made provision to prevent any problems
 * caused by concurrent actions. This could be either by locking rel and
 * pkrel at ShareRowExclusiveLock or higher, or by otherwise ensuring
 * that triggers implementing the checks are already active.
 * Hence, we do not need to lock individual rows for the check.
 *
 * If the check fails because the current user doesn't have permissions
 * to read both tables, return false to let our caller know that they will
 * need to do something else to check the constraint.
 */
bool
RI_Initial_Check(Trigger *trigger, Relation fk_rel, Relation pk_rel)
{























































































































































































































































}

/*
 * RI_PartitionRemove_Check -
 *
 * Verify no referencing values exist, when a partition is detached on
 * the referenced side of a foreign key constraint.
 */
void
RI_PartitionRemove_Check(Trigger *trigger, Relation fk_rel, Relation pk_rel)
{
































































































































































































}

/* ----------
 * Local functions below
 * ----------
 */

/*
 * quoteOneName --- safely quote a single SQL name
 *
 * buffer must be MAX_QUOTED_NAME_LEN long (includes room for \0)
 */
static void
quoteOneName(char *buffer, const char *name)
{












}

/*
 * quoteRelationName --- safely quote a fully qualified relation name
 *
 * buffer must be MAX_QUOTED_REL_NAME_LEN long (includes room for \0)
 */
static void
quoteRelationName(char *buffer, Relation rel)
{




}

/*
 * ri_GenerateQual --- generate a WHERE clause equating two variables
 *
 * This basically appends " sep leftop op rightop" to buf, adding casts
 * and schema qualification as needed to ensure that the parser will select
 * the operator we specify.  leftop and rightop should be parenthesized
 * if they aren't variables or parameters.
 */
static void
ri_GenerateQual(StringInfo buf, const char *sep, const char *leftop, Oid leftoptype, Oid opoid, const char *rightop, Oid rightoptype)
{


}

/*
 * ri_GenerateQualCollation --- add a COLLATE spec to a WHERE clause
 *
 * At present, we intentionally do not use this function for RI queries that
 * compare a variable to a $n parameter.  Since parameter symbols always have
 * default collation, the effect will be to use the variable's collation.
 * Now that is only strictly correct when testing the referenced column, since
 * the SQL standard specifies that RI comparisons should use the referenced
 * column's collation.  However, so long as all collations have the same
 * notion of equality (which they do, because texteq reduces to bitwise
 * equality), there's no visible semantic impact from using the referencing
 * column's collation when testing it, and this is a good thing to do because
 * it lets us use a normal index on the referencing column.  However, we do
 * have to use this function when directly comparing the referencing and
 * referenced columns, if they are of different collations; else the parser
 * will fail to resolve the collation to use.
 */
static void
ri_GenerateQualCollation(StringInfo buf, Oid collation)
{





























}

/* ----------
 * ri_BuildQueryKey -
 *
 *	Construct a hashtable key for a prepared SPI plan of an FK constraint.
 *
 *		key: output argument, *key is filled in based on the other
 *arguments riinfo: info from pg_constraint entry constr_queryno: an internal
 *number identifying the query type (see RI_PLAN_XXX constants at head of file)
 * ----------
 */
static void
ri_BuildQueryKey(RI_QueryKey *key, const RI_ConstraintInfo *riinfo, int32 constr_queryno)
{






}

/*
 * Check that RI trigger function was called in expected context
 */
static void
ri_CheckTrigger(FunctionCallInfo fcinfo, const char *funcname, int tgkind)
{




































}

/*
 * Fetch the RI_ConstraintInfo struct for the trigger's FK constraint.
 */
static const RI_ConstraintInfo *
ri_FetchConstraintInfo(Trigger *trigger, Relation trig_rel, bool rel_is_pk)
{











































}

/*
 * Fetch or create the RI_ConstraintInfo struct for an FK constraint.
 */
static const RI_ConstraintInfo *
ri_LoadConstraintInfo(Oid constraintOid)
{

































































}

/*
 * Callback for pg_constraint inval events
 *
 * While most syscache callbacks just flush all their entries, pg_constraint
 * gets enough update traffic that it's probably worth being smarter.
 * Invalidate any ri_constraint_cache entry associated with the syscache
 * entry with the specified hash value, or all entries if hashvalue == 0.
 *
 * Note: at the time a cache invalidation message is processed there may be
 * active references to the cache.  Because of this we never remove entries
 * from the cache, but only mark them invalid, which is harmless to active
 * uses.  (Any query using an entry should hold a lock sufficient to keep that
 * data from changing under it --- but we may get cache flushes anyway.)
 */
static void
InvalidateConstraintCacheCallBack(Datum arg, int cacheid, uint32 hashvalue)
{



























}

/*
 * Prepare execution plan for a query to enforce an RI restriction
 *
 * If cache_plan is true, the plan is saved into our plan hashtable
 * so that we don't need to plan it again.
 */
static SPIPlanPtr
ri_PlanCheck(const char *querystr, int nargs, Oid *argtypes, RI_QueryKey *qkey, Relation fk_rel, Relation pk_rel, bool cache_plan)
{









































}

/*
 * Perform a query to enforce an RI restriction
 */
static bool
ri_PerformCheck(const RI_ConstraintInfo *riinfo, RI_QueryKey *qkey, SPIPlanPtr qplan, Relation fk_rel, Relation pk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, bool detectNewRows, int expect_OK)
{




















































































































}

/*
 * Extract fields from a tuple into Datum/nulls arrays
 */
static void
ri_ExtractValues(Relation rel, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk, Datum *vals, char *nulls)
{

















}

/*
 * Produce an error report
 *
 * If the failed constraint was on insert/update to the FK table,
 * we want the key names and values extracted from there, and the error
 * message to look like 'key blah is not present in PK'.
 * Otherwise, the attr names and values come from the PK table and the
 * message looks like 'key blah is still referenced from FK'.
 */
static void
ri_ReportViolation(const RI_ConstraintInfo *riinfo, Relation pk_rel, Relation fk_rel, TupleTableSlot *violatorslot, TupleDesc tupdesc, int queryno, bool partgone)
{




























































































































}

/*
 * ri_NullCheck -
 *
 * Determine the NULL state of all key values in a tuple
 *
 * Returns one of RI_KEYS_ALL_NULL, RI_KEYS_NONE_NULL or RI_KEYS_SOME_NULL.
 */
static int
ri_NullCheck(TupleDesc tupDesc, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk)
{




































}

/*
 * ri_InitHashTables -
 *
 * Initialize our internal hash tables.
 */
static void
ri_InitHashTables(void)
{



















}

/*
 * ri_FetchPreparedPlan -
 *
 * Lookup for a query key in our private hash table of prepared
 * and saved SPI execution plans. Return the plan if found or NULL.
 */
static SPIPlanPtr
ri_FetchPreparedPlan(RI_QueryKey *key)
{















































}

/*
 * ri_HashPreparedPlan -
 *
 * Add another plan to our private SPI query plan hashtable.
 */
static void
ri_HashPreparedPlan(RI_QueryKey *key, SPIPlanPtr plan)
{


















}

/*
 * ri_KeysEqual -
 *
 * Check if all key values in OLD and NEW are equal.
 *
 * Note: at some point we might wish to redefine this as checking for
 * "IS NOT DISTINCT" rather than "=", that is, allow two nulls to be
 * considered equal.  Currently there is no need since all callers have
 * previously found at least one of the rows to contain no nulls.
 */
static bool
ri_KeysEqual(Relation rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, const RI_ConstraintInfo *riinfo, bool rel_is_pk)
{




































































}

/*
 * ri_AttributesEqual -
 *
 * Call the appropriate equality comparison operator for two values.
 *
 * NB: we have already checked that neither value is null.
 */
static bool
ri_AttributesEqual(Oid eq_opr, Oid typeid, Datum oldvalue, Datum newvalue)
{
























}

/*
 * ri_HashCompareOp -
 *
 * See if we know how to compare two values, and create a new hash entry
 * if not.
 */
static RI_CompareHashEntry *
ri_HashCompareOp(Oid eq_opr, Oid typeid)
{




















































































}

/*
 * Given a trigger function OID, determine whether it is an RI trigger,
 * and if so whether it is attached to PK or FK relation.
 */
int
RI_FKey_trigger_type(Oid tgfoid)
{




















}