/*-------------------------------------------------------------------------
 *
 * rangetypes.c
 *	  I/O functions, operators, and support functions for range types.
 *
 * The stored (serialized) format of a range value is:
 *
 *	4 bytes: varlena header
 *	4 bytes: range type's OID
 *	Lower boundary value, if any, aligned according to subtype's typalign
 *	Upper boundary value, if any, aligned according to subtype's typalign
 *	1 byte for flags
 *
 * This representation is chosen to avoid needing any padding before the
 * lower boundary value, even when it requires double alignment.  We can
 * expect that the varlena header is presented to us on a suitably aligned
 * boundary (possibly after detoasting), and then the lower boundary is too.
 * Note that this means we can't work with a packed (short varlena header)
 * value; we must detoast it first.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/rangetypes.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/tupmacs.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/hashutils.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"
#include "utils/timestamp.h"

#define RANGE_EMPTY_LITERAL "empty"

/* fn_extra cache entry for one of the range I/O functions */
typedef struct RangeIOData
{
  TypeCacheEntry *typcache; /* range type's typcache entry */
  Oid typiofunc;            /* element type's I/O function */
  Oid typioparam;           /* element type's I/O parameter */
  FmgrInfo proc;            /* lookup result for typiofunc */
} RangeIOData;

static RangeIOData *
get_range_io_data(FunctionCallInfo fcinfo, Oid rngtypid, IOFuncSelector func);
static char
range_parse_flags(const char *flags_str);
static void
range_parse(const char *input_str, char *flags, char **lbound_str, char **ubound_str);
static const char *
range_parse_bound(const char *string, const char *ptr, char **bound_str, bool *infinite);
static char *
range_deparse(char flags, const char *lbound_str, const char *ubound_str);
static char *
range_bound_escape(const char *value);
static Size
datum_compute_size(Size sz, Datum datum, bool typbyval, char typalign, int16 typlen, char typstorage);
static Pointer
datum_write(Pointer ptr, Datum datum, bool typbyval, char typalign, int16 typlen, char typstorage);

/*
 *----------------------------------------------------------
 * I/O FUNCTIONS
 *----------------------------------------------------------
 */

Datum
range_in(PG_FUNCTION_ARGS)
{







































}

Datum
range_out(PG_FUNCTION_ARGS)
{
































}

/*
 * Binary representation: The first byte is the flags, then the lower bound
 * (if present), then the upper bound (if present).  Each bound is represented
 * by a 4-byte length header and the binary representation of that bound (as
 * returned by a call to the send function for the subtype).
 */

Datum
range_recv(PG_FUNCTION_ARGS)
{








































































}

Datum
range_send(PG_FUNCTION_ARGS)
{










































}

/*
 * get_range_io_data: get cached information needed for range type I/O
 *
 * The range I/O functions need a bit more cached info than other range
 * functions, so they store a RangeIOData struct in fn_extra, not just a
 * pointer to a type cache entry.
 */
static RangeIOData *
get_range_io_data(FunctionCallInfo fcinfo, Oid rngtypid, IOFuncSelector func)
{





































}

/*
 *----------------------------------------------------------
 * GENERIC FUNCTIONS
 *----------------------------------------------------------
 */

/* Construct standard-form range value from two arguments */
Datum
range_constructor2(PG_FUNCTION_ARGS)
{























}

/* Construct general range value from three arguments */
Datum
range_constructor3(PG_FUNCTION_ARGS)
{































}

/* range -> subtype functions */

/* extract lower bound value */
Datum
range_lower(PG_FUNCTION_ARGS)
{

















}

/* extract upper bound value */
Datum
range_upper(PG_FUNCTION_ARGS)
{

















}

/* range -> bool functions */

/* is range empty? */
Datum
range_empty(PG_FUNCTION_ARGS)
{




}

/* is lower bound inclusive? */
Datum
range_lower_inc(PG_FUNCTION_ARGS)
{




}

/* is upper bound inclusive? */
Datum
range_upper_inc(PG_FUNCTION_ARGS)
{




}

/* is lower bound infinite? */
Datum
range_lower_inf(PG_FUNCTION_ARGS)
{




}

/* is upper bound infinite? */
Datum
range_upper_inf(PG_FUNCTION_ARGS)
{




}

/* range, element -> bool functions */

/* contains? */
Datum
range_contains_elem(PG_FUNCTION_ARGS)
{







}

