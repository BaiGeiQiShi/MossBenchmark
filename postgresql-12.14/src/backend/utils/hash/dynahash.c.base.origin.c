/*-------------------------------------------------------------------------
 *
 * dynahash.c
 *	  dynamic hash tables
 *
 * dynahash.c supports both local-to-a-backend hash tables and hash tables in
 * shared memory.  For shared hash tables, it is the caller's responsibility
 * to provide appropriate access interlocking.  The simplest convention is
 * that a single LWLock protects the whole hash table.  Searches (HASH_FIND or
 * hash_seq_search) need only shared lock, but any update requires exclusive
 * lock.  For heavily-used shared tables, the single-lock approach creates a
 * concurrency bottleneck, so we also support "partitioned" locking wherein
 * there are multiple LWLocks guarding distinct subsets of the table.  To use
 * a hash table in partitioned mode, the HASH_PARTITION flag must be given
 * to hash_create.  This prevents any attempt to split buckets on-the-fly.
 * Therefore, each hash bucket chain operates independently, and no fields
 * of the hash header change after init except nentries and freeList.
 * (A partitioned table uses multiple copies of those fields, guarded by
 * spinlocks, for additional concurrency.)
 * This lets any subset of the hash buckets be treated as a separately
 * lockable partition.  We expect callers to use the low-order bits of a
 * lookup key's hash value as a partition number --- this will work because
 * of the way calc_bucket() maps hash values to bucket numbers.
 *
 * For hash tables in shared memory, the memory allocator function should
 * match malloc's semantics of returning NULL on failure.  For hash tables
 * in local memory, we typically use palloc() which will throw error on
 * failure.  The code in this file has to cope with both cases.
 *
 * dynahash.c provides support for these types of lookup keys:
 *
 * 1. Null-terminated C strings (truncated if necessary to fit in keysize),
 * compared as though by strcmp().  This is the default behavior.
 *
 * 2. Arbitrary binary data of size keysize, compared as though by memcmp().
 * (Caller must ensure there are no undefined padding bits in the keys!)
 * This is selected by specifying HASH_BLOBS flag to hash_create.
 *
 * 3. More complex key behavior can be selected by specifying user-supplied
 * hashing, comparison, and/or key-copying functions.  At least a hashing
 * function must be supplied; comparison defaults to memcmp() and key copying
 * to memcpy() when a user-defined hashing function is selected.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/hash/dynahash.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * Original comments:
 *
 * Dynamic hashing, after CACM April 1988 pp 446-457, by Per-Ake Larson.
 * Coded into C, with minor code improvements, and with hsearch(3) interface,
 * by ejp@ausmelb.oz, Jul 26, 1988: 13:16;
 * also, hcreate/hdestroy routines added to simulate hsearch(3).
 *
 * These routines simulate hsearch(3) and family, with the important
 * difference that the hash table is dynamic - can grow indefinitely
 * beyond its original size (as supplied to hcreate()).
 *
 * Performance appears to be comparable to that of hsearch(3).
 * The 'source-code' options referred to in hsearch(3)'s 'man' page
 * are not implemented; otherwise functionality is identical.
 *
 * Compilation controls:
 * HASH_DEBUG controls some informative traces, mainly for debugging.
 * HASH_STATISTICS causes HashAccesses and HashCollisions to be maintained;
 * when combined with HASH_DEBUG, these are displayed by hdestroy().
 *
 * Problems & fixes to ejp@ausmelb.oz. WARNING: relies on pre-processor
 * concatenation property, in probably unnecessary code 'optimization'.
 *
 * Modified margo@postgres.berkeley.edu February 1990
 *		added multiple table interface
 * Modified by sullivan@postgres.berkeley.edu April 1990
 *		changed ctl structure for shared memory
 */

#include "postgres.h"

#include <limits.h>

#include "access/xact.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/dynahash.h"
#include "utils/memutils.h"

/*
 * Constants
 *
 * A hash table has a top-level "directory", each of whose entries points
 * to a "segment" of ssize bucket headers.  The maximum number of hash
 * buckets is thus dsize * ssize (but dsize may be expansible).  Of course,
 * the number of records in the table can be larger, but we don't want a
 * whole lot of records per bucket or performance goes down.
 *
 * In a hash table allocated in shared memory, the directory cannot be
 * expanded because it must stay at a fixed address.  The directory size
 * should be selected using hash_select_dirsize (and you'd better have
 * a good idea of the maximum number of entries!).  For non-shared hash
 * tables, the initial directory size can be left at the default.
 */
