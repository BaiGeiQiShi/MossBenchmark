                                                                            
   
              
                         
   
                                                                              
                                                                             
                                                                           
                                                                               
                                                                             
                                                                             
                                                                            
                                                                              
                                                                           
                                                                           
                                                                           
                                                                      
                                                                         
                                           
                                                                       
                                                                         
                                                                            
                                                                
   
                                                                          
                                                                           
                                                                        
                                                                
   
                                                               
   
                                                                            
                                                                  
   
                                                                             
                                                                         
                                                                  
   
                                                                            
                                                                          
                                                                              
                                                                 
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

   
                      
   
                                                                         
                                                                              
                                           
                                                                 
   
                                                                     
                                                                     
                                                        
   
                                                               
                                                                    
                                                              
   
                         
                                                                      
                                                                            
                                                                     
   
                                                                        
                                                                        
   
                                                      
                                   
                                                         
                                            
   

#include "postgres.h"

#include <limits.h>

#include "access/xact.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/dynahash.h"
#include "utils/memutils.h"

   
             
   
                                                                          
                                                                       
                                                                            
                                                                         
                                                             
   
                                                                       
                                                                         
                                                                       
                                                                        
                                                                  
   
#define DEF_SEGSIZE 256
#define DEF_SEGSIZE_SHIFT 8                                
#define DEF_DIRSIZE 256
#define DEF_FFACTOR 1                          

                                                                  
#define NUM_FREELISTS 32

                                                    
typedef HASHELEMENT *HASHBUCKET;

                                                  
typedef HASHBUCKET *HASHSEGMENT;

   
                      
   
                                                                            
                                                                      
                                                                               
                                                                     
   
                                                                              
                                                                              
                                                                              
                                                                         
   
                                                                         
                                                                         
                      
   
typedef struct
{
  slock_t mutex;                                         
  long nentries;                                                      
  HASHELEMENT *freeList;                             
} FreeListData;

   
                                                                      
   
                                                                         
                                                                              
                                                                            
                                                              
   
struct HASHHDR
{
     
                                                                            
                                                                          
                                                                           
                                                                             
                                                              
     
                                                                            
                                                                          
     
  FreeListData freeList[NUM_FREELISTS];

                                                               
                                                                         
  long dsize;                            
  long nsegs;                                                     
  uint32 max_bucket;                                  
  uint32 high_mask;                                        
  uint32 low_mask;                                                

                                                    
  Size keysize;                                      
  Size entrysize;                                            
  long num_partitions;                                              
  long ffactor;                                
  long max_dsize;                                                    
  long ssize;                                                   
  int sshift;                                           
  int nelem_alloc;                                                

#ifdef HASH_STATISTICS

     
                                                                          
                                                             
     
  long accesses;
  long collisions;
#endif
};

#define IS_PARTITIONED(hctl) ((hctl)->num_partitions != 0)

#define FREELIST_IDX(hctl, hashcode) (IS_PARTITIONED(hctl) ? (hashcode) % NUM_FREELISTS : 0)

   
                                                                             
                                                           
   
struct HTAB
{
  HASHHDR *hctl;                                            
  HASHSEGMENT *dir;                                       
  HashValueFunc hash;                       
  HashCompareFunc match;                              
  HashCopyFunc keycopy;                            
  HashAllocFunc alloc;                         
  MemoryContext hcxt;                                                  
  char *tabname;                                              
  bool isshared;                                                
  bool isfixed;                                      

                                                                        
  bool frozen;                                     

                                                                       
  Size keysize;                               
  long ssize;                                            
  int sshift;                                    
};

   
                                          
   
#define ELEMENTKEY(helem) (((char *)(helem)) + MAXALIGN(sizeof(HASHELEMENT)))

   
                                               
   
#define ELEMENT_FROM_KEY(key) ((HASHELEMENT *)(((char *)(key)) - MAXALIGN(sizeof(HASHELEMENT))))

   
                                                          
   
#define MOD(x, y) ((x) & ((y)-1))

#if HASH_STATISTICS
static long hash_accesses, hash_collisions, hash_expansions;
#endif

   
                               
   
static void *
DynaHashAlloc(Size size);
static HASHSEGMENT
seg_alloc(HTAB *hashp);
static bool
element_alloc(HTAB *hashp, int nelem, int freelist_idx);
static bool
dir_realloc(HTAB *hashp);
static bool
expand_table(HTAB *hashp);
static HASHBUCKET
get_hash_entry(HTAB *hashp, int freelist_idx);
static void
hdefault(HTAB *hashp);
static int
choose_nelem_alloc(Size entrysize);
static bool
init_htab(HTAB *hashp, long nelem);
static void
hash_corrupted(HTAB *hashp);
static long
next_pow2_long(long num);
static int
next_pow2_int(long num);
static void
register_seq_scan(HTAB *hashp);
static void
deregister_seq_scan(HTAB *hashp);
static bool
has_seq_scans(HTAB *hashp);

   
                             
   
static MemoryContext CurrentDynaHashCxt = NULL;

static void *
DynaHashAlloc(Size size)
{
  Assert(MemoryContextIsValid(CurrentDynaHashCxt));
  return MemoryContextAlloc(CurrentDynaHashCxt, size);
}

   
                                   
   
                                                                            
                                                                           
                              
   
static int
string_compare(const char *key1, const char *key2, Size keysize)
{
  return strncmp(key1, key2, keysize - 1);
}

                                                                   

   
                                                  
   
                                                          
                                              
                                                             
                                                                 
   
                                                                        
                                                                          
                                                                        
                                                                         
                                                                        
   
