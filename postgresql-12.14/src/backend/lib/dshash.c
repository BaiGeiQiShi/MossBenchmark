                                                                            
   
            
                                                                   
   
                                                                        
                                                                           
                                                                       
                                                    
   
                                                                            
                                                                               
                                                                           
                                                                              
                                                                       
                                                                             
                                                                             
                                     
   
                                                                           
                                     
   
                                                                         
                                                                        
   
                  
                              
   
                                                                            
   

#include "postgres.h"

#include "lib/dshash.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "utils/dsa.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

   
                                                                        
                                                                             
                       
   
struct dshash_table_item
{
                                         
  dsa_pointer next;
                                                        
  dshash_hash hash;
                                                                         
};

   
                                                                        
                                                                               
                                                                          
                                         
   
#define DSHASH_NUM_PARTITIONS_LOG2 7
#define DSHASH_NUM_PARTITIONS (1 << DSHASH_NUM_PARTITIONS_LOG2)

                                                     
#define DSHASH_MAGIC 0x75ff6a20

   
                                                                            
                                                                              
                                                                             
   
                                                                              
                                                                     
   
typedef struct dshash_partition
{
  LWLock lock;                                               
  size_t count;                                             
} dshash_partition;

   
                                                                            
           
   
typedef struct dshash_table_control
{
  dshash_table_handle handle;
  uint32 magic;
  dshash_partition partitions[DSHASH_NUM_PARTITIONS];
  int lwlock_tranche_id;

     
                                                                             
                                                                  
     

                                                                    
  size_t size_log2;                                 
  dsa_pointer buckets;                           
} dshash_table_control;

   
                                               
   
struct dshash_table
{
  dsa_area *area;                                                         
  dshash_parameters params;                       
  void *arg;                                                      
  dshash_table_control *control;                             
  dsa_pointer *buckets;                                               
  size_t size_log2;                                           
};

                                                                      
#define ENTRY_FROM_ITEM(item) ((char *)(item) + MAXALIGN(sizeof(dshash_table_item)))

                                                               
#define ITEM_FROM_ENTRY(entry) ((dshash_table_item *)((char *)(entry)-MAXALIGN(sizeof(dshash_table_item))))

                                                                 
#define NUM_SPLITS(size_log2) (size_log2 - DSHASH_NUM_PARTITIONS_LOG2)

                                                                   
#define BUCKETS_PER_PARTITION(size_log2) (((size_t)1) << NUM_SPLITS(size_log2))

                                                                            
#define MAX_COUNT_PER_PARTITION(hash_table) (BUCKETS_PER_PARTITION(hash_table->size_log2) / 2 + BUCKETS_PER_PARTITION(hash_table->size_log2) / 4)

                                                                   
#define PARTITION_FOR_HASH(hash) (hash >> ((sizeof(dshash_hash) * CHAR_BIT) - DSHASH_NUM_PARTITIONS_LOG2))

   
                                                                               
                                                                              
                                                                               
              
   
#define BUCKET_INDEX_FOR_HASH_AND_SIZE(hash, size_log2) (hash >> ((sizeof(dshash_hash) * CHAR_BIT) - (size_log2)))

                                                         
#define BUCKET_INDEX_FOR_PARTITION(partition, size_log2) ((partition) << NUM_SPLITS(size_log2))

                                                                    
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

   
                                                                             
                                                                               
                                                                         
                               
   
