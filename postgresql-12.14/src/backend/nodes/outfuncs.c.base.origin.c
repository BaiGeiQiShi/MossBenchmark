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

























}

/*
 * Convert one char.  Goes through outToken() so that special characters are
 * escaped.
 */
static void
outChar(StringInfo str, char c)
{






}

static void
_outList(StringInfo str, const List *node)
{











































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










}

/*
 * Print the value of a Datum given its type.
 */
void
outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
































}

/*
 *	Stuff from plannodes.h
 */

static void
_outPlannedStmt(StringInfo str, const PlannedStmt *node)
{
























}

/*
 * print the basic stuff of all nodes that inherit from Plan
 */
static void
_outPlanInfo(StringInfo str, const Plan *node)
{














}

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_outScanInfo(StringInfo str, const Scan *node)
{



}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_outJoinPlanInfo(StringInfo str, const Join *node)
{





}

static void
_outPlan(StringInfo str, const Plan *node)
{



}

static void
_outResult(StringInfo str, const Result *node)
{





}

static void
_outProjectSet(StringInfo str, const ProjectSet *node)
{



}

static void
_outModifyTable(StringInfo str, const ModifyTable *node)
{

























}

static void
_outAppend(StringInfo str, const Append *node)
{







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



}

static void
_outSampleScan(StringInfo str, const SampleScan *node)
{





}

static void
_outIndexScan(StringInfo str, const IndexScan *node)
{











}

static void
_outIndexOnlyScan(StringInfo str, const IndexOnlyScan *node)
{










}

static void
_outBitmapIndexScan(StringInfo str, const BitmapIndexScan *node)
{








}

static void
_outBitmapHeapScan(StringInfo str, const BitmapHeapScan *node)
{





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





}

static void
_outMergeJoin(StringInfo str, const MergeJoin *node)
{















}

static void
_outHashJoin(StringInfo str, const HashJoin *node)
{








}

static void
_outAgg(StringInfo str, const Agg *node)
{














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









}

static void
_outUnique(StringInfo str, const Unique *node)
{








}

static void
_outHash(StringInfo str, const Hash *node)
{









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




}

static void
_outPlanRowMark(StringInfo str, const PlanRowMark *node)
{










}

static void
_outPartitionPruneInfo(StringInfo str, const PartitionPruneInfo *node)
{




}

static void
_outPartitionedRelPruneInfo(StringInfo str, const PartitionedRelPruneInfo *node)
{











}

static void
_outPartitionPruneStepOp(StringInfo str, const PartitionPruneStepOp *node)
{







}

static void
_outPartitionPruneStepCombine(StringInfo str, const PartitionPruneStepCombine *node)
{





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




}

static void
_outRangeVar(StringInfo str, const RangeVar *node)
{












}

static void
_outTableFunc(StringInfo str, const TableFunc *node)
{















}

static void
_outIntoClause(StringInfo str, const IntoClause *node)
{










}

static void
_outVar(StringInfo str, const Var *node)
{











}

static void
_outConst(StringInfo str, const Const *node)
{



















}

static void
_outParam(StringInfo str, const Param *node)
{








}

static void
_outAggref(StringInfo str, const Aggref *node)
{



















}

static void
_outGroupingFunc(StringInfo str, const GroupingFunc *node)
{







}

static void
_outWindowFunc(StringInfo str, const WindowFunc *node)
{












}

static void
_outSubscriptingRef(StringInfo str, const SubscriptingRef *node)
{










}

static void
_outFuncExpr(StringInfo str, const FuncExpr *node)
{











}

static void
_outNamedArgExpr(StringInfo str, const NamedArgExpr *node)
{






}

static void
_outOpExpr(StringInfo str, const OpExpr *node)
{










}

static void
_outDistinctExpr(StringInfo str, const DistinctExpr *node)
{










}

static void
_outNullIfExpr(StringInfo str, const NullIfExpr *node)
{










}

static void
_outScalarArrayOpExpr(StringInfo str, const ScalarArrayOpExpr *node)
{








}

static void
_outBoolExpr(StringInfo str, const BoolExpr *node)
{






















}