HTAB *
hash_create(const char *tabname, long nelem, HASHCTL *info, int flags)
{
  HTAB *hashp;
  HASHHDR *hctl;

     
                                                                            
                                                                    
     
                                                                            
                                                                          
                                                                             
                                                                          
                
     
  if (flags & HASH_SHARED_MEM)
  {
                                            
    CurrentDynaHashCxt = TopMemoryContext;
  }
  else
  {
                                                        
    if (flags & HASH_CONTEXT)
    {
      CurrentDynaHashCxt = info->hcxt;
    }
    else
    {
      CurrentDynaHashCxt = TopMemoryContext;
    }
    CurrentDynaHashCxt = AllocSetContextCreate(CurrentDynaHashCxt, "dynahash", ALLOCSET_DEFAULT_SIZES);
  }

                                                                 
  hashp = (HTAB *)DynaHashAlloc(sizeof(HTAB) + strlen(tabname) + 1);
  MemSet(hashp, 0, sizeof(HTAB));

  hashp->tabname = (char *)(hashp + 1);
  strcpy(hashp->tabname, tabname);

                                                                    
  if (!(flags & HASH_SHARED_MEM))
  {
    MemoryContextSetIdentifier(CurrentDynaHashCxt, hashp->tabname);
  }

     
                                                                          
     
  if (flags & HASH_FUNCTION)
  {
    hashp->hash = info->hash;
  }
  else if (flags & HASH_BLOBS)
  {
                                                      
    Assert(flags & HASH_ELEM);
    if (info->keysize == sizeof(uint32))
    {
      hashp->hash = uint32_hash;
    }
    else
    {
      hashp->hash = tag_hash;
    }
  }
  else
  {
    hashp->hash = string_hash;                            
  }

     
                                                                             
                                                                          
                
     
                                                                         
                                                                             
                                                                          
                              
     
  if (flags & HASH_COMPARE)
  {
    hashp->match = info->match;
  }
  else if (hashp->hash == string_hash)
  {
    hashp->match = (HashCompareFunc)string_compare;
  }
  else
  {
    hashp->match = memcmp;
  }

     
                                                                        
     
  if (flags & HASH_KEYCOPY)
  {
    hashp->keycopy = info->keycopy;
  }
  else if (hashp->hash == string_hash)
  {
    hashp->keycopy = (HashCopyFunc)strlcpy;
  }
  else
  {
    hashp->keycopy = memcpy;
  }

                                                      
  if (flags & HASH_ALLOC)
  {
    hashp->alloc = info->alloc;
  }
  else
  {
    hashp->alloc = DynaHashAlloc;
  }

  if (flags & HASH_SHARED_MEM)
  {
       
                                                                      
                                                                           
             
       
    hashp->hctl = info->hctl;
    hashp->dir = (HASHSEGMENT *)(((char *)info->hctl) + sizeof(HASHHDR));
    hashp->hcxt = NULL;
    hashp->isshared = true;

                                                               
    if (flags & HASH_ATTACH)
    {
                                                         
      hctl = hashp->hctl;
      hashp->keysize = hctl->keysize;
      hashp->ssize = hctl->ssize;
      hashp->sshift = hctl->sshift;

      return hashp;
    }
  }
  else
  {
                                   
    hashp->hctl = NULL;
    hashp->dir = NULL;
    hashp->hcxt = CurrentDynaHashCxt;
    hashp->isshared = false;
  }

  if (!hashp->hctl)
  {
    hashp->hctl = (HASHHDR *)hashp->alloc(sizeof(HASHHDR));
    if (!hashp->hctl)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
  }

  hashp->frozen = false;

  hdefault(hashp);

  hctl = hashp->hctl;

  if (flags & HASH_PARTITION)
  {
                                                            
    Assert(flags & HASH_SHARED_MEM);

       
                                                                          
                                                                          
                  
       
    Assert(info->num_partitions == next_pow2_int(info->num_partitions));

    hctl->num_partitions = info->num_partitions;
  }

  if (flags & HASH_SEGMENT)
  {
    hctl->ssize = info->ssize;
    hctl->sshift = my_log2(info->ssize);
                                          
    Assert(hctl->ssize == (1L << hctl->sshift));
  }
  if (flags & HASH_FFACTOR)
  {
    hctl->ffactor = info->ffactor;
  }

     
                                                                     
     
  if (flags & HASH_DIRSIZE)
  {
    hctl->max_dsize = info->max_dsize;
    hctl->dsize = info->dsize;
  }

     
                                                                             
                            
     
  if (flags & HASH_ELEM)
  {
    Assert(info->entrysize >= info->keysize);
    hctl->keysize = info->keysize;
    hctl->entrysize = info->entrysize;
  }

                                                         
  hashp->keysize = hctl->keysize;
  hashp->ssize = hctl->ssize;
  hashp->sshift = hctl->sshift;

                                          
  if (!init_htab(hashp, nelem))
  {
    elog(ERROR, "failed to initialize hash table \"%s\"", hashp->tabname);
  }

     
                                                                            
                                                                          
     
                                                                      
                                                                             
                                                                 
     
  if ((flags & HASH_SHARED_MEM) || nelem < hctl->nelem_alloc)
  {
    int i, freelist_partitions, nelem_alloc, nelem_alloc_first;

       
                                                                          
                                                                    
       
    if (IS_PARTITIONED(hashp->hctl))
    {
      freelist_partitions = NUM_FREELISTS;
    }
    else
    {
      freelist_partitions = 1;
    }

    nelem_alloc = nelem / freelist_partitions;
    if (nelem_alloc <= 0)
    {
      nelem_alloc = 1;
    }

       
                                                                        
                                                                        
       
    if (nelem_alloc * freelist_partitions < nelem)
    {
      nelem_alloc_first = nelem - nelem_alloc * (freelist_partitions - 1);
    }
    else
    {
      nelem_alloc_first = nelem_alloc;
    }

    for (i = 0; i < freelist_partitions; i++)
    {
      int temp = (i == 0) ? nelem_alloc_first : nelem_alloc;

      if (!element_alloc(hashp, temp, i))
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
      }
    }
  }

  if (flags & HASH_FIXED_SIZE)
  {
    hashp->isfixed = true;
  }
  return hashp;
}

   
                                   
   
static void
hdefault(HTAB *hashp)
{
  HASHHDR *hctl = hashp->hctl;

  MemSet(hctl, 0, sizeof(HASHHDR));

  hctl->dsize = DEF_DIRSIZE;
  hctl->nsegs = 0;

                                                      
  hctl->keysize = sizeof(char *);
  hctl->entrysize = 2 * sizeof(char *);

  hctl->num_partitions = 0;                      

  hctl->ffactor = DEF_FFACTOR;

                                       
  hctl->max_dsize = NO_MAX_DSIZE;

  hctl->ssize = DEF_SEGSIZE;
  hctl->sshift = DEF_SEGSIZE_SHIFT;

#ifdef HASH_STATISTICS
  hctl->accesses = hctl->collisions = 0;
#endif
}

   
                                                                         
                                                        
   
