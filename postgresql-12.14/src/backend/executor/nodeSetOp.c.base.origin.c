/*-------------------------------------------------------------------------
 *
 * nodeSetOp.c
 *	  Routines to handle INTERSECT and EXCEPT selection
 *
 * The input of a SetOp node consists of tuples from two relations,
 * which have been combined into one dataset, with a junk attribute added
 * that shows which relation each tuple came from.  In SETOP_SORTED mode,
 * the input has furthermore been sorted according to all the grouping
 * columns (ie, all the non-junk attributes).  The SetOp node scans each
 * group of identical tuples to determine how many came from each input
 * relation.  Then it is a simple matter to emit the output demanded by the
 * SQL spec for INTERSECT, INTERSECT ALL, EXCEPT, or EXCEPT ALL.
 *
 * In SETOP_HASHED mode, the input is delivered in no particular order,
 * except that we know all the tuples from one input relation will come before
 * all the tuples of the other.  The planner guarantees that the first input
 * relation is the left-hand one for EXCEPT, and tries to make the smaller
 * input relation come first for INTERSECT.  We build a hash table in memory
 * with one entry for each group of identical tuples, and count the number of
 * tuples in the group from each relation.  After seeing all the input, we
 * scan the hashtable and generate the correct output using those counts.
 * We can avoid making hashtable entries for any tuples appearing only in the
 * second input relation, since they cannot result in any output.
 *
 * This node type is not used for UNION or UNION ALL, since those can be
 * implemented more cheaply (there's no need for the junk attribute to
 * identify the source relation).
 *
 * Note that SetOp does no qual checking nor projection.  The delivered
 * output tuples are just copies of the first-to-arrive tuple in each
 * input group.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeSetOp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "executor/executor.h"
#include "executor/nodeSetOp.h"
#include "miscadmin.h"
#include "utils/memutils.h"

/*
 * SetOpStatePerGroupData - per-group working state
 *
 * These values are working state that is initialized at the start of
 * an input tuple group and updated for each input tuple.
 *
 * In SETOP_SORTED mode, we need only one of these structs, and it's kept in
 * the plan state node.  In SETOP_HASHED mode, the hash table contains one
 * of these for each tuple group.
 */
typedef struct SetOpStatePerGroupData
{
  long numLeft;  /* number of left-input dups in group */
  long numRight; /* number of right-input dups in group */
} SetOpStatePerGroupData;

static TupleTableSlot *
setop_retrieve_direct(SetOpState *setopstate);
static void
setop_fill_hash_table(SetOpState *setopstate);
static TupleTableSlot *
setop_retrieve_hash_table(SetOpState *setopstate);

/*
 * Initialize state for a new group of input values.
 */
static inline void
initialize_counts(SetOpStatePerGroup pergroup)
{

}

/*
 * Advance the appropriate counter for one input tuple.
 */
static inline void
advance_counts(SetOpStatePerGroup pergroup, int flag)
{








}

/*
 * Fetch the "flag" column from an input tuple.
 * This is an integer column with value 0 for left side, 1 for right side.
 */
static int
fetch_tuple_flag(SetOpState *setopstate, TupleTableSlot *inputslot)
{








}

/*
 * Initialize the hash table to empty.
 */
static void
build_hash_table(SetOpState *setopstate)
{








}

/*
 * We've completed processing a tuple group.  Decide how many copies (if any)
 * of its representative row to emit, and store the count into numOutput.
 * This logic is straight from the SQL92 specification.
 */
static void
set_output_count(SetOpState *setopstate, SetOpStatePerGroup pergroup)
{


































}

/* ----------------------------------------------------------------
 *		ExecSetOp
 * ----------------------------------------------------------------
 */
static TupleTableSlot * /* return: a tuple or NULL */
ExecSetOp(PlanState *pstate)
{



































}

/*
 * ExecSetOp for non-hashed case
 */
static TupleTableSlot *
setop_retrieve_direct(SetOpState *setopstate)
{




































































































}

/*
 * ExecSetOp for hashed case: phase 1, read input and build hash table
 */
static void
setop_fill_hash_table(SetOpState *setopstate)
{











































































}

/*
 * ExecSetOp for hashed case: phase 2, retrieving groups from hash table
 */
static TupleTableSlot *
setop_retrieve_hash_table(SetOpState *setopstate)
{










































}

/* ----------------------------------------------------------------
 *		ExecInitSetOp
 *
 *		This initializes the setop node state structures and
 *		the node's subplan.
 * ----------------------------------------------------------------
 */
SetOpState *
ExecInitSetOp(SetOp *node, EState *estate, int eflags)
{



















































































}

/* ----------------------------------------------------------------
 *		ExecEndSetOp
 *
 *		This shuts down the subplan and frees resources allocated
 *		to this node.
 * ----------------------------------------------------------------
 */
void
ExecEndSetOp(SetOpState *node)
{











}

void
ExecReScanSetOp(SetOpState *node)
{

























































}