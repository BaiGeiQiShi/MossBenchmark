/*-------------------------------------------------------------------------
 *
 * xlogreader.c
 *		Generic XLog reading facility
 *
 * Portions Copyright (c) 2013-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		src/backend/access/transam/xlogreader.c
 *
 * NOTES
 *		See xlogreader.h for more notes on this facility.
 *
 *		This file is compiled as both front-end and backend code, so it
 *		may not use ereport, server-defined static variables, etc.
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "access/xlogrecord.h"
#include "access/xlog_internal.h"
#include "access/xlogreader.h"
#include "catalog/pg_control.h"
#include "common/pg_lzcompress.h"
#include "replication/origin.h"

#ifndef FRONTEND
#include "utils/memutils.h"
#endif

static bool
allocate_recordbuf(XLogReaderState *state, uint32 reclength);

static bool
ValidXLogRecordHeader(XLogReaderState *state, XLogRecPtr RecPtr, XLogRecPtr PrevRecPtr, XLogRecord *record, bool randAccess);
static bool
ValidXLogRecord(XLogReaderState *state, XLogRecord *record, XLogRecPtr recptr);
static int
ReadPageInternal(XLogReaderState *state, XLogRecPtr pageptr, int reqLen);
static void
report_invalid_record(XLogReaderState *state, const char *fmt, ...) pg_attribute_printf(2, 3);

static void
ResetDecoder(XLogReaderState *state);

/* size of the buffer allocated for error message. */
#define MAX_ERRORMSG_LEN 1000

/*
 * Construct a string in state->errormsg_buf explaining what's wrong with
 * the current record being read.
 */
static void
report_invalid_record(XLogReaderState *state, const char *fmt, ...)
{







}

/*
 * Allocate and initialize a new XLogReader.
 *
 * Returns NULL if the xlogreader couldn't be allocated.
 */
XLogReaderState *
XLogReaderAllocate(int wal_segment_size, XLogPageReadCB pagereadfunc, void *private_data)
{
  XLogReaderState *state;

  state = (XLogReaderState *)palloc_extended(sizeof(XLogReaderState), MCXT_ALLOC_NO_OOM | MCXT_ALLOC_ZERO);
  if (!state) {

  }

  state->max_block_id = -1;

  /*
   * Permanently allocate readBuf.  We do it this way, rather than just
   * making a static array, for two reasons: (1) no need to waste the
   * storage in most instantiations of the backend; (2) a static char array
   * isn't guaranteed to have any particular alignment, whereas
   * palloc_extended() will provide MAXALIGN'd storage.
   */
  state->readBuf = (char *)palloc_extended(XLOG_BLCKSZ, MCXT_ALLOC_NO_OOM);
  if (!state->readBuf) {


  }

  state->wal_segment_size = wal_segment_size;
  state->read_page = pagereadfunc;
  /* system_identifier initialized to zeroes above */
  state->private_data = private_data;
  /* ReadRecPtr and EndRecPtr initialized to zeroes above */
  /* readSegNo, readOff, readLen, readPageTLI initialized to zeroes above */
  state->errormsg_buf = palloc_extended(MAX_ERRORMSG_LEN + 1, MCXT_ALLOC_NO_OOM);
  if (!state->errormsg_buf) {



  }
  state->errormsg_buf[0] = '\0';

  /*
   * Allocate an initial readRecordBuf of minimal size, which can later be
   * enlarged if necessary.
   */
  if (!allocate_recordbuf(state, 0)) {




  }

  return state;
}

void
XLogReaderFree(XLogReaderState *state)
{
  int block_id;

  for (block_id = 0; block_id <= XLR_MAX_BLOCK_ID; block_id++) {
    if (state->blocks[block_id].data) {

    }
  }
  if (state->main_data) {
    pfree(state->main_data);
  }

  pfree(state->errormsg_buf);
  if (state->readRecordBuf) {
    pfree(state->readRecordBuf);
  }
  pfree(state->readBuf);
  pfree(state);
}

