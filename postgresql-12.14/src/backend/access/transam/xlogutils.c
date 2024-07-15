                                                                            
   
               
   
                                                       
   
                                                                               
                                                             
   
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "access/timeline.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/rel.h"

   
                                                                          
                                                                           
                                                                         
                                                                          
                                                                          
                                                                          
                                                                          
                    
   
typedef struct xl_invalid_page_key
{
  RelFileNode node;                    
  ForkNumber forkno;                      
  BlockNumber blkno;               
} xl_invalid_page_key;

typedef struct xl_invalid_page
{
  xl_invalid_page_key key;                                 
  bool present;                                                   
} xl_invalid_page;

static HTAB *invalid_page_tab = NULL;

                                           
static void
report_invalid_page(int elevel, RelFileNode node, ForkNumber forkno, BlockNumber blkno, bool present)
{
  char *path = relpathperm(node, forkno);

  if (present)
  {
    elog(elevel, "page %u of relation %s is uninitialized", blkno, path);
  }
  else
  {
    elog(elevel, "page %u of relation %s does not exist", blkno, path);
  }
  pfree(path);
}

                                        
static void
log_invalid_page(RelFileNode node, ForkNumber forkno, BlockNumber blkno, bool present)
{
  xl_invalid_page_key key;
  xl_invalid_page *hentry;
  bool found;

     
                                                                          
                                                                         
                                                                            
                                                                           
                                                                        
                                                             
     
  if (reachedConsistency)
  {
    report_invalid_page(WARNING, node, forkno, blkno, present);
    elog(PANIC, "WAL contains references to invalid pages");
  }

     
                                                                        
                                                                        
                                                                    
     
  if (log_min_messages <= DEBUG1 || client_min_messages <= DEBUG1)
  {
    report_invalid_page(DEBUG1, node, forkno, blkno, present);
  }

  if (invalid_page_tab == NULL)
  {
                                             
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(xl_invalid_page_key);
    ctl.entrysize = sizeof(xl_invalid_page);

    invalid_page_tab = hash_create("XLOG invalid-page table", 100, &ctl, HASH_ELEM | HASH_BLOBS);
  }

                                                                   
  key.node = node;
  key.forkno = forkno;
  key.blkno = blkno;
  hentry = (xl_invalid_page *)hash_search(invalid_page_tab, (void *)&key, HASH_ENTER, &found);

  if (!found)
  {
                                               
    hentry->present = present;
  }
  else
  {
                                                        
  }
}

                                                                        
static void
forget_invalid_pages(RelFileNode node, ForkNumber forkno, BlockNumber minblkno)
{
  HASH_SEQ_STATUS status;
  xl_invalid_page *hentry;

  if (invalid_page_tab == NULL)
  {
    return;                    
  }

  hash_seq_init(&status, invalid_page_tab);

  while ((hentry = (xl_invalid_page *)hash_seq_search(&status)) != NULL)
  {
    if (RelFileNodeEquals(hentry->key.node, node) && hentry->key.forkno == forkno && hentry->key.blkno >= minblkno)
    {
      if (log_min_messages <= DEBUG2 || client_min_messages <= DEBUG2)
      {
        char *path = relpathperm(hentry->key.node, forkno);

        elog(DEBUG2, "page %u of relation %s has been dropped", hentry->key.blkno, path);
        pfree(path);
      }

      if (hash_search(invalid_page_tab, (void *)&hentry->key, HASH_REMOVE, NULL) == NULL)
      {
        elog(ERROR, "hash table corrupted");
      }
    }
  }
}

                                                  
static void
forget_invalid_pages_db(Oid dbid)
{
  HASH_SEQ_STATUS status;
  xl_invalid_page *hentry;

  if (invalid_page_tab == NULL)
  {
    return;                    
  }

  hash_seq_init(&status, invalid_page_tab);

  while ((hentry = (xl_invalid_page *)hash_seq_search(&status)) != NULL)
  {
    if (hentry->key.node.dbNode == dbid)
    {
      if (log_min_messages <= DEBUG2 || client_min_messages <= DEBUG2)
      {
        char *path = relpathperm(hentry->key.node, hentry->key.forkno);

        elog(DEBUG2, "page %u of relation %s has been dropped", hentry->key.blkno, path);
        pfree(path);
      }

      if (hash_search(invalid_page_tab, (void *)&hentry->key, HASH_REMOVE, NULL) == NULL)
      {
        elog(ERROR, "hash table corrupted");
      }
    }
  }
}

                                                           
bool
XLogHaveInvalidPages(void)
{
  if (invalid_page_tab != NULL && hash_get_num_entries(invalid_page_tab) > 0)
  {
    return true;
  }
  return false;
}

                                                       
void
XLogCheckInvalidPages(void)
{
  HASH_SEQ_STATUS status;
  xl_invalid_page *hentry;
  bool foundone = false;

  if (invalid_page_tab == NULL)
  {
    return;                    
  }

  hash_seq_init(&status, invalid_page_tab);

     
                                                                            
                                                           
     
  while ((hentry = (xl_invalid_page *)hash_seq_search(&status)) != NULL)
  {
    report_invalid_page(WARNING, hentry->key.node, hentry->key.forkno, hentry->key.blkno, hentry->present);
    foundone = true;
  }

  if (foundone)
  {
    elog(PANIC, "WAL contains references to invalid pages");
  }

  hash_destroy(invalid_page_tab);
  invalid_page_tab = NULL;
}

   
                         
                                   
   
                                                                          
                                                                           
                                                                  
   
                                                                           
                                                                    
                                                                           
                       
   
                                 
   
                                                                   
                                            
                                                                        
                    
                                                                        
                                               
   
                                                                            
                                                                       
                                                                       
                                                                               
                                                                      
                          
   
                                                                               
                                                                             
                                                                         
                                                                             
                                                                             
                                                                              
                                                                      
                                            
   
