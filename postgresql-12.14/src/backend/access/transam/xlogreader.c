                                                                            
   
                
                                  
   
                                                                         
   
                  
                                            
   
         
                                                      
   
                                                                    
                                                               
                                                                            
   
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

                                                     
#define MAX_ERRORMSG_LEN 1000

   
                                                                          
                                  
   
static void
report_invalid_record(XLogReaderState *state, const char *fmt, ...)
{
  va_list args;

  fmt = _(fmt);

  va_start(args, fmt);
  vsnprintf(state->errormsg_buf, MAX_ERRORMSG_LEN, fmt, args);
  va_end(args);
}

   
                                             
   
                                                         
   
XLogReaderState *
XLogReaderAllocate(int wal_segment_size, XLogPageReadCB pagereadfunc, void *private_data)
{
  XLogReaderState *state;

  state = (XLogReaderState *)palloc_extended(sizeof(XLogReaderState), MCXT_ALLOC_NO_OOM | MCXT_ALLOC_ZERO);
  if (!state)
  {
    return NULL;
  }

  state->max_block_id = -1;

     
                                                                        
                                                                      
                                                                            
                                                                
                                                        
     
  state->readBuf = (char *)palloc_extended(XLOG_BLCKSZ, MCXT_ALLOC_NO_OOM);
  if (!state->readBuf)
  {
    pfree(state);
    return NULL;
  }

  state->wal_segment_size = wal_segment_size;
  state->read_page = pagereadfunc;
                                                     
  state->private_data = private_data;
                                                            
                                                                            
  state->errormsg_buf = palloc_extended(MAX_ERRORMSG_LEN + 1, MCXT_ALLOC_NO_OOM);
  if (!state->errormsg_buf)
  {
    pfree(state->readBuf);
    pfree(state);
    return NULL;
  }
  state->errormsg_buf[0] = '\0';

     
                                                                           
                            
     
  if (!allocate_recordbuf(state, 0))
  {
    pfree(state->errormsg_buf);
    pfree(state->readBuf);
    pfree(state);
    return NULL;
  }

  return state;
}

void
XLogReaderFree(XLogReaderState *state)
{
  int block_id;

  for (block_id = 0; block_id <= XLR_MAX_BLOCK_ID; block_id++)
  {
    if (state->blocks[block_id].data)
    {
      pfree(state->blocks[block_id].data);
    }
  }
  if (state->main_data)
  {
    pfree(state->main_data);
  }

  pfree(state->errormsg_buf);
  if (state->readRecordBuf)
  {
    pfree(state->readRecordBuf);
  }
  pfree(state->readBuf);
  pfree(state);
}

   
                                                                        
                                                       
   
                                                    
   
                                                                     
                                                                                
                                                                             
                                         
   
static bool
allocate_recordbuf(XLogReaderState *state, uint32 reclength)
{
  uint32 newSize = reclength;

  newSize += XLOG_BLCKSZ - (newSize % XLOG_BLCKSZ);
  newSize = Max(newSize, 5 * Max(BLCKSZ, XLOG_BLCKSZ));

#ifndef FRONTEND

     
                                                                          
                                                                      
                                                                            
                                                                         
                                                                             
                                                                            
                                                                          
                                                                           
                                                                           
                                                                       
     
  if (!AllocSizeIsValid(newSize))
  {
    return false;
  }

#endif

  if (state->readRecordBuf)
  {
    pfree(state->readRecordBuf);
  }
  state->readRecordBuf = (char *)palloc_extended(newSize, MCXT_ALLOC_NO_OOM);
  if (state->readRecordBuf == NULL)
  {
    state->readRecordBufSize = 0;
    return false;
  }
  state->readRecordBufSize = newSize;
  return true;
}

   
                                   
   
                                                                         
                                                                 
   
                                                                       
                                                                            
                   
   
                                                                          
                                                             
   
                                                                           
                                                
   
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

     
                                                                           
                                                                 
                                                      
     
  randAccess = false;

                         
  *errormsg = NULL;
  state->errormsg_buf[0] = '\0';

  ResetDecoder(state);
  state->abortedRecPtr = InvalidXLogRecPtr;
  state->missingContrecPtr = InvalidXLogRecPtr;

  if (RecPtr == InvalidXLogRecPtr)
  {
                                                                             
    RecPtr = state->EndRecPtr;

    if (state->ReadRecPtr == InvalidXLogRecPtr)
    {
      randAccess = true;
    }

       
                                                                         
                                                                           
                                                                        
                                                            
       
  }
  else
  {
       
                                               
       
                                                                    
                                                     
       
    Assert(XRecOffIsValid(RecPtr));
    randAccess = true;
  }

