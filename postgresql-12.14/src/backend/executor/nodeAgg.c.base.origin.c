/*-------------------------------------------------------------------------
 *
 * nodeAgg.c
 *	  Routines to handle aggregate nodes.
 *
 *	  ExecAgg normally evaluates each aggregate in the following steps:
 *
 *		 transvalue = initcond
 *		 foreach input_tuple do
 *			transvalue = transfunc(transvalue, input_value(s))
 *		 result = finalfunc(transvalue, direct_argument(s))
 *
 *	  If a finalfunc is not supplied then the result is just the ending
 *	  value of transvalue.
 *
 *	  Other behaviors can be selected by the "aggsplit" mode, which exists
 *	  to support partial aggregation.  It is possible to:
 *	  * Skip running the finalfunc, so that the output is always the
 *	  final transvalue state.
 *	  * Substitute the combinefunc for the transfunc, so that transvalue
 *	  states (propagated up from a child partial-aggregation step) are
 *merged rather than processing raw input rows.  (The statements below about the
 *transfunc apply equally to the combinefunc, when it's selected.)
 *	  * Apply the serializefunc to the output values (this only makes sense
 *	  when skipping the finalfunc, since the serializefunc works on the
 *	  transvalue data type).
 *	  * Apply the deserializefunc to the input values (this only makes sense
 *	  when using the combinefunc, for similar reasons).
 *	  It is the planner's responsibility to connect up Agg nodes using these
 *	  alternate behaviors in a way that makes sense, with partial
 *aggregation results being fed to nodes that expect them.
 *
 *	  If a normal aggregate call specifies DISTINCT or ORDER BY, we sort the
 *	  input tuples and eliminate duplicates (if required) before performing
 *	  the above-depicted process.  (However, we don't do that for
 *ordered-set aggregates; their "ORDER BY" inputs are ordinary aggregate
 *arguments so far as this module is concerned.)	Note that partial
 *aggregation is not supported in these cases, since we couldn't ensure global
 *	  ordering or distinctness of the inputs.
 *
 *	  If transfunc is marked "strict" in pg_proc and initcond is NULL,
 *	  then the first non-NULL input_value is assigned directly to
 *transvalue, and transfunc isn't applied until the second non-NULL input_value.
 *	  The agg's first input type and transtype must be the same in this
 *case!
 *
 *	  If transfunc is marked "strict" then NULL input_values are skipped,
 *	  keeping the previous transvalue.  If transfunc is not strict then it
 *	  is called for every input tuple and must deal with NULL initcond
 *	  or NULL input_values for itself.
 *
 *	  If finalfunc is marked "strict" then it is not called when the
 *	  ending transvalue is NULL, instead a NULL result is created
 *	  automatically (this is just the usual handling of strict functions,
 *	  of course).  A non-strict finalfunc can make its own choice of
 *	  what to return for a NULL ending transvalue.
 *
 *	  Ordered-set aggregates are treated specially in one other way: we
 *	  evaluate any "direct" arguments and pass them to the finalfunc along
 *	  with the transition value.
 *
 *	  A finalfunc can have additional arguments beyond the transvalue and
 *	  any "direct" arguments, corresponding to the input arguments of the
 *	  aggregate.  These are always just passed as NULL.  Such arguments may
 *be needed to allow resolution of a polymorphic aggregate's result type.
 *
 *	  We compute aggregate input expressions and run the transition
 *functions in a temporary econtext (aggstate->tmpcontext).  This is reset at
 *least once per input tuple, so when the transvalue datatype is
 *	  pass-by-reference, we have to be careful to copy it into a
 *longer-lived memory context, and free the prior value to avoid memory leakage.
 *We store transvalues in another set of econtexts, aggstate->aggcontexts (one
 *per grouping set, see below), which are also used for the hashtable structures
 *in AGG_HASHED mode.  These econtexts are rescanned, not just reset, at group
 *boundaries so that aggregate transition functions can register shutdown
 *callbacks via AggRegisterCallback.
 *
 *	  The node's regular econtext (aggstate->ss.ps.ps_ExprContext) is used
 *to run finalize functions and compute the output tuple; this context can be
 *	  reset once per output tuple.
 *
 *	  The executor's AggState node is passed as the fmgr "context" value in
 *	  all transfunc and finalfunc calls.  It is not recommended that the
 *	  transition functions look at the AggState node directly, but they can
 *	  use AggCheckCallContext() to verify that they are being called by
 *	  nodeAgg.c (and not as ordinary SQL functions).  The main reason a
 *	  transition function might want to know this is so that it can avoid
 *	  palloc'ing a fixed-size pass-by-ref transition value on every call:
 *	  it can instead just scribble on and return its left input.  Ordinarily
 *	  it is completely forbidden for functions to modify pass-by-ref inputs,
 *	  but in the aggregate case we know the left input is either the initial
 *	  transition value or a previous function result, and in either case its
 *	  value need not be preserved.  See int8inc() for an example.  Notice
 *that the EEOP_AGG_PLAIN_TRANS step is coded to avoid a data copy step when the
 *previous transition value pointer is returned.  It is also possible to avoid
 *repeated data copying when the transition value is an expanded object: to do
 *that, the transition function must take care to return an expanded object that
 *is in a child context of the memory context returned by AggCheckCallContext().
 *Also, some transition functions want to store working state in addition to the
 *nominal transition value; they can use the memory context returned by
 *AggCheckCallContext() to do that.
 *
 *	  Note: AggCheckCallContext() is available as of PostgreSQL 9.0.  The
 *	  AggState is available as context in earlier releases (back to 8.1),
 *	  but direct examination of the node is needed to use it before 9.0.
 *
 *	  As of 9.4, aggregate transition functions can also use AggGetAggref()
 *	  to get hold of the Aggref expression node for their aggregate call.
 *	  This is mainly intended for ordered-set aggregates, which are not
 *	  supported as window functions.  (A regular aggregate function would
 *	  need some fallback logic to use this, since there's no Aggref node
 *	  for a window function.)
 *
 *	  Grouping sets:
 *
 *	  A list of grouping sets which is structurally equivalent to a ROLLUP
 *	  clause (e.g. (a,b,c), (a,b), (a)) can be processed in a single pass
 *over ordered data.  We do this by keeping a separate set of transition values
 *	  for each grouping set being concurrently processed; for each input
 *tuple we update them all, and on group boundaries we reset those states
 *	  (starting at the front of the list) whose grouping values have changed
 *	  (the list of grouping sets is ordered from most specific to least
 *	  specific).
 *
 *	  Where more complex grouping sets are used, we break them down into
 *	  "phases", where each phase has a different sort order (except phase 0
 *	  which is reserved for hashing).  During each phase but the last, the
 *	  input tuples are additionally stored in a tuplesort which is keyed to
 *the next phase's sort order; during each phase but the first, the input tuples
 *are drawn from the previously sorted data.  (The sorting of the data for the
 *first phase is handled by the planner, as it might be satisfied by underlying
 *nodes.)
 *
 *	  Hashing can be mixed with sorted grouping.  To do this, we have an
 *	  AGG_MIXED strategy that populates the hashtables during the first
 *sorted phase, and switches to reading them out after completing all sort
 *phases. We can also support AGG_HASHED with multiple hash tables and no
 *sorting at all.
 *
 *	  From the perspective of aggregate transition and final functions, the
 *	  only issue regarding grouping sets is this: a single call site
 *(flinfo) of an aggregate function may be used for updating several different
 *	  transition values in turn. So the function must not cache in the
 *flinfo anything which logically belongs as part of the transition value (most
 *	  importantly, the memory context in which the transition value exists).
 *	  The support API functions (AggCheckCallContext, AggRegisterCallback)
 *are sensitive to the grouping set for which the aggregate function is
 *	  currently being called.
 *
 *	  Plan structure:
 *
 *	  What we get from the planner is actually one "real" Agg node which is
 *	  part of the plan tree proper, but which optionally has an additional
 *list of Agg nodes hung off the side via the "chain" field.  This is because an
 *	  Agg node happens to be a convenient representation of all the data we
 *	  need for grouping sets.
 *
 *	  For many purposes, we treat the "real" node as if it were just the
 *first node in the chain.  The chain must be ordered such that hashed entries
 *	  come before sorted/plain entries; the real node is marked AGG_MIXED if
 *	  there are both types present (in which case the real node describes
 *one of the hashed groupings, other AGG_HASHED nodes may optionally follow in
 *	  the chain, followed in turn by AGG_SORTED or (one) AGG_PLAIN node). If
 *	  the real node is marked AGG_HASHED or AGG_SORTED, then all the chained
 *	  nodes must be of the same type; if it is AGG_PLAIN, there can be no
 *	  chained nodes.
 *
 *	  We collect all hashed nodes into a single "phase", numbered 0, and
 *create a sorted phase (numbered 1..n) for each AGG_SORTED or AGG_PLAIN node.
 *	  Phase 0 is allocated even if there are no hashes, but remains unused
 *in that case.
 *
 *	  AGG_HASHED nodes actually refer to only a single grouping set each,
 *	  because for each hashed grouping we need a separate grpColIdx and
 *	  numGroups estimate.  AGG_SORTED nodes represent a "rollup", a list of
 *	  grouping sets that share a sort order.  Each AGG_SORTED node other
 *than the first one has an associated Sort node which describes the sort order
 *	  to be used; the first sorted node takes its input from the outer
 *subtree, which the planner has already arranged to provide ordered data.
 *
 *	  Memory and ExprContext usage:
 *
 *	  Because we're accumulating aggregate values across input rows, we need
 *to use more memory contexts than just simple input/output tuple contexts. In
 *fact, for a rollup, we need a separate context for each grouping set so that
 *we can reset the inner (finer-grained) aggregates on their group boundaries
 *while continuing to accumulate values for outer (coarser-grained) groupings.
 *On top of this, we might be simultaneously populating hashtables; however, we
 *only need one context for all the hashtables.
 *
 *	  So we create an array, aggcontexts, with an ExprContext for each
 *grouping set in the largest rollup that we're going to process, and use the
 *	  per-tuple memory context of those ExprContexts to store the aggregate
 *	  transition values.  hashcontext is the single context created to
 *support all hash tables.
 *
 *    Transition / Combine function invocation:
 *
 *    For performance reasons transition functions, including combine
 *    functions, aren't invoked one-by-one from nodeAgg.c after computing
 *    arguments using the expression evaluation engine. Instead
 *    ExecBuildAggTrans() builds one large expression that does both argument
 *    evaluation and transition function invocation. That avoids performance
 *    issues due to repeated uses of expression evaluation, complications due
 *    to filter expressions having to be evaluated early, and allows to JIT
 *    the entire expression into one native function.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeAgg.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/execExpr.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/datum.h"

static void
select_current_set(AggState *aggstate, int setno, bool is_hash);
static void
initialize_phase(AggState *aggstate, int newphase);
static TupleTableSlot *
fetch_input_tuple(AggState *aggstate);
static void
initialize_aggregates(AggState *aggstate, AggStatePerGroup *pergroups, int numReset);
static void
advance_transition_function(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate);
static void
advance_aggregates(AggState *aggstate);
static void
process_ordered_aggregate_single(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate);
static void
process_ordered_aggregate_multi(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate);
static void
finalize_aggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull);
static void
finalize_partialaggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull);
static void
prepare_projection_slot(AggState *aggstate, TupleTableSlot *slot, int currentSet);
static void
finalize_aggregates(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroup);
static TupleTableSlot *
project_aggregates(AggState *aggstate);
static Bitmapset *
find_unaggregated_cols(AggState *aggstate);
static bool
find_unaggregated_cols_walker(Node *node, Bitmapset **colnos);
static void
build_hash_table(AggState *aggstate);
static TupleHashEntryData *
lookup_hash_entry(AggState *aggstate);
static void
lookup_hash_entries(AggState *aggstate);
static TupleTableSlot *
agg_retrieve_direct(AggState *aggstate);
static void
agg_fill_hash_table(AggState *aggstate);
static TupleTableSlot *
agg_retrieve_hash_table(AggState *aggstate);
static Datum
GetAggInitVal(Datum textInitVal, Oid transtype);
static void
build_pertrans_for_aggref(AggStatePerTrans pertrans, AggState *aggstate, EState *estate, Aggref *aggref, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, Oid *inputTypes, int numArguments);
static int
find_compatible_peragg(Aggref *newagg, AggState *aggstate, int lastaggno, List **same_input_transnos);
static int
find_compatible_pertrans(AggState *aggstate, Aggref *newagg, bool shareable, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, List *transnos);

/*
 * Select the current grouping set; affects current_set and
 * curaggcontext.
 */
