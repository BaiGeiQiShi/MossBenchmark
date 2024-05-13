/*-------------------------------------------------------------------------
 *
 * index.c
 *	  code to create and destroy POSTGRES index relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/index.c
 *
 *
 * INTERFACE ROUTINES
 *		index_create()			- Create a cataloged index
 *relation index_drop()			- Removes index relation from catalogs
 *		BuildIndexInfo()		- Prepare to insert index tuples
 *		FormIndexDatum()		- Construct datum vector for one
 *index tuple
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/visibilitymap.h"
#include "access/xact.h"
#include "bootstrap/bootstrap.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/objectaccess.h"
#include "catalog/partition.h"
#include "catalog/pg_am.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_description.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "commands/event_trigger.h"
#include "commands/progress.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "pgstat.h"
#include "rewrite/rewriteManip.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/snapmgr.h"

/* Potentially set by pg_upgrade_support functions */
Oid binary_upgrade_next_index_pg_class_oid = InvalidOid;

/*
 * Pointer-free representation of variables used when reindexing system
 * catalogs; we use this to propagate those values to parallel workers.
 */
typedef struct
{
  Oid currentlyReindexedHeap;
  Oid currentlyReindexedIndex;
  int numPendingReindexedIndexes;
  Oid pendingReindexedIndexes[FLEXIBLE_ARRAY_MEMBER];
} SerializedReindexState;

/* non-export function prototypes */
static bool
relationHasPrimaryKey(Relation rel);
static TupleDesc
ConstructTupleDescriptor(Relation heapRelation, IndexInfo *indexInfo, List *indexColNames, Oid accessMethodObjectId, Oid *collationObjectId, Oid *classObjectId);
static void
InitializeAttributeOids(Relation indexRelation, int numatts, Oid indexoid);
static void
AppendAttributeTuples(Relation indexRelation, int numatts);
static void
UpdateIndexRelation(Oid indexoid, Oid heapoid, Oid parentIndexId, IndexInfo *indexInfo, Oid *collationOids, Oid *classOids, int16 *coloptions, bool primary, bool isexclusion, bool immediate, bool isvalid, bool isready);
static void
index_update_stats(Relation rel, bool hasindex, double reltuples);
static void
IndexCheckExclusion(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo);
static bool
validate_index_callback(ItemPointer itemptr, void *opaque);
static bool
ReindexIsCurrentlyProcessingIndex(Oid indexOid);
static void
SetReindexProcessing(Oid heapOid, Oid indexOid);
static void
ResetReindexProcessing(void);
static void
SetReindexPending(List *indexes);
static void
RemoveReindexPending(Oid indexOid);

/*
 * relationHasPrimaryKey
 *		See whether an existing relation has a primary key.
 *
 * Caller must have suitable lock on the relation.
 *
 * Note: we intentionally do not check indisvalid here; that's because this
 * is used to enforce the rule that there can be only one indisprimary index,
 * and we want that to be true even if said index is invalid.
 */
static bool
relationHasPrimaryKey(Relation rel)
{
































}

/*
 * index_check_primary_key
 *		Apply special checks needed before creating a PRIMARY KEY index
 *
 * This processing used to be in DefineIndex(), but has been split out
 * so that it can be applied during ALTER TABLE ADD PRIMARY KEY USING INDEX.
 *
 * We check for a pre-existing primary key, and that all columns of the index
 * are simple column references (not expressions), and that all those
 * columns are marked NOT NULL.  If not, fail.
 *
 * We used to automatically change unmarked columns to NOT NULL here by doing
 * our own local ALTER TABLE command.  But that doesn't work well if we're
 * executing one subcommand of an ALTER TABLE: the operations may not get
 * performed in the right order overall.  Now we expect that the parser
 * inserted any required ALTER TABLE SET NOT NULL operations before trying
 * to create a primary-key index.
 *
 * Caller had better have at least ShareLock on the table, else the not-null
 * checking isn't trustworthy.
 */
void
index_check_primary_key(Relation heapRel, IndexInfo *indexInfo, bool is_alter_table, IndexStmt *stmt)
{

















































}

/*
 *		ConstructTupleDescriptor
 *
 * Build an index tuple descriptor for a new index
 */
static TupleDesc
ConstructTupleDescriptor(Relation heapRelation, IndexInfo *indexInfo, List *indexColNames, Oid accessMethodObjectId, Oid *collationObjectId, Oid *classObjectId)
{







































































































































































































}