/*
 * Allocate readRecordBuf to fit a record of at least the given length.
 * Returns true if successful, false if out of memory.
 *
 * readRecordBufSize is set to the new buffer size.
 *
 * To avoid useless small increases, round its size to a multiple of
 * XLOG_BLCKSZ, and make sure it's at least 5*Max(BLCKSZ, XLOG_BLCKSZ) to start
 * with.  (That is enough for all "normal" records, but very large commit or
 * abort records might need more space.)
 */
static bool
allocate_recordbuf(XLogReaderState *state, uint32 reclength)
{
  uint32 newSize = reclength;

  newSize += XLOG_BLCKSZ - (newSize % XLOG_BLCKSZ);
  newSize = Max(newSize, 5 * Max(BLCKSZ, XLOG_BLCKSZ));

#ifndef FRONTEND

  /*
   * Note that in much unlucky circumstances, the random data read from a
   * recycled segment can cause this routine to be called with a size
   * causing a hard failure at allocation.  For a standby, this would cause
   * the instance to stop suddenly with a hard failure, preventing it to
   * retry fetching WAL from one of its sources which could allow it to move
   * on with replay without a manual restart. If the data comes from a past
   * recycled segment and is still valid, then the allocation may succeed
   * but record checks are going to fail so this would be short-lived.  If
   * the allocation fails because of a memory shortage, then this is not a
   * hard failure either per the guarantee given by MCXT_ALLOC_NO_OOM.
   */
  if (!AllocSizeIsValid(newSize)) {

  }

#endif

  if (state->readRecordBuf) {

  }
  state->readRecordBuf = (char *)palloc_extended(newSize, MCXT_ALLOC_NO_OOM);
  if (state->readRecordBuf == NULL) {


  }
  state->readRecordBufSize = newSize;
  return true;
}

/*
 * Attempt to read an XLOG record.
 *
 * If RecPtr is valid, try to read a record at that position.  Otherwise
 * try to read a record just after the last one previously read.
 *
 * If the read_page callback fails to read the requested data, NULL is
 * returned.  The callback is expected to have reported the error; errormsg
 * is set to NULL.
 *
 * If the reading fails for some other reason, NULL is also returned, and
 * *errormsg is set to a string with details of the failure.
 *
 * The returned pointer (or *errormsg) points to an internal buffer that's
 * valid until the next call to XLogReadRecord.
 */
