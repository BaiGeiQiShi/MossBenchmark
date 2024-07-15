                                                                            
   
                 
                                          
   
                                                                          
                                                        
   
             
   
                                                                    
                                                                    
                                                                        
                                                                   
   
                        
   
                      
                            
     
                         
                               
           
        
                                                  
                          
        
     
                    
   
                                                                       
                               
   
   
                  
   
                                                                          
                                                                    
                                                                            
                                                                          
                                                                          
                                                                         
                                           
   
                                                                          
                                                                            
                        
   
                                                                         
                                                                           
                                                                           
                    
   
                                                                        
                                                                      
                                                                 
                                                                           
                                                      
   
                                                                        
                                                                           
                                                                          
                                                                         
                                                                         
                                                                        
                                                                            
                                                                       
                                                                     
   
                                                                           
                                                                   
                                                                          
                                                                           
                                                                       
                                                                           
                                                                            
                                                                           
                                                                         
                                                                          
                                                                             
                                                                           
                                                                 
   
                                                                       
                                                                    
                                                                    
                                                                         
                                                                      
                                                                      
                                                                    
                                                                        
                                                         
   
   
                                                                         
                                                                          
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "miscadmin.h"

#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/rewriteheap.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "access/xloginsert.h"

#include "catalog/catalog.h"

#include "lib/ilist.h"

#include "pgstat.h"

#include "replication/logical.h"
#include "replication/slot.h"

#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/smgr.h"

#include "utils/memutils.h"
#include "utils/rel.h"

#include "storage/procarray.h"

   
                                                                         
                            
   
typedef struct RewriteStateData
{
  Relation rs_old_rel;                             
  Relation rs_new_rel;                                  
  Page rs_buffer;                                                 
  BlockNumber rs_blockno;                                       
  bool rs_buffer_valid;                                          
  bool rs_use_wal;                                              
  bool rs_logical_rewrite;                                                
  TransactionId rs_oldest_xmin;                                              
                                                        
  TransactionId rs_freeze_xid;                                              
                                             
  TransactionId rs_logical_xmin;                                           
                                                            
  MultiXactId rs_cutoff_multi;                                               
                                                            
  MemoryContext rs_cxt;                                                        
                                            
  XLogRecPtr rs_begin_lsn;                                                     
  HTAB *rs_unresolved_tups;                               
  HTAB *rs_old_new_tid_map;                               
  HTAB *rs_logical_mappings;                                   
  uint32 rs_num_rewrite_mappings;                           
} RewriteStateData;

   
                                                                             
                                                                        
                                                                         
                                
   
typedef struct
{
  TransactionId xmin;                  
  ItemPointerData tid;                                 
} TidHashKey;

   
                                        
   
typedef struct
{
  TidHashKey key;                                                     
  ItemPointerData old_tid;                                   
  HeapTuple tuple;                                 
} UnresolvedTupData;

typedef UnresolvedTupData *UnresolvedTup;

typedef struct
{
  TidHashKey key;                                                   
  ItemPointerData new_tid;                                      
} OldToNewMappingData;

typedef OldToNewMappingData *OldToNewMapping;

   
                                                                       
                 
   
typedef struct RewriteMappingFile
{
  TransactionId xid;                                            
  int vfd;                                       
  off_t off;                                             
  uint32 num_mappings;                                    
  dlist_head mappings;                                  
  char path[MAXPGPATH];                               
} RewriteMappingFile;

   
                                                           
                                 
   