/* contained by? */
Datum
elem_contained_by_range(PG_FUNCTION_ARGS)
{







}

/* range, range -> bool functions */

/* equality (internal version) */
bool
range_eq_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{

































}

/* equality */
Datum
range_eq(PG_FUNCTION_ARGS)
{







}

/* inequality (internal version) */
bool
range_ne_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{

}

/* inequality */
Datum
range_ne(PG_FUNCTION_ARGS)
{







}

/* contains? */
Datum
range_contains(PG_FUNCTION_ARGS)
{







}

/* contained by? */
Datum
range_contained_by(PG_FUNCTION_ARGS)
{







}

/* strictly left of? (internal version) */
bool
range_before_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{




















}

/* strictly left of? */
Datum
range_before(PG_FUNCTION_ARGS)
{







}

/* strictly right of? (internal version) */
bool
range_after_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{




















}

/* strictly right of? */
Datum
range_after(PG_FUNCTION_ARGS)
{







}

/*
 * Check if two bounds A and B are "adjacent", where A is an upper bound and B
 * is a lower bound. For the bounds to be adjacent, each subtype value must
 * satisfy strictly one of the bounds: there are no values which satisfy both
 * bounds (i.e. less than A and greater than B); and there are no values which
 * satisfy neither bound (i.e. greater than A and less than B).
 *
 * For discrete ranges, we rely on the canonicalization function to see if A..B
 * normalizes to empty. (If there is no canonicalization function, it's
 * impossible for such a range to normalize to empty, so we needn't bother to
 * try.)
 *
 * If A == B, the ranges are adjacent only if the bounds have different
 * inclusive flags (i.e., exactly one of the ranges includes the common
 * boundary point).
 *
 * And if A > B then the ranges are not adjacent in this order.
 */
bool
bounds_adjacent(TypeCacheEntry *typcache, RangeBound boundA, RangeBound boundB)
{









































}

/* adjacent to (but not overlapping)? (internal version) */
bool
range_adjacent_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
























}

/* adjacent to (but not overlapping)? */
Datum
range_adjacent(PG_FUNCTION_ARGS)
{







}

/* overlaps? (internal version) */
bool
range_overlaps_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{






























}

/* overlaps? */
Datum
range_overlaps(PG_FUNCTION_ARGS)
{







}

/* does not extend to right of? (internal version) */
bool
range_overleft_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{

























}

/* does not extend to right of? */
Datum
range_overleft(PG_FUNCTION_ARGS)
{







}

/* does not extend to left of? (internal version) */
bool
range_overright_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{

























}

/* does not extend to left of? */
Datum
range_overright(PG_FUNCTION_ARGS)
{







}

/* range, range -> range functions */

/* set difference */
Datum
range_minus(PG_FUNCTION_ARGS)
{





























































}

/*
 * Set union.  If strict is true, it is an error that the two input ranges
 * are not adjacent or overlapping.
 */
static RangeType *
range_union_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2, bool strict)
{

















































}

Datum
range_union(PG_FUNCTION_ARGS)
{







}

/*
 * range merge: like set union, except also allow and account for non-adjacent
 * input ranges.
 */
Datum
range_merge(PG_FUNCTION_ARGS)
{







}

/* set intersection */
Datum
range_intersect(PG_FUNCTION_ARGS)
{












































}

/* Btree support */

/* btree comparator */
Datum
range_cmp(PG_FUNCTION_ARGS)
{















































}

/* inequality operators using the range_cmp function */
Datum
range_lt(PG_FUNCTION_ARGS)
{



}

Datum
range_le(PG_FUNCTION_ARGS)
{



}

Datum
range_ge(PG_FUNCTION_ARGS)
{



}

Datum
range_gt(PG_FUNCTION_ARGS)
{



}

/* Hash support */

/* hash a range value */
Datum
hash_range(PG_FUNCTION_ARGS)
{




























































}

/*
 * Returns 64-bit value by hashing a value to a 64-bit value, with a seed.
 * Otherwise, similar to hash_range.
 */
Datum
hash_range_extended(PG_FUNCTION_ARGS)
{






















































}

/*
 *----------------------------------------------------------
 * CANONICAL FUNCTIONS
 *
 *	 Functions for specific built-in range types.
 *----------------------------------------------------------
 */

Datum
int4range_canonical(PG_FUNCTION_ARGS)
{




























}

Datum
int8range_canonical(PG_FUNCTION_ARGS)
{




























}

