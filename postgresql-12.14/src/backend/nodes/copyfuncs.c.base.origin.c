/*-------------------------------------------------------------------------
 *
 * copyfuncs.c
 *	  Copy functions for Postgres tree nodes.
 *
 * NOTE: we currently support copying all node types found in parse and
 * plan trees.  We do not support copying executor state trees; there
 * is no need for that, and no point in maintaining all the code that
 * would be needed.  We also do not support copying Path trees, mainly
 * because the circular linkages between RelOptInfo and Path nodes can't
 * be handled easily in a simple depth-first traversal.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/nodes/copyfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "nodes/extensible.h"
#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"
#include "utils/datum.h"
#include "utils/rel.h"

/*
 * Macros to simplify copying of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.  Note that these
 * hard-wire the convention that the local variables in a Copy routine are
 * named 'newnode' and 'from'.
 */

/* Copy a simple scalar field (int, float, bool, enum, etc) */
#define COPY_SCALAR_FIELD(fldname) (newnode->fldname = from->fldname)

/* Copy a field that is a pointer to some kind of Node or Node tree */
#define COPY_NODE_FIELD(fldname) (newnode->fldname = copyObjectImpl(from->fldname))

/* Copy a field that is a pointer to a Bitmapset */
#define COPY_BITMAPSET_FIELD(fldname) (newnode->fldname = bms_copy(from->fldname))

/* Copy a field that is a pointer to a C string, or perhaps NULL */
#define COPY_STRING_FIELD(fldname) (newnode->fldname = from->fldname ? pstrdup(from->fldname) : (char *)NULL)

/* Copy a field that is a pointer to a simple palloc'd object of size sz */
#define COPY_POINTER_FIELD(fldname, sz)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    Size _size = (sz);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (_size > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      newnode->fldname = palloc(_size);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      memcpy(newnode->fldname, from->fldname, _size);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/* Copy a parse location field (for Copy, this is same as scalar case) */
#define COPY_LOCATION_FIELD(fldname) (newnode->fldname = from->fldname)

/* ****************************************************************
 *					 plannodes.h copy functions
 * ****************************************************************
 */

/*
 * _copyPlannedStmt
 */
static PlannedStmt *
_copyPlannedStmt(const PlannedStmt *from)
{


























}

/*
 * CopyPlanFields
 *
 *		This function copies the fields of the Plan node.  It is used by
 *		all the copy functions for classes which inherit from Plan.
 */
static void
CopyPlanFields(const Plan *from, Plan *newnode)
{














}

/*
 * _copyPlan
 */
static Plan *
_copyPlan(const Plan *from)
{








}

/*
 * _copyResult
 */
static Result *
_copyResult(const Result *from)
{













}

/*
 * _copyProjectSet
 */
static ProjectSet *
_copyProjectSet(const ProjectSet *from)
{








}

/*
 * _copyModifyTable
 */
static ModifyTable *
_copyModifyTable(const ModifyTable *from)
{

































}

/*
 * _copyAppend
 */
static Append *
_copyAppend(const Append *from)
{















}

/*
 * _copyMergeAppend
 */
static MergeAppend *
_copyMergeAppend(const MergeAppend *from)
{



















}

/*
 * _copyRecursiveUnion
 */
static RecursiveUnion *
_copyRecursiveUnion(const RecursiveUnion *from)
{


















}

/*
 * _copyBitmapAnd
 */
static BitmapAnd *
_copyBitmapAnd(const BitmapAnd *from)
{













}

/*
 * _copyBitmapOr
 */
static BitmapOr *
_copyBitmapOr(const BitmapOr *from)
{














}

/*
 * _copyGather
 */
static Gather *
_copyGather(const Gather *from)
{

















}

/*
 * _copyGatherMerge
 */
static GatherMerge *
_copyGatherMerge(const GatherMerge *from)
{




















}

/*
 * CopyScanFields
 *
 *		This function copies the fields of the Scan node.  It is used by
 *		all the copy functions for classes which inherit from Scan.
 */
static void
CopyScanFields(const Scan *from, Scan *newnode)
{



}

/*
 * _copyScan
 */
static Scan *
_copyScan(const Scan *from)
{








}

/*
 * _copySeqScan
 */
static SeqScan *
_copySeqScan(const SeqScan *from)
{








}

/*
 * _copySampleScan
 */
static SampleScan *
_copySampleScan(const SampleScan *from)
{













}

/*
 * _copyIndexScan
 */
static IndexScan *
_copyIndexScan(const IndexScan *from)
{



















}

