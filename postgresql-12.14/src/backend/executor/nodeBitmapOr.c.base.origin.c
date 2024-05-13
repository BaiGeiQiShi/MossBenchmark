/*-------------------------------------------------------------------------
 *
 * nodeBitmapOr.c
 *	  routines to handle BitmapOr nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeBitmapOr.c
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitBitmapOr	- initialize the BitmapOr node
 *		MultiExecBitmapOr	- retrieve the result bitmap from the
 *node ExecEndBitmapOr		- shut down the BitmapOr node ExecReScanBitmapOr
 *- rescan the BitmapOr node
 *
 *	 NOTES
 *		BitmapOr nodes don't make use of their left and right
 *		subtrees, rather they maintain a list of subplans,
 *		much like Append nodes.  The logic is much simpler than
 *		Append, however, since we needn't cope with forward/backward
 *		execution.
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeBitmapOr.h"
#include "miscadmin.h"

/* ----------------------------------------------------------------
 *		ExecBitmapOr
 *
 *		stub for pro forma compliance
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecBitmapOr(PlanState *pstate)
{


}

/* ----------------------------------------------------------------
 *		ExecInitBitmapOr
 *
 *		Begin all of the subscans of the BitmapOr node.
 * ----------------------------------------------------------------
 */
BitmapOrState *
ExecInitBitmapOr(BitmapOr *node, EState *estate, int eflags)
{














































}

/* ----------------------------------------------------------------
 *	   MultiExecBitmapOr
 * ----------------------------------------------------------------
 */
Node *
MultiExecBitmapOr(BitmapOrState *node)
{


















































































}

/* ----------------------------------------------------------------
 *		ExecEndBitmapOr
 *
 *		Shuts down the subscans of the BitmapOr node.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndBitmapOr(BitmapOrState *node)
{




















}

void
ExecReScanBitmapOr(BitmapOrState *node)
{
























}