dshash_table *
dshash_create(dsa_area *area, const dshash_parameters *params, void *arg)
{
  dshash_table *hash_table;
  dsa_pointer control;

                                                                      
  hash_table = palloc(sizeof(dshash_table));

                                                     
  control = dsa_allocate(area, sizeof(dshash_table_control));

                                                       
  hash_table->area = area;
  hash_table->params = *params;
  hash_table->arg = arg;
  hash_table->control = dsa_get_address(area, control);
  hash_table->control->handle = control;
  hash_table->control->magic = DSHASH_MAGIC;
  hash_table->control->lwlock_tranche_id = params->tranche_id;

                                            
  {
    dshash_partition *partitions = hash_table->control->partitions;
    int tranche_id = hash_table->control->lwlock_tranche_id;
    int i;

    for (i = 0; i < DSHASH_NUM_PARTITIONS; ++i)
    {
      LWLockInitialize(&partitions[i].lock, tranche_id);
      partitions[i].count = 0;
    }
  }

     
                                                                           
                               
     
  hash_table->control->size_log2 = DSHASH_NUM_PARTITIONS_LOG2;
  hash_table->control->buckets = dsa_allocate_extended(area, sizeof(dsa_pointer) * DSHASH_NUM_PARTITIONS, DSA_ALLOC_NO_OOM | DSA_ALLOC_ZERO);
  if (!DsaPointerIsValid(hash_table->control->buckets))
  {
    dsa_free(area, control);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory"), errdetail("Failed on DSA request of size %zu.", sizeof(dsa_pointer) * DSHASH_NUM_PARTITIONS)));
  }
  hash_table->buckets = dsa_get_address(area, hash_table->control->buckets);
  hash_table->size_log2 = hash_table->control->size_log2;

  return hash_table;
}

   
                                                                            
                                                                             
                                                             
   
dshash_table *
dshash_attach(dsa_area *area, const dshash_parameters *params, dshash_table_handle handle, void *arg)
{
  dshash_table *hash_table;
  dsa_pointer control;

                                                                      
  hash_table = palloc(sizeof(dshash_table));

                                                 
  control = handle;

                                           
  hash_table->area = area;
  hash_table->params = *params;
  hash_table->arg = arg;
  hash_table->control = dsa_get_address(area, control);
  Assert(hash_table->control->magic == DSHASH_MAGIC);

     
                                                      
                                                                      
                                                                  
     
  hash_table->buckets = NULL;
  hash_table->size_log2 = 0;

  return hash_table;
}

   
                                                                            
                                                                              
                                                                               
                                                               
   
void
dshash_detach(dshash_table *hash_table)
{
  ASSERT_NO_PARTITION_LOCKS_HELD_BY_ME(hash_table);

                                                                        
  pfree(hash_table);
}

   
                                                                               
                                                                              
                                                                               
                                                                             
                                                          
   
void
dshash_destroy(dshash_table *hash_table)
{
  size_t size;
  size_t i;

  Assert(hash_table->control->magic == DSHASH_MAGIC);
  ensure_valid_bucket_pointers(hash_table);

                             
  size = ((size_t)1) << hash_table->size_log2;
  for (i = 0; i < size; ++i)
  {
    dsa_pointer item_pointer = hash_table->buckets[i];

    while (DsaPointerIsValid(item_pointer))
    {
      dshash_table_item *item;
      dsa_pointer next_item_pointer;

      item = dsa_get_address(hash_table->area, item_pointer);
      next_item_pointer = item->next;
      dsa_free(hash_table->area, item_pointer);
      item_pointer = next_item_pointer;
    }
  }

     
                                                                        
                                                                            
     
  hash_table->control->magic = 0;

                                                 
  dsa_free(hash_table->area, hash_table->control->buckets);
  dsa_free(hash_table->area, hash_table->control->handle);

  pfree(hash_table);
}

   
                                                                           
          
   
dshash_table_handle
dshash_get_hash_table_handle(dshash_table *hash_table)
{
  Assert(hash_table->control->magic == DSHASH_MAGIC);

  return hash_table->control->handle;
}

   
                                                                               
                                                                          
                                                                           
                                                              
                                                                               
                                                                             
                                                                         
   
                                            
   
                                                                               
                                                                           
                                                                               
   
void *
dshash_find(dshash_table *hash_table, const void *key, bool exclusive)
{
  dshash_hash hash;
  size_t partition;
  dshash_table_item *item;

  hash = hash_key(hash_table, key);
  partition = PARTITION_FOR_HASH(hash);

  Assert(hash_table->control->magic == DSHASH_MAGIC);
  ASSERT_NO_PARTITION_LOCKS_HELD_BY_ME(hash_table);

  LWLockAcquire(PARTITION_LOCK(hash_table, partition), exclusive ? LW_EXCLUSIVE : LW_SHARED);
  ensure_valid_bucket_pointers(hash_table);

                                 
  item = find_in_bucket(hash_table, key, BUCKET_FOR_HASH(hash_table, hash));

  if (!item)
  {
                    
    LWLockRelease(PARTITION_LOCK(hash_table, partition));
    return NULL;
  }
  else
  {
                                                                       
    return ENTRY_FROM_ITEM(item);
  }
}

   
                                                                               
                                                                               
                                                                               
                                                                             
             
   
                                                                          
               
   
