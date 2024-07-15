                                                                            
   
             
                                             
   
                                                                         
                                                                        
   
                                     
   
                                                                            
   

#include "postgres.h"

#include "port/atomics.h"
#include "storage/shm_toc.h"
#include "storage/spin.h"

typedef struct shm_toc_entry
{
  uint64 key;                            
  Size offset;                                       
} shm_toc_entry;

struct shm_toc
{
  uint64 toc_magic;                                                
  slock_t toc_mutex;                                           
  Size toc_total_bytes;                                    
  Size toc_allocated_bytes;                                       
  uint32 toc_nentry;                                      
  shm_toc_entry toc_entry[FLEXIBLE_ARRAY_MEMBER];
};

   
                                                                  
   
shm_toc *
shm_toc_create(uint64 magic, void *address, Size nbytes)
{
  shm_toc *toc = (shm_toc *)address;

  Assert(nbytes > offsetof(shm_toc, toc_entry));
  toc->toc_magic = magic;
  SpinLockInit(&toc->toc_mutex);

     
                                                                        
                              
     
  toc->toc_total_bytes = BUFFERALIGN_DOWN(nbytes);
  toc->toc_allocated_bytes = 0;
  toc->toc_nentry = 0;

  return toc;
}

   
                                                                          
                                                                   
   
shm_toc *
shm_toc_attach(uint64 magic, void *address)
{
  shm_toc *toc = (shm_toc *)address;

  if (toc->toc_magic != magic)
  {
    return NULL;
  }

  Assert(toc->toc_total_bytes >= toc->toc_allocated_bytes);
  Assert(toc->toc_total_bytes > offsetof(shm_toc, toc_entry));

  return toc;
}

   
                                                                         
   
                                                                            
                                                                               
                                                   
   
                                                                              
                                                   
   
void *
shm_toc_allocate(shm_toc *toc, Size nbytes)
{
  volatile shm_toc *vtoc = toc;
  Size total_bytes;
  Size allocated_bytes;
  Size nentry;
  Size toc_bytes;

     
                                                                      
                                                                       
                                                                    
                                     
     
  nbytes = BUFFERALIGN(nbytes);

  SpinLockAcquire(&toc->toc_mutex);

  total_bytes = vtoc->toc_total_bytes;
  allocated_bytes = vtoc->toc_allocated_bytes;
  nentry = vtoc->toc_nentry;
  toc_bytes = offsetof(shm_toc, toc_entry) + nentry * sizeof(shm_toc_entry) + allocated_bytes;

                                                 
  if (toc_bytes + nbytes > total_bytes || toc_bytes + nbytes < toc_bytes)
  {
    SpinLockRelease(&toc->toc_mutex);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory")));
  }
  vtoc->toc_allocated_bytes += nbytes;

  SpinLockRelease(&toc->toc_mutex);

  return ((char *)toc) + (total_bytes - allocated_bytes - nbytes);
}

   
                                                           
   
Size
shm_toc_freespace(shm_toc *toc)
{
  volatile shm_toc *vtoc = toc;
  Size total_bytes;
  Size allocated_bytes;
  Size nentry;
  Size toc_bytes;

  SpinLockAcquire(&toc->toc_mutex);
  total_bytes = vtoc->toc_total_bytes;
  allocated_bytes = vtoc->toc_allocated_bytes;
  nentry = vtoc->toc_nentry;
  SpinLockRelease(&toc->toc_mutex);

  toc_bytes = offsetof(shm_toc, toc_entry) + nentry * sizeof(shm_toc_entry);
  Assert(allocated_bytes + BUFFERALIGN(toc_bytes) <= total_bytes);
  return total_bytes - (allocated_bytes + BUFFERALIGN(toc_bytes));
}

   
                       
   
                                                                               
                                                                           
                                                                               
                                                                           
                                                                
                                                                        
   
                                                                               
                                                                        
   
                                                                           
                                                                                
                                                                                
                                                                          
                                                                              
                                   
   
void
shm_toc_insert(shm_toc *toc, uint64 key, void *address)
{
  volatile shm_toc *vtoc = toc;
  Size total_bytes;
  Size allocated_bytes;
  Size nentry;
  Size toc_bytes;
  Size offset;

                           
  Assert(address > (void *)toc);
  offset = ((char *)address) - (char *)toc;

  SpinLockAcquire(&toc->toc_mutex);

  total_bytes = vtoc->toc_total_bytes;
  allocated_bytes = vtoc->toc_allocated_bytes;
  nentry = vtoc->toc_nentry;
  toc_bytes = offsetof(shm_toc, toc_entry) + nentry * sizeof(shm_toc_entry) + allocated_bytes;

                                                 
  if (toc_bytes + sizeof(shm_toc_entry) > total_bytes || toc_bytes + sizeof(shm_toc_entry) < toc_bytes || nentry >= PG_UINT32_MAX)
  {
    SpinLockRelease(&toc->toc_mutex);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory")));
  }

  Assert(offset < total_bytes);
  vtoc->toc_entry[nentry].key = key;
  vtoc->toc_entry[nentry].offset = offset;

     
                                                                      
                                                                     
               
     
  pg_write_barrier();

  vtoc->toc_nentry++;

  SpinLockRelease(&toc->toc_mutex);
}

   
                        
   
                                                                       
                       
   
                                                                             
                                                                                
                                                                          
                                                                          
                                                                            
   
void *
shm_toc_lookup(shm_toc *toc, uint64 key, bool noError)
{
  uint32 nentry;
  uint32 i;

     
                                                                             
                                 
     
  nentry = toc->toc_nentry;
  pg_read_barrier();

                                        
  for (i = 0; i < nentry; ++i)
  {
    if (toc->toc_entry[i].key == key)
    {
      return ((char *)toc) + toc->toc_entry[i].offset;
    }
  }

                                    
  if (!noError)
  {
    elog(ERROR, "could not find key " UINT64_FORMAT " in shm TOC at %p", key, toc);
  }
  return NULL;
}

   
                                                                           
                              
   
Size
shm_toc_estimate(shm_toc_estimator *e)
{
  Size sz;

  sz = offsetof(shm_toc, toc_entry);
  sz = add_size(sz, mul_size(e->number_of_keys, sizeof(shm_toc_entry)));
  sz = add_size(sz, e->space_for_chunks);

  return BUFFERALIGN(sz);
}
