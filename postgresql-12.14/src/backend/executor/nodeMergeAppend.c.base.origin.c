/*-------------------------------------------------------------------------
 *
 * nodeMergeAppend.c
 *	  routines to handle MergeAppend nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeMergeAppend.c
 *
 *-------------------------------------------------------------------------
 */
/* INTERFACE ROUTINES
 *		ExecInitMergeAppend		- initialize the MergeAppend
 *node ExecMergeAppend			- retrieve the next tuple from the node
 *		ExecEndMergeAppend		- shut down the MergeAppend node
 *		ExecReScanMergeAppend	- rescan the MergeAppend node
 *
 *	 NOTES
 *		A MergeAppend node contains a list of one or more subplans.
 *		These are each expected to deliver tuples that are sorted
 *according to a common sort key.  The MergeAppend node merges these streams to
 *produce output sorted the same way.
 *
 *		MergeAppend nodes don't make use of their left and right
 *		subtrees, rather they maintain a list of subplans so
 *		a typical MergeAppend node looks like this in the plan tree:
 *
 *				   ...
 *				   /
 *				MergeAppend---+------+------+--- nil
 *				/	\		  |		 |
 *| nil	nil		 ...    ...    ... subplans
 */

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/execPartition.h"
#include "executor/nodeMergeAppend.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"

/*
 * We have one slot for each item in the heap array.  We use SlotNumber
 * to store slot indexes.  This doesn't actually provide any formal
 * type-safety, but it makes the code more self-documenting.
 */
typedef int32 SlotNumber;

static TupleTableSlot *
ExecMergeAppend(PlanState *pstate);
static int
heap_compare_slots(Datum a, Datum b, void *arg);

/* ----------------------------------------------------------------
 *		ExecInitMergeAppend
 *
 *		Begin all of the subscans of the MergeAppend node.
 * ----------------------------------------------------------------
 */
MergeAppendState *
ExecInitMergeAppend(MergeAppend *node, EState *estate, int eflags)
{



























































































































































}

/* ----------------------------------------------------------------
 *	   ExecMergeAppend
 *
 *		Handles iteration over multiple subplans.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecMergeAppend(PlanState *pstate)
{










































































}

/*
 * Compare the tuples in the two given slots.
 */
static int32
heap_compare_slots(Datum a, Datum b, void *arg)
{






























}

/* ----------------------------------------------------------------
 *		ExecEndMergeAppend
 *
 *		Shuts down the subscans of the MergeAppend node.
 *
 *		Returns nothing of interest.
 * ----------------------------------------------------------------
 */
void
ExecEndMergeAppend(MergeAppendState *node)
{

















}

void
ExecReScanMergeAppend(MergeAppendState *node)
{





































}