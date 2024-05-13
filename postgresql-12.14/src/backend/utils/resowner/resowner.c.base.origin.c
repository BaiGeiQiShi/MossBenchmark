/*-------------------------------------------------------------------------
 *
 * resowner.c
 *	  POSTGRES resource owner management code.
 *
 * Query-lifespan resources are tracked by associating them with
 * ResourceOwner objects.  This provides a simple mechanism for ensuring
 * that such resources are freed at the right time.
 * See utils/resowner/README for more info.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/resowner/resowner.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "jit/jit.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "utils/hashutils.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/snapmgr.h"

/*
 * All resource IDs managed by this code are required to fit into a Datum,
 * which is fine since they are generally pointers or integers.
 *
 * Provide Datum conversion macros for a couple of things that are really
 * just "int".
 */
#define FileGetDatum(file) Int32GetDatum(file)
#define DatumGetFile(datum) ((File)DatumGetInt32(datum))
#define BufferGetDatum(buffer) Int32GetDatum(buffer)
#define DatumGetBuffer(datum) ((Buffer)DatumGetInt32(datum))

/*
 * ResourceArray is a common structure for storing all types of resource IDs.
 *
 * We manage small sets of resource IDs by keeping them in a simple array:
 * itemsarr[k] holds an ID, for 0 <= k < nitems <= maxitems = capacity.
 *
 * If a set grows large, we switch over to using open-addressing hashing.
 * Then, itemsarr[] is a hash table of "capacity" slots, with each
 * slot holding either an ID or "invalidval".  nitems is the number of valid
 * items present; if it would exceed maxitems, we enlarge the array and
 * re-hash.  In this mode, maxitems should be rather less than capacity so
 * that we don't waste too much time searching for empty slots.
 *
 * In either mode, lastidx remembers the location of the last item inserted
 * or returned by GetAny; this speeds up searches in ResourceArrayRemove.
 */
typedef struct ResourceArray
{
  Datum *itemsarr;  /* buffer for storing values */
  Datum invalidval; /* value that is considered invalid */
  uint32 capacity;  /* allocated length of itemsarr[] */
  uint32 nitems;    /* how many items are stored in items array */
  uint32 maxitems;  /* current limit on nitems before enlarging */
  uint32 lastidx;   /* index of last item returned by GetAny */
} ResourceArray;

/*
 * Initially allocated size of a ResourceArray.  Must be power of two since
 * we'll use (arraysize - 1) as mask for hashing.
 */
#define RESARRAY_INIT_SIZE 16

/*
 * When to switch to hashing vs. simple array logic in a ResourceArray.
 */
#define RESARRAY_MAX_ARRAY 64
#define RESARRAY_IS_ARRAY(resarr) ((resarr)->capacity <= RESARRAY_MAX_ARRAY)

/*
 * How many items may be stored in a resource array of given capacity.
 * When this number is reached, we must resize.
 */
#define RESARRAY_MAX_ITEMS(capacity) ((capacity) <= RESARRAY_MAX_ARRAY ? (capacity) : (capacity) / 4 * 3)

/*
 * To speed up bulk releasing or reassigning locks from a resource owner to
 * its parent, each resource owner has a small cache of locks it owns. The
 * lock manager has the same information in its local lock hash table, and
 * we fall back on that if cache overflows, but traversing the hash table
 * is slower when there are a lot of locks belonging to other resource owners.
 *
 * MAX_RESOWNER_LOCKS is the size of the per-resource owner cache. It's
 * chosen based on some testing with pg_dump with a large schema. When the
 * tests were done (on 9.2), resource owners in a pg_dump run contained up
 * to 9 locks, regardless of the schema size, except for the top resource
 * owner which contained much more (overflowing the cache). 15 seems like a
 * nice round number that's somewhat higher than what pg_dump needs. Note that
 * making this number larger is not free - the bigger the cache, the slower
 * it is to release locks (in retail), when a resource owner holds many locks.
 */
#define MAX_RESOWNER_LOCKS 15

/*
 * ResourceOwner objects look like this
 */