/* ----------------------------------------------------------------
 *		InitializeAttributeOids
 * ----------------------------------------------------------------
 */
static void
InitializeAttributeOids(Relation indexRelation, int numatts, Oid indexoid)
{









}

/* ----------------------------------------------------------------
 *		AppendAttributeTuples
 * ----------------------------------------------------------------
 */
static void
AppendAttributeTuples(Relation indexRelation, int numatts)
{





























}

/* ----------------------------------------------------------------
 *		UpdateIndexRelation
 *
 * Construct and insert a new entry in the pg_index catalog
 * ----------------------------------------------------------------
 */
static void
UpdateIndexRelation(Oid indexoid, Oid heapoid, Oid parentIndexId, IndexInfo *indexInfo, Oid *collationOids, Oid *classOids, int16 *coloptions, bool primary, bool isexclusion, bool immediate, bool isvalid, bool isready)
{













































































































}

/*
 * index_create
 *
 * heapRelation: table to build index on (suitably locked by caller)
 * indexRelationName: what it say
 * indexRelationId: normally, pass InvalidOid to let this routine
 *		generate an OID for the index.  During bootstrap this may be
 *		nonzero to specify a preselected OID.
 * parentIndexRelid: if creating an index partition, the OID of the
 *		parent index; otherwise InvalidOid.
 * parentConstraintId: if creating a constraint on a partition, the OID
 *		of the constraint in the parent; otherwise InvalidOid.
 * relFileNode: normally, pass InvalidOid to get new storage.  May be
 *		nonzero to attach an existing valid build.
 * indexInfo: same info executor uses to insert into the index
 * indexColNames: column names to use for index (List of char *)
 * accessMethodObjectId: OID of index AM to use
 * tableSpaceId: OID of tablespace to use
 * collationObjectId: array of collation OIDs, one per index column
 * classObjectId: array of index opclass OIDs, one per index column
 * coloptions: array of per-index-column indoption settings
 * reloptions: AM-specific options
 * flags: bitmask that can include any combination of these bits:
 *		INDEX_CREATE_IS_PRIMARY
 *			the index is a primary key
 *		INDEX_CREATE_ADD_CONSTRAINT:
 *			invoke index_constraint_create also
 *		INDEX_CREATE_SKIP_BUILD:
 *			skip the index_build() step for the moment; caller must
 *do it later (typically via reindex_index()) INDEX_CREATE_CONCURRENT: do not
 *lock the table against writers.  The index will be marked "invalid" and the
 *caller must take additional steps to fix it up. INDEX_CREATE_IF_NOT_EXISTS: do
 *not throw an error if a relation with the same name already exists.
 *		INDEX_CREATE_PARTITIONED:
 *			create a partitioned index (table must be partitioned)
 * constr_flags: flags passed to index_constraint_create
 *		(only if INDEX_CREATE_ADD_CONSTRAINT is set)
 * allow_system_table_mods: allow table to be a system catalog
 * is_internal: if true, post creation hook for new index
 * constraintId: if not NULL, receives OID of created constraint
 *
 * Returns the OID of the created index.
 */
Oid
index_create(Relation heapRelation, const char *indexRelationName, Oid indexRelationId, Oid parentIndexRelid, Oid parentConstraintId, Oid relFileNode, IndexInfo *indexInfo, List *indexColNames, Oid accessMethodObjectId, Oid tableSpaceId, Oid *collationObjectId, Oid *classObjectId, int16 *coloptions, Datum reloptions, bits16 flags, bits16 constr_flags, bool allow_system_table_mods, bool is_internal, Oid *constraintId)
{


















































































































































































































































































































































































































































































}

/*
 * index_concurrently_create_copy
 *
 * Create concurrently an index based on the definition of the one provided by
 * caller.  The index is inserted into catalogs and needs to be built later
 * on.  This is called during concurrent reindex processing.
 */
Oid
index_concurrently_create_copy(Relation heapRelation, Oid oldIndexId, const char *newName)
{


























































































































}

/*
 * index_concurrently_build
 *
 * Build index for a concurrent operation.  Low-level locks are taken when
 * this operation is performed to prevent only schema changes, but they need
 * to be kept until the end of the transaction performing this operation.
 * 'indexOid' refers to an index relation OID already created as part of
 * previous processing, and 'heapOid' refers to its parent heap relation.
 */
void
index_concurrently_build(Oid heapRelationId, Oid indexRelationId)
{





















































}