typedef struct RewriteMappingDataEntry
{
  LogicalRewriteMappingData map;                                            
                                            
  dlist_node node;
} RewriteMappingDataEntry;

                                       
static void
raw_heap_insert(RewriteState state, HeapTuple tup);

                                           
static void
logical_begin_heap_rewrite(RewriteState state);
static void
logical_rewrite_heap_tuple(RewriteState state, ItemPointerData old_tid, HeapTuple new_tuple);
static void
logical_end_heap_rewrite(RewriteState state);

   
                              
   
                                                                
                                                           
                                                                         
                                                     
                                                           
                                                              
   
                                                                        
                                                          
   
RewriteState
begin_heap_rewrite(Relation old_heap, Relation new_heap, TransactionId oldest_xmin, TransactionId freeze_xid, MultiXactId cutoff_multi, bool use_wal)
{
  RewriteState state;
  MemoryContext rw_cxt;
  MemoryContext old_cxt;
  HASHCTL hash_ctl;

     
                                                                    
                                                          
     
  rw_cxt = AllocSetContextCreate(CurrentMemoryContext, "Table rewrite", ALLOCSET_DEFAULT_SIZES);
  old_cxt = MemoryContextSwitchTo(rw_cxt);

                                           
  state = palloc0(sizeof(RewriteStateData));

  state->rs_old_rel = old_heap;
  state->rs_new_rel = new_heap;
  state->rs_buffer = (Page)palloc(BLCKSZ);
                                              
  state->rs_blockno = RelationGetNumberOfBlocks(new_heap);
  state->rs_buffer_valid = false;
  state->rs_use_wal = use_wal;
  state->rs_oldest_xmin = oldest_xmin;
  state->rs_freeze_xid = freeze_xid;
  state->rs_cutoff_multi = cutoff_multi;
  state->rs_cxt = rw_cxt;

                                                          
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(TidHashKey);
  hash_ctl.entrysize = sizeof(UnresolvedTupData);
  hash_ctl.hcxt = state->rs_cxt;

  state->rs_unresolved_tups = hash_create("Rewrite / Unresolved ctids", 128,                             
      &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  hash_ctl.entrysize = sizeof(OldToNewMappingData);

  state->rs_old_new_tid_map = hash_create("Rewrite / Old to new tid map", 128,                             
      &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  MemoryContextSwitchTo(old_cxt);

  logical_begin_heap_rewrite(state);

  return state;
}

   
                  
   
                                            
   
void
end_heap_rewrite(RewriteState state)
{
  HASH_SEQ_STATUS seq_status;
  UnresolvedTup unresolved;

     
                                                                            
                                                                        
     
  hash_seq_init(&seq_status, state->rs_unresolved_tups);

  while ((unresolved = hash_seq_search(&seq_status)) != NULL)
  {
    ItemPointerSetInvalid(&unresolved->tuple->t_data->t_ctid);
    raw_heap_insert(state, unresolved->tuple);
  }

                                   
  if (state->rs_buffer_valid)
  {
    if (state->rs_use_wal)
    {
      log_newpage(&state->rs_new_rel->rd_node, MAIN_FORKNUM, state->rs_blockno, state->rs_buffer, true);
    }

    PageSetChecksumInplace(state->rs_buffer, state->rs_blockno);

    smgrextend(RelationGetSmgr(state->rs_new_rel), MAIN_FORKNUM, state->rs_blockno, (char *)state->rs_buffer, true);
  }

     
                                                                           
                                                      
     
                                                                       
                                                                         
                                                                       
                                                                    
                                                                           
                                  
     
  if (RelationNeedsWAL(state->rs_new_rel))
  {
    heap_sync(state->rs_new_rel);
  }

  logical_end_heap_rewrite(state);

                                             
  MemoryContextDelete(state->rs_cxt);
}

   
                                
   
                                                                         
                                                                           
                                                                      
   
                                                         
                                            
                                                             
   
void
rewrite_heap_tuple(RewriteState state, HeapTuple old_tuple, HeapTuple new_tuple)
{
  MemoryContext old_cxt;
  ItemPointerData old_tid;
  TidHashKey hashkey;
  bool found;
  bool free_new;

  old_cxt = MemoryContextSwitchTo(state->rs_cxt);

     
                                                                      
     
                                                                            
                                                 
     
  memcpy(&new_tuple->t_data->t_choice.t_heap, &old_tuple->t_data->t_choice.t_heap, sizeof(HeapTupleFields));

  new_tuple->t_data->t_infomask &= ~HEAP_XACT_MASK;
  new_tuple->t_data->t_infomask2 &= ~HEAP2_XACT_MASK;
  new_tuple->t_data->t_infomask |= old_tuple->t_data->t_infomask & HEAP_XACT_MASK;

     
                                                                     
                                                                       
     
  heap_freeze_tuple(new_tuple->t_data, state->rs_old_rel->rd_rel->relfrozenxid, state->rs_old_rel->rd_rel->relminmxid, state->rs_freeze_xid, state->rs_cutoff_multi);

     
                                                                          
                                                                
     
  ItemPointerSetInvalid(&new_tuple->t_data->t_ctid);

     
                                                                             
     
  if (!((old_tuple->t_data->t_infomask & HEAP_XMAX_INVALID) || HeapTupleHeaderIsOnlyLocked(old_tuple->t_data)) && !HeapTupleHeaderIndicatesMovedPartitions(old_tuple->t_data) && !(ItemPointerEquals(&(old_tuple->t_self), &(old_tuple->t_data->t_ctid))))
  {
    OldToNewMapping mapping;

    memset(&hashkey, 0, sizeof(hashkey));
    hashkey.xmin = HeapTupleHeaderGetUpdateXid(old_tuple->t_data);
    hashkey.tid = old_tuple->t_data->t_ctid;

    mapping = (OldToNewMapping)hash_search(state->rs_old_new_tid_map, &hashkey, HASH_FIND, NULL);

    if (mapping != NULL)
    {
         
                                                                         
                                                                      
                               
         
      new_tuple->t_data->t_ctid = mapping->new_tid;

                                                   
      hash_search(state->rs_old_new_tid_map, &hashkey, HASH_REMOVE, &found);
      Assert(found);
    }
    else
    {
         
                                                                    
                                                         
         
      UnresolvedTup unresolved;

      unresolved = hash_search(state->rs_unresolved_tups, &hashkey, HASH_ENTER, &found);
      Assert(!found);

      unresolved->old_tid = old_tuple->t_self;
      unresolved->tuple = heap_copytuple(new_tuple);

         
                                                                      
                                
         
      MemoryContextSwitchTo(old_cxt);
      return;
    }
  }

     
                                                                             
                                                                         
                                                                        
                                                            
     
  old_tid = old_tuple->t_self;
  free_new = false;

  for (;;)
  {
    ItemPointerData new_tid;

                                                                  
    raw_heap_insert(state, new_tuple);
    new_tid = new_tuple->t_self;

    logical_rewrite_heap_tuple(state, old_tid, new_tuple);

       
                                                                           
                                                                      
                                                                         
                                                                           
                                                                  
                                                                       
       
    if ((new_tuple->t_data->t_infomask & HEAP_UPDATED) && !TransactionIdPrecedes(HeapTupleHeaderGetXmin(new_tuple->t_data), state->rs_oldest_xmin))
    {
         
                                                                  
         
      UnresolvedTup unresolved;

      memset(&hashkey, 0, sizeof(hashkey));
      hashkey.xmin = HeapTupleHeaderGetXmin(new_tuple->t_data);
      hashkey.tid = old_tid;

      unresolved = hash_search(state->rs_unresolved_tups, &hashkey, HASH_FIND, NULL);

      if (unresolved != NULL)
      {
           
                                                                      
                                                                      
                                                             
           
        if (free_new)
        {
          heap_freetuple(new_tuple);
        }
        new_tuple = unresolved->tuple;
        free_new = true;
        old_tid = unresolved->old_tid;
        new_tuple->t_data->t_ctid = new_tid;

           
                                                                    
                           
           
        hash_search(state->rs_unresolved_tups, &hashkey, HASH_REMOVE, &found);
        Assert(found);

                                                                 
        continue;
      }
      else
      {
           
                                                                       
                                                              
           
        OldToNewMapping mapping;

        mapping = hash_search(state->rs_old_new_tid_map, &hashkey, HASH_ENTER, &found);
        Assert(!found);

        mapping->new_tid = new_tid;
      }
    }

                                                   
    if (free_new)
    {
      heap_freetuple(new_tuple);
    }
    break;
  }

  MemoryContextSwitchTo(old_cxt);
}

   
                                                                      
                                                                      
                                       
   
                                                                       
                                                                             
                                                                
   
bool
rewrite_heap_dead_tuple(RewriteState state, HeapTuple old_tuple)
{
     
                                                                       
                                                                          
                                                             
                                                                          
                                                             
                                               
     
                                                                         
                                                                           
                                                                           
                                                                          
                                                                            
         
     
  UnresolvedTup unresolved;
  TidHashKey hashkey;
  bool found;

  memset(&hashkey, 0, sizeof(hashkey));
  hashkey.xmin = HeapTupleHeaderGetXmin(old_tuple->t_data);
  hashkey.tid = old_tuple->t_self;

  unresolved = hash_search(state->rs_unresolved_tups, &hashkey, HASH_FIND, NULL);

  if (unresolved != NULL)
  {
                                                                         
    heap_freetuple(unresolved->tuple);
    hash_search(state->rs_unresolved_tups, &hashkey, HASH_REMOVE, &found);
    Assert(found);
    return true;
  }

  return false;
}

   
                                                                      
                                 
   
                                                                            
                                                                         
                                                      
   
static void
raw_heap_insert(RewriteState state, HeapTuple tup)
{
  Page page = state->rs_buffer;
  Size pageFreeSpace, saveFreeSpace;
  Size len;
  OffsetNumber newoff;
  HeapTuple heaptup;

     
                                                                         
                                                                          
     
                                                                             
                                                                     
     
  if (state->rs_new_rel->rd_rel->relkind == RELKIND_TOASTVALUE)
  {
                                                                 
    Assert(!HeapTupleHasExternal(tup));
    heaptup = tup;
  }
  else if (HeapTupleHasExternal(tup) || tup->t_len > TOAST_TUPLE_THRESHOLD)
  {
    int options = HEAP_INSERT_SKIP_FSM;

    if (!state->rs_use_wal)
    {
      options |= HEAP_INSERT_SKIP_WAL;
    }

       
                                                                          
                                                                        
                                                                        
       
    options |= HEAP_INSERT_NO_LOGICAL;

    heaptup = toast_insert_or_update(state->rs_new_rel, tup, NULL, options);
  }
  else
  {
    heaptup = tup;
  }

  len = MAXALIGN(heaptup->t_len);                      

     
                                                              
     
  if (len > MaxHeapTupleSize)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("row is too big: size %zu, maximum size %zu", len, MaxHeapTupleSize)));
  }

                                                                
  saveFreeSpace = RelationGetTargetPageFreeSpace(state->rs_new_rel, HEAP_DEFAULT_FILLFACTOR);

                                                                     
  if (state->rs_buffer_valid)
  {
    pageFreeSpace = PageGetHeapFreeSpace(page);

    if (len + saveFreeSpace > pageFreeSpace)
    {
                                                       

                      
      if (state->rs_use_wal)
      {
        log_newpage(&state->rs_new_rel->rd_node, MAIN_FORKNUM, state->rs_blockno, page, true);
      }

         
                                                                     
                                                                     
                                                        
                           
         
      PageSetChecksumInplace(page, state->rs_blockno);

      smgrextend(RelationGetSmgr(state->rs_new_rel), MAIN_FORKNUM, state->rs_blockno, (char *)page, true);

      state->rs_blockno++;
      state->rs_buffer_valid = false;
    }
  }

  if (!state->rs_buffer_valid)
  {
                                     
    PageInit(page, BLCKSZ, 0);
    state->rs_buffer_valid = true;
  }

                                                     
  newoff = PageAddItem(page, (Item)heaptup->t_data, heaptup->t_len, InvalidOffsetNumber, false, true);
  if (newoff == InvalidOffsetNumber)
  {
    elog(ERROR, "failed to add tuple");
  }

                                                                         
  ItemPointerSet(&(tup->t_self), state->rs_blockno, newoff);

     
                                                                            
                                        
     
  if (!ItemPointerIsValid(&tup->t_data->t_ctid))
  {
    ItemId newitemid;
    HeapTupleHeader onpage_tup;

    newitemid = PageGetItemId(page, newoff);
    onpage_tup = (HeapTupleHeader)PageGetItem(page, newitemid);

    onpage_tup->t_ctid = tup->t_self;
  }

                                                 
  if (heaptup != tup)
  {
    heap_freetuple(heaptup);
  }
}

                                                                            
                           
   
                                                                            
                                                                          
                                                                             
                                                                               
   
                                                                          
                                                                           
                                     
   
                                                                            
                                                                        
                                 
   
                                                                            
                                                                              
                                                         
   
                                                                          
                                                                        
                                                                             
                                                                        
                                                                             
                                                                         
                                                                              
   
                                                                       
                                                                              
                                                                            
                                                                             
                                                                        
                                                                      
                                                                         
                                                                               
                       
   
                                                                              
                                                                              
                                                                               
                                                                               
                                                               
                                                                            
                                                                          
          
                                                                            
   

   
                                                                    
                                                                            
                                                                
   
static void
logical_begin_heap_rewrite(RewriteState state)
{
  HASHCTL hash_ctl;
  TransactionId logical_xmin;

     
                                                                          
                                                                     
                      
     
  state->rs_logical_rewrite = RelationIsAccessibleInLogicalDecoding(state->rs_old_rel);

  if (!state->rs_logical_rewrite)
  {
    return;
  }

  ProcArrayGetReplicationSlotXmin(NULL, &logical_xmin);

     
                                                                             
                                                                          
                                     
     
  if (logical_xmin == InvalidTransactionId)
  {
    state->rs_logical_rewrite = false;
    return;
  }

  state->rs_logical_xmin = logical_xmin;
  state->rs_begin_lsn = GetXLogInsertRecPtr();
  state->rs_num_rewrite_mappings = 0;

  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(TransactionId);
  hash_ctl.entrysize = sizeof(RewriteMappingFile);
  hash_ctl.hcxt = state->rs_cxt;

  state->rs_logical_mappings = hash_create("Logical rewrite mapping", 128,                             
      &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

   
                                                                           
   
static void
logical_heap_rewrite_flush_mappings(RewriteState state)
{
  HASH_SEQ_STATUS seq_status;
  RewriteMappingFile *src;
  dlist_mutable_iter iter;

  Assert(state->rs_logical_rewrite);

                                                                        
  if (state->rs_num_rewrite_mappings == 0)
  {
    return;
  }

  elog(DEBUG1, "flushing %u logical rewrite mapping entries", state->rs_num_rewrite_mappings);

  hash_seq_init(&seq_status, state->rs_logical_mappings);
  while ((src = (RewriteMappingFile *)hash_seq_search(&seq_status)) != NULL)
  {
    char *waldata;
    char *waldata_start;
    xl_heap_rewrite_mapping xlrec;
    Oid dboid;
    uint32 len;
    int written;

                                               
    if (src->num_mappings == 0)
    {
      continue;
    }

    if (state->rs_old_rel->rd_rel->relisshared)
    {
      dboid = InvalidOid;
    }
    else
    {
      dboid = MyDatabaseId;
    }

    xlrec.num_mappings = src->num_mappings;
    xlrec.mapped_rel = RelationGetRelid(state->rs_old_rel);
    xlrec.mapped_xid = src->xid;
    xlrec.mapped_db = dboid;
    xlrec.offset = src->off;
    xlrec.start_lsn = state->rs_begin_lsn;

                                          
    len = src->num_mappings * sizeof(LogicalRewriteMappingData);
    waldata_start = waldata = palloc(len);

       
                                                                           
       
    dlist_foreach_modify(iter, &src->mappings)
    {
      RewriteMappingDataEntry *pmap;

      pmap = dlist_container(RewriteMappingDataEntry, node, iter.cur);

      memcpy(waldata, &pmap->map, sizeof(pmap->map));
      waldata += sizeof(pmap->map);

                                         
      dlist_delete(&pmap->node);
      pfree(pmap);

                              
      state->rs_num_rewrite_mappings--;
      src->num_mappings--;
    }

    Assert(src->num_mappings == 0);
    Assert(waldata == waldata_start + len);

       
                                                                      
                                                                        
       
    written = FileWrite(src->vfd, waldata_start, len, src->off, WAIT_EVENT_LOGICAL_REWRITE_WRITE);
    if (written != len)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\", wrote %d of %d: %m", src->path, written, len)));
    }
    src->off += len;

    XLogBeginInsert();
    XLogRegisterData((char *)(&xlrec), sizeof(xlrec));
    XLogRegisterData(waldata_start, len);

                           
    XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_REWRITE);

    pfree(waldata_start);
  }
  Assert(state->rs_num_rewrite_mappings == 0);
}

   
                                                 
   
static void
logical_end_heap_rewrite(RewriteState state)
{
  HASH_SEQ_STATUS seq_status;
  RewriteMappingFile *src;

                                            
  if (!state->rs_logical_rewrite)
  {
    return;
  }

                                            
  if (state->rs_num_rewrite_mappings > 0)
  {
    logical_heap_rewrite_flush_mappings(state);
  }

                                                                      
  hash_seq_init(&seq_status, state->rs_logical_mappings);
  while ((src = (RewriteMappingFile *)hash_seq_search(&seq_status)) != NULL)
  {
    if (FileSync(src->vfd, WAIT_EVENT_LOGICAL_REWRITE_SYNC) != 0)
    {
      ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", src->path)));
    }
    FileClose(src->vfd);
  }
                                                      
}

   
                                              
   
static void
logical_rewrite_log_mapping(RewriteState state, TransactionId xid, LogicalRewriteMappingData *map)
{
  RewriteMappingFile *src;
  RewriteMappingDataEntry *pmap;
  Oid relid;
  bool found;

  relid = RelationGetRelid(state->rs_old_rel);

                                                        
  src = hash_search(state->rs_logical_mappings, &xid, HASH_ENTER, &found);

     
                                                                      
                              
     
  if (!found)
  {
    char path[MAXPGPATH];
    Oid dboid;

    if (state->rs_old_rel->rd_rel->relisshared)
    {
      dboid = InvalidOid;
    }
    else
    {
      dboid = MyDatabaseId;
    }

    snprintf(path, MAXPGPATH, "pg_logical/mappings/" LOGICAL_REWRITE_FORMAT, dboid, relid, (uint32)(state->rs_begin_lsn >> 32), (uint32)state->rs_begin_lsn, xid, GetCurrentTransactionId());

    dlist_init(&src->mappings);
    src->num_mappings = 0;
    src->off = 0;
    memcpy(src->path, path, sizeof(path));
    src->vfd = PathNameOpenFile(path, O_CREAT | O_EXCL | O_WRONLY | PG_BINARY);
    if (src->vfd < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", path)));
    }
  }

  pmap = MemoryContextAlloc(state->rs_cxt, sizeof(RewriteMappingDataEntry));
  memcpy(&pmap->map, map, sizeof(LogicalRewriteMappingData));
  dlist_push_tail(&src->mappings, &pmap->node);
  src->num_mappings++;
  state->rs_num_rewrite_mappings++;

     
                                                                             
                    
     
  if (state->rs_num_rewrite_mappings >= 1000                       )
  {
    logical_heap_rewrite_flush_mappings(state);
  }
}

   
                                                                       
                                                                         
   
