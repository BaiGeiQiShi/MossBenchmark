                                                                            
   
                
                                           
   
                                                                    
                                                                       
                                                                           
                                                                 
                                      
   
                                                                         
                                                                        
   
                                           
   
                                                                            
   

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

                                                                              
#define PGLZ_MAX_BLCKSZ PGLZ_MAX_OUTPUT(BLCKSZ)

   
                                                                           
                               
   
typedef struct
{
  bool in_use;                                 
  uint8 flags;                           
  RelFileNode rnode;                                        
  ForkNumber forkno;
  BlockNumber block;
  Page page;                                 
  uint32 rdata_len;                                                 
  XLogRecData *rdata_head;                                              
                                           
  XLogRecData *rdata_tail;                                               
                                      

  XLogRecData bkp_rdatas[2];                                                
                                                                            

                                                                  
  char compressed_page[PGLZ_MAX_BLCKSZ];
} registered_buffer;

static registered_buffer *registered_buffers;
static int max_registered_buffers;                          
static int max_registered_block_id = 0;                                   
                                                        

   
                                                                               
                               
   
static XLogRecData *mainrdata_head;
static XLogRecData *mainrdata_last = (XLogRecData *)&mainrdata_head;
static uint32 mainrdata_len;                                

                                         
static uint8 curinsert_flags = 0;

   
                                                                         
                                                                             
                                                                 
   
                                                                           
               
   
static XLogRecData hdr_rdt;
static char *hdr_scratch = NULL;

#define SizeOfXlogOrigin (sizeof(RepOriginId) + sizeof(char))

#define HEADER_SCRATCH_SIZE (SizeOfXLogRecord + MaxSizeOfXLogRecordBlockHeader * (XLR_MAX_BLOCK_ID + 1) + SizeOfXLogRecordDataHeaderLong + SizeOfXlogOrigin)

   
                                                             
   
static XLogRecData *rdatas;
static int num_rdatas;                             
static int max_rdatas;                     

static bool begininsert_called = false;

                                                                       
static MemoryContext xloginsert_cxt;

static XLogRecData *
XLogRecordAssemble(RmgrId rmid, uint8 info, XLogRecPtr RedoRecPtr, bool doPageWrites, XLogRecPtr *fpw_lsn);
static bool
XLogCompressBackupBlock(char *page, uint16 hole_offset, uint16 hole_length, char *dest, uint16 *dlen);

   
                                                                   
                                             
   
void
XLogBeginInsert(void)
{
  Assert(max_registered_block_id == 0);
  Assert(mainrdata_last == (XLogRecData *)&mainrdata_head);
  Assert(mainrdata_len == 0);

                                                       
  if (!XLogInsertAllowed())
  {
    elog(ERROR, "cannot make new WAL entries during recovery");
  }

  if (begininsert_called)
  {
    elog(ERROR, "XLogBeginInsert was already called");
  }

  begininsert_called = true;
}

   
                                                                           
                                                                               
          
   
                                                                               
                                                                               
         
   
void
XLogEnsureRecordSpace(int max_block_id, int ndatas)
{
  int nbuffers;

     
                                                                     
                                                                           
                                                                     
                                                                        
     
  Assert(CritSectionCount == 0);

                                             
  if (max_block_id < XLR_NORMAL_MAX_BLOCK_ID)
  {
    max_block_id = XLR_NORMAL_MAX_BLOCK_ID;
  }
  if (ndatas < XLR_NORMAL_RDATAS)
  {
    ndatas = XLR_NORMAL_RDATAS;
  }

  if (max_block_id > XLR_MAX_BLOCK_ID)
  {
    elog(ERROR, "maximum number of WAL record block references exceeded");
  }
  nbuffers = max_block_id + 1;

  if (nbuffers > max_registered_buffers)
  {
    registered_buffers = (registered_buffer *)repalloc(registered_buffers, sizeof(registered_buffer) * nbuffers);

       
                                                                         
                                                                          
       
    MemSet(&registered_buffers[max_registered_buffers], 0, (nbuffers - max_registered_buffers) * sizeof(registered_buffer));
    max_registered_buffers = nbuffers;
  }

  if (ndatas > max_rdatas)
  {
    rdatas = (XLogRecData *)repalloc(rdatas, sizeof(XLogRecData) * ndatas);
    max_rdatas = ndatas;
  }
}

   
                                          
   
void
XLogResetInsertion(void)
{
  int i;

  for (i = 0; i < max_registered_block_id; i++)
  {
    registered_buffers[i].in_use = false;
  }

  num_rdatas = 0;
  max_registered_block_id = 0;
  mainrdata_len = 0;
  mainrdata_last = (XLogRecData *)&mainrdata_head;
  curinsert_flags = 0;
  begininsert_called = false;
}

   
                                                                           
                                                                              
   
