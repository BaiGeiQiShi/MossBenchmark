/*-------------------------------------------------------------------------
 *
 * parse_clause.c
 *	  handle clauses in parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_clause.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"

#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/table.h"
#include "access/tsmapi.h"
#include "catalog/catalog.h"
#include "catalog/heap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "parser/parser.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/catcache.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/rel.h"

/* Convenience macro for the most common makeNamespaceItem() case */
#define makeDefaultNSItem(rte) makeNamespaceItem(rte, true, true, false, true)

static void
extractRemainingColumns(List *common_colnames, List *src_colnames, List *src_colvars, List **res_colnames, List **res_colvars);
static Node *
transformJoinUsingClause(ParseState *pstate, RangeTblEntry *leftRTE, RangeTblEntry *rightRTE, List *leftVars, List *rightVars);
static Node *
transformJoinOnClause(ParseState *pstate, JoinExpr *j, List *namespace);
static RangeTblEntry *
getRTEForSpecialRelationTypes(ParseState *pstate, RangeVar *rv);
static RangeTblEntry *
transformTableEntry(ParseState *pstate, RangeVar *r);
static RangeTblEntry *
transformRangeSubselect(ParseState *pstate, RangeSubselect *r);
static RangeTblEntry *
transformRangeFunction(ParseState *pstate, RangeFunction *r);
static RangeTblEntry *
transformRangeTableFunc(ParseState *pstate, RangeTableFunc *t);
static TableSampleClause *
transformRangeTableSample(ParseState *pstate, RangeTableSample *rts);
static Node *
transformFromClauseItem(ParseState *pstate, Node *n, RangeTblEntry **top_rte, int *top_rti, List **namespace);
static Node *
buildMergedJoinVar(ParseState *pstate, JoinType jointype, Var *l_colvar, Var *r_colvar);
static ParseNamespaceItem *
makeNamespaceItem(RangeTblEntry *rte, bool rel_visible, bool cols_visible, bool lateral_only, bool lateral_ok);
static void
setNamespaceColumnVisibility(List *namespace, bool cols_visible);
static void
setNamespaceLateralState(List *namespace, bool lateral_only, bool lateral_ok);
static void
checkExprIsVarFree(ParseState *pstate, Node *n, const char *constructName);
static TargetEntry *
findTargetlistEntrySQL92(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind);
static TargetEntry *
findTargetlistEntrySQL99(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind);
static int
get_matching_location(int sortgroupref, List *sortgrouprefs, List *exprs);
static List *
resolve_unique_index_expr(ParseState *pstate, InferClause *infer, Relation heapRel);
static List *
addTargetToGroupList(ParseState *pstate, TargetEntry *tle, List *grouplist, List *targetlist, int location);
static WindowClause *
findWindowClause(List *wclist, const char *name);
static Node *
transformFrameOffset(ParseState *pstate, int frameOptions, Oid rangeopfamily, Oid rangeopcintype, Oid *inRangeFunc, Node *clause);

/*
 * transformFromClause -
 *	  Process the FROM clause and add items to the query's range table,
 *	  joinlist, and namespace.
 *
 * Note: we assume that the pstate's p_rtable, p_joinlist, and p_namespace
 * lists were initialized to NIL when the pstate was created.
 * We will add onto any entries already present --- this is needed for rule
 * processing, as well as for UPDATE and DELETE.
 */
void
transformFromClause(ParseState *pstate, List *frmList)
{




































}

/*
 * setTargetTable
 *	  Add the target relation of INSERT/UPDATE/DELETE to the range table,
 *	  and make the special links to it in the ParseState.
 *
 *	  We also open the target relation and acquire a write lock on it.
 *	  This must be done before processing the FROM list, in case the target
 *	  is also mentioned as a source relation --- we want to be sure to grab
 *	  the write lock before any read lock.
 *
 *	  If alsoSource is true, add the target to the query's joinlist and
 *	  namespace.  For INSERT, we don't want the target to be joined to;
 *	  it's a destination of tuples, not a source.   For UPDATE/DELETE,
 *	  we do need to scan or join the target.  (NOTE: we do not bother
 *	  to check for namespace conflict; we assume that the namespace was
 *	  initially empty in these cases.)
 *
 *	  Finally, we mark the relation as requiring the permissions specified
 *	  by requiredPerms.
 *
 *	  Returns the rangetable index of the target relation.
 */
