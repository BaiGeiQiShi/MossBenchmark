/*-------------------------------------------------------------------------
 *
 * clauses.c
 *	  routines to manipulate qualification clauses
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/clauses.c
 *
 * HISTORY
 *	  AUTHOR			DATE			MAJOR EVENT
 *	  Andrew Yu			Nov 3, 1994		clause.c and
 *clauses.c combined
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_class.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/functions.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "rewrite/rewriteManip.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)

typedef struct
{
  PlannerInfo *root;
  AggSplit aggsplit;
  AggClauseCosts *costs;
} get_agg_clause_costs_context;

typedef struct
{
  ParamListInfo boundParams;
  PlannerInfo *root;
  List *active_fns;
  Node *case_val;
  bool estimate;
} eval_const_expressions_context;

typedef struct
{
  int nargs;
  List *args;
  int *usecounts;
} substitute_actual_parameters_context;

typedef struct
{
  int nargs;
  List *args;
  int sublevels_up;
} substitute_actual_srf_parameters_context;

typedef struct
{
  char *proname;
  char *prosrc;
} inline_error_callback_arg;

typedef struct
{
  char max_hazard;      /* worst proparallel hazard found so far */
  char max_interesting; /* worst proparallel hazard of interest */
  List *safe_param_ids; /* PARAM_EXEC Param IDs to treat as safe */
} max_parallel_hazard_context;

static bool
contain_agg_clause_walker(Node *node, void *context);
static bool
get_agg_clause_costs_walker(Node *node, get_agg_clause_costs_context *context);
static bool
find_window_functions_walker(Node *node, WindowFuncLists *lists);
static bool
contain_subplans_walker(Node *node, void *context);
static bool
contain_mutable_functions_walker(Node *node, void *context);
static bool
contain_volatile_functions_walker(Node *node, void *context);
static bool
contain_volatile_functions_not_nextval_walker(Node *node, void *context);
static bool
max_parallel_hazard_walker(Node *node, max_parallel_hazard_context *context);
static bool
contain_nonstrict_functions_walker(Node *node, void *context);
static bool
contain_exec_param_walker(Node *node, List *param_ids);
static bool
contain_context_dependent_node(Node *clause);
static bool
contain_context_dependent_node_walker(Node *node, int *flags);
static bool
contain_leaked_vars_walker(Node *node, void *context);
static Relids
find_nonnullable_rels_walker(Node *node, bool top_level);
static List *
find_nonnullable_vars_walker(Node *node, bool top_level);
static bool
is_strict_saop(ScalarArrayOpExpr *expr, bool falseOK);
static Node *
eval_const_expressions_mutator(Node *node, eval_const_expressions_context *context);
static bool
contain_non_const_walker(Node *node, void *context);
static bool
ece_function_is_safe(Oid funcid, eval_const_expressions_context *context);
static List *
simplify_or_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceTrue);
static List *
simplify_and_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceFalse);
static Node *
simplify_boolean_equality(Oid opno, List *args);
static Expr *
simplify_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List **args_p, bool funcvariadic, bool process_args, bool allow_non_const, eval_const_expressions_context *context);
static List *
reorder_function_arguments(List *args, HeapTuple func_tuple);
static List *
add_function_defaults(List *args, HeapTuple func_tuple);
static List *
fetch_function_defaults(HeapTuple func_tuple);
static void
recheck_cast_function_args(List *args, Oid result_type, HeapTuple func_tuple);
static Expr *
evaluate_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context);
static Expr *
inline_function(Oid funcid, Oid result_type, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context);
static Node *
substitute_actual_parameters(Node *expr, int nargs, List *args, int *usecounts);
static Node *
substitute_actual_parameters_mutator(Node *node, substitute_actual_parameters_context *context);
static void
sql_inline_error_callback(void *arg);
static Query *
substitute_actual_srf_parameters(Query *expr, int nargs, List *args);
static Node *
substitute_actual_srf_parameters_mutator(Node *node, substitute_actual_srf_parameters_context *context);
static bool
tlist_matches_coltypelist(List *tlist, List *coltypelist);

/*****************************************************************************
 *		Aggregate-function clause manipulation
 *****************************************************************************/

/*
 * contain_agg_clause
 *	  Recursively search for Aggref/GroupingFunc nodes within a clause.
 *
 *	  Returns true if any aggregate found.
 *
 * This does not descend into subqueries, and so should be used only after
 * reduction of sublinks to subplans, or in contexts where it's known there
 * are no subqueries.  There mustn't be outer-aggregate references either.
 *
 * (If you want something like this but able to deal with subqueries,
 * see rewriteManip.c's contain_aggs_of_level().)
 */