XLogRedoAction
XLogReadBufferForRedo(XLogReaderState *record, uint8 block_id, Buffer *buf)
{
  return XLogReadBufferForRedoExtended(record, block_id, RBM_NORMAL, false, buf);
}

   
                                                                        
                       
   
Buffer
XLogInitBufferForRedo(XLogReaderState *record, uint8 block_id)
{
  Buffer buf;

  XLogReadBufferForRedoExtended(record, block_id, RBM_ZERO_AND_LOCK, false, &buf);
  return buf;
}

   
                                 
                                                        
   
                                                                            
                                                                
                                                                           
                             
   
                                                                              
                                                       
   
                                                                             
                                                                      
   
XLogRedoAction
XLogReadBufferForRedoExtended(XLogReaderState *record, uint8 block_id, ReadBufferMode mode, bool get_cleanup_lock, Buffer *buf)
{
  XLogRecPtr lsn = record->EndRecPtr;
  RelFileNode rnode;
  ForkNumber forknum;
  BlockNumber blkno;
  Page page;
  bool zeromode;
  bool willinit;

  if (!XLogRecGetBlockTag(record, block_id, &rnode, &forknum, &blkno))
  {
                                           
    elog(PANIC, "failed to locate backup block with ID %d", block_id);
  }

     
                                                                         
                                             
     
  zeromode = (mode == RBM_ZERO_AND_LOCK || mode == RBM_ZERO_AND_CLEANUP_LOCK);
  willinit = (record->blocks[block_id].flags & BKPBLOCK_WILL_INIT) != 0;
  if (willinit && !zeromode)
  {
    elog(PANIC, "block with WILL_INIT flag in WAL record must be zeroed by redo routine");
  }
  if (!willinit && zeromode)
  {
    elog(PANIC, "block to be initialized in redo routine must be marked with WILL_INIT flag in the WAL record");
  }

                                                                     
  if (XLogRecBlockImageApply(record, block_id))
  {
    Assert(XLogRecHasBlockImage(record, block_id));
    *buf = XLogReadBufferExtended(rnode, forknum, blkno, get_cleanup_lock ? RBM_ZERO_AND_CLEANUP_LOCK : RBM_ZERO_AND_LOCK);
    page = BufferGetPage(*buf);
    if (!RestoreBlockImage(record, block_id, page))
    {
      elog(ERROR, "failed to restore block image");
    }

       
                                                                          
                                    
       
    if (!PageIsNew(page))
    {
      PageSetLSN(page, lsn);
    }

    MarkBufferDirty(*buf);

       
                                                                         
                                                                       
                                                                           
                                
       
    if (forknum == INIT_FORKNUM)
    {
      FlushOneBuffer(*buf);
    }

    return BLK_RESTORED;
  }
  else
  {
    *buf = XLogReadBufferExtended(rnode, forknum, blkno, mode);
    if (BufferIsValid(*buf))
    {
      if (mode != RBM_ZERO_AND_LOCK && mode != RBM_ZERO_AND_CLEANUP_LOCK)
      {
        if (get_cleanup_lock)
        {
          LockBufferForCleanup(*buf);
        }
        else
        {
          LockBuffer(*buf, BUFFER_LOCK_EXCLUSIVE);
        }
      }
      if (lsn <= PageGetLSN(BufferGetPage(*buf)))
      {
        return BLK_DONE;
      }
      else
      {
        return BLK_NEEDS_REDO;
      }
    }
    else
    {
      return BLK_NOTFOUND;
    }
  }
}

   
                          
                                   
   
                                                                       
                                                         
   
                                                                             
                                                                          
                                                                              
                                                                           
                                                       
   
                                                                            
                                                       
   
                                                                          
                                                                         
                                                                
   
                                                                             
                                                                              
                                                                            
                                                                          
             
   
