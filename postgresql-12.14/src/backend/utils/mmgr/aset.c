                                                                            
   
          
                                 
   
                                                                         
         
   
   
                                                                         
                                                                        
   
                  
                                   
   
         
                                                                      
                                                                  
                                                                       
                                                                          
                                                                       
                                                                           
                                                                          
              
                
   
                                                                            
                                                                          
                                                                      
                                                                          
                                                                           
                                                                          
                                      
   
                                                                      
                                                                         
                                                                        
                                                                         
                                                                         
                                                                        
                                                                          
                                                               
   
                                                                            
   

#include "postgres.h"

#include "utils/memdebug.h"
#include "utils/memutils.h"

                                                   
                            

                       
                                                                   
                                          
   
                                                                      
                                                                         
                                                                  
   
                                                                        
                                                                          
                                                                          
                                             
   
                                                       
                                          
                                                           
                                                               
   
                                                                              
                                                                            
                                                                           
                                                                            
                                                                           
                       
   

#define ALLOC_MINBITS 3                                     
#define ALLOCSET_NUM_FREELISTS 11
#define ALLOC_CHUNK_LIMIT (1 << (ALLOCSET_NUM_FREELISTS - 1 + ALLOC_MINBITS))
                                                        
#define ALLOC_CHUNK_FRACTION 4
                                                                       

                       
                                                                     
                                                                         
                                                                      
                                     
   
                                                                              
                                                                        
   
                                                                          
                                                    
                       
   

#define ALLOC_BLOCKHDRSZ MAXALIGN(sizeof(AllocBlockData))
#define ALLOC_CHUNKHDRSZ sizeof(struct AllocChunkData)

typedef struct AllocBlockData *AllocBlock;                        
typedef struct AllocChunkData *AllocChunk;

   
                
                                                                
   
typedef void *AllocPointer;

   
                                                                    
   
                                                                        
                                                                              
                                                                                
                                                                           
                      
   
typedef struct AllocSetContext
{
  MemoryContextData header;                                     
                                                     
  AllocBlock blocks;                                                                   
  AllocChunk freelist[ALLOCSET_NUM_FREELISTS];                       
                                               
  Size initBlockSize;                           
  Size maxBlockSize;                            
  Size nextBlockSize;                                    
  Size allocChunkLimit;                                 
  AllocBlock keeper;                                     
                                                                        
  int freeListIndex;                                          
} AllocSetContext;

typedef AllocSetContext *AllocSet;

   
              
                                                                   
                                                                   
                                                                       
                                                                      
                                                                      
                             
   
                                                                       
                                                            
   
typedef struct AllocBlockData
{
  AllocSet aset;                                  
  AllocBlock prev;                                               
  AllocBlock next;                                               
  char *freeptr;                                          
  char *endptr;                                    
} AllocBlockData;

   
              
                                                        
   
                                                                             
                                                                          
                                                                           
                                                                         
                                                                             
                                                                           
                      
   
typedef struct AllocChunkData
{
                                                                
  Size size;
#ifdef MEMORY_CONTEXT_CHECKING
                                                                     
                                    
  Size requested_size;

#define ALLOCCHUNK_RAWSIZE (SIZEOF_SIZE_T * 2 + SIZEOF_VOID_P)
#else
#define ALLOCCHUNK_RAWSIZE (SIZEOF_SIZE_T + SIZEOF_VOID_P)
#endif                              

                                                           
#if (ALLOCCHUNK_RAWSIZE % MAXIMUM_ALIGNOF) != 0
  char padding[MAXIMUM_ALIGNOF - ALLOCCHUNK_RAWSIZE % MAXIMUM_ALIGNOF];
#endif

                                                                          
  void *aset;
                                                                        
} AllocChunkData;

   
                                                                 
                                                                              
                                                                          
                               
   
#define ALLOCCHUNK_PRIVATE_LEN offsetof(AllocChunkData, aset)

   
                       
                                                  
   
#define AllocPointerIsValid(pointer) PointerIsValid(pointer)

   
                   
                                          
   
#define AllocSetIsValid(set) PointerIsValid(set)

