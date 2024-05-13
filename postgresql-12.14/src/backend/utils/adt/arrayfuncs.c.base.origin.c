/*-------------------------------------------------------------------------
 *
 * arrayfuncs.c
 *	  Support functions for arrays.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/arrayfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/optimizer.h"
#include "utils/array.h"
#include "utils/arrayaccess.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/selfuncs.h"
#include "utils/typcache.h"

/*
 * GUC parameter
 */
bool Array_nulls = true;

/*
 * Local definitions
 */
#define ASSGN "="

#define AARR_FREE_IF_COPY(array, n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!VARATT_IS_EXPANDED_HEADER(array))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      PG_FREE_IF_COPY(array, n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

typedef enum
{
  ARRAY_NO_LEVEL,
  ARRAY_LEVEL_STARTED,
  ARRAY_ELEM_STARTED,
  ARRAY_ELEM_COMPLETED,
  ARRAY_QUOTED_ELEM_STARTED,
  ARRAY_QUOTED_ELEM_COMPLETED,
  ARRAY_ELEM_DELIMITED,
  ARRAY_LEVEL_COMPLETED,
  ARRAY_LEVEL_DELIMITED
} ArrayParseState;

/* Working state for array_iterate() */
typedef struct ArrayIteratorData
{
  /* basic info about the array, set up during array_create_iterator() */
  ArrayType *arr;    /* array we're iterating through */
  bits8 *nullbitmap; /* its null bitmap, if any */
  int nitems;        /* total number of elements in array */
  int16 typlen;      /* element type's length */
  bool typbyval;     /* element type's byval property */
  char typalign;     /* element type's align property */

  /* information about the requested slice size */
  int slice_ndim;      /* slice dimension, or 0 if not slicing */
  int slice_len;       /* number of elements per slice */
  int *slice_dims;     /* slice dims array */
  int *slice_lbound;   /* slice lbound array */
  Datum *slice_values; /* workspace of length slice_len */
  bool *slice_nulls;   /* workspace of length slice_len */

  /* current position information, updated on each iteration */
  char *data_ptr;   /* our current position in the array */
  int current_item; /* the item # we're at in the array */
} ArrayIteratorData;

static bool
array_isspace(char ch);
static int
ArrayCount(const char *str, int *dim, char typdelim);
static void
ReadArrayStr(char *arrayStr, const char *origStr, int nitems, int ndim, int *dim, FmgrInfo *inputproc, Oid typioparam, int32 typmod, char typdelim, int typlen, bool typbyval, char typalign, Datum *values, bool *nulls, bool *hasnulls, int32 *nbytes);
static void
ReadArrayBinary(StringInfo buf, int nitems, FmgrInfo *receiveproc, Oid typioparam, int32 typmod, int typlen, bool typbyval, char typalign, Datum *values, bool *nulls, bool *hasnulls, int32 *nbytes);
static Datum
array_get_element_expanded(Datum arraydatum, int nSubscripts, int *indx, int arraytyplen, int elmlen, bool elmbyval, char elmalign, bool *isNull);
static Datum
array_set_element_expanded(Datum arraydatum, int nSubscripts, int *indx, Datum dataValue, bool isNull, int arraytyplen, int elmlen, bool elmbyval, char elmalign);
static bool
array_get_isnull(const bits8 *nullbitmap, int offset);
static void
array_set_isnull(bits8 *nullbitmap, int offset, bool isNull);
static Datum
ArrayCast(char *value, bool byval, int len);
static int
ArrayCastAndSet(Datum src, int typlen, bool typbyval, char typalign, char *dest);
static char *
array_seek(char *ptr, int offset, bits8 *nullbitmap, int nitems, int typlen, bool typbyval, char typalign);
static int
array_nelems_size(char *ptr, int offset, bits8 *nullbitmap, int nitems, int typlen, bool typbyval, char typalign);
static int
array_copy(char *destptr, int nitems, char *srcptr, int offset, bits8 *nullbitmap, int typlen, bool typbyval, char typalign);
static int
array_slice_size(char *arraydataptr, bits8 *arraynullsptr, int ndim, int *dim, int *lb, int *st, int *endp, int typlen, bool typbyval, char typalign);
static void
array_extract_slice(ArrayType *newarray, int ndim, int *dim, int *lb, char *arraydataptr, bits8 *arraynullsptr, int *st, int *endp, int typlen, bool typbyval, char typalign);
static void
array_insert_slice(ArrayType *destArray, ArrayType *origArray, ArrayType *srcArray, int ndim, int *dim, int *lb, int *st, int *endp, int typlen, bool typbyval, char typalign);
static int
array_cmp(FunctionCallInfo fcinfo);
static ArrayType *
create_array_envelope(int ndims, int *dimv, int *lbv, int nbytes, Oid elmtype, int dataoffset);
static ArrayType *
array_fill_internal(ArrayType *dims, ArrayType *lbs, Datum value, bool isnull, Oid elmtype, FunctionCallInfo fcinfo);
static ArrayType *
array_replace_internal(ArrayType *array, Datum search, bool search_isnull, Datum replace, bool replace_isnull, bool remove, Oid collation, FunctionCallInfo fcinfo);
static int
width_bucket_array_float8(Datum operand, ArrayType *thresholds);
static int
width_bucket_array_fixed(Datum operand, ArrayType *thresholds, Oid collation, TypeCacheEntry *typentry);
static int
width_bucket_array_variable(Datum operand, ArrayType *thresholds, Oid collation, TypeCacheEntry *typentry);

