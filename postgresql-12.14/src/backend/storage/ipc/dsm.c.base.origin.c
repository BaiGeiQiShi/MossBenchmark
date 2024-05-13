/*-------------------------------------------------------------------------
 *
 * dsm.c
 *	  manage dynamic shared memory segments
 *
 * This file provides a set of services to make programming with dynamic
 * shared memory segments more convenient.  Unlike the low-level
 * facilities provided by dsm_impl.h and dsm_impl.c, mappings and segments
 * created using this module will be cleaned up automatically.  Mappings
 * will be removed when the resource owner under which they were created
 * is cleaned up, unless dsm_pin_mapping() is used, in which case they
 * have session lifespan.  Segments will be removed when there are no
 * remaining mappings, or at postmaster shutdown in any case.  After a
 * hard postmaster crash, remaining segments will be removed, if they
 * still exist, at the next postmaster startup.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/ipc/dsm.c
 *
 *-------------------------------------------------------------------------
 */

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

/* Backend-local tracking for on-detach callbacks. */
typedef struct dsm_segment_detach_callback
{
  on_dsm_detach_callback function;
  Datum arg;
  slist_node node;
} dsm_segment_detach_callback;

/* Backend-local state for a dynamic shared memory segment. */
struct dsm_segment
{
  dlist_node node;        /* List link in dsm_segment_list. */
  ResourceOwner resowner; /* Resource owner. */
  dsm_handle handle;      /* Segment name. */
  uint32 control_slot;    /* Slot in control segment. */
  void *impl_private;     /* Implementation-specific private data. */
  void *mapped_address;   /* Mapping address, or NULL if unmapped. */
  Size mapped_size;       /* Size of our mapping. */
  slist_head on_detach;   /* On-detach callbacks. */
};

/* Shared-memory state for a dynamic shared memory segment. */
typedef struct dsm_control_item
{
  dsm_handle handle;
  uint32 refcnt;                /* 2+ = active, 1 = moribund, 0 = gone */
  void *impl_private_pm_handle; /* only needed on Windows */
  bool pinned;
} dsm_control_item;

/* Layout of the dynamic shared memory control segment. */
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

/* Has this backend initialized the dynamic shared memory system yet? */
static bool dsm_init_done = false;

/*
 * List of dynamic shared memory segments used by this backend.
 *
 * At process exit time, we must decrement the reference count of each
 * segment we have attached; this list makes it possible to find all such
 * segments.
 *
 * This list should always be empty in the postmaster.  We could probably
 * allow the postmaster to map dynamic shared memory segments before it
 * begins to start child processes, provided that each process adjusted
 * the reference counts for those segments in the control segment at
 * startup time, but there's no obvious need for such a facility, which
 * would also be complex to handle in the EXEC_BACKEND case.  Once the
 * postmaster has begun spawning children, there's an additional problem:
 * each new mapping would require an update to the control segment,
 * which requires locking, in which the postmaster must not be involved.
 */
static dlist_head dsm_segment_list = DLIST_STATIC_INIT(dsm_segment_list);

/*
 * Control segment information.
 *
 * Unlike ordinary shared memory segments, the control segment is not
 * reference counted; instead, it lasts for the postmaster's entire
 * life cycle.  For simplicity, it doesn't have a dsm_segment object either.
 */
static dsm_handle dsm_control_handle;
static dsm_control_header *dsm_control;
static Size dsm_control_mapped_size = 0;
static void *dsm_control_impl_private = NULL;

/*
 * Start up the dynamic shared memory system.
 *
 * This is called just once during each cluster lifetime, at postmaster
 * startup time.
 */
void
dsm_postmaster_startup(PGShmemHeader *shim)
{



















































}

/*
 * Determine whether the control segment from the previous postmaster
 * invocation still exists.  If so, remove the dynamic shared memory
 * segments to which it refers, and then the control segment itself.
 */
