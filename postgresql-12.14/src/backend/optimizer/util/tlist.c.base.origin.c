/*-------------------------------------------------------------------------
 *
 * tlist.c
 *	  Target list manipulation routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/tlist.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/tlist.h"

/* Test if an expression node represents a SRF call.  Beware multiple eval! */
#define IS_SRF_CALL(node) ((IsA(node, FuncExpr) && ((FuncExpr *)(node))->funcretset) || (IsA(node, OpExpr) && ((OpExpr *)(node))->opretset))

/*
 * Data structures for split_pathtarget_at_srfs().  To preserve the identity
 * of sortgroupref items even if they are textually equal(), what we track is
 * not just bare expressions but expressions plus their sortgroupref indexes.
 */
typedef struct
{
  Node *expr;         /* some subexpression of a PathTarget */
  Index sortgroupref; /* its sortgroupref, or 0 if none */
} split_pathtarget_item;

typedef struct
{
  /* This is a List of bare expressions: */
  List *input_target_exprs; /* exprs available from input */
  /* These are Lists of Lists of split_pathtarget_items: */
  List *level_srfs;       /* SRF exprs to evaluate at each level */
  List *level_input_vars; /* input vars needed at each level */
  List *level_input_srfs; /* input SRFs needed at each level */
  /* These are Lists of split_pathtarget_items: */
  List *current_input_vars; /* vars needed in current subexpr */
  List *current_input_srfs; /* SRFs needed in current subexpr */
  /* Auxiliary data for current split_pathtarget_walker traversal: */
  int current_depth;   /* max SRF depth in current subexpr */
  Index current_sgref; /* current subexpr's sortgroupref, or 0 */
} split_pathtarget_context;

static bool
split_pathtarget_walker(Node *node, split_pathtarget_context *context);
static void
add_sp_item_to_pathtarget(PathTarget *target, split_pathtarget_item *item);
static void
add_sp_items_to_pathtarget(PathTarget *target, List *items);

/*****************************************************************************
 *		Target list creation and searching utilities
 *****************************************************************************/

/*
 * tlist_member
 *	  Finds the (first) member of the given tlist whose expression is
 *	  equal() to the given expression.  Result is NULL if no such member.
 */
TargetEntry *
tlist_member(Expr *node, List *targetlist)
{












}

/*
 * tlist_member_ignore_relabel
 *	  Same as above, except that we ignore top-level RelabelType nodes
 *	  while checking for a match.  This is needed for some scenarios
 *	  involving binary-compatible sort operations.
 */
TargetEntry *
tlist_member_ignore_relabel(Expr *node, List *targetlist)
{























}

/*
 * tlist_member_match_var
 *	  Same as above, except that we match the provided Var on the basis
 *	  of varno/varattno/varlevelsup/vartype only, rather than full equal().
 *
 * This is needed in some cases where we can't be sure of an exact typmod
 * match.  For safety, though, we insist on vartype match.
 */
static TargetEntry *
tlist_member_match_var(Var *var, List *targetlist)
{

















}

/*
 * add_to_flat_tlist
 *		Add more items to a flattened tlist (if they're not already in
 *it)
 *
 * 'tlist' is the flattened tlist
 * 'exprs' is a list of expressions (usually, but not necessarily, Vars)
 *
 * Returns the extended tlist.
 */
List *
add_to_flat_tlist(List *tlist, List *exprs)
{

















}

/*
 * get_tlist_exprs
 *		Get just the expression subtrees of a tlist
 *
 * Resjunk columns are ignored unless includeJunk is true
 */
List *
get_tlist_exprs(List *tlist, bool includeJunk)
{















}

/*
 * count_nonjunk_tlist_entries
 *		What it says ...
 */
int
count_nonjunk_tlist_entries(List *tlist)
{













}

/*
 * tlist_same_exprs
 *		Check whether two target lists contain the same expressions
 *
 * Note: this function is used to decide whether it's safe to jam a new tlist
 * into a non-projection-capable plan node.  Obviously we can't do that unless
 * the node's tlist shows it already returns the column values we want.
 * However, we can ignore the TargetEntry attributes resname, ressortgroupref,
 * resorigtbl, resorigcol, and resjunk, because those are only labelings that
 * don't affect the row values computed by the node.  (Moreover, if we didn't
 * ignore them, we'd frequently fail to make the desired optimization, since
 * the planner tends to not bother to make resname etc. valid in intermediate
 * plan nodes.)  Note that on success, the caller must still jam the desired
 * tlist into the plan node, else it won't have the desired labeling fields.
 */
bool
tlist_same_exprs(List *tlist1, List *tlist2)
{



















}

/*
 * Does tlist have same output datatypes as listed in colTypes?
 *
 * Resjunk columns are ignored if junkOK is true; otherwise presence of
 * a resjunk column will always cause a 'false' result.
 *
 * Note: currently no callers care about comparing typmods.
 */
bool
tlist_same_datatypes(List *tlist, List *colTypes, bool junkOK)
{
































}

