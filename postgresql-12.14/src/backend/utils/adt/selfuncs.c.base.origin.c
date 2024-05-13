/*-------------------------------------------------------------------------
 *
 * selfuncs.c
 *	  Selectivity functions and index cost estimation functions for
 *	  standard operators and index access methods.
 *
 *	  Selectivity routines are registered in the pg_operator catalog
 *	  in the "oprrest" and "oprjoin" attributes.
 *
 *	  Index cost functions are located via the index AM's API struct,
 *	  which is obtained from the handler function registered in pg_am.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/selfuncs.c
 *
 *-------------------------------------------------------------------------
 */

/*----------
 * Operator selectivity estimation functions are called to estimate the
 * selectivity of WHERE clauses whose top-level operator is their operator.
 * We divide the problem into two cases:
 *		Restriction clause estimation: the clause involves vars of just
 *			one relation.
 *		Join clause estimation: the clause involves vars of multiple
 *rels. Join selectivity estimation is far more difficult and usually less
 *accurate than restriction estimation.
 *
 * When dealing with the inner scan of a nestloop join, we consider the
 * join's joinclauses as restriction clauses for the inner relation, and
 * treat vars of the outer relation as parameters (a/k/a constants of unknown
 * values).  So, restriction estimators need to be able to accept an argument
 * telling which relation is to be treated as the variable.
 *
 * The call convention for a restriction estimator (oprrest function) is
 *
 *		Selectivity oprrest (PlannerInfo *root,
 *							 Oid operator,
 *							 List *args,
 *							 int varRelid);
 *
 * root: general information about the query (rtable and RelOptInfo lists
 * are particularly important for the estimator).
 * operator: OID of the specific operator in question.
 * args: argument list from the operator clause.
 * varRelid: if not zero, the relid (rtable index) of the relation to
 * be treated as the variable relation.  May be zero if the args list
 * is known to contain vars of only one relation.
 *
 * This is represented at the SQL level (in pg_proc) as
 *
 *		float8 oprrest (internal, oid, internal, int4);
 *
 * The result is a selectivity, that is, a fraction (0 to 1) of the rows
 * of the relation that are expected to produce a TRUE result for the
 * given operator.
 *
 * The call convention for a join estimator (oprjoin function) is similar
 * except that varRelid is not needed, and instead join information is
 * supplied:
 *
 *		Selectivity oprjoin (PlannerInfo *root,
 *							 Oid operator,
 *							 List *args,
 *							 JoinType jointype,
 *							 SpecialJoinInfo
 **sjinfo);
 *
 *		float8 oprjoin (internal, oid, internal, int2, internal);
 *
 * (Before Postgres 8.4, join estimators had only the first four of these
 * parameters.  That signature is still allowed, but deprecated.)  The
 * relationship between jointype and sjinfo is explained in the comments for
 * clause_selectivity() --- the short version is that jointype is usually
 * best ignored in favor of examining sjinfo.
 *
 * Join selectivity for regular inner and outer joins is defined as the
 * fraction (0 to 1) of the cross product of the relations that is expected
 * to produce a TRUE result for the given operator.  For both semi and anti
 * joins, however, the selectivity is defined as the fraction of the left-hand
 * side relation's rows that are expected to have a match (ie, at least one
 * row with a TRUE result) in the right-hand side.
 *
 * For both oprrest and oprjoin functions, the operator's input collation OID
 * (if any) is passed using the standard fmgr mechanism, so that the estimator
 * function can fetch it with PG_GET_COLLATION().  Note, however, that all
 * statistics in pg_statistic are currently built using the relevant column's
 * collation.
 *----------
 */

#include "postgres.h"

#include <ctype.h>
#include <math.h>

#include "access/brin.h"
#include "access/brin_page.h"
#include "access/gin.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/visibilitymap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_statistic_ext.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "statistics/statistics.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/index_selfuncs.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/snapmgr.h"
#include "utils/spccache.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define NumRelids(a, b) NumRelids_new(a, b)

/* Hooks for plugins to get control when we ask for stats */
get_relation_stats_hook_type get_relation_stats_hook = NULL;
get_index_stats_hook_type get_index_stats_hook = NULL;

