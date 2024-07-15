                                                                            
   
          
                                 
   
                                                                         
                                                               
   
   
                                                                         
   
                  
                                   
   
   
         
                                                                              
                                                                             
                                                                           
           
   
                                                                               
                                                                             
                                             
   
                                                                            
                                                                            
                                                                         
                                                                             
                                       
   
                                                                             
                                                                             
                                            
   
                                                                           
                                                                            
                                                                             
                             
   
                                                                               
                                                                          
                                                                              
                                                            
   
                                                                          
                                                                          
                                               
   
                                                                            
   

#include "postgres.h"

#include "utils/memdebug.h"
#include "utils/memutils.h"
#include "lib/ilist.h"

#define Slab_BLOCKHDRSZ MAXALIGN(sizeof(SlabBlock))

   
                                                                 
   
typedef struct SlabContext
{
  MemoryContextData header;                                     
                                               
  Size chunkSize;                     
  Size fullChunkSize;                                                
  Size blockSize;                     
  Size headerSize;                                          
  int chunksPerBlock;                                 
  int minFreeChunks;                                              
  int nblocks;                                        
#ifdef MEMORY_CONTEXT_CHECKING
  bool *freechunks;                                       
#endif
                                                                 
  dlist_head freelist[FLEXIBLE_ARRAY_MEMBER];
} SlabContext;

   
             
                                                   
   
                                                         
                                              
                                                 
   
typedef struct SlabBlock
{
  dlist_node node;                            
  int nfree;                                     
  int firstFreeChunk;                                                 
} SlabBlock;

   
             
                                                      
   
                                                                             
                                                                          
                                                                              
                                                                              
                                                                      
                                                         
   
typedef struct SlabChunk
{
  SlabBlock *block;                               
  SlabContext *slab;                     
                                                                                           
} SlabChunk;

#define SlabPointerGetChunk(ptr) ((SlabChunk *)(((char *)(ptr)) - sizeof(SlabChunk)))
#define SlabChunkGetPointer(chk) ((void *)(((char *)(chk)) + sizeof(SlabChunk)))
#define SlabBlockGetChunk(slab, block, idx) ((SlabChunk *)((char *)(block) + Slab_BLOCKHDRSZ + (idx * slab->fullChunkSize)))
#define SlabBlockStart(block) ((char *)block + Slab_BLOCKHDRSZ)
#define SlabChunkIndex(slab, block, chunk) (((char *)chunk - SlabBlockStart(block)) / slab->fullChunkSize)

   
                                                                      
   
static void *
SlabAlloc(MemoryContext context, Size size);
static void
SlabFree(MemoryContext context, void *pointer);
static void *
SlabRealloc(MemoryContext context, void *pointer, Size size);
static void
SlabReset(MemoryContext context);
static void
SlabDelete(MemoryContext context);
static Size
SlabGetChunkSpace(MemoryContext context, void *pointer);
static bool
SlabIsEmpty(MemoryContext context);
static void
SlabStats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals);
#ifdef MEMORY_CONTEXT_CHECKING
static void
SlabCheck(MemoryContext context);
#endif

   
                                                         
   
static const MemoryContextMethods SlabMethods = {SlabAlloc, SlabFree, SlabRealloc, SlabReset, SlabDelete, SlabGetChunkSpace, SlabIsEmpty, SlabStats
#ifdef MEMORY_CONTEXT_CHECKING
    ,
    SlabCheck
#endif
};

              
                
              
   
#ifdef HAVE_ALLOCINFO
#define SlabFreeInfo(_cxt, _chunk) fprintf(stderr, "SlabFree: %s: %p, %zu\n", (_cxt)->header.name, (_chunk), (_chunk)->header.size)
#define SlabAllocInfo(_cxt, _chunk) fprintf(stderr, "SlabAlloc: %s: %p, %zu\n", (_cxt)->header.name, (_chunk), (_chunk)->header.size)
#else
#define SlabFreeInfo(_cxt, _chunk)
#define SlabAllocInfo(_cxt, _chunk)
#endif

   
                     
                               
   
                                                        
                                                        
                                    
                                    
   
                                 
                                                                            
   
