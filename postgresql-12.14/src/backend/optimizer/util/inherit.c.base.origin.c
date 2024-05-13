/*-------------------------------------------------------------------------
 *
 * inherit.c
 *	  Routines to process child relations in inheritance trees
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/inherit.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "partitioning/partprune.h"
#include "utils/rel.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

static void
expand_partitioned_rtentry(PlannerInfo *root, RelOptInfo *relinfo, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, LOCKMODE lockmode);
static void
expand_single_inheritance_child(PlannerInfo *root, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, Relation childrel, RangeTblEntry **childrte_p, Index *childRTindex_p);
static Bitmapset *
translate_col_privs(const Bitmapset *parent_privs, List *translated_vars);
static void
expand_appendrel_subquery(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte, Index rti);

/*
 * expand_inherited_rtentry
 *		Expand a rangetable entry that has the "inh" bit set.
 *
 * "inh" is only allowed in two cases: RELATION and SUBQUERY RTEs.
 *
 * "inh" on a plain RELATION RTE means that it is a partitioned table or the
 * parent of a traditional-inheritance set.  In this case we must add entries
 * for all the interesting child tables to the query's rangetable, and build
 * additional planner data structures for them, including RelOptInfos,
 * AppendRelInfos, and possibly PlanRowMarks.
 *
 * Note that the original RTE is considered to represent the whole inheritance
 * set.  In the case of traditional inheritance, the first of the generated
 * RTEs is an RTE for the same table, but with inh = false, to represent the
 * parent table in its role as a simple member of the inheritance set.  For
 * partitioning, we don't need a second RTE because the partitioned table
 * itself has no data and need not be scanned.
 *
 * "inh" on a SUBQUERY RTE means that it's the parent of a UNION ALL group,
 * which is treated as an appendrel similarly to inheritance cases; however,
 * we already made RTEs and AppendRelInfos for the subqueries.  We only need
 * to build RelOptInfos for them, which is done by expand_appendrel_subquery.
 */
void
expand_inherited_rtentry(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte, Index rti)
{































































































































































































}

/*
 * expand_partitioned_rtentry
 *		Recursively expand an RTE for a partitioned table.
 */
static void
expand_partitioned_rtentry(PlannerInfo *root, RelOptInfo *relinfo, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, LOCKMODE lockmode)
{







































































































}

/*
 * expand_single_inheritance_child
 *		Build a RangeTblEntry and an AppendRelInfo, plus maybe a
 *PlanRowMark.
 *
 * We now expand the partition hierarchy level by level, creating a
 * corresponding hierarchy of AppendRelInfos and RelOptInfos, where each
 * partitioned descendant acts as a parent of its immediate partitions.
 * (This is a difference from what older versions of PostgreSQL did and what
 * is still done in the case of table inheritance for unpartitioned tables,
 * where the hierarchy is flattened during RTE expansion.)
 *
 * PlanRowMarks still carry the top-parent's RTI, and the top-parent's
 * allMarkTypes field still accumulates values from all descendents.
 *
 * "parentrte" and "parentRTindex" are immediate parent's RTE and
 * RTI. "top_parentrc" is top parent's PlanRowMark.
 *
 * The child RangeTblEntry and its RTI are returned in "childrte_p" and
 * "childRTindex_p" resp.
 */
static void
expand_single_inheritance_child(PlannerInfo *root, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, Relation childrel, RangeTblEntry **childrte_p, Index *childRTindex_p)
{




































































































}

/*
 * translate_col_privs
 *	  Translate a bitmapset representing per-column privileges from the
 *	  parent rel's attribute numbering to the child's.
 *
 * The only surprise here is that we don't translate a parent whole-row
 * reference into a child whole-row reference.  That would mean requiring
 * permissions on all child columns, which is overly strict, since the
 * query is really only going to reference the inherited columns.  Instead
 * we set the per-column bits for all inherited columns.
 */
static Bitmapset *
translate_col_privs(const Bitmapset *parent_privs, List *translated_vars)
{



































}

/*
 * expand_appendrel_subquery
 *		Add "other rel" RelOptInfos for the children of an appendrel
 *baserel
 *
 * "rel" is a subquery relation that has the rte->inh flag set, meaning it
 * is a UNION ALL subquery that's been flattened into an appendrel, with
 * child subqueries listed in root->append_rel_list.  We need to build
 * a RelOptInfo for each child relation so that we can plan scans on them.
 */
static void
expand_appendrel_subquery(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte, Index rti)
{





























}

/*
 * apply_child_basequals
 *		Populate childrel's base restriction quals from parent rel's
 *quals, translating them using appinfo.
 *
 * If any of the resulting clauses evaluate to constant false or NULL, we
 * return false and don't apply any quals.  Caller should mark the relation as
 * a dummy rel in this case, since it doesn't need to be scanned.
 */
bool
apply_child_basequals(PlannerInfo *root, RelOptInfo *parentrel, RelOptInfo *childrel, RangeTblEntry *childRTE, AppendRelInfo *appinfo)
{






























































































}