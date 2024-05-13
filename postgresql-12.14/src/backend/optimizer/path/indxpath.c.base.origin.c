/*-------------------------------------------------------------------------
 *
 * indxpath.c
 *	  Routines to determine which indexes are usable for scanning a
 *	  given relation, and create Paths accordingly.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/indxpath.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/stratnum.h"
#include "access/sysattr.h"
#include "catalog/pg_am.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)
#undef make_simple_restrictinfo
#define make_simple_restrictinfo(root, clause) make_restrictinfo_new(root, clause, true, false, false, 0, NULL, NULL, NULL)

/* XXX see PartCollMatchesExprColl */
#define IndexCollMatchesExprColl(idxcollation, exprcollation) ((idxcollation) == InvalidOid || (idxcollation) == (exprcollation))

/* Whether we are looking for plain indexscan, bitmap scan, or either */
typedef enum
{
  ST_INDEXSCAN,  /* must support amgettuple */
  ST_BITMAPSCAN, /* must support amgetbitmap */
  ST_ANYSCAN     /* either is okay */
} ScanTypeControl;

/* Data structure for collecting qual clauses that match an index */
typedef struct
{
  bool nonempty; /* True if lists are not all empty */
  /* Lists of IndexClause nodes, one list per index column */
  List *indexclauses[INDEX_MAX_KEYS];
} IndexClauseSet;

/* Per-path data used within choose_bitmap_and() */
typedef struct
{
  Path *path;           /* IndexPath, BitmapAndPath, or BitmapOrPath */
  List *quals;          /* the WHERE clauses it uses */
  List *preds;          /* predicates of its partial index(es) */
  Bitmapset *clauseids; /* quals+preds represented as a bitmapset */
  bool unclassifiable;  /* has too many quals+preds to process? */
} PathClauseUsage;

/* Callback argument for ec_member_matches_indexcol */
typedef struct
{
  IndexOptInfo *index; /* index we're considering */
  int indexcol;        /* index column we want to match to */
} ec_member_matches_arg;