/*
 * array_in :
 *		  converts an array from the external format in "string" to
 *		  its internal format.
 *
 * return value :
 *		  the internal representation of the input array
 */
Datum
array_in(PG_FUNCTION_ARGS)
{




































































































































































































































}

/*
 * array_isspace() --- a non-locale-dependent isspace()
 *
 * We used to use isspace() for parsing array values, but that has
 * undesirable results: an array value might be silently interpreted
 * differently depending on the locale setting.  Now we just hard-wire
 * the traditional ASCII definition of isspace().
 */
static bool
array_isspace(char ch)
{





}

/*
 * ArrayCount
 *	 Determines the dimensions for an array string.
 *
 * Returns number of dimensions as function result.  The axis lengths are
 * returned in dim[], which must be of size MAXDIM.
 */
static int
ArrayCount(const char *str, int *dim, char typdelim)
{






















































































































































































































}

/*
 * ReadArrayStr :
 *	 parses the array string pointed to by "arrayStr" and converts the
 *values to internal format.  Unspecified elements are initialized to nulls. The
 *array dimensions must already have been determined.
 *
 * Inputs:
 *	arrayStr: the string to parse.
 *			  CAUTION: the contents of "arrayStr" will be modified!
 *	origStr: the unmodified input string, used only in error messages.
 *	nitems: total number of array elements, as already determined.
 *	ndim: number of array dimensions
 *	dim[]: array axis lengths
 *	inputproc: type-specific input procedure for element datatype.
 *	typioparam, typmod: auxiliary values to pass to inputproc.
 *	typdelim: the value delimiter (type-specific).
 *	typlen, typbyval, typalign: storage parameters of element datatype.
 *
 * Outputs:
 *	values[]: filled with converted data values.
 *	nulls[]: filled with is-null markers.
 *	*hasnulls: set true iff there are any null elements.
 *	*nbytes: set to total size of data area needed (including alignment
 *		padding but not including array header overhead).
 *
 * Note that values[] and nulls[] are allocated by the caller, and must have
 * nitems elements.
 */
static void
ReadArrayStr(char *arrayStr, const char *origStr, int nitems, int ndim, int *dim, FmgrInfo *inputproc, Oid typioparam, int32 typmod, char typdelim, int typlen, bool typbyval, char typalign, Datum *values, bool *nulls, bool *hasnulls, int32 *nbytes)
{






















































































































































































































}

/*
 * Copy data into an array object from a temporary array of Datums.
 *
 * array: array object (with header fields already filled in)
 * values: array of Datums to be copied
 * nulls: array of is-null flags (can be NULL if no nulls)
 * nitems: number of Datums to be copied
 * typbyval, typlen, typalign: info about element datatype
 * freedata: if true and element type is pass-by-ref, pfree data values
 * referenced by Datums after copying them.
 *
 * If the input data is of varlena type, the caller must have ensured that
 * the values are not toasted.  (Doing it here doesn't work since the
 * caller has already allocated space for the array...)
 */
void
CopyArrayEls(ArrayType *array, Datum *values, bool *nulls, int nitems, int typlen, bool typbyval, char typalign, bool freedata)
{














































}

/*
 * array_out :
 *		   takes the internal representation of an array and returns a
 *string containing the array in its external format.
 */
Datum
array_out(PG_FUNCTION_ARGS)
{



















































































































































































































































}

/*
 * array_recv :
 *		  converts an array from the external binary format to
 *		  its internal format.
 *
 * return value :
 *		  the internal representation of the input array
 */
Datum
array_recv(PG_FUNCTION_ARGS)
{




















































































































}

/*
 * ReadArrayBinary:
 *	 collect the data elements of an array being read in binary style.
 *
 * Inputs:
 *	buf: the data buffer to read from.
 *	nitems: total number of array elements (already read).
 *	receiveproc: type-specific receive procedure for element datatype.
 *	typioparam, typmod: auxiliary values to pass to receiveproc.
 *	typlen, typbyval, typalign: storage parameters of element datatype.
 *
 * Outputs:
 *	values[]: filled with converted data values.
 *	nulls[]: filled with is-null markers.
 *	*hasnulls: set true iff there are any null elements.
 *	*nbytes: set to total size of data area needed (including alignment
 *		padding but not including array header overhead).
 *
 * Note that values[] and nulls[] are allocated by the caller, and must have
 * nitems elements.
 */
static void
ReadArrayBinary(StringInfo buf, int nitems, FmgrInfo *receiveproc, Oid typioparam, int32 typmod, int typlen, bool typbyval, char typalign, Datum *values, bool *nulls, bool *hasnulls, int32 *nbytes)
{



















































































}

/*
 * array_send :
 *		  takes the internal representation of an array and returns a
 *bytea containing the array in its external binary format.
 */
Datum
array_send(PG_FUNCTION_ARGS)
{




















































































}

/*
 * array_ndims :
 *		  returns the number of dimensions of the array pointed to by
 *"v"
 */
Datum
array_ndims(PG_FUNCTION_ARGS)
{









}

