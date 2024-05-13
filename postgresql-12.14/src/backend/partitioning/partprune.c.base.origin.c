/*-------------------------------------------------------------------------
 *
 * partprune.c
 *		Support for partition pruning during query planning and
 *execution
 *
 * This module implements partition pruning using the information contained in
 * a table's partition descriptor, query clauses, and run-time parameters.
 *
 * During planning, clauses that can be matched to the table's partition key
 * are turned into a set of "pruning steps", which are then executed to
 * identify a set of partitions (as indexes in the RelOptInfo->part_rels
 * array) that satisfy the constraints in the step.  Partitions not in the set
 * are said to have been pruned.
 *
 * A base pruning step may involve expressions whose values are only known
 * during execution, such as Params, in which case pruning cannot occur
 * entirely during planning.  In that case, such steps are included alongside
 * the plan, so that they can be used by the executor for further pruning.
 *
 * There are two kinds of pruning steps.  A "base" pruning step represents
 * tests on partition key column(s), typically comparisons to expressions.
 * A "combine" pruning step represents a Boolean connector (AND/OR), and
 * combines the outputs of some previous steps using the appropriate
 * combination method.
 *
 * See gen_partprune_steps_internal() for more details on step generation.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		  src/backend/partitioning/partprune.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/nbtree.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "parser/parsetree.h"
#include "partitioning/partbounds.h"
#include "partitioning/partprune.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"

/*
 * Information about a clause matched with a partition key.
 */
typedef struct PartClauseInfo
{
  int keyno;       /* Partition key number (0 to partnatts - 1) */
  Oid opno;        /* operator used to compare partkey to expr */
  bool op_is_ne;   /* is clause's original operator <> ? */
  Expr *expr;      /* expr the partition key is compared to */
  Oid cmpfn;       /* Oid of function to compare 'expr' to the
                    * partition key */
  int op_strategy; /* btree strategy identifying the operator */
} PartClauseInfo;

/*
 * PartClauseMatchStatus
 *		Describes the result of match_clause_to_partition_key()
 */
typedef enum PartClauseMatchStatus
{
  PARTCLAUSE_NOMATCH,
  PARTCLAUSE_MATCH_CLAUSE,
  PARTCLAUSE_MATCH_NULLNESS,
  PARTCLAUSE_MATCH_STEPS,
  PARTCLAUSE_MATCH_CONTRADICT,
  PARTCLAUSE_UNSUPPORTED
} PartClauseMatchStatus;

/*
 * PartClauseTarget
 *		Identifies which qual clauses we can use for generating pruning
 *steps
 */
typedef enum PartClauseTarget
{
  PARTTARGET_PLANNER, /* want to prune during planning */
  PARTTARGET_INITIAL, /* want to prune during executor startup */
  PARTTARGET_EXEC     /* want to prune during each plan node scan */
} PartClauseTarget;

/*
 * GeneratePruningStepsContext
 *		Information about the current state of generation of "pruning
 *steps" for a given set of clauses
 *
 * gen_partprune_steps() initializes and returns an instance of this struct.
 *
 * Note that has_mutable_op, has_mutable_arg, and has_exec_param are set if
 * we found any potentially-useful-for-pruning clause having those properties,
 * whether or not we actually used the clause in the steps list.  This
 * definition allows us to skip the PARTTARGET_EXEC pass in some cases.
 */
typedef struct GeneratePruningStepsContext
{
  /* Copies of input arguments for gen_partprune_steps: */
  RelOptInfo *rel;         /* the partitioned relation */
  PartClauseTarget target; /* use-case we're generating steps for */
  /* Result data: */
  List *steps;          /* list of PartitionPruneSteps */
  bool has_mutable_op;  /* clauses include any stable operators */
  bool has_mutable_arg; /* clauses include any mutable comparison
                         * values, *other than* exec params */
  bool has_exec_param;  /* clauses include any PARAM_EXEC params */
  bool contradictory;   /* clauses were proven self-contradictory */
  /* Working state: */
  int next_step_id;
} GeneratePruningStepsContext;

/* The result of performing one PartitionPruneStep */
typedef struct PruneStepResult
{
  /*
   * The offsets of bounds (in a table's boundinfo) whose partition is
   * selected by the pruning step.
   */
  Bitmapset *bound_offsets;

  bool scan_default; /* Scan the default partition? */
  bool scan_null;    /* Scan the partition for NULL values? */
} PruneStepResult;

