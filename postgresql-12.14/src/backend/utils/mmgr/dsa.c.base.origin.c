/*-------------------------------------------------------------------------
 *
 * dsa.c
 *	  Dynamic shared memory areas.
 *
 * This module provides dynamic shared memory areas which are built on top of
 * DSM segments.  While dsm.c allows segments of memory of shared memory to be
 * created and shared between backends, it isn't designed to deal with small
 * objects.  A DSA area is a shared memory heap usually backed by one or more
 * DSM segments which can allocate memory using dsa_allocate() and dsa_free().
 * Alternatively, it can be created in pre-existing shared memory, including a
 * DSM segment, and then create extra DSM segments as required.  Unlike the
 * regular system heap, it deals in pseudo-pointers which must be converted to
 * backend-local pointers before they are dereferenced.  These pseudo-pointers
 * can however be shared with other backends, and can be used to construct
 * shared data structures.
 *
 * Each DSA area manages a set of DSM segments, adding new segments as
 * required and detaching them when they are no longer needed.  Each segment
 * contains a number of 4KB pages, a free page manager for tracking
 * consecutive runs of free pages, and a page map for tracking the source of
 * objects allocated on each page.  Allocation requests above 8KB are handled
 * by choosing a segment and finding consecutive free pages in its free page
 * manager.  Allocation requests for smaller sizes are handled using pools of
 * objects of a selection of sizes.  Each pool consists of a number of 16 page
 * (64KB) superblocks allocated in the same way as large objects.  Allocation
 * of large objects and new superblocks is serialized by a single LWLock, but
 * allocation of small objects from pre-existing superblocks uses one LWLock
 * per pool.  Currently there is one pool, and therefore one lock, per size
 * class.  Per-core pools to increase concurrency and strategies for reducing
 * the resulting fragmentation are areas for future research.  Each superblock
 * is managed with a 'span', which tracks the superblock's freelist.  Free
 * requests are handled by looking in the page map to find which span an
 * address was allocated from, so that small objects can be returned to the
 * appropriate free list, and large object pages can be returned directly to
 * the free page map.  When allocating, simple heuristics for selecting
 * segments and superblocks try to encourage occupied memory to be
 * concentrated, increasing the likelihood that whole superblocks can become
 * empty and be returned to the free page manager, and whole segments can
 * become empty and be returned to the operating system.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/mmgr/dsa.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "port/atomics.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/dsa.h"
#include "utils/freepage.h"
#include "utils/memutils.h"

/*
 * The size of the initial DSM segment that backs a dsa_area created by
 * dsa_create.  After creating some number of segments of this size we'll
 * double this size, and so on.  Larger segments may be created if necessary
 * to satisfy large requests.
 */
#define DSA_INITIAL_SEGMENT_SIZE ((size_t)(1 * 1024 * 1024))

/*
 * How many segments to create before we double the segment size.  If this is
 * low, then there is likely to be a lot of wasted space in the largest
 * segment.  If it is high, then we risk running out of segment slots (see
 * dsm.c's limits on total number of segments), or limiting the total size
 * an area can manage when using small pointers.
 */
#define DSA_NUM_SEGMENTS_AT_EACH_SIZE 2

/*
 * The number of bits used to represent the offset part of a dsa_pointer.
 * This controls the maximum size of a segment, the maximum possible
 * allocation size and also the maximum number of segments per area.
 */
#if SIZEOF_DSA_POINTER == 4
#define DSA_OFFSET_WIDTH 27 /* 32 segments of size up to 128MB */
#else
#define DSA_OFFSET_WIDTH 40 /* 1024 segments of size up to 1TB */
#endif

/*
 * The maximum number of DSM segments that an area can own, determined by
 * the number of bits remaining (but capped at 1024).
 */
#define DSA_MAX_SEGMENTS Min(1024, (1 << ((SIZEOF_DSA_POINTER * 8) - DSA_OFFSET_WIDTH)))