void
XLogRegisterBuffer(uint8 block_id, Buffer buffer, uint8 flags)
{
  registered_buffer *regbuf;

                                                    
  Assert(!((flags & REGBUF_FORCE_IMAGE) && (flags & (REGBUF_NO_IMAGE))));
  Assert(begininsert_called);

  if (block_id >= max_registered_block_id)
  {
    if (block_id >= max_registered_buffers)
    {
      elog(ERROR, "too many registered buffers");
    }
    max_registered_block_id = block_id + 1;
  }

  regbuf = &registered_buffers[block_id];

  BufferGetTag(buffer, &regbuf->rnode, &regbuf->forkno, &regbuf->block);
  regbuf->page = BufferGetPage(buffer);
  regbuf->flags = flags;
  regbuf->rdata_tail = (XLogRecData *)&regbuf->rdata_head;
  regbuf->rdata_len = 0;

     
                                                                         
               
     
#ifdef USE_ASSERT_CHECKING
  {
    int i;

    for (i = 0; i < max_registered_block_id; i++)
    {
      registered_buffer *regbuf_old = &registered_buffers[i];

      if (i == block_id || !regbuf_old->in_use)
      {
        continue;
      }

      Assert(!RelFileNodeEquals(regbuf_old->rnode, regbuf->rnode) || regbuf_old->forkno != regbuf->forkno || regbuf_old->block != regbuf->block);
    }
  }
#endif

  regbuf->in_use = true;
}

   
                                                                          
                                                                  
   
void
XLogRegisterBlock(uint8 block_id, RelFileNode *rnode, ForkNumber forknum, BlockNumber blknum, Page page, uint8 flags)
{
  registered_buffer *regbuf;

                                                                          
  Assert(flags & REGBUF_FORCE_IMAGE);
  Assert(begininsert_called);

  if (block_id >= max_registered_block_id)
  {
    max_registered_block_id = block_id + 1;
  }

  if (block_id >= max_registered_buffers)
  {
    elog(ERROR, "too many registered buffers");
  }

  regbuf = &registered_buffers[block_id];

  regbuf->rnode = *rnode;
  regbuf->forkno = forknum;
  regbuf->block = blknum;
  regbuf->page = page;
  regbuf->flags = flags;
  regbuf->rdata_tail = (XLogRecData *)&regbuf->rdata_head;
  regbuf->rdata_len = 0;

     
                                                                         
               
     
#ifdef USE_ASSERT_CHECKING
  {
    int i;

    for (i = 0; i < max_registered_block_id; i++)
    {
      registered_buffer *regbuf_old = &registered_buffers[i];

      if (i == block_id || !regbuf_old->in_use)
      {
        continue;
      }

      Assert(!RelFileNodeEquals(regbuf_old->rnode, regbuf->rnode) || regbuf_old->forkno != regbuf->forkno || regbuf_old->block != regbuf->block);
    }
  }
#endif

  regbuf->in_use = true;
}

   
                                                        
   
                                                                      
                     
   
