/*-------------------------------------------------------------------------
 *
 * plancat.c
 *	   routines for accessing the system catalogs
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/plancat.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/tableam.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic_ext.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/plancat.h"
#include "optimizer/prep.h"
#include "partitioning/partdesc.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "statistics/statistics.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"

/* GUC parameter */
int constraint_exclusion = CONSTRAINT_EXCLUSION_PARTITION;

/* Hook for plugins to get control in get_relation_info() */
get_relation_info_hook_type get_relation_info_hook = NULL;

static void
get_relation_foreign_keys(PlannerInfo *root, RelOptInfo *rel, Relation relation, bool inhparent);
static bool
infer_collation_opclass_match(InferenceElem *elem, Relation idxRel, List *idxExprs);
static List *
get_relation_constraints(PlannerInfo *root, Oid relationObjectId, RelOptInfo *rel, bool include_noinherit, bool include_notnull, bool include_partition);
static List *
build_index_tlist(PlannerInfo *root, IndexOptInfo *index, Relation heapRelation);
static List *
get_relation_statistics(RelOptInfo *rel, Relation relation);
static void
set_relation_partition_info(PlannerInfo *root, RelOptInfo *rel, Relation relation);
static PartitionScheme
find_partition_scheme(PlannerInfo *root, Relation rel);
static void
set_baserel_partition_key_exprs(Relation relation, RelOptInfo *rel);

/*
 * get_relation_info -
 *	  Retrieves catalog information for a given relation.
 *
 * Given the Oid of the relation, return the following info into fields
 * of the RelOptInfo struct:
 *
 *	min_attr	lowest valid AttrNumber
 *	max_attr	highest valid AttrNumber
 *	indexlist	list of IndexOptInfos for relation's indexes
 *	statlist	list of StatisticExtInfo for relation's statistic
 *objects serverid	if it's a foreign table, the server OID fdwroutine
 *if it's a foreign table, the FDW function pointers pages		number
 *of pages tuples		number of tuples rel_parallel_workers
 *user-defined number of parallel workers
 *
 * Also, add information about the relation's foreign keys to root->fkey_list.
 *
 * Also, initialize the attr_needed[] and attr_widths[] arrays.  In most
 * cases these are left as zeroes, but sometimes we need to compute attr
 * widths here, and we may as well cache the results for costsize.c.
 *
 * If inhparent is true, all we need to do is set up the attr arrays:
 * the RelOptInfo actually represents the appendrel formed by an inheritance
 * tree, and so the parent rel's physical size and index information isn't
 * important for it.
 */
void
get_relation_info(PlannerInfo *root, Oid relationObjectId, bool inhparent, RelOptInfo *rel)
{









































































































































































































































































































































































}

/*
 * get_relation_foreign_keys -
 *	  Retrieves foreign key information for a given relation.
 *
 * ForeignKeyOptInfos for relevant foreign keys are created and added to
 * root->fkey_list.  We do this now while we have the relcache entry open.
 * We could sometimes avoid making useless ForeignKeyOptInfos if we waited
 * until all RelOptInfos have been built, but the cost of re-opening the
 * relcache entries would probably exceed any savings.
 */
static void
get_relation_foreign_keys(PlannerInfo *root, RelOptInfo *rel, Relation relation, bool inhparent)
{































































































}

/*
 * infer_arbiter_indexes -
 *	  Determine the unique indexes used to arbitrate speculative insertion.
 *
 * Uses user-supplied inference clause expressions and predicate to match a
 * unique index from those defined and ready on the heap relation (target).
 * An exact match is required on columns/expressions (although they can appear
 * in any order).  However, the predicate given by the user need only restrict
 * insertion to a subset of some part of the table covered by some particular
 * unique index (in particular, a partial unique index) in order to be
 * inferred.
 *
 * The implementation does not consider which B-Tree operator class any
 * particular available unique index attribute uses, unless one was specified
 * in the inference specification. The same is true of collations.  In
 * particular, there is no system dependency on the default operator class for
 * the purposes of inference.  If no opclass (or collation) is specified, then
 * all matching indexes (that may or may not match the default in terms of
 * each attribute opclass/collation) are used for inference.
 */
List *
infer_arbiter_indexes(PlannerInfo *root)
{

























































































































































































































































}