bool
contain_agg_clause(Node *clause)
{

}

static bool
contain_agg_clause_walker(Node *node, void *context)
{
















}

/*
 * get_agg_clause_costs
 *	  Recursively find the Aggref nodes in an expression tree, and
 *	  accumulate cost information about them.
 *
 * 'aggsplit' tells us the expected partial-aggregation mode, which affects
 * the cost estimates.
 *
 * NOTE that the counts/costs are ADDED to those already in *costs ... so
 * the caller is responsible for zeroing the struct initially.
 *
 * We count the nodes, estimate their execution costs, and estimate the total
 * space needed for their transition state values if all are evaluated in
 * parallel (as would be done in a HashAgg plan).  Also, we check whether
 * partial aggregation is feasible.  See AggClauseCosts for the exact set
 * of statistics collected.
 *
 * In addition, we mark Aggref nodes with the correct aggtranstype, so
 * that that doesn't need to be done repeatedly.  (That makes this function's
 * name a bit of a misnomer.)
 *
 * This does not descend into subqueries, and so should be used only after
 * reduction of sublinks to subplans, or in contexts where it's known there
 * are no subqueries.  There mustn't be outer-aggregate references either.
 */
void
get_agg_clause_costs(PlannerInfo *root, Node *clause, AggSplit aggsplit, AggClauseCosts *costs)
{






}

static bool
get_agg_clause_costs_walker(Node *node, get_agg_clause_costs_context *context)
{




















































































































































































































































}

/*****************************************************************************
 *		Window-function clause manipulation
 *****************************************************************************/

/*
 * contain_window_function
 *	  Recursively search for WindowFunc nodes within a clause.
 *
 * Since window functions don't have level fields, but are hard-wired to
 * be associated with the current query level, this is just the same as
 * rewriteManip.c's function.
 */
bool
contain_window_function(Node *clause)
{

}

/*
 * find_window_functions
 *	  Locate all the WindowFunc nodes in an expression tree, and organize
 *	  them by winref ID number.
 *
 * Caller must provide an upper bound on the winref IDs expected in the tree.
 */
WindowFuncLists *
find_window_functions(Node *clause, Index maxWinRef)
{







}

static bool
find_window_functions_walker(Node *node, WindowFuncLists *lists)
{






























}

/*****************************************************************************
 *		Support for expressions returning sets
 *****************************************************************************/

/*
 * expression_returns_set_rows
 *	  Estimate the number of rows returned by a set-returning expression.
 *	  The result is 1 if it's not a set-returning expression.
 *
 * We should only examine the top-level function or operator; it used to be
 * appropriate to recurse, but not anymore.  (Even if there are more SRFs in
 * the function's inputs, their multipliers are accounted for separately.)
 *
 * Note: keep this in sync with expression_returns_set() in nodes/nodeFuncs.c.
 */
double
expression_returns_set_rows(PlannerInfo *root, Node *clause)
{
























}

/*****************************************************************************
 *		Subplan clause manipulation
 *****************************************************************************/

/*
 * contain_subplans
 *	  Recursively search for subplan nodes within a clause.
 *
 * If we see a SubLink node, we will return true.  This is only possible if
 * the expression tree hasn't yet been transformed by subselect.c.  We do not
 * know whether the node will produce a true subplan or just an initplan,
 * but we make the conservative assumption that it will be a subplan.
 *
 * Returns true if any subplan found.
 */
bool
contain_subplans(Node *clause)
{

}

static bool
contain_subplans_walker(Node *node, void *context)
{









}

/*****************************************************************************
 *		Check clauses for mutable functions
 *****************************************************************************/

/*
 * contain_mutable_functions
 *	  Recursively search for mutable functions within a clause.
 *
 * Returns true if any mutable function (or operator implemented by a
 * mutable function) is found.  This test is needed so that we don't
 * mistakenly think that something like "WHERE random() < 0.5" can be treated
 * as a constant qualification.
 *
 * We will recursively look into Query nodes (i.e., SubLink sub-selects)
 * but not into SubPlans.  See comments for contain_volatile_functions().
 */
bool
contain_mutable_functions(Node *clause)
{

}

static bool
contain_mutable_functions_checker(Oid func_id, void *context)
{

}

static bool
contain_mutable_functions_walker(Node *node, void *context)
{







































}

/*****************************************************************************
 *		Check clauses for volatile functions
 *****************************************************************************/