static void
select_current_set(AggState *aggstate, int setno, bool is_hash)
{











}

/*
 * Switch to phase "newphase", which must either be 0 or 1 (to reset) or
 * current_phase + 1. Juggle the tuplesorts accordingly.
 *
 * Phase 0 is for hashing, which we currently handle last in the AGG_MIXED
 * case, so when entering phase 0, all we need to do is drop open sorts.
 */
static void
initialize_phase(AggState *aggstate, int newphase)
{


















































}

/*
 * Fetch a tuple from either the outer plan (for phase 1) or from the sorter
 * populated by the previous phase.  Copy it to the sorter for the next phase
 * if any.
 *
 * Callers cannot rely on memory for tuple in returned slot remaining valid
 * past any subsequently fetched tuple.
 */
static TupleTableSlot *
fetch_input_tuple(AggState *aggstate)
{























}

/*
 * (Re)Initialize an individual aggregate.
 *
 * This function handles only one grouping set, already set in
 * aggstate->current_set.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
initialize_aggregate(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{



























































}

/*
 * Initialize all aggregate transition states for a new group of input values.
 *
 * If there are multiple grouping sets, we initialize only the first numReset
 * of them (the grouping sets are ordered so that the most specific one, which
 * is reset most often, is first). As a convenience, if numReset is 0, we
 * reinitialize all sets.
 *
 * NB: This cannot be used for hash aggregates, as for those the grouping set
 * number has to be specified from further up.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
initialize_aggregates(AggState *aggstate, AggStatePerGroup *pergroups, int numReset)
{

























}

/*
 * Given new input value(s), advance the transition function of one aggregate
 * state within one grouping set only (already set in aggstate->current_set)
 *
 * The new values (and null flags) have been preloaded into argument positions
 * 1 and up in pertrans->transfn_fcinfo, so that we needn't copy them again to
 * pass to the transition function.  We also expect that the static fields of
 * the fcinfo are already initialized; that was done by ExecInitAgg().
 *
 * It doesn't matter which memory context this is called in.
 */