XLogRecord *
XLogReadRecord(XLogReaderState *state, XLogRecPtr RecPtr, char **errormsg)
{
  XLogRecord *record;
  XLogRecPtr targetPagePtr;
  bool randAccess;
  uint32 len, total_len;
  uint32 targetRecOff;
  uint32 pageHeaderSize;
  bool assembled;
  bool gotheader;
  int readOff;

  /*
   * randAccess indicates whether to verify the previous-record pointer of
   * the record we're reading.  We only do this if we're reading
   * sequentially, which is what we initially assume.
   */
  randAccess = false;

  /* reset error state */
  *errormsg = NULL;
  state->errormsg_buf[0] = '\0';

  ResetDecoder(state);
  state->abortedRecPtr = InvalidXLogRecPtr;
  state->missingContrecPtr = InvalidXLogRecPtr;

  if (RecPtr == InvalidXLogRecPtr) {
    /* No explicit start point; read the record after the one we just read */






    /*
     * RecPtr is pointing to end+1 of the previous WAL record.  If we're
     * at a page boundary, no more records can fit on the current page. We
     * must skip over the page header, but we can't do that until we've
     * read in the page, since the header size is variable.
     */
  } else {
    /*
     * Caller supplied a position to start at.
     *
     * In this case, the passed-in record pointer should already be
     * pointing to a valid record starting position.
     */
    Assert(XRecOffIsValid(RecPtr));
    randAccess = true;
  }

restart:;;
  state->currRecPtr = RecPtr;
  assembled = false;

  targetPagePtr = RecPtr - (RecPtr % XLOG_BLCKSZ);
  targetRecOff = RecPtr % XLOG_BLCKSZ;

  /*
   * Read the page containing the record into state->readBuf. Request enough
   * byte to cover the whole record header, or at least the part of it that
   * fits on the same page.
   */
  readOff = ReadPageInternal(state, targetPagePtr, Min(targetRecOff + SizeOfXLogRecord, XLOG_BLCKSZ));
  if (readOff < 0) {

  }

  /*
   * ReadPageInternal always returns at least the page header, so we can
   * examine it now.
   */
  pageHeaderSize = XLogPageHeaderSize((XLogPageHeader)state->readBuf);
  if (targetRecOff == 0) {
    /*
     * At page start, so skip over page header.
     */


  } else if (targetRecOff < pageHeaderSize) {


  }

  if ((((XLogPageHeader)state->readBuf)->xlp_info & XLP_FIRST_IS_CONTRECORD) && targetRecOff == pageHeaderSize) {


  }

  /* ReadPageInternal has verified the page header */
  Assert(pageHeaderSize <= readOff);

  /*
   * Read the record length.
   *
   * NB: Even though we use an XLogRecord pointer here, the whole record
   * header might not fit on this page. xl_tot_len is the first field of the
   * struct, so it must be on this page (the records are MAXALIGNed), but we
   * cannot access any other fields until we've verified that we got the
   * whole header.
   */
  record = (XLogRecord *)(state->readBuf + RecPtr % XLOG_BLCKSZ);
  total_len = record->xl_tot_len;

  /*
   * If the whole record header is on this page, validate it immediately.
   * Otherwise do just a basic sanity check on xl_tot_len, and validate the
   * rest of the header after reading it from the next page.  The xl_tot_len
   * check is necessary here to ensure that we enter the "Need to reassemble
   * record" code path below; otherwise we might fail to apply
   * ValidXLogRecordHeader at all.
   */
  if (targetRecOff <= XLOG_BLCKSZ - SizeOfXLogRecord) {
    if (!ValidXLogRecordHeader(state, RecPtr, state->ReadRecPtr, record, randAccess)) {

    }
    gotheader = true;
  } else {
    /* XXX: more validation should be done here */





  }

  len = XLOG_BLCKSZ - RecPtr % XLOG_BLCKSZ;
  if (total_len > len) {
    /* Need to reassemble record */







    /*
     * Enlarge readRecordBuf as needed.
     */






    /* Copy the first fragment of the record from the first page. */




























































































  } else {
    /* Wait for the record data to become available */
    readOff = ReadPageInternal(state, targetPagePtr, Min(targetRecOff + total_len, XLOG_BLCKSZ));
    if (readOff < 0) {

    }

    /* Record does not cross a page boundary */
    if (!ValidXLogRecord(state, record, RecPtr)) {

    }

    state->EndRecPtr = RecPtr + MAXALIGN(total_len);

    state->ReadRecPtr = RecPtr;
  }

  /*
   * Special processing if it's an XLOG SWITCH record
   */
  if (record->xl_rmid == RM_XLOG_ID && (record->xl_info & ~XLR_INFO_MASK) == XLOG_SWITCH) {
    /* Pretend it extends to end of segment */


  }

  if (DecodeXLogRecord(state, record, errormsg)) {
    return record;
  } else {

  }

err:;;














  /*
   * Invalidate the read state. We might read from a different source after
   * failure.
   */







}

/*
 * Read a single xlog page including at least [pageptr, reqLen] of valid data
 * via the read_page() callback.
 *
 * Returns -1 if the required page cannot be read for some reason; errormsg_buf
 * is set in that case (unless the error occurs in the read_page callback).
 *
 * We fetch the page from a reader-local cache if we know we have the required
 * data and if there hasn't been any error since caching the data.
 */