/*
 * contain_volatile_functions
 *	  Recursively search for volatile functions within a clause.
 *
 * Returns true if any volatile function (or operator implemented by a
 * volatile function) is found. This test prevents, for example,
 * invalid conversions of volatile expressions into indexscan quals.
 *
 * We will recursively look into Query nodes (i.e., SubLink sub-selects)
 * but not into SubPlans.  This is a bit odd, but intentional.  If we are
 * looking at a SubLink, we are probably deciding whether a query tree
 * transformation is safe, and a contained sub-select should affect that;
 * for example, duplicating a sub-select containing a volatile function
 * would be bad.  However, once we've got to the stage of having SubPlans,
 * subsequent planning need not consider volatility within those, since
 * the executor won't change its evaluation rules for a SubPlan based on
 * volatility.
 */
bool
contain_volatile_functions(Node *clause)
{

}

static bool
contain_volatile_functions_checker(Oid func_id, void *context)
{

}

static bool
contain_volatile_functions_walker(Node *node, void *context)
{





























}

/*
 * Special purpose version of contain_volatile_functions() for use in COPY:
 * ignore nextval(), but treat all other functions normally.
 */
bool
contain_volatile_functions_not_nextval(Node *clause)
{

}

static bool
contain_volatile_functions_not_nextval_checker(Oid func_id, void *context)
{

}

static bool
contain_volatile_functions_not_nextval_walker(Node *node, void *context)
{

























}

/*****************************************************************************
 *		Check queries for parallel unsafe and/or restricted constructs
 *****************************************************************************/

/*
 * max_parallel_hazard
 *		Find the worst parallel-hazard level in the given query
 *
 * Returns the worst function hazard property (the earliest in this list:
 * PROPARALLEL_UNSAFE, PROPARALLEL_RESTRICTED, PROPARALLEL_SAFE) that can
 * be found in the given parsetree.  We use this to find out whether the query
 * can be parallelized at all.  The caller will also save the result in
 * PlannerGlobal so as to short-circuit checks of portions of the querytree
 * later, in the common case where everything is SAFE.
 */
char
max_parallel_hazard(Query *parse)
{







}

/*
 * is_parallel_safe
 *		Detect whether the given expr contains only parallel-safe
 *functions
 *
 * root->glob->maxParallelHazard must previously have been set to the
 * result of max_parallel_hazard() on the whole query.
 */
bool
is_parallel_safe(PlannerInfo *root, Node *node)
{







































}

/* core logic for all parallel-hazard checks */
static bool
max_parallel_hazard_test(char proparallel, max_parallel_hazard_context *context)
{
























}

/* check_functions_in_node callback */
static bool
max_parallel_hazard_checker(Oid func_id, void *context)
{

}

static bool
max_parallel_hazard_walker(Node *node, max_parallel_hazard_context *context)
{



























































































































































}

/*****************************************************************************
 *		Check clauses for nonstrict functions
 *****************************************************************************/

/*
 * contain_nonstrict_functions
 *	  Recursively search for nonstrict functions within a clause.
 *
 * Returns true if any nonstrict construct is found --- ie, anything that
 * could produce non-NULL output with a NULL input.
 *
 * The idea here is that the caller has verified that the expression contains
 * one or more Var or Param nodes (as appropriate for the caller's need), and
 * now wishes to prove that the expression result will be NULL if any of these
 * inputs is NULL.  If we return false, then the proof succeeded.
 */
bool
contain_nonstrict_functions(Node *clause)
{

}

static bool
contain_nonstrict_functions_checker(Oid func_id, void *context)
{

}

static bool
contain_nonstrict_functions_walker(Node *node, void *context)
{










































































































































}

/*****************************************************************************
 *		Check clauses for Params
 *****************************************************************************/

/*
 * contain_exec_param
 *	  Recursively search for PARAM_EXEC Params within a clause.
 *
 * Returns true if the clause contains any PARAM_EXEC Param with a paramid
 * appearing in the given list of Param IDs.  Does not descend into
 * subqueries!
 */
bool
contain_exec_param(Node *clause, List *param_ids)
{

}

static bool
contain_exec_param_walker(Node *node, List *param_ids)
{














}

/*****************************************************************************
 *		Check clauses for context-dependent nodes
 *****************************************************************************/

/*
 * contain_context_dependent_node
 *	  Recursively search for context-dependent nodes within a clause.
 *
 * CaseTestExpr nodes must appear directly within the corresponding CaseExpr,
 * not nested within another one, or they'll see the wrong test value.  If one
 * appears "bare" in the arguments of a SQL function, then we can't inline the
 * SQL function for fear of creating such a situation.  The same applies for
 * CaseTestExpr used within the elemexpr of an ArrayCoerceExpr.
 *
 * CoerceToDomainValue would have the same issue if domain CHECK expressions
 * could get inlined into larger expressions, but presently that's impossible.
 * Still, it might be allowed in future, or other node types with similar
 * issues might get invented.  So give this function a generic name, and set
 * up the recursion state to allow multiple flag bits.
 */