/*
 * _copyIndexOnlyScan
 */
static IndexOnlyScan *
_copyIndexOnlyScan(const IndexOnlyScan *from)
{


















}

/*
 * _copyBitmapIndexScan
 */
static BitmapIndexScan *
_copyBitmapIndexScan(const BitmapIndexScan *from)
{
















}

/*
 * _copyBitmapHeapScan
 */
static BitmapHeapScan *
_copyBitmapHeapScan(const BitmapHeapScan *from)
{













}

/*
 * _copyTidScan
 */
static TidScan *
_copyTidScan(const TidScan *from)
{













}

/*
 * _copySubqueryScan
 */
static SubqueryScan *
_copySubqueryScan(const SubqueryScan *from)
{













}

/*
 * _copyFunctionScan
 */
static FunctionScan *
_copyFunctionScan(const FunctionScan *from)
{














}

/*
 * _copyTableFuncScan
 */
static TableFuncScan *
_copyTableFuncScan(const TableFuncScan *from)
{













}

/*
 * _copyValuesScan
 */
static ValuesScan *
_copyValuesScan(const ValuesScan *from)
{













}

/*
 * _copyCteScan
 */
static CteScan *
_copyCteScan(const CteScan *from)
{














}

/*
 * _copyNamedTuplestoreScan
 */
static NamedTuplestoreScan *
_copyNamedTuplestoreScan(const NamedTuplestoreScan *from)
{













}

/*
 * _copyWorkTableScan
 */
static WorkTableScan *
_copyWorkTableScan(const WorkTableScan *from)
{













}

/*
 * _copyForeignScan
 */
static ForeignScan *
_copyForeignScan(const ForeignScan *from)
{




















}

/*
 * _copyCustomScan
 */
static CustomScan *
_copyCustomScan(const CustomScan *from)
{

























}

/*
 * CopyJoinFields
 *
 *		This function copies the fields of the Join node.  It is used by
 *		all the copy functions for classes which inherit from Join.
 */
static void
CopyJoinFields(const Join *from, Join *newnode)
{





}

/*
 * _copyJoin
 */
static Join *
_copyJoin(const Join *from)
{








}

/*
 * _copyNestLoop
 */
static NestLoop *
_copyNestLoop(const NestLoop *from)
{













}

/*
 * _copyMergeJoin
 */
static MergeJoin *
_copyMergeJoin(const MergeJoin *from)
{




















}

/*
 * _copyHashJoin
 */
static HashJoin *
_copyHashJoin(const HashJoin *from)
{
















}

/*
 * _copyMaterial
 */
static Material *
_copyMaterial(const Material *from)
{








}

/*
 * _copySort
 */
static Sort *
_copySort(const Sort *from)
{














}

/*
 * _copyGroup
 */
static Group *
_copyGroup(const Group *from)
{










}

/*
 * _copyAgg
 */
static Agg *
_copyAgg(const Agg *from)
{
















}

/*
 * _copyWindowAgg
 */
static WindowAgg *
_copyWindowAgg(const WindowAgg *from)
{























}

/*
 * _copyUnique
 */
static Unique *
_copyUnique(const Unique *from)
{
















}

/*
 * _copyHash
 */
static Hash *
_copyHash(const Hash *from)
{

















}

/*
 * _copySetOp
 */
static SetOp *
_copySetOp(const SetOp *from)
{





















}

/*
 * _copyLockRows
 */
static LockRows *
_copyLockRows(const LockRows *from)
{














}

/*
 * _copyLimit
 */
static Limit *
_copyLimit(const Limit *from)
{














}

/*
 * _copyNestLoopParam
 */
static NestLoopParam *
_copyNestLoopParam(const NestLoopParam *from)
{






}

/*
 * _copyPlanRowMark
 */
static PlanRowMark *
_copyPlanRowMark(const PlanRowMark *from)
{












}

static PartitionPruneInfo *
_copyPartitionPruneInfo(const PartitionPruneInfo *from)
{






}

static PartitionedRelPruneInfo *
_copyPartitionedRelPruneInfo(const PartitionedRelPruneInfo *from)
{













}

/*
 * _copyPartitionPruneStepOp
 */
static PartitionPruneStepOp *
_copyPartitionPruneStepOp(const PartitionPruneStepOp *from)
{









}

/*
 * _copyPartitionPruneStepCombine
 */
static PartitionPruneStepCombine *
_copyPartitionPruneStepCombine(const PartitionPruneStepCombine *from)
{







}

