                                                                            
   
         
                                  
   
                                                                              
                                                                               
                                                                             
                                                                              
                                                                               
                                                                               
                                                                            
                                                                               
                                                                               
                                                                           
                           
   
                                                                       
                                                                             
                                                                    
                                                                             
                                                                              
                                                                             
                                                                              
                                                                               
                                                                              
                                                                              
                                                                             
                                                                            
                                                                              
                                                                               
                                                                           
                                                                         
                                                                            
                                                                             
                                                                        
                                                                   
                                                                             
                                                                          
                                                         
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include "port/atomics.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/dsa.h"
#include "utils/freepage.h"
#include "utils/memutils.h"

   
                                                                        
                                                                          
                                                                             
                              
   
#define DSA_INITIAL_SEGMENT_SIZE ((size_t)(1 * 1024 * 1024))

   
                                                                              
                                                                        
                                                                           
                                                                           
                                                 
   
#define DSA_NUM_SEGMENTS_AT_EACH_SIZE 2

   
                                                                          
                                                                     
                                                                     
   
#if SIZEOF_DSA_POINTER == 4
#define DSA_OFFSET_WIDTH 27                                      
#else
#define DSA_OFFSET_WIDTH 40                                      
#endif

   
                                                                          
                                                      
   
#define DSA_MAX_SEGMENTS Min(1024, (1 << ((SIZEOF_DSA_POINTER * 8) - DSA_OFFSET_WIDTH)))

                                                               
#define DSA_OFFSET_BITMASK (((dsa_pointer)1 << DSA_OFFSET_WIDTH) - 1)

                                        
#define DSA_MAX_SEGMENT_SIZE ((size_t)1 << DSA_OFFSET_WIDTH)

                                                                 
#define DSA_PAGES_PER_SUPERBLOCK 16

   
                                                                              
                                                                     
                       
   
#define DSA_SEGMENT_HEADER_MAGIC 0x0ce26608

                                                            
#define DSA_MAKE_POINTER(segment_number, offset) (((dsa_pointer)(segment_number) << DSA_OFFSET_WIDTH) | (offset))

                                                    
#define DSA_EXTRACT_SEGMENT_NUMBER(dp) ((dp) >> DSA_OFFSET_WIDTH)

                                            
#define DSA_EXTRACT_OFFSET(dp) ((dp) & DSA_OFFSET_BITMASK)

                                                           
typedef size_t dsa_segment_index;

                                                                      
#define DSA_SEGMENT_INDEX_NONE (~(dsa_segment_index)0)

   
                                                                          
                                                           
   
#define DSA_NUM_SEGMENT_BINS 16

   
                                                                             
                                                                            
                                                        
   
#define contiguous_pages_to_segment_bin(n) Min(fls(n), DSA_NUM_SEGMENT_BINS - 1)

                                 
#define DSA_AREA_LOCK(area) (&area->control->lock)
#define DSA_SCLASS_LOCK(area, sclass) (&area->control->pools[sclass].lock)

   
                                                                              
                                                                             
                                            
   
typedef struct
{
                                 
  uint32 magic;
                                                                        
  size_t usable_pages;
                                            
  size_t size;

     
                                                                             
                                                      
     
  dsa_segment_index prev;

     
                                                                            
                                                     
     
  dsa_segment_index next;
                                                        
  size_t bin;

     
                                                                          
                                             
     
  bool freed;
} dsa_segment_header;

   
                                
   
                                                                           
                                                                              
                                                                           
                                                                       
                                                                             
                                                                          
                                                                       
                                                                             
                                                        
   
typedef struct
{
  dsa_pointer pool;                           
  dsa_pointer prevspan;                     
  dsa_pointer nextspan;                 
  dsa_pointer start;                           
  size_t npages;                                      
  uint16 size_class;                     
  uint16 ninitialized;                                                 
  uint16 nallocatable;                                                
  uint16 firstfree;                                     
  uint16 nmax;                                                        
  uint16 fclass;                                     
} dsa_area_span;

   
                                                                             
                                                                      
   
#define NextFreeObjectIndex(object) (*(uint16 *)(object))

   
                                                                           
                                                                        
                                                                               
                                                                             
                                                                             
                                                                         
                                                                             
   
                                                                                
                                                                             
                             
   
                                                                               
                                                                            
                                                                             
                                                                     
                                                                           
                                                                      
                                
   
static const uint16 dsa_size_classes[] = {
    sizeof(dsa_area_span), 0,                                
    8, 16, 24, 32, 40, 48, 56, 64,                                     
    80, 96, 112, 128,                                                   
    160, 192, 224, 256,                                                 
    320, 384, 448, 512,                                                 
    640, 768, 896, 1024,                                                 
    1280, 1560, 1816, 2048,                                               
    2616, 3120, 3640, 4096,                                               
    5456, 6552, 7280, 8192                                                 
};
#define DSA_NUM_SIZE_CLASSES lengthof(dsa_size_classes)

                           
#define DSA_SCLASS_BLOCK_OF_SPANS 0
#define DSA_SCLASS_SPAN_LARGE 1

   
                                                                       
                                                                          
                                                                             
                          
   
static const uint8 dsa_size_class_map[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25};
#define DSA_SIZE_CLASS_MAP_QUANTUM 8

   
                                                                          
                                                                   
                                                                       
                                        
   
#define DSA_FULLNESS_CLASSES 4

   
                                                                      
   
                                                                      
                                                        
   
typedef struct
{
                                              
  LWLock lock;
                                                             
  dsa_pointer spans[DSA_FULLNESS_CLASSES];
                                                       
} dsa_area_pool;

   
                                                                                
                                                  
   
typedef struct
{
                                                 
  dsa_segment_header segment_header;
                                 
  dsa_handle handle;
                                                       
  dsm_handle segment_handles[DSA_MAX_SEGMENTS];
                                                                          
  dsa_segment_index segment_bins[DSA_NUM_SEGMENT_BINS];
                                             
  dsa_area_pool pools[DSA_NUM_SIZE_CLASSES];
                                              
  size_t total_segment_size;
                                                                 
  size_t max_total_segment_size;
                                                               
  dsa_segment_index high_segment_index;
                                          
  int refcnt;
                                                         
  bool pinned;
                                                          
  size_t freed_segment_counter;
                              
  int lwlock_tranche_id;
                                                                   
  LWLock lock;
} dsa_area_control;

                                                    
#define DsaAreaPoolToDsaPointer(area, p) DSA_MAKE_POINTER(0, (char *)p - (char *)area->control)

   
                                                                         
                                                                             
                                                                         
                                                                              
           
   
typedef struct
{
  dsm_segment *segment;                        
  char *mapped_address;                                               
  dsa_segment_header *header;                                      
  FreePageManager *fpm;                                              
  dsa_pointer *pagemap;                                     
} dsa_segment_map;

   
                                                                          
                                                                          
                                                                            
                        
   
struct dsa_area
{
                                                       
  dsa_area_control *control;

                                    
  bool mapping_pinned;

     
                                                                    
                                                                             
                                                                         
                                                                       
     
  dsa_segment_map segment_maps[DSA_MAX_SEGMENTS];

                                                               
  dsa_segment_index high_segment_index;

                                                
  size_t freed_segment_counter;
};