void
XLogRegisterData(char *data, int len)
{
  XLogRecData *rdata;

  Assert(begininsert_called);

  if (num_rdatas >= max_rdatas)
  {
    elog(ERROR, "too much WAL data");
  }
  rdata = &rdatas[num_rdatas++];

  rdata->data = data;
  rdata->len = len;

     
                                                                            
                                
     

  mainrdata_last->next = rdata;
  mainrdata_last = rdata;

  mainrdata_len += len;
}

   
                                                                        
   
                                                              
                                                                       
                                   
   
                                                                        
                                                                       
                                                                          
                                                                          
            
   
void
XLogRegisterBufData(uint8 block_id, char *data, int len)
{
  registered_buffer *regbuf;
  XLogRecData *rdata;

  Assert(begininsert_called);

                                         
  regbuf = &registered_buffers[block_id];
  if (!regbuf->in_use)
  {
    elog(ERROR, "no block with id %d registered with WAL insertion", block_id);
  }

  if (num_rdatas >= max_rdatas)
  {
    elog(ERROR, "too much WAL data");
  }
  rdata = &rdatas[num_rdatas++];

  rdata->data = data;
  rdata->len = len;

  regbuf->rdata_tail->next = rdata;
  regbuf->rdata_tail = rdata;
  regbuf->rdata_len += len;
}

   
                                                        
   
                                        
                                                                           
                            
                                                                           
                                                                         
                         
   
void
XLogSetRecordFlags(uint8 flags)
{
  Assert(begininsert_called);
  curinsert_flags = flags;
}

   
                                                                            
                                                                              
                             
   
                                                                     
                                                                         
                                                                       
                                                                       
                                              
   
XLogRecPtr
XLogInsert(RmgrId rmid, uint8 info)
{
  XLogRecPtr EndPos;

                                                
  if (!begininsert_called)
  {
    elog(ERROR, "XLogBeginInsert was not called");
  }

     
                                                              
                                                                 
     
  if ((info & ~(XLR_RMGR_INFO_MASK | XLR_SPECIAL_REL_UPDATE | XLR_CHECK_CONSISTENCY)) != 0)
  {
    elog(PANIC, "invalid xlog info mask %02X", info);
  }

  TRACE_POSTGRESQL_WAL_INSERT(rmid, info);

     
                                                                           
                                    
     
  if (IsBootstrapProcessingMode() && rmid != RM_XLOG_ID)
  {
    XLogResetInsertion();
    EndPos = SizeOfXLogLongPHD;                                
    return EndPos;
  }

  do
  {
    XLogRecPtr RedoRecPtr;
    bool doPageWrites;
    XLogRecPtr fpw_lsn;
    XLogRecData *rdt;

       
                                                                         
                                                                         
                                                                  
       
    GetFullPageWriteInfo(&RedoRecPtr, &doPageWrites);

    rdt = XLogRecordAssemble(rmid, info, RedoRecPtr, doPageWrites, &fpw_lsn);

    EndPos = XLogInsertRecord(rdt, fpw_lsn, curinsert_flags);
  } while (EndPos == InvalidXLogRecPtr);

  XLogResetInsertion();

  return EndPos;
}

   
                                                                      
                                                                   
   
                                                                             
                                                          
   
                                                                            
                                                                            
                                                                       
                                                                           
   
