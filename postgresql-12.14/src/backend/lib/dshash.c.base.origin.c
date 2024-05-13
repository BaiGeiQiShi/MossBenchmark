/*-------------------------------------------------------------------------
 *
 * dshash.c
 *	  Concurrent hash tables backed by dynamic shared memory areas.
 *
 * This is an open hashing hash table, with a linked list at each table
 * entry.  It supports dynamic resizing, as required to prevent the linked
 * lists from growing too long on average.  Currently, only growing is
 * supported: the hash table never becomes smaller.
 *
 * To deal with concurrency, it has a fixed size set of partitions, each of
 * which is independently locked.  Each bucket maps to a partition; so insert,
 * find and iterate operations normally only acquire one lock.  Therefore,
 * good concurrency is achieved whenever such operations don't collide at the
 * lock partition level.  However, when a resize operation begins, all
 * partition locks must be acquired simultaneously for a brief period.  This
 * is only expected to happen a small number of times until a stable size is
 * found, since growth is geometric.
 *
 * Future versions may support iterators and incremental resizing; for now
 * the implementation is minimalist.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/lib/dshash.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "lib/dshash.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "utils/dsa.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

/*
 * An item in the hash table.  This wraps the user's entry object in an
 * envelop that holds a pointer back to the bucket and a pointer to the next
 * item in the bucket.
 */
struct dshash_table_item
{
  /* The next item in the same bucket. */
  dsa_pointer next;
  /* The hashed key, to avoid having to recompute it. */
  dshash_hash hash;
  /* The user's entry object follows here.  See ENTRY_FROM_ITEM(item). */
};

/*
 * The number of partitions for locking purposes.  This is set to match
 * NUM_BUFFER_PARTITIONS for now, on the basis that whatever's good enough for
 * the buffer pool must be good enough for any other purpose.  This could
 * become a runtime parameter in future.
 */
#define DSHASH_NUM_PARTITIONS_LOG2 7
#define DSHASH_NUM_PARTITIONS (1 << DSHASH_NUM_PARTITIONS_LOG2)

/* A magic value used to identify our hash tables. */
#define DSHASH_MAGIC 0x75ff6a20

/*
 * Tracking information for each lock partition.  Initially, each partition
 * corresponds to one bucket, but each time the hash table grows, the buckets
 * covered by each partition split so the number of buckets covered doubles.
 *
 * We might want to add padding here so that each partition is on a different
 * cache line, but doing so would bloat this structure considerably.
 */
typedef struct dshash_partition
{
  LWLock lock;  /* Protects all buckets in this partition. */
  size_t count; /* # of items in this partition's buckets */
} dshash_partition;

/*
 * The head object for a hash table.  This will be stored in dynamic shared
 * memory.
 */
typedef struct dshash_table_control
{
  dshash_table_handle handle;
  uint32 magic;
  dshash_partition partitions[DSHASH_NUM_PARTITIONS];
  int lwlock_tranche_id;

  /*
   * The following members are written to only when ALL partitions locks are
   * held.  They can be read when any one partition lock is held.
   */

  /* Number of buckets expressed as power of 2 (8 = 256 buckets). */
  size_t size_log2;    /* log2(number of buckets) */
  dsa_pointer buckets; /* current bucket array */
} dshash_table_control;

/*
 * Per-backend state for a dynamic hash table.
 */
struct dshash_table
{
  dsa_area *area;                /* Backing dynamic shared memory area. */
  dshash_parameters params;      /* Parameters. */
  void *arg;                     /* User-supplied data pointer. */
  dshash_table_control *control; /* Control object in DSM. */
  dsa_pointer *buckets;          /* Current bucket pointers in DSM. */
  size_t size_log2;              /* log2(number of buckets) */
};

/* Given a pointer to an item, find the entry (user data) it holds. */
#define ENTRY_FROM_ITEM(item) ((char *)(item) + MAXALIGN(sizeof(dshash_table_item)))

/* Given a pointer to an entry, find the item that holds it. */
#define ITEM_FROM_ENTRY(entry) ((dshash_table_item *)((char *)(entry)-MAXALIGN(sizeof(dshash_table_item))))

/* How many resize operations (bucket splits) have there been? */
#define NUM_SPLITS(size_log2) (size_log2 - DSHASH_NUM_PARTITIONS_LOG2)

/* How many buckets are there in each partition at a given size? */
#define BUCKETS_PER_PARTITION(size_log2) (((size_t)1) << NUM_SPLITS(size_log2))

