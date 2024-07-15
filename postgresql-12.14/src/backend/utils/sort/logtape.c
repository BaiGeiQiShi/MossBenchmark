                                                                            
   
             
                                                           
   
                                                                        
                                                                         
                                                                        
                                                                        
                                                                          
                                                                     
                                                                     
                                                                          
   
                                                                    
                                                                        
                                                                       
                                                                        
                                                                           
                                                                          
                                                      
   
                                                                           
                                                                          
                                                                           
                                                                         
                                                                        
                                                   
   
                                                                        
                                                                        
                                                                        
                                                                
                                                                        
                          
   
                                                                    
                                                                             
                                                                       
                                                                         
                                                                      
                                                                      
                                                                          
                                                                         
                                                                      
                                            
   
                                                                        
                                                                        
                                      
   
                                                                    
                                                                        
                                                                        
                                                                           
                                                                     
                           
   
                                                                           
                                                                             
                                                                          
                                                                             
                                                                        
                   
   
                                                                        
                                                                    
                                                                           
                                                                  
                                                                        
                                                                             
                                                                   
   
                                                                         
                                                                        
   
                  
                                      
   
                                                                            
   

#include "postgres.h"

#include "storage/buffile.h"
#include "utils/builtins.h"
#include "utils/logtape.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

   
                                                                 
   
                                                                       
                                                                      
                                                
   
typedef struct TapeBlockTrailer
{
  long prev;                                                
                        
  long next;                                           
                                               
} TapeBlockTrailer;

#define TapeBlockPayloadSize (BLCKSZ - sizeof(TapeBlockTrailer))
#define TapeBlockGetTrailer(buf) ((TapeBlockTrailer *)((char *)buf + TapeBlockPayloadSize))

#define TapeBlockIsLast(buf) (TapeBlockGetTrailer(buf)->next < 0)
#define TapeBlockGetNBytes(buf) (TapeBlockIsLast(buf) ? (-TapeBlockGetTrailer(buf)->next) : TapeBlockPayloadSize)
#define TapeBlockSetNBytes(buf, nbytes) (TapeBlockGetTrailer(buf)->next = -(nbytes))

   
                                                                         
                                             
   
                                                                          
                                                                            
                                                                        
                                                                             
                  
   
typedef struct LogicalTape
{
  bool writing;                             
  bool frozen;                                                 
  bool dirty;                                        

     
                                                                      
     
                                                                            
                                                                          
                                                                       
                 
     
                                                                           
                                                                           
     
  long firstBlockNumber;
  long curBlockNumber;
  long nextBlockNumber;
  long offsetBlockNumber;

     
                                       
     
  char *buffer;                                               
  int buffer_size;                                   
  int max_size;                                          
  int pos;                                                 
  int nbytes;                                            
} LogicalTape;

   
                                                                           
                                                                              
                                                                               
                                             
   
struct LogicalTapeSet
{
  BufFile *pfile;                                         

     
                                                                             
                                                                           
                                                                     
                                                                             
                                                                           
                                                                         
                                                                             
                    
     
  long nBlocksAllocated;                            
  long nBlocksWritten;                                            
  long nHoleBlocks;                                   

     
                                                                            
                                                                   
     
                                                                           
                                                                  
                                      
     
                                                                         
                                                                             
                                                                           
                                                                      
     
  bool forgetFreeSpace;                                      
  bool blocksSorted;                                             
  long *freeBlocks;                          
  int nFreeBlocks;                                      
  int freeBlocksLen;                                                  

                                   
  int nTapes;                                                              
  LogicalTape tapes[FLEXIBLE_ARRAY_MEMBER];                          
};

