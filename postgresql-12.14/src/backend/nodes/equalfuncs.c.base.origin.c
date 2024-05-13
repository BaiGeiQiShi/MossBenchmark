/*-------------------------------------------------------------------------
 *
 * equalfuncs.c
 *	  Equality functions to compare node trees.
 *
 * NOTE: we currently support comparing all node types found in parse
 * trees.  We do not support comparing executor state trees; there
 * is no need for that, and no point in maintaining all the code that
 * would be needed.  We also do not support comparing Path trees, mainly
 * because the circular linkages between RelOptInfo and Path nodes can't
 * be handled easily in a simple depth-first traversal.
 *
 * Currently, in fact, equal() doesn't know how to compare Plan trees
 * either.  This might need to be fixed someday.
 *
 * NOTE: it is intentional that parse location fields (in nodes that have
 * one) are not compared.  This is because we want, for example, a variable
 * "x" to be considered equal() to another reference to "x" in the query.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/nodes/equalfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "nodes/extensible.h"
#include "nodes/pathnodes.h"
#include "utils/datum.h"

/*
 * Macros to simplify comparison of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.  Note that these
 * hard-wire the convention that the local variables in an Equal routine are
 * named 'a' and 'b'.
 */

/* Compare a simple scalar field (int, float, bool, enum, etc) */
#define COMPARE_SCALAR_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (a->fldname != b->fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      return false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

/* Compare a field that is a pointer to some kind of Node or Node tree */
#define COMPARE_NODE_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!equal(a->fldname, b->fldname))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      return false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

/* Compare a field that is a pointer to a Bitmapset */
#define COMPARE_BITMAPSET_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!bms_equal(a->fldname, b->fldname))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      return false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

/* Compare a field that is a pointer to a C string, or perhaps NULL */
#define COMPARE_STRING_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!equalstr(a->fldname, b->fldname))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      return false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

/* Macro for comparing string fields that might be NULL */
#define equalstr(a, b) (((a) != NULL && (b) != NULL) ? (strcmp(a, b) == 0) : (a) == (b))

/* Compare a field that is a pointer to a simple palloc'd object of size sz */
#define COMPARE_POINTER_FIELD(fldname, sz)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (memcmp(a->fldname, b->fldname, (sz)) != 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
      return false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

/* Compare a parse location field (this is a no-op, per note above) */
#define COMPARE_LOCATION_FIELD(fldname) ((void)0)

/* Compare a CoercionForm field (also a no-op, per comment in primnodes.h) */
#define COMPARE_COERCIONFORM_FIELD(fldname) ((void)0)

/*
 *	Stuff from primnodes.h
 */

static bool
_equalAlias(const Alias *a, const Alias *b)
{




}

static bool
_equalRangeVar(const RangeVar *a, const RangeVar *b)
{









}

static bool
_equalTableFunc(const TableFunc *a, const TableFunc *b)
{















}

static bool
_equalIntoClause(const IntoClause *a, const IntoClause *b)
{










}

/*
 * We don't need an _equalExpr because Expr is an abstract supertype which
 * should never actually get instantiated.  Also, since it has no common
 * fields except NodeTag, there's no need for a helper routine to factor
 * out comparing the common fields...
 */

static bool
_equalVar(const Var *a, const Var *b)
{











}

static bool
_equalConst(const Const *a, const Const *b)
{

















}

static bool
_equalParam(const Param *a, const Param *b)
{








}

static bool
_equalAggref(const Aggref *a, const Aggref *b)
{



















}

static bool
_equalGroupingFunc(const GroupingFunc *a, const GroupingFunc *b)
{










}

static bool
_equalWindowFunc(const WindowFunc *a, const WindowFunc *b)
{












}

static bool
_equalSubscriptingRef(const SubscriptingRef *a, const SubscriptingRef *b)
{










}

static bool
_equalFuncExpr(const FuncExpr *a, const FuncExpr *b)
{











}

static bool
_equalNamedArgExpr(const NamedArgExpr *a, const NamedArgExpr *b)
{






}