#define AllocPointerGetChunk(ptr) ((AllocChunk)(((char *)(ptr)) - ALLOC_CHUNKHDRSZ))
#define AllocChunkGetPointer(chk) ((AllocPointer)(((char *)(chk)) + ALLOC_CHUNKHDRSZ))

   
                                                                              
                                                                              
                                                                             
                                                                         
                                                                          
                                                                             
                                                 
   
                                                                         
                                                                   
                                                                        
   
                                                                          
                                                                         
                                                                            
                                                                
                                                                             
                                                                             
                                                                        
   
                                                                    
   
#define MAX_FREE_CONTEXTS 100                                         

typedef struct AllocSetFreeList
{
  int num_free;                                         
  AllocSetContext *first_free;                  
} AllocSetFreeList;

                                                                      
static AllocSetFreeList context_freelists[2] = {{0, NULL}, {0, NULL}};

   
                                                                          
   
static void *
AllocSetAlloc(MemoryContext context, Size size);
static void
AllocSetFree(MemoryContext context, void *pointer);
static void *
AllocSetRealloc(MemoryContext context, void *pointer, Size size);
static void
AllocSetReset(MemoryContext context);
static void
AllocSetDelete(MemoryContext context);
static Size
AllocSetGetChunkSpace(MemoryContext context, void *pointer);
static bool
AllocSetIsEmpty(MemoryContext context);
static void
AllocSetStats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals);

#ifdef MEMORY_CONTEXT_CHECKING
static void
AllocSetCheck(MemoryContext context);
#endif

   
                                                             
   
static const MemoryContextMethods AllocSetMethods = {AllocSetAlloc, AllocSetFree, AllocSetRealloc, AllocSetReset, AllocSetDelete, AllocSetGetChunkSpace, AllocSetIsEmpty, AllocSetStats
#ifdef MEMORY_CONTEXT_CHECKING
    ,
    AllocSetCheck
#endif
};

   
                               
   
#define LT16(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n

static const unsigned char LogTable256[256] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, LT16(5), LT16(6), LT16(6), LT16(7), LT16(7), LT16(7), LT16(7), LT16(8), LT16(8), LT16(8), LT16(8), LT16(8), LT16(8), LT16(8), LT16(8)};

              
                
              
   
#ifdef HAVE_ALLOCINFO
#define AllocFreeInfo(_cxt, _chunk) fprintf(stderr, "AllocFree: %s: %p, %zu\n", (_cxt)->header.name, (_chunk), (_chunk)->size)
#define AllocAllocInfo(_cxt, _chunk) fprintf(stderr, "AllocAlloc: %s: %p, %zu\n", (_cxt)->header.name, (_chunk), (_chunk)->size)
#else
#define AllocFreeInfo(_cxt, _chunk)
#define AllocAllocInfo(_cxt, _chunk)
#endif

              
                       
   
                                                                   
                                                                    
                                    
              
   
static inline int
AllocSetFreeIndex(Size size)
{
  int idx;
  unsigned int t, tsize;

  if (size > (1 << ALLOC_MINBITS))
  {
    tsize = (size - 1) >> ALLOC_MINBITS;

       
                                                                        
                                                                  
                                                                         
                                                                         
                                                                
                                                                          
                        
       
    t = tsize >> 8;
    idx = t ? LogTable256[t] + 8 : LogTable256[tsize];

    Assert(idx < ALLOCSET_NUM_FREELISTS);
  }
  else
  {
    idx = 0;
  }

  return idx;
}

   
                   
   

   
                                 
                                   
   
                                                        
                                                        
                                        
                                                
                                               
   
                                                                          
                                   
   
                                                                
                          
   