#define DSA_SPAN_NOTHING_FREE ((uint16)-1)
#define DSA_SUPERBLOCK_SIZE (DSA_PAGES_PER_SUPERBLOCK * FPM_PAGE_SIZE)

                                                                      
#define get_segment_index(area, segment_map_ptr) (segment_map_ptr - &area->segment_maps[0])

static void
init_span(dsa_area *area, dsa_pointer span_pointer, dsa_area_pool *pool, dsa_pointer start, size_t npages, uint16 size_class);
static bool
transfer_first_span(dsa_area *area, dsa_area_pool *pool, int fromclass, int toclass);
static inline dsa_pointer
alloc_object(dsa_area *area, int size_class);
static bool
ensure_active_superblock(dsa_area *area, dsa_area_pool *pool, int size_class);
static dsa_segment_map *
get_segment_by_index(dsa_area *area, dsa_segment_index index);
static void
destroy_superblock(dsa_area *area, dsa_pointer span_pointer);
static void
unlink_span(dsa_area *area, dsa_area_span *span);
static void
add_span_to_fullness_class(dsa_area *area, dsa_area_span *span, dsa_pointer span_pointer, int fclass);
static void
unlink_segment(dsa_area *area, dsa_segment_map *segment_map);
static dsa_segment_map *
get_best_segment(dsa_area *area, size_t npages);
static dsa_segment_map *
make_new_segment(dsa_area *area, size_t requested_pages);
static dsa_area *
create_internal(void *place, size_t size, int tranche_id, dsm_handle control_handle, dsm_segment *control_segment);
static dsa_area *
attach_internal(void *place, dsm_segment *segment, dsa_handle handle);
static void
check_for_freed_segments(dsa_area *area);
static void
check_for_freed_segments_locked(dsa_area *area);

   
                                                                             
                                                           
   
                                                                               
                                                                              
                                                                            
                                         
   
dsa_area *
dsa_create(int tranche_id)
{
  dsm_segment *segment;
  dsa_area *area;

     
                                                                             
                                    
     
  segment = dsm_create(DSA_INITIAL_SEGMENT_SIZE, 0);

     
                                                                           
                                                                            
                                                                            
                                           
     
  dsm_pin_segment(segment);

                                                                      
  area = create_internal(dsm_segment_address(segment), DSA_INITIAL_SEGMENT_SIZE, tranche_id, dsm_segment_handle(segment), segment);

                                                   
  on_dsm_detach(segment, &dsa_on_dsm_detach_release_in_place, PointerGetDatum(dsm_segment_address(segment)));

  return area;
}

   
                                                                             
                                                                      
                                                                           
                                                                              
                           
   
                                                                          
                                                                        
                                                                              
                                                                               
                                                                           
            
   
                                                            
   
dsa_area *
dsa_create_in_place(void *place, size_t size, int tranche_id, dsm_segment *segment)
{
  dsa_area *area;

  area = create_internal(place, size, tranche_id, DSM_HANDLE_INVALID, NULL);

     
                                                                             
                   
     
  if (segment != NULL)
  {
    on_dsm_detach(segment, &dsa_on_dsm_detach_release_in_place, PointerGetDatum(place));
  }

  return area;
}

   
                                                                          
                                                                      
                        
   
dsa_handle
dsa_get_handle(dsa_area *area)
{
  Assert(area->control->handle != DSM_HANDLE_INVALID);
  return area->control->handle;
}

   
                                                                               
                                                                         
                         
   
dsa_area *
dsa_attach(dsa_handle handle)
{
  dsm_segment *segment;
  dsa_area *area;

     
                                                                             
                                     
     
  segment = dsm_attach(handle);
  if (segment == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not attach to dynamic shared area")));
  }

  area = attach_internal(dsm_segment_address(segment), segment, handle);

                                                   
  on_dsm_detach(segment, &dsa_on_dsm_detach_release_in_place, PointerGetDatum(dsm_segment_address(segment)));

  return area;
}

   
                                                                            
                                                                            
                                                                           
            
   
                                                                            
                                                                        
                                                                 
   
dsa_area *
dsa_attach_in_place(void *place, dsm_segment *segment)
{
  dsa_area *area;

  area = attach_internal(place, NULL, DSM_HANDLE_INVALID);

     
                                                                             
                   
     
  if (segment != NULL)
  {
    on_dsm_detach(segment, &dsa_on_dsm_detach_release_in_place, PointerGetDatum(place));
  }

  return area;
}

   
                                                                  
                                                                           
                                                                               
                                                                      
                                                                       
                                                                          
   
                                                                            
                                                                      
                                                                              
                    
   
void
dsa_on_dsm_detach_release_in_place(dsm_segment *segment, Datum place)
{
  dsa_release_in_place(DatumGetPointer(place));
}

   
                                                                  
                                                                        
                                                                      
                                                                              
                                                                               
                                                                          
   
void
dsa_on_shmem_exit_release_in_place(int code, Datum place)
{
  dsa_release_in_place(DatumGetPointer(place));
}

   
                                                                  
                                                                         
                                                                               
                                                            
   
                                                                         
                                           
   
void
dsa_release_in_place(void *place)
{
  dsa_area_control *control = (dsa_area_control *)place;
  int i;

  LWLockAcquire(&control->lock, LW_EXCLUSIVE);
  Assert(control->segment_header.magic == (DSA_SEGMENT_HEADER_MAGIC ^ control->handle ^ 0));
  Assert(control->refcnt > 0);
  if (--control->refcnt == 0)
  {
    for (i = 0; i <= control->high_segment_index; ++i)
    {
      dsm_handle handle;

      handle = control->segment_handles[i];
      if (handle != DSM_HANDLE_INVALID)
      {
        dsm_unpin_segment(handle);
      }
    }
  }
  LWLockRelease(&control->lock);
}

   
                                                                     
   
                                                                               
                                                    
   
