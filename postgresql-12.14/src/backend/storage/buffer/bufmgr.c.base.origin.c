/*-------------------------------------------------------------------------
 *
 * bufmgr.c
 *	  buffer manager interface routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/buffer/bufmgr.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * Principal entry points:
 *
 * ReadBuffer() -- find or create a buffer holding the requested page,
 *		and pin it so that no one can destroy it while this process
 *		is using it.
 *
 * ReleaseBuffer() -- unpin a buffer
 *
 * MarkBufferDirty() -- mark a pinned buffer's contents as "dirty".
 *		The disk write is delayed until buffer replacement or
 *checkpoint.
 *
 * See also these files:
 *		freelist.c -- chooses victim for buffer replacement
 *		buf_table.c -- manages the buffer lookup table
 */
#include "postgres.h"

#include <sys/file.h>
#include <unistd.h>

#include "access/tableam.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/storage.h"
#include "executor/instrument.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "storage/buf_internals.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/smgr.h"
#include "storage/standby.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/timestamp.h"

/* Note: these two macros only work on shared buffers, not local ones! */
#define BufHdrGetBlock(bufHdr) ((Block)(BufferBlocks + ((Size)(bufHdr)->buf_id) * BLCKSZ))
#define BufferGetLSN(bufHdr) (PageGetLSN(BufHdrGetBlock(bufHdr)))

/* Note: this macro only works on local buffers, not shared ones! */
#define LocalBufHdrGetBlock(bufHdr) LocalBufferBlockPointers[-((bufHdr)->buf_id + 2)]

/* Bits in SyncOneBuffer's return value */
#define BUF_WRITTEN 0x01
#define BUF_REUSABLE 0x02

#define DROP_RELS_BSEARCH_THRESHOLD 20

typedef struct PrivateRefCountEntry
{
  Buffer buffer;
  int32 refcount;
} PrivateRefCountEntry;

/* 64 bytes, about the size of a cache line on common systems */
#define REFCOUNT_ARRAY_ENTRIES 8

/*
 * Status of buffers to checkpoint for a particular tablespace, used
 * internally in BufferSync.
 */
typedef struct CkptTsStatus
{
  /* oid of the tablespace */
  Oid tsId;

  /*
   * Checkpoint progress for this tablespace. To make progress comparable
   * between tablespaces the progress is, for each tablespace, measured as a
   * number between 0 and the total number of to-be-checkpointed pages. Each
   * page checkpointed in this tablespace increments this space's progress
   * by progress_slice.
   */
  float8 progress;
  float8 progress_slice;

  /* number of to-be checkpointed pages in this tablespace */
  int num_to_scan;
  /* already processed pages in this tablespace */
  int num_scanned;

  /* current offset in CkptBufferIds for this tablespace */
  int index;
} CkptTsStatus;

/* GUC variables */
bool zero_damaged_pages = false;
int bgwriter_lru_maxpages = 100;
double bgwriter_lru_multiplier = 2.0;
bool track_io_timing = false;
int effective_io_concurrency = 0;

/*
 * GUC variables about triggering kernel writeback for buffers written; OS
 * dependent defaults are set via the GUC mechanism.
 */
int checkpoint_flush_after = 0;
int bgwriter_flush_after = 0;
int backend_flush_after = 0;

/*
 * How many buffers PrefetchBuffer callers should try to stay ahead of their
 * ReadBuffer calls by.  This is maintained by the assign hook for
 * effective_io_concurrency.  Zero means "never prefetch".  This value is
 * only used for buffers not belonging to tablespaces that have their
 * effective_io_concurrency parameter set.
 */
int target_prefetch_pages = 0;

/* local state for StartBufferIO and related functions */
static BufferDesc *InProgressBuf = NULL;
static bool IsForInput;

/* local state for LockBufferForCleanup */
static BufferDesc *PinCountWaitBuf = NULL;

/*
 * Backend-Private refcount management:
 *
 * Each buffer also has a private refcount that keeps track of the number of
 * times the buffer is pinned in the current process.  This is so that the
 * shared refcount needs to be modified only once if a buffer is pinned more
 * than once by an individual backend.  It's also used to check that no buffers
 * are still pinned at the end of transactions and when exiting.
 *
 *
 * To avoid - as we used to - requiring an array with NBuffers entries to keep
 * track of local buffers, we use a small sequentially searched array
 * (PrivateRefCountArray) and an overflow hash table (PrivateRefCountHash) to
 * keep track of backend local pins.
 *
 * Until no more than REFCOUNT_ARRAY_ENTRIES buffers are pinned at once, all
 * refcounts are kept track of in the array; after that, new array entries
 * displace old ones into the hash table. That way a frequently used entry
 * can't get "stuck" in the hashtable while infrequent ones clog the array.
 *
 * Note that in most scenarios the number of pinned buffers will not exceed
 * REFCOUNT_ARRAY_ENTRIES.
 *
 *
 * To enter a buffer into the refcount tracking mechanism first reserve a free
 * entry using ReservePrivateRefCountEntry() and then later, if necessary,
 * fill it with NewPrivateRefCountEntry(). That split lets us avoid doing
 * memory allocations in NewPrivateRefCountEntry() which can be important
 * because in some scenarios it's called with a spinlock held...
 */
