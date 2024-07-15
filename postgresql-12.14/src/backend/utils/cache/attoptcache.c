                                                                            
   
                 
                                         
   
                                                                          
                                                            
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/reloptions.h"
#include "utils/attoptcache.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/syscache.h"

                                                               
static HTAB *AttoptCacheHash = NULL;

                                                                    
typedef struct
{
  Oid attrelid;
  int attnum;
} AttoptCacheKey;

typedef struct
{
  AttoptCacheKey key;                                  
  AttributeOpts *opts;                               
} AttoptCacheEntry;

   
                                 
                                                          
   
                                                                        
                                                                            
                                                                         
                                    
   
static void
InvalidateAttoptCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{
  HASH_SEQ_STATUS status;
  AttoptCacheEntry *attopt;

  hash_seq_init(&status, AttoptCacheHash);
  while ((attopt = (AttoptCacheEntry *)hash_seq_search(&status)) != NULL)
  {
    if (attopt->opts)
    {
      pfree(attopt->opts);
    }
    if (hash_search(AttoptCacheHash, (void *)&attopt->key, HASH_REMOVE, NULL) == NULL)
    {
      elog(ERROR, "hash table corrupted");
    }
  }
}

   
                         
                                            
   
static void
InitializeAttoptCache(void)
{
  HASHCTL ctl;

                                  
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(AttoptCacheKey);
  ctl.entrysize = sizeof(AttoptCacheEntry);
  AttoptCacheHash = hash_create("Attopt cache", 256, &ctl, HASH_ELEM | HASH_BLOBS);

                                                       
  if (!CacheMemoryContext)
  {
    CreateCacheMemoryContext();
  }

                                      
  CacheRegisterSyscacheCallback(ATTNUM, InvalidateAttoptCacheCallback, (Datum)0);
}

   
                         
                                                       
   
AttributeOpts *
get_attribute_options(Oid attrelid, int attnum)
{
  AttoptCacheKey key;
  AttoptCacheEntry *attopt;
  AttributeOpts *result;
  HeapTuple tp;

                                          
  if (!AttoptCacheHash)
  {
    InitializeAttoptCache();
  }
  memset(&key, 0, sizeof(key));                                           
  key.attrelid = attrelid;
  key.attnum = attnum;
  attopt = (AttoptCacheEntry *)hash_search(AttoptCacheHash, (void *)&key, HASH_FIND, NULL);

                                                              
  if (!attopt)
  {
    AttributeOpts *opts;

    tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(attrelid), Int16GetDatum(attnum));

       
                                                                    
                                                                          
                                                           
       
    if (!HeapTupleIsValid(tp))
    {
      opts = NULL;
    }
    else
    {
      Datum datum;
      bool isNull;

      datum = SysCacheGetAttr(ATTNUM, tp, Anum_pg_attribute_attoptions, &isNull);
      if (isNull)
      {
        opts = NULL;
      }
      else
      {
        bytea *bytea_opts = attribute_reloptions(datum, false);

        opts = MemoryContextAlloc(CacheMemoryContext, VARSIZE(bytea_opts));
        memcpy(opts, bytea_opts, VARSIZE(bytea_opts));
      }
      ReleaseSysCache(tp);
    }

       
                                                                          
                                                               
       
    attopt = (AttoptCacheEntry *)hash_search(AttoptCacheHash, (void *)&key, HASH_ENTER, NULL);
    attopt->opts = opts;
  }

                                                  
  if (attopt->opts == NULL)
  {
    return NULL;
  }
  result = palloc(VARSIZE(attopt->opts));
  memcpy(result, attopt->opts, VARSIZE(attopt->opts));
  return result;
}
