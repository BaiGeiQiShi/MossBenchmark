/*-------------------------------------------------------------------------
 *
 * spgtextproc.c
 *	  implementation of radix tree (compressed trie) over text
 *
 * In a text_ops SPGiST index, inner tuples can have a prefix which is the
 * common prefix of all strings indexed under that tuple.  The node labels
 * represent the next byte of the string(s) after the prefix.  Assuming we
 * always use the longest possible prefix, we will get more than one node
 * label unless the prefix length is restricted by SPGIST_MAX_PREFIX_LENGTH.
 *
 * To reconstruct the indexed string for any index entry, concatenate the
 * inner-tuple prefixes and node labels starting at the root and working
 * down to the leaf entry, then append the datum in the leaf entry.
 * (While descending the tree, "level" is the number of bytes reconstructed
 * so far.)
 *
 * However, there are two special cases for node labels: -1 indicates that
 * there are no more bytes after the prefix-so-far, and -2 indicates that we
 * had to split an existing allTheSame tuple (in such a case we have to create
 * a node label that doesn't correspond to any string byte).  In either case,
 * the node label does not contribute anything to the reconstructed string.
 *
 * Previously, we used a node label of zero for both special cases, but
 * this was problematic because one can't tell whether a string ending at
 * the current level can be pushed down into such a child node.  For
 * backwards compatibility, we still support such node labels for reading;
 * but no new entries will ever be pushed down into a zero-labeled child.
 * No new entries ever get pushed into a -2-labeled child, either.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/spgist/spgtextproc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/spgist.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/pg_locale.h"
#include "utils/varlena.h"

/*
 * In the worst case, an inner tuple in a text radix tree could have as many
 * as 258 nodes (one for each possible byte value, plus the two special
 * cases).  Each node can take 16 bytes on MAXALIGN=8 machines.  The inner
 * tuple must fit on an index page of size BLCKSZ.  Rather than assuming we
 * know the exact amount of overhead imposed by page headers, tuple headers,
 * etc, we leave 100 bytes for that (the actual overhead should be no more
 * than 56 bytes at this writing, so there is slop in this number).
 * So we can safely create prefixes up to BLCKSZ - 258 * 16 - 100 bytes long.
 * Unfortunately, because 258 * 16 is over 4K, there is no safe prefix length
 * when BLCKSZ is less than 8K; it is always possible to get "SPGiST inner
 * tuple size exceeds maximum" if there are too many distinct next-byte values
 * at a given place in the tree.  Since use of nonstandard block sizes appears
 * to be negligible in the field, we just live with that fact for now,
 * choosing a max prefix size of 32 bytes when BLCKSZ is configured smaller
 * than default.
 */
#define SPGIST_MAX_PREFIX_LENGTH Max((int)(BLCKSZ - 258 * 16 - 100), 32)

/*
 * Strategy for collation aware operator on text is equal to btree strategy
 * plus value of 10.
 *
 * Current collation aware strategies and their corresponding btree strategies:
 * 11 BTLessStrategyNumber
 * 12 BTLessEqualStrategyNumber
 * 14 BTGreaterEqualStrategyNumber
 * 15 BTGreaterStrategyNumber
 */
#define SPG_STRATEGY_ADDITION (10)
#define SPG_IS_COLLATION_AWARE_STRATEGY(s) ((s) > SPG_STRATEGY_ADDITION && (s) != RTPrefixStrategyNumber)

/* Struct for sorting values in picksplit */
typedef struct spgNodePtr
{
  Datum d;
  int i;
  int16 c;
} spgNodePtr;

Datum
spg_text_config(PG_FUNCTION_ARGS)
{








}

/*
 * Form a text datum from the given not-necessarily-null-terminated string,
 * using short varlena header format if possible
 */
static Datum
formTextDatum(const char *data, int datalen)
{



















}

/*
 * Find the length of the common prefix of a and b
 */
static int
commonPrefix(const char *a, const char *b, int lena, int lenb)
{










}

/*
 * Binary search an array of int16 datums for a match to c
 *
 * On success, *i gets the match location; on failure, it gets where to insert
 */
static bool
searchChar(Datum *nodeLabels, int nNodes, int16 c, int *i)
{
























}

Datum
spg_text_choose(PG_FUNCTION_ARGS)
{





































































































































}

/* qsort comparator to sort spgNodePtr structs by "c" */
static int
cmpNodePtr(const void *a, const void *b)
{




}

Datum
spg_text_picksplit(PG_FUNCTION_ARGS)
{





























































































}

Datum
spg_text_inner_consistent(PG_FUNCTION_ARGS)
{



























































































































































}

Datum
spg_text_leaf_consistent(PG_FUNCTION_ARGS)
{

































































































































}