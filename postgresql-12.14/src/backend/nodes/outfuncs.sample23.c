/*-------------------------------------------------------------------------
 *
 * outfuncs.c
 *	  Output functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/outfuncs.c
 *
 * NOTES
 *	  Every node type that can appear in stored rules' parsetrees *must*
 *	  have an output function defined here (as well as an input function
 *	  in readfuncs.c).  In addition, plan nodes should have input and
 *	  output functions so that they can be sent to parallel workers.
 *
 *	  For use in debugging, we also provide output functions for nodes
 *	  that appear in raw parsetrees and planner Paths.  These node types
 *	  need not have input functions.  Output support for raw parsetrees
 *	  is somewhat incomplete, too; in particular, utility statements are
 *	  almost entirely unsupported.  We try to support everything that can
 *	  appear in a raw SELECT, though.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "nodes/extensible.h"
#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"
#include "utils/datum.h"
#include "utils/rel.h"

static void
outChar(StringInfo str, char c);

/*
 * Macros to simplify output of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.  Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) appendStringInfoString(str, nodelabel)

/* Write an integer field (anything written as ":fldname %d") */
#define WRITE_INT_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write an unsigned integer field (anything written as ":fldname %u") */
#define WRITE_UINT_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write an unsigned integer field (anything written with UINT64_FORMAT) */
#define WRITE_UINT64_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " " UINT64_FORMAT, node->fldname)

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " %ld", node->fldname)

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) (appendStringInfo(str, " :" CppAsString(fldname) " "), outChar(str, node->fldname))

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) appendStringInfo(str, " :" CppAsString(fldname) " %d", (int)node->fldname)

/* Write a float field --- caller must give format to define precision */
#define WRITE_FLOAT_FIELD(fldname, format) appendStringInfo(str, " :" CppAsString(fldname) " " format, node->fldname)

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " %s", booltostr(node->fldname))

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname) (appendStringInfoString(str, " :" CppAsString(fldname) " "), outToken(str, node->fldname))

/* Write a parse location field (actually same as INT case) */
#define WRITE_LOCATION_FIELD(fldname) appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) (appendStringInfoString(str, " :" CppAsString(fldname) " "), outNode(str, node->fldname))

/* Write a bitmapset field */
#define WRITE_BITMAPSET_FIELD(fldname) (appendStringInfoString(str, " :" CppAsString(fldname) " "), outBitmapset(str, node->fldname))

#define WRITE_ATTRNUMBER_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    appendStringInfoString(str, " :" CppAsString(fldname) " ");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    for (int i = 0; i < len; i++)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      appendStringInfo(str, " %d", node->fldname[i]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#define WRITE_OID_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    appendStringInfoString(str, " :" CppAsString(fldname) " ");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    for (int i = 0; i < len; i++)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      appendStringInfo(str, " %u", node->fldname[i]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#define WRITE_INT_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    appendStringInfoString(str, " :" CppAsString(fldname) " ");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    for (int i = 0; i < len; i++)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      appendStringInfo(str, " %d", node->fldname[i]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#define WRITE_BOOL_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    appendStringInfoString(str, " :" CppAsString(fldname) " ");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    for (int i = 0; i < len; i++)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      appendStringInfo(str, " %s", booltostr(node->fldname[i]));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

#define booltostr(x) ((x) ? "true" : "false")

/*
 * outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
void
outToken(StringInfo str, const char *s)
{
  if (s == NULL || *s == '\0')
  {
    appendStringInfoString(str, "<>");
    return;
  }

  /*
   * Look for characters or patterns that are treated specially by read.c
   * (either in pg_strtok() or in nodeRead()), and therefore need a
   * protective backslash.
   */
  /* These characters only need to be quoted at the start of the string */
  if (*s == '<' || *s == '"' || isdigit((unsigned char)*s) || ((*s == '+' || *s == '-') && (isdigit((unsigned char)s[1]) || s[1] == '.')))
  {

  }
  while (*s)
  {
    /* These chars must be backslashed anywhere in the string */
    if (*s == ' ' || *s == '\n' || *s == '\t' || *s == '(' || *s == ')' || *s == '{' || *s == '}' || *s == '\\')
    {
      appendStringInfoChar(str, '\\');
    }
    appendStringInfoChar(str, *s++);
  }
}

/*
 * Convert one char.  Goes through outToken() so that special characters are
 * escaped.
 */
static void
outChar(StringInfo str, char c)
{
  char in[2];

  in[0] = c;
  in[1] = '\0';

  outToken(str, in);
}

static void
_outList(StringInfo str, const List *node)
{
  const ListCell *lc;

  appendStringInfoChar(str, '(');

  if (IsA(node, IntList))
  {
    appendStringInfoChar(str, 'i');
  }
  else if (IsA(node, OidList))
  {
    appendStringInfoChar(str, 'o');
  }

  foreach (lc, node)
  {
    /*
     * For the sake of backward compatibility, we emit a slightly
     * different whitespace format for lists of nodes vs. other types of
     * lists. XXX: is this necessary?
     */
    if (IsA(node, List))
    {
      outNode(str, lfirst(lc));
      if (lnext(lc))
      {
        appendStringInfoChar(str, ' ');
      }
    }
    else if (IsA(node, IntList))
    {
      appendStringInfo(str, " %d", lfirst_int(lc));
    }
    else if (IsA(node, OidList))
    {
      appendStringInfo(str, " %u", lfirst_oid(lc));
    }
    else
    {

    }
  }

  appendStringInfoChar(str, ')');
}

/*
 * outBitmapset -
 *	   converts a bitmap set of integers
 *
 * Note: the output format is "(b int int ...)", similar to an integer List.
 */
void
outBitmapset(StringInfo str, const Bitmapset *bms)
{
  int x;

  appendStringInfoChar(str, '(');
  appendStringInfoChar(str, 'b');
  x = -1;
  while ((x = bms_next_member(bms, x)) >= 0)
  {
    appendStringInfo(str, " %d", x);
  }
  appendStringInfoChar(str, ')');
}

/*
 * Print the value of a Datum given its type.
 */
void
outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
  Size length, i;
  char *s;

  length = datumGetSize(value, typbyval, typlen);

  if (typbyval)
  {
    s = (char *)(&value);
    appendStringInfo(str, "%u [ ", (unsigned int)length);
    for (i = 0; i < (Size)sizeof(Datum); i++)
    {
      appendStringInfo(str, "%d ", (int)(s[i]));
    }
    appendStringInfoChar(str, ']');
  }
  else
  {
    s = (char *)DatumGetPointer(value);
    if (!PointerIsValid(s))
    {

    }
    else
    {
      appendStringInfo(str, "%u [ ", (unsigned int)length);
      for (i = 0; i < length; i++)
      {
        appendStringInfo(str, "%d ", (int)(s[i]));
      }
      appendStringInfoChar(str, ']');
    }
  }
}

/*
 *	Stuff from plannodes.h
 */

static void
_outPlannedStmt(StringInfo str, const PlannedStmt *node)
{
  WRITE_NODE_TYPE("PLANNEDSTMT");

  WRITE_ENUM_FIELD(commandType, CmdType);
  WRITE_UINT64_FIELD(queryId);
  WRITE_BOOL_FIELD(hasReturning);
  WRITE_BOOL_FIELD(hasModifyingCTE);
  WRITE_BOOL_FIELD(canSetTag);
  WRITE_BOOL_FIELD(transientPlan);
  WRITE_BOOL_FIELD(dependsOnRole);
  WRITE_BOOL_FIELD(parallelModeNeeded);
  WRITE_INT_FIELD(jitFlags);
  WRITE_NODE_FIELD(planTree);
  WRITE_NODE_FIELD(rtable);
  WRITE_NODE_FIELD(resultRelations);
  WRITE_NODE_FIELD(rootResultRelations);
  WRITE_NODE_FIELD(subplans);
  WRITE_BITMAPSET_FIELD(rewindPlanIDs);
  WRITE_NODE_FIELD(rowMarks);
  WRITE_NODE_FIELD(relationOids);
  WRITE_NODE_FIELD(invalItems);
  WRITE_NODE_FIELD(paramExecTypes);
  WRITE_NODE_FIELD(utilityStmt);
  WRITE_LOCATION_FIELD(stmt_location);
  WRITE_LOCATION_FIELD(stmt_len);
}

/*
 * print the basic stuff of all nodes that inherit from Plan
 */
static void
_outPlanInfo(StringInfo str, const Plan *node)
{
  WRITE_FLOAT_FIELD(startup_cost, "%.2f");
  WRITE_FLOAT_FIELD(total_cost, "%.2f");
  WRITE_FLOAT_FIELD(plan_rows, "%.0f");
  WRITE_INT_FIELD(plan_width);
  WRITE_BOOL_FIELD(parallel_aware);
  WRITE_BOOL_FIELD(parallel_safe);
  WRITE_INT_FIELD(plan_node_id);
  WRITE_NODE_FIELD(targetlist);
  WRITE_NODE_FIELD(qual);
  WRITE_NODE_FIELD(lefttree);
  WRITE_NODE_FIELD(righttree);
  WRITE_NODE_FIELD(initPlan);
  WRITE_BITMAPSET_FIELD(extParam);
  WRITE_BITMAPSET_FIELD(allParam);
}

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_outScanInfo(StringInfo str, const Scan *node)
{
  _outPlanInfo(str, (const Plan *)node);

  WRITE_UINT_FIELD(scanrelid);
}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_outJoinPlanInfo(StringInfo str, const Join *node)
{
  _outPlanInfo(str, (const Plan *)node);

  WRITE_ENUM_FIELD(jointype, JoinType);
  WRITE_BOOL_FIELD(inner_unique);
  WRITE_NODE_FIELD(joinqual);
}