/*
 * array_dims :
 *		  returns the dimensions of the array pointed to by "v", as a
 *"text"
 */
Datum
array_dims(PG_FUNCTION_ARGS)
{





























}

/*
 * array_lower :
 *		returns the lower dimension, of the DIM requested, for
 *		the array pointed to by "v", as an int4
 */
Datum
array_lower(PG_FUNCTION_ARGS)
{





















}

/*
 * array_upper :
 *		returns the upper dimension, of the DIM requested, for
 *		the array pointed to by "v", as an int4
 */
Datum
array_upper(PG_FUNCTION_ARGS)
{























}

/*
 * array_length :
 *		returns the length, of the dimension requested, for
 *		the array pointed to by "v", as an int4
 */
Datum
array_length(PG_FUNCTION_ARGS)
{






















}

/*
 * array_cardinality:
 *		returns the total number of elements in an array
 */
Datum
array_cardinality(PG_FUNCTION_ARGS)
{



}

/*
 * array_get_element :
 *	  This routine takes an array datum and a subscript array and returns
 *	  the referenced item as a Datum.  Note that for a pass-by-reference
 *	  datatype, the returned Datum is a pointer into the array object.
 *
 * This handles both ordinary varlena arrays and fixed-length arrays.
 *
 * Inputs:
 *	arraydatum: the array object (mustn't be NULL)
 *	nSubscripts: number of subscripts supplied
 *	indx[]: the subscript values
 *	arraytyplen: pg_type.typlen for the array type
 *	elmlen: pg_type.typlen for the array's element type
 *	elmbyval: pg_type.typbyval for the array's element type
 *	elmalign: pg_type.typalign for the array's element type
 *
 * Outputs:
 *	The return value is the element Datum.
 *	*isNull is set to indicate whether the element is NULL.
 */
Datum
array_get_element(Datum arraydatum, int nSubscripts, int *indx, int arraytyplen, int elmlen, bool elmbyval, char elmalign, bool *isNull)
{







































































}

/*
 * Implementation of array_get_element() for an expanded array
 */
static Datum
array_get_element_expanded(Datum arraydatum, int nSubscripts, int *indx, int arraytyplen, int elmlen, bool elmbyval, char elmalign, bool *isNull)
{


































































}

/*
 * array_get_slice :
 *		   This routine takes an array and a range of indices
 *(upperIndex and lowerIndx), creates a new array structure for the referred
 *elements and returns a pointer to it.
 *
 * This handles both ordinary varlena arrays and fixed-length arrays.
 *
 * Inputs:
 *	arraydatum: the array object (mustn't be NULL)
 *	nSubscripts: number of subscripts supplied (must be same for
 *upper/lower) upperIndx[]: the upper subscript values lowerIndx[]: the lower
 *subscript values upperProvided[]: true for provided upper subscript values
 *	lowerProvided[]: true for provided lower subscript values
 *	arraytyplen: pg_type.typlen for the array type
 *	elmlen: pg_type.typlen for the array's element type
 *	elmbyval: pg_type.typbyval for the array's element type
 *	elmalign: pg_type.typalign for the array's element type
 *
 * Outputs:
 *	The return value is the new array Datum (it's never NULL)
 *
 * Omitted upper and lower subscript values are replaced by the corresponding
 * array bound.
 *
 * NOTE: we assume it is OK to scribble on the provided subscript arrays
 * lowerIndx[] and upperIndx[].  These are generally just temporaries.
 */
Datum
array_get_slice(Datum arraydatum, int nSubscripts, int *upperIndx, int *lowerIndx, bool *upperProvided, bool *lowerProvided, int arraytyplen, int elmlen, bool elmbyval, char elmalign)
{


























































































































}

/*
 * array_set_element :
 *		  This routine sets the value of one array element (specified by
 *		  a subscript array) to a new value specified by "dataValue".
 *
 * This handles both ordinary varlena arrays and fixed-length arrays.
 *
 * Inputs:
 *	arraydatum: the initial array object (mustn't be NULL)
 *	nSubscripts: number of subscripts supplied
 *	indx[]: the subscript values
 *	dataValue: the datum to be inserted at the given position
 *	isNull: whether dataValue is NULL
 *	arraytyplen: pg_type.typlen for the array type
 *	elmlen: pg_type.typlen for the array's element type
 *	elmbyval: pg_type.typbyval for the array's element type
 *	elmalign: pg_type.typalign for the array's element type
 *
 * Result:
 *		  A new array is returned, just like the old except for the one
 *		  modified entry.  The original array object is not changed, *		  unless what is passed is a read-write reference to an expanded
 *		  array object; in that case the expanded array is updated
 *in-place.
 *
 * For one-dimensional arrays only, we allow the array to be extended
 * by assigning to a position outside the existing subscript range; any
 * positions between the existing elements and the new one are set to NULLs.
 * (XXX TODO: allow a corresponding behavior for multidimensional arrays)
 *
 * NOTE: For assignments, we throw an error for invalid subscripts etc, * rather than returning a NULL as the fetch operations do.
 */
Datum
array_set_element(Datum arraydatum, int nSubscripts, int *indx, Datum dataValue, bool isNull, int arraytyplen, int elmlen, bool elmbyval, char elmalign)
{


























































































































































































































































}

