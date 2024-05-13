/*-------------------------------------------------------------------------
 *
 * nodeIndexscan.c
 *	  Routines to support indexed scans of relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeIndexscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecIndexScan			scans a relation using an index
 *		IndexNext				retrieve next tuple
 *using index IndexNextWithReorder	same, but recheck ORDER BY expressions
 *		ExecInitIndexScan		creates and initializes state
 *info. ExecReScanIndexScan		rescans the indexed relation.
 *		ExecEndIndexScan		releases all storage.
 *		ExecIndexMarkPos		marks scan position.
 *		ExecIndexRestrPos		restores scan position.
 *		ExecIndexScanEstimate	estimates DSM space needed for parallel
 *index scan ExecIndexScanInitializeDSM initialize DSM for parallel indexscan
 *		ExecIndexScanReInitializeDSM reinitialize DSM for fresh scan
 *		ExecIndexScanInitializeWorker attach to DSM info in parallel
 *worker
 */
#include "postgres.h"

#include "access/nbtree.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "catalog/pg_am.h"
#include "executor/execdebug.h"
#include "executor/nodeIndexscan.h"
#include "lib/pairingheap.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/array.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * When an ordering operator is used, tuples fetched from the index that
 * need to be reordered are queued in a pairing heap, as ReorderTuples.
 */
typedef struct
{
  pairingheap_node ph_node;
  HeapTuple htup;
  Datum *orderbyvals;
  bool *orderbynulls;
} ReorderTuple;

static TupleTableSlot *
IndexNext(IndexScanState *node);
static TupleTableSlot *
IndexNextWithReorder(IndexScanState *node);
static void
EvalOrderByExpressions(IndexScanState *node, ExprContext *econtext);
static bool
IndexRecheck(IndexScanState *node, TupleTableSlot *slot);
static int
cmp_orderbyvals(const Datum *adist, const bool *anulls, const Datum *bdist, const bool *bnulls, IndexScanState *node);
static int
reorderqueue_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg);
static void
reorderqueue_push(IndexScanState *node, TupleTableSlot *slot, Datum *orderbyvals, bool *orderbynulls);
static HeapTuple
reorderqueue_pop(IndexScanState *node);

/* ----------------------------------------------------------------
 *		IndexNext
 *
 *		Retrieve a tuple from the IndexScan node's currentRelation
 *		using the index specified in the IndexScanState information.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
IndexNext(IndexScanState *node)
{














































































}

/* ----------------------------------------------------------------
 *		IndexNextWithReorder
 *
 *		Like IndexNext, but this version can also re-check ORDER BY
 *		expressions, and reorder the tuples as necessary.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
IndexNextWithReorder(IndexScanState *node)
{













































































































































































}

/*
 * Calculate the expressions in the ORDER BY clause, based on the heap tuple.
 */
static void
EvalOrderByExpressions(IndexScanState *node, ExprContext *econtext)
{
















}

/*
 * IndexRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
IndexRecheck(IndexScanState *node, TupleTableSlot *slot)
{










}

/*
 * Compare ORDER BY expression values.
 */
static int
cmp_orderbyvals(const Datum *adist, const bool *anulls, const Datum *bdist, const bool *bnulls, IndexScanState *node)
{

































}

/*
 * Pairing heap provides getting topmost (greatest) element while KNN provides
 * ascending sort.  That's why we invert the sort order.
 */
static int
reorderqueue_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{






}

/*
 * Helper function to push a tuple to the reorder queue.
 */
static void
reorderqueue_push(IndexScanState *node, TupleTableSlot *slot, Datum *orderbyvals, bool *orderbynulls)
{

























}

/*
 * Helper function to pop the next tuple from the reorder queue.
 */
static HeapTuple
reorderqueue_pop(IndexScanState *node)
{



















}

/* ----------------------------------------------------------------
 *		ExecIndexScan(node)
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecIndexScan(PlanState *pstate)
{


















}

/* ----------------------------------------------------------------
 *		ExecReScanIndexScan(node)
 *
 *		Recalculates the values of any scan keys whose value depends on
 *		information known at runtime, then rescans the indexed relation.
 *
 *		Updating the scan key was formerly done separately in
 *		ExecUpdateIndexScanKeys. Integrating it into ReScan makes
 *		rescans of indices and relations/general streams more uniform.
 * ----------------------------------------------------------------
 */
void
ExecReScanIndexScan(IndexScanState *node)
{



































}

/*
 * ExecIndexEvalRuntimeKeys
 *		Evaluate any runtime key values, and update the scankeys.
 */
void
ExecIndexEvalRuntimeKeys(ExprContext *econtext, IndexRuntimeKeyInfo *runtimeKeys, int numRuntimeKeys)
{
















































}

/*
 * ExecIndexEvalArrayKeys
 *		Evaluate any array key values, and set up to iterate through
 *arrays.
 *
 * Returns true if there are array elements to consider; false means there
 * is at least one null or empty array, so no match is possible.  On true
 * result, the scankeys are initialized with the first elements of the arrays.
 */
bool
ExecIndexEvalArrayKeys(ExprContext *econtext, IndexArrayKeyInfo *arrayKeys, int numArrayKeys)
{
































































}

/*
 * ExecIndexAdvanceArrayKeys
 *		Advance to the next set of array key values, if any.
 *
 * Returns true if there is another set of values to consider, false if not.
 * On true result, the scankeys are initialized with the next set of values.
 */
bool
ExecIndexAdvanceArrayKeys(IndexArrayKeyInfo *arrayKeys, int numArrayKeys)
{











































}

/* ----------------------------------------------------------------
 *		ExecEndIndexScan
 * ----------------------------------------------------------------
 */