static void
_outPlan(StringInfo str, const Plan *node)
{



}

static void
_outResult(StringInfo str, const Result *node)
{
  WRITE_NODE_TYPE("RESULT");

  _outPlanInfo(str, (const Plan *)node);

  WRITE_NODE_FIELD(resconstantqual);
}

static void
_outProjectSet(StringInfo str, const ProjectSet *node)
{
  WRITE_NODE_TYPE("PROJECTSET");

  _outPlanInfo(str, (const Plan *)node);
}

static void
_outModifyTable(StringInfo str, const ModifyTable *node)
{

























}

static void
_outAppend(StringInfo str, const Append *node)
{
  WRITE_NODE_TYPE("APPEND");

  _outPlanInfo(str, (const Plan *)node);

  WRITE_NODE_FIELD(appendplans);
  WRITE_INT_FIELD(first_partial_plan);
  WRITE_NODE_FIELD(part_prune_info);
}

static void
_outMergeAppend(StringInfo str, const MergeAppend *node)
{











}

static void
_outRecursiveUnion(StringInfo str, const RecursiveUnion *node)
{










}

static void
_outBitmapAnd(StringInfo str, const BitmapAnd *node)
{





}

static void
_outBitmapOr(StringInfo str, const BitmapOr *node)
{






}

static void
_outGather(StringInfo str, const Gather *node)
{









}

static void
_outGatherMerge(StringInfo str, const GatherMerge *node)
{












}

static void
_outScan(StringInfo str, const Scan *node)
{



}

static void
_outSeqScan(StringInfo str, const SeqScan *node)
{
  WRITE_NODE_TYPE("SEQSCAN");

  _outScanInfo(str, (const Scan *)node);
}

static void
_outSampleScan(StringInfo str, const SampleScan *node)
{





}

static void
_outIndexScan(StringInfo str, const IndexScan *node)
{
  WRITE_NODE_TYPE("INDEXSCAN");

  _outScanInfo(str, (const Scan *)node);

  WRITE_OID_FIELD(indexid);
  WRITE_NODE_FIELD(indexqual);
  WRITE_NODE_FIELD(indexqualorig);
  WRITE_NODE_FIELD(indexorderby);
  WRITE_NODE_FIELD(indexorderbyorig);
  WRITE_NODE_FIELD(indexorderbyops);
  WRITE_ENUM_FIELD(indexorderdir, ScanDirection);
}

static void
_outIndexOnlyScan(StringInfo str, const IndexOnlyScan *node)
{
  WRITE_NODE_TYPE("INDEXONLYSCAN");

  _outScanInfo(str, (const Scan *)node);

  WRITE_OID_FIELD(indexid);
  WRITE_NODE_FIELD(indexqual);
  WRITE_NODE_FIELD(recheckqual);
  WRITE_NODE_FIELD(indexorderby);
  WRITE_NODE_FIELD(indextlist);
  WRITE_ENUM_FIELD(indexorderdir, ScanDirection);
}

static void
_outBitmapIndexScan(StringInfo str, const BitmapIndexScan *node)
{
  WRITE_NODE_TYPE("BITMAPINDEXSCAN");

  _outScanInfo(str, (const Scan *)node);

  WRITE_OID_FIELD(indexid);
  WRITE_BOOL_FIELD(isshared);
  WRITE_NODE_FIELD(indexqual);
  WRITE_NODE_FIELD(indexqualorig);
}

static void
_outBitmapHeapScan(StringInfo str, const BitmapHeapScan *node)
{
  WRITE_NODE_TYPE("BITMAPHEAPSCAN");

  _outScanInfo(str, (const Scan *)node);

  WRITE_NODE_FIELD(bitmapqualorig);
}

static void
_outTidScan(StringInfo str, const TidScan *node)
{





}

static void
_outSubqueryScan(StringInfo str, const SubqueryScan *node)
{





}

static void
_outFunctionScan(StringInfo str, const FunctionScan *node)
{






}

static void
_outTableFuncScan(StringInfo str, const TableFuncScan *node)
{





}

static void
_outValuesScan(StringInfo str, const ValuesScan *node)
{





}

static void
_outCteScan(StringInfo str, const CteScan *node)
{






}

static void
_outNamedTuplestoreScan(StringInfo str, const NamedTuplestoreScan *node)
{





}

static void
_outWorkTableScan(StringInfo str, const WorkTableScan *node)
{





}

static void
_outForeignScan(StringInfo str, const ForeignScan *node)
{












}

static void
_outCustomScan(StringInfo str, const CustomScan *node)
{













}

static void
_outJoin(StringInfo str, const Join *node)
{



}

static void
_outNestLoop(StringInfo str, const NestLoop *node)
{
  WRITE_NODE_TYPE("NESTLOOP");

  _outJoinPlanInfo(str, (const Join *)node);

  WRITE_NODE_FIELD(nestParams);
}

static void
_outMergeJoin(StringInfo str, const MergeJoin *node)
{
  int numCols;

  WRITE_NODE_TYPE("MERGEJOIN");

  _outJoinPlanInfo(str, (const Join *)node);

  WRITE_BOOL_FIELD(skip_mark_restore);
  WRITE_NODE_FIELD(mergeclauses);

  numCols = list_length(node->mergeclauses);

  WRITE_OID_ARRAY(mergeFamilies, numCols);
  WRITE_OID_ARRAY(mergeCollations, numCols);
  WRITE_INT_ARRAY(mergeStrategies, numCols);
  WRITE_BOOL_ARRAY(mergeNullsFirst, numCols);
}

static void
_outHashJoin(StringInfo str, const HashJoin *node)
{
  WRITE_NODE_TYPE("HASHJOIN");

  _outJoinPlanInfo(str, (const Join *)node);

  WRITE_NODE_FIELD(hashclauses);
  WRITE_NODE_FIELD(hashoperators);
  WRITE_NODE_FIELD(hashcollations);
  WRITE_NODE_FIELD(hashkeys);
}

static void
_outAgg(StringInfo str, const Agg *node)
{
  WRITE_NODE_TYPE("AGG");

  _outPlanInfo(str, (const Plan *)node);

  WRITE_ENUM_FIELD(aggstrategy, AggStrategy);
  WRITE_ENUM_FIELD(aggsplit, AggSplit);
  WRITE_INT_FIELD(numCols);
  WRITE_ATTRNUMBER_ARRAY(grpColIdx, node->numCols);
  WRITE_OID_ARRAY(grpOperators, node->numCols);
  WRITE_OID_ARRAY(grpCollations, node->numCols);
  WRITE_LONG_FIELD(numGroups);
  WRITE_BITMAPSET_FIELD(aggParams);
  WRITE_NODE_FIELD(groupingSets);
  WRITE_NODE_FIELD(chain);
}

static void
_outWindowAgg(StringInfo str, const WindowAgg *node)
{





















}

static void
_outGroup(StringInfo str, const Group *node)
{








}

static void
_outMaterial(StringInfo str, const Material *node)
{



}

static void
_outSort(StringInfo str, const Sort *node)
{
  WRITE_NODE_TYPE("SORT");

  _outPlanInfo(str, (const Plan *)node);

  WRITE_INT_FIELD(numCols);
  WRITE_ATTRNUMBER_ARRAY(sortColIdx, node->numCols);
  WRITE_OID_ARRAY(sortOperators, node->numCols);
  WRITE_OID_ARRAY(collations, node->numCols);
  WRITE_BOOL_ARRAY(nullsFirst, node->numCols);
}

static void
_outUnique(StringInfo str, const Unique *node)
{








}

static void
_outHash(StringInfo str, const Hash *node)
{
  WRITE_NODE_TYPE("HASH");

  _outPlanInfo(str, (const Plan *)node);

  WRITE_NODE_FIELD(hashkeys);
  WRITE_OID_FIELD(skewTable);
  WRITE_INT_FIELD(skewColumn);
  WRITE_BOOL_FIELD(skewInherit);
  WRITE_FLOAT_FIELD(rows_total, "%.0f");
}

static void
_outSetOp(StringInfo str, const SetOp *node)
{













}

static void
_outLockRows(StringInfo str, const LockRows *node)
{






}

static void
_outLimit(StringInfo str, const Limit *node)
{






}

static void
_outNestLoopParam(StringInfo str, const NestLoopParam *node)
{
  WRITE_NODE_TYPE("NESTLOOPPARAM");

  WRITE_INT_FIELD(paramno);
  WRITE_NODE_FIELD(paramval);
}

static void
_outPlanRowMark(StringInfo str, const PlanRowMark *node)
{










}

static void
_outPartitionPruneInfo(StringInfo str, const PartitionPruneInfo *node)
{
  WRITE_NODE_TYPE("PARTITIONPRUNEINFO");

  WRITE_NODE_FIELD(prune_infos);
  WRITE_BITMAPSET_FIELD(other_subplans);
}