static List *
make_partitionedrel_pruneinfo(PlannerInfo *root, RelOptInfo *parentrel, int *relid_subplan_map, List *partitioned_rels, List *prunequal, Bitmapset **matchedsubplans);
static void
gen_partprune_steps(RelOptInfo *rel, List *clauses, PartClauseTarget target, GeneratePruningStepsContext *context);
static List *
gen_partprune_steps_internal(GeneratePruningStepsContext *context, List *clauses);
static PartitionPruneStep *
gen_prune_step_op(GeneratePruningStepsContext *context, StrategyNumber opstrategy, bool op_is_ne, List *exprs, List *cmpfns, Bitmapset *nullkeys);
static PartitionPruneStep *
gen_prune_step_combine(GeneratePruningStepsContext *context, List *source_stepids, PartitionPruneCombineOp combineOp);
static PartitionPruneStep *
gen_prune_steps_from_opexps(GeneratePruningStepsContext *context, List **keyclauses, Bitmapset *nullkeys);
static PartClauseMatchStatus
match_clause_to_partition_key(GeneratePruningStepsContext *context, Expr *clause, Expr *partkey, int partkeyidx, bool *clause_is_not_null, PartClauseInfo **pc, List **clause_steps);
static List *
get_steps_using_prefix(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, List *prefix);
static List *
get_steps_using_prefix_recurse(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, ListCell *start, List *step_exprs, List *step_cmpfns);
static PruneStepResult *
get_matching_hash_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys);
static PruneStepResult *
get_matching_list_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum value, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys);
static PruneStepResult *
get_matching_range_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys);
static Bitmapset *
pull_exec_paramids(Expr *expr);
static bool
pull_exec_paramids_walker(Node *node, Bitmapset **context);
static Bitmapset *
get_partkey_exec_paramids(List *steps);
static PruneStepResult *
perform_pruning_base_step(PartitionPruneContext *context, PartitionPruneStepOp *opstep);
static PruneStepResult *
perform_pruning_combine_step(PartitionPruneContext *context, PartitionPruneStepCombine *cstep, PruneStepResult **step_results);
static PartClauseMatchStatus
match_boolean_partition_clause(Oid partopfamily, Expr *clause, Expr *partkey, Expr **outconst);
static void
partkey_datum_from_expr(PartitionPruneContext *context, Expr *expr, int stateidx, Datum *value, bool *isnull);

/*
 * make_partition_pruneinfo
 *		Builds a PartitionPruneInfo which can be used in the executor to
 *allow additional partition pruning to take place.  Returns NULL when partition
 *pruning would be useless.
 *
 * 'parentrel' is the RelOptInfo for an appendrel, and 'subpaths' is the list
 * of scan paths for its child rels.
 *
 * 'partitioned_rels' is a List containing Lists of relids of partitioned
 * tables (a/k/a non-leaf partitions) that are parents of some of the child
 * rels.  Here we attempt to populate the PartitionPruneInfo by adding a
 * 'prune_infos' item for each sublist in the 'partitioned_rels' list.
 * However, some of the sets of partitioned relations may not require any
 * run-time pruning.  In these cases we'll simply not include a 'prune_infos'
 * item for that set and instead we'll add all the subplans which belong to
 * that set into the PartitionPruneInfo's 'other_subplans' field.  Callers
 * will likely never want to prune subplans which are mentioned in this field.
 *
 * 'prunequal' is a list of potential pruning quals.
 */
PartitionPruneInfo *
make_partition_pruneinfo(PlannerInfo *root, RelOptInfo *parentrel, List *subpaths, List *partitioned_rels, List *prunequal)
{
























































































}

/*
 * make_partitionedrel_pruneinfo
 *		Build a List of PartitionedRelPruneInfos, one for each
 *partitioned rel.  These can be used in the executor to allow additional
 *partition pruning to take place.
 *
 * Here we generate partition pruning steps for 'prunequal' and also build a
 * data structure which allows mapping of partition indexes into 'subpaths'
 * indexes.
 *
 * If no non-Const expressions are being compared to the partition key in any
 * of the 'partitioned_rels', then we return NIL to indicate no run-time
 * pruning should be performed.  Run-time pruning would be useless since the
 * pruning done during planning will have pruned everything that can be.
 *
 * On non-NIL return, 'matchedsubplans' is set to the subplan indexes which
 * were matched to this partition hierarchy.
 */
static List *
make_partitionedrel_pruneinfo(PlannerInfo *root, RelOptInfo *parentrel, int *relid_subplan_map, List *partitioned_rels, List *prunequal, Bitmapset **matchedsubplans)
{



























































































































































































































































}