static struct PrivateRefCountEntry PrivateRefCountArray[REFCOUNT_ARRAY_ENTRIES];
static HTAB *PrivateRefCountHash = NULL;
static int32 PrivateRefCountOverflowed = 0;
static uint32 PrivateRefCountClock = 0;
static PrivateRefCountEntry *ReservedRefCountEntry = NULL;

static void
ReservePrivateRefCountEntry(void);
static PrivateRefCountEntry *
NewPrivateRefCountEntry(Buffer buffer);
static PrivateRefCountEntry *
GetPrivateRefCountEntry(Buffer buffer, bool do_move);
static inline int32
GetPrivateRefCount(Buffer buffer);
static void
ForgetPrivateRefCountEntry(PrivateRefCountEntry *ref);

/*
 * Ensure that the PrivateRefCountArray has sufficient space to store one more
 * entry. This has to be called before using NewPrivateRefCountEntry() to fill
 * a new entry - but it's perfectly fine to not use a reserved entry.
 */
static void
ReservePrivateRefCountEntry(void)
{
























































}

/*
 * Fill a previously reserved refcount entry.
 */
static PrivateRefCountEntry *
NewPrivateRefCountEntry(Buffer buffer)
{














}

/*
 * Return the PrivateRefCount entry for the passed buffer.
 *
 * Returns NULL if a buffer doesn't have a refcount entry. Otherwise, if
 * do_move is true, and the entry resides in the hashtable the entry is
 * optimized for frequent access by moving it to the array.
 */
static PrivateRefCountEntry *
GetPrivateRefCountEntry(Buffer buffer, bool do_move)
{






































































}

/*
 * Returns how many times the passed buffer is pinned by this backend.
 *
 * Only works for shared memory buffers!
 */
static inline int32
GetPrivateRefCount(Buffer buffer)
{
















}

/*
 * Release resources used to track the reference count of a buffer which we no
 * longer have pinned and don't want to pin again immediately.
 */
static void
ForgetPrivateRefCountEntry(PrivateRefCountEntry *ref)
{























}

/*
 * BufferIsPinned
 *		True iff the buffer is pinned (also checks for valid buffer
 *number).
 *
 *		NOTE: what we check here is that *this* backend holds a pin on
 *		the buffer.  We do not care whether some other backend does.
 */
#define BufferIsPinned(bufnum) (!BufferIsValid(bufnum) ? false : BufferIsLocal(bufnum) ? (LocalRefCount[-(bufnum)-1] > 0) : (GetPrivateRefCount(bufnum) > 0))

static Buffer
ReadBuffer_common(SMgrRelation reln, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy, bool *hit);
static bool
PinBuffer(BufferDesc *buf, BufferAccessStrategy strategy);
static void
PinBuffer_Locked(BufferDesc *buf);
static void
UnpinBuffer(BufferDesc *buf, bool fixOwner);
static void
BufferSync(int flags);
static uint32
WaitBufHdrUnlocked(BufferDesc *buf);
static int
SyncOneBuffer(int buf_id, bool skip_recently_used, WritebackContext *flush_context);
static void
WaitIO(BufferDesc *buf);
static bool
StartBufferIO(BufferDesc *buf, bool forInput);
static void
TerminateBufferIO(BufferDesc *buf, bool clear_dirty, uint32 set_flag_bits);
static void
shared_buffer_write_error_callback(void *arg);
static void
local_buffer_write_error_callback(void *arg);
static BufferDesc *
BufferAlloc(SMgrRelation smgr, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, BufferAccessStrategy strategy, bool *foundPtr);
static void
FlushBuffer(BufferDesc *buf, SMgrRelation reln);
static void
AtProcExit_Buffers(int code, Datum arg);
static void
CheckForBufferLeaks(void);
static int
rnode_comparator(const void *p1, const void *p2);
static int
buffertag_comparator(const void *p1, const void *p2);
static int
ckpt_buforder_comparator(const void *pa, const void *pb);
static int
ts_ckpt_progress_comparator(Datum a, Datum b, void *arg);

/*
 * ComputeIoConcurrency -- get the number of pages to prefetch for a given
 *		number of spindles.
 */
bool
ComputeIoConcurrency(int io_concurrency, double *target)
{



















































}

/*
 * PrefetchBuffer -- initiate asynchronous read of a block of a relation
 *
 * This is named by analogy to ReadBuffer but doesn't actually allocate a
 * buffer.  Instead it tries to ensure that a future ReadBuffer for the given
 * block will not be delayed by the I/O.  Prefetching is optional.
 * No-op if prefetching isn't compiled in.
 */