#define DEF_SEGSIZE 256
#define DEF_SEGSIZE_SHIFT 8 /* must be log2(DEF_SEGSIZE) */
#define DEF_DIRSIZE 256
#define DEF_FFACTOR 1 /* default fill factor */

/* Number of freelists to be used for a partitioned hash table. */
#define NUM_FREELISTS 32

/* A hash bucket is a linked list of HASHELEMENTs */
typedef HASHELEMENT *HASHBUCKET;

/* A hash segment is an array of bucket headers */
typedef HASHBUCKET *HASHSEGMENT;

/*
 * Per-freelist data.
 *
 * In a partitioned hash table, each freelist is associated with a specific
 * set of hashcodes, as determined by the FREELIST_IDX() macro below.
 * nentries tracks the number of live hashtable entries having those hashcodes
 * (NOT the number of entries in the freelist, as you might expect).
 *
 * The coverage of a freelist might be more or less than one partition, so it
 * needs its own lock rather than relying on caller locking.  Relying on that
 * wouldn't work even if the coverage was the same, because of the occasional
 * need to "borrow" entries from another freelist; see get_hash_entry().
 *
 * Using an array of FreeListData instead of separate arrays of mutexes,
 * nentries and freeLists helps to reduce sharing of cache lines between
 * different mutexes.
 */
typedef struct
{
  slock_t mutex;         /* spinlock for this freelist */
  long nentries;         /* number of entries in associated buckets */
  HASHELEMENT *freeList; /* chain of free elements */
} FreeListData;

/*
 * Header structure for a hash table --- contains all changeable info
 *
 * In a shared-memory hash table, the HASHHDR is in shared memory, while
 * each backend has a local HTAB struct.  For a non-shared table, there isn't
 * any functional difference between HASHHDR and HTAB, but we separate them
 * anyway to share code between shared and non-shared tables.
 */
struct HASHHDR
{
  /*
   * The freelist can become a point of contention in high-concurrency hash
   * tables, so we use an array of freelists, each with its own mutex and
   * nentries count, instead of just a single one.  Although the freelists
   * normally operate independently, we will scavenge entries from freelists
   * other than a hashcode's default freelist when necessary.
   *
   * If the hash table is not partitioned, only freeList[0] is used and its
   * spinlock is not used at all; callers' locking is assumed sufficient.
   */
  FreeListData freeList[NUM_FREELISTS];

  /* These fields can change, but not in a partitioned table */
  /* Also, dsize can't change in a shared table, even if unpartitioned */
  long dsize;        /* directory size */
  long nsegs;        /* number of allocated segments (<= dsize) */
  uint32 max_bucket; /* ID of maximum bucket in use */
  uint32 high_mask;  /* mask to modulo into entire table */
  uint32 low_mask;   /* mask to modulo into lower half of table */

  /* These fields are fixed at hashtable creation */
  Size keysize;        /* hash key length in bytes */
  Size entrysize;      /* total user element size in bytes */
  long num_partitions; /* # partitions (must be power of 2), or 0 */
  long ffactor;        /* target fill factor */
  long max_dsize;      /* 'dsize' limit if directory is fixed size */
  long ssize;          /* segment size --- must be power of 2 */
  int sshift;          /* segment shift = log2(ssize) */
  int nelem_alloc;     /* number of entries to allocate at once */

#ifdef HASH_STATISTICS

  /*
   * Count statistics here.  NB: stats code doesn't bother with mutex, so
   * counts could be corrupted a bit in a partitioned table.
   */
  long accesses;
  long collisions;
#endif
};

#define IS_PARTITIONED(hctl) ((hctl)->num_partitions != 0)

#define FREELIST_IDX(hctl, hashcode) (IS_PARTITIONED(hctl) ? (hashcode) % NUM_FREELISTS : 0)

/*
 * Top control structure for a hashtable --- in a shared table, each backend
 * has its own copy (OK since no fields change at runtime)
 */