static bool
contain_context_dependent_node(Node *clause)
{



}

#define CCDN_CASETESTEXPR_OK 0x0001 /* CaseTestExpr okay here? */

static bool
contain_context_dependent_node_walker(Node *node, int *flags)
{
























































}

/*****************************************************************************
 *		  Check clauses for Vars passed to non-leakproof functions
 *****************************************************************************/

/*
 * contain_leaked_vars
 *		Recursively scan a clause to discover whether it contains any
 *Var nodes (of the current query level) that are passed as arguments to leaky
 *functions.
 *
 * Returns true if the clause contains any non-leakproof functions that are
 * passed Var nodes of the current query level, and which might therefore leak
 * data.  Such clauses must be applied after any lower-level security barrier
 * clauses.
 */
bool
contain_leaked_vars(Node *clause)
{

}

static bool
contain_leaked_vars_checker(Oid func_id, void *context)
{

}

static bool
contain_leaked_vars_walker(Node *node, void *context)
{

















































































































































}

/*
 * find_nonnullable_rels
 *		Determine which base rels are forced nonnullable by given
 *clause.
 *
 * Returns the set of all Relids that are referenced in the clause in such
 * a way that the clause cannot possibly return TRUE if any of these Relids
 * is an all-NULL row.  (It is OK to err on the side of conservatism; hence
 * the analysis here is simplistic.)
 *
 * The semantics here are subtly different from contain_nonstrict_functions:
 * that function is concerned with NULL results from arbitrary expressions,
 * but here we assume that the input is a Boolean expression, and wish to
 * see if NULL inputs will provably cause a FALSE-or-NULL result.  We expect
 * the expression to have been AND/OR flattened and converted to implicit-AND
 * format.
 *
 * Note: this function is largely duplicative of find_nonnullable_vars().
 * The reason not to simplify this function into a thin wrapper around
 * find_nonnullable_vars() is that the tested conditions really are different:
 * a clause like "t1.v1 IS NOT NULL OR t1.v2 IS NOT NULL" does not prove
 * that either v1 or v2 can't be NULL, but it does prove that the t1 row
 * as a whole can't be all-NULL.  Also, the behavior for PHVs is different.
 *
 * top_level is true while scanning top-level AND/OR structure; here, showing
 * the result is either FALSE or NULL is good enough.  top_level is false when
 * we have descended below a NOT or a strict function: now we must be able to
 * prove that the subexpression goes to NULL.
 *
 * We don't use expression_tree_walker here because we don't want to descend
 * through very many kinds of nodes; only the ones we can be sure are strict.
 */
Relids
find_nonnullable_rels(Node *clause)
{

}

static Relids
find_nonnullable_rels_walker(Node *node, bool top_level)
{








































































































































































































}

/*
 * find_nonnullable_vars
 *		Determine which Vars are forced nonnullable by given clause.
 *
 * Returns a list of all level-zero Vars that are referenced in the clause in
 * such a way that the clause cannot possibly return TRUE if any of these Vars
 * is NULL.  (It is OK to err on the side of conservatism; hence the analysis
 * here is simplistic.)
 *
 * The semantics here are subtly different from contain_nonstrict_functions:
 * that function is concerned with NULL results from arbitrary expressions,
 * but here we assume that the input is a Boolean expression, and wish to
 * see if NULL inputs will provably cause a FALSE-or-NULL result.  We expect
 * the expression to have been AND/OR flattened and converted to implicit-AND
 * format.
 *
 * The result is a palloc'd List, but we have not copied the member Var nodes.
 * Also, we don't bother trying to eliminate duplicate entries.
 *
 * top_level is true while scanning top-level AND/OR structure; here, showing
 * the result is either FALSE or NULL is good enough.  top_level is false when
 * we have descended below a NOT or a strict function: now we must be able to
 * prove that the subexpression goes to NULL.
 *
 * We don't use expression_tree_walker here because we don't want to descend
 * through very many kinds of nodes; only the ones we can be sure are strict.
 */
List *
find_nonnullable_vars(Node *clause)
{

}

static List *
find_nonnullable_vars_walker(Node *node, bool top_level)
{






















































































































































































}

