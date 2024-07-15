                                                                            
   
         
                                           
   
                                                                         
                                                                 
                                                                           
                                                                         
                                                                         
                                                                       
                                                                      
                                                                       
                                                                      
                                                
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>
#ifndef WIN32
#include <sys/mman.h>
#endif
#include <sys/stat.h>

#include "lib/ilist.h"
#include "miscadmin.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/pg_shmem.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/resowner_private.h"

#define PG_DYNSHMEM_CONTROL_MAGIC 0x9a503d32

#define PG_DYNSHMEM_FIXED_SLOTS 64
#define PG_DYNSHMEM_SLOTS_PER_BACKEND 5

#define INVALID_CONTROL_SLOT ((uint32)-1)

                                                     
typedef struct dsm_segment_detach_callback
{
  on_dsm_detach_callback function;
  Datum arg;
  slist_node node;
} dsm_segment_detach_callback;

                                                              
struct dsm_segment
{
  dlist_node node;                                            
  ResourceOwner resowner;                      
  dsm_handle handle;                         
  uint32 control_slot;                                  
  void *impl_private;                                                
  void *mapped_address;                                              
  Size mapped_size;                                 
  slist_head on_detach;                             
};

                                                              
typedef struct dsm_control_item
{
  dsm_handle handle;
  uint32 refcnt;                                                         
  void *impl_private_pm_handle;                             
  bool pinned;
} dsm_control_item;

                                                          
typedef struct dsm_control_header
{
  uint32 magic;
  uint32 nitems;
  uint32 maxitems;
  dsm_control_item item[FLEXIBLE_ARRAY_MEMBER];
} dsm_control_header;

static void
dsm_cleanup_for_mmap(void);
static void
dsm_postmaster_shutdown(int code, Datum arg);
static dsm_segment *
dsm_create_descriptor(void);
static bool
dsm_control_segment_sane(dsm_control_header *control, Size mapped_size);
static uint64
dsm_control_bytes_needed(uint32 nitems);

                                                                        
static bool dsm_init_done = false;

   
                                                                
   
                                                                       
                                                                          
             
   
                                                                          
                                                                        
                                                                        
                                                                     
                                                                        
                                                                       
                                                                          
                                                                    
                                                                         
   
static dlist_head dsm_segment_list = DLIST_STATIC_INIT(dsm_segment_list);

   
                                
   
                                                                      
                                                                    
                                                                             
   
static dsm_handle dsm_control_handle;
static dsm_control_header *dsm_control;
static Size dsm_control_mapped_size = 0;
static void *dsm_control_impl_private = NULL;

   
                                              
   
                                                                        
                 
   