struct HTAB
{
  HASHHDR *hctl;         /* => shared control information */
  HASHSEGMENT *dir;      /* directory of segment starts */
  HashValueFunc hash;    /* hash function */
  HashCompareFunc match; /* key comparison function */
  HashCopyFunc keycopy;  /* key copying function */
  HashAllocFunc alloc;   /* memory allocator */
  MemoryContext hcxt;    /* memory context if default allocator used */
  char *tabname;         /* table name (for error messages) */
  bool isshared;         /* true if table is in shared memory */
  bool isfixed;          /* if true, don't enlarge */

  /* freezing a shared table isn't allowed, so we can keep state here */
  bool frozen; /* true = no more inserts allowed */

  /* We keep local copies of these fixed values to reduce contention */
  Size keysize; /* hash key length in bytes */
  long ssize;   /* segment size --- must be power of 2 */
  int sshift;   /* segment shift = log2(ssize) */
};

/*
 * Key (also entry) part of a HASHELEMENT
 */
#define ELEMENTKEY(helem) (((char *)(helem)) + MAXALIGN(sizeof(HASHELEMENT)))

/*
 * Obtain element pointer given pointer to key
 */
#define ELEMENT_FROM_KEY(key) ((HASHELEMENT *)(((char *)(key)) - MAXALIGN(sizeof(HASHELEMENT))))

/*
 * Fast MOD arithmetic, assuming that y is a power of 2 !
 */
#define MOD(x, y) ((x) & ((y)-1))

#if HASH_STATISTICS
static long hash_accesses, hash_collisions, hash_expansions;
#endif

/*
 * Private function prototypes
 */
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

/*
 * memory allocation support
 */
static MemoryContext CurrentDynaHashCxt = NULL;

static void *
DynaHashAlloc(Size size)
{


}

/*
 * HashCompareFunc for string keys
 *
 * Because we copy keys with strlcpy(), they will be truncated at keysize-1
 * bytes, so we can only compare that many ... hence strncmp is almost but
 * not quite the right thing.
 */
static int
string_compare(const char *key1, const char *key2, Size keysize)
{

}

/************************** CREATE ROUTINES **********************/

/*
 * hash_create -- create a new dynamic hash table
 *
 *	tabname: a name for the table (for debugging purposes)
 *	nelem: maximum number of elements expected
 *	*info: additional table parameters, as indicated by flags
 *	flags: bitmask indicating which parameters to take from *info
 *
 * Note: for a shared-memory hashtable, nelem needs to be a pretty good
 * estimate, since we can't expand the table on the fly.  But an unshared
 * hashtable can be expanded on-the-fly, so it's better for nelem to be
 * on the small side and let the table grow if it's exceeded.  An overly
 * large nelem will penalize hash_seq_search speed without buying much.
 */
HTAB *
hash_create(const char *tabname, long nelem, HASHCTL *info, int flags)
{





























































































































































































































































































}

/*
 * Set default HASHHDR parameters.
 */
static void
hdefault(HTAB *hashp)
{
























}

/*
 * Given the user-specified entry size, choose nelem_alloc, ie, how many
 * elements to add to the hash table when we need more.
 */
static int
choose_nelem_alloc(Size entrysize)
{
























}

/*
 * Compute derived fields of hctl and build the initial directory/segment
 * arrays
 */
static bool
init_htab(HTAB *hashp, long nelem)
{
























































































}

/*
 * Estimate the space needed for a hashtable containing the given number
 * of entries of given size.
 * NOTE: this is used to estimate the footprint of hashtables in shared
 * memory; therefore it does not count HTAB which is in local memory.
 * NB: assumes that all hash structure parameters have default values!
 */
Size
hash_estimate_size(long num_entries, Size entrysize)
{



























}

/*
 * Select an appropriate directory size for a hashtable with the given
 * maximum number of entries.
 * This is only needed for hashtables in shared memory, whose directories
 * cannot be expanded dynamically.
 * NB: assumes that all hash structure parameters have default values!
 *
 * XXX this had better agree with the behavior of init_htab()...
 */
long
hash_select_dirsize(long num_entries)
{














}

/*
 * Compute the required initial memory allocation for a shared-memory
 * hashtable with the given parameters.  We need space for the HASHHDR
 * and for the (non expansible) directory.
 */
Size
hash_get_shared_size(HASHCTL *info, int flags)
{



}

