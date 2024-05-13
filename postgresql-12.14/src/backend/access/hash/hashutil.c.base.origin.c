/*-------------------------------------------------------------------------
 *
 * hashutil.c
 *	  Utility code for Postgres hash implementation.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashutil.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "storage/buf_internals.h"

#define CALC_NEW_BUCKET(old_bucket, lowmask) old_bucket | (lowmask + 1)

/*
 * _hash_checkqual -- does the index tuple satisfy the scan conditions?
 */
bool
_hash_checkqual(IndexScanDesc scan, IndexTuple itup)
{










































}

/*
 * _hash_datum2hashkey -- given a Datum, call the index's hash function
 *
 * The Datum is assumed to be of the index's column type, so we can use the
 * "primary" hash function that's tracked for us by the generic index code.
 */
uint32
_hash_datum2hashkey(Relation rel, Datum key)
{








}

/*
 * _hash_datum2hashkey_type -- given a Datum of a specified type,
 *			hash it in a fashion compatible with this index
 *
 * This is much more expensive than _hash_datum2hashkey, so use it only in
 * cross-type situations.
 */
uint32
_hash_datum2hashkey_type(Relation rel, Datum key, Oid keytype)
{












}

/*
 * _hash_hashkey2bucket -- determine which bucket the hashkey maps to.
 */
Bucket
_hash_hashkey2bucket(uint32 hashkey, uint32 maxbucket, uint32 highmask, uint32 lowmask)
{









}

/*
 * _hash_log2 -- returns ceil(lg2(num))
 */
uint32
_hash_log2(uint32 num)
{






}

/*
 * _hash_spareindex -- returns spare index / global splitpoint phase of the
 *					   bucket
 */
uint32
_hash_spareindex(uint32 num_bucket)
{




















}

/*
 *	_hash_get_totalbuckets -- returns total number of buckets allocated till
 *							the given splitpoint
 *phase.
 */
uint32
_hash_get_totalbuckets(uint32 splitpoint_phase)
{





















}

/*
 * _hash_checkpage -- sanity checks on the format of all hash pages
 *
 * If flags is not zero, it is a bitwise OR of the acceptable page types
 * (values of hasho_flag & LH_PAGE_TYPE).
 */
void
_hash_checkpage(Relation rel, Buffer buf, int flags)
{
















































}

bytea *
hashoptions(Datum reloptions, bool validate)
{

}

/*
 * _hash_get_indextuple_hashkey - get the hash index tuple's hash key value
 */
uint32
_hash_get_indextuple_hashkey(IndexTuple itup)
{








}

/*
 * _hash_convert_tuple - convert raw index data to hash key
 *
 * Inputs: values and isnull arrays for the user data column(s)
 * Outputs: values and isnull arrays for the index tuple, suitable for
 *		passing to index_form_tuple().
 *
 * Returns true if successful, false if not (because there are null values).
 * On a false result, the given data need not be indexed.
 *
 * Note: callers know that the index-column arrays are always of length 1.
 * In principle, there could be more than one input column, though we do not
 * currently support that.
 */
bool
_hash_convert_tuple(Relation index, Datum *user_values, bool *user_isnull, Datum *index_values, bool *index_isnull)
{















}

/*
 * _hash_binsearch - Return the offset number in the page where the
 *					 specified hash value should be sought
 *or inserted.
 *
 * We use binary search, relying on the assumption that the existing entries
 * are ordered by hash key.
 *
 * Returns the offset of the first index entry having hashkey >= hash_value,
 * or the page's max offset plus one if hash_value is greater than all
 * existing hash keys in the page.  This is the appropriate place to start
 * a search, or to insert a new item.
 */
OffsetNumber
_hash_binsearch(Page page, uint32 hash_value)
{





























}

/*
 * _hash_binsearch_last
 *
 * Same as above, except that if there are multiple matching items in the
 * page, we return the offset of the last one instead of the first one,
 * and the possible range of outputs is 0..maxoffset not 1..maxoffset+1.
 * This is handy for starting a new page in a backwards scan.
 */
OffsetNumber
_hash_binsearch_last(Page page, uint32 hash_value)
{





























}

/*
 *	_hash_get_oldblock_from_newbucket() -- get the block number of a bucket
 *			from which current (new) bucket is being split.
 */
BlockNumber
_hash_get_oldblock_from_newbucket(Relation rel, Bucket new_bucket)
{

























}

/*
 *	_hash_get_newblock_from_oldbucket() -- get the block number of a bucket
 *			that will be generated after split from old bucket.
 *
 * This is used to find the new bucket from old bucket based on current table
 * half.  It is mainly required to finish the incomplete splits where we are
 * sure that not more than one bucket could have split in progress from old
 * bucket.
 */
BlockNumber
_hash_get_newblock_from_oldbucket(Relation rel, Bucket old_bucket)
{














}

/*
 *	_hash_get_newbucket_from_oldbucket() -- get the new bucket that will be
 *			generated after split from current (old) bucket.
 *
 * This is used to find the new bucket from old bucket.  New bucket can be
 * obtained by OR'ing old bucket with most significant bit of current table
 * half (lowmask passed in this function can be used to identify msb of
 * current table half).  There could be multiple buckets that could have
 * been split from current bucket.  We need the first such bucket that exists.
 * Caller must ensure that no more than one split has happened from old
 * bucket.
 */
Bucket
_hash_get_newbucket_from_oldbucket(Relation rel, Bucket old_bucket, uint32 lowmask, uint32 maxbucket)
{










}

/*
 * _hash_kill_items - set LP_DEAD state for items an indexscan caller has
 * told us were killed.
 *
 * scan->opaque, referenced locally through so, contains information about the
 * current page and killed tuples thereon (generally, this should only be
 * called if so->numKilled > 0).
 *
 * The caller does not have a lock on the page and may or may not have the
 * page pinned in a buffer.  Note that read-lock is sufficient for setting
 * LP_DEAD status (which is only a hint).
 *
 * The caller must have pin on bucket buffer, but may or may not have pin
 * on overflow buffer, as indicated by HashScanPosIsPinned(so->currPos).
 *
 * We match items by heap TID before assuming they are the right ones to
 * delete.
 *
 * There are never any scans active in a bucket at the time VACUUM begins,
 * because VACUUM takes a cleanup lock on the primary bucket page and scans
 * hold a pin.  A scan can begin after VACUUM leaves the primary bucket page
 * but before it finishes the entire bucket, but it can never pass VACUUM,
 * because VACUUM always locks the next page before releasing the lock on
 * the previous one.  Therefore, we don't have to worry about accidentally
 * killing a TID that has been reused for an unrelated tuple.
 */
void
_hash_kill_items(IndexScanDesc scan)
{






















































































}