/*
 * Implementation of array_set_element() for an expanded array
 *
 * Note: as with any operation on a read/write expanded object, we must
 * take pains not to leave the object in a corrupt state if we fail partway
 * through.
 */
static Datum
array_set_element_expanded(Datum arraydatum, int nSubscripts, int *indx, Datum dataValue, bool isNull, int arraytyplen, int elmlen, bool elmbyval, char elmalign)
{























































































































































































































































}

/*
 * array_set_slice :
 *		  This routine sets the value of a range of array locations
 *(specified by upper and lower subscript values) to new values passed as
 *		  another array.
 *
 * This handles both ordinary varlena arrays and fixed-length arrays.
 *
 * Inputs:
 *	arraydatum: the initial array object (mustn't be NULL)
 *	nSubscripts: number of subscripts supplied (must be same for
 *upper/lower) upperIndx[]: the upper subscript values lowerIndx[]: the lower
 *subscript values upperProvided[]: true for provided upper subscript values
 *	lowerProvided[]: true for provided lower subscript values
 *	srcArrayDatum: the source for the inserted values
 *	isNull: indicates whether srcArrayDatum is NULL
 *	arraytyplen: pg_type.typlen for the array type
 *	elmlen: pg_type.typlen for the array's element type
 *	elmbyval: pg_type.typbyval for the array's element type
 *	elmalign: pg_type.typalign for the array's element type
 *
 * Result:
 *		  A new array is returned, just like the old except for the
 *		  modified range.  The original array object is not changed.
 *
 * Omitted upper and lower subscript values are replaced by the corresponding
 * array bound.
 *
 * For one-dimensional arrays only, we allow the array to be extended
 * by assigning to positions outside the existing subscript range; any
 * positions between the existing elements and the new ones are set to NULLs.
 * (XXX TODO: allow a corresponding behavior for multidimensional arrays)
 *
 * NOTE: we assume it is OK to scribble on the provided index arrays
 * lowerIndx[] and upperIndx[].  These are generally just temporaries.
 *
 * NOTE: For assignments, we throw an error for silly subscripts etc, * rather than returning a NULL or empty array as the fetch operations do.
 */
Datum
array_set_slice(Datum arraydatum, int nSubscripts, int *upperIndx, int *lowerIndx, bool *upperProvided, bool *lowerProvided, Datum srcArrayDatum, bool isNull, int arraytyplen, int elmlen, bool elmbyval, char elmalign)
{





































































































































































































































































}

/*
 * array_ref : backwards compatibility wrapper for array_get_element
 *
 * This only works for detoasted/flattened varlena arrays, since the array
 * argument is declared as "ArrayType *".  However there's enough code like
 * that to justify preserving this API.
 */
Datum
array_ref(ArrayType *array, int nSubscripts, int *indx, int arraytyplen, int elmlen, bool elmbyval, char elmalign, bool *isNull)
{

}

/*
 * array_set : backwards compatibility wrapper for array_set_element
 *
 * This only works for detoasted/flattened varlena arrays, since the array
 * argument and result are declared as "ArrayType *".  However there's enough
 * code like that to justify preserving this API.
 */
ArrayType *
array_set(ArrayType *array, int nSubscripts, int *indx, Datum dataValue, bool isNull, int arraytyplen, int elmlen, bool elmbyval, char elmalign)
{

}

/*
 * array_map()
 *
 * Map an array through an arbitrary expression.  Return a new array with
 * the same dimensions and each source element transformed by the given, * already-compiled expression.  Each source element is placed in the
 * innermost_caseval/innermost_casenull fields of the ExprState.
 *
 * Parameters are:
 * * arrayd: Datum representing array argument.
 * * exprstate: ExprState representing the per-element transformation.
 * * econtext: context for expression evaluation.
 * * retType: OID of element type of output array.  This must be the same as, *	 or binary-compatible with, the result type of the expression.  It might
 *	 be different from the input array's element type.
 * * amstate: workspace for array_map.  Must be zeroed by caller before
 *	 first call, and not touched after that.
 *
 * It is legitimate to pass a freshly-zeroed ArrayMapState on each call, * but better performance can be had if the state can be preserved across
 * a series of calls.
 *
 * NB: caller must assure that input array is not NULL.  NULL elements in
 * the array are OK however.
 * NB: caller should be running in econtext's per-tuple memory context.
 */
Datum
array_map(Datum arrayd, ExprState *exprstate, ExprContext *econtext, Oid retType, ArrayMapState *amstate)
{
































































































































}

/*
 * construct_array	--- simple method for constructing an array object
 *
 * elems: array of Datum items to become the array contents
 *		  (NULL element values are not supported).
 * nelems: number of items
 * elmtype, elmlen, elmbyval, elmalign: info for the datatype of the items
 *
 * A palloc'd 1-D array object is constructed and returned.  Note that
 * elem values will be copied into the object even if pass-by-ref type.
 * Also note the result will be 0-D not 1-D if nelems = 0.
 *
 * NOTE: it would be cleaner to look up the elmlen/elmbval/elmalign info
 * from the system catalogs, given the elmtype.  However, the caller is
 * in a better position to cache this info across multiple uses, or even
 * to hard-wire values if the element type is hard-wired.
 */
ArrayType *
construct_array(Datum *elems, int nelems, Oid elmtype, int elmlen, bool elmbyval, char elmalign)
{







}