void
dsm_postmaster_startup(PGShmemHeader *shim)
{
  void *dsm_control_address = NULL;
  uint32 maxitems;
  Size segsize;

  Assert(!IsUnderPostmaster);

     
                                                                      
                                                                         
                                                            
                                        
     
  if (dynamic_shared_memory_type == DSM_IMPL_MMAP)
  {
    dsm_cleanup_for_mmap();
  }

                                               
  maxitems = PG_DYNSHMEM_FIXED_SLOTS + PG_DYNSHMEM_SLOTS_PER_BACKEND * MaxBackends;
  elog(DEBUG2, "dynamic shared memory system will support %u segments", maxitems);
  segsize = dsm_control_bytes_needed(maxitems);

     
                                                                             
                                                                            
                                                                     
              
     
  for (;;)
  {
    Assert(dsm_control_address == NULL);
    Assert(dsm_control_mapped_size == 0);
    dsm_control_handle = random();
    if (dsm_control_handle == DSM_HANDLE_INVALID)
    {
      continue;
    }
    if (dsm_impl_op(DSM_OP_CREATE, dsm_control_handle, segsize, &dsm_control_impl_private, &dsm_control_address, &dsm_control_mapped_size, ERROR))
    {
      break;
    }
  }
  dsm_control = dsm_control_address;
  on_shmem_exit(dsm_postmaster_shutdown, PointerGetDatum(shim));
  elog(DEBUG2, "created dynamic shared memory control segment %u (%zu bytes)", dsm_control_handle, segsize);
  shim->dsm_control = dsm_control_handle;

                                   
  dsm_control->magic = PG_DYNSHMEM_CONTROL_MAGIC;
  dsm_control->nitems = 0;
  dsm_control->maxitems = maxitems;
}

   
                                                                      
                                                                     
                                                                     
   
void
dsm_cleanup_using_control_segment(dsm_handle old_control_handle)
{
  void *mapped_address = NULL;
  void *junk_mapped_address = NULL;
  void *impl_private = NULL;
  void *junk_impl_private = NULL;
  Size mapped_size = 0;
  Size junk_mapped_size = 0;
  uint32 nitems;
  uint32 i;
  dsm_control_header *old_control;

     
                                                                            
                                                                      
                                                                             
                  
     
  if (!dsm_impl_op(DSM_OP_ATTACH, old_control_handle, 0, &impl_private, &mapped_address, &mapped_size, DEBUG1))
  {
    return;
  }

     
                                                                          
                                                      
     
  old_control = (dsm_control_header *)mapped_address;
  if (!dsm_control_segment_sane(old_control, mapped_size))
  {
    dsm_impl_op(DSM_OP_DETACH, old_control_handle, 0, &impl_private, &mapped_address, &mapped_size, LOG);
    return;
  }

     
                                                                            
                                                 
     
  nitems = old_control->nitems;
  for (i = 0; i < nitems; ++i)
  {
    dsm_handle handle;
    uint32 refcnt;

                                                                   
    refcnt = old_control->item[i].refcnt;
    if (refcnt == 0)
    {
      continue;
    }

                                    
    handle = old_control->item[i].handle;
    elog(DEBUG2, "cleaning up orphaned dynamic shared memory with ID %u (reference count %u)", handle, refcnt);

                                         
    dsm_impl_op(DSM_OP_DESTROY, handle, 0, &junk_impl_private, &junk_mapped_address, &junk_mapped_size, LOG);
  }

                                             
  elog(DEBUG2, "cleaning up dynamic shared memory control segment with ID %u", old_control_handle);
  dsm_impl_op(DSM_OP_DESTROY, old_control_handle, 0, &impl_private, &mapped_address, &mapped_size, LOG);
}

   
                                                                           
                                                                     
                                                                           
                                                                        
                                                                          
                                                                          
                                                                             
                                                                        
   
static void
dsm_cleanup_for_mmap(void)
{
  DIR *dir;
  struct dirent *dent;

                                                                           
  dir = AllocateDir(PG_DYNSHMEM_DIR);

  while ((dent = ReadDir(dir, PG_DYNSHMEM_DIR)) != NULL)
  {
    if (strncmp(dent->d_name, PG_DYNSHMEM_MMAP_FILE_PREFIX, strlen(PG_DYNSHMEM_MMAP_FILE_PREFIX)) == 0)
    {
      char buf[MAXPGPATH + sizeof(PG_DYNSHMEM_DIR)];

      snprintf(buf, sizeof(buf), PG_DYNSHMEM_DIR "/%s", dent->d_name);

      elog(DEBUG2, "removing file \"%s\"", buf);

                                                   
      if (unlink(buf) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", buf)));
      }
    }
  }

                         
  FreeDir(dir);
}

   
                                                                        
                                                                             
                                                                             
                     
   
static void
dsm_postmaster_shutdown(int code, Datum arg)
{
  uint32 nitems;
  uint32 i;
  void *dsm_control_address;
  void *junk_mapped_address = NULL;
  void *junk_impl_private = NULL;
  Size junk_mapped_size = 0;
  PGShmemHeader *shim = (PGShmemHeader *)DatumGetPointer(arg);

     
                                                                         
                                                                           
                                                                          
                                                                             
                              
     
  nitems = dsm_control->nitems;
  if (!dsm_control_segment_sane(dsm_control, dsm_control_mapped_size))
  {
    ereport(LOG, (errmsg("dynamic shared memory control segment is corrupt")));
    return;
  }

                                      
  for (i = 0; i < nitems; ++i)
  {
    dsm_handle handle;

                                                                   
    if (dsm_control->item[i].refcnt == 0)
    {
      continue;
    }

                                    
    handle = dsm_control->item[i].handle;
    elog(DEBUG2, "cleaning up orphaned dynamic shared memory with ID %u", handle);

                              
    dsm_impl_op(DSM_OP_DESTROY, handle, 0, &junk_impl_private, &junk_mapped_address, &junk_mapped_size, LOG);
  }

                                          
  elog(DEBUG2, "cleaning up dynamic shared memory control segment with ID %u", dsm_control_handle);
  dsm_control_address = dsm_control;
  dsm_impl_op(DSM_OP_DESTROY, dsm_control_handle, 0, &dsm_control_impl_private, &dsm_control_address, &dsm_control_mapped_size, LOG);
  dsm_control = dsm_control_address;
  shim->dsm_control = 0;
}

   
                                                                              
                                                                              
                                                                       
   
static void
dsm_backend_startup(void)
{
#ifdef EXEC_BACKEND
  {
    void *control_address = NULL;

                                 
    Assert(dsm_control_handle != 0);
    dsm_impl_op(DSM_OP_ATTACH, dsm_control_handle, 0, &dsm_control_impl_private, &control_address, &dsm_control_mapped_size, ERROR);
    dsm_control = control_address;
                                                                         
    if (!dsm_control_segment_sane(dsm_control, dsm_control_mapped_size))
    {
      dsm_impl_op(DSM_OP_DETACH, dsm_control_handle, 0, &dsm_control_impl_private, &control_address, &dsm_control_mapped_size, WARNING);
      ereport(FATAL, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("dynamic shared memory control segment is not valid")));
    }
  }