restart:
  state->currRecPtr = RecPtr;
  assembled = false;

  targetPagePtr = RecPtr - (RecPtr % XLOG_BLCKSZ);
  targetRecOff = RecPtr % XLOG_BLCKSZ;

     
                                                                             
                                                                            
                            
     
  readOff = ReadPageInternal(state, targetPagePtr, Min(targetRecOff + SizeOfXLogRecord, XLOG_BLCKSZ));
  if (readOff < 0)
  {
    goto err;
  }

     
                                                                         
                     
     
  pageHeaderSize = XLogPageHeaderSize((XLogPageHeader)state->readBuf);
  if (targetRecOff == 0)
  {
       
                                                
       
    RecPtr += pageHeaderSize;
    targetRecOff = pageHeaderSize;
  }
  else if (targetRecOff < pageHeaderSize)
  {
    report_invalid_record(state, "invalid record offset at %X/%X", (uint32)(RecPtr >> 32), (uint32)RecPtr);
    goto err;
  }

  if ((((XLogPageHeader)state->readBuf)->xlp_info & XLP_FIRST_IS_CONTRECORD) && targetRecOff == pageHeaderSize)
  {
    report_invalid_record(state, "contrecord is requested by %X/%X", (uint32)(RecPtr >> 32), (uint32)RecPtr);
    goto err;
  }

                                                     
  Assert(pageHeaderSize <= readOff);

     
                             
     
                                                                         
                                                                             
                                                                             
                                                                         
                   
     
  record = (XLogRecord *)(state->readBuf + RecPtr % XLOG_BLCKSZ);
  total_len = record->xl_tot_len;

     
                                                                          
                                                                            
                                                                             
                                                                             
                                                               
                                   
     
  if (targetRecOff <= XLOG_BLCKSZ - SizeOfXLogRecord)
  {
    if (!ValidXLogRecordHeader(state, RecPtr, state->ReadRecPtr, record, randAccess))
    {
      goto err;
    }
    gotheader = true;
  }
  else
  {
                                                  
    if (total_len < SizeOfXLogRecord)
    {
      report_invalid_record(state, "invalid record length at %X/%X: wanted %u, got %u", (uint32)(RecPtr >> 32), (uint32)RecPtr, (uint32)SizeOfXLogRecord, total_len);
      goto err;
    }
    gotheader = false;
  }

  len = XLOG_BLCKSZ - RecPtr % XLOG_BLCKSZ;
  if (total_len > len)
  {
                                   
    char *contdata;
    XLogPageHeader pageHeader;
    char *buffer;
    uint32 gotlen;

    assembled = true;

       
                                        
       
    if (total_len > state->readRecordBufSize && !allocate_recordbuf(state, total_len))
    {
                                                     
      report_invalid_record(state, "record length %u at %X/%X too long", total_len, (uint32)(RecPtr >> 32), (uint32)RecPtr);
      goto err;
    }

                                                                    
    memcpy(state->readRecordBuf, state->readBuf + RecPtr % XLOG_BLCKSZ, len);
    buffer = state->readRecordBuf + len;
    gotlen = len;

    do
    {
                                                       
      targetPagePtr += XLOG_BLCKSZ;

                                                      
      readOff = ReadPageInternal(state, targetPagePtr, Min(total_len - gotlen + SizeOfXLogShortPHD, XLOG_BLCKSZ));

      if (readOff < 0)
      {
        goto err;
      }

      Assert(SizeOfXLogShortPHD <= readOff);

      pageHeader = (XLogPageHeader)state->readBuf;

         
                                                               
                                                                         
                                                                       
                                                                     
                                                                    
                                          
         
      if (pageHeader->xlp_info & XLP_FIRST_IS_OVERWRITE_CONTRECORD)
      {
        state->overwrittenRecPtr = RecPtr;
        ResetDecoder(state);
        RecPtr = targetPagePtr;
        goto restart;
      }

                                                                
      if (!(pageHeader->xlp_info & XLP_FIRST_IS_CONTRECORD))
      {
        report_invalid_record(state, "there is no contrecord flag at %X/%X", (uint32)(RecPtr >> 32), (uint32)RecPtr);
        goto err;
      }

         
                                                                         
                                     
         
      if (pageHeader->xlp_rem_len == 0 || total_len != (pageHeader->xlp_rem_len + gotlen))
      {
        report_invalid_record(state, "invalid contrecord length %u at %X/%X", pageHeader->xlp_rem_len, (uint32)(RecPtr >> 32), (uint32)RecPtr);
        goto err;
      }

                                                                
      pageHeaderSize = XLogPageHeaderSize(pageHeader);

      if (readOff < pageHeaderSize)
      {
        readOff = ReadPageInternal(state, targetPagePtr, pageHeaderSize);
      }

      Assert(pageHeaderSize <= readOff);

      contdata = (char *)state->readBuf + pageHeaderSize;
      len = XLOG_BLCKSZ - pageHeaderSize;
      if (pageHeader->xlp_rem_len < len)
      {
        len = pageHeader->xlp_rem_len;
      }

      if (readOff < pageHeaderSize + len)
      {
        readOff = ReadPageInternal(state, targetPagePtr, pageHeaderSize + len);
      }

      memcpy(buffer, (char *)contdata, len);
      buffer += len;
      gotlen += len;

                                                                  
      if (!gotheader)
      {
        record = (XLogRecord *)state->readRecordBuf;
        if (!ValidXLogRecordHeader(state, RecPtr, state->ReadRecPtr, record, randAccess))
        {
          goto err;
        }
        gotheader = true;
      }
    } while (gotlen < total_len);

    Assert(gotheader);

    record = (XLogRecord *)state->readRecordBuf;
    if (!ValidXLogRecord(state, record, RecPtr))
    {
      goto err;
    }

    pageHeaderSize = XLogPageHeaderSize((XLogPageHeader)state->readBuf);
    state->ReadRecPtr = RecPtr;
    state->EndRecPtr = targetPagePtr + pageHeaderSize + MAXALIGN(pageHeader->xlp_rem_len);
  }
  else
  {
                                                      
    readOff = ReadPageInternal(state, targetPagePtr, Min(targetRecOff + total_len, XLOG_BLCKSZ));
    if (readOff < 0)
    {
      goto err;
    }

                                               
    if (!ValidXLogRecord(state, record, RecPtr))
    {
      goto err;
    }

    state->EndRecPtr = RecPtr + MAXALIGN(total_len);

    state->ReadRecPtr = RecPtr;
  }

     
                                                      
     
  if (record->xl_rmid == RM_XLOG_ID && (record->xl_info & ~XLR_INFO_MASK) == XLOG_SWITCH)
  {
                                              
    state->EndRecPtr += state->wal_segment_size - 1;
    state->EndRecPtr -= XLogSegmentOffset(state->EndRecPtr, state->wal_segment_size);
  }

  if (DecodeXLogRecord(state, record, errormsg))
  {
    return record;
  }
  else
  {
    return NULL;
  }