static bool
_equalOpExpr(const OpExpr *a, const OpExpr *b)
{





















}

static bool
_equalDistinctExpr(const DistinctExpr *a, const DistinctExpr *b)
{





















}

static bool
_equalNullIfExpr(const NullIfExpr *a, const NullIfExpr *b)
{





















}

static bool
_equalScalarArrayOpExpr(const ScalarArrayOpExpr *a, const ScalarArrayOpExpr *b)
{



















}

static bool
_equalBoolExpr(const BoolExpr *a, const BoolExpr *b)
{





}

static bool
_equalSubLink(const SubLink *a, const SubLink *b)
{








}

static bool
_equalSubPlan(const SubPlan *a, const SubPlan *b)
{



















}

static bool
_equalAlternativeSubPlan(const AlternativeSubPlan *a, const AlternativeSubPlan *b)
{



}

static bool
_equalFieldSelect(const FieldSelect *a, const FieldSelect *b)
{







}

static bool
_equalFieldStore(const FieldStore *a, const FieldStore *b)
{






}

static bool
_equalRelabelType(const RelabelType *a, const RelabelType *b)
{








}

static bool
_equalCoerceViaIO(const CoerceViaIO *a, const CoerceViaIO *b)
{







}

static bool
_equalArrayCoerceExpr(const ArrayCoerceExpr *a, const ArrayCoerceExpr *b)
{









}

static bool
_equalConvertRowtypeExpr(const ConvertRowtypeExpr *a, const ConvertRowtypeExpr *b)
{






}

static bool
_equalCollateExpr(const CollateExpr *a, const CollateExpr *b)
{





}

static bool
_equalCaseExpr(const CaseExpr *a, const CaseExpr *b)
{








}

static bool
_equalCaseWhen(const CaseWhen *a, const CaseWhen *b)
{





}

static bool
_equalCaseTestExpr(const CaseTestExpr *a, const CaseTestExpr *b)
{





}

static bool
_equalArrayExpr(const ArrayExpr *a, const ArrayExpr *b)
{








}

static bool
_equalRowExpr(const RowExpr *a, const RowExpr *b)
{







}

static bool
_equalRowCompareExpr(const RowCompareExpr *a, const RowCompareExpr *b)
{








}

static bool
_equalCoalesceExpr(const CoalesceExpr *a, const CoalesceExpr *b)
{






}

static bool
_equalMinMaxExpr(const MinMaxExpr *a, const MinMaxExpr *b)
{








}

static bool
_equalSQLValueFunction(const SQLValueFunction *a, const SQLValueFunction *b)
{






}

static bool
_equalXmlExpr(const XmlExpr *a, const XmlExpr *b)
{











}

static bool
_equalNullTest(const NullTest *a, const NullTest *b)
{






}

static bool
_equalBooleanTest(const BooleanTest *a, const BooleanTest *b)
{





}

static bool
_equalCoerceToDomain(const CoerceToDomain *a, const CoerceToDomain *b)
{








}

static bool
_equalCoerceToDomainValue(const CoerceToDomainValue *a, const CoerceToDomainValue *b)
{






}

static bool
_equalSetToDefault(const SetToDefault *a, const SetToDefault *b)
{






}

static bool
_equalCurrentOfExpr(const CurrentOfExpr *a, const CurrentOfExpr *b)
{





}

static bool
_equalNextValueExpr(const NextValueExpr *a, const NextValueExpr *b)
{




}

static bool
_equalInferenceElem(const InferenceElem *a, const InferenceElem *b)
{





}

static bool
_equalTargetEntry(const TargetEntry *a, const TargetEntry *b)
{









}

static bool
_equalRangeTblRef(const RangeTblRef *a, const RangeTblRef *b)
{



}

static bool
_equalJoinExpr(const JoinExpr *a, const JoinExpr *b)
{










}

static bool
_equalFromExpr(const FromExpr *a, const FromExpr *b)
{




}

static bool
_equalOnConflictExpr(const OnConflictExpr *a, const OnConflictExpr *b)
{










}