#endif

  dsm_init_done = true;
}

#ifdef EXEC_BACKEND
   
                                                                         
                                                                           
                             
   
void
dsm_set_control_handle(dsm_handle h)
{
  Assert(dsm_control_handle == 0 && h != 0);
  dsm_control_handle = h;
}
#endif

   
                                               
   
                                                                              
                                                                         
                                                                         
                                                                   
                                                                       
                                                                          
   
dsm_segment *
dsm_create(Size size, int flags)
{
  dsm_segment *seg;
  uint32 i;
  uint32 nitems;

                                                                      
  Assert(IsUnderPostmaster);

  if (!dsm_init_done)
  {
    dsm_backend_startup();
  }

                                        
  seg = dsm_create_descriptor();

                                                        
  for (;;)
  {
    Assert(seg->mapped_address == NULL && seg->mapped_size == 0);
    seg->handle = random();
    if (seg->handle == DSM_HANDLE_INVALID)                       
    {
      continue;
    }
    if (dsm_impl_op(DSM_OP_CREATE, seg->handle, size, &seg->impl_private, &seg->mapped_address, &seg->mapped_size, ERROR))
    {
      break;
    }
  }

                                                                    
  LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);

                                                      
  nitems = dsm_control->nitems;
  for (i = 0; i < nitems; ++i)
  {
    if (dsm_control->item[i].refcnt == 0)
    {
      dsm_control->item[i].handle = seg->handle;
                                                           
      dsm_control->item[i].refcnt = 2;
      dsm_control->item[i].impl_private_pm_handle = NULL;
      dsm_control->item[i].pinned = false;
      seg->control_slot = i;
      LWLockRelease(DynamicSharedMemoryControlLock);
      return seg;
    }
  }

                                                         
  if (nitems >= dsm_control->maxitems)
  {
    LWLockRelease(DynamicSharedMemoryControlLock);
    dsm_impl_op(DSM_OP_DESTROY, seg->handle, 0, &seg->impl_private, &seg->mapped_address, &seg->mapped_size, WARNING);
    if (seg->resowner != NULL)
    {
      ResourceOwnerForgetDSM(seg->resowner, seg);
    }
    dlist_delete(&seg->node);
    pfree(seg);

    if ((flags & DSM_CREATE_NULL_IF_MAXSEGMENTS) != 0)
    {
      return NULL;
    }
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("too many dynamic shared memory segments")));
  }

                                               
  dsm_control->item[nitems].handle = seg->handle;
                                                       
  dsm_control->item[nitems].refcnt = 2;
  dsm_control->item[nitems].impl_private_pm_handle = NULL;
  dsm_control->item[nitems].pinned = false;
  seg->control_slot = nitems;
  dsm_control->nitems++;
  LWLockRelease(DynamicSharedMemoryControlLock);

  return seg;
}

   
                                           
   
                                                                        
                           
   
                                                                            
                                                                           
                                                                         
                 
   
                                                                        
                                                                               
                                                                              
                                                                             
   
