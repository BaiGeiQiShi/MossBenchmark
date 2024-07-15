                                                                            
   
                
                                         
   
                                                                             
                                 
   
                                                                         
   
                  
                                         
   
   
                                                                            
                                                                              
                                                                           
                                                                               
                                   
   
                                                                            
                                                                    
                                                                            
                                                                         
                                                                        
                                
   
                                                                             
                                                                        
                                                          
   
                                                                            
                                                                           
                          
   
                                                                            
   

#include "postgres.h"

#include "lib/ilist.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

#define Generation_BLOCKHDRSZ MAXALIGN(sizeof(GenerationBlock))
#define Generation_CHUNKHDRSZ sizeof(GenerationChunk)

typedef struct GenerationBlock GenerationBlock;                        
typedef struct GenerationChunk GenerationChunk;

typedef void *GenerationPointer;

   
                                                                              
                                                 
   
typedef struct GenerationContext
{
  MemoryContextData header;                                     

                                       
  Size blockSize;                          

  GenerationBlock *block;                                              
  dlist_head blocks;                          
} GenerationContext;

   
                   
                                                                           
                                                                        
                                                                            
                                                                 
                                                                         
                                                      
   
                                                                        
                                                            
   
struct GenerationBlock
{
  dlist_node node;                                   
  Size blksize;                                      
  int nchunks;                                        
  int nfree;                                  
  char *freeptr;                                          
  char *endptr;                                    
};

   
                   
                                                            
   
                                                                             
                                                                             
                                                                           
                                                                          
                                                                             
                                                                             
                      
   
struct GenerationChunk
{
                                                                
  Size size;
#ifdef MEMORY_CONTEXT_CHECKING
                                                                     
                                    
  Size requested_size;

#define GENERATIONCHUNK_RAWSIZE (SIZEOF_SIZE_T * 2 + SIZEOF_VOID_P * 2)
#else
#define GENERATIONCHUNK_RAWSIZE (SIZEOF_SIZE_T + SIZEOF_VOID_P * 2)
#endif                              

                                                           
#if (GENERATIONCHUNK_RAWSIZE % MAXIMUM_ALIGNOF) != 0
  char padding[MAXIMUM_ALIGNOF - GENERATIONCHUNK_RAWSIZE % MAXIMUM_ALIGNOF];
#endif

  GenerationBlock *block;                                  
  GenerationContext *context;                                             
                                                                                                    
};

   
                                                                    
                                                                              
                                                                         
               
   
#define GENERATIONCHUNK_PRIVATE_LEN offsetof(GenerationChunk, context)

   
                     
                                          
   
#define GenerationIsValid(set) PointerIsValid(set)

#define GenerationPointerGetChunk(ptr) ((GenerationChunk *)(((char *)(ptr)) - Generation_CHUNKHDRSZ))
#define GenerationChunkGetPointer(chk) ((GenerationPointer *)(((char *)(chk)) + Generation_CHUNKHDRSZ))

   
                                                                            
   
static void *
GenerationAlloc(MemoryContext context, Size size);
static void
GenerationFree(MemoryContext context, void *pointer);
static void *
GenerationRealloc(MemoryContext context, void *pointer, Size size);
static void
GenerationReset(MemoryContext context);
static void
GenerationDelete(MemoryContext context);
static Size
GenerationGetChunkSpace(MemoryContext context, void *pointer);
static bool
GenerationIsEmpty(MemoryContext context);
static void
GenerationStats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals);

#ifdef MEMORY_CONTEXT_CHECKING
static void
GenerationCheck(MemoryContext context);
#endif

   
                                                               
   
static const MemoryContextMethods GenerationMethods = {GenerationAlloc, GenerationFree, GenerationRealloc, GenerationReset, GenerationDelete, GenerationGetChunkSpace, GenerationIsEmpty, GenerationStats
#ifdef MEMORY_CONTEXT_CHECKING
    ,
    GenerationCheck
#endif
};

              
                
              
   
