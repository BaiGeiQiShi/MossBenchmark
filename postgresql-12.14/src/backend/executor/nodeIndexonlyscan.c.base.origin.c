/*-------------------------------------------------------------------------
 *
 * nodeIndexonlyscan.c
 *	  Routines to support index-only scans
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeIndexonlyscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecIndexOnlyScan			scans an index
 *		IndexOnlyNext				retrieve next tuple
 *		ExecInitIndexOnlyScan		creates and initializes state
 *info. ExecReScanIndexOnlyScan		rescans the indexed relation.
 *		ExecEndIndexOnlyScan		releases all storage.
 *		ExecIndexOnlyMarkPos		marks scan position.
 *		ExecIndexOnlyRestrPos		restores scan position.
 *		ExecIndexOnlyScanEstimate	estimates DSM space needed for
 *						parallel index-only scan
 *		ExecIndexOnlyScanInitializeDSM	initialize DSM for parallel
 *						index-only scan
 *		ExecIndexOnlyScanReInitializeDSM	reinitialize DSM for
 *fresh scan ExecIndexOnlyScanInitializeWorker attach to DSM info in parallel
 *worker
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/tupdesc.h"
#include "access/visibilitymap.h"
#include "executor/execdebug.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeIndexscan.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
IndexOnlyNext(IndexOnlyScanState *node);
static void
StoreIndexTuple(TupleTableSlot *slot, IndexTuple itup, TupleDesc itupdesc);

/* ----------------------------------------------------------------
 *		IndexOnlyNext
 *
 *		Retrieve a tuple from the IndexOnlyScan node's index.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
IndexOnlyNext(IndexOnlyScanState *node)
{







































































































































































































}

/*
 * StoreIndexTuple
 *		Fill the slot with data from the index tuple.
 *
 * At some point this might be generally-useful functionality, but
 * right now we don't need it elsewhere.
 */
static void
StoreIndexTuple(TupleTableSlot *slot, IndexTuple itup, TupleDesc itupdesc)
{












}

/*
 * IndexOnlyRecheck -- access method routine to recheck a tuple in EvalPlanQual
 *
 * This can't really happen, since an index can't supply CTID which would
 * be necessary data for any potential EvalPlanQual target relation.  If it
 * did happen, the EPQ code would pass us the wrong data, namely a heap
 * tuple not an index tuple.  So throw an error.
 */
static bool
IndexOnlyRecheck(IndexOnlyScanState *node, TupleTableSlot *slot)
{


}

/* ----------------------------------------------------------------
 *		ExecIndexOnlyScan(node)
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecIndexOnlyScan(PlanState *pstate)
{











}

/* ----------------------------------------------------------------
 *		ExecReScanIndexOnlyScan(node)
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
ExecReScanIndexOnlyScan(IndexOnlyScanState *node)
{























}

/* ----------------------------------------------------------------
 *		ExecEndIndexOnlyScan
 * ----------------------------------------------------------------
 */
void
ExecEndIndexOnlyScan(IndexOnlyScanState *node)
{















































}

/* ----------------------------------------------------------------
 *		ExecIndexOnlyMarkPos
 *
 * Note: we assume that no caller attempts to set a mark before having read
 * at least one tuple.  Otherwise, ioss_ScanDesc might still be NULL.
 * ----------------------------------------------------------------
 */
void
ExecIndexOnlyMarkPos(IndexOnlyScanState *node)
{





























}

/* ----------------------------------------------------------------
 *		ExecIndexOnlyRestrPos
 * ----------------------------------------------------------------
 */
void
ExecIndexOnlyRestrPos(IndexOnlyScanState *node)
{





















}

/* ----------------------------------------------------------------
 *		ExecInitIndexOnlyScan
 *
 *		Initializes the index scan's state information, creates
 *		scan keys, and opens the base and index relations.
 *
 *		Note: index scans have 2 sets of state information because
 *			  we have to keep track of the base relation and the
 *			  index relation.
 * ----------------------------------------------------------------
 */
IndexOnlyScanState *
ExecInitIndexOnlyScan(IndexOnlyScan *node, EState *estate, int eflags)
{




















































































































}

/* ----------------------------------------------------------------
 *		Parallel Index-only Scan Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecIndexOnlyScanEstimate
 *
 *		Compute the amount of space we'll need in the parallel
 *		query DSM, and inform pcxt->estimator about our needs.
 * ----------------------------------------------------------------
 */
void
ExecIndexOnlyScanEstimate(IndexOnlyScanState *node, ParallelContext *pcxt)
{





}

/* ----------------------------------------------------------------
 *		ExecIndexOnlyScanInitializeDSM
 *
 *		Set up a parallel index-only scan descriptor.
 * ----------------------------------------------------------------
 */
void
ExecIndexOnlyScanInitializeDSM(IndexOnlyScanState *node, ParallelContext *pcxt)
{


















}

/* ----------------------------------------------------------------
 *		ExecIndexOnlyScanReInitializeDSM
 *
 *		Reset shared state before beginning a fresh scan.
 * ----------------------------------------------------------------
 */
void
ExecIndexOnlyScanReInitializeDSM(IndexOnlyScanState *node, ParallelContext *pcxt)
{

}

/* ----------------------------------------------------------------
 *		ExecIndexOnlyScanInitializeWorker
 *
 *		Copy relevant information from TOC into planstate.
 * ----------------------------------------------------------------
 */
void
ExecIndexOnlyScanInitializeWorker(IndexOnlyScanState *node, ParallelWorkerContext *pwcxt)
{














}