MemoryContext
SlabContextCreate(MemoryContext parent, const char *name, Size blockSize, Size chunkSize)
{
  int chunksPerBlock;
  Size fullChunkSize;
  Size freelistSize;
  Size headerSize;
  SlabContext *slab;
  int i;

                                           
  StaticAssertStmt(sizeof(SlabChunk) == MAXALIGN(sizeof(SlabChunk)), "sizeof(SlabChunk) is not maxaligned");
  StaticAssertStmt(offsetof(SlabChunk, slab) + sizeof(MemoryContext) == sizeof(SlabChunk), "padding calculation in SlabChunk is wrong");

                                                                
  if (chunkSize < sizeof(int))
  {
    chunkSize = sizeof(int);
  }

                                                                    
  fullChunkSize = sizeof(SlabChunk) + MAXALIGN(chunkSize);

                                                         
  if (blockSize < fullChunkSize + Slab_BLOCKHDRSZ)
  {
    elog(ERROR, "block size %zu for slab is too small for %zu chunks", blockSize, chunkSize);
  }

                                                  
  chunksPerBlock = (blockSize - Slab_BLOCKHDRSZ) / fullChunkSize;

                                                             
  freelistSize = sizeof(dlist_head) * (chunksPerBlock + 1);

     
                                                                          
                                                                          
     

                                         
  headerSize = offsetof(SlabContext, freelist) + freelistSize;

#ifdef MEMORY_CONTEXT_CHECKING
     
                                                                          
                                                                          
                            
     
  headerSize += chunksPerBlock * sizeof(bool);
#endif

  slab = (SlabContext *)malloc(headerSize);
  if (slab == NULL)
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed while creating memory context \"%s\".", name)));
  }

     
                                                                            
                                                         
     

                                                  
  slab->chunkSize = chunkSize;
  slab->fullChunkSize = fullChunkSize;
  slab->blockSize = blockSize;
  slab->headerSize = headerSize;
  slab->chunksPerBlock = chunksPerBlock;
  slab->minFreeChunks = 0;
  slab->nblocks = 0;

                                     
  for (i = 0; i < (slab->chunksPerBlock + 1); i++)
  {
    dlist_init(&slab->freelist[i]);
  }

#ifdef MEMORY_CONTEXT_CHECKING
                                                                  
  slab->freechunks = (bool *)slab + offsetof(SlabContext, freelist) + freelistSize;
#endif

                                                                 
  MemoryContextCreate((MemoryContext)slab, T_SlabContext, &SlabMethods, parent, name);

  return (MemoryContext)slab;
}

   
             
                                                          
   
                                                                           
                                        
   
static void
SlabReset(MemoryContext context)
{
  int i;
  SlabContext *slab = castNode(SlabContext, context);

  Assert(slab);

#ifdef MEMORY_CONTEXT_CHECKING
                                                     
  SlabCheck(context);
#endif

                                               
  for (i = 0; i <= slab->chunksPerBlock; i++)
  {
    dlist_mutable_iter miter;

    dlist_foreach_modify(miter, &slab->freelist[i])
    {
      SlabBlock *block = dlist_container(SlabBlock, node, miter.cur);

      dlist_delete(miter.cur);

#ifdef CLOBBER_FREED_MEMORY
      wipe_mem(block, slab->blockSize);
#endif
      free(block);
      slab->nblocks--;
    }
  }

  slab->minFreeChunks = 0;

  Assert(slab->nblocks == 0);
}

   
              
                                                             
   