static void
advance_transition_function(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{




























































































}

/*
 * Advance each aggregate transition state for one input tuple.  The input
 * tuple has been stored in tmpcontext->ecxt_outertuple, so that it is
 * accessible to ExecEvalExpr.
 *
 * We have two sets of transition states to handle: one for sorted aggregation
 * and one for hashed; we do them both here, to avoid multiple evaluation of
 * the inputs.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
advance_aggregates(AggState *aggstate)
{



}

/*
 * Run the transition function for a DISTINCT or ORDER BY aggregate
 * with only one input.  This is called after we have completed
 * entering all the input values into the sort object.  We complete the
 * sort, read out the values in sorted order, and run the transition
 * function on each value (applying DISTINCT if appropriate).
 *
 * Note that the strictness of the transition function was checked when
 * entering the values into the sort, so we don't check it again here;
 * we just apply standard SQL DISTINCT logic.
 *
 * The one-input case is handled separately from the multi-input case
 * for performance reasons: for single by-value inputs, such as the
 * common case of count(distinct id), the tuplesort_getdatum code path
 * is around 300% faster.  (The speedup for by-reference types is less
 * but still noticeable.)
 *
 * This function handles only one grouping set (already set in
 * aggstate->current_set).
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
process_ordered_aggregate_single(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{







































































}

/*
 * Run the transition function for a DISTINCT or ORDER BY aggregate
 * with more than one input.  This is called after we have completed
 * entering all the input values into the sort object.  We complete the
 * sort, read out the values in sorted order, and run the transition
 * function on each value (applying DISTINCT if appropriate).
 *
 * This function handles only one grouping set (already set in
 * aggstate->current_set).
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static void
process_ordered_aggregate_multi(AggState *aggstate, AggStatePerTrans pertrans, AggStatePerGroup pergroupstate)
{










































































}

/*
 * Compute the final value of one aggregate.
 *
 * This function handles only one grouping set (already set in
 * aggstate->current_set).
 *
 * The finalfunction will be run, and the result delivered, in the
 * output-tuple context; caller's CurrentMemoryContext does not matter.
 *
 * The finalfn uses the state as set in the transno. This also might be
 * being used by another aggregate function, so it's important that we do
 * nothing destructive here.
 */
static void
finalize_aggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull)
{















































































}