void
ExecEndIndexScan(IndexScanState *node)
{








































}

/* ----------------------------------------------------------------
 *		ExecIndexMarkPos
 *
 * Note: we assume that no caller attempts to set a mark before having read
 * at least one tuple.  Otherwise, iss_ScanDesc might still be NULL.
 * ----------------------------------------------------------------
 */
void
ExecIndexMarkPos(IndexScanState *node)
{





























}

/* ----------------------------------------------------------------
 *		ExecIndexRestrPos
 * ----------------------------------------------------------------
 */
void
ExecIndexRestrPos(IndexScanState *node)
{





















}

/* ----------------------------------------------------------------
 *		ExecInitIndexScan
 *
 *		Initializes the index scan's state information, creates
 *		scan keys, and opens the base and index relations.
 *
 *		Note: index scans have 2 sets of state information because
 *			  we have to keep track of the base relation and the
 *			  index relation.
 * ----------------------------------------------------------------
 */
IndexScanState *
ExecInitIndexScan(IndexScan *node, EState *estate, int eflags)
{





























































































































































}

/*
 * ExecIndexBuildScanKeys
 *		Build the index scan keys from the index qualification
 *expressions
 *
 * The index quals are passed to the index AM in the form of a ScanKey array.
 * This routine sets up the ScanKeys, fills in all constant fields of the
 * ScanKeys, and prepares information about the keys that have non-constant
 * comparison values.  We divide index qual expressions into five types:
 *
 * 1. Simple operator with constant comparison value ("indexkey op constant").
 * For these, we just fill in a ScanKey containing the constant value.
 *
 * 2. Simple operator with non-constant value ("indexkey op expression").
 * For these, we create a ScanKey with everything filled in except the
 * expression value, and set up an IndexRuntimeKeyInfo struct to drive
 * evaluation of the expression at the right times.
 *
 * 3. RowCompareExpr ("(indexkey, indexkey, ...) op (expr, expr, ...)").
 * For these, we create a header ScanKey plus a subsidiary ScanKey array,
 * as specified in access/skey.h.  The elements of the row comparison
 * can have either constant or non-constant comparison values.
 *
 * 4. ScalarArrayOpExpr ("indexkey op ANY (array-expression)").  If the index
 * supports amsearcharray, we handle these the same as simple operators,
 * setting the SK_SEARCHARRAY flag to tell the AM to handle them.  Otherwise,
 * we create a ScanKey with everything filled in except the comparison value,
 * and set up an IndexArrayKeyInfo struct to drive processing of the qual.
 * (Note that if we use an IndexArrayKeyInfo struct, the array expression is
 * always treated as requiring runtime evaluation, even if it's a constant.)
 *
 * 5. NullTest ("indexkey IS NULL/IS NOT NULL").  We just fill in the
 * ScanKey properly.
 *
 * This code is also used to prepare ORDER BY expressions for amcanorderbyop
 * indexes.  The behavior is exactly the same, except that we have to look up
 * the operator differently.  Note that only cases 1 and 2 are currently
 * possible for ORDER BY.
 *
 * Input params are:
 *
 * planstate: executor state node we are working for
 * index: the index we are building scan keys for
 * quals: indexquals (or indexorderbys) expressions
 * isorderby: true if processing ORDER BY exprs, false if processing quals
 * *runtimeKeys: ptr to pre-existing IndexRuntimeKeyInfos, or NULL if none
 * *numRuntimeKeys: number of pre-existing runtime keys
 *
 * Output params are:
 *
 * *scanKeys: receives ptr to array of ScanKeys
 * *numScanKeys: receives number of scankeys
 * *runtimeKeys: receives ptr to array of IndexRuntimeKeyInfos, or NULL if none
 * *numRuntimeKeys: receives number of runtime keys
 * *arrayKeys: receives ptr to array of IndexArrayKeyInfos, or NULL if none
 * *numArrayKeys: receives number of array keys
 *
 * Caller may pass NULL for arrayKeys and numArrayKeys to indicate that
 * IndexArrayKeyInfos are not supported.
 */
void
ExecIndexBuildScanKeys(PlanState *planstate, Relation index, List *quals, bool isorderby, ScanKey *scanKeys, int *numScanKeys, IndexRuntimeKeyInfo **runtimeKeys, int *numRuntimeKeys, IndexArrayKeyInfo **arrayKeys, int *numArrayKeys)
{






































































































































































































































































































































































































































































































}

/* ----------------------------------------------------------------
 *						Parallel Scan Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecIndexScanEstimate
 *
 *		Compute the amount of space we'll need in the parallel
 *		query DSM, and inform pcxt->estimator about our needs.
 * ----------------------------------------------------------------
 */
void
ExecIndexScanEstimate(IndexScanState *node, ParallelContext *pcxt)
{





}

/* ----------------------------------------------------------------
 *		ExecIndexScanInitializeDSM
 *
 *		Set up a parallel index scan descriptor.
 * ----------------------------------------------------------------
 */
void
ExecIndexScanInitializeDSM(IndexScanState *node, ParallelContext *pcxt)
{
















}

/* ----------------------------------------------------------------
 *		ExecIndexScanReInitializeDSM
 *
 *		Reset shared state before beginning a fresh scan.
 * ----------------------------------------------------------------
 */
void
ExecIndexScanReInitializeDSM(IndexScanState *node, ParallelContext *pcxt)
{

}

/* ----------------------------------------------------------------
 *		ExecIndexScanInitializeWorker
 *
 *		Copy relevant information from TOC into planstate.
 * ----------------------------------------------------------------
 */
void
ExecIndexScanInitializeWorker(IndexScanState *node, ParallelWorkerContext *pwcxt)
{













}