                                                                            
   
              
                                                                     
                                                             
   
                                                                         
                                                                          
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/parallel.h"
#include "catalog/catalog.h"
#include "executor/instrument.h"
#include "storage/buf_internals.h"
#include "storage/bufmgr.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner_private.h"

                   

                                       
typedef struct
{
  BufferTag key;                         
  int id;                                             
} LocalBufferLookupEnt;

                                                                    
#define LocalBufHdrGetBlock(bufHdr) LocalBufferBlockPointers[-((bufHdr)->buf_id + 2)]

int NLocBuffer = 0;                                    

BufferDesc *LocalBufferDescriptors = NULL;
Block *LocalBufferBlockPointers = NULL;
int32 *LocalRefCount = NULL;

static int nextFreeLocalBuf = 0;

static HTAB *LocalBufHash = NULL;

static void
InitLocalBuffers(void);
static Block
GetLocalBufferStorage(void);

   
                         
                                                         
   
                                                     
                                           
   
void
LocalPrefetchBuffer(SMgrRelation smgr, ForkNumber forkNum, BlockNumber blockNum)
{
#ifdef USE_PREFETCH
  BufferTag newTag;                                  
  LocalBufferLookupEnt *hresult;

  INIT_BUFFERTAG(newTag, smgr->smgr_rnode.node, forkNum, blockNum);

                                                                 
  if (LocalBufHash == NULL)
  {
    InitLocalBuffers();
  }

                                                
  hresult = (LocalBufferLookupEnt *)hash_search(LocalBufHash, (void *)&newTag, HASH_FIND, NULL);

  if (hresult)
  {
                               
    return;
  }

                                            
  smgrprefetch(smgr, forkNum, blockNum);
#endif                   
}

   
                      
                                                                             
   
                                                                        
                                                                     
                                                                      
                                            
   
BufferDesc *
LocalBufferAlloc(SMgrRelation smgr, ForkNumber forkNum, BlockNumber blockNum, bool *foundPtr)
{
  BufferTag newTag;                                  
  LocalBufferLookupEnt *hresult;
  BufferDesc *bufHdr;
  int b;
  int trycounter;
  bool found;
  uint32 buf_state;

  INIT_BUFFERTAG(newTag, smgr->smgr_rnode.node, forkNum, blockNum);

                                                                 
  if (LocalBufHash == NULL)
  {
    InitLocalBuffers();
  }

                                                
  hresult = (LocalBufferLookupEnt *)hash_search(LocalBufHash, (void *)&newTag, HASH_FIND, NULL);

  if (hresult)
  {
    b = hresult->id;
    bufHdr = GetLocalBufferDescriptor(b);
    Assert(BUFFERTAGS_EQUAL(bufHdr->tag, newTag));
#ifdef LBDEBUG
    fprintf(stderr, "LB ALLOC (%u,%d,%d) %d\n", smgr->smgr_rnode.node.relNode, forkNum, blockNum, -b - 1);
#endif
    buf_state = pg_atomic_read_u32(&bufHdr->state);

                                                                  
    if (LocalRefCount[b] == 0)
    {
      if (BUF_STATE_GET_USAGECOUNT(buf_state) < BM_MAX_USAGE_COUNT)
      {
        buf_state += BUF_USAGECOUNT_ONE;
        pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
      }
    }
    LocalRefCount[b]++;
    ResourceOwnerRememberBuffer(CurrentResourceOwner, BufferDescriptorGetBuffer(bufHdr));
    if (buf_state & BM_VALID)
    {
      *foundPtr = true;
    }
    else
    {
                                                             
      *foundPtr = false;
    }
    return bufHdr;
  }

#ifdef LBDEBUG
  fprintf(stderr, "LB ALLOC (%u,%d,%d) %d\n", smgr->smgr_rnode.node.relNode, forkNum, blockNum, -nextFreeLocalBuf - 1);
#endif

     
                                                                            
                                              
     
  trycounter = NLocBuffer;
  for (;;)
  {
    b = nextFreeLocalBuf;

    if (++nextFreeLocalBuf >= NLocBuffer)
    {
      nextFreeLocalBuf = 0;
    }

    bufHdr = GetLocalBufferDescriptor(b);

    if (LocalRefCount[b] == 0)
    {
      buf_state = pg_atomic_read_u32(&bufHdr->state);

      if (BUF_STATE_GET_USAGECOUNT(buf_state) > 0)
      {
        buf_state -= BUF_USAGECOUNT_ONE;
        pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
        trycounter = NLocBuffer;
      }
      else
      {
                                   
        LocalRefCount[b]++;
        ResourceOwnerRememberBuffer(CurrentResourceOwner, BufferDescriptorGetBuffer(bufHdr));
        break;
      }
    }
    else if (--trycounter == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("no empty local buffer available")));
    }
  }

     
                                                                          
                                               
     
  if (buf_state & BM_DIRTY)
  {
    SMgrRelation oreln;
    Page localpage = (char *)LocalBufHdrGetBlock(bufHdr);

                                       
    oreln = smgropen(bufHdr->tag.rnode, MyBackendId);

    PageSetChecksumInplace(localpage, bufHdr->tag.blockNum);

                      
    smgrwrite(oreln, bufHdr->tag.forkNum, bufHdr->tag.blockNum, localpage, false);

                                                       
    buf_state &= ~BM_DIRTY;
    pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);

    pgBufferUsage.local_blks_written++;
  }

     
                                                                      
     
  if (LocalBufHdrGetBlock(bufHdr) == NULL)
  {
                                                       
    LocalBufHdrGetBlock(bufHdr) = GetLocalBufferStorage();
  }

     
                                                                        
     
  if (buf_state & BM_TAG_VALID)
  {
    hresult = (LocalBufferLookupEnt *)hash_search(LocalBufHash, (void *)&bufHdr->tag, HASH_REMOVE, NULL);
    if (!hresult)                       
    {
      elog(ERROR, "local buffer hash table corrupted");
    }
                                                            
    CLEAR_BUFFERTAG(bufHdr->tag);
    buf_state &= ~(BM_VALID | BM_TAG_VALID);
    pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
  }

  hresult = (LocalBufferLookupEnt *)hash_search(LocalBufHash, (void *)&newTag, HASH_ENTER, &found);
  if (found)                       
  {
    elog(ERROR, "local buffer hash table corrupted");
  }
  hresult->id = b;

     
                        
     
  bufHdr->tag = newTag;
  buf_state &= ~(BM_VALID | BM_DIRTY | BM_JUST_DIRTIED | BM_IO_ERROR);
  buf_state |= BM_TAG_VALID;
  buf_state &= ~BUF_USAGECOUNT_MASK;
  buf_state += BUF_USAGECOUNT_ONE;
  pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);

  *foundPtr = false;
  return bufHdr;
}

   
                          
                               
   