/*
 * Compute the output value of one partial aggregate.
 *
 * The serialization function will be run, and the result delivered, in the
 * output-tuple context; caller's CurrentMemoryContext does not matter.
 */
static void
finalize_partialaggregate(AggState *aggstate, AggStatePerAgg peragg, AggStatePerGroup pergroupstate, Datum *resultVal, bool *resultIsNull)
{











































}

/*
 * Prepare to finalize and project based on the specified representative tuple
 * slot and grouping set.
 *
 * In the specified tuple slot, force to null all attributes that should be
 * read as null in the context of the current grouping set.  Also stash the
 * current group bitmap where GroupingExpr can get at it.
 *
 * This relies on three conditions:
 *
 * 1) Nothing is ever going to try and extract the whole tuple from this slot,
 * only reference it in evaluations, which will only access individual
 * attributes.
 *
 * 2) No system columns are going to need to be nulled. (If a system column is
 * referenced in a group clause, it is actually projected in the outer plan
 * tlist.)
 *
 * 3) Within a given phase, we never need to recover the value of an attribute
 * once it has been set to null.
 *
 * Poking into the slot this way is a bit ugly, but the consensus is that the
 * alternative was worse.
 */
static void
prepare_projection_slot(AggState *aggstate, TupleTableSlot *slot, int currentSet)
{

































}

