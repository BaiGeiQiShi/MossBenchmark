/*-------------------------------------------------------------------------
 *
 * appendinfo.c
 *	  Routines for mapping between append parent(s) and children
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/appendinfo.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  PlannerInfo *root;
  int nappinfos;
  AppendRelInfo **appinfos;
} adjust_appendrel_attrs_context;

static void
make_inh_translation_list(Relation oldrelation, Relation newrelation, Index newvarno, List **translated_vars);
static Node *
adjust_appendrel_attrs_mutator(Node *node, adjust_appendrel_attrs_context *context);
static List *
adjust_inherited_tlist(List *tlist, AppendRelInfo *context);

/*
 * make_append_rel_info
 *	  Build an AppendRelInfo for the parent-child pair
 */
AppendRelInfo *
make_append_rel_info(Relation parentrel, Relation childrel, Index parentRTindex, Index childRTindex)
{










}

/*
 * make_inh_translation_list
 *	  Build the list of translations from parent Vars to child Vars for
 *	  an inheritance child.
 *
 * For paranoia's sake, we match type/collation as well as attribute name.
 */
static void
make_inh_translation_list(Relation oldrelation, Relation newrelation, Index newvarno, List **translated_vars)
{














































































}

/*
 * adjust_appendrel_attrs
 *	  Copy the specified query or expression and translate Vars referring to
 *a parent rel to refer to the corresponding child rel instead.  We also update
 *rtindexes appearing outside Vars, such as resultRelation and jointree relids.
 *
 * Note: this is only applied after conversion of sublinks to subplans,
 * so we don't need to cope with recursion into sub-queries.
 *
 * Note: this is not hugely different from what pullup_replace_vars() does;
 * maybe we should try to fold the two routines together.
 */
Node *
adjust_appendrel_attrs(PlannerInfo *root, Node *node, int nappinfos, AppendRelInfo **appinfos)
{











































}

static Node *
adjust_appendrel_attrs_mutator(Node *node, adjust_appendrel_attrs_context *context)
{
































































































































































































































}

/*
 * adjust_appendrel_attrs_multilevel
 *	  Apply Var translations from a toplevel appendrel parent down to a
 *child.
 *
 * In some cases we need to translate expressions referencing a parent relation
 * to reference an appendrel child that's multiple levels removed from it.
 */
Node *
adjust_appendrel_attrs_multilevel(PlannerInfo *root, Node *node, Relids child_relids, Relids top_parent_relids)
{





























}

/*
 * Substitute child relids for parent relids in a Relid set.  The array of
 * appinfos specifies the substitutions to be performed.
 */
Relids
adjust_child_relids(Relids relids, int nappinfos, AppendRelInfo **appinfos)
{





























}

/*
 * Replace any relid present in top_parent_relids with its child in
 * child_relids. Members of child_relids can be multiple levels below top
 * parent in the partition hierarchy.
 */
Relids
adjust_child_relids_multilevel(PlannerInfo *root, Relids relids, Relids child_relids, Relids top_parent_relids)
{












































}

/*
 * Adjust the targetlist entries of an inherited UPDATE operation
 *
 * The expressions have already been fixed, but we have to make sure that
 * the target resnos match the child table (they may not, in the case of
 * a column that was added after-the-fact by ALTER TABLE).  In some cases
 * this can force us to re-order the tlist to preserve resno ordering.
 * (We do all this work in special cases so that preptlist.c is fast for
 * the typical case.)
 *
 * The given tlist has already been through expression_tree_mutator;
 * therefore the TargetEntry nodes are fresh copies that it's okay to
 * scribble on.
 *
 * Note that this is not needed for INSERT because INSERT isn't inheritable.
 */
static List *
adjust_inherited_tlist(List *tlist, AppendRelInfo *context)
{

























































































}

/*
 * find_appinfos_by_relids
 * 		Find AppendRelInfo structures for all relations specified by
 * relids.
 *
 * The AppendRelInfos are returned in an array, which can be pfree'd by the
 * caller. *nappinfos is set to the number of entries in the array.
 */
AppendRelInfo **
find_appinfos_by_relids(PlannerInfo *root, Relids relids, int *nappinfos)
{




















}