/*
 * index_concurrently_swap
 *
 * Swap name, dependencies, and constraints of the old index over to the new
 * index, while marking the old index as invalid and the new as valid.
 */
void
index_concurrently_swap(Oid newIndexId, Oid oldIndexId, const char *oldName)
{































































































































































































































































































































}

/*
 * index_concurrently_set_dead
 *
 * Perform the last invalidation stage of DROP INDEX CONCURRENTLY or REINDEX
 * CONCURRENTLY before actually dropping the index.  After calling this
 * function, the index is seen by all the backends as dead.  Low-level locks
 * taken here are kept until the end of the transaction calling this function.
 */
void
index_concurrently_set_dead(Oid heapId, Oid indexId)
{
































}

/*
 * index_constraint_create
 *
 * Set up a constraint associated with an index.  Return the new constraint's
 * address.
 *
 * heapRelation: table owning the index (must be suitably locked by caller)
 * indexRelationId: OID of the index
 * parentConstraintId: if constraint is on a partition, the OID of the
 *		constraint in the parent.
 * indexInfo: same info executor uses to insert into the index
 * constraintName: what it say (generally, should match name of index)
 * constraintType: one of CONSTRAINT_PRIMARY, CONSTRAINT_UNIQUE, or
 *		CONSTRAINT_EXCLUSION
 * flags: bitmask that can include any combination of these bits:
 *		INDEX_CONSTR_CREATE_MARK_AS_PRIMARY: index is a PRIMARY KEY
 *		INDEX_CONSTR_CREATE_DEFERRABLE: constraint is DEFERRABLE
 *		INDEX_CONSTR_CREATE_INIT_DEFERRED: constraint is INITIALLY
 *DEFERRED INDEX_CONSTR_CREATE_UPDATE_INDEX: update the pg_index row
 *		INDEX_CONSTR_CREATE_REMOVE_OLD_DEPS: remove existing
 *dependencies of index on table's columns allow_system_table_mods: allow table
 *to be a system catalog is_internal: index is constructed due to internal
 *process
 */
ObjectAddress
index_constraint_create(Relation heapRelation, Oid indexRelationId, Oid parentConstraintId, IndexInfo *indexInfo, const char *constraintName, char constraintType, bits16 constr_flags, bool allow_system_table_mods, bool is_internal)
{

















































































































































































}

/*
 *		index_drop
 *
 * NOTE: this routine should now only be called through performDeletion(),
 * else associated dependencies won't be cleaned up.
 *
 * If concurrent is true, do a DROP INDEX CONCURRENTLY.  If concurrent is
 * false but concurrent_lock_mode is true, then do a normal DROP INDEX but
 * take a lock for CONCURRENTLY processing.  That is used as part of REINDEX
 * CONCURRENTLY.
 */
void
index_drop(Oid indexId, bool concurrent, bool concurrent_lock_mode)
{














































































































































































































































































}

/* ----------------------------------------------------------------
 *						index_build support
 * ----------------------------------------------------------------
 */

/* ----------------
 *		BuildIndexInfo
 *			Construct an IndexInfo record for an open index
 *
 * IndexInfo stores the information about the index that's needed by
 * FormIndexDatum, which is used for both index_build() and later insertion
 * of individual index tuples.  Normally we build an IndexInfo for an index
 * just once per command, and then use it for (potentially) many tuples.
 * ----------------
 */
IndexInfo *
BuildIndexInfo(Relation index)
{




























































}

/* ----------------
 *		BuildDummyIndexInfo
 *			Construct a dummy IndexInfo record for an open index
 *
 * This differs from the real BuildIndexInfo in that it will never run any
 * user-defined code that might exist in index expressions or predicates.
 * Instead of the real index expressions, we return null constants that have
 * the right types/typmods/collations.  Predicates and exclusion clauses are
 * just ignored.  This is sufficient for the purpose of truncating an index,
 * since we will not need to actually evaluate the expressions or predicates;
 * the only thing that's likely to be done with the data is construction of
 * a tupdesc describing the index's rowtype.
 * ----------------
 */
IndexInfo *
BuildDummyIndexInfo(Relation index)
{



























}

/*
 * CompareIndexInfo
 *		Return whether the properties of two indexes (in different
 *tables) indicate that they have the "same" definitions.
 *
 * Note: passing collations and opfamilies separately is a kludge.  Adding
 * them to IndexInfo may result in better coding here and elsewhere.
 *
 * Use convert_tuples_by_name_map(index2, index1) to build the attmap.
 */