static int
choose_nelem_alloc(Size entrysize)
{
  int nelem_alloc;
  Size elementSize;
  Size allocSize;

                                                             
                                                 
  elementSize = MAXALIGN(sizeof(HASHELEMENT)) + MAXALIGN(entrysize);

     
                                                                         
                                                                         
                                                                            
                                                                            
                                                                             
                                          
     
  allocSize = 32 * 4;                                    
  do
  {
    allocSize <<= 1;
    nelem_alloc = allocSize / elementSize;
  } while (nelem_alloc < 32);

  return nelem_alloc;
}

   
                                                                          
          
   
static bool
init_htab(HTAB *hashp, long nelem)
{
  HASHHDR *hctl = hashp->hctl;
  HASHSEGMENT *segp;
  int nbuckets;
  int nsegs;
  int i;

     
                                                    
     
  if (IS_PARTITIONED(hctl))
  {
    for (i = 0; i < NUM_FREELISTS; i++)
    {
      SpinLockInit(&(hctl->freeList[i].mutex));
    }
  }

     
                                                                         
                                                                          
                       
     
  nbuckets = next_pow2_int((nelem - 1) / hctl->ffactor + 1);

     
                                                                
                                                                            
                                                                            
                                                                           
     
  while (nbuckets < hctl->num_partitions)
  {
    nbuckets <<= 1;
  }

  hctl->max_bucket = hctl->low_mask = nbuckets - 1;
  hctl->high_mask = (nbuckets << 1) - 1;

     
                                                                          
     
  nsegs = (nbuckets - 1) / hctl->ssize + 1;
  nsegs = next_pow2_int(nsegs);

     
                                                                          
                                       
     
  if (nsegs > hctl->dsize)
  {
    if (!(hashp->dir))
    {
      hctl->dsize = nsegs;
    }
    else
    {
      return false;
    }
  }

                            
  if (!(hashp->dir))
  {
    CurrentDynaHashCxt = hashp->hcxt;
    hashp->dir = (HASHSEGMENT *)hashp->alloc(hctl->dsize * sizeof(HASHSEGMENT));
    if (!hashp->dir)
    {
      return false;
    }
  }

                                 
  for (segp = hashp->dir; hctl->nsegs < nsegs; hctl->nsegs++, segp++)
  {
    *segp = seg_alloc(hashp);
    if (*segp == NULL)
    {
      return false;
    }
  }

                                                      
  hctl->nelem_alloc = choose_nelem_alloc(hctl->entrysize);

#if HASH_DEBUG
  fprintf(stderr, "init_htab:\n%s%p\n%s%ld\n%s%ld\n%s%d\n%s%ld\n%s%u\n%s%x\n%s%x\n%s%ld\n", "TABLE POINTER   ", hashp, "DIRECTORY SIZE  ", hctl->dsize, "SEGMENT SIZE    ", hctl->ssize, "SEGMENT SHIFT   ", hctl->sshift, "FILL FACTOR     ", hctl->ffactor, "MAX BUCKET      ", hctl->max_bucket, "HIGH MASK       ", hctl->high_mask, "LOW  MASK       ", hctl->low_mask, "NSEGS           ", hctl->nsegs);
#endif
  return true;
}

   
                                                                         
                             
                                                                        
                                                                      
                                                                       
   
Size
hash_estimate_size(long num_entries, Size entrysize)
{
  Size size;
  long nBuckets, nSegments, nDirEntries, nElementAllocs, elementSize, elementAllocCnt;

                                         
  nBuckets = next_pow2_long((num_entries - 1) / DEF_FFACTOR + 1);
                                         
  nSegments = next_pow2_long((nBuckets - 1) / DEF_SEGSIZE + 1);
                         
  nDirEntries = DEF_DIRSIZE;
  while (nDirEntries < nSegments)
  {
    nDirEntries <<= 1;                                           
  }

                          
  size = MAXALIGN(sizeof(HASHHDR));                              
                 
  size = add_size(size, mul_size(nDirEntries, sizeof(HASHSEGMENT)));
                
  size = add_size(size, mul_size(nSegments, MAXALIGN(DEF_SEGSIZE * sizeof(HASHBUCKET))));
                                                                        
  elementAllocCnt = choose_nelem_alloc(entrysize);
  nElementAllocs = (num_entries - 1) / elementAllocCnt + 1;
  elementSize = MAXALIGN(sizeof(HASHELEMENT)) + MAXALIGN(entrysize);
  size = add_size(size, mul_size(nElementAllocs, mul_size(elementAllocCnt, elementSize)));

  return size;
}

   
                                                                       
                              
                                                                          
                                   
                                                                       
   
                                                                 
   
long
hash_select_dirsize(long num_entries)
{
  long nBuckets, nSegments, nDirEntries;

                                         
  nBuckets = next_pow2_long((num_entries - 1) / DEF_FFACTOR + 1);
                                         
  nSegments = next_pow2_long((nBuckets - 1) / DEF_SEGSIZE + 1);
                         
  nDirEntries = DEF_DIRSIZE;
  while (nDirEntries < nSegments)
  {
    nDirEntries <<= 1;                                           
  }

  return nDirEntries;
}

   
                                                                      
                                                                       
                                           
   
Size
hash_get_shared_size(HASHCTL *info, int flags)
{
  Assert(flags & HASH_DIRSIZE);
  Assert(info->dsize == info->max_dsize);
  return sizeof(HASHHDR) + info->dsize * sizeof(HASHSEGMENT);
}

                                                                  