/* The bitmask for extracting the offset from a dsa_pointer. */
#define DSA_OFFSET_BITMASK (((dsa_pointer)1 << DSA_OFFSET_WIDTH) - 1)

/* The maximum size of a DSM segment. */
#define DSA_MAX_SEGMENT_SIZE ((size_t)1 << DSA_OFFSET_WIDTH)

/* Number of pages (see FPM_PAGE_SIZE) per regular superblock. */
#define DSA_PAGES_PER_SUPERBLOCK 16

/*
 * A magic number used as a sanity check for following DSM segments belonging
 * to a DSA area (this number will be XORed with the area handle and
 * the segment index).
 */
#define DSA_SEGMENT_HEADER_MAGIC 0x0ce26608

/* Build a dsa_pointer given a segment number and offset. */
#define DSA_MAKE_POINTER(segment_number, offset) (((dsa_pointer)(segment_number) << DSA_OFFSET_WIDTH) | (offset))

/* Extract the segment number from a dsa_pointer. */
#define DSA_EXTRACT_SEGMENT_NUMBER(dp) ((dp) >> DSA_OFFSET_WIDTH)

/* Extract the offset from a dsa_pointer. */
#define DSA_EXTRACT_OFFSET(dp) ((dp) & DSA_OFFSET_BITMASK)

/* The type used for index segment indexes (zero based). */
typedef size_t dsa_segment_index;

/* Sentinel value for dsa_segment_index indicating 'none' or 'end'. */
#define DSA_SEGMENT_INDEX_NONE (~(dsa_segment_index)0)

/*
 * How many bins of segments do we have?  The bins are used to categorize
 * segments by their largest contiguous run of free pages.
 */
#define DSA_NUM_SEGMENT_BINS 16

/*
 * What is the lowest bin that holds segments that *might* have n contiguous
 * free pages?	There is no point in looking in segments in lower bins; they
 * definitely can't service a request for n free pages.
 */
#define contiguous_pages_to_segment_bin(n) Min(fls(n), DSA_NUM_SEGMENT_BINS - 1)

/* Macros for access to locks. */
#define DSA_AREA_LOCK(area) (&area->control->lock)
#define DSA_SCLASS_LOCK(area, sclass) (&area->control->pools[sclass].lock)

/*
 * The header for an individual segment.  This lives at the start of each DSM
 * segment owned by a DSA area including the first segment (where it appears
 * as part of the dsa_area_control struct).
 */
typedef struct
{
  /* Sanity check magic value. */
  uint32 magic;
  /* Total number of pages in this segment (excluding metadata area). */
  size_t usable_pages;
  /* Total size of this segment in bytes. */
  size_t size;

  /*
   * Index of the segment that precedes this one in the same segment bin, or
   * DSA_SEGMENT_INDEX_NONE if this is the first one.
   */
  dsa_segment_index prev;

  /*
   * Index of the segment that follows this one in the same segment bin, or
   * DSA_SEGMENT_INDEX_NONE if this is the last one.
   */
  dsa_segment_index next;
  /* The index of the bin that contains this segment. */
  size_t bin;

  /*
   * A flag raised to indicate that this segment is being returned to the
   * operating system and has been unpinned.
   */
  bool freed;
} dsa_segment_header;

/*
 * Metadata for one superblock.
 *
 * For most blocks, span objects are stored out-of-line; that is, the span
 * object is not stored within the block itself.  But, as an exception, for a
 * "span of spans", the span object is stored "inline".  The allocation is
 * always exactly one page, and the dsa_area_span object is located at
 * the beginning of that page.  The size class is DSA_SCLASS_BLOCK_OF_SPANS,
 * and the remaining fields are used just as they would be in an ordinary
 * block.  We can't allocate spans out of ordinary superblocks because
 * creating an ordinary superblock requires us to be able to allocate a span
 * *first*.  Doing it this way avoids that circularity.
 */
