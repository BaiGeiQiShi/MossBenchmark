/*-------------------------------------------------------------------------
 *
 * rewriteManip.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/rewrite/rewriteManip.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"

typedef struct
{
  int sublevels_up;
} contain_aggs_of_level_context;

typedef struct
{
  int agg_location;
  int sublevels_up;
} locate_agg_of_level_context;

typedef struct
{
  int win_location;
} locate_windowfunc_context;

static bool
contain_aggs_of_level_walker(Node *node, contain_aggs_of_level_context *context);
static bool
locate_agg_of_level_walker(Node *node, locate_agg_of_level_context *context);
static bool
contain_windowfuncs_walker(Node *node, void *context);
static bool
locate_windowfunc_walker(Node *node, locate_windowfunc_context *context);
static bool
checkExprHasSubLink_walker(Node *node, void *context);
static Relids
offset_relid_set(Relids relids, int offset);
static Relids
adjust_relid_set(Relids relids, int oldrelid, int newrelid);

/*
 * contain_aggs_of_level -
 *	Check if an expression contains an aggregate function call of a
 *	specified query level.
 *
 * The objective of this routine is to detect whether there are aggregates
 * belonging to the given query level.  Aggregates belonging to subqueries
 * or outer queries do NOT cause a true result.  We must recurse into
 * subqueries to detect outer-reference aggregates that logically belong to
 * the specified query level.
 */
bool
contain_aggs_of_level(Node *node, int levelsup)
{









}

static bool
contain_aggs_of_level_walker(Node *node, contain_aggs_of_level_context *context)
{































}

/*
 * locate_agg_of_level -
 *	  Find the parse location of any aggregate of the specified query level.
 *
 * Returns -1 if no such agg is in the querytree, or if they all have
 * unknown parse location.  (The former case is probably caller error,
 * but we don't bother to distinguish it from the latter case.)
 *
 * Note: it might seem appropriate to merge this functionality into
 * contain_aggs_of_level, but that would complicate that function's API.
 * Currently, the only uses of this function are for error reporting,
 * and so shaving cycles probably isn't very important.
 */
int
locate_agg_of_level(Node *node, int levelsup)
{












}

static bool
locate_agg_of_level_walker(Node *node, locate_agg_of_level_context *context)
{
































}

/*
 * contain_windowfuncs -
 *	Check if an expression contains a window function call of the
 *	current query level.
 */
bool
contain_windowfuncs(Node *node)
{





}

static bool
contain_windowfuncs_walker(Node *node, void *context)
{










}

/*
 * locate_windowfunc -
 *	  Find the parse location of any windowfunc of the current query level.
 *
 * Returns -1 if no such windowfunc is in the querytree, or if they all have
 * unknown parse location.  (The former case is probably caller error,
 * but we don't bother to distinguish it from the latter case.)
 *
 * Note: it might seem appropriate to merge this functionality into
 * contain_windowfuncs, but that would complicate that function's API.
 * Currently, the only uses of this function are for error reporting,
 * and so shaving cycles probably isn't very important.
 */
int
locate_windowfunc(Node *node)
{











}

static bool
locate_windowfunc_walker(Node *node, locate_windowfunc_context *context)
{















}

/*
 * checkExprHasSubLink -
 *	Check if an expression contains a SubLink.
 */
bool
checkExprHasSubLink(Node *node)
{





}

static bool
checkExprHasSubLink_walker(Node *node, void *context)
{









}

/*
 * Check for MULTIEXPR Param within expression tree
 *
 * We intentionally don't descend into SubLinks: only Params at the current
 * query level are of interest.
 */
static bool
contains_multiexpr_param(Node *node, void *context)
{













}

/*
 * OffsetVarNodes - adjust Vars when appending one query's RT to another
 *
 * Find all Var nodes in the given tree with varlevelsup == sublevels_up,
 * and increment their varno fields (rangetable indexes) by 'offset'.
 * The varnoold fields are adjusted similarly.  Also, adjust other nodes
 * that contain rangetable indexes, such as RangeTblRef and JoinExpr.
 *
 * NOTE: although this has the form of a walker, we cheat and modify the
 * nodes in-place.  The given expression tree should have been copied
 * earlier to ensure that no unwanted side-effects occur!
 */

typedef struct
{
  int offset;
  int sublevels_up;
} OffsetVarNodes_context;

static bool
OffsetVarNodes_walker(Node *node, OffsetVarNodes_context *context)
{




















































































}