static int
ReadPageInternal(XLogReaderState *state, XLogRecPtr pageptr, int reqLen)
{
  int readLen;
  uint32 targetPageOff;
  XLogSegNo targetSegNo;
  XLogPageHeader hdr;

  Assert((pageptr % XLOG_BLCKSZ) == 0);

  XLByteToSeg(pageptr, targetSegNo, state->wal_segment_size);
  targetPageOff = XLogSegmentOffset(pageptr, state->wal_segment_size);

  /* check whether we have all the requested data already */
  if (targetSegNo == state->readSegNo && targetPageOff == state->readOff && reqLen <= state->readLen) {
    return state->readLen;
  }

  /*
   * Data is not in our buffer.
   *
   * Every time we actually read the page, even if we looked at parts of it
   * before, we need to do verification as the read_page callback might now
   * be rereading data from a different source.
   *
   * Whenever switching to a new WAL segment, we read the first page of the
   * file and validate its header, even if that's not where the target
   * record is.  This is so that we can check the additional identification
   * info that is present in the first page's "long" header.
   */
  if (targetSegNo != state->readSegNo && targetPageOff != 0) {
    XLogRecPtr targetSegmentPtr = pageptr - targetPageOff;

    readLen = state->read_page(state, targetSegmentPtr, XLOG_BLCKSZ, state->currRecPtr, state->readBuf, &state->readPageTLI);
    if (readLen < 0) {

    }

    /* we can be sure to have enough WAL available, we scrolled back */
    Assert(readLen == XLOG_BLCKSZ);

    if (!XLogReaderValidatePageHeader(state, targetSegmentPtr, state->readBuf)) {

    }
  }

  /*
   * First, read the requested data length, but at least a short page header
   * so that we can validate it.
   */
  readLen = state->read_page(state, pageptr, Max(reqLen, SizeOfXLogShortPHD), state->currRecPtr, state->readBuf, &state->readPageTLI);
  if (readLen < 0) {

  }

  Assert(readLen <= XLOG_BLCKSZ);

  /* Do we have enough data to check the header length? */
  if (readLen <= SizeOfXLogShortPHD) {

  }

  Assert(readLen >= reqLen);

  hdr = (XLogPageHeader)state->readBuf;

  /* still not enough */
  if (readLen < XLogPageHeaderSize(hdr)) {




  }

  /*
   * Now that we know we have the full header, validate it.
   */
  if (!XLogReaderValidatePageHeader(state, pageptr, (char *)hdr)) {

  }

  /* update read state information */
  state->readSegNo = targetSegNo;
  state->readOff = targetPageOff;
  state->readLen = readLen;

  return readLen;

err:;;


}

/*
 * Invalidate the xlogreader's read state to force a re-read.
 */
void
XLogReaderInvalReadState(XLogReaderState *state)
{



}

/*
 * Validate an XLOG record header.
 *
 * This is just a convenience subroutine to avoid duplicated code in
 * XLogReadRecord.  It's not intended for use from anywhere else.
 */
static bool
ValidXLogRecordHeader(XLogReaderState *state, XLogRecPtr RecPtr, XLogRecPtr PrevRecPtr, XLogRecord *record, bool randAccess)
{
  if (record->xl_tot_len < SizeOfXLogRecord) {


  }
  if (record->xl_rmid > RM_MAX_ID) {


  }
  if (randAccess) {
    /*
     * We can't exactly verify the prev-link, but surely it should be less
     * than the record's own address.
     */
    if (!(record->xl_prev < RecPtr)) {


    }
  } else {
    /*
     * Record's prev-link should exactly match our previous location. This
     * check guards against torn WAL pages where a stale but valid-looking
     * WAL record starts on a sector boundary.
     */




  }

  return true;
}

