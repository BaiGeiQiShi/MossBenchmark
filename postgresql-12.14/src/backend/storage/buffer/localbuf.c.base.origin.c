/*-------------------------------------------------------------------------
 *
 * localbuf.c
 *	  local buffer manager. Fast buffer manager for temporary tables,
 *	  which never need to be WAL-logged or checkpointed, etc.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/buffer/localbuf.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/parallel.h"
#include "catalog/catalog.h"
#include "executor/instrument.h"
#include "storage/buf_internals.h"
#include "storage/bufmgr.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner_private.h"

/*#define LBDEBUG*/

/* entry for buffer lookup hashtable */
typedef struct
{
  BufferTag key; /* Tag of a disk page */
  int id;        /* Associated local buffer's index */
} LocalBufferLookupEnt;

/* Note: this macro only works on local buffers, not shared ones! */
#define LocalBufHdrGetBlock(bufHdr) LocalBufferBlockPointers[-((bufHdr)->buf_id + 2)]

int NLocBuffer = 0; /* until buffers are initialized */

BufferDesc *LocalBufferDescriptors = NULL;
Block *LocalBufferBlockPointers = NULL;
int32 *LocalRefCount = NULL;

static int nextFreeLocalBuf = 0;

static HTAB *LocalBufHash = NULL;

static void
InitLocalBuffers(void);
static Block
GetLocalBufferStorage(void);

/*
 * LocalPrefetchBuffer -
 *	  initiate asynchronous read of a block of a relation
 *
 * Do PrefetchBuffer's work for temporary relations.
 * No-op if prefetching isn't compiled in.
 */
void
LocalPrefetchBuffer(SMgrRelation smgr, ForkNumber forkNum, BlockNumber blockNum)
{
























}

/*
 * LocalBufferAlloc -
 *	  Find or create a local buffer for the given page of the given
 *relation.
 *
 * API is similar to bufmgr.c's BufferAlloc, except that we do not need
 * to do any locking since this is all local.   Also, IO_IN_PROGRESS
 * does not get set.  Lastly, we support only default access strategy
 * (hence, usage_count is always advanced).
 */
BufferDesc *
LocalBufferAlloc(SMgrRelation smgr, ForkNumber forkNum, BlockNumber blockNum, bool *foundPtr)
{




































































































































































}

/*
 * MarkLocalBufferDirty -
 *	  mark a local buffer dirty
 */
void
MarkLocalBufferDirty(Buffer buffer)
{


























}

/*
 * DropRelFileNodeLocalBuffers
 *		This function removes from the buffer pool all the pages of the
 *		specified relation that have block numbers >= firstDelBlock.
 *		(In particular, with firstDelBlock = 0, all pages are removed.)
 *		Dirty pages are simply dropped, without bothering to write them
 *		out first.  Therefore, this is NOT rollback-able, and so should
 *be used only with extreme caution!
 *
 *		See DropRelFileNodeBuffers in bufmgr.c for more notes.
 */
void
DropRelFileNodeLocalBuffers(RelFileNode rnode, ForkNumber forkNum, BlockNumber firstDelBlock)
{





























}

/*
 * DropRelFileNodeAllLocalBuffers
 *		This function removes from the buffer pool all pages of all
 *forks of the specified relation.
 *
 *		See DropRelFileNodeAllBuffers in bufmgr.c for more notes.
 */
void
DropRelFileNodeAllLocalBuffers(RelFileNode rnode)
{





























}

/*
 * InitLocalBuffers -
 *	  init the local buffer cache. Since most queries (esp. multi-user ones)
 *	  don't involve local buffers, we delay allocating actual memory for the
 *	  buffers until we need them; just make the buffer headers here.
 */
static void
InitLocalBuffers(void)
{































































}

/*
 * GetLocalBufferStorage - allocate memory for a local buffer
 *
 * The idea of this function is to aggregate our requests for storage
 * so that the memory manager doesn't see a whole lot of relatively small
 * requests.  Since we'll never give back a local buffer once it's created
 * within a particular process, no point in burdening memmgr with separately
 * managed chunks.
 */
static Block
GetLocalBufferStorage(void)
{











































}

/*
 * CheckForLocalBufferLeaks - ensure this backend holds no local buffer pins
 *
 * This is just like CheckForBufferLeaks(), but for local buffers.
 */
static void
CheckForLocalBufferLeaks(void)
{



















}

/*
 * AtEOXact_LocalBuffers - clean up at end of transaction.
 *
 * This is just like AtEOXact_Buffers, but for local buffers.
 */
void
AtEOXact_LocalBuffers(bool isCommit)
{

}

/*
 * AtProcExit_LocalBuffers - ensure we have dropped pins during backend exit.
 *
 * This is just like AtProcExit_Buffers, but for local buffers.
 */
void
AtProcExit_LocalBuffers(void)
{






}