/*
 * gen_partprune_steps
 *		Process 'clauses' (typically a rel's baserestrictinfo list of
 *clauses) and create a list of "partition pruning steps".
 *
 * 'target' tells whether to generate pruning steps for planning (use
 * immutable clauses only), or for executor startup (use any allowable
 * clause except ones containing PARAM_EXEC Params), or for executor
 * per-scan pruning (use any allowable clause).
 *
 * 'context' is an output argument that receives the steps list as well as
 * some subsidiary flags; see the GeneratePruningStepsContext typedef.
 */
static void
gen_partprune_steps(RelOptInfo *rel, List *clauses, PartClauseTarget target, GeneratePruningStepsContext *context)
{




































}

/*
 * prune_append_rel_partitions
 *		Process rel's baserestrictinfo and make use of quals which can
 *be evaluated during query planning in order to determine the minimum set of
 *partitions which must be scanned to satisfy these quals.  Returns the matching
 *partitions in the form of a Bitmapset containing the partitions' indexes in
 *the rel's part_rels array.
 *
 * Callers must ensure that 'rel' is a partitioned table.
 */
Bitmapset *
prune_append_rel_partitions(RelOptInfo *rel)
{
























































}

/*
 * get_matching_partitions
 *		Determine partitions that survive partition pruning
 *
 * Note: context->planstate must be set to a valid PlanState when the
 * pruning_steps were generated with a target other than PARTTARGET_PLANNER.
 *
 * Returns a Bitmapset of the RelOptInfo->part_rels indexes of the surviving
 * partitions.
 */
Bitmapset *
get_matching_partitions(PartitionPruneContext *context, List *pruning_steps)
{




























































































}

/*
 * gen_partprune_steps_internal
 *		Processes 'clauses' to generate partition pruning steps.
 *
 * From OpExpr clauses that are mutually AND'd, we find combinations of those
 * that match to the partition key columns and for every such combination,
 * we emit a PartitionPruneStepOp containing a vector of expressions whose
 * values are used as a look up key to search partitions by comparing the
 * values with partition bounds.  Relevant details of the operator and a
 * vector of (possibly cross-type) comparison functions is also included with
 * each step.
 *
 * For BoolExpr clauses, we recursively generate steps for each argument, and
 * return a PartitionPruneStepCombine of their results.
 *
 * The return value is a list of the steps generated, which are also added to
 * the context's steps list.  Each step is assigned a step identifier, unique
 * even across recursive calls.
 *
 * If we find clauses that are mutually contradictory, or a pseudoconstant
 * clause that contains false, we set context->contradictory to true and
 * return NIL (that is, no pruning steps).  Caller should consider all
 * partitions as pruned in that case.
 */
static List *
gen_partprune_steps_internal(GeneratePruningStepsContext *context, List *clauses)
{















































































































































































































































































































































}

/*
 * gen_prune_step_op
 *		Generate a pruning step for a specific operator
 *
 * The step is assigned a unique step identifier and added to context's 'steps'
 * list.
 */
static PartitionPruneStep *
gen_prune_step_op(GeneratePruningStepsContext *context, StrategyNumber opstrategy, bool op_is_ne, List *exprs, List *cmpfns, Bitmapset *nullkeys)
{


















}

/*
 * gen_prune_step_combine
 *		Generate a pruning step for a combination of several other steps
 *
 * The step is assigned a unique step identifier and added to context's
 * 'steps' list.
 */
static PartitionPruneStep *
gen_prune_step_combine(GeneratePruningStepsContext *context, List *source_stepids, PartitionPruneCombineOp combineOp)
{









}

/*
 * gen_prune_steps_from_opexps
 *		Generate pruning steps based on clauses for partition keys
 *
 * 'keyclauses' contains one list of clauses per partition key.  We check here
 * if we have found clauses for a valid subset of the partition key. In some
 * cases, (depending on the type of partitioning being used) if we didn't
 * find clauses for a given key, we discard clauses that may have been
 * found for any subsequent keys; see specific notes below.
 */
static PartitionPruneStep *
gen_prune_steps_from_opexps(GeneratePruningStepsContext *context, List **keyclauses, Bitmapset *nullkeys)
{












































































































































































































































































































































































}

/*
 * If the partition key has a collation, then the clause must have the same
 * input collation.  If the partition key is non-collatable, we assume the
 * collation doesn't matter, because while collation wasn't considered when
 * performing partitioning, the clause still may have a collation assigned
 * due to the other input being of a collatable type.
 *
 * See also IndexCollMatchesExprColl.
 */
#define PartCollMatchesExprColl(partcoll, exprcoll) ((partcoll) == InvalidOid || (partcoll) == (exprcoll))