/********************** DESTROY ROUTINES ************************/

void
hash_destroy(HTAB *hashp)
{














}

void
hash_stats(const char *where, HTAB *hashp)
{







}

/*******************************SEARCH ROUTINES *****************************/

/*
 * get_hash_value -- exported routine to calculate a key's hash value
 *
 * We export this because for partitioned tables, callers need to compute
 * the partition number (from the low-order bits of the hash value) before
 * searching.
 */
uint32
get_hash_value(HTAB *hashp, const void *keyPtr)
{

}

/* Convert a hash value to a bucket number */
static inline uint32
calc_bucket(HASHHDR *hctl, uint32 hash_val)
{









}

/*
 * hash_search -- look up key in table and perform action
 * hash_search_with_hash_value -- same, with key's hash value already computed
 *
 * action is one of:
 *		HASH_FIND: look up key in table
 *		HASH_ENTER: look up key in table, creating entry if not present
 *		HASH_ENTER_NULL: same, but return NULL if out of memory
 *		HASH_REMOVE: look up key in table, remove entry if present
 *
 * Return value is a pointer to the element found/entered/removed if any,
 * or NULL if no match was found.  (NB: in the case of the REMOVE action,
 * the result is a dangling pointer that shouldn't be dereferenced!)
 *
 * HASH_ENTER will normally ereport a generic "out of memory" error if
 * it is unable to create a new entry.  The HASH_ENTER_NULL operation is
 * the same except it will return NULL if out of memory.  Note that
 * HASH_ENTER_NULL cannot be used with the default palloc-based allocator,
 * since palloc internally ereports on out-of-memory.
 *
 * If foundPtr isn't NULL, then *foundPtr is set true if we found an
 * existing entry in the table, false otherwise.  This is needed in the
 * HASH_ENTER case, but is redundant with the return value otherwise.
 *
 * For hash_search_with_hash_value, the hashvalue parameter must have been
 * calculated with get_hash_value().
 */
void *
hash_search(HTAB *hashp, const void *keyPtr, HASHACTION action, bool *foundPtr)
{

}

void *
hash_search_with_hash_value(HTAB *hashp, const void *keyPtr, uint32 hashvalue, HASHACTION action, bool *foundPtr)
{
























































































































































































}

/*
 * hash_update_hash_key -- change the hash key of an existing table entry
 *
 * This is equivalent to removing the entry, making a new entry, and copying
 * over its data, except that the entry never goes to the table's freelist.
 * Therefore this cannot suffer an out-of-memory failure, even if there are
 * other processes operating in other partitions of the hashtable.
 *
 * Returns true if successful, false if the requested new hash key is already
 * present.  Throws error if the specified entry pointer isn't actually a
 * table member.
 *
 * NB: currently, there is no special case for old and new hash keys being
 * identical, which means we'll report false for that situation.  This is
 * preferable for existing uses.
 *
 * NB: for a partitioned hashtable, caller must hold lock on both relevant
 * partitions, if the new hash key would belong to a different partition.
 */
bool
hash_update_hash_key(HTAB *hashp, void *existingEntry, const void *newKeyPtr)
{






































































































































}

/*
 * Allocate a new hashtable entry if possible; return NULL if out of memory.
 * (Or, if the underlying space allocator throws error for out-of-memory,
 * we won't return at all.)
 */
static HASHBUCKET
get_hash_entry(HTAB *hashp, int freelist_idx)
{

























































































}

/*
 * hash_get_num_entries -- get the number of entries in a hashtable
 */
long
hash_get_num_entries(HTAB *hashp)
{

















}

/*
 * hash_seq_init/_search/_term
 *			Sequentially search through hash table and return
 *			all the elements one by one, return NULL when no more.
 *
 * hash_seq_term should be called if and only if the scan is abandoned before
 * completion; if hash_seq_search returns NULL then it has already done the
 * end-of-scan cleanup.
 *
 * NOTE: caller may delete the returned element before continuing the scan.
 * However, deleting any other element while the scan is in progress is
 * UNDEFINED (it might be the one that curIndex is pointing at!).  Also,
 * if elements are added to the table while the scan is in progress, it is
 * unspecified whether they will be visited by the scan or not.
 *
 * NOTE: it is possible to use hash_seq_init/hash_seq_search without any
 * worry about hash_seq_term cleanup, if the hashtable is first locked against
 * further insertions by calling hash_freeze.
 *
 * NOTE: to use this with a partitioned hashtable, caller had better hold
 * at least shared lock on all partitions of the table throughout the scan!
 * We can cope with insertions or deletions by our own backend, but *not*
 * with concurrent insertions or deletions by another.
 */