static double
eqsel_internal(PG_FUNCTION_ARGS, bool negate);
static double
eqjoinsel_inner(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2);
static double
eqjoinsel_semi(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2, RelOptInfo *inner_rel);
static bool
estimate_multivariate_ndistinct(PlannerInfo *root, RelOptInfo *rel, List **varinfos, double *ndistinct);
static bool
convert_to_scalar(Datum value, Oid valuetypid, Oid collid, double *scaledvalue, Datum lobound, Datum hibound, Oid boundstypid, double *scaledlobound, double *scaledhibound);
static double
convert_numeric_to_scalar(Datum value, Oid typid, bool *failure);
static void
convert_string_to_scalar(char *value, double *scaledvalue, char *lobound, double *scaledlobound, char *hibound, double *scaledhibound);
static void
convert_bytea_to_scalar(Datum value, double *scaledvalue, Datum lobound, double *scaledlobound, Datum hibound, double *scaledhibound);
static double
convert_one_string_to_scalar(char *value, int rangelo, int rangehi);
static double
convert_one_bytea_to_scalar(unsigned char *value, int valuelen, int rangelo, int rangehi);
static char *
convert_string_datum(Datum value, Oid typid, Oid collid, bool *failure);
static double
convert_timevalue_to_scalar(Datum value, Oid typid, bool *failure);
static void
examine_simple_variable(PlannerInfo *root, Var *var, VariableStatData *vardata);
static bool
get_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max);
static bool
get_actual_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max);
static bool
get_actual_variable_endpoint(Relation heapRel, Relation indexRel, ScanDirection indexscandir, ScanKey scankeys, int16 typLen, bool typByVal, TupleTableSlot *tableslot, MemoryContext outercontext, Datum *endpointDatum);
static RelOptInfo *
find_join_input_rel(PlannerInfo *root, Relids relids);

/*
 *		eqsel			- Selectivity of "=" for any data types.
 *
 * Note: this routine is also used to estimate selectivity for some
 * operators that are not "=" but have comparable selectivity behavior,
 * such as "~=" (geometric approximate-match).  Even for "=", we must
 * keep in mind that the left and right datatypes may differ.
 */
Datum
eqsel(PG_FUNCTION_ARGS)
{

}

/*
 * Common code for eqsel() and neqsel()
 */
static double
eqsel_internal(PG_FUNCTION_ARGS, bool negate)
{


















































}

/*
 * var_eq_const --- eqsel for var = const case
 *
 * This is exported so that some other estimation functions can use it.
 */
double
var_eq_const(VariableStatData *vardata, Oid operator, Datum constval, bool constisnull, bool varonleft, bool negate)
{

}

double
var_eq_const_ext(VariableStatData *vardata, Oid operator, Oid collation, Datum constval, bool constisnull, bool varonleft, bool negate)
{



















































































































































}

/*
 * var_eq_non_const --- eqsel for var = something-other-than-const case
 *
 * This is exported so that some other estimation functions can use it.
 */
double
var_eq_non_const(VariableStatData *vardata, Oid operator, Node * other, bool varonleft, bool negate)
{

















































































}

/*
 *		neqsel			- Selectivity of "!=" for any data
 *types.
 *
 * This routine is also used for some operators that are not "!="
 * but have comparable selectivity behavior.  See above comments
 * for eqsel().
 */
Datum
neqsel(PG_FUNCTION_ARGS)
{

}

/*
 *	scalarineqsel		- Selectivity of "<", "<=", ">", ">=" for
 *scalars.
 *
 * This is the guts of scalarltsel/scalarlesel/scalargtsel/scalargesel.
 * The isgt and iseq flags distinguish which of the four cases apply.
 *
 * The caller has commuted the clause, if necessary, so that we can treat
 * the variable as being on the left.  The caller must also make sure that
 * the other side of the clause is a non-null Const, and dissect that into
 * a value and datatype.  (This definition simplifies some callers that
 * want to estimate against a computed value instead of a Const node.)
 *
 * This routine works for any datatype (or pair of datatypes) known to
 * convert_to_scalar().  If it is applied to some other datatype,
 * it will return an approximate estimate based on assuming that the constant
 * value falls in the middle of the bin identified by binary search.
 */
static double
scalarineqsel(PlannerInfo *root, Oid operator, bool isgt, bool iseq, Oid collation, VariableStatData *vardata, Datum constval, Oid consttype)
{








































































































































}

/*
 *	mcv_selectivity			- Examine the MCV list for selectivity
 *estimates
 *
 * Determine the fraction of the variable's MCV population that satisfies
 * the predicate (VAR OP CONST), or (CONST OP VAR) if !varonleft.  Also
 * compute the fraction of the total column population represented by the MCV
 * list.  This code will work for any boolean-returning predicate operator.
 *
 * The function result is the MCV selectivity, and the fraction of the
 * total population is returned into *sumcommonp.  Zeroes are returned
 * if there is no MCV list.
 */
double
mcv_selectivity(VariableStatData *vardata, FmgrInfo *opproc, Datum constval, bool varonleft, double *sumcommonp)
{

}

double
mcv_selectivity_ext(VariableStatData *vardata, FmgrInfo *opproc, Oid collation, Datum constval, bool varonleft, double *sumcommonp)
{






















}