void
MarkLocalBufferDirty(Buffer buffer)
{
  int bufid;
  BufferDesc *bufHdr;
  uint32 buf_state;

  Assert(BufferIsLocal(buffer));

#ifdef LBDEBUG
  fprintf(stderr, "LB DIRTY %d\n", buffer);
#endif

  bufid = -(buffer + 1);

  Assert(LocalRefCount[bufid] > 0);

  bufHdr = GetLocalBufferDescriptor(bufid);

  buf_state = pg_atomic_read_u32(&bufHdr->state);

  if (!(buf_state & BM_DIRTY))
  {
    pgBufferUsage.local_blks_dirtied++;
  }

  buf_state |= BM_DIRTY;

  pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
}

   
                               
                                                                    
                                                                 
                                                                    
                                                                    
                                                                       
                                    
   
                                                           
   
void
DropRelFileNodeLocalBuffers(RelFileNode rnode, ForkNumber forkNum, BlockNumber firstDelBlock)
{
  int i;

  for (i = 0; i < NLocBuffer; i++)
  {
    BufferDesc *bufHdr = GetLocalBufferDescriptor(i);
    LocalBufferLookupEnt *hresult;
    uint32 buf_state;

    buf_state = pg_atomic_read_u32(&bufHdr->state);

    if ((buf_state & BM_TAG_VALID) && RelFileNodeEquals(bufHdr->tag.rnode, rnode) && bufHdr->tag.forkNum == forkNum && bufHdr->tag.blockNum >= firstDelBlock)
    {
      if (LocalRefCount[i] != 0)
      {
        elog(ERROR, "block %u of %s is still referenced (local %u)", bufHdr->tag.blockNum, relpathbackend(bufHdr->tag.rnode, MyBackendId, bufHdr->tag.forkNum), LocalRefCount[i]);
      }
                                       
      hresult = (LocalBufferLookupEnt *)hash_search(LocalBufHash, (void *)&bufHdr->tag, HASH_REMOVE, NULL);
      if (!hresult)                       
      {
        elog(ERROR, "local buffer hash table corrupted");
      }
                               
      CLEAR_BUFFERTAG(bufHdr->tag);
      buf_state &= ~BUF_FLAG_MASK;
      buf_state &= ~BUF_USAGECOUNT_MASK;
      pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
    }
  }
}

   
                                  
                                                                      
                               
   
                                                              
   
void
DropRelFileNodeAllLocalBuffers(RelFileNode rnode)
{
  int i;

  for (i = 0; i < NLocBuffer; i++)
  {
    BufferDesc *bufHdr = GetLocalBufferDescriptor(i);
    LocalBufferLookupEnt *hresult;
    uint32 buf_state;

    buf_state = pg_atomic_read_u32(&bufHdr->state);

    if ((buf_state & BM_TAG_VALID) && RelFileNodeEquals(bufHdr->tag.rnode, rnode))
    {
      if (LocalRefCount[i] != 0)
      {
        elog(ERROR, "block %u of %s is still referenced (local %u)", bufHdr->tag.blockNum, relpathbackend(bufHdr->tag.rnode, MyBackendId, bufHdr->tag.forkNum), LocalRefCount[i]);
      }
                                       
      hresult = (LocalBufferLookupEnt *)hash_search(LocalBufHash, (void *)&bufHdr->tag, HASH_REMOVE, NULL);
      if (!hresult)                       
      {
        elog(ERROR, "local buffer hash table corrupted");
      }
                               
      CLEAR_BUFFERTAG(bufHdr->tag);
      buf_state &= ~BUF_FLAG_MASK;
      buf_state &= ~BUF_USAGECOUNT_MASK;
      pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
    }
  }
}

   
                      
                                                                            
                                                                            
                                                                    
   