void
PrefetchBuffer(Relation reln, ForkNumber forkNum, BlockNumber blockNum)
{





















































}

/*
 * ReadBuffer -- a shorthand for ReadBufferExtended, for reading from main
 *		fork with RBM_NORMAL mode and default strategy.
 */
Buffer
ReadBuffer(Relation reln, BlockNumber blockNum)
{

}

/*
 * ReadBufferExtended -- returns a buffer containing the requested
 *		block of the requested relation.  If the blknum
 *		requested is P_NEW, extend the relation file and
 *		allocate a new block.  (Caller is responsible for
 *		ensuring that only one backend tries to extend a
 *		relation at the same time!)
 *
 * Returns: the buffer number for the buffer containing
 *		the block read.  The returned buffer has been pinned.
 *		Does not return on error --- elog's instead.
 *
 * Assume when this function is called, that reln has been opened already.
 *
 * In RBM_NORMAL mode, the page is read from disk, and the page header is
 * validated.  An error is thrown if the page header is not valid.  (But
 * note that an all-zero page is considered "valid"; see
 * PageIsVerifiedExtended().)
 *
 * RBM_ZERO_ON_ERROR is like the normal mode, but if the page header is not
 * valid, the page is zeroed instead of throwing an error. This is intended
 * for non-critical data, where the caller is prepared to repair errors.
 *
 * In RBM_ZERO_AND_LOCK mode, if the page isn't in buffer cache already, it's
 * filled with zeros instead of reading it from disk.  Useful when the caller
 * is going to fill the page from scratch, since this saves I/O and avoids
 * unnecessary failure if the page-on-disk has corrupt page headers.
 * The page is returned locked to ensure that the caller has a chance to
 * initialize the page before it's made visible to others.
 * Caution: do not use this mode to read a page that is beyond the relation's
 * current physical EOF; that is likely to cause problems in md.c when
 * the page is modified and written out. P_NEW is OK, though.
 *
 * RBM_ZERO_AND_CLEANUP_LOCK is the same as RBM_ZERO_AND_LOCK, but acquires
 * a cleanup-strength lock on the page.
 *
 * RBM_NORMAL_NO_LOG mode is treated the same as RBM_NORMAL here.
 *
 * If strategy is not NULL, a nondefault buffer access strategy is used.
 * See buffer/README for details.
 */
Buffer
ReadBufferExtended(Relation reln, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy)
{
























}

/*
 * ReadBufferWithoutRelcache -- like ReadBufferExtended, but doesn't require
 *		a relcache entry for the relation.
 *
 * NB: At present, this function may only be used on permanent relations, which
 * is OK, because we only use it during XLOG replay.  If in the future we
 * want to use it on temporary or unlogged relations, we could pass additional
 * parameters.
 */
Buffer
ReadBufferWithoutRelcache(RelFileNode rnode, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy)
{







}

/*
 * ReadBuffer_common -- common logic for all ReadBuffer variants
 *
 * *hit is set to true if the request was satisfied from shared buffer cache.
 */
static Buffer
ReadBuffer_common(SMgrRelation smgr, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy, bool *hit)
{









































































































































































































































































}

/*
 * BufferAlloc -- subroutine for ReadBuffer.  Handles lookup of a shared
 *		buffer.  If no buffer exists already, selects a replacement
 *		victim and evicts the old page, but does NOT read in new page.
 *
 * "strategy" can be a buffer replacement strategy object, or NULL for
 * the default strategy.  The selected buffer's usage_count is advanced when
 * using the default strategy, but otherwise possibly not (see PinBuffer).
 *
 * The returned buffer is pinned and is already marked as holding the
 * desired page.  If it already did have the desired page, *foundPtr is
 * set true.  Otherwise, *foundPtr is set false and the buffer is marked
 * as IO_IN_PROGRESS; ReadBuffer will now need to do I/O to fill it.
 *
 * *foundPtr is actually redundant with the buffer's BM_VALID flag, but
 * we keep it for simplicity in ReadBuffer.
 *
 * No locks are held either at entry or exit.
 */
static BufferDesc *
BufferAlloc(SMgrRelation smgr, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, BufferAccessStrategy strategy, bool *foundPtr)
{























































































































































































































































































































































}

/*
 * InvalidateBuffer -- mark a shared buffer invalid and return it to the
 * freelist.
 *
 * The buffer header spinlock must be held at entry.  We drop it before
 * returning.  (This is sane because the caller must have locked the
 * buffer in order to be sure it should be dropped.)
 *
 * This is used only in contexts such as dropping a relation.  We assume
 * that no other backend could possibly be interested in using the page,
 * so the only reason the buffer might be pinned is if someone else is
 * trying to write it out.  We have to let them finish before we can
 * reclaim the buffer.
 *
 * The buffer could get reclaimed by someone else while we are waiting
 * to acquire the necessary locks; if so, don't mess it up.
 */
static void
InvalidateBuffer(BufferDesc *buf)
{
























































































}