void
OffsetVarNodes(Node *node, int offset, int sublevels_up)
{
















































}

static Relids
offset_relid_set(Relids relids, int offset)
{









}

/*
 * ChangeVarNodes - adjust Var nodes for a specific change of RT index
 *
 * Find all Var nodes in the given tree belonging to a specific relation
 * (identified by sublevels_up and rt_index), and change their varno fields
 * to 'new_index'.  The varnoold fields are changed too.  Also, adjust other
 * nodes that contain rangetable indexes, such as RangeTblRef and JoinExpr.
 *
 * NOTE: although this has the form of a walker, we cheat and modify the
 * nodes in-place.  The given expression tree should have been copied
 * earlier to ensure that no unwanted side-effects occur!
 */

typedef struct
{
  int rt_index;
  int new_index;
  int sublevels_up;
} ChangeVarNodes_context;

static bool
ChangeVarNodes_walker(Node *node, ChangeVarNodes_context *context)
{










































































































}

void
ChangeVarNodes(Node *node, int rt_index, int new_index, int sublevels_up)
{





















































}

/*
 * Substitute newrelid for oldrelid in a Relid set
 */
static Relids
adjust_relid_set(Relids relids, int oldrelid, int newrelid)
{









}

/*
 * IncrementVarSublevelsUp - adjust Var nodes when pushing them down in tree
 *
 * Find all Var nodes in the given tree having varlevelsup >= min_sublevels_up,
 * and add delta_sublevels_up to their varlevelsup value.  This is needed when
 * an expression that's correct for some nesting level is inserted into a
 * subquery.  Ordinarily the initial call has min_sublevels_up == 0 so that
 * all Vars are affected.  The point of min_sublevels_up is that we can
 * increment it when we recurse into a sublink, so that local variables in
 * that sublink are not affected, only outer references to vars that belong
 * to the expression's original query level or parents thereof.
 *
 * Likewise for other nodes containing levelsup fields, such as Aggref.
 *
 * NOTE: although this has the form of a walker, we cheat and modify the
 * Var nodes in-place.  The given expression tree should have been copied
 * earlier to ensure that no unwanted side-effects occur!
 */

typedef struct
{
  int delta_sublevels_up;
  int min_sublevels_up;
} IncrementVarSublevelsUp_context;

static bool
IncrementVarSublevelsUp_walker(Node *node, IncrementVarSublevelsUp_context *context)
{













































































}

void
IncrementVarSublevelsUp(Node *node, int delta_sublevels_up, int min_sublevels_up)
{










}

/*
 * IncrementVarSublevelsUp_rtable -
 *	Same as IncrementVarSublevelsUp, but to be invoked on a range table.
 */
void
IncrementVarSublevelsUp_rtable(List *rtable, int delta_sublevels_up, int min_sublevels_up)
{






}

/*
 * rangeTableEntry_used - detect whether an RTE is referenced somewhere
 *	in var nodes or join or setOp trees of a query or expression.
 */

typedef struct
{
  int rt_index;
  int sublevels_up;
} rangeTableEntry_used_context;

static bool
rangeTableEntry_used_walker(Node *node, rangeTableEntry_used_context *context)
{
































































}

bool
rangeTableEntry_used(Node *node, int rt_index, int sublevels_up)
{










}

/*
 * If the given Query is an INSERT ... SELECT construct, extract and
 * return the sub-Query node that represents the SELECT part.  Otherwise
 * return the given Query.
 *
 * If subquery_ptr is not NULL, then *subquery_ptr is set to the location
 * of the link to the SELECT subquery inside parsetree, or NULL if not an
 * INSERT ... SELECT.
 *
 * This is a hack needed because transformations on INSERT ... SELECTs that
 * appear in rule actions should be applied to the source SELECT, not to the
 * INSERT part.  Perhaps this can be cleaned up with redesigned querytrees.
 */
Query *
getInsertSelectQuery(Query *parsetree, Query ***subquery_ptr)
{



















































}

/*
 * Add the given qualifier condition to the query's WHERE clause
 */
void
AddQual(Query *parsetree, Node *qual)
{




























































}

/*
 * Invert the given clause and add it to the WHERE qualifications of the
 * given querytree.  Inversion means "x IS NOT TRUE", not just "NOT x",
 * else we will do the wrong thing when x evaluates to NULL.
 */
void
AddInvertedQual(Query *parsetree, Node *qual)
{














}