typedef struct
{
  dsa_pointer pool;     /* Containing pool. */
  dsa_pointer prevspan; /* Previous span. */
  dsa_pointer nextspan; /* Next span. */
  dsa_pointer start;    /* Starting address. */
  size_t npages;        /* Length of span in pages. */
  uint16 size_class;    /* Size class. */
  uint16 ninitialized;  /* Maximum number of objects ever allocated. */
  uint16 nallocatable;  /* Number of objects currently allocatable. */
  uint16 firstfree;     /* First object on free list. */
  uint16 nmax;          /* Maximum number of objects ever possible. */
  uint16 fclass;        /* Current fullness class. */
} dsa_area_span;

/*
 * Given a pointer to an object in a span, access the index of the next free
 * object in the same span (ie in the span's freelist) as an L-value.
 */
#define NextFreeObjectIndex(object) (*(uint16 *)(object))

/*
 * Small allocations are handled by dividing a single block of memory into
 * many small objects of equal size.  The possible allocation sizes are
 * defined by the following array.  Larger size classes are spaced more widely
 * than smaller size classes.  We fudge the spacing for size classes >1kB to
 * avoid space wastage: based on the knowledge that we plan to allocate 64kB
 * blocks, we bump the maximum object size up to the largest multiple of
 * 8 bytes that still lets us fit the same number of objects into one block.
 *
 * NB: Because of this fudging, if we were ever to use differently-sized blocks
 * for small allocations, these size classes would need to be reworked to be
 * optimal for the new size.
 *
 * NB: The optimal spacing for size classes, as well as the size of the blocks
 * out of which small objects are allocated, is not a question that has one
 * right answer.  Some allocators (such as tcmalloc) use more closely-spaced
 * size classes than we do here, while others (like aset.c) use more
 * widely-spaced classes.  Spacing the classes more closely avoids wasting
 * memory within individual chunks, but also means a larger number of
 * potentially-unfilled blocks.
 */
static const uint16 dsa_size_classes[] = {
    sizeof(dsa_area_span), 0,      /* special size classes */
    8, 16, 24, 32, 40, 48, 56, 64, /* 8 classes separated by 8 bytes */
    80, 96, 112, 128,              /* 4 classes separated by 16 bytes */
    160, 192, 224, 256,            /* 4 classes separated by 32 bytes */
    320, 384, 448, 512,            /* 4 classes separated by 64 bytes */
    640, 768, 896, 1024,           /* 4 classes separated by 128 bytes */
    1280, 1560, 1816, 2048,        /* 4 classes separated by ~256 bytes */
    2616, 3120, 3640, 4096,        /* 4 classes separated by ~512 bytes */
    5456, 6552, 7280, 8192         /* 4 classes separated by ~1024 bytes */
};
#define DSA_NUM_SIZE_CLASSES lengthof(dsa_size_classes)

/* Special size classes. */
#define DSA_SCLASS_BLOCK_OF_SPANS 0
#define DSA_SCLASS_SPAN_LARGE 1

/*
 * The following lookup table is used to map the size of small objects
 * (less than 1kB) onto the corresponding size class.  To use this table,
 * round the size of the object up to the next multiple of 8 bytes, and then
 * index into this array.
 */
static const uint8 dsa_size_class_map[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25};
#define DSA_SIZE_CLASS_MAP_QUANTUM 8

/*
 * Superblocks are binned by how full they are.  Generally, each fullness
 * class corresponds to one quartile, but the block being used for
 * allocations is always at the head of the list for fullness class 1,
 * regardless of how full it really is.
 */
#define DSA_FULLNESS_CLASSES 4

/*
 * A dsa_area_pool represents a set of objects of a given size class.
 *
 * Perhaps there should be multiple pools for the same size class for
 * contention avoidance, but for now there is just one!
 */