static void
ltsWriteBlock(LogicalTapeSet *lts, long blocknum, void *buffer);
static void
ltsReadBlock(LogicalTapeSet *lts, long blocknum, void *buffer);
static long
ltsGetFreeBlock(LogicalTapeSet *lts);
static void
ltsReleaseBlock(LogicalTapeSet *lts, long blocknum);
static void
ltsConcatWorkerTapes(LogicalTapeSet *lts, TapeShare *shared, SharedFileSet *fileset);

   
                                                                             
   
                                                                      
   
static void
ltsWriteBlock(LogicalTapeSet *lts, long blocknum, void *buffer)
{
     
                                                                          
                                                                             
                                                  
     
                                                                   
                                                                         
                                                                         
                                                                           
                                                                        
               
     
                                                                          
                                                                          
                                                                           
                              
     
  while (blocknum > lts->nBlocksWritten)
  {
    PGAlignedBlock zerobuf;

    MemSet(zerobuf.data, 0, sizeof(zerobuf));

    ltsWriteBlock(lts, lts->nBlocksWritten, zerobuf.data);
  }

                                 
  if (BufFileSeekBlock(lts->pfile, blocknum) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek to block %ld of temporary file", blocknum)));
  }
  BufFileWrite(lts->pfile, buffer, BLCKSZ);

                                                      
  if (blocknum == lts->nBlocksWritten)
  {
    lts->nBlocksWritten++;
  }
}

   
                                                                              
   
                                                                             
                                                                         
   
static void
ltsReadBlock(LogicalTapeSet *lts, long blocknum, void *buffer)
{
  size_t nread;

  if (BufFileSeekBlock(lts->pfile, blocknum) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek to block %ld of temporary file", blocknum)));
  }
  nread = BufFileRead(lts->pfile, buffer, BLCKSZ);
  if (nread != BLCKSZ)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read block %ld of temporary file: read only %zu of %zu bytes", blocknum, nread, (size_t)BLCKSZ)));
  }
}

   
                                                           
   
                                                      
   
static bool
ltsReadFillBuffer(LogicalTapeSet *lts, LogicalTape *lt)
{
  lt->pos = 0;
  lt->nbytes = 0;

  do
  {
    char *thisbuf = lt->buffer + lt->nbytes;
    long datablocknum = lt->nextBlockNumber;

                                 
    if (datablocknum == -1L)
    {
      break;          
    }
                                                         
    datablocknum += lt->offsetBlockNumber;

                        
    ltsReadBlock(lts, datablocknum, (void *)thisbuf);
    if (!lt->frozen)
    {
      ltsReleaseBlock(lts, datablocknum);
    }
    lt->curBlockNumber = lt->nextBlockNumber;

    lt->nbytes += TapeBlockGetNBytes(thisbuf);
    if (TapeBlockIsLast(thisbuf))
    {
      lt->nextBlockNumber = -1L;
               
      break;
    }
    else
    {
      lt->nextBlockNumber = TapeBlockGetTrailer(thisbuf)->next;
    }

                                                             
  } while (lt->buffer_size - lt->nbytes > BLCKSZ);

  return (lt->nbytes > 0);
}

   
                                                                    
   
static int
freeBlocks_cmp(const void *a, const void *b)
{
  long ablk = *((const long *)a);
  long bblk = *((const long *)b);

                                                                
  if (ablk < bblk)
  {
    return 1;
  }
  if (ablk > bblk)
  {
    return -1;
  }
  return 0;
}

   
                                                   
   
static long
ltsGetFreeBlock(LogicalTapeSet *lts)
{
     
                                                                            
                                                                           
                                                   
     
  if (lts->nFreeBlocks > 0)
  {
    if (!lts->blocksSorted)
    {
      qsort((void *)lts->freeBlocks, lts->nFreeBlocks, sizeof(long), freeBlocks_cmp);
      lts->blocksSorted = true;
    }
    return lts->freeBlocks[--lts->nFreeBlocks];
  }
  else
  {
    return lts->nBlocksAllocated++;
  }
}

   
                                    
   
static void
ltsReleaseBlock(LogicalTapeSet *lts, long blocknum)
{
  int ndx;

     
                                                                         
     
  if (lts->forgetFreeSpace)
  {
    return;
  }

     
                                       
     
  if (lts->nFreeBlocks >= lts->freeBlocksLen)
  {
    lts->freeBlocksLen *= 2;
    lts->freeBlocks = (long *)repalloc(lts->freeBlocks, lts->freeBlocksLen * sizeof(long));
  }

     
                                                                             
                       
     
  ndx = lts->nFreeBlocks++;
  lts->freeBlocks[ndx] = blocknum;
  if (ndx > 0 && lts->freeBlocks[ndx - 1] < blocknum)
  {
    lts->blocksSorted = false;
  }
}

   
                                                                            
   
                                                                          
                                                                                
                                                                               
                       
   
