/*-------------------------------------------------------------------------
 *
 * execPartition.c
 *	  Support routines for partitioning.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/execPartition.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/table.h"
#include "access/tableam.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "executor/execPartition.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "partitioning/partprune.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"

/*-----------------------
 * PartitionTupleRouting - Encapsulates all information required to
 * route a tuple inserted into a partitioned table to one of its leaf
 * partitions.
 *
 * partition_root
 *		The partitioned table that's the target of the command.
 *
 * partition_dispatch_info
 *		Array of 'max_dispatch' elements containing a pointer to a
 *		PartitionDispatch object for every partitioned table touched by
 *tuple routing.  The entry for the target partitioned table is *always* present
 *in the 0th element of this array.  See comment for
 *		PartitionDispatchData->indexes for details on how this array is
 *		indexed.
 *
 * nonleaf_partitions
 *		Array of 'max_dispatch' elements containing pointers to fake
 *		ResultRelInfo objects for nonleaf partitions, useful for
 *checking the partition constraint.
 *
 * num_dispatch
 *		The current number of items stored in the
 *'partition_dispatch_info' array.  Also serves as the index of the next free
 *array element for new PartitionDispatch objects that need to be stored.
 *
 * max_dispatch
 *		The current allocated size of the 'partition_dispatch_info'
 *array.
 *
 * partitions
 *		Array of 'max_partitions' elements containing a pointer to a
 *		ResultRelInfo for every leaf partitions touched by tuple
 *routing. Some of these are pointers to ResultRelInfos which are borrowed out
 *of 'subplan_resultrel_htab'.  The remainder have been built especially for
 *tuple routing.  See comment for PartitionDispatchData->indexes for details on
 *how this array is indexed.
 *
 * num_partitions
 *		The current number of items stored in the 'partitions' array.
 *Also serves as the index of the next free array element for new ResultRelInfo
 *objects that need to be stored.
 *
 * max_partitions
 *		The current allocated size of the 'partitions' array.
 *
 * subplan_resultrel_htab
 *		Hash table to store subplan ResultRelInfos by Oid.  This is used
 *to cache ResultRelInfos from subplans of an UPDATE ModifyTable node; NULL in
 *other cases.  Some of these may be useful for tuple routing to save having to
 *build duplicates.
 *
 * memcxt
 *		Memory context used to allocate subsidiary structs.
 *-----------------------
 */
struct PartitionTupleRouting
{
  Relation partition_root;
  PartitionDispatch *partition_dispatch_info;
  ResultRelInfo **nonleaf_partitions;
  int num_dispatch;
  int max_dispatch;
  ResultRelInfo **partitions;
  int num_partitions;
  int max_partitions;
  HTAB *subplan_resultrel_htab;
  MemoryContext memcxt;
};

/*-----------------------
 * PartitionDispatch - information about one partitioned table in a partition
 * hierarchy required to route a tuple to any of its partitions.  A
 * PartitionDispatch is always encapsulated inside a PartitionTupleRouting
 * struct and stored inside its 'partition_dispatch_info' array.
 *
 * reldesc
 *		Relation descriptor of the table
 *
 * key
 *		Partition key information of the table
 *
 * keystate
 *		Execution state required for expressions in the partition key
 *
 * partdesc
 *		Partition descriptor of the table
 *
 * tupslot
 *		A standalone TupleTableSlot initialized with this table's tuple
 *		descriptor, or NULL if no tuple conversion between the parent is
 *		required.
 *
 * tupmap
 *		TupleConversionMap to convert from the parent's rowtype to this
 *table's rowtype  (when extracting the partition key of a tuple just before
 *		routing it through this table). A NULL value is stored if no
 *tuple conversion is required.
 *
 * indexes
 *		Array of partdesc->nparts elements.  For leaf partitions the
 *index corresponds to the partition's ResultRelInfo in the encapsulating
 *		PartitionTupleRouting's partitions array.  For partitioned
 *partitions, the index corresponds to the PartitionDispatch for it in its
 *		partition_dispatch_info array.  -1 indicates we've not yet
 *allocated anything in PartitionTupleRouting for the partition.
 *-----------------------
 */