dsm_segment *
dsm_attach(dsm_handle h)
{
  dsm_segment *seg;
  dlist_iter iter;
  uint32 i;
  uint32 nitems;

                                                                      
  Assert(IsUnderPostmaster);

  if (!dsm_init_done)
  {
    dsm_backend_startup();
  }

     
                                                                       
                                                                             
                                                                            
                              
     
                                                                           
                                                                            
                       
     
  dlist_foreach(iter, &dsm_segment_list)
  {
    seg = dlist_container(dsm_segment, node, iter.cur);
    if (seg->handle == h)
    {
      elog(ERROR, "can't attach the same segment more than once");
    }
  }

                                        
  seg = dsm_create_descriptor();
  seg->handle = h;

                                                               
  LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);
  nitems = dsm_control->nitems;
  for (i = 0; i < nitems; ++i)
  {
       
                                                                         
                                                                          
                                                                         
                                                                    
                                                 
       
    if (dsm_control->item[i].refcnt <= 1)
    {
      continue;
    }

                                                                 
    if (dsm_control->item[i].handle != seg->handle)
    {
      continue;
    }

                                        
    dsm_control->item[i].refcnt++;
    seg->control_slot = i;
    break;
  }
  LWLockRelease(DynamicSharedMemoryControlLock);

     
                                                                            
                                                                           
                                                                        
                                             
     
  if (seg->control_slot == INVALID_CONTROL_SLOT)
  {
    dsm_detach(seg);
    return NULL;
  }

                                                        
  dsm_impl_op(DSM_OP_ATTACH, seg->handle, 0, &seg->impl_private, &seg->mapped_address, &seg->mapped_size, ERROR);

  return seg;
}

   
                                                                          
                                                                        
                                                                  
   
void
dsm_backend_shutdown(void)
{
  while (!dlist_is_empty(&dsm_segment_list))
  {
    dsm_segment *seg;

    seg = dlist_head_element(dsm_segment, node, &dsm_segment_list);
    dsm_detach(seg);
  }
}

   
                                                                            
                                                                        
                                                                          
                  
   
void
dsm_detach_all(void)
{
  void *control_address = dsm_control;

  while (!dlist_is_empty(&dsm_segment_list))
  {
    dsm_segment *seg;

    seg = dlist_head_element(dsm_segment, node, &dsm_segment_list);
    dsm_detach(seg);
  }

  if (control_address != NULL)
  {
    dsm_impl_op(DSM_OP_DETACH, dsm_control_handle, 0, &dsm_control_impl_private, &control_address, &dsm_control_mapped_size, ERROR);
  }
}

   
                                                                     
                              
   
                                                                            
                                                                           
                                                                            
                                                                            
   
void
dsm_detach(dsm_segment *seg)
{
     
                                                                       
                                                                       
                                                                         
                                                                         
                                                                             
                                     
     
  HOLD_INTERRUPTS();
  while (!slist_is_empty(&seg->on_detach))
  {
    slist_node *node;
    dsm_segment_detach_callback *cb;
    on_dsm_detach_callback function;
    Datum arg;

    node = slist_pop_head_node(&seg->on_detach);
    cb = slist_container(dsm_segment_detach_callback, node, node);
    function = cb->function;
    arg = cb->arg;
    pfree(cb);

    function(seg, arg);
  }
  RESUME_INTERRUPTS();

     
                                                                             
                                                                           
                                                                           
                                                                         
                                                                       
                                                                
     
  if (seg->mapped_address != NULL)
  {
    dsm_impl_op(DSM_OP_DETACH, seg->handle, 0, &seg->impl_private, &seg->mapped_address, &seg->mapped_size, WARNING);
    seg->impl_private = NULL;
    seg->mapped_address = NULL;
    seg->mapped_size = 0;
  }

                                                              
  if (seg->control_slot != INVALID_CONTROL_SLOT)
  {
    uint32 refcnt;
    uint32 control_slot = seg->control_slot;

    LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);
    Assert(dsm_control->item[control_slot].handle == seg->handle);
    Assert(dsm_control->item[control_slot].refcnt > 1);
    refcnt = --dsm_control->item[control_slot].refcnt;
    seg->control_slot = INVALID_CONTROL_SLOT;
    LWLockRelease(DynamicSharedMemoryControlLock);

                                                                  
    if (refcnt == 1)
    {
                                                  
      Assert(!dsm_control->item[control_slot].pinned);

         
                                                                         
                                                                      
                                                                   
                                                                       
                                                                       
                  
         
                                                                     
                                                                    
                                                                     
                                                                     
                                                                        
                                                                 
         
      if (dsm_impl_op(DSM_OP_DESTROY, seg->handle, 0, &seg->impl_private, &seg->mapped_address, &seg->mapped_size, WARNING))
      {
        LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);
        Assert(dsm_control->item[control_slot].handle == seg->handle);
        Assert(dsm_control->item[control_slot].refcnt == 1);
        dsm_control->item[control_slot].refcnt = 0;
        LWLockRelease(DynamicSharedMemoryControlLock);
      }
    }
  }

                                                               
  if (seg->resowner != NULL)
  {
    ResourceOwnerForgetDSM(seg->resowner, seg);
  }
  dlist_delete(&seg->node);
  pfree(seg);
}

   
                                                              
   
                                                                       
                                                                           
         
   
void
dsm_pin_mapping(dsm_segment *seg)
{
  if (seg->resowner != NULL)
  {
    ResourceOwnerForgetDSM(seg->resowner, seg);
    seg->resowner = NULL;
  }
}

   
                                                                      
   
                                                                      
                                                                       
                                                                        
                                                                          
                                   
   
void
dsm_unpin_mapping(dsm_segment *seg)
{
  Assert(seg->resowner == NULL);
  ResourceOwnerEnlargeDSMs(CurrentResourceOwner);
  seg->resowner = CurrentResourceOwner;
  ResourceOwnerRememberDSM(seg->resowner, seg);
}

   
                                                                            
                                
   
                                                                             
                                                                           
   
                                                                       
                                                                      
                                                                    
                       
   