static void
ltsConcatWorkerTapes(LogicalTapeSet *lts, TapeShare *shared, SharedFileSet *fileset)
{
  LogicalTape *lt = NULL;
  long tapeblocks = 0L;
  long nphysicalblocks = 0L;
  int i;

                                                                
  Assert(lts->nTapes >= 2);

     
                                                                           
                                                                           
           
     
  for (i = 0; i < lts->nTapes - 1; i++)
  {
    char filename[MAXPGPATH];
    BufFile *file;
    int64 filesize;

    lt = &lts->tapes[i];

    pg_itoa(i, filename);
    file = BufFileOpenShared(fileset, filename);
    filesize = BufFileSize(file);

       
                                                                         
                                                   
       
    lt->firstBlockNumber = shared[i].firstblocknumber;
    if (i == 0)
    {
      lts->pfile = file;
      lt->offsetBlockNumber = 0L;
    }
    else
    {
      lt->offsetBlockNumber = BufFileAppend(lts->pfile, file);
    }
                                                                      
    lt->max_size = Min(MaxAllocSize, filesize);
    tapeblocks = filesize / BLCKSZ;
    nphysicalblocks += tapeblocks;
  }

     
                                                                            
                                                                             
                                                                      
                                    
     
  lts->nBlocksAllocated = lt->offsetBlockNumber + tapeblocks;
  lts->nBlocksWritten = lts->nBlocksAllocated;

     
                                                                            
                                                                         
                                                                             
                                            
     
                                                                             
                                                                            
                                                                           
                                                                            
                                                                            
              
     
                                                                            
                                                                            
                                                                     
                                                                             
                                                                           
                 
     
  lts->nHoleBlocks = lts->nBlocksAllocated - nphysicalblocks;
}

   
                                                                 
   
                                                                         
                                                                         
                                                                           
                                                                         
                                                                           
                       
   
                                                                           
                                                                             
                                                                      
                                                                            
                                                                        
                                                                          
                                                                            
                                                                  
                                                    
   