void
dsm_cleanup_using_control_segment(dsm_handle old_control_handle)
{




























































}

/*
 * When we're using the mmap shared memory implementation, "shared memory"
 * segments might even manage to survive an operating system reboot.
 * But there's no guarantee as to exactly what will survive: some segments
 * may survive, and others may not, and the contents of some may be out
 * of date.  In particular, the control segment may be out of date, so we
 * can't rely on it to figure out what to remove.  However, since we know
 * what directory contains the files we used as shared memory, we can simply
 * scan the directory and blow everything away that shouldn't be there.
 */
static void
dsm_cleanup_for_mmap(void)
{


























}

/*
 * At shutdown time, we iterate over the control segment and remove all
 * remaining dynamic shared memory segments.  We avoid throwing errors here;
 * the postmaster is shutting down either way, and this is just non-critical
 * resource cleanup.
 */
static void
dsm_postmaster_shutdown(int code, Datum arg)
{















































}

/*
 * Prepare this backend for dynamic shared memory usage.  Under EXEC_BACKEND,
 * we must reread the state file and map the control segment; in other cases,
 * we'll have inherited the postmaster's mapping and global variables.
 */
static void
dsm_backend_startup(void)
{


















}

#ifdef EXEC_BACKEND
/*
 * When running under EXEC_BACKEND, we get a callback here when the main
 * shared memory segment is re-attached, so that we can record the control
 * handle retrieved from it.
 */
void
dsm_set_control_handle(dsm_handle h)
{
  Assert(dsm_control_handle == 0 && h != 0);
  dsm_control_handle = h;
}
#endif

/*
 * Create a new dynamic shared memory segment.
 *
 * If there is a non-NULL CurrentResourceOwner, the new segment is associated
 * with it and must be detached before the resource owner releases, or a
 * warning will be logged.  If CurrentResourceOwner is NULL, the segment
 * remains attached until explicitly detached or the session ends.
 * Creating with a NULL CurrentResourceOwner is equivalent to creating
 * with a non-NULL CurrentResourceOwner and then calling dsm_pin_mapping.
 */
dsm_segment *
dsm_create(Size size, int flags)
{
















































































}

/*
 * Attach a dynamic shared memory segment.
 *
 * See comments for dsm_segment_handle() for an explanation of how this
 * is intended to be used.
 *
 * This function will return NULL if the segment isn't known to the system.
 * This can happen if we're asked to attach the segment, but then everyone
 * else detaches it (causing it to be destroyed) before we get around to
 * attaching it.
 *
 * If there is a non-NULL CurrentResourceOwner, the attached segment is
 * associated with it and must be detached before the resource owner releases,
 * or a warning will be logged.  Otherwise the segment remains attached until
 * explicitly detached or the session ends.  See the note atop dsm_create().
 */
dsm_segment *
dsm_attach(dsm_handle h)
{


















































































}

/*
 * At backend shutdown time, detach any segments that are still attached.
 * (This is similar to dsm_detach_all, except that there's no reason to
 * unmap the control segment before exiting, so we don't bother.)
 */
void
dsm_backend_shutdown(void)
{







}

/*
 * Detach all shared memory segments, including the control segments.  This
 * should be called, along with PGSharedMemoryDetach, in processes that
 * might inherit mappings but are not intended to be connected to dynamic
 * shared memory.
 */
void
dsm_detach_all(void)
{














}

/*
 * Detach from a shared memory segment, destroying the segment if we
 * remove the last reference.
 *
 * This function should never fail.  It will often be invoked when aborting
 * a transaction, and a further error won't serve any purpose.  It's not a
 * complete disaster if we fail to unmap or destroy the segment; it means a
 * resource leak, but that doesn't necessarily preclude further operations.
 */
void
dsm_detach(dsm_segment *seg)
{






























































































}

/*
 * Keep a dynamic shared memory mapping until end of session.
 *
 * By default, mappings are owned by the current resource owner, which
 * typically means they stick around for the duration of the current query
 * only.
 */