int
setTargetTable(ParseState *pstate, RangeVar *relation, bool inh, bool alsoSource, AclMode requiredPerms)
{





























































}

/*
 * Extract all not-in-common columns from column lists of a source table
 */
static void
extractRemainingColumns(List *common_colnames, List *src_colnames, List *src_colvars, List **res_colnames, List **res_colvars)
{
































}

/* transformJoinUsingClause()
 *	  Build a complete ON clause from a partially-transformed USING list.
 *	  We are given lists of nodes representing left and right match columns.
 *	  Result is a transformed qualification expression.
 */
static Node *
transformJoinUsingClause(ParseState *pstate, RangeTblEntry *leftRTE, RangeTblEntry *rightRTE, List *leftVars, List *rightVars)
{


















































}

/* transformJoinOnClause()
 *	  Transform the qual conditions for JOIN/ON.
 *	  Result is a transformed qualification expression.
 */
static Node *
transformJoinOnClause(ParseState *pstate, JoinExpr *j, List *namespace)
{





















}

/*
 * transformTableEntry --- transform a RangeVar (simple relation reference)
 */
static RangeTblEntry *
transformTableEntry(ParseState *pstate, RangeVar *r)
{






}

/*
 * transformRangeSubselect --- transform a sub-SELECT appearing in FROM
 */
static RangeTblEntry *
transformRangeSubselect(ParseState *pstate, RangeSubselect *r)
{























































}

/*
 * transformRangeFunction --- transform a function call appearing in FROM
 */
static RangeTblEntry *
transformRangeFunction(ParseState *pstate, RangeFunction *r)
{


















































































































































































}

/*
 * transformRangeTableFunc -
 *			Transform a raw RangeTableFunc into TableFunc.
 *
 * Transform the namespace clauses, the document-generating expression, the
 * row-generating expression, the column-generating expressions, and the
 * default value expressions.
 */
static RangeTblEntry *
transformRangeTableFunc(ParseState *pstate, RangeTableFunc *rtf)
{
































































































































































































}

/*
 * transformRangeTableSample --- transform a TABLESAMPLE clause
 *
 * Caller has already transformed rts->relation, we just have to validate
 * the remaining fields and create a TableSampleClause node.
 */
static TableSampleClause *
transformRangeTableSample(ParseState *pstate, RangeTableSample *rts)
{

















































































}

/*
 * getRTEForSpecialRelationTypes
 *
 * If given RangeVar refers to a CTE or an EphemeralNamedRelation,
 * build and return an appropriate RTE, otherwise return NULL
 */
static RangeTblEntry *
getRTEForSpecialRelationTypes(ParseState *pstate, RangeVar *rv)
{



























}

/*
 * transformFromClauseItem -
 *	  Transform a FROM-clause item, adding any required entries to the
 *	  range table list being built in the ParseState, and return the
 *	  transformed item ready to include in the joinlist.  Also build a
 *	  ParseNamespaceItem list describing the names exposed by this item.
 *	  This routine can recurse to handle SQL92 JOIN expressions.
 *
 * The function return value is the node to add to the jointree (a
 * RangeTblRef or JoinExpr).  Additional output parameters are:
 *
 * *top_rte: receives the RTE corresponding to the jointree item.
 * (We could extract this from the function return node, but it saves cycles
 * to pass it back separately.)
 *
 * *top_rti: receives the rangetable index of top_rte.  (Ditto.)
 *
 * *namespace: receives a List of ParseNamespaceItems for the RTEs exposed
 * as table/column names by this item.  (The lateral_only flags in these items
 * are indeterminate and should be explicitly set by the caller before use.)
 */
static Node *
transformFromClauseItem(ParseState *pstate, Node *n, RangeTblEntry **top_rte, int *top_rti, List **namespace)
{




























































































































































































































































































































































































}

/*
 * buildMergedJoinVar -
 *	  generate a suitable replacement expression for a merged join column
 */
static Node *
buildMergedJoinVar(ParseState *pstate, JoinType jointype, Var *l_colvar, Var *r_colvar)
{


















































































































}

/*
 * makeNamespaceItem -
 *	  Convenience subroutine to construct a ParseNamespaceItem.
 */
static ParseNamespaceItem *
makeNamespaceItem(RangeTblEntry *rte, bool rel_visible, bool cols_visible, bool lateral_only, bool lateral_ok)
{









}

