/*-------------------------------------------------------------------------
 *
 * dependencies.c
 *	  POSTGRES functional dependencies
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/statistics/dependencies.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "lib/stringinfo.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "nodes/nodes.h"
#include "nodes/pathnodes.h"
#include "parser/parsetree.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "utils/bytea.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/* size of the struct header fields (magic, type, ndeps) */
#define SizeOfHeader (3 * sizeof(uint32))

/* size of a serialized dependency (degree, natts, atts) */
#define SizeOfItem(natts) (sizeof(double) + sizeof(AttrNumber) * (1 + (natts)))

/* minimal size of a dependency (with two attributes) */
#define MinSizeOfItem SizeOfItem(2)

/* minimal size of dependencies, when all deps are minimal */
#define MinSizeOfItems(ndeps) (SizeOfHeader + (ndeps) * MinSizeOfItem)

/*
 * Internal state for DependencyGenerator of dependencies. Dependencies are
 * similar to k-permutations of n elements, except that the order does not
 * matter for the first (k-1) elements. That is, (a,b=>c) and (b,a=>c) are
 * equivalent.
 */
typedef struct DependencyGeneratorData
{
  int k;                    /* size of the dependency */
  int n;                    /* number of possible attributes */
  int current;              /* next dependency to return (index) */
  AttrNumber ndependencies; /* number of dependencies generated */
  AttrNumber *dependencies; /* array of pre-generated dependencies	*/
} DependencyGeneratorData;

typedef DependencyGeneratorData *DependencyGenerator;

static void
generate_dependencies_recurse(DependencyGenerator state, int index, AttrNumber start, AttrNumber *current);
static void
generate_dependencies(DependencyGenerator state);
static DependencyGenerator
DependencyGenerator_init(int n, int k);
static void
DependencyGenerator_free(DependencyGenerator state);
static AttrNumber *
DependencyGenerator_next(DependencyGenerator state);
static double
dependency_degree(int numrows, HeapTuple *rows, int k, AttrNumber *dependency, VacAttrStats **stats, Bitmapset *attrs);
static bool
dependency_is_fully_matched(MVDependency *dependency, Bitmapset *attnums);
static bool
dependency_implies_attribute(MVDependency *dependency, AttrNumber attnum);
static bool
dependency_is_compatible_clause(Node *clause, Index relid, AttrNumber *attnum);
static MVDependency *
find_strongest_dependency(StatisticExtInfo *stats, MVDependencies *dependencies, Bitmapset *attnums);

static void
generate_dependencies_recurse(DependencyGenerator state, int index, AttrNumber start, AttrNumber *current)
{

























































}

/* generate all dependencies (k-permutations of n elements) */
static void
generate_dependencies(DependencyGenerator state)
{





}

/*
 * initialize the DependencyGenerator of variations, and prebuild the variations
 *
 * This pre-builds all the variations. We could also generate them in
 * DependencyGenerator_next(), but this seems simpler.
 */
static DependencyGenerator
DependencyGenerator_init(int n, int k)
{

















}

/* free the DependencyGenerator state */
static void
DependencyGenerator_free(DependencyGenerator state)
{


}

/* generate next combination */
static AttrNumber *
DependencyGenerator_next(DependencyGenerator state)
{






}

/*
 * validates functional dependency on the data
 *
 * An actual work horse of detecting functional dependencies. Given a variation
 * of k attributes, it checks that the first (k-1) are sufficient to determine
 * the last one.
 */
static double
dependency_degree(int numrows, HeapTuple *rows, int k, AttrNumber *dependency, VacAttrStats **stats, Bitmapset *attrs)
{




















































































































}

/*
 * detects functional dependencies between groups of columns
 *
 * Generates all possible subsets of columns (variations) and computes
 * the degree of validity for each one. For example when creating statistics
 * on three columns (a,b,c) there are 9 possible dependencies
 *
 *	   two columns			  three columns
 *	   -----------			  -------------
 *	   (a) -> b				  (a,b) -> c
 *	   (a) -> c				  (a,c) -> b
 *	   (b) -> a				  (b,c) -> a
 *	   (b) -> c
 *	   (c) -> a
 *	   (c) -> b
 */
MVDependencies *
statext_dependencies_build(int numrows, HeapTuple *rows, Bitmapset *attrs, VacAttrStats **stats)
{



























































































}