static void
consider_index_join_clauses(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths);
static void
consider_index_join_outer_rels(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, List *indexjoinclauses, int considered_clauses, List **considered_relids);
static void
get_join_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, Relids relids, List **considered_relids);
static bool
eclass_already_used(EquivalenceClass *parent_ec, Relids oldrelids, List *indexjoinclauses);
static bool
bms_equal_any(Relids relids, List *relids_list);
static void
get_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, List **bitindexpaths);
static List *
build_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, bool useful_predicate, ScanTypeControl scantype, bool *skip_nonnative_saop, bool *skip_lower_saop);
static List *
build_paths_for_OR(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses);
static List *
generate_bitmap_or_paths(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses);
static Path *
choose_bitmap_and(PlannerInfo *root, RelOptInfo *rel, List *paths);
static int
path_usage_comparator(const void *a, const void *b);
static Cost
bitmap_scan_cost_est(PlannerInfo *root, RelOptInfo *rel, Path *ipath);
static Cost
bitmap_and_cost_est(PlannerInfo *root, RelOptInfo *rel, List *paths);
static PathClauseUsage *
classify_index_clause_usage(Path *path, List **clauselist);
static void
find_indexpath_quals(Path *bitmapqual, List **quals, List **preds);
static int
find_list_position(Node *node, List **nodelist);
static bool
check_index_only(RelOptInfo *rel, IndexOptInfo *index);
static double
get_loop_count(PlannerInfo *root, Index cur_relid, Relids outer_relids);
static double
adjust_rowcount_for_semijoins(PlannerInfo *root, Index cur_relid, Index outer_relid, double rowcount);
static double
approximate_joinrel_size(PlannerInfo *root, Relids relids);
static void
match_restriction_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset);
static void
match_join_clauses_to_index(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauseset, List **joinorclauses);
static void
match_eclass_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset);
static void
match_clauses_to_index(PlannerInfo *root, List *clauses, IndexOptInfo *index, IndexClauseSet *clauseset);
static void
match_clause_to_index(PlannerInfo *root, RestrictInfo *rinfo, IndexOptInfo *index, IndexClauseSet *clauseset);
static IndexClause *
match_clause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_boolean_index_clause(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_opclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_funcclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
get_index_clause_from_support(PlannerInfo *root, RestrictInfo *rinfo, Oid funcid, int indexarg, int indexcol, IndexOptInfo *index);
static IndexClause *
match_saopclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_rowcompare_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
expand_indexqual_rowcompare(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index, Oid expr_op, bool var_on_left);
static void
match_pathkeys_to_index(IndexOptInfo *index, List *pathkeys, List **orderby_clauses_p, List **clause_columns_p);
static Expr *
match_clause_to_ordering_op(IndexOptInfo *index, int indexcol, Expr *clause, Oid pk_opfamily);
static bool
ec_member_matches_indexcol(PlannerInfo *root, RelOptInfo *rel, EquivalenceClass *ec, EquivalenceMember *em, void *arg);

/*
 * create_index_paths()
 *	  Generate all interesting index paths for the given relation.
 *	  Candidate paths are added to the rel's pathlist (using add_path).
 *
 * To be considered for an index scan, an index must match one or more
 * restriction clauses or join clauses from the query's qual condition,
 * or match the query's ORDER BY condition, or have a predicate that
 * matches the query's qual condition.
 *
 * There are two basic kinds of index scans.  A "plain" index scan uses
 * only restriction clauses (possibly none at all) in its indexqual,
 * so it can be applied in any context.  A "parameterized" index scan uses
 * join clauses (plus restriction clauses, if available) in its indexqual.
 * When joining such a scan to one of the relations supplying the other
 * variables used in its indexqual, the parameterized scan must appear as
 * the inner relation of a nestloop join; it can't be used on the outer side,
 * nor in a merge or hash join.  In that context, values for the other rels'
 * attributes are available and fixed during any one scan of the indexpath.
 *
 * An IndexPath is generated and submitted to add_path() for each plain or
 * parameterized index scan this routine deems potentially interesting for
 * the current query.
 *
 * 'rel' is the relation for which we want to generate index paths
 *
 * Note: check_index_predicates() must have been run previously for this rel.
 *
 * Note: in cases involving LATERAL references in the relation's tlist, it's
 * possible that rel->lateral_relids is nonempty.  Currently, we include
 * lateral_relids into the parameterization reported for each path, but don't
 * take it into account otherwise.  The fact that any such rels *must* be
 * available as parameter sources perhaps should influence our choices of
 * index quals ... but for now, it doesn't seem worth troubling over.
 * In particular, comments below about "unparameterized" paths should be read
 * as meaning "unparameterized so far as the indexquals are concerned".
 */
void
create_index_paths(PlannerInfo *root, RelOptInfo *rel)
{



















































































































































































}

/*
 * consider_index_join_clauses
 *	  Given sets of join clauses for an index, decide which parameterized
 *	  index paths to build.
 *
 * Plain indexpaths are sent directly to add_path, while potential
 * bitmap indexpaths are added to *bitindexpaths for later processing.
 *
 * 'rel' is the index's heap relation
 * 'index' is the index for which we want to generate paths
 * 'rclauseset' is the collection of indexable restriction clauses
 * 'jclauseset' is the collection of indexable simple join clauses
 * 'eclauseset' is the collection of indexable clauses from EquivalenceClasses
 * '*bitindexpaths' is the list to add bitmap paths to
 */
static void
consider_index_join_clauses(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths)
{


































}

/*
 * consider_index_join_outer_rels
 *	  Generate parameterized paths based on clause relids in the clause
 *list.
 *
 * Workhorse for consider_index_join_clauses; see notes therein for rationale.
 *
 * 'rel', 'index', 'rclauseset', 'jclauseset', 'eclauseset', and
 *		'bitindexpaths' as above
 * 'indexjoinclauses' is a list of IndexClauses for join clauses
 * 'considered_clauses' is the total number of clauses considered (so far)
 * '*considered_relids' is a list of all relids sets already considered
 */
static void
consider_index_join_outer_rels(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, List *indexjoinclauses, int considered_clauses, List **considered_relids)
{










































































}

/*
 * get_join_index_paths
 *	  Generate index paths using clauses from the specified outer relations.
 *	  In addition to generating paths, relids is added to *considered_relids
 *	  if not already present.
 *
 * Workhorse for consider_index_join_clauses; see notes therein for rationale.
 *
 * 'rel', 'index', 'rclauseset', 'jclauseset', 'eclauseset',
 *		'bitindexpaths', 'considered_relids' as above
 * 'relids' is the current set of relids to consider (the target rel plus
 *		one or more outer rels)
 */
static void
get_join_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, Relids relids, List **considered_relids)
{
































































}

/*
 * eclass_already_used
 *		True if any join clause usable with oldrelids was generated from
 *		the specified equivalence class.
 */