/*
 * Compute the final value of all aggregates for one group.
 *
 * This function handles only one grouping set at a time, which the caller must
 * have selected.  It's also the caller's responsibility to adjust the supplied
 * pergroup parameter to point to the current set's transvalues.
 *
 * Results are stored in the output econtext aggvalues/aggnulls.
 */
static void
finalize_aggregates(AggState *aggstate, AggStatePerAgg peraggs, AggStatePerGroup pergroup)
{




















































}

/*
 * Project the result of a group (whose aggs have already been calculated by
 * finalize_aggregates). Returns the result slot, or NULL if no row is
 * projected (suppressed by qual).
 */
static TupleTableSlot *
project_aggregates(AggState *aggstate)
{



















}

/*
 * find_unaggregated_cols
 *	  Construct a bitmapset of the column numbers of un-aggregated Vars
 *	  appearing in our targetlist and qual (HAVING clause)
 */
static Bitmapset *
find_unaggregated_cols(AggState *aggstate)
{







}

static bool
find_unaggregated_cols_walker(Node *node, Bitmapset **colnos)
{




















}

/*
 * (Re-)initialize the hash table(s) to empty.
 *
 * To implement hashed aggregation, we need a hashtable that stores a
 * representative tuple and an array of AggStatePerGroup structs for each
 * distinct set of GROUP BY column values.  We compute the hash key from the
 * GROUP BY columns.  The per-group data is allocated in lookup_hash_entry(),
 * for each entry.
 *
 * We have a separate hashtable and associated perhash data structure for each
 * grouping set for which we're doing hashing.
 *
 * The contents of the hash tables always live in the hashcontext's per-tuple
 * memory context (there is only one of these for all tables together, since
 * they are all reset at the same time).
 */
static void
build_hash_table(AggState *aggstate)
{























}

/*
 * Compute columns that actually need to be stored in hashtable entries.  The
 * incoming tuples from the child plan node will contain grouping columns,
 * other columns referenced in our targetlist and qual, columns used to
 * compute the aggregate functions, and perhaps just junk columns we don't use
 * at all.  Only columns of the first two types need to be stored in the
 * hashtable, and getting rid of the others can make the table entries
 * significantly smaller.  The hashtable only contains the relevant columns,
 * and is packed/unpacked in lookup_hash_entry() / agg_retrieve_hash_table()
 * into the format of the normal input descriptor.
 *
 * Additional columns, in addition to the columns grouped by, come from two
 * sources: Firstly functionally dependent columns that we don't need to group
 * by themselves, and secondly ctids for row-marks.
 *
 * To eliminate duplicates, we build a bitmapset of the needed columns, and
 * then build an array of the columns included in the hashtable. We might
 * still have duplicates if the passed-in grpColIdx has them, which can happen
 * in edge cases from semijoins/distinct; these can't always be removed,
 * because it's not certain that the duplicate cols will be using the same
 * hash function.
 *
 * Note that the array is preserved over ExecReScanAgg, so we allocate it in
 * the per-query context (unlike the hash table itself).
 */
static void
find_hash_columns(AggState *aggstate)
{







































































































}