/*
 *	histogram_selectivity	- Examine the histogram for selectivity
 *estimates
 *
 * Determine the fraction of the variable's histogram entries that satisfy
 * the predicate (VAR OP CONST), or (CONST OP VAR) if !varonleft.
 *
 * This code will work for any boolean-returning predicate operator, whether
 * or not it has anything to do with the histogram sort operator.  We are
 * essentially using the histogram just as a representative sample.  However,
 * small histograms are unlikely to be all that representative, so the caller
 * should be prepared to fall back on some other estimation approach when the
 * histogram is missing or very small.  It may also be prudent to combine this
 * approach with another one when the histogram is small.
 *
 * If the actual histogram size is not at least min_hist_size, we won't bother
 * to do the calculation at all.  Also, if the n_skip parameter is > 0, we
 * ignore the first and last n_skip histogram elements, on the grounds that
 * they are outliers and hence not very representative.  Typical values for
 * these parameters are 10 and 1.
 *
 * The function result is the selectivity, or -1 if there is no histogram
 * or it's smaller than min_hist_size.
 *
 * The output parameter *hist_size receives the actual histogram size,
 * or zero if no histogram.  Callers may use this number to decide how
 * much faith to put in the function result.
 *
 * Note that the result disregards both the most-common-values (if any) and
 * null entries.  The caller is expected to combine this result with
 * statistics for those portions of the column population.  It may also be
 * prudent to clamp the result range, ie, disbelieve exact 0 or 1 outputs.
 */
double
histogram_selectivity(VariableStatData *vardata, FmgrInfo *opproc, Datum constval, bool varonleft, int min_hist_size, int n_skip, int *hist_size)
{

}

double
histogram_selectivity_ext(VariableStatData *vardata, FmgrInfo *opproc, Oid collation, Datum constval, bool varonleft, int min_hist_size, int n_skip, int *hist_size)
{





































}

/*
 *	ineq_histogram_selectivity	- Examine the histogram for
 *scalarineqsel
 *
 * Determine the fraction of the variable's histogram population that
 * satisfies the inequality condition, ie, VAR < (or <=, >, >=) CONST.
 * The isgt and iseq flags distinguish which of the four cases apply.
 *
 * Returns -1 if there is no histogram (valid results will always be >= 0).
 *
 * Note that the result disregards both the most-common-values (if any) and
 * null entries.  The caller is expected to combine this result with
 * statistics for those portions of the column population.
 *
 * This is exported so that some other estimation functions can use it.
 */
double
ineq_histogram_selectivity(PlannerInfo *root, VariableStatData *vardata, FmgrInfo *opproc, bool isgt, bool iseq, Datum constval, Oid consttype)
{

}

double
ineq_histogram_selectivity_ext(PlannerInfo *root, VariableStatData *vardata, FmgrInfo *opproc, bool isgt, bool iseq, Oid collation, Datum constval, Oid consttype)
{













































































































































































































































































































}

/*
 * Common wrapper function for the selectivity estimators that simply
 * invoke scalarineqsel().
 */
static Datum
scalarineqsel_wrapper(PG_FUNCTION_ARGS, bool isgt, bool iseq)
{































































}

/*
 *		scalarltsel		- Selectivity of "<" for scalars.
 */
Datum
scalarltsel(PG_FUNCTION_ARGS)
{

}

/*
 *		scalarlesel		- Selectivity of "<=" for scalars.
 */
Datum
scalarlesel(PG_FUNCTION_ARGS)
{

}

/*
 *		scalargtsel		- Selectivity of ">" for scalars.
 */
Datum
scalargtsel(PG_FUNCTION_ARGS)
{

}

/*
 *		scalargesel		- Selectivity of ">=" for scalars.
 */
Datum
scalargesel(PG_FUNCTION_ARGS)
{

}

/*
 *		boolvarsel		- Selectivity of Boolean variable.
 *
 * This can actually be called on any boolean-valued expression.  If it
 * involves only Vars of the specified relation, and if there are statistics
 * about the Var or expression (the latter is possible if it's indexed) then
 * we'll produce a real estimate; otherwise it's just a default.
 */
Selectivity
boolvarsel(PlannerInfo *root, Node *arg, int varRelid)
{



















}

/*
 *		booltestsel		- Selectivity of BooleanTest Node.
 */