bool
CompareIndexInfo(IndexInfo *info1, IndexInfo *info2, Oid *collations1, Oid *collations2, Oid *opfamilies1, Oid *opfamilies2, AttrNumber *attmap, int maplen)
{

























































































































}

/* ----------------
 *		BuildSpeculativeIndexInfo
 *			Add extra state to IndexInfo record
 *
 * For unique indexes, we usually don't want to add info to the IndexInfo for
 * checking uniqueness, since the B-Tree AM handles that directly.  However,
 * in the case of speculative insertion, additional support is required.
 *
 * Do this processing here rather than in BuildIndexInfo() to not incur the
 * overhead in the common non-speculative cases.
 * ----------------
 */
void
BuildSpeculativeIndexInfo(Relation index, IndexInfo *ii)
{


































}

/* ----------------
 *		FormIndexDatum
 *			Construct values[] and isnull[] arrays for a new index
 *tuple.
 *
 *	indexInfo		Info about the index
 *	slot			Heap tuple for which we must prepare an index
 *entry estate			executor state for evaluating any index
 *expressions values			Array of index Datums (output area)
 *	isnull			Array of is-null indicators (output area)
 *
 * When there are no index expressions, estate may be NULL.  Otherwise it
 * must be supplied, *and* the ecxt_scantuple slot of its per-tuple expr
 * context must point to the heap tuple passed in.
 *
 * Notice we don't actually call index_form_tuple() here; we just prepare
 * its input arrays values[] and isnull[].  This is because the index AM
 * may wish to alter the data before storage.
 * ----------------
 */
void
FormIndexDatum(IndexInfo *indexInfo, TupleTableSlot *slot, EState *estate, Datum *values, bool *isnull)
{


















































}

/*
 * index_update_stats --- update pg_class entry after CREATE INDEX or REINDEX
 *
 * This routine updates the pg_class row of either an index or its parent
 * relation after CREATE INDEX or REINDEX.  Its rather bizarre API is designed
 * to ensure we can do all the necessary work in just one update.
 *
 * hasindex: set relhasindex to this value
 * reltuples: if >= 0, set reltuples to this value; else no change
 *
 * If reltuples >= 0, relpages and relallvisible are also updated (using
 * RelationGetNumberOfBlocks() and visibilitymap_count()).
 *
 * NOTE: an important side-effect of this operation is that an SI invalidation
 * message is sent out to all backends --- including me --- causing relcache
 * entries to be flushed or updated with the new data.  This must happen even
 * if we find that no change is needed in the pg_class row.  When updating
 * a heap entry, this ensures that other backends find out about the new
 * index.  When updating an index, it's important because some index AMs
 * expect a relcache flush to occur after REINDEX.
 */
static void
index_update_stats(Relation rel, bool hasindex, double reltuples)
{






























































































































}

/*
 * index_build - invoke access-method-specific index build procedure
 *
 * On entry, the index's catalog entries are valid, and its physical disk
 * file has been created but is empty.  We call the AM-specific build
 * procedure to fill in the index contents.  We then update the pg_class
 * entries of the index and heap relation as needed, using statistics
 * returned by ambuild as well as data passed by the caller.
 *
 * isreindex indicates we are recreating a previously-existing index.
 * parallel indicates if parallelism may be useful.
 *
 * Note: before Postgres 8.2, the passed-in heap and index Relations
 * were automatically closed by this routine.  This is no longer the case.
 * The caller opened 'em, and the caller should close 'em.
 */
void
index_build(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo, bool isreindex, bool parallel)
{






















































































































































}

/*
 * IndexCheckExclusion - verify that a new exclusion constraint is satisfied
 *
 * When creating an exclusion constraint, we first build the index normally
 * and then rescan the heap to check for conflicts.  We assume that we only
 * need to validate tuples that are live according to an up-to-date snapshot,
 * and that these were correctly indexed even in the presence of broken HOT
 * chains.  This should be OK since we are holding at least ShareLock on the
 * table, meaning there can be no uncommitted updates from other transactions.
 * (Note: that wouldn't necessarily work for system catalogs, since many
 * operations release write lock early on the system catalogs.)
 */
static void
IndexCheckExclusion(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo)
{


















































































}