/*
 * setNamespaceColumnVisibility -
 *	  Convenience subroutine to update cols_visible flags in a namespace
 *list.
 */
static void
setNamespaceColumnVisibility(List *namespace, bool cols_visible)
{








}

/*
 * setNamespaceLateralState -
 *	  Convenience subroutine to update LATERAL flags in a namespace list.
 */
static void
setNamespaceLateralState(List *namespace, bool lateral_only, bool lateral_ok)
{









}

/*
 * transformWhereClause -
 *	  Transform the qualification and make sure it is of type boolean.
 *	  Used for WHERE and allied clauses.
 *
 * constructName does not affect the semantics, but is used in error messages
 */
Node *
transformWhereClause(ParseState *pstate, Node *clause, ParseExprKind exprKind, const char *constructName)
{












}

/*
 * transformLimitClause -
 *	  Transform the expression and make sure it is of type bigint.
 *	  Used for LIMIT and allied clauses.
 *
 * Note: as of Postgres 8.2, LIMIT expressions are expected to yield int8,
 * rather than int4 as before.
 *
 * constructName does not affect the semantics, but is used in error messages
 */
Node *
transformLimitClause(ParseState *pstate, Node *clause, ParseExprKind exprKind, const char *constructName)
{















}

/*
 * checkExprIsVarFree
 *		Check that given expr has no Vars of the current query level
 *		(aggregates and window functions should have been rejected
 *already).
 *
 * This is used to check expressions that have to have a consistent value
 * across all rows of the query, such as a LIMIT.  Arguably it should reject
 * volatile functions, too, but we don't do that --- whatever value the
 * function gives on first execution is what you get.
 *
 * constructName does not affect the semantics, but is used in error messages
 */
static void
checkExprIsVarFree(ParseState *pstate, Node *n, const char *constructName)
{




}

/*
 * checkTargetlistEntrySQL92 -
 *	  Validate a targetlist entry found by findTargetlistEntrySQL92
 *
 * When we select a pre-existing tlist entry as a result of syntax such
 * as "GROUP BY 1", we have to make sure it is acceptable for use in the
 * indicated clause type; transformExpr() will have treated it as a regular
 * targetlist item.
 */
static void
checkTargetlistEntrySQL92(ParseState *pstate, TargetEntry *tle, ParseExprKind exprKind)
{























}

/*
 *	findTargetlistEntrySQL92 -
 *	  Returns the targetlist entry matching the given (untransformed) node.
 *	  If no matching entry exists, one is created and appended to the target
 *	  list as a "resjunk" node.
 *
 * This function supports the old SQL92 ORDER BY interpretation, where the
 * expression is an output column name or number.  If we fail to find a
 * match of that sort, we fall through to the SQL99 rules.  For historical
 * reasons, Postgres also allows this interpretation for GROUP BY, though
 * the standard never did.  However, for GROUP BY we prefer a SQL99 match.
 * This function is *not* used for WINDOW definitions.
 *
 * node		the ORDER BY, GROUP BY, or DISTINCT ON expression to be matched
 * tlist	the target list (passed by reference so we can append to it)
 * exprKind identifies clause type being processed
 */
static TargetEntry *
findTargetlistEntrySQL92(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind)
{







































































































































}

/*
 *	findTargetlistEntrySQL99 -
 *	  Returns the targetlist entry matching the given (untransformed) node.
 *	  If no matching entry exists, one is created and appended to the target
 *	  list as a "resjunk" node.
 *
 * This function supports the SQL99 interpretation, wherein the expression
 * is just an ordinary expression referencing input column names.
 *
 * node		the ORDER BY, GROUP BY, etc expression to be matched
 * tlist	the target list (passed by reference so we can append to it)
 * exprKind identifies clause type being processed
 */
static TargetEntry *
findTargetlistEntrySQL99(ParseState *pstate, Node *node, List **tlist, ParseExprKind exprKind)
{













































}

