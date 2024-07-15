                                                                            
   
           
                                                                        
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
   
                                                                  
                                                                 
                                                                        
                                                                    
                                             
   
          
                                                               
                                                                 
                                                                       
                                                                      
                                                                     
                                                                   
                                                                       
                                                                           
                                                                      
                                                 
   
                                                         
                                                                    
                                                                 
                                                                   
                                                                    
                               
                                                         
                                                                
                                                             
                                                                   
                                                                 
                                                                 
                                                                
                                                               
                                                  
   
                                                                      
                                                                         
                                                                      
                                                                      
                                                                        
                                                     
   
                                                            
                                                                   
                                                                    
                                                                  
                                                                     
                                                                  
                
   

#include "postgres.h"

#include "access/transam.h"
#include "miscadmin.h"
#include "storage/lwlock.h"
#include "storage/pg_shmem.h"
#include "storage/shmem.h"
#include "storage/spin.h"

                                    

static PGShmemHeader *ShmemSegHdr;                                

static void *ShmemBase;                                     

static void *ShmemEnd;                                     

slock_t *ShmemLock;                                          
                                    

static HTAB *ShmemIndex = NULL;                                        

   
                                                                 
   
                                                                  
                                                                
   
void
InitShmemAccess(void *seghdr)
{
  PGShmemHeader *shmhdr = (PGShmemHeader *)seghdr;

  ShmemSegHdr = shmhdr;
  ShmemBase = (void *)shmhdr;
  ShmemEnd = (char *)ShmemBase + shmhdr->totalsize;
}

   
                                                                    
   
                                                                         
   
void
InitShmemAllocation(void)
{
  PGShmemHeader *shmhdr = ShmemSegHdr;
  char *aligned;

  Assert(shmhdr != NULL);

     
                                                              
                                                                         
     
  ShmemLock = (slock_t *)ShmemAllocUnlocked(sizeof(slock_t));

  SpinLockInit(ShmemLock);

     
                                                                      
                                                                             
                                                       
     
  aligned = (char *)(CACHELINEALIGN((((char *)shmhdr) + shmhdr->freeoffset)));
  shmhdr->freeoffset = aligned - (char *)shmhdr;

                                                           
  shmhdr->index = NULL;
  ShmemIndex = (HTAB *)NULL;

     
                                                                          
                                                
     
  ShmemVariableCache = (VariableCache)ShmemAlloc(sizeof(*ShmemVariableCache));
  memset(ShmemVariableCache, 0, sizeof(*ShmemVariableCache));
}

   
                                                               
   
                                                
   
                                                      
   
void *
ShmemAlloc(Size size)
{
  void *newSpace;

  newSpace = ShmemAllocNoError(size);
  if (!newSpace)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory (%zu bytes requested)", size)));
  }
  return newSpace;
}

   
                                                                      
   
                                                                          
   
void *
ShmemAllocNoError(Size size)
{
  Size newStart;
  Size newFree;
  void *newSpace;

     
                                                                            
                                                                             
                                                                           
                                                                            
                                                                      
                                                                            
                                                                            
                                                                         
                          
     
  size = CACHELINEALIGN(size);

  Assert(ShmemSegHdr != NULL);

  SpinLockAcquire(ShmemLock);

  newStart = ShmemSegHdr->freeoffset;

  newFree = newStart + size;
  if (newFree <= ShmemSegHdr->totalsize)
  {
    newSpace = (void *)((char *)ShmemBase + newStart);
    ShmemSegHdr->freeoffset = newFree;
  }
  else
  {
    newSpace = NULL;
  }

  SpinLockRelease(ShmemLock);

                                                      
  Assert(newSpace == (void *)CACHELINEALIGN(newSpace));

  return newSpace;
}

   
                                                                       
   
                                                                       
                                                                         
   
                                                                  
   
void *
ShmemAllocUnlocked(Size size)
{
  Size newStart;
  Size newFree;
  void *newSpace;

     
                                                   
     
  size = MAXALIGN(size);

  Assert(ShmemSegHdr != NULL);

  newStart = ShmemSegHdr->freeoffset;

  newFree = newStart + size;
  if (newFree > ShmemSegHdr->totalsize)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory (%zu bytes requested)", size)));
  }
  ShmemSegHdr->freeoffset = newFree;

  newSpace = (void *)((char *)ShmemBase + newStart);

  Assert(newSpace == (void *)MAXALIGN(newSpace));

  return newSpace;
}

   
                                                                  
   
                                                                        
   
bool
ShmemAddrIsValid(const void *addr)
{
  return (addr >= ShmemBase) && (addr < ShmemEnd);
}

   
                                                               
   
void
InitShmemIndex(void)
{
  HASHCTL info;
  int hash_flags;

     
                                           
     
                                                                             
                                                                           
                                                                        
                                                      
     
  info.keysize = SHMEM_INDEX_KEYSIZE;
  info.entrysize = sizeof(ShmemIndexEnt);
  hash_flags = HASH_ELEM;

  ShmemIndex = ShmemInitHash("ShmemIndex", SHMEM_INDEX_SIZE, SHMEM_INDEX_SIZE, &info, hash_flags);
}

   
                                                           
                              
   
                                                          
                                                                 
                                                                          
                                                                            
   
                                                                           
                                                                     
                                                                         
                                              
   
                                                                             
                                                                         
                                                                     
   
                                                                           
                                                                          
             
   