void *
dshash_find_or_insert(dshash_table *hash_table, const void *key, bool *found)
{
  dshash_hash hash;
  size_t partition_index;
  dshash_partition *partition;
  dshash_table_item *item;

  hash = hash_key(hash_table, key);
  partition_index = PARTITION_FOR_HASH(hash);
  partition = &hash_table->control->partitions[partition_index];

  Assert(hash_table->control->magic == DSHASH_MAGIC);
  ASSERT_NO_PARTITION_LOCKS_HELD_BY_ME(hash_table);

restart:
  LWLockAcquire(PARTITION_LOCK(hash_table, partition_index), LW_EXCLUSIVE);
  ensure_valid_bucket_pointers(hash_table);

                                 
  item = find_in_bucket(hash_table, key, BUCKET_FOR_HASH(hash_table, hash));

  if (item)
  {
    *found = true;
  }
  else
  {
    *found = false;

                                           
    if (partition->count > MAX_COUNT_PER_PARTITION(hash_table))
    {
         
                                                                         
                                                                
                                                                        
                                                                    
                                                                
                                                          
         
                                                                    
                                                                        
         
      LWLockRelease(PARTITION_LOCK(hash_table, partition_index));
      resize(hash_table, hash_table->size_log2 + 1);

      goto restart;
    }

                                                    
    item = insert_into_bucket(hash_table, key, &BUCKET_FOR_HASH(hash_table, hash));
    item->hash = hash;
                                                                      
    ++partition->count;
  }

                                                                  
  return ENTRY_FROM_ITEM(item);
}

   
                                                                      
                                    
   
                                                              
                        
   
bool
dshash_delete_key(dshash_table *hash_table, const void *key)
{
  dshash_hash hash;
  size_t partition;
  bool found;

  Assert(hash_table->control->magic == DSHASH_MAGIC);
  ASSERT_NO_PARTITION_LOCKS_HELD_BY_ME(hash_table);

  hash = hash_key(hash_table, key);
  partition = PARTITION_FOR_HASH(hash);

  LWLockAcquire(PARTITION_LOCK(hash_table, partition), LW_EXCLUSIVE);
  ensure_valid_bucket_pointers(hash_table);

  if (delete_key_from_bucket(hash_table, key, &BUCKET_FOR_HASH(hash_table, hash)))
  {
    Assert(hash_table->control->partitions[partition].count > 0);
    found = true;
    --hash_table->control->partitions[partition].count;
  }
  else
  {
    found = false;
  }

  LWLockRelease(PARTITION_LOCK(hash_table, partition));

  return found;
}

   
                                                                            
                                                                               
                                                             
   
                                                     
   
void
dshash_delete_entry(dshash_table *hash_table, void *entry)
{
  dshash_table_item *item = ITEM_FROM_ENTRY(entry);
  size_t partition = PARTITION_FOR_HASH(item->hash);

  Assert(hash_table->control->magic == DSHASH_MAGIC);
  Assert(LWLockHeldByMeInMode(PARTITION_LOCK(hash_table, partition), LW_EXCLUSIVE));

  delete_item(hash_table, item);
  LWLockRelease(PARTITION_LOCK(hash_table, partition));
}

   
                                                                             
   
void
dshash_release_lock(dshash_table *hash_table, void *entry)
{
  dshash_table_item *item = ITEM_FROM_ENTRY(entry);
  size_t partition_index = PARTITION_FOR_HASH(item->hash);

  Assert(hash_table->control->magic == DSHASH_MAGIC);

  LWLockRelease(PARTITION_LOCK(hash_table, partition_index));
}

   
                                               
   
int
dshash_memcmp(const void *a, const void *b, size_t size, void *arg)
{
  return memcmp(a, b, size);
}

   
                                              
   
dshash_hash
dshash_memhash(const void *v, size_t size, void *arg)
{
  return tag_hash(v, size);
}

   
                                                                             
                                                     
   
