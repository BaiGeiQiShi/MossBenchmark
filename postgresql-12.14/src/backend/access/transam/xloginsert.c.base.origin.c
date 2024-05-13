/*-------------------------------------------------------------------------
 *
 * xloginsert.c
 *		Functions for constructing WAL records
 *
 * Constructing a WAL record begins with a call to XLogBeginInsert,
 * followed by a number of XLogRegister* calls. The registered data is
 * collected in private working memory, and finally assembled into a chain
 * of XLogRecData structs by a call to XLogRecordAssemble(). See
 * access/transam/README for details.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/xloginsert.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/xact.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xloginsert.h"
#include "catalog/pg_control.h"
#include "common/pg_lzcompress.h"
#include "miscadmin.h"
#include "replication/origin.h"
#include "storage/bufmgr.h"
#include "storage/proc.h"
#include "utils/memutils.h"
#include "pg_trace.h"

/* Buffer size required to store a compressed version of backup block image */
#define PGLZ_MAX_BLCKSZ PGLZ_MAX_OUTPUT(BLCKSZ)

/*
 * For each block reference registered with XLogRegisterBuffer, we fill in
 * a registered_buffer struct.
 */
typedef struct
{
  bool in_use;       /* is this slot in use? */
  uint8 flags;       /* REGBUF_* flags */
  RelFileNode rnode; /* identifies the relation and block */
  ForkNumber forkno;
  BlockNumber block;
  Page page;               /* page content */
  uint32 rdata_len;        /* total length of data in rdata chain */
  XLogRecData *rdata_head; /* head of the chain of data registered with
                            * this block */
  XLogRecData *rdata_tail; /* last entry in the chain, or &rdata_head if
                            * empty */

  XLogRecData bkp_rdatas[2]; /* temporary rdatas used to hold references to
                              * backup block data in XLogRecordAssemble() */

  /* buffer to store a compressed version of backup block image */
  char compressed_page[PGLZ_MAX_BLCKSZ];
} registered_buffer;

static registered_buffer *registered_buffers;
static int max_registered_buffers;      /* allocated size */
static int max_registered_block_id = 0; /* highest block_id + 1 currently
                                         * registered */

/*
 * A chain of XLogRecDatas to hold the "main data" of a WAL record, registered
 * with XLogRegisterData(...).
 */
static XLogRecData *mainrdata_head;
static XLogRecData *mainrdata_last = (XLogRecData *)&mainrdata_head;
static uint32 mainrdata_len; /* total # of bytes in chain */

/* flags for the in-progress insertion */
static uint8 curinsert_flags = 0;

/*
 * These are used to hold the record header while constructing a record.
 * 'hdr_scratch' is not a plain variable, but is palloc'd at initialization,
 * because we want it to be MAXALIGNed and padding bytes zeroed.
 *
 * For simplicity, it's allocated large enough to hold the headers for any
 * WAL record.
 */
static XLogRecData hdr_rdt;
static char *hdr_scratch = NULL;

#define SizeOfXlogOrigin (sizeof(RepOriginId) + sizeof(char))

#define HEADER_SCRATCH_SIZE (SizeOfXLogRecord + MaxSizeOfXLogRecordBlockHeader * (XLR_MAX_BLOCK_ID + 1) + SizeOfXLogRecordDataHeaderLong + SizeOfXlogOrigin)

/*
 * An array of XLogRecData structs, to hold registered data.
 */
static XLogRecData *rdatas;
static int num_rdatas; /* entries currently used */
static int max_rdatas; /* allocated size */

static bool begininsert_called = false;

/* Memory context to hold the registered buffer and data references. */
static MemoryContext xloginsert_cxt;

static XLogRecData *
XLogRecordAssemble(RmgrId rmid, uint8 info, XLogRecPtr RedoRecPtr, bool doPageWrites, XLogRecPtr *fpw_lsn);
static bool
XLogCompressBackupBlock(char *page, uint16 hole_offset, uint16 hole_length, char *dest, uint16 *dlen);