void
dsa_pin_mapping(dsa_area *area)
{
  int i;

  Assert(!area->mapping_pinned);
  area->mapping_pinned = true;

  for (i = 0; i <= area->high_segment_index; ++i)
  {
    if (area->segment_maps[i].segment != NULL)
    {
      dsm_pin_mapping(area->segment_maps[i].segment);
    }
  }
}

   
                                                                            
                                                                           
                                                                          
                              
   
                                                                          
                            
   
                                                                          
                                                                            
                                                                            
   
                                                                            
                                                         
   
                                                           
                                                                       
                                                                            
          
   
dsa_pointer
dsa_allocate_extended(dsa_area *area, size_t size, int flags)
{
  uint16 size_class;
  dsa_pointer start_pointer;
  dsa_segment_map *segment_map;
  dsa_pointer result;

  Assert(size > 0);

                                                        
  if (((flags & DSA_ALLOC_HUGE) != 0 && !AllocHugeSizeIsValid(size)) || ((flags & DSA_ALLOC_HUGE) == 0 && !AllocSizeIsValid(size)))
  {
    elog(ERROR, "invalid DSA memory alloc request size %zu", size);
  }

     
                                                                          
                                                                         
                                                                       
                                                                          
                                                
     
  if (size > dsa_size_classes[lengthof(dsa_size_classes) - 1])
  {
    size_t npages = fpm_size_to_pages(size);
    size_t first_page;
    dsa_pointer span_pointer;
    dsa_area_pool *pool = &area->control->pools[DSA_SCLASS_SPAN_LARGE];

                               
    span_pointer = alloc_object(area, DSA_SCLASS_BLOCK_OF_SPANS);
    if (!DsaPointerIsValid(span_pointer))
    {
                                            
      if ((flags & DSA_ALLOC_NO_OOM) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on DSA request of size %zu.", size)));
      }
      return InvalidDsaPointer;
    }

    LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);

                                                
    segment_map = get_best_segment(area, npages);
    if (segment_map == NULL)
    {
      segment_map = make_new_segment(area, npages);
    }
    if (segment_map == NULL)
    {
                                                    
      LWLockRelease(DSA_AREA_LOCK(area));
      dsa_free(area, span_pointer);

                                            
      if ((flags & DSA_ALLOC_NO_OOM) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on DSA request of size %zu.", size)));
      }
      return InvalidDsaPointer;
    }

       
                                                                         
                                                                        
                                                                     
                                                                        
                                                                        
       
    if (!FreePageManagerGet(segment_map->fpm, npages, &first_page))
    {
      elog(FATAL, "dsa_allocate could not find %zu free pages", npages);
    }
    LWLockRelease(DSA_AREA_LOCK(area));

    start_pointer = DSA_MAKE_POINTER(get_segment_index(area, segment_map), first_page * FPM_PAGE_SIZE);

                                      
    LWLockAcquire(DSA_SCLASS_LOCK(area, DSA_SCLASS_SPAN_LARGE), LW_EXCLUSIVE);
    init_span(area, span_pointer, pool, start_pointer, npages, DSA_SCLASS_SPAN_LARGE);
    segment_map->pagemap[first_page] = span_pointer;
    LWLockRelease(DSA_SCLASS_LOCK(area, DSA_SCLASS_SPAN_LARGE));

                                                  
    if ((flags & DSA_ALLOC_ZERO) != 0)
    {
      memset(dsa_get_address(area, start_pointer), 0, size);
    }

    return start_pointer;
  }

                                       
  if (size < lengthof(dsa_size_class_map) * DSA_SIZE_CLASS_MAP_QUANTUM)
  {
    int mapidx;

                                                     
    mapidx = ((size + DSA_SIZE_CLASS_MAP_QUANTUM - 1) / DSA_SIZE_CLASS_MAP_QUANTUM) - 1;
    size_class = dsa_size_class_map[mapidx];
  }
  else
  {
    uint16 min;
    uint16 max;

                                                        
    min = dsa_size_class_map[lengthof(dsa_size_class_map) - 1];
    max = lengthof(dsa_size_classes) - 1;

    while (min < max)
    {
      uint16 mid = (min + max) / 2;
      uint16 class_size = dsa_size_classes[mid];

      if (class_size < size)
      {
        min = mid + 1;
      }
      else
      {
        max = mid;
      }
    }

    size_class = min;
  }
  Assert(size <= dsa_size_classes[size_class]);
  Assert(size_class == 0 || size > dsa_size_classes[size_class - 1]);

                                                                
  result = alloc_object(area, size_class);

                                      
  if (!DsaPointerIsValid(result))
  {
                                          
    if ((flags & DSA_ALLOC_NO_OOM) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on DSA request of size %zu.", size)));
    }
    return InvalidDsaPointer;
  }

                                                
  if ((flags & DSA_ALLOC_ZERO) != 0)
  {
    memset(dsa_get_address(area, result), 0, size);
  }

  return result;
}

   
                                           
   
void
dsa_free(dsa_area *area, dsa_pointer dp)
{
  dsa_segment_map *segment_map;
  int pageno;
  dsa_pointer span_pointer;
  dsa_area_span *span;
  char *superblock;
  char *object;
  size_t size;
  int size_class;

                                                                           
  check_for_freed_segments(area);

                                         
  segment_map = get_segment_by_index(area, DSA_EXTRACT_SEGMENT_NUMBER(dp));
  pageno = DSA_EXTRACT_OFFSET(dp) / FPM_PAGE_SIZE;
  span_pointer = segment_map->pagemap[pageno];
  span = dsa_get_address(area, span_pointer);
  superblock = dsa_get_address(area, span->start);
  object = dsa_get_address(area, dp);
  size_class = span->size_class;
  size = dsa_size_classes[size_class];

     
                                                                           
                                                                      
     
  if (span->size_class == DSA_SCLASS_SPAN_LARGE)
  {

#ifdef CLOBBER_FREED_MEMORY
    memset(object, 0x7f, span->npages * FPM_PAGE_SIZE);
#endif

                                               
    LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
    FreePageManagerPut(segment_map->fpm, DSA_EXTRACT_OFFSET(span->start) / FPM_PAGE_SIZE, span->npages);
    LWLockRelease(DSA_AREA_LOCK(area));
                      
    LWLockAcquire(DSA_SCLASS_LOCK(area, DSA_SCLASS_SPAN_LARGE), LW_EXCLUSIVE);
    unlink_span(area, span);
    LWLockRelease(DSA_SCLASS_LOCK(area, DSA_SCLASS_SPAN_LARGE));
                                                   
    dsa_free(area, span_pointer);
    return;
  }

#ifdef CLOBBER_FREED_MEMORY
  memset(object, 0x7f, size);
#endif

  LWLockAcquire(DSA_SCLASS_LOCK(area, size_class), LW_EXCLUSIVE);

                                              
  Assert(object >= superblock);
  Assert(object < superblock + DSA_SUPERBLOCK_SIZE);
  Assert((object - superblock) % size == 0);
  NextFreeObjectIndex(object) = span->firstfree;
  span->firstfree = (object - superblock) / size;
  ++span->nallocatable;

     
                                                                         
                                                          
     
  if (span->nallocatable == 1 && span->fclass == DSA_FULLNESS_CLASSES - 1)
  {
       
                                                           
                                                                        
                                                                  
       
    unlink_span(area, span);
    add_span_to_fullness_class(area, span, span_pointer, DSA_FULLNESS_CLASSES - 2);

       
                                                                      
                                                                          
                                                                          
                                                                         
                                      
       
  }
  else if (span->nallocatable == span->nmax && (span->fclass != 1 || span->prevspan != InvalidDsaPointer))
  {
       
                                                                         
                                                                         
                                                                 
                                                                           
                                                                          
                   
       
    destroy_superblock(area, span_pointer);
  }

  LWLockRelease(DSA_SCLASS_LOCK(area, size_class));
}

   
                                                                         
                                                                         
                                                                          
                                                                             
   