HTAB *
ShmemInitHash(const char *name,                                        
    long init_size,                                     
    long max_size,                                         
    HASHCTL *infoP,                                                 
    int hash_flags)                                   
{
  bool found;
  void *location;

     
                                                                             
                                                                          
                                       
     
                                                        
     
  infoP->dsize = infoP->max_dsize = hash_select_dirsize(max_size);
  infoP->alloc = ShmemAllocNoError;
  hash_flags |= HASH_SHARED_MEM | HASH_ALLOC | HASH_DIRSIZE;

                                     
  location = ShmemInitStruct(name, hash_get_shared_size(infoP, hash_flags), &found);

     
                                                                            
               
     
  if (found)
  {
    hash_flags |= HASH_ATTACH;
  }

                                                        
  infoP->hctl = (HASHHDR *)location;

  return hash_create(name, init_size, infoP, hash_flags);
}

   
                                                                     
   
                                                             
                                                            
                                                            
                                                             
                           
   
                                                                            
                                                             
   
                                                                           
                                                                          
             
   
void *
ShmemInitStruct(const char *name, Size size, bool *foundPtr)
{
  ShmemIndexEnt *result;
  void *structPtr;

  LWLockAcquire(ShmemIndexLock, LW_EXCLUSIVE);

  if (!ShmemIndex)
  {
    PGShmemHeader *shmemseghdr = ShmemSegHdr;

                                                              
    Assert(strcmp(name, "ShmemIndex") == 0);

    if (IsUnderPostmaster)
    {
                                                           
      Assert(shmemseghdr->index != NULL);
      structPtr = shmemseghdr->index;
      *foundPtr = true;
    }
    else
    {
         
                                                                         
                                                   
         
                                                                     
                                                                         
                                                     
         
      Assert(shmemseghdr->index == NULL);
      structPtr = ShmemAlloc(size);
      shmemseghdr->index = structPtr;
      *foundPtr = false;
    }
    LWLockRelease(ShmemIndexLock);
    return structPtr;
  }

                                     
  result = (ShmemIndexEnt *)hash_search(ShmemIndex, name, HASH_ENTER_NULL, foundPtr);

  if (!result)
  {
    LWLockRelease(ShmemIndexLock);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("could not create ShmemIndex entry for data structure \"%s\"", name)));
  }

  if (*foundPtr)
  {
       
                                                                        
                                                                          
                                                              
       
    if (result->size != size)
    {
      LWLockRelease(ShmemIndexLock);
      ereport(ERROR, (errmsg("ShmemIndex entry size is wrong for data structure"
                             " \"%s\": expected %zu, actual %zu",
                         name, size, result->size)));
    }
    structPtr = result->location;
  }
  else
  {
                                                               
    structPtr = ShmemAllocNoError(size);
    if (structPtr == NULL)
    {
                                                             
      hash_search(ShmemIndex, name, HASH_REMOVE, NULL);
      LWLockRelease(ShmemIndexLock);
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("not enough shared memory for data structure"
                                                             " \"%s\" (%zu bytes requested)",
                                                          name, size)));
    }
    result->size = size;
    result->location = structPtr;
  }

  LWLockRelease(ShmemIndexLock);

  Assert(ShmemAddrIsValid(structPtr));

  Assert(structPtr == (void *)CACHELINEALIGN(structPtr));

  return structPtr;
}

   
                                              
   
Size
add_size(Size s1, Size s2)
{
  Size result;

  result = s1 + s2;
                                                        
  if (result < s1 || result < s2)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested shared memory size overflows size_t")));
  }
  return result;
}

   
                                                   
   
Size
mul_size(Size s1, Size s2)
{
  Size result;

  if (s1 == 0 || s2 == 0)
  {
    return 0;
  }
  result = s1 * s2;
                                                        
  if (result / s2 != s1)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("requested shared memory size overflows size_t")));
  }
  return result;
}