/*
 * construct_md_array	--- simple method for constructing an array object
 *							with arbitrary
 *dimensions and possible NULLs
 *
 * elems: array of Datum items to become the array contents
 * nulls: array of is-null flags (can be NULL if no nulls)
 * ndims: number of dimensions
 * dims: integer array with size of each dimension
 * lbs: integer array with lower bound of each dimension
 * elmtype, elmlen, elmbyval, elmalign: info for the datatype of the items
 *
 * A palloc'd ndims-D array object is constructed and returned.  Note that
 * elem values will be copied into the object even if pass-by-ref type.
 * Also note the result will be 0-D not ndims-D if any dims[i] = 0.
 *
 * NOTE: it would be cleaner to look up the elmlen/elmbval/elmalign info
 * from the system catalogs, given the elmtype.  However, the caller is
 * in a better position to cache this info across multiple uses, or even
 * to hard-wire values if the element type is hard-wired.
 */
ArrayType *
construct_md_array(Datum *elems, bool *nulls, int ndims, int *dims, int *lbs, Oid elmtype, int elmlen, bool elmbyval, char elmalign)
{








































































}

/*
 * construct_empty_array	--- make a zero-dimensional array of given type
 */
ArrayType *
construct_empty_array(Oid elmtype)
{








}

/*
 * construct_empty_expanded_array: make an empty expanded array
 * given only type information.  (metacache can be NULL if not needed.)
 */
ExpandedArrayHeader *
construct_empty_expanded_array(Oid element_type, MemoryContext parentcontext, ArrayMetaState *metacache)
{






}

/*
 * deconstruct_array  --- simple method for extracting data from an array
 *
 * array: array object to examine (must not be NULL)
 * elmtype, elmlen, elmbyval, elmalign: info for the datatype of the items
 * elemsp: return value, set to point to palloc'd array of Datum values
 * nullsp: return value, set to point to palloc'd array of isnull markers
 * nelemsp: return value, set to number of extracted values
 *
 * The caller may pass nullsp == NULL if it does not support NULLs in the
 * array.  Note that this produces a very uninformative error message, * so do it only in cases where a NULL is really not expected.
 *
 * If array elements are pass-by-ref data type, the returned Datums will
 * be pointers into the array object.
 *
 * NOTE: it would be cleaner to look up the elmlen/elmbval/elmalign info
 * from the system catalogs, given the elmtype.  However, in most current
 * uses the type is hard-wired into the caller and so we can save a lookup
 * cycle by hard-wiring the type info as well.
 */
void
deconstruct_array(ArrayType *array, Oid elmtype, int elmlen, bool elmbyval, char elmalign, Datum **elemsp, bool **nullsp, int *nelemsp)
{



























































}

/*
 * array_contains_nulls --- detect whether an array has any null elements
 *
 * This gives an accurate answer, whereas testing ARR_HASNULL only tells
 * if the array *might* contain a null.
 */
bool
array_contains_nulls(ArrayType *array)
{






































}

/*
 * array_eq :
 *		  compares two arrays for equality
 * result :
 *		  returns true if the arrays are equal, false otherwise.
 *
 * Note: we do not use array_cmp here, since equality may be meaningful in
 * datatypes that don't have a total ordering (and hence no btree support).
 */
Datum
array_eq(PG_FUNCTION_ARGS)
{














































































































}

/*-----------------------------------------------------------------------------
 * array-array bool operators:
 *		Given two arrays, iterate comparison operators
 *		over the array. Uses logic similar to text comparison
 *		functions, except element-by-element instead of
 *		character-by-character.
 *----------------------------------------------------------------------------
 */

Datum
array_ne(PG_FUNCTION_ARGS)
{

}

Datum
array_lt(PG_FUNCTION_ARGS)
{

}

Datum
array_gt(PG_FUNCTION_ARGS)
{

}

Datum
array_le(PG_FUNCTION_ARGS)
{

}

Datum
array_ge(PG_FUNCTION_ARGS)
{

}

Datum
btarraycmp(PG_FUNCTION_ARGS)
{

}

/*
 * array_cmp()
 * Internal comparison function for arrays.
 *
 * Returns -1, 0 or 1
 */
static int
array_cmp(FunctionCallInfo fcinfo)
{



































































































































































}

/*-----------------------------------------------------------------------------
 * array hashing
 *		Hash the elements and combine the results.
 *----------------------------------------------------------------------------
 */

Datum
hash_array(PG_FUNCTION_ARGS)
{



















































































}

/*
 * Returns 64-bit value by hashing a value to a 64-bit value, with a seed.
 * Otherwise, similar to hash_array.
 */
Datum
hash_array_extended(PG_FUNCTION_ARGS)
{
































































}

/*-----------------------------------------------------------------------------
 * array overlap/containment comparisons
 *		These use the same methods of comparing array elements as
 *array_eq. We consider only the elements of the arrays, ignoring
 *dimensionality.
 *----------------------------------------------------------------------------
 */

/*
 * array_contain_compare :
 *		  compares two arrays for overlap/containment
 *
 * When matchall is true, return true if all members of array1 are in array2.
 * When matchall is false, return true if any members of array1 are in array2.
 */
static bool
array_contain_compare(AnyArrayType *array1, AnyArrayType *array2, Oid collation, bool matchall, void **fn_extra)
{









































































































































}