typedef struct ResourceOwnerData
{
  ResourceOwner parent;     /* NULL if no parent (toplevel owner) */
  ResourceOwner firstchild; /* head of linked list of children */
  ResourceOwner nextchild;  /* next child of same parent */
  const char *name;         /* name (just for debugging) */

  /* We have built-in support for remembering: */
  ResourceArray bufferarr;     /* owned buffers */
  ResourceArray catrefarr;     /* catcache references */
  ResourceArray catlistrefarr; /* catcache-list pins */
  ResourceArray relrefarr;     /* relcache references */
  ResourceArray planrefarr;    /* plancache references */
  ResourceArray tupdescarr;    /* tupdesc references */
  ResourceArray snapshotarr;   /* snapshot references */
  ResourceArray filearr;       /* open temporary files */
  ResourceArray dsmarr;        /* dynamic shmem segments */
  ResourceArray jitarr;        /* JIT contexts */

  /* We can remember up to MAX_RESOWNER_LOCKS references to local locks. */
  int nlocks;                           /* number of owned locks */
  LOCALLOCK *locks[MAX_RESOWNER_LOCKS]; /* list of owned locks */
} ResourceOwnerData;

/*****************************************************************************
 *	  GLOBAL MEMORY
 **
 *****************************************************************************/

ResourceOwner CurrentResourceOwner = NULL;
ResourceOwner CurTransactionResourceOwner = NULL;
ResourceOwner TopTransactionResourceOwner = NULL;
ResourceOwner AuxProcessResourceOwner = NULL;

/*
 * List of add-on callbacks for resource releasing
 */
typedef struct ResourceReleaseCallbackItem
{
  struct ResourceReleaseCallbackItem *next;
  ResourceReleaseCallback callback;
  void *arg;
} ResourceReleaseCallbackItem;

static ResourceReleaseCallbackItem *ResourceRelease_callbacks = NULL;

/* Internal routines */
static void
ResourceArrayInit(ResourceArray *resarr, Datum invalidval);
static void
ResourceArrayEnlarge(ResourceArray *resarr);
static void
ResourceArrayAdd(ResourceArray *resarr, Datum value);
static bool
ResourceArrayRemove(ResourceArray *resarr, Datum value);
static bool
ResourceArrayGetAny(ResourceArray *resarr, Datum *value);
static void
ResourceArrayFree(ResourceArray *resarr);
static void
ResourceOwnerReleaseInternal(ResourceOwner owner, ResourceReleasePhase phase, bool isCommit, bool isTopLevel);
static void
ReleaseAuxProcessResourcesCallback(int code, Datum arg);
static void
PrintRelCacheLeakWarning(Relation rel);
static void
PrintPlanCacheLeakWarning(CachedPlan *plan);
static void
PrintTupleDescLeakWarning(TupleDesc tupdesc);
static void
PrintSnapshotLeakWarning(Snapshot snapshot);
static void
PrintFileLeakWarning(File file);
static void
PrintDSMLeakWarning(dsm_segment *seg);

/*****************************************************************************
 *	  INTERNAL ROUTINES
 **
 *****************************************************************************/

/*
 * Initialize a ResourceArray
 */
static void
ResourceArrayInit(ResourceArray *resarr, Datum invalidval)
{








}

/*
 * Make sure there is room for at least one more resource in an array.
 *
 * This is separate from actually inserting a resource because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
static void
ResourceArrayEnlarge(ResourceArray *resarr)
{
















































}

/*
 * Add a resource to ResourceArray
 *
 * Caller must have previously done ResourceArrayEnlarge()
 */
static void
ResourceArrayAdd(ResourceArray *resarr, Datum value)
{




























}

/*
 * Remove a resource from ResourceArray
 *
 * Returns true on success, false if resource was not found.
 *
 * Note: if same resource ID appears more than once, one instance is removed.
 */
static bool
ResourceArrayRemove(ResourceArray *resarr, Datum value)
{



















































}

/*
 * Get any convenient entry in a ResourceArray.
 *
 * "Convenient" is defined as "easy for ResourceArrayRemove to remove";
 * we help that along by setting lastidx to match.  This avoids O(N^2) cost
 * when removing all ResourceArray items during ResourceOwner destruction.
 *
 * Returns true if we found an element, or false if the array is empty.
 */
static bool
ResourceArrayGetAny(ResourceArray *resarr, Datum *value)
{




























}

/*
 * Trash a ResourceArray (we don't care about its state after this)
 */
static void
ResourceArrayFree(ResourceArray *resarr)
{




}

/*****************************************************************************
 *	  EXPORTED ROUTINES
 **
 *****************************************************************************/

/*
 * ResourceOwnerCreate
 *		Create an empty ResourceOwner.
 *
 * All ResourceOwner objects are kept in TopMemoryContext, since they should
 * only be freed explicitly.
 */
ResourceOwner
ResourceOwnerCreate(ResourceOwner parent, const char *name)
{
























}

