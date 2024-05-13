/*-------------------------------------------------------------------------
 *
 * hashpage.c
 *	  Hash table page management code for the Postgres hash access method
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashpage.c
 *
 * NOTES
 *	  Postgres hash pages look like ordinary relation pages.  The opaque
 *	  data at high addresses includes information about the page including
 *	  whether a page is an overflow page or a true bucket, the bucket
 *	  number, and the block numbers of the preceding and following pages
 *	  in the same bucket.
 *
 *	  The first page in a hash relation, page zero, is special -- it stores
 *	  information describing the hash table; it is referred to as the
 *	  "meta page." Pages one and higher store the actual data.
 *
 *	  There are also bitmap pages, which are not manipulated here;
 *	  see hashovfl.c.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "storage/predicate.h"

static bool
_hash_alloc_buckets(Relation rel, BlockNumber firstblock, uint32 nblocks);
static void
_hash_splitbucket(Relation rel, Buffer metabuf, Bucket obucket, Bucket nbucket, Buffer obuf, Buffer nbuf, HTAB *htab, uint32 maxbucket, uint32 highmask, uint32 lowmask);
static void
log_split_page(Relation rel, Buffer buf);

/*
 *	_hash_getbuf() -- Get a buffer by block number for read or write.
 *
 *		'access' must be HASH_READ, HASH_WRITE, or HASH_NOLOCK.
 *		'flags' is a bitwise OR of the allowed page types.
 *
 *		This must be used only to fetch pages that are expected to be
 *valid already.  _hash_checkpage() is applied using the given flags.
 *
 *		When this routine returns, the appropriate lock is set on the
 *		requested buffer and its reference count has been incremented
 *		(ie, the buffer is "locked and pinned").
 *
 *		P_NEW is disallowed because this routine can only be used
 *		to access pages that are known to be before the filesystem EOF.
 *		Extending the index should be done with _hash_getnewbuf.
 */
Buffer
_hash_getbuf(Relation rel, BlockNumber blkno, int access, int flags)
{



















}

/*
 * _hash_getbuf_with_condlock_cleanup() -- Try to get a buffer for cleanup.
 *
 *		We read the page and try to acquire a cleanup lock.  If we get
 *it, we return the buffer; otherwise, we return InvalidBuffer.
 */
Buffer
_hash_getbuf_with_condlock_cleanup(Relation rel, BlockNumber blkno, int flags)
{




















}

/*
 *	_hash_getinitbuf() -- Get and initialize a buffer by block number.
 *
 *		This must be used only to fetch pages that are known to be
 *before the index's filesystem EOF, but are to be filled from scratch.
 *		_hash_pageinit() is applied automatically.  Otherwise it has
 *		effects similar to _hash_getbuf() with access = HASH_WRITE.
 *
 *		When this routine returns, a write lock is set on the
 *		requested buffer and its reference count has been incremented
 *		(ie, the buffer is "locked and pinned").
 *
 *		P_NEW is disallowed because this routine can only be used
 *		to access pages that are known to be before the filesystem EOF.
 *		Extending the index should be done with _hash_getnewbuf.
 */
Buffer
_hash_getinitbuf(Relation rel, BlockNumber blkno)
{















}

/*
 *	_hash_initbuf() -- Get and initialize a buffer by bucket number.
 */
void
_hash_initbuf(Buffer buf, uint32 max_bucket, uint32 num_bucket, uint32 flag, bool initpage)
{























}

/*
 *	_hash_getnewbuf() -- Get a new page at the end of the index.
 *
 *		This has the same API as _hash_getinitbuf, except that we are
 *adding a page to the index, and hence expect the page to be past the logical
 *EOF.  (However, we have to support the case where it isn't, since a prior try
 *might have crashed after extending the filesystem EOF but before updating the
 *metapage to reflect the added page.)
 *
 *		It is caller's responsibility to ensure that only one process
 *can extend the index at a time.  In practice, this function is called only
 *while holding write lock on the metapage, because adding a page is always
 *associated with an update of metapage data.
 */
Buffer
_hash_getnewbuf(Relation rel, BlockNumber blkno, ForkNumber forkNum)
{

































}

