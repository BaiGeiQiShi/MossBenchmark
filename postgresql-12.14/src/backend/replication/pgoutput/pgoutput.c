                                                                            
   
              
                                      
   
                                                                
   
                  
                                                  
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_publication.h"

#include "replication/logical.h"
#include "replication/logicalproto.h"
#include "replication/origin.h"
#include "replication/pgoutput.h"

#include "utils/inval.h"
#include "utils/int8.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

PG_MODULE_MAGIC;

extern void
_PG_output_plugin_init(OutputPluginCallbacks *cb);

static void
pgoutput_startup(LogicalDecodingContext *ctx, OutputPluginOptions *opt, bool is_init);
static void
pgoutput_shutdown(LogicalDecodingContext *ctx);
static void
pgoutput_begin_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn);
static void
pgoutput_commit_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn, XLogRecPtr commit_lsn);
static void
pgoutput_change(LogicalDecodingContext *ctx, ReorderBufferTXN *txn, Relation rel, ReorderBufferChange *change);
static void
pgoutput_truncate(LogicalDecodingContext *ctx, ReorderBufferTXN *txn, int nrelations, Relation relations[], ReorderBufferChange *change);
static bool
pgoutput_origin_filter(LogicalDecodingContext *ctx, RepOriginId origin_id);

static bool publications_valid;

static List *
LoadPublications(List *pubnames);
static void
publication_invalidation_cb(Datum arg, int cacheid, uint32 hashvalue);
static void
update_replication_progress(LogicalDecodingContext *ctx);

                                                                       
typedef struct RelationSyncEntry
{
  Oid relid;                          
  bool schema_sent;                              
  bool replicate_valid;
  PublicationActions pubactions;
} RelationSyncEntry;

                                                          
static HTAB *RelationSyncCache = NULL;

static void
init_rel_sync_cache(MemoryContext decoding_context);
static RelationSyncEntry *
get_rel_sync_entry(PGOutputData *data, Oid relid);
static void
rel_sync_cache_relation_cb(Datum arg, Oid relid);
static void
rel_sync_cache_publication_cb(Datum arg, int cacheid, uint32 hashvalue);

   
                                   
   
void
_PG_output_plugin_init(OutputPluginCallbacks *cb)
{
  AssertVariableIsOfType(&_PG_output_plugin_init, LogicalOutputPluginInit);

  cb->startup_cb = pgoutput_startup;
  cb->begin_cb = pgoutput_begin_txn;
  cb->change_cb = pgoutput_change;
  cb->truncate_cb = pgoutput_truncate;
  cb->commit_cb = pgoutput_commit_txn;
  cb->filter_by_origin_cb = pgoutput_origin_filter;
  cb->shutdown_cb = pgoutput_shutdown;
}