err:
  if (assembled)
  {
       
                                                                       
                                                                         
                                                                          
                                                                          
                                                                         
                                                                          
                         
       
    state->abortedRecPtr = RecPtr;
    state->missingContrecPtr = targetPagePtr;
  }

     
                                                                            
              
     
  XLogReaderInvalReadState(state);

  if (state->errormsg_buf[0] != '\0')
  {
    *errormsg = state->errormsg_buf;
  }

  return NULL;
}

   
                                                                              
                                 
   
                                                                                
                                                                            
   
                                                                               
                                                                   
   
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

                                                            
  if (targetSegNo == state->readSegNo && targetPageOff == state->readOff && reqLen <= state->readLen)
  {
    return state->readLen;
  }

     
                                
     
                                                                            
                                                                            
                                                
     
                                                                            
                                                                       
                                                                            
                                                             
     
  if (targetSegNo != state->readSegNo && targetPageOff != 0)
  {
    XLogRecPtr targetSegmentPtr = pageptr - targetPageOff;

    readLen = state->read_page(state, targetSegmentPtr, XLOG_BLCKSZ, state->currRecPtr, state->readBuf, &state->readPageTLI);
    if (readLen < 0)
    {
      goto err;
    }

                                                                       
    Assert(readLen == XLOG_BLCKSZ);

    if (!XLogReaderValidatePageHeader(state, targetSegmentPtr, state->readBuf))
    {
      goto err;
    }
  }

     
                                                                             
                                 
     
  readLen = state->read_page(state, pageptr, Max(reqLen, SizeOfXLogShortPHD), state->currRecPtr, state->readBuf, &state->readPageTLI);
  if (readLen < 0)
  {
    goto err;
  }

  Assert(readLen <= XLOG_BLCKSZ);

                                                          
  if (readLen <= SizeOfXLogShortPHD)
  {
    goto err;
  }

  Assert(readLen >= reqLen);

  hdr = (XLogPageHeader)state->readBuf;

                        
  if (readLen < XLogPageHeaderSize(hdr))
  {
    readLen = state->read_page(state, pageptr, XLogPageHeaderSize(hdr), state->currRecPtr, state->readBuf, &state->readPageTLI);
    if (readLen < 0)
    {
      goto err;
    }
  }

     
                                                            
     
  if (!XLogReaderValidatePageHeader(state, pageptr, (char *)hdr))
  {
    goto err;
  }

                                     
  state->readSegNo = targetSegNo;
  state->readOff = targetPageOff;
  state->readLen = readLen;

  return readLen;