MemoryContext
AllocSetContextCreateInternal(MemoryContext parent, const char *name, Size minContextSize, Size initBlockSize, Size maxBlockSize)
{
  int freeListIndex;
  Size firstBlockSize;
  AllocSet set;
  AllocBlock block;

                                                
  StaticAssertStmt(ALLOC_CHUNKHDRSZ == MAXALIGN(ALLOC_CHUNKHDRSZ), "sizeof(AllocChunkData) is not maxaligned");
  StaticAssertStmt(offsetof(AllocChunkData, aset) + sizeof(MemoryContext) == ALLOC_CHUNKHDRSZ, "padding calculation in AllocChunkData is wrong");

     
                                                                             
                                                                             
                                                                            
                            
     
  Assert(initBlockSize == MAXALIGN(initBlockSize) && initBlockSize >= 1024);
  Assert(maxBlockSize == MAXALIGN(maxBlockSize) && maxBlockSize >= initBlockSize && AllocHugeSizeIsValid(maxBlockSize));                             
  Assert(minContextSize == 0 || (minContextSize == MAXALIGN(minContextSize) && minContextSize >= 1024 && minContextSize <= maxBlockSize));

     
                                                                          
                                                 
     
  if (minContextSize == ALLOCSET_DEFAULT_MINSIZE && initBlockSize == ALLOCSET_DEFAULT_INITSIZE)
  {
    freeListIndex = 0;
  }
  else if (minContextSize == ALLOCSET_SMALL_MINSIZE && initBlockSize == ALLOCSET_SMALL_INITSIZE)
  {
    freeListIndex = 1;
  }
  else
  {
    freeListIndex = -1;
  }

     
                                                                     
     
  if (freeListIndex >= 0)
  {
    AllocSetFreeList *freelist = &context_freelists[freeListIndex];

    if (freelist->first_free != NULL)
    {
                                      
      set = freelist->first_free;
      freelist->first_free = (AllocSet)set->header.nextchild;
      freelist->num_free--;

                                                                 
      set->maxBlockSize = maxBlockSize;

                                                                       
      MemoryContextCreate((MemoryContext)set, T_AllocSetContext, &AllocSetMethods, parent, name);

      return (MemoryContext)set;
    }
  }

                                       
  firstBlockSize = MAXALIGN(sizeof(AllocSetContext)) + ALLOC_BLOCKHDRSZ + ALLOC_CHUNKHDRSZ;
  if (minContextSize != 0)
  {
    firstBlockSize = Max(firstBlockSize, minContextSize);
  }
  else
  {
    firstBlockSize = Max(firstBlockSize, initBlockSize);
  }

     
                                                                             
                                                           
     
  set = (AllocSet)malloc(firstBlockSize);
  if (set == NULL)
  {
    if (TopMemoryContext)
    {
      MemoryContextStats(TopMemoryContext);
    }
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed while creating memory context \"%s\".", name)));
  }

     
                                                                            
                                                                       
     

                                                
  block = (AllocBlock)(((char *)set) + MAXALIGN(sizeof(AllocSetContext)));
  block->aset = set;
  block->freeptr = ((char *)block) + ALLOC_BLOCKHDRSZ;
  block->endptr = ((char *)set) + firstBlockSize;
  block->prev = NULL;
  block->next = NULL;

                                                                      
  VALGRIND_MAKE_MEM_NOACCESS(block->freeptr, block->endptr - block->freeptr);

                                            
  set->blocks = block;
                                                      
  set->keeper = block;

                                                                   
  MemSetAligned(set->freelist, 0, sizeof(set->freelist));

  set->initBlockSize = initBlockSize;
  set->maxBlockSize = maxBlockSize;
  set->nextBlockSize = initBlockSize;
  set->freeListIndex = freeListIndex;

     
                                                                            
                                                                           
                                                                           
                                                                          
                                                                             
                                                                         
                                                                        
                                   
     
                                                                           
                                                                           
                                                                       
     
                                                                        
     
  StaticAssertStmt(ALLOC_CHUNK_LIMIT == ALLOCSET_SEPARATE_THRESHOLD, "ALLOC_CHUNK_LIMIT != ALLOCSET_SEPARATE_THRESHOLD");

  set->allocChunkLimit = ALLOC_CHUNK_LIMIT;
  while ((Size)(set->allocChunkLimit + ALLOC_CHUNKHDRSZ) > (Size)((maxBlockSize - ALLOC_BLOCKHDRSZ) / ALLOC_CHUNK_FRACTION))
  {
    set->allocChunkLimit >>= 1;
  }

                                                                 
  MemoryContextCreate((MemoryContext)set, T_AllocSetContext, &AllocSetMethods, parent, name);

  return (MemoryContext)set;
}

   
                 
                                                          
   
                                                                
                                                                          
                                                                           
                                                                           
                                                                             
                                                                               
                                                     
   