/*
 * CRC-check an XLOG record.  We do not believe the contents of an XLOG
 * record (other than to the minimal extent of computing the amount of
 * data to read in) until we've checked the CRCs.
 *
 * We assume all of the record (that is, xl_tot_len bytes) has been read
 * into memory at *record.  Also, ValidXLogRecordHeader() has accepted the
 * record's header, which means in particular that xl_tot_len is at least
 * SizeOfXlogRecord.
 */
static bool
ValidXLogRecord(XLogReaderState *state, XLogRecord *record, XLogRecPtr recptr)
{
  pg_crc32c crc;

  /* Calculate the CRC */
  INIT_CRC32C(crc);
  COMP_CRC32C(crc, ((char *)record) + SizeOfXLogRecord, record->xl_tot_len - SizeOfXLogRecord);
  /* include the record header last */
  COMP_CRC32C(crc, (char *)record, offsetof(XLogRecord, xl_crc));
  FIN_CRC32C(crc);

  if (!EQ_CRC32C(record->xl_crc, crc)) {


  }

  return true;
}

/*
 * Validate a page header.
 *
 * Check if 'phdr' is valid as the header of the XLog page at position
 * 'recptr'.
 */
bool
XLogReaderValidatePageHeader(XLogReaderState *state, XLogRecPtr recptr, char *phdr)
{
  XLogRecPtr recaddr;
  XLogSegNo segno;
  int32 offset;
  XLogPageHeader hdr = (XLogPageHeader)phdr;

  Assert((recptr % XLOG_BLCKSZ) == 0);

  XLByteToSeg(recptr, segno, state->wal_segment_size);
  offset = XLogSegmentOffset(recptr, state->wal_segment_size);

  XLogSegNoOffsetToRecPtr(segno, offset, state->wal_segment_size, recaddr);

  if (hdr->xlp_magic != XLOG_PAGE_MAGIC) {






  }

  if ((hdr->xlp_info & ~XLP_ALL_FLAGS) != 0) {






  }

  if (hdr->xlp_info & XLP_LONG_HEADER) {
    XLogLongPageHeader longhdr = (XLogLongPageHeader)hdr;

    if (state->system_identifier && longhdr->xlp_sysid != state->system_identifier) {



      /*
       * Format sysids separately to keep platform-dependent format code
       * out of the translatable message string.
       */






    } else if (longhdr->xlp_seg_size != state->wal_segment_size) {


    } else if (longhdr->xlp_xlog_blcksz != XLOG_BLCKSZ) {


    }
  } else if (offset == 0) {




    /* hmm, first page of file doesn't have a long header? */


  }

  /*
   * Check that the address on the page agrees with what we expected. This
   * check typically fails when an old WAL segment is recycled, and hasn't
   * yet been overwritten with new data yet.
   */
  if (hdr->xlp_pageaddr != recaddr) {






  }

  /*
   * Since child timelines are always assigned a TLI greater than their
   * immediate parent's TLI, we should never see TLI go backwards across
   * successive pages of a consistent WAL sequence.
   *
   * Sometimes we re-read a segment that's already been (partially) read. So
   * we only verify TLIs for pages that are later than the last remembered
   * LSN.
   */
  if (recptr > state->latestPagePtr) {
    if (hdr->xlp_tli < state->latestPageTLI) {








    }
  }
  state->latestPagePtr = recptr;
  state->latestPageTLI = hdr->xlp_tli;

  return true;
}

#ifdef FRONTEND
/*
 * Functions that are currently not needed in the backend, but are better
 * implemented inside xlogreader.c because of the internal facilities available
 * here.
 */

/*
 * Find the first record with an lsn >= RecPtr.
 *
 * Useful for checking whether RecPtr is a valid xlog address for reading, and
 * to find the first valid address after some address when dumping records for
 * debugging purposes.
 */