/*
 * _copyPlanInvalItem
 */
static PlanInvalItem *
_copyPlanInvalItem(const PlanInvalItem *from)
{






}

/* ****************************************************************
 *					   primnodes.h copy functions
 * ****************************************************************
 */

/*
 * _copyAlias
 */
static Alias *
_copyAlias(const Alias *from)
{






}

/*
 * _copyRangeVar
 */
static RangeVar *
_copyRangeVar(const RangeVar *from)
{











}

/*
 * _copyTableFunc
 */
static TableFunc *
_copyTableFunc(const TableFunc *from)
{

















}

/*
 * _copyIntoClause
 */
static IntoClause *
_copyIntoClause(const IntoClause *from)
{












}

/*
 * We don't need a _copyExpr because Expr is an abstract supertype which
 * should never actually get instantiated.  Also, since it has no common
 * fields except NodeTag, there's no need for a helper routine to factor
 * out copying the common fields...
 */

/*
 * _copyVar
 */
static Var *
_copyVar(const Var *from)
{













}

/*
 * _copyConst
 */
static Const *
_copyConst(const Const *from)
{




























}

/*
 * _copyParam
 */
static Param *
_copyParam(const Param *from)
{










}

/*
 * _copyAggref
 */
static Aggref *
_copyAggref(const Aggref *from)
{





















}

/*
 * _copyGroupingFunc
 */
static GroupingFunc *
_copyGroupingFunc(const GroupingFunc *from)
{









}

/*
 * _copyWindowFunc
 */
static WindowFunc *
_copyWindowFunc(const WindowFunc *from)
{














}

/*
 * _copySubscriptingRef
 */
static SubscriptingRef *
_copySubscriptingRef(const SubscriptingRef *from)
{












}

/*
 * _copyFuncExpr
 */
static FuncExpr *
_copyFuncExpr(const FuncExpr *from)
{













}

/*
 * _copyNamedArgExpr *
 */
static NamedArgExpr *
_copyNamedArgExpr(const NamedArgExpr *from)
{








}

/*
 * _copyOpExpr
 */
static OpExpr *
_copyOpExpr(const OpExpr *from)
{












}

/*
 * _copyDistinctExpr (same as OpExpr)
 */
static DistinctExpr *
_copyDistinctExpr(const DistinctExpr *from)
{












}

/*
 * _copyNullIfExpr (same as OpExpr)
 */
static NullIfExpr *
_copyNullIfExpr(const NullIfExpr *from)
{












}

/*
 * _copyScalarArrayOpExpr
 */
static ScalarArrayOpExpr *
_copyScalarArrayOpExpr(const ScalarArrayOpExpr *from)
{










}

/*
 * _copyBoolExpr
 */
static BoolExpr *
_copyBoolExpr(const BoolExpr *from)
{







}

/*
 * _copySubLink
 */
static SubLink *
_copySubLink(const SubLink *from)
{










}

/*
 * _copySubPlan
 */
static SubPlan *
_copySubPlan(const SubPlan *from)
{





















}

/*
 * _copyAlternativeSubPlan
 */
static AlternativeSubPlan *
_copyAlternativeSubPlan(const AlternativeSubPlan *from)
{





}

/*
 * _copyFieldSelect
 */
static FieldSelect *
_copyFieldSelect(const FieldSelect *from)
{









}

/*
 * _copyFieldStore
 */
static FieldStore *
_copyFieldStore(const FieldStore *from)
{








}

/*
 * _copyRelabelType
 */
static RelabelType *
_copyRelabelType(const RelabelType *from)
{










}

/*
 * _copyCoerceViaIO
 */
static CoerceViaIO *
_copyCoerceViaIO(const CoerceViaIO *from)
{









}

/*
 * _copyArrayCoerceExpr
 */
static ArrayCoerceExpr *
_copyArrayCoerceExpr(const ArrayCoerceExpr *from)
{











}

/*
 * _copyConvertRowtypeExpr
 */
static ConvertRowtypeExpr *
_copyConvertRowtypeExpr(const ConvertRowtypeExpr *from)
{








}

/*
 * _copyCollateExpr
 */
static CollateExpr *
_copyCollateExpr(const CollateExpr *from)
{







}

/*
 * _copyCaseExpr
 */
static CaseExpr *
_copyCaseExpr(const CaseExpr *from)
{










}

/*
 * _copyCaseWhen
 */
static CaseWhen *
_copyCaseWhen(const CaseWhen *from)
{







}