/*
 * replace_rte_variables() finds all Vars in an expression tree
 * that reference a particular RTE, and replaces them with substitute
 * expressions obtained from a caller-supplied callback function.
 *
 * When invoking replace_rte_variables on a portion of a Query, pass the
 * address of the containing Query's hasSubLinks field as outer_hasSubLinks.
 * Otherwise, pass NULL, but inserting a SubLink into a non-Query expression
 * will then cause an error.
 *
 * Note: the business with inserted_sublink is needed to update hasSubLinks
 * in subqueries when the replacement adds a subquery inside a subquery.
 * Messy, isn't it?  We do not need to do similar pushups for hasAggs,
 * because it isn't possible for this transformation to insert a level-zero
 * aggregate reference into a subquery --- it could only insert outer aggs.
 * Likewise for hasWindowFuncs.
 *
 * Note: usually, we'd not expose the mutator function or context struct
 * for a function like this.  We do so because callbacks often find it
 * convenient to recurse directly to the mutator on sub-expressions of
 * what they will return.
 */
Node *
replace_rte_variables(Node *node, int target_varno, int sublevels_up, replace_rte_variables_callback callback, void *callback_arg, bool *outer_hasSubLinks)
{
















































}

Node *
replace_rte_variables_mutator(Node *node, replace_rte_variables_context *context)
{























































}

/*
 * map_variable_attnos() finds all user-column Vars in an expression tree
 * that reference a particular RTE, and adjusts their varattnos according
 * to the given mapping array (varattno n is replaced by attno_map[n-1]).
 * Vars for system columns are not modified.
 *
 * A zero in the mapping array represents a dropped column, which should not
 * appear in the expression.
 *
 * If the expression tree contains a whole-row Var for the target RTE,
 * *found_whole_row is set to true.  In addition, if to_rowtype is
 * not InvalidOid, we replace the Var with a Var of that vartype, inserting
 * a ConvertRowtypeExpr to map back to the rowtype expected by the expression.
 * (Therefore, to_rowtype had better be a child rowtype of the rowtype of the
 * RTE we're changing references to.)  Callers that don't provide to_rowtype
 * should report an error if *found_row_type is true; we don't do that here
 * because we don't know exactly what wording for the error message would
 * be most appropriate.  The caller will be aware of the context.
 *
 * This could be built using replace_rte_variables and a callback function,
 * but since we don't ever need to insert sublinks, replace_rte_variables is
 * overly complicated.
 */

typedef struct
{
  int target_varno;            /* RTE index to search for */
  int sublevels_up;            /* (current) nesting depth */
  const AttrNumber *attno_map; /* map array for user attnos */
  int map_length;              /* number of entries in attno_map[] */
  Oid to_rowtype;              /* change whole-row Vars to this type */
  bool *found_whole_row;       /* output flag */
} map_variable_attnos_context;

static Node *
map_variable_attnos_mutator(Node *node, map_variable_attnos_context *context)
{









































































































}

Node *
map_variable_attnos(Node *node, int target_varno, int sublevels_up, const AttrNumber *attno_map, int map_length, Oid to_rowtype, bool *found_whole_row)
{
















}

/*
 * ReplaceVarsFromTargetList - replace Vars with items from a targetlist
 *
 * Vars matching target_varno and sublevels_up are replaced by the
 * entry with matching resno from targetlist, if there is one.
 *
 * If there is no matching resno for such a Var, the action depends on the
 * nomatch_option:
 *	REPLACEVARS_REPORT_ERROR: throw an error
 *	REPLACEVARS_CHANGE_VARNO: change Var's varno to nomatch_varno
 *	REPLACEVARS_SUBSTITUTE_NULL: replace Var with a NULL Const of same type
 *
 * The caller must also provide target_rte, the RTE describing the target
 * relation.  This is needed to handle whole-row Vars referencing the target.
 * We expand such Vars into RowExpr constructs.
 *
 * outer_hasSubLinks works the same as for replace_rte_variables().
 */

typedef struct
{
  RangeTblEntry *target_rte;
  List *targetlist;
  ReplaceVarsNoMatchOption nomatch_option;
  int nomatch_varno;
} ReplaceVarsFromTargetList_context;

static Node *
ReplaceVarsFromTargetList_callback(Var *var, replace_rte_variables_context *context)
{























































































}

Node *
ReplaceVarsFromTargetList(Node *node, int target_varno, int sublevels_up, RangeTblEntry *target_rte, List *targetlist, ReplaceVarsNoMatchOption nomatch_option, int nomatch_varno, bool *outer_hasSubLinks)
{








}