/*
 * find_forced_null_vars
 *		Determine which Vars must be NULL for the given clause to return
 *TRUE.
 *
 * This is the complement of find_nonnullable_vars: find the level-zero Vars
 * that must be NULL for the clause to return TRUE.  (It is OK to err on the
 * side of conservatism; hence the analysis here is simplistic.  In fact,
 * we only detect simple "var IS NULL" tests at the top level.)
 *
 * The result is a palloc'd List, but we have not copied the member Var nodes.
 * Also, we don't bother trying to eliminate duplicate entries.
 */
List *
find_forced_null_vars(Node *node)
{










































}

/*
 * find_forced_null_var
 *		Return the Var forced null by the given clause, or NULL if it's
 *		not an IS NULL-type clause.  For success, the clause must
 *enforce *only* nullness of the particular Var, not any other conditions.
 *
 * This is just the single-clause case of find_forced_null_vars(), without
 * any allowance for AND conditions.  It's used by initsplan.c on individual
 * qual clauses.  The reason for not just applying find_forced_null_vars()
 * is that if an AND of an IS NULL clause with something else were to somehow
 * survive AND/OR flattening, initsplan.c might get fooled into discarding
 * the whole clause when only the IS NULL part of it had been proved redundant.
 */
Var *
find_forced_null_var(Node *node)
{



































}

/*
 * Can we treat a ScalarArrayOpExpr as strict?
 *
 * If "falseOK" is true, then a "false" result can be considered strict,
 * else we need to guarantee an actual NULL result for NULL input.
 *
 * "foo op ALL array" is strict if the op is strict *and* we can prove
 * that the array input isn't an empty array.  We can check that
 * for the cases of an array constant and an ARRAY[] construct.
 *
 * "foo op ANY array" is strict in the falseOK sense if the op is strict.
 * If not falseOK, the test is the same as for "foo op ALL array".
 */
static bool
is_strict_saop(ScalarArrayOpExpr *expr, bool falseOK)
{












































}

/*****************************************************************************
 *		Check for "pseudo-constant" clauses
 *****************************************************************************/

/*
 * is_pseudo_constant_clause
 *	  Detect whether an expression is "pseudo constant", ie, it contains no
 *	  variables of the current query level and no uses of volatile
 *functions. Such an expr is not necessarily a true constant: it can still
 *contain Params and outer-level Vars, not to mention functions whose results
 *	  may vary from one statement to the next.  However, the expr's value
 *	  will be constant over any one scan of the current query, so it can be
 *	  used as, eg, an indexscan key.  (Actually, the condition for indexscan
 *	  keys is weaker than this; see is_pseudo_constant_for_index().)
 *
 * CAUTION: this function omits to test for one very important class of
 * not-constant expressions, namely aggregates (Aggrefs).  In current usage
 * this is only applied to WHERE clauses and so a check for Aggrefs would be
 * a waste of cycles; but be sure to also check contain_agg_clause() if you
 * want to know about pseudo-constness in other contexts.  The same goes
 * for window functions (WindowFuncs).
 */
bool
is_pseudo_constant_clause(Node *clause)
{











}

/*
 * is_pseudo_constant_clause_relids
 *	  Same as above, except caller already has available the var membership
 *	  of the expression; this lets us avoid the contain_var_clause() scan.
 */
bool
is_pseudo_constant_clause_relids(Node *clause, Relids relids)
{





}

/*****************************************************************************
 *																			 *
 *		General clause-manipulating routines
 **
 *																			 *
 *****************************************************************************/

/*
 * NumRelids
 *		(formerly clause_relids)
 *
 * Returns the number of different relations referenced in 'clause'.
 */
int
NumRelids(Node *clause)
{

}

int
NumRelids_new(PlannerInfo *root, Node *clause)
{





}

/*
 * CommuteOpExpr: commute a binary operator clause
 *
 * XXX the clause is destructively modified!
 */
void
CommuteOpExpr(OpExpr *clause)
{


























}

/*
 * Helper for eval_const_expressions: check that datatype of an attribute
 * is still what it was when the expression was parsed.  This is needed to
 * guard against improper simplification after ALTER COLUMN TYPE.  (XXX we
 * may well need to make similar checks elsewhere?)
 *
 * rowtypeid may come from a whole-row Var, and therefore it can be a domain
 * over composite, but for this purpose we only care about checking the type
 * of a contained field.
 */
static bool
rowtype_field_matches(Oid rowtypeid, int fieldnum, Oid expectedtype, int32 expectedtypmod, Oid expectedcollation)
{






















}