void *
dsa_get_address(dsa_area *area, dsa_pointer dp)
{
  dsa_segment_index index;
  size_t offset;

                                          
  if (!DsaPointerIsValid(dp))
  {
    return NULL;
  }

                                                           
  check_for_freed_segments(area);

                                                  
  index = DSA_EXTRACT_SEGMENT_NUMBER(dp);
  offset = DSA_EXTRACT_OFFSET(dp);
  Assert(index < DSA_MAX_SEGMENTS);

                                                               
  if (unlikely(area->segment_maps[index].mapped_address == NULL))
  {
                                                     
    get_segment_by_index(area, index);
  }

  return area->segment_maps[index].mapped_address + offset;
}

   
                                                                         
                                                                           
                                       
   
void
dsa_pin(dsa_area *area)
{
  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  if (area->control->pinned)
  {
    LWLockRelease(DSA_AREA_LOCK(area));
    elog(ERROR, "dsa_area already pinned");
  }
  area->control->pinned = true;
  ++area->control->refcnt;
  LWLockRelease(DSA_AREA_LOCK(area));
}

   
                                                                            
                                                                        
           
   
void
dsa_unpin(dsa_area *area)
{
  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  Assert(area->control->refcnt > 1);
  if (!area->control->pinned)
  {
    LWLockRelease(DSA_AREA_LOCK(area));
    elog(ERROR, "dsa_area not pinned");
  }
  area->control->pinned = false;
  --area->control->refcnt;
  LWLockRelease(DSA_AREA_LOCK(area));
}

   
                                                                               
                                                                             
                                                            
   
                                                                           
                                                                         
                                        
   
void
dsa_set_size_limit(dsa_area *area, size_t limit)
{
  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  area->control->max_total_segment_size = limit;
  LWLockRelease(DSA_AREA_LOCK(area));
}

   
                                                                               
                         
   
void
dsa_trim(dsa_area *area)
{
  int size_class;

     
                                                                           
                                                                            
     
  for (size_class = DSA_NUM_SIZE_CLASSES - 1; size_class >= 0; --size_class)
  {
    dsa_area_pool *pool = &area->control->pools[size_class];
    dsa_pointer span_pointer;

    if (size_class == DSA_SCLASS_SPAN_LARGE)
    {
                                                                       
      continue;
    }

       
                                                                         
                                                                      
                                                                        
       
    LWLockAcquire(DSA_SCLASS_LOCK(area, size_class), LW_EXCLUSIVE);
    span_pointer = pool->spans[1];
    while (DsaPointerIsValid(span_pointer))
    {
      dsa_area_span *span = dsa_get_address(area, span_pointer);
      dsa_pointer next = span->nextspan;

      if (span->nallocatable == span->nmax)
      {
        destroy_superblock(area, span_pointer);
      }

      span_pointer = next;
    }
    LWLockRelease(DSA_SCLASS_LOCK(area, size_class));
  }
}

   
                                                                          
                
   
void
dsa_dump(dsa_area *area)
{
  size_t i, j;

     
                                                                           
                                    
     

  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  check_for_freed_segments_locked(area);
  fprintf(stderr, "dsa_area handle %x:\n", area->control->handle);
  fprintf(stderr, "  max_total_segment_size: %zu\n", area->control->max_total_segment_size);
  fprintf(stderr, "  total_segment_size: %zu\n", area->control->total_segment_size);
  fprintf(stderr, "  refcnt: %d\n", area->control->refcnt);
  fprintf(stderr, "  pinned: %c\n", area->control->pinned ? 't' : 'f');
  fprintf(stderr, "  segment bins:\n");
  for (i = 0; i < DSA_NUM_SEGMENT_BINS; ++i)
  {
    if (area->control->segment_bins[i] != DSA_SEGMENT_INDEX_NONE)
    {
      dsa_segment_index segment_index;

      fprintf(stderr, "    segment bin %zu (at least %d contiguous pages free):\n", i, 1 << (i - 1));
      segment_index = area->control->segment_bins[i];
      while (segment_index != DSA_SEGMENT_INDEX_NONE)
      {
        dsa_segment_map *segment_map;

        segment_map = get_segment_by_index(area, segment_index);

        fprintf(stderr,
            "      segment index %zu, usable_pages = %zu, "
            "contiguous_pages = %zu, mapped at %p\n",
            segment_index, segment_map->header->usable_pages, fpm_largest(segment_map->fpm), segment_map->mapped_address);
        segment_index = segment_map->header->next;
      }
    }
  }
  LWLockRelease(DSA_AREA_LOCK(area));

  fprintf(stderr, "  pools:\n");
  for (i = 0; i < DSA_NUM_SIZE_CLASSES; ++i)
  {
    bool found = false;

    LWLockAcquire(DSA_SCLASS_LOCK(area, i), LW_EXCLUSIVE);
    for (j = 0; j < DSA_FULLNESS_CLASSES; ++j)
    {
      if (DsaPointerIsValid(area->control->pools[i].spans[j]))
      {
        found = true;
      }
    }
    if (found)
    {
      if (i == DSA_SCLASS_BLOCK_OF_SPANS)
      {
        fprintf(stderr, "    pool for blocks of span objects:\n");
      }
      else if (i == DSA_SCLASS_SPAN_LARGE)
      {
        fprintf(stderr, "    pool for large object spans:\n");
      }
      else
      {
        fprintf(stderr, "    pool for size class %zu (object size %hu bytes):\n", i, dsa_size_classes[i]);
      }
      for (j = 0; j < DSA_FULLNESS_CLASSES; ++j)
      {
        if (!DsaPointerIsValid(area->control->pools[i].spans[j]))
        {
          fprintf(stderr, "      fullness class %zu is empty\n", j);
        }
        else
        {
          dsa_pointer span_pointer = area->control->pools[i].spans[j];

          fprintf(stderr, "      fullness class %zu:\n", j);
          while (DsaPointerIsValid(span_pointer))
          {
            dsa_area_span *span;

            span = dsa_get_address(area, span_pointer);
            fprintf(stderr, "        span descriptor at " DSA_POINTER_FORMAT ", superblock at " DSA_POINTER_FORMAT ", pages = %zu, objects free = %hu/%hu\n", span_pointer, span->start, span->npages, span->nallocatable, span->nmax);
            span_pointer = span->nextspan;
          }
        }
      }
    }
    LWLockRelease(DSA_SCLASS_LOCK(area, i));
  }
}

   
                                                                 
                        
   