/*
 * Estimate per-hash-table-entry overhead for the planner.
 *
 * Note that the estimate does not include space for pass-by-reference
 * transition data values, nor for the representative tuple of each group.
 * Nor does this account of the target fill-factor and growth policy of the
 * hash table.
 */
Size
hash_agg_entry_size(int numAggs)
{







}

/*
 * Find or create a hashtable entry for the tuple group containing the current
 * tuple (already set in tmpcontext's outertuple slot), in the current grouping
 * set (which the caller must have selected - note that initialize_aggregate
 * depends on this).
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
static TupleHashEntryData *
lookup_hash_entry(AggState *aggstate)
{













































}

/*
 * Look up hash entries for the current tuple in all hashed grouping sets,
 * returning an array of pergroup pointers suitable for advance_aggregates.
 *
 * Be aware that lookup_hash_entry can reset the tmpcontext.
 */
static void
lookup_hash_entries(AggState *aggstate)
{









}

/*
 * ExecAgg -
 *
 *	  ExecAgg receives tuples from its outer subplan and aggregates over
 *	  the appropriate attribute for each aggregate function use (Aggref
 *	  node) appearing in the targetlist or qual of the node.  The number
 *	  of tuples to aggregate over depends on whether grouped or plain
 *	  aggregation is selected.  In grouped aggregation, we produce a result
 *	  row for each group; in plain aggregation there's a single result row
 *	  for the whole query.  In either case, the value of each aggregate is
 *	  stored in the expression context to be used when ExecProject evaluates
 *	  the result tuple.
 */
static TupleTableSlot *
ExecAgg(PlanState *pstate)
{
































}

/*
 * ExecAgg for non-hashed case
 */
static TupleTableSlot *
agg_retrieve_direct(AggState *aggstate)
{











































































































































































































































































































































}

/*
 * ExecAgg for hashed case: read input and build hash table
 */
static void
agg_fill_hash_table(AggState *aggstate)
{



































}

/*
 * ExecAgg for hashed case: retrieving groups from hash table
 */
static TupleTableSlot *
agg_retrieve_hash_table(AggState *aggstate)
{

















































































































}

/* -----------------
 * ExecInitAgg
 *
 *	Creates the run-time information for the agg node produced by the
 *	planner and initializes its outer subtree.
 *
 * -----------------
 */
AggState *
ExecInitAgg(Agg *node, EState *estate, int eflags)
{










































































































































































































































































































































































































































































































































































































































































































































































































































}

/*
 * Build the state needed to calculate a state value for an aggregate.
 *
 * This initializes all the fields in 'pertrans'. 'aggref' is the aggregate
 * to initialize the state for. 'aggtransfn', 'aggtranstype', and the rest
 * of the arguments could be calculated from 'aggref', but the caller has
 * calculated them already, so might as well pass them.
 */
static void
build_pertrans_for_aggref(AggStatePerTrans pertrans, AggState *aggstate, EState *estate, Aggref *aggref, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, Oid *inputTypes, int numArguments)
{
























































































































































































































































}

static Datum
GetAggInitVal(Datum textInitVal, Oid transtype)
{









}

/*
 * find_compatible_peragg - search for a previously initialized per-Agg struct
 *
 * Searches the previously looked at aggregates to find one which is compatible
 * with this one, with the same input parameters. If no compatible aggregate
 * can be found, returns -1.
 *
 * As a side-effect, this also collects a list of existing, shareable per-Trans
 * structs with matching inputs. If no identical Aggref is found, the list is
 * passed later to find_compatible_pertrans, to see if we can at least reuse
 * the state value of another aggregate.
 */
static int
find_compatible_peragg(Aggref *newagg, AggState *aggstate, int lastaggno, List **same_input_transnos)
{


























































}

/*
 * find_compatible_pertrans - search for a previously initialized per-Trans
 * struct
 *
 * Searches the list of transnos for a per-Trans struct with the same
 * transition function and initial condition. (The inputs have already been
 * verified to match.)
 */
static int
find_compatible_pertrans(AggState *aggstate, Aggref *newagg, bool shareable, Oid aggtransfn, Oid aggtranstype, Oid aggserialfn, Oid aggdeserialfn, Datum initValue, bool initValueIsNull, List *transnos)
{
















































}

void
ExecEndAgg(AggState *node)
{



















































}