/*
 * Begin constructing a WAL record. This must be called before the
 * XLogRegister* functions and XLogInsert().
 */
void
XLogBeginInsert(void)
{
















}

/*
 * Ensure that there are enough buffer and data slots in the working area,
 * for subsequent XLogRegisterBuffer, XLogRegisterData and XLogRegisterBufData
 * calls.
 *
 * There is always space for a small number of buffers and data chunks, enough
 * for most record types. This function is for the exceptional cases that need
 * more.
 */
void
XLogEnsureRecordSpace(int max_block_id, int ndatas)
{











































}

/*
 * Reset WAL record construction buffers.
 */
void
XLogResetInsertion(void)
{













}

/*
 * Register a reference to a buffer with the WAL record being constructed.
 * This must be called for every page that the WAL-logged operation modifies.
 */
void
XLogRegisterBuffer(uint8 block_id, Buffer buffer, uint8 flags)
{














































}

/*
 * Like XLogRegisterBuffer, but for registering a block that's not in the
 * shared buffer pool (i.e. when you don't have a Buffer for it).
 */
void
XLogRegisterBlock(uint8 block_id, RelFileNode *rnode, ForkNumber forknum, BlockNumber blknum, Page page, uint8 flags)
{

















































}

/*
 * Add data to the WAL record that's being constructed.
 *
 * The data is appended to the "main chunk", available at replay with
 * XLogRecGetData().
 */
void
XLogRegisterData(char *data, int len)
{






















}

/*
 * Add buffer-specific data to the WAL record that's being constructed.
 *
 * Block_id must reference a block previously registered with
 * XLogRegisterBuffer(). If this is called more than once for the same
 * block_id, the data is appended.
 *
 * The maximum amount of data that can be registered per block is 65535
 * bytes. That should be plenty; if you need more than BLCKSZ bytes to
 * reconstruct the changes to the page, you might as well just log a full
 * copy of it. (the "main data" that's not associated with a block is not
 * limited)
 */
void
XLogRegisterBufData(uint8 block_id, char *data, int len)
{
























}

/*
 * Set insert status flags for the upcoming WAL record.
 *
 * The flags that can be used here are:
 * - XLOG_INCLUDE_ORIGIN, to determine if the replication origin should be
 *	 included in the record.
 * - XLOG_MARK_UNIMPORTANT, to signal that the record is not important for
 *	 durability, which allows to avoid triggering WAL archiving and other
 *	 background activity.
 */
void
XLogSetRecordFlags(uint8 flags)
{


}

/*
 * Insert an XLOG record having the specified RMID and info bytes, with the
 * body of the record being the data and buffer references registered earlier
 * with XLogRegister* calls.
 *
 * Returns XLOG pointer to end of record (beginning of next record).
 * This can be used as LSN for data pages affected by the logged action.
 * (LSN is the XLOG point up to which the XLOG must be flushed to disk
 * before the data page can be written out.  This implements the basic
 * WAL rule "write the log before the data".)
 */
XLogRecPtr
XLogInsert(RmgrId rmid, uint8 info)
{




















































}

/*
 * Assemble a WAL record from the registered data and buffers into an
 * XLogRecData chain, ready for insertion with XLogInsertRecord().
 *
 * The record header fields are filled in, except for the xl_prev field. The
 * calculated CRC does not include the record header yet.
 *
 * If there are any registered buffers, and a full-page image was not taken
 * of all of them, *fpw_lsn is set to the lowest LSN among such pages. This
 * signals that the assembled record is only good for insertion on the
 * assumption that the RedoRecPtr and doPageWrites values were up-to-date.
 */
static XLogRecData *
XLogRecordAssemble(RmgrId rmid, uint8 info, XLogRecPtr RedoRecPtr, bool doPageWrites, XLogRecPtr *fpw_lsn)
{




































































































































































































































































































































}