LogicalTapeSet *
LogicalTapeSetCreate(int ntapes, TapeShare *shared, SharedFileSet *fileset, int worker)
{
  LogicalTapeSet *lts;
  LogicalTape *lt;
  int i;

     
                                                                     
     
  Assert(ntapes > 0);
  lts = (LogicalTapeSet *)palloc(offsetof(LogicalTapeSet, tapes) + ntapes * sizeof(LogicalTape));
  lts->nBlocksAllocated = 0L;
  lts->nBlocksWritten = 0L;
  lts->nHoleBlocks = 0L;
  lts->forgetFreeSpace = false;
  lts->blocksSorted = true;                                        
  lts->freeBlocksLen = 32;                                
  lts->freeBlocks = (long *)palloc(lts->freeBlocksLen * sizeof(long));
  lts->nFreeBlocks = 0;
  lts->nTapes = ntapes;

     
                                                                           
                                                                             
                                                                           
                      
     
  for (i = 0; i < ntapes; i++)
  {
    lt = &lts->tapes[i];
    lt->writing = true;
    lt->frozen = false;
    lt->dirty = false;
    lt->firstBlockNumber = -1L;
    lt->curBlockNumber = -1L;
    lt->nextBlockNumber = -1L;
    lt->offsetBlockNumber = 0L;
    lt->buffer = NULL;
    lt->buffer_size = 0;
                                                      
    lt->max_size = MaxAllocSize;
    lt->pos = 0;
    lt->nbytes = 0;
  }

     
                                              
     
                                                                            
                                                                         
                                                                            
                                                                             
     
  if (shared)
  {
    ltsConcatWorkerTapes(lts, shared, fileset);
  }
  else if (fileset)
  {
    char filename[MAXPGPATH];

    pg_itoa(worker, filename);
    lts->pfile = BufFileCreateShared(fileset, filename);
  }
  else
  {
    lts->pfile = BufFileCreateTemp(false);
  }

  return lts;
}

   
                                                       
   
void
LogicalTapeSetClose(LogicalTapeSet *lts)
{
  LogicalTape *lt;
  int i;

  BufFileClose(lts->pfile);
  for (i = 0; i < lts->nTapes; i++)
  {
    lt = &lts->tapes[i];
    if (lt->buffer)
    {
      pfree(lt->buffer);
    }
  }
  pfree(lts->freeBlocks);
  pfree(lts);
}

   
                                                                            
   
                                                                              
                                                                          
                                                                             
                                                                              
                                                           
   
void
LogicalTapeSetForgetFreeSpace(LogicalTapeSet *lts)
{
  lts->forgetFreeSpace = true;
}

   
                            
   
                                                        
   
void
LogicalTapeWrite(LogicalTapeSet *lts, int tapenum, void *ptr, size_t size)
{
  LogicalTape *lt;
  size_t nthistime;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];
  Assert(lt->writing);
  Assert(lt->offsetBlockNumber == 0L);

                                                           
  if (lt->buffer == NULL)
  {
    lt->buffer = (char *)palloc(BLCKSZ);
    lt->buffer_size = BLCKSZ;
  }
  if (lt->curBlockNumber == -1)
  {
    Assert(lt->firstBlockNumber == -1);
    Assert(lt->pos == 0);

    lt->curBlockNumber = ltsGetFreeBlock(lts);
    lt->firstBlockNumber = lt->curBlockNumber;

    TapeBlockGetTrailer(lt->buffer)->prev = -1L;
  }

  Assert(lt->buffer_size == BLCKSZ);
  while (size > 0)
  {
    if (lt->pos >= TapeBlockPayloadSize)
    {
                                    
      long nextBlockNumber;

      if (!lt->dirty)
      {
                                                         
        elog(ERROR, "invalid logtape state: should be dirty");
      }

         
                                                                       
                                       
         
      nextBlockNumber = ltsGetFreeBlock(lts);

                                                            
      TapeBlockGetTrailer(lt->buffer)->next = nextBlockNumber;
      ltsWriteBlock(lts, lt->curBlockNumber, (void *)lt->buffer);

                                                         
      TapeBlockGetTrailer(lt->buffer)->prev = lt->curBlockNumber;
      lt->curBlockNumber = nextBlockNumber;
      lt->pos = 0;
      lt->nbytes = 0;
    }

    nthistime = TapeBlockPayloadSize - lt->pos;
    if (nthistime > size)
    {
      nthistime = size;
    }
    Assert(nthistime > 0);

    memcpy(lt->buffer + lt->pos, ptr, nthistime);

    lt->dirty = true;
    lt->pos += nthistime;
    if (lt->nbytes < lt->pos)
    {
      lt->nbytes = lt->pos;
    }
    ptr = (void *)((char *)ptr + nthistime);
    size -= nthistime;
  }
}

   
                                                           
   
                                                                           
   
                                                                       
                                                                           
                                                                             
                                                                              
                                                                             
                        
   