void
hash_seq_init(HASH_SEQ_STATUS *status, HTAB *hashp)
{







}

void *
hash_seq_search(HASH_SEQ_STATUS *status)
{











































































}

void
hash_seq_term(HASH_SEQ_STATUS *status)
{




}

/*
 * hash_freeze
 *			Freeze a hashtable against future insertions (deletions
 *are still allowed)
 *
 * The reason for doing this is that by preventing any more bucket splits,
 * we no longer need to worry about registering hash_seq_search scans,
 * and thus caller need not be careful about ensuring hash_seq_term gets
 * called at the right times.
 *
 * Multiple calls to hash_freeze() are allowed, but you can't freeze a table
 * with active scans (since hash_seq_term would then do the wrong thing).
 */
void
hash_freeze(HTAB *hashp)
{









}

/********************************* UTILITIES ************************/

/*
 * Expand the table by adding one more hash bucket.
 */
static bool
expand_table(HTAB *hashp)
{

























































































}

static bool
dir_realloc(HTAB *hashp)
{



































}

static HASHSEGMENT
seg_alloc(HTAB *hashp)
{













}

/*
 * allocate some new elements and link them into the indicated free list
 */
static bool
element_alloc(HTAB *hashp, int nelem, int freelist_idx)
{

















































}

/* complain when we have detected a corrupted hashtable */
static void
hash_corrupted(HTAB *hashp)
{












}

/* calculate ceil(log base 2) of num */
int
my_log2(long num)
{












}

/* calculate first power of 2 >= num, bounded to what will fit in a long */
static long
next_pow2_long(long num)
{


}

/* calculate first power of 2 >= num, bounded to what will fit in an int */
static int
next_pow2_int(long num)
{





}

/************************* SEQ SCAN TRACKING ************************/

/*
 * We track active hash_seq_search scans here.  The need for this mechanism
 * comes from the fact that a scan will get confused if a bucket split occurs
 * while it's in progress: it might visit entries twice, or even miss some
 * entirely (if it's partway through the same bucket that splits).  Hence
 * we want to inhibit bucket splits if there are any active scans on the
 * table being inserted into.  This is a fairly rare case in current usage,
 * so just postponing the split until the next insertion seems sufficient.
 *
 * Given present usages of the function, only a few scans are likely to be
 * open concurrently; so a finite-size stack of open scans seems sufficient,
 * and we don't worry that linear search is too slow.  Note that we do
 * allow multiple scans of the same hashtable to be open concurrently.
 *
 * This mechanism can support concurrent scan and insertion in a shared
 * hashtable if it's the same backend doing both.  It would fail otherwise,
 * but locking reasons seem to preclude any such scenario anyway, so we don't
 * worry.
 *
 * This arrangement is reasonably robust if a transient hashtable is deleted
 * without notifying us.  The absolute worst case is we might inhibit splits
 * in another table created later at exactly the same address.  We will give
 * a warning at transaction end for reference leaks, so any bugs leading to
 * lack of notification should be easy to catch.
 */

#define MAX_SEQ_SCANS 100

static HTAB *seq_scan_tables[MAX_SEQ_SCANS]; /* tables being scanned */
static int seq_scan_level[MAX_SEQ_SCANS];    /* subtransaction nest level */
static int num_seq_scans = 0;

/* Register a table as having an active hash_seq_search scan */
static void
register_seq_scan(HTAB *hashp)
{







}

/* Deregister an active scan */
static void
deregister_seq_scan(HTAB *hashp)
{














}

/* Check if a table has any active scan */
static bool
has_seq_scans(HTAB *hashp)
{










}

/* Clean up any open scans at end of transaction */
void
AtEOXact_HashTables(bool isCommit)
{



















}

/* Clean up any open scans at end of subtransaction */
void
AtEOSubXact_HashTables(bool isCommit, int nestDepth)
{




















}