void
hash_destroy(HTAB *hashp)
{
  if (hashp != NULL)
  {
                                                                
    Assert(hashp->alloc == DynaHashAlloc);
                                                     
    Assert(hashp->hcxt != NULL);

    hash_stats("destroy", hashp);

       
                                                                      
       
    MemoryContextDelete(hashp->hcxt);
  }
}

void
hash_stats(const char *where, HTAB *hashp)
{
#if HASH_STATISTICS
  fprintf(stderr, "%s: this HTAB -- accesses %ld collisions %ld\n", where, hashp->hctl->accesses, hashp->hctl->collisions);

  fprintf(stderr, "hash_stats: entries %ld keysize %ld maxp %u segmentcount %ld\n", hash_get_num_entries(hashp), (long)hashp->hctl->keysize, hashp->hctl->max_bucket, hashp->hctl->nsegs);
  fprintf(stderr, "%s: total accesses %ld total collisions %ld\n", where, hash_accesses, hash_collisions);
  fprintf(stderr, "hash_stats: total expansions %ld\n", hash_expansions);
#endif
}

                                                                              

   
                                                                      
   
                                                                          
                                                                           
              
   
uint32
get_hash_value(HTAB *hashp, const void *keyPtr)
{
  return hashp->hash(keyPtr, hashp->keysize);
}

                                             
static inline uint32
calc_bucket(HASHHDR *hctl, uint32 hash_val)
{
  uint32 bucket;

  bucket = hash_val & hctl->high_mask;
  if (bucket > hctl->max_bucket)
  {
    bucket = bucket & hctl->low_mask;
  }

  return bucket;
}

   
                                                          
                                                                               
   
                     
                                    
                                                                    
                                                            
                                                               
   
                                                                          
                                                                          
                                                                     
   
                                                                       
                                                                         
                                                                    
                                                                           
                                                      
   
                                                                     
                                                                        
                                                                      
   
                                                                           
                                     
   
void *
hash_search(HTAB *hashp, const void *keyPtr, HASHACTION action, bool *foundPtr)
{
  return hash_search_with_hash_value(hashp, keyPtr, hashp->hash(keyPtr, hashp->keysize), action, foundPtr);
}

