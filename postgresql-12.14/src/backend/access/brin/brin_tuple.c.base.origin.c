/*
 * brin_tuple.c
 *		Method implementations for tuples in BRIN indexes.
 *
 * Intended usage is that code outside this file only deals with
 * BrinMemTuples, and convert to and from the on-disk representation through
 * functions in this file.
 *
 * NOTES
 *
 * A BRIN tuple is similar to a heap tuple, with a few key differences.  The
 * first interesting difference is that the tuple header is much simpler, only
 * containing its total length and a small area for flags.  Also, the stored
 * data does not match the relation tuple descriptor exactly: for each
 * attribute in the descriptor, the index tuple carries an arbitrary number
 * of values, depending on the opclass.
 *
 * Also, for each column of the index relation there are two null bits: one
 * (hasnulls) stores whether any tuple within the page range has that column
 * set to null; the other one (allnulls) stores whether the column values are
 * all null.  If allnulls is true, then the tuple data area does not contain
 * values for that column at all; whereas it does if the hasnulls is set.
 * Note the size of the null bitmask may not be the same as that of the
 * datum array.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_tuple.c
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/brin_tuple.h"
#include "access/tupdesc.h"
#include "access/tupmacs.h"
#include "access/tuptoaster.h"
#include "utils/datum.h"
#include "utils/memutils.h"

static inline void
brin_deconstruct_tuple(BrinDesc *brdesc, char *tp, bits8 *nullbits, bool nulls, Datum *values, bool *allnulls, bool *hasnulls);

/*
 * Return a tuple descriptor used for on-disk storage of BRIN tuples.
 */
static TupleDesc
brtuple_disk_tupdesc(BrinDesc *brdesc)
{




























}

/*
 * Generate a new on-disk tuple to be inserted in a BRIN index.
 *
 * See brin_form_placeholder_tuple if you touch this.
 */
BrinTuple *
brin_form_tuple(BrinDesc *brdesc, BlockNumber blkno, BrinMemTuple *tuple, Size *size)
{






























































































































































































































































}

/*
 * Generate a new on-disk tuple with no data values, marked as placeholder.
 *
 * This is a cut-down version of brin_form_tuple.
 */
BrinTuple *
brin_form_placeholder_tuple(BrinDesc *brdesc, BlockNumber blkno, Size *size)
{







































}

/*
 * Free a tuple created by brin_form_tuple
 */
void
brin_free_tuple(BrinTuple *tuple)
{

}

/*
 * Given a brin tuple of size len, create a copy of it.  If 'dest' is not
 * NULL, its size is destsz, and can be used as output buffer; if the tuple
 * to be copied does not fit, it is enlarged by repalloc, and the size is
 * updated to match.  This avoids palloc/free cycles when many brin tuples
 * are being processed in loops.
 */
BrinTuple *
brin_copy_tuple(BrinTuple *tuple, Size len, BrinTuple *dest, Size *destsz)
{













}

/*
 * Return whether two BrinTuples are bitwise identical.
 */
bool
brin_tuples_equal(const BrinTuple *a, Size alen, const BrinTuple *b, Size blen)
{









}

/*
 * Create a new BrinMemTuple from scratch, and initialize it to an empty
 * state.
 *
 * Note: we don't provide any means to free a deformed tuple, so make sure to
 * use a temporary memory context.
 */
BrinMemTuple *
brin_new_memtuple(BrinDesc *brdesc)
{















}

/*
 * Reset a BrinMemTuple to initial state.  We return the same tuple, for
 * notational convenience.
 */
BrinMemTuple *
brin_memtuple_initialize(BrinMemTuple *dtuple, BrinDesc *brdesc)
{
















}

/*
 * Convert a BrinTuple back to a BrinMemTuple.  This is the reverse of
 * brin_form_tuple.
 *
 * As an optimization, the caller can pass a previously allocated 'dMemtuple'.
 * This avoids having to allocate it here, which can be useful when this
 * function is called many times in a loop.  It is caller's responsibility
 * that the given BrinMemTuple matches what we need here.
 *
 * Note we don't need the "on disk tupdesc" here; we rely on our own routine to
 * deconstruct the tuple from the on-disk format.
 */
BrinMemTuple *
brin_deform_tuple(BrinDesc *brdesc, BrinTuple *tuple, BrinMemTuple *dMemtuple)
{

































































}

/*
 * brin_deconstruct_tuple
 *		Guts of attribute extraction from an on-disk BRIN tuple.
 *
 * Its arguments are:
 *	brdesc		BRIN descriptor for the stored tuple
 *	tp			pointer to the tuple data area
 *	nullbits	pointer to the tuple nulls bitmask
 *	nulls		"has nulls" bit in tuple infomask
 *	values		output values, array of size brdesc->bd_totalstored
 *	allnulls	output "allnulls", size brdesc->bd_tupdesc->natts
 *	hasnulls	output "hasnulls", size brdesc->bd_tupdesc->natts
 *
 * Output arrays must have been allocated by caller.
 */
static inline void
brin_deconstruct_tuple(BrinDesc *brdesc, char *tp, bits8 *nullbits, bool nulls, Datum *values, bool *allnulls, bool *hasnulls)
{



































































}