static bool
eclass_already_used(EquivalenceClass *parent_ec, Relids oldrelids, List *indexjoinclauses)
{













}

/*
 * bms_equal_any
 *		True if relids is bms_equal to any member of relids_list
 *
 * Perhaps this should be in bitmapset.c someday.
 */
static bool
bms_equal_any(Relids relids, List *relids_list)
{










}

/*
 * get_index_paths
 *	  Given an index and a set of index clauses for it, construct
 *IndexPaths.
 *
 * Plain indexpaths are sent directly to add_path, while potential
 * bitmap indexpaths are added to *bitindexpaths for later processing.
 *
 * This is a fairly simple frontend to build_index_paths().  Its reason for
 * existence is mainly to handle ScalarArrayOpExpr quals properly.  If the
 * index AM supports them natively, we should just include them in simple
 * index paths.  If not, we should exclude them while building simple index
 * paths, and then make a separate attempt to include them in bitmap paths.
 * Furthermore, we should consider excluding lower-order ScalarArrayOpExpr
 * quals so as to create ordered paths.
 */
static void
get_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, List **bitindexpaths)
{




























































}

/*
 * build_index_paths
 *	  Given an index and a set of index clauses for it, construct zero
 *	  or more IndexPaths. It also constructs zero or more partial
 *IndexPaths.
 *
 * We return a list of paths because (1) this routine checks some cases
 * that should cause us to not generate any IndexPath, and (2) in some
 * cases we want to consider both a forward and a backward scan, so as
 * to obtain both sort orders.  Note that the paths are just returned
 * to the caller and not immediately fed to add_path().
 *
 * At top level, useful_predicate should be exactly the index's predOK flag
 * (ie, true if it has a predicate that was proven from the restriction
 * clauses).  When working on an arm of an OR clause, useful_predicate
 * should be true if the predicate required the current OR list to be proven.
 * Note that this routine should never be called at all if the index has an
 * unprovable predicate.
 *
 * scantype indicates whether we want to create plain indexscans, bitmap
 * indexscans, or both.  When it's ST_BITMAPSCAN, we will not consider
 * index ordering while deciding if a Path is worth generating.
 *
 * If skip_nonnative_saop is non-NULL, we ignore ScalarArrayOpExpr clauses
 * unless the index AM supports them directly, and we set *skip_nonnative_saop
 * to true if we found any such clauses (caller must initialize the variable
 * to false).  If it's NULL, we do not ignore ScalarArrayOpExpr clauses.
 *
 * If skip_lower_saop is non-NULL, we ignore ScalarArrayOpExpr clauses for
 * non-first index columns, and we set *skip_lower_saop to true if we found
 * any such clauses (caller must initialize the variable to false).  If it's
 * NULL, we do not ignore non-first ScalarArrayOpExpr clauses, but they will
 * result in considering the scan's output to be unordered.
 *
 * 'rel' is the index's heap relation
 * 'index' is the index for which we want to generate paths
 * 'clauses' is the collection of indexable clauses (IndexClause nodes)
 * 'useful_predicate' indicates whether the index has a useful predicate
 * 'scantype' indicates whether we need plain or bitmap scan support
 * 'skip_nonnative_saop' indicates whether to accept SAOP if index AM doesn't
 * 'skip_lower_saop' indicates whether to accept non-first-column SAOP
 */
static List *
build_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, bool useful_predicate, ScanTypeControl scantype, bool *skip_nonnative_saop, bool *skip_lower_saop)
{










































































































































































































































}

/*
 * build_paths_for_OR
 *	  Given a list of restriction clauses from one arm of an OR clause,
 *	  construct all matching IndexPaths for the relation.
 *
 * Here we must scan all indexes of the relation, since a bitmap OR tree
 * can use multiple indexes.
 *
 * The caller actually supplies two lists of restriction clauses: some
 * "current" ones and some "other" ones.  Both lists can be used freely
 * to match keys of the index, but an index must use at least one of the
 * "current" clauses to be considered usable.  The motivation for this is
 * examples like
 *		WHERE (x = 42) AND (... OR (y = 52 AND z = 77) OR ....)
 * While we are considering the y/z subclause of the OR, we can use "x = 42"
 * as one of the available index conditions; but we shouldn't match the
 * subclause to any index on x alone, because such a Path would already have
 * been generated at the upper level.  So we could use an index on x,y,z
 * or an index on x,y for the OR subclause, but not an index on just x.
 * When dealing with a partial index, a match of the index predicate to
 * one of the "current" clauses also makes the index usable.
 *
 * 'rel' is the relation for which we want to generate index paths
 * 'clauses' is the current list of clauses (RestrictInfo nodes)
 * 'other_clauses' is the list of additional upper-level clauses
 */