Datum
arrayoverlap(PG_FUNCTION_ARGS)
{












}

Datum
arraycontains(PG_FUNCTION_ARGS)
{












}

Datum
arraycontained(PG_FUNCTION_ARGS)
{












}

/*-----------------------------------------------------------------------------
 * Array iteration functions
 *		These functions are used to iterate efficiently through arrays
 *-----------------------------------------------------------------------------
 */

/*
 * array_create_iterator --- set up to iterate through an array
 *
 * If slice_ndim is zero, we will iterate element-by-element; the returned
 * datums are of the array's element type.
 *
 * If slice_ndim is 1..ARR_NDIM(arr), we will iterate by slices: the
 * returned datums are of the same array type as 'arr', but of size
 * equal to the rightmost N dimensions of 'arr'.
 *
 * The passed-in array must remain valid for the lifetime of the iterator.
 */
ArrayIterator
array_create_iterator(ArrayType *arr, int slice_ndim, ArrayMetaState *mstate)
{


































































}

/*
 * Iterate through the array referenced by 'iterator'.
 *
 * As long as there is another element (or slice), return it into
 * *value / *isnull, and return true.  Return false when no more data.
 */
bool
array_iterate(ArrayIterator iterator, Datum *value, bool *isnull)
{




































































}

/*
 * Release an ArrayIterator data structure
 */
void
array_free_iterator(ArrayIterator iterator)
{






}

/***************************************************************************/
/******************|		  Support  Routines
 * |*****************/
/***************************************************************************/

/*
 * Check whether a specific array element is NULL
 *
 * nullbitmap: pointer to array's null bitmap (NULL if none)
 * offset: 0-based linear element number of array element
 */
static bool
array_get_isnull(const bits8 *nullbitmap, int offset)
{









}

/*
 * Set a specific array element's null-bitmap entry
 *
 * nullbitmap: pointer to array's null bitmap (mustn't be NULL)
 * offset: 0-based linear element number of array element
 * isNull: null status to set
 */
static void
array_set_isnull(bits8 *nullbitmap, int offset, bool isNull)
{












}

/*
 * Fetch array element at pointer, converted correctly to a Datum
 *
 * Caller must have handled case of NULL element
 */
static Datum
ArrayCast(char *value, bool byval, int len)
{

}

/*
 * Copy datum to *dest and return total space used (including align padding)
 *
 * Caller must have handled case of NULL element
 */
static int
ArrayCastAndSet(Datum src, int typlen, bool typbyval, char typalign, char *dest)
{























}

/*
 * Advance ptr over nitems array elements
 *
 * ptr: starting location in array
 * offset: 0-based linear element number of first element (the one at *ptr)
 * nullbitmap: start of array's null bitmap, or NULL if none
 * nitems: number of array elements to advance over (>= 0)
 * typlen, typbyval, typalign: storage parameters of array element datatype
 *
 * It is caller's responsibility to ensure that nitems is within range
 */
static char *
array_seek(char *ptr, int offset, bits8 *nullbitmap, int nitems, int typlen, bool typbyval, char typalign)
{







































}

/*
 * Compute total size of the nitems array elements starting at *ptr
 *
 * Parameters same as for array_seek
 */
static int
array_nelems_size(char *ptr, int offset, bits8 *nullbitmap, int nitems, int typlen, bool typbyval, char typalign)
{

}

/*
 * Copy nitems array elements from srcptr to destptr
 *
 * destptr: starting destination location (must be enough room!)
 * nitems: number of array elements to copy (>= 0)
 * srcptr: starting location in source array
 * offset: 0-based linear element number of first element (the one at *srcptr)
 * nullbitmap: start of source array's null bitmap, or NULL if none
 * typlen, typbyval, typalign: storage parameters of array element datatype
 *
 * Returns number of bytes copied
 *
 * NB: this does not take care of setting up the destination's null bitmap!
 */
static int
array_copy(char *destptr, int nitems, char *srcptr, int offset, bits8 *nullbitmap, int typlen, bool typbyval, char typalign)
{





}

/*
 * Copy nitems null-bitmap bits from source to destination
 *
 * destbitmap: start of destination array's null bitmap (mustn't be NULL)
 * destoffset: 0-based linear element number of first dest element
 * srcbitmap: start of source array's null bitmap, or NULL if none
 * srcoffset: 0-based linear element number of first source element
 * nitems: number of bits to copy (>= 0)
 *
 * If srcbitmap is NULL then we assume the source is all-non-NULL and
 * fill 1's into the destination bitmap.  Note that only the specified
 * bits in the destination map are changed, not any before or after.
 *
 * Note: this could certainly be optimized using standard bitblt methods.
 * However, it's not clear that the typical Postgres array has enough elements
 * to make it worth worrying too much.  For the moment, KISS.
 */
void
array_bitmap_copy(bits8 *destbitmap, int destoffset, const bits8 *srcbitmap, int srcoffset, int nitems)
{








































































}

/*
 * Compute space needed for a slice of an array
 *
 * We assume the caller has verified that the slice coordinates are valid.
 */
static int
array_slice_size(char *arraydataptr, bits8 *arraynullsptr, int ndim, int *dim, int *lb, int *st, int *endp, int typlen, bool typbyval, char typalign)
{








































}

