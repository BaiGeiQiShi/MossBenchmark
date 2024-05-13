/*-------------------------------------------------------------------------
 *
 * execAmi.c
 *	  miscellaneous executor access method routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/executor/execAmi.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/amapi.h"
#include "access/htup_details.h"
#include "executor/execdebug.h"
#include "executor/nodeAgg.h"
#include "executor/nodeAppend.h"
#include "executor/nodeBitmapAnd.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeBitmapOr.h"
#include "executor/nodeCtescan.h"
#include "executor/nodeCustom.h"
#include "executor/nodeForeignscan.h"
#include "executor/nodeFunctionscan.h"
#include "executor/nodeGather.h"
#include "executor/nodeGatherMerge.h"
#include "executor/nodeGroup.h"
#include "executor/nodeGroup.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeLimit.h"
#include "executor/nodeLockRows.h"
#include "executor/nodeMaterial.h"
#include "executor/nodeMergeAppend.h"
#include "executor/nodeMergejoin.h"
#include "executor/nodeModifyTable.h"
#include "executor/nodeNamedtuplestorescan.h"
#include "executor/nodeNestloop.h"
#include "executor/nodeProjectSet.h"
#include "executor/nodeRecursiveunion.h"
#include "executor/nodeResult.h"
#include "executor/nodeSamplescan.h"
#include "executor/nodeSeqscan.h"
#include "executor/nodeSetOp.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/nodeSubqueryscan.h"
#include "executor/nodeTableFuncscan.h"
#include "executor/nodeTidscan.h"
#include "executor/nodeUnique.h"
#include "executor/nodeValuesscan.h"
#include "executor/nodeWindowAgg.h"
#include "executor/nodeWorktablescan.h"
#include "nodes/extensible.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static bool
IndexSupportsBackwardScan(Oid indexid);

/*
 * ExecReScan
 *		Reset a plan node so that its output can be re-scanned.
 *
 * Note that if the plan node has parameters that have changed value,
 * the output might be different from last time.
 */
void
ExecReScan(PlanState *node)
{










































































































































































































































}

/*
 * ExecMarkPos
 *
 * Marks the current scan position.
 *
 * NOTE: mark/restore capability is currently needed only for plan nodes
 * that are the immediate inner child of a MergeJoin node.  Since MergeJoin
 * requires sorted input, there is never any need to support mark/restore in
 * node types that cannot produce sorted output.  There are some cases in
 * which a node can pass through sorted data from its child; if we don't
 * implement mark/restore for such a node type, the planner compensates by
 * inserting a Material node above that node.
 */
void
ExecMarkPos(PlanState *node)
{































}

/*
 * ExecRestrPos
 *
 * restores the scan position previously saved with ExecMarkPos()
 *
 * NOTE: the semantics of this are that the first ExecProcNode following
 * the restore operation will yield the same tuple as the first one following
 * the mark operation.  It is unspecified what happens to the plan node's
 * result TupleTableSlot.  (In most cases the result slot is unchanged by
 * a restore, but the node may choose to clear it or to load it with the
 * restored-to tuple.)	Hence the caller should discard any previously
 * returned TupleTableSlot after doing a restore.
 */
void
ExecRestrPos(PlanState *node)
{






























}

/*
 * ExecSupportsMarkRestore - does a Path support mark/restore?
 *
 * This is used during planning and so must accept a Path, not a Plan.
 * We keep it here to be adjacent to the routines above, which also must
 * know which plan types support mark/restore.
 */
bool
ExecSupportsMarkRestore(Path *pathnode)
{






























































































}

/*
 * ExecSupportsBackwardScan - does a plan type support backwards scanning?
 *
 * Ideally, all plan types would support backwards scan, but that seems
 * unlikely to happen soon.  In some cases, a plan node passes the backwards
 * scan down to its children, and so supports backwards scan only if its
 * children do.  Therefore, this routine must be passed a complete plan tree.
 */
bool
ExecSupportsBackwardScan(Plan *node)
{





















































































}

/*
 * An IndexScan or IndexOnlyScan node supports backward scan only if the
 * index's AM does.
 */
static bool
IndexSupportsBackwardScan(Oid indexid)
{






















}

/*
 * ExecMaterializesOutput - does a plan type materialize its output?
 *
 * Returns true if the plan node type is one that automatically materializes
 * its output (typically by keeping it in a tuplestore).  For such plans,
 * a rescan without any parameter change will have zero startup cost and
 * very low per-tuple cost.
 */
bool
ExecMaterializesOutput(NodeTag plantype)
{
















}