void *
hash_search_with_hash_value(HTAB *hashp, const void *keyPtr, uint32 hashvalue, HASHACTION action, bool *foundPtr)
{
  HASHHDR *hctl = hashp->hctl;
  int freelist_idx = FREELIST_IDX(hctl, hashvalue);
  Size keysize;
  uint32 bucket;
  long segment_num;
  long segment_ndx;
  HASHSEGMENT segp;
  HASHBUCKET currBucket;
  HASHBUCKET *prevBucketPtr;
  HashCompareFunc match;

#if HASH_STATISTICS
  hash_accesses++;
  hctl->accesses++;
#endif

     
                                                          
     
                                                                          
                                                                          
                                                                   
                                                                   
     
  if (action == HASH_ENTER || action == HASH_ENTER_NULL)
  {
       
                                                                         
                                                                          
                                                                         
       
    if (!IS_PARTITIONED(hctl) && !hashp->frozen && hctl->freeList[0].nentries / (long)(hctl->max_bucket + 1) >= hctl->ffactor && !has_seq_scans(hashp))
    {
      (void)expand_table(hashp);
    }
  }

     
                           
     
  bucket = calc_bucket(hctl, hashvalue);

  segment_num = bucket >> hashp->sshift;
  segment_ndx = MOD(bucket, hashp->ssize);

  segp = hashp->dir[segment_num];

  if (segp == NULL)
  {
    hash_corrupted(hashp);
  }

  prevBucketPtr = &segp[segment_ndx];
  currBucket = *prevBucketPtr;

     
                                                     
     
  match = hashp->match;                                       
  keysize = hashp->keysize;            

  while (currBucket != NULL)
  {
    if (currBucket->hashvalue == hashvalue && match(ELEMENTKEY(currBucket), keyPtr, keysize) == 0)
    {
      break;
    }
    prevBucketPtr = &(currBucket->link);
    currBucket = *prevBucketPtr;
#if HASH_STATISTICS
    hash_collisions++;
    hctl->collisions++;
#endif
  }

  if (foundPtr)
  {
    *foundPtr = (bool)(currBucket != NULL);
  }

     
                   
     
  switch (action)
  {
  case HASH_FIND:
    if (currBucket != NULL)
    {
      return (void *)ELEMENTKEY(currBucket);
    }
    return NULL;

  case HASH_REMOVE:
    if (currBucket != NULL)
    {
                                                                    
      if (IS_PARTITIONED(hctl))
      {
        SpinLockAcquire(&(hctl->freeList[freelist_idx].mutex));
      }

                                                                    
      Assert(hctl->freeList[freelist_idx].nentries > 0);
      hctl->freeList[freelist_idx].nentries--;

                                                   
      *prevBucketPtr = currBucket->link;

                                                       
      currBucket->link = hctl->freeList[freelist_idx].freeList;
      hctl->freeList[freelist_idx].freeList = currBucket;

      if (IS_PARTITIONED(hctl))
      {
        SpinLockRelease(&hctl->freeList[freelist_idx].mutex);
      }

         
                                                                
                                                                     
                                              
         
      return (void *)ELEMENTKEY(currBucket);
    }
    return NULL;

  case HASH_ENTER_NULL:
                                                              
    Assert(hashp->alloc != DynaHashAlloc);
                   

  case HASH_ENTER:
                                                           
    if (currBucket != NULL)
    {
      return (void *)ELEMENTKEY(currBucket);
    }

                                    
    if (hashp->frozen)
    {
      elog(ERROR, "cannot insert into frozen hashtable \"%s\"", hashp->tabname);
    }

    currBucket = get_hash_entry(hashp, freelist_idx);
    if (currBucket == NULL)
    {
                         
      if (action == HASH_ENTER_NULL)
      {
        return NULL;
      }
                                    
      if (hashp->isshared)
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
      }
    }

                                    
    *prevBucketPtr = currBucket;
    currBucket->link = NULL;

                              
    currBucket->hashvalue = hashvalue;
    hashp->keycopy(ELEMENTKEY(currBucket), keyPtr, keysize);

       
                                                                    
                                                                      
                                                                       
                                
       

    return (void *)ELEMENTKEY(currBucket);
  }

  elog(ERROR, "unrecognized hash action code: %d", (int)action);

  return NULL;                          
}

   
                                                                          
   
                                                                             
                                                                            
                                                                            
                                                                   
   
                                                                              
                                                                          
                 
   
                                                                           
                                                                          
                                 
   
                                                                           
                                                                          
   
bool
hash_update_hash_key(HTAB *hashp, void *existingEntry, const void *newKeyPtr)
{
  HASHELEMENT *existingElement = ELEMENT_FROM_KEY(existingEntry);
  HASHHDR *hctl = hashp->hctl;
  uint32 newhashvalue;
  Size keysize;
  uint32 bucket;
  uint32 newbucket;
  long segment_num;
  long segment_ndx;
  HASHSEGMENT segp;
  HASHBUCKET currBucket;
  HASHBUCKET *prevBucketPtr;
  HASHBUCKET *oldPrevPtr;
  HashCompareFunc match;

#if HASH_STATISTICS
  hash_accesses++;
  hctl->accesses++;
#endif

                                  
  if (hashp->frozen)
  {
    elog(ERROR, "cannot update in frozen hashtable \"%s\"", hashp->tabname);
  }

     
                                                                            
                                                                             
                                                                     
     
  bucket = calc_bucket(hctl, existingElement->hashvalue);

  segment_num = bucket >> hashp->sshift;
  segment_ndx = MOD(bucket, hashp->ssize);

  segp = hashp->dir[segment_num];

  if (segp == NULL)
  {
    hash_corrupted(hashp);
  }

  prevBucketPtr = &segp[segment_ndx];
  currBucket = *prevBucketPtr;

  while (currBucket != NULL)
  {
    if (currBucket == existingElement)
    {
      break;
    }
    prevBucketPtr = &(currBucket->link);
    currBucket = *prevBucketPtr;
  }

  if (currBucket == NULL)
  {
    elog(ERROR, "hash_update_hash_key argument is not in hashtable \"%s\"", hashp->tabname);
  }

  oldPrevPtr = prevBucketPtr;

     
                                                                             
                                          
     
  newhashvalue = hashp->hash(newKeyPtr, hashp->keysize);

  newbucket = calc_bucket(hctl, newhashvalue);

  segment_num = newbucket >> hashp->sshift;
  segment_ndx = MOD(newbucket, hashp->ssize);

  segp = hashp->dir[segment_num];

  if (segp == NULL)
  {
    hash_corrupted(hashp);
  }

  prevBucketPtr = &segp[segment_ndx];
  currBucket = *prevBucketPtr;

     
                                                     
     
  match = hashp->match;                                       
  keysize = hashp->keysize;            

  while (currBucket != NULL)
  {
    if (currBucket->hashvalue == newhashvalue && match(ELEMENTKEY(currBucket), newKeyPtr, keysize) == 0)
    {
      break;
    }
    prevBucketPtr = &(currBucket->link);
    currBucket = *prevBucketPtr;
#if HASH_STATISTICS
    hash_collisions++;
    hctl->collisions++;
#endif
  }

  if (currBucket != NULL)
  {
    return false;                                       
  }

  currBucket = existingElement;

     
                                                                       
                                                                         
                                                                          
                                                                           
                                
     
  if (bucket != newbucket)
  {
                                                           
    *oldPrevPtr = currBucket->link;

                                        
    *prevBucketPtr = currBucket;
    currBucket->link = NULL;
  }

                                
  currBucket->hashvalue = newhashvalue;
  hashp->keycopy(ELEMENTKEY(currBucket), newKeyPtr, keysize);

                                   

  return true;
}

   
                                                                             
                                                                          
                            
   
static HASHBUCKET
get_hash_entry(HTAB *hashp, int freelist_idx)
{
  HASHHDR *hctl = hashp->hctl;
  HASHBUCKET newElement;

  for (;;)
  {
                                                                  
    if (IS_PARTITIONED(hctl))
    {
      SpinLockAcquire(&hctl->freeList[freelist_idx].mutex);
    }

                                               
    newElement = hctl->freeList[freelist_idx].freeList;

    if (newElement != NULL)
    {
      break;
    }

    if (IS_PARTITIONED(hctl))
    {
      SpinLockRelease(&hctl->freeList[freelist_idx].mutex);
    }

       
                                                                         
                                                                        
                                                                         
                                                                           
                                                                         
                                                                         
                                                                         
                                                                         
                                                                        
                       
       
    if (!element_alloc(hashp, hctl->nelem_alloc, freelist_idx))
    {
      int borrow_from_idx;

      if (!IS_PARTITIONED(hctl))
      {
        return NULL;                    
      }

                                                       
      borrow_from_idx = freelist_idx;
      for (;;)
      {
        borrow_from_idx = (borrow_from_idx + 1) % NUM_FREELISTS;
        if (borrow_from_idx == freelist_idx)
        {
          break;                                   
        }

        SpinLockAcquire(&(hctl->freeList[borrow_from_idx].mutex));
        newElement = hctl->freeList[borrow_from_idx].freeList;

        if (newElement != NULL)
        {
          hctl->freeList[borrow_from_idx].freeList = newElement->link;
          SpinLockRelease(&(hctl->freeList[borrow_from_idx].mutex));

                                                                     
          SpinLockAcquire(&hctl->freeList[freelist_idx].mutex);
          hctl->freeList[freelist_idx].nentries++;
          SpinLockRelease(&hctl->freeList[freelist_idx].mutex);

          return newElement;
        }

        SpinLockRelease(&(hctl->freeList[borrow_from_idx].mutex));
      }

                                                                    
      return NULL;
    }
  }

                                                 
  hctl->freeList[freelist_idx].freeList = newElement->link;
  hctl->freeList[freelist_idx].nentries++;

  if (IS_PARTITIONED(hctl))
  {
    SpinLockRelease(&hctl->freeList[freelist_idx].mutex);
  }

  return newElement;
}

   
                                                                    
   
long
hash_get_num_entries(HTAB *hashp)
{
  int i;
  long sum = hashp->hctl->freeList[0].nentries;

     
                                                                     
                                                                            
                
     
  if (IS_PARTITIONED(hashp->hctl))
  {
    for (i = 1; i < NUM_FREELISTS; i++)
    {
      sum += hashp->hctl->freeList[i].nentries;
    }
  }

  return sum;
}

   
                               
                                                       
                                                            
   
                                                                              
                                                                            
                        
   
                                                                            
                                                                        
                                                                         
                                                                           
                                                                
   
                                                                         
                                                                               
                                              
   
                                                                          
                                                                            
                                                                          
                                                       
   
