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
  COMPARE_STRING_FIELD(aliasname);
  COMPARE_NODE_FIELD(colnames);

  return true;
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
  COMPARE_SCALAR_FIELD(varno);
  COMPARE_SCALAR_FIELD(varattno);
  COMPARE_SCALAR_FIELD(vartype);
  COMPARE_SCALAR_FIELD(vartypmod);
  COMPARE_SCALAR_FIELD(varcollid);
  COMPARE_SCALAR_FIELD(varlevelsup);
  COMPARE_SCALAR_FIELD(varnoold);
  COMPARE_SCALAR_FIELD(varoattno);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalConst(const Const *a, const Const *b)
{
  COMPARE_SCALAR_FIELD(consttype);
  COMPARE_SCALAR_FIELD(consttypmod);
  COMPARE_SCALAR_FIELD(constcollid);
  COMPARE_SCALAR_FIELD(constlen);
  COMPARE_SCALAR_FIELD(constisnull);
  COMPARE_SCALAR_FIELD(constbyval);
  COMPARE_LOCATION_FIELD(location);

  /*
   * We treat all NULL constants of the same type as equal. Someday this
   * might need to change?  But datumIsEqual doesn't work on nulls, so...
   */
  if (a->constisnull)
  {
    return true;
  }
  return datumIsEqual(a->constvalue, b->constvalue, a->constbyval, a->constlen);
}