XLogRecPtr
XLogFindNextRecord(XLogReaderState *state, XLogRecPtr RecPtr)
{
  XLogReaderState saved_state = *state;
  XLogRecPtr tmpRecPtr;
  XLogRecPtr found = InvalidXLogRecPtr;
  XLogPageHeader header;
  char *errormsg;

  Assert(!XLogRecPtrIsInvalid(RecPtr));

  /*
   * skip over potential continuation data, keeping in mind that it may span
   * multiple pages
   */
  tmpRecPtr = RecPtr;
  while (true) {
    XLogRecPtr targetPagePtr;
    int targetRecOff;
    uint32 pageHeaderSize;
    int readLen;

    /*
     * Compute targetRecOff. It should typically be equal or greater than
     * short page-header since a valid record can't start anywhere before
     * that, except when caller has explicitly specified the offset that
     * falls somewhere there or when we are skipping multi-page
     * continuation record. It doesn't matter though because
     * ReadPageInternal() is prepared to handle that and will read at
     * least short page-header worth of data
     */
    targetRecOff = tmpRecPtr % XLOG_BLCKSZ;

    /* scroll back to page boundary */
    targetPagePtr = tmpRecPtr - targetRecOff;

    /* Read the page containing the record */
    readLen = ReadPageInternal(state, targetPagePtr, targetRecOff);
    if (readLen < 0) {
      goto err;
    }

    header = (XLogPageHeader)state->readBuf;

    pageHeaderSize = XLogPageHeaderSize(header);

    /* make sure we have enough data for the page header */
    readLen = ReadPageInternal(state, targetPagePtr, pageHeaderSize);
    if (readLen < 0) {
      goto err;
    }

    /* skip over potential continuation data */
    if (header->xlp_info & XLP_FIRST_IS_CONTRECORD) {
      /*
       * If the length of the remaining continuation data is more than
       * what can fit in this page, the continuation record crosses over
       * this page. Read the next page and try again. xlp_rem_len in the
       * next page header will contain the remaining length of the
       * continuation data
       *
       * Note that record headers are MAXALIGN'ed
       */
      if (MAXALIGN(header->xlp_rem_len) >= (XLOG_BLCKSZ - pageHeaderSize)) {
        tmpRecPtr = targetPagePtr + XLOG_BLCKSZ;
      } else {
        /*
         * The previous continuation record ends in this page. Set
         * tmpRecPtr to point to the first valid record
         */
        tmpRecPtr = targetPagePtr + pageHeaderSize + MAXALIGN(header->xlp_rem_len);
        break;
      }
    } else {
      tmpRecPtr = targetPagePtr + pageHeaderSize;
      break;
    }
  }

  /*
   * we know now that tmpRecPtr is an address pointing to a valid XLogRecord
   * because either we're at the first record after the beginning of a page
   * or we just jumped over the remaining data of a continuation.
   */
  while (XLogReadRecord(state, tmpRecPtr, &errormsg) != NULL) {
    /* continue after the record */
    tmpRecPtr = InvalidXLogRecPtr;

    /* past the record we've found, break out */
    if (RecPtr <= state->ReadRecPtr) {
      found = state->ReadRecPtr;
      goto out;
    }
  }

err:
out:
  /* Reset state to what we had before finding the record */
  state->ReadRecPtr = saved_state.ReadRecPtr;
  state->EndRecPtr = saved_state.EndRecPtr;
  XLogReaderInvalReadState(state);

  return found;
}

#endif /* FRONTEND */

/* ----------------------------------------
 * Functions for decoding the data and block references in a record.
 * ----------------------------------------
 */

/* private function to reset the state between records */
static void
ResetDecoder(XLogReaderState *state)
{
  int block_id;

  state->decoded_record = NULL;

  state->main_data_len = 0;

  for (block_id = 0; block_id <= state->max_block_id; block_id++) {




  }
  state->max_block_id = -1;
}

/*
 * Decode the previously read record.
 *
 * On error, a human-readable error message is returned in *errormsg, and
 * the return value is false.
 */