/*
 * _copyCaseTestExpr
 */
static CaseTestExpr *
_copyCaseTestExpr(const CaseTestExpr *from)
{







}

/*
 * _copyArrayExpr
 */
static ArrayExpr *
_copyArrayExpr(const ArrayExpr *from)
{










}

/*
 * _copyRowExpr
 */
static RowExpr *
_copyRowExpr(const RowExpr *from)
{









}

/*
 * _copyRowCompareExpr
 */
static RowCompareExpr *
_copyRowCompareExpr(const RowCompareExpr *from)
{










}

/*
 * _copyCoalesceExpr
 */
static CoalesceExpr *
_copyCoalesceExpr(const CoalesceExpr *from)
{








}

/*
 * _copyMinMaxExpr
 */
static MinMaxExpr *
_copyMinMaxExpr(const MinMaxExpr *from)
{










}

/*
 * _copySQLValueFunction
 */
static SQLValueFunction *
_copySQLValueFunction(const SQLValueFunction *from)
{








}

/*
 * _copyXmlExpr
 */
static XmlExpr *
_copyXmlExpr(const XmlExpr *from)
{













}

/*
 * _copyNullTest
 */
static NullTest *
_copyNullTest(const NullTest *from)
{








}

/*
 * _copyBooleanTest
 */
static BooleanTest *
_copyBooleanTest(const BooleanTest *from)
{







}

/*
 * _copyCoerceToDomain
 */
static CoerceToDomain *
_copyCoerceToDomain(const CoerceToDomain *from)
{










}

/*
 * _copyCoerceToDomainValue
 */
static CoerceToDomainValue *
_copyCoerceToDomainValue(const CoerceToDomainValue *from)
{








}

/*
 * _copySetToDefault
 */
static SetToDefault *
_copySetToDefault(const SetToDefault *from)
{








}

/*
 * _copyCurrentOfExpr
 */
static CurrentOfExpr *
_copyCurrentOfExpr(const CurrentOfExpr *from)
{







}

/*
 * _copyNextValueExpr
 */
static NextValueExpr *
_copyNextValueExpr(const NextValueExpr *from)
{






}

/*
 * _copyInferenceElem
 */
static InferenceElem *
_copyInferenceElem(const InferenceElem *from)
{







}

/*
 * _copyTargetEntry
 */
static TargetEntry *
_copyTargetEntry(const TargetEntry *from)
{











}

/*
 * _copyRangeTblRef
 */
static RangeTblRef *
_copyRangeTblRef(const RangeTblRef *from)
{





}

/*
 * _copyJoinExpr
 */
static JoinExpr *
_copyJoinExpr(const JoinExpr *from)
{












}

/*
 * _copyFromExpr
 */
static FromExpr *
_copyFromExpr(const FromExpr *from)
{






}

/*
 * _copyOnConflictExpr
 */
static OnConflictExpr *
_copyOnConflictExpr(const OnConflictExpr *from)
{












}

/* ****************************************************************
 *						pathnodes.h copy functions
 *
 * We don't support copying RelOptInfo, IndexOptInfo, or Path nodes.
 * There are some subsidiary structs that are useful to copy, though.
 * ****************************************************************
 */

/*
 * _copyPathKey
 */
static PathKey *
_copyPathKey(const PathKey *from)
{









}

/*
 * _copyRestrictInfo
 */
static RestrictInfo *
_copyRestrictInfo(const RestrictInfo *from)
{





































}

/*
 * _copyPlaceHolderVar
 */
static PlaceHolderVar *
_copyPlaceHolderVar(const PlaceHolderVar *from)
{








}

/*
 * _copySpecialJoinInfo
 */
static SpecialJoinInfo *
_copySpecialJoinInfo(const SpecialJoinInfo *from)
{















}

/*
 * _copyAppendRelInfo
 */
static AppendRelInfo *
_copyAppendRelInfo(const AppendRelInfo *from)
{










}

/*
 * _copyPlaceHolderInfo
 */
static PlaceHolderInfo *
_copyPlaceHolderInfo(const PlaceHolderInfo *from)
{










}

/* ****************************************************************
 *					parsenodes.h copy functions
 * ****************************************************************
 */

static RangeTblEntry *
_copyRangeTblEntry(const RangeTblEntry *from)
{





































}

static RangeTblFunction *
_copyRangeTblFunction(const RangeTblFunction *from)
{











}

static TableSampleClause *
_copyTableSampleClause(const TableSampleClause *from)
{







}