void
dshash_dump(dshash_table *hash_table)
{
  size_t i;
  size_t j;

  Assert(hash_table->control->magic == DSHASH_MAGIC);
  ASSERT_NO_PARTITION_LOCKS_HELD_BY_ME(hash_table);

  for (i = 0; i < DSHASH_NUM_PARTITIONS; ++i)
  {
    Assert(!LWLockHeldByMe(PARTITION_LOCK(hash_table, i)));
    LWLockAcquire(PARTITION_LOCK(hash_table, i), LW_SHARED);
  }

  ensure_valid_bucket_pointers(hash_table);

  fprintf(stderr, "hash table size = %zu\n", (size_t)1 << hash_table->size_log2);
  for (i = 0; i < DSHASH_NUM_PARTITIONS; ++i)
  {
    dshash_partition *partition = &hash_table->control->partitions[i];
    size_t begin = BUCKET_INDEX_FOR_PARTITION(i, hash_table->size_log2);
    size_t end = BUCKET_INDEX_FOR_PARTITION(i + 1, hash_table->size_log2);

    fprintf(stderr, "  partition %zu\n", i);
    fprintf(stderr, "    active buckets (key count = %zu)\n", partition->count);

    for (j = begin; j < end; ++j)
    {
      size_t count = 0;
      dsa_pointer bucket = hash_table->buckets[j];

      while (DsaPointerIsValid(bucket))
      {
        dshash_table_item *item;

        item = dsa_get_address(hash_table->area, bucket);

        bucket = item->next;
        ++count;
      }
      fprintf(stderr, "      bucket %zu (key count = %zu)\n", j, count);
    }
  }

  for (i = 0; i < DSHASH_NUM_PARTITIONS; ++i)
  {
    LWLockRelease(PARTITION_LOCK(hash_table, i));
  }
}

   
                                                    
   
static void
delete_item(dshash_table *hash_table, dshash_table_item *item)
{
  size_t hash = item->hash;
  size_t partition = PARTITION_FOR_HASH(hash);

  Assert(LWLockHeldByMe(PARTITION_LOCK(hash_table, partition)));

  if (delete_item_from_bucket(hash_table, item, &BUCKET_FOR_HASH(hash_table, hash)))
  {
    Assert(hash_table->control->partitions[partition].count > 0);
    --hash_table->control->partitions[partition].count;
  }
  else
  {
    Assert(false);
  }
}

   
                                                                             
                                                                
   
                                                   
   