static void
SlabDelete(MemoryContext context)
{
                                           
  SlabReset(context);
                                   
  free(context);
}

   
             
                                                                 
                                                                 
   
static void *
SlabAlloc(MemoryContext context, Size size)
{
  SlabContext *slab = castNode(SlabContext, context);
  SlabBlock *block;
  SlabChunk *chunk;
  int idx;

  Assert(slab);

  Assert((slab->minFreeChunks >= 0) && (slab->minFreeChunks < slab->chunksPerBlock));

                                                    
  if (size != slab->chunkSize)
  {
    elog(ERROR, "unexpected alloc chunk size %zu (expected %zu)", size, slab->chunkSize);
  }

     
                                                                           
                                             
     
                                                                          
                                                                       
     
  if (slab->minFreeChunks == 0)
  {
    block = (SlabBlock *)malloc(slab->blockSize);

    if (block == NULL)
    {
      return NULL;
    }

    block->nfree = slab->chunksPerBlock;
    block->firstFreeChunk = 0;

       
                                                                        
                            
       
    for (idx = 0; idx < slab->chunksPerBlock; idx++)
    {
      chunk = SlabBlockGetChunk(slab, block, idx);
      *(int32 *)SlabChunkGetPointer(chunk) = (idx + 1);
    }

       
                                                              
       
                                                                          
                         
       
    Assert(dlist_is_empty(&slab->freelist[slab->chunksPerBlock]));

    dlist_push_head(&slab->freelist[slab->chunksPerBlock], &block->node);

    slab->minFreeChunks = slab->chunksPerBlock;
    slab->nblocks += 1;
  }

                                                                      
  block = dlist_head_element(SlabBlock, node, &slab->freelist[slab->minFreeChunks]);

                                                                    
  Assert(block != NULL);
  Assert(slab->minFreeChunks == block->nfree);
  Assert(block->nfree > 0);

                                                          
  idx = block->firstFreeChunk;

                                                                         
  Assert((idx >= 0) && (idx < slab->chunksPerBlock));

                                                                       
  chunk = SlabBlockGetChunk(slab, block, idx);

     
                                                                       
                                                                        
                                              
     
  block->nfree--;
  slab->minFreeChunks = block->nfree;

     
                                                                         
                                          
     
  VALGRIND_MAKE_MEM_DEFINED(SlabChunkGetPointer(chunk), sizeof(int32));
  block->firstFreeChunk = *(int32 *)SlabChunkGetPointer(chunk);

  Assert(block->firstFreeChunk >= 0);
  Assert(block->firstFreeChunk <= slab->chunksPerBlock);

  Assert((block->nfree != 0 && block->firstFreeChunk < slab->chunksPerBlock) || (block->nfree == 0 && block->firstFreeChunk == slab->chunksPerBlock));

                                                               
  dlist_delete(&block->node);
  dlist_push_head(&slab->freelist[block->nfree], &block->node);

     
                                                                            
                                                                          
                                                                            
                                                               
     
  if (slab->minFreeChunks == 0)
  {
    for (idx = 1; idx <= slab->chunksPerBlock; idx++)
    {
      if (dlist_is_empty(&slab->freelist[idx]))
      {
        continue;
      }

                                      
      slab->minFreeChunks = idx;
      break;
    }
  }

  if (slab->minFreeChunks == slab->chunksPerBlock)
  {
    slab->minFreeChunks = 0;
  }

                                               
  VALGRIND_MAKE_MEM_UNDEFINED(chunk, sizeof(SlabChunk));

  chunk->block = block;
  chunk->slab = slab;

#ifdef MEMORY_CONTEXT_CHECKING
                                                    
  if (slab->chunkSize < (slab->fullChunkSize - sizeof(SlabChunk)))
  {
    set_sentinel(SlabChunkGetPointer(chunk), size);
    VALGRIND_MAKE_MEM_NOACCESS(((char *)chunk) + sizeof(SlabChunk) + slab->chunkSize, slab->fullChunkSize - (slab->chunkSize + sizeof(SlabChunk)));
  }
#endif
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                          
  randomize_mem((char *)SlabChunkGetPointer(chunk), size);
#endif

  SlabAllocInfo(slab, chunk);
  return SlabChunkGetPointer(chunk);
}

   
            
                                                             
   
static void
SlabFree(MemoryContext context, void *pointer)
{
  int idx;
  SlabContext *slab = castNode(SlabContext, context);
  SlabChunk *chunk = SlabPointerGetChunk(pointer);
  SlabBlock *block = chunk->block;

  SlabFreeInfo(slab, chunk);

#ifdef MEMORY_CONTEXT_CHECKING
                                                            
  if (slab->chunkSize < (slab->fullChunkSize - sizeof(SlabChunk)))
  {
    if (!sentinel_ok(pointer, slab->chunkSize))
    {
      elog(WARNING, "detected write past chunk end in %s %p", slab->header.name, chunk);
    }
  }
#endif

                                                              
  idx = SlabChunkIndex(slab, block, chunk);

                                                           
  *(int32 *)pointer = block->firstFreeChunk;
  block->firstFreeChunk = idx;
  block->nfree++;

  Assert(block->nfree > 0);
  Assert(block->nfree <= slab->chunksPerBlock);

#ifdef CLOBBER_FREED_MEMORY
                                                                     
  wipe_mem((char *)pointer + sizeof(int32), slab->chunkSize - sizeof(int32));
#endif

                                        
  dlist_delete(&block->node);

     
                                                                             
                                                                       
                                                                           
                                                                             
                                                                          
                                    
     
                                                                            
                                                                            
                                                      
     
  if (slab->minFreeChunks == (block->nfree - 1))
  {
                                                           
    if (dlist_is_empty(&slab->freelist[slab->minFreeChunks]))
    {
                                                                 
      if (block->nfree == slab->chunksPerBlock)
      {
        slab->minFreeChunks = 0;
      }
      else
      {
        slab->minFreeChunks++;
      }
    }
  }

                                                      
  if (block->nfree == slab->chunksPerBlock)
  {
    free(block);
    slab->nblocks--;
  }
  else
  {
    dlist_push_head(&slab->freelist[block->nfree], &block->node);
  }

  Assert(slab->nblocks >= 0);
}

   
               
                                          
   
                                                                               
                                                                             
                                                                        
                                                     
   
                                                                           
                                                                              
                                                 
   
static void *
SlabRealloc(MemoryContext context, void *pointer, Size size)
{
  SlabContext *slab = castNode(SlabContext, context);

  Assert(slab);

                                                                     
  if (size == slab->chunkSize)
  {
    return pointer;
  }

  elog(ERROR, "slab allocator does not support realloc()");
  return NULL;                          
}

   
                     
                                                                 
                                                            
   
static Size
SlabGetChunkSpace(MemoryContext context, void *pointer)
{
  SlabContext *slab = castNode(SlabContext, context);

  Assert(slab);

  return slab->fullChunkSize;
}

   
               
                                             
   