static void
AllocSetReset(MemoryContext context)
{
  AllocSet set = (AllocSet)context;
  AllocBlock block;

  AssertArg(AllocSetIsValid(set));

#ifdef MEMORY_CONTEXT_CHECKING
                                                     
  AllocSetCheck(context);
#endif

                             
  MemSetAligned(set->freelist, 0, sizeof(set->freelist));

  block = set->blocks;

                                                     
  set->blocks = set->keeper;

  while (block != NULL)
  {
    AllocBlock next = block->next;

    if (block == set->keeper)
    {
                                                          
      char *datastart = ((char *)block) + ALLOC_BLOCKHDRSZ;

#ifdef CLOBBER_FREED_MEMORY
      wipe_mem(datastart, block->freeptr - datastart);
#else
                                           
      VALGRIND_MAKE_MEM_NOACCESS(datastart, block->freeptr - datastart);
#endif
      block->freeptr = datastart;
      block->prev = NULL;
      block->next = NULL;
    }
    else
    {
                                          
#ifdef CLOBBER_FREED_MEMORY
      wipe_mem(block, block->freeptr - ((char *)block));
#endif
      free(block);
    }
    block = next;
  }

                                                 
  set->nextBlockSize = set->initBlockSize;
}

   
                  
                                                          
                                            
   
                                                                    
   
static void
AllocSetDelete(MemoryContext context)
{
  AllocSet set = (AllocSet)context;
  AllocBlock block = set->blocks;

  AssertArg(AllocSetIsValid(set));

#ifdef MEMORY_CONTEXT_CHECKING
                                                     
  AllocSetCheck(context);
#endif

     
                                                                             
                               
     
  if (set->freeListIndex >= 0)
  {
    AllocSetFreeList *freelist = &context_freelists[set->freeListIndex];

       
                                                                          
                                           
       
    if (!context->isReset)
    {
      MemoryContextResetOnly(context);
    }

       
                                                                        
                                          
       
    if (freelist->num_free >= MAX_FREE_CONTEXTS)
    {
      while (freelist->first_free != NULL)
      {
        AllocSetContext *oldset = freelist->first_free;

        freelist->first_free = (AllocSetContext *)oldset->header.nextchild;
        freelist->num_free--;

                                                                  
        free(oldset);
      }
      Assert(freelist->num_free == 0);
    }

                                                           
    set->header.nextchild = (MemoryContext)freelist->first_free;
    freelist->first_free = set;
    freelist->num_free++;

    return;
  }

                                                                          
  while (block != NULL)
  {
    AllocBlock next = block->next;

#ifdef CLOBBER_FREED_MEMORY
    wipe_mem(block, block->freeptr - ((char *)block));
#endif

    if (block != set->keeper)
    {
      free(block);
    }

    block = next;
  }

                                                                    
  free(set);
}

   
                 
                                                                 
                                                                
   
                          
                                                                  
                                       
   
                                                                            
                                                                         
                                                                         
   
static void *
AllocSetAlloc(MemoryContext context, Size size)
{
  AllocSet set = (AllocSet)context;
  AllocBlock block;
  AllocChunk chunk;
  int fidx;
  Size chunk_size;
  Size blksize;

  AssertArg(AllocSetIsValid(set));

     
                                                                            
                       
     
  if (size > set->allocChunkLimit)
  {
    chunk_size = MAXALIGN(size);
    blksize = chunk_size + ALLOC_BLOCKHDRSZ + ALLOC_CHUNKHDRSZ;
    block = (AllocBlock)malloc(blksize);
    if (block == NULL)
    {
      return NULL;
    }
    block->aset = set;
    block->freeptr = block->endptr = ((char *)block) + blksize;

    chunk = (AllocChunk)(((char *)block) + ALLOC_BLOCKHDRSZ);
    chunk->aset = set;
    chunk->size = chunk_size;
#ifdef MEMORY_CONTEXT_CHECKING
    chunk->requested_size = size;
                                                     
    if (size < chunk_size)
    {
      set_sentinel(AllocChunkGetPointer(chunk), size);
    }
#endif
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                            
    randomize_mem((char *)AllocChunkGetPointer(chunk), size);
#endif

       
                                                                           
                                                                     
       
    if (set->blocks != NULL)
    {
      block->prev = set->blocks;
      block->next = set->blocks->next;
      if (block->next)
      {
        block->next->prev = block;
      }
      set->blocks->next = block;
    }
    else
    {
      block->prev = NULL;
      block->next = NULL;
      set->blocks = block;
    }

    AllocAllocInfo(set, chunk);

                                                       
    VALGRIND_MAKE_MEM_NOACCESS((char *)AllocChunkGetPointer(chunk) + size, chunk_size - size);

                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);

    return AllocChunkGetPointer(chunk);
  }

     
                                                                    
                                                                             
                                                                           
                                                   
     
  fidx = AllocSetFreeIndex(size);
  chunk = set->freelist[fidx];
  if (chunk != NULL)
  {
    Assert(chunk->size >= size);

    set->freelist[fidx] = (AllocChunk)chunk->aset;

    chunk->aset = (void *)set;

#ifdef MEMORY_CONTEXT_CHECKING
    chunk->requested_size = size;
                                                     
    if (size < chunk->size)
    {
      set_sentinel(AllocChunkGetPointer(chunk), size);
    }
#endif
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                            
    randomize_mem((char *)AllocChunkGetPointer(chunk), size);
#endif

    AllocAllocInfo(set, chunk);

                                                       
    VALGRIND_MAKE_MEM_NOACCESS((char *)AllocChunkGetPointer(chunk) + size, chunk->size - size);

                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);

    return AllocChunkGetPointer(chunk);
  }

     
                                               
     
  chunk_size = (1 << ALLOC_MINBITS) << fidx;
  Assert(chunk_size >= size);

     
                                                                             
                                                        
     
  if ((block = set->blocks) != NULL)
  {
    Size availspace = block->endptr - block->freeptr;

    if (availspace < (chunk_size + ALLOC_CHUNKHDRSZ))
    {
         
                                                                       
                                                                    
                                                                         
                                                                       
                                                                         
                              
         
                                                             
                                                                       
                                                   
         
      while (availspace >= ((1 << ALLOC_MINBITS) + ALLOC_CHUNKHDRSZ))
      {
        Size availchunk = availspace - ALLOC_CHUNKHDRSZ;
        int a_fidx = AllocSetFreeIndex(availchunk);

           
                                                                      
                                                                    
                                                                 
           
        if (availchunk != ((Size)1 << (a_fidx + ALLOC_MINBITS)))
        {
          a_fidx--;
          Assert(a_fidx >= 0);
          availchunk = ((Size)1 << (a_fidx + ALLOC_MINBITS));
        }

        chunk = (AllocChunk)(block->freeptr);

                                                     
        VALGRIND_MAKE_MEM_UNDEFINED(chunk, ALLOC_CHUNKHDRSZ);

        block->freeptr += (availchunk + ALLOC_CHUNKHDRSZ);
        availspace -= (availchunk + ALLOC_CHUNKHDRSZ);

        chunk->size = availchunk;
#ifdef MEMORY_CONTEXT_CHECKING
        chunk->requested_size = 0;                   
#endif
        chunk->aset = (void *)set->freelist[a_fidx];
        set->freelist[a_fidx] = chunk;
      }

                                                   
      block = NULL;
    }
  }

     
                                                       
     
  if (block == NULL)
  {
    Size required_size;

       
                                                                      
                                                                       
       
    blksize = set->nextBlockSize;
    set->nextBlockSize <<= 1;
    if (set->nextBlockSize > set->maxBlockSize)
    {
      set->nextBlockSize = set->maxBlockSize;
    }

       
                                                                           
                                                 
       
    required_size = chunk_size + ALLOC_BLOCKHDRSZ + ALLOC_CHUNKHDRSZ;
    while (blksize < required_size)
    {
      blksize <<= 1;
    }

                            
    block = (AllocBlock)malloc(blksize);

       
                                                                        
                                                                         
       
    while (block == NULL && blksize > 1024 * 1024)
    {
      blksize >>= 1;
      if (blksize < required_size)
      {
        break;
      }
      block = (AllocBlock)malloc(blksize);
    }

    if (block == NULL)
    {
      return NULL;
    }

    block->aset = set;
    block->freeptr = ((char *)block) + ALLOC_BLOCKHDRSZ;
    block->endptr = ((char *)block) + blksize;

                                          
    VALGRIND_MAKE_MEM_NOACCESS(block->freeptr, blksize - ALLOC_BLOCKHDRSZ);

    block->prev = NULL;
    block->next = set->blocks;
    if (block->next)
    {
      block->next->prev = block;
    }
    set->blocks = block;
  }

     
                           
     
  chunk = (AllocChunk)(block->freeptr);

                                               
  VALGRIND_MAKE_MEM_UNDEFINED(chunk, ALLOC_CHUNKHDRSZ);

  block->freeptr += (chunk_size + ALLOC_CHUNKHDRSZ);
  Assert(block->freeptr <= block->endptr);

  chunk->aset = (void *)set;
  chunk->size = chunk_size;