err:
  XLogReaderInvalReadState(state);
  return -1;
}

   
                                                              
   
void
XLogReaderInvalReadState(XLogReaderState *state)
{
  state->readSegNo = 0;
  state->readOff = 0;
  state->readLen = 0;
}

   
                                   
   
                                                                     
                                                                  
   
static bool
ValidXLogRecordHeader(XLogReaderState *state, XLogRecPtr RecPtr, XLogRecPtr PrevRecPtr, XLogRecord *record, bool randAccess)
{
  if (record->xl_tot_len < SizeOfXLogRecord)
  {
    report_invalid_record(state, "invalid record length at %X/%X: wanted %u, got %u", (uint32)(RecPtr >> 32), (uint32)RecPtr, (uint32)SizeOfXLogRecord, record->xl_tot_len);
    return false;
  }
  if (record->xl_rmid > RM_MAX_ID)
  {
    report_invalid_record(state, "invalid resource manager ID %u at %X/%X", record->xl_rmid, (uint32)(RecPtr >> 32), (uint32)RecPtr);
    return false;
  }
  if (randAccess)
  {
       
                                                                           
                                      
       
    if (!(record->xl_prev < RecPtr))
    {
      report_invalid_record(state, "record with incorrect prev-link %X/%X at %X/%X", (uint32)(record->xl_prev >> 32), (uint32)record->xl_prev, (uint32)(RecPtr >> 32), (uint32)RecPtr);
      return false;
    }
  }
  else
  {
       
                                                                           
                                                                           
                                               
       
    if (record->xl_prev != PrevRecPtr)
    {
      report_invalid_record(state, "record with incorrect prev-link %X/%X at %X/%X", (uint32)(record->xl_prev >> 32), (uint32)record->xl_prev, (uint32)(RecPtr >> 32), (uint32)RecPtr);
      return false;
    }
  }

  return true;
}

   
                                                                        
                                                                       
                                                  
   
                                                                         
                                                                           
                                                                          
                     
   