/*--------------------
 * eval_const_expressions
 *
 * Reduce any recognizably constant subexpressions of the given
 * expression tree, for example "2 + 2" => "4".  More interestingly,
 * we can reduce certain boolean expressions even when they contain
 * non-constant subexpressions: "x OR true" => "true" no matter what
 * the subexpression x is.  (XXX We assume that no such subexpression
 * will have important side-effects, which is not necessarily a good
 * assumption in the presence of user-defined functions; do we need a
 * pg_proc flag that prevents discarding the execution of a function?)
 *
 * We do understand that certain functions may deliver non-constant
 * results even with constant inputs, "nextval()" being the classic
 * example.  Functions that are not marked "immutable" in pg_proc
 * will not be pre-evaluated here, although we will reduce their
 * arguments as far as possible.
 *
 * Whenever a function is eliminated from the expression by means of
 * constant-expression evaluation or inlining, we add the function to
 * root->glob->invalItems.  This ensures the plan is known to depend on
 * such functions, even though they aren't referenced anymore.
 *
 * We assume that the tree has already been type-checked and contains
 * only operators and functions that are reasonable to try to execute.
 *
 * NOTE: "root" can be passed as NULL if the caller never wants to do any
 * Param substitutions nor receive info about inlined functions.
 *
 * NOTE: the planner assumes that this will always flatten nested AND and
 * OR clauses into N-argument form.  See comments in prepqual.c.
 *
 * NOTE: another critical effect is that any function calls that require
 * default arguments will be expanded, and named-argument calls will be
 * converted to positional notation.  The executor won't handle either.
 *--------------------
 */
Node *
eval_const_expressions(PlannerInfo *root, Node *node)
{















}

/*--------------------
 * estimate_expression_value
 *
 * This function attempts to estimate the value of an expression for
 * planning purposes.  It is in essence a more aggressive version of
 * eval_const_expressions(): we will perform constant reductions that are
 * not necessarily 100% safe, but are reasonable for estimation purposes.
 *
 * Currently the extra steps that are taken in this mode are:
 * 1. Substitute values for Params, where a bound Param value has been made
 *	  available by the caller of planner(), even if the Param isn't marked
 *	  constant.  This effectively means that we plan using the first
 *supplied value of the Param.
 * 2. Fold stable, as well as immutable, functions to constants.
 * 3. Reduce PlaceHolderVar nodes to their contained expressions.
 *--------------------
 */
Node *
estimate_expression_value(PlannerInfo *root, Node *node)
{









}

/*
 * The generic case in eval_const_expressions_mutator is to recurse using
 * expression_tree_mutator, which will copy the given node unchanged but
 * const-simplify its arguments (if any) as far as possible.  If the node
 * itself does immutable processing, and each of its arguments were reduced
 * to a Const, we can then reduce it to a Const using evaluate_expr.  (Some
 * node types need more complicated logic; for example, a CASE expression
 * might be reducible to a constant even if not all its subtrees are.)
 */
#define ece_generic_processing(node) expression_tree_mutator((Node *)(node), eval_const_expressions_mutator, (void *)context)

/*
 * Check whether all arguments of the given node were reduced to Consts.
 * By going directly to expression_tree_walker, contain_non_const_walker
 * is not applied to the node itself, only to its children.
 */
#define ece_all_arguments_const(node) (!expression_tree_walker((Node *)(node), contain_non_const_walker, NULL))

/* Generic macro for applying evaluate_expr */
#define ece_evaluate_expr(node) ((Node *)evaluate_expr((Expr *)(node), exprType((Node *)(node)), exprTypmod((Node *)(node)), exprCollation((Node *)(node))))

/*
 * Recursive guts of eval_const_expressions/estimate_expression_value
 */
static Node *
eval_const_expressions_mutator(Node *node, eval_const_expressions_context *context)
{




































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































}

/*
 * Subroutine for eval_const_expressions: check for non-Const nodes.
 *
 * We can abort recursion immediately on finding a non-Const node.  This is
 * critical for performance, else eval_const_expressions_mutator would take
 * O(N^2) time on non-simplifiable trees.  However, we do need to descend
 * into List nodes since expression_tree_walker sometimes invokes the walker
 * function directly on List subtrees.
 */
static bool
contain_non_const_walker(Node *node, void *context)
{














}

/*
 * Subroutine for eval_const_expressions: check if a function is OK to evaluate
 */
static bool
ece_function_is_safe(Oid funcid, eval_const_expressions_context *context)
{


















}

