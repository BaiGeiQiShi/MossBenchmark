/*-------------------------------------------------------------------------
 *
 * gistsplit.c
 *	  Multi-column page splitting algorithm
 *
 * This file is concerned with making good page-split decisions in multi-column
 * GiST indexes.  The opclass-specific picksplit functions can only be expected
 * to produce answers based on a single column.  We first run the picksplit
 * function for column 1; then, if there are more columns, we check if any of
 * the tuples are "don't cares" so far as the column 1 split is concerned
 * (that is, they could go to either side for no additional penalty).  If so,
 * we try to redistribute those tuples on the basis of the next column.
 * Repeat till we're out of columns.
 *
 * gistSplitByKey() is the entry point to this file.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/gist/gistsplit.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/gist_private.h"
#include "utils/rel.h"

typedef struct
{
  OffsetNumber *entries;
  int len;
  Datum *attr;
  bool *isnull;
  bool *dontcare;
} GistSplitUnion;

/*
 * Form unions of subkeys in itvec[] entries listed in gsvp->entries[],
 * ignoring any tuples that are marked in gsvp->dontcare[].  Subroutine for
 * gistunionsubkey.
 */
static void
gistunionsubkeyvec(GISTSTATE *giststate, IndexTuple *itvec, GistSplitUnion *gsvp)
{


















}

/*
 * Recompute unions of left- and right-side subkeys after a page split,
 * ignoring any tuples that are marked in spl->spl_dontcare[].
 *
 * Note: we always recompute union keys for all index columns.  In some cases
 * this might represent duplicate work for the leftmost column(s), but it's
 * not safe to assume that "zero penalty to move a tuple" means "the union
 * key doesn't change at all".  Penalty functions aren't 100% accurate.
 */
static void
gistunionsubkey(GISTSTATE *giststate, IndexTuple *itvec, GistSplitVector *spl)
{

















}

/*
 * Find tuples that are "don't cares", that is could be moved to the other
 * side of the split with zero penalty, so far as the attno column is
 * concerned.
 *
 * Don't-care tuples are marked by setting the corresponding entry in
 * spl->spl_dontcare[] to "true".  Caller must have initialized that array
 * to zeroes.
 *
 * Returns number of don't-cares found.
 */
static int
findDontCares(Relation r, GISTSTATE *giststate, GISTENTRY *valvec, GistSplitVector *spl, int attno)
{







































}

/*
 * Remove tuples that are marked don't-cares from the tuple index array a[]
 * of length *len.  This is applied separately to the spl_left and spl_right
 * arrays.
 */
static void
removeDontCares(OffsetNumber *a, int *len, const bool *dontcare)
{






















}

/*
 * Place a single don't-care tuple into either the left or right side of the
 * split, according to which has least penalty for merging the tuple into
 * the previously-computed union keys.  We need consider only columns starting
 * at attno.
 */
static void
placeOne(Relation r, GISTSTATE *giststate, GistSplitVector *v, IndexTuple itup, OffsetNumber off, int attno)
{


































}

#define SWAPVAR(s, d, t)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (t) = (s);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    (s) = (d);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    (d) = (t);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

/*
 * Clean up when we did a secondary split but the user-defined PickSplit
 * method didn't support it (leaving spl_ldatum_exists or spl_rdatum_exists
 * true).
 *
 * We consider whether to swap the left and right outputs of the secondary
 * split; this can be worthwhile if the penalty for merging those tuples into
 * the previously chosen sets is less that way.
 *
 * In any case we must update the union datums for the current column by
 * adding in the previous union keys (oldL/oldR), since the user-defined
 * PickSplit method didn't do so.
 */
static void
supportSecondarySplit(Relation r, GISTSTATE *giststate, int attno, GIST_SPLITVEC *sv, Datum oldL, Datum oldR)
{










































































}

/*
 * Trivial picksplit implementation. Function called only
 * if user-defined picksplit puts all keys on the same side of the split.
 * That is a bug of user-defined picksplit but we don't want to fail.
 */
static void
genericPickSplit(GISTSTATE *giststate, GistEntryVector *entryvec, GIST_SPLITVEC *v, int attno)
{






































}

/*
 * Calls user picksplit method for attno column to split tuples into
 * two vectors.
 *
 * Returns false if split is complete (there are no more index columns, or
 * there is no need to consider them because split is optimal already).
 *
 * Returns true and v->spl_dontcare = NULL if the picksplit result is
 * degenerate (all tuples seem to be don't-cares), so we should just
 * disregard this column and split on the next column(s) instead.
 *
 * Returns true and v->spl_dontcare != NULL if there are don't-care tuples
 * that could be relocated based on the next column(s).  The don't-care
 * tuples have been removed from the split and must be reinserted by caller.
 * There is at least one non-don't-care tuple on each side of the split,
 * and union keys for all columns are updated to include just those tuples.
 *
 * A true result implies there is at least one more index column.
 */
static bool
gistUserPicksplit(Relation r, GistEntryVector *entryvec, int attno, GistSplitVector *v, IndexTuple *itup, int len, GISTSTATE *giststate)
{





































































































































































}

/*
 * simply split page in half
 */
static void
gistSplitHalf(GIST_SPLITVEC *v, int len)
{


















}

/*
 * gistSplitByKey: main entry point for page-splitting algorithm
 *
 * r: index relation
 * page: page being split
 * itup: array of IndexTuples to be processed
 * len: number of IndexTuples to be processed (must be at least 2)
 * giststate: additional info about index
 * v: working state and output area
 * attno: column we are working on (zero-based index)
 *
 * Outside caller must initialize v->spl_lisnull and v->spl_risnull arrays
 * to all-true.  On return, spl_left/spl_nleft contain indexes of tuples
 * to go left, spl_right/spl_nright contain indexes of tuples to go right,
 * spl_lattr/spl_lisnull contain left-side union key values, and
 * spl_rattr/spl_risnull contain right-side union key values.  Other fields
 * in this struct are workspace for this file.
 *
 * Outside caller must pass zero for attno.  The function may internally
 * recurse to the next column by passing attno+1.
 */
void
gistSplitByKey(Relation r, Page page, IndexTuple *itup, int len, GISTSTATE *giststate, GistSplitVector *v, int attno)
{






































































































































































}