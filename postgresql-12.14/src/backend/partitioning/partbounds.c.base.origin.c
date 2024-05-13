/*-------------------------------------------------------------------------
 *
 * partbounds.c
 *		Support routines for manipulating partition bounds
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		  src/backend/partitioning/partbounds.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/relation.h"
#include "access/table.h"
#include "access/tableam.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "commands/tablecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "partitioning/partprune.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/ruleutils.h"
#include "utils/syscache.h"

/*
 * When qsort'ing partition bounds after reading from the catalog, each bound
 * is represented with one of the following structs.
 */

/* One bound of a hash partition */
typedef struct PartitionHashBound
{
  int modulus;
  int remainder;
  int index;
} PartitionHashBound;

/* One value coming from some (index'th) list partition */
typedef struct PartitionListValue
{
  int index;
  Datum value;
} PartitionListValue;

/* One bound of a range partition */
typedef struct PartitionRangeBound
{
  int index;
  Datum *datums;                 /* range bound datums */
  PartitionRangeDatumKind *kind; /* the kind of each datum */
  bool lower;                    /* this is the lower (vs upper) bound */
} PartitionRangeBound;

static int32
qsort_partition_hbound_cmp(const void *a, const void *b);
static int32
qsort_partition_list_value_cmp(const void *a, const void *b, void *arg);
static int32
qsort_partition_rbound_cmp(const void *a, const void *b, void *arg);
static PartitionBoundInfo
create_hash_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping);
static PartitionBoundInfo
create_list_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping);
static PartitionBoundInfo
create_range_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping);
static PartitionRangeBound *
make_one_partition_rbound(PartitionKey key, int index, List *datums, bool lower);
static int32
partition_hbound_cmp(int modulus1, int remainder1, int modulus2, int remainder2);
static int32
partition_rbound_cmp(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, Datum *datums1, PartitionRangeDatumKind *kind1, bool lower1, PartitionRangeBound *b2);
static int
partition_range_bsearch(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, PartitionRangeBound *probe, bool *is_equal);
static Expr *
make_partition_op_expr(PartitionKey key, int keynum, uint16 strategy, Expr *arg1, Expr *arg2);
static Oid
get_partition_operator(PartitionKey key, int col, StrategyNumber strategy, bool *need_relabel);
static List *
get_qual_for_hash(Relation parent, PartitionBoundSpec *spec);
static List *
get_qual_for_list(Relation parent, PartitionBoundSpec *spec);
static List *
get_qual_for_range(Relation parent, PartitionBoundSpec *spec, bool for_default);
static void
get_range_key_properties(PartitionKey key, int keynum, PartitionRangeDatum *ldatum, PartitionRangeDatum *udatum, ListCell **partexprs_item, Expr **keyCol, Const **lower_val, Const **upper_val);
static List *
get_range_nulltest(PartitionKey key);

/*
 * get_qual_from_partbound
 *		Given a parser node for partition bound, return the list of
 *executable expressions as partition constraint
 */
List *
get_qual_from_partbound(Relation rel, Relation parent, PartitionBoundSpec *spec)
{



























}

/*
 *	partition_bounds_create
 *		Build a PartitionBoundInfo struct from a list of
 *PartitionBoundSpec nodes
 *
 * This function creates a PartitionBoundInfo and fills the values of its
 * various members based on the input list.  Importantly, 'datums' array will
 * contain Datum representation of individual bounds (possibly after
 * de-duplication as in case of range bounds), sorted in a canonical order
 * defined by qsort_partition_* functions of respective partitioning methods.
 * 'indexes' array will contain as many elements as there are bounds (specific
 * exceptions to this rule are listed in the function body), which represent
 * the 0-based canonical positions of partitions.
 *
 * Upon return from this function, *mapping is set to an array of
 * list_length(boundspecs) elements, each of which maps the original index of
 * a partition to its canonical index.
 *
 * Note: The objects returned by this function are wholly allocated in the
 * current memory context.
 */