/*
 *	_hash_getbuf_with_strategy() -- Get a buffer with nondefault strategy.
 *
 *		This is identical to _hash_getbuf() but also allows a buffer
 *access strategy to be specified.  We use this for VACUUM operations.
 */
Buffer
_hash_getbuf_with_strategy(Relation rel, BlockNumber blkno, int access, int flags, BufferAccessStrategy bstrategy)
{



















}

/*
 *	_hash_relbuf() -- release a locked buffer.
 *
 * Lock and pin (refcount) are both dropped.
 */
void
_hash_relbuf(Relation rel, Buffer buf)
{

}

/*
 *	_hash_dropbuf() -- release an unlocked buffer.
 *
 * This is used to unpin a buffer on which we hold no lock.
 */
void
_hash_dropbuf(Relation rel, Buffer buf)
{

}

/*
 *	_hash_dropscanbuf() -- release buffers used in scan.
 *
 * This routine unpins the buffers used during scan on which we
 * hold no lock.
 */
void
_hash_dropscanbuf(Relation rel, HashScanOpaque so)
{
























}

/*
 *	_hash_init() -- Initialize the metadata page of a hash index,
 *				the initial buckets, and the initial bitmap
 *page.
 *
 * The initial number of buckets is dependent on num_tuples, an estimate
 * of the number of tuples to be loaded into the index initially.  The
 * chosen number of buckets is returned.
 *
 * We are fairly cavalier about locking here, since we know that no one else
 * could be accessing this index.  In particular the rule about not holding
 * multiple buffer locks is ignored.
 */
uint32
_hash_init(Relation rel, double num_tuples, ForkNumber forkNum)
{


































































































































































}

/*
 *	_hash_init_metabuffer() -- Initialize the metadata page of a hash index.
 */
void
_hash_init_metabuffer(Buffer buf, double num_tuples, RegProcedure procid, uint16 ffactor, bool initpage)
{





































































































}

/*
 *	_hash_pageinit() -- Initialize a new hash index page.
 */
void
_hash_pageinit(Page page, Size size)
{

}

/*
 * Attempt to expand the hash table by creating one new bucket.
 *
 * This will silently do nothing if we don't get cleanup lock on old or
 * new bucket.
 *
 * Complete the pending splits and remove the tuples from old bucket,
 * if there are any left over from the previous split.
 *
 * The caller must hold a pin, but no lock, on the metapage buffer.
 * The buffer is returned in the same state.
 */
void
_hash_expandtable(Relation rel, Buffer metabuf)
{
























































































































































































































































































































































}

/*
 * _hash_alloc_buckets -- allocate a new splitpoint's worth of bucket pages
 *
 * This does not need to initialize the new bucket pages; we'll do that as
 * each one is used by _hash_expandtable().  But we have to extend the logical
 * EOF to the end of the splitpoint; this keeps smgr's idea of the EOF in
 * sync with ours, so that we don't get complaints from smgr.
 *
 * We do this by writing a page of zeroes at the end of the splitpoint range.
 * We expect that the filesystem will ensure that the intervening pages read
 * as zeroes too.  On many filesystems this "hole" will not be allocated
 * immediately, which means that the index file may end up more fragmented
 * than if we forced it all to be allocated now; but since we don't scan
 * hash indexes sequentially anyway, that probably doesn't matter.
 *
 * XXX It's annoying that this code is executed with the metapage lock held.
 * We need to interlock against _hash_addovflpage() adding a new overflow page
 * concurrently, but it'd likely be better to use LockRelationForExtension
 * for the purpose.  OTOH, adding a splitpoint is a very infrequent operation,
 * so it may not be worth worrying about.
 *
 * Returns true if successful, or false if allocation failed due to
 * BlockNumber overflow.
 */
static bool
_hash_alloc_buckets(Relation rel, BlockNumber firstblock, uint32 nblocks)
{










































}