/*
 * Subroutine for eval_const_expressions: process arguments of an OR clause
 *
 * This includes flattening of nested ORs as well as recursion to
 * eval_const_expressions to simplify the OR arguments.
 *
 * After simplification, OR arguments are handled as follows:
 *		non constant: keep
 *		FALSE: drop (does not affect result)
 *		TRUE: force result to TRUE
 *		NULL: keep only one
 * We must keep one NULL input because OR expressions evaluate to NULL when no
 * input is TRUE and at least one is NULL.  We don't actually include the NULL
 * here, that's supposed to be done by the caller.
 *
 * The output arguments *haveNull and *forceTrue must be initialized false
 * by the caller.  They will be set true if a NULL constant or TRUE constant,
 * respectively, is detected anywhere in the argument list.
 */
static List *
simplify_or_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceTrue)
{


























































































}

/*
 * Subroutine for eval_const_expressions: process arguments of an AND clause
 *
 * This includes flattening of nested ANDs as well as recursion to
 * eval_const_expressions to simplify the AND arguments.
 *
 * After simplification, AND arguments are handled as follows:
 *		non constant: keep
 *		TRUE: drop (does not affect result)
 *		FALSE: force result to FALSE
 *		NULL: keep only one
 * We must keep one NULL input because AND expressions evaluate to NULL when
 * no input is FALSE and at least one is NULL.  We don't actually include the
 * NULL here, that's supposed to be done by the caller.
 *
 * The output arguments *haveNull and *forceFalse must be initialized false
 * by the caller.  They will be set true if a null constant or false constant,
 * respectively, is detected anywhere in the argument list.
 */
static List *
simplify_and_arguments(List *args, eval_const_expressions_context *context, bool *haveNull, bool *forceFalse)
{
















































































}

/*
 * Subroutine for eval_const_expressions: try to simplify boolean equality
 * or inequality condition
 *
 * Inputs are the operator OID and the simplified arguments to the operator.
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the expression.
 *
 * The idea here is to reduce "x = true" to "x" and "x = false" to "NOT x",
 * or similarly "x <> true" to "NOT x" and "x <> false" to "x".
 * This is only marginally useful in itself, but doing it in constant folding
 * ensures that we will recognize these forms as being equivalent in, for
 * example, partial index matching.
 *
 * We come here only if simplify_function has failed; therefore we cannot
 * see two constant inputs, nor a constant-NULL input.
 */
static Node *
simplify_boolean_equality(Oid opno, List *args)
{



























































}

/*
 * Subroutine for eval_const_expressions: try to simplify a function call
 * (which might originally have been an operator; we don't care)
 *
 * Inputs are the function OID, actual result type OID (which is needed for
 * polymorphic functions), result typmod, result collation, the input
 * collation to use for the function, the original argument list (not
 * const-simplified yet, unless process_args is false), and some flags;
 * also the context data for eval_const_expressions.
 *
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the function call.
 *
 * This function is also responsible for converting named-notation argument
 * lists into positional notation and/or adding any needed default argument
 * expressions; which is a bit grotty, but it avoids extra fetches of the
 * function's pg_proc tuple.  For this reason, the args list is
 * pass-by-reference.  Conversion and const-simplification of the args list
 * will be done even if simplification of the function call itself is not
 * possible.
 */
static Expr *
simplify_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List **args_p, bool funcvariadic, bool process_args, bool allow_non_const, eval_const_expressions_context *context)
{



















































































}

/*
 * expand_function_arguments: convert named-notation args to positional args
 * and/or insert default args, as needed
 *
 * If we need to change anything, the input argument list is copied, not
 * modified.
 *
 * Note: this gets applied to operator argument lists too, even though the
 * cases it handles should never occur there.  This should be OK since it
 * will fall through very quickly if there's nothing to do.
 */
List *
expand_function_arguments(List *args, Oid result_type, HeapTuple func_tuple)
{
































}

/*
 * reorder_function_arguments: convert named-notation args to positional args
 *
 * This function also inserts default argument values as needed, since it's
 * impossible to form a truly valid positional call without that.
 */
static List *
reorder_function_arguments(List *args, HeapTuple func_tuple)
{































































}

/*
 * add_function_defaults: add missing function arguments from its defaults
 *
 * This is used only when the argument list was positional to begin with,
 * and so we know we just need to add defaults at the end.
 */
static List *
add_function_defaults(List *args, HeapTuple func_tuple)
{





















}

/*
 * fetch_function_defaults: get function's default arguments as expression list
 */
static List *
fetch_function_defaults(HeapTuple func_tuple)
{















}

/*
 * recheck_cast_function_args: recheck function args and typecast as needed
 * after adding defaults.
 *
 * It is possible for some of the defaulted arguments to be polymorphic;
 * therefore we can't assume that the default expressions have the correct
 * data types already.  We have to re-resolve polymorphics and do coercion
 * just like the parser did.
 *
 * This should be a no-op if there are no polymorphic arguments,
 * but we do it anyway to be sure.
 *
 * Note: if any casts are needed, the args list is modified in-place;
 * caller should have already copied the list structure.
 */