#ifdef MEMORY_CONTEXT_CHECKING
  chunk->requested_size = size;
                                                   
  if (size < chunk->size)
  {
    set_sentinel(AllocChunkGetPointer(chunk), size);
  }
#endif
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                          
  randomize_mem((char *)AllocChunkGetPointer(chunk), size);
#endif

  AllocAllocInfo(set, chunk);

                                                     
  VALGRIND_MAKE_MEM_NOACCESS((char *)AllocChunkGetPointer(chunk) + size, chunk_size - size);

                                                                 
  VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);

  return AllocChunkGetPointer(chunk);
}

   
                
                                                            
   
static void
AllocSetFree(MemoryContext context, void *pointer)
{
  AllocSet set = (AllocSet)context;
  AllocChunk chunk = AllocPointerGetChunk(pointer);

                                                     
  VALGRIND_MAKE_MEM_DEFINED(chunk, ALLOCCHUNK_PRIVATE_LEN);

  AllocFreeInfo(set, chunk);

#ifdef MEMORY_CONTEXT_CHECKING
                                                            
  if (chunk->requested_size < chunk->size)
  {
    if (!sentinel_ok(pointer, chunk->requested_size))
    {
      elog(WARNING, "detected write past chunk end in %s %p", set->header.name, chunk);
    }
  }
#endif

  if (chunk->size > set->allocChunkLimit)
  {
       
                                                                     
                                                                  
       
    AllocBlock block = (AllocBlock)(((char *)chunk) - ALLOC_BLOCKHDRSZ);

       
                                                                  
                                                                       
                            
       
    if (block->aset != set || block->freeptr != block->endptr || block->freeptr != ((char *)block) + (chunk->size + ALLOC_BLOCKHDRSZ + ALLOC_CHUNKHDRSZ))
    {
      elog(ERROR, "could not find block containing chunk %p", chunk);
    }

                                                       
    if (block->prev)
    {
      block->prev->next = block->next;
    }
    else
    {
      set->blocks = block->next;
    }
    if (block->next)
    {
      block->next->prev = block->prev;
    }
#ifdef CLOBBER_FREED_MEMORY
    wipe_mem(block, block->freeptr - ((char *)block));
#endif
    free(block);
  }
  else
  {
                                                              
    int fidx = AllocSetFreeIndex(chunk->size);

    chunk->aset = (void *)set->freelist[fidx];

#ifdef CLOBBER_FREED_MEMORY
    wipe_mem(pointer, chunk->size);
#endif

#ifdef MEMORY_CONTEXT_CHECKING
                                                                  
    chunk->requested_size = 0;
#endif
    set->freelist[fidx] = chunk;
  }
}

   
                   
                                                                     
                                                                     
                                                                        
                                 
   
                                                                              
                                                                               
                                                                          
                  
   