/*-------------------------------------------------------------------------
 * Flatten out parenthesized sublists in grouping lists, and some cases
 * of nested grouping sets.
 *
 * Inside a grouping set (ROLLUP, CUBE, or GROUPING SETS), we expect the
 * content to be nested no more than 2 deep: i.e. ROLLUP((a,b),(c,d)) is
 * ok, but ROLLUP((a,(b,c)),d) is flattened to ((a,b,c),d), which we then
 * (later) normalize to ((a,b,c),(d)).
 *
 * CUBE or ROLLUP can be nested inside GROUPING SETS (but not the reverse),
 * and we leave that alone if we find it. But if we see GROUPING SETS inside
 * GROUPING SETS, we can flatten and normalize as follows:
 *	 GROUPING SETS (a, (b,c), GROUPING SETS ((c,d),(e)), (f,g))
 * becomes
 *	 GROUPING SETS ((a), (b,c), (c,d), (e), (f,g))
 *
 * This is per the spec's syntax transformations, but these are the only such
 * transformations we do in parse analysis, so that queries retain the
 * originally specified grouping set syntax for CUBE and ROLLUP as much as
 * possible when deparsed. (Full expansion of the result into a list of
 * grouping sets is left to the planner.)
 *
 * When we're done, the resulting list should contain only these possible
 * elements:
 *	 - an expression
 *	 - a CUBE or ROLLUP with a list of expressions nested 2 deep
 *	 - a GROUPING SET containing any of:
 *		- expression lists
 *		- empty grouping sets
 *		- CUBE or ROLLUP nodes with lists nested 2 deep
 * The return is a new list, but doesn't deep-copy the old nodes except for
 * GroupingSet nodes.
 *
 * As a side effect, flag whether the list has any GroupingSet nodes.
 *-------------------------------------------------------------------------
 */
static Node *
flatten_grouping_sets(Node *expr, bool toplevel, bool *hasGroupingSets)
{





































































































}

/*
 * Transform a single expression within a GROUP BY clause or grouping set.
 *
 * The expression is added to the targetlist if not already present, and to the
 * flatresult list (which will become the groupClause) if not already present
 * there.  The sortClause is consulted for operator and sort order hints.
 *
 * Returns the ressortgroupref of the expression.
 *
 * flatresult	reference to flat list of SortGroupClause nodes
 * seen_local	bitmapset of sortgrouprefs already seen at the local level
 * pstate		ParseState
 * gexpr		node to transform
 * targetlist	reference to TargetEntry list
 * sortClause	ORDER BY clause (SortGroupClause nodes)
 * exprKind		expression kind
 * useSQL99		SQL99 rather than SQL92 syntax
 * toplevel		false if within any grouping set
 */
static Index
transformGroupClauseExpr(List **flatresult, Bitmapset *seen_local, ParseState *pstate, Node *gexpr, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99, bool toplevel)
{



























































































}

/*
 * Transform a list of expressions within a GROUP BY clause or grouping set.
 *
 * The list of expressions belongs to a single clause within which duplicates
 * can be safely eliminated.
 *
 * Returns an integer list of ressortgroupref values.
 *
 * flatresult	reference to flat list of SortGroupClause nodes
 * pstate		ParseState
 * list			nodes to transform
 * targetlist	reference to TargetEntry list
 * sortClause	ORDER BY clause (SortGroupClause nodes)
 * exprKind		expression kind
 * useSQL99		SQL99 rather than SQL92 syntax
 * toplevel		false if within any grouping set
 */
static List *
transformGroupClauseList(List **flatresult, ParseState *pstate, List *list, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99, bool toplevel)
{


















}

/*
 * Transform a grouping set and (recursively) its content.
 *
 * The grouping set might be a GROUPING SETS node with other grouping sets
 * inside it, but SETS within SETS have already been flattened out before
 * reaching here.
 *
 * Returns the transformed node, which now contains SIMPLE nodes with lists
 * of ressortgrouprefs rather than expressions.
 *
 * flatresult	reference to flat list of SortGroupClause nodes
 * pstate		ParseState
 * gset			grouping set to transform
 * targetlist	reference to TargetEntry list
 * sortClause	ORDER BY clause (SortGroupClause nodes)
 * exprKind		expression kind
 * useSQL99		SQL99 rather than SQL92 syntax
 * toplevel		false if within any grouping set
 */
static Node *
transformGroupingSet(List **flatresult, ParseState *pstate, GroupingSet *gset, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99, bool toplevel)
{







































}

