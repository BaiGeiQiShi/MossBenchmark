/*-------------------------------------------------------------------------
 *
 * hashsort.c
 *		Sort tuples for insertion into a new hash index.
 *
 * When building a very large hash index, we pre-sort the tuples by bucket
 * number to improve locality of access to the index, and thereby avoid
 * thrashing.  We use tuplesort.c to sort the given index tuples into order.
 *
 * Note: if the number of rows in the table has been underestimated,
 * bucket splits may occur during the index build.  In that case we'd
 * be inserting into two or more buckets for each possible masked-off
 * hash code value.  That's no big problem though, since we'll still have
 * plenty of locality of access.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashsort.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "commands/progress.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/tuplesort.h"

/*
 * Status record for spooling/sorting phase.
 */
struct HSpool
{
  Tuplesortstate *sortstate; /* state data for tuplesort.c */
  Relation index;

  /*
   * We sort the hash keys based on the buckets they belong to. Below masks
   * are used in _hash_hashkey2bucket to determine the bucket of given hash
   * key.
   */
  uint32 high_mask;
  uint32 low_mask;
  uint32 max_buckets;
};

/*
 * create and initialize a spool structure
 */
HSpool *
_h_spoolinit(Relation heap, Relation index, uint32 num_buckets)
{
























}

/*
 * clean up a spool structure and its substructures.
 */
void
_h_spooldestroy(HSpool *hspool)
{


}

/*
 * spool an index entry into the sort file.
 */
void
_h_spool(HSpool *hspool, ItemPointer self, Datum *values, bool *isnull)
{

}

/*
 * given a spool loaded by successive calls to _h_spool,
 * create an entire index.
 */
void
_h_indexbuild(HSpool *hspool, Relation heapRel)
{




























}