static WithCheckOption *
_copyWithCheckOption(const WithCheckOption *from)
{









}

static SortGroupClause *
_copySortGroupClause(const SortGroupClause *from)
{









}

static GroupingSet *
_copyGroupingSet(const GroupingSet *from)
{







}

static WindowClause *
_copyWindowClause(const WindowClause *from)
{


















}

static RowMarkClause *
_copyRowMarkClause(const RowMarkClause *from)
{








}

static WithClause *
_copyWithClause(const WithClause *from)
{







}

static InferClause *
_copyInferClause(const InferClause *from)
{








}

static OnConflictClause *
_copyOnConflictClause(const OnConflictClause *from)
{









}

static CommonTableExpr *
_copyCommonTableExpr(const CommonTableExpr *from)
{















}

static A_Expr *
_copyAExpr(const A_Expr *from)
{









}

static ColumnRef *
_copyColumnRef(const ColumnRef *from)
{






}

static ParamRef *
_copyParamRef(const ParamRef *from)
{






}

static A_Const *
_copyAConst(const A_Const *from)
{

























}

static FuncCall *
_copyFuncCall(const FuncCall *from)
{














}

static A_Star *
_copyAStar(const A_Star *from)
{



}

static A_Indices *
_copyAIndices(const A_Indices *from)
{







}

static A_Indirection *
_copyA_Indirection(const A_Indirection *from)
{






}

static A_ArrayExpr *
_copyA_ArrayExpr(const A_ArrayExpr *from)
{






}

static ResTarget *
_copyResTarget(const ResTarget *from)
{








}

static MultiAssignRef *
_copyMultiAssignRef(const MultiAssignRef *from)
{







}

static TypeName *
_copyTypeName(const TypeName *from)
{












}

static SortBy *
_copySortBy(const SortBy *from)
{









}

static WindowDef *
_copyWindowDef(const WindowDef *from)
{












}

static RangeSubselect *
_copyRangeSubselect(const RangeSubselect *from)
{







}

static RangeFunction *
_copyRangeFunction(const RangeFunction *from)
{










}

static RangeTableSample *
_copyRangeTableSample(const RangeTableSample *from)
{









}

static RangeTableFunc *
_copyRangeTableFunc(const RangeTableFunc *from)
{











}

static RangeTableFuncCol *
_copyRangeTableFuncCol(const RangeTableFuncCol *from)
{











}

static TypeCast *
_copyTypeCast(const TypeCast *from)
{







}

static CollateClause *
_copyCollateClause(const CollateClause *from)
{







}

static IndexElem *
_copyIndexElem(const IndexElem *from)
{











}

static ColumnDef *
_copyColumnDef(const ColumnDef *from)
{





















}

static Constraint *
_copyConstraint(const Constraint *from)
{
































}

static DefElem *
_copyDefElem(const DefElem *from)
{









}

static LockingClause *
_copyLockingClause(const LockingClause *from)
{







}

static XmlSerialize *
_copyXmlSerialize(const XmlSerialize *from)
{








}

static RoleSpec *
_copyRoleSpec(const RoleSpec *from)
{







}

static TriggerTransition *
_copyTriggerTransition(const TriggerTransition *from)
{







}

static Query *
_copyQuery(const Query *from)
{








































}

static RawStmt *
_copyRawStmt(const RawStmt *from)
{







}

static InsertStmt *
_copyInsertStmt(const InsertStmt *from)
{











}

static DeleteStmt *
_copyDeleteStmt(const DeleteStmt *from)
{









}

static UpdateStmt *
_copyUpdateStmt(const UpdateStmt *from)
{










}

static SelectStmt *
_copySelectStmt(const SelectStmt *from)
{






















}

static SetOperationStmt *
_copySetOperationStmt(const SetOperationStmt *from)
{












}

static AlterTableStmt *
_copyAlterTableStmt(const AlterTableStmt *from)
{








}

static AlterTableCmd *
_copyAlterTableCmd(const AlterTableCmd *from)
{












}

static AlterCollationStmt *
_copyAlterCollationStmt(const AlterCollationStmt *from)
{





}

static AlterDomainStmt *
_copyAlterDomainStmt(const AlterDomainStmt *from)
{










}

static GrantStmt *
_copyGrantStmt(const GrantStmt *from)
{












}

static ObjectWithArgs *
_copyObjectWithArgs(const ObjectWithArgs *from)
{







}

static AccessPriv *
_copyAccessPriv(const AccessPriv *from)
{






}