static void
_outPartitionedRelPruneInfo(StringInfo str, const PartitionedRelPruneInfo *node)
{
  WRITE_NODE_TYPE("PARTITIONEDRELPRUNEINFO");

  WRITE_UINT_FIELD(rtindex);
  WRITE_BITMAPSET_FIELD(present_parts);
  WRITE_INT_FIELD(nparts);
  WRITE_INT_ARRAY(subplan_map, node->nparts);
  WRITE_INT_ARRAY(subpart_map, node->nparts);
  WRITE_OID_ARRAY(relid_map, node->nparts);
  WRITE_NODE_FIELD(initial_pruning_steps);
  WRITE_NODE_FIELD(exec_pruning_steps);
  WRITE_BITMAPSET_FIELD(execparamids);
}

static void
_outPartitionPruneStepOp(StringInfo str, const PartitionPruneStepOp *node)
{
  WRITE_NODE_TYPE("PARTITIONPRUNESTEPOP");

  WRITE_INT_FIELD(step.step_id);
  WRITE_INT_FIELD(opstrategy);
  WRITE_NODE_FIELD(exprs);
  WRITE_NODE_FIELD(cmpfns);
  WRITE_BITMAPSET_FIELD(nullkeys);
}

static void
_outPartitionPruneStepCombine(StringInfo str, const PartitionPruneStepCombine *node)
{
  WRITE_NODE_TYPE("PARTITIONPRUNESTEPCOMBINE");

  WRITE_INT_FIELD(step.step_id);
  WRITE_ENUM_FIELD(combineOp, PartitionPruneCombineOp);
  WRITE_NODE_FIELD(source_stepids);
}

static void
_outPlanInvalItem(StringInfo str, const PlanInvalItem *node)
{




}

/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

static void
_outAlias(StringInfo str, const Alias *node)
{
  WRITE_NODE_TYPE("ALIAS");

  WRITE_STRING_FIELD(aliasname);
  WRITE_NODE_FIELD(colnames);
}

static void
_outRangeVar(StringInfo str, const RangeVar *node)
{












}

static void
_outTableFunc(StringInfo str, const TableFunc *node)
{
  WRITE_NODE_TYPE("TABLEFUNC");

  WRITE_NODE_FIELD(ns_uris);
  WRITE_NODE_FIELD(ns_names);
  WRITE_NODE_FIELD(docexpr);
  WRITE_NODE_FIELD(rowexpr);
  WRITE_NODE_FIELD(colnames);
  WRITE_NODE_FIELD(coltypes);
  WRITE_NODE_FIELD(coltypmods);
  WRITE_NODE_FIELD(colcollations);
  WRITE_NODE_FIELD(colexprs);
  WRITE_NODE_FIELD(coldefexprs);
  WRITE_BITMAPSET_FIELD(notnulls);
  WRITE_INT_FIELD(ordinalitycol);
  WRITE_LOCATION_FIELD(location);
}

static void
_outIntoClause(StringInfo str, const IntoClause *node)
{










}

static void
_outVar(StringInfo str, const Var *node)
{
  WRITE_NODE_TYPE("VAR");

  WRITE_UINT_FIELD(varno);
  WRITE_INT_FIELD(varattno);
  WRITE_OID_FIELD(vartype);
  WRITE_INT_FIELD(vartypmod);
  WRITE_OID_FIELD(varcollid);
  WRITE_UINT_FIELD(varlevelsup);
  WRITE_UINT_FIELD(varnoold);
  WRITE_INT_FIELD(varoattno);
  WRITE_LOCATION_FIELD(location);
}

static void
_outConst(StringInfo str, const Const *node)
{
  WRITE_NODE_TYPE("CONST");

  WRITE_OID_FIELD(consttype);
  WRITE_INT_FIELD(consttypmod);
  WRITE_OID_FIELD(constcollid);
  WRITE_INT_FIELD(constlen);
  WRITE_BOOL_FIELD(constbyval);
  WRITE_BOOL_FIELD(constisnull);
  WRITE_LOCATION_FIELD(location);

  appendStringInfoString(str, " :constvalue ");
  if (node->constisnull)
  {
    appendStringInfoString(str, "<>");
  }
  else
  {
    outDatum(str, node->constvalue, node->constlen, node->constbyval);
  }
}

static void
_outParam(StringInfo str, const Param *node)
{
  WRITE_NODE_TYPE("PARAM");

  WRITE_ENUM_FIELD(paramkind, ParamKind);
  WRITE_INT_FIELD(paramid);
  WRITE_OID_FIELD(paramtype);
  WRITE_INT_FIELD(paramtypmod);
  WRITE_OID_FIELD(paramcollid);
  WRITE_LOCATION_FIELD(location);
}