static void
parse_output_parameters(List *options, uint32 *protocol_version, List **publication_names)
{
  ListCell *lc;
  bool protocol_version_given = false;
  bool publication_names_given = false;

  foreach (lc, options)
  {
    DefElem *defel = (DefElem *)lfirst(lc);

    Assert(defel->arg == NULL || IsA(defel->arg, String));

                                                          
    if (strcmp(defel->defname, "proto_version") == 0)
    {
      int64 parsed;

      if (protocol_version_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      protocol_version_given = true;

      if (!scanint8(strVal(defel->arg), true, &parsed))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid proto_version")));
      }

      if (parsed > PG_UINT32_MAX || parsed < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("proto_version \"%s\" out of range", strVal(defel->arg))));
      }

      *protocol_version = (uint32)parsed;
    }
    else if (strcmp(defel->defname, "publication_names") == 0)
    {
      if (publication_names_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      publication_names_given = true;

      if (!SplitIdentifierString(strVal(defel->arg), ',', publication_names))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid publication_names syntax")));
      }
    }
    else
    {
      elog(ERROR, "unrecognized pgoutput option: %s", defel->defname);
    }
  }
}

   
                          
   
static void
pgoutput_startup(LogicalDecodingContext *ctx, OutputPluginOptions *opt, bool is_init)
{
  PGOutputData *data = palloc0(sizeof(PGOutputData));

                                                          
  data->context = AllocSetContextCreate(ctx->context, "logical replication output context", ALLOCSET_DEFAULT_SIZES);

  ctx->output_plugin_private = data;

                                         
  opt->output_type = OUTPUT_PLUGIN_BINARY_OUTPUT;

     
                                                            
     
                                                      
     
  if (!is_init)
  {
                                                                     
    parse_output_parameters(ctx->output_plugin_options, &data->protocol_version, &data->publication_names);

                                                
    if (data->protocol_version > LOGICALREP_PROTO_VERSION_NUM)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("client sent proto_version=%d but we only support protocol %d or lower", data->protocol_version, LOGICALREP_PROTO_VERSION_NUM)));
    }

    if (data->protocol_version < LOGICALREP_PROTO_MIN_VERSION_NUM)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("client sent proto_version=%d but we only support protocol %d or higher", data->protocol_version, LOGICALREP_PROTO_MIN_VERSION_NUM)));
    }

    if (list_length(data->publication_names) < 1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("publication_names parameter missing")));
    }

                                 
    data->publications = NIL;
    publications_valid = false;
    CacheRegisterSyscacheCallback(PUBLICATIONOID, publication_invalidation_cb, (Datum)0);

                                           
    init_rel_sync_cache(CacheMemoryContext);
  }
}

   
                  
   
static void
pgoutput_begin_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn)
{
  bool send_replication_origin = txn->origin_id != InvalidRepOriginId;

  OutputPluginPrepareWrite(ctx, !send_replication_origin);
  logicalrep_write_begin(ctx->out, txn);

  if (send_replication_origin)
  {
    char *origin;

                          
    OutputPluginWrite(ctx, false);
    OutputPluginPrepareWrite(ctx, true);

                 
                                             
       
                     
                                                            
                                 
                                                             
                                            
                 
       
    if (replorigin_by_oid(txn->origin_id, true, &origin))
    {
      logicalrep_write_origin(ctx->out, origin, txn->origin_lsn);
    }
  }

  OutputPluginWrite(ctx, true);
}

   
                   
   
static void
pgoutput_commit_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn, XLogRecPtr commit_lsn)
{
  update_replication_progress(ctx);

  OutputPluginPrepareWrite(ctx, true);
  logicalrep_write_commit(ctx->out, txn, commit_lsn);
  OutputPluginWrite(ctx, true);
}

   
                                                                         
   
static void
maybe_send_schema(LogicalDecodingContext *ctx, Relation relation, RelationSyncEntry *relentry)
{
  if (!relentry->schema_sent)
  {
    TupleDesc desc;
    int i;

    desc = RelationGetDescr(relation);

       
                                                                        
                                                                         
                                                                          
                                                                        
                                                                         
                                               
       
    for (i = 0; i < desc->natts; i++)
    {
      Form_pg_attribute att = TupleDescAttr(desc, i);

      if (att->attisdropped || att->attgenerated)
      {
        continue;
      }

      if (att->atttypid < FirstGenbkiObjectId)
      {
        continue;
      }

      OutputPluginPrepareWrite(ctx, false);
      logicalrep_write_typ(ctx->out, att->atttypid);
      OutputPluginWrite(ctx, false);
    }

    OutputPluginPrepareWrite(ctx, false);
    logicalrep_write_rel(ctx->out, relation);
    OutputPluginWrite(ctx, false);
    relentry->schema_sent = true;
  }
}

   
                                    
   
static void
pgoutput_change(LogicalDecodingContext *ctx, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{
  PGOutputData *data = (PGOutputData *)ctx->output_plugin_private;
  MemoryContext old;
  RelationSyncEntry *relentry;

  update_replication_progress(ctx);

  if (!is_publishable_relation(relation))
  {
    return;
  }

  relentry = get_rel_sync_entry(data, RelationGetRelid(relation));

                                    
  switch (change->action)
  {
  case REORDER_BUFFER_CHANGE_INSERT:
    if (!relentry->pubactions.pubinsert)
    {
      return;
    }
    break;
  case REORDER_BUFFER_CHANGE_UPDATE:
    if (!relentry->pubactions.pubupdate)
    {
      return;
    }
    break;
  case REORDER_BUFFER_CHANGE_DELETE:
    if (!relentry->pubactions.pubdelete)
    {
      return;
    }
    break;
  default:
    Assert(false);
  }

                                                                   
  old = MemoryContextSwitchTo(data->context);

  maybe_send_schema(ctx, relation, relentry);

                     
  switch (change->action)
  {
  case REORDER_BUFFER_CHANGE_INSERT:
    OutputPluginPrepareWrite(ctx, true);
    logicalrep_write_insert(ctx->out, relation, &change->data.tp.newtuple->tuple);
    OutputPluginWrite(ctx, true);
    break;
  case REORDER_BUFFER_CHANGE_UPDATE:
  {
    HeapTuple oldtuple = change->data.tp.oldtuple ? &change->data.tp.oldtuple->tuple : NULL;

    OutputPluginPrepareWrite(ctx, true);
    logicalrep_write_update(ctx->out, relation, oldtuple, &change->data.tp.newtuple->tuple);
    OutputPluginWrite(ctx, true);
    break;
  }
  case REORDER_BUFFER_CHANGE_DELETE:
    if (change->data.tp.oldtuple)
    {
      OutputPluginPrepareWrite(ctx, true);
      logicalrep_write_delete(ctx->out, relation, &change->data.tp.oldtuple->tuple);
      OutputPluginWrite(ctx, true);
    }
    else
    {
      elog(DEBUG1, "didn't send DELETE change because of missing oldtuple");
    }
    break;
  default:
    Assert(false);
  }

               
  MemoryContextSwitchTo(old);
  MemoryContextReset(data->context);
}

static void
pgoutput_truncate(LogicalDecodingContext *ctx, ReorderBufferTXN *txn, int nrelations, Relation relations[], ReorderBufferChange *change)
{
  PGOutputData *data = (PGOutputData *)ctx->output_plugin_private;
  MemoryContext old;
  RelationSyncEntry *relentry;
  int i;
  int nrelids;
  Oid *relids;

  update_replication_progress(ctx);

  old = MemoryContextSwitchTo(data->context);

  relids = palloc0(nrelations * sizeof(Oid));
  nrelids = 0;

  for (i = 0; i < nrelations; i++)
  {
    Relation relation = relations[i];
    Oid relid = RelationGetRelid(relation);

    if (!is_publishable_relation(relation))
    {
      continue;
    }

    relentry = get_rel_sync_entry(data, relid);

    if (!relentry->pubactions.pubtruncate)
    {
      continue;
    }

    relids[nrelids++] = relid;
    maybe_send_schema(ctx, relation, relentry);
  }

  if (nrelids > 0)
  {
    OutputPluginPrepareWrite(ctx, true);
    logicalrep_write_truncate(ctx->out, nrelids, relids, change->data.truncate.cascade, change->data.truncate.restart_seqs);
    OutputPluginWrite(ctx, true);
  }

  MemoryContextSwitchTo(old);
  MemoryContextReset(data->context);
}

   
                                
   
static bool
pgoutput_origin_filter(LogicalDecodingContext *ctx, RepOriginId origin_id)
{
  return false;
}

   
                               
   
                                                                        
                                                                               
   
static void
pgoutput_shutdown(LogicalDecodingContext *ctx)
{
  if (RelationSyncCache)
  {
    hash_destroy(RelationSyncCache);
    RelationSyncCache = NULL;
  }
}

   
                                                         
   
static List *
LoadPublications(List *pubnames)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, pubnames)
  {
    char *pubname = (char *)lfirst(lc);
    Publication *pub = GetPublicationByName(pubname, false);

    result = lappend(result, pub);
  }

  return result;
}

   
                                            
   
static void
publication_invalidation_cb(Datum arg, int cacheid, uint32 hashvalue)
{
  publications_valid = false;

     
                                                                             
                                                                      
     
  rel_sync_cache_publication_cb(arg, cacheid, hashvalue);
}

   
                                                                     
   
                                                                       
                                                                      
                                                                
   
static void
init_rel_sync_cache(MemoryContext cachectx)
{
  HASHCTL ctl;
  MemoryContext old_ctxt;

  if (RelationSyncCache != NULL)
  {
    return;
  }

                                           
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(RelationSyncEntry);
  ctl.hcxt = cachectx;

  old_ctxt = MemoryContextSwitchTo(cachectx);
  RelationSyncCache = hash_create("logical replication output relation cache", 128, &ctl, HASH_ELEM | HASH_CONTEXT | HASH_BLOBS);
  (void)MemoryContextSwitchTo(old_ctxt);

  Assert(RelationSyncCache != NULL);

  CacheRegisterRelcacheCallback(rel_sync_cache_relation_cb, (Datum)0);
  CacheRegisterSyscacheCallback(PUBLICATIONRELMAP, rel_sync_cache_publication_cb, (Datum)0);
}

   
                                                      
   
static RelationSyncEntry *
get_rel_sync_entry(PGOutputData *data, Oid relid)
{
  RelationSyncEntry *entry;
  bool found;
  MemoryContext oldctx;

  Assert(RelationSyncCache != NULL);

                                                        
  oldctx = MemoryContextSwitchTo(CacheMemoryContext);
  entry = (RelationSyncEntry *)hash_search(RelationSyncCache, (void *)&relid, HASH_ENTER, &found);
  MemoryContextSwitchTo(oldctx);
  Assert(entry != NULL);

                                          
  if (!found || !entry->replicate_valid)
  {
    List *pubids = GetRelationPublications(relid);
    ListCell *lc;

                                                   
    if (!publications_valid)
    {
      oldctx = MemoryContextSwitchTo(CacheMemoryContext);
      if (data->publications)
      {
        list_free_deep(data->publications);
      }

      data->publications = LoadPublications(data->publication_names);
      MemoryContextSwitchTo(oldctx);
      publications_valid = true;
    }

       
                                                                         
                                                                          
                                                                    
       
    entry->pubactions.pubinsert = entry->pubactions.pubupdate = entry->pubactions.pubdelete = entry->pubactions.pubtruncate = false;

    foreach (lc, data->publications)
    {
      Publication *pub = lfirst(lc);

      if (pub->alltables || list_member_oid(pubids, pub->oid))
      {
        entry->pubactions.pubinsert |= pub->pubactions.pubinsert;
        entry->pubactions.pubupdate |= pub->pubactions.pubupdate;
        entry->pubactions.pubdelete |= pub->pubactions.pubdelete;
        entry->pubactions.pubtruncate |= pub->pubactions.pubtruncate;
      }

      if (entry->pubactions.pubinsert && entry->pubactions.pubupdate && entry->pubactions.pubdelete && entry->pubactions.pubtruncate)
      {
        break;
      }
    }

    list_free(pubids);

    entry->replicate_valid = true;
  }

  if (!found)
  {
    entry->schema_sent = false;
  }

  return entry;
}

   
                                  
   
static void
rel_sync_cache_relation_cb(Datum arg, Oid relid)
{
  RelationSyncEntry *entry;

     
                                                                    
                                                                           
                                                                 
     
  if (RelationSyncCache == NULL)
  {
    return;
  }

     
                                                                        
                                                                           
                                                                            
                                                                            
                                                                           
                 
     
                                                                     
                                                                             
                                                   
     
  entry = (RelationSyncEntry *)hash_search(RelationSyncCache, &relid, HASH_FIND, NULL);

     
                                                                           
     
  if (entry != NULL)
  {
    entry->schema_sent = false;
  }
}

   
                                                           
   
static void
rel_sync_cache_publication_cb(Datum arg, int cacheid, uint32 hashvalue)
{
  HASH_SEQ_STATUS status;
  RelationSyncEntry *entry;

     
                                                                    
                                                                           
                                                                 
     
  if (RelationSyncCache == NULL)
  {
    return;
  }

     
                                                                             
                                      
     
  hash_seq_init(&status, RelationSyncCache);
  while ((entry = (RelationSyncEntry *)hash_seq_search(&status)) != NULL)
  {
    entry->replicate_valid = false;
  }
}

   
                                                                                
              
   
                                                                                
                                                                                
                                                                      
   
static void
update_replication_progress(LogicalDecodingContext *ctx)
{
  static int changes_count = 0;

     
                                                                            
                                                                       
                                                                             
              
     
#define CHANGES_THRESHOLD 100

     
                                                                        
                                                                            
                                                  
     
  if (ctx->end_xact || ++changes_count >= CHANGES_THRESHOLD)
  {
    OutputPluginUpdateProgress(ctx);
    changes_count = 0;
  }
}
