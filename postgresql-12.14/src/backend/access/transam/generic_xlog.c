                                                                            
   
                  
                                            
   
   
                                                                         
                                                                        
   
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/bufmask.h"
#include "access/generic_xlog.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "utils/memutils.h"

                                                                            
                                                                           
                                                                             
                          
   
                                          
                                          
                                                                         
   
                                                                               
                                                                        
                                                                            
                                                                           
                                                                             
                                                    
   
                                                                            
                                                                              
                                                                          
                                                                     
                                                                           
                                
                                                                            
   
#define FRAGMENT_HEADER_SIZE (2 * sizeof(OffsetNumber))
#define MATCH_THRESHOLD FRAGMENT_HEADER_SIZE
#define MAX_DELTA_SIZE (BLCKSZ + 2 * FRAGMENT_HEADER_SIZE)

                                                 
typedef struct
{
  Buffer buffer;                                     
  int flags;                                             
  int deltaLen;                                                  
  char *image;                                                               
                                                                               
  char delta[MAX_DELTA_SIZE];                                
} PageData;

                                               
struct GenericXLogState
{
                                       
  PageData pages[MAX_GENERIC_XLOG_PAGES];
  bool isLogged;
                                      
  PGAlignedBlock images[MAX_GENERIC_XLOG_PAGES];
};

static void
writeFragment(PageData *pageData, OffsetNumber offset, OffsetNumber len, const char *data);
static void
computeRegionDelta(PageData *pageData, const char *curpage, const char *targetpage, int targetStart, int targetEnd, int validStart, int validEnd);
static void
computeDelta(PageData *pageData, Page curpage, Page targetpage);
static void
applyPageRedo(Page page, const char *delta, Size deltaSize);

   
                                              
   
                                                                        
                                   
   
static void
writeFragment(PageData *pageData, OffsetNumber offset, OffsetNumber length, const char *data)
{
  char *ptr = pageData->delta + pageData->deltaLen;

                                   
  Assert(pageData->deltaLen + sizeof(offset) + sizeof(length) + length <= sizeof(pageData->delta));

                           
  memcpy(ptr, &offset, sizeof(offset));
  ptr += sizeof(offset);
  memcpy(ptr, &length, sizeof(length));
  ptr += sizeof(length);
  memcpy(ptr, data, length);
  ptr += length;

  pageData->deltaLen = ptr - pageData->delta;
}

   
                                                                               
                                                                           
                                                                         
                                                                         
                                                                
   
                                                                         
                                  
   
static void
computeRegionDelta(PageData *pageData, const char *curpage, const char *targetpage, int targetStart, int targetEnd, int validStart, int validEnd)
{
  int i, loopEnd, fragmentBegin = -1, fragmentEnd = -1;

                                                                            
  if (validStart > targetStart)
  {
    fragmentBegin = targetStart;
    targetStart = validStart;
  }

                                                                  
  loopEnd = Min(targetEnd, validEnd);

                                                   
  i = targetStart;
  while (i < loopEnd)
  {
    if (curpage[i] != targetpage[i])
    {
                                                                       
      if (fragmentBegin < 0)
      {
        fragmentBegin = i;
      }
                                                     
      fragmentEnd = -1;
                                                                  
      i++;
      while (i < loopEnd && curpage[i] != targetpage[i])
      {
        i++;
      }
      if (i >= loopEnd)
      {
        break;
      }
    }

                                                                     
    fragmentEnd = i;

       
                                                                         
                                                                           
       
    i++;
    while (i < loopEnd && curpage[i] == targetpage[i])
    {
      i++;
    }

       
                                                       
       
                                                                      
                                                                    
       
                                                                         
                                                                 
       
                                                                         
                                                                         
                                                                         
                                                                         
       
                                                                           
                                                                        
                                                                       
                                                               
       
                                                                           
                                                                       
                                       
       
    if (fragmentBegin >= 0 && i - fragmentEnd > MATCH_THRESHOLD)
    {
      writeFragment(pageData, fragmentBegin, fragmentEnd - fragmentBegin, targetpage + fragmentBegin);
      fragmentBegin = -1;
      fragmentEnd = -1;                           
    }
  }

                                                                          
  if (loopEnd < targetEnd)
  {
    if (fragmentBegin < 0)
    {
      fragmentBegin = loopEnd;
    }
    fragmentEnd = targetEnd;
  }

                                   
  if (fragmentBegin >= 0)
  {
    if (fragmentEnd < 0)
    {
      fragmentEnd = targetEnd;
    }
    writeFragment(pageData, fragmentBegin, fragmentEnd - fragmentBegin, targetpage + fragmentBegin);
  }
}

   
                                                                              
                                           
   