static bool
ValidXLogRecord(XLogReaderState *state, XLogRecord *record, XLogRecPtr recptr)
{
  pg_crc32c crc;

                         
  INIT_CRC32C(crc);
  COMP_CRC32C(crc, ((char *)record) + SizeOfXLogRecord, record->xl_tot_len - SizeOfXLogRecord);
                                      
  COMP_CRC32C(crc, (char *)record, offsetof(XLogRecord, xl_crc));
  FIN_CRC32C(crc);

  if (!EQ_CRC32C(record->xl_crc, crc))
  {
    report_invalid_record(state, "incorrect resource manager data checksum in record at %X/%X", (uint32)(recptr >> 32), (uint32)recptr);
    return false;
  }

  return true;
}

   
                           
   
                                                                       
             
   
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

  if (hdr->xlp_magic != XLOG_PAGE_MAGIC)
  {
    char fname[MAXFNAMELEN];

    XLogFileName(fname, state->readPageTLI, segno, state->wal_segment_size);

    report_invalid_record(state, "invalid magic number %04X in log segment %s, offset %u", hdr->xlp_magic, fname, offset);
    return false;
  }

  if ((hdr->xlp_info & ~XLP_ALL_FLAGS) != 0)
  {
    char fname[MAXFNAMELEN];

    XLogFileName(fname, state->readPageTLI, segno, state->wal_segment_size);

    report_invalid_record(state, "invalid info bits %04X in log segment %s, offset %u", hdr->xlp_info, fname, offset);
    return false;
  }

  if (hdr->xlp_info & XLP_LONG_HEADER)
  {
    XLogLongPageHeader longhdr = (XLogLongPageHeader)hdr;

    if (state->system_identifier && longhdr->xlp_sysid != state->system_identifier)
    {
      char fhdrident_str[32];
      char sysident_str[32];

         
                                                                         
                                                 
         
      snprintf(fhdrident_str, sizeof(fhdrident_str), UINT64_FORMAT, longhdr->xlp_sysid);
      snprintf(sysident_str, sizeof(sysident_str), UINT64_FORMAT, state->system_identifier);
      report_invalid_record(state, "WAL file is from different database system: WAL file database system identifier is %s, pg_control database system identifier is %s", fhdrident_str, sysident_str);
      return false;
    }
    else if (longhdr->xlp_seg_size != state->wal_segment_size)
    {
      report_invalid_record(state, "WAL file is from different database system: incorrect segment size in page header");
      return false;
    }
    else if (longhdr->xlp_xlog_blcksz != XLOG_BLCKSZ)
    {
      report_invalid_record(state, "WAL file is from different database system: incorrect XLOG_BLCKSZ in page header");
      return false;
    }
  }
  else if (offset == 0)
  {
    char fname[MAXFNAMELEN];

    XLogFileName(fname, state->readPageTLI, segno, state->wal_segment_size);

                                                             
    report_invalid_record(state, "invalid info bits %04X in log segment %s, offset %u", hdr->xlp_info, fname, offset);
    return false;
  }

     
                                                                           
                                                                           
                                             
     
  if (hdr->xlp_pageaddr != recaddr)
  {
    char fname[MAXFNAMELEN];

    XLogFileName(fname, state->readPageTLI, segno, state->wal_segment_size);

    report_invalid_record(state, "unexpected pageaddr %X/%X in log segment %s, offset %u", (uint32)(hdr->xlp_pageaddr >> 32), (uint32)hdr->xlp_pageaddr, fname, offset);
    return false;
  }

     
                                                                        
                                                                         
                                                    
     
                                                                             
                                                                           
          
     
  if (recptr > state->latestPagePtr)
  {
    if (hdr->xlp_tli < state->latestPageTLI)
    {
      char fname[MAXFNAMELEN];

      XLogFileName(fname, state->readPageTLI, segno, state->wal_segment_size);

      report_invalid_record(state, "out-of-sequence timeline ID %u (after %u) in log segment %s, offset %u", hdr->xlp_tli, state->latestPageTLI, fname, offset);
      return false;
    }
  }
  state->latestPagePtr = recptr;
  state->latestPageTLI = hdr->xlp_tli;

  return true;
}