/*
 * Stuff from pathnodes.h
 */

static bool
_equalPathKey(const PathKey *a, const PathKey *b)
{







}

static bool
_equalRestrictInfo(const RestrictInfo *a, const RestrictInfo *b)
{














}

static bool
_equalPlaceHolderVar(const PlaceHolderVar *a, const PlaceHolderVar *b)
{




















}

static bool
_equalSpecialJoinInfo(const SpecialJoinInfo *a, const SpecialJoinInfo *b)
{













}

static bool
_equalAppendRelInfo(const AppendRelInfo *a, const AppendRelInfo *b)
{








}

static bool
_equalPlaceHolderInfo(const PlaceHolderInfo *a, const PlaceHolderInfo *b)
{








}

/*
 * Stuff from extensible.h
 */
static bool
_equalExtensibleNode(const ExtensibleNode *a, const ExtensibleNode *b)
{














}

/*
 * Stuff from parsenodes.h
 */

static bool
_equalQuery(const Query *a, const Query *b)
{






































}

static bool
_equalRawStmt(const RawStmt *a, const RawStmt *b)
{





}

static bool
_equalInsertStmt(const InsertStmt *a, const InsertStmt *b)
{









}

static bool
_equalDeleteStmt(const DeleteStmt *a, const DeleteStmt *b)
{







}

static bool
_equalUpdateStmt(const UpdateStmt *a, const UpdateStmt *b)
{








}

static bool
_equalSelectStmt(const SelectStmt *a, const SelectStmt *b)
{




















}

static bool
_equalSetOperationStmt(const SetOperationStmt *a, const SetOperationStmt *b)
{










}

static bool
_equalAlterTableStmt(const AlterTableStmt *a, const AlterTableStmt *b)
{






}

static bool
_equalAlterTableCmd(const AlterTableCmd *a, const AlterTableCmd *b)
{










}

static bool
_equalAlterCollationStmt(const AlterCollationStmt *a, const AlterCollationStmt *b)
{



}

static bool
_equalAlterDomainStmt(const AlterDomainStmt *a, const AlterDomainStmt *b)
{








}

static bool
_equalGrantStmt(const GrantStmt *a, const GrantStmt *b)
{










}

static bool
_equalObjectWithArgs(const ObjectWithArgs *a, const ObjectWithArgs *b)
{





}

static bool
_equalAccessPriv(const AccessPriv *a, const AccessPriv *b)
{




}

static bool
_equalGrantRoleStmt(const GrantRoleStmt *a, const GrantRoleStmt *b)
{








}

static bool
_equalAlterDefaultPrivilegesStmt(const AlterDefaultPrivilegesStmt *a, const AlterDefaultPrivilegesStmt *b)
{




}

static bool
_equalDeclareCursorStmt(const DeclareCursorStmt *a, const DeclareCursorStmt *b)
{





}

static bool
_equalClosePortalStmt(const ClosePortalStmt *a, const ClosePortalStmt *b)
{



}

static bool
_equalCallStmt(const CallStmt *a, const CallStmt *b)
{




}

static bool
_equalClusterStmt(const ClusterStmt *a, const ClusterStmt *b)
{





}

static bool
_equalCopyStmt(const CopyStmt *a, const CopyStmt *b)
{










}

static bool
_equalCreateStmt(const CreateStmt *a, const CreateStmt *b)
{














}

static bool
_equalTableLikeClause(const TableLikeClause *a, const TableLikeClause *b)
{





}

static bool
_equalDefineStmt(const DefineStmt *a, const DefineStmt *b)
{









}

static bool
_equalDropStmt(const DropStmt *a, const DropStmt *b)
{







}

static bool
_equalTruncateStmt(const TruncateStmt *a, const TruncateStmt *b)
{





}

static bool
_equalCommentStmt(const CommentStmt *a, const CommentStmt *b)
{





}

static bool
_equalSecLabelStmt(const SecLabelStmt *a, const SecLabelStmt *b)
{






}

static bool
_equalFetchStmt(const FetchStmt *a, const FetchStmt *b)
{






}