static GrantRoleStmt *
_copyGrantRoleStmt(const GrantRoleStmt *from)
{










}

static AlterDefaultPrivilegesStmt *
_copyAlterDefaultPrivilegesStmt(const AlterDefaultPrivilegesStmt *from)
{






}

static DeclareCursorStmt *
_copyDeclareCursorStmt(const DeclareCursorStmt *from)
{







}

static ClosePortalStmt *
_copyClosePortalStmt(const ClosePortalStmt *from)
{





}

static CallStmt *
_copyCallStmt(const CallStmt *from)
{






}

static ClusterStmt *
_copyClusterStmt(const ClusterStmt *from)
{







}

static CopyStmt *
_copyCopyStmt(const CopyStmt *from)
{












}

/*
 * CopyCreateStmtFields
 *
 *		This function copies the fields of the CreateStmt node.  It is
 *used by copy functions for classes which inherit from CreateStmt.
 */
static void
CopyCreateStmtFields(const CreateStmt *from, CreateStmt *newnode)
{












}

static CreateStmt *
_copyCreateStmt(const CreateStmt *from)
{





}

static TableLikeClause *
_copyTableLikeClause(const TableLikeClause *from)
{







}

static DefineStmt *
_copyDefineStmt(const DefineStmt *from)
{











}

static DropStmt *
_copyDropStmt(const DropStmt *from)
{









}

static TruncateStmt *
_copyTruncateStmt(const TruncateStmt *from)
{







}

static CommentStmt *
_copyCommentStmt(const CommentStmt *from)
{







}

static SecLabelStmt *
_copySecLabelStmt(const SecLabelStmt *from)
{








}

static FetchStmt *
_copyFetchStmt(const FetchStmt *from)
{








}

static IndexStmt *
_copyIndexStmt(const IndexStmt *from)
{

























}

static CreateStatsStmt *
_copyCreateStatsStmt(const CreateStatsStmt *from)
{










}

static CreateFunctionStmt *
_copyCreateFunctionStmt(const CreateFunctionStmt *from)
{










}

static FunctionParameter *
_copyFunctionParameter(const FunctionParameter *from)
{








}

static AlterFunctionStmt *
_copyAlterFunctionStmt(const AlterFunctionStmt *from)
{







}

static DoStmt *
_copyDoStmt(const DoStmt *from)
{





}

static RenameStmt *
_copyRenameStmt(const RenameStmt *from)
{












}

static AlterObjectDependsStmt *
_copyAlterObjectDependsStmt(const AlterObjectDependsStmt *from)
{








}

static AlterObjectSchemaStmt *
_copyAlterObjectSchemaStmt(const AlterObjectSchemaStmt *from)
{









}

static AlterOwnerStmt *
_copyAlterOwnerStmt(const AlterOwnerStmt *from)
{








}

static AlterOperatorStmt *
_copyAlterOperatorStmt(const AlterOperatorStmt *from)
{






}

static RuleStmt *
_copyRuleStmt(const RuleStmt *from)
{











}

static NotifyStmt *
_copyNotifyStmt(const NotifyStmt *from)
{






}

static ListenStmt *
_copyListenStmt(const ListenStmt *from)
{





}

static UnlistenStmt *
_copyUnlistenStmt(const UnlistenStmt *from)
{





}

static TransactionStmt *
_copyTransactionStmt(const TransactionStmt *from)
{









}

static CompositeTypeStmt *
_copyCompositeTypeStmt(const CompositeTypeStmt *from)
{






}

static CreateEnumStmt *
_copyCreateEnumStmt(const CreateEnumStmt *from)
{






}

static CreateRangeStmt *
_copyCreateRangeStmt(const CreateRangeStmt *from)
{






}

static AlterEnumStmt *
_copyAlterEnumStmt(const AlterEnumStmt *from)
{










}

static ViewStmt *
_copyViewStmt(const ViewStmt *from)
{










}

static LoadStmt *
_copyLoadStmt(const LoadStmt *from)
{





}

static CreateDomainStmt *
_copyCreateDomainStmt(const CreateDomainStmt *from)
{








}

static CreateOpClassStmt *
_copyCreateOpClassStmt(const CreateOpClassStmt *from)
{










}

static CreateOpClassItem *
_copyCreateOpClassItem(const CreateOpClassItem *from)
{










}

static CreateOpFamilyStmt *
_copyCreateOpFamilyStmt(const CreateOpFamilyStmt *from)
{






}

static AlterOpFamilyStmt *
_copyAlterOpFamilyStmt(const AlterOpFamilyStmt *from)
{








}