/* Max entries before we need to grow.  Half + quarter = 75% load factor. */
#define MAX_COUNT_PER_PARTITION(hash_table) (BUCKETS_PER_PARTITION(hash_table->size_log2) / 2 + BUCKETS_PER_PARTITION(hash_table->size_log2) / 4)

/* Choose partition based on the highest order bits of the hash. */
#define PARTITION_FOR_HASH(hash) (hash >> ((sizeof(dshash_hash) * CHAR_BIT) - DSHASH_NUM_PARTITIONS_LOG2))

/*
 * Find the bucket index for a given hash and table size.  Each time the table
 * doubles in size, the appropriate bucket for a given hash value doubles and
 * possibly adds one, depending on the newly revealed bit, so that all buckets
 * are split.
 */
#define BUCKET_INDEX_FOR_HASH_AND_SIZE(hash, size_log2) (hash >> ((sizeof(dshash_hash) * CHAR_BIT) - (size_log2)))

/* The index of the first bucket in a given partition. */
#define BUCKET_INDEX_FOR_PARTITION(partition, size_log2) ((partition) << NUM_SPLITS(size_log2))

/* The head of the active bucket for a given hash value (lvalue). */
#define BUCKET_FOR_HASH(hash_table, hash) (hash_table->buckets[BUCKET_INDEX_FOR_HASH_AND_SIZE(hash, hash_table->size_log2)])

static void
delete_item(dshash_table *hash_table, dshash_table_item *item);
static void
resize(dshash_table *hash_table, size_t new_size);
static inline void
ensure_valid_bucket_pointers(dshash_table *hash_table);
static inline dshash_table_item *
find_in_bucket(dshash_table *hash_table, const void *key, dsa_pointer item_pointer);
static void
insert_item_into_bucket(dshash_table *hash_table, dsa_pointer item_pointer, dshash_table_item *item, dsa_pointer *bucket);
static dshash_table_item *
insert_into_bucket(dshash_table *hash_table, const void *key, dsa_pointer *bucket);
static bool
delete_key_from_bucket(dshash_table *hash_table, const void *key, dsa_pointer *bucket_head);
static bool
delete_item_from_bucket(dshash_table *hash_table, dshash_table_item *item, dsa_pointer *bucket_head);
static inline dshash_hash
hash_key(dshash_table *hash_table, const void *key);
static inline bool
equal_keys(dshash_table *hash_table, const void *a, const void *b);

#define PARTITION_LOCK(hash_table, i) (&(hash_table)->control->partitions[(i)].lock)

#define ASSERT_NO_PARTITION_LOCKS_HELD_BY_ME(hash_table) Assert(!LWLockAnyHeldByMe(&(hash_table)->control->partitions[0].lock, DSHASH_NUM_PARTITIONS, sizeof(dshash_partition)))

/*
 * Create a new hash table backed by the given dynamic shared area, with the
 * given parameters.  The returned object is allocated in backend-local memory
 * using the current MemoryContext.  'arg' will be passed through to the
 * compare and hash functions.
 */
dshash_table *
dshash_create(dsa_area *area, const dshash_parameters *params, void *arg)
{














































}

/*
 * Attach to an existing hash table using a handle.  The returned object is
 * allocated in backend-local memory using the current MemoryContext.  'arg'
 * will be passed through to the compare and hash functions.
 */
dshash_table *
dshash_attach(dsa_area *area, const dshash_parameters *params, dshash_table_handle handle, void *arg)
{

























}

/*
 * Detach from a hash table.  This frees backend-local resources associated
 * with the hash table, but the hash table will continue to exist until it is
 * either explicitly destroyed (by a backend that is still attached to it), or
 * the area that backs it is returned to the operating system.
 */
void
dshash_detach(dshash_table *hash_table)
{




}

/*
 * Destroy a hash table, returning all memory to the area.  The caller must be
 * certain that no other backend will attempt to access the hash table before
 * calling this function.  Other backend must explicitly call dshash_detach to
 * free up backend-local memory associated with the hash table.  The backend
 * that calls dshash_destroy must not call dshash_detach.
 */
void
dshash_destroy(dshash_table *hash_table)
{



































}

/*
 * Get a handle that can be used by other processes to attach to this hash
 * table.
 */
dshash_table_handle
dshash_get_hash_table_handle(dshash_table *hash_table)
{



}