static List *
build_paths_for_OR(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses)
{




















































































}

/*
 * generate_bitmap_or_paths
 *		Look through the list of clauses to find OR clauses, and
 *generate a BitmapOrPath for each one we can handle that way.  Return a list of
 *the generated BitmapOrPaths.
 *
 * other_clauses is a list of additional clauses that can be assumed true
 * for the purpose of generating indexquals, but are not to be searched for
 * ORs.  (See build_paths_for_OR() for motivation.)
 */
static List *
generate_bitmap_or_paths(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses)
{




















































































}

/*
 * choose_bitmap_and
 *		Given a nonempty list of bitmap paths, AND them into one path.
 *
 * This is a nontrivial decision since we can legally use any subset of the
 * given path set.  We want to choose a good tradeoff between selectivity
 * and cost of computing the bitmap.
 *
 * The result is either a single one of the inputs, or a BitmapAndPath
 * combining multiple inputs.
 */
static Path *
choose_bitmap_and(PlannerInfo *root, RelOptInfo *rel, List *paths)
{





















































































































































































































}

/* qsort comparator to sort in increasing index access cost order */
static int
path_usage_comparator(const void *a, const void *b)
{
































}

/*
 * Estimate the cost of actually executing a bitmap scan with a single
 * index path (which could be a BitmapAnd or BitmapOr node).
 */
static Cost
bitmap_scan_cost_est(PlannerInfo *root, RelOptInfo *rel, Path *ipath)
{





















}

/*
 * Estimate the cost of actually executing a BitmapAnd scan with the given
 * inputs.
 */
static Cost
bitmap_and_cost_est(PlannerInfo *root, RelOptInfo *rel, List *paths)
{









}

/*
 * classify_index_clause_usage
 *		Construct a PathClauseUsage struct describing the WHERE clauses
 *and index predicate clauses used by the given indexscan path. We consider two
 *clauses the same if they are equal().
 *
 * At some point we might want to migrate this info into the Path data
 * structure proper, but for the moment it's only needed within
 * choose_bitmap_and().
 *
 * *clauselist is used and expanded as needed to identify all the distinct
 * clauses seen across successive calls.  Caller must initialize it to NIL
 * before first call of a set.
 */
static PathClauseUsage *
classify_index_clause_usage(Path *path, List **clauselist)
{













































}

/*
 * find_indexpath_quals
 *
 * Given the Path structure for a plain or bitmap indexscan, extract lists
 * of all the index clauses and index predicate conditions used in the Path.
 * These are appended to the initial contents of *quals and *preds (hence
 * caller should initialize those to NIL).
 *
 * Note we are not trying to produce an accurate representation of the AND/OR
 * semantics of the Path, but just find out all the base conditions used.
 *
 * The result lists contain pointers to the expressions used in the Path,
 * but all the list cells are freshly built, so it's safe to destructively
 * modify the lists (eg, by concat'ing with other lists).
 */
static void
find_indexpath_quals(Path *bitmapqual, List **quals, List **preds)
{





































}

/*
 * find_list_position
 *		Return the given node's position (counting from 0) in the given
 *		list of nodes.  If it's not equal() to any existing list member,
 *		add it at the end, and return that position.
 */
static int
find_list_position(Node *node, List **nodelist)
{


















}

/*
 * check_index_only
 *		Determine whether an index-only scan is possible for this index.
 */
static bool
check_index_only(RelOptInfo *rel, IndexOptInfo *index)
{




















































































}

/*
 * get_loop_count
 *		Choose the loop count estimate to use for costing a
 *parameterized path with the given set of outer relids.
 *
 * Since we produce parameterized paths before we've begun to generate join
 * relations, it's impossible to predict exactly how many times a parameterized
 * path will be iterated; we don't know the size of the relation that will be
 * on the outside of the nestloop.  However, we should try to account for
 * multiple iterations somehow in costing the path.  The heuristic embodied
 * here is to use the rowcount of the smallest other base relation needed in
 * the join clauses used by the path.  (We could alternatively consider the
 * largest one, but that seems too optimistic.)  This is of course the right
 * answer for single-other-relation cases, and it seems like a reasonable
 * zero-order approximation for multiway-join cases.
 *
 * In addition, we check to see if the other side of each join clause is on
 * the inside of some semijoin that the current relation is on the outside of.
 * If so, the only way that a parameterized path could be used is if the
 * semijoin RHS has been unique-ified, so we should use the number of unique
 * RHS rows rather than using the relation's raw rowcount.
 *
 * Note: for this to work, allpaths.c must establish all baserel size
 * estimates before it begins to compute paths, or at least before it
 * calls create_index_paths().
 */
