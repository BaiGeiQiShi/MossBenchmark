/*-------------------------------------------------------------------------
 *
 * array_expanded.c
 *	  Basic functions for manipulating expanded arrays.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/array_expanded.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/tupmacs.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

/* "Methods" required for an expanded object */
static Size
EA_get_flat_size(ExpandedObjectHeader *eohptr);
static void
EA_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size);

static const ExpandedObjectMethods EA_methods = {EA_get_flat_size, EA_flatten_into};

/* Other local functions */
static void
copy_byval_expanded_array(ExpandedArrayHeader *eah, ExpandedArrayHeader *oldeah);

/*
 * expand_array: convert an array Datum into an expanded array
 *
 * The expanded object will be a child of parentcontext.
 *
 * Some callers can provide cache space to avoid repeated lookups of element
 * type data across calls; if so, pass a metacache pointer, making sure that
 * metacache->element_type is initialized to InvalidOid before first call.
 * If no cross-call caching is required, pass NULL for metacache.
 */
Datum
expand_array(Datum arraydatum, MemoryContext parentcontext, ArrayMetaState *metacache)
{


























































































































}

/*
 * helper for expand_array(): copy pass-by-value Datum-array representation
 */
static void
copy_byval_expanded_array(ExpandedArrayHeader *eah, ExpandedArrayHeader *oldeah)
{







































}

/*
 * get_flat_size method for expanded arrays
 */
static Size
EA_get_flat_size(ExpandedObjectHeader *eohptr)
{





























































}

/*
 * flatten_into method for expanded arrays
 */
static void
EA_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size)
{











































}

/*
 * Argument fetching support code
 */

/*
 * DatumGetExpandedArray: get a writable expanded array from an input argument
 *
 * Caution: if the input is a read/write pointer, this returns the input
 * argument; so callers must be sure that their changes are "safe", that is
 * they cannot leave the array in a corrupt state.
 */
ExpandedArrayHeader *
DatumGetExpandedArray(Datum d)
{












}

/*
 * As above, when caller has the ability to cache element type info
 */
ExpandedArrayHeader *
DatumGetExpandedArrayX(Datum d, ArrayMetaState *metacache)
{




















}

/*
 * DatumGetAnyArrayP: return either an expanded array or a detoasted varlena
 * array.  The result must not be modified in-place.
 */
AnyArrayType *
DatumGetAnyArrayP(Datum d)
{














}

/*
 * Create the Datum/isnull representation of an expanded array object
 * if we didn't do so previously
 */
void
deconstruct_expanded_array(ExpandedArrayHeader *eah)
{






















}