/*
 * _hash_splitbucket -- split 'obucket' into 'obucket' and 'nbucket'
 *
 * This routine is used to partition the tuples between old and new bucket and
 * is used to finish the incomplete split operations.  To finish the previously
 * interrupted split operation, the caller needs to fill htab.  If htab is set,
 * then we skip the movement of tuples that exists in htab, otherwise NULL
 * value of htab indicates movement of all the tuples that belong to the new
 * bucket.
 *
 * We are splitting a bucket that consists of a base bucket page and zero
 * or more overflow (bucket chain) pages.  We must relocate tuples that
 * belong in the new bucket.
 *
 * The caller must hold cleanup locks on both buckets to ensure that
 * no one else is trying to access them (see README).
 *
 * The caller must hold a pin, but no lock, on the metapage buffer.
 * The buffer is returned in the same state.  (The metapage is only
 * touched if it becomes necessary to add or remove overflow pages.)
 *
 * Split needs to retain pin on primary bucket pages of both old and new
 * buckets till end of operation.  This is to prevent vacuum from starting
 * while a split is in progress.
 *
 * In addition, the caller must have created the new bucket's base page,
 * which is passed in buffer nbuf, pinned and write-locked.  The lock will be
 * released here and pin must be released by the caller.  (The API is set up
 * this way because we must do _hash_getnewbuf() before releasing the metapage
 * write lock.  So instead of passing the new bucket's start block number, we
 * pass an actual buffer.)
 */
static void
_hash_splitbucket(Relation rel, Buffer metabuf, Bucket obucket, Bucket nbucket, Buffer obuf, Buffer nbuf, HTAB *htab, uint32 maxbucket, uint32 highmask, uint32 lowmask)
{










































































































































































































































































}

/*
 *	_hash_finish_split() -- Finish the previously interrupted split
 *operation
 *
 * To complete the split operation, we form the hash table of TIDs in new
 * bucket which is then used by split operation to skip tuples that are
 * already moved before the split operation was previously interrupted.
 *
 * The caller must hold a pin, but no lock, on the metapage and old bucket's
 * primary page buffer.  The buffers are returned in the same state.  (The
 * metapage is only touched if it becomes necessary to add or remove overflow
 * pages.)
 */
void
_hash_finish_split(Relation rel, Buffer metabuf, Buffer obuf, Bucket obucket, uint32 maxbucket, uint32 highmask, uint32 lowmask)
{







































































































}

/*
 *	log_split_page() -- Log the split operation
 *
 *	We log the split operation when the new page in new bucket gets full,
 *	so we log the entire page.
 *
 *	'buf' must be locked by the caller which is also responsible for
 *unlocking it.
 */
static void
log_split_page(Relation rel, Buffer buf)
{












}

/*
 *	_hash_getcachedmetap() -- Returns cached metapage data.
 *
 *	If metabuf is not InvalidBuffer, caller must hold a pin, but no lock, on
 *	the metapage.  If not set, we'll set it before returning if we have to
 *	refresh the cache, and return with a pin but no lock on it; caller is
 *	responsible for releasing the pin.
 *
 *	We refresh the cache if it's not initialized yet or force_refresh is
 *true.
 */
HashMetaPage
_hash_getcachedmetap(Relation rel, Buffer *metabuf, bool force_refresh)
{









































}

/*
 *	_hash_getbucketbuf_from_hashkey() -- Get the bucket's buffer for the
 *given hashkey.
 *
 *	Bucket pages do not move or get removed once they are allocated. This
 *give us an opportunity to use the previously saved metapage contents to reach
 *	the target bucket buffer, instead of reading from the metapage every
 *time. This saves one buffer access every time we want to reach the target
 *bucket buffer, which is very helpful savings in bufmgr traffic and contention.
 *
 *	The access type parameter (HASH_READ or HASH_WRITE) indicates whether
 *the bucket buffer has to be locked for reading or writing.
 *
 *	The out parameter cachedmetap is set with metapage contents used for
 *	hashkey to bucket buffer mapping. Some callers need this info to reach
 *the old bucket in case of bucket split, see _hash_doinsert().
 */
Buffer
_hash_getbucketbuf_from_hashkey(Relation rel, uint32 hashkey, int access, HashMetaPage *cachedmetap)
{


























































}