static double
get_loop_count(PlannerInfo *root, Index cur_relid, Relids outer_relids)
{
















































}

/*
 * Check to see if outer_relid is on the inside of any semijoin that cur_relid
 * is on the outside of.  If so, replace rowcount with the estimated number of
 * unique rows from the semijoin RHS (assuming that's smaller, which it might
 * not be).  The estimate is crude but it's the best we can do at this stage
 * of the proceedings.
 */
static double
adjust_rowcount_for_semijoins(PlannerInfo *root, Index cur_relid, Index outer_relid, double rowcount)
{





















}

/*
 * Make an approximate estimate of the size of a joinrel.
 *
 * We don't have enough info at this point to get a good estimate, so we
 * just multiply the base relation sizes together.  Fortunately, this is
 * the right answer anyway for the most common case with a single relation
 * on the RHS of a semijoin.  Also, estimate_num_groups() has only a weak
 * dependency on its input_rows argument (it basically uses it as a clamp).
 * So we might be able to get a fairly decent end result even with a severe
 * overestimate of the RHS's raw size.
 */
static double
approximate_joinrel_size(PlannerInfo *root, Relids relids)
{

































}

/****************************************************************************
 *				----  ROUTINES TO CHECK QUERY CLAUSES  ----
 ****************************************************************************/

/*
 * match_restriction_clauses_to_index
 *	  Identify restriction clauses for the rel that match the index.
 *	  Matching clauses are added to *clauseset.
 */
static void
match_restriction_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset)
{


}

/*
 * match_join_clauses_to_index
 *	  Identify join clauses for the rel that match the index.
 *	  Matching clauses are added to *clauseset.
 *	  Also, add any potentially usable join OR clauses to *joinorclauses.
 */
static void
match_join_clauses_to_index(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauseset, List **joinorclauses)
{























}

/*
 * match_eclass_clauses_to_index
 *	  Identify EquivalenceClass join clauses for the rel that match the
 *index. Matching clauses are added to *clauseset.
 */
static void
match_eclass_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset)
{

























}

/*
 * match_clauses_to_index
 *	  Perform match_clause_to_index() for each clause in a list.
 *	  Matching clauses are added to *clauseset.
 */
static void
match_clauses_to_index(PlannerInfo *root, List *clauses, IndexOptInfo *index, IndexClauseSet *clauseset)
{








}

/*
 * match_clause_to_index
 *	  Test whether a qual clause can be used with an index.
 *
 * If the clause is usable, add an IndexClause entry for it to the appropriate
 * list in *clauseset.  (*clauseset must be initialized to zeroes before first
 * call.)
 *
 * Note: in some circumstances we may find the same RestrictInfos coming from
 * multiple places.  Defend against redundant outputs by refusing to add a
 * clause twice (pointer equality should be a good enough check for this).
 *
 * Note: it's possible that a badly-defined index could have multiple matching
 * columns.  We always select the first match if so; this avoids scenarios
 * wherein we get an inflated idea of the index's selectivity by using the
 * same clause multiple times with different index columns.
 */
static void
match_clause_to_index(PlannerInfo *root, RestrictInfo *rinfo, IndexOptInfo *index, IndexClauseSet *clauseset)
{

















































}

