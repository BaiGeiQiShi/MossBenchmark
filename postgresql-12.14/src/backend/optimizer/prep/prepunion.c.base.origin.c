/*-------------------------------------------------------------------------
 *
 * prepunion.c
 *	  Routines to plan set-operation queries.  The filename is a leftover
 *	  from a time when only UNIONs were implemented.
 *
 * There are two code paths in the planner for set-operation queries.
 * If a subquery consists entirely of simple UNION ALL operations, it
 * is converted into an "append relation".  Otherwise, it is handled
 * by the general code in this module (plan_set_operations and its
 * subroutines).  There is some support code here for the append-relation
 * case, but most of the heavy lifting for that is done elsewhere,
 * notably in prepjointree.c and allpaths.c.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/prep/prepunion.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/tlist.h"
#include "parser/parse_coerce.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

static RelOptInfo *
recurse_set_operations(Node *setOp, PlannerInfo *root, List *colTypes, List *colCollations, bool junkOK, int flag, List *refnames_tlist, List **pTargetList, double *pNumGroups);
static RelOptInfo *
generate_recursion_path(SetOperationStmt *setOp, PlannerInfo *root, List *refnames_tlist, List **pTargetList);
static RelOptInfo *
generate_union_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList);
static RelOptInfo *
generate_nonunion_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList);
static List *
plan_union_children(PlannerInfo *root, SetOperationStmt *top_union, List *refnames_tlist, List **tlist_list);
static Path *
make_union_unique(SetOperationStmt *op, Path *path, List *tlist, PlannerInfo *root);
static void
postprocess_setop_rel(PlannerInfo *root, RelOptInfo *rel);
static bool
choose_hashed_setop(PlannerInfo *root, List *groupClauses, Path *input_path, double dNumGroups, double dNumOutputRows, const char *construct);
static List *
generate_setop_tlist(List *colTypes, List *colCollations, int flag, Index varno, bool hack_constants, List *input_tlist, List *refnames_tlist);
static List *
generate_append_tlist(List *colTypes, List *colCollations, bool flag, List *input_tlists, List *refnames_tlist);
static List *
generate_setop_grouplist(SetOperationStmt *op, List *targetlist);

/*
 * plan_set_operations
 *
 *	  Plans the queries for a tree of set operations
 *(UNION/INTERSECT/EXCEPT)
 *
 * This routine only deals with the setOperations tree of the given query.
 * Any top-level ORDER BY requested in root->parse->sortClause will be handled
 * when we return to grouping_planner; likewise for LIMIT.
 *
 * What we return is an "upperrel" RelOptInfo containing at least one Path
 * that implements the set-operation tree.  In addition, root->processed_tlist
 * receives a targetlist representing the output of the topmost setop node.
 */
RelOptInfo *
plan_set_operations(PlannerInfo *root)
{



































































}

/*
 * recurse_set_operations
 *	  Recursively handle one step in a tree of set operations
 *
 * colTypes: OID list of set-op's result column datatypes
 * colCollations: OID list of set-op's result column collations
 * junkOK: if true, child resjunk columns may be left in the result
 * flag: if >= 0, add a resjunk output column indicating value of flag
 * refnames_tlist: targetlist to take column names from
 *
 * Returns a RelOptInfo for the subtree, as well as these output parameters:
 * *pTargetList: receives the fully-fledged tlist for the subtree's top plan
 * *pNumGroups: if not NULL, we estimate the number of distinct groups
 *		in the result, and store it there
 *
 * The pTargetList output parameter is mostly redundant with the pathtarget
 * of the returned RelOptInfo, but for the moment we need it because much of
 * the logic in this file depends on flag columns being marked resjunk.
 * Pending a redesign of how that works, this is the easy way out.
 *
 * We don't have to care about typmods here: the only allowed difference
 * between set-op input and output typmods is input is a specific typmod
 * and output is -1, and that does not require a coercion.
 */
static RelOptInfo *
recurse_set_operations(Node *setOp, PlannerInfo *root, List *colTypes, List *colCollations, bool junkOK, int flag, List *refnames_tlist, List **pTargetList, double *pNumGroups)
{



































































































































































































}