/*
 * Does tlist have same exposed collations as listed in colCollations?
 *
 * Identical logic to the above, but for collations.
 */
bool
tlist_same_collations(List *tlist, List *colCollations, bool junkOK)
{
































}

/*
 * apply_tlist_labeling
 *		Apply the TargetEntry labeling attributes of src_tlist to
 *dest_tlist
 *
 * This is useful for reattaching column names etc to a plan's final output
 * targetlist.
 */
void
apply_tlist_labeling(List *dest_tlist, List *src_tlist)
{















}

/*
 * get_sortgroupref_tle
 *		Find the targetlist entry matching the given SortGroupRef index,
 *		and return it.
 */
TargetEntry *
get_sortgroupref_tle(Index sortref, List *targetList)
{














}

/*
 * get_sortgroupclause_tle
 *		Find the targetlist entry matching the given SortGroupClause
 *		by ressortgroupref, and return it.
 */
TargetEntry *
get_sortgroupclause_tle(SortGroupClause *sgClause, List *targetList)
{

}

/*
 * get_sortgroupclause_expr
 *		Find the targetlist entry matching the given SortGroupClause
 *		by ressortgroupref, and return its expression.
 */
Node *
get_sortgroupclause_expr(SortGroupClause *sgClause, List *targetList)
{



}

/*
 * get_sortgrouplist_exprs
 *		Given a list of SortGroupClauses, build a list
 *		of the referenced targetlist expressions.
 */
List *
get_sortgrouplist_exprs(List *sgClauses, List *targetList)
{












}

/*****************************************************************************
 *		Functions to extract data from a list of SortGroupClauses
 *
 * These don't really belong in tlist.c, but they are sort of related to the
 * functions just above, and they don't seem to deserve their own file.
 *****************************************************************************/

/*
 * get_sortgroupref_clause
 *		Find the SortGroupClause matching the given SortGroupRef index,
 *		and return it.
 */
SortGroupClause *
get_sortgroupref_clause(Index sortref, List *clauses)
{














}

/*
 * get_sortgroupref_clause_noerr
 *		As above, but return NULL rather than throwing an error if not
 *found.
 */
SortGroupClause *
get_sortgroupref_clause_noerr(Index sortref, List *clauses)
{













}

/*
 * extract_grouping_ops - make an array of the equality operator OIDs
 *		for a SortGroupClause list
 */
Oid *
extract_grouping_ops(List *groupClause)
{

















}

/*
 * extract_grouping_collations - make an array of the grouping column collations
 *		for a SortGroupClause list
 */
Oid *
extract_grouping_collations(List *groupClause, List *tlist)
{
















}

/*
 * extract_grouping_cols - make an array of the grouping column resnos
 *		for a SortGroupClause list
 */
AttrNumber *
extract_grouping_cols(List *groupClause, List *tlist)
{
















}

/*
 * grouping_is_sortable - is it possible to implement grouping list by sorting?
 *
 * This is easy since the parser will have included a sortop if one exists.
 */
bool
grouping_is_sortable(List *groupClause)
{












}

/*
 * grouping_is_hashable - is it possible to implement grouping list by hashing?
 *
 * We rely on the parser to have set the hashable flag correctly.
 */
bool
grouping_is_hashable(List *groupClause)
{












}

/*****************************************************************************
 *		PathTarget manipulation functions
 *
 * PathTarget is a somewhat stripped-down version of a full targetlist; it
 * omits all the TargetEntry decoration except (optionally) sortgroupref data,
 * and it adds evaluation cost and output data width info.
 *****************************************************************************/

/*
 * make_pathtarget_from_tlist
 *	  Construct a PathTarget equivalent to the given targetlist.
 *
 * This leaves the cost and width fields as zeroes.  Most callers will want
 * to use create_pathtarget(), so as to get those set.
 */
PathTarget *
make_pathtarget_from_tlist(List *tlist)
{

















}

/*
 * make_tlist_from_pathtarget
 *	  Construct a targetlist from a PathTarget.
 */
List *
make_tlist_from_pathtarget(PathTarget *target)
{




















}

/*
 * copy_pathtarget
 *	  Copy a PathTarget.
 *
 * The new PathTarget has its own List cells, but shares the underlying
 * target expression trees with the old one.  We duplicate the List cells
 * so that items can be added to one target without damaging the other.
 */
PathTarget *
copy_pathtarget(PathTarget *src)
{















}

/*
 * create_empty_pathtarget
 *	  Create an empty (zero columns, zero cost) PathTarget.
 */
PathTarget *
create_empty_pathtarget(void)
{


}

/*
 * add_column_to_pathtarget
 *		Append a target column to the PathTarget.
 *
 * As with make_pathtarget_from_tlist, we leave it to the caller to update
 * the cost and width fields.
 */
void
add_column_to_pathtarget(PathTarget *target, Expr *expr, Index sortgroupref)
{



















}