void
dsm_pin_mapping(dsm_segment *seg)
{





}

/*
 * Arrange to remove a dynamic shared memory mapping at cleanup time.
 *
 * dsm_pin_mapping() can be used to preserve a mapping for the entire
 * lifetime of a process; this function reverses that decision, making
 * the segment owned by the current resource owner.  This may be useful
 * just before performing some operation that will invalidate the segment
 * for future use by this backend.
 */
void
dsm_unpin_mapping(dsm_segment *seg)
{




}

/*
 * Keep a dynamic shared memory segment until postmaster shutdown, or until
 * dsm_unpin_segment is called.
 *
 * This function should not be called more than once per segment, unless the
 * segment is explicitly unpinned with dsm_unpin_segment in between calls.
 *
 * Note that this function does not arrange for the current process to
 * keep the segment mapped indefinitely; if that behavior is desired,
 * dsm_pin_mapping() should be used from each process that needs to
 * retain the mapping.
 */
void
dsm_pin_segment(dsm_segment *seg)
{


















}

/*
 * Unpin a dynamic shared memory segment that was previously pinned with
 * dsm_pin_segment.  This function should not be called unless dsm_pin_segment
 * was previously called for this segment.
 *
 * The argument is a dsm_handle rather than a dsm_segment in case you want
 * to unpin a segment to which you haven't attached.  This turns out to be
 * useful if, for example, a reference to one shared memory segment is stored
 * within another shared memory segment.  You might want to unpin the
 * referenced segment before destroying the referencing segment.
 */
void
dsm_unpin_segment(dsm_handle handle)
{
















































































}

/*
 * Find an existing mapping for a shared memory segment, if there is one.
 */
dsm_segment *
dsm_find_mapping(dsm_handle h)
{













}

/*
 * Get the address at which a dynamic shared memory segment is mapped.
 */
void *
dsm_segment_address(dsm_segment *seg)
{


}

/*
 * Get the size of a mapping.
 */
Size
dsm_segment_map_length(dsm_segment *seg)
{


}

/*
 * Get a handle for a mapping.
 *
 * To establish communication via dynamic shared memory between two backends,
 * one of them should first call dsm_create() to establish a new shared
 * memory mapping.  That process should then call dsm_segment_handle() to
 * obtain a handle for the mapping, and pass that handle to the
 * coordinating backend via some means (e.g. bgw_main_arg, or via the
 * main shared memory segment).  The recipient, once in possession of the
 * handle, should call dsm_attach().
 */
dsm_handle
dsm_segment_handle(dsm_segment *seg)
{

}

/*
 * Register an on-detach callback for a dynamic shared memory segment.
 */
void
on_dsm_detach(dsm_segment *seg, on_dsm_detach_callback function, Datum arg)
{






}

/*
 * Unregister an on-detach callback for a dynamic shared memory segment.
 */
void
cancel_on_dsm_detach(dsm_segment *seg, on_dsm_detach_callback function, Datum arg)
{














}

/*
 * Discard all registered on-detach callbacks without executing them.
 */
void
reset_on_dsm_detach(void)
{























}

/*
 * Create a segment descriptor.
 */
static dsm_segment *
dsm_create_descriptor(void)
{

























}

/*
 * Sanity check a control segment.
 *
 * The goal here isn't to detect everything that could possibly be wrong with
 * the control segment; there's not enough information for that.  Rather, the
 * goal is to make sure that someone can iterate over the items in the segment
 * without overrunning the end of the mapping and crashing.  We also check
 * the magic number since, if that's messed up, this may not even be one of
 * our segments at all.
 */
static bool
dsm_control_segment_sane(dsm_control_header *control, Size mapped_size)
{

















}

/*
 * Compute the number of control-segment bytes needed to store a given
 * number of items.
 */
static uint64
dsm_control_bytes_needed(uint32 nitems)
{

}