/*
 * Create a compressed version of a backup block image.
 *
 * Returns false if compression fails (i.e., compressed result is actually
 * bigger than original). Otherwise, returns true and sets 'dlen' to
 * the length of compressed block image.
 */
static bool
XLogCompressBackupBlock(char *page, uint16 hole_offset, uint16 hole_length, char *dest, uint16 *dlen)
{




































}

/*
 * Determine whether the buffer referenced has to be backed up.
 *
 * Since we don't yet have the insert lock, fullPageWrites and forcePageWrites
 * could change later, so the result should be used for optimization purposes
 * only.
 */
bool
XLogCheckBufferNeedsBackup(Buffer buffer)
{














}

/*
 * Write a backup block if needed when we are setting a hint. Note that
 * this may be called for a variety of page types, not just heaps.
 *
 * Callable while holding just share lock on the buffer content.
 *
 * We can't use the plain backup block mechanism since that relies on the
 * Buffer being exclusively locked. Since some modifications (setting LSN, hint
 * bits) are allowed in a sharelocked buffer that can lead to wal checksum
 * failures. So instead we copy the page and insert the copied data as normal
 * record data.
 *
 * We only need to do something if page has not yet been full page written in
 * this checkpoint round. The LSN of the inserted wal record is returned if we
 * had to write, InvalidXLogRecPtr otherwise.
 *
 * It is possible that multiple concurrent backends could attempt to write WAL
 * records. In that case, multiple copies of the same block would be recorded
 * in separate WAL records by different backends, though that is still OK from
 * a correctness perspective.
 */
XLogRecPtr
XLogSaveBufferForHint(Buffer buffer, bool buffer_std)
{


































































}

/*
 * Write a WAL record containing a full image of a page. Caller is responsible
 * for writing the page to disk after calling this routine.
 *
 * Note: If you're using this function, you should be building pages in private
 * memory and writing them directly to smgr.  If you're using buffers, call
 * log_newpage_buffer instead.
 *
 * If the page follows the standard page layout, with a PageHeader and unused
 * space between pd_lower and pd_upper, set 'page_std' to true. That allows
 * the unused space to be left out from the WAL record, making it smaller.
 */
XLogRecPtr
log_newpage(RelFileNode *rnode, ForkNumber forkNum, BlockNumber blkno, Page page, bool page_std)
{























}

/*
 * Write a WAL record containing a full image of a page.
 *
 * Caller should initialize the buffer and mark it dirty before calling this
 * function.  This function will set the page LSN.
 *
 * If the page follows the standard page layout, with a PageHeader and unused
 * space between pd_lower and pd_upper, set 'page_std' to true. That allows
 * the unused space to be left out from the WAL record, making it smaller.
 */
XLogRecPtr
log_newpage_buffer(Buffer buffer, bool page_std)
{











}

/*
 * WAL-log a range of blocks in a relation.
 *
 * An image of all pages with block numbers 'startblk' <= X < 'endblk' is
 * written to the WAL. If the range is large, this is done in multiple WAL
 * records.
 *
 * If all page follows the standard page layout, with a PageHeader and unused
 * space between pd_lower and pd_upper, set 'page_std' to true. That allows
 * the unused space to be left out from the WAL records, making them smaller.
 *
 * NOTE: This function acquires exclusive-locks on the pages. Typically, this
 * is used on a newly-built relation, and the caller is holding a
 * AccessExclusiveLock on it, so no other backend can be accessing it at the
 * same time. If that's not the case, you must ensure that this does not
 * cause a deadlock through some other means.
 */
void
log_newpage_range(Relation rel, ForkNumber forkNum, BlockNumber startblk, BlockNumber endblk, bool page_std)
{





































































}

/*
 * Allocate working buffers needed for WAL record construction.
 */
void
InitXLogInsert(void)
{
























}