typedef struct PartitionDispatchData
{
  Relation reldesc;
  PartitionKey key;
  List *keystate; /* list of ExprState */
  PartitionDesc partdesc;
  TupleTableSlot *tupslot;
  AttrNumber *tupmap;
  int indexes[FLEXIBLE_ARRAY_MEMBER];
} PartitionDispatchData;

/* struct to hold result relations coming from UPDATE subplans */
typedef struct SubplanResultRelHashElem
{
  Oid relid; /* hash key -- must be first */
  ResultRelInfo *rri;
} SubplanResultRelHashElem;

static void
ExecHashSubPlanResultRelsByOid(ModifyTableState *mtstate, PartitionTupleRouting *proute);
static ResultRelInfo *
ExecInitPartitionInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *rootResultRelInfo, int partidx);
static void
ExecInitRoutingInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *partRelInfo, int partidx);
static PartitionDispatch
ExecInitPartitionDispatchInfo(EState *estate, PartitionTupleRouting *proute, Oid partoid, PartitionDispatch parent_pd, int partidx, ResultRelInfo *rootResultRelInfo);
static void
FormPartitionKeyDatum(PartitionDispatch pd, TupleTableSlot *slot, EState *estate, Datum *values, bool *isnull);
static int
get_partition_for_tuple(PartitionDispatch pd, Datum *values, bool *isnull);
static char *
ExecBuildSlotPartitionKeyDescription(Relation rel, Datum *values, bool *isnull, int maxfieldlen);
static List *
adjust_partition_tlist(List *tlist, TupleConversionMap *map);
static void
ExecInitPruningContext(PartitionPruneContext *context, List *pruning_steps, PartitionDesc partdesc, PartitionKey partkey, PlanState *planstate);
static void
find_matching_subplans_recurse(PartitionPruningData *prunedata, PartitionedRelPruningData *pprune, bool initial_prune, Bitmapset **validsubplans);

/*
 * ExecSetupPartitionTupleRouting - sets up information needed during
 * tuple routing for partitioned tables, encapsulates it in
 * PartitionTupleRouting, and returns it.
 *
 * Callers must use the returned PartitionTupleRouting during calls to
 * ExecFindPartition().  The actual ResultRelInfo for a partition is only
 * allocated when the partition is found for the first time.
 *
 * The current memory context is used to allocate this struct and all
 * subsidiary structs that will be allocated from it later on.  Typically
 * it should be estate->es_query_cxt.
 */
PartitionTupleRouting *
ExecSetupPartitionTupleRouting(EState *estate, ModifyTableState *mtstate, Relation rel)
{




































}

/*
 * ExecFindPartition -- Return the ResultRelInfo for the leaf partition that
 * the tuple contained in *slot should belong to.
 *
 * If the partition's ResultRelInfo does not yet exist in 'proute' then we set
 * one up or reuse one from mtstate's resultRelInfo array.  When reusing a
 * ResultRelInfo from the mtstate we verify that the relation is a valid
 * target for INSERTs and then set up a PartitionRoutingInfo for it.
 *
 * rootResultRelInfo is the relation named in the query.
 *
 * estate must be non-NULL; we'll need it to compute any expressions in the
 * partition keys.  Also, its per-tuple contexts are used as evaluation
 * scratch space.
 *
 * If no leaf partition is found, this routine errors out with the appropriate
 * error message.  An error may also be raised if the found target partition
 * is not a valid target for an INSERT.
 */
ResultRelInfo *
ExecFindPartition(ModifyTableState *mtstate, ResultRelInfo *rootResultRelInfo, PartitionTupleRouting *proute, TupleTableSlot *slot, EState *estate)
{
























































































































































































































}

/*
 * ExecHashSubPlanResultRelsByOid
 *		Build a hash table to allow fast lookups of subplan
 *ResultRelInfos by partition Oid.  We also populate the subplan ResultRelInfo
 *with an ri_PartitionRoot.
 */
static void
ExecHashSubPlanResultRelsByOid(ModifyTableState *mtstate, PartitionTupleRouting *proute)
{































}

/*
 * ExecInitPartitionInfo
 *		Lock the partition and initialize ResultRelInfo.  Also setup
 *other information for the partition and store it in the next empty slot in the
 *proute->partitions array.
 *
 * Returns the ResultRelInfo
 */