void
hash_seq_init(HASH_SEQ_STATUS *status, HTAB *hashp)
{
  status->hashp = hashp;
  status->curBucket = 0;
  status->curEntry = NULL;
  if (!hashp->frozen)
  {
    register_seq_scan(hashp);
  }
}

void *
hash_seq_search(HASH_SEQ_STATUS *status)
{
  HTAB *hashp;
  HASHHDR *hctl;
  uint32 max_bucket;
  long ssize;
  long segment_num;
  long segment_ndx;
  HASHSEGMENT segp;
  uint32 curBucket;
  HASHELEMENT *curElem;

  if ((curElem = status->curEntry) != NULL)
  {
                                         
    status->curEntry = curElem->link;
    if (status->curEntry == NULL)                         
    {
      ++status->curBucket;
    }
    return (void *)ELEMENTKEY(curElem);
  }

     
                                                            
     
  curBucket = status->curBucket;
  hashp = status->hashp;
  hctl = hashp->hctl;
  ssize = hashp->ssize;
  max_bucket = hctl->max_bucket;

  if (curBucket > max_bucket)
  {
    hash_seq_term(status);
    return NULL;                     
  }

     
                                                          
     
  segment_num = curBucket >> hashp->sshift;
  segment_ndx = MOD(curBucket, ssize);

  segp = hashp->dir[segment_num];

     
                                                                           
                                                                          
                                                                           
                                                          
     
  while ((curElem = segp[segment_ndx]) == NULL)
  {
                                       
    if (++curBucket > max_bucket)
    {
      status->curBucket = curBucket;
      hash_seq_term(status);
      return NULL;                     
    }
    if (++segment_ndx >= ssize)
    {
      segment_num++;
      segment_ndx = 0;
      segp = hashp->dir[segment_num];
    }
  }

                                  
  status->curEntry = curElem->link;
  if (status->curEntry == NULL)                         
  {
    ++curBucket;
  }
  status->curBucket = curBucket;
  return (void *)ELEMENTKEY(curElem);
}