Datum
daterange_canonical(PG_FUNCTION_ARGS)
{




























}

/*
 *----------------------------------------------------------
 * SUBTYPE_DIFF FUNCTIONS
 *
 * Functions for specific built-in range types.
 *
 * Note that subtype_diff does return the difference, not the absolute value
 * of the difference, and it must take care to avoid overflow.
 * (numrange_subdiff is at some risk there ...)
 *----------------------------------------------------------
 */

Datum
int4range_subdiff(PG_FUNCTION_ARGS)
{




}

Datum
int8range_subdiff(PG_FUNCTION_ARGS)
{




}

Datum
numrange_subdiff(PG_FUNCTION_ARGS)
{










}

Datum
daterange_subdiff(PG_FUNCTION_ARGS)
{




}

Datum
tsrange_subdiff(PG_FUNCTION_ARGS)
{






}

Datum
tstzrange_subdiff(PG_FUNCTION_ARGS)
{






}

/*
 *----------------------------------------------------------
 * SUPPORT FUNCTIONS
 *
 *	 These functions aren't in pg_proc, but are useful for
 *	 defining new generic range functions in C.
 *----------------------------------------------------------
 */

/*
 * range_get_typcache: get cached information about a range type
 *
 * This is for use by range-related functions that follow the convention
 * of using the fn_extra field as a pointer to the type cache entry for
 * the range type.  Functions that need to cache more information than
 * that must fend for themselves.
 */
TypeCacheEntry *
range_get_typcache(FunctionCallInfo fcinfo, Oid rngtypid)
{













}

/*
 * range_serialize: construct a range value from bounds and empty-flag
 *
 * This does not force canonicalization of the range value.  In most cases,
 * external callers should only be canonicalization functions.  Note that
 * we perform some datatype-independent canonicalization checks anyway.
 */
RangeType *
range_serialize(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, bool empty)
{






























































































































}

/*
 * range_deserialize: deconstruct a range value
 *
 * NB: the given range object must be fully detoasted; it cannot have a
 * short varlena header.
 *
 * Note that if the element type is pass-by-reference, the datums in the
 * RangeBound structs will be pointers into the given range object.
 */
void
range_deserialize(TypeCacheEntry *typcache, RangeType *range, RangeBound *lower, RangeBound *upper, bool *empty)
{



























































}

/*
 * range_get_flags: just get the flags from a RangeType value.
 *
 * This is frequently useful in places that only need the flags and not
 * the full results of range_deserialize.
 */
char
range_get_flags(RangeType *range)
{


}

/*
 * range_set_contain_empty: set the RANGE_CONTAIN_EMPTY bit in the value.
 *
 * This is only needed in GiST operations, so we don't include a provision
 * for setting it in range_serialize; rather, this function must be applied
 * afterwards.
 */
void
range_set_contain_empty(RangeType *range)
{






}

/*
 * This both serializes and canonicalizes (if applicable) the range.
 * This should be used by most callers.
 */
RangeType *
make_range(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, bool empty)
{











}

/*
 * Compare two range boundary points, returning <0, 0, or >0 according to
 * whether b1 is less than, equal to, or greater than b2.
 *
 * The boundaries can be any combination of upper and lower; so it's useful
 * for a variety of operators.
 *
 * The simple case is when b1 and b2 are both finite and inclusive, in which
 * case the result is just a comparison of the values held in b1 and b2.
 *
 * If a bound is exclusive, then we need to know whether it's a lower bound,
 * in which case we treat the boundary point as "just greater than" the held
 * value; or an upper bound, in which case we treat the boundary point as
 * "just less than" the held value.
 *
 * If a bound is infinite, it represents minus infinity (less than every other
 * point) if it's a lower bound; or plus infinity (greater than every other
 * point) if it's an upper bound.
 *
 * There is only one case where two boundaries compare equal but are not
 * identical: when both bounds are inclusive and hold the same finite value,
 * but one is an upper bound and the other a lower bound.
 */
int
range_cmp_bounds(TypeCacheEntry *typcache, RangeBound *b1, RangeBound *b2)
{










































































}

/*
 * Compare two range boundary point values, returning <0, 0, or >0 according
 * to whether b1 is less than, equal to, or greater than b2.
 *
 * This is similar to but simpler than range_cmp_bounds().  We just compare
 * the values held in b1 and b2, ignoring inclusive/exclusive flags.  The
 * lower/upper flags only matter for infinities, where they tell us if the
 * infinity is plus or minus.
 */