size_t
dsa_minimum_size(void)
{
  size_t size;
  int pages = 0;

  size = MAXALIGN(sizeof(dsa_area_control)) + MAXALIGN(sizeof(FreePageManager));

                                                                    
  while (((size + FPM_PAGE_SIZE - 1) / FPM_PAGE_SIZE) > pages)
  {
    ++pages;
    size += sizeof(dsa_pointer);
  }

  return pages * FPM_PAGE_SIZE;
}

   
                                                              
   
static dsa_area *
create_internal(void *place, size_t size, int tranche_id, dsm_handle control_handle, dsm_segment *control_segment)
{
  dsa_area_control *control;
  dsa_area *area;
  dsa_segment_map *segment_map;
  size_t usable_pages;
  size_t total_pages;
  size_t metadata_bytes;
  int i;

                                                     
  if (size < dsa_minimum_size())
  {
    elog(ERROR, "dsa_area space must be at least %zu, but %zu provided", dsa_minimum_size(), size);
  }

                                               
  total_pages = size / FPM_PAGE_SIZE;
  metadata_bytes = MAXALIGN(sizeof(dsa_area_control)) + MAXALIGN(sizeof(FreePageManager)) + total_pages * sizeof(dsa_pointer);
                                             
  if (metadata_bytes % FPM_PAGE_SIZE != 0)
  {
    metadata_bytes += FPM_PAGE_SIZE - (metadata_bytes % FPM_PAGE_SIZE);
  }
  Assert(metadata_bytes <= size);
  usable_pages = (size - metadata_bytes) / FPM_PAGE_SIZE;

     
                                                                        
            
     
  control = (dsa_area_control *)place;
  control->segment_header.magic = DSA_SEGMENT_HEADER_MAGIC ^ control_handle ^ 0;
  control->segment_header.next = DSA_SEGMENT_INDEX_NONE;
  control->segment_header.prev = DSA_SEGMENT_INDEX_NONE;
  control->segment_header.usable_pages = usable_pages;
  control->segment_header.freed = false;
  control->segment_header.size = DSA_INITIAL_SEGMENT_SIZE;
  control->handle = control_handle;
  control->max_total_segment_size = (size_t)-1;
  control->total_segment_size = size;
  memset(&control->segment_handles[0], 0, sizeof(dsm_handle) * DSA_MAX_SEGMENTS);
  control->segment_handles[0] = control_handle;
  for (i = 0; i < DSA_NUM_SEGMENT_BINS; ++i)
  {
    control->segment_bins[i] = DSA_SEGMENT_INDEX_NONE;
  }
  control->high_segment_index = 0;
  control->refcnt = 1;
  control->freed_segment_counter = 0;
  control->lwlock_tranche_id = tranche_id;

     
                                                                         
                                                                            
                
     
  area = palloc(sizeof(dsa_area));
  area->control = control;
  area->mapping_pinned = false;
  memset(area->segment_maps, 0, sizeof(dsa_segment_map) * DSA_MAX_SEGMENTS);
  area->high_segment_index = 0;
  area->freed_segment_counter = 0;
  LWLockInitialize(&control->lock, control->lwlock_tranche_id);
  for (i = 0; i < DSA_NUM_SIZE_CLASSES; ++i)
  {
    LWLockInitialize(DSA_SCLASS_LOCK(area, i), control->lwlock_tranche_id);
  }

                                                          
  segment_map = &area->segment_maps[0];
  segment_map->segment = control_segment;
  segment_map->mapped_address = place;
  segment_map->header = (dsa_segment_header *)place;
  segment_map->fpm = (FreePageManager *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_area_control)));
  segment_map->pagemap = (dsa_pointer *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_area_control)) + MAXALIGN(sizeof(FreePageManager)));

                                 
  FreePageManagerInitialize(segment_map->fpm, segment_map->mapped_address);
                                                                  

  if (usable_pages > 0)
  {
    FreePageManagerPut(segment_map->fpm, metadata_bytes / FPM_PAGE_SIZE, usable_pages);
  }

                                                  
  control->segment_bins[contiguous_pages_to_segment_bin(usable_pages)] = 0;
  segment_map->header->bin = contiguous_pages_to_segment_bin(usable_pages);

  return area;
}

   
                                                              
   
static dsa_area *
attach_internal(void *place, dsm_segment *segment, dsa_handle handle)
{
  dsa_area_control *control;
  dsa_area *area;
  dsa_segment_map *segment_map;

  control = (dsa_area_control *)place;
  Assert(control->handle == handle);
  Assert(control->segment_handles[0] == handle);
  Assert(control->segment_header.magic == (DSA_SEGMENT_HEADER_MAGIC ^ handle ^ 0));

                                            
  area = palloc(sizeof(dsa_area));
  area->control = control;
  area->mapping_pinned = false;
  memset(&area->segment_maps[0], 0, sizeof(dsa_segment_map) * DSA_MAX_SEGMENTS);
  area->high_segment_index = 0;

                                                          
  segment_map = &area->segment_maps[0];
  segment_map->segment = segment;                        
  segment_map->mapped_address = place;
  segment_map->header = (dsa_segment_header *)segment_map->mapped_address;
  segment_map->fpm = (FreePageManager *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_area_control)));
  segment_map->pagemap = (dsa_pointer *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_area_control)) + MAXALIGN(sizeof(FreePageManager)));

                                 
  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  if (control->refcnt == 0)
  {
                                                                        
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not attach to dynamic shared area")));
  }
  ++control->refcnt;
  area->freed_segment_counter = area->control->freed_segment_counter;
  LWLockRelease(DSA_AREA_LOCK(area));

  return area;
}

   
                                                             
   
static void
init_span(dsa_area *area, dsa_pointer span_pointer, dsa_area_pool *pool, dsa_pointer start, size_t npages, uint16 size_class)
{
  dsa_area_span *span = dsa_get_address(area, span_pointer);
  size_t obsize = dsa_size_classes[size_class];

     
                                                                            
                
     
  Assert(LWLockHeldByMe(DSA_SCLASS_LOCK(area, size_class)));

                                                                            
  if (DsaPointerIsValid(pool->spans[1]))
  {
    dsa_area_span *head = (dsa_area_span *)dsa_get_address(area, pool->spans[1]);

    head->prevspan = span_pointer;
  }
  span->pool = DsaAreaPoolToDsaPointer(area, pool);
  span->nextspan = pool->spans[1];
  span->prevspan = InvalidDsaPointer;
  pool->spans[1] = span_pointer;

  span->start = start;
  span->npages = npages;
  span->size_class = size_class;
  span->ninitialized = 0;
  if (size_class == DSA_SCLASS_BLOCK_OF_SPANS)
  {
       
                                                                           
                                                                       
                                                                         
                                                                          
             
       
    span->ninitialized = 1;
    span->nallocatable = FPM_PAGE_SIZE / obsize - 1;
  }
  else if (size_class != DSA_SCLASS_SPAN_LARGE)
  {
    span->nallocatable = DSA_SUPERBLOCK_SIZE / obsize;
  }
  span->firstfree = DSA_SPAN_NOTHING_FREE;
  span->nmax = span->nallocatable;
  span->fclass = 1;
}

   
                                                                        
                   
   