void
LogicalTapeRewindForRead(LogicalTapeSet *lts, int tapenum, size_t buffer_size)
{
  LogicalTape *lt;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];

     
                                          
     
  if (lt->frozen)
  {
    buffer_size = BLCKSZ;
  }
  else
  {
                                 
    if (buffer_size < BLCKSZ)
    {
      buffer_size = BLCKSZ;
    }

                                                                 
    if (buffer_size > lt->max_size)
    {
      buffer_size = lt->max_size;
    }

                                       
    buffer_size -= buffer_size % BLCKSZ;
  }

  if (lt->writing)
  {
       
                                                                        
                                             
       
    if (lt->dirty)
    {
         
                                                                        
                                                                         
                                                                       
                                                                      
                                                                       
                                                                   
                        
         
      VALGRIND_MAKE_MEM_DEFINED(lt->buffer + lt->nbytes, lt->buffer_size - lt->nbytes);

      TapeBlockSetNBytes(lt->buffer, lt->nbytes);
      ltsWriteBlock(lts, lt->curBlockNumber, (void *)lt->buffer);
    }
    lt->writing = false;
  }
  else
  {
       
                                                                       
             
       
    Assert(lt->frozen);
  }

                                                         
  if (lt->buffer)
  {
    pfree(lt->buffer);
  }
  lt->buffer = NULL;
  lt->buffer_size = 0;
  if (lt->firstBlockNumber != -1L)
  {
    lt->buffer = palloc(buffer_size);
    lt->buffer_size = buffer_size;
  }

                                                       
  lt->nextBlockNumber = lt->firstBlockNumber;
  lt->pos = 0;
  lt->nbytes = 0;
  ltsReadFillBuffer(lts, lt);
}

   
                                                           
   
                                                                      
                                                                           
                                                                          
         
   
void
LogicalTapeRewindForWrite(LogicalTapeSet *lts, int tapenum)
{
  LogicalTape *lt;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];

  Assert(!lt->writing && !lt->frozen);
  lt->writing = true;
  lt->dirty = false;
  lt->firstBlockNumber = -1L;
  lt->curBlockNumber = -1L;
  lt->pos = 0;
  lt->nbytes = 0;
  if (lt->buffer)
  {
    pfree(lt->buffer);
  }
  lt->buffer = NULL;
  lt->buffer_size = 0;
}

   
                             
   
                                                                      
   
size_t
LogicalTapeRead(LogicalTapeSet *lts, int tapenum, void *ptr, size_t size)
{
  LogicalTape *lt;
  size_t nread = 0;
  size_t nthistime;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];
  Assert(!lt->writing);

  while (size > 0)
  {
    if (lt->pos >= lt->nbytes)
    {
                                              
      if (!ltsReadFillBuffer(lts, lt))
      {
        break;          
      }
    }

    nthistime = lt->nbytes - lt->pos;
    if (nthistime > size)
    {
      nthistime = size;
    }
    Assert(nthistime > 0);

    memcpy(ptr, lt->buffer + lt->pos, nthistime);

    lt->pos += nthistime;
    ptr = (void *)((char *)ptr + nthistime);
    size -= nthistime;
    nread += nthistime;
  }

  return nread;
}

   
                                                                         
                                                                        
                                                                        
                                                         
   
                                                                     
                                                                      
                                                                         
                                          
   
                                                                            
                                                                       
                                                                       
                                                                      
                                          
   
void
LogicalTapeFreeze(LogicalTapeSet *lts, int tapenum, TapeShare *share)
{
  LogicalTape *lt;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];
  Assert(lt->writing);
  Assert(lt->offsetBlockNumber == 0L);

     
                                                                             
                              
     
  if (lt->dirty)
  {
       
                                                                          
                                                                   
                                                                     
                                                                         
                                                                           
                                                                    
       
    VALGRIND_MAKE_MEM_DEFINED(lt->buffer + lt->nbytes, lt->buffer_size - lt->nbytes);

    TapeBlockSetNBytes(lt->buffer, lt->nbytes);
    ltsWriteBlock(lts, lt->curBlockNumber, (void *)lt->buffer);
    lt->writing = false;
  }
  lt->writing = false;
  lt->frozen = true;

     
                                                                         
                                                                           
                                                                           
                                                                          
                                                             
     
  if (!lt->buffer || lt->buffer_size != BLCKSZ)
  {
    if (lt->buffer)
    {
      pfree(lt->buffer);
    }
    lt->buffer = palloc(BLCKSZ);
    lt->buffer_size = BLCKSZ;
  }

                                                       
  lt->curBlockNumber = lt->firstBlockNumber;
  lt->pos = 0;
  lt->nbytes = 0;

  if (lt->firstBlockNumber == -1L)
  {
    lt->nextBlockNumber = -1L;
  }
  ltsReadBlock(lts, lt->curBlockNumber, (void *)lt->buffer);
  if (TapeBlockIsLast(lt->buffer))
  {
    lt->nextBlockNumber = -1L;
  }
  else
  {
    lt->nextBlockNumber = TapeBlockGetTrailer(lt->buffer)->next;
  }
  lt->nbytes = TapeBlockGetNBytes(lt->buffer);

                                                              
  if (share)
  {
    BufFileExportShared(lts->pfile);
    share->firstblocknumber = lt->firstBlockNumber;
  }
}

   
                                                                        
                                       
   
                                                                    
                                                                  
                                       
   
                                                                   
                                                                      
                                                                     
              
   
size_t
LogicalTapeBackspace(LogicalTapeSet *lts, int tapenum, size_t size)
{
  LogicalTape *lt;
  size_t seekpos = 0;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];
  Assert(lt->frozen);
  Assert(lt->buffer_size == BLCKSZ);

     
                                              
     
  if (size <= (size_t)lt->pos)
  {
    lt->pos -= (int)size;
    return size;
  }

     
                                                                    
                                                                       
                                                                  
     
  seekpos = (size_t)lt->pos;                             
  while (size > seekpos)
  {
    long prev = TapeBlockGetTrailer(lt->buffer)->prev;

    if (prev == -1L)
    {
                                                          
      if (lt->curBlockNumber != lt->firstBlockNumber)
      {
        elog(ERROR, "unexpected end of tape");
      }
      lt->pos = 0;
      return seekpos;
    }

    ltsReadBlock(lts, prev, (void *)lt->buffer);

    if (TapeBlockGetTrailer(lt->buffer)->next != lt->curBlockNumber)
    {
      elog(ERROR, "broken tape, next of block %ld is %ld, expected %ld", prev, TapeBlockGetTrailer(lt->buffer)->next, lt->curBlockNumber);
    }

    lt->nbytes = TapeBlockPayloadSize;
    lt->curBlockNumber = prev;
    lt->nextBlockNumber = TapeBlockGetTrailer(lt->buffer)->next;

    seekpos += TapeBlockPayloadSize;
  }

     
                                                                        
                                                                            
           
     
  lt->pos = seekpos - size;
  return size;
}

   
                                                    
   
                                                
   
                                                             
                      
   
