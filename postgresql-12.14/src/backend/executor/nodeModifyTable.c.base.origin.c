/*-------------------------------------------------------------------------
 *
 * nodeModifyTable.c
 *	  routines to handle ModifyTable nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeModifyTable.c
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitModifyTable - initialize the ModifyTable node
 *		ExecModifyTable		- retrieve the next tuple from the node
 *		ExecEndModifyTable	- shut down the ModifyTable node
 *		ExecReScanModifyTable - rescan the ModifyTable node
 *
 *	 NOTES
 *		Each ModifyTable node contains a list of one or more subplans,
 *		much like an Append node.  There is one subplan per result
 *relation. The key reason for this is that in an inherited UPDATE command, each
 *		result relation could have a different schema (more or different
 *		columns) requiring a different plan tree to produce it.  In an
 *		inherited DELETE, all the subplans should produce the same
 *output rowtype, but we might still find that different plans are appropriate
 *		for different child relations.
 *
 *		If the query specifies RETURNING, then the ModifyTable returns a
 *		RETURNING tuple after completing each row insert, update, or
 *delete. It must be called again to continue the operation.  Without RETURNING,
 *		we just loop within the node until all the work is done, then
 *		return NULL.  This avoids useless call/return overhead.
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "commands/trigger.h"
#include "executor/execPartition.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static bool
ExecOnConflictUpdate(ModifyTableState *mtstate, ResultRelInfo *resultRelInfo, ItemPointer conflictTid, TupleTableSlot *planSlot, TupleTableSlot *excludedSlot, EState *estate, bool canSetTag, TupleTableSlot **returning);
static TupleTableSlot *
ExecPrepareTupleRouting(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, ResultRelInfo *targetRelInfo, TupleTableSlot *slot);
static ResultRelInfo *
getTargetResultRelInfo(ModifyTableState *node);
static void
ExecSetupChildParentMapForSubplan(ModifyTableState *mtstate);
static TupleConversionMap *
tupconv_map_for_subplan(ModifyTableState *node, int whichplan);

/*
 * Verify that the tuples to be produced by INSERT or UPDATE match the
 * target relation's rowtype
 *
 * We do this to guard against stale plans.  If plan invalidation is
 * functioning properly then we should never get a failure here, but better
 * safe than sorry.  Note that this is called after we have obtained lock
 * on the target rel, so the rowtype can't change underneath us.
 *
 * The plan output is represented by its targetlist, because that makes
 * handling the dropped-column case easier.
 */
static void
ExecCheckPlanOutput(Relation resultRel, List *targetList)
{














































}

/*
 * ExecProcessReturning --- evaluate a RETURNING list
 *
 * projectReturning: the projection to evaluate
 * resultRelOid: result relation's OID
 * tupleSlot: slot holding tuple actually inserted/updated/deleted
 * planSlot: slot holding tuple returned by top subplan node
 *
 * In cross-partition UPDATE cases, projectReturning and planSlot are as
 * for the source partition, and tupleSlot must conform to that.  But
 * resultRelOid is for the destination partition.
 *
 * Note: If tupleSlot is NULL, the FDW should have already provided econtext's
 * scan tuple.
 *
 * Returns a slot holding the result tuple
 */
static TupleTableSlot *
ExecProcessReturning(ProjectionInfo *projectReturning, Oid resultRelOid, TupleTableSlot *tupleSlot, TupleTableSlot *planSlot)
{





















}

/*
 * ExecCheckTupleVisible -- verify tuple is visible
 *
 * It would not be consistent with guarantees of the higher isolation levels to
 * proceed with avoiding insertion (taking speculative insertion's alternative
 * path) on the basis of another tuple that is not visible to MVCC snapshot.
 * Check for the need to raise a serialization failure, and do so as necessary.
 */
static void
ExecCheckTupleVisible(EState *estate, Relation rel, TupleTableSlot *slot)
{


























}

/*
 * ExecCheckTIDVisible -- convenience variant of ExecCheckTupleVisible()
 */
static void
ExecCheckTIDVisible(EState *estate, ResultRelInfo *relinfo, ItemPointer tid, TupleTableSlot *tempSlot)
{














}

/*
 * Compute stored generated columns for a tuple
 */
void
ExecComputeStoredGenerated(EState *estate, TupleTableSlot *slot)
{



























































































}