/*
 * MarkBufferDirty
 *
 *		Marks buffer contents as dirty (actual write happens later).
 *
 * Buffer must be pinned and exclusive-locked.  (If caller does not hold
 * exclusive lock, then somebody could be in process of writing the buffer,
 * leading to risk of bad data written to disk.)
 */
void
MarkBufferDirty(Buffer buffer)
{



















































}

/*
 * ReleaseAndReadBuffer -- combine ReleaseBuffer() and ReadBuffer()
 *
 * Formerly, this saved one cycle of acquiring/releasing the BufMgrLock
 * compared to calling the two routines separately.  Now it's mainly just
 * a convenience function.  However, if the passed buffer is valid and
 * already contains the desired block, we just return it as-is; and that
 * does save considerable work compared to a full release and reacquire.
 *
 * Note: it is OK to pass buffer == InvalidBuffer, indicating that no old
 * buffer actually needs to be released.  This case is the same as ReadBuffer,
 * but can save some tests in the caller.
 */
Buffer
ReleaseAndReadBuffer(Buffer buffer, Relation relation, BlockNumber blockNum)
{





























}

/*
 * PinBuffer -- make buffer unavailable for replacement.
 *
 * For the default access strategy, the buffer's usage_count is incremented
 * when we first pin it; for other strategies we just make sure the usage_count
 * isn't zero.  (The idea of the latter is that we don't want synchronized
 * heap scans to inflate the count, but we need it to not be zero to discourage
 * other backends from stealing buffers from our ring.  As long as we cycle
 * through the ring faster than the global clock-sweep cycles, buffers in
 * our ring won't be chosen as victims for replacement by other backends.)
 *
 * This should be applied only to shared buffers, never local ones.
 *
 * Since buffers are pinned/unpinned very frequently, pin buffers without
 * taking the buffer header lock; instead update the state variable in loop of
 * CAS operations. Hopefully it's just a single CAS.
 *
 * Note that ResourceOwnerEnlargeBuffers must have been done already.
 *
 * Returns true if buffer is BM_VALID, else false.  This provision allows
 * some callers to avoid an extra spinlock cycle.
 */
static bool
PinBuffer(BufferDesc *buf, BufferAccessStrategy strategy)
{
































































}

/*
 * PinBuffer_Locked -- as above, but caller already locked the buffer header.
 * The spinlock is released before return.
 *
 * As this function is called with the spinlock held, the caller has to
 * previously call ReservePrivateRefCountEntry().
 *
 * Currently, no callers of this function want to modify the buffer's
 * usage_count at all, so there's no need for a strategy parameter.
 * Also we don't bother with a BM_VALID test (the caller could check that for
 * itself).
 *
 * Also all callers only ever use this function when it's known that the
 * buffer can't have a preexisting pin by this backend. That allows us to skip
 * searching the private refcount array & hash, which is a boon, because the
 * spinlock is still held.
 *
 * Note: use of this routine is frequently mandatory, not just an optimization
 * to save a spin lock/unlock cycle, because we need to pin a buffer before
 * its state can change under us.
 */
static void
PinBuffer_Locked(BufferDesc *buf)
{

























}

/*
 * UnpinBuffer -- make buffer available for replacement.
 *
 * This should be applied only to shared buffers, never local ones.
 *
 * Most but not all callers want CurrentResourceOwner to be adjusted.
 * Those that don't should pass fixOwner = false.
 */
static void
UnpinBuffer(BufferDesc *buf, bool fixOwner)
{











































































}

/*
 * BufferSync -- Write out all dirty buffers in the pool.
 *
 * This is called at checkpoint time to write out all dirty shared buffers.
 * The checkpoint request flags should be passed in.  If CHECKPOINT_IMMEDIATE
 * is set, we disable delays between writes; if CHECKPOINT_IS_SHUTDOWN,
 * CHECKPOINT_END_OF_RECOVERY or CHECKPOINT_FLUSH_ALL is set, we write even
 * unlogged buffers, which are otherwise skipped.  The remaining flags
 * currently have no effect here.
 */
static void
BufferSync(int flags)
{






























































































































































































































































}

/*
 * BgBufferSync -- Write out some dirty buffers in the pool.
 *
 * This is called periodically by the background writer process.
 *
 * Returns true if it's appropriate for the bgwriter process to go into
 * low-power hibernation mode.  (This happens if the strategy clock sweep
 * has been "lapped" and no buffer allocations have occurred recently,
 * or if the bgwriter has been effectively disabled by setting
 * bgwriter_lru_maxpages to 0.)
 */
bool
BgBufferSync(WritebackContext *wb_context)
{











































































































































































































































































}