typedef struct
{
  /* A lock protecting access to this pool. */
  LWLock lock;
  /* A set of linked lists of spans, arranged by fullness. */
  dsa_pointer spans[DSA_FULLNESS_CLASSES];
  /* Should we pad this out to a cacheline boundary? */
} dsa_area_pool;

/*
 * The control block for an area.  This lives in shared memory, at the start of
 * the first DSM segment controlled by this area.
 */
typedef struct
{
  /* The segment header for the first segment. */
  dsa_segment_header segment_header;
  /* The handle for this area. */
  dsa_handle handle;
  /* The handles of the segments owned by this area. */
  dsm_handle segment_handles[DSA_MAX_SEGMENTS];
  /* Lists of segments, binned by maximum contiguous run of free pages. */
  dsa_segment_index segment_bins[DSA_NUM_SEGMENT_BINS];
  /* The object pools for each size class. */
  dsa_area_pool pools[DSA_NUM_SIZE_CLASSES];
  /* The total size of all active segments. */
  size_t total_segment_size;
  /* The maximum total size of backing storage we are allowed. */
  size_t max_total_segment_size;
  /* Highest used segment index in the history of this area. */
  dsa_segment_index high_segment_index;
  /* The reference count for this area. */
  int refcnt;
  /* A flag indicating that this area has been pinned. */
  bool pinned;
  /* The number of times that segments have been freed. */
  size_t freed_segment_counter;
  /* The LWLock tranche ID. */
  int lwlock_tranche_id;
  /* The general lock (protects everything except object pools). */
  LWLock lock;
} dsa_area_control;

/* Given a pointer to a pool, find a dsa_pointer. */
#define DsaAreaPoolToDsaPointer(area, p) DSA_MAKE_POINTER(0, (char *)p - (char *)area->control)

/*
 * A dsa_segment_map is stored within the backend-private memory of each
 * individual backend.  It holds the base address of the segment within that
 * backend, plus the addresses of key objects within the segment.  Those
 * could instead be derived from the base address but it's handy to have them
 * around.
 */
typedef struct
{
  dsm_segment *segment;       /* DSM segment */
  char *mapped_address;       /* Address at which segment is mapped */
  dsa_segment_header *header; /* Header (same as mapped_address) */
  FreePageManager *fpm;       /* Free page manager within segment. */
  dsa_pointer *pagemap;       /* Page map within segment. */
} dsa_segment_map;

/*
 * Per-backend state for a storage area.  Backends obtain one of these by
 * creating an area or attaching to an existing one using a handle.  Each
 * process that needs to use an area uses its own object to track where the
 * segments are mapped.
 */
struct dsa_area
{
  /* Pointer to the control object in shared memory. */
  dsa_area_control *control;

  /* Has the mapping been pinned? */
  bool mapping_pinned;

  /*
   * This backend's array of segment maps, ordered by segment index
   * corresponding to control->segment_handles.  Some of the area's segments
   * may not be mapped in this backend yet, and some slots may have been
   * freed and need to be detached; these operations happen on demand.
   */
  dsa_segment_map segment_maps[DSA_MAX_SEGMENTS];

  /* The highest segment index this backend has ever mapped. */
  dsa_segment_index high_segment_index;

  /* The last observed freed_segment_counter. */
  size_t freed_segment_counter;
};

#define DSA_SPAN_NOTHING_FREE ((uint16)-1)
#define DSA_SUPERBLOCK_SIZE (DSA_PAGES_PER_SUPERBLOCK * FPM_PAGE_SIZE)

/* Given a pointer to a segment_map, obtain a segment index number. */
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

/*
 * Create a new shared area in a new DSM segment.  Further DSM segments will
 * be allocated as required to extend the available space.
 *
 * We can't allocate a LWLock tranche_id within this function, because tranche
 * IDs are a scarce resource; there are only 64k available, using low numbers
 * when possible matters, and we have no provision for recycling them.  So,
 * we require the caller to provide one.
 */