static bool
transfer_first_span(dsa_area *area, dsa_area_pool *pool, int fromclass, int toclass)
{
  dsa_pointer span_pointer;
  dsa_area_span *span;
  dsa_area_span *nextspan;

                                            
  span_pointer = pool->spans[fromclass];
  if (!DsaPointerIsValid(span_pointer))
  {
    return false;
  }

                                             
  span = dsa_get_address(area, span_pointer);
  pool->spans[fromclass] = span->nextspan;
  if (DsaPointerIsValid(span->nextspan))
  {
    nextspan = (dsa_area_span *)dsa_get_address(area, span->nextspan);
    nextspan->prevspan = InvalidDsaPointer;
  }

                                        
  span->nextspan = pool->spans[toclass];
  pool->spans[toclass] = span_pointer;
  if (DsaPointerIsValid(span->nextspan))
  {
    nextspan = (dsa_area_span *)dsa_get_address(area, span->nextspan);
    nextspan->prevspan = span_pointer;
  }
  span->fclass = toclass;

  return true;
}

   
                                                                        
   
static inline dsa_pointer
alloc_object(dsa_area *area, int size_class)
{
  dsa_area_pool *pool = &area->control->pools[size_class];
  dsa_area_span *span;
  dsa_pointer block;
  dsa_pointer result;
  char *object;
  size_t size;

     
                                                                           
                                                                           
                                                                           
                                                          
     
  Assert(!LWLockHeldByMe(DSA_SCLASS_LOCK(area, size_class)));
  LWLockAcquire(DSA_SCLASS_LOCK(area, size_class), LW_EXCLUSIVE);

     
                                                                         
                       
     
  if (!DsaPointerIsValid(pool->spans[1]) && !ensure_active_superblock(area, pool, size_class))
  {
    result = InvalidDsaPointer;
  }
  else
  {
       
                                                                         
                                                                          
                                                                     
       
    Assert(DsaPointerIsValid(pool->spans[1]));
    span = (dsa_area_span *)dsa_get_address(area, pool->spans[1]);
    Assert(span->nallocatable > 0);
    block = span->start;
    Assert(size_class < DSA_NUM_SIZE_CLASSES);
    size = dsa_size_classes[size_class];
    if (span->firstfree != DSA_SPAN_NOTHING_FREE)
    {
      result = block + span->firstfree * size;
      object = dsa_get_address(area, result);
      span->firstfree = NextFreeObjectIndex(object);
    }
    else
    {
      result = block + span->ninitialized * size;
      ++span->ninitialized;
    }
    --span->nallocatable;

                                                                           
    if (span->nallocatable == 0)
    {
      transfer_first_span(area, pool, 1, DSA_FULLNESS_CLASSES - 1);
    }
  }

  Assert(LWLockHeldByMe(DSA_SCLASS_LOCK(area, size_class)));
  LWLockRelease(DSA_SCLASS_LOCK(area, size_class));

  return result;
}

   
                                                                            
                                                                 
   
                                                                             
                                                                               
                                                                          
                                                                           
                                                                            
                                                                          
                                                                             
                                                                           
                                                                     
                                    
   
                                                                           
                                                                          
                                                                             
                                                                       
                                                                               
                                
   
static bool
ensure_active_superblock(dsa_area *area, dsa_area_pool *pool, int size_class)
{
  dsa_pointer span_pointer;
  dsa_pointer start_pointer;
  size_t obsize = dsa_size_classes[size_class];
  size_t nmax;
  int fclass;
  size_t npages = 1;
  size_t first_page;
  size_t i;
  dsa_segment_map *segment_map;

  Assert(LWLockHeldByMe(DSA_SCLASS_LOCK(area, size_class)));

     
                                                                         
                                                                        
                                                                            
             
     
  if (size_class == DSA_SCLASS_BLOCK_OF_SPANS)
  {
    nmax = FPM_PAGE_SIZE / obsize - 1;
  }
  else
  {
    nmax = DSA_SUPERBLOCK_SIZE / obsize;
  }

     
                                                                      
                                                                        
                                                          
     
  for (fclass = 2; fclass < DSA_FULLNESS_CLASSES - 1; ++fclass)
  {
    span_pointer = pool->spans[fclass];

    while (DsaPointerIsValid(span_pointer))
    {
      int tfclass;
      dsa_area_span *span;
      dsa_area_span *nextspan;
      dsa_area_span *prevspan;
      dsa_pointer next_span_pointer;

      span = (dsa_area_span *)dsa_get_address(area, span_pointer);
      next_span_pointer = span->nextspan;

                                                                    
      tfclass = (nmax - span->nallocatable) * (DSA_FULLNESS_CLASSES - 1) / nmax;

                              
      if (DsaPointerIsValid(span->nextspan))
      {
        nextspan = (dsa_area_span *)dsa_get_address(area, span->nextspan);
      }
      else
      {
        nextspan = NULL;
      }

         
                                                                         
                                              
         
      if (tfclass < fclass)
      {
                                                          
        if (pool->spans[fclass] == span_pointer)
        {
                                           
          Assert(!DsaPointerIsValid(span->prevspan));
          pool->spans[fclass] = span->nextspan;
          if (nextspan != NULL)
          {
            nextspan->prevspan = InvalidDsaPointer;
          }
        }
        else
        {
                                    
          Assert(DsaPointerIsValid(span->prevspan));
          prevspan = (dsa_area_span *)dsa_get_address(area, span->prevspan);
          prevspan->nextspan = span->nextspan;
        }
        if (nextspan != NULL)
        {
          nextspan->prevspan = span->prevspan;
        }

                                                                
        span->nextspan = pool->spans[tfclass];
        pool->spans[tfclass] = span_pointer;
        span->prevspan = InvalidDsaPointer;
        if (DsaPointerIsValid(span->nextspan))
        {
          nextspan = (dsa_area_span *)dsa_get_address(area, span->nextspan);
          nextspan->prevspan = span_pointer;
        }
        span->fclass = tfclass;
      }

                                         
      span_pointer = next_span_pointer;
    }

                                                
    if (DsaPointerIsValid(pool->spans[1]))
    {
      return true;
    }
  }

     
                                                                           
                                                                             
                                                                        
                                                                         
                                                                            
                                                   
     
  Assert(!DsaPointerIsValid(pool->spans[1]));
  for (fclass = 2; fclass < DSA_FULLNESS_CLASSES - 1; ++fclass)
  {
    if (transfer_first_span(area, pool, fclass, 1))
    {
      return true;
    }
  }
  if (!DsaPointerIsValid(pool->spans[1]) && transfer_first_span(area, pool, 0, 1))
  {
    return true;
  }

     
                                                                         
                                                                      
     
                                                                            
                                                                             
                                                                          
                                              
     
  if (size_class != DSA_SCLASS_BLOCK_OF_SPANS)
  {
    span_pointer = alloc_object(area, DSA_SCLASS_BLOCK_OF_SPANS);
    if (!DsaPointerIsValid(span_pointer))
    {
      return false;
    }
    npages = DSA_PAGES_PER_SUPERBLOCK;
  }

                                                             
  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  segment_map = get_best_segment(area, npages);
  if (segment_map == NULL)
  {
    segment_map = make_new_segment(area, npages);
    if (segment_map == NULL)
    {
      LWLockRelease(DSA_AREA_LOCK(area));
      return false;
    }
  }

     
                                                                     
                                                        
     
  if (!FreePageManagerGet(segment_map->fpm, npages, &first_page))
  {
    elog(FATAL, "dsa_allocate could not find %zu free pages for superblock", npages);
  }
  LWLockRelease(DSA_AREA_LOCK(area));

                                            
  start_pointer = DSA_MAKE_POINTER(get_segment_index(area, segment_map), first_page * FPM_PAGE_SIZE);

     
                                                                        
                      
     
  if (size_class == DSA_SCLASS_BLOCK_OF_SPANS)
  {
       
                                                                           
                                                           
       
    span_pointer = start_pointer;
  }

                                    
  init_span(area, span_pointer, pool, start_pointer, npages, size_class);
  for (i = 0; i < npages; ++i)
  {
    segment_map->pagemap[first_page + i] = span_pointer;
  }

  return true;
}

   
                                                                              
                                                                               
                                                                               
                                                                           
                                                                           
                                                            
   