Buffer
XLogReadBufferExtended(RelFileNode rnode, ForkNumber forknum, BlockNumber blkno, ReadBufferMode mode)
{
  BlockNumber lastblock;
  Buffer buffer;
  SMgrRelation smgr;

  Assert(blkno != P_NEW);

                                       
  smgr = smgropen(rnode, InvalidBackendId);

     
                                                                            
                                                                        
                                                                           
                                                                          
                                                                         
                                                     
     
  smgrcreate(smgr, forknum, true);

  lastblock = smgrnblocks(smgr, forknum);

  if (blkno < lastblock)
  {
                             
    buffer = ReadBufferWithoutRelcache(rnode, forknum, blkno, mode, NULL);
  }
  else
  {
                                        
    if (mode == RBM_NORMAL)
    {
      log_invalid_page(rnode, forknum, blkno, false);
      return InvalidBuffer;
    }
    if (mode == RBM_NORMAL_NO_LOG)
    {
      return InvalidBuffer;
    }
                               
                                                                    
    Assert(InRecovery);
    buffer = InvalidBuffer;
    do
    {
      if (buffer != InvalidBuffer)
      {
        if (mode == RBM_ZERO_AND_LOCK || mode == RBM_ZERO_AND_CLEANUP_LOCK)
        {
          LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
        }
        ReleaseBuffer(buffer);
      }
      buffer = ReadBufferWithoutRelcache(rnode, forknum, P_NEW, mode, NULL);
    } while (BufferGetBlockNumber(buffer) < blkno);
                                                                         
    if (BufferGetBlockNumber(buffer) != blkno)
    {
      if (mode == RBM_ZERO_AND_LOCK || mode == RBM_ZERO_AND_CLEANUP_LOCK)
      {
        LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
      }
      ReleaseBuffer(buffer);
      buffer = ReadBufferWithoutRelcache(rnode, forknum, blkno, mode, NULL);
    }
  }

  if (mode == RBM_NORMAL)
  {
                                              
    Page page = (Page)BufferGetPage(buffer);

       
                                                                         
                                                                         
                      
       
    if (PageIsNew(page))
    {
      ReleaseBuffer(buffer);
      log_invalid_page(rnode, forknum, blkno, true);
      return InvalidBuffer;
    }
  }

  return buffer;
}

   
                                                                          
                            
   
typedef struct
{
  RelationData reldata;                               
  FormData_pg_class pgc;
} FakeRelCacheEntryData;

typedef FakeRelCacheEntryData *FakeRelCacheEntry;

   
                                                              
   
                                                                            
                                                                            
                                                                       
                                                                          
                                                                            
                                                                        
   
                                                                     
   
Relation
CreateFakeRelcacheEntry(RelFileNode rnode)
{
  FakeRelCacheEntry fakeentry;
  Relation rel;

  Assert(InRecovery);

                                                                        
  fakeentry = palloc0(sizeof(FakeRelCacheEntryData));
  rel = (Relation)fakeentry;

  rel->rd_rel = &fakeentry->pgc;
  rel->rd_node = rnode;
                                                               
  rel->rd_backend = InvalidBackendId;

                                                          
  rel->rd_rel->relpersistence = RELPERSISTENCE_PERMANENT;

                                                                       
  sprintf(RelationGetRelationName(rel), "%u", rnode.relNode);

     
                                                                      
                                                                    
                                                                            
                                                                          
                   
     
  rel->rd_lockInfo.lockRelId.dbId = rnode.dbNode;
  rel->rd_lockInfo.lockRelId.relId = rnode.relNode;

  rel->rd_smgr = NULL;

  return rel;
}

   
                                     
   
void
FreeFakeRelcacheEntry(Relation fakerel)
{
                                                                           
  if (fakerel->rd_smgr != NULL)
  {
    smgrclearowner(&fakerel->rd_smgr, fakerel->rd_smgr);
  }
  pfree(fakerel);
}

   
                                      
   
                                                                              
                                                     
   