#ifdef FRONTEND
   
                                                                          
                                                                                
         
   

   
                                                
   
                                                                               
                                                                               
                       
   
XLogRecPtr
XLogFindNextRecord(XLogReaderState *state, XLogRecPtr RecPtr)
{
  XLogReaderState saved_state = *state;
  XLogRecPtr tmpRecPtr;
  XLogRecPtr found = InvalidXLogRecPtr;
  XLogPageHeader header;
  char *errormsg;

  Assert(!XLogRecPtrIsInvalid(RecPtr));

     
                                                                             
                    
     
  tmpRecPtr = RecPtr;
  while (true)
  {
    XLogRecPtr targetPagePtr;
    int targetRecOff;
    uint32 pageHeaderSize;
    int readLen;

       
                                                                          
                                                                          
                                                                         
                                                                
                                                             
                                                                      
                                             
       
    targetRecOff = tmpRecPtr % XLOG_BLCKSZ;

                                      
    targetPagePtr = tmpRecPtr - targetRecOff;

                                             
    readLen = ReadPageInternal(state, targetPagePtr, targetRecOff);
    if (readLen < 0)
    {
      goto err;
    }

    header = (XLogPageHeader)state->readBuf;

    pageHeaderSize = XLogPageHeaderSize(header);

                                                           
    readLen = ReadPageInternal(state, targetPagePtr, pageHeaderSize);
    if (readLen < 0)
    {
      goto err;
    }

                                               
    if (header->xlp_info & XLP_FIRST_IS_CONTRECORD)
    {
         
                                                                       
                                                                         
                                                                         
                                                                   
                           
         
                                                  
         
      if (MAXALIGN(header->xlp_rem_len) >= (XLOG_BLCKSZ - pageHeaderSize))
      {
        tmpRecPtr = targetPagePtr + XLOG_BLCKSZ;
      }
      else
      {
           
                                                                   
                                                        
           
        tmpRecPtr = targetPagePtr + pageHeaderSize + MAXALIGN(header->xlp_rem_len);
        break;
      }
    }
    else
    {
      tmpRecPtr = targetPagePtr + pageHeaderSize;
      break;
    }
  }

     
                                                                             
                                                                            
                                                                  
     
  while (XLogReadRecord(state, tmpRecPtr, &errormsg) != NULL)
  {
                                   
    tmpRecPtr = InvalidXLogRecPtr;

                                                
    if (RecPtr <= state->ReadRecPtr)
    {
      found = state->ReadRecPtr;
      goto out;
    }
  }

err:
out:
                                                            
  state->ReadRecPtr = saved_state.ReadRecPtr;
  state->EndRecPtr = saved_state.EndRecPtr;
  XLogReaderInvalReadState(state);

  return found;
}

#endif               

                                            
                                                                     
                                            
   

                                                         
static void
ResetDecoder(XLogReaderState *state)
{
  int block_id;

  state->decoded_record = NULL;

  state->main_data_len = 0;

  for (block_id = 0; block_id <= state->max_block_id; block_id++)
  {
    state->blocks[block_id].in_use = false;
    state->blocks[block_id].has_image = false;
    state->blocks[block_id].has_data = false;
    state->blocks[block_id].apply_image = false;
  }
  state->max_block_id = -1;
}

   
                                      
   
                                                                          
                              
   