static dsa_segment_map *
get_segment_by_index(dsa_area *area, dsa_segment_index index)
{
  if (unlikely(area->segment_maps[index].mapped_address == NULL))
  {
    dsm_handle handle;
    dsm_segment *segment;
    dsa_segment_map *segment_map;

       
                                                                          
                                                                         
                                                                         
                                                                       
                                                                         
                                                                       
                                                                   
                                            
       
    handle = area->control->segment_handles[index];

                                                        
    if (handle == DSM_HANDLE_INVALID)
    {
      elog(ERROR, "dsa_area could not attach to a segment that has been freed");
    }

    segment = dsm_attach(handle);
    if (segment == NULL)
    {
      elog(ERROR, "dsa_area could not attach to segment");
    }
    if (area->mapping_pinned)
    {
      dsm_pin_mapping(segment);
    }
    segment_map = &area->segment_maps[index];
    segment_map->segment = segment;
    segment_map->mapped_address = dsm_segment_address(segment);
    segment_map->header = (dsa_segment_header *)segment_map->mapped_address;
    segment_map->fpm = (FreePageManager *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_segment_header)));
    segment_map->pagemap = (dsa_pointer *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_segment_header)) + MAXALIGN(sizeof(FreePageManager)));

                                                                  
    if (area->high_segment_index < index)
    {
      area->high_segment_index = index;
    }

    Assert(segment_map->header->magic == (DSA_SEGMENT_HEADER_MAGIC ^ area->control->handle ^ index));
  }

     
                                                                           
                                                                      
                                                                            
                                                
     
                                                                        
                                                                            
                                                                           
                                                      
                                                                          
                             
     
                                                                        
     
  Assert(!area->segment_maps[index].header->freed);

  return &area->segment_maps[index];
}

   
                                                                            
                                                                     
   
                                           
   
static void
destroy_superblock(dsa_area *area, dsa_pointer span_pointer)
{
  dsa_area_span *span = dsa_get_address(area, span_pointer);
  int size_class = span->size_class;
  dsa_segment_map *segment_map;

                                               
  unlink_span(area, span);

     
                                                                          
                                                                         
                     
     
  LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
  check_for_freed_segments_locked(area);
  segment_map = get_segment_by_index(area, DSA_EXTRACT_SEGMENT_NUMBER(span->start));
  FreePageManagerPut(segment_map->fpm, DSA_EXTRACT_OFFSET(span->start) / FPM_PAGE_SIZE, span->npages);
                                                  
  if (fpm_largest(segment_map->fpm) == segment_map->header->usable_pages)
  {
    dsa_segment_index index = get_segment_index(area, segment_map);

                                                                   
    if (index != 0)
    {
         
                                                                         
                              
         
      unlink_segment(area, segment_map);
      segment_map->header->freed = true;
      Assert(area->control->total_segment_size >= segment_map->header->size);
      area->control->total_segment_size -= segment_map->header->size;
      dsm_unpin_segment(dsm_segment_handle(segment_map->segment));
      dsm_detach(segment_map->segment);
      area->control->segment_handles[index] = DSM_HANDLE_INVALID;
      ++area->control->freed_segment_counter;
      segment_map->segment = NULL;
      segment_map->header = NULL;
      segment_map->mapped_address = NULL;
    }
  }
  LWLockRelease(DSA_AREA_LOCK(area));

     
                                                                         
                                                                          
                                                                             
                                                                             
                                                                           
                                                    
     
  if (size_class != DSA_SCLASS_BLOCK_OF_SPANS)
  {
    dsa_free(area, span_pointer);
  }
}

static void
unlink_span(dsa_area *area, dsa_area_span *span)
{
  if (DsaPointerIsValid(span->nextspan))
  {
    dsa_area_span *next = dsa_get_address(area, span->nextspan);

    next->prevspan = span->prevspan;
  }
  if (DsaPointerIsValid(span->prevspan))
  {
    dsa_area_span *prev = dsa_get_address(area, span->prevspan);

    prev->nextspan = span->nextspan;
  }
  else
  {
    dsa_area_pool *pool = dsa_get_address(area, span->pool);

    pool->spans[span->fclass] = span->nextspan;
  }
}

static void
add_span_to_fullness_class(dsa_area *area, dsa_area_span *span, dsa_pointer span_pointer, int fclass)
{
  dsa_area_pool *pool = dsa_get_address(area, span->pool);

  if (DsaPointerIsValid(pool->spans[fclass]))
  {
    dsa_area_span *head = dsa_get_address(area, pool->spans[fclass]);

    head->prevspan = span_pointer;
  }
  span->prevspan = InvalidDsaPointer;
  span->nextspan = pool->spans[fclass];
  pool->spans[fclass] = span_pointer;
  span->fclass = fclass;
}

   
                                                                               
   
void
dsa_detach(dsa_area *area)
{
  int i;

                                 
  for (i = 0; i <= area->high_segment_index; ++i)
  {
    if (area->segment_maps[i].segment != NULL)
    {
      dsm_detach(area->segment_maps[i].segment);
    }
  }

     
                                                                           
                                                                         
                                                                         
                                                                           
                                                                         
                                                             
     

                                           
  pfree(area);
}

   
                                                   
   
static void
unlink_segment(dsa_area *area, dsa_segment_map *segment_map)
{
  if (segment_map->header->prev != DSA_SEGMENT_INDEX_NONE)
  {
    dsa_segment_map *prev;

    prev = get_segment_by_index(area, segment_map->header->prev);
    prev->header->next = segment_map->header->next;
  }
  else
  {
    Assert(area->control->segment_bins[segment_map->header->bin] == get_segment_index(area, segment_map));
    area->control->segment_bins[segment_map->header->bin] = segment_map->header->next;
  }
  if (segment_map->header->next != DSA_SEGMENT_INDEX_NONE)
  {
    dsa_segment_map *next;

    next = get_segment_by_index(area, segment_map->header->next);
    next->header->prev = segment_map->header->prev;
  }
}

   
                                                                          
                                                                               
                                                                             
              
   