static void *
AllocSetRealloc(MemoryContext context, void *pointer, Size size)
{
  AllocSet set = (AllocSet)context;
  AllocChunk chunk = AllocPointerGetChunk(pointer);
  Size oldsize;

                                                     
  VALGRIND_MAKE_MEM_DEFINED(chunk, ALLOCCHUNK_PRIVATE_LEN);

  oldsize = chunk->size;

#ifdef MEMORY_CONTEXT_CHECKING
                                                            
  if (chunk->requested_size < oldsize)
  {
    if (!sentinel_ok(pointer, chunk->requested_size))
    {
      elog(WARNING, "detected write past chunk end in %s %p", set->header.name, chunk);
    }
  }
#endif

  if (oldsize > set->allocChunkLimit)
  {
       
                                                                        
                                                                       
                              
       
    AllocBlock block = (AllocBlock)(((char *)chunk) - ALLOC_BLOCKHDRSZ);
    Size chksize;
    Size blksize;

       
                                                                  
                                                                       
                            
       
    if (block->aset != set || block->freeptr != block->endptr || block->freeptr != ((char *)block) + (oldsize + ALLOC_BLOCKHDRSZ + ALLOC_CHUNKHDRSZ))
    {
      elog(ERROR, "could not find block containing chunk %p", chunk);
    }

       
                                                                           
                                                                
                                                                           
                                                          
       
    chksize = Max(size, set->allocChunkLimit + 1);
    chksize = MAXALIGN(chksize);

                        
    blksize = chksize + ALLOC_BLOCKHDRSZ + ALLOC_CHUNKHDRSZ;
    block = (AllocBlock)realloc(block, blksize);
    if (block == NULL)
    {
                                                                     
      VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);
      return NULL;
    }
    block->freeptr = block->endptr = ((char *)block) + blksize;

                                                           
    chunk = (AllocChunk)(((char *)block) + ALLOC_BLOCKHDRSZ);
    pointer = AllocChunkGetPointer(chunk);
    if (block->prev)
    {
      block->prev->next = block;
    }
    else
    {
      set->blocks = block;
    }
    if (block->next)
    {
      block->next->prev = block;
    }
    chunk->size = chksize;

#ifdef MEMORY_CONTEXT_CHECKING
#ifdef RANDOMIZE_ALLOCATED_MEMORY
                                                                       
    if (size > chunk->requested_size)
    {
      randomize_mem((char *)pointer + chunk->requested_size, size - chunk->requested_size);
    }
#endif

       
                                                                         
                                                                         
                       
       
#ifdef USE_VALGRIND
    if (oldsize > chunk->requested_size)
    {
      VALGRIND_MAKE_MEM_UNDEFINED((char *)pointer + chunk->requested_size, oldsize - chunk->requested_size);
    }
#endif

    chunk->requested_size = size;

                                                     
    if (size < chunk->size)
    {
      set_sentinel(pointer, size);
    }
#else                               

       
                                                                   
                                                                           
                                                                  
       
    VALGRIND_MAKE_MEM_DEFINED(pointer, oldsize);
#endif

                                                       
    VALGRIND_MAKE_MEM_NOACCESS((char *)pointer + size, chksize - size);

                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);

    return pointer;
  }

     
                                                                          
                                                                         
                                                         
     
  else if (oldsize >= size)
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

                                                                   
    VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);

    return pointer;
  }
  else
  {
       
                                                                        
                                                                           
                                                                        
                                                                     
                                                                           
                                                                  
                                                                      
                                                                          
                                                                         
       
    AllocPointer newPointer;

                            
    newPointer = AllocSetAlloc((MemoryContext)set, size);

                                                        
    if (newPointer == NULL)
    {
                                                                     
      VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);
      return NULL;
    }

       
                                                                          
                                                                          
                                                                           
                                                                        
                                                                       
                       
       
    VALGRIND_MAKE_MEM_UNDEFINED(newPointer, size);
