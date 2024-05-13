/*-------------------------------------------------------------------------
 * unicode_norm.c
 *		Normalize a Unicode string to NFKC form
 *
 * This implements Unicode normalization, per the documentation at
 * http://www.unicode.org/reports/tr15/.
 *
 * Portions Copyright (c) 2017-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/common/unicode_norm.c
 *
 *-------------------------------------------------------------------------
 */
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/unicode_norm.h"
#include "common/unicode_norm_table.h"

#ifndef FRONTEND
#define ALLOC(size) palloc(size)
#define FREE(size) pfree(size)
#else
#define ALLOC(size) malloc(size)
#define FREE(size) free(size)
#endif

/* Constants for calculations with Hangul characters */
#define SBASE 0xAC00 /* U+AC00 */
#define LBASE 0x1100 /* U+1100 */
#define VBASE 0x1161 /* U+1161 */
#define TBASE 0x11A7 /* U+11A7 */
#define LCOUNT 19
#define VCOUNT 21
#define TCOUNT 28
#define NCOUNT VCOUNT *TCOUNT
#define SCOUNT LCOUNT *NCOUNT

/* comparison routine for bsearch() of decomposition lookup table. */
static int
conv_compare(const void *p1, const void *p2)
{





}

/*
 * Get the entry corresponding to code in the decomposition lookup table.
 */
static pg_unicode_decomposition *
get_code_entry(pg_wchar code)
{

}

/*
 * Given a decomposition entry looked up earlier, get the decomposed
 * characters.
 *
 * Note: the returned pointer can point to statically allocated buffer, and
 * is only valid until next call to this function!
 */
static const pg_wchar *
get_code_decomposition(pg_unicode_decomposition *entry, int *dec_size)
{














}

/*
 * Calculate how many characters a given character will decompose to.
 *
 * This needs to recurse, if the character decomposes into characters that
 * are, in turn, decomposable.
 */
static int
get_decomposed_size(pg_wchar code)
{


















































}

/*
 * Recompose a set of characters. For hangul characters, the calculation
 * is algorithmic. For others, an inverse lookup at the decomposition
 * table is necessary. Returns true if a recomposition can be done, and
 * false otherwise.
 */
static bool
recompose_code(uint32 start, uint32 code, uint32 *result)
{
























































}

/*
 * Decompose the given code into the array given by caller. The
 * decomposition begins at the position given by caller, saving one
 * lookup on the decomposition table. The current position needs to be
 * updated here to let the caller know from where to continue filling
 * in the array result.
 */
static void
decompose_code(pg_wchar code, pg_wchar **result, int *current)
{































































}

/*
 * unicode_normalize_kc - Normalize a Unicode string to NFKC form.
 *
 * The input is a 0-terminated array of codepoints.
 *
 * In frontend, returns a 0-terminated array of codepoints, allocated with
 * malloc. Or NULL if we run out of memory. In backend, the returned
 * string is palloc'd instead, and OOM is reported with ereport().
 */
pg_wchar *
unicode_normalize_kc(const pg_wchar *input)
{
















































































































































}