static void
_outAggref(StringInfo str, const Aggref *node)
{
  WRITE_NODE_TYPE("AGGREF");

  WRITE_OID_FIELD(aggfnoid);
  WRITE_OID_FIELD(aggtype);
  WRITE_OID_FIELD(aggcollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_OID_FIELD(aggtranstype);
  WRITE_NODE_FIELD(aggargtypes);
  WRITE_NODE_FIELD(aggdirectargs);
  WRITE_NODE_FIELD(args);
  WRITE_NODE_FIELD(aggorder);
  WRITE_NODE_FIELD(aggdistinct);
  WRITE_NODE_FIELD(aggfilter);
  WRITE_BOOL_FIELD(aggstar);
  WRITE_BOOL_FIELD(aggvariadic);
  WRITE_CHAR_FIELD(aggkind);
  WRITE_UINT_FIELD(agglevelsup);
  WRITE_ENUM_FIELD(aggsplit, AggSplit);
  WRITE_LOCATION_FIELD(location);
}

static void
_outGroupingFunc(StringInfo str, const GroupingFunc *node)
{







}

static void
_outWindowFunc(StringInfo str, const WindowFunc *node)
{
  WRITE_NODE_TYPE("WINDOWFUNC");

  WRITE_OID_FIELD(winfnoid);
  WRITE_OID_FIELD(wintype);
  WRITE_OID_FIELD(wincollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_NODE_FIELD(args);
  WRITE_NODE_FIELD(aggfilter);
  WRITE_UINT_FIELD(winref);
  WRITE_BOOL_FIELD(winstar);
  WRITE_BOOL_FIELD(winagg);
  WRITE_LOCATION_FIELD(location);
}

static void
_outSubscriptingRef(StringInfo str, const SubscriptingRef *node)
{
  WRITE_NODE_TYPE("SUBSCRIPTINGREF");

  WRITE_OID_FIELD(refcontainertype);
  WRITE_OID_FIELD(refelemtype);
  WRITE_INT_FIELD(reftypmod);
  WRITE_OID_FIELD(refcollid);
  WRITE_NODE_FIELD(refupperindexpr);
  WRITE_NODE_FIELD(reflowerindexpr);
  WRITE_NODE_FIELD(refexpr);
  WRITE_NODE_FIELD(refassgnexpr);
}

static void
_outFuncExpr(StringInfo str, const FuncExpr *node)
{
  WRITE_NODE_TYPE("FUNCEXPR");

  WRITE_OID_FIELD(funcid);
  WRITE_OID_FIELD(funcresulttype);
  WRITE_BOOL_FIELD(funcretset);
  WRITE_BOOL_FIELD(funcvariadic);
  WRITE_ENUM_FIELD(funcformat, CoercionForm);
  WRITE_OID_FIELD(funccollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outNamedArgExpr(StringInfo str, const NamedArgExpr *node)
{
  WRITE_NODE_TYPE("NAMEDARGEXPR");

  WRITE_NODE_FIELD(arg);
  WRITE_STRING_FIELD(name);
  WRITE_INT_FIELD(argnumber);
  WRITE_LOCATION_FIELD(location);
}

static void
_outOpExpr(StringInfo str, const OpExpr *node)
{
  WRITE_NODE_TYPE("OPEXPR");

  WRITE_OID_FIELD(opno);
  WRITE_OID_FIELD(opfuncid);
  WRITE_OID_FIELD(opresulttype);
  WRITE_BOOL_FIELD(opretset);
  WRITE_OID_FIELD(opcollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outDistinctExpr(StringInfo str, const DistinctExpr *node)
{
  WRITE_NODE_TYPE("DISTINCTEXPR");

  WRITE_OID_FIELD(opno);
  WRITE_OID_FIELD(opfuncid);
  WRITE_OID_FIELD(opresulttype);
  WRITE_BOOL_FIELD(opretset);
  WRITE_OID_FIELD(opcollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outNullIfExpr(StringInfo str, const NullIfExpr *node)
{
  WRITE_NODE_TYPE("NULLIFEXPR");

  WRITE_OID_FIELD(opno);
  WRITE_OID_FIELD(opfuncid);
  WRITE_OID_FIELD(opresulttype);
  WRITE_BOOL_FIELD(opretset);
  WRITE_OID_FIELD(opcollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outScalarArrayOpExpr(StringInfo str, const ScalarArrayOpExpr *node)
{
  WRITE_NODE_TYPE("SCALARARRAYOPEXPR");

  WRITE_OID_FIELD(opno);
  WRITE_OID_FIELD(opfuncid);
  WRITE_BOOL_FIELD(useOr);
  WRITE_OID_FIELD(inputcollid);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outBoolExpr(StringInfo str, const BoolExpr *node)
{
  char *opstr = NULL;

  WRITE_NODE_TYPE("BOOLEXPR");

  /* do-it-yourself enum representation */
  switch (node->boolop)
  {
  case AND_EXPR:;;
    opstr = "and";
    break;
  case OR_EXPR:;;
    opstr = "or";
    break;
  case NOT_EXPR:;;
    opstr = "not";
    break;
  }
  appendStringInfoString(str, " :boolop ");
  outToken(str, opstr);

  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outSubLink(StringInfo str, const SubLink *node)
{
  WRITE_NODE_TYPE("SUBLINK");

  WRITE_ENUM_FIELD(subLinkType, SubLinkType);
  WRITE_INT_FIELD(subLinkId);
  WRITE_NODE_FIELD(testexpr);
  WRITE_NODE_FIELD(operName);
  WRITE_NODE_FIELD(subselect);
  WRITE_LOCATION_FIELD(location);
}

static void
_outSubPlan(StringInfo str, const SubPlan *node)
{
  WRITE_NODE_TYPE("SUBPLAN");

  WRITE_ENUM_FIELD(subLinkType, SubLinkType);
  WRITE_NODE_FIELD(testexpr);
  WRITE_NODE_FIELD(paramIds);
  WRITE_INT_FIELD(plan_id);
  WRITE_STRING_FIELD(plan_name);
  WRITE_OID_FIELD(firstColType);
  WRITE_INT_FIELD(firstColTypmod);
  WRITE_OID_FIELD(firstColCollation);
  WRITE_BOOL_FIELD(useHashTable);
  WRITE_BOOL_FIELD(unknownEqFalse);
  WRITE_BOOL_FIELD(parallel_safe);
  WRITE_NODE_FIELD(setParam);
  WRITE_NODE_FIELD(parParam);
  WRITE_NODE_FIELD(args);
  WRITE_FLOAT_FIELD(startup_cost, "%.2f");
  WRITE_FLOAT_FIELD(per_call_cost, "%.2f");
  WRITE_INT_FIELD(subLinkId);
}

static void
_outAlternativeSubPlan(StringInfo str, const AlternativeSubPlan *node)
{



}

static void
_outFieldSelect(StringInfo str, const FieldSelect *node)
{
  WRITE_NODE_TYPE("FIELDSELECT");

  WRITE_NODE_FIELD(arg);
  WRITE_INT_FIELD(fieldnum);
  WRITE_OID_FIELD(resulttype);
  WRITE_INT_FIELD(resulttypmod);
  WRITE_OID_FIELD(resultcollid);
}

static void
_outFieldStore(StringInfo str, const FieldStore *node)
{
  WRITE_NODE_TYPE("FIELDSTORE");

  WRITE_NODE_FIELD(arg);
  WRITE_NODE_FIELD(newvals);
  WRITE_NODE_FIELD(fieldnums);
  WRITE_OID_FIELD(resulttype);
}

static void
_outRelabelType(StringInfo str, const RelabelType *node)
{
  WRITE_NODE_TYPE("RELABELTYPE");

  WRITE_NODE_FIELD(arg);
  WRITE_OID_FIELD(resulttype);
  WRITE_INT_FIELD(resulttypmod);
  WRITE_OID_FIELD(resultcollid);
  WRITE_ENUM_FIELD(relabelformat, CoercionForm);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCoerceViaIO(StringInfo str, const CoerceViaIO *node)
{
  WRITE_NODE_TYPE("COERCEVIAIO");

  WRITE_NODE_FIELD(arg);
  WRITE_OID_FIELD(resulttype);
  WRITE_OID_FIELD(resultcollid);
  WRITE_ENUM_FIELD(coerceformat, CoercionForm);
  WRITE_LOCATION_FIELD(location);
}

static void
_outArrayCoerceExpr(StringInfo str, const ArrayCoerceExpr *node)
{
  WRITE_NODE_TYPE("ARRAYCOERCEEXPR");

  WRITE_NODE_FIELD(arg);
  WRITE_NODE_FIELD(elemexpr);
  WRITE_OID_FIELD(resulttype);
  WRITE_INT_FIELD(resulttypmod);
  WRITE_OID_FIELD(resultcollid);
  WRITE_ENUM_FIELD(coerceformat, CoercionForm);
  WRITE_LOCATION_FIELD(location);
}

static void
_outConvertRowtypeExpr(StringInfo str, const ConvertRowtypeExpr *node)
{






}

static void
_outCollateExpr(StringInfo str, const CollateExpr *node)
{
  WRITE_NODE_TYPE("COLLATE");

  WRITE_NODE_FIELD(arg);
  WRITE_OID_FIELD(collOid);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCaseExpr(StringInfo str, const CaseExpr *node)
{
  WRITE_NODE_TYPE("CASE");

  WRITE_OID_FIELD(casetype);
  WRITE_OID_FIELD(casecollid);
  WRITE_NODE_FIELD(arg);
  WRITE_NODE_FIELD(args);
  WRITE_NODE_FIELD(defresult);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCaseWhen(StringInfo str, const CaseWhen *node)
{
  WRITE_NODE_TYPE("WHEN");

  WRITE_NODE_FIELD(expr);
  WRITE_NODE_FIELD(result);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCaseTestExpr(StringInfo str, const CaseTestExpr *node)
{
  WRITE_NODE_TYPE("CASETESTEXPR");

  WRITE_OID_FIELD(typeId);
  WRITE_INT_FIELD(typeMod);
  WRITE_OID_FIELD(collation);
}

static void
_outArrayExpr(StringInfo str, const ArrayExpr *node)
{
  WRITE_NODE_TYPE("ARRAY");

  WRITE_OID_FIELD(array_typeid);
  WRITE_OID_FIELD(array_collid);
  WRITE_OID_FIELD(element_typeid);
  WRITE_NODE_FIELD(elements);
  WRITE_BOOL_FIELD(multidims);
  WRITE_LOCATION_FIELD(location);
}

static void
_outRowExpr(StringInfo str, const RowExpr *node)
{
  WRITE_NODE_TYPE("ROW");

  WRITE_NODE_FIELD(args);
  WRITE_OID_FIELD(row_typeid);
  WRITE_ENUM_FIELD(row_format, CoercionForm);
  WRITE_NODE_FIELD(colnames);
  WRITE_LOCATION_FIELD(location);
}

static void
_outRowCompareExpr(StringInfo str, const RowCompareExpr *node)
{
  WRITE_NODE_TYPE("ROWCOMPARE");

  WRITE_ENUM_FIELD(rctype, RowCompareType);
  WRITE_NODE_FIELD(opnos);
  WRITE_NODE_FIELD(opfamilies);
  WRITE_NODE_FIELD(inputcollids);
  WRITE_NODE_FIELD(largs);
  WRITE_NODE_FIELD(rargs);
}

static void
_outCoalesceExpr(StringInfo str, const CoalesceExpr *node)
{
  WRITE_NODE_TYPE("COALESCE");

  WRITE_OID_FIELD(coalescetype);
  WRITE_OID_FIELD(coalescecollid);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outMinMaxExpr(StringInfo str, const MinMaxExpr *node)
{
  WRITE_NODE_TYPE("MINMAX");

  WRITE_OID_FIELD(minmaxtype);
  WRITE_OID_FIELD(minmaxcollid);
  WRITE_OID_FIELD(inputcollid);
  WRITE_ENUM_FIELD(op, MinMaxOp);
  WRITE_NODE_FIELD(args);
  WRITE_LOCATION_FIELD(location);
}

static void
_outSQLValueFunction(StringInfo str, const SQLValueFunction *node)
{
  WRITE_NODE_TYPE("SQLVALUEFUNCTION");

  WRITE_ENUM_FIELD(op, SQLValueFunctionOp);
  WRITE_OID_FIELD(type);
  WRITE_INT_FIELD(typmod);
  WRITE_LOCATION_FIELD(location);
}

static void
_outXmlExpr(StringInfo str, const XmlExpr *node)
{
  WRITE_NODE_TYPE("XMLEXPR");

  WRITE_ENUM_FIELD(op, XmlExprOp);
  WRITE_STRING_FIELD(name);
  WRITE_NODE_FIELD(named_args);
  WRITE_NODE_FIELD(arg_names);
  WRITE_NODE_FIELD(args);
  WRITE_ENUM_FIELD(xmloption, XmlOptionType);
  WRITE_OID_FIELD(type);
  WRITE_INT_FIELD(typmod);
  WRITE_LOCATION_FIELD(location);
}

static void
_outNullTest(StringInfo str, const NullTest *node)
{
  WRITE_NODE_TYPE("NULLTEST");

  WRITE_NODE_FIELD(arg);
  WRITE_ENUM_FIELD(nulltesttype, NullTestType);
  WRITE_BOOL_FIELD(argisrow);
  WRITE_LOCATION_FIELD(location);
}

static void
_outBooleanTest(StringInfo str, const BooleanTest *node)
{
  WRITE_NODE_TYPE("BOOLEANTEST");

  WRITE_NODE_FIELD(arg);
  WRITE_ENUM_FIELD(booltesttype, BoolTestType);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCoerceToDomain(StringInfo str, const CoerceToDomain *node)
{
  WRITE_NODE_TYPE("COERCETODOMAIN");

  WRITE_NODE_FIELD(arg);
  WRITE_OID_FIELD(resulttype);
  WRITE_INT_FIELD(resulttypmod);
  WRITE_OID_FIELD(resultcollid);
  WRITE_ENUM_FIELD(coercionformat, CoercionForm);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCoerceToDomainValue(StringInfo str, const CoerceToDomainValue *node)
{
  WRITE_NODE_TYPE("COERCETODOMAINVALUE");

  WRITE_OID_FIELD(typeId);
  WRITE_INT_FIELD(typeMod);
  WRITE_OID_FIELD(collation);
  WRITE_LOCATION_FIELD(location);
}

static void
_outSetToDefault(StringInfo str, const SetToDefault *node)
{
  WRITE_NODE_TYPE("SETTODEFAULT");

  WRITE_OID_FIELD(typeId);
  WRITE_INT_FIELD(typeMod);
  WRITE_OID_FIELD(collation);
  WRITE_LOCATION_FIELD(location);
}

static void
_outCurrentOfExpr(StringInfo str, const CurrentOfExpr *node)
{





}

static void
_outNextValueExpr(StringInfo str, const NextValueExpr *node)
{




}

static void
_outInferenceElem(StringInfo str, const InferenceElem *node)
{
  WRITE_NODE_TYPE("INFERENCEELEM");

  WRITE_NODE_FIELD(expr);
  WRITE_OID_FIELD(infercollid);
  WRITE_OID_FIELD(inferopclass);
}

static void
_outTargetEntry(StringInfo str, const TargetEntry *node)
{
  WRITE_NODE_TYPE("TARGETENTRY");

  WRITE_NODE_FIELD(expr);
  WRITE_INT_FIELD(resno);
  WRITE_STRING_FIELD(resname);
  WRITE_UINT_FIELD(ressortgroupref);
  WRITE_OID_FIELD(resorigtbl);
  WRITE_INT_FIELD(resorigcol);
  WRITE_BOOL_FIELD(resjunk);
}

static void
_outRangeTblRef(StringInfo str, const RangeTblRef *node)
{
  WRITE_NODE_TYPE("RANGETBLREF");

  WRITE_INT_FIELD(rtindex);
}

static void
_outJoinExpr(StringInfo str, const JoinExpr *node)
{
  WRITE_NODE_TYPE("JOINEXPR");

  WRITE_ENUM_FIELD(jointype, JoinType);
  WRITE_BOOL_FIELD(isNatural);
  WRITE_NODE_FIELD(larg);
  WRITE_NODE_FIELD(rarg);
  WRITE_NODE_FIELD(usingClause);
  WRITE_NODE_FIELD(quals);
  WRITE_NODE_FIELD(alias);
  WRITE_INT_FIELD(rtindex);
}

static void
_outFromExpr(StringInfo str, const FromExpr *node)
{
  WRITE_NODE_TYPE("FROMEXPR");

  WRITE_NODE_FIELD(fromlist);
  WRITE_NODE_FIELD(quals);
}

static void
_outOnConflictExpr(StringInfo str, const OnConflictExpr *node)
{
  WRITE_NODE_TYPE("ONCONFLICTEXPR");

  WRITE_ENUM_FIELD(action, OnConflictAction);
  WRITE_NODE_FIELD(arbiterElems);
  WRITE_NODE_FIELD(arbiterWhere);
  WRITE_OID_FIELD(constraint);
  WRITE_NODE_FIELD(onConflictSet);
  WRITE_NODE_FIELD(onConflictWhere);
  WRITE_INT_FIELD(exclRelIndex);
  WRITE_NODE_FIELD(exclRelTlist);
}

/*****************************************************************************
 *
 *	Stuff from pathnodes.h.
 *
 *****************************************************************************/

/*
 * print the basic stuff of all nodes that inherit from Path
 *
 * Note we do NOT print the parent, else we'd be in infinite recursion.
 * We can print the parent's relids for identification purposes, though.
 * We print the pathtarget only if it's not the default one for the rel.
 * We also do not print the whole of param_info, since it's printed by
 * _outRelOptInfo; it's sufficient and less cluttering to print just the
 * required outer relids.
 */
static void
_outPathInfo(StringInfo str, const Path *node)
{























}

/*
 * print the basic stuff of all nodes that inherit from JoinPath
 */
static void
_outJoinPathInfo(StringInfo str, const JoinPath *node)
{







}

static void
_outPath(StringInfo str, const Path *node)
{



}

static void
_outIndexPath(StringInfo str, const IndexPath *node)
{











}

static void
_outBitmapHeapPath(StringInfo str, const BitmapHeapPath *node)
{





}

static void
_outBitmapAndPath(StringInfo str, const BitmapAndPath *node)
{






}

static void
_outBitmapOrPath(StringInfo str, const BitmapOrPath *node)
{






}

static void
_outTidPath(StringInfo str, const TidPath *node)
{





}

static void
_outSubqueryScanPath(StringInfo str, const SubqueryScanPath *node)
{





}

static void
_outForeignPath(StringInfo str, const ForeignPath *node)
{






}

static void
_outCustomPath(StringInfo str, const CustomPath *node)
{









}

static void
_outAppendPath(StringInfo str, const AppendPath *node)
{








}

static void
_outMergeAppendPath(StringInfo str, const MergeAppendPath *node)
{







}

static void
_outGroupResultPath(StringInfo str, const GroupResultPath *node)
{





}

static void
_outMaterialPath(StringInfo str, const MaterialPath *node)
{





}

static void
_outUniquePath(StringInfo str, const UniquePath *node)
{








}

static void
_outGatherPath(StringInfo str, const GatherPath *node)
{







}

static void
_outProjectionPath(StringInfo str, const ProjectionPath *node)
{






}

static void
_outProjectSetPath(StringInfo str, const ProjectSetPath *node)
{





}

static void
_outSortPath(StringInfo str, const SortPath *node)
{





}

static void
_outGroupPath(StringInfo str, const GroupPath *node)
{







}

static void
_outUpperUniquePath(StringInfo str, const UpperUniquePath *node)
{






}

static void
_outAggPath(StringInfo str, const AggPath *node)
{










}

static void
_outRollupData(StringInfo str, const RollupData *node)
{








}

static void
_outGroupingSetData(StringInfo str, const GroupingSetData *node)
{




}

static void
_outGroupingSetsPath(StringInfo str, const GroupingSetsPath *node)
{








}

static void
_outMinMaxAggPath(StringInfo str, const MinMaxAggPath *node)
{






}

static void
_outWindowAggPath(StringInfo str, const WindowAggPath *node)
{






}

static void
_outSetOpPath(StringInfo str, const SetOpPath *node)
{











}

static void
_outRecursiveUnionPath(StringInfo str, const RecursiveUnionPath *node)
{









}

static void
_outLockRowsPath(StringInfo str, const LockRowsPath *node)
{







}

static void
_outModifyTablePath(StringInfo str, const ModifyTablePath *node)
{

















}

static void
_outLimitPath(StringInfo str, const LimitPath *node)
{







}

static void
_outGatherMergePath(StringInfo str, const GatherMergePath *node)
{






}

static void
_outNestPath(StringInfo str, const NestPath *node)
{



}

static void
_outMergePath(StringInfo str, const MergePath *node)
{









}

static void
_outHashPath(StringInfo str, const HashPath *node)
{







}

static void
_outPlannerGlobal(StringInfo str, const PlannerGlobal *node)
{




















}

static void
_outPlannerInfo(StringInfo str, const PlannerInfo *node)
{














































}

static void
_outRelOptInfo(StringInfo str, const RelOptInfo *node)
{














































}

static void
_outIndexOptInfo(StringInfo str, const IndexOptInfo *node)
{




















}

static void
_outForeignKeyOptInfo(StringInfo str, const ForeignKeyOptInfo *node)
{
























}

static void
_outStatisticExtInfo(StringInfo str, const StatisticExtInfo *node)
{







}

static void
_outEquivalenceClass(StringInfo str, const EquivalenceClass *node)
{
























}

static void
_outEquivalenceMember(StringInfo str, const EquivalenceMember *node)
{








}

static void
_outPathKey(StringInfo str, const PathKey *node)
{






}

static void
_outPathTarget(StringInfo str, const PathTarget *node)
{
















}

static void
_outParamPathInfo(StringInfo str, const ParamPathInfo *node)
{





}

static void
_outRestrictInfo(StringInfo str, const RestrictInfo *node)
{



























}

static void
_outIndexClause(StringInfo str, const IndexClause *node)
{







}

static void
_outPlaceHolderVar(StringInfo str, const PlaceHolderVar *node)
{






}

static void
_outSpecialJoinInfo(StringInfo str, const SpecialJoinInfo *node)
{













}

static void
_outAppendRelInfo(StringInfo str, const AppendRelInfo *node)
{








}

static void
_outPlaceHolderInfo(StringInfo str, const PlaceHolderInfo *node)
{








}

static void
_outMinMaxAggInfo(StringInfo str, const MinMaxAggInfo *node)
{









}

static void
_outPlannerParamItem(StringInfo str, const PlannerParamItem *node)
{




}

/*****************************************************************************
 *
 *	Stuff from extensible.h
 *
 *****************************************************************************/

static void
_outExtensibleNode(StringInfo str, const ExtensibleNode *node)
{










}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

/*
 * print the basic stuff of all nodes that inherit from CreateStmt
 */
static void
_outCreateStmtInfo(StringInfo str, const CreateStmt *node)
{












}

static void
_outCreateStmt(StringInfo str, const CreateStmt *node)
{



}

static void
_outCreateForeignTableStmt(StringInfo str, const CreateForeignTableStmt *node)
{






}

static void
_outImportForeignSchemaStmt(StringInfo str, const ImportForeignSchemaStmt *node)
{








}

static void
_outIndexStmt(StringInfo str, const IndexStmt *node)
{























}

static void
_outCreateStatsStmt(StringInfo str, const CreateStatsStmt *node)
{








}

static void
_outNotifyStmt(StringInfo str, const NotifyStmt *node)
{
  WRITE_NODE_TYPE("NOTIFY");

  WRITE_STRING_FIELD(conditionname);
  WRITE_STRING_FIELD(payload);
}

static void
_outDeclareCursorStmt(StringInfo str, const DeclareCursorStmt *node)
{





}

static void
_outSelectStmt(StringInfo str, const SelectStmt *node)
{




















}

static void
_outFuncCall(StringInfo str, const FuncCall *node)
{












}

static void
_outDefElem(StringInfo str, const DefElem *node)
{







}

static void
_outTableLikeClause(StringInfo str, const TableLikeClause *node)
{





}

static void
_outLockingClause(StringInfo str, const LockingClause *node)
{





}

static void
_outXmlSerialize(StringInfo str, const XmlSerialize *node)
{






}

static void
_outTriggerTransition(StringInfo str, const TriggerTransition *node)
{





}

static void
_outColumnDef(StringInfo str, const ColumnDef *node)
{



















}

static void
_outTypeName(StringInfo str, const TypeName *node)
{










}

static void
_outTypeCast(StringInfo str, const TypeCast *node)
{





}

static void
_outCollateClause(StringInfo str, const CollateClause *node)
{





}

static void
_outIndexElem(StringInfo str, const IndexElem *node)
{









}

static void
_outQuery(StringInfo str, const Query *node)
{
  WRITE_NODE_TYPE("QUERY");

  WRITE_ENUM_FIELD(commandType, CmdType);
  WRITE_ENUM_FIELD(querySource, QuerySource);
  /* we intentionally do not print the queryId field */
  WRITE_BOOL_FIELD(canSetTag);

  /*
   * Hack to work around missing outfuncs routines for a lot of the
   * utility-statement node types.  (The only one we actually *need* for
   * rules support is NotifyStmt.)  Someday we ought to support 'em all, but
   * for the meantime do this to avoid getting lots of warnings when running
   * with debug_print_parse on.
   */
  if (node->utilityStmt)
  {
    switch (nodeTag(node->utilityStmt))
    {
    case T_CreateStmt:;;
    case T_IndexStmt:;;
    case T_NotifyStmt:;;
    case T_DeclareCursorStmt:;;
      WRITE_NODE_FIELD(utilityStmt);
      break;
    default:;;;


    }
  }
  else
  {
    appendStringInfoString(str, " :utilityStmt <>");
  }

  WRITE_INT_FIELD(resultRelation);
  WRITE_BOOL_FIELD(hasAggs);
  WRITE_BOOL_FIELD(hasWindowFuncs);
  WRITE_BOOL_FIELD(hasTargetSRFs);
  WRITE_BOOL_FIELD(hasSubLinks);
  WRITE_BOOL_FIELD(hasDistinctOn);
  WRITE_BOOL_FIELD(hasRecursive);
  WRITE_BOOL_FIELD(hasModifyingCTE);
  WRITE_BOOL_FIELD(hasForUpdate);
  WRITE_BOOL_FIELD(hasRowSecurity);
  WRITE_NODE_FIELD(cteList);
  WRITE_NODE_FIELD(rtable);
  WRITE_NODE_FIELD(jointree);
  WRITE_NODE_FIELD(targetList);
  WRITE_ENUM_FIELD(override, OverridingKind);
  WRITE_NODE_FIELD(onConflict);
  WRITE_NODE_FIELD(returningList);
  WRITE_NODE_FIELD(groupClause);
  WRITE_NODE_FIELD(groupingSets);
  WRITE_NODE_FIELD(havingQual);
  WRITE_NODE_FIELD(windowClause);
  WRITE_NODE_FIELD(distinctClause);
  WRITE_NODE_FIELD(sortClause);
  WRITE_NODE_FIELD(limitOffset);
  WRITE_NODE_FIELD(limitCount);
  WRITE_NODE_FIELD(rowMarks);
  WRITE_NODE_FIELD(setOperations);
  WRITE_NODE_FIELD(constraintDeps);
  WRITE_NODE_FIELD(withCheckOptions);
  WRITE_LOCATION_FIELD(stmt_location);
  WRITE_LOCATION_FIELD(stmt_len);
}

static void
_outWithCheckOption(StringInfo str, const WithCheckOption *node)
{







}

static void
_outSortGroupClause(StringInfo str, const SortGroupClause *node)
{
  WRITE_NODE_TYPE("SORTGROUPCLAUSE");

  WRITE_UINT_FIELD(tleSortGroupRef);
  WRITE_OID_FIELD(eqop);
  WRITE_OID_FIELD(sortop);
  WRITE_BOOL_FIELD(nulls_first);
  WRITE_BOOL_FIELD(hashable);
}

static void
_outGroupingSet(StringInfo str, const GroupingSet *node)
{





}

static void
_outWindowClause(StringInfo str, const WindowClause *node)
{
  WRITE_NODE_TYPE("WINDOWCLAUSE");

  WRITE_STRING_FIELD(name);
  WRITE_STRING_FIELD(refname);
  WRITE_NODE_FIELD(partitionClause);
  WRITE_NODE_FIELD(orderClause);
  WRITE_INT_FIELD(frameOptions);
  WRITE_NODE_FIELD(startOffset);
  WRITE_NODE_FIELD(endOffset);
  WRITE_OID_FIELD(startInRangeFunc);
  WRITE_OID_FIELD(endInRangeFunc);
  WRITE_OID_FIELD(inRangeColl);
  WRITE_BOOL_FIELD(inRangeAsc);
  WRITE_BOOL_FIELD(inRangeNullsFirst);
  WRITE_UINT_FIELD(winref);
  WRITE_BOOL_FIELD(copiedOrder);
}

static void
_outRowMarkClause(StringInfo str, const RowMarkClause *node)
{
  WRITE_NODE_TYPE("ROWMARKCLAUSE");

  WRITE_UINT_FIELD(rti);
  WRITE_ENUM_FIELD(strength, LockClauseStrength);
  WRITE_ENUM_FIELD(waitPolicy, LockWaitPolicy);
  WRITE_BOOL_FIELD(pushedDown);
}

static void
_outWithClause(StringInfo str, const WithClause *node)
{





}

static void
_outCommonTableExpr(StringInfo str, const CommonTableExpr *node)
{
  WRITE_NODE_TYPE("COMMONTABLEEXPR");

  WRITE_STRING_FIELD(ctename);
  WRITE_NODE_FIELD(aliascolnames);
  WRITE_ENUM_FIELD(ctematerialized, CTEMaterialize);
  WRITE_NODE_FIELD(ctequery);
  WRITE_LOCATION_FIELD(location);
  WRITE_BOOL_FIELD(cterecursive);
  WRITE_INT_FIELD(cterefcount);
  WRITE_NODE_FIELD(ctecolnames);
  WRITE_NODE_FIELD(ctecoltypes);
  WRITE_NODE_FIELD(ctecoltypmods);
  WRITE_NODE_FIELD(ctecolcollations);
}

static void
_outSetOperationStmt(StringInfo str, const SetOperationStmt *node)
{
  WRITE_NODE_TYPE("SETOPERATIONSTMT");

  WRITE_ENUM_FIELD(op, SetOperation);
  WRITE_BOOL_FIELD(all);
  WRITE_NODE_FIELD(larg);
  WRITE_NODE_FIELD(rarg);
  WRITE_NODE_FIELD(colTypes);
  WRITE_NODE_FIELD(colTypmods);
  WRITE_NODE_FIELD(colCollations);
  WRITE_NODE_FIELD(groupClauses);
}

static void
_outRangeTblEntry(StringInfo str, const RangeTblEntry *node)
{
  WRITE_NODE_TYPE("RTE");

  /* put alias + eref first to make dump more legible */
  WRITE_NODE_FIELD(alias);
  WRITE_NODE_FIELD(eref);
  WRITE_ENUM_FIELD(rtekind, RTEKind);

  switch (node->rtekind)
  {
  case RTE_RELATION:;;
    WRITE_OID_FIELD(relid);
    WRITE_CHAR_FIELD(relkind);
    WRITE_INT_FIELD(rellockmode);
    WRITE_NODE_FIELD(tablesample);
    break;
  case RTE_SUBQUERY:;;
    WRITE_NODE_FIELD(subquery);
    WRITE_BOOL_FIELD(security_barrier);
    break;
  case RTE_JOIN:;;
    WRITE_ENUM_FIELD(jointype, JoinType);
    WRITE_NODE_FIELD(joinaliasvars);
    break;
  case RTE_FUNCTION:;;
    WRITE_NODE_FIELD(functions);
    WRITE_BOOL_FIELD(funcordinality);
    break;
  case RTE_TABLEFUNC:;;
    WRITE_NODE_FIELD(tablefunc);
    break;
  case RTE_VALUES:;;
    WRITE_NODE_FIELD(values_lists);
    WRITE_NODE_FIELD(coltypes);
    WRITE_NODE_FIELD(coltypmods);
    WRITE_NODE_FIELD(colcollations);
    break;
  case RTE_CTE:;;
    WRITE_STRING_FIELD(ctename);
    WRITE_UINT_FIELD(ctelevelsup);
    WRITE_BOOL_FIELD(self_reference);
    WRITE_NODE_FIELD(coltypes);
    WRITE_NODE_FIELD(coltypmods);
    WRITE_NODE_FIELD(colcollations);
    break;
  case RTE_NAMEDTUPLESTORE:;;







  case RTE_RESULT:;;
    /* no extra fields */
    break;
  default:;;;


  }

  WRITE_BOOL_FIELD(lateral);
  WRITE_BOOL_FIELD(inh);
  WRITE_BOOL_FIELD(inFromCl);
  WRITE_UINT_FIELD(requiredPerms);
  WRITE_OID_FIELD(checkAsUser);
  WRITE_BITMAPSET_FIELD(selectedCols);
  WRITE_BITMAPSET_FIELD(insertedCols);
  WRITE_BITMAPSET_FIELD(updatedCols);
  WRITE_BITMAPSET_FIELD(extraUpdatedCols);
  WRITE_NODE_FIELD(securityQuals);
}

static void
_outRangeTblFunction(StringInfo str, const RangeTblFunction *node)
{
  WRITE_NODE_TYPE("RANGETBLFUNCTION");

  WRITE_NODE_FIELD(funcexpr);
  WRITE_INT_FIELD(funccolcount);
  WRITE_NODE_FIELD(funccolnames);
  WRITE_NODE_FIELD(funccoltypes);
  WRITE_NODE_FIELD(funccoltypmods);
  WRITE_NODE_FIELD(funccolcollations);
  WRITE_BITMAPSET_FIELD(funcparams);
}

static void
_outTableSampleClause(StringInfo str, const TableSampleClause *node)
{
  WRITE_NODE_TYPE("TABLESAMPLECLAUSE");

  WRITE_OID_FIELD(tsmhandler);
  WRITE_NODE_FIELD(args);
  WRITE_NODE_FIELD(repeatable);
}

static void
_outAExpr(StringInfo str, const A_Expr *node)
{













































































}

static void
_outValue(StringInfo str, const Value *value)
{
  switch (value->type)
  {
  case T_Integer:;;


  case T_Float:;;

    /*
     * We assume the value is a valid numeric literal and so does not
     * need quoting.
     */


  case T_String:;;

    /*
     * We use outToken to provide escaping of the string's content,
     * but we don't want it to do anything with an empty string.
     */
    appendStringInfoChar(str, '"');
    if (value->val.str[0] != '\0')
    {
      outToken(str, value->val.str);
    }
    appendStringInfoChar(str, '"');
    break;
  case T_BitString:;;
    /* internal representation already has leading 'b' */


  case T_Null:;;
    /* this is seen only within A_Const, not in transformed trees */


  default:;;;


  }
}

static void
_outColumnRef(StringInfo str, const ColumnRef *node)
{




}

static void
_outParamRef(StringInfo str, const ParamRef *node)
{




}

/*
 * Node types found in raw parse trees (supported for debug purposes)
 */

static void
_outRawStmt(StringInfo str, const RawStmt *node)
{





}

static void
_outAConst(StringInfo str, const A_Const *node)
{





}

static void
_outA_Star(StringInfo str, const A_Star *node)
{

}

static void
_outA_Indices(StringInfo str, const A_Indices *node)
{





}

static void
_outA_Indirection(StringInfo str, const A_Indirection *node)
{




}

static void
_outA_ArrayExpr(StringInfo str, const A_ArrayExpr *node)
{




}

static void
_outResTarget(StringInfo str, const ResTarget *node)
{






}

static void
_outMultiAssignRef(StringInfo str, const MultiAssignRef *node)
{





}

static void
_outSortBy(StringInfo str, const SortBy *node)
{







}

static void
_outWindowDef(StringInfo str, const WindowDef *node)
{










}

static void
_outRangeSubselect(StringInfo str, const RangeSubselect *node)
{





}

static void
_outRangeFunction(StringInfo str, const RangeFunction *node)
{








}

static void
_outRangeTableSample(StringInfo str, const RangeTableSample *node)
{







}

static void
_outRangeTableFunc(StringInfo str, const RangeTableFunc *node)
{









}

static void
_outRangeTableFuncCol(StringInfo str, const RangeTableFuncCol *node)
{









}

static void
_outConstraint(StringInfo str, const Constraint *node)
{


















































































































}

static void
_outForeignKeyCacheInfo(StringInfo str, const ForeignKeyCacheInfo *node)
{









}

static void
_outPartitionElem(StringInfo str, const PartitionElem *node)
{







}

static void
_outPartitionSpec(StringInfo str, const PartitionSpec *node)
{





}

static void
_outPartitionBoundSpec(StringInfo str, const PartitionBoundSpec *node)
{
  WRITE_NODE_TYPE("PARTITIONBOUNDSPEC");

  WRITE_CHAR_FIELD(strategy);
  WRITE_BOOL_FIELD(is_default);
  WRITE_INT_FIELD(modulus);
  WRITE_INT_FIELD(remainder);
  WRITE_NODE_FIELD(listdatums);
  WRITE_NODE_FIELD(lowerdatums);
  WRITE_NODE_FIELD(upperdatums);
  WRITE_LOCATION_FIELD(location);
}

static void
_outPartitionRangeDatum(StringInfo str, const PartitionRangeDatum *node)
{
  WRITE_NODE_TYPE("PARTITIONRANGEDATUM");

  WRITE_ENUM_FIELD(kind, PartitionRangeDatumKind);
  WRITE_NODE_FIELD(value);
  WRITE_LOCATION_FIELD(location);
}

/*
 * outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
void
outNode(StringInfo str, const void *obj)
{
  /* Guard against stack overflow due to overly complex expressions */
  check_stack_depth();

  if (obj == NULL)
  {
    appendStringInfoString(str, "<>");
  }
  else if (IsA(obj, List) || IsA(obj, IntList) || IsA(obj, OidList))
  {
    _outList(str, obj);
  }
  else if (IsA(obj, Integer) || IsA(obj, Float) || IsA(obj, String) || IsA(obj, BitString))
  {
    /* nodeRead does not want to see { } around these! */
    _outValue(str, obj);
  }
  else
  {
    appendStringInfoChar(str, '{');
    switch (nodeTag(obj))
    {
    case T_PlannedStmt:;;
      _outPlannedStmt(str, obj);
      break;
    case T_Plan:;;


    case T_Result:;;
      _outResult(str, obj);
      break;
    case T_ProjectSet:;;
      _outProjectSet(str, obj);
      break;
    case T_ModifyTable:;;


    case T_Append:;;
      _outAppend(str, obj);
      break;
    case T_MergeAppend:;;


    case T_RecursiveUnion:;;


    case T_BitmapAnd:;;


    case T_BitmapOr:;;


    case T_Gather:;;


    case T_GatherMerge:;;


    case T_Scan:;;


    case T_SeqScan:;;
      _outSeqScan(str, obj);
      break;
    case T_SampleScan:;;


    case T_IndexScan:;;
      _outIndexScan(str, obj);
      break;
    case T_IndexOnlyScan:;;
      _outIndexOnlyScan(str, obj);
      break;
    case T_BitmapIndexScan:;;
      _outBitmapIndexScan(str, obj);
      break;
    case T_BitmapHeapScan:;;
      _outBitmapHeapScan(str, obj);
      break;
    case T_TidScan:;;


    case T_SubqueryScan:;;


    case T_FunctionScan:;;


    case T_TableFuncScan:;;


    case T_ValuesScan:;;


    case T_CteScan:;;


    case T_NamedTuplestoreScan:;;


    case T_WorkTableScan:;;


    case T_ForeignScan:;;


    case T_CustomScan:;;


    case T_Join:;;


    case T_NestLoop:;;
      _outNestLoop(str, obj);
      break;
    case T_MergeJoin:;;
      _outMergeJoin(str, obj);
      break;
    case T_HashJoin:;;
      _outHashJoin(str, obj);
      break;
    case T_Agg:;;
      _outAgg(str, obj);
      break;
    case T_WindowAgg:;;


    case T_Group:;;


    case T_Material:;;


    case T_Sort:;;
      _outSort(str, obj);
      break;
    case T_Unique:;;


    case T_Hash:;;
      _outHash(str, obj);
      break;
    case T_SetOp:;;


    case T_LockRows:;;


    case T_Limit:;;


    case T_NestLoopParam:;;
      _outNestLoopParam(str, obj);
      break;
    case T_PlanRowMark:;;


    case T_PartitionPruneInfo:;;
      _outPartitionPruneInfo(str, obj);
      break;
    case T_PartitionedRelPruneInfo:;;
      _outPartitionedRelPruneInfo(str, obj);
      break;
    case T_PartitionPruneStepOp:;;
      _outPartitionPruneStepOp(str, obj);
      break;
    case T_PartitionPruneStepCombine:;;
      _outPartitionPruneStepCombine(str, obj);
      break;
    case T_PlanInvalItem:;;


    case T_Alias:;;
      _outAlias(str, obj);
      break;
    case T_RangeVar:;;


    case T_TableFunc:;;
      _outTableFunc(str, obj);
      break;
    case T_IntoClause:;;


    case T_Var:;;
      _outVar(str, obj);
      break;
    case T_Const:;;
      _outConst(str, obj);
      break;
    case T_Param:;;
      _outParam(str, obj);
      break;
    case T_Aggref:;;
      _outAggref(str, obj);
      break;
    case T_GroupingFunc:;;


    case T_WindowFunc:;;
      _outWindowFunc(str, obj);
      break;
    case T_SubscriptingRef:;;
      _outSubscriptingRef(str, obj);
      break;
    case T_FuncExpr:;;
      _outFuncExpr(str, obj);
      break;
    case T_NamedArgExpr:;;
      _outNamedArgExpr(str, obj);
      break;
    case T_OpExpr:;;
      _outOpExpr(str, obj);
      break;
    case T_DistinctExpr:;;
      _outDistinctExpr(str, obj);
      break;
    case T_NullIfExpr:;;
      _outNullIfExpr(str, obj);
      break;
    case T_ScalarArrayOpExpr:;;
      _outScalarArrayOpExpr(str, obj);
      break;
    case T_BoolExpr:;;
      _outBoolExpr(str, obj);
      break;
    case T_SubLink:;;
      _outSubLink(str, obj);
      break;
    case T_SubPlan:;;
      _outSubPlan(str, obj);
      break;
    case T_AlternativeSubPlan:;;


    case T_FieldSelect:;;
      _outFieldSelect(str, obj);
      break;
    case T_FieldStore:;;
      _outFieldStore(str, obj);
      break;
    case T_RelabelType:;;
      _outRelabelType(str, obj);
      break;
    case T_CoerceViaIO:;;
      _outCoerceViaIO(str, obj);
      break;
    case T_ArrayCoerceExpr:;;
      _outArrayCoerceExpr(str, obj);
      break;
    case T_ConvertRowtypeExpr:;;


    case T_CollateExpr:;;
      _outCollateExpr(str, obj);
      break;
    case T_CaseExpr:;;
      _outCaseExpr(str, obj);
      break;
    case T_CaseWhen:;;
      _outCaseWhen(str, obj);
      break;
    case T_CaseTestExpr:;;
      _outCaseTestExpr(str, obj);
      break;
    case T_ArrayExpr:;;
      _outArrayExpr(str, obj);
      break;
    case T_RowExpr:;;
      _outRowExpr(str, obj);
      break;
    case T_RowCompareExpr:;;
      _outRowCompareExpr(str, obj);
      break;
    case T_CoalesceExpr:;;
      _outCoalesceExpr(str, obj);
      break;
    case T_MinMaxExpr:;;
      _outMinMaxExpr(str, obj);
      break;
    case T_SQLValueFunction:;;
      _outSQLValueFunction(str, obj);
      break;
    case T_XmlExpr:;;
      _outXmlExpr(str, obj);
      break;
    case T_NullTest:;;
      _outNullTest(str, obj);
      break;
    case T_BooleanTest:;;
      _outBooleanTest(str, obj);
      break;
    case T_CoerceToDomain:;;
      _outCoerceToDomain(str, obj);
      break;
    case T_CoerceToDomainValue:;;
      _outCoerceToDomainValue(str, obj);
      break;
    case T_SetToDefault:;;
      _outSetToDefault(str, obj);
      break;
    case T_CurrentOfExpr:;;


    case T_NextValueExpr:;;


    case T_InferenceElem:;;
      _outInferenceElem(str, obj);
      break;
    case T_TargetEntry:;;
      _outTargetEntry(str, obj);
      break;
    case T_RangeTblRef:;;
      _outRangeTblRef(str, obj);
      break;
    case T_JoinExpr:;;
      _outJoinExpr(str, obj);
      break;
    case T_FromExpr:;;
      _outFromExpr(str, obj);
      break;
    case T_OnConflictExpr:;;
      _outOnConflictExpr(str, obj);
      break;
    case T_Path:;;


    case T_IndexPath:;;


    case T_BitmapHeapPath:;;


    case T_BitmapAndPath:;;


    case T_BitmapOrPath:;;


    case T_TidPath:;;


    case T_SubqueryScanPath:;;


    case T_ForeignPath:;;


    case T_CustomPath:;;


    case T_AppendPath:;;


    case T_MergeAppendPath:;;


    case T_GroupResultPath:;;


    case T_MaterialPath:;;


    case T_UniquePath:;;


    case T_GatherPath:;;


    case T_ProjectionPath:;;


    case T_ProjectSetPath:;;


    case T_SortPath:;;


    case T_GroupPath:;;


    case T_UpperUniquePath:;;


    case T_AggPath:;;


    case T_GroupingSetsPath:;;


    case T_MinMaxAggPath:;;


    case T_WindowAggPath:;;


    case T_SetOpPath:;;


    case T_RecursiveUnionPath:;;


    case T_LockRowsPath:;;


    case T_ModifyTablePath:;;


    case T_LimitPath:;;


    case T_GatherMergePath:;;


    case T_NestPath:;;


    case T_MergePath:;;


    case T_HashPath:;;


    case T_PlannerGlobal:;;


    case T_PlannerInfo:;;


    case T_RelOptInfo:;;


    case T_IndexOptInfo:;;


    case T_ForeignKeyOptInfo:;;


    case T_EquivalenceClass:;;


    case T_EquivalenceMember:;;


    case T_PathKey:;;


    case T_PathTarget:;;


    case T_ParamPathInfo:;;


    case T_RestrictInfo:;;


    case T_IndexClause:;;


    case T_PlaceHolderVar:;;


    case T_SpecialJoinInfo:;;


    case T_AppendRelInfo:;;


    case T_PlaceHolderInfo:;;


    case T_MinMaxAggInfo:;;


    case T_PlannerParamItem:;;


    case T_RollupData:;;


    case T_GroupingSetData:;;


    case T_StatisticExtInfo:;;


    case T_ExtensibleNode:;;


    case T_CreateStmt:;;


    case T_CreateForeignTableStmt:;;


    case T_ImportForeignSchemaStmt:;;


    case T_IndexStmt:;;


    case T_CreateStatsStmt:;;


    case T_NotifyStmt:;;
      _outNotifyStmt(str, obj);
      break;
    case T_DeclareCursorStmt:;;


    case T_SelectStmt:;;


    case T_ColumnDef:;;


    case T_TypeName:;;


    case T_TypeCast:;;


    case T_CollateClause:;;


    case T_IndexElem:;;


    case T_Query:;;
      _outQuery(str, obj);
      break;
    case T_WithCheckOption:;;


    case T_SortGroupClause:;;
      _outSortGroupClause(str, obj);
      break;
    case T_GroupingSet:;;


    case T_WindowClause:;;
      _outWindowClause(str, obj);
      break;
    case T_RowMarkClause:;;
      _outRowMarkClause(str, obj);
      break;
    case T_WithClause:;;


    case T_CommonTableExpr:;;
      _outCommonTableExpr(str, obj);
      break;
    case T_SetOperationStmt:;;
      _outSetOperationStmt(str, obj);
      break;
    case T_RangeTblEntry:;;
      _outRangeTblEntry(str, obj);
      break;
    case T_RangeTblFunction:;;
      _outRangeTblFunction(str, obj);
      break;
    case T_TableSampleClause:;;
      _outTableSampleClause(str, obj);
      break;
    case T_A_Expr:;;


    case T_ColumnRef:;;


    case T_ParamRef:;;


    case T_RawStmt:;;


    case T_A_Const:;;


    case T_A_Star:;;


    case T_A_Indices:;;


    case T_A_Indirection:;;


    case T_A_ArrayExpr:;;


    case T_ResTarget:;;


    case T_MultiAssignRef:;;


    case T_SortBy:;;


    case T_WindowDef:;;


    case T_RangeSubselect:;;


    case T_RangeFunction:;;


    case T_RangeTableSample:;;


    case T_RangeTableFunc:;;


    case T_RangeTableFuncCol:;;


    case T_Constraint:;;


    case T_FuncCall:;;


    case T_DefElem:;;


    case T_TableLikeClause:;;


    case T_LockingClause:;;


    case T_XmlSerialize:;;


    case T_ForeignKeyCacheInfo:;;


    case T_TriggerTransition:;;


    case T_PartitionElem:;;


    case T_PartitionSpec:;;


    case T_PartitionBoundSpec:;;
      _outPartitionBoundSpec(str, obj);
      break;
    case T_PartitionRangeDatum:;;
      _outPartitionRangeDatum(str, obj);
      break;

    default:;;;

      /*
       * This should be an ERROR, but it's too useful to be able to
       * dump structures that outNode only understands part of.
       */


    }
    appendStringInfoChar(str, '}');
  }
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToString(const void *obj)
{
  StringInfoData str;

  /* see stringinfo.h for an explanation of this maneuver */
  initStringInfo(&str);
  outNode(&str, obj);
  return str.data;
}

/*
 * bmsToString -
 *	   returns the ascii representation of the Bitmapset as a palloc'd
 *string
 */
char *
bmsToString(const Bitmapset *bms)
{






}