/*
 * infer_collation_opclass_match - ensure infer element opclass/collation match
 *
 * Given unique index inference element from inference specification, if
 * collation was specified, or if opclass was specified, verify that there is
 * at least one matching indexed attribute (occasionally, there may be more).
 * Skip this in the common case where inference specification does not include
 * collation or opclass (instead matching everything, regardless of cataloged
 * collation/opclass of indexed attribute).
 *
 * At least historically, Postgres has not offered collations or opclasses
 * with alternative-to-default notions of equality, so these additional
 * criteria should only be required infrequently.
 *
 * Don't give up immediately when an inference element matches some attribute
 * cataloged as indexed but not matching additional opclass/collation
 * criteria.  This is done so that the implementation is as forgiving as
 * possible of redundancy within cataloged index attributes (or, less
 * usefully, within inference specification elements).  If collations actually
 * differ between apparently redundantly indexed attributes (redundant within
 * or across indexes), then there really is no redundancy as such.
 *
 * Note that if an inference element specifies an opclass and a collation at
 * once, both must match in at least one particular attribute within index
 * catalog definition in order for that inference element to be considered
 * inferred/satisfied.
 */
static bool
infer_collation_opclass_match(InferenceElem *elem, Relation idxRel, List *idxExprs)
{








































































}

/*
 * estimate_rel_size - estimate # pages and # tuples in a table or index
 *
 * We also estimate the fraction of the pages that are marked all-visible in
 * the visibility map, for use in estimation of index-only scans.
 *
 * If attr_widths isn't NULL, it points to the zero-index entry of the
 * relation's attr_widths[] cache; we fill this in if we have need to compute
 * the attribute widths for estimation purposes.
 */
void
estimate_rel_size(Relation rel, int32 *attr_widths, BlockNumber *pages, double *tuples, double *allvisfrac)
{
































































































































}

/*
 * get_rel_data_width
 *
 * Estimate the average width of (the data part of) the relation's tuples.
 *
 * If attr_widths isn't NULL, it points to the zero-index entry of the
 * relation's attr_widths[] cache; use and update that cache as appropriate.
 *
 * Currently we ignore dropped columns.  Ideally those should be included
 * in the result, but we haven't got any way to get info about them; and
 * since they might be mostly NULLs, treating them as zero-width is not
 * necessarily the wrong thing anyway.
 */
int32
get_rel_data_width(Relation rel, int32 *attr_widths)
{



































}

/*
 * get_relation_data_width
 *
 * External API for get_rel_data_width: same behavior except we have to
 * open the relcache entry.
 */
int32
get_relation_data_width(Oid relid, int32 *attr_widths)
{











}

/*
 * get_relation_constraints
 *
 * Retrieve the applicable constraint expressions of the given relation.
 *
 * Returns a List (possibly empty) of constraint expressions.  Each one
 * has been canonicalized, and its Vars are changed to have the varno
 * indicated by rel->relid.  This allows the expressions to be easily
 * compared to expressions taken from WHERE.
 *
 * If include_noinherit is true, it's okay to include constraints that
 * are marked NO INHERIT.
 *
 * If include_notnull is true, "col IS NOT NULL" expressions are generated
 * and added to the result for each column that's marked attnotnull.
 *
 * If include_partition is true, and the relation is a partition,
 * also include the partitioning constraints.
 *
 * Note: at present this is invoked at most once per relation per planner
 * run, and in many cases it won't be invoked at all, so there seems no
 * point in caching the data in RelOptInfo.
 */
static List *
get_relation_constraints(PlannerInfo *root, Oid relationObjectId, RelOptInfo *rel, bool include_noinherit, bool include_notnull, bool include_partition)
{



























































































































}

/*
 * get_relation_statistics
 *		Retrieve extended statistics defined on the table.
 *
 * Returns a List (possibly empty) of StatisticExtInfo objects describing
 * the statistics.  Note that this doesn't load the actual statistics data,
 * just the identifying metadata.  Only stats actually built are considered.
 */
static List *
get_relation_statistics(RelOptInfo *rel, Relation relation)
{



















































































}

/*
 * relation_excluded_by_constraints
 *
 * Detect whether the relation need not be scanned because it has either
 * self-inconsistent restrictions, or restrictions inconsistent with the
 * relation's applicable constraints.
 *
 * Note: this examines only rel->relid, rel->reloptkind, and
 * rel->baserestrictinfo; therefore it can be called before filling in
 * other fields of the RelOptInfo.
 */