dsa_area *
dsa_create(int tranche_id)
{
























}

/*
 * Create a new shared area in an existing shared memory space, which may be
 * either DSM or Postmaster-initialized memory.  DSM segments will be
 * allocated as required to extend the available space, though that can be
 * prevented with dsa_set_size_limit(area, size) using the same size provided
 * to dsa_create_in_place.
 *
 * Areas created in-place must eventually be released by the backend that
 * created them and all backends that attach to them.  This can be done
 * explicitly with dsa_release_in_place, or, in the special case that 'place'
 * happens to be in a pre-existing DSM segment, by passing in a pointer to the
 * segment so that a detach hook can be registered with the containing DSM
 * segment.
 *
 * See dsa_create() for a note about the tranche arguments.
 */
dsa_area *
dsa_create_in_place(void *place, size_t size, int tranche_id, dsm_segment *segment)
{














}

/*
 * Obtain a handle that can be passed to other processes so that they can
 * attach to the given area.  Cannot be called for areas created with
 * dsa_create_in_place.
 */
dsa_handle
dsa_get_handle(dsa_area *area)
{


}

/*
 * Attach to an area given a handle generated (possibly in another process) by
 * dsa_get_handle.  The area must have been created with dsa_create (not
 * dsa_create_in_place).
 */
dsa_area *
dsa_attach(dsa_handle handle)
{



















}

/*
 * Attach to an area that was created with dsa_create_in_place.  The caller
 * must somehow know the location in memory that was used when the area was
 * created, though it may be mapped at a different virtual address in this
 * process.
 *
 * See dsa_create_in_place for note about releasing in-place areas, and the
 * optional 'segment' argument which can be provided to allow automatic
 * release if the containing memory happens to be a DSM segment.
 */
dsa_area *
dsa_attach_in_place(void *place, dsm_segment *segment)
{














}

/*
 * Release a DSA area that was produced by dsa_create_in_place or
 * dsa_attach_in_place.  The 'segment' argument is ignored but provides an
 * interface suitable for on_dsm_detach, for the convenience of users who want
 * to create a DSA segment inside an existing DSM segment and have it
 * automatically released when the containing DSM segment is detached.
 * 'place' should be the address of the place where the area was created.
 *
 * This callback is automatically registered for the DSM segment containing
 * the control object of in-place areas when a segment is provided to
 * dsa_create_in_place or dsa_attach_in_place, and also for all areas created
 * with dsa_create.
 */
void
dsa_on_dsm_detach_release_in_place(dsm_segment *segment, Datum place)
{

}

/*
 * Release a DSA area that was produced by dsa_create_in_place or
 * dsa_attach_in_place.  The 'code' argument is ignored but provides an
 * interface suitable for on_shmem_exit or before_shmem_exit, for the
 * convenience of users who want to create a DSA segment inside shared memory
 * other than a DSM segment and have it automatically release at backend exit.
 * 'place' should be the address of the place where the area was created.
 */
void
dsa_on_shmem_exit_release_in_place(int code, Datum place)
{

}

/*
 * Release a DSA area that was produced by dsa_create_in_place or
 * dsa_attach_in_place.  It is preferable to use one of the 'dsa_on_XXX'
 * callbacks so that this is managed automatically, because failure to release
 * an area created in-place leaks its segments permanently.
 *
 * This is also called automatically for areas produced by dsa_create or
 * dsa_attach as an implementation detail.
 */
void
dsa_release_in_place(void *place)
{




















}

/*
 * Keep a DSA area attached until end of session or explicit detach.
 *
 * By default, areas are owned by the current resource owner, which means they
 * are detached automatically when that scope ends.
 */
void
dsa_pin_mapping(dsa_area *area)
{












}