static void
computeDelta(PageData *pageData, Page curpage, Page targetpage)
{
  int targetLower = ((PageHeader)targetpage)->pd_lower, targetUpper = ((PageHeader)targetpage)->pd_upper, curLower = ((PageHeader)curpage)->pd_lower, curUpper = ((PageHeader)curpage)->pd_upper;

  pageData->deltaLen = 0;

                                                        
  computeRegionDelta(pageData, curpage, targetpage, 0, targetLower, 0, curLower);
                                                       
  computeRegionDelta(pageData, curpage, targetpage, targetUpper, BLCKSZ, curUpper, BLCKSZ);

     
                                                                           
                                                                
     
#ifdef WAL_DEBUG
  if (XLOG_DEBUG)
  {
    PGAlignedBlock tmp;

    memcpy(tmp.data, curpage, BLCKSZ);
    applyPageRedo(tmp.data, pageData->delta, pageData->deltaLen);
    if (memcmp(tmp.data, targetpage, targetLower) != 0 || memcmp(tmp.data + targetUpper, targetpage + targetUpper, BLCKSZ - targetUpper) != 0)
    {
      elog(ERROR, "result of generic xlog apply does not match");
    }
  }
#endif
}

   
                                                                          
   
GenericXLogState *
GenericXLogStart(Relation relation)
{
  GenericXLogState *state;
  int i;

  state = (GenericXLogState *)palloc(sizeof(GenericXLogState));
  state->isLogged = RelationNeedsWAL(relation);

  for (i = 0; i < MAX_GENERIC_XLOG_PAGES; i++)
  {
    state->pages[i].image = state->images[i].data;
    state->pages[i].buffer = InvalidBuffer;
  }

  return state;
}

   
                                                
   
                                                                      
                                     
   
                                                                        
                                                                      
                                             
   
Page
GenericXLogRegisterBuffer(GenericXLogState *state, Buffer buffer, int flags)
{
  int block_id;

                                                            
  for (block_id = 0; block_id < MAX_GENERIC_XLOG_PAGES; block_id++)
  {
    PageData *page = &state->pages[block_id];

    if (BufferIsInvalid(page->buffer))
    {
                                                                 
      page->buffer = buffer;
      page->flags = flags;
      memcpy(page->image, BufferGetPage(buffer), BLCKSZ);
      return (Page)page->image;
    }
    else if (page->buffer == buffer)
    {
         
                                                                        
                           
         
      return (Page)page->image;
    }
  }

  elog(ERROR, "maximum number %d of generic xlog buffers is exceeded", MAX_GENERIC_XLOG_PAGES);
                           
  return NULL;
}

   
                                                                        
                                   
   
