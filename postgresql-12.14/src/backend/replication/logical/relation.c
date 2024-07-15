                                                                            
              
                                     
   
                                                                
   
                  
                                                
   
         
                                                                          
                    
   
                                                                            
   

#include "postgres.h"

#include "access/relation.h"
#include "access/table.h"
#include "catalog/namespace.h"
#include "catalog/pg_subscription_rel.h"
#include "executor/executor.h"
#include "nodes/makefuncs.h"
#include "replication/logicalrelation.h"
#include "replication/worker_internal.h"
#include "utils/inval.h"

static MemoryContext LogicalRepRelMapContext = NULL;

static HTAB *LogicalRepRelMap = NULL;

   
                                                              
   
static void
logicalrep_relmap_invalidate_cb(Datum arg, Oid reloid)
{
  LogicalRepRelMapEntry *entry;

                        
  if (LogicalRepRelMap == NULL)
  {
    return;
  }

  if (reloid != InvalidOid)
  {
    HASH_SEQ_STATUS status;

    hash_seq_init(&status, LogicalRepRelMap);

                                             
    while ((entry = (LogicalRepRelMapEntry *)hash_seq_search(&status)) != NULL)
    {
      if (entry->localreloid == reloid)
      {
        entry->localrelvalid = false;
        hash_seq_term(&status);
        break;
      }
    }
  }
  else
  {
                                      
    HASH_SEQ_STATUS status;

    hash_seq_init(&status, LogicalRepRelMap);

    while ((entry = (LogicalRepRelMapEntry *)hash_seq_search(&status)) != NULL)
    {
      entry->localrelvalid = false;
    }
  }
}

   
                                      
   
static void
logicalrep_relmap_init(void)
{
  HASHCTL ctl;

  if (!LogicalRepRelMapContext)
  {
    LogicalRepRelMapContext = AllocSetContextCreate(CacheMemoryContext, "LogicalRepRelMapContext", ALLOCSET_DEFAULT_SIZES);
  }

                                           
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(LogicalRepRelId);
  ctl.entrysize = sizeof(LogicalRepRelMapEntry);
  ctl.hcxt = LogicalRepRelMapContext;

  LogicalRepRelMap = hash_create("logicalrep relation map cache", 128, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

                                      
  CacheRegisterRelcacheCallback(logicalrep_relmap_invalidate_cb, (Datum)0);
}

   
                                           
   
static void
logicalrep_relmap_free_entry(LogicalRepRelMapEntry *entry)
{
  LogicalRepRelation *remoterel;

  remoterel = &entry->remoterel;

  pfree(remoterel->nspname);
  pfree(remoterel->relname);

  if (remoterel->natts > 0)
  {
    int i;

    for (i = 0; i < remoterel->natts; i++)
    {
      pfree(remoterel->attnames[i]);
    }

    pfree(remoterel->attnames);
    pfree(remoterel->atttyps);
  }
  bms_free(remoterel->attkeys);

  if (entry->attrmap)
  {
    pfree(entry->attrmap);
  }
}

   
                                                                     
   
                                                                       
                                                           
   
void
logicalrep_relmap_update(LogicalRepRelation *remoterel)
{
  MemoryContext oldctx;
  LogicalRepRelMapEntry *entry;
  bool found;
  int i;

  if (LogicalRepRelMap == NULL)
  {
    logicalrep_relmap_init();
  }

     
                                                                            
     
  entry = hash_search(LogicalRepRelMap, (void *)&remoterel->remoteid, HASH_ENTER, &found);

  if (found)
  {
    logicalrep_relmap_free_entry(entry);
  }

  memset(entry, 0, sizeof(LogicalRepRelMapEntry));

                                    
  oldctx = MemoryContextSwitchTo(LogicalRepRelMapContext);
  entry->remoterel.remoteid = remoterel->remoteid;
  entry->remoterel.nspname = pstrdup(remoterel->nspname);
  entry->remoterel.relname = pstrdup(remoterel->relname);
  entry->remoterel.natts = remoterel->natts;
  entry->remoterel.attnames = palloc(remoterel->natts * sizeof(char *));
  entry->remoterel.atttyps = palloc(remoterel->natts * sizeof(Oid));
  for (i = 0; i < remoterel->natts; i++)
  {
    entry->remoterel.attnames[i] = pstrdup(remoterel->attnames[i]);
    entry->remoterel.atttyps[i] = remoterel->atttyps[i];
  }
  entry->remoterel.replident = remoterel->replident;
  entry->remoterel.attkeys = bms_copy(remoterel->attkeys);
  MemoryContextSwitchTo(oldctx);
}

   
                                                               
   
                            
   
static int
logicalrep_rel_att_by_name(LogicalRepRelation *remoterel, const char *attname)
{
  int i;

  for (i = 0; i < remoterel->natts; i++)
  {
    if (strcmp(remoterel->attnames[i], attname) == 0)
    {
      return i;
    }
  }

  return -1;
}

   
                                                           
   
                                                                     
   
LogicalRepRelMapEntry *
logicalrep_rel_open(LogicalRepRelId remoteid, LOCKMODE lockmode)
{
  LogicalRepRelMapEntry *entry;
  bool found;
  LogicalRepRelation *remoterel;

  if (LogicalRepRelMap == NULL)
  {
    logicalrep_relmap_init();
  }

                                  
  entry = hash_search(LogicalRepRelMap, (void *)&remoteid, HASH_FIND, &found);

  if (!found)
  {
    elog(ERROR, "no relation map entry for remote relation ID %u", remoteid);
  }

  remoterel = &entry->remoterel;

                                                 
  if (entry->localrel)
  {
    elog(ERROR, "remote relation ID %u is already open", remoteid);
  }

     
                                                                            
                                                                          
                                                                           
                                 
     
  if (entry->localrelvalid)
  {
    entry->localrel = try_relation_open(entry->localreloid, lockmode);
    if (!entry->localrel)
    {
                                         
      entry->localrelvalid = false;
    }
    else if (!entry->localrelvalid)
    {
                                                           
      table_close(entry->localrel, lockmode);
      entry->localrel = NULL;
    }
  }

     
                                                                        
                                                                      
     
  if (!entry->localrelvalid)
  {
    Oid relid;
    int found;
    Bitmapset *idkey;
    TupleDesc desc;
    MemoryContext oldctx;
    int i;

                                                       
    if (entry->attrmap)
    {
      pfree(entry->attrmap);
      entry->attrmap = NULL;
    }

                                                    
    relid = RangeVarGetRelid(makeRangeVar(remoterel->nspname, remoterel->relname, -1), lockmode, true);
    if (!OidIsValid(relid))
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication target relation \"%s.%s\" does not exist", remoterel->nspname, remoterel->relname)));
    }
    entry->localrel = table_open(relid, NoLock);
    entry->localreloid = relid;

                                      
    CheckSubscriptionRelkind(entry->localrel->rd_rel->relkind, remoterel->nspname, remoterel->relname);

       
                                                                        
                                                                         
                                                            
       
    desc = RelationGetDescr(entry->localrel);
    oldctx = MemoryContextSwitchTo(LogicalRepRelMapContext);
    entry->attrmap = palloc(desc->natts * sizeof(AttrNumber));
    MemoryContextSwitchTo(oldctx);

    found = 0;
    for (i = 0; i < desc->natts; i++)
    {
      int attnum;
      Form_pg_attribute attr = TupleDescAttr(desc, i);

      if (attr->attisdropped || attr->attgenerated)
      {
        entry->attrmap[i] = -1;
        continue;
      }

      attnum = logicalrep_rel_att_by_name(remoterel, NameStr(attr->attname));

      entry->attrmap[i] = attnum;
      if (attnum >= 0)
      {
        found++;
      }
    }

                                                            
    if (found < remoterel->natts)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication target relation \"%s.%s\" is missing "
                                                                                "some replicated columns",
                                                                             remoterel->nspname, remoterel->relname)));
    }

       
                                                                          
                                                                       
                                                                
                                                                           
                                             
       
                                                                      
                                                                          
                                                  
       
    entry->updatable = true;
    idkey = RelationGetIndexAttrBitmap(entry->localrel, INDEX_ATTR_BITMAP_IDENTITY_KEY);
                                               
    if (idkey == NULL)
    {
      idkey = RelationGetIndexAttrBitmap(entry->localrel, INDEX_ATTR_BITMAP_PRIMARY_KEY);

         
                                                                     
                                          
         
      if (idkey == NULL && remoterel->replident != REPLICA_IDENTITY_FULL)
      {
        entry->updatable = false;
      }
    }

    i = -1;
    while ((i = bms_next_member(idkey, i)) >= 0)
    {
      int attnum = i + FirstLowInvalidHeapAttributeNumber;

      if (!AttrNumberIsForUserDefinedAttr(attnum))
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication target relation \"%s.%s\" uses "
                                                                                  "system columns in REPLICA IDENTITY index",
                                                                               remoterel->nspname, remoterel->relname)));
      }

      attnum = AttrNumberGetAttrOffset(attnum);

      if (entry->attrmap[attnum] < 0 || !bms_is_member(entry->attrmap[attnum], remoterel->attkeys))
      {
        entry->updatable = false;
        break;
      }
    }

    entry->localrelvalid = true;
  }

  if (entry->state != SUBREL_STATE_READY)
  {
    entry->state = GetSubscriptionRelState(MySubscription->oid, entry->localreloid, &entry->statelsn, true);
  }

  return entry;
}

   
                                                 
   
void
logicalrep_rel_close(LogicalRepRelMapEntry *rel, LOCKMODE lockmode)
{
  table_close(rel->localrel, lockmode);
  rel->localrel = NULL;
}