/* ----------------------------------------------------------------
 *		ExecInsert
 *
 *		For INSERT, we have to insert the tuple into the target relation
 *		and insert appropriate tuples into the index relations.
 *
 *		slot contains the new tuple value to be stored.
 *		planSlot is the output of the ModifyTable's subplan; we use it
 *		to access "junk" columns that are not going to be stored.
 *		In a cross-partition UPDATE, srcSlot is the slot that held the
 *		updated tuple for the source relation; otherwise it's NULL.
 *
 *		returningRelInfo is the resultRelInfo for the source relation of
 *a cross-partition UPDATE; otherwise it's the current result relation. We use
 *it to process RETURNING lists, for reasons explained below.
 *
 *		Returns RETURNING result if any, otherwise NULL.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecInsert(ModifyTableState *mtstate, TupleTableSlot *slot, TupleTableSlot *planSlot, TupleTableSlot *srcSlot, ResultRelInfo *returningRelInfo, EState *estate, bool canSetTag)
{
















































































































































































































































































































































}

/* ----------------------------------------------------------------
 *		ExecDelete
 *
 *		DELETE is like UPDATE, except that we delete the tuple and no
 *		index modifications are needed.
 *
 *		When deleting from a table, tupleid identifies the tuple to
 *		delete and oldtuple is NULL.  When deleting from a view,
 *		oldtuple is passed to the INSTEAD OF triggers and identifies
 *		what to delete, and tupleid is invalid.  When deleting from a
 *		foreign table, tupleid is invalid; the FDW has to figure out
 *		which row to delete using data from the planSlot.  oldtuple is
 *		passed to foreign table triggers; it is NULL when the foreign
 *		table has no relevant triggers.  We use tupleDeleted to indicate
 *		whether the tuple is actually deleted, callers can use it to
 *		decide whether to continue the operation.  When this DELETE is a
 *		part of an UPDATE of partition-key, then the slot returned by
 *		EvalPlanQual() is passed back using output parameter epqslot.
 *
 *		Returns RETURNING result if any, otherwise NULL.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecDelete(ModifyTableState *mtstate, ItemPointer tupleid, HeapTuple oldtuple, TupleTableSlot *planSlot, EPQState *epqstate, EState *estate, bool processReturning, bool canSetTag, bool changingPart, bool *tupleDeleted, TupleTableSlot **epqreturnslot)
{





















































































































































































































































































































}

/* ----------------------------------------------------------------
 *		ExecUpdate
 *
 *		note: we can't run UPDATE queries with transactions
 *		off because UPDATEs are actually INSERTs and our
 *		scan will mistakenly loop forever, updating the tuple
 *		it just inserted..  This should be fixed but until it
 *		is, we don't want to get stuck in an infinite loop
 *		which corrupts your database..
 *
 *		When updating a table, tupleid identifies the tuple to
 *		update and oldtuple is NULL.  When updating a view, oldtuple
 *		is passed to the INSTEAD OF triggers and identifies what to
 *		update, and tupleid is invalid.  When updating a foreign table,
 *		tupleid is invalid; the FDW has to figure out which row to
 *		update using data from the planSlot.  oldtuple is passed to
 *		foreign table triggers; it is NULL when the foreign table has
 *		no relevant triggers.
 *
 *		Returns RETURNING result if any, otherwise NULL.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecUpdate(ModifyTableState *mtstate, ItemPointer tupleid, HeapTuple oldtuple, TupleTableSlot *slot, TupleTableSlot *planSlot, EPQState *epqstate, EState *estate, bool canSetTag)
{












































































































































































































































































































































































































































}

/*
 * ExecOnConflictUpdate --- execute UPDATE of INSERT ON CONFLICT DO UPDATE
 *
 * Try to lock tuple for update as part of speculative insertion.  If
 * a qual originating from ON CONFLICT DO UPDATE is satisfied, update
 * (but still lock row, even though it may not satisfy estate's
 * snapshot).
 *
 * Returns true if we're done (with or without an update), or false if
 * the caller must retry the INSERT from scratch.
 */
static bool
ExecOnConflictUpdate(ModifyTableState *mtstate, ResultRelInfo *resultRelInfo, ItemPointer conflictTid, TupleTableSlot *planSlot, TupleTableSlot *excludedSlot, EState *estate, bool canSetTag, TupleTableSlot **returning)
{
























































































































































































}

/*
 * Process BEFORE EACH STATEMENT triggers
 */
static void
fireBSTriggers(ModifyTableState *node)
{
































}

/*
 * Return the target rel ResultRelInfo.
 *
 * This relation is the same as :
 * - the relation for which we will fire AFTER STATEMENT triggers.
 * - the relation into whose tuple format all captured transition tuples must
 *   be converted.
 * - the root partitioned table.
 */
static ResultRelInfo *
getTargetResultRelInfo(ModifyTableState *node)
{












}

/*
 * Process AFTER EACH STATEMENT triggers
 */
static void
fireASTriggers(ModifyTableState *node)
{






















}

/*
 * Set up the state needed for collecting transition tuples for AFTER
 * triggers.
 */
static void
ExecSetupTransitionCaptureState(ModifyTableState *mtstate, EState *estate)
{































}

/*
 * ExecPrepareTupleRouting --- prepare for routing one tuple
 *
 * Determine the partition in which the tuple in slot is to be inserted,
 * and modify mtstate and estate to prepare for it.
 *
 * Caller must revert the estate changes after executing the insertion!
 * In mtstate, transition capture changes may also need to be reverted.
 *
 * Returns a slot holding the tuple of the partition rowtype.
 */
static TupleTableSlot *
ExecPrepareTupleRouting(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, ResultRelInfo *targetRelInfo, TupleTableSlot *slot)
{






























































}

/*
 * Initialize the child-to-root tuple conversion map array for UPDATE subplans.
 *
 * This map array is required to convert the tuple from the subplan result rel
 * to the target table descriptor. This requirement arises for two independent
 * scenarios:
 * 1. For update-tuple-routing.
 * 2. For capturing tuples in transition tables.
 */
static void
ExecSetupChildParentMapForSubplan(ModifyTableState *mtstate)
{





















}

/*
 * For a given subplan index, get the tuple conversion map.
 */
static TupleConversionMap *
tupconv_map_for_subplan(ModifyTableState *mtstate, int whichplan)
{








}

/* ----------------------------------------------------------------
 *	   ExecModifyTable
 *
 *		Perform table modifications as required, and return RETURNING
 *results if needed.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecModifyTable(PlanState *pstate)
{



















































































































































































































































































}

/* ----------------------------------------------------------------
 *		ExecInitModifyTable
 * ----------------------------------------------------------------
 */
ModifyTableState *
ExecInitModifyTable(ModifyTable *node, EState *estate, int eflags)
{
















































































































































































































































































































































































































































































}

/* ----------------------------------------------------------------
 *		ExecEndModifyTable
 *
 *		Shuts down the plan.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndModifyTable(ModifyTableState *node)
{






















































}

void
ExecReScanModifyTable(ModifyTableState *node)
{





}