static void
resize(dshash_table *hash_table, size_t new_size_log2)
{
  dsa_pointer old_buckets;
  dsa_pointer new_buckets_shared;
  dsa_pointer *new_buckets;
  size_t size;
  size_t new_size = ((size_t)1) << new_size_log2;
  size_t i;

     
                                                                           
                                         
     
  for (i = 0; i < DSHASH_NUM_PARTITIONS; ++i)
  {
    Assert(!LWLockHeldByMe(PARTITION_LOCK(hash_table, i)));

    LWLockAcquire(PARTITION_LOCK(hash_table, i), LW_EXCLUSIVE);
    if (i == 0 && hash_table->control->size_log2 >= new_size_log2)
    {
         
                                                                      
                                                   
         
      LWLockRelease(PARTITION_LOCK(hash_table, 0));
      return;
    }
  }

  Assert(new_size_log2 == hash_table->control->size_log2 + 1);

                                             
  new_buckets_shared = dsa_allocate0(hash_table->area, sizeof(dsa_pointer) * new_size);
  new_buckets = dsa_get_address(hash_table->area, new_buckets_shared);

     
                                                                            
                                                                      
     
  size = ((size_t)1) << hash_table->control->size_log2;
  for (i = 0; i < size; ++i)
  {
    dsa_pointer item_pointer = hash_table->buckets[i];

    while (DsaPointerIsValid(item_pointer))
    {
      dshash_table_item *item;
      dsa_pointer next_item_pointer;

      item = dsa_get_address(hash_table->area, item_pointer);
      next_item_pointer = item->next;
      insert_item_into_bucket(hash_table, item_pointer, item, &new_buckets[BUCKET_INDEX_FOR_HASH_AND_SIZE(item->hash, new_size_log2)]);
      item_pointer = next_item_pointer;
    }
  }

                                                            
  old_buckets = hash_table->control->buckets;
  hash_table->control->buckets = new_buckets_shared;
  hash_table->control->size_log2 = new_size_log2;
  hash_table->buckets = new_buckets;
  dsa_free(hash_table->area, old_buckets);

                              
  for (i = 0; i < DSHASH_NUM_PARTITIONS; ++i)
  {
    LWLockRelease(PARTITION_LOCK(hash_table, i));
  }
}

   
                                                                         
                                                                            
                         
   
static inline void
ensure_valid_bucket_pointers(dshash_table *hash_table)
{
  if (hash_table->size_log2 != hash_table->control->size_log2)
  {
    hash_table->buckets = dsa_get_address(hash_table->area, hash_table->control->buckets);
    hash_table->size_log2 = hash_table->control->size_log2;
  }
}

   
                                                                          
   
static inline dshash_table_item *
find_in_bucket(dshash_table *hash_table, const void *key, dsa_pointer item_pointer)
{
  while (DsaPointerIsValid(item_pointer))
  {
    dshash_table_item *item;

    item = dsa_get_address(hash_table->area, item_pointer);
    if (equal_keys(hash_table, key, ENTRY_FROM_ITEM(item)))
    {
      return item;
    }
    item_pointer = item->next;
  }
  return NULL;
}

   
                                                   
   
static void
insert_item_into_bucket(dshash_table *hash_table, dsa_pointer item_pointer, dshash_table_item *item, dsa_pointer *bucket)
{
  Assert(item == dsa_get_address(hash_table->area, item_pointer));

  item->next = *bucket;
  *bucket = item_pointer;
}

   
                                                                         
                    
   
static dshash_table_item *
insert_into_bucket(dshash_table *hash_table, const void *key, dsa_pointer *bucket)
{
  dsa_pointer item_pointer;
  dshash_table_item *item;

  item_pointer = dsa_allocate(hash_table->area, hash_table->params.entry_size + MAXALIGN(sizeof(dshash_table_item)));
  item = dsa_get_address(hash_table->area, item_pointer);
  memcpy(ENTRY_FROM_ITEM(item), key, hash_table->params.key_size);
  insert_item_into_bucket(hash_table, item_pointer, item, bucket);
  return item;
}

   
                                                     
   
static bool
delete_key_from_bucket(dshash_table *hash_table, const void *key, dsa_pointer *bucket_head)
{
  while (DsaPointerIsValid(*bucket_head))
  {
    dshash_table_item *item;

    item = dsa_get_address(hash_table->area, *bucket_head);

    if (equal_keys(hash_table, key, ENTRY_FROM_ITEM(item)))
    {
      dsa_pointer next;

      next = item->next;
      dsa_free(hash_table->area, *bucket_head);
      *bucket_head = next;

      return true;
    }
    bucket_head = &item->next;
  }
  return false;
}

   
                                              
   
static bool
delete_item_from_bucket(dshash_table *hash_table, dshash_table_item *item, dsa_pointer *bucket_head)
{
  while (DsaPointerIsValid(*bucket_head))
  {
    dshash_table_item *bucket_item;

    bucket_item = dsa_get_address(hash_table->area, *bucket_head);

    if (bucket_item == item)
    {
      dsa_pointer next;

      next = item->next;
      dsa_free(hash_table->area, *bucket_head);
      *bucket_head = next;
      return true;
    }
    bucket_head = &bucket_item->next;
  }
  return false;
}

   
                                     
   
static inline dshash_hash
hash_key(dshash_table *hash_table, const void *key)
{
  return hash_table->params.hash_function(key, hash_table->params.key_size, hash_table->arg);
}

   
                                         
   
static inline bool
equal_keys(dshash_table *hash_table, const void *a, const void *b)
{
  return hash_table->params.compare_function(a, b, hash_table->params.key_size, hash_table->arg) == 0;
}