/*
 * SyncOneBuffer -- process a single buffer during syncing.
 *
 * If skip_recently_used is true, we don't write currently-pinned buffers, nor
 * buffers marked recently used, as these are not replacement candidates.
 *
 * Returns a bitmask containing the following flag bits:
 *	BUF_WRITTEN: we wrote the buffer.
 *	BUF_REUSABLE: buffer is available for replacement, ie, it has
 *		pin count 0 and usage count 0.
 *
 * (BUF_WRITTEN could be set in error if FlushBuffers finds the buffer clean
 * after locking it, but we don't care all that much.)
 *
 * Note: caller must have done ResourceOwnerEnlargeBuffers.
 */
static int
SyncOneBuffer(int buf_id, bool skip_recently_used, WritebackContext *wb_context)
{






















































}

/*
 *		AtEOXact_Buffers - clean up at end of transaction.
 *
 *		As of PostgreSQL 8.0, buffer pins should get released by the
 *		ResourceOwner mechanism.  This routine is just a debugging
 *		cross-check that no pins remain.
 */
void
AtEOXact_Buffers(bool isCommit)
{





}

/*
 * Initialize access to shared buffer pool
 *
 * This is called during backend startup (whether standalone or under the
 * postmaster).  It sets up for this backend's access to the already-existing
 * buffer pool.
 *
 * NB: this is called before InitProcess(), so we do not have a PGPROC and
 * cannot do LWLockAcquire; hence we can't actually access stuff in
 * shared memory yet.  We are only initializing local data here.
 * (See also InitBufferPoolBackend)
 */
void
InitBufferPoolAccess(void)
{









}

/*
 * InitBufferPoolBackend --- second-stage initialization of a new backend
 *
 * This is called after we have acquired a PGPROC and so can safely get
 * LWLocks.  We don't currently need to do anything at this stage ...
 * except register a shmem-exit callback.  AtProcExit_Buffers needs LWLock
 * access, and thereby has to be called at the corresponding phase of
 * backend shutdown.
 */
void
InitBufferPoolBackend(void)
{

}

/*
 * During backend exit, ensure that we released all shared-buffer locks and
 * assert that we have no remaining pins.
 */
static void
AtProcExit_Buffers(int code, Datum arg)
{







}

/*
 *		CheckForBufferLeaks - ensure this backend holds no buffer pins
 *
 *		As of PostgreSQL 8.0, buffer pins should get released by the
 *		ResourceOwner mechanism.  This routine is just a debugging
 *		cross-check that no pins remain.
 */
static void
CheckForBufferLeaks(void)
{
































}

/*
 * Helper routine to issue warnings when a buffer is unexpectedly pinned
 */
void
PrintBufferLeakWarning(Buffer buffer)
{

























}

/*
 * CheckPointBuffers
 *
 * Flush all dirty blocks in buffer pool to disk at checkpoint time.
 *
 * Note: temporary relations do not participate in checkpoints, so they don't
 * need to be flushed.
 */
void
CheckPointBuffers(int flags)
{








}

/*
 * Do whatever is needed to prepare for commit at the bufmgr and smgr levels
 */
void
BufmgrCommit(void)
{
}

/*
 * BufferGetBlockNumber
 *		Returns the block number associated with a buffer.
 *
 * Note:
 *		Assumes that the buffer is valid and pinned, else the
 *		value may be obsolete immediately...
 */
BlockNumber
BufferGetBlockNumber(Buffer buffer)
{















}

/*
 * BufferGetTag
 *		Returns the relfilenode, fork number and block number associated
 *with a buffer.
 */
void
BufferGetTag(Buffer buffer, RelFileNode *rnode, ForkNumber *forknum, BlockNumber *blknum)
{


















}

/*
 * FlushBuffer
 *		Physically write out a shared buffer.
 *
 * NOTE: this actually just passes the buffer contents to the kernel; the
 * real write to disk won't happen until the kernel feels like it.  This
 * is okay from our point of view since we can redo the changes from WAL.
 * However, we will need to force the changes to disk via fsync before
 * we can checkpoint WAL.
 *
 * The caller must hold a pin on the buffer and have share-locked the
 * buffer contents.  (Note: a share-lock does not prevent updates of
 * hint bits in the buffer, so the page could change while the write
 * is in progress, but we assume that that will not invalidate the data
 * written.)
 *
 * If the caller has an smgr reference for the buffer's relation, pass it
 * as the second parameter.  If not, pass NULL.
 */
static void
FlushBuffer(BufferDesc *buf, SMgrRelation reln)
{













































































































}

/*
 * RelationGetNumberOfBlocksInFork
 *		Determines the current number of pages in the specified relation
 *fork.
 *
 * Note that the accuracy of the result will depend on the details of the
 * relation's storage. For builtin AMs it'll be accurate, but for external AMs
 * it might not be.
 */
BlockNumber
RelationGetNumberOfBlocksInFork(Relation relation, ForkNumber forkNum)
{

































}

/*
 * BufferIsPermanent
 *		Determines whether a buffer will potentially still be around
 *after a crash.  Caller must hold a buffer pin.
 */
bool
BufferIsPermanent(Buffer buffer)
{





















}