#ifdef HAVE_ALLOCINFO
#define GenerationFreeInfo(_cxt, _chunk) fprintf(stderr, "GenerationFree: %s: %p, %lu\n", (_cxt)->name, (_chunk), (_chunk)->size)
#define GenerationAllocInfo(_cxt, _chunk) fprintf(stderr, "GenerationAlloc: %s: %p, %lu\n", (_cxt)->name, (_chunk), (_chunk)->size)
#else
#define GenerationFreeInfo(_cxt, _chunk)
#define GenerationAllocInfo(_cxt, _chunk)
#endif

   
                   
   

   
                           
                                     
   
                                                        
                                                        
                                    
   
MemoryContext
GenerationContextCreate(MemoryContext parent, const char *name, Size blockSize)
{
  GenerationContext *set;

                                                 
  StaticAssertStmt(Generation_CHUNKHDRSZ == MAXALIGN(Generation_CHUNKHDRSZ), "sizeof(GenerationChunk) is not maxaligned");
  StaticAssertStmt(offsetof(GenerationChunk, context) + sizeof(MemoryContext) == Generation_CHUNKHDRSZ, "padding calculation in GenerationChunk is wrong");

     
                                                                         
                                                                           
                                                                          
                                
     
  if (blockSize != MAXALIGN(blockSize) || blockSize < 1024 || !AllocHugeSizeIsValid(blockSize))
  {
    elog(ERROR, "invalid blockSize for memory context: %zu", blockSize);
  }

     
                                                                          
                                                                         
                                                  
     

  set = (GenerationContext *)malloc(MAXALIGN(sizeof(GenerationContext)));
  if (set == NULL)
  {
    MemoryContextStats(TopMemoryContext);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed while creating memory context \"%s\".", name)));
  }

     
                                                                            
                                                         
     

                                                        
  set->blockSize = blockSize;
  set->block = NULL;
  dlist_init(&set->blocks);

                                                                 
  MemoryContextCreate((MemoryContext)set, T_GenerationContext, &GenerationMethods, parent, name);

  return (MemoryContext)set;
}

   
                   
                                                          
   
                                                                           
                                        
   
static void
GenerationReset(MemoryContext context)
{
  GenerationContext *set = (GenerationContext *)context;
  dlist_mutable_iter miter;

  AssertArg(GenerationIsValid(set));

#ifdef MEMORY_CONTEXT_CHECKING
                                                     
  GenerationCheck(context);
#endif

  dlist_foreach_modify(miter, &set->blocks)
  {
    GenerationBlock *block = dlist_container(GenerationBlock, node, miter.cur);

    dlist_delete(miter.cur);

#ifdef CLOBBER_FREED_MEMORY
    wipe_mem(block, block->blksize);
#endif

    free(block);
  }

  set->block = NULL;

  Assert(dlist_is_empty(&set->blocks));
}

   
                    
                                                             
   