/*
 * ResourceOwnerRelease
 *		Release all resources owned by a ResourceOwner and its
 *descendants, but don't delete the owner objects themselves.
 *
 * Note that this executes just one phase of release, and so typically
 * must be called three times.  We do it this way because (a) we want to
 * do all the recursion separately for each phase, thereby preserving
 * the needed order of operations; and (b) xact.c may have other operations
 * to do between the phases.
 *
 * phase: release phase to execute
 * isCommit: true for successful completion of a query or transaction,
 *			false for unsuccessful
 * isTopLevel: true if completing a main transaction, else false
 *
 * isCommit is passed because some modules may expect that their resources
 * were all released already if the transaction or portal finished normally.
 * If so it is reasonable to give a warning (NOT an error) should any
 * unreleased resources be present.  When isCommit is false, such warnings
 * are generally inappropriate.
 *
 * isTopLevel is passed when we are releasing TopTransactionResourceOwner
 * at completion of a main transaction.  This generally means that *all*
 * resources will be released, and so we can optimize things a bit.
 */
void
ResourceOwnerRelease(ResourceOwner owner, ResourceReleasePhase phase, bool isCommit, bool isTopLevel)
{


}

static void
ResourceOwnerReleaseInternal(ResourceOwner owner, ResourceReleasePhase phase, bool isCommit, bool isTopLevel)
{




















































































































































































































}

/*
 * ResourceOwnerDelete
 *		Delete an owner object and its descendants.
 *
 * The caller must have already released all resources in the object tree.
 */
void
ResourceOwnerDelete(ResourceOwner owner)
{













































}

/*
 * Fetch parent of a ResourceOwner (returns NULL if top-level owner)
 */
ResourceOwner
ResourceOwnerGetParent(ResourceOwner owner)
{

}

/*
 * Reassign a ResourceOwner to have a new parent
 */
void
ResourceOwnerNewParent(ResourceOwner owner, ResourceOwner newparent)
{



































}

/*
 * Register or deregister callback functions for resource cleanup
 *
 * These functions are intended for use by dynamically loaded modules.
 * For built-in modules we generally just hardwire the appropriate calls.
 *
 * Note that the callback occurs post-commit or post-abort, so the callback
 * functions can only do noncritical cleanup.
 */
void
RegisterResourceReleaseCallback(ResourceReleaseCallback callback, void *arg)
{







}

void
UnregisterResourceReleaseCallback(ResourceReleaseCallback callback, void *arg)
{




















}

/*
 * Establish an AuxProcessResourceOwner for the current process.
 */
void
CreateAuxProcessResourceOwner(void)
{










}

/*
 * Convenience routine to release all resources tracked in
 * AuxProcessResourceOwner (but that resowner is not destroyed here).
 * Warn about leaked resources if isCommit is true.
 */
void
ReleaseAuxProcessResources(bool isCommit)
{







}

/*
 * Shmem-exit callback for the same.
 * Warn about leaked resources if process exit code is zero (ie normal).
 */
static void
ReleaseAuxProcessResourcesCallback(int code, Datum arg)
{



}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * buffer array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeBuffers(ResourceOwner owner)
{



}

/*
 * Remember that a buffer pin is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeBuffers()
 */
void
ResourceOwnerRememberBuffer(ResourceOwner owner, Buffer buffer)
{

}

/*
 * Forget that a buffer pin is owned by a ResourceOwner
 */
void
ResourceOwnerForgetBuffer(ResourceOwner owner, Buffer buffer)
{




}

/*
 * Remember that a Local Lock is owned by a ResourceOwner
 *
 * This is different from the other Remember functions in that the list of
 * locks is only a lossy cache. It can hold up to MAX_RESOWNER_LOCKS entries,
 * and when it overflows, we stop tracking locks. The point of only remembering
 * only up to MAX_RESOWNER_LOCKS entries is that if a lot of locks are held,
 * ResourceOwnerForgetLock doesn't need to scan through a large array to find
 * the entry.
 */
void
ResourceOwnerRememberLock(ResourceOwner owner, LOCALLOCK *locallock)
{
















}

/*
 * Forget that a Local Lock is owned by a ResourceOwner
 */
void
ResourceOwnerForgetLock(ResourceOwner owner, LOCALLOCK *locallock)
{


















}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * catcache reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeCatCacheRefs(ResourceOwner owner)
{

}

/*
 * Remember that a catcache reference is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeCatCacheRefs()
 */
void
ResourceOwnerRememberCatCacheRef(ResourceOwner owner, HeapTuple tuple)
{

}

/*
 * Forget that a catcache reference is owned by a ResourceOwner
 */
void
ResourceOwnerForgetCatCacheRef(ResourceOwner owner, HeapTuple tuple)
{




}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * catcache-list reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeCatCacheListRefs(ResourceOwner owner)
{

}