/*
 * BufferGetLSNAtomic
 *		Retrieves the LSN of the buffer atomically using a buffer header
 *lock. This is necessary for some callers who may not have an exclusive lock on
 *the buffer.
 */
XLogRecPtr
BufferGetLSNAtomic(Buffer buffer)
{






















}

/* ---------------------------------------------------------------------
 *		DropRelFileNodeBuffers
 *
 *		This function removes from the buffer pool all the pages of the
 *		specified relation fork that have block numbers >=
 *firstDelBlock. (In particular, with firstDelBlock = 0, all pages are removed.)
 *		Dirty pages are simply dropped, without bothering to write them
 *		out first.  Therefore, this is NOT rollback-able, and so should
 *be used only with extreme caution!
 *
 *		Currently, this is called only from smgr.c when the underlying
 *file is about to be deleted or truncated (firstDelBlock is needed for the
 *truncation case).  The data in the affected pages would therefore be deleted
 *momentarily anyway, and there is no point in writing it. It is the
 *responsibility of higher-level code to ensure that the deletion or truncation
 *does not lose any data that could be needed later.  It is also the
 *responsibility of higher-level code to ensure that no other process could be
 *trying to load more pages of the relation into buffers.
 *
 *		XXX currently it sequentially searches the buffer pool, should
 *be changed to more clever ways of searching.  However, this routine is used
 *only in code paths that aren't very performance-critical, and we shouldn't
 *slow down the hot paths to make it faster ...
 * --------------------------------------------------------------------
 */
void
DropRelFileNodeBuffers(RelFileNodeBackend rnode, ForkNumber forkNum, BlockNumber firstDelBlock)
{
















































}

/* ---------------------------------------------------------------------
 *		DropRelFileNodesAllBuffers
 *
 *		This function removes from the buffer pool all the pages of all
 *		forks of the specified relations.  It's equivalent to calling
 *		DropRelFileNodeBuffers once per fork per relation with
 *		firstDelBlock = 0.
 * --------------------------------------------------------------------
 */
void
DropRelFileNodesAllBuffers(RelFileNodeBackend *rnodes, int nnodes)
{


































































































}

/* ---------------------------------------------------------------------
 *		DropDatabaseBuffers
 *
 *		This function removes all the buffers in the buffer cache for a
 *		particular database.  Dirty pages are simply dropped, without
 *		bothering to write them out first.  This is used when we destroy
 *a database, to avoid trying to flush data to disk when the directory tree no
 *longer exists.  Implementation is pretty similar to DropRelFileNodeBuffers()
 *which is for destroying just one relation.
 * --------------------------------------------------------------------
 */
void
DropDatabaseBuffers(Oid dbid)
{































}

/* -----------------------------------------------------------------
 *		PrintBufferDescs
 *
 *		this function prints all the buffer descriptors, for debugging
 *		use only.
 * -----------------------------------------------------------------
 */
#ifdef NOT_USED
void
PrintBufferDescs(void)
{
  int i;

  for (i = 0; i < NBuffers; ++i)
  {
    BufferDesc *buf = GetBufferDescriptor(i);
    Buffer b = BufferDescriptorGetBuffer(buf);

    /* theoretically we should lock the bufhdr here */
    elog(LOG, "[%02d] (freeNext=%d, rel=%s, blockNum=%u, flags=0x%x, refcount=%u %d)", i, buf->freeNext, relpathbackend(buf->tag.rnode, InvalidBackendId, buf->tag.forkNum), buf->tag.blockNum, buf->flags, buf->refcount, GetPrivateRefCount(b));
  }
}
#endif

#ifdef NOT_USED
void
PrintPinnedBufs(void)
{
  int i;

  for (i = 0; i < NBuffers; ++i)
  {
    BufferDesc *buf = GetBufferDescriptor(i);
    Buffer b = BufferDescriptorGetBuffer(buf);

    if (GetPrivateRefCount(b) > 0)
    {
      /* theoretically we should lock the bufhdr here */
      elog(LOG, "[%02d] (freeNext=%d, rel=%s, blockNum=%u, flags=0x%x, refcount=%u %d)", i, buf->freeNext, relpathperm(buf->tag.rnode, buf->tag.forkNum), buf->tag.blockNum, buf->flags, buf->refcount, GetPrivateRefCount(b));
    }
  }
}
#endif

/* ---------------------------------------------------------------------
 *		FlushRelationBuffers
 *
 *		This function writes all dirty pages of a relation out to disk
 *		(or more accurately, out to kernel disk buffers), ensuring that
 *the kernel has an up-to-date view of the relation.
 *
 *		Generally, the caller should be holding AccessExclusiveLock on
 *the target relation to ensure that no other backend is busy dirtying more
 *blocks of the relation; the effects can't be expected to last after the lock
 *is released.
 *
 *		XXX currently it sequentially searches the buffer pool, should
 *be changed to more clever ways of searching.  This routine is not used in any
 *performance-critical code paths, so it's not worth adding additional overhead
 *to normal paths to make it go faster; but see also DropRelFileNodeBuffers.
 * --------------------------------------------------------------------
 */