bool
DecodeXLogRecord(XLogReaderState *state, XLogRecord *record, char **errormsg)
{
     
                                                                            
     
#define COPY_HEADER_FIELD(_dst, _size)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
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

                          
  datatotal = 0;
  while (remaining > datatotal)
  {
    COPY_HEADER_FIELD(&block_id, sizeof(uint8));

    if (block_id == XLR_BLOCK_ID_DATA_SHORT)
    {
                                     
      uint8 main_data_len;

      COPY_HEADER_FIELD(&main_data_len, sizeof(uint8));

      state->main_data_len = main_data_len;
      datatotal += main_data_len;
      break;                                             
                              
    }
    else if (block_id == XLR_BLOCK_ID_DATA_LONG)
    {
                                    
      uint32 main_data_len;

      COPY_HEADER_FIELD(&main_data_len, sizeof(uint32));
      state->main_data_len = main_data_len;
      datatotal += main_data_len;
      break;                                             
                              
    }
    else if (block_id == XLR_BLOCK_ID_ORIGIN)
    {
      COPY_HEADER_FIELD(&state->record_origin, sizeof(RepOriginId));
    }
    else if (block_id <= XLR_MAX_BLOCK_ID)
    {
                                 
      DecodedBkpBlock *blk;
      uint8 fork_flags;

      if (block_id <= state->max_block_id)
      {
        report_invalid_record(state, "out-of-order block_id %u at %X/%X", block_id, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
        goto err;
      }
      state->max_block_id = block_id;

      blk = &state->blocks[block_id];
      blk->in_use = true;
      blk->apply_image = false;

      COPY_HEADER_FIELD(&fork_flags, sizeof(uint8));
      blk->forknum = fork_flags & BKPBLOCK_FORK_MASK;
      blk->flags = fork_flags;
      blk->has_image = ((fork_flags & BKPBLOCK_HAS_IMAGE) != 0);
      blk->has_data = ((fork_flags & BKPBLOCK_HAS_DATA) != 0);

      COPY_HEADER_FIELD(&blk->data_len, sizeof(uint16));
                                                                         
      if (blk->has_data && blk->data_len == 0)
      {
        report_invalid_record(state, "BKPBLOCK_HAS_DATA set, but no data included at %X/%X", (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
        goto err;
      }
      if (!blk->has_data && blk->data_len != 0)
      {
        report_invalid_record(state, "BKPBLOCK_HAS_DATA not set, but data length is %u at %X/%X", (unsigned int)blk->data_len, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
        goto err;
      }
      datatotal += blk->data_len;

      if (blk->has_image)
      {
        COPY_HEADER_FIELD(&blk->bimg_len, sizeof(uint16));
        COPY_HEADER_FIELD(&blk->hole_offset, sizeof(uint16));
        COPY_HEADER_FIELD(&blk->bimg_info, sizeof(uint8));

        blk->apply_image = ((blk->bimg_info & BKPIMAGE_APPLY) != 0);

        if (blk->bimg_info & BKPIMAGE_IS_COMPRESSED)
        {
          if (blk->bimg_info & BKPIMAGE_HAS_HOLE)
          {
            COPY_HEADER_FIELD(&blk->hole_length, sizeof(uint16));
          }
          else
          {
            blk->hole_length = 0;
          }
        }
        else
        {
          blk->hole_length = BLCKSZ - blk->bimg_len;
        }
        datatotal += blk->bimg_len;

           
                                                                 
                                                          
           
        if ((blk->bimg_info & BKPIMAGE_HAS_HOLE) && (blk->hole_offset == 0 || blk->hole_length == 0 || blk->bimg_len == BLCKSZ))
        {
          report_invalid_record(state, "BKPIMAGE_HAS_HOLE set, but hole offset %u length %u block image length %u at %X/%X", (unsigned int)blk->hole_offset, (unsigned int)blk->hole_length, (unsigned int)blk->bimg_len, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
          goto err;
        }

           
                                                                     
                                         
           
        if (!(blk->bimg_info & BKPIMAGE_HAS_HOLE) && (blk->hole_offset != 0 || blk->hole_length != 0))
        {
          report_invalid_record(state, "BKPIMAGE_HAS_HOLE not set, but hole offset %u length %u at %X/%X", (unsigned int)blk->hole_offset, (unsigned int)blk->hole_length, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
          goto err;
        }

           
                                                                   
                        
           
        if ((blk->bimg_info & BKPIMAGE_IS_COMPRESSED) && blk->bimg_len == BLCKSZ)
        {
          report_invalid_record(state, "BKPIMAGE_IS_COMPRESSED set, but block image length %u at %X/%X", (unsigned int)blk->bimg_len, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
          goto err;
        }

           
                                                                      
                                      
           
        if (!(blk->bimg_info & BKPIMAGE_HAS_HOLE) && !(blk->bimg_info & BKPIMAGE_IS_COMPRESSED) && blk->bimg_len != BLCKSZ)
        {
          report_invalid_record(state, "neither BKPIMAGE_HAS_HOLE nor BKPIMAGE_IS_COMPRESSED set, but block image length is %u at %X/%X", (unsigned int)blk->data_len, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
          goto err;
        }
      }
      if (!(fork_flags & BKPBLOCK_SAME_REL))
      {
        COPY_HEADER_FIELD(&blk->rnode, sizeof(RelFileNode));
        rnode = &blk->rnode;
      }
      else
      {
        if (rnode == NULL)
        {
          report_invalid_record(state, "BKPBLOCK_SAME_REL set but no previous rel at %X/%X", (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
          goto err;
        }

        blk->rnode = *rnode;
      }
      COPY_HEADER_FIELD(&blk->blkno, sizeof(BlockNumber));
    }
    else
    {
      report_invalid_record(state, "invalid block_id %u at %X/%X", block_id, (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
      goto err;
    }
  }

  if (remaining != datatotal)
  {
    goto shortdata_err;
  }

     
                                                                        
                                                                           
                                                                
     
                                                                            
                                                                        
                                                 
     

                        
  for (block_id = 0; block_id <= state->max_block_id; block_id++)
  {
    DecodedBkpBlock *blk = &state->blocks[block_id];

    if (!blk->in_use)
    {
      continue;
    }

    Assert(blk->has_image || !blk->apply_image);

    if (blk->has_image)
    {
      blk->bkp_image = ptr;
      ptr += blk->bimg_len;
    }
    if (blk->has_data)
    {
      if (!blk->data || blk->data_len > blk->data_bufsz)
      {
        if (blk->data)
        {
          pfree(blk->data);
        }

           
                                                                   
                                                                  
                                      
           
        blk->data_bufsz = MAXALIGN(Max(blk->data_len, BLCKSZ));
        blk->data = palloc(blk->data_bufsz);
      }
      memcpy(blk->data, ptr, blk->data_len);
      ptr += blk->data_len;
    }
  }

                                  
  if (state->main_data_len > 0)
  {
    if (!state->main_data || state->main_data_len > state->main_data_bufsz)
    {
      if (state->main_data)
      {
        pfree(state->main_data);
      }

         
                                                                   
                                                                      
                                                                       
                                                                    
                                                                        
                                                                         
                               
         
                                                                       
                                                                     
                                                                  
         
      state->main_data_bufsz = MAXALIGN(Max(state->main_data_len, BLCKSZ / 2));
      state->main_data = palloc(state->main_data_bufsz);
    }
    memcpy(state->main_data, ptr, state->main_data_len);
    ptr += state->main_data_len;
  }

  return true;

shortdata_err:
  report_invalid_record(state, "record with invalid length at %X/%X", (uint32)(state->ReadRecPtr >> 32), (uint32)state->ReadRecPtr);
err:
  *errormsg = state->errormsg_buf;

  return false;
}

   
                                                                         
   
                                                                           
                                                                        
                            
   
bool
XLogRecGetBlockTag(XLogReaderState *record, uint8 block_id, RelFileNode *rnode, ForkNumber *forknum, BlockNumber *blknum)
{
  DecodedBkpBlock *bkpb;

  if (!record->blocks[block_id].in_use)
  {
    return false;
  }

  bkpb = &record->blocks[block_id];
  if (rnode)
  {
    *rnode = bkpb->rnode;
  }
  if (forknum)
  {
    *forknum = bkpb->forknum;
  }
  if (blknum)
  {
    *blknum = bkpb->blkno;
  }
  return true;
}

   
                                                                           
                                                                            
                                          
   
char *
XLogRecGetBlockData(XLogReaderState *record, uint8 block_id, Size *len)
{
  DecodedBkpBlock *bkpb;

  if (!record->blocks[block_id].in_use)
  {
    return NULL;
  }

  bkpb = &record->blocks[block_id];

  if (!bkpb->has_data)
  {
    if (len)
    {
      *len = 0;
    }
    return NULL;
  }
  else
  {
    if (len)
    {
      *len = bkpb->data_len;
    }
    return bkpb->data;
  }
}

   
                                                                             
   
                                                  
   
bool
RestoreBlockImage(XLogReaderState *record, uint8 block_id, char *page)
{
  DecodedBkpBlock *bkpb;
  char *ptr;
  PGAlignedBlock tmp;

  if (!record->blocks[block_id].in_use)
  {
    return false;
  }
  if (!record->blocks[block_id].has_image)
  {
    return false;
  }

  bkpb = &record->blocks[block_id];
  ptr = bkpb->bkp_image;

  if (bkpb->bimg_info & BKPIMAGE_IS_COMPRESSED)
  {
                                                              
    if (pglz_decompress(ptr, bkpb->bimg_len, tmp.data, BLCKSZ - bkpb->hole_length, true) < 0)
    {
      report_invalid_record(record, "invalid compressed image at %X/%X, block %d", (uint32)(record->ReadRecPtr >> 32), (uint32)record->ReadRecPtr, block_id);
      return false;
    }
    ptr = tmp.data;
  }

                                                            
  if (bkpb->hole_length == 0)
  {
    memcpy(page, ptr, BLCKSZ);
  }
  else
  {
    memcpy(page, ptr, bkpb->hole_offset);
                                 
    MemSet(page + bkpb->hole_offset, 0, bkpb->hole_length);
    memcpy(page + (bkpb->hole_offset + bkpb->hole_length), ptr + bkpb->hole_offset, BLCKSZ - (bkpb->hole_offset + bkpb->hole_length));
  }

  return true;
}