static bool
SlabIsEmpty(MemoryContext context)
{
  SlabContext *slab = castNode(SlabContext, context);

  Assert(slab);

  return (slab->nblocks == 0);
}

   
             
                                                              
   
                                                                       
                                                     
                                                                   
   
static void
SlabStats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals)
{
  SlabContext *slab = castNode(SlabContext, context);
  Size nblocks = 0;
  Size freechunks = 0;
  Size totalspace;
  Size freespace = 0;
  int i;

                                            
  totalspace = slab->headerSize;

  for (i = 0; i <= slab->chunksPerBlock; i++)
  {
    dlist_iter iter;

    dlist_foreach(iter, &slab->freelist[i])
    {
      SlabBlock *block = dlist_container(SlabBlock, node, iter.cur);

      nblocks++;
      totalspace += slab->blockSize;
      freespace += slab->fullChunkSize * block->nfree;
      freechunks += block->nfree;
    }
  }

  if (printfunc)
  {
    char stats_string[200];

    snprintf(stats_string, sizeof(stats_string), "%zu total in %zd blocks; %zu free (%zd chunks); %zu used", totalspace, nblocks, freespace, freechunks, totalspace - freespace);
    printfunc(context, passthru, stats_string);
  }

  if (totals)
  {
    totals->nblocks += nblocks;
    totals->freechunks += freechunks;
    totals->totalspace += totalspace;
    totals->freespace += freespace;
  }
}

#ifdef MEMORY_CONTEXT_CHECKING

   
             
                                                         
   
                                                                           
                                                                       
                                                                            
   
static void
SlabCheck(MemoryContext context)
{
  int i;
  SlabContext *slab = castNode(SlabContext, context);
  const char *name = slab->header.name;

  Assert(slab);
  Assert(slab->chunksPerBlock > 0);

                              
  for (i = 0; i <= slab->chunksPerBlock; i++)
  {
    int j, nfree;
    dlist_iter iter;

                                          
    dlist_foreach(iter, &slab->freelist[i])
    {
      int idx;
      SlabBlock *block = dlist_container(SlabBlock, node, iter.cur);

         
                                                                   
                                           
         
      if (block->nfree != i)
      {
        elog(WARNING, "problem in slab %s: number of free chunks %d in block %p does not match freelist %d", name, block->nfree, block, i);
      }

                                                          
      memset(slab->freechunks, 0, (slab->chunksPerBlock * sizeof(bool)));
      idx = block->firstFreeChunk;

         
                                                                   
                                                                        
                                                                     
                                                               
         

      nfree = 0;
      while (idx < slab->chunksPerBlock)
      {
        SlabChunk *chunk;

                                                           
        nfree++;
        slab->freechunks[idx] = true;

                                               
        chunk = SlabBlockGetChunk(slab, block, idx);
        VALGRIND_MAKE_MEM_DEFINED(SlabChunkGetPointer(chunk), sizeof(int32));
        idx = *(int32 *)SlabChunkGetPointer(chunk);
      }

      for (j = 0; j < slab->chunksPerBlock; j++)
      {
                                                                      
        if (!slab->freechunks[j])
        {
          SlabChunk *chunk = SlabBlockGetChunk(slab, block, j);

                                                                       
          if (chunk->block != block)
          {
            elog(WARNING, "problem in slab %s: bogus block link in block %p, chunk %p", name, block, chunk);
          }

          if (chunk->slab != slab)
          {
            elog(WARNING, "problem in slab %s: bogus slab link in block %p, chunk %p", name, block, chunk);
          }

                                                             
          if (slab->chunkSize < (slab->fullChunkSize - sizeof(SlabChunk)))
          {
            if (!sentinel_ok(chunk, sizeof(SlabChunk) + slab->chunkSize))
            {
              elog(WARNING, "problem in slab %s: detected write past chunk end in block %p, chunk %p", name, block, chunk);
            }
          }
        }
      }

         
                                                                         
                               
         
      if (nfree != block->nfree)
      {
        elog(WARNING, "problem in slab %s: number of free chunks %d in block %p does not match bitmap %d", name, block->nfree, block, nfree);
      }
    }
  }
}

#endif                              