bool
relation_excluded_by_constraints(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{

















































































































































































}

/*
 * build_physical_tlist
 *
 * Build a targetlist consisting of exactly the relation's user attributes,
 * in order.  The executor can special-case such tlists to avoid a projection
 * step at runtime, so we use such tlists preferentially for scan nodes.
 *
 * Exception: if there are any dropped or missing columns, we punt and return
 * NIL.  Ideally we would like to handle these cases too.  However this
 * creates problems for ExecTypeFromTL, which may be asked to build a tupdesc
 * for a tlist that includes vars of no-longer-existent types.  In theory we
 * could dig out the required info from the pg_attribute entries of the
 * relation, but that data is not readily available to ExecTypeFromTL.
 * For now, we don't apply the physical-tlist optimization when there are
 * dropped cols.
 *
 * We also support building a "physical" tlist for subqueries, functions,
 * values lists, table expressions, and CTEs, since the same optimization can
 * occur in SubqueryScan, FunctionScan, ValuesScan, CteScan, TableFunc,
 * NamedTuplestoreScan, and WorkTableScan nodes.
 */
List *
build_physical_tlist(PlannerInfo *root, RelOptInfo *rel)
{





















































































}

/*
 * build_index_tlist
 *
 * Build a targetlist representing the columns of the specified index.
 * Each column is represented by a Var for the corresponding base-relation
 * column, or an expression in base-relation Vars, as appropriate.
 *
 * There are never any dropped columns in indexes, so unlike
 * build_physical_tlist, we need no failure case.
 */
static List *
build_index_tlist(PlannerInfo *root, IndexOptInfo *index, Relation heapRelation)
{














































}

/*
 * restriction_selectivity
 *
 * Returns the selectivity of a specified restriction operator clause.
 * This code executes registered procedures stored in the
 * operator relation, by calling the function manager.
 *
 * See clause_selectivity() for the meaning of the additional parameters.
 */
Selectivity
restriction_selectivity(PlannerInfo *root, Oid operatorid, List *args, Oid inputcollid, int varRelid)
{




















}

/*
 * join_selectivity
 *
 * Returns the selectivity of a specified join operator clause.
 * This code executes registered procedures stored in the
 * operator relation, by calling the function manager.
 *
 * See clause_selectivity() for the meaning of the additional parameters.
 */
Selectivity
join_selectivity(PlannerInfo *root, Oid operatorid, List *args, Oid inputcollid, JoinType jointype, SpecialJoinInfo *sjinfo)
{




















}

/*
 * function_selectivity
 *
 * Returns the selectivity of a specified boolean function clause.
 * This code executes registered procedures stored in the
 * pg_proc relation, by calling the function manager.
 *
 * See clause_selectivity() for the meaning of the additional parameters.
 */
Selectivity
function_selectivity(PlannerInfo *root, Oid funcid, List *args, Oid inputcollid, bool is_join, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{









































}

/*
 * add_function_cost
 *
 * Get an estimate of the execution cost of a function, and *add* it to
 * the contents of *cost.  The estimate may include both one-time and
 * per-tuple components, since QualCost does.
 *
 * The funcid must always be supplied.  If it is being called as the
 * implementation of a specific parsetree node (FuncExpr, OpExpr,
 * WindowFunc, etc), pass that as "node", else pass NULL.
 *
 * In some usages root might be NULL, too.
 */
void
add_function_cost(PlannerInfo *root, Oid funcid, Node *node, QualCost *cost)
{








































}

/*
 * get_function_rows
 *
 * Get an estimate of the number of rows returned by a set-returning function.
 *
 * The funcid must always be supplied.  In current usage, the calling node
 * will always be supplied, and will be either a FuncExpr or OpExpr.
 * But it's a good idea to not fail if it's NULL.
 *
 * In some usages root might be NULL, too.
 *
 * Note: this returns the unfiltered result of the support function, if any.
 * It's usually a good idea to apply clamp_row_est() to the result, but we
 * leave it to the caller to do so.
 */
double
get_function_rows(PlannerInfo *root, Oid funcid, Node *node)
{









































}

/*
 * has_unique_index
 *
 * Detect whether there is a unique index on the specified attribute
 * of the specified relation, thus allowing us to conclude that all
 * the (non-null) values of the attribute are distinct.
 *
 * This function does not check the index's indimmediate property, which
 * means that uniqueness may transiently fail to hold intra-transaction.
 * That's appropriate when we are making statistical estimates, but beware
 * of using this for any correctness proofs.
 */
bool
has_unique_index(RelOptInfo *rel, AttrNumber attno)
{




















}

/*
 * has_row_triggers
 *
 * Detect whether the specified relation has any row-level triggers for event.
 */
bool
has_row_triggers(PlannerInfo *root, Index rti, CmdType event)
{




































}

bool
has_stored_generated_columns(PlannerInfo *root, Index rti)
{














}

/*
 * set_relation_partition_info
 *
 * Set partitioning scheme and related information for a partitioned table.
 */
static void
set_relation_partition_info(PlannerInfo *root, RelOptInfo *rel, Relation relation)
{















}

/*
 * find_partition_scheme
 *
 * Find or create a PartitionScheme for this Relation.
 */
static PartitionScheme
find_partition_scheme(PlannerInfo *root, Relation relation)
{























































































}

/*
 * set_baserel_partition_key_exprs
 *
 * Builds partition key expressions for the given base relation and sets them
 * in given RelOptInfo.  Any single column partition keys are converted to Var
 * nodes.  All Var nodes are restamped with the relid of given relation.
 */
static void
set_baserel_partition_key_exprs(Relation relation, RelOptInfo *rel)
{





















































}