bool
DecodeXLogRecord(XLogReaderState *state, XLogRecord *record, char **errormsg)
{
  /*
   * read next _size bytes from record buffer, but check for overrun first.
   */
#define COPY_HEADER_FIELD(_dst, _size)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (remaining < _size)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      goto shortdata_err;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    memcpy(_dst, ptr, _size);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    ptr += _size;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    remaining -= _size;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  } while (0)

  char *ptr;
  uint32 remaining;
  uint32 datatotal;
  RelFileNode *rnode = NULL;
  uint8 block_id;

  ResetDecoder(state);

  state->decoded_record = record;
  state->record_origin = InvalidRepOriginId;

  ptr = (char *)record;
  ptr += SizeOfXLogRecord;
  remaining = record->xl_tot_len - SizeOfXLogRecord;

  /* Decode the headers */
  datatotal = 0;
  while (remaining > datatotal) {
    COPY_HEADER_FIELD(&block_id, sizeof(uint8));

    if (block_id == XLR_BLOCK_ID_DATA_SHORT) {
      /* XLogRecordDataHeaderShort */
      uint8 main_data_len;

      COPY_HEADER_FIELD(&main_data_len, sizeof(uint8));

      state->main_data_len = main_data_len;
      datatotal += main_data_len;
      break; /* by convention, the main data fragment is
              * always last */
























































































































  }

  if (remaining != datatotal) {

  }

  /*
   * Ok, we've parsed the fragment headers, and verified that the total
   * length of the payload in the fragments is equal to the amount of data
   * left. Copy the data of each fragment to a separate buffer.
   *
   * We could just set up pointers into readRecordBuf, but we want to align
   * the data for the convenience of the callers. Backup images are not
   * copied, however; they don't need alignment.
   */

  /* block data first */
  for (block_id = 0; block_id <= state->max_block_id; block_id++) {





























  }

  /* and finally, the main data */
  if (state->main_data_len > 0) {
    if (!state->main_data || state->main_data_len > state->main_data_bufsz) {
      if (state->main_data) {

      }

      /*
       * main_data_bufsz must be MAXALIGN'ed.  In many xlog record
       * types, we omit trailing struct padding on-disk to save a few
       * bytes; but compilers may generate accesses to the xlog struct
       * that assume that padding bytes are present.  If the palloc
       * request is not large enough to include such padding bytes then
       * we'll get valgrind complaints due to otherwise-harmless fetches
       * of the padding bytes.
       *
       * In addition, force the initial request to be reasonably large
       * so that we don't waste time with lots of trips through this
       * stanza.  BLCKSZ / 2 seems like a good compromise choice.
       */
      state->main_data_bufsz = MAXALIGN(Max(state->main_data_len, BLCKSZ / 2));
      state->main_data = palloc(state->main_data_bufsz);
    }
    memcpy(state->main_data, ptr, state->main_data_len);
    ptr += state->main_data_len;
  }

  return true;

shortdata_err:;;

err:;;



}

/*
 * Returns information about the block that a block reference refers to.
 *
 * If the WAL record contains a block reference with the given ID, *rnode,
 * *forknum, and *blknum are filled in (if not NULL), and returns true.
 * Otherwise returns false.
 */
bool
XLogRecGetBlockTag(XLogReaderState *record, uint8 block_id, RelFileNode *rnode, ForkNumber *forknum, BlockNumber *blknum)
{

















}

/*
 * Returns the data associated with a block reference, or NULL if there is
 * no data (e.g. because a full-page image was taken instead). The returned
 * pointer points to a MAXALIGNed buffer.
 */
char *
XLogRecGetBlockData(XLogReaderState *record, uint8 block_id, Size *len)
{



















}

/*
 * Restore a full-page image from a backup block attached to an XLOG record.
 *
 * Returns true if a full-page image is restored.
 */
bool
RestoreBlockImage(XLogReaderState *record, uint8 block_id, char *page)
{


































}