/*
 * Allocate memory in this storage area.  The return value is a dsa_pointer
 * that can be passed to other processes, and converted to a local pointer
 * with dsa_get_address.  'flags' is a bitmap which should be constructed
 * from the following values:
 *
 * DSA_ALLOC_HUGE allows allocations >= 1GB.  Otherwise, such allocations
 * will result in an ERROR.
 *
 * DSA_ALLOC_NO_OOM causes this function to return InvalidDsaPointer when
 * no memory is available or a size limit established by dsa_set_size_limit
 * would be exceeded.  Otherwise, such allocations will result in an ERROR.
 *
 * DSA_ALLOC_ZERO causes the allocated memory to be zeroed.  Otherwise, the
 * contents of newly-allocated memory are indeterminate.
 *
 * These flags correspond to similarly named flags used by
 * MemoryContextAllocExtended().  See also the macros dsa_allocate and
 * dsa_allocate0 which expand to a call to this function with commonly used
 * flags.
 */
dsa_pointer
dsa_allocate_extended(dsa_area *area, size_t size, int flags)
{






















































































































































}

/*
 * Free memory obtained with dsa_allocate.
 */
void
dsa_free(dsa_area *area, dsa_pointer dp)
{
































































































}

/*
 * Obtain a backend-local address for a dsa_pointer.  'dp' must point to
 * memory allocated by the given area (possibly in another process) that
 * hasn't yet been freed.  This may cause a segment to be mapped into the
 * current process if required, and may cause freed segments to be unmapped.
 */
void *
dsa_get_address(dsa_area *area, dsa_pointer dp)
{

























}

/*
 * Pin this area, so that it will continue to exist even if all backends
 * detach from it.  In that case, the area can still be reattached to if a
 * handle has been recorded somewhere.
 */
void
dsa_pin(dsa_area *area)
{









}

/*
 * Undo the effects of dsa_pin, so that the given area can be freed when no
 * backends are attached to it.  May be called only if dsa_pin has been
 * called.
 */
void
dsa_unpin(dsa_area *area)
{










}

/*
 * Set the total size limit for this area.  This limit is checked whenever new
 * segments need to be allocated from the operating system.  If the new size
 * limit is already exceeded, this has no immediate effect.
 *
 * Note that the total virtual memory usage may be temporarily larger than
 * this limit when segments have been freed, but not yet detached by all
 * backends that have attached to them.
 */
void
dsa_set_size_limit(dsa_area *area, size_t limit)
{



}

/*
 * Aggressively free all spare memory in the hope of returning DSM segments to
 * the operating system.
 */
void
dsa_trim(dsa_area *area)
{






































}

/*
 * Print out debugging information about the internal state of the shared
 * memory area.
 */
void
dsa_dump(dsa_area *area)
{























































































}

/*
 * Return the smallest size that you can successfully provide to
 * dsa_create_in_place.
 */
size_t
dsa_minimum_size(void)
{













}

/*
 * Workhorse function for dsa_create and dsa_create_in_place.
 */
static dsa_area *
create_internal(void *place, size_t size, int tranche_id, dsm_handle control_handle, dsm_segment *control_segment)
{

























































































}

/*
 * Workhorse function for dsa_attach and dsa_attach_in_place.
 */
static dsa_area *
attach_internal(void *place, dsm_segment *segment, dsa_handle handle)
{




































}

/*
 * Add a new span to fullness class 1 of the indicated pool.
 */
static void
init_span(dsa_area *area, dsa_pointer span_pointer, dsa_area_pool *pool, dsa_pointer start, size_t npages, uint16 size_class)
{












































}

/*
 * Transfer the first span in one fullness class to the head of another
 * fullness class.
 */
static bool
transfer_first_span(dsa_area *area, dsa_area_pool *pool, int fromclass, int toclass)
{































}

/*
 * Allocate one object of the requested size class from the given area.
 */
static inline dsa_pointer
alloc_object(dsa_area *area, int size_class)
{





























































}