static CreatedbStmt *
_copyCreatedbStmt(const CreatedbStmt *from)
{






}

static AlterDatabaseStmt *
_copyAlterDatabaseStmt(const AlterDatabaseStmt *from)
{






}

static AlterDatabaseSetStmt *
_copyAlterDatabaseSetStmt(const AlterDatabaseSetStmt *from)
{






}

static DropdbStmt *
_copyDropdbStmt(const DropdbStmt *from)
{






}

static VacuumStmt *
_copyVacuumStmt(const VacuumStmt *from)
{







}

static VacuumRelation *
_copyVacuumRelation(const VacuumRelation *from)
{







}

static ExplainStmt *
_copyExplainStmt(const ExplainStmt *from)
{






}

static CreateTableAsStmt *
_copyCreateTableAsStmt(const CreateTableAsStmt *from)
{









}

static RefreshMatViewStmt *
_copyRefreshMatViewStmt(const RefreshMatViewStmt *from)
{







}

static ReplicaIdentityStmt *
_copyReplicaIdentityStmt(const ReplicaIdentityStmt *from)
{






}

static AlterSystemStmt *
_copyAlterSystemStmt(const AlterSystemStmt *from)
{





}

static CreateSeqStmt *
_copyCreateSeqStmt(const CreateSeqStmt *from)
{









}

static AlterSeqStmt *
_copyAlterSeqStmt(const AlterSeqStmt *from)
{








}

static VariableSetStmt *
_copyVariableSetStmt(const VariableSetStmt *from)
{








}

static VariableShowStmt *
_copyVariableShowStmt(const VariableShowStmt *from)
{





}

static DiscardStmt *
_copyDiscardStmt(const DiscardStmt *from)
{





}

static CreateTableSpaceStmt *
_copyCreateTableSpaceStmt(const CreateTableSpaceStmt *from)
{








}

static DropTableSpaceStmt *
_copyDropTableSpaceStmt(const DropTableSpaceStmt *from)
{






}

static AlterTableSpaceOptionsStmt *
_copyAlterTableSpaceOptionsStmt(const AlterTableSpaceOptionsStmt *from)
{







}

static AlterTableMoveAllStmt *
_copyAlterTableMoveAllStmt(const AlterTableMoveAllStmt *from)
{









}

static CreateExtensionStmt *
_copyCreateExtensionStmt(const CreateExtensionStmt *from)
{







}

static AlterExtensionStmt *
_copyAlterExtensionStmt(const AlterExtensionStmt *from)
{






}

static AlterExtensionContentsStmt *
_copyAlterExtensionContentsStmt(const AlterExtensionContentsStmt *from)
{








}

static CreateFdwStmt *
_copyCreateFdwStmt(const CreateFdwStmt *from)
{







}

static AlterFdwStmt *
_copyAlterFdwStmt(const AlterFdwStmt *from)
{







}

static CreateForeignServerStmt *
_copyCreateForeignServerStmt(const CreateForeignServerStmt *from)
{










}

static AlterForeignServerStmt *
_copyAlterForeignServerStmt(const AlterForeignServerStmt *from)
{








}

static CreateUserMappingStmt *
_copyCreateUserMappingStmt(const CreateUserMappingStmt *from)
{








}

static AlterUserMappingStmt *
_copyAlterUserMappingStmt(const AlterUserMappingStmt *from)
{







}

static DropUserMappingStmt *
_copyDropUserMappingStmt(const DropUserMappingStmt *from)
{







}

static CreateForeignTableStmt *
_copyCreateForeignTableStmt(const CreateForeignTableStmt *from)
{








}

static ImportForeignSchemaStmt *
_copyImportForeignSchemaStmt(const ImportForeignSchemaStmt *from)
{










}

static CreateTransformStmt *
_copyCreateTransformStmt(const CreateTransformStmt *from)
{









}

static CreateAmStmt *
_copyCreateAmStmt(const CreateAmStmt *from)
{







}

static CreateTrigStmt *
_copyCreateTrigStmt(const CreateTrigStmt *from)
{


















}

static CreateEventTrigStmt *
_copyCreateEventTrigStmt(const CreateEventTrigStmt *from)
{








}

static AlterEventTrigStmt *
_copyAlterEventTrigStmt(const AlterEventTrigStmt *from)
{






}

static CreatePLangStmt *
_copyCreatePLangStmt(const CreatePLangStmt *from)
{










}