int
range_cmp_bound_values(TypeCacheEntry *typcache, RangeBound *b1, RangeBound *b2)
{
































}

/*
 * Build an empty range value of the type indicated by the typcache entry.
 */
RangeType *
make_empty_range(TypeCacheEntry *typcache)
{














}

/*
 *----------------------------------------------------------
 * STATIC FUNCTIONS
 *----------------------------------------------------------
 */

/*
 * Given a string representing the flags for the range type, return the flags
 * represented as a char.
 */
static char
range_parse_flags(const char *flags_str)
{






























}

/*
 * Parse range input.
 *
 * Input parameters:
 *	string: input string to be parsed
 * Output parameters:
 *	*flags: receives flags bitmask
 *	*lbound_str: receives palloc'd lower bound string, or NULL if none
 *	*ubound_str: receives palloc'd upper bound string, or NULL if none
 *
 * This is modeled somewhat after record_in in rowtypes.c.
 * The input syntax is:
 *	<range>   := EMPTY
 *			   | <lb-inc> <string>, <string> <ub-inc>
 *	<lb-inc>  := '[' | '('
 *	<ub-inc>  := ']' | ')'
 *
 * Whitespace before or after <range> is ignored.  Whitespace within a <string>
 * is taken literally and becomes part of the input string for that bound.
 *
 * A <string> of length zero is taken as "infinite" (i.e. no bound), unless it
 * is surrounded by double-quotes, in which case it is the literal empty
 * string.
 *
 * Within a <string>, special characters (such as comma, parenthesis, or
 * brackets) can be enclosed in double-quotes or escaped with backslash. Within
 * double-quotes, a double-quote can be escaped with double-quote or backslash.
 */
static void
range_parse(const char *string, char *flags, char **lbound_str, char **ubound_str)
{






























































































}

/*
 * Helper for range_parse: parse and de-quote one bound string.
 *
 * We scan until finding comma, right parenthesis, or right bracket.
 *
 * Input parameters:
 *	string: entire input string (used only for error reports)
 *	ptr: where to start parsing bound
 * Output parameters:
 *	*bound_str: receives palloc'd bound string, or NULL if none
 *	*infinite: set true if no bound, else false
 *
 * The return value is the scan ptr, advanced past the bound string.
 */
static const char *
range_parse_bound(const char *string, const char *ptr, char **bound_str, bool *infinite)
{

























































}

/*
 * Convert a deserialized range value to text form
 *
 * Inputs are the flags byte, and the two bound values already converted to
 * text (but not yet quoted).  If no bound value, pass NULL.
 *
 * Result is a palloc'd string
 */
static char *
range_deparse(char flags, const char *lbound_str, const char *ubound_str)
{


























}

/*
 * Helper for range_deparse: quote a bound value as needed
 *
 * Result is a palloc'd string
 */
static char *
range_bound_escape(const char *value)
{








































}

/*
 * Test whether range r1 contains range r2.
 *
 * Caller has already checked that they are the same range type, and looked up
 * the necessary typcache entry.
 */
bool
range_contains_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{





































}

bool
range_contained_by_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{

}

/*
 * Test whether range r contains a specific element value.
 */
bool
range_contains_elem_internal(TypeCacheEntry *typcache, RangeType *r, Datum val)
{







































}

/*
 * datum_compute_size() and datum_write() are used to insert the bound
 * values into a range object.  They are modeled after heaptuple.c's
 * heap_compute_data_size() and heap_fill_tuple(), but we need not handle
 * null values here.  TYPE_IS_PACKABLE must test the same conditions as
 * heaptuple.c's ATT_IS_PACKABLE macro.
 */

/* Does datatype allow packing into the 1-byte-header varlena format? */
#define TYPE_IS_PACKABLE(typlen, typstorage) ((typlen) == -1 && (typstorage) != 'p')

/*
 * Increment data_length by the space needed by the datum, including any
 * preceding alignment padding.
 */
static Size
datum_compute_size(Size data_length, Datum val, bool typbyval, char typalign, int16 typlen, char typstorage)
{















}

/*
 * Write the given datum beginning at ptr (after advancing to correct
 * alignment, if needed).  Return the pointer incremented by space used.
 */
static Pointer
datum_write(Pointer ptr, Datum datum, bool typbyval, char typalign, int16 typlen, char typstorage)
{































































}