void
FlushRelationBuffers(Relation rel)
{








































































}

/* ---------------------------------------------------------------------
 *		FlushDatabaseBuffers
 *
 *		This function writes all dirty pages of a database out to disk
 *		(or more accurately, out to kernel disk buffers), ensuring that
 *the kernel has an up-to-date view of the database.
 *
 *		Generally, the caller should be holding an appropriate lock to
 *ensure no other backend is active in the target database; otherwise more pages
 *could get dirtied.
 *
 *		Note we don't worry about flushing any pages of temporary
 *relations. It's assumed these wouldn't be interesting.
 * --------------------------------------------------------------------
 */
void
FlushDatabaseBuffers(Oid dbid)
{





































}

/*
 * Flush a previously, shared or exclusively, locked and pinned buffer to the
 * OS.
 */
void
FlushOneBuffer(Buffer buffer)
{












}

/*
 * ReleaseBuffer -- release the pin on a buffer
 */
void
ReleaseBuffer(Buffer buffer)
{















}

/*
 * UnlockReleaseBuffer -- release the content lock and pin on a buffer
 *
 * This is just a shorthand for a common combination.
 */
void
UnlockReleaseBuffer(Buffer buffer)
{


}

/*
 * IncrBufferRefCount
 *		Increment the pin count on a buffer that we have *already*
 *pinned at least once.
 *
 *		This function cannot be used on a buffer we do not have pinned,
 *		because it doesn't change the shared buffer state.
 */
void
IncrBufferRefCount(Buffer buffer)
{















}

/*
 * MarkBufferDirtyHint
 *
 *	Mark a buffer dirty for non-critical changes.
 *
 * This is essentially the same as MarkBufferDirty, except:
 *
 * 1. The caller does not write WAL; so if checksums are enabled, we may need
 *	  to write an XLOG_FPI WAL record to protect against torn pages.
 * 2. The caller might have only share-lock instead of exclusive-lock on the
 *	  buffer's content lock.
 * 3. This function does not guarantee that the buffer is always marked dirty
 *	  (due to a race condition), so it cannot be used for important changes.
 */
void
MarkBufferDirtyHint(Buffer buffer, bool buffer_std)
{







































































































































}

/*
 * Release buffer content locks for shared buffers.
 *
 * Used to clean up after errors.
 *
 * Currently, we can expect that lwlock.c's LWLockReleaseAll() took care
 * of releasing buffer content locks per se; the only thing we need to deal
 * with here is clearing any PIN_COUNT request that was in progress.
 */
void
UnlockBuffers(void)
{





















}

/*
 * Acquire or release the content_lock for the buffer.
 */
void
LockBuffer(Buffer buffer, int mode)
{


























}

/*
 * Acquire the content_lock for the buffer, but only if we don't have to wait.
 *
 * This assumes the caller wants BUFFER_LOCK_EXCLUSIVE mode.
 */
bool
ConditionalLockBuffer(Buffer buffer)
{











}

/*
 * LockBufferForCleanup - lock a buffer in preparation for deleting items
 *
 * Items may be deleted from a disk page only when the caller (a) holds an
 * exclusive lock on the buffer and (b) has observed that no other backend
 * holds a pin on the buffer.  If there is a pin, then the other backend
 * might have a pointer into the buffer (for example, a heapscan reference
 * to an item --- see README for more details).  It's OK if a pin is added
 * after the cleanup starts, however; the newly-arrived backend will be
 * unable to look at the page until we release the exclusive lock.
 *
 * To implement this protocol, a would-be deleter must pin the buffer and
 * then call LockBufferForCleanup().  LockBufferForCleanup() is similar to
 * LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE), except that it loops until
 * it has successfully observed pin count = 1.
 */
void
LockBufferForCleanup(Buffer buffer)
{





















































































}

/*
 * Check called from RecoveryConflictInterrupt handler when Startup
 * process requests cancellation of all pin holders that are blocking it.
 */
bool
HoldingBufferPinThatDelaysRecovery(void)
{



















}

/*
 * ConditionalLockBufferForCleanup - as above, but don't wait to get the lock
 *
 * We won't loop, but just check once to see if the pin count is OK.  If
 * not, return false with no lock held.
 */
bool
ConditionalLockBufferForCleanup(Buffer buffer)
{
















































}

/*
 * IsBufferCleanupOK - as above, but we already have the lock
 *
 * Check whether it's OK to perform cleanup on a buffer we've already
 * locked.  If we observe that the pin count is 1, our exclusive lock
 * happens to be a cleanup lock, and we can proceed with anything that
 * would have been allowable had we sought a cleanup lock originally.
 */
bool
IsBufferCleanupOK(Buffer buffer)
{







































}

/*
 *	Functions for buffer I/O handling
 *
 *	Note: We assume that nested buffer I/O never occurs.
 *	i.e at most one io_in_progress lock is held per proc.
 *
 *	Also note that these are used only for shared buffers, not local ones.
 */