/*
 * validate_index - support code for concurrent index builds
 *
 * We do a concurrent index build by first inserting the catalog entry for the
 * index via index_create(), marking it not indisready and not indisvalid.
 * Then we commit our transaction and start a new one, then we wait for all
 * transactions that could have been modifying the table to terminate.  Now
 * we know that any subsequently-started transactions will see the index and
 * honor its constraints on HOT updates; so while existing HOT-chains might
 * be broken with respect to the index, no currently live tuple will have an
 * incompatible HOT update done to it.  We now build the index normally via
 * index_build(), while holding a weak lock that allows concurrent
 * insert/update/delete.  Also, we index only tuples that are valid
 * as of the start of the scan (see table_index_build_scan), whereas a normal
 * build takes care to include recently-dead tuples.  This is OK because
 * we won't mark the index valid until all transactions that might be able
 * to see those tuples are gone.  The reason for doing that is to avoid
 * bogus unique-index failures due to concurrent UPDATEs (we might see
 * different versions of the same row as being valid when we pass over them,
 * if we used HeapTupleSatisfiesVacuum).  This leaves us with an index that
 * does not contain any tuples added to the table while we built the index.
 *
 * Next, we mark the index "indisready" (but still not "indisvalid") and
 * commit the second transaction and start a third.  Again we wait for all
 * transactions that could have been modifying the table to terminate.  Now
 * we know that any subsequently-started transactions will see the index and
 * insert their new tuples into it.  We then take a new reference snapshot
 * which is passed to validate_index().  Any tuples that are valid according
 * to this snap, but are not in the index, must be added to the index.
 * (Any tuples committed live after the snap will be inserted into the
 * index by their originating transaction.  Any tuples committed dead before
 * the snap need not be indexed, because we will wait out all transactions
 * that might care about them before we mark the index valid.)
 *
 * validate_index() works by first gathering all the TIDs currently in the
 * index, using a bulkdelete callback that just stores the TIDs and doesn't
 * ever say "delete it".  (This should be faster than a plain indexscan;
 * also, not all index AMs support full-index indexscan.)  Then we sort the
 * TIDs, and finally scan the table doing a "merge join" against the TID list
 * to see which tuples are missing from the index.  Thus we will ensure that
 * all tuples valid according to the reference snapshot are in the index.
 *
 * Building a unique index this way is tricky: we might try to insert a
 * tuple that is already dead or is in process of being deleted, and we
 * mustn't have a uniqueness failure against an updated version of the same
 * row.  We could try to check the tuple to see if it's already dead and tell
 * index_insert() not to do the uniqueness check, but that still leaves us
 * with a race condition against an in-progress update.  To handle that,
 * we expect the index AM to recheck liveness of the to-be-inserted tuple
 * before it declares a uniqueness error.
 *
 * After completing validate_index(), we wait until all transactions that
 * were alive at the time of the reference snapshot are gone; this is
 * necessary to be sure there are none left with a transaction snapshot
 * older than the reference (and hence possibly able to see tuples we did
 * not index).  Then we mark the index "indisvalid" and commit.  Subsequent
 * transactions will be able to use it for queries.
 *
 * Doing two full table scans is a brute-force strategy.  We could try to be
 * cleverer, eg storing new tuples in a special area of the table (perhaps
 * making the table append-only by setting use_fsm).  However that would
 * add yet more locking issues.
 */
void
validate_index(Oid heapId, Oid indexId, Snapshot snapshot)
{



























































































}

/*
 * validate_index_callback - bulkdelete callback to collect the index TIDs
 */
static bool
validate_index_callback(ItemPointer itemptr, void *opaque)
{






}

/*
 * index_set_state_flags - adjust pg_index state flags
 *
 * This is used during CREATE/DROP INDEX CONCURRENTLY to adjust the pg_index
 * flags that denote the index's state.
 *
 * Note that CatalogTupleUpdate() sends a cache invalidation message for the
 * tuple, so other sessions will hear about the update as soon as we commit.
 */
void
index_set_state_flags(Oid indexId, IndexStateFlagsAction action)
{







































































}

/*
 * IndexGetRelation: given an index's relation OID, get the OID of the
 * relation it is an index on.  Uses the system cache.
 */
Oid
IndexGetRelation(Oid indexId, bool missing_ok)
{



















}

/*
 * reindex_index - This routine is used to recreate a single index
 */
void
reindex_index(Oid indexId, bool skip_constraint_checks, char persistence, int options)
{






















































































































































































































}