void
ExecReScanAgg(AggState *node)
{















































































































}

/***********************************************************************
 * API exposed to aggregate functions
 ***********************************************************************/

/*
 * AggCheckCallContext - test if a SQL function is being called as an aggregate
 *
 * The transition and/or final functions of an aggregate may want to verify
 * that they are being called as aggregates, rather than as plain SQL
 * functions.  They should use this function to do so.  The return value
 * is nonzero if being called as an aggregate, or zero if not.  (Specific
 * nonzero values are AGG_CONTEXT_AGGREGATE or AGG_CONTEXT_WINDOW, but more
 * values could conceivably appear in future.)
 *
 * If aggcontext isn't NULL, the function also stores at *aggcontext the
 * identity of the memory context that aggregate transition values are being
 * stored in.  Note that the same aggregate call site (flinfo) may be called
 * interleaved on different transition values in different contexts, so it's
 * not kosher to cache aggcontext under fn_extra.  It is, however, kosher to
 * cache it in the transvalue itself (for internal-type transvalues).
 */
int
AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext)
{


























}

/*
 * AggGetAggref - allow an aggregate support function to get its Aggref
 *
 * If the function is being called as an aggregate support function,
 * return the Aggref node for the aggregate call.  Otherwise, return NULL.
 *
 * Aggregates sharing the same inputs and transition functions can get
 * merged into a single transition calculation.  If the transition function
 * calls AggGetAggref, it will get some one of the Aggrefs for which it is
 * executing.  It must therefore not pay attention to the Aggref fields that
 * relate to the final function, as those are indeterminate.  But if a final
 * function calls AggGetAggref, it will get a precise result.
 *
 * Note that if an aggregate is being used as a window function, this will
 * return NULL.  We could provide a similar function to return the relevant
 * WindowFunc node in such cases, but it's not needed yet.
 */
Aggref *
AggGetAggref(FunctionCallInfo fcinfo)
{























}

/*
 * AggGetTempMemoryContext - fetch short-term memory context for aggregates
 *
 * This is useful in agg final functions; the context returned is one that
 * the final function can safely reset as desired.  This isn't useful for
 * transition functions, since the context returned MAY (we don't promise)
 * be the same as the context those are called in.
 *
 * As above, this is currently not useful for aggs called as window functions.
 */
MemoryContext
AggGetTempMemoryContext(FunctionCallInfo fcinfo)
{







}

/*
 * AggStateIsShared - find out whether transition state is shared
 *
 * If the function is being called as an aggregate support function,
 * return true if the aggregate's transition state is shared across
 * multiple aggregates, false if it is not.
 *
 * Returns true if not called as an aggregate support function.
 * This is intended as a conservative answer, ie "no you'd better not
 * scribble on your input".  In particular, will return true if the
 * aggregate is being used as a window function, which is a scenario
 * in which changing the transition state is a bad idea.  We might
 * want to refine the behavior for the window case in future.
 */
bool
AggStateIsShared(FunctionCallInfo fcinfo)
{























}

/*
 * AggRegisterCallback - register a cleanup callback for an aggregate
 *
 * This is useful for aggs to register shutdown callbacks, which will ensure
 * that non-memory resources are freed.  The callback will occur just before
 * the associated aggcontext (as returned by AggCheckCallContext) is reset,
 * either between groups or as a result of rescanning the query.  The callback
 * will NOT be called on error paths.  The typical use-case is for freeing of
 * tuplestores or tuplesorts maintained in aggcontext, or pins held by slots
 * created by the agg functions.  (The callback will not be called until after
 * the result of the finalfn is no longer needed, so it's safe for the finalfn
 * to return data that will be freed by the callback.)
 *
 * As above, this is currently not useful for aggs called as window functions.
 */
void
AggRegisterCallback(FunctionCallInfo fcinfo, ExprContextCallbackFunction func, Datum arg)
{










}

/*
 * aggregate_dummy - dummy execution routine for aggregate functions
 *
 * This function is listed as the implementation (prosrc field) of pg_proc
 * entries for aggregate functions.  Its only purpose is to throw an error
 * if someone mistakenly executes such a function in the normal way.
 *
 * Perhaps someday we could assign real meaning to the prosrc field of
 * an aggregate?
 */
Datum
aggregate_dummy(PG_FUNCTION_ARGS)
{


}