/*
 * Extract a slice of an array into consecutive elements in the destination
 * array.
 *
 * We assume the caller has verified that the slice coordinates are valid, * allocated enough storage for the result, and initialized the header
 * of the new array.
 */
static void
array_extract_slice(ArrayType *newarray, int ndim, int *dim, int *lb, char *arraydataptr, bits8 *arraynullsptr, int *st, int *endp, int typlen, bool typbyval, char typalign)
{



































}

/*
 * Insert a slice into an array.
 *
 * ndim/dim[]/lb[] are dimensions of the original array.  A new array with
 * those same dimensions is to be constructed.  destArray must already
 * have been allocated and its header initialized.
 *
 * st[]/endp[] identify the slice to be replaced.  Elements within the slice
 * volume are taken from consecutive elements of the srcArray; elements
 * outside it are copied from origArray.
 *
 * We assume the caller has verified that the slice coordinates are valid.
 */
static void
array_insert_slice(ArrayType *destArray, ArrayType *origArray, ArrayType *srcArray, int ndim, int *dim, int *lb, int *st, int *endp, int typlen, bool typbyval, char typalign)
{

































































}

/*
 * initArrayResult - initialize an empty ArrayBuildState
 *
 *	element_type is the array element type (must be a valid array element
 *type) rcontext is where to keep working state subcontext is a flag determining
 *whether to use a separate memory context
 *
 * Note: there are two common schemes for using accumArrayResult().
 * In the older scheme, you start with a NULL ArrayBuildState pointer, and
 * call accumArrayResult once per element.  In this scheme you end up with
 * a NULL pointer if there were no elements, which you need to special-case.
 * In the newer scheme, call initArrayResult and then call accumArrayResult
 * once per element.  In this scheme you always end with a non-NULL pointer
 * that you can pass to makeArrayResult; you get an empty array if there
 * were no elements.  This is preferred if an empty array is what you want.
 *
 * It's possible to choose whether to create a separate memory context for the
 * array build state, or whether to allocate it directly within rcontext.
 *
 * When there are many concurrent small states (e.g. array_agg() using hash
 * aggregation of many small groups), using a separate memory context for each
 * one may result in severe memory bloat. In such cases, use the same memory
 * context to initialize all such array build states, and pass
 * subcontext=false.
 *
 * In cases when the array build states have different lifetimes, using a
 * single memory context is impractical. Instead, pass subcontext=true so that
 * the array build states can be freed individually.
 */
ArrayBuildState *
initArrayResult(Oid element_type, MemoryContext rcontext, bool subcontext)
{




















}

/*
 * accumArrayResult - accumulate one (more) Datum for an array result
 *
 *	astate is working state (can be NULL on first call)
 *	dvalue/disnull represent the new Datum to append to the array
 *	element_type is the Datum's type (must be a valid array element type)
 *	rcontext is where to keep working state
 */
ArrayBuildState *
accumArrayResult(ArrayBuildState *astate, Datum dvalue, bool disnull, Oid element_type, MemoryContext rcontext)
{

















































}

/*
 * makeArrayResult - produce 1-D final result of accumArrayResult
 *
 * Note: only releases astate if it was initialized within a separate memory
 * context (i.e. using subcontext=true when calling initArrayResult).
 *
 *	astate is working state (must not be NULL)
 *	rcontext is where to construct result
 */
Datum
makeArrayResult(ArrayBuildState *astate, MemoryContext rcontext)
{










}

/*
 * makeMdArrayResult - produce multi-D final result of accumArrayResult
 *
 * beware: no check that specified dimensions match the number of values
 * accumulated.
 *
 * Note: if the astate was not initialized within a separate memory context
 * (that is, initArrayResult was called with subcontext=false), then using
 * release=true is illegal. Instead, release astate along with the rest of its
 * context when appropriate.
 *
 *	astate is working state (must not be NULL)
 *	rcontext is where to construct result
 *	release is true if okay to release working state
 */
Datum
makeMdArrayResult(ArrayBuildState *astate, int ndims, int *dims, int *lbs, MemoryContext rcontext, bool release)
{


















}

/*
 * The following three functions provide essentially the same API as
 * initArrayResult/accumArrayResult/makeArrayResult, but instead of accepting
 * inputs that are array elements, they accept inputs that are arrays and
 * produce an output array having N+1 dimensions.  The inputs must all have
 * identical dimensionality as well as element type.
 */

/*
 * initArrayResultArr - initialize an empty ArrayBuildStateArr
 *
 *	array_type is the array type (must be a valid varlena array type)
 *	element_type is the type of the array's elements (lookup if InvalidOid)
 *	rcontext is where to keep working state
 *	subcontext is a flag determining whether to use a separate memory
 *context
 */
ArrayBuildStateArr *
initArrayResultArr(Oid array_type, Oid element_type, MemoryContext rcontext, bool subcontext)
{






























}

/*
 * accumArrayResultArr - accumulate one (more) sub-array for an array result
 *
 *	astate is working state (can be NULL on first call)
 *	dvalue/disnull represent the new sub-array to append to the array
 *	array_type is the array type (must be a valid varlena array type)
 *	rcontext is where to keep working state
 */
ArrayBuildStateArr *
accumArrayResultArr(ArrayBuildStateArr *astate, Datum dvalue, bool disnull, Oid array_type, MemoryContext rcontext)
{













































































































































}