/*
 * WaitIO -- Block until the IO_IN_PROGRESS flag on 'buf' is cleared.
 */
static void
WaitIO(BufferDesc *buf)
{


























}

/*
 * StartBufferIO: begin I/O on this buffer
 *	(Assumptions)
 *	My process is executing no IO
 *	The buffer is Pinned
 *
 * In some scenarios there are race conditions in which multiple backends
 * could attempt the same I/O operation concurrently.  If someone else
 * has already started I/O on this buffer then we will block on the
 * io_in_progress lock until he's done.
 *
 * Input operations are only attempted on buffers that are not BM_VALID,
 * and output operations only on buffers that are BM_VALID and BM_DIRTY,
 * so we can always tell if the work is already done.
 *
 * Returns true if we successfully marked the buffer as I/O busy,
 * false if someone else already did the work.
 */
static bool
StartBufferIO(BufferDesc *buf, bool forInput)
{















































}

/*
 * TerminateBufferIO: release a buffer we were doing I/O on
 *	(Assumptions)
 *	My process is executing IO for the buffer
 *	BM_IO_IN_PROGRESS bit is set for the buffer
 *	We hold the buffer's io_in_progress lock
 *	The buffer is Pinned
 *
 * If clear_dirty is true and BM_JUST_DIRTIED is not set, we clear the
 * buffer's BM_DIRTY flag.  This is appropriate when terminating a
 * successful write.  The check on BM_JUST_DIRTIED is necessary to avoid
 * marking the buffer clean if it was re-dirtied while we were writing.
 *
 * set_flag_bits gets ORed into the buffer's flags.  It must include
 * BM_IO_ERROR in a failure case.  For successful completion it could
 * be 0, or BM_VALID if we just finished reading in the page.
 */
static void
TerminateBufferIO(BufferDesc *buf, bool clear_dirty, uint32 set_flag_bits)
{




















}

/*
 * AbortBufferIO: Clean up any active buffer I/O after an error.
 *
 *	All LWLocks we might have held have been released,
 *	but we haven't yet released buffer pins, so the buffer is still pinned.
 *
 *	If I/O was in progress, we always set BM_IO_ERROR, even though it's
 *	possible the error condition wasn't related to the I/O.
 */
void
AbortBufferIO(void)
{









































}

/*
 * Error context callback for errors occurring during shared buffer writes.
 */
static void
shared_buffer_write_error_callback(void *arg)
{










}

/*
 * Error context callback for errors occurring during local buffer writes.
 */
static void
local_buffer_write_error_callback(void *arg)
{









}

/*
 * RelFileNode qsort/bsearch comparator; see RelFileNodeEquals.
 */
static int
rnode_comparator(const void *p1, const void *p2)
{

































}

/*
 * Lock buffer header - set BM_LOCKED in buffer state.
 */
uint32
LockBufHdr(BufferDesc *desc)
{


















}

/*
 * Wait until the BM_LOCKED flag isn't set anymore and return the buffer's
 * state at that point.
 *
 * Obviously the buffer could be locked by the time the value is returned, so
 * this is primarily useful in CAS style loops.
 */
static uint32
WaitBufHdrUnlocked(BufferDesc *buf)
{
















}

/*
 * BufferTag comparator.
 */
static int
buffertag_comparator(const void *a, const void *b)
{






























}

/*
 * Comparator determining the writeout order in a checkpoint.
 *
 * It is important that tablespaces are compared first, the logic balancing
 * writes between tablespaces relies on it.
 */
static int
ckpt_buforder_comparator(const void *pa, const void *pb)
{









































}

/*
 * Comparator for a Min-Heap over the per-tablespace checkpoint completion
 * progress.
 */
static int
ts_ckpt_progress_comparator(Datum a, Datum b, void *arg)
{
















}

/*
 * Initialize a writeback context, discarding potential previous state.
 *
 * *max_pending is a pointer instead of an immediate value, so the coalesce
 * limits can easily changed by the GUC mechanism, and so calling code does
 * not have to check the current configuration. A value is 0 means that no
 * writeback control will be performed.
 */
void
WritebackContextInit(WritebackContext *context, int *max_pending)
{




}

/*
 * Add buffer to list of pending writeback requests.
 */
void
ScheduleBufferTagForWriteback(WritebackContext *context, BufferTag *tag)
{
























}

/*
 * Issue all pending writeback requests, previously scheduled with
 * ScheduleBufferTagForWriteback, to the OS.
 *
 * Because this is only used to improve the OSs IO scheduling we try to never
 * error out - it's just a hint.
 */
void
IssuePendingWritebacks(WritebackContext *context)
{




































































}

/*
 * Implement slower/larger portions of TestForOldSnapshot
 *
 * Smaller/faster portions are put inline, but the entire set of logic is too
 * big for that.
 */
void
TestForOldSnapshot_impl(Snapshot snapshot, Relation relation)
{




}