static void
GenerationDelete(MemoryContext context)
{
                                                 
  GenerationReset(context);
                                   
  free(context);
}

   
                   
                                                                 
                                                                
   
                          
                                                                            
                                       
   
                                                                            
                                                                         
                                                                           
   
static void *
GenerationAlloc(MemoryContext context, Size size)
{
  GenerationContext *set = (GenerationContext *)context;
  GenerationBlock *block;
  GenerationChunk *chunk;
  Size chunk_size = MAXALIGN(size);

                                                                 
  if (chunk_size > set->blockSize / 8)
  {
    Size blksize = chunk_size + Generation_BLOCKHDRSZ + Generation_CHUNKHDRSZ;

    block = (GenerationBlock *)malloc(blksize);
    if (block == NULL)
    {
      return NULL;
    }

                                          
    block->blksize = blksize;
    block->nchunks = 1;
    block->nfree = 0;

                                      
    block->freeptr = block->endptr = ((char *)block) + blksize;

    chunk = (GenerationChunk *)(((char *)block) + Generation_BLOCKHDRSZ);
    chunk->block = block;
    chunk->context = set;
    chunk->size = chunk_size;

#ifdef MEMORY_CONTEXT_CHECKING
    chunk->requested_size = size;
                                                     
    if (size < chunk_size)
    {
      set_sentinel(GenerationChunkGetPointer(chunk), size);
    }
#endif
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                            
    randomize_mem((char *)GenerationChunkGetPointer(chunk), size);
#endif

                                                       
    dlist_push_head(&set->blocks, &block->node);

    GenerationAllocInfo(set, chunk);

                                                       
    VALGRIND_MAKE_MEM_NOACCESS((char *)GenerationChunkGetPointer(chunk) + size, chunk_size - size);

                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, GENERATIONCHUNK_PRIVATE_LEN);

    return GenerationChunkGetPointer(chunk);
  }

     
                                                                             
                                          
     
  block = set->block;

  if ((block == NULL) || (block->endptr - block->freeptr) < Generation_CHUNKHDRSZ + chunk_size)
  {
    Size blksize = set->blockSize;

    block = (GenerationBlock *)malloc(blksize);

    if (block == NULL)
    {
      return NULL;
    }

    block->blksize = blksize;
    block->nchunks = 0;
    block->nfree = 0;

    block->freeptr = ((char *)block) + Generation_BLOCKHDRSZ;
    block->endptr = ((char *)block) + blksize;

                                          
    VALGRIND_MAKE_MEM_NOACCESS(block->freeptr, blksize - Generation_BLOCKHDRSZ);

                                                    
    dlist_push_head(&set->blocks, &block->node);

                                                         
    set->block = block;
  }

                                                                 
  Assert(block != NULL);
  Assert((block->endptr - block->freeptr) >= Generation_CHUNKHDRSZ + chunk_size);

  chunk = (GenerationChunk *)block->freeptr;

                                               
  VALGRIND_MAKE_MEM_UNDEFINED(chunk, Generation_CHUNKHDRSZ);

  block->nchunks += 1;
  block->freeptr += (Generation_CHUNKHDRSZ + chunk_size);

  Assert(block->freeptr <= block->endptr);

  chunk->block = block;
  chunk->context = set;
  chunk->size = chunk_size;

#ifdef MEMORY_CONTEXT_CHECKING
  chunk->requested_size = size;
                                                   
  if (size < chunk->size)
  {
    set_sentinel(GenerationChunkGetPointer(chunk), size);
  }
#endif
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                          
  randomize_mem((char *)GenerationChunkGetPointer(chunk), size);
#endif

  GenerationAllocInfo(set, chunk);

                                                     
  VALGRIND_MAKE_MEM_NOACCESS((char *)GenerationChunkGetPointer(chunk) + size, chunk_size - size);

                                                                 
  VALGRIND_MAKE_MEM_NOACCESS(chunk, GENERATIONCHUNK_PRIVATE_LEN);

  return GenerationChunkGetPointer(chunk);
}

   
                  
                                                                         
                                         
   
static void
GenerationFree(MemoryContext context, void *pointer)
{
  GenerationContext *set = (GenerationContext *)context;
  GenerationChunk *chunk = GenerationPointerGetChunk(pointer);
  GenerationBlock *block;

                                                     
  VALGRIND_MAKE_MEM_DEFINED(chunk, GENERATIONCHUNK_PRIVATE_LEN);

  block = chunk->block;

#ifdef MEMORY_CONTEXT_CHECKING
                                                            
  if (chunk->requested_size < chunk->size)
  {
    if (!sentinel_ok(pointer, chunk->requested_size))
    {
      elog(WARNING, "detected write past chunk end in %s %p", ((MemoryContext)set)->name, chunk);
    }
  }
#endif

#ifdef CLOBBER_FREED_MEMORY
  wipe_mem(pointer, chunk->size);
#endif

                                             
  chunk->context = NULL;

#ifdef MEMORY_CONTEXT_CHECKING
                                                 
  chunk->requested_size = 0;
#endif

  block->nfree += 1;

  Assert(block->nchunks > 0);
  Assert(block->nfree <= block->nchunks);

                                                                     
  if (block->nfree < block->nchunks)
  {
    return;
  }

     
                                                                          
                                                 
     
  dlist_delete(&block->node);

                                                                    
  if (set->block == block)
  {
    set->block = NULL;
  }

  free(block);
}

   
                     
                                                                          
                                                                          
                                                                   
   