/*
 * match_clause_to_indexcol()
 *	  Determine whether a restriction clause matches a column of an index,
 *	  and if so, build an IndexClause node describing the details.
 *
 *	  To match an index normally, an operator clause:
 *
 *	  (1)  must be in the form (indexkey op const) or (const op indexkey);
 *		   and
 *	  (2)  must contain an operator which is in the index's operator family
 *		   for this column; and
 *	  (3)  must match the collation of the index, if collation is relevant.
 *
 *	  Our definition of "const" is exceedingly liberal: we allow anything
 *that doesn't involve a volatile function or a Var of the index's relation. In
 *particular, Vars belonging to other relations of the query are accepted here,
 *since a clause of that form can be used in a parameterized indexscan.  It's
 *the responsibility of higher code levels to manage restriction and join
 *clauses appropriately.
 *
 *	  Note: we do need to check for Vars of the index's relation on the
 *	  "const" side of the clause, since clauses like (a.f1 OP (b.f2 OP
 *a.f3)) are not processable by a parameterized indexscan on a.f1, whereas
 *	  something like (a.f1 OP (b.f2 OP c.f3)) is.
 *
 *	  Presently, the executor can only deal with indexquals that have the
 *	  indexkey on the left, so we can only use clauses that have the
 *indexkey on the right if we can commute the clause to put the key on the left.
 *	  We handle that by generating an IndexClause with the
 *correctly-commuted opclause as a derived indexqual.
 *
 *	  If the index has a collation, the clause must have the same collation.
 *	  For collation-less indexes, we assume it doesn't matter; this is
 *	  necessary for cases like "hstore ? text", wherein hstore's operators
 *	  don't care about collation but the clause will get marked with a
 *	  collation anyway because of the text argument.  (This logic is
 *	  embodied in the macro IndexCollMatchesExprColl.)
 *
 *	  It is also possible to match RowCompareExpr clauses to indexes (but
 *	  currently, only btree indexes handle this).
 *
 *	  It is also possible to match ScalarArrayOpExpr clauses to indexes,
 *when the clause is of the form "indexkey op ANY (arrayconst)".
 *
 *	  For boolean indexes, it is also possible to match the clause directly
 *	  to the indexkey; or perhaps the clause is (NOT indexkey).
 *
 *	  And, last but not least, some operators and functions can be processed
 *	  to derive (typically lossy) indexquals from a clause that isn't in
 *	  itself indexable.  If we see that any operand of an OpExpr or FuncExpr
 *	  matches the index key, and the function has a planner support function
 *	  attached to it, we'll invoke the support function to see if such an
 *	  indexqual can be built.
 *
 * 'rinfo' is the clause to be tested (as a RestrictInfo node).
 * 'indexcol' is a column number of 'index' (counting from 0).
 * 'index' is the index of interest.
 *
 * Returns an IndexClause if the clause can be used with this index key,
 * or NULL if not.
 *
 * NOTE:  returns NULL if clause is an OR or AND clause; it is the
 * responsibility of higher-level routines to cope with those.
 */
static IndexClause *
match_clause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
































































}

/*
 * match_boolean_index_clause
 *	  Recognize restriction clauses that can be matched to a boolean index.
 *
 * The idea here is that, for an index on a boolean column that supports the
 * BooleanEqualOperator, we can transform a plain reference to the indexkey
 * into "indexkey = true", or "NOT indexkey" into "indexkey = false", etc,
 * so as to make the expression indexable using the index's "=" operator.
 * Since Postgres 8.1, we must do this because constant simplification does
 * the reverse transformation; without this code there'd be no way to use
 * such an index at all.
 *
 * This should be called only when IsBooleanOpfamily() recognizes the
 * index's operator family.  We check to see if the clause matches the
 * index's key, and if so, build a suitable IndexClause.
 */
static IndexClause *
match_boolean_index_clause(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{




























































}

/*
 * match_opclause_to_indexcol()
 *	  Handles the OpExpr case for match_clause_to_indexcol(),
 *	  which see for comments.
 */
static IndexClause *
match_opclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{





























































































}

/*
 * match_funcclause_to_indexcol()
 *	  Handles the FuncExpr case for match_clause_to_indexcol(),
 *	  which see for comments.
 */
static IndexClause *
match_funcclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{




























}

/*
 * get_index_clause_from_support()
 *		If the function has a planner support function, try to construct
 *		an IndexClause using indexquals created by the support function.
 */
static IndexClause *
get_index_clause_from_support(PlannerInfo *root, RestrictInfo *rinfo, Oid funcid, int indexarg, int indexcol, IndexOptInfo *index)
{


















































}

/*
 * match_saopclause_to_indexcol()
 *	  Handles the ScalarArrayOpExpr case for match_clause_to_indexcol(),
 *	  which see for comments.
 */
static IndexClause *
match_saopclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
















































}

/*
 * match_rowcompare_to_indexcol()
 *	  Handles the RowCompareExpr case for match_clause_to_indexcol(),
 *	  which see for comments.
 *
 * In this routine we check whether the first column of the row comparison
 * matches the target index column.  This is sufficient to guarantee that some
 * index condition can be constructed from the RowCompareExpr --- the rest
 * is handled by expand_indexqual_rowcompare().
 */