static XLogRecData *
XLogRecordAssemble(RmgrId rmid, uint8 info, XLogRecPtr RedoRecPtr, bool doPageWrites, XLogRecPtr *fpw_lsn)
{
  XLogRecData *rdt;
  uint32 total_len = 0;
  int block_id;
  pg_crc32c rdata_crc;
  registered_buffer *prev_regbuf = NULL;
  XLogRecData *rdt_datas_last;
  XLogRecord *rechdr;
  char *scratch = hdr_scratch;

     
                                                                           
                                                                             
     

                                                    
  rechdr = (XLogRecord *)scratch;
  scratch += SizeOfXLogRecord;

  hdr_rdt.next = NULL;
  rdt_datas_last = &hdr_rdt;
  hdr_rdt.data = hdr_scratch;

     
                                                                           
                                                                             
                                                                            
               
     
  if (wal_consistency_checking[rmid])
  {
    info |= XLR_CHECK_CONSISTENCY;
  }

     
                                                                       
                                                                          
                                                                 
     
  *fpw_lsn = InvalidXLogRecPtr;
  for (block_id = 0; block_id < max_registered_block_id; block_id++)
  {
    registered_buffer *regbuf = &registered_buffers[block_id];
    bool needs_backup;
    bool needs_data;
    XLogRecordBlockHeader bkpb;
    XLogRecordBlockImageHeader bimg;
    XLogRecordBlockCompressHeader cbimg = {0};
    bool samerel;
    bool is_compressed = false;
    bool include_image;

    if (!regbuf->in_use)
    {
      continue;
    }

                                                       
    if (regbuf->flags & REGBUF_FORCE_IMAGE)
    {
      needs_backup = true;
    }
    else if (regbuf->flags & REGBUF_NO_IMAGE)
    {
      needs_backup = false;
    }
    else if (!doPageWrites)
    {
      needs_backup = false;
    }
    else
    {
         
                                                                      
                                                                       
                 
         
      XLogRecPtr page_lsn = PageGetLSN(regbuf->page);

      needs_backup = (page_lsn <= RedoRecPtr);
      if (!needs_backup)
      {
        if (*fpw_lsn == InvalidXLogRecPtr || page_lsn < *fpw_lsn)
        {
          *fpw_lsn = page_lsn;
        }
      }
    }

                                                        
    if (regbuf->rdata_len == 0)
    {
      needs_data = false;
    }
    else if ((regbuf->flags & REGBUF_KEEP_DATA) != 0)
    {
      needs_data = true;
    }
    else
    {
      needs_data = !needs_backup;
    }

    bkpb.id = block_id;
    bkpb.fork_flags = regbuf->forkno;
    bkpb.data_length = 0;

    if ((regbuf->flags & REGBUF_WILL_INIT) == REGBUF_WILL_INIT)
    {
      bkpb.fork_flags |= BKPBLOCK_WILL_INIT;
    }

       
                                                                      
                                                                      
       
    include_image = needs_backup || (info & XLR_CHECK_CONSISTENCY) != 0;

    if (include_image)
    {
      Page page = regbuf->page;
      uint16 compressed_len = 0;

         
                                                                      
                     
         
      if (regbuf->flags & REGBUF_STANDARD)
      {
                                                                   
        uint16 lower = ((PageHeader)page)->pd_lower;
        uint16 upper = ((PageHeader)page)->pd_upper;

        if (lower >= SizeOfPageHeaderData && upper > lower && upper <= BLCKSZ)
        {
          bimg.hole_offset = lower;
          cbimg.hole_length = upper - lower;
        }
        else
        {
                                   
          bimg.hole_offset = 0;
          cbimg.hole_length = 0;
        }
      }
      else
      {
                                                                       
        bimg.hole_offset = 0;
        cbimg.hole_length = 0;
      }

         
                                                                     
         
      if (wal_compression)
      {
        is_compressed = XLogCompressBackupBlock(page, bimg.hole_offset, cbimg.hole_length, regbuf->compressed_page, &compressed_len);
      }

         
                                                                   
                
         
      bkpb.fork_flags |= BKPBLOCK_HAS_IMAGE;

         
                                                             
         
      rdt_datas_last->next = &regbuf->bkp_rdatas[0];
      rdt_datas_last = rdt_datas_last->next;

      bimg.bimg_info = (cbimg.hole_length == 0) ? 0 : BKPIMAGE_HAS_HOLE;

         
                                                                         
                                                                         
                                                                        
                                        
         
      if (needs_backup)
      {
        bimg.bimg_info |= BKPIMAGE_APPLY;
      }

      if (is_compressed)
      {
        bimg.length = compressed_len;
        bimg.bimg_info |= BKPIMAGE_IS_COMPRESSED;

        rdt_datas_last->data = regbuf->compressed_page;
        rdt_datas_last->len = compressed_len;
      }
      else
      {
        bimg.length = BLCKSZ - cbimg.hole_length;

        if (cbimg.hole_length == 0)
        {
          rdt_datas_last->data = page;
          rdt_datas_last->len = BLCKSZ;
        }
        else
        {
                                  
          rdt_datas_last->data = page;
          rdt_datas_last->len = bimg.hole_offset;

          rdt_datas_last->next = &regbuf->bkp_rdatas[1];
          rdt_datas_last = rdt_datas_last->next;

          rdt_datas_last->data = page + (bimg.hole_offset + cbimg.hole_length);
          rdt_datas_last->len = BLCKSZ - (bimg.hole_offset + cbimg.hole_length);
        }
      }

      total_len += bimg.length;
    }

    if (needs_data)
    {
         
                                                                     
                       
         
      bkpb.fork_flags |= BKPBLOCK_HAS_DATA;
      bkpb.data_length = regbuf->rdata_len;
      total_len += regbuf->rdata_len;

      rdt_datas_last->next = regbuf->rdata_head;
      rdt_datas_last = regbuf->rdata_tail;
    }

    if (prev_regbuf && RelFileNodeEquals(regbuf->rnode, prev_regbuf->rnode))
    {
      samerel = true;
      bkpb.fork_flags |= BKPBLOCK_SAME_REL;
    }
    else
    {
      samerel = false;
    }
    prev_regbuf = regbuf;

                                                   
    memcpy(scratch, &bkpb, SizeOfXLogRecordBlockHeader);
    scratch += SizeOfXLogRecordBlockHeader;
    if (include_image)
    {
      memcpy(scratch, &bimg, SizeOfXLogRecordBlockImageHeader);
      scratch += SizeOfXLogRecordBlockImageHeader;
      if (cbimg.hole_length != 0 && is_compressed)
      {
        memcpy(scratch, &cbimg, SizeOfXLogRecordBlockCompressHeader);
        scratch += SizeOfXLogRecordBlockCompressHeader;
      }
    }
    if (!samerel)
    {
      memcpy(scratch, &regbuf->rnode, sizeof(RelFileNode));
      scratch += sizeof(RelFileNode);
    }
    memcpy(scratch, &regbuf->block, sizeof(BlockNumber));
    scratch += sizeof(BlockNumber);
  }

                                               
  if ((curinsert_flags & XLOG_INCLUDE_ORIGIN) && replorigin_session_origin != InvalidRepOriginId)
  {
    *(scratch++) = (char)XLR_BLOCK_ID_ORIGIN;
    memcpy(scratch, &replorigin_session_origin, sizeof(replorigin_session_origin));
    scratch += sizeof(replorigin_session_origin);
  }

                                     
  if (mainrdata_len > 0)
  {
    if (mainrdata_len > 255)
    {
      *(scratch++) = (char)XLR_BLOCK_ID_DATA_LONG;
      memcpy(scratch, &mainrdata_len, sizeof(uint32));
      scratch += sizeof(uint32);
    }
    else
    {
      *(scratch++) = (char)XLR_BLOCK_ID_DATA_SHORT;
      *(scratch++) = (uint8)mainrdata_len;
    }
    rdt_datas_last->next = mainrdata_head;
    rdt_datas_last = mainrdata_last;
    total_len += mainrdata_len;
  }
  rdt_datas_last->next = NULL;

  hdr_rdt.len = (scratch - hdr_scratch);
  total_len += hdr_rdt.len;

     
                               
     
                                                                             
                                                                            
                                                                           
             
     
  INIT_CRC32C(rdata_crc);
  COMP_CRC32C(rdata_crc, hdr_scratch + SizeOfXLogRecord, hdr_rdt.len - SizeOfXLogRecord);
  for (rdt = hdr_rdt.next; rdt != NULL; rdt = rdt->next)
  {
    COMP_CRC32C(rdata_crc, rdt->data, rdt->len);
  }

     
                                                                            
                                                                             
                                        
     
  rechdr->xl_xid = GetCurrentTransactionIdIfAny();
  rechdr->xl_tot_len = total_len;
  rechdr->xl_info = info;
  rechdr->xl_rmid = rmid;
  rechdr->xl_prev = InvalidXLogRecPtr;
  rechdr->xl_crc = rdata_crc;

  return &hdr_rdt;
}

   
                                                        
   
                                                                           
                                                                     
                                         
   
static bool
XLogCompressBackupBlock(char *page, uint16 hole_offset, uint16 hole_length, char *dest, uint16 *dlen)
{
  int32 orig_len = BLCKSZ - hole_length;
  int32 len;
  int32 extra_bytes = 0;
  char *source;
  PGAlignedBlock tmp;

  if (hole_length != 0)
  {
                            
    source = tmp.data;
    memcpy(source, page, hole_offset);
    memcpy(source + hole_offset, page + (hole_offset + hole_length), BLCKSZ - (hole_length + hole_offset));

       
                                                                      
                                                  
       
    extra_bytes = SizeOfXLogRecordBlockCompressHeader;
  }
  else
  {
    source = page;
  }

     
                                                                            
                                                                        
                                                                            
     
  len = pglz_compress(source, orig_len, dest, PGLZ_strategy_default);
  if (len >= 0 && len + extra_bytes < orig_len)
  {
    *dlen = (uint16)len;                             
    return true;
  }
  return false;
}

   
                                                                
   
                                                                               
                                                                              
         
   
bool
XLogCheckBufferNeedsBackup(Buffer buffer)
{
  XLogRecPtr RedoRecPtr;
  bool doPageWrites;
  Page page;

  GetFullPageWriteInfo(&RedoRecPtr, &doPageWrites);

  page = BufferGetPage(buffer);

  if (doPageWrites && PageGetLSN(page) <= RedoRecPtr)
  {
    return true;                             
  }

  return false;                                           
}

   
                                                                        
                                                                   
   
                                                                 
   
                                                                          
                                                                                
                                                                           
                                                                              
                
   
                                                                              
                                                                               
                                              
   
                                                                               
                                                                              
                                                                               
                              
   
XLogRecPtr
XLogSaveBufferForHint(Buffer buffer, bool buffer_std)
{
  XLogRecPtr recptr = InvalidXLogRecPtr;
  XLogRecPtr lsn;
  XLogRecPtr RedoRecPtr;

     
                                                             
     
  Assert(MyPgXact->delayChkpt);

     
                                                              
     
  RedoRecPtr = GetRedoRecPtr();

     
                                                                            
                                                                             
                                                                           
                                   
     
  lsn = BufferGetLSNAtomic(buffer);

  if (lsn <= RedoRecPtr)
  {
    int flags;
    PGAlignedBlock copied_buffer;
    char *origdata = (char *)BufferGetBlock(buffer);
    RelFileNode rnode;
    ForkNumber forkno;
    BlockNumber blkno;

       
                                                                          
                                                                          
                                                         
       
    if (buffer_std)
    {
                                                                 
      Page page = BufferGetPage(buffer);
      uint16 lower = ((PageHeader)page)->pd_lower;
      uint16 upper = ((PageHeader)page)->pd_upper;

      memcpy(copied_buffer.data, origdata, lower);
      memcpy(copied_buffer.data + upper, origdata + upper, BLCKSZ - upper);
    }
    else
    {
      memcpy(copied_buffer.data, origdata, BLCKSZ);
    }

    XLogBeginInsert();

    flags = REGBUF_FORCE_IMAGE;
    if (buffer_std)
    {
      flags |= REGBUF_STANDARD;
    }

    BufferGetTag(buffer, &rnode, &forkno, &blkno);
    XLogRegisterBlock(0, &rnode, forkno, blkno, copied_buffer.data, flags);

    recptr = XLogInsert(RM_XLOG_ID, XLOG_FPI_FOR_HINT);
  }

  return recptr;
}

   
                                                                               
                                                            
   
                                                                                
                                                                            
                               
   
                                                                              
                                                                            
                                                                           
   