void
XLogDropRelation(RelFileNode rnode, ForkNumber forknum)
{
  forget_invalid_pages(rnode, forknum, 0);
}

   
                                            
   
                                                                    
   
void
XLogDropDatabase(Oid dbid)
{
     
                                                                       
                                                                             
                                                                          
                                                                        
     
  smgrcloseall();

  forget_invalid_pages_db(dbid);
}

   
                                          
   
                                                                              
   
void
XLogTruncateRelation(RelFileNode rnode, ForkNumber forkNum, BlockNumber nblocks)
{
  forget_invalid_pages(rnode, forkNum, nblocks);
}

   
                                                                           
                      
   
                                                                       
                                                                          
                                                                        
                  
   
                                                                             
                                                                       
                                                              
   
static void
XLogRead(char *buf, int segsize, TimeLineID tli, XLogRecPtr startptr, Size count)
{
  char *p;
  XLogRecPtr recptr;
  Size nbytes;

                                     
  static int sendFile = -1;
  static XLogSegNo sendSegNo = 0;
  static TimeLineID sendTLI = 0;
  static uint32 sendOff = 0;

  Assert(segsize == wal_segment_size);

  p = buf;
  recptr = startptr;
  nbytes = count;

  while (nbytes > 0)
  {
    uint32 startoff;
    int segbytes;
    int readbytes;

    startoff = XLogSegmentOffset(recptr, segsize);

                                                           
    if (sendFile < 0 || !XLByteInSeg(recptr, sendSegNo, segsize) || sendTLI != tli)
    {
      char path[MAXPGPATH];

      if (sendFile >= 0)
      {
        close(sendFile);
      }

      XLByteToSeg(recptr, sendSegNo, segsize);

      XLogFilePath(path, tli, sendSegNo, segsize);

      sendFile = BasicOpenFile(path, O_RDONLY | PG_BINARY);

      if (sendFile < 0)
      {
        if (errno == ENOENT)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("requested WAL segment %s has already been removed", path)));
        }
        else
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
        }
      }
      sendOff = 0;
      sendTLI = tli;
    }

                                   
    if (sendOff != startoff)
    {
      if (lseek(sendFile, (off_t)startoff, SEEK_SET) < 0)
      {
        char path[MAXPGPATH];
        int save_errno = errno;

        XLogFilePath(path, tli, sendSegNo, segsize);
        errno = save_errno;
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in log segment %s to offset %u: %m", path, startoff)));
      }
      sendOff = startoff;
    }

                                                 
    if (nbytes > (segsize - startoff))
    {
      segbytes = segsize - startoff;
    }
    else
    {
      segbytes = nbytes;
    }

    pgstat_report_wait_start(WAIT_EVENT_WAL_READ);
    readbytes = read(sendFile, p, segbytes);
    pgstat_report_wait_end();
    if (readbytes <= 0)
    {
      char path[MAXPGPATH];
      int save_errno = errno;

      XLogFilePath(path, tli, sendSegNo, segsize);
      errno = save_errno;
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from log segment %s, offset %u, length %lu: %m", path, sendOff, (unsigned long)segbytes)));
    }

                               
    recptr += readbytes;

    sendOff += readbytes;
    nbytes -= readbytes;
    p += readbytes;
  }
}

   
                                                                  
                                                  
   
                                                                       
                                                                          
                                                                             
                                
   
                                                                     
                                                                 
                                                                        
   
                                                                        
                                                                          
                                                                             
                                                                               
                                                                           
                                                                            
                                                        
   
                                                                               
                                                                             
                                                                               
                                                                                
                                                                         
   
                                                                             
                                                                              
                                                                           
                             
   
                                                                          
                                                                         
                                                                           
                                                             
                                                             
   
