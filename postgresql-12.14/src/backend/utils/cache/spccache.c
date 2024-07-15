                                                                            
   
              
                                  
   
                                                                          
                                                                           
                                                                           
                                         
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/reloptions.h"
#include "catalog/pg_tablespace.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "storage/bufmgr.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/spccache.h"
#include "utils/syscache.h"

                                                      
static HTAB *TableSpaceCacheHash = NULL;

typedef struct
{
  Oid oid;                                              
  TableSpaceOpts *opts;                               
} TableSpaceCacheEntry;

   
                                     
                                                           
   
                                                                         
                                                                           
                                                                          
                                                                 
   
static void
InvalidateTableSpaceCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{
  HASH_SEQ_STATUS status;
  TableSpaceCacheEntry *spc;

  hash_seq_init(&status, TableSpaceCacheHash);
  while ((spc = (TableSpaceCacheEntry *)hash_seq_search(&status)) != NULL)
  {
    if (spc->opts)
    {
      pfree(spc->opts);
    }
    if (hash_search(TableSpaceCacheHash, (void *)&spc->oid, HASH_REMOVE, NULL) == NULL)
    {
      elog(ERROR, "hash table corrupted");
    }
  }
}

   
                             
                                     
   
static void
InitializeTableSpaceCache(void)
{
  HASHCTL ctl;

                                  
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(TableSpaceCacheEntry);
  TableSpaceCacheHash = hash_create("TableSpace cache", 16, &ctl, HASH_ELEM | HASH_BLOBS);

                                                       
  if (!CacheMemoryContext)
  {
    CreateCacheMemoryContext();
  }

                                      
  CacheRegisterSyscacheCallback(TABLESPACEOID, InvalidateTableSpaceCacheCallback, (Datum)0);
}

   
                  
                                                                    
   
                                                                          
                               
   
static TableSpaceCacheEntry *
get_tablespace(Oid spcid)
{
  TableSpaceCacheEntry *spc;
  HeapTuple tp;
  TableSpaceOpts *opts;

     
                                                                         
              
     
  if (spcid == InvalidOid)
  {
    spcid = MyDatabaseTableSpace;
  }

                                          
  if (!TableSpaceCacheHash)
  {
    InitializeTableSpaceCache();
  }
  spc = (TableSpaceCacheEntry *)hash_search(TableSpaceCacheHash, (void *)&spcid, HASH_FIND, NULL);
  if (spc)
  {
    return spc;
  }

     
                                                                         
                                                                             
                                                                           
                                   
     
  tp = SearchSysCache1(TABLESPACEOID, ObjectIdGetDatum(spcid));
  if (!HeapTupleIsValid(tp))
  {
    opts = NULL;
  }
  else
  {
    Datum datum;
    bool isNull;

    datum = SysCacheGetAttr(TABLESPACEOID, tp, Anum_pg_tablespace_spcoptions, &isNull);
    if (isNull)
    {
      opts = NULL;
    }
    else
    {
      bytea *bytea_opts = tablespace_reloptions(datum, false);

      opts = MemoryContextAlloc(CacheMemoryContext, VARSIZE(bytea_opts));
      memcpy(opts, bytea_opts, VARSIZE(bytea_opts));
    }
    ReleaseSysCache(tp);
  }

     
                                                                       
                                                                         
            
     
  spc = (TableSpaceCacheEntry *)hash_search(TableSpaceCacheHash, (void *)&spcid, HASH_ENTER, NULL);
  spc->opts = opts;
  return spc;
}

   
                             
                                                                       
   
                                                                   
                                                                      
                        
   
void
get_tablespace_page_costs(Oid spcid, double *spc_random_page_cost, double *spc_seq_page_cost)
{
  TableSpaceCacheEntry *spc = get_tablespace(spcid);

  Assert(spc != NULL);

  if (spc_random_page_cost)
  {
    if (!spc->opts || spc->opts->random_page_cost < 0)
    {
      *spc_random_page_cost = random_page_cost;
    }
    else
    {
      *spc_random_page_cost = spc->opts->random_page_cost;
    }
  }

  if (spc_seq_page_cost)
  {
    if (!spc->opts || spc->opts->seq_page_cost < 0)
    {
      *spc_seq_page_cost = seq_page_cost;
    }
    else
    {
      *spc_seq_page_cost = spc->opts->seq_page_cost;
    }
  }
}

   
                                 
   
                                                                   
                                                                      
                        
   
int
get_tablespace_io_concurrency(Oid spcid)
{
  TableSpaceCacheEntry *spc = get_tablespace(spcid);

  if (!spc->opts || spc->opts->effective_io_concurrency < 0)
  {
    return effective_io_concurrency;
  }
  else
  {
    return spc->opts->effective_io_concurrency;
  }
}