Selectivity
booltestsel(PlannerInfo *root, BoolTestType booltesttype, Node *arg, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{















































































































































}

/*
 *		nulltestsel		- Selectivity of NullTest Node.
 */
Selectivity
nulltestsel(PlannerInfo *root, NullTestType nulltesttype, Node *arg, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{




































































}

/*
 * strip_array_coercion - strip binary-compatible relabeling from an array expr
 *
 * For array values, the parser normally generates ArrayCoerceExpr conversions,
 * but it seems possible that RelabelType might show up.  Also, the planner
 * is not currently tense about collapsing stacked ArrayCoerceExpr nodes,
 * so we need to be ready to deal with more than one level.
 */
static Node *
strip_array_coercion(Node *node)
{






























}

/*
 *		scalararraysel		- Selectivity of ScalarArrayOpExpr Node.
 */
Selectivity
scalararraysel(PlannerInfo *root, ScalarArrayOpExpr *clause, bool is_join_clause, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{














































































































































































































































































































}

/*
 * Estimate number of elements in the array yielded by an expression.
 *
 * It's important that this agree with scalararraysel.
 */
int
estimate_array_length(Node *arrayexpr)
{

























}

/*
 *		rowcomparesel		- Selectivity of RowCompareExpr Node.
 *
 * We estimate RowCompare selectivity by considering just the first (high
 * order) columns, which makes it equivalent to an ordinary OpExpr.  While
 * this estimate could be refined by considering additional columns, it
 * seems unlikely that we could do a lot better without multi-column
 * statistics.
 */
Selectivity
rowcomparesel(PlannerInfo *root, RowCompareExpr *clause, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{


















































}

/*
 *		eqjoinsel		- Join selectivity of "="
 */
Datum
eqjoinsel(PG_FUNCTION_ARGS)
{






















































































































}

/*
 * eqjoinsel_inner --- eqjoinsel for normal inner join
 *
 * We also use this for LEFT/FULL outer joins; it's not presently clear
 * that it's worth trying to distinguish them here.
 */
static double
eqjoinsel_inner(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2)
{










































































































































































}

/*
 * eqjoinsel_semi --- eqjoinsel for semi join
 *
 * (Also used for anti join, which we are supposed to estimate the same way.)
 * Caller has ensured that vardata1 is the LHS variable.
 * Unlike eqjoinsel_inner, we have to cope with opfuncoid being InvalidOid.
 */
static double
eqjoinsel_semi(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2, RelOptInfo *inner_rel)
{





































































































































































}

/*
 *		neqjoinsel		- Join selectivity of "!="
 */
Datum
neqjoinsel(PG_FUNCTION_ARGS)
{



































































}

/*
 *		scalarltjoinsel - Join selectivity of "<" for scalars
 */
Datum
scalarltjoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		scalarlejoinsel - Join selectivity of "<=" for scalars
 */
Datum
scalarlejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		scalargtjoinsel - Join selectivity of ">" for scalars
 */
Datum
scalargtjoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		scalargejoinsel - Join selectivity of ">=" for scalars
 */
Datum
scalargejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 * mergejoinscansel			- Scan selectivity of merge join.
 *
 * A merge join will stop as soon as it exhausts either input stream.
 * Therefore, if we can estimate the ranges of both input variables,
 * we can estimate how much of the input will actually be read.  This
 * can have a considerable impact on the cost when using indexscans.
 *
 * Also, we can estimate how much of each input has to be read before the
 * first join pair is found, which will affect the join's startup time.
 *
 * clause should be a clause already known to be mergejoinable.  opfamily,
 * strategy, and nulls_first specify the sort ordering being used.
 *
 * The outputs are:
 *		*leftstart is set to the fraction of the left-hand variable
 *expected to be scanned before the first join pair is found (0 to 1). *leftend
 *is set to the fraction of the left-hand variable expected to be scanned before
 *the join terminates (0 to 1). *rightstart, *rightend similarly for the
 *right-hand variable.
 */
void
mergejoinscansel(PlannerInfo *root, Node *clause, Oid opfamily, int strategy, bool nulls_first, Selectivity *leftstart, Selectivity *leftend, Selectivity *rightstart, Selectivity *rightend)
{



























































































































































































































































}

/*
 * Helper routine for estimate_num_groups: add an item to a list of
 * GroupVarInfos, but only if it's not known equal to any of the existing
 * entries.
 */
typedef struct
{
  Node *var;        /* might be an expression, not just a Var */
  RelOptInfo *rel;  /* relation it belongs to */
  double ndistinct; /* # distinct values */
} GroupVarInfo;

static List *
add_unique_group_var(PlannerInfo *root, List *varinfos, Node *var, VariableStatData *vardata)
{
















































}

/*
 * estimate_num_groups		- Estimate number of groups in a grouped query
 *
 * Given a query having a GROUP BY clause, estimate how many groups there
 * will be --- ie, the number of distinct combinations of the GROUP BY
 * expressions.
 *
 * This routine is also used to estimate the number of rows emitted by
 * a DISTINCT filtering step; that is an isomorphic problem.  (Note:
 * actually, we only use it for DISTINCT when there's no grouping or
 * aggregation ahead of the DISTINCT.)
 *
 * Inputs:
 *	root - the query
 *	groupExprs - list of expressions being grouped by
 *	input_rows - number of rows estimated to arrive at the group/unique
 *		filter step
 *	pgset - NULL, or a List** pointing to a grouping set to filter the
 *		groupExprs against
 *
 * Given the lack of any cross-correlation statistics in the system, it's
 * impossible to do anything really trustworthy with GROUP BY conditions
 * involving multiple Vars.  We should however avoid assuming the worst
 * case (all possible cross-product terms actually appear as groups) since
 * very often the grouped-by Vars are highly correlated.  Our current approach
 * is as follows:
 *	1.  Expressions yielding boolean are assumed to contribute two groups,
 *		independently of their content, and are ignored in the
 *subsequent steps.  This is mainly because tests like "col IS NULL" break the
 *		heuristic used in step 2 especially badly.
 *	2.  Reduce the given expressions to a list of unique Vars used.  For
 *		example, GROUP BY a, a + b is treated the same as GROUP BY a, b.
 *		It is clearly correct not to count the same Var more than once.
 *		It is also reasonable to treat f(x) the same as x: f() cannot
 *		increase the number of distinct values (unless it is volatile,
 *		which we consider unlikely for grouping), but it probably won't
 *		reduce the number of distinct values much either.
 *		As a special case, if a GROUP BY expression can be matched to an
 *		expressional index for which we have statistics, then we treat
 *the whole expression as though it were just a Var.
 *	3.  If the list contains Vars of different relations that are known
 *equal due to equivalence classes, then drop all but one of the Vars from each
 *		known-equal set, keeping the one with smallest estimated # of
 *values (since the extra values of the others can't appear in joined rows).
 *		Note the reason we only consider Vars of different relations is
 *that if we considered ones of the same rel, we'd be double-counting the
 *		restriction selectivity of the equality in the next step.
 *	4.  For Vars within a single source rel, we multiply together the
 *numbers of values, clamp to the number of rows in the rel (divided by 10 if
 *		more than one Var), and then multiply by a factor based on the
 *		selectivity of the restriction clauses for that rel.  When
 *there's more than one Var, the initial product is probably too high (it's the
 *		worst case) but clamping to a fraction of the rel's rows seems
 *to be a helpful heuristic for not letting the estimate get out of hand.  (The
 *		factor of 10 is derived from pre-Postgres-7.4 practice.)  The
 *factor we multiply by to adjust for the restriction selectivity assumes that
 *		the restriction clauses are independent of the grouping, which
 *may not be a valid assumption, but it's hard to do better.
 *	5.  If there are Vars from multiple rels, we repeat step 4 for each such
 *		rel, and multiply the results together.
 * Note that rels not containing grouped Vars are ignored completely, as are
 * join clauses.  Such rels cannot increase the number of groups, and we
 * assume such clauses do not reduce the number either (somewhat bogus,
 * but we don't have the info to do better).
 */
double
estimate_num_groups(PlannerInfo *root, List *groupExprs, double input_rows, List **pgset)
{











































































































































































































































































































































}

/*
 * Estimate hash bucket statistics when the specified expression is used
 * as a hash key for the given number of buckets.
 *
 * This attempts to determine two values:
 *
 * 1. The frequency of the most common value of the expression (returns
 * zero into *mcv_freq if we can't get that).
 *
 * 2. The "bucketsize fraction", ie, average number of entries in a bucket
 * divided by total tuples in relation.
 *
 * XXX This is really pretty bogus since we're effectively assuming that the
 * distribution of hash keys will be the same after applying restriction
 * clauses as it was in the underlying relation.  However, we are not nearly
 * smart enough to figure out how the restrict clauses might change the
 * distribution, so this will have to do for now.
 *
 * We are passed the number of buckets the executor will use for the given
 * input relation.  If the data were perfectly distributed, with the same
 * number of tuples going into each available bucket, then the bucketsize
 * fraction would be 1/nbuckets.  But this happy state of affairs will occur
 * only if (a) there are at least nbuckets distinct data values, and (b)
 * we have a not-too-skewed data distribution.  Otherwise the buckets will
 * be nonuniformly occupied.  If the other relation in the join has a key
 * distribution similar to this one's, then the most-loaded buckets are
 * exactly those that will be probed most often.  Therefore, the "average"
 * bucket size for costing purposes should really be taken as something close
 * to the "worst case" bucket size.  We try to estimate this by adjusting the
 * fraction if there are too few distinct data values, and then scaling up
 * by the ratio of the most common value's frequency to the average frequency.
 *
 * If no statistics are available, use a default estimate of 0.1.  This will
 * discourage use of a hash rather strongly if the inner relation is large,
 * which is what we want.  We do not want to hash unless we know that the
 * inner rel is well-dispersed (or the alternatives seem much worse).
 *
 * The caller should also check that the mcv_freq is not so large that the
 * most common value would by itself require an impractically large bucket.
 * In a hash join, the executor can split buckets if they get too big, but
 * obviously that doesn't help for a bucket that contains many duplicates of
 * the same value.
 */
void
estimate_hash_bucket_stats(PlannerInfo *root, Node *hashkey, double nbuckets, Selectivity *mcv_freq, Selectivity *bucketsize_frac)
{












































































































}

/*
 * estimate_hashagg_tablesize
 *	  estimate the number of bytes that a hash aggregate hashtable will
 *	  require based on the agg_costs, path width and number of groups.
 *
 * We return the result as "double" to forestall any possible overflow
 * problem in the multiplication by dNumGroups.
 *
 * XXX this may be over-estimating the size now that hashagg knows to omit
 * unneeded columns from the hashtable.  Also for mixed-mode grouping sets,
 * grouping columns not in the hashed set are counted here even though hashagg
 * won't store them.  Is this a problem?
 */
double
estimate_hashagg_tablesize(Path *path, const AggClauseCosts *agg_costs, double dNumGroups)
{

















}

/*-------------------------------------------------------------------------
 *
 * Support routines
 *
 *-------------------------------------------------------------------------
 */

/*
 * Find applicable ndistinct statistics for the given list of VarInfos (which
 * must all belong to the given rel), and update *ndistinct to the estimate of
 * the MVNDistinctItem that best matches.  If a match it found, *varinfos is
 * updated to remove the list of matched varinfos.
 *
 * Varinfos that aren't for simple Vars are ignored.
 *
 * Return true if we're able to find a match, false otherwise.
 */
static bool
estimate_multivariate_ndistinct(PlannerInfo *root, RelOptInfo *rel, List **varinfos, double *ndistinct)
{




















































































































































}

/*
 * convert_to_scalar
 *	  Convert non-NULL values of the indicated types to the comparison
 *	  scale needed by scalarineqsel().
 *	  Returns "true" if successful.
 *
 * XXX this routine is a hack: ideally we should look up the conversion
 * subroutines in pg_type.
 *
 * All numeric datatypes are simply converted to their equivalent
 * "double" values.  (NUMERIC values that are outside the range of "double"
 * are clamped to +/- HUGE_VAL.)
 *
 * String datatypes are converted by convert_string_to_scalar(),
 * which is explained below.  The reason why this routine deals with
 * three values at a time, not just one, is that we need it for strings.
 *
 * The bytea datatype is just enough different from strings that it has
 * to be treated separately.
 *
 * The several datatypes representing absolute times are all converted
 * to Timestamp, which is actually a double, and then we just use that
 * double value.  Note this will give correct results even for the "special"
 * values of Timestamp, since those are chosen to compare correctly;
 * see timestamp_cmp.
 *
 * The several datatypes representing relative times (intervals) are all
 * converted to measurements expressed in seconds.
 */
static bool
convert_to_scalar(Datum value, Oid valuetypid, Oid collid, double *scaledvalue, Datum lobound, Datum hibound, Oid boundstypid, double *scaledlobound, double *scaledhibound)
{

























































































































}

/*
 * Do convert_to_scalar()'s work for any numeric data type.
 *
 * On failure (e.g., unsupported typid), set *failure to true;
 * otherwise, that variable is not changed.
 */
static double
convert_numeric_to_scalar(Datum value, Oid typid, bool *failure)
{


































}

/*
 * Do convert_to_scalar()'s work for any character-string data type.
 *
 * String datatypes are converted to a scale that ranges from 0 to 1,
 * where we visualize the bytes of the string as fractional digits.
 *
 * We do not want the base to be 256, however, since that tends to
 * generate inflated selectivity estimates; few databases will have
 * occurrences of all 256 possible byte values at each position.
 * Instead, use the smallest and largest byte values seen in the bounds
 * as the estimated range for each byte, after some fudging to deal with
 * the fact that we probably aren't going to see the full range that way.
 *
 * An additional refinement is that we discard any common prefix of the
 * three strings before computing the scaled values.  This allows us to
 * "zoom in" when we encounter a narrow data range.  An example is a phone
 * number database where all the values begin with the same area code.
 * (Actually, the bounds will be adjacent histogram-bin-boundary values,
 * so this is more likely to happen than you might think.)
 */
static void
convert_string_to_scalar(char *value, double *scaledvalue, char *lobound, double *scaledlobound, char *hibound, double *scaledhibound)
{



























































































}

static double
convert_one_string_to_scalar(char *value, int rangelo, int rangehi)
{











































}

/*
 * Convert a string-type Datum into a palloc'd, null-terminated string.
 *
 * On failure (e.g., unsupported typid), set *failure to true;
 * otherwise, that variable is not changed.  (We'll return NULL on failure.)
 *
 * When using a non-C locale, we must pass the string through strxfrm()
 * before continuing, so as to generate correct locale-specific results.
 */
static char *
convert_string_datum(Datum value, Oid typid, Oid collid, bool *failure)
{
















































































}

/*
 * Do convert_to_scalar()'s work for any bytea data type.
 *
 * Very similar to convert_string_to_scalar except we can't assume
 * null-termination and therefore pass explicit lengths around.
 *
 * Also, assumptions about likely "normal" ranges of characters have been
 * removed - a data range of 0..255 is always used, for now.  (Perhaps
 * someday we will add information about actual byte data range to
 * pg_statistic.)
 */
static void
convert_bytea_to_scalar(Datum value, double *scaledvalue, Datum lobound, double *scaledlobound, Datum hibound, double *scaledhibound)
{


































}

static double
convert_one_bytea_to_scalar(unsigned char *value, int valuelen, int rangelo, int rangehi)
{





































}

/*
 * Do convert_to_scalar()'s work for any timevalue data type.
 *
 * On failure (e.g., unsupported typid), set *failure to true;
 * otherwise, that variable is not changed.
 */
static double
convert_timevalue_to_scalar(Datum value, Oid typid, bool *failure)
{
































}

/*
 * get_restriction_variable
 *		Examine the args of a restriction clause to see if it's of the
 *		form (variable op pseudoconstant) or (pseudoconstant op
 *variable), where "variable" could be either a Var or an expression in vars of
 *a single relation.  If so, extract information about the variable, and also
 *indicate which side it was on and the other argument.
 *
 * Inputs:
 *	root: the planner info
 *	args: clause argument list
 *	varRelid: see specs for restriction selectivity functions
 *
 * Outputs: (these are valid only if true is returned)
 *	*vardata: gets information about variable (see examine_variable)
 *	*other: gets other clause argument, aggressively reduced to a constant
 *	*varonleft: set true if variable is on the left, false if on the right
 *
 * Returns true if a variable is identified, otherwise false.
 *
 * Note: if there are Vars on both sides of the clause, we must fail, because
 * callers are expecting that the other side will act like a pseudoconstant.
 */
bool
get_restriction_variable(PlannerInfo *root, List *args, int varRelid, VariableStatData *vardata, Node **other, bool *varonleft)
{












































}

/*
 * get_join_variables
 *		Apply examine_variable() to each side of a join clause.
 *		Also, attempt to identify whether the join clause has the same
 *		or reversed sense compared to the SpecialJoinInfo.
 *
 * We consider the join clause "normal" if it is "lhs_var OP rhs_var",
 * or "reversed" if it is "rhs_var OP lhs_var".  In complicated cases
 * where we can't tell for sure, we default to assuming it's normal.
 */
void
get_join_variables(PlannerInfo *root, List *args, SpecialJoinInfo *sjinfo, VariableStatData *vardata1, VariableStatData *vardata2, bool *join_is_reversed)
{

























}

/*
 * examine_variable
 *		Try to look up statistical data about an expression.
 *		Fill in a VariableStatData struct to describe the expression.
 *
 * Inputs:
 *	root: the planner info
 *	node: the expression tree to examine
 *	varRelid: see specs for restriction selectivity functions
 *
 * Outputs: *vardata is filled as follows:
 *	var: the input expression (with any binary relabeling stripped, if
 *		it is or contains a variable; but otherwise the type is
 *preserved) rel: RelOptInfo for relation containing variable; NULL if
 *expression contains no Vars (NOTE this could point to a RelOptInfo of a
 *		subquery, not one in the current query).
 *	statsTuple: the pg_statistic entry for the variable, if one exists;
 *		otherwise NULL.
 *	freefunc: pointer to a function to release statsTuple with.
 *	vartype: exposed type of the expression; this should always match
 *		the declared input type of the operator we are estimating for.
 *	atttype, atttypmod: actual type/typmod of the "var" expression.  This is
 *		commonly the same as the exposed type of the variable argument,
 *		but can be different in binary-compatible-type cases.
 *	isunique: true if we were able to match the var to a unique index or a
 *		single-column DISTINCT clause, implying its values are unique
 *for this query.  (Caution: this should be trusted for statistical purposes
 *only, since we do not check indimmediate nor verify that the exact same
 *definition of equality applies.) acl_ok: true if current user has permission
 *to read the column(s) underlying the pg_statistic entry.  This is consulted by
 *		statistic_proc_security_check().
 *
 * Caller is responsible for doing ReleaseVariableStats() before exiting.
 */
void
examine_variable(PlannerInfo *root, Node *node, int varRelid, VariableStatData *vardata)
{






























































































































































































































































}

/*
 * examine_simple_variable
 *		Handle a simple Var for examine_variable
 *
 * This is split out as a subroutine so that we can recurse to deal with
 * Vars referencing subqueries.
 *
 * We already filled in all the fields of *vardata except for the stats tuple.
 */
static void
examine_simple_variable(PlannerInfo *root, Var *var, VariableStatData *vardata)
{


















































































































































































































































}

/*
 * Check whether it is permitted to call func_oid passing some of the
 * pg_statistic data in vardata.  We allow this either if the user has SELECT
 * privileges on the table or column underlying the pg_statistic data or if
 * the function is marked leak-proof.
 */
bool
statistic_proc_security_check(VariableStatData *vardata, Oid func_oid)
{

















}

/*
 * get_variable_numdistinct
 *	  Estimate the number of distinct values of a variable.
 *
 * vardata: results of examine_variable
 * *isdefault: set to true if the result is a default rather than based on
 * anything meaningful.
 *
 * NB: be careful to produce a positive integral result, since callers may
 * compare the result to exact integer counts, or might divide by it.
 */
double
get_variable_numdistinct(VariableStatData *vardata, bool *isdefault)
{
































































































































}

/*
 * get_variable_range
 *		Estimate the minimum and maximum value of the specified
 *variable. If successful, store values in *min and *max, and return true. If no
 *data available, return false.
 *
 * sortop is the "<" comparison operator to use.  This should generally
 * be "<" not ">", as only the former is likely to be found in pg_statistic.
 * The collation must be specified too.
 */
static bool
get_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max)
{














































































































































}

/*
 * get_actual_variable_range
 *		Attempt to identify the current *actual* minimum and/or maximum
 *		of the specified variable, by looking for a suitable btree index
 *		and fetching its low and/or high values.
 *		If successful, store values in *min and *max, and return true.
 *		(Either pointer can be NULL if that endpoint isn't needed.)
 *		If unsuccessful, return false.
 *
 * sortop is the "<" comparison operator to use.
 * collation is the required collation.
 */
static bool
get_actual_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max)
{

























































































































































}

/*
 * Get one endpoint datum (min or max depending on indexscandir) from the
 * specified index.  Return true if successful, false if not.
 * On success, endpoint value is stored to *endpointDatum (and copied into
 * outercontext).
 *
 * scankeys is a 1-element scankey array set up to reject nulls.
 * typLen/typByVal describe the datatype of the index's first column.
 * tableslot is a slot suitable to hold table tuples, in case we need
 * to probe the heap.
 * (We could compute these values locally, but that would mean computing them
 * twice when get_actual_variable_range needs both the min and the max.)
 *
 * Failure occurs either when the index is empty, or we decide that it's
 * taking too long to find a suitable tuple.
 */
static bool
get_actual_variable_endpoint(Relation heapRel, Relation indexRel, ScanDirection indexscandir, ScanKey scankeys, int16 typLen, bool typByVal, TupleTableSlot *tableslot, MemoryContext outercontext, Datum *endpointDatum)
{














































































































































}

/*
 * find_join_input_rel
 *		Look up the input relation for a join.
 *
 * We assume that the input relation's RelOptInfo must have been constructed
 * already.
 */
static RelOptInfo *
find_join_input_rel(PlannerInfo *root, Relids relids)
{





















}

/*-------------------------------------------------------------------------
 *
 * Index cost estimation functions
 *
 *-------------------------------------------------------------------------
 */

/*
 * Extract the actual indexquals (as RestrictInfos) from an IndexClause list
 */
List *
get_quals_from_indexclauses(List *indexclauses)
{
















}

/*
 * Compute the total evaluation cost of the comparison operands in a list
 * of index qual expressions.  Since we know these will be evaluated just
 * once per scan, there's no need to distinguish startup from per-row cost.
 *
 * This can be used either on the result of get_quals_from_indexclauses(),
 * or directly on an indexorderbys list.  In both cases, we expect that the
 * index key expression is on the left side of binary clauses.
 */
Cost
index_other_operands_eval_cost(PlannerInfo *root, List *indexquals)
{


















































}

void
genericcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, GenericCosts *costs)
{






























































































































































































}

/*
 * If the index is partial, add its predicate to the given qual list.
 *
 * ANDing the index predicate with the explicitly given indexquals produces
 * a more accurate idea of the index's selectivity.  However, we need to be
 * careful not to insert redundant clauses, because clauselist_selectivity()
 * is easily fooled into computing a too-low selectivity estimate.  Our
 * approach is to add only the predicate clause(s) that cannot be proven to
 * be implied by the given indexquals.  This successfully handles cases such
 * as a qual "x = 42" used with a partial index "WHERE x >= 40 AND x < 50".
 * There are many other cases where we won't detect redundancy, leading to a
 * too-low selectivity estimate, which will bias the system in favor of using
 * partial indexes where possible.  That is not necessarily bad though.
 *
 * Note that indexQuals contains RestrictInfo nodes while the indpred
 * does not, so the output list will be mixed.  This is OK for both
 * predicate_implied_by() and clauselist_selectivity(), but might be
 * problematic if the result were passed to other things.
 */
List *
add_predicate_to_index_quals(IndexOptInfo *index, List *indexQuals)
{




















}

void
btcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{







































































































































































































































































































}

void
hashcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{




































}

void
gistcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{





















































}

void
spgcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{





















































}

/*
 * Support routines for gincostestimate
 */

typedef struct
{
  bool haveFullScan;
  double partialEntries;
  double exactEntries;
  double searchEntries;
  double arrayScans;
} GinQualCounts;

/*
 * Estimate the number of index terms that need to be searched for while
 * testing the given GIN query, and increment the counts in *counts
 * appropriately.  If the query is unsatisfiable, return false.
 */
static bool
gincost_pattern(IndexOptInfo *index, int indexcol, Oid clause_op, Datum query, GinQualCounts *counts)
{





















































































}

/*
 * Estimate the number of index terms that need to be searched for while
 * testing the given GIN index clause, and increment the counts in *counts
 * appropriately.  If the query is unsatisfiable, return false.
 */
static bool
gincost_opexpr(PlannerInfo *root, IndexOptInfo *index, int indexcol, OpExpr *clause, GinQualCounts *counts)
{































}

/*
 * Estimate the number of index terms that need to be searched for while
 * testing the given GIN index clause, and increment the counts in *counts
 * appropriately.  If the query is unsatisfiable, return false.
 *
 * A ScalarArrayOpExpr will give rise to N separate indexscans at runtime,
 * each of which involves one value from the RHS array, plus all the
 * non-array quals (if any).  To model this, we average the counts across
 * the RHS elements, and add the averages to the counts in *counts (which
 * correspond to per-indexscan costs).  We also multiply counts->arrayScans
 * by N, causing gincostestimate to scale up its estimates accordingly.
 */
static bool
gincost_scalararrayopexpr(PlannerInfo *root, IndexOptInfo *index, int indexcol, ScalarArrayOpExpr *clause, double numIndexEntries, GinQualCounts *counts)
{







































































































}

/*
 * GIN has search behavior completely different from other index types
 */
void
gincostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{























































































































































































































































































}

/*
 * BRIN has search behavior completely different from other index types
 */
void
brincostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{








































































































































































































}