static dsa_segment_map *
get_best_segment(dsa_area *area, size_t npages)
{
  size_t bin;

  Assert(LWLockHeldByMe(DSA_AREA_LOCK(area)));
  check_for_freed_segments_locked(area);

     
                                                                            
            
     
  for (bin = contiguous_pages_to_segment_bin(npages); bin < DSA_NUM_SEGMENT_BINS; ++bin)
  {
       
                                                                       
                                                          
       
    size_t threshold = (size_t)1 << (bin - 1);
    dsa_segment_index segment_index;

                                                                     
    segment_index = area->control->segment_bins[bin];
    while (segment_index != DSA_SEGMENT_INDEX_NONE)
    {
      dsa_segment_map *segment_map;
      dsa_segment_index next_segment_index;
      size_t contiguous_pages;

      segment_map = get_segment_by_index(area, segment_index);
      next_segment_index = segment_map->header->next;
      contiguous_pages = fpm_largest(segment_map->fpm);

                                                                  
      if (contiguous_pages >= threshold && contiguous_pages < npages)
      {
        segment_index = next_segment_index;
        continue;
      }

                                                               
      if (contiguous_pages < threshold)
      {
        size_t new_bin;

        new_bin = contiguous_pages_to_segment_bin(contiguous_pages);

                                             
        unlink_segment(area, segment_map);

                                                    
        segment_map->header->prev = DSA_SEGMENT_INDEX_NONE;
        segment_map->header->next = area->control->segment_bins[new_bin];
        segment_map->header->bin = new_bin;
        area->control->segment_bins[new_bin] = segment_index;
        if (segment_map->header->next != DSA_SEGMENT_INDEX_NONE)
        {
          dsa_segment_map *next;

          next = get_segment_by_index(area, segment_map->header->next);
          Assert(next->header->bin == new_bin);
          next->header->prev = segment_index;
        }

           
                                                                  
                              
           
      }

                                 
      if (contiguous_pages >= npages)
      {
        return segment_map;
      }

                                            
      segment_index = next_segment_index;
    }
  }

                  
  return NULL;
}

   
                                                                           
                                                                       
                               
   
static dsa_segment_map *
make_new_segment(dsa_area *area, size_t requested_pages)
{
  dsa_segment_index new_index;
  size_t metadata_bytes;
  size_t total_size;
  size_t total_pages;
  size_t usable_pages;
  dsa_segment_map *segment_map;
  dsm_segment *segment;

  Assert(LWLockHeldByMe(DSA_AREA_LOCK(area)));

                                                                  
  for (new_index = 1; new_index < DSA_MAX_SEGMENTS; ++new_index)
  {
    if (area->control->segment_handles[new_index] == DSM_HANDLE_INVALID)
    {
      break;
    }
  }
  if (new_index == DSA_MAX_SEGMENTS)
  {
    return NULL;
  }

     
                                                                         
                                                                    
     
  if (area->control->total_segment_size >= area->control->max_total_segment_size)
  {
    return NULL;
  }

     
                                                                       
                                                                        
                                                                        
                                                                       
                                                                          
                                                                             
                                               
     
                                                                      
                                                                         
                                                                           
                       
     
  total_size = DSA_INITIAL_SEGMENT_SIZE * ((size_t)1 << (new_index / DSA_NUM_SEGMENTS_AT_EACH_SIZE));
  total_size = Min(total_size, DSA_MAX_SEGMENT_SIZE);
  total_size = Min(total_size, area->control->max_total_segment_size - area->control->total_segment_size);

  total_pages = total_size / FPM_PAGE_SIZE;
  metadata_bytes = MAXALIGN(sizeof(dsa_segment_header)) + MAXALIGN(sizeof(FreePageManager)) + sizeof(dsa_pointer) * total_pages;

                                             
  if (metadata_bytes % FPM_PAGE_SIZE != 0)
  {
    metadata_bytes += FPM_PAGE_SIZE - (metadata_bytes % FPM_PAGE_SIZE);
  }
  if (total_size <= metadata_bytes)
  {
    return NULL;
  }
  usable_pages = (total_size - metadata_bytes) / FPM_PAGE_SIZE;
  Assert(metadata_bytes + usable_pages * FPM_PAGE_SIZE <= total_size);

                                
  if (requested_pages > usable_pages)
  {
       
                                                                           
                        
       
    usable_pages = requested_pages;
    metadata_bytes = MAXALIGN(sizeof(dsa_segment_header)) + MAXALIGN(sizeof(FreePageManager)) + usable_pages * sizeof(dsa_pointer);

                                               
    if (metadata_bytes % FPM_PAGE_SIZE != 0)
    {
      metadata_bytes += FPM_PAGE_SIZE - (metadata_bytes % FPM_PAGE_SIZE);
    }
    total_size = metadata_bytes + usable_pages * FPM_PAGE_SIZE;

                                                                
    if (total_size > DSA_MAX_SEGMENT_SIZE)
    {
      return NULL;
    }

                                      
    if (total_size > area->control->max_total_segment_size - area->control->total_segment_size)
    {
      return NULL;
    }
  }

                           
  segment = dsm_create(total_size, 0);
  if (segment == NULL)
  {
    return NULL;
  }
  dsm_pin_segment(segment);
  if (area->mapping_pinned)
  {
    dsm_pin_mapping(segment);
  }

                                                               
  area->control->segment_handles[new_index] = dsm_segment_handle(segment);
                                                                   
  if (area->control->high_segment_index < new_index)
  {
    area->control->high_segment_index = new_index;
  }
                                                                     
  if (area->high_segment_index < new_index)
  {
    area->high_segment_index = new_index;
  }
                                         
  area->control->total_segment_size += total_size;
  Assert(area->control->total_segment_size <= area->control->max_total_segment_size);

                                                             
  segment_map = &area->segment_maps[new_index];
  segment_map->segment = segment;
  segment_map->mapped_address = dsm_segment_address(segment);
  segment_map->header = (dsa_segment_header *)segment_map->mapped_address;
  segment_map->fpm = (FreePageManager *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_segment_header)));
  segment_map->pagemap = (dsa_pointer *)(segment_map->mapped_address + MAXALIGN(sizeof(dsa_segment_header)) + MAXALIGN(sizeof(FreePageManager)));

                                 
  FreePageManagerInitialize(segment_map->fpm, segment_map->mapped_address);
  FreePageManagerPut(segment_map->fpm, metadata_bytes / FPM_PAGE_SIZE, usable_pages);

                                                                    
  segment_map->header->magic = DSA_SEGMENT_HEADER_MAGIC ^ area->control->handle ^ new_index;
  segment_map->header->usable_pages = usable_pages;
  segment_map->header->size = total_size;
  segment_map->header->bin = contiguous_pages_to_segment_bin(usable_pages);
  segment_map->header->prev = DSA_SEGMENT_INDEX_NONE;
  segment_map->header->next = area->control->segment_bins[segment_map->header->bin];
  segment_map->header->freed = false;
  area->control->segment_bins[segment_map->header->bin] = new_index;
  if (segment_map->header->next != DSA_SEGMENT_INDEX_NONE)
  {
    dsa_segment_map *next = get_segment_by_index(area, segment_map->header->next);

    Assert(next->header->bin == segment_map->header->bin);
    next->header->prev = new_index;
  }

  return segment_map;
}

   
                                                                          
                                                                 
                                                                          
                                                    
   
                                                                              
                                                                           
                                                                              
                                                                               
                                                                            
                                   
   
static void
check_for_freed_segments(dsa_area *area)
{
  size_t freed_segment_counter;

     
                                                                
                                                                             
                                                                      
                                                                            
                                                                         
                                                                        
                                                                         
                                                                             
                                                                             
                                                                           
                                                            
     
  pg_read_barrier();
  freed_segment_counter = area->control->freed_segment_counter;
  if (unlikely(area->freed_segment_counter != freed_segment_counter))
  {
                                                                        
    LWLockAcquire(DSA_AREA_LOCK(area), LW_EXCLUSIVE);
    check_for_freed_segments_locked(area);
    LWLockRelease(DSA_AREA_LOCK(area));
  }
}

   
                                                                            
                                                                               
                                                                               
                                                                               
                    
   
static void
check_for_freed_segments_locked(dsa_area *area)
{
  size_t freed_segment_counter;
  int i;

  Assert(LWLockHeldByMe(DSA_AREA_LOCK(area)));
  freed_segment_counter = area->control->freed_segment_counter;
  if (unlikely(area->freed_segment_counter != freed_segment_counter))
  {
    for (i = 0; i <= area->high_segment_index; ++i)
    {
      if (area->segment_maps[i].header != NULL && area->segment_maps[i].header->freed)
      {
        dsm_detach(area->segment_maps[i].segment);
        area->segment_maps[i].segment = NULL;
        area->segment_maps[i].header = NULL;
        area->segment_maps[i].mapped_address = NULL;
      }
    }
    area->freed_segment_counter = freed_segment_counter;
  }
}