XLogRecPtr
GenericXLogFinish(GenericXLogState *state)
{
  XLogRecPtr lsn;
  int i;

  if (state->isLogged)
  {
                                                                
    XLogBeginInsert();

    START_CRIT_SECTION();

    for (i = 0; i < MAX_GENERIC_XLOG_PAGES; i++)
    {
      PageData *pageData = &state->pages[i];
      Page page;
      PageHeader pageHeader;

      if (BufferIsInvalid(pageData->buffer))
      {
        continue;
      }

      page = BufferGetPage(pageData->buffer);
      pageHeader = (PageHeader)pageData->image;

      if (pageData->flags & GENERIC_XLOG_FULL_IMAGE)
      {
           
                                                                    
                                                                  
                                                                  
                                                                      
                    
           
        memcpy(page, pageData->image, pageHeader->pd_lower);
        memset(page + pageHeader->pd_lower, 0, pageHeader->pd_upper - pageHeader->pd_lower);
        memcpy(page + pageHeader->pd_upper, pageData->image + pageHeader->pd_upper, BLCKSZ - pageHeader->pd_upper);

        XLogRegisterBuffer(i, pageData->buffer, REGBUF_FORCE_IMAGE | REGBUF_STANDARD);
      }
      else
      {
           
                                                                     
                                      
           
        computeDelta(pageData, page, (Page)pageData->image);

                                                          
        memcpy(page, pageData->image, pageHeader->pd_lower);
        memset(page + pageHeader->pd_lower, 0, pageHeader->pd_upper - pageHeader->pd_lower);
        memcpy(page + pageHeader->pd_upper, pageData->image + pageHeader->pd_upper, BLCKSZ - pageHeader->pd_upper);

        XLogRegisterBuffer(i, pageData->buffer, REGBUF_STANDARD);
        XLogRegisterBufData(i, pageData->delta, pageData->deltaLen);
      }
    }

                            
    lsn = XLogInsert(RM_GENERIC_ID, 0);

                                        
    for (i = 0; i < MAX_GENERIC_XLOG_PAGES; i++)
    {
      PageData *pageData = &state->pages[i];

      if (BufferIsInvalid(pageData->buffer))
      {
        continue;
      }
      PageSetLSN(BufferGetPage(pageData->buffer), lsn);
      MarkBufferDirty(pageData->buffer);
    }
    END_CRIT_SECTION();
  }
  else
  {
                                                    
    START_CRIT_SECTION();
    for (i = 0; i < MAX_GENERIC_XLOG_PAGES; i++)
    {
      PageData *pageData = &state->pages[i];

      if (BufferIsInvalid(pageData->buffer))
      {
        continue;
      }
      memcpy(BufferGetPage(pageData->buffer), pageData->image, BLCKSZ);
                                                                
      MarkBufferDirty(pageData->buffer);
    }
    END_CRIT_SECTION();
                                                     
    lsn = InvalidXLogRecPtr;
  }

  pfree(state);

  return lsn;
}

   
                                                                               
   
                                                                               
   
void
GenericXLogAbort(GenericXLogState *state)
{
  pfree(state);
}

   
                                    
   
static void
applyPageRedo(Page page, const char *delta, Size deltaSize)
{
  const char *ptr = delta;
  const char *end = delta + deltaSize;

  while (ptr < end)
  {
    OffsetNumber offset, length;

    memcpy(&offset, ptr, sizeof(offset));
    ptr += sizeof(offset);
    memcpy(&length, ptr, sizeof(length));
    ptr += sizeof(length);

    memcpy(page + offset, ptr, length);

    ptr += length;
  }
}

   
                                          
   
void
generic_redo(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  Buffer buffers[MAX_GENERIC_XLOG_PAGES];
  uint8 block_id;

                                               
  Assert(record->max_block_id < MAX_GENERIC_XLOG_PAGES);

                           
  for (block_id = 0; block_id <= record->max_block_id; block_id++)
  {
    XLogRedoAction action;

    if (!XLogRecHasBlockRef(record, block_id))
    {
      buffers[block_id] = InvalidBuffer;
      continue;
    }

    action = XLogReadBufferForRedo(record, block_id, &buffers[block_id]);

                                             
    if (action == BLK_NEEDS_REDO)
    {
      Page page;
      PageHeader pageHeader;
      char *blockDelta;
      Size blockDeltaSize;

      page = BufferGetPage(buffers[block_id]);
      blockDelta = XLogRecGetBlockData(record, block_id, &blockDeltaSize);
      applyPageRedo(page, blockDelta, blockDeltaSize);

         
                                                                     
                                                                   
                                                                       
                                                 
         
      pageHeader = (PageHeader)page;
      memset(page + pageHeader->pd_lower, 0, pageHeader->pd_upper - pageHeader->pd_lower);

      PageSetLSN(page, lsn);
      MarkBufferDirty(buffers[block_id]);
    }
  }

                                                        
  for (block_id = 0; block_id <= record->max_block_id; block_id++)
  {
    if (BufferIsValid(buffers[block_id]))
    {
      UnlockReleaseBuffer(buffers[block_id]);
    }
  }
}

   
                                                                   
   
void
generic_mask(char *page, BlockNumber blkno)
{
  mask_page_lsn_and_checksum(page);

  mask_unused_space(page);
}