static bool
_equalIndexStmt(const IndexStmt *a, const IndexStmt *b)
{























}

static bool
_equalCreateStatsStmt(const CreateStatsStmt *a, const CreateStatsStmt *b)
{








}

static bool
_equalCreateFunctionStmt(const CreateFunctionStmt *a, const CreateFunctionStmt *b)
{








}

static bool
_equalFunctionParameter(const FunctionParameter *a, const FunctionParameter *b)
{






}

static bool
_equalAlterFunctionStmt(const AlterFunctionStmt *a, const AlterFunctionStmt *b)
{





}

static bool
_equalDoStmt(const DoStmt *a, const DoStmt *b)
{



}

static bool
_equalRenameStmt(const RenameStmt *a, const RenameStmt *b)
{










}

static bool
_equalAlterObjectDependsStmt(const AlterObjectDependsStmt *a, const AlterObjectDependsStmt *b)
{






}

static bool
_equalAlterObjectSchemaStmt(const AlterObjectSchemaStmt *a, const AlterObjectSchemaStmt *b)
{







}

static bool
_equalAlterOwnerStmt(const AlterOwnerStmt *a, const AlterOwnerStmt *b)
{






}

static bool
_equalAlterOperatorStmt(const AlterOperatorStmt *a, const AlterOperatorStmt *b)
{




}

static bool
_equalRuleStmt(const RuleStmt *a, const RuleStmt *b)
{









}

static bool
_equalNotifyStmt(const NotifyStmt *a, const NotifyStmt *b)
{




}

static bool
_equalListenStmt(const ListenStmt *a, const ListenStmt *b)
{



}

static bool
_equalUnlistenStmt(const UnlistenStmt *a, const UnlistenStmt *b)
{



}

static bool
_equalTransactionStmt(const TransactionStmt *a, const TransactionStmt *b)
{







}

static bool
_equalCompositeTypeStmt(const CompositeTypeStmt *a, const CompositeTypeStmt *b)
{




}

static bool
_equalCreateEnumStmt(const CreateEnumStmt *a, const CreateEnumStmt *b)
{




}

static bool
_equalCreateRangeStmt(const CreateRangeStmt *a, const CreateRangeStmt *b)
{




}

static bool
_equalAlterEnumStmt(const AlterEnumStmt *a, const AlterEnumStmt *b)
{








}

static bool
_equalViewStmt(const ViewStmt *a, const ViewStmt *b)
{








}

static bool
_equalLoadStmt(const LoadStmt *a, const LoadStmt *b)
{



}

static bool
_equalCreateDomainStmt(const CreateDomainStmt *a, const CreateDomainStmt *b)
{






}

static bool
_equalCreateOpClassStmt(const CreateOpClassStmt *a, const CreateOpClassStmt *b)
{








}

static bool
_equalCreateOpClassItem(const CreateOpClassItem *a, const CreateOpClassItem *b)
{








}

static bool
_equalCreateOpFamilyStmt(const CreateOpFamilyStmt *a, const CreateOpFamilyStmt *b)
{




}

static bool
_equalAlterOpFamilyStmt(const AlterOpFamilyStmt *a, const AlterOpFamilyStmt *b)
{






}

static bool
_equalCreatedbStmt(const CreatedbStmt *a, const CreatedbStmt *b)
{




}

static bool
_equalAlterDatabaseStmt(const AlterDatabaseStmt *a, const AlterDatabaseStmt *b)
{




}

static bool
_equalAlterDatabaseSetStmt(const AlterDatabaseSetStmt *a, const AlterDatabaseSetStmt *b)
{




}

static bool
_equalDropdbStmt(const DropdbStmt *a, const DropdbStmt *b)
{




}

static bool
_equalVacuumStmt(const VacuumStmt *a, const VacuumStmt *b)
{





}

static bool
_equalVacuumRelation(const VacuumRelation *a, const VacuumRelation *b)
{





}

static bool
_equalExplainStmt(const ExplainStmt *a, const ExplainStmt *b)
{




}