/*
 * Serialize list of dependencies into a bytea value.
 */
bytea *
statext_dependencies_serialize(MVDependencies *dependencies)
{

















































}

/*
 * Reads serialized dependencies into MVDependencies structure.
 */
MVDependencies *
statext_dependencies_deserialize(bytea *data)
{




























































































}

/*
 * dependency_is_fully_matched
 *		checks that a functional dependency is fully matched given
 *clauses on attributes (assuming the clauses are suitable equality clauses)
 */
static bool
dependency_is_fully_matched(MVDependency *dependency, Bitmapset *attnums)
{

















}

/*
 * dependency_implies_attribute
 *		check that the attnum matches is implied by the functional
 *dependency
 */
static bool
dependency_implies_attribute(MVDependency *dependency, AttrNumber attnum)
{






}

/*
 * statext_dependencies_load
 *		Load the functional dependencies for the indicated
 *pg_statistic_ext tuple
 */
MVDependencies *
statext_dependencies_load(Oid mvoid)
{






















}

/*
 * pg_dependencies_in		- input routine for type pg_dependencies.
 *
 * pg_dependencies is real enough to be a table column, but it has no operations
 * of its own, and disallows input too
 */
Datum
pg_dependencies_in(PG_FUNCTION_ARGS)
{







}

/*
 * pg_dependencies		- output routine for type pg_dependencies.
 */
Datum
pg_dependencies_out(PG_FUNCTION_ARGS)
{





































}

/*
 * pg_dependencies_recv		- binary input routine for type pg_dependencies.
 */
Datum
pg_dependencies_recv(PG_FUNCTION_ARGS)
{



}

/*
 * pg_dependencies_send		- binary output routine for type
 * pg_dependencies.
 *
 * Functional dependencies are serialized in a bytea value (although the type
 * is named differently), so let's just send that.
 */
Datum
pg_dependencies_send(PG_FUNCTION_ARGS)
{

}

/*
 * dependency_is_compatible_clause
 *		Determines if the clause is compatible with functional
 *dependencies
 *
 * Only clauses that have the form of equality to a pseudoconstant, or can be
 * interpreted that way, are currently accepted.  Furthermore the variable
 * part of the clause must be a simple Var belonging to the specified
 * relation, whose attribute number we return in *attnum on success.
 */
static bool
dependency_is_compatible_clause(Node *clause, Index relid, AttrNumber *attnum)
{




















































































































}

/*
 * find_strongest_dependency
 *		find the strongest dependency on the attributes
 *
 * When applying functional dependencies, we start with the strongest
 * dependencies. That is, we select the dependency that:
 *
 * (a) has all attributes covered by equality clauses
 *
 * (b) has the most attributes
 *
 * (c) has the highest degree of validity
 *
 * This guarantees that we eliminate the most redundant conditions first
 * (see the comment in dependencies_clauselist_selectivity).
 */
static MVDependency *
find_strongest_dependency(StatisticExtInfo *stats, MVDependencies *dependencies, Bitmapset *attnums)
{



















































}

/*
 * dependencies_clauselist_selectivity
 *		Return the estimated selectivity of (a subset of) the given
 *clauses using functional dependency statistics, or 1.0 if no useful functional
 *		dependency statistic exists.
 *
 * 'estimatedclauses' is an input/output argument that gets a bit set
 * corresponding to the (zero-based) list index of each clause that is included
 * in the estimated selectivity.
 *
 * Given equality clauses on attributes (a,b) we find the strongest dependency
 * between them, i.e. either (a=>b) or (b=>a). Assuming (a=>b) is the selected
 * dependency, we then combine the per-clause selectivities using the formula
 *
 *	   P(a,b) = P(a) * [f + (1-f)*P(b)]
 *
 * where 'f' is the degree of the dependency.
 *
 * With clauses on more than two attributes, the dependencies are applied
 * recursively, starting with the widest/strongest dependencies. For example
 * P(a,b,c) is first split like this:
 *
 *	   P(a,b,c) = P(a,b) * [f + (1-f)*P(c)]
 *
 * assuming (a,b=>c) is the strongest dependency.
 */
Selectivity
dependencies_clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Bitmapset **estimatedclauses)
{











































































































































































}