static CreateRoleStmt *
_copyCreateRoleStmt(const CreateRoleStmt *from)
{







}

static AlterRoleStmt *
_copyAlterRoleStmt(const AlterRoleStmt *from)
{







}

static AlterRoleSetStmt *
_copyAlterRoleSetStmt(const AlterRoleSetStmt *from)
{







}

static DropRoleStmt *
_copyDropRoleStmt(const DropRoleStmt *from)
{






}

static LockStmt *
_copyLockStmt(const LockStmt *from)
{







}

static ConstraintsSetStmt *
_copyConstraintsSetStmt(const ConstraintsSetStmt *from)
{






}

static ReindexStmt *
_copyReindexStmt(const ReindexStmt *from)
{









}

static CreateSchemaStmt *
_copyCreateSchemaStmt(const CreateSchemaStmt *from)
{








}

static CreateConversionStmt *
_copyCreateConversionStmt(const CreateConversionStmt *from)
{









}

static CreateCastStmt *
_copyCreateCastStmt(const CreateCastStmt *from)
{









}

static PrepareStmt *
_copyPrepareStmt(const PrepareStmt *from)
{







}

static ExecuteStmt *
_copyExecuteStmt(const ExecuteStmt *from)
{






}

static DeallocateStmt *
_copyDeallocateStmt(const DeallocateStmt *from)
{





}

static DropOwnedStmt *
_copyDropOwnedStmt(const DropOwnedStmt *from)
{






}

static ReassignOwnedStmt *
_copyReassignOwnedStmt(const ReassignOwnedStmt *from)
{






}

static AlterTSDictionaryStmt *
_copyAlterTSDictionaryStmt(const AlterTSDictionaryStmt *from)
{






}

static AlterTSConfigurationStmt *
_copyAlterTSConfigurationStmt(const AlterTSConfigurationStmt *from)
{











}

static CreatePolicyStmt *
_copyCreatePolicyStmt(const CreatePolicyStmt *from)
{











}

static AlterPolicyStmt *
_copyAlterPolicyStmt(const AlterPolicyStmt *from)
{









}

static PartitionElem *
_copyPartitionElem(const PartitionElem *from)
{









}

static PartitionSpec *
_copyPartitionSpec(const PartitionSpec *from)
{







}

static PartitionBoundSpec *
_copyPartitionBoundSpec(const PartitionBoundSpec *from)
{












}

static PartitionRangeDatum *
_copyPartitionRangeDatum(const PartitionRangeDatum *from)
{







}

static PartitionCmd *
_copyPartitionCmd(const PartitionCmd *from)
{






}

static CreatePublicationStmt *
_copyCreatePublicationStmt(const CreatePublicationStmt *from)
{








}

static AlterPublicationStmt *
_copyAlterPublicationStmt(const AlterPublicationStmt *from)
{









}

static CreateSubscriptionStmt *
_copyCreateSubscriptionStmt(const CreateSubscriptionStmt *from)
{








}

static AlterSubscriptionStmt *
_copyAlterSubscriptionStmt(const AlterSubscriptionStmt *from)
{









}

static DropSubscriptionStmt *
_copyDropSubscriptionStmt(const DropSubscriptionStmt *from)
{







}

/* ****************************************************************
 *					pg_list.h copy functions
 * ****************************************************************
 */

/*
 * Perform a deep copy of the specified list, using copyObject(). The
 * list MUST be of type T_List; T_IntList and T_OidList nodes don't
 * need deep copies, so they should be copied via list_copy()
 */
#define COPY_NODE_CELL(new, old)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  (new) = (ListCell *)palloc(sizeof(ListCell));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  lfirst(new) = copyObjectImpl(lfirst(old));

static List *
_copyList(const List *from)
{























}

/* ****************************************************************
 *					extensible.h copy functions
 * ****************************************************************
 */
static ExtensibleNode *
_copyExtensibleNode(const ExtensibleNode *from)
{











}

/* ****************************************************************
 *					value.h copy functions
 * ****************************************************************
 */
static Value *
_copyValue(const Value *from)
{























}

static ForeignKeyCacheInfo *
_copyForeignKeyCacheInfo(const ForeignKeyCacheInfo *from)
{












}

/*
 * copyObjectImpl -- implementation of copyObject(); see nodes/nodes.h
 *
 * Create a copy of a Node tree or list.  This is a "deep" copy: all
 * substructure is copied too, recursively.
 */
void *
copyObjectImpl(const void *from)
{




























































































































































































































































































































































































































































































































































































































































































































































































































































































































}