static void
InitLocalBuffers(void)
{
  int nbufs = num_temp_buffers;
  HASHCTL info;
  int i;

     
                                                                          
                                                                           
                                                                            
                                                                            
                                                                      
                    
     
  if (IsParallelWorker())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot access temporary tables during a parallel operation")));
  }

                                                             
  LocalBufferDescriptors = (BufferDesc *)calloc(nbufs, sizeof(BufferDesc));
  LocalBufferBlockPointers = (Block *)calloc(nbufs, sizeof(Block));
  LocalRefCount = (int32 *)calloc(nbufs, sizeof(int32));
  if (!LocalBufferDescriptors || !LocalBufferBlockPointers || !LocalRefCount)
  {
    ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }

  nextFreeLocalBuf = 0;

                                                        
  for (i = 0; i < nbufs; i++)
  {
    BufferDesc *buf = GetLocalBufferDescriptor(i);

       
                                                                         
                                                                      
                                                                         
               
       
    buf->buf_id = -i - 2;

       
                                                                    
                                                                      
                                                                        
                                            
       
  }

                                    
  MemSet(&info, 0, sizeof(info));
  info.keysize = sizeof(BufferTag);
  info.entrysize = sizeof(LocalBufferLookupEnt);

  LocalBufHash = hash_create("Local Buffer Lookup Table", nbufs, &info, HASH_ELEM | HASH_BLOBS);

  if (!LocalBufHash)
  {
    elog(ERROR, "could not initialize local buffer hash table");
  }

                                                   
  NLocBuffer = nbufs;
}

   
                                                              
   
                                                                      
                                                                          
                                                                           
                                                                             
                   
   
static Block
GetLocalBufferStorage(void)
{
  static char *cur_block = NULL;
  static int next_buf_in_block = 0;
  static int num_bufs_in_block = 0;
  static int total_bufs_allocated = 0;
  static MemoryContext LocalBufferContext = NULL;

  char *this_buf;

  Assert(total_bufs_allocated < NLocBuffer);

  if (next_buf_in_block >= num_bufs_in_block)
  {
                                              
    int num_bufs;

       
                                                                        
                                                                         
                                                 
       
    if (LocalBufferContext == NULL)
    {
      LocalBufferContext = AllocSetContextCreate(TopMemoryContext, "LocalBufferContext", ALLOCSET_DEFAULT_SIZES);
    }

                                                                          
    num_bufs = Max(num_bufs_in_block * 2, 16);
                                                                     
    num_bufs = Min(num_bufs, NLocBuffer - total_bufs_allocated);
                                                 
    num_bufs = Min(num_bufs, MaxAllocSize / BLCKSZ);

    cur_block = (char *)MemoryContextAlloc(LocalBufferContext, num_bufs * BLCKSZ);
    next_buf_in_block = 0;
    num_bufs_in_block = num_bufs;
  }

                                                    
  this_buf = cur_block + next_buf_in_block * BLCKSZ;
  next_buf_in_block++;
  total_bufs_allocated++;

  return (Block)this_buf;
}

   
                                                                             
   
                                                                   
   
static void
CheckForLocalBufferLeaks(void)
{
#ifdef USE_ASSERT_CHECKING
  if (LocalRefCount)
  {
    int RefCountErrors = 0;
    int i;

    for (i = 0; i < NLocBuffer; i++)
    {
      if (LocalRefCount[i] != 0)
      {
        Buffer b = -i - 1;

        PrintBufferLeakWarning(b);
        RefCountErrors++;
      }
    }
    Assert(RefCountErrors == 0);
  }
#endif
}

   
                                                           
   
                                                              
   
void
AtEOXact_LocalBuffers(bool isCommit)
{
  CheckForLocalBufferLeaks();
}

   
                                                                              
   
                                                                
   
void
AtProcExit_LocalBuffers(void)
{
     
                                                                           
                                                                             
                            
     
  CheckForLocalBufferLeaks();
}