void
hash_seq_term(HASH_SEQ_STATUS *status)
{
  if (!status->hashp->frozen)
  {
    deregister_seq_scan(status->hashp);
  }
}

   
               
                                                                 
                    
   
                                                                           
                                                                       
                                                                         
                              
   
                                                                             
                                                                          
   
void
hash_freeze(HTAB *hashp)
{
  if (hashp->isshared)
  {
    elog(ERROR, "cannot freeze shared hashtable \"%s\"", hashp->tabname);
  }
  if (!hashp->frozen && has_seq_scans(hashp))
  {
    elog(ERROR, "cannot freeze hashtable \"%s\" because it has active scans", hashp->tabname);
  }
  hashp->frozen = true;
}

                                                                      

   
                                                    
   
static bool
expand_table(HTAB *hashp)
{
  HASHHDR *hctl = hashp->hctl;
  HASHSEGMENT old_seg, new_seg;
  long old_bucket, new_bucket;
  long new_segnum, new_segndx;
  long old_segnum, old_segndx;
  HASHBUCKET *oldlink, *newlink;
  HASHBUCKET currElement, nextElement;

  Assert(!IS_PARTITIONED(hctl));

#ifdef HASH_STATISTICS
  hash_expansions++;
#endif

  new_bucket = hctl->max_bucket + 1;
  new_segnum = new_bucket >> hashp->sshift;
  new_segndx = MOD(new_bucket, hashp->ssize);

  if (new_segnum >= hctl->nsegs)
  {
                                                                     
    if (new_segnum >= hctl->dsize)
    {
      if (!dir_realloc(hashp))
      {
        return false;
      }
    }
    if (!(hashp->dir[new_segnum] = seg_alloc(hashp)))
    {
      return false;
    }
    hctl->nsegs++;
  }

                                   
  hctl->max_bucket++;

     
                                                                         
                                                                           
                                                                           
                                                                  
     
  old_bucket = (new_bucket & hctl->low_mask);

     
                                                 
     
  if ((uint32)new_bucket > hctl->high_mask)
  {
    hctl->low_mask = hctl->high_mask;
    hctl->high_mask = (uint32)new_bucket | hctl->low_mask;
  }

     
                                                                            
                                                                        
                                                                            
                             
     
  old_segnum = old_bucket >> hashp->sshift;
  old_segndx = MOD(old_bucket, hashp->ssize);

  old_seg = hashp->dir[old_segnum];
  new_seg = hashp->dir[new_segnum];

  oldlink = &old_seg[old_segndx];
  newlink = &new_seg[new_segndx];

  for (currElement = *oldlink; currElement != NULL; currElement = nextElement)
  {
    nextElement = currElement->link;
    if ((long)calc_bucket(hctl, currElement->hashvalue) == old_bucket)
    {
      *oldlink = currElement;
      oldlink = &currElement->link;
    }
    else
    {
      *newlink = currElement;
      newlink = &currElement->link;
    }
  }
                                                            
  *oldlink = NULL;
  *newlink = NULL;

  return true;
}

static bool
dir_realloc(HTAB *hashp)
{
  HASHSEGMENT *p;
  HASHSEGMENT *old_p;
  long new_dsize;
  long old_dirsize;
  long new_dirsize;

  if (hashp->hctl->max_dsize != NO_MAX_DSIZE)
  {
    return false;
  }

                            
  new_dsize = hashp->hctl->dsize << 1;
  old_dirsize = hashp->hctl->dsize * sizeof(HASHSEGMENT);
  new_dirsize = new_dsize * sizeof(HASHSEGMENT);

  old_p = hashp->dir;
  CurrentDynaHashCxt = hashp->hcxt;
  p = (HASHSEGMENT *)hashp->alloc((Size)new_dirsize);

  if (p != NULL)
  {
    memcpy(p, old_p, old_dirsize);
    MemSet(((char *)p) + old_dirsize, 0, new_dirsize - old_dirsize);
    hashp->dir = p;
    hashp->hctl->dsize = new_dsize;

                                                                    
    Assert(hashp->alloc == DynaHashAlloc);
    pfree(old_p);

    return true;
  }

  return false;
}