static ResultRelInfo *
ExecInitPartitionInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *rootResultRelInfo, int partidx)
{







































































































































































































































































































}

/*
 * ExecInitRoutingInfo
 *		Set up information needed for translating tuples between root
 *		partitioned table format and partition format, and keep track of
 *it in PartitionTupleRouting.
 */
static void
ExecInitRoutingInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *partRelInfo, int partidx)
{
























































































}

/*
 * ExecInitPartitionDispatchInfo
 *		Lock the partitioned table (if not locked already) and
 *initialize PartitionDispatch for a partitioned table and store it in the next
 *		available slot in the proute->partition_dispatch_info array.
 *Also, record the index into this array in the parent_pd->indexes[] array in
 *		the partidx element so that we can properly retrieve the newly
 *created PartitionDispatch later.
 */
static PartitionDispatch
ExecInitPartitionDispatchInfo(EState *estate, PartitionTupleRouting *proute, Oid partoid, PartitionDispatch parent_pd, int partidx, ResultRelInfo *rootResultRelInfo)
{

















































































































}

/*
 * ExecCleanupTupleRouting -- Clean up objects allocated for partition tuple
 * routing.
 *
 * Close all the partitioned tables, leaf partitions, and their indices.
 */
void
ExecCleanupTupleRouting(ModifyTableState *mtstate, PartitionTupleRouting *proute)
{





















































}

/* ----------------
 *		FormPartitionKeyDatum
 *			Construct values[] and isnull[] arrays for the partition
 *key of a tuple.
 *
 *	pd				Partition dispatch object of the
 *partitioned table slot			Heap tuple from which to extract
 *partition key estate			executor state for evaluating any
 *partition key expressions (must be non-NULL) values			Array of
 *partition key Datums (output area) isnull			Array of is-null
 *indicators (output area)
 *
 * the ecxt_scantuple slot of estate's per-tuple expr context must point to
 * the heap tuple passed in.
 * ----------------
 */
static void
FormPartitionKeyDatum(PartitionDispatch pd, TupleTableSlot *slot, EState *estate, Datum *values, bool *isnull)
{










































}

/*
 * get_partition_for_tuple
 *		Finds partition of relation which accepts the partition key
 *specified in values and isnull
 *
 * Return value is index of the partition (>= 0 and < partdesc->nparts) if one
 * found or -1 if none found.
 */
static int
get_partition_for_tuple(PartitionDispatch pd, Datum *values, bool *isnull)
{






















































































}

/*
 * ExecBuildSlotPartitionKeyDescription
 *
 * This works very much like BuildIndexValueDescription() and is currently
 * used for building error messages when ExecFindPartition() fails to find
 * partition for a row.
 */
static char *
ExecBuildSlotPartitionKeyDescription(Relation rel, Datum *values, bool *isnull, int maxfieldlen)
{















































































}

/*
 * adjust_partition_tlist
 *		Re-order the targetlist entries for a given partition to account
 *for column position differences between the parent and the partition.
 *
 * The expressions have already been fixed, but we must now re-order the
 * entries in case the partition has different column order, and possibly
 * add or remove dummy entries for dropped columns.
 *
 * Although a new List is returned, this feels free to scribble on resno
 * fields of the given tlist, so that should be a working copy.
 */
static List *
adjust_partition_tlist(List *tlist, TupleConversionMap *map)
{























































}