static IndexClause *
match_rowcompare_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{










































































}

/*
 * expand_indexqual_rowcompare --- expand a single indexqual condition
 *		that is a RowCompareExpr
 *
 * It's already known that the first column of the row comparison matches
 * the specified column of the index.  We can use additional columns of the
 * row comparison as index qualifications, so long as they match the index
 * in the "same direction", ie, the indexkeys are all on the same side of the
 * clause and the operators are all the same-type members of the opfamilies.
 *
 * If all the columns of the RowCompareExpr match in this way, we just use it
 * as-is, except for possibly commuting it to put the indexkeys on the left.
 *
 * Otherwise, we build a shortened RowCompareExpr (if more than one
 * column matches) or a simple OpExpr (if the first-column match is all
 * there is).  In these cases the modified clause is always "<=" or ">="
 * even when the original was "<" or ">" --- this is necessary to match all
 * the rows that could match the original.  (We are building a lossy version
 * of the row comparison when we do this, so we set lossy = true.)
 *
 * Note: this is really just the last half of match_rowcompare_to_indexcol,
 * but we split it out for comprehensibility.
 */
static IndexClause *
expand_indexqual_rowcompare(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index, Oid expr_op, bool var_on_left)
{









































































































































































































}

/****************************************************************************
 *				----  ROUTINES TO CHECK ORDERING OPERATORS
 *----
 ****************************************************************************/

/*
 * match_pathkeys_to_index
 *		Test whether an index can produce output ordered according to
 *the given pathkeys using "ordering operators".
 *
 * If it can, return a list of suitable ORDER BY expressions, each of the form
 * "indexedcol operator pseudoconstant", along with an integer list of the
 * index column numbers (zero based) that each clause would be used with.
 * NIL lists are returned if the ordering is not achievable this way.
 *
 * On success, the result list is ordered by pathkeys, and in fact is
 * one-to-one with the requested pathkeys.
 */
static void
match_pathkeys_to_index(IndexOptInfo *index, List *pathkeys, List **orderby_clauses_p, List **clause_columns_p)
{



























































































}

/*
 * match_clause_to_ordering_op
 *	  Determines whether an ordering operator expression matches an
 *	  index column.
 *
 *	  This is similar to, but simpler than, match_clause_to_indexcol.
 *	  We only care about simple OpExpr cases.  The input is a bare
 *	  expression that is being ordered by, which must be of the form
 *	  (indexkey op const) or (const op indexkey) where op is an ordering
 *	  operator for the column's opfamily.
 *
 * 'index' is the index of interest.
 * 'indexcol' is a column number of 'index' (counting from 0).
 * 'clause' is the ordering expression to be tested.
 * 'pk_opfamily' is the btree opfamily describing the required sort order.
 *
 * Note that we currently do not consider the collation of the ordering
 * operator's result.  In practical cases the result type will be numeric
 * and thus have no collation, and it's not very clear what to match to
 * if it did have a collation.  The index's collation should match the
 * ordering operator's input collation, not its result.
 *
 * If successful, return 'clause' as-is if the indexkey is on the left,
 * otherwise a commuted copy of 'clause'.  If no match, return NULL.
 */
static Expr *
match_clause_to_ordering_op(IndexOptInfo *index, int indexcol, Expr *clause, Oid pk_opfamily)
{























































































}

/****************************************************************************
 *				----  ROUTINES TO DO PARTIAL INDEX PREDICATE
 *TESTS	----
 ****************************************************************************/

/*
 * check_index_predicates
 *		Set the predicate-derived IndexOptInfo fields for each index
 *		of the specified relation.
 *
 * predOK is set true if the index is partial and its predicate is satisfied
 * for this query, ie the query's WHERE clauses imply the predicate.
 *
 * indrestrictinfo is set to the relation's baserestrictinfo list less any
 * conditions that are implied by the index's predicate.  (Obviously, for a
 * non-partial index, this is the same as baserestrictinfo.)  Such conditions
 * can be dropped from the plan when using the index, in certain cases.
 *
 * At one time it was possible for this to get re-run after adding more
 * restrictions to the rel, thus possibly letting us prove more indexes OK.
 * That doesn't happen any more (at least not in the core code's usage),
 * but this code still supports it in case extensions want to mess with the
 * baserestrictinfo list.  We assume that adding more restrictions can't make
 * an index not predOK.  We must recompute indrestrictinfo each time, though,
 * to make sure any newly-added restrictions get into it if needed.
 */