/*
 * transformGroupClause -
 *	  transform a GROUP BY clause
 *
 * GROUP BY items will be added to the targetlist (as resjunk columns)
 * if not already present, so the targetlist must be passed by reference.
 *
 * This is also used for window PARTITION BY clauses (which act almost the
 * same, but are always interpreted per SQL99 rules).
 *
 * Grouping sets make this a lot more complex than it was. Our goal here is
 * twofold: we make a flat list of SortGroupClause nodes referencing each
 * distinct expression used for grouping, with those expressions added to the
 * targetlist if needed. At the same time, we build the groupingSets tree,
 * which stores only ressortgrouprefs as integer lists inside GroupingSet nodes
 * (possibly nested, but limited in depth: a GROUPING_SET_SETS node can contain
 * nested SIMPLE, CUBE or ROLLUP nodes, but not more sets - we flatten that
 * out; while CUBE and ROLLUP can contain only SIMPLE nodes).
 *
 * We skip much of the hard work if there are no grouping sets.
 *
 * One subtlety is that the groupClause list can end up empty while the
 * groupingSets list is not; this happens if there are only empty grouping
 * sets, or an explicit GROUP BY (). This has the same effect as specifying
 * aggregates or a HAVING clause with no GROUP BY; the output is one row per
 * grouping set even if the input is empty.
 *
 * Returns the transformed (flat) groupClause.
 *
 * pstate		ParseState
 * grouplist	clause to transform
 * groupingSets reference to list to contain the grouping set tree
 * targetlist	reference to TargetEntry list
 * sortClause	ORDER BY clause (SortGroupClause nodes)
 * exprKind		expression kind
 * useSQL99		SQL99 rather than SQL92 syntax
 */
List *
transformGroupClause(ParseState *pstate, List *grouplist, List **groupingSets, List **targetlist, List *sortClause, ParseExprKind exprKind, bool useSQL99)
{









































































}

/*
 * transformSortClause -
 *	  transform an ORDER BY clause
 *
 * ORDER BY items will be added to the targetlist (as resjunk columns)
 * if not already present, so the targetlist must be passed by reference.
 *
 * This is also used for window and aggregate ORDER BY clauses (which act
 * almost the same, but are always interpreted per SQL99 rules).
 */
List *
transformSortClause(ParseState *pstate, List *orderlist, List **targetlist, ParseExprKind exprKind, bool useSQL99)
{





















}

/*
 * transformWindowDefinitions -
 *		transform window definitions (WindowDef to WindowClause)
 */
List *
transformWindowDefinitions(ParseState *pstate, List *windowdefs, List **targetlist)
{
































































































































































}

/*
 * transformDistinctClause -
 *	  transform a DISTINCT clause
 *
 * Since we may need to add items to the query's targetlist, that list
 * is passed by reference.
 *
 * As with GROUP BY, we absorb the sorting semantics of ORDER BY as much as
 * possible into the distinctClause.  This avoids a possible need to re-sort,
 * and allows the user to choose the equality semantics used by DISTINCT,
 * should she be working with a datatype that has more than one equality
 * operator.
 *
 * is_agg is true if we are transforming an aggregate(DISTINCT ...)
 * function call.  This does not affect any behavior, only the phrasing
 * of error messages.
 */
List *
transformDistinctClause(ParseState *pstate, List **targetlist, List *sortClause, bool is_agg)
{



























































}

/*
 * transformDistinctOnClause -
 *	  transform a DISTINCT ON clause
 *
 * Since we may need to add items to the query's targetlist, that list
 * is passed by reference.
 *
 * As with GROUP BY, we absorb the sorting semantics of ORDER BY as much as
 * possible into the distinctClause.  This avoids a possible need to re-sort,
 * and allows the user to choose the equality semantics used by DISTINCT,
 * should she be working with a datatype that has more than one equality
 * operator.
 */
List *
transformDistinctOnClause(ParseState *pstate, List *distinctlist, List **targetlist, List *sortClause)
{
























































































}

/*
 * get_matching_location
 *		Get the exprLocation of the exprs member corresponding to the
 *		(first) member of sortgrouprefs that equals sortgroupref.
 *
 * This is used so that we can point at a troublesome DISTINCT ON entry.
 * (Note that we need to use the original untransformed DISTINCT ON list
 * item, as whatever TLE it corresponds to will very possibly have a
 * parse location pointing to some matching entry in the SELECT list
 * or ORDER BY list.)
 */
static int
get_matching_location(int sortgroupref, List *sortgrouprefs, List *exprs)
{













}

