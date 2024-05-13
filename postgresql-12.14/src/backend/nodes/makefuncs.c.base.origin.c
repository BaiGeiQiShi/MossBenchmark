/*-------------------------------------------------------------------------
 *
 * makefuncs.c
 *	  creator functions for various nodes. The functions here are for the
 *	  most frequently created nodes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/makefuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_class.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "utils/lsyscache.h"

/*
 * makeA_Expr -
 *		makes an A_Expr node
 */
A_Expr *
makeA_Expr(A_Expr_Kind kind, List *name, Node *lexpr, Node *rexpr, int location)
{








}

/*
 * makeSimpleA_Expr -
 *		As above, given a simple (unqualified) operator name
 */
A_Expr *
makeSimpleA_Expr(A_Expr_Kind kind, char *name, Node *lexpr, Node *rexpr, int location)
{








}

/*
 * makeVar -
 *	  creates a Var node
 */
Var *
makeVar(Index varno, AttrNumber varattno, Oid vartype, int32 vartypmod, Oid varcollid, Index varlevelsup)
{






















}

/*
 * makeVarFromTargetEntry -
 *		convenience function to create a same-level Var node from a
 *		TargetEntry
 */
Var *
makeVarFromTargetEntry(Index varno, TargetEntry *tle)
{

}

/*
 * makeWholeRowVar -
 *	  creates a Var node representing a whole row of the specified RTE
 *
 * A whole-row reference is a Var with varno set to the correct range
 * table entry, and varattno == 0 to signal that it references the whole
 * tuple.  (Use of zero here is unclean, since it could easily be confused
 * with error cases, but it's not worth changing now.)  The vartype indicates
 * a rowtype; either a named composite type, or a domain over a named
 * composite type (only possible if the RTE is a function returning that),
 * or RECORD.  This function encapsulates the logic for determining the
 * correct rowtype OID to use.
 *
 * If allowScalar is true, then for the case where the RTE is a single function
 * returning a non-composite result type, we produce a normal Var referencing
 * the function's result directly, instead of the single-column composite
 * value that the whole-row notation might otherwise suggest.
 */
Var *
makeWholeRowVar(RangeTblEntry *rte, Index varno, Index varlevelsup, bool allowScalar)
{






























































}

/*
 * makeTargetEntry -
 *	  creates a TargetEntry node
 */
TargetEntry *
makeTargetEntry(Expr *expr, AttrNumber resno, char *resname, bool resjunk)
{


















}

/*
 * flatCopyTargetEntry -
 *	  duplicate a TargetEntry, but don't copy substructure
 *
 * This is commonly used when we just want to modify the resno or substitute
 * a new expression.
 */
TargetEntry *
flatCopyTargetEntry(TargetEntry *src_tle)
{





}

/*
 * makeFromExpr -
 *	  creates a FromExpr node
 */
FromExpr *
makeFromExpr(List *fromlist, Node *quals)
{





}

/*
 * makeConst -
 *	  creates a Const node
 */
Const *
makeConst(Oid consttype, int32 consttypmod, Oid constcollid, int constlen, Datum constvalue, bool constisnull, bool constbyval)
{






















}

/*
 * makeNullConst -
 *	  creates a Const node representing a NULL of the specified type/typmod
 *
 * This is a convenience routine that just saves a lookup of the type's
 * storage properties.
 */
Const *
makeNullConst(Oid consttype, int32 consttypmod, Oid constcollid)
{





}

/*
 * makeBoolConst -
 *	  creates a Const node representing a boolean value (can be NULL too)
 */
Node *
makeBoolConst(bool value, bool isnull)
{


}

/*
 * makeBoolExpr -
 *	  creates a BoolExpr node
 */
Expr *
makeBoolExpr(BoolExprType boolop, List *args, int location)
{







}

/*
 * makeAlias -
 *	  creates an Alias node
 *
 * NOTE: the given name is copied, but the colnames list (if any) isn't.
 */
Alias *
makeAlias(const char *aliasname, List *colnames)
{






}

/*
 * makeRelabelType -
 *	  creates a RelabelType node
 */
RelabelType *
makeRelabelType(Expr *arg, Oid rtype, int32 rtypmod, Oid rcollid, CoercionForm rformat)
{










}

/*
 * makeRangeVar -
 *	  creates a RangeVar node (rather oversimplified case)
 */