void
LogicalTapeSeek(LogicalTapeSet *lts, int tapenum, long blocknum, int offset)
{
  LogicalTape *lt;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];
  Assert(lt->frozen);
  Assert(offset >= 0 && offset <= TapeBlockPayloadSize);
  Assert(lt->buffer_size == BLCKSZ);

  if (blocknum != lt->curBlockNumber)
  {
    ltsReadBlock(lts, blocknum, (void *)lt->buffer);
    lt->curBlockNumber = blocknum;
    lt->nbytes = TapeBlockPayloadSize;
    lt->nextBlockNumber = TapeBlockGetTrailer(lt->buffer)->next;
  }

  if (offset > lt->nbytes)
  {
    elog(ERROR, "invalid tape seek position");
  }
  lt->pos = offset;
}

   
                                                                           
   
                                                                          
                                                                            
   
void
LogicalTapeTell(LogicalTapeSet *lts, int tapenum, long *blocknum, int *offset)
{
  LogicalTape *lt;

  Assert(tapenum >= 0 && tapenum < lts->nTapes);
  lt = &lts->tapes[tapenum];
  Assert(lt->offsetBlockNumber == 0L);

                                                                              
  Assert(lt->buffer_size == BLCKSZ);

  *blocknum = lt->curBlockNumber;
  *offset = lt->pos;
}

   
                                                                          
   
long
LogicalTapeSetBlocks(LogicalTapeSet *lts)
{
  return lts->nBlocksAllocated - lts->nHoleBlocks;
}