/*
 * Look up an entry, given a key.  Returns a pointer to an entry if one can be
 * found with the given key.  Returns NULL if the key is not found.  If a
 * non-NULL value is returned, the entry is locked and must be released by
 * calling dshash_release_lock.  If an error is raised before
 * dshash_release_lock is called, the lock will be released automatically, but
 * the caller must take care to ensure that the entry is not left corrupted.
 * The lock mode is either shared or exclusive depending on 'exclusive'.
 *
 * The caller must not hold a lock already.
 *
 * Note that the lock held is in fact an LWLock, so interrupts will be held on
 * return from this function, and not resumed until dshash_release_lock is
 * called.  It is a very good idea for the caller to release the lock quickly.
 */
void *
dshash_find(dshash_table *hash_table, const void *key, bool exclusive)
{



























}

/*
 * Returns a pointer to an exclusively locked item which must be released with
 * dshash_release_lock.  If the key is found in the hash table, 'found' is set
 * to true and a pointer to the existing entry is returned.  If the key is not
 * found, 'found' is set to false, and a pointer to a newly created entry is
 * returned.
 *
 * Notes above dshash_find() regarding locking and error handling equally
 * apply here.
 */
void *
dshash_find_or_insert(dshash_table *hash_table, const void *key, bool *found)
{
























































}

/*
 * Remove an entry by key.  Returns true if the key was found and the
 * corresponding entry was removed.
 *
 * To delete an entry that you already have a pointer to, see
 * dshash_delete_entry.
 */
bool
dshash_delete_key(dshash_table *hash_table, const void *key)
{



























}

/*
 * Remove an entry.  The entry must already be exclusively locked, and must
 * have been obtained by dshash_find or dshash_find_or_insert.  Note that this
 * function releases the lock just like dshash_release_lock.
 *
 * To delete an entry by key, see dshash_delete_key.
 */
void
dshash_delete_entry(dshash_table *hash_table, void *entry)
{








}

/*
 * Unlock an entry which was locked by dshash_find or dshash_find_or_insert.
 */
void
dshash_release_lock(dshash_table *hash_table, void *entry)
{






}

/*
 * A compare function that forwards to memcmp.
 */
int
dshash_memcmp(const void *a, const void *b, size_t size, void *arg)
{

}

/*
 * A hash function that forwards to tag_hash.
 */
dshash_hash
dshash_memhash(const void *v, size_t size, void *arg)
{

}

/*
 * Print debugging information about the internal state of the hash table to
 * stderr.  The caller must hold no partition locks.
 */
void
dshash_dump(dshash_table *hash_table)
{














































}

/*
 * Delete a locked item to which we have a pointer.
 */
static void
delete_item(dshash_table *hash_table, dshash_table_item *item)
{














}

/*
 * Grow the hash table if necessary to the requested number of buckets.  The
 * requested size must be double some previously observed size.
 *
 * Must be called without any partition lock held.
 */
static void
resize(dshash_table *hash_table, size_t new_size_log2)
{


































































}

/*
 * Make sure that our backend-local bucket pointers are up to date.  The
 * caller must have locked one lock partition, which prevents resize() from
 * running concurrently.
 */
static inline void
ensure_valid_bucket_pointers(dshash_table *hash_table)
{





}

/*
 * Scan a locked bucket for a match, using the provided compare function.
 */
static inline dshash_table_item *
find_in_bucket(dshash_table *hash_table, const void *key, dsa_pointer item_pointer)
{












}

/*
 * Insert an already-allocated item into a bucket.
 */
static void
insert_item_into_bucket(dshash_table *hash_table, dsa_pointer item_pointer, dshash_table_item *item, dsa_pointer *bucket)
{




}

/*
 * Allocate space for an entry with the given key and insert it into the
 * provided bucket.
 */
static dshash_table_item *
insert_into_bucket(dshash_table *hash_table, const void *key, dsa_pointer *bucket)
{








}

/*
 * Search a bucket for a matching key and delete it.
 */
static bool
delete_key_from_bucket(dshash_table *hash_table, const void *key, dsa_pointer *bucket_head)
{



















}

/*
 * Delete the specified item from the bucket.
 */
static bool
delete_item_from_bucket(dshash_table *hash_table, dshash_table_item *item, dsa_pointer *bucket_head)
{


















}

/*
 * Compute the hash value for a key.
 */
static inline dshash_hash
hash_key(dshash_table *hash_table, const void *key)
{

}

/*
 * Check whether two keys compare equal.
 */
static inline bool
equal_keys(dshash_table *hash_table, const void *a, const void *b)
{

}