static HASHSEGMENT
seg_alloc(HTAB *hashp)
{
  HASHSEGMENT segp;

  CurrentDynaHashCxt = hashp->hcxt;
  segp = (HASHSEGMENT)hashp->alloc(sizeof(HASHBUCKET) * hashp->ssize);

  if (!segp)
  {
    return NULL;
  }

  MemSet(segp, 0, sizeof(HASHBUCKET) * hashp->ssize);

  return segp;
}

   
                                                                         
   
static bool
element_alloc(HTAB *hashp, int nelem, int freelist_idx)
{
  HASHHDR *hctl = hashp->hctl;
  Size elementSize;
  HASHELEMENT *firstElement;
  HASHELEMENT *tmpElement;
  HASHELEMENT *prevElement;
  int i;

  if (hashp->isfixed)
  {
    return false;
  }

                                                             
  elementSize = MAXALIGN(sizeof(HASHELEMENT)) + MAXALIGN(hctl->entrysize);

  CurrentDynaHashCxt = hashp->hcxt;
  firstElement = (HASHELEMENT *)hashp->alloc(nelem * elementSize);

  if (!firstElement)
  {
    return false;
  }

                                                             
  prevElement = NULL;
  tmpElement = firstElement;
  for (i = 0; i < nelem; i++)
  {
    tmpElement->link = prevElement;
    prevElement = tmpElement;
    tmpElement = (HASHELEMENT *)(((char *)tmpElement) + elementSize);
  }

                                                   
  if (IS_PARTITIONED(hctl))
  {
    SpinLockAcquire(&hctl->freeList[freelist_idx].mutex);
  }

                                                                        
  firstElement->link = hctl->freeList[freelist_idx].freeList;
  hctl->freeList[freelist_idx].freeList = prevElement;

  if (IS_PARTITIONED(hctl))
  {
    SpinLockRelease(&hctl->freeList[freelist_idx].mutex);
  }

  return true;
}

                                                          
static void
hash_corrupted(HTAB *hashp)
{
     
                                                                     
                                                                      
     
  if (hashp->isshared)
  {
    elog(PANIC, "hash table \"%s\" corrupted", hashp->tabname);
  }
  else
  {
    elog(FATAL, "hash table \"%s\" corrupted", hashp->tabname);
  }
}

                                       
int
my_log2(long num)
{
  int i;
  long limit;

                                                                            
  if (num > LONG_MAX / 2)
  {
    num = LONG_MAX / 2;
  }

  for (i = 0, limit = 1; limit < num; i++, limit <<= 1)
    ;
  return i;
}

                                                                           
static long
next_pow2_long(long num)
{
                                                    
  return 1L << my_log2(num);
}

                                                                           
static int
next_pow2_int(long num)
{
  if (num > INT_MAX / 2)
  {
    num = INT_MAX / 2;
  }
  return 1 << my_log2(num);
}

                                                                      

   
                                                                            
                                                                              
                                                                           
                                                                          
                                                                         
                                                                            
                                                                           
   
                                                                           
                                                                             
                                                                       
                                                                       
   
                                                                        
                                                                            
                                                                              
          
   
                                                                             
                                                                             
                                                                             
                                                                            
                                                 
   

#define MAX_SEQ_SCANS 100

static HTAB *seq_scan_tables[MAX_SEQ_SCANS];                           
static int seq_scan_level[MAX_SEQ_SCANS];                                   
static int num_seq_scans = 0;

                                                               
static void
register_seq_scan(HTAB *hashp)
{
  if (num_seq_scans >= MAX_SEQ_SCANS)
  {
    elog(ERROR, "too many active hash_seq_search scans, cannot start one on \"%s\"", hashp->tabname);
  }
  seq_scan_tables[num_seq_scans] = hashp;
  seq_scan_level[num_seq_scans] = GetCurrentTransactionNestLevel();
  num_seq_scans++;
}

                               
static void
deregister_seq_scan(HTAB *hashp)
{
  int i;

                                                               
  for (i = num_seq_scans - 1; i >= 0; i--)
  {
    if (seq_scan_tables[i] == hashp)
    {
      seq_scan_tables[i] = seq_scan_tables[num_seq_scans - 1];
      seq_scan_level[i] = seq_scan_level[num_seq_scans - 1];
      num_seq_scans--;
      return;
    }
  }
  elog(ERROR, "no hash_seq_search scan for hash table \"%s\"", hashp->tabname);
}

                                          
static bool
has_seq_scans(HTAB *hashp)
{
  int i;

  for (i = 0; i < num_seq_scans; i++)
  {
    if (seq_scan_tables[i] == hashp)
    {
      return true;
    }
  }
  return false;
}

                                                   
void
AtEOXact_HashTables(bool isCommit)
{
     
                                                                            
                                                                         
                        
     
                                                                           
                                                                            
                                      
     
  if (isCommit)
  {
    int i;

    for (i = 0; i < num_seq_scans; i++)
    {
      elog(WARNING, "leaked hash_seq_search scan for hash table %p", seq_scan_tables[i]);
    }
  }
  num_seq_scans = 0;
}

                                                      
void
AtEOSubXact_HashTables(bool isCommit, int nestDepth)
{
  int i;

     
                                                                            
                                                                        
                                 
     
  for (i = num_seq_scans - 1; i >= 0; i--)
  {
    if (seq_scan_level[i] >= nestDepth)
    {
      if (isCommit)
      {
        elog(WARNING, "leaked hash_seq_search scan for hash table %p", seq_scan_tables[i]);
      }
      seq_scan_tables[i] = seq_scan_tables[num_seq_scans - 1];
      seq_scan_level[i] = seq_scan_level[num_seq_scans - 1];
      num_seq_scans--;
    }
  }
}