/*
 * match_clause_to_partition_key
 *		Attempt to match the given 'clause' with the specified partition
 *key.
 *
 * Return value is:
 * * PARTCLAUSE_NOMATCH if the clause doesn't match this partition key (but
 *   caller should keep trying, because it might match a subsequent key).
 *   Output arguments: none set.
 *
 * * PARTCLAUSE_MATCH_CLAUSE if there is a match.
 *   Output arguments: *pc is set to a PartClauseInfo constructed for the
 *   matched clause.
 *
 * * PARTCLAUSE_MATCH_NULLNESS if there is a match, and the matched clause was
 *   either a "a IS NULL" or "a IS NOT NULL" clause.
 *   Output arguments: *clause_is_not_null is set to false in the former case
 *   true otherwise.
 *
 * * PARTCLAUSE_MATCH_STEPS if there is a match.
 *   Output arguments: *clause_steps is set to a list of PartitionPruneStep
 *   generated for the clause.
 *
 * * PARTCLAUSE_MATCH_CONTRADICT if the clause is self-contradictory, ie
 *   it provably returns FALSE or NULL.
 *   Output arguments: none set.
 *
 * * PARTCLAUSE_UNSUPPORTED if the clause doesn't match this partition key
 *   and couldn't possibly match any other one either, due to its form or
 *   properties (such as containing a volatile function).
 *   Output arguments: none set.
 */
static PartClauseMatchStatus
match_clause_to_partition_key(GeneratePruningStepsContext *context, Expr *clause, Expr *partkey, int partkeyidx, bool *clause_is_not_null, PartClauseInfo **pc, List **clause_steps)
{









































































































































































































































































































































































































































































































































































































}

/*
 * get_steps_using_prefix
 *		Generate list of PartitionPruneStepOp steps each consisting of
 *given opstrategy
 *
 * To generate steps, step_lastexpr and step_lastcmpfn are appended to
 * expressions and cmpfns, respectively, extracted from the clauses in
 * 'prefix'.  Actually, since 'prefix' may contain multiple clauses for the
 * same partition key column, we must generate steps for various combinations
 * of the clauses of different keys.
 *
 * For list/range partitioning, callers must ensure that step_nullkeys is
 * NULL, and that prefix contains at least one clause for each of the
 * partition keys earlier than one specified in step_lastkeyno if it's
 * greater than zero.  For hash partitioning, step_nullkeys is allowed to be
 * non-NULL, but they must ensure that prefix contains at least one clause
 * for each of the partition keys other than those specified in step_nullkeys
 * and step_lastkeyno.
 *
 * For both cases, callers must also ensure that clauses in prefix are sorted
 * in ascending order of their partition key numbers.
 */
static List *
get_steps_using_prefix(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, List *prefix)
{













}

/*
 * get_steps_using_prefix_recurse
 *		Recursively generate combinations of clauses for different
 *partition keys and start generating steps upon reaching clauses for the
 *greatest column that is less than the one for which we're currently generating
 *		steps (that is, step_lastkeyno)
 *
 * 'start' is where we should start iterating for the current invocation.
 * 'step_exprs' and 'step_cmpfns' each contains the expressions and cmpfns
 * we've generated so far from the clauses for the previous part keys.
 */
static List *
get_steps_using_prefix_recurse(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, ListCell *start, List *step_exprs, List *step_cmpfns)
{







































































































}

/*
 * get_matching_hash_bounds
 *		Determine offset of the hash bound matching the specified
 *values, considering that all the non-null values come from clauses containing
 *		a compatible hash equality operator and any keys that are null
 *come from an IS NULL clause.
 *
 * Generally this function will return a single matching bound offset,
 * although if a partition has not been setup for a given modulus then we may
 * return no matches.  If the number of clauses found don't cover the entire
 * partition key, then we'll need to return all offsets.
 *
 * 'opstrategy' if non-zero must be HTEqualStrategyNumber.
 *
 * 'values' contains Datums indexed by the partition key to use for pruning.
 *
 * 'nvalues', the number of Datums in the 'values' array.
 *
 * 'partsupfunc' contains partition hashing functions that can produce correct
 * hash for the type of the values contained in 'values'.
 *
 * 'nullkeys' is the set of partition keys that are null.
 */
static PruneStepResult *
get_matching_hash_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys)
{



















































}