/*
 * Ensure an active (i.e. fullness class 1) superblock, unless all existing
 * superblocks are completely full and no more can be allocated.
 *
 * Fullness classes K of 0..N are loosely intended to represent blocks whose
 * utilization percentage is at least K/N, but we only enforce this rigorously
 * for the highest-numbered fullness class, which always contains exactly
 * those blocks that are completely full.  It's otherwise acceptable for a
 * block to be in a higher-numbered fullness class than the one to which it
 * logically belongs.  In addition, the active block, which is always the
 * first block in fullness class 1, is permitted to have a higher allocation
 * percentage than would normally be allowable for that fullness class; we
 * don't move it until it's completely full, and then it goes to the
 * highest-numbered fullness class.
 *
 * It might seem odd that the active block is the head of fullness class 1
 * rather than fullness class 0, but experience with other allocators has
 * shown that it's usually better to allocate from a block that's moderately
 * full rather than one that's nearly empty.  Insofar as is reasonably
 * possible, we want to avoid performing new allocations in a block that would
 * otherwise become empty soon.
 */
static bool
ensure_active_superblock(dsa_area *area, dsa_area_pool *pool, int size_class)
{







































































































































































































}

/*
 * Return the segment map corresponding to a given segment index, mapping the
 * segment in if necessary.  For internal segment book-keeping, this is called
 * with the area lock held.  It is also called by dsa_free and dsa_get_address
 * without any locking, relying on the fact they have a known live segment
 * index and they always call check_for_freed_segments to ensures that any
 * freed segment occupying the same slot is detached first.
 */
static dsa_segment_map *
get_segment_by_index(dsa_area *area, dsa_segment_index index)
{



































































}

/*
 * Return a superblock to the free page manager.  If the underlying segment
 * has become entirely free, then return it to the operating system.
 *
 * The appropriate pool lock must be held.
 */
static void
destroy_superblock(dsa_area *area, dsa_pointer span_pointer)
{























































}

static void
unlink_span(dsa_area *area, dsa_area_span *span)
{


















}

static void
add_span_to_fullness_class(dsa_area *area, dsa_area_span *span, dsa_pointer span_pointer, int fclass)
{












}

/*
 * Detach from an area that was either created or attached to by this process.
 */
void
dsa_detach(dsa_area *area)
{






















}

/*
 * Unlink a segment from the bin that contains it.
 */
static void
unlink_segment(dsa_area *area, dsa_segment_map *segment_map)
{



















}

/*
 * Find a segment that could satisfy a request for 'npages' of contiguous
 * memory, or return NULL if none can be found.  This may involve attaching to
 * segments that weren't previously attached so that we can query their free
 * pages map.
 */
static dsa_segment_map *
get_best_segment(dsa_area *area, size_t npages)
{
















































































}

/*
 * Create a new segment that can handle at least requested_pages.  Returns
 * NULL if the requested total size limit or maximum allowed number of
 * segments would be exceeded.
 */
static dsa_segment_map *
make_new_segment(dsa_area *area, size_t requested_pages)
{

























































































































































}

/*
 * Check if any segments have been freed by destroy_superblock, so we can
 * detach from them in this backend.  This function is called by
 * dsa_get_address and dsa_free to make sure that a dsa_pointer they have
 * received can be resolved to the correct segment.
 *
 * The danger we want to defend against is that there could be an old segment
 * mapped into a given slot in this backend, and the dsa_pointer they have
 * might refer to some new segment in the same slot.  So those functions must
 * be sure to process all instructions to detach from a freed segment that had
 * been generated by the time this process received the dsa_pointer, before
 * they call get_segment_by_index.
 */
static void
check_for_freed_segments(dsa_area *area)
{
























}

/*
 * Workhorse for check_for_freed_segments(), and also used directly in path
 * where the area lock is already held.  This should be called after acquiring
 * the lock but before looking up any segment by index number, to make sure we
 * unmap any stale segments that might have previously had the same index as a
 * current segment.
 */
static void
check_for_freed_segments_locked(dsa_area *area)
{



















}