/*
 * makeArrayResultArr - produce N+1-D final result of accumArrayResultArr
 *
 *	astate is working state (must not be NULL)
 *	rcontext is where to construct result
 *	release is true if okay to release working state
 */
Datum
makeArrayResultArr(ArrayBuildStateArr *astate, MemoryContext rcontext, bool release)
{


























































}

/*
 * The following three functions provide essentially the same API as
 * initArrayResult/accumArrayResult/makeArrayResult, but can accept either
 * scalar or array inputs, invoking the appropriate set of functions above.
 */

/*
 * initArrayResultAny - initialize an empty ArrayBuildStateAny
 *
 *	input_type is the input datatype (either element or array type)
 *	rcontext is where to keep working state
 *	subcontext is a flag determining whether to use a separate memory
 *context
 */
ArrayBuildStateAny *
initArrayResultAny(Oid input_type, MemoryContext rcontext, bool subcontext)
{




























}

/*
 * accumArrayResultAny - accumulate one (more) input for an array result
 *
 *	astate is working state (can be NULL on first call)
 *	dvalue/disnull represent the new input to append to the array
 *	input_type is the input datatype (either element or array type)
 *	rcontext is where to keep working state
 */
ArrayBuildStateAny *
accumArrayResultAny(ArrayBuildStateAny *astate, Datum dvalue, bool disnull, Oid input_type, MemoryContext rcontext)
{















}

/*
 * makeArrayResultAny - produce final result of accumArrayResultAny
 *
 *	astate is working state (must not be NULL)
 *	rcontext is where to construct result
 *	release is true if okay to release working state
 */
Datum
makeArrayResultAny(ArrayBuildStateAny *astate, MemoryContext rcontext, bool release)
{





















}

Datum
array_larger(PG_FUNCTION_ARGS)
{








}

Datum
array_smaller(PG_FUNCTION_ARGS)
{








}

typedef struct generate_subscripts_fctx
{
  int32 lower;
  int32 upper;
  bool reverse;
} generate_subscripts_fctx;

/*
 * generate_subscripts(array anyarray, dim int [, reverse bool])
 *		Returns all subscripts of the array for any dimension
 */
Datum
generate_subscripts(PG_FUNCTION_ARGS)
{
































































}

/*
 * generate_subscripts_nodir
 *		Implements the 2-argument version of generate_subscripts
 */
Datum
generate_subscripts_nodir(PG_FUNCTION_ARGS)
{


}

/*
 * array_fill_with_lower_bounds
 *		Create and fill array with defined lower bounds.
 */
Datum
array_fill_with_lower_bounds(PG_FUNCTION_ARGS)
{


































}

/*
 * array_fill
 *		Create and fill array with default lower bounds.
 */
Datum
array_fill(PG_FUNCTION_ARGS)
{
































}

static ArrayType *
create_array_envelope(int ndims, int *dimv, int *lbsv, int nbytes, Oid elmtype, int dataoffset)
{











}

static ArrayType *
array_fill_internal(ArrayType *dims, ArrayType *lbs, Datum value, bool isnull, Oid elmtype, FunctionCallInfo fcinfo)
{


























































































































































}

/*
 * UNNEST
 */
Datum
array_unnest(PG_FUNCTION_ARGS)
{














































































}

/*
 * Planner support function for array_unnest(anyarray)
 */
Datum
array_unnest_support(PG_FUNCTION_ARGS)
{






















}

/*
 * array_replace/array_remove support
 *
 * Find all array entries matching (not distinct from) search/search_isnull, * and delete them if remove is true, else replace them with
 * replace/replace_isnull.  Comparisons are done using the specified
 * collation.  fcinfo is passed only for caching purposes.
 */
static ArrayType *
array_replace_internal(ArrayType *array, Datum search, bool search_isnull, Datum replace, bool replace_isnull, bool remove, Oid collation, FunctionCallInfo fcinfo)
{





















































































































































































































































}

/*
 * Remove any occurrences of an element from an array
 *
 * If used on a multi-dimensional array this will raise an error.
 */
Datum
array_remove(PG_FUNCTION_ARGS)
{












}

/*
 * Replace any occurrences of an element in an array
 */
Datum
array_replace(PG_FUNCTION_ARGS)
{














}

/*
 * Implements width_bucket(anyelement, anyarray).
 *
 * 'thresholds' is an array containing lower bound values for each bucket;
 * these must be sorted from smallest to largest, or bogus results will be
 * produced.  If N thresholds are supplied, the output is from 0 to N:
 * 0 is for inputs < first threshold, N is for inputs >= last threshold.
 */
Datum
width_bucket_array(PG_FUNCTION_ARGS)
{
























































}

/*
 * width_bucket_array for float8 data.
 */
static int
width_bucket_array_float8(Datum operand, ArrayType *thresholds)
{










































}

/*
 * width_bucket_array for generic fixed-width data types.
 */
static int
width_bucket_array_fixed(Datum operand, ArrayType *thresholds, Oid collation, TypeCacheEntry *typentry)
{













































}

/*
 * width_bucket_array for generic variable-width data types.
 */
static int
width_bucket_array_variable(Datum operand, ArrayType *thresholds, Oid collation, TypeCacheEntry *typentry)
{
























































}