/*
 * get_matching_list_bounds
 *		Determine the offsets of list bounds matching the specified
 *value, according to the semantics of the given operator strategy
 *
 * scan_default will be set in the returned struct, if the default partition
 * needs to be scanned, provided one exists at all.  scan_null will be set if
 * the special null-accepting partition needs to be scanned.
 *
 * 'opstrategy' if non-zero must be a btree strategy number.
 *
 * 'value' contains the value to use for pruning.
 *
 * 'nvalues', if non-zero, should be exactly 1, because of list partitioning.
 *
 * 'partsupfunc' contains the list partitioning comparison function to be used
 * to perform partition_list_bsearch
 *
 * 'nullkeys' is the set of partition keys that are null.
 */
static PruneStepResult *
get_matching_list_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum value, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys)
{
















































































































































































}

/*
 * get_matching_range_bounds
 *		Determine the offsets of range bounds matching the specified
 *values, according to the semantics of the given operator strategy
 *
 * Each datum whose offset is in result is to be treated as the upper bound of
 * the partition that will contain the desired values.
 *
 * scan_default is set in the returned struct if a default partition exists
 * and we're absolutely certain that it needs to be scanned.  We do *not* set
 * it just because values match portions of the key space uncovered by
 * partitions other than default (space which we normally assume to belong to
 * the default partition): the final set of bounds obtained after combining
 * multiple pruning steps might exclude it, so we infer its inclusion
 * elsewhere.
 *
 * 'opstrategy' if non-zero must be a btree strategy number.
 *
 * 'values' contains Datums indexed by the partition key to use for pruning.
 *
 * 'nvalues', number of Datums in 'values' array. Must be <= context->partnatts.
 *
 * 'partsupfunc' contains the range partitioning comparison functions to be
 * used to perform partition_range_datum_bsearch or partition_rbound_datum_cmp
 * using.
 *
 * 'nullkeys' is the set of partition keys that are null.
 */
static PruneStepResult *
get_matching_range_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys)
{





























































































































































































































































































































































}

/*
 * pull_exec_paramids
 *		Returns a Bitmapset containing the paramids of all Params with
 *		paramkind = PARAM_EXEC in 'expr'.
 */
static Bitmapset *
pull_exec_paramids(Expr *expr)
{





}

static bool
pull_exec_paramids_walker(Node *node, Bitmapset **context)
{















}

/*
 * get_partkey_exec_paramids
 *		Loop through given pruning steps and find out which exec Params
 *		are used.
 *
 * Returns a Bitmapset of Param IDs.
 */
static Bitmapset *
get_partkey_exec_paramids(List *steps)
{


























}

/*
 * perform_pruning_base_step
 *		Determines the indexes of datums that satisfy conditions
 *specified in 'opstep'.
 *
 * Result also contains whether special null-accepting and/or default
 * partition need to be scanned.
 */
static PruneStepResult *
perform_pruning_base_step(PartitionPruneContext *context, PartitionPruneStepOp *opstep)
{

























































































































}

/*
 * perform_pruning_combine_step
 *		Determines the indexes of datums obtained by combining those
 *given by the steps identified by cstep->source_stepids using the specified
 *		combination method
 *
 * Since cstep may refer to the result of earlier steps, we also receive
 * step_results here.
 */
static PruneStepResult *
perform_pruning_combine_step(PartitionPruneContext *context, PartitionPruneStepCombine *cstep, PruneStepResult **step_results)
{
































































































}

/*
 * match_boolean_partition_clause
 *
 * If we're able to match the clause to the partition key as specially-shaped
 * boolean clause, set *outconst to a Const containing a true or false value
 * and return PARTCLAUSE_MATCH_CLAUSE.  Returns PARTCLAUSE_UNSUPPORTED if the
 * clause is not a boolean clause or if the boolean clause is unsuitable for
 * partition pruning.  Returns PARTCLAUSE_NOMATCH if it's a bool quals but
 * just does not match this partition key.  *outconst is set to NULL in the
 * latter two cases.
 */
static PartClauseMatchStatus
match_boolean_partition_clause(Oid partopfamily, Expr *clause, Expr *partkey, Expr **outconst)
{































































}

/*
 * partkey_datum_from_expr
 *		Evaluate expression for potential partition pruning
 *
 * Evaluate 'expr'; set *value and *isnull to the resulting Datum and nullflag.
 *
 * If expr isn't a Const, its ExprState is in stateidx of the context
 * exprstate array.
 *
 * Note that the evaluated result may be in the per-tuple memory context of
 * context->planstate->ps_ExprContext, and we may have leaked other memory
 * there too.  This memory must be recovered by resetting that ExprContext
 * after we're done with the pruning operation (see execPartition.c).
 */
static void
partkey_datum_from_expr(PartitionPruneContext *context, Expr *expr, int stateidx, Datum *value, bool *isnull)
{























}