/*
 * Remember that a catcache-list reference is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeCatCacheListRefs()
 */
void
ResourceOwnerRememberCatCacheListRef(ResourceOwner owner, CatCList *list)
{

}

/*
 * Forget that a catcache-list reference is owned by a ResourceOwner
 */
void
ResourceOwnerForgetCatCacheListRef(ResourceOwner owner, CatCList *list)
{




}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * relcache reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeRelationRefs(ResourceOwner owner)
{

}

/*
 * Remember that a relcache reference is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeRelationRefs()
 */
void
ResourceOwnerRememberRelationRef(ResourceOwner owner, Relation rel)
{

}

/*
 * Forget that a relcache reference is owned by a ResourceOwner
 */
void
ResourceOwnerForgetRelationRef(ResourceOwner owner, Relation rel)
{




}

/*
 * Debugging subroutine
 */
static void
PrintRelCacheLeakWarning(Relation rel)
{

}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * plancache reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargePlanCacheRefs(ResourceOwner owner)
{

}

/*
 * Remember that a plancache reference is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargePlanCacheRefs()
 */
void
ResourceOwnerRememberPlanCacheRef(ResourceOwner owner, CachedPlan *plan)
{

}

/*
 * Forget that a plancache reference is owned by a ResourceOwner
 */
void
ResourceOwnerForgetPlanCacheRef(ResourceOwner owner, CachedPlan *plan)
{




}

/*
 * Debugging subroutine
 */
static void
PrintPlanCacheLeakWarning(CachedPlan *plan)
{

}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * tupdesc reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeTupleDescs(ResourceOwner owner)
{

}

/*
 * Remember that a tupdesc reference is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeTupleDescs()
 */
void
ResourceOwnerRememberTupleDesc(ResourceOwner owner, TupleDesc tupdesc)
{

}

/*
 * Forget that a tupdesc reference is owned by a ResourceOwner
 */
void
ResourceOwnerForgetTupleDesc(ResourceOwner owner, TupleDesc tupdesc)
{




}

/*
 * Debugging subroutine
 */
static void
PrintTupleDescLeakWarning(TupleDesc tupdesc)
{

}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * snapshot reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeSnapshots(ResourceOwner owner)
{

}

/*
 * Remember that a snapshot reference is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeSnapshots()
 */
void
ResourceOwnerRememberSnapshot(ResourceOwner owner, Snapshot snapshot)
{

}

/*
 * Forget that a snapshot reference is owned by a ResourceOwner
 */
void
ResourceOwnerForgetSnapshot(ResourceOwner owner, Snapshot snapshot)
{




}

/*
 * Debugging subroutine
 */
static void
PrintSnapshotLeakWarning(Snapshot snapshot)
{

}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * files reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeFiles(ResourceOwner owner)
{

}

/*
 * Remember that a temporary file is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeFiles()
 */
void
ResourceOwnerRememberFile(ResourceOwner owner, File file)
{

}

/*
 * Forget that a temporary file is owned by a ResourceOwner
 */
void
ResourceOwnerForgetFile(ResourceOwner owner, File file)
{




}

/*
 * Debugging subroutine
 */
static void
PrintFileLeakWarning(File file)
{

}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * dynamic shmem segment reference array.
 *
 * This is separate from actually inserting an entry because if we run out
 * of memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeDSMs(ResourceOwner owner)
{

}

/*
 * Remember that a dynamic shmem segment is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeDSMs()
 */
void
ResourceOwnerRememberDSM(ResourceOwner owner, dsm_segment *seg)
{

}

/*
 * Forget that a dynamic shmem segment is owned by a ResourceOwner
 */
void
ResourceOwnerForgetDSM(ResourceOwner owner, dsm_segment *seg)
{




}

/*
 * Debugging subroutine
 */
static void
PrintDSMLeakWarning(dsm_segment *seg)
{

}

/*
 * Make sure there is room for at least one more entry in a ResourceOwner's
 * JIT context reference array.
 *
 * This is separate from actually inserting an entry because if we run out of
 * memory, it's critical to do so *before* acquiring the resource.
 */
void
ResourceOwnerEnlargeJIT(ResourceOwner owner)
{

}

/*
 * Remember that a JIT context is owned by a ResourceOwner
 *
 * Caller must have previously done ResourceOwnerEnlargeJIT()
 */
void
ResourceOwnerRememberJIT(ResourceOwner owner, Datum handle)
{

}

/*
 * Forget that a JIT context is owned by a ResourceOwner
 */
void
ResourceOwnerForgetJIT(ResourceOwner owner, Datum handle)
{




}