static bool
_equalParam(const Param *a, const Param *b)
{
  COMPARE_SCALAR_FIELD(paramkind);
  COMPARE_SCALAR_FIELD(paramid);
  COMPARE_SCALAR_FIELD(paramtype);
  COMPARE_SCALAR_FIELD(paramtypmod);
  COMPARE_SCALAR_FIELD(paramcollid);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalAggref(const Aggref *a, const Aggref *b)
{
  COMPARE_SCALAR_FIELD(aggfnoid);
  COMPARE_SCALAR_FIELD(aggtype);
  COMPARE_SCALAR_FIELD(aggcollid);
  COMPARE_SCALAR_FIELD(inputcollid);
  /* ignore aggtranstype since it might not be set yet */
  COMPARE_NODE_FIELD(aggargtypes);
  COMPARE_NODE_FIELD(aggdirectargs);
  COMPARE_NODE_FIELD(args);
  COMPARE_NODE_FIELD(aggorder);
  COMPARE_NODE_FIELD(aggdistinct);
  COMPARE_NODE_FIELD(aggfilter);
  COMPARE_SCALAR_FIELD(aggstar);
  COMPARE_SCALAR_FIELD(aggvariadic);
  COMPARE_SCALAR_FIELD(aggkind);
  COMPARE_SCALAR_FIELD(agglevelsup);
  COMPARE_SCALAR_FIELD(aggsplit);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalGroupingFunc(const GroupingFunc *a, const GroupingFunc *b)
{
  COMPARE_NODE_FIELD(args);

  /*
   * We must not compare the refs or cols field
   */

  COMPARE_SCALAR_FIELD(agglevelsup);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalWindowFunc(const WindowFunc *a, const WindowFunc *b)
{
  COMPARE_SCALAR_FIELD(winfnoid);
  COMPARE_SCALAR_FIELD(wintype);
  COMPARE_SCALAR_FIELD(wincollid);
  COMPARE_SCALAR_FIELD(inputcollid);
  COMPARE_NODE_FIELD(args);
  COMPARE_NODE_FIELD(aggfilter);
  COMPARE_SCALAR_FIELD(winref);
  COMPARE_SCALAR_FIELD(winstar);
  COMPARE_SCALAR_FIELD(winagg);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalSubscriptingRef(const SubscriptingRef *a, const SubscriptingRef *b)
{
  COMPARE_SCALAR_FIELD(refcontainertype);
  COMPARE_SCALAR_FIELD(refelemtype);
  COMPARE_SCALAR_FIELD(reftypmod);
  COMPARE_SCALAR_FIELD(refcollid);
  COMPARE_NODE_FIELD(refupperindexpr);
  COMPARE_NODE_FIELD(reflowerindexpr);
  COMPARE_NODE_FIELD(refexpr);
  COMPARE_NODE_FIELD(refassgnexpr);

  return true;
}

static bool
_equalFuncExpr(const FuncExpr *a, const FuncExpr *b)
{
  COMPARE_SCALAR_FIELD(funcid);
  COMPARE_SCALAR_FIELD(funcresulttype);
  COMPARE_SCALAR_FIELD(funcretset);
  COMPARE_SCALAR_FIELD(funcvariadic);
  COMPARE_COERCIONFORM_FIELD(funcformat);
  COMPARE_SCALAR_FIELD(funccollid);
  COMPARE_SCALAR_FIELD(inputcollid);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalNamedArgExpr(const NamedArgExpr *a, const NamedArgExpr *b)
{






}

static bool
_equalOpExpr(const OpExpr *a, const OpExpr *b)
{
  COMPARE_SCALAR_FIELD(opno);

  /*
   * Special-case opfuncid: it is allowable for it to differ if one node
   * contains zero and the other doesn't.  This just means that the one node
   * isn't as far along in the parse/plan pipeline and hasn't had the
   * opfuncid cache filled yet.
   */
  if (a->opfuncid != b->opfuncid && a->opfuncid != 0 && b->opfuncid != 0)
  {

  }

  COMPARE_SCALAR_FIELD(opresulttype);
  COMPARE_SCALAR_FIELD(opretset);
  COMPARE_SCALAR_FIELD(opcollid);
  COMPARE_SCALAR_FIELD(inputcollid);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalDistinctExpr(const DistinctExpr *a, const DistinctExpr *b)
{





















}

static bool
_equalNullIfExpr(const NullIfExpr *a, const NullIfExpr *b)
{
  COMPARE_SCALAR_FIELD(opno);

  /*
   * Special-case opfuncid: it is allowable for it to differ if one node
   * contains zero and the other doesn't.  This just means that the one node
   * isn't as far along in the parse/plan pipeline and hasn't had the
   * opfuncid cache filled yet.
   */
  if (a->opfuncid != b->opfuncid && a->opfuncid != 0 && b->opfuncid != 0)
  {

  }

  COMPARE_SCALAR_FIELD(opresulttype);
  COMPARE_SCALAR_FIELD(opretset);
  COMPARE_SCALAR_FIELD(opcollid);
  COMPARE_SCALAR_FIELD(inputcollid);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalScalarArrayOpExpr(const ScalarArrayOpExpr *a, const ScalarArrayOpExpr *b)
{
  COMPARE_SCALAR_FIELD(opno);

  /*
   * Special-case opfuncid: it is allowable for it to differ if one node
   * contains zero and the other doesn't.  This just means that the one node
   * isn't as far along in the parse/plan pipeline and hasn't had the
   * opfuncid cache filled yet.
   */
  if (a->opfuncid != b->opfuncid && a->opfuncid != 0 && b->opfuncid != 0)
  {

  }

  COMPARE_SCALAR_FIELD(useOr);
  COMPARE_SCALAR_FIELD(inputcollid);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalBoolExpr(const BoolExpr *a, const BoolExpr *b)
{
  COMPARE_SCALAR_FIELD(boolop);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalSubLink(const SubLink *a, const SubLink *b)
{
  COMPARE_SCALAR_FIELD(subLinkType);
  COMPARE_SCALAR_FIELD(subLinkId);
  COMPARE_NODE_FIELD(testexpr);
  COMPARE_NODE_FIELD(operName);
  COMPARE_NODE_FIELD(subselect);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalSubPlan(const SubPlan *a, const SubPlan *b)
{
  COMPARE_SCALAR_FIELD(subLinkType);
  COMPARE_NODE_FIELD(testexpr);
  COMPARE_NODE_FIELD(paramIds);
  COMPARE_SCALAR_FIELD(plan_id);
  COMPARE_STRING_FIELD(plan_name);
  COMPARE_SCALAR_FIELD(firstColType);
  COMPARE_SCALAR_FIELD(firstColTypmod);
  COMPARE_SCALAR_FIELD(firstColCollation);
  COMPARE_SCALAR_FIELD(useHashTable);
  COMPARE_SCALAR_FIELD(unknownEqFalse);
  COMPARE_SCALAR_FIELD(parallel_safe);
  COMPARE_NODE_FIELD(setParam);
  COMPARE_NODE_FIELD(parParam);
  COMPARE_NODE_FIELD(args);
  COMPARE_SCALAR_FIELD(startup_cost);
  COMPARE_SCALAR_FIELD(per_call_cost);
  COMPARE_SCALAR_FIELD(subLinkId);

  return true;
}

static bool
_equalAlternativeSubPlan(const AlternativeSubPlan *a, const AlternativeSubPlan *b)
{



}

static bool
_equalFieldSelect(const FieldSelect *a, const FieldSelect *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(fieldnum);
  COMPARE_SCALAR_FIELD(resulttype);
  COMPARE_SCALAR_FIELD(resulttypmod);
  COMPARE_SCALAR_FIELD(resultcollid);

  return true;
}

static bool
_equalFieldStore(const FieldStore *a, const FieldStore *b)
{






}

static bool
_equalRelabelType(const RelabelType *a, const RelabelType *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(resulttype);
  COMPARE_SCALAR_FIELD(resulttypmod);
  COMPARE_SCALAR_FIELD(resultcollid);
  COMPARE_COERCIONFORM_FIELD(relabelformat);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalCoerceViaIO(const CoerceViaIO *a, const CoerceViaIO *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(resulttype);
  COMPARE_SCALAR_FIELD(resultcollid);
  COMPARE_COERCIONFORM_FIELD(coerceformat);
  COMPARE_LOCATION_FIELD(location);

  return true;
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
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(collOid);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalCaseExpr(const CaseExpr *a, const CaseExpr *b)
{
  COMPARE_SCALAR_FIELD(casetype);
  COMPARE_SCALAR_FIELD(casecollid);
  COMPARE_NODE_FIELD(arg);
  COMPARE_NODE_FIELD(args);
  COMPARE_NODE_FIELD(defresult);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalCaseWhen(const CaseWhen *a, const CaseWhen *b)
{
  COMPARE_NODE_FIELD(expr);
  COMPARE_NODE_FIELD(result);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalCaseTestExpr(const CaseTestExpr *a, const CaseTestExpr *b)
{
  COMPARE_SCALAR_FIELD(typeId);
  COMPARE_SCALAR_FIELD(typeMod);
  COMPARE_SCALAR_FIELD(collation);

  return true;
}

static bool
_equalArrayExpr(const ArrayExpr *a, const ArrayExpr *b)
{
  COMPARE_SCALAR_FIELD(array_typeid);
  COMPARE_SCALAR_FIELD(array_collid);
  COMPARE_SCALAR_FIELD(element_typeid);
  COMPARE_NODE_FIELD(elements);
  COMPARE_SCALAR_FIELD(multidims);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalRowExpr(const RowExpr *a, const RowExpr *b)
{
  COMPARE_NODE_FIELD(args);
  COMPARE_SCALAR_FIELD(row_typeid);
  COMPARE_COERCIONFORM_FIELD(row_format);
  COMPARE_NODE_FIELD(colnames);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalRowCompareExpr(const RowCompareExpr *a, const RowCompareExpr *b)
{
  COMPARE_SCALAR_FIELD(rctype);
  COMPARE_NODE_FIELD(opnos);






}

static bool
_equalCoalesceExpr(const CoalesceExpr *a, const CoalesceExpr *b)
{
  COMPARE_SCALAR_FIELD(coalescetype);
  COMPARE_SCALAR_FIELD(coalescecollid);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalMinMaxExpr(const MinMaxExpr *a, const MinMaxExpr *b)
{
  COMPARE_SCALAR_FIELD(minmaxtype);
  COMPARE_SCALAR_FIELD(minmaxcollid);
  COMPARE_SCALAR_FIELD(inputcollid);
  COMPARE_SCALAR_FIELD(op);
  COMPARE_NODE_FIELD(args);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalSQLValueFunction(const SQLValueFunction *a, const SQLValueFunction *b)
{
  COMPARE_SCALAR_FIELD(op);
  COMPARE_SCALAR_FIELD(type);
  COMPARE_SCALAR_FIELD(typmod);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalXmlExpr(const XmlExpr *a, const XmlExpr *b)
{











}

static bool
_equalNullTest(const NullTest *a, const NullTest *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(nulltesttype);
  COMPARE_SCALAR_FIELD(argisrow);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalBooleanTest(const BooleanTest *a, const BooleanTest *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(booltesttype);
  COMPARE_LOCATION_FIELD(location);


}

static bool
_equalCoerceToDomain(const CoerceToDomain *a, const CoerceToDomain *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_SCALAR_FIELD(resulttype);
  COMPARE_SCALAR_FIELD(resulttypmod);
  COMPARE_SCALAR_FIELD(resultcollid);
  COMPARE_COERCIONFORM_FIELD(coercionformat);
  COMPARE_LOCATION_FIELD(location);

  return true;
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
  COMPARE_NODE_FIELD(expr);
  COMPARE_SCALAR_FIELD(resno);
  COMPARE_STRING_FIELD(resname);
  COMPARE_SCALAR_FIELD(ressortgroupref);
  COMPARE_SCALAR_FIELD(resorigtbl);
  COMPARE_SCALAR_FIELD(resorigcol);
  COMPARE_SCALAR_FIELD(resjunk);

  return true;
}

static bool
_equalRangeTblRef(const RangeTblRef *a, const RangeTblRef *b)
{
  COMPARE_SCALAR_FIELD(rtindex);

  return true;
}

static bool
_equalJoinExpr(const JoinExpr *a, const JoinExpr *b)
{
  COMPARE_SCALAR_FIELD(jointype);
  COMPARE_SCALAR_FIELD(isNatural);
  COMPARE_NODE_FIELD(larg);
  COMPARE_NODE_FIELD(rarg);
  COMPARE_NODE_FIELD(usingClause);
  COMPARE_NODE_FIELD(quals);
  COMPARE_NODE_FIELD(alias);
  COMPARE_SCALAR_FIELD(rtindex);

  return true;
}

static bool
_equalFromExpr(const FromExpr *a, const FromExpr *b)
{
  COMPARE_NODE_FIELD(fromlist);
  COMPARE_NODE_FIELD(quals);

  return true;
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
  /*
   * We intentionally do not compare phexpr.  Two PlaceHolderVars with the
   * same ID and levelsup should be considered equal even if the contained
   * expressions have managed to mutate to different states.  This will
   * happen during final plan construction when there are nested PHVs, since
   * the inner PHV will get replaced by a Param in some copies of the outer
   * PHV.  Another way in which it can happen is that initplan sublinks
   * could get replaced by differently-numbered Params when sublink folding
   * is done.  (The end result of such a situation would be some
   * unreferenced initplans, which is annoying but not really a problem.) On
   * the same reasoning, there is no need to examine phrels.
   *
   * COMPARE_NODE_FIELD(phexpr);
   *
   * COMPARE_BITMAPSET_FIELD(phrels);
   */
  COMPARE_SCALAR_FIELD(phid);
  COMPARE_SCALAR_FIELD(phlevelsup);

  return true;
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
  COMPARE_SCALAR_FIELD(commandType);
  COMPARE_SCALAR_FIELD(querySource);
  /* we intentionally ignore queryId, since it might not be set */
  COMPARE_SCALAR_FIELD(canSetTag);
  COMPARE_NODE_FIELD(utilityStmt);
  COMPARE_SCALAR_FIELD(resultRelation);
  COMPARE_SCALAR_FIELD(hasAggs);
  COMPARE_SCALAR_FIELD(hasWindowFuncs);
  COMPARE_SCALAR_FIELD(hasTargetSRFs);
  COMPARE_SCALAR_FIELD(hasSubLinks);
  COMPARE_SCALAR_FIELD(hasDistinctOn);
  COMPARE_SCALAR_FIELD(hasRecursive);
  COMPARE_SCALAR_FIELD(hasModifyingCTE);
  COMPARE_SCALAR_FIELD(hasForUpdate);
  COMPARE_SCALAR_FIELD(hasRowSecurity);
  COMPARE_NODE_FIELD(cteList);
  COMPARE_NODE_FIELD(rtable);
  COMPARE_NODE_FIELD(jointree);
  COMPARE_NODE_FIELD(targetList);
  COMPARE_SCALAR_FIELD(override);
  COMPARE_NODE_FIELD(onConflict);
  COMPARE_NODE_FIELD(returningList);
  COMPARE_NODE_FIELD(groupClause);
  COMPARE_NODE_FIELD(groupingSets);
  COMPARE_NODE_FIELD(havingQual);
  COMPARE_NODE_FIELD(windowClause);
  COMPARE_NODE_FIELD(distinctClause);
  COMPARE_NODE_FIELD(sortClause);
  COMPARE_NODE_FIELD(limitOffset);
  COMPARE_NODE_FIELD(limitCount);
  COMPARE_NODE_FIELD(rowMarks);
  COMPARE_NODE_FIELD(setOperations);
  COMPARE_NODE_FIELD(constraintDeps);
  COMPARE_NODE_FIELD(withCheckOptions);
  COMPARE_LOCATION_FIELD(stmt_location);
  COMPARE_LOCATION_FIELD(stmt_len);

  return true;
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
  COMPARE_SCALAR_FIELD(op);
  COMPARE_SCALAR_FIELD(all);
  COMPARE_NODE_FIELD(larg);
  COMPARE_NODE_FIELD(rarg);
  COMPARE_NODE_FIELD(colTypes);
  COMPARE_NODE_FIELD(colTypmods);
  COMPARE_NODE_FIELD(colCollations);
  COMPARE_NODE_FIELD(groupClauses);

  return true;
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
  COMPARE_SCALAR_FIELD(kind);
  COMPARE_NODE_FIELD(name);
  COMPARE_NODE_FIELD(lexpr);
  COMPARE_NODE_FIELD(rexpr);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalColumnRef(const ColumnRef *a, const ColumnRef *b)
{
  COMPARE_NODE_FIELD(fields);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalParamRef(const ParamRef *a, const ParamRef *b)
{




}

static bool
_equalAConst(const A_Const *a, const A_Const *b)
{
  if (!equal(&a->val, &b->val))
  { /* hack for in-line Value field */

  }
  COMPARE_LOCATION_FIELD(location);

  return true;
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
  COMPARE_NODE_FIELD(names);
  COMPARE_SCALAR_FIELD(typeOid);
  COMPARE_SCALAR_FIELD(setof);
  COMPARE_SCALAR_FIELD(pct_type);
  COMPARE_NODE_FIELD(typmods);
  COMPARE_SCALAR_FIELD(typemod);
  COMPARE_NODE_FIELD(arrayBounds);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalTypeCast(const TypeCast *a, const TypeCast *b)
{
  COMPARE_NODE_FIELD(arg);
  COMPARE_NODE_FIELD(typeName);
  COMPARE_LOCATION_FIELD(location);

  return true;
}

static bool
_equalCollateClause(const CollateClause *a, const CollateClause *b)
{





}

static bool
_equalSortBy(const SortBy *a, const SortBy *b)
{
  COMPARE_NODE_FIELD(node);
  COMPARE_SCALAR_FIELD(sortby_dir);
  COMPARE_SCALAR_FIELD(sortby_nulls);
  COMPARE_NODE_FIELD(useOp);
  COMPARE_LOCATION_FIELD(location);

  return true;
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
  COMPARE_STRING_FIELD(name);
  COMPARE_NODE_FIELD(expr);
  COMPARE_STRING_FIELD(indexcolname);
  COMPARE_NODE_FIELD(collation);
  COMPARE_NODE_FIELD(opclass);
  COMPARE_SCALAR_FIELD(ordering);
  COMPARE_SCALAR_FIELD(nulls_ordering);

  return true;
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
  COMPARE_SCALAR_FIELD(rtekind);
  COMPARE_SCALAR_FIELD(relid);
  COMPARE_SCALAR_FIELD(relkind);
  COMPARE_SCALAR_FIELD(rellockmode);
  COMPARE_NODE_FIELD(tablesample);
  COMPARE_NODE_FIELD(subquery);
  COMPARE_SCALAR_FIELD(security_barrier);
  COMPARE_SCALAR_FIELD(jointype);
  COMPARE_NODE_FIELD(joinaliasvars);
  COMPARE_NODE_FIELD(functions);
  COMPARE_SCALAR_FIELD(funcordinality);
  COMPARE_NODE_FIELD(tablefunc);
  COMPARE_NODE_FIELD(values_lists);
  COMPARE_STRING_FIELD(ctename);
  COMPARE_SCALAR_FIELD(ctelevelsup);
  COMPARE_SCALAR_FIELD(self_reference);
  COMPARE_NODE_FIELD(coltypes);
  COMPARE_NODE_FIELD(coltypmods);
  COMPARE_NODE_FIELD(colcollations);
  COMPARE_STRING_FIELD(enrname);
  COMPARE_SCALAR_FIELD(enrtuples);
  COMPARE_NODE_FIELD(alias);
  COMPARE_NODE_FIELD(eref);
  COMPARE_SCALAR_FIELD(lateral);
  COMPARE_SCALAR_FIELD(inh);
  COMPARE_SCALAR_FIELD(inFromCl);
  COMPARE_SCALAR_FIELD(requiredPerms);
  COMPARE_SCALAR_FIELD(checkAsUser);
  COMPARE_BITMAPSET_FIELD(selectedCols);
  COMPARE_BITMAPSET_FIELD(insertedCols);
  COMPARE_BITMAPSET_FIELD(updatedCols);
  COMPARE_BITMAPSET_FIELD(extraUpdatedCols);
  COMPARE_NODE_FIELD(securityQuals);

  return true;
}

static bool
_equalRangeTblFunction(const RangeTblFunction *a, const RangeTblFunction *b)
{
  COMPARE_NODE_FIELD(funcexpr);
  COMPARE_SCALAR_FIELD(funccolcount);
  COMPARE_NODE_FIELD(funccolnames);
  COMPARE_NODE_FIELD(funccoltypes);
  COMPARE_NODE_FIELD(funccoltypmods);
  COMPARE_NODE_FIELD(funccolcollations);
  COMPARE_BITMAPSET_FIELD(funcparams);

  return true;
}

static bool
_equalTableSampleClause(const TableSampleClause *a, const TableSampleClause *b)
{





}

static bool
_equalWithCheckOption(const WithCheckOption *a, const WithCheckOption *b)
{
  COMPARE_SCALAR_FIELD(kind);
  COMPARE_STRING_FIELD(relname);
  COMPARE_STRING_FIELD(polname);
  COMPARE_NODE_FIELD(qual);
  COMPARE_SCALAR_FIELD(cascaded);

  return true;
}

static bool
_equalSortGroupClause(const SortGroupClause *a, const SortGroupClause *b)
{
  COMPARE_SCALAR_FIELD(tleSortGroupRef);
  COMPARE_SCALAR_FIELD(eqop);
  COMPARE_SCALAR_FIELD(sortop);
  COMPARE_SCALAR_FIELD(nulls_first);
  COMPARE_SCALAR_FIELD(hashable);

  return true;
}

static bool
_equalGroupingSet(const GroupingSet *a, const GroupingSet *b)
{





}

static bool
_equalWindowClause(const WindowClause *a, const WindowClause *b)
{
  COMPARE_STRING_FIELD(name);
  COMPARE_STRING_FIELD(refname);
  COMPARE_NODE_FIELD(partitionClause);
  COMPARE_NODE_FIELD(orderClause);
  COMPARE_SCALAR_FIELD(frameOptions);
  COMPARE_NODE_FIELD(startOffset);
  COMPARE_NODE_FIELD(endOffset);
  COMPARE_SCALAR_FIELD(startInRangeFunc);
  COMPARE_SCALAR_FIELD(endInRangeFunc);
  COMPARE_SCALAR_FIELD(inRangeColl);
  COMPARE_SCALAR_FIELD(inRangeAsc);
  COMPARE_SCALAR_FIELD(inRangeNullsFirst);
  COMPARE_SCALAR_FIELD(winref);
  COMPARE_SCALAR_FIELD(copiedOrder);

  return true;
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
  COMPARE_STRING_FIELD(ctename);
  COMPARE_NODE_FIELD(aliascolnames);
  COMPARE_SCALAR_FIELD(ctematerialized);
  COMPARE_NODE_FIELD(ctequery);
  COMPARE_LOCATION_FIELD(location);
  COMPARE_SCALAR_FIELD(cterecursive);
  COMPARE_SCALAR_FIELD(cterefcount);
  COMPARE_NODE_FIELD(ctecolnames);
  COMPARE_NODE_FIELD(ctecoltypes);
  COMPARE_NODE_FIELD(ctecoltypmods);
  COMPARE_NODE_FIELD(ctecolcollations);

  return true;
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
  const ListCell *item_a;
  const ListCell *item_b;

  /*
   * Try to reject by simple scalar checks before grovelling through all the
   * list elements...
   */
  COMPARE_SCALAR_FIELD(type);
  COMPARE_SCALAR_FIELD(length);

  /*
   * We place the switch outside the loop for the sake of efficiency; this
   * may not be worth doing...
   */
  switch (a->type)
  {
  case T_List:;
    forboth(item_a, a, item_b, b)
    {
      if (!equal(lfirst(item_a), lfirst(item_b)))
      {
        return false;
      }
    }
    break;
  case T_IntList:;
    forboth(item_a, a, item_b, b)
    {
      if (lfirst_int(item_a) != lfirst_int(item_b))
      {

      }
    }
    break;
  case T_OidList:;
    forboth(item_a, a, item_b, b)
    {
      if (lfirst_oid(item_a) != lfirst_oid(item_b))
      {
        return false;
      }
    }
    break;
  default:;;


  }

  /*
   * If we got here, we should have run out of elements of both lists
   */
  Assert(item_a == NULL);
  Assert(item_b == NULL);

  return true;
}

/*
 * Stuff from value.h
 */

static bool
_equalValue(const Value *a, const Value *b)
{
  COMPARE_SCALAR_FIELD(type);

  switch (a->type)
  {
  case T_Integer:;
    COMPARE_SCALAR_FIELD(val.ival);
    break;
  case T_Float:;
  case T_String:;
  case T_BitString:;
    COMPARE_STRING_FIELD(val.str);
    break;
  case T_Null:;
    /* nothing to do */

  default:;;


  }

  return true;
}

/*
 * equal
 *	  returns whether two nodes are equal
 */
bool
equal(const void *a, const void *b)
{
  bool retval;

  if (a == b)
  {
    return true;
  }

  /*
   * note that a!=b, so only one of them can be NULL
   */
  if (a == NULL || b == NULL)
  {
    return false;
  }

  /*
   * are they the same type of nodes?
   */
  if (nodeTag(a) != nodeTag(b))
  {
    return false;
  }

  /* Guard against stack overflow due to overly complex expressions */
  check_stack_depth();

  switch (nodeTag(a))
  {
    /*
     * PRIMITIVE NODES
     */
  case T_Alias:;
    retval = _equalAlias(a, b);
    break;
  case T_RangeVar:;


  case T_TableFunc:;


  case T_IntoClause:;


  case T_Var:;
    retval = _equalVar(a, b);
    break;
  case T_Const:;
    retval = _equalConst(a, b);
    break;
  case T_Param:;
    retval = _equalParam(a, b);
    break;
  case T_Aggref:;
    retval = _equalAggref(a, b);
    break;
  case T_GroupingFunc:;
    retval = _equalGroupingFunc(a, b);
    break;
  case T_WindowFunc:;
    retval = _equalWindowFunc(a, b);
    break;
  case T_SubscriptingRef:;
    retval = _equalSubscriptingRef(a, b);
    break;
  case T_FuncExpr:;
    retval = _equalFuncExpr(a, b);
    break;
  case T_NamedArgExpr:;


  case T_OpExpr:;
    retval = _equalOpExpr(a, b);
    break;
  case T_DistinctExpr:;


  case T_NullIfExpr:;
    retval = _equalNullIfExpr(a, b);
    break;
  case T_ScalarArrayOpExpr:;
    retval = _equalScalarArrayOpExpr(a, b);
    break;
  case T_BoolExpr:;
    retval = _equalBoolExpr(a, b);
    break;
  case T_SubLink:;
    retval = _equalSubLink(a, b);
    break;
  case T_SubPlan:;
    retval = _equalSubPlan(a, b);
    break;
  case T_AlternativeSubPlan:;


  case T_FieldSelect:;
    retval = _equalFieldSelect(a, b);
    break;
  case T_FieldStore:;


  case T_RelabelType:;
    retval = _equalRelabelType(a, b);
    break;
  case T_CoerceViaIO:;
    retval = _equalCoerceViaIO(a, b);
    break;
  case T_ArrayCoerceExpr:;


  case T_ConvertRowtypeExpr:;


  case T_CollateExpr:;
    retval = _equalCollateExpr(a, b);
    break;
  case T_CaseExpr:;
    retval = _equalCaseExpr(a, b);
    break;
  case T_CaseWhen:;
    retval = _equalCaseWhen(a, b);
    break;
  case T_CaseTestExpr:;
    retval = _equalCaseTestExpr(a, b);
    break;
  case T_ArrayExpr:;
    retval = _equalArrayExpr(a, b);
    break;
  case T_RowExpr:;
    retval = _equalRowExpr(a, b);
    break;
  case T_RowCompareExpr:;
    retval = _equalRowCompareExpr(a, b);
    break;
  case T_CoalesceExpr:;
    retval = _equalCoalesceExpr(a, b);
    break;
  case T_MinMaxExpr:;
    retval = _equalMinMaxExpr(a, b);
    break;
  case T_SQLValueFunction:;
    retval = _equalSQLValueFunction(a, b);
    break;
  case T_XmlExpr:;


  case T_NullTest:;
    retval = _equalNullTest(a, b);
    break;
  case T_BooleanTest:;
    retval = _equalBooleanTest(a, b);
    break;
  case T_CoerceToDomain:;
    retval = _equalCoerceToDomain(a, b);
    break;
  case T_CoerceToDomainValue:;


  case T_SetToDefault:;


  case T_CurrentOfExpr:;


  case T_NextValueExpr:;


  case T_InferenceElem:;


  case T_TargetEntry:;
    retval = _equalTargetEntry(a, b);
    break;
  case T_RangeTblRef:;
    retval = _equalRangeTblRef(a, b);
    break;
  case T_FromExpr:;
    retval = _equalFromExpr(a, b);
    break;
  case T_OnConflictExpr:;


  case T_JoinExpr:;
    retval = _equalJoinExpr(a, b);
    break;

    /*
     * RELATION NODES
     */
  case T_PathKey:;


  case T_RestrictInfo:;


  case T_PlaceHolderVar:;
    retval = _equalPlaceHolderVar(a, b);
    break;
  case T_SpecialJoinInfo:;


  case T_AppendRelInfo:;


  case T_PlaceHolderInfo:;



  case T_List:;
  case T_IntList:;
  case T_OidList:;
    retval = _equalList(a, b);
    break;

  case T_Integer:;
  case T_Float:;
  case T_String:;
  case T_BitString:;
  case T_Null:;
    retval = _equalValue(a, b);
    break;

    /*
     * EXTENSIBLE NODES
     */
  case T_ExtensibleNode:;



    /*
     * PARSE NODES
     */
  case T_Query:;
    retval = _equalQuery(a, b);
    break;
  case T_RawStmt:;


  case T_InsertStmt:;


  case T_DeleteStmt:;


  case T_UpdateStmt:;


  case T_SelectStmt:;


  case T_SetOperationStmt:;
    retval = _equalSetOperationStmt(a, b);
    break;
  case T_AlterTableStmt:;


  case T_AlterTableCmd:;


  case T_AlterCollationStmt:;


  case T_AlterDomainStmt:;


  case T_GrantStmt:;


  case T_GrantRoleStmt:;


  case T_AlterDefaultPrivilegesStmt:;


  case T_DeclareCursorStmt:;


  case T_ClosePortalStmt:;


  case T_CallStmt:;


  case T_ClusterStmt:;


  case T_CopyStmt:;


  case T_CreateStmt:;


  case T_TableLikeClause:;


  case T_DefineStmt:;


  case T_DropStmt:;


  case T_TruncateStmt:;


  case T_CommentStmt:;


  case T_SecLabelStmt:;


  case T_FetchStmt:;


  case T_IndexStmt:;


  case T_CreateStatsStmt:;


  case T_CreateFunctionStmt:;


  case T_FunctionParameter:;


  case T_AlterFunctionStmt:;


  case T_DoStmt:;


  case T_RenameStmt:;


  case T_AlterObjectDependsStmt:;


  case T_AlterObjectSchemaStmt:;


  case T_AlterOwnerStmt:;


  case T_AlterOperatorStmt:;


  case T_RuleStmt:;


  case T_NotifyStmt:;


  case T_ListenStmt:;


  case T_UnlistenStmt:;


  case T_TransactionStmt:;


  case T_CompositeTypeStmt:;


  case T_CreateEnumStmt:;


  case T_CreateRangeStmt:;


  case T_AlterEnumStmt:;


  case T_ViewStmt:;


  case T_LoadStmt:;


  case T_CreateDomainStmt:;


  case T_CreateOpClassStmt:;


  case T_CreateOpClassItem:;


  case T_CreateOpFamilyStmt:;


  case T_AlterOpFamilyStmt:;


  case T_CreatedbStmt:;


  case T_AlterDatabaseStmt:;


  case T_AlterDatabaseSetStmt:;


  case T_DropdbStmt:;


  case T_VacuumStmt:;


  case T_VacuumRelation:;


  case T_ExplainStmt:;


  case T_CreateTableAsStmt:;


  case T_RefreshMatViewStmt:;


  case T_ReplicaIdentityStmt:;


  case T_AlterSystemStmt:;


  case T_CreateSeqStmt:;


  case T_AlterSeqStmt:;


  case T_VariableSetStmt:;


  case T_VariableShowStmt:;


  case T_DiscardStmt:;


  case T_CreateTableSpaceStmt:;


  case T_DropTableSpaceStmt:;


  case T_AlterTableSpaceOptionsStmt:;


  case T_AlterTableMoveAllStmt:;


  case T_CreateExtensionStmt:;


  case T_AlterExtensionStmt:;


  case T_AlterExtensionContentsStmt:;


  case T_CreateFdwStmt:;


  case T_AlterFdwStmt:;


  case T_CreateForeignServerStmt:;


  case T_AlterForeignServerStmt:;


  case T_CreateUserMappingStmt:;


  case T_AlterUserMappingStmt:;


  case T_DropUserMappingStmt:;


  case T_CreateForeignTableStmt:;


  case T_ImportForeignSchemaStmt:;


  case T_CreateTransformStmt:;


  case T_CreateAmStmt:;


  case T_CreateTrigStmt:;


  case T_CreateEventTrigStmt:;


  case T_AlterEventTrigStmt:;


  case T_CreatePLangStmt:;


  case T_CreateRoleStmt:;


  case T_AlterRoleStmt:;


  case T_AlterRoleSetStmt:;


  case T_DropRoleStmt:;


  case T_LockStmt:;


  case T_ConstraintsSetStmt:;


  case T_ReindexStmt:;


  case T_CheckPointStmt:;


  case T_CreateSchemaStmt:;


  case T_CreateConversionStmt:;


  case T_CreateCastStmt:;


  case T_PrepareStmt:;


  case T_ExecuteStmt:;


  case T_DeallocateStmt:;


  case T_DropOwnedStmt:;


  case T_ReassignOwnedStmt:;


  case T_AlterTSDictionaryStmt:;


  case T_AlterTSConfigurationStmt:;


  case T_CreatePolicyStmt:;


  case T_AlterPolicyStmt:;


  case T_CreatePublicationStmt:;


  case T_AlterPublicationStmt:;


  case T_CreateSubscriptionStmt:;


  case T_AlterSubscriptionStmt:;


  case T_DropSubscriptionStmt:;


  case T_A_Expr:;
    retval = _equalAExpr(a, b);
    break;
  case T_ColumnRef:;
    retval = _equalColumnRef(a, b);
    break;
  case T_ParamRef:;


  case T_A_Const:;
    retval = _equalAConst(a, b);
    break;
  case T_FuncCall:;


  case T_A_Star:;


  case T_A_Indices:;


  case T_A_Indirection:;


  case T_A_ArrayExpr:;


  case T_ResTarget:;


  case T_MultiAssignRef:;


  case T_TypeCast:;
    retval = _equalTypeCast(a, b);
    break;
  case T_CollateClause:;


  case T_SortBy:;
    retval = _equalSortBy(a, b);
    break;
  case T_WindowDef:;


  case T_RangeSubselect:;


  case T_RangeFunction:;


  case T_RangeTableSample:;


  case T_RangeTableFunc:;


  case T_RangeTableFuncCol:;


  case T_TypeName:;
    retval = _equalTypeName(a, b);
    break;
  case T_IndexElem:;
    retval = _equalIndexElem(a, b);
    break;
  case T_ColumnDef:;


  case T_Constraint:;


  case T_DefElem:;


  case T_LockingClause:;


  case T_RangeTblEntry:;
    retval = _equalRangeTblEntry(a, b);
    break;
  case T_RangeTblFunction:;
    retval = _equalRangeTblFunction(a, b);
    break;
  case T_TableSampleClause:;


  case T_WithCheckOption:;
    retval = _equalWithCheckOption(a, b);
    break;
  case T_SortGroupClause:;
    retval = _equalSortGroupClause(a, b);
    break;
  case T_GroupingSet:;


  case T_WindowClause:;
    retval = _equalWindowClause(a, b);
    break;
  case T_RowMarkClause:;


  case T_WithClause:;


  case T_InferClause:;


  case T_OnConflictClause:;


  case T_CommonTableExpr:;
    retval = _equalCommonTableExpr(a, b);
    break;
  case T_ObjectWithArgs:;


  case T_AccessPriv:;


  case T_XmlSerialize:;


  case T_RoleSpec:;


  case T_TriggerTransition:;


  case T_PartitionElem:;


  case T_PartitionSpec:;


  case T_PartitionBoundSpec:;


  case T_PartitionRangeDatum:;


  case T_PartitionCmd:;



  default:;;



  }

  return retval;
}