/*
 * add_new_column_to_pathtarget
 *		Append a target column to the PathTarget, but only if it's not
 *		equal() to any pre-existing target expression.
 *
 * The caller cannot specify a sortgroupref, since it would be unclear how
 * to merge that with a pre-existing column.
 *
 * As with make_pathtarget_from_tlist, we leave it to the caller to update
 * the cost and width fields.
 */
void
add_new_column_to_pathtarget(PathTarget *target, Expr *expr)
{




}

/*
 * add_new_columns_to_pathtarget
 *		Apply add_new_column_to_pathtarget() for each element of the
 *list.
 */
void
add_new_columns_to_pathtarget(PathTarget *target, List *exprs)
{








}

/*
 * apply_pathtarget_labeling_to_tlist
 *		Apply any sortgrouprefs in the PathTarget to matching tlist
 *entries
 *
 * Here, we do not assume that the tlist entries are one-for-one with the
 * PathTarget.  The intended use of this function is to deal with cases
 * where createplan.c has decided to use some other tlist and we have
 * to identify what matches exist.
 */
void
apply_pathtarget_labeling_to_tlist(List *tlist, PathTarget *target)
{






















































}

/*
 * split_pathtarget_at_srfs
 *		Split given PathTarget into multiple levels to position SRFs
 *safely
 *
 * The executor can only handle set-returning functions that appear at the
 * top level of the targetlist of a ProjectSet plan node.  If we have any SRFs
 * that are not at top level, we need to split up the evaluation into multiple
 * plan levels in which each level satisfies this constraint.  This function
 * creates appropriate PathTarget(s) for each level.
 *
 * As an example, consider the tlist expression
 *		x + srf1(srf2(y + z))
 * This expression should appear as-is in the top PathTarget, but below that
 * we must have a PathTarget containing
 *		x, srf1(srf2(y + z))
 * and below that, another PathTarget containing
 *		x, srf2(y + z)
 * and below that, another PathTarget containing
 *		x, y, z
 * When these tlists are processed by setrefs.c, subexpressions that match
 * output expressions of the next lower tlist will be replaced by Vars,
 * so that what the executor gets are tlists looking like
 *		Var1 + Var2
 *		Var1, srf1(Var2)
 *		Var1, srf2(Var2 + Var3)
 *		x, y, z
 * which satisfy the desired property.
 *
 * Another example is
 *		srf1(x), srf2(srf3(y))
 * That must appear as-is in the top PathTarget, but below that we need
 *		srf1(x), srf3(y)
 * That is, each SRF must be computed at a level corresponding to the nesting
 * depth of SRFs within its arguments.
 *
 * In some cases, a SRF has already been evaluated in some previous plan level
 * and we shouldn't expand it again (that is, what we see in the target is
 * already meant as a reference to a lower subexpression).  So, don't expand
 * any tlist expressions that appear in input_target, if that's not NULL.
 *
 * It's also important that we preserve any sortgroupref annotation appearing
 * in the given target, especially on expressions matching input_target items.
 *
 * The outputs of this function are two parallel lists, one a list of
 * PathTargets and the other an integer list of bool flags indicating
 * whether the corresponding PathTarget contains any evaluable SRFs.
 * The lists are given in the order they'd need to be evaluated in, with
 * the "lowest" PathTarget first.  So the last list entry is always the
 * originally given PathTarget, and any entries before it indicate evaluation
 * levels that must be inserted below it.  The first list entry must not
 * contain any SRFs (other than ones duplicating input_target entries), since
 * it will typically be attached to a plan node that cannot evaluate SRFs.
 *
 * Note: using a list for the flags may seem like overkill, since there
 * are only a few possible patterns for which levels contain SRFs.
 * But this representation decouples callers from that knowledge.
 */
void
split_pathtarget_at_srfs(PlannerInfo *root, PathTarget *target, PathTarget *input_target, List **targets, List **targets_contain_srfs)
{



















































































































































































}

/*
 * Recursively examine expressions for split_pathtarget_at_srfs.
 *
 * Note we make no effort here to prevent duplicate entries in the output
 * lists.  Duplicates will be gotten rid of later.
 */
static bool
split_pathtarget_walker(Node *node, split_pathtarget_context *context)
{





































































































}

/*
 * Add a split_pathtarget_item to the PathTarget, unless a matching item is
 * already present.  This is like add_new_column_to_pathtarget, but allows
 * for sortgrouprefs to be handled.  An item having zero sortgroupref can
 * be merged with one that has a sortgroupref, acquiring the latter's
 * sortgroupref.
 *
 * Note that we don't worry about possibly adding duplicate sortgrouprefs
 * to the PathTarget.  That would be bad, but it should be impossible unless
 * the target passed to split_pathtarget_at_srfs already had duplicates.
 * As long as it didn't, we can have at most one split_pathtarget_item with
 * any particular nonzero sortgroupref.
 */
static void
add_sp_item_to_pathtarget(PathTarget *target, split_pathtarget_item *item)
{

































}

/*
 * Apply add_sp_item_to_pathtarget to each element of list.
 */
static void
add_sp_items_to_pathtarget(PathTarget *target, List *items)
{








}