RangeVar *
makeRangeVar(char *schemaname, char *relname, int location)
{











}

/*
 * makeTypeName -
 *	build a TypeName node for an unqualified name.
 *
 * typmod is defaulted, but can be changed later by caller.
 */
TypeName *
makeTypeName(char *typnam)
{

}

/*
 * makeTypeNameFromNameList -
 *	build a TypeName node for a String list representing a qualified name.
 *
 * typmod is defaulted, but can be changed later by caller.
 */
TypeName *
makeTypeNameFromNameList(List *names)
{







}

/*
 * makeTypeNameFromOid -
 *	build a TypeName node to represent a type already known by OID/typmod.
 */
TypeName *
makeTypeNameFromOid(Oid typeOid, int32 typmod)
{






}

/*
 * makeColumnDef -
 *	build a ColumnDef node to represent a simple column definition.
 *
 * Type and collation are specified by OID.
 * Other properties are all basic to start with.
 */
ColumnDef *
makeColumnDef(const char *colname, Oid typeOid, int32 typmod, Oid collOid)
{


















}

/*
 * makeFuncExpr -
 *	build an expression tree representing a function call.
 *
 * The argument expressions must have been transformed already.
 */
FuncExpr *
makeFuncExpr(Oid funcid, Oid rettype, List *args, Oid funccollid, Oid inputcollid, CoercionForm fformat)
{














}

/*
 * makeDefElem -
 *	build a DefElem node
 *
 * This is sufficient for the "typical" case with an unqualified option name
 * and no special action.
 */
DefElem *
makeDefElem(char *name, Node *arg, int location)
{









}

/*
 * makeDefElemExtended -
 *	build a DefElem node with all fields available to be specified
 */
DefElem *
makeDefElemExtended(char *nameSpace, char *name, Node *arg, DefElemAction defaction, int location)
{









}

/*
 * makeFuncCall -
 *
 * Initialize a FuncCall struct with the information every caller must
 * supply.  Any non-default parameters have to be inserted by the caller.
 */
FuncCall *
makeFuncCall(List *name, List *args, int location)
{













}

/*
 * make_opclause
 *	  Creates an operator clause given its operator info, left operand
 *	  and right operand (pass NULL to create single-operand clause),
 *	  and collation info.
 */
Expr *
make_opclause(Oid opno, Oid opresulttype, bool opretset, Expr *leftop, Expr *rightop, Oid opcollid, Oid inputcollid)
{


















}

/*
 * make_andclause
 *
 * Creates an 'and' clause given a list of its subclauses.
 */
Expr *
make_andclause(List *andclauses)
{






}

/*
 * make_orclause
 *
 * Creates an 'or' clause given a list of its subclauses.
 */
Expr *
make_orclause(List *orclauses)
{






}

/*
 * make_notclause
 *
 * Create a 'not' clause given the expression to be negated.
 */
Expr *
make_notclause(Expr *notclause)
{






}

/*
 * make_and_qual
 *
 * Variant of make_andclause for ANDing two qual conditions together.
 * Qual conditions have the property that a NULL nodetree is interpreted
 * as 'true'.
 *
 * NB: this makes no attempt to preserve AND/OR flatness; so it should not
 * be used on a qual that has already been run through prepqual.c.
 */
Node *
make_and_qual(Node *qual1, Node *qual2)
{









}

/*
 * The planner and executor usually represent qualification expressions
 * as lists of boolean expressions with implicit AND semantics.
 *
 * These functions convert between an AND-semantics expression list and the
 * ordinary representation of a boolean expression.
 *
 * Note that an empty list is considered equivalent to TRUE.
 */
Expr *
make_ands_explicit(List *andclauses)
{












}

List *
make_ands_implicit(Expr *clause)
{





















}

/*
 * makeIndexInfo
 *	  create an IndexInfo node
 */
IndexInfo *
makeIndexInfo(int numattrs, int numkeyattrs, Oid amoid, List *expressions, List *predicates, bool unique, bool isready, bool concurrent)
{






































}

/*
 * makeGroupingSet
 *
 */
GroupingSet *
makeGroupingSet(GroupingSetKind kind, List *content, int location)
{






}

/*
 * makeVacuumRelation -
 *	  create a VacuumRelation node
 */
VacuumRelation *
makeVacuumRelation(RangeVar *relation, Oid oid, List *va_cols)
{






}