void
check_index_predicates(PlannerInfo *root, RelOptInfo *rel)
{































































































































}

/****************************************************************************
 *				----  ROUTINES TO CHECK EXTERNALLY-VISIBLE
 *CONDITIONS  ----
 ****************************************************************************/

/*
 * ec_member_matches_indexcol
 *	  Test whether an EquivalenceClass member matches an index column.
 *
 * This is a callback for use by generate_implied_equalities_for_column.
 */
static bool
ec_member_matches_indexcol(PlannerInfo *root, RelOptInfo *rel, EquivalenceClass *ec, EquivalenceMember *em, void *arg)
{
































}

/*
 * relation_has_unique_index_for
 *	  Determine whether the relation provably has at most one row satisfying
 *	  a set of equality conditions, because the conditions constrain all
 *	  columns of some unique index.
 *
 * The conditions can be represented in either or both of two ways:
 * 1. A list of RestrictInfo nodes, where the caller has already determined
 * that each condition is a mergejoinable equality with an expression in
 * this relation on one side, and an expression not involving this relation
 * on the other.  The transient outer_is_left flag is used to identify which
 * side we should look at: left side if outer_is_left is false, right side
 * if it is true.
 * 2. A list of expressions in this relation, and a corresponding list of
 * equality operators. The caller must have already checked that the operators
 * represent equality.  (Note: the operators could be cross-type; the
 * expressions should correspond to their RHS inputs.)
 *
 * The caller need only supply equality conditions arising from joins;
 * this routine automatically adds in any usable baserestrictinfo clauses.
 * (Note that the passed-in restrictlist will be destructively modified!)
 */
bool
relation_has_unique_index_for(PlannerInfo *root, RelOptInfo *rel, List *restrictlist, List *exprlist, List *oprlist)
{













































































































































































}

/*
 * indexcol_is_bool_constant_for_query
 *
 * If an index column is constrained to have a constant value by the query's
 * WHERE conditions, then it's irrelevant for sort-order considerations.
 * Usually that means we have a restriction clause WHERE indexcol = constant,
 * which gets turned into an EquivalenceClass containing a constant, which
 * is recognized as redundant by build_index_pathkeys().  But if the index
 * column is a boolean variable (or expression), then we are not going to
 * see WHERE indexcol = constant, because expression preprocessing will have
 * simplified that to "WHERE indexcol" or "WHERE NOT indexcol".  So we are not
 * going to have a matching EquivalenceClass (unless the query also contains
 * "ORDER BY indexcol").  To allow such cases to work the same as they would
 * for non-boolean values, this function is provided to detect whether the
 * specified index column matches a boolean restriction clause.
 */
bool
indexcol_is_bool_constant_for_query(PlannerInfo *root, IndexOptInfo *index, int indexcol)
{































}

/****************************************************************************
 *				----  ROUTINES TO CHECK OPERANDS  ----
 ****************************************************************************/

/*
 * match_index_to_operand()
 *	  Generalized test for a match between an index's key
 *	  and the operand on one side of a restriction or join clause.
 *
 * operand: the nodetree to be compared to the index
 * indexcol: the column number of the index (counting from 0)
 * index: the index of interest
 *
 * Note that we aren't interested in collations here; the caller must check
 * for a collation match, if it's dealing with an operator where that matters.
 *
 * This is exported for use in selfuncs.c.
 */
bool
match_index_to_operand(Node *operand, int indexcol, IndexOptInfo *index)
{




































































}

/*
 * is_pseudo_constant_for_index()
 *	  Test whether the given expression can be used as an indexscan
 *	  comparison value.
 *
 * An indexscan comparison value must not contain any volatile functions,
 * and it can't contain any Vars of the index's own table.  Vars of
 * other tables are okay, though; in that case we'd be producing an
 * indexqual usable in a parameterized indexscan.  This is, therefore,
 * a weaker condition than is_pseudo_constant_clause().
 *
 * This function is exported for use by planner support functions,
 * which will have available the IndexOptInfo, but not any RestrictInfo
 * infrastructure.  It is making the same test made by functions above
 * such as match_opclause_to_indexcol(), but those rely where possible
 * on RestrictInfo information about variable membership.
 *
 * expr: the nodetree to be checked
 * index: the index of interest
 */
bool
is_pseudo_constant_for_index(Node *expr, IndexOptInfo *index)
{

}

bool
is_pseudo_constant_for_index_new(PlannerInfo *root, Node *expr, IndexOptInfo *index)
{










}