/*
 * Generate paths for a recursive UNION node
 */
static RelOptInfo *
generate_recursion_path(SetOperationStmt *setOp, PlannerInfo *root, List *refnames_tlist, List **pTargetList)
{












































































}

/*
 * Generate paths for a UNION or UNION ALL node
 */
static RelOptInfo *
generate_union_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList)
{





















































































































































}

/*
 * Generate paths for an INTERSECT, INTERSECT ALL, EXCEPT, or EXCEPT ALL node
 */
static RelOptInfo *
generate_nonunion_paths(SetOperationStmt *op, PlannerInfo *root, List *refnames_tlist, List **pTargetList)
{






















































































































}

/*
 * Pull up children of a UNION node that are identically-propertied UNIONs.
 *
 * NOTE: we can also pull a UNION ALL up into a UNION, since the distinct
 * output rows will be lost anyway.
 *
 * NOTE: currently, we ignore collations while determining if a child has
 * the same properties.  This is semantically sound only so long as all
 * collations have the same notion of equality.  It is valid from an
 * implementation standpoint because we don't care about the ordering of
 * a UNION child's result: UNION ALL results are always unordered, and
 * generate_union_paths will force a fresh sort if the top level is a UNION.
 */
static List *
plan_union_children(PlannerInfo *root, SetOperationStmt *top_union, List *refnames_tlist, List **tlist_list)
{








































}

/*
 * Add nodes to the given path tree to unique-ify the result of a UNION.
 */
static Path *
make_union_unique(SetOperationStmt *op, Path *path, List *tlist, PlannerInfo *root)
{


































}

/*
 * postprocess_setop_rel - perform steps required after adding paths
 */
static void
postprocess_setop_rel(PlannerInfo *root, RelOptInfo *rel)
{











}

/*
 * choose_hashed_setop - should we use hashing for a set operation?
 */
static bool
choose_hashed_setop(PlannerInfo *root, List *groupClauses, Path *input_path, double dNumGroups, double dNumOutputRows, const char *construct)
{




















































































}

/*
 * Generate targetlist for a set-operation plan node
 *
 * colTypes: OID list of set-op's result column datatypes
 * colCollations: OID list of set-op's result column collations
 * flag: -1 if no flag column needed, 0 or 1 to create a const flag column
 * varno: varno to use in generated Vars
 * hack_constants: true to copy up constants (see comments in code)
 * input_tlist: targetlist of this node's input node
 * refnames_tlist: targetlist to take column names from
 */
static List *
generate_setop_tlist(List *colTypes, List *colCollations, int flag, Index varno, bool hack_constants, List *input_tlist, List *refnames_tlist)
{




























































































}

/*
 * Generate targetlist for a set-operation Append node
 *
 * colTypes: OID list of set-op's result column datatypes
 * colCollations: OID list of set-op's result column collations
 * flag: true to create a flag column copied up from subplans
 * input_tlists: list of tlists for sub-plans of the Append
 * refnames_tlist: targetlist to take column names from
 *
 * The entries in the Append's targetlist should always be simple Vars;
 * we just have to make sure they have the right datatypes/typmods/collations.
 * The Vars are always generated with varno 0.
 *
 * XXX a problem with the varno-zero approach is that set_pathtarget_cost_width
 * cannot figure out a realistic width for the tlist we make here.  But we
 * ought to refactor this code to produce a PathTarget directly, anyway.
 */
static List *
generate_append_tlist(List *colTypes, List *colCollations, bool flag, List *input_tlists, List *refnames_tlist)
{


































































































}

/*
 * generate_setop_grouplist
 *		Build a SortGroupClause list defining the sort/grouping
 *properties of the setop's output columns.
 *
 * Parse analysis already determined the properties and built a suitable
 * list, except that the entries do not have sortgrouprefs set because
 * the parser output representation doesn't include a tlist for each
 * setop.  So what we need to do here is copy that list and install
 * proper sortgrouprefs into it (copying those from the targetlist).
 */
static List *
generate_setop_grouplist(SetOperationStmt *op, List *targetlist)
{






























}