static bool
_equalCreateTableAsStmt(const CreateTableAsStmt *a, const CreateTableAsStmt *b)
{







}

static bool
_equalRefreshMatViewStmt(const RefreshMatViewStmt *a, const RefreshMatViewStmt *b)
{





}

static bool
_equalReplicaIdentityStmt(const ReplicaIdentityStmt *a, const ReplicaIdentityStmt *b)
{




}

static bool
_equalAlterSystemStmt(const AlterSystemStmt *a, const AlterSystemStmt *b)
{



}

static bool
_equalCreateSeqStmt(const CreateSeqStmt *a, const CreateSeqStmt *b)
{







}

static bool
_equalAlterSeqStmt(const AlterSeqStmt *a, const AlterSeqStmt *b)
{






}

static bool
_equalVariableSetStmt(const VariableSetStmt *a, const VariableSetStmt *b)
{






}

static bool
_equalVariableShowStmt(const VariableShowStmt *a, const VariableShowStmt *b)
{



}

static bool
_equalDiscardStmt(const DiscardStmt *a, const DiscardStmt *b)
{



}

static bool
_equalCreateTableSpaceStmt(const CreateTableSpaceStmt *a, const CreateTableSpaceStmt *b)
{






}

static bool
_equalDropTableSpaceStmt(const DropTableSpaceStmt *a, const DropTableSpaceStmt *b)
{




}

static bool
_equalAlterTableSpaceOptionsStmt(const AlterTableSpaceOptionsStmt *a, const AlterTableSpaceOptionsStmt *b)
{





}

static bool
_equalAlterTableMoveAllStmt(const AlterTableMoveAllStmt *a, const AlterTableMoveAllStmt *b)
{







}

static bool
_equalCreateExtensionStmt(const CreateExtensionStmt *a, const CreateExtensionStmt *b)
{





}

static bool
_equalAlterExtensionStmt(const AlterExtensionStmt *a, const AlterExtensionStmt *b)
{




}

static bool
_equalAlterExtensionContentsStmt(const AlterExtensionContentsStmt *a, const AlterExtensionContentsStmt *b)
{






}

static bool
_equalCreateFdwStmt(const CreateFdwStmt *a, const CreateFdwStmt *b)
{





}

static bool
_equalAlterFdwStmt(const AlterFdwStmt *a, const AlterFdwStmt *b)
{





}

static bool
_equalCreateForeignServerStmt(const CreateForeignServerStmt *a, const CreateForeignServerStmt *b)
{








}

static bool
_equalAlterForeignServerStmt(const AlterForeignServerStmt *a, const AlterForeignServerStmt *b)
{






}

static bool
_equalCreateUserMappingStmt(const CreateUserMappingStmt *a, const CreateUserMappingStmt *b)
{






}

static bool
_equalAlterUserMappingStmt(const AlterUserMappingStmt *a, const AlterUserMappingStmt *b)
{





}

static bool
_equalDropUserMappingStmt(const DropUserMappingStmt *a, const DropUserMappingStmt *b)
{





}

static bool
_equalCreateForeignTableStmt(const CreateForeignTableStmt *a, const CreateForeignTableStmt *b)
{









}

static bool
_equalImportForeignSchemaStmt(const ImportForeignSchemaStmt *a, const ImportForeignSchemaStmt *b)
{








}

static bool
_equalCreateTransformStmt(const CreateTransformStmt *a, const CreateTransformStmt *b)
{







}

static bool
_equalCreateAmStmt(const CreateAmStmt *a, const CreateAmStmt *b)
{





}

static bool
_equalCreateTrigStmt(const CreateTrigStmt *a, const CreateTrigStmt *b)
{
















}

static bool
_equalCreateEventTrigStmt(const CreateEventTrigStmt *a, const CreateEventTrigStmt *b)
{






}

static bool
_equalAlterEventTrigStmt(const AlterEventTrigStmt *a, const AlterEventTrigStmt *b)
{




}

