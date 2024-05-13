/*-------------------------------------------------------------------------
 *
 * var.c
 *	  Var node manipulation routines
 *
 * Note: for most purposes, PlaceHolderVar is considered a Var too,
 * even if its contained expression is variable-free.  Also, CurrentOfExpr
 * is treated as a Var for purposes of determining whether an expression
 * contains variables.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/var.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/sysattr.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/placeholder.h"
#include "optimizer/prep.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"

typedef struct
{
  Relids varnos;
  PlannerInfo *root;
  int sublevels_up;
} pull_varnos_context;

typedef struct
{
  Bitmapset *varattnos;
  Index varno;
} pull_varattnos_context;

typedef struct
{
  List *vars;
  int sublevels_up;
} pull_vars_context;

typedef struct
{
  int var_location;
  int sublevels_up;
} locate_var_of_level_context;

typedef struct
{
  List *varlist;
  int flags;
} pull_var_clause_context;

typedef struct
{
  Query *query; /* outer Query */
  int sublevels_up;
  bool possible_sublink; /* could aliases include a SubLink? */
  bool inserted_sublink; /* have we inserted a SubLink? */
} flatten_join_alias_vars_context;

static bool
pull_varnos_walker(Node *node, pull_varnos_context *context);
static bool
pull_varattnos_walker(Node *node, pull_varattnos_context *context);
static bool
pull_vars_walker(Node *node, pull_vars_context *context);
static bool
contain_var_clause_walker(Node *node, void *context);
static bool
contain_vars_of_level_walker(Node *node, int *sublevels_up);
static bool
locate_var_of_level_walker(Node *node, locate_var_of_level_context *context);
static bool
pull_var_clause_walker(Node *node, pull_var_clause_context *context);
static Node *
flatten_join_alias_vars_mutator(Node *node, flatten_join_alias_vars_context *context);
static Relids
alias_relid_set(Query *query, Relids relids);

/*
 * pull_varnos
 *		Create a set of all the distinct varnos present in a parsetree.
 *		Only varnos that reference level-zero rtable entries are
 *considered.
 *
 * NOTE: this is used on not-yet-planned expressions.  It may therefore find
 * bare SubLinks, and if so it needs to recurse into them to look for uplevel
 * references to the desired rtable level!	But when we find a completed
 * SubPlan, we only need to look at the parameters passed to the subplan.
 */
Relids
pull_varnos(Node *node)
{

}

Relids
pull_varnos_new(PlannerInfo *root, Node *node)
{













}

/*
 * pull_varnos_of_level
 *		Create a set of all the distinct varnos present in a parsetree.
 *		Only Vars of the specified level are considered.
 */
Relids
pull_varnos_of_level(Node *node, int levelsup)
{

}

Relids
pull_varnos_of_level_new(PlannerInfo *root, Node *node, int levelsup)
{













}

static bool
pull_varnos_walker(Node *node, pull_varnos_context *context)
{























































































































}

/*
 * pull_varattnos
 *		Find all the distinct attribute numbers present in an expression
 *tree, and add them to the initial contents of *varattnos. Only Vars of the
 *given varno and rtable level zero are considered.
 *
 * Attribute numbers are offset by FirstLowInvalidHeapAttributeNumber so that
 * we can include system attributes (e.g., OID) in the bitmap representation.
 *
 * Currently, this does not support unplanned subqueries; that is not needed
 * for current uses.  It will handle already-planned SubPlan nodes, though,
 * looking into only the "testexpr" and the "args" list.  (The subplan cannot
 * contain any other references to Vars of the current level.)
 */
void
pull_varattnos(Node *node, Index varno, Bitmapset **varattnos)
{








}

static bool
pull_varattnos_walker(Node *node, pull_varattnos_context *context)
{



















}

/*
 * pull_vars_of_level
 *		Create a list of all Vars (and PlaceHolderVars) referencing the
 *		specified query level in the given parsetree.
 *
 * Caution: the Vars are not copied, only linked into the list.
 */
List *
pull_vars_of_level(Node *node, int levelsup)
{












}

static bool
pull_vars_walker(Node *node, pull_vars_context *context)
{




































}

/*
 * contain_var_clause
 *	  Recursively scan a clause to discover whether it contains any Var
 *nodes (of the current query level).
 *
 *	  Returns true if any varnode found.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
bool
contain_var_clause(Node *node)
{

}

static bool
contain_var_clause_walker(Node *node, void *context)
{

























}

/*
 * contain_vars_of_level
 *	  Recursively scan a clause to discover whether it contains any Var
 *nodes of the specified query level.
 *
 *	  Returns true if any such Var found.
 *
 * Will recurse into sublinks.  Also, may be invoked directly on a Query.
 */