void
XLogReadDetermineTimeline(XLogReaderState *state, XLogRecPtr wantPage, uint32 wantLength)
{
  const XLogRecPtr lastReadPage = state->readSegNo * state->wal_segment_size + state->readOff;

  Assert(wantPage != InvalidXLogRecPtr && wantPage % XLOG_BLCKSZ == 0);
  Assert(wantLength <= XLOG_BLCKSZ);
  Assert(state->readLen == 0 || state->readLen <= XLOG_BLCKSZ);

     
                                                                            
         
     
                                                                            
                                                                        
                                              
     
  if (lastReadPage == wantPage && state->readLen != 0 && lastReadPage + state->readLen >= wantPage + Min(wantLength, XLOG_BLCKSZ - 1))
  {
    return;
  }

     
                                                                             
                                                                          
                                                                         
                                            
     
                                                                             
                                                                            
                                                                      
                                                        
     
  if (state->currTLI == ThisTimeLineID && wantPage >= lastReadPage)
  {
    Assert(state->currTLIValidUntil == InvalidXLogRecPtr);
    return;
  }

     
                                                                        
                                                                            
                                                   
     
  if (state->currTLIValidUntil != InvalidXLogRecPtr && state->currTLI != ThisTimeLineID && state->currTLI != 0 && ((wantPage + wantLength) / state->wal_segment_size) < (state->currTLIValidUntil / state->wal_segment_size))
  {
    return;
  }

     
                                                                      
                                                                           
                                                                           
                                                      
     
                                                                            
                                                                             
                                                                             
                                                                           
                                   
     
  {
       
                                                                         
                                                         
       
    List *timelineHistory = readTimeLineHistory(ThisTimeLineID);

    XLogRecPtr endOfSegment = (((wantPage / state->wal_segment_size) + 1) * state->wal_segment_size) - 1;

    Assert(wantPage / state->wal_segment_size == endOfSegment / state->wal_segment_size);

       
                                                                   
                 
       
    state->currTLI = tliOfPointInHistory(endOfSegment, timelineHistory);
    state->currTLIValidUntil = tliSwitchPoint(state->currTLI, timelineHistory, &state->nextTLI);

    Assert(state->currTLIValidUntil == InvalidXLogRecPtr || wantPage + wantLength < state->currTLIValidUntil);

    list_free_deep(timelineHistory);

    elog(DEBUG3, "switched to timeline %u valid until %X/%X", state->currTLI, (uint32)(state->currTLIValidUntil >> 32), (uint32)(state->currTLIValidUntil));
  }
}

   
                                                   
   
                                                                              
                                                        
   
                                                                         
                                                                               
                                                                              
                 
   
int
read_local_xlog_page(XLogReaderState *state, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *cur_page, TimeLineID *pageTLI)
{
  XLogRecPtr read_upto, loc;
  int count;

  loc = targetPagePtr + reqLen;

                                                          
  while (1)
  {
       
                                                                          
                                
       
                                                                     
                                                                         
                                          
       
    if (!RecoveryInProgress())
    {
      read_upto = GetFlushRecPtr();
    }
    else
    {
      read_upto = GetXLogReplayRecPtr(&ThisTimeLineID);
    }

    *pageTLI = ThisTimeLineID;

       
                                                    
       
                                                                       
                                                                      
                                                                           
                                    
       
                   
       
                                                                          
                                                         
       
                                                                        
                                                               
                      
       
                                                                          
                                                                           
                                                                      
                                                                           
                                                                     
                                    
       
    XLogReadDetermineTimeline(state, targetPagePtr, reqLen);

    if (state->currTLI == ThisTimeLineID)
    {

      if (loc <= read_upto)
      {
        break;
      }

      CHECK_FOR_INTERRUPTS();
      pg_usleep(1000L);
    }
    else
    {
         
                                                                        
                                                    
         
                                                                         
                                                                        
             
         
      read_upto = state->currTLIValidUntil;

         
                                                                       
                                                                    
                                                                       
                                                                     
                                                                         
                                                             
         
      *pageTLI = state->currTLI;

                                                    
      break;
    }
  }

  if (targetPagePtr + XLOG_BLCKSZ <= read_upto)
  {
       
                                                                        
                                    
       
    count = XLOG_BLCKSZ;
  }
  else if (targetPagePtr + reqLen > read_upto)
  {
                               
    return -1;
  }
  else
  {
                                                       
    count = read_upto - targetPagePtr;
  }

     
                                                                             
                                                                   
                                                             
     
  XLogRead(cur_page, state->wal_segment_size, *pageTLI, targetPagePtr, XLOG_BLCKSZ);

                                           
  return count;
}