static bool
_equalCreatePLangStmt(const CreatePLangStmt *a, const CreatePLangStmt *b)
{








}

static bool
_equalCreateRoleStmt(const CreateRoleStmt *a, const CreateRoleStmt *b)
{





}

static bool
_equalAlterRoleStmt(const AlterRoleStmt *a, const AlterRoleStmt *b)
{





}

static bool
_equalAlterRoleSetStmt(const AlterRoleSetStmt *a, const AlterRoleSetStmt *b)
{





}

static bool
_equalDropRoleStmt(const DropRoleStmt *a, const DropRoleStmt *b)
{




}

static bool
_equalLockStmt(const LockStmt *a, const LockStmt *b)
{





}

static bool
_equalConstraintsSetStmt(const ConstraintsSetStmt *a, const ConstraintsSetStmt *b)
{




}

static bool
_equalReindexStmt(const ReindexStmt *a, const ReindexStmt *b)
{







}

static bool
_equalCreateSchemaStmt(const CreateSchemaStmt *a, const CreateSchemaStmt *b)
{






}

static bool
_equalCreateConversionStmt(const CreateConversionStmt *a, const CreateConversionStmt *b)
{







}

static bool
_equalCreateCastStmt(const CreateCastStmt *a, const CreateCastStmt *b)
{







}

static bool
_equalPrepareStmt(const PrepareStmt *a, const PrepareStmt *b)
{





}

static bool
_equalExecuteStmt(const ExecuteStmt *a, const ExecuteStmt *b)
{




}

static bool
_equalDeallocateStmt(const DeallocateStmt *a, const DeallocateStmt *b)
{



}

static bool
_equalDropOwnedStmt(const DropOwnedStmt *a, const DropOwnedStmt *b)
{




}

static bool
_equalReassignOwnedStmt(const ReassignOwnedStmt *a, const ReassignOwnedStmt *b)
{




}

static bool
_equalAlterTSDictionaryStmt(const AlterTSDictionaryStmt *a, const AlterTSDictionaryStmt *b)
{




}

static bool
_equalAlterTSConfigurationStmt(const AlterTSConfigurationStmt *a, const AlterTSConfigurationStmt *b)
{









}

static bool
_equalCreatePublicationStmt(const CreatePublicationStmt *a, const CreatePublicationStmt *b)
{






}

static bool
_equalAlterPublicationStmt(const AlterPublicationStmt *a, const AlterPublicationStmt *b)
{







}

static bool
_equalCreateSubscriptionStmt(const CreateSubscriptionStmt *a, const CreateSubscriptionStmt *b)
{






}

static bool
_equalAlterSubscriptionStmt(const AlterSubscriptionStmt *a, const AlterSubscriptionStmt *b)
{







}

static bool
_equalDropSubscriptionStmt(const DropSubscriptionStmt *a, const DropSubscriptionStmt *b)
{





}

static bool
_equalCreatePolicyStmt(const CreatePolicyStmt *a, const CreatePolicyStmt *b)
{









}

static bool
_equalAlterPolicyStmt(const AlterPolicyStmt *a, const AlterPolicyStmt *b)
{







}

static bool
_equalAExpr(const A_Expr *a, const A_Expr *b)
{







}

static bool
_equalColumnRef(const ColumnRef *a, const ColumnRef *b)
{




}

static bool
_equalParamRef(const ParamRef *a, const ParamRef *b)
{




}

static bool
_equalAConst(const A_Const *a, const A_Const *b)
{







}

static bool
_equalFuncCall(const FuncCall *a, const FuncCall *b)
{












}

static bool
_equalAStar(const A_Star *a, const A_Star *b)
{

}

static bool
_equalAIndices(const A_Indices *a, const A_Indices *b)
{





}

static bool
_equalA_Indirection(const A_Indirection *a, const A_Indirection *b)
{




}

static bool
_equalA_ArrayExpr(const A_ArrayExpr *a, const A_ArrayExpr *b)
{




}

static bool
_equalResTarget(const ResTarget *a, const ResTarget *b)
{






}