PartitionBoundInfo
partition_bounds_create(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{












































}

/*
 * create_hash_bounds
 *		Create a PartitionBoundInfo for a hash partitioned table
 */
static PartitionBoundInfo
create_hash_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{










































































}

/*
 * create_list_bounds
 *		Create a PartitionBoundInfo for a list partitioned table
 */
static PartitionBoundInfo
create_list_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{























































































































































}

/*
 * create_range_bounds
 *		Create a PartitionBoundInfo for a range partitioned table
 */
static PartitionBoundInfo
create_range_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{




















































































































































































}

/*
 * Are two partition bound collections logically equal?
 *
 * Used in the keep logic of relcache.c (ie, in RelationClearRelation()).
 * This is also useful when b1 and b2 are bound collections of two separate
 * relations, respectively, because PartitionBoundInfo is a canonical
 * representation of partition bounds.
 */
bool
partition_bounds_equal(int partnatts, int16 *parttyplen, bool *parttypbyval, PartitionBoundInfo b1, PartitionBoundInfo b2)
{













































































































}

/*
 * Return a copy of given PartitionBoundInfo structure. The data types of bounds
 * are described by given partition key specification.
 */
PartitionBoundInfo
partition_bounds_copy(PartitionBoundInfo src, PartitionKey key)
{












































































}

/*
 * partitions_are_ordered
 *		Determine whether the partitions described by 'boundinfo' are
 *ordered, that is partitions appearing earlier in the PartitionDesc sequence
 *		contain partition keys strictly less than those appearing later.
 *		Also, if NULL values are possible, they must come in the last
 *		partition defined in the PartitionDesc.
 *
 * If out of order, or there is insufficient info to know the order,
 * then we return false.
 */
bool
partitions_are_ordered(PartitionBoundInfo boundinfo, int nparts)
{





















































}

/*
 * check_new_partition_bound
 *
 * Checks if the new partition's bound overlaps any of the existing partitions
 * of parent.  Also performs additional checks as necessary per strategy.
 */
void
check_new_partition_bound(char *relname, Relation parent, PartitionBoundSpec *spec)
{



















































































































































































































































}

/*
 * check_default_partition_contents
 *
 * This function checks if there exists a row in the default partition that
 * would properly belong to the new partition being added.  If it finds one,
 * it throws an error.
 */
void
check_default_partition_contents(Relation parent, Relation default_rel, PartitionBoundSpec *new_spec)
{













































































































































}

/*
 * get_hash_partition_greatest_modulus
 *
 * Returns the greatest modulus of the hash partition bound.
 * This is no longer used in the core code, but we keep it around
 * in case external modules are using it.
 */
int
get_hash_partition_greatest_modulus(PartitionBoundInfo bound)
{


}

/*
 * make_one_partition_rbound
 *
 * Return a PartitionRangeBound given a list of PartitionRangeDatum elements
 * and a flag telling whether the bound is lower or not.  Made into a function
 * because there are multiple sites that want to use this facility.
 */
static PartitionRangeBound *
make_one_partition_rbound(PartitionKey key, int index, List *datums, bool lower)
{



































}

/*
 * partition_rbound_cmp
 *
 * Return for two range bounds whether the 1st one (specified in datums1,
 * kind1, and lower1) is <, =, or > the bound specified in *b2.
 *
 * partnatts, partsupfunc and partcollation give the number of attributes in the
 * bounds to be compared, comparison function to be used and the collations of
 * attributes, respectively.
 *
 * Note that if the values of the two range bounds compare equal, then we take
 * into account whether they are upper or lower bounds, and an upper bound is
 * considered to be smaller than a lower bound. This is important to the way
 * that RelationBuildPartitionDesc() builds the PartitionBoundInfoData
 * structure, which only stores the upper bound of a common boundary between
 * two contiguous partitions.
 */
static int32
partition_rbound_cmp(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, Datum *datums1, PartitionRangeDatumKind *kind1, bool lower1, PartitionRangeBound *b2)
{




















































}

/*
 * partition_rbound_datum_cmp
 *
 * Return whether range bound (specified in rb_datums, rb_kind, and rb_lower)
 * is <, =, or > partition key of tuple (tuple_datums)
 *
 * n_tuple_datums, partsupfunc and partcollation give number of attributes in
 * the bounds to be compared, comparison function to be used and the collations
 * of attributes resp.
 *
 */
int32
partition_rbound_datum_cmp(FmgrInfo *partsupfunc, Oid *partcollation, Datum *rb_datums, PartitionRangeDatumKind *rb_kind, Datum *tuple_datums, int n_tuple_datums)
{






















}

/*
 * partition_hbound_cmp
 *
 * Compares modulus first, then remainder if modulus is equal.
 */
static int32
partition_hbound_cmp(int modulus1, int remainder1, int modulus2, int remainder2)
{













}

/*
 * partition_list_bsearch
 *		Returns the index of the greatest bound datum that is less than
 *equal to the given value or -1 if all of the bound datums are greater
 *
 * *is_equal is set to true if the bound datum at the returned index is equal
 * to the input value.
 */
int
partition_list_bsearch(FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, Datum value, bool *is_equal)
{


























}

/*
 * partition_range_bsearch
 *		Returns the index of the greatest range bound that is less than
 *or equal to the given range bound or -1 if all of the range bounds are greater
 *
 * *is_equal is set to true if the range bound at the returned index is equal
 * to the input range bound
 */
static int
partition_range_bsearch(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, PartitionRangeBound *probe, bool *is_equal)
{



























}

/*
 * partition_range_bsearch
 *		Returns the index of the greatest range bound that is less than
 *or equal to the given tuple or -1 if all of the range bounds are greater
 *
 * *is_equal is set to true if the range bound at the returned index is equal
 * to the input tuple.
 */
int
partition_range_datum_bsearch(FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, int nvalues, Datum *values, bool *is_equal)
{



























}

/*
 * partition_hash_bsearch
 *		Returns the index of the greatest (modulus, remainder) pair that
 *is less than or equal to the given (modulus, remainder) pair or -1 if all of
 *them are greater
 */
int
partition_hash_bsearch(PartitionBoundInfo boundinfo, int modulus, int remainder)
{




























}

/*
 * qsort_partition_hbound_cmp
 *
 * Hash bounds are sorted by modulus, then by remainder.
 */
static int32
qsort_partition_hbound_cmp(const void *a, const void *b)
{




}

/*
 * qsort_partition_list_value_cmp
 *
 * Compare two list partition bound datums.
 */
static int32
qsort_partition_list_value_cmp(const void *a, const void *b, void *arg)
{




}

/*
 * qsort_partition_rbound_cmp
 *
 * Used when sorting range bounds across all range partitions.
 */
static int32
qsort_partition_rbound_cmp(const void *a, const void *b, void *arg)
{





}

/*
 * get_partition_operator
 *
 * Return oid of the operator of the given strategy for the given partition
 * key column.  It is assumed that the partitioning key is of the same type as
 * the chosen partitioning opclass, or at least binary-compatible.  In the
 * latter case, *need_relabel is set to true if the opclass is not of a
 * polymorphic type (indicating a RelabelType node needed on top), otherwise
 * false.
 */
static Oid
get_partition_operator(PartitionKey key, int col, StrategyNumber strategy, bool *need_relabel)
{




















}

/*
 * make_partition_op_expr
 *		Returns an Expr for the given partition key column with arg1 and
 *		arg2 as its leftop and rightop, respectively
 */
static Expr *
make_partition_op_expr(PartitionKey key, int keynum, uint16 strategy, Expr *arg1, Expr *arg2)
{

















































































}

/*
 * get_qual_for_hash
 *
 * Returns a CHECK constraint expression to use as a hash partition's
 * constraint, given the parent relation and partition bound structure.
 *
 * The partition constraint for a hash partition is always a call to the
 * built-in function satisfies_hash_partition().
 */
static List *
get_qual_for_hash(Relation parent, PartitionBoundSpec *spec)
{









































}

/*
 * get_qual_for_list
 *
 * Returns an implicit-AND list of expressions to use as a list partition's
 * constraint, given the parent relation and partition bound structure.
 *
 * The function returns NIL for a default partition when it's the only
 * partition since in that case there is no constraint.
 */
static List *
get_qual_for_list(Relation parent, PartitionBoundSpec *spec)
{

































































































































































}

/*
 * get_qual_for_range
 *
 * Returns an implicit-AND list of expressions to use as a range partition's
 * constraint, given the parent relation and partition bound structure.
 *
 * For a multi-column range partition key, say (a, b, c), with (al, bl, cl)
 * as the lower bound tuple and (au, bu, cu) as the upper bound tuple, we
 * generate an expression tree of the following form:
 *
 *	(a IS NOT NULL) and (b IS NOT NULL) and (c IS NOT NULL)
 *		AND
 *	(a > al OR (a = al AND b > bl) OR (a = al AND b = bl AND c >= cl))
 *		AND
 *	(a < au OR (a = au AND b < bu) OR (a = au AND b = bu AND c < cu))
 *
 * It is often the case that a prefix of lower and upper bound tuples contains
 * the same values, for example, (al = au), in which case, we will emit an
 * expression tree of the following form:
 *
 *	(a IS NOT NULL) and (b IS NOT NULL) and (c IS NOT NULL)
 *		AND
 *	(a = al)
 *		AND
 *	(b > bl OR (b = bl AND c >= cl))
 *		AND
 *	(b < bu OR (b = bu AND c < cu))
 *
 * If a bound datum is either MINVALUE or MAXVALUE, these expressions are
 * simplified using the fact that any value is greater than MINVALUE and less
 * than MAXVALUE. So, for example, if cu = MAXVALUE, c < cu is automatically
 * true, and we need not emit any expression for it, and the last line becomes
 *
 *	(b < bu) OR (b = bu), which is simplified to (b <= bu)
 *
 * In most common cases with only one partition column, say a, the following
 * expression tree will be generated: a IS NOT NULL AND a >= al AND a < au
 *
 * For default partition, it returns the negation of the constraints of all
 * the other partitions.
 *
 * External callers should pass for_default as false; we set it to true only
 * when recursing.
 */
static List *
get_qual_for_range(Relation parent, PartitionBoundSpec *spec, bool for_default)
{



































































































































































































































































































































}

/*
 * get_range_key_properties
 *		Returns range partition key information for a given column
 *
 * This is a subroutine for get_qual_for_range, and its API is pretty
 * specialized to that caller.
 *
 * Constructs an Expr for the key column (returned in *keyCol) and Consts
 * for the lower and upper range limits (returned in *lower_val and
 * *upper_val).  For MINVALUE/MAXVALUE limits, NULL is returned instead of
 * a Const.  All of these structures are freshly palloc'd.
 *
 * *partexprs_item points to the cell containing the next expression in
 * the key->partexprs list, or NULL.  It may be advanced upon return.
 */
static void
get_range_key_properties(PartitionKey key, int keynum, PartitionRangeDatum *ldatum, PartitionRangeDatum *udatum, ListCell **partexprs_item, Expr **keyCol, Const **lower_val, Const **upper_val)
{

































}

/*
 * get_range_nulltest
 *
 * A non-default range partition table does not currently allow partition
 * keys to be null, so emit an IS NOT NULL expression for each key column.
 */
static List *
get_range_nulltest(PartitionKey key)
{

































}

/*
 * compute_partition_hash_value
 *
 * Compute the hash value for given partition key values.
 */
uint64
compute_partition_hash_value(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, Datum *values, bool *isnull)
{


























}

/*
 * satisfies_hash_partition
 *
 * This is an SQL-callable function for use in hash partition constraints.
 * The first three arguments are the parent table OID, modulus, and remainder.
 * The remaining arguments are the value of the partitioning columns (or
 * expressions); these are hashed and the results are combined into a single
 * hash value by calling hash_combine64.
 *
 * Returns true if remainder produced when this computed single hash value is
 * divided by the given modulus is equal to given remainder, otherwise false.
 * NB: it's important that this never return null, as the constraint machinery
 * would consider that to be a "pass".
 *
 * See get_qual_for_hash() for usage.
 */
Datum
satisfies_hash_partition(PG_FUNCTION_ARGS)
{






















































































































































































}