static void
recheck_cast_function_args(List *args, Oid result_type, HeapTuple func_tuple)
{



























}

/*
 * evaluate_function: try to pre-evaluate a function call
 *
 * We can do this if the function is strict and has any constant-null inputs
 * (just return a null constant), or if the function is immutable and has all
 * constant inputs (call it and return the result as a Const node).  In
 * estimation mode we are willing to pre-evaluate stable functions too.
 *
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the function.
 */
static Expr *
evaluate_function(Oid funcid, Oid result_type, int32 result_typmod, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context)
{



































































































}

/*
 * inline_function: try to expand a function call inline
 *
 * If the function is a sufficiently simple SQL-language function
 * (just "SELECT expression"), then we can inline it and avoid the rather
 * high per-call overhead of SQL functions.  Furthermore, this can expose
 * opportunities for constant-folding within the function expression.
 *
 * We have to beware of some special cases however.  A directly or
 * indirectly recursive function would cause us to recurse forever,
 * so we keep track of which functions we are already expanding and
 * do not re-expand them.  Also, if a parameter is used more than once
 * in the SQL-function body, we require it not to contain any volatile
 * functions (volatiles might deliver inconsistent answers) nor to be
 * unreasonably expensive to evaluate.  The expensiveness check not only
 * prevents us from doing multiple evaluations of an expensive parameter
 * at runtime, but is a safety value to limit growth of an expression due
 * to repeated inlining.
 *
 * We must also beware of changing the volatility or strictness status of
 * functions by inlining them.
 *
 * Also, at the moment we can't inline functions returning RECORD.  This
 * doesn't work in the general case because it discards information such
 * as OUT-parameter declarations.
 *
 * Also, context-dependent expression nodes in the argument list are trouble.
 *
 * Returns a simplified expression if successful, or NULL if cannot
 * simplify the function.
 */
static Expr *
inline_function(Oid funcid, Oid result_type, Oid result_collid, Oid input_collid, List *args, bool funcvariadic, HeapTuple func_tuple, eval_const_expressions_context *context)
{


















































































































































































































































































































}

/*
 * Replace Param nodes by appropriate actual parameters
 */
static Node *
substitute_actual_parameters(Node *expr, int nargs, List *args, int *usecounts)
{







}

static Node *
substitute_actual_parameters_mutator(Node *node, substitute_actual_parameters_context *context)
{

























}

/*
 * error context callback to let us supply a call-stack traceback
 */
static void
sql_inline_error_callback(void *arg)
{













}

/*
 * evaluate_expr: pre-evaluate a constant expression
 *
 * We use the executor's routine ExecEvalExpr() to avoid duplication of
 * code and ensure we get the same result as the executor would get.
 */
Expr *
evaluate_expr(Expr *expr, Oid result_type, int32 result_typmod, Oid result_collation)
{




































































}

/*
 * inline_set_returning_function
 *		Attempt to "inline" a set-returning function in the FROM clause.
 *
 * "rte" is an RTE_FUNCTION rangetable entry.  If it represents a call of a
 * set-returning SQL function that can safely be inlined, expand the function
 * and return the substitute Query structure.  Otherwise, return NULL.
 *
 * This has a good deal of similarity to inline_function(), but that's
 * for the non-set-returning case, and there are enough differences to
 * justify separate functions.
 */
Query *
inline_set_returning_function(PlannerInfo *root, RangeTblEntry *rte)
{






























































































































































































































































































}

/*
 * Replace Param nodes by appropriate actual parameters
 *
 * This is just enough different from substitute_actual_parameters()
 * that it needs its own code.
 */
static Query *
substitute_actual_srf_parameters(Query *expr, int nargs, List *args)
{







}

static Node *
substitute_actual_srf_parameters_mutator(Node *node, substitute_actual_srf_parameters_context *context)
{


































}

/*
 * Check whether a SELECT targetlist emits the specified column types,
 * to see if it's safe to inline a function returning record.
 *
 * We insist on exact match here.  The executor allows binary-coercible
 * cases too, but we don't have a way to preserve the correct column types
 * in the correct places if we inline the function in such a case.
 *
 * Note that we only check type OIDs not typmods; this agrees with what the
 * executor would do at runtime, and attributing a specific typmod to a
 * function result is largely wishful thinking anyway.
 */
static bool
tlist_matches_coltypelist(List *tlist, List *coltypelist)
{


































}