/*-------------------------------------------------------------------------
 * Run-Time Partition Pruning Support.
 *
 * The following series of functions exist to support the removal of unneeded
 * subplans for queries against partitioned tables.  The supporting functions
 * here are designed to work with any plan type which supports an arbitrary
 * number of subplans, e.g. Append, MergeAppend.
 *
 * When pruning involves comparison of a partition key to a constant, it's
 * done by the planner.  However, if we have a comparison to a non-constant
 * but not volatile expression, that presents an opportunity for run-time
 * pruning by the executor, allowing irrelevant partitions to be skipped
 * dynamically.
 *
 * We must distinguish expressions containing PARAM_EXEC Params from
 * expressions that don't contain those.  Even though a PARAM_EXEC Param is
 * considered to be a stable expression, it can change value from one plan
 * node scan to the next during query execution.  Stable comparison
 * expressions that don't involve such Params allow partition pruning to be
 * done once during executor startup.  Expressions that do involve such Params
 * require us to prune separately for each scan of the parent plan node.
 *
 * Note that pruning away unneeded subplans during executor startup has the
 * added benefit of not having to initialize the unneeded subplans at all.
 *
 *
 * Functions:
 *
 * ExecCreatePartitionPruneState:
 *		Creates the PartitionPruneState required by each of the two
 *pruning functions.  Details stored include how to map the partition index
 *		returned by the partition pruning code into subplan indexes.
 *
 * ExecFindInitialMatchingSubPlans:
 *		Returns indexes of matching subplans.  Partition pruning is
 *attempted without any evaluation of expressions containing PARAM_EXEC Params.
 *		This function must be called during executor startup for the
 *parent plan before the subplans themselves are initialized.  Subplans which
 *		are found not to match by this function must be removed from the
 *		plan's list of subplans during execution, as this function
 *performs a remap of the partition index to subplan index map and the newly
 *		created map provides indexes only for subplans which remain
 *after calling this function.
 *
 * ExecFindMatchingSubPlans:
 *		Returns indexes of matching subplans after evaluating all
 *available expressions.  This function can only be called during execution and
 *		must be called again each time the value of a Param listed in
 *		PartitionPruneState's 'execparamids' changes.
 *-------------------------------------------------------------------------
 */

/*
 * ExecCreatePartitionPruneState
 *		Build the data structure required for calling
 *		ExecFindInitialMatchingSubPlans and ExecFindMatchingSubPlans.
 *
 * 'planstate' is the parent plan node's execution state.
 *
 * 'partitionpruneinfo' is a PartitionPruneInfo as generated by
 * make_partition_pruneinfo.  Here we build a PartitionPruneState containing a
 * PartitionPruningData for each partitioning hierarchy (i.e., each sublist of
 * partitionpruneinfo->prune_infos), each of which contains a
 * PartitionedRelPruningData for each PartitionedRelPruneInfo appearing in
 * that sublist.  This two-level system is needed to keep from confusing the
 * different hierarchies when a UNION ALL contains multiple partitioned tables
 * as children.  The data stored in each PartitionedRelPruningData can be
 * re-used each time we re-evaluate which partitions match the pruning steps
 * provided in each PartitionedRelPruneInfo.
 */
PartitionPruneState *
ExecCreatePartitionPruneState(PlanState *planstate, PartitionPruneInfo *partitionpruneinfo)
{



























































































































































































}

/*
 * Initialize a PartitionPruneContext for the given list of pruning steps.
 */
static void
ExecInitPruningContext(PartitionPruneContext *context, List *pruning_steps, PartitionDesc partdesc, PartitionKey partkey, PlanState *planstate)
{


















































}

/*
 * ExecFindInitialMatchingSubPlans
 *		Identify the set of subplans that cannot be eliminated by
 *initial pruning, disregarding any pruning constraints involving PARAM_EXEC
 *		Params.
 *
 * If additional pruning passes will be required (because of PARAM_EXEC
 * Params), we must also update the translation data that allows conversion
 * of partition indexes into subplan indexes to account for the unneeded
 * subplans having been removed.
 *
 * Must only be called once per 'prunestate', and only if initial pruning
 * is required.
 *
 * 'nsubplans' must be passed as the total number of unpruned subplans.
 */
Bitmapset *
ExecFindInitialMatchingSubPlans(PartitionPruneState *prunestate, int nsubplans)
{
































































































































































}

/*
 * ExecFindMatchingSubPlans
 *		Determine which subplans match the pruning steps detailed in
 *		'prunestate' for the current comparison expression values.
 *
 * Here we assume we may evaluate PARAM_EXEC Params.
 */
Bitmapset *
ExecFindMatchingSubPlans(PartitionPruneState *prunestate)
{

















































}

/*
 * find_matching_subplans_recurse
 *		Recursive worker function for ExecFindMatchingSubPlans and
 *		ExecFindInitialMatchingSubPlans
 *
 * Adds valid (non-prunable) subplan IDs to *validsubplans
 */
static void
find_matching_subplans_recurse(PartitionPruningData *prunedata, PartitionedRelPruningData *pprune, bool initial_prune, Bitmapset **validsubplans)
{




















































}