static void *
GenerationRealloc(MemoryContext context, void *pointer, Size size)
{
  GenerationContext *set = (GenerationContext *)context;
  GenerationChunk *chunk = GenerationPointerGetChunk(pointer);
  GenerationPointer newPointer;
  Size oldsize;

                                                     
  VALGRIND_MAKE_MEM_DEFINED(chunk, GENERATIONCHUNK_PRIVATE_LEN);

  oldsize = chunk->size;

#ifdef MEMORY_CONTEXT_CHECKING
                                                            
  if (chunk->requested_size < oldsize)
  {
    if (!sentinel_ok(pointer, chunk->requested_size))
    {
      elog(WARNING, "detected write past chunk end in %s %p", ((MemoryContext)set)->name, chunk);
    }
  }
#endif

     
                                                                           
                                                                   
     
                                                                          
                                                                            
                                                    
     
                                                                    
     
  if (oldsize >= size)
  {
#ifdef MEMORY_CONTEXT_CHECKING
    Size oldrequest = chunk->requested_size;

#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                                                       
    if (size > oldrequest)
    {
      randomize_mem((char *)pointer + oldrequest, size - oldrequest);
    }
#endif

    chunk->requested_size = size;

       
                                                                        
                                                   
       
    if (size > oldrequest)
    {
      VALGRIND_MAKE_MEM_UNDEFINED((char *)pointer + oldrequest, size - oldrequest);
    }
    else
    {
      VALGRIND_MAKE_MEM_NOACCESS((char *)pointer + size, oldsize - size);
    }

                                                     
    if (size < oldsize)
    {
      set_sentinel(pointer, size);
    }
#else                               

       
                                                                        
                                                                      
                                      
       
    VALGRIND_MAKE_MEM_NOACCESS(pointer, oldsize);
    VALGRIND_MAKE_MEM_DEFINED(pointer, size);
#endif

                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, GENERATIONCHUNK_PRIVATE_LEN);

    return pointer;
  }

                          
  newPointer = GenerationAlloc((MemoryContext)set, size);

                                                      
  if (newPointer == NULL)
  {
                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, GENERATIONCHUNK_PRIVATE_LEN);
    return NULL;
  }

     
                                                                          
                                                                        
                                                                         
                                                                            
                                                                        
            
     
  VALGRIND_MAKE_MEM_UNDEFINED(newPointer, size);
#ifdef MEMORY_CONTEXT_CHECKING
  oldsize = chunk->requested_size;
#else
  VALGRIND_MAKE_MEM_DEFINED(pointer, oldsize);
#endif

                                               
  memcpy(newPointer, pointer, oldsize);

                      
  GenerationFree((MemoryContext)set, pointer);

  return newPointer;
}

   
                           
                                                                 
                                                            
   
static Size
GenerationGetChunkSpace(MemoryContext context, void *pointer)
{
  GenerationChunk *chunk = GenerationPointerGetChunk(pointer);
  Size result;

  VALGRIND_MAKE_MEM_DEFINED(chunk, GENERATIONCHUNK_PRIVATE_LEN);
  result = chunk->size + Generation_CHUNKHDRSZ;
  VALGRIND_MAKE_MEM_NOACCESS(chunk, GENERATIONCHUNK_PRIVATE_LEN);
  return result;
}

   
                     
                                                         
   