static bool
_equalMultiAssignRef(const MultiAssignRef *a, const MultiAssignRef *b)
{





}

static bool
_equalTypeName(const TypeName *a, const TypeName *b)
{










}

static bool
_equalTypeCast(const TypeCast *a, const TypeCast *b)
{





}

static bool
_equalCollateClause(const CollateClause *a, const CollateClause *b)
{





}

static bool
_equalSortBy(const SortBy *a, const SortBy *b)
{







}

static bool
_equalWindowDef(const WindowDef *a, const WindowDef *b)
{










}

static bool
_equalRangeSubselect(const RangeSubselect *a, const RangeSubselect *b)
{





}

static bool
_equalRangeFunction(const RangeFunction *a, const RangeFunction *b)
{








}

static bool
_equalRangeTableSample(const RangeTableSample *a, const RangeTableSample *b)
{







}

static bool
_equalRangeTableFunc(const RangeTableFunc *a, const RangeTableFunc *b)
{









}

static bool
_equalRangeTableFuncCol(const RangeTableFuncCol *a, const RangeTableFuncCol *b)
{









}

static bool
_equalIndexElem(const IndexElem *a, const IndexElem *b)
{









}

static bool
_equalColumnDef(const ColumnDef *a, const ColumnDef *b)
{



















}

static bool
_equalConstraint(const Constraint *a, const Constraint *b)
{






























}

static bool
_equalDefElem(const DefElem *a, const DefElem *b)
{







}

static bool
_equalLockingClause(const LockingClause *a, const LockingClause *b)
{





}

static bool
_equalRangeTblEntry(const RangeTblEntry *a, const RangeTblEntry *b)
{



































}

static bool
_equalRangeTblFunction(const RangeTblFunction *a, const RangeTblFunction *b)
{









}

static bool
_equalTableSampleClause(const TableSampleClause *a, const TableSampleClause *b)
{





}

static bool
_equalWithCheckOption(const WithCheckOption *a, const WithCheckOption *b)
{







}

static bool
_equalSortGroupClause(const SortGroupClause *a, const SortGroupClause *b)
{







}

static bool
_equalGroupingSet(const GroupingSet *a, const GroupingSet *b)
{





}

static bool
_equalWindowClause(const WindowClause *a, const WindowClause *b)
{
















}

static bool
_equalRowMarkClause(const RowMarkClause *a, const RowMarkClause *b)
{






}

static bool
_equalWithClause(const WithClause *a, const WithClause *b)
{





}

static bool
_equalInferClause(const InferClause *a, const InferClause *b)
{






}

static bool
_equalOnConflictClause(const OnConflictClause *a, const OnConflictClause *b)
{







}

static bool
_equalCommonTableExpr(const CommonTableExpr *a, const CommonTableExpr *b)
{













}

static bool
_equalXmlSerialize(const XmlSerialize *a, const XmlSerialize *b)
{






}

static bool
_equalRoleSpec(const RoleSpec *a, const RoleSpec *b)
{





}

static bool
_equalTriggerTransition(const TriggerTransition *a, const TriggerTransition *b)
{





}

static bool
_equalPartitionElem(const PartitionElem *a, const PartitionElem *b)
{







}

static bool
_equalPartitionSpec(const PartitionSpec *a, const PartitionSpec *b)
{





}

static bool
_equalPartitionBoundSpec(const PartitionBoundSpec *a, const PartitionBoundSpec *b)
{










}

static bool
_equalPartitionRangeDatum(const PartitionRangeDatum *a, const PartitionRangeDatum *b)
{





}

static bool
_equalPartitionCmd(const PartitionCmd *a, const PartitionCmd *b)
{




}

/*
 * Stuff from pg_list.h
 */

static bool
_equalList(const List *a, const List *b)
{























































}

/*
 * Stuff from value.h
 */

static bool
_equalValue(const Value *a, const Value *b)
{





















}

/*
 * equal
 *	  returns whether two nodes are equal
 */
bool
equal(const void *a, const void *b)
{






























































































































































































































































































































































































































































































































































































































































































































































}