bool
contain_vars_of_level(Node *node, int levelsup)
{



}

static bool
contain_vars_of_level_walker(Node *node, int *sublevels_up)
{







































}

/*
 * locate_var_of_level
 *	  Find the parse location of any Var of the specified query level.
 *
 * Returns -1 if no such Var is in the querytree, or if they all have
 * unknown parse location.  (The former case is probably caller error,
 * but we don't bother to distinguish it from the latter case.)
 *
 * Will recurse into sublinks.  Also, may be invoked directly on a Query.
 *
 * Note: it might seem appropriate to merge this functionality into
 * contain_vars_of_level, but that would complicate that function's API.
 * Currently, the only uses of this function are for error reporting,
 * and so shaving cycles probably isn't very important.
 */
int
locate_var_of_level(Node *node, int levelsup)
{








}

static bool
locate_var_of_level_walker(Node *node, locate_var_of_level_context *context)
{
































}

/*
 * pull_var_clause
 *	  Recursively pulls all Var nodes from an expression clause.
 *
 *	  Aggrefs are handled according to these bits in 'flags':
 *		PVC_INCLUDE_AGGREGATES		include Aggrefs in output list
 *		PVC_RECURSE_AGGREGATES		recurse into Aggref arguments
 *		neither flag				throw error if Aggref
 *found Vars within an Aggref's expression are included in the result only when
 *PVC_RECURSE_AGGREGATES is specified.
 *
 *	  WindowFuncs are handled according to these bits in 'flags':
 *		PVC_INCLUDE_WINDOWFUNCS		include WindowFuncs in output
 *list PVC_RECURSE_WINDOWFUNCS		recurse into WindowFunc arguments
 *		neither flag				throw error if
 *WindowFunc found Vars within a WindowFunc's expression are included in the
 *result only when PVC_RECURSE_WINDOWFUNCS is specified.
 *
 *	  PlaceHolderVars are handled according to these bits in 'flags':
 *		PVC_INCLUDE_PLACEHOLDERS	include PlaceHolderVars in
 *output list PVC_RECURSE_PLACEHOLDERS	recurse into PlaceHolderVar arguments
 *		neither flag				throw error if
 *PlaceHolderVar found Vars within a PHV's expression are included in the result
 *only when PVC_RECURSE_PLACEHOLDERS is specified.
 *
 *	  GroupingFuncs are treated exactly like Aggrefs, and so do not need
 *	  their own flag bits.
 *
 *	  CurrentOfExpr nodes are ignored in all cases.
 *
 *	  Upper-level vars (with varlevelsup > 0) should not be seen here,
 *	  likewise for upper-level Aggrefs and PlaceHolderVars.
 *
 *	  Returns list of nodes found.  Note the nodes themselves are not
 *	  copied, only referenced.
 *
 * Does not examine subqueries, therefore must only be used after reduction
 * of sublinks to subplans!
 */
List *
pull_var_clause(Node *node, int flags)
{












}

static bool
pull_var_clause_walker(Node *node, pull_var_clause_context *context)
{































































































}

/*
 * flatten_join_alias_vars
 *	  Replace Vars that reference JOIN outputs with references to the
 *original relation variables instead.  This allows quals involving such vars to
 *be pushed down.  Whole-row Vars that reference JOIN relations are expanded
 *	  into RowExpr constructs that name the individual output Vars.  This
 *	  is necessary since we will not scan the JOIN as a base relation, which
 *	  is the only way that the executor can directly handle whole-row Vars.
 *
 * This also adjusts relid sets found in some expression node types to
 * substitute the contained base rels for any join relid.
 *
 * If a JOIN contains sub-selects that have been flattened, its join alias
 * entries might now be arbitrary expressions, not just Vars.  This affects
 * this function in one important way: we might find ourselves inserting
 * SubLink expressions into subqueries, and we must make sure that their
 * Query.hasSubLinks fields get set to true if so.  If there are any
 * SubLinks in the join alias lists, the outer Query should already have
 * hasSubLinks = true, so this is only relevant to un-flattened subqueries.
 *
 * NOTE: this is used on not-yet-planned expressions.  We do not expect it
 * to be applied directly to the whole Query, so if we see a Query to start
 * with, we do want to increment sublevels_up (this occurs for LATERAL
 * subqueries).
 */
Node *
flatten_join_alias_vars(Query *query, Node *node)
{










}

static Node *
flatten_join_alias_vars_mutator(Node *node, flatten_join_alias_vars_context *context)
{











































































































































}

/*
 * alias_relid_set: in a set of RT indexes, replace joins by their
 * underlying base relids
 */
static Relids
alias_relid_set(Query *query, Relids relids)
{


















}