static void
logical_rewrite_heap_tuple(RewriteState state, ItemPointerData old_tid, HeapTuple new_tuple)
{
  ItemPointerData new_tid = new_tuple->t_self;
  TransactionId cutoff = state->rs_logical_xmin;
  TransactionId xmin;
  TransactionId xmax;
  bool do_log_xmin = false;
  bool do_log_xmax = false;
  LogicalRewriteMappingData map;

                                                                     
  if (!state->rs_logical_rewrite)
  {
    return;
  }

  xmin = HeapTupleHeaderGetXmin(new_tuple->t_data);
                                                           
  xmax = HeapTupleHeaderGetUpdateXid(new_tuple->t_data);

     
                                                              
     
  if (TransactionIdIsNormal(xmin) && !TransactionIdPrecedes(xmin, cutoff))
  {
    do_log_xmin = true;
  }

  if (!TransactionIdIsNormal(xmax))
  {
       
                                                                       
                  
       
  }
  else if (HEAP_XMAX_IS_LOCKED_ONLY(new_tuple->t_data->t_infomask))
  {
                                    
  }
  else if (!TransactionIdPrecedes(xmax, cutoff))
  {
                                              
    do_log_xmax = true;
  }

                                                 
  if (!do_log_xmin && !do_log_xmax)
  {
    return;
  }

                                    
  map.old_node = state->rs_old_rel->rd_node;
  map.old_tid = old_tid;
  map.new_node = state->rs_new_rel->rd_node;
  map.new_tid = new_tid;

         
                                                                           
                                                                            
                                                     
                                                                          
                                                                             
                                                                       
                                                                         
                                          
         
     
  if (do_log_xmin)
  {
    logical_rewrite_log_mapping(state, xmin, &map);
  }
                                                                
  if (do_log_xmax && !TransactionIdEquals(xmin, xmax))
  {
    logical_rewrite_log_mapping(state, xmax, &map);
  }
}

   
                                     
   
void
heap_xlog_logical_rewrite(XLogReaderState *r)
{
  char path[MAXPGPATH];
  int fd;
  xl_heap_rewrite_mapping *xlrec;
  uint32 len;
  char *data;

  xlrec = (xl_heap_rewrite_mapping *)XLogRecGetData(r);

  snprintf(path, MAXPGPATH, "pg_logical/mappings/" LOGICAL_REWRITE_FORMAT, xlrec->mapped_db, xlrec->mapped_rel, (uint32)(xlrec->start_lsn >> 32), (uint32)xlrec->start_lsn, xlrec->mapped_xid, XLogRecGetXid(r));

  fd = OpenTransientFile(path, O_CREAT | O_WRONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", path)));
  }

     
                                                                             
                                                 
     
  pgstat_report_wait_start(WAIT_EVENT_LOGICAL_REWRITE_TRUNCATE);
  if (ftruncate(fd, xlrec->offset) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not truncate file \"%s\" to %u: %m", path, (uint32)xlrec->offset)));
  }
  pgstat_report_wait_end();

                                                             
  if (lseek(fd, xlrec->offset, SEEK_SET) != xlrec->offset)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek to end of file \"%s\": %m", path)));
  }

  data = XLogRecGetData(r) + sizeof(*xlrec);

  len = xlrec->num_mappings * sizeof(LogicalRewriteMappingData);

                                                  
  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_LOGICAL_REWRITE_MAPPING_WRITE);
  if (write(fd, data, len) != len)
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", path)));
  }
  pgstat_report_wait_end();

     
                                                                             
                                                                        
                                     
     
  pgstat_report_wait_start(WAIT_EVENT_LOGICAL_REWRITE_MAPPING_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", path)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }
}

       
                                                     
   
                          
                                                                              
                                                                              
                                                                             
                                   
       
   
