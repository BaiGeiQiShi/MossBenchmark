                                                                            
   
                    
                                       
   
                                                                         
                                                                        
   
                  
                                              
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_class.h"
#include "catalog/pg_tablespace.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/hsearch.h"
#include "utils/inval.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/relfilenodemap.h"
#include "utils/relmapper.h"

                                                                    
static HTAB *RelfilenodeMapHash = NULL;

                                                          
static ScanKeyData relfilenode_skey[2];

typedef struct
{
  Oid reltablespace;
  Oid relfilenode;
} RelfilenodeMapKey;

typedef struct
{
  RelfilenodeMapKey key;                                 
  Oid relid;                               
} RelfilenodeMapEntry;

   
                                    
                                                                          
   
static void
RelfilenodeMapInvalidateCallback(Datum arg, Oid relid)
{
  HASH_SEQ_STATUS status;
  RelfilenodeMapEntry *entry;

                                                             
  Assert(RelfilenodeMapHash != NULL);

  hash_seq_init(&status, RelfilenodeMapHash);
  while ((entry = (RelfilenodeMapEntry *)hash_seq_search(&status)) != NULL)
  {
       
                                                                           
                                                                         
                                             
       
    if (relid == InvalidOid ||                            
        entry->relid == InvalidOid ||                           
        entry->relid == relid)                                         
    {
      if (hash_search(RelfilenodeMapHash, (void *)&entry->key, HASH_REMOVE, NULL) == NULL)
      {
        elog(ERROR, "hash table corrupted");
      }
    }
  }
}

   
                                    
                                                            
   
static void
InitializeRelfilenodeMap(void)
{
  HASHCTL ctl;
  int i;

                                                       
  if (CacheMemoryContext == NULL)
  {
    CreateCacheMemoryContext();
  }

                  
  MemSet(&relfilenode_skey, 0, sizeof(relfilenode_skey));

  for (i = 0; i < 2; i++)
  {
    fmgr_info_cxt(F_OIDEQ, &relfilenode_skey[i].sk_func, CacheMemoryContext);
    relfilenode_skey[i].sk_strategy = BTEqualStrategyNumber;
    relfilenode_skey[i].sk_subtype = InvalidOid;
    relfilenode_skey[i].sk_collation = InvalidOid;
  }

  relfilenode_skey[0].sk_attno = Anum_pg_class_reltablespace;
  relfilenode_skey[1].sk_attno = Anum_pg_class_relfilenode;

                                  
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(RelfilenodeMapKey);
  ctl.entrysize = sizeof(RelfilenodeMapEntry);
  ctl.hcxt = CacheMemoryContext;

     
                                                                          
                                                                             
            
     
  RelfilenodeMapHash = hash_create("RelfilenodeMap cache", 64, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

                                      
  CacheRegisterRelcacheCallback(RelfilenodeMapInvalidateCallback, (Datum)0);
}

   
                                                                             
           
   
                                                                           
   
Oid
RelidByRelfilenode(Oid reltablespace, Oid relfilenode)
{
  RelfilenodeMapKey key;
  RelfilenodeMapEntry *entry;
  bool found;
  SysScanDesc scandesc;
  Relation relation;
  HeapTuple ntp;
  ScanKeyData skey[2];
  Oid relid;

  if (RelfilenodeMapHash == NULL)
  {
    InitializeRelfilenodeMap();
  }

                                                                            
  if (reltablespace == MyDatabaseTableSpace)
  {
    reltablespace = 0;
  }

  MemSet(&key, 0, sizeof(key));
  key.reltablespace = reltablespace;
  key.relfilenode = relfilenode;

     
                                                                     
                                                                             
                                                                        
                                                                          
                              
     
  entry = hash_search(RelfilenodeMapHash, (void *)&key, HASH_FIND, &found);

  if (found)
  {
    return entry->relid;
  }

                                                       

                                                                             
  relid = InvalidOid;

  if (reltablespace == GLOBALTABLESPACE_OID)
  {
       
                                          
       
    relid = RelationMapFilenodeToOid(relfilenode, true);
  }
  else
  {
       
                                                                 
                                                   
       

                                                          
    relation = table_open(RelationRelationId, AccessShareLock);

                                                                         
    memcpy(skey, relfilenode_skey, sizeof(skey));

                            
    skey[0].sk_argument = ObjectIdGetDatum(reltablespace);
    skey[1].sk_argument = ObjectIdGetDatum(relfilenode);

    scandesc = systable_beginscan(relation, ClassTblspcRelfilenodeIndexId, true, NULL, 2, skey);

    found = false;

    while (HeapTupleIsValid(ntp = systable_getnext(scandesc)))
    {
      Form_pg_class classform = (Form_pg_class)GETSTRUCT(ntp);

      if (found)
      {
        elog(ERROR, "unexpected duplicate for tablespace %u, relfilenode %u", reltablespace, relfilenode);
      }
      found = true;

      Assert(classform->reltablespace == reltablespace);
      Assert(classform->relfilenode == relfilenode);
      relid = classform->oid;
    }

    systable_endscan(scandesc);
    table_close(relation, AccessShareLock);

                                                         
    if (!found)
    {
      relid = RelationMapFilenodeToOid(relfilenode, false);
    }
  }

     
                                                                         
                                                                          
                                           
     
  entry = hash_search(RelfilenodeMapHash, (void *)&key, HASH_ENTER, &found);
  if (found)
  {
    elog(ERROR, "corrupted hashtable");
  }
  entry->relid = relid;

  return relid;
}