#ifdef MEMORY_CONTEXT_CHECKING
    oldsize = chunk->requested_size;
#else
    VALGRIND_MAKE_MEM_DEFINED(pointer, oldsize);
#endif

                                                 
    memcpy(newPointer, pointer, oldsize);

                        
    AllocSetFree((MemoryContext)set, pointer);

    return newPointer;
  }
}

   
                         
                                                                 
                                                            
   
static Size
AllocSetGetChunkSpace(MemoryContext context, void *pointer)
{
  AllocChunk chunk = AllocPointerGetChunk(pointer);
  Size result;

  VALGRIND_MAKE_MEM_DEFINED(chunk, ALLOCCHUNK_PRIVATE_LEN);
  result = chunk->size + ALLOC_CHUNKHDRSZ;
  VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);
  return result;
}

   
                   
                                                 
   
static bool
AllocSetIsEmpty(MemoryContext context)
{
     
                                                                          
                                                                           
                                                                    
                    
     
  if (context->isReset)
  {
    return true;
  }
  return false;
}

   
                 
                                                           
   
                                                                       
                                                     
                                                                   
   
static void
AllocSetStats(MemoryContext context, MemoryStatsPrintFunc printfunc, void *passthru, MemoryContextCounters *totals)
{
  AllocSet set = (AllocSet)context;
  Size nblocks = 0;
  Size freechunks = 0;
  Size totalspace;
  Size freespace = 0;
  AllocBlock block;
  int fidx;

                                            
  totalspace = MAXALIGN(sizeof(AllocSetContext));

  for (block = set->blocks; block != NULL; block = block->next)
  {
    nblocks++;
    totalspace += block->endptr - ((char *)block);
    freespace += block->endptr - block->freeptr;
  }
  for (fidx = 0; fidx < ALLOCSET_NUM_FREELISTS; fidx++)
  {
    AllocChunk chunk;

    for (chunk = set->freelist[fidx]; chunk != NULL; chunk = (AllocChunk)chunk->aset)
    {
      freechunks++;
      freespace += chunk->size + ALLOC_CHUNKHDRSZ;
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
AllocSetCheck(MemoryContext context)
{
  AllocSet set = (AllocSet)context;
  const char *name = set->header.name;
  AllocBlock prevblock;
  AllocBlock block;

  for (prevblock = NULL, block = set->blocks; block != NULL; prevblock = block, block = block->next)
  {
    char *bpoz = ((char *)block) + ALLOC_BLOCKHDRSZ;
    long blk_used = block->freeptr - bpoz;
    long blk_data = 0;
    long nchunks = 0;

       
                                                    
       
    if (!blk_used)
    {
      if (set->keeper != block)
      {
        elog(WARNING, "problem in alloc set %s: empty block %p", name, block);
      }
    }

       
                                 
       
    if (block->aset != set || block->prev != prevblock || block->freeptr < bpoz || block->freeptr > block->endptr)
    {
      elog(WARNING, "problem in alloc set %s: corrupt header in block %p", name, block);
    }

       
                    
       
    while (bpoz < block->freeptr)
    {
      AllocChunk chunk = (AllocChunk)bpoz;
      Size chsize, dsize;

                                                         
      VALGRIND_MAKE_MEM_DEFINED(chunk, ALLOCCHUNK_PRIVATE_LEN);

      chsize = chunk->size;                                  
      dsize = chunk->requested_size;                

         
                          
         
      if (dsize > chsize)
      {
        elog(WARNING, "problem in alloc set %s: req size > alloc size for chunk %p in block %p", name, chunk, block);
      }
      if (chsize < (1 << ALLOC_MINBITS))
      {
        elog(WARNING, "problem in alloc set %s: bad size %zu for chunk %p in block %p", name, chsize, chunk, block);
      }

                               
      if (chsize > set->allocChunkLimit && chsize + ALLOC_CHUNKHDRSZ != blk_used)
      {
        elog(WARNING, "problem in alloc set %s: bad single-chunk %p in block %p", name, chunk, block);
      }

         
                                                                         
                                                                         
                                                                      
                                                               
         
      if (dsize > 0 && chunk->aset != (void *)set)
      {
        elog(WARNING, "problem in alloc set %s: bogus aset link in block %p, chunk %p", name, block, chunk);
      }

         
                                                                     
         
      if (chunk->aset == (void *)set && dsize < chsize && !sentinel_ok(chunk, ALLOC_CHUNKHDRSZ + dsize))
      {
        elog(WARNING, "problem in alloc set %s: detected write past chunk end in block %p, chunk %p", name, block, chunk);
      }

         
                                                                         
                          
         
      if (chunk->aset == (void *)set)
      {
        VALGRIND_MAKE_MEM_NOACCESS(chunk, ALLOCCHUNK_PRIVATE_LEN);
      }

      blk_data += chsize;
      nchunks++;

      bpoz += ALLOC_CHUNKHDRSZ + chsize;
    }

    if ((blk_data + (nchunks * ALLOC_CHUNKHDRSZ)) != blk_used)
    {
      elog(WARNING, "problem in alloc set %s: found inconsistent memory block %p", name, block);
    }
  }
}

#endif                              