void
CheckPointLogicalRewriteHeap(void)
{
  XLogRecPtr cutoff;
  XLogRecPtr redo;
  DIR *mappings_dir;
  struct dirent *mapping_de;
  char path[MAXPGPATH + 20];

     
                                                                          
                                                                            
     
  redo = GetRedoRecPtr();

                                                          
  cutoff = ReplicationSlotsComputeLogicalRestartLSN();

                                                
  if (cutoff != InvalidXLogRecPtr && redo < cutoff)
  {
    cutoff = redo;
  }

  mappings_dir = AllocateDir("pg_logical/mappings");
  while ((mapping_de = ReadDir(mappings_dir, "pg_logical/mappings")) != NULL)
  {
    struct stat statbuf;
    Oid dboid;
    Oid relid;
    XLogRecPtr lsn;
    TransactionId rewrite_xid;
    TransactionId create_xid;
    uint32 hi, lo;

    if (strcmp(mapping_de->d_name, ".") == 0 || strcmp(mapping_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(path, sizeof(path), "pg_logical/mappings/%s", mapping_de->d_name);
    if (lstat(path, &statbuf) == 0 && !S_ISREG(statbuf.st_mode))
    {
      continue;
    }

                                              
    if (strncmp(mapping_de->d_name, "map-", 4) != 0)
    {
      continue;
    }

    if (sscanf(mapping_de->d_name, LOGICAL_REWRITE_FORMAT, &dboid, &relid, &hi, &lo, &rewrite_xid, &create_xid) != 6)
    {
      elog(ERROR, "could not parse filename \"%s\"", mapping_de->d_name);
    }

    lsn = ((uint64)hi) << 32 | lo;

    if (lsn < cutoff || cutoff == InvalidXLogRecPtr)
    {
      elog(DEBUG1, "removing logical rewrite file \"%s\"", path);
      if (unlink(path) < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
      }
    }
    else
    {
                                                                     
      int fd = OpenTransientFile(path, O_RDWR | PG_BINARY);

         
                                                                       
                                                                      
                                             
         
      if (fd < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
      }

         
                                                                  
                                                                         
                                                         
         
      pgstat_report_wait_start(WAIT_EVENT_LOGICAL_REWRITE_CHECKPOINT_SYNC);
      if (pg_fsync(fd) != 0)
      {
        ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", path)));
      }
      pgstat_report_wait_end();

      if (CloseTransientFile(fd))
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
      }
    }
  }
  FreeDir(mappings_dir);

                                         
  fsync_fname("pg_logical/mappings", true);
}
