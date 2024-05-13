/*-------------------------------------------------------------------------
 *
 * shmem.c
 *	  create shared memory and initialize shared memory data structures.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/ipc/shmem.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * POSTGRES processes share one or more regions of shared memory.
 * The shared memory is created by a postmaster and is inherited
 * by each backend via fork() (or, in some ports, via other OS-specific
 * methods).  The routines in this file are used for allocating and
 * binding to shared memory data structures.
 *
 * NOTES:
 *		(a) There are three kinds of shared memory data structures
 *	available to POSTGRES: fixed-size structures, queues and hash
 *	tables.  Fixed-size structures contain things like global variables
 *	for a module and should never be allocated after the shared memory
 *	initialization phase.  Hash tables have a fixed maximum size, but
 *	their actual size can vary dynamically.  When entries are added
 *	to the table, more space is allocated.  Queues link data structures
 *	that have been allocated either within fixed-size structures or as hash
 *	buckets.  Each shared data structure has a string name to identify
 *	it (assigned in the module that declares it).
 *
 *		(b) During initialization, each module looks for its
 *	shared data structures in a hash table called the "Shmem Index".
 *	If the data structure is not present, the caller can allocate
 *	a new one and initialize it.  If the data structure is present,
 *	the caller "attaches" to the structure by initializing a pointer
 *	in the local address space.
 *		The shmem index has two purposes: first, it gives us
 *	a simple model of how the world looks when a backend process
 *	initializes.  If something is present in the shmem index,
 *	it is initialized.  If it is not, it is uninitialized.  Second,
 *	the shmem index allows us to allocate shared memory on demand
 *	instead of trying to preallocate structures and hard-wire the
 *	sizes and locations in header files.  If you are using a lot
 *	of shared memory in a lot of different places (and changing
 *	things during development), this is important.
 *
 *		(c) In standard Unix-ish environments, individual backends do
 *not need to re-establish their local pointers into shared memory, because they
 *inherit correct values of those variables via fork() from the postmaster.
 *However, this does not work in the EXEC_BACKEND case. In ports using
 *EXEC_BACKEND, new backends have to set up their local pointers using the
 *method described in (b) above.
 *
 *		(d) memory allocation model: shared memory can never be
 *	freed, once allocated.   Each hash table has its own free list,
 *	so hash buckets can be reused when an item is deleted.  However,
 *	if one hash table grows very large and then shrinks, its space
 *	cannot be redistributed to other tables.  We could build a simple
 *	hash bucket garbage collector if need be.  Right now, it seems
 *	unnecessary.
 */

#include "postgres.h"

#include "access/transam.h"
#include "miscadmin.h"
#include "storage/lwlock.h"
#include "storage/pg_shmem.h"
#include "storage/shmem.h"
#include "storage/spin.h"

/* shared memory global variables */

static PGShmemHeader *ShmemSegHdr; /* shared mem segment header */

static void *ShmemBase; /* start address of shared memory */

static void *ShmemEnd; /* end+1 address of shared memory */

slock_t *ShmemLock; /* spinlock for shared memory and LWLock
                     * allocation */

static HTAB *ShmemIndex = NULL; /* primary index hashtable for shmem */

/*
 *	InitShmemAccess() --- set up basic pointers to shared memory.
 *
 * Note: the argument should be declared "PGShmemHeader *seghdr",
 * but we use void to avoid having to include ipc.h in shmem.h.
 */
void
InitShmemAccess(void *seghdr)
{





}

/*
 *	InitShmemAllocation() --- set up shared-memory space allocation.
 *
 * This should be called only in the postmaster or a standalone backend.
 */
void
InitShmemAllocation(void)
{































}

/*
 * ShmemAlloc -- allocate max-aligned chunk from shared memory
 *
 * Throws error if request cannot be satisfied.
 *
 * Assumes ShmemLock and ShmemSegHdr are initialized.
 */
void *
ShmemAlloc(Size size)
{








}

/*
 * ShmemAllocNoError -- allocate max-aligned chunk from shared memory
 *
 * As ShmemAlloc, but returns NULL if out of space, rather than erroring.
 */
void *
ShmemAllocNoError(Size size)
{








































}

/*
 * ShmemAllocUnlocked -- allocate max-aligned chunk from shared memory
 *
 * Allocate space without locking ShmemLock.  This should be used for,
 * and only for, allocations that must happen before ShmemLock is ready.
 *
 * We consider maxalign, rather than cachealign, sufficient here.
 */
void *
ShmemAllocUnlocked(Size size)
{

























}

/*
 * ShmemAddrIsValid -- test if an address refers to shared memory
 *
 * Returns true if the pointer points within the shared memory segment.
 */
bool
ShmemAddrIsValid(const void *addr)
{

}

/*
 *	InitShmemIndex() --- set up or attach to shmem index table.
 */
void
InitShmemIndex(void)
{
















}

/*
 * ShmemInitHash -- Create and initialize, or attach to, a
 *		shared memory hash table.
 *
 * We assume caller is doing some kind of synchronization
 * so that two processes don't try to create/initialize the same
 * table at once.  (In practice, all creations are done in the postmaster
 * process; child processes should always be attaching to existing tables.)
 *
 * max_size is the estimated maximum number of hashtable entries.  This is
 * not a hard limit, but the access efficiency will degrade if it is
 * exceeded substantially (since it's used to compute directory size and
 * the hash table buckets will get overfull).
 *
 * init_size is the number of hashtable entries to preallocate.  For a table
 * whose maximum size is certain, this should be equal to max_size; that
 * ensures that no run-time out-of-shared-memory failures can occur.
 *
 * Note: before Postgres 9.0, this function returned NULL for some failure
 * cases.  Now, it always throws error instead, so callers need not check
 * for NULL.
 */
HTAB *
ShmemInitHash(const char *name, /* table string name for shmem index */
    long init_size,             /* initial table size */
    long max_size,              /* max size of the table */
    HASHCTL *infoP,             /* info about key and bucket size */
    int hash_flags)             /* info about infoP */
{






























}

/*
 * ShmemInitStruct -- Create/attach to a structure in shared memory.
 *
 *		This is called during initialization to find or allocate
 *		a data structure in shared memory.  If no other process
 *		has created the structure, this routine allocates space
 *		for it.  If it exists already, a pointer to the existing
 *		structure is returned.
 *
 *	Returns: pointer to the object.  *foundPtr is set true if the object was
 *		already in the shmem index (hence, already initialized).
 *
 *	Note: before Postgres 9.0, this function returned NULL for some failure
 *	cases.  Now, it always throws error instead, so callers need not check
 *	for NULL.
 */
void *
ShmemInitStruct(const char *name, Size size, bool *foundPtr)
{



















































































}

/*
 * Add two Size values, checking for overflow
 */
Size
add_size(Size s1, Size s2)
{









}

/*
 * Multiply two Size values, checking for overflow
 */
Size
mul_size(Size s1, Size s2)
{













}