void
dsm_pin_segment(dsm_segment *seg)
{
  void *handle;

     
                                                                       
                                                                       
                                                                           
               
     
  LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);
  if (dsm_control->item[seg->control_slot].pinned)
  {
    elog(ERROR, "cannot pin a segment that is already pinned");
  }
  dsm_impl_pin_segment(seg->handle, seg->impl_private, &handle);
  dsm_control->item[seg->control_slot].pinned = true;
  dsm_control->item[seg->control_slot].refcnt++;
  dsm_control->item[seg->control_slot].impl_private_pm_handle = handle;
  LWLockRelease(DynamicSharedMemoryControlLock);
}

   
                                                                         
                                                                               
                                           
   
                                                                           
                                                                           
                                                                              
                                                                      
                                                                 
   
void
dsm_unpin_segment(dsm_handle handle)
{
  uint32 control_slot = INVALID_CONTROL_SLOT;
  bool destroy = false;
  uint32 i;

                                                   
  LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);
  for (i = 0; i < dsm_control->nitems; ++i)
  {
                                                                          
    if (dsm_control->item[i].refcnt <= 1)
    {
      continue;
    }

                                                           
    if (dsm_control->item[i].handle == handle)
    {
      control_slot = i;
      break;
    }
  }

     
                                                                            
                                                                        
                                          
     
  if (control_slot == INVALID_CONTROL_SLOT)
  {
    elog(ERROR, "cannot unpin unknown segment handle");
  }
  if (!dsm_control->item[control_slot].pinned)
  {
    elog(ERROR, "cannot unpin a segment that is not pinned");
  }
  Assert(dsm_control->item[control_slot].refcnt > 1);

     
                                                                           
                                                                            
                             
     
  dsm_impl_unpin_segment(handle, &dsm_control->item[control_slot].impl_private_pm_handle);

                                                              
  if (--dsm_control->item[control_slot].refcnt == 1)
  {
    destroy = true;
  }
  dsm_control->item[control_slot].pinned = false;

                                    
  LWLockRelease(DynamicSharedMemoryControlLock);

                                                          
  if (destroy)
  {
    void *junk_impl_private = NULL;
    void *junk_mapped_address = NULL;
    Size junk_mapped_size = 0;

       
                                                                        
                                                                      
                                                                           
                                                                           
                                                                           
                                                                        
                                                                      
             
       
    if (dsm_impl_op(DSM_OP_DESTROY, handle, 0, &junk_impl_private, &junk_mapped_address, &junk_mapped_size, WARNING))
    {
      LWLockAcquire(DynamicSharedMemoryControlLock, LW_EXCLUSIVE);
      Assert(dsm_control->item[control_slot].handle == handle);
      Assert(dsm_control->item[control_slot].refcnt == 1);
      dsm_control->item[control_slot].refcnt = 0;
      LWLockRelease(DynamicSharedMemoryControlLock);
    }
  }
}

   
                                                                          
   
dsm_segment *
dsm_find_mapping(dsm_handle h)
{
  dlist_iter iter;
  dsm_segment *seg;

  dlist_foreach(iter, &dsm_segment_list)
  {
    seg = dlist_container(dsm_segment, node, iter.cur);
    if (seg->handle == h)
    {
      return seg;
    }
  }

  return NULL;
}

   
                                                                       
   
void *
dsm_segment_address(dsm_segment *seg)
{
  Assert(seg->mapped_address != NULL);
  return seg->mapped_address;
}

   
                              
   
Size
dsm_segment_map_length(dsm_segment *seg)
{
  Assert(seg->mapped_address != NULL);
  return seg->mapped_size;
}

   
                               
   
                                                                              
                                                                        
                                                                          
                                                                
                                                                      
                                                                          
                                     
   
dsm_handle
dsm_segment_handle(dsm_segment *seg)
{
  return seg->handle;
}

   
                                                                       
   
void
on_dsm_detach(dsm_segment *seg, on_dsm_detach_callback function, Datum arg)
{
  dsm_segment_detach_callback *cb;

  cb = MemoryContextAlloc(TopMemoryContext, sizeof(dsm_segment_detach_callback));
  cb->function = function;
  cb->arg = arg;
  slist_push_head(&seg->on_detach, &cb->node);
}

   
                                                                         
   
void
cancel_on_dsm_detach(dsm_segment *seg, on_dsm_detach_callback function, Datum arg)
{
  slist_mutable_iter iter;

  slist_foreach_modify(iter, &seg->on_detach)
  {
    dsm_segment_detach_callback *cb;

    cb = slist_container(dsm_segment_detach_callback, node, iter.cur);
    if (cb->function == function && cb->arg == arg)
    {
      slist_delete_current(&iter);
      pfree(cb);
      break;
    }
  }
}

   
                                                                      
   
void
reset_on_dsm_detach(void)
{
  dlist_iter iter;

  dlist_foreach(iter, &dsm_segment_list)
  {
    dsm_segment *seg = dlist_container(dsm_segment, node, iter.cur);

                                                           
    while (!slist_is_empty(&seg->on_detach))
    {
      slist_node *node;
      dsm_segment_detach_callback *cb;

      node = slist_pop_head_node(&seg->on_detach);
      cb = slist_container(dsm_segment_detach_callback, node, node);
      pfree(cb);
    }

       
                                                                        
                                                   
       
    seg->control_slot = INVALID_CONTROL_SLOT;
  }
}

   
                                
   
static dsm_segment *
dsm_create_descriptor(void)
{
  dsm_segment *seg;

  if (CurrentResourceOwner)
  {
    ResourceOwnerEnlargeDSMs(CurrentResourceOwner);
  }

  seg = MemoryContextAlloc(TopMemoryContext, sizeof(dsm_segment));
  dlist_push_head(&dsm_segment_list, &seg->node);

                                                     
  seg->control_slot = INVALID_CONTROL_SLOT;
  seg->impl_private = NULL;
  seg->mapped_address = NULL;
  seg->mapped_size = 0;

  seg->resowner = CurrentResourceOwner;
  if (CurrentResourceOwner)
  {
    ResourceOwnerRememberDSM(CurrentResourceOwner, seg);
  }

  slist_init(&seg->on_detach);

  return seg;
}

   
                                   
   
                                                                              
                                                                              
                                                                               
                                                                           
                                                                            
                        
   
static bool
dsm_control_segment_sane(dsm_control_header *control, Size mapped_size)
{
  if (mapped_size < offsetof(dsm_control_header, item))
  {
    return false;                                            
  }
  if (control->magic != PG_DYNSHMEM_CONTROL_MAGIC)
  {
    return false;                                  
  }
  if (dsm_control_bytes_needed(control->maxitems) > mapped_size)
  {
    return false;                                       
  }
  if (control->nitems > control->maxitems)
  {
    return false;                
  }
  return true;
}

   
                                                                       
                    
   
static uint64
dsm_control_bytes_needed(uint32 nitems)
{
  return offsetof(dsm_control_header, item) + sizeof(dsm_control_item) * (uint64)nitems;
}