/*
 * resolve_unique_index_expr
 *		Infer a unique index from a list of indexElems, for ON
 *		CONFLICT clause
 *
 * Perform parse analysis of expressions and columns appearing within ON
 * CONFLICT clause.  During planning, the returned list of expressions is used
 * to infer which unique index to use.
 */
static List *
resolve_unique_index_expr(ParseState *pstate, InferClause *infer, Relation heapRel)
{




















































































}

/*
 * transformOnConflictArbiter -
 *		transform arbiter expressions in an ON CONFLICT clause.
 *
 * Transformed expressions used to infer one unique index relation to serve as
 * an ON CONFLICT arbiter.  Partial unique indexes may be inferred using WHERE
 * clause from inference specification clause.
 */
void
transformOnConflictArbiter(ParseState *pstate, OnConflictClause *onConflictClause, List **arbiterExpr, Node **arbiterWhere, Oid *constraint)
{




















































































}

/*
 * addTargetToSortList
 *		If the given targetlist entry isn't already in the
 *SortGroupClause list, add it to the end of the list, using the given sort
 *ordering info.
 *
 * Returns the updated SortGroupClause list.
 */
List *
addTargetToSortList(ParseState *pstate, TargetEntry *tle, List *sortlist, List *targetlist, SortBy *sortby)
{









































































































}

/*
 * addTargetToGroupList
 *		If the given targetlist entry isn't already in the
 *SortGroupClause list, add it to the end of the list, using default sort/group
 *		semantics.
 *
 * This is very similar to addTargetToSortList, except that we allow the
 * case where only a grouping (equality) operator can be found, and that
 * the TLE is considered "already in the list" if it appears there with any
 * sorting semantics.
 *
 * location is the parse location to be fingered in event of trouble.  Note
 * that we can't rely on exprLocation(tle->expr), because that might point
 * to a SELECT item that matches the GROUP BY item; it'd be pretty confusing
 * to report such a location.
 *
 * Returns the updated SortGroupClause list.
 */
static List *
addTargetToGroupList(ParseState *pstate, TargetEntry *tle, List *grouplist, List *targetlist, int location)
{



































}

/*
 * assignSortGroupRef
 *	  Assign the targetentry an unused ressortgroupref, if it doesn't
 *	  already have one.  Return the assigned or pre-existing refnumber.
 *
 * 'tlist' is the targetlist containing (or to contain) the given targetentry.
 */
Index
assignSortGroupRef(TargetEntry *tle, List *tlist)
{





















}

/*
 * targetIsInSortList
 *		Is the given target item already in the sortlist?
 *		If sortop is not InvalidOid, also test for a match to the
 *sortop.
 *
 * It is not an oversight that this function ignores the nulls_first flag.
 * We check sortop when determining if an ORDER BY item is redundant with
 * earlier ORDER BY items, because it's conceivable that "ORDER BY
 * foo USING <, foo USING <<<" is not redundant, if <<< distinguishes
 * values that < considers equal.  We need not check nulls_first
 * however, because a lower-order column with the same sortop but
 * opposite nulls direction is redundant.  Also, we can consider
 * ORDER BY foo ASC, foo DESC redundant, so check for a commutator match.
 *
 * Works for both ordering and grouping lists (sortop would normally be
 * InvalidOid when considering grouping).  Note that the main reason we need
 * this routine (and not just a quick test for nonzeroness of ressortgroupref)
 * is that a TLE might be in only one of the lists.
 */
bool
targetIsInSortList(TargetEntry *tle, Oid sortop, List *sortList)
{



















}

/*
 * findWindowClause
 *		Find the named WindowClause in the list, or return NULL if not
 *there
 */
static WindowClause *
findWindowClause(List *wclist, const char *name)
{













}

/*
 * transformFrameOffset
 *		Process a window frame offset expression
 *
 * In RANGE mode, rangeopfamily is the sort opfamily for the input ORDER BY
 * column, and rangeopcintype is the input data type the sort operator is
 * registered with.  We expect the in_range function to be registered with
 * that same type.  (In binary-compatible cases, it might be different from
 * the input column's actual type, so we can't use that for the lookups.)
 * We'll return the OID of the in_range function to *inRangeFunc.
 */
static Node *
transformFrameOffset(ParseState *pstate, int frameOptions, Oid rangeopfamily, Oid rangeopcintype, Oid *inRangeFunc, Node *clause)
{




























































































































}