XLogRecPtr
log_newpage(RelFileNode *rnode, ForkNumber forkNum, BlockNumber blkno, Page page, bool page_std)
{
  int flags;
  XLogRecPtr recptr;

  flags = REGBUF_FORCE_IMAGE;
  if (page_std)
  {
    flags |= REGBUF_STANDARD;
  }

  XLogBeginInsert();
  XLogRegisterBlock(0, rnode, forkNum, blkno, page, flags);
  recptr = XLogInsert(RM_XLOG_ID, XLOG_FPI);

     
                                                                             
                             
     
  if (!PageIsNew(page))
  {
    PageSetLSN(page, recptr);
  }

  return recptr;
}

   
                                                         
   
                                                                             
                                                   
   
                                                                              
                                                                            
                                                                           
   
XLogRecPtr
log_newpage_buffer(Buffer buffer, bool page_std)
{
  Page page = BufferGetPage(buffer);
  RelFileNode rnode;
  ForkNumber forkNum;
  BlockNumber blkno;

                                                                
  Assert(CritSectionCount > 0);

  BufferGetTag(buffer, &rnode, &forkNum, &blkno);

  return log_newpage(&rnode, forkNum, blkno, page, page_std);
}

   
                                            
   
                                                                          
                                                                           
            
   
                                                                              
                                                                            
                                                                              
   
                                                                              
                                                                  
                                                                             
                                                                         
                                              
   
void
log_newpage_range(Relation rel, ForkNumber forkNum, BlockNumber startblk, BlockNumber endblk, bool page_std)
{
  int flags;
  BlockNumber blkno;

  flags = REGBUF_FORCE_IMAGE;
  if (page_std)
  {
    flags |= REGBUF_STANDARD;
  }

     
                                                                      
                                                                           
                     
     
  XLogEnsureRecordSpace(XLR_MAX_BLOCK_ID - 1, 0);

  blkno = startblk;
  while (blkno < endblk)
  {
    Buffer bufpack[XLR_MAX_BLOCK_ID];
    XLogRecPtr recptr;
    int nbufs;
    int i;

    CHECK_FOR_INTERRUPTS();

                                    
    nbufs = 0;
    while (nbufs < XLR_MAX_BLOCK_ID && blkno < endblk)
    {
      Buffer buf = ReadBufferExtended(rel, forkNum, blkno, RBM_NORMAL, NULL);

      LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

         
                                                                         
                                                                        
                        
         
      if (!PageIsNew(BufferGetPage(buf)))
      {
        bufpack[nbufs++] = buf;
      }
      else
      {
        UnlockReleaseBuffer(buf);
      }
      blkno++;
    }

                                          
    XLogBeginInsert();

    START_CRIT_SECTION();
    for (i = 0; i < nbufs; i++)
    {
      XLogRegisterBuffer(i, bufpack[i], flags);
      MarkBufferDirty(bufpack[i]);
    }

    recptr = XLogInsert(RM_XLOG_ID, XLOG_FPI);

    for (i = 0; i < nbufs; i++)
    {
      PageSetLSN(BufferGetPage(bufpack[i]), recptr);
      UnlockReleaseBuffer(bufpack[i]);
    }
    END_CRIT_SECTION();
  }
}

   
                                                                
   
void
InitXLogInsert(void)
{
                                    
  if (xloginsert_cxt == NULL)
  {
    xloginsert_cxt = AllocSetContextCreate(TopMemoryContext, "WAL record construction", ALLOCSET_DEFAULT_SIZES);
  }

  if (registered_buffers == NULL)
  {
    registered_buffers = (registered_buffer *)MemoryContextAllocZero(xloginsert_cxt, sizeof(registered_buffer) * (XLR_NORMAL_MAX_BLOCK_ID + 1));
    max_registered_buffers = XLR_NORMAL_MAX_BLOCK_ID + 1;
  }
  if (rdatas == NULL)
  {
    rdatas = MemoryContextAlloc(xloginsert_cxt, sizeof(XLogRecData) * XLR_NORMAL_RDATAS);
    max_rdatas = XLR_NORMAL_RDATAS;
  }

     
                                                                        
     
  if (hdr_scratch == NULL)
  {
    hdr_scratch = MemoryContextAllocZero(xloginsert_cxt, HEADER_SCRATCH_SIZE);
  }
}