static bool
GenerationIsEmpty(MemoryContext context)
{
  GenerationContext *set = (GenerationContext *)context;

  return dlist_is_empty(&set->blocks);
}

   
                   
                                                                    
   
                                                                       
                                                     
                                                                   
   
                                                                            
                                             
   
static void
GenerationStats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals)
{
  GenerationContext *set = (GenerationContext *)context;
  Size nblocks = 0;
  Size nchunks = 0;
  Size nfreechunks = 0;
  Size totalspace;
  Size freespace = 0;
  dlist_iter iter;

                                            
  totalspace = MAXALIGN(sizeof(GenerationContext));

  dlist_foreach(iter, &set->blocks)
  {
    GenerationBlock *block = dlist_container(GenerationBlock, node, iter.cur);

    nblocks++;
    nchunks += block->nchunks;
    nfreechunks += block->nfree;
    totalspace += block->blksize;
    freespace += (block->endptr - block->freeptr);
  }

  if (printfunc)
  {
    char stats_string[200];

    snprintf(stats_string, sizeof(stats_string), "%zu total in %zd blocks (%zd chunks); %zu free (%zd chunks); %zu used", totalspace, nblocks, nchunks, freespace, nfreechunks, totalspace - freespace);
    printfunc(context, passthru, stats_string);
  }

  if (totals)
  {
    totals->nblocks += nblocks;
    totals->freechunks += nfreechunks;
    totals->totalspace += totalspace;
    totals->freespace += freespace;
  }
}

#ifdef MEMORY_CONTEXT_CHECKING

   
                   
                                                         
   
                                                                           
                                                                       
                                                                            
   
static void
GenerationCheck(MemoryContext context)
{
  GenerationContext *gen = (GenerationContext *)context;
  const char *name = context->name;
  dlist_iter iter;

                                       
  dlist_foreach(iter, &gen->blocks)
  {
    GenerationBlock *block = dlist_container(GenerationBlock, node, iter.cur);
    int nfree, nchunks;
    char *ptr;

       
                                                                   
                                                                       
       
    if (block->nfree >= block->nchunks)
    {
      elog(WARNING, "problem in Generation %s: number of free chunks %d in block %p exceeds %d allocated", name, block->nfree, block, block->nchunks);
    }

                                                     
    nfree = 0;
    nchunks = 0;
    ptr = ((char *)block) + Generation_BLOCKHDRSZ;

    while (ptr < block->freeptr)
    {
      GenerationChunk *chunk = (GenerationChunk *)ptr;

                                                         
      VALGRIND_MAKE_MEM_DEFINED(chunk, GENERATIONCHUNK_PRIVATE_LEN);

                                  
      ptr += (chunk->size + Generation_CHUNKHDRSZ);

      nchunks += 1;

                                                                      
      if (chunk->block != block)
      {
        elog(WARNING, "problem in Generation %s: bogus block link in block %p, chunk %p", name, block, chunk);
      }

         
                                                                      
                                                                
                              
         
      if ((chunk->requested_size > 0 && chunk->context != gen) || (chunk->context != gen && chunk->context != NULL))
      {
        elog(WARNING, "problem in Generation %s: bogus context link in block %p, chunk %p", name, block, chunk);
      }

                                                   
      if (chunk->size < chunk->requested_size || chunk->size != MAXALIGN(chunk->size))
      {
        elog(WARNING, "problem in Generation %s: bogus chunk size in block %p, chunk %p", name, block, chunk);
      }

                               
      if (chunk->context != NULL)
      {
                                                          
        if (chunk->requested_size < chunk->size && !sentinel_ok(chunk, Generation_CHUNKHDRSZ + chunk->requested_size))
        {
          elog(WARNING, "problem in Generation %s: detected write past chunk end in block %p, chunk %p", name, block, chunk);
        }
      }
      else
      {
        nfree += 1;
      }

         
                                                                         
                          
         
      if (chunk->context != NULL)
      {
        VALGRIND_MAKE_MEM_NOACCESS(chunk, GENERATIONCHUNK_PRIVATE_LEN);
      }
    }

       
                                                                         
                                         
       
    if (nchunks != block->nchunks)
    {
      elog(WARNING, "problem in Generation %s: number of allocated chunks %d in block %p does not match header %d", name, nchunks, block, block->nchunks);
    }

    if (nfree != block->nfree)
    {
      elog(WARNING, "problem in Generation %s: number of free chunks %d in block %p does not match header %d", name, nfree, block, block->nfree);
    }
  }
}

#endif                              