static void
_outSubLink(StringInfo str, const SubLink *node)
{








}

static void
_outSubPlan(StringInfo str, const SubPlan *node)
{



















}

static void
_outAlternativeSubPlan(StringInfo str, const AlternativeSubPlan *node)
{



}

static void
_outFieldSelect(StringInfo str, const FieldSelect *node)
{







}

static void
_outFieldStore(StringInfo str, const FieldStore *node)
{






}

static void
_outRelabelType(StringInfo str, const RelabelType *node)
{








}

static void
_outCoerceViaIO(StringInfo str, const CoerceViaIO *node)
{







}

static void
_outArrayCoerceExpr(StringInfo str, const ArrayCoerceExpr *node)
{









}

static void
_outConvertRowtypeExpr(StringInfo str, const ConvertRowtypeExpr *node)
{






}

static void
_outCollateExpr(StringInfo str, const CollateExpr *node)
{





}

static void
_outCaseExpr(StringInfo str, const CaseExpr *node)
{








}

static void
_outCaseWhen(StringInfo str, const CaseWhen *node)
{





}

static void
_outCaseTestExpr(StringInfo str, const CaseTestExpr *node)
{





}

static void
_outArrayExpr(StringInfo str, const ArrayExpr *node)
{








}

static void
_outRowExpr(StringInfo str, const RowExpr *node)
{







}

static void
_outRowCompareExpr(StringInfo str, const RowCompareExpr *node)
{








}

static void
_outCoalesceExpr(StringInfo str, const CoalesceExpr *node)
{






}

static void
_outMinMaxExpr(StringInfo str, const MinMaxExpr *node)
{








}

static void
_outSQLValueFunction(StringInfo str, const SQLValueFunction *node)
{






}

static void
_outXmlExpr(StringInfo str, const XmlExpr *node)
{











}

static void
_outNullTest(StringInfo str, const NullTest *node)
{






}

static void
_outBooleanTest(StringInfo str, const BooleanTest *node)
{





}

static void
_outCoerceToDomain(StringInfo str, const CoerceToDomain *node)
{








}

static void
_outCoerceToDomainValue(StringInfo str, const CoerceToDomainValue *node)
{






}

static void
_outSetToDefault(StringInfo str, const SetToDefault *node)
{






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





}

static void
_outTargetEntry(StringInfo str, const TargetEntry *node)
{









}

static void
_outRangeTblRef(StringInfo str, const RangeTblRef *node)
{



}

static void
_outJoinExpr(StringInfo str, const JoinExpr *node)
{










}

static void
_outFromExpr(StringInfo str, const FromExpr *node)
{




}

static void
_outOnConflictExpr(StringInfo str, const OnConflictExpr *node)
{










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

































































}

static void
_outWithCheckOption(StringInfo str, const WithCheckOption *node)
{







}

static void
_outSortGroupClause(StringInfo str, const SortGroupClause *node)
{







}

static void
_outGroupingSet(StringInfo str, const GroupingSet *node)
{





}

static void
_outWindowClause(StringInfo str, const WindowClause *node)
{
















}

static void
_outRowMarkClause(StringInfo str, const RowMarkClause *node)
{






}

static void
_outWithClause(StringInfo str, const WithClause *node)
{





}

static void
_outCommonTableExpr(StringInfo str, const CommonTableExpr *node)
{













}

static void
_outSetOperationStmt(StringInfo str, const SetOperationStmt *node)
{










}

static void
_outRangeTblEntry(StringInfo str, const RangeTblEntry *node)
{






































































}

static void
_outRangeTblFunction(StringInfo str, const RangeTblFunction *node)
{









}

static void
_outTableSampleClause(StringInfo str, const TableSampleClause *node)
{





}

static void
_outAExpr(StringInfo str, const A_Expr *node)
{













































































}

static void
_outValue(StringInfo str, const Value *value)
{






































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










}

static void
_outPartitionRangeDatum(StringInfo str, const PartitionRangeDatum *node)
{





}

/*
 * outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
void
outNode(StringInfo str, const void *obj)
{


























































































































































































































































































































































































































































































































































































































































































}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToString(const void *obj)
{






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