/*
 * reindex_relation - This routine is used to recreate all indexes
 * of a relation (and optionally its toast relation too, if any).
 *
 * "flags" is a bitmask that can include any combination of these bits:
 *
 * REINDEX_REL_PROCESS_TOAST: if true, process the toast table too (if any).
 *
 * REINDEX_REL_SUPPRESS_INDEX_USE: if true, the relation was just completely
 * rebuilt by an operation such as VACUUM FULL or CLUSTER, and therefore its
 * indexes are inconsistent with it.  This makes things tricky if the relation
 * is a system catalog that we might consult during the reindexing.  To deal
 * with that case, we mark all of the indexes as pending rebuild so that they
 * won't be trusted until rebuilt.  The caller is required to call us *without*
 * having made the rebuilt table visible by doing CommandCounterIncrement;
 * we'll do CCI after having collected the index list.  (This way we can still
 * use catalog indexes while collecting the list.)
 *
 * REINDEX_REL_CHECK_CONSTRAINTS: if true, recheck unique and exclusion
 * constraint conditions, else don't.  To avoid deadlocks, VACUUM FULL or
 * CLUSTER on a system catalog must omit this flag.  REINDEX should be used to
 * rebuild an index if constraint inconsistency is suspected.  For optimal
 * performance, other callers should include the flag only after transforming
 * the data in a manner that risks a change in constraint validity.
 *
 * REINDEX_REL_FORCE_INDEXES_UNLOGGED: if true, set the persistence of the
 * rebuilt indexes to unlogged.
 *
 * REINDEX_REL_FORCE_INDEXES_PERMANENT: if true, set the persistence of the
 * rebuilt indexes to permanent.
 *
 * Returns true if any indexes were rebuilt (including toast table's index
 * when relevant).  Note that a CommandCounterIncrement will occur after each
 * index rebuild.
 */
bool
reindex_relation(Oid relid, int flags, int options)
{

















































































































}

/* ----------------------------------------------------------------
 *		System index reindexing support
 *
 * When we are busy reindexing a system index, this code provides support
 * for preventing catalog lookups from using that index.  We also make use
 * of this to catch attempted uses of user indexes during reindexing of
 * those indexes.  This information is propagated to parallel workers;
 * attempting to change it during a parallel operation is not permitted.
 * ----------------------------------------------------------------
 */

static Oid currentlyReindexedHeap = InvalidOid;
static Oid currentlyReindexedIndex = InvalidOid;
static List *pendingReindexedIndexes = NIL;
static int reindexingNestLevel = 0;

/*
 * ReindexIsProcessingHeap
 *		True if heap specified by OID is currently being reindexed.
 */
bool
ReindexIsProcessingHeap(Oid heapOid)
{

}

/*
 * ReindexIsCurrentlyProcessingIndex
 *		True if index specified by OID is currently being reindexed.
 */
static bool
ReindexIsCurrentlyProcessingIndex(Oid indexOid)
{

}

/*
 * ReindexIsProcessingIndex
 *		True if index specified by OID is currently being reindexed,
 *		or should be treated as invalid because it is awaiting reindex.
 */
bool
ReindexIsProcessingIndex(Oid indexOid)
{

}

/*
 * SetReindexProcessing
 *		Set flag that specified heap/index are being reindexed.
 */
static void
SetReindexProcessing(Oid heapOid, Oid indexOid)
{












}

/*
 * ResetReindexProcessing
 *		Unset reindexing status.
 */
static void
ResetReindexProcessing(void)
{



}

/*
 * SetReindexPending
 *		Mark the given indexes as pending reindex.
 *
 * NB: we assume that the current memory context stays valid throughout.
 */
static void
SetReindexPending(List *indexes)
{











}

/*
 * RemoveReindexPending
 *		Remove the given index from the pending list.
 */
static void
RemoveReindexPending(Oid indexOid)
{





}

/*
 * ResetReindexState
 *		Clear all reindexing state during (sub)transaction abort.
 */
void
ResetReindexState(int nestLevel)
{




















}

/*
 * EstimateReindexStateSpace
 *		Estimate space needed to pass reindex state to parallel workers.
 */
Size
EstimateReindexStateSpace(void)
{

}

/*
 * SerializeReindexState
 *		Serialize reindex state for parallel workers.
 */
void
SerializeReindexState(Size maxsize, char *start_address)
{











}

/*
 * RestoreReindexState
 *		Restore reindex state in a parallel worker.
 */
void
RestoreReindexState(void *reindexstate)
{

















}