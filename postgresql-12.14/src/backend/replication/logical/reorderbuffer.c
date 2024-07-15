                                                                            
   
                   
                                                         
   
   
                                                                
   
   
                  
                                             
   
         
                                                                            
                                                                            
                                                                         
                                                                           
                                                                           
                                                                       
                                         
   
                                                                     
                                                                  
                                                                             
                                                                             
                                                                              
                                                                               
                                                                              
                                                                               
                                                                           
                                         
   
                                                                              
                                                                              
                                                                           
                                                                         
             
   
                                                                           
                                                                           
                                                                           
                                                                          
                                                                              
                                                                       
              
   
                                                                           
                                                                            
                                                                           
                                                 
   
                                                                             
   
#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "access/heapam.h"
#include "access/rewriteheap.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "catalog/catalog.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/logical.h"
#include "replication/reorderbuffer.h"
#include "replication/slot.h"
#include "replication/snapbuild.h"                                        
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/sinval.h"
#include "utils/builtins.h"
#include "utils/combocid.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/relfilenodemap.h"

                                                                            
typedef struct ReorderBufferTXNByIdEnt
{
  TransactionId xid;
  ReorderBufferTXN *txn;
} ReorderBufferTXNByIdEnt;

                                                                     
typedef struct ReorderBufferTupleCidKey
{
  RelFileNode relnode;
  ItemPointerData tid;
} ReorderBufferTupleCidKey;

typedef struct ReorderBufferTupleCidEnt
{
  ReorderBufferTupleCidKey key;
  CommandId cmin;
  CommandId cmax;
  CommandId combocid;                         
} ReorderBufferTupleCidEnt;

                                                       
typedef struct TXNEntryFile
{
  File vfd;                                        
  off_t curOffset;                                              
                                            
} TXNEntryFile;

                                                        
typedef struct ReorderBufferIterTXNEntry
{
  XLogRecPtr lsn;
  ReorderBufferChange *change;
  ReorderBufferTXN *txn;
  TXNEntryFile file;
  XLogSegNo segno;
} ReorderBufferIterTXNEntry;

typedef struct ReorderBufferIterTXNState
{
  binaryheap *heap;
  Size nr_txns;
  dlist_head old_change;
  ReorderBufferIterTXNEntry entries[FLEXIBLE_ARRAY_MEMBER];
} ReorderBufferIterTXNState;

                          
typedef struct ReorderBufferToastEnt
{
  Oid chunk_id;                                            
  int32 last_chunk_seq;                                                        
                                                
  Size num_chunks;                                                        
  Size size;                                                       
  dlist_head chunks;                                        
  struct varlena *reconstructed;                                            
                                               
} ReorderBufferToastEnt;

                                               
typedef struct ReorderBufferDiskChange
{
  Size size;
  ReorderBufferChange change;
                    
} ReorderBufferDiskChange;

   
                                                                          
                                
   
                                                                           
                                                                              
                                    
   
                                                                                
                                                                              
         
   
static const Size max_changes_in_memory = 4096;

                                           
                                          
                                           
   
static ReorderBufferTXN *
ReorderBufferGetTXN(ReorderBuffer *rb);
static void
ReorderBufferReturnTXN(ReorderBuffer *rb, ReorderBufferTXN *txn);
static ReorderBufferTXN *
ReorderBufferTXNByXid(ReorderBuffer *rb, TransactionId xid, bool create, bool *is_new, XLogRecPtr lsn, bool create_as_top);
static void
ReorderBufferTransferSnapToParent(ReorderBufferTXN *txn, ReorderBufferTXN *subtxn);

static void
AssertTXNLsnOrder(ReorderBuffer *rb);

                                           
                                                                     
                                       
   
                                                                         
                   
                                           
   
static void
ReorderBufferIterTXNInit(ReorderBuffer *rb, ReorderBufferTXN *txn, ReorderBufferIterTXNState *volatile *iter_state);
static ReorderBufferChange *
ReorderBufferIterTXNNext(ReorderBuffer *rb, ReorderBufferIterTXNState *state);
static void
ReorderBufferIterTXNFinish(ReorderBuffer *rb, ReorderBufferIterTXNState *state);
static void
ReorderBufferExecuteInvalidations(ReorderBuffer *rb, ReorderBufferTXN *txn);

   
                                           
                                        
                                           
   
static void
ReorderBufferCheckSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferSerializeChange(ReorderBuffer *rb, ReorderBufferTXN *txn, int fd, ReorderBufferChange *change);
static Size
ReorderBufferRestoreChanges(ReorderBuffer *rb, ReorderBufferTXN *txn, TXNEntryFile *file, XLogSegNo *segno);
static void
ReorderBufferRestoreChange(ReorderBuffer *rb, ReorderBufferTXN *txn, char *change);
static void
ReorderBufferRestoreCleanup(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferCleanupSerializedTXNs(const char *slotname);
static void
ReorderBufferSerializedPath(char *path, ReplicationSlot *slot, TransactionId xid, XLogSegNo segno);

static void
ReorderBufferFreeSnap(ReorderBuffer *rb, Snapshot snap);
static Snapshot
ReorderBufferCopySnap(ReorderBuffer *rb, Snapshot orig_snap, ReorderBufferTXN *txn, CommandId cid);

                                           
                            
                                           
   
static void
ReorderBufferToastInitHash(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferToastReset(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferToastReplace(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change);
static void
ReorderBufferToastAppendChunk(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change);

   
                                                                            
                                                    
   
ReorderBuffer *
ReorderBufferAllocate(void)
{
  ReorderBuffer *buffer;
  HASHCTL hash_ctl;
  MemoryContext new_ctx;

  Assert(MyReplicationSlot != NULL);

                                                                     
  new_ctx = AllocSetContextCreate(CurrentMemoryContext, "ReorderBuffer", ALLOCSET_DEFAULT_SIZES);

  buffer = (ReorderBuffer *)MemoryContextAlloc(new_ctx, sizeof(ReorderBuffer));

  memset(&hash_ctl, 0, sizeof(hash_ctl));

  buffer->context = new_ctx;

  buffer->change_context = SlabContextCreate(new_ctx, "Change", SLAB_DEFAULT_BLOCK_SIZE, sizeof(ReorderBufferChange));

  buffer->txn_context = SlabContextCreate(new_ctx, "TXN", SLAB_DEFAULT_BLOCK_SIZE, sizeof(ReorderBufferTXN));

  buffer->tup_context = GenerationContextCreate(new_ctx, "Tuples", SLAB_LARGE_BLOCK_SIZE);

  hash_ctl.keysize = sizeof(TransactionId);
  hash_ctl.entrysize = sizeof(ReorderBufferTXNByIdEnt);
  hash_ctl.hcxt = buffer->context;

  buffer->by_txn = hash_create("ReorderBufferByXid", 1000, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  buffer->by_txn_last_xid = InvalidTransactionId;
  buffer->by_txn_last_txn = NULL;

  buffer->outbuf = NULL;
  buffer->outbufsize = 0;

  buffer->current_restart_decoding_lsn = InvalidXLogRecPtr;

  dlist_init(&buffer->toplevel_by_lsn);
  dlist_init(&buffer->txns_by_base_snapshot_lsn);

     
                                                                             
                                                                          
                                                                            
     
  ReorderBufferCleanupSerializedTXNs(NameStr(MyReplicationSlot->data.name));

  return buffer;
}

   
                        
   
void
ReorderBufferFree(ReorderBuffer *rb)
{
  MemoryContext context = rb->context;

     
                                                                             
                     
     
  MemoryContextDelete(context);

                                                          
  ReorderBufferCleanupSerializedTXNs(NameStr(MyReplicationSlot->data.name));
}

   
                                                           
   
static ReorderBufferTXN *
ReorderBufferGetTXN(ReorderBuffer *rb)
{
  ReorderBufferTXN *txn;

  txn = (ReorderBufferTXN *)MemoryContextAlloc(rb->txn_context, sizeof(ReorderBufferTXN));

  memset(txn, 0, sizeof(ReorderBufferTXN));

  dlist_init(&txn->changes);
  dlist_init(&txn->tuplecids);
  dlist_init(&txn->subtxns);

  return txn;
}

   
                            
   
static void
ReorderBufferReturnTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
                                                               
  if (rb->by_txn_last_xid == txn->xid)
  {
    rb->by_txn_last_xid = InvalidTransactionId;
    rb->by_txn_last_txn = NULL;
  }

                                  

  if (txn->tuplecid_hash != NULL)
  {
    hash_destroy(txn->tuplecid_hash);
    txn->tuplecid_hash = NULL;
  }

  if (txn->invalidations)
  {
    pfree(txn->invalidations);
    txn->invalidations = NULL;
  }

                            
  ReorderBufferToastReset(rb, txn);

  pfree(txn);
}

   
                                     
   
ReorderBufferChange *
ReorderBufferGetChange(ReorderBuffer *rb)
{
  ReorderBufferChange *change;

  change = (ReorderBufferChange *)MemoryContextAlloc(rb->change_context, sizeof(ReorderBufferChange));

  memset(change, 0, sizeof(ReorderBufferChange));
  return change;
}

   
                                
   
void
ReorderBufferReturnChange(ReorderBuffer *rb, ReorderBufferChange *change)
{
                           
  switch (change->action)
  {
  case REORDER_BUFFER_CHANGE_INSERT:
  case REORDER_BUFFER_CHANGE_UPDATE:
  case REORDER_BUFFER_CHANGE_DELETE:
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_INSERT:
    if (change->data.tp.newtuple)
    {
      ReorderBufferReturnTupleBuf(rb, change->data.tp.newtuple);
      change->data.tp.newtuple = NULL;
    }

    if (change->data.tp.oldtuple)
    {
      ReorderBufferReturnTupleBuf(rb, change->data.tp.oldtuple);
      change->data.tp.oldtuple = NULL;
    }
    break;
  case REORDER_BUFFER_CHANGE_MESSAGE:
    if (change->data.msg.prefix != NULL)
    {
      pfree(change->data.msg.prefix);
    }
    change->data.msg.prefix = NULL;
    if (change->data.msg.message != NULL)
    {
      pfree(change->data.msg.message);
    }
    change->data.msg.message = NULL;
    break;
  case REORDER_BUFFER_CHANGE_INTERNAL_SNAPSHOT:
    if (change->data.snapshot)
    {
      ReorderBufferFreeSnap(rb, change->data.snapshot);
      change->data.snapshot = NULL;
    }
    break;
                                                  
  case REORDER_BUFFER_CHANGE_TRUNCATE:
    if (change->data.truncate.relids != NULL)
    {
      ReorderBufferReturnRelids(rb, change->data.truncate.relids);
      change->data.truncate.relids = NULL;
    }
    break;
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_CONFIRM:
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_ABORT:
  case REORDER_BUFFER_CHANGE_INTERNAL_COMMAND_ID:
  case REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID:
    break;
  }

  pfree(change);
}

   
                                                                      
                                          
   
ReorderBufferTupleBuf *
ReorderBufferGetTupleBuf(ReorderBuffer *rb, Size tuple_len)
{
  ReorderBufferTupleBuf *tuple;
  Size alloc_len;

  alloc_len = tuple_len + SizeofHeapTupleHeader;

  tuple = (ReorderBufferTupleBuf *)MemoryContextAlloc(rb->tup_context, sizeof(ReorderBufferTupleBuf) + MAXIMUM_ALIGNOF + alloc_len);
  tuple->alloc_tuple_size = alloc_len;
  tuple->tuple.t_data = ReorderBufferTupleBufData(tuple);

  return tuple;
}

   
                                  
   
void
ReorderBufferReturnTupleBuf(ReorderBuffer *rb, ReorderBufferTupleBuf *tuple)
{
  pfree(tuple);
}

   
                                                   
   
                                                                            
                                                                           
                                                                             
                                                                              
                                                                    
   
Oid *
ReorderBufferGetRelids(ReorderBuffer *rb, int nrelids)
{
  Oid *relids;
  Size alloc_len;

  alloc_len = sizeof(Oid) * nrelids;

  relids = (Oid *)MemoryContextAlloc(rb->context, alloc_len);

  return relids;
}

   
                            
   
void
ReorderBufferReturnRelids(ReorderBuffer *rb, Oid *relids)
{
  pfree(relids);
}

   
                                                                        
                                                                         
                                                                     
                                             
   
static ReorderBufferTXN *
ReorderBufferTXNByXid(ReorderBuffer *rb, TransactionId xid, bool create, bool *is_new, XLogRecPtr lsn, bool create_as_top)
{
  ReorderBufferTXN *txn;
  ReorderBufferTXNByIdEnt *ent;
  bool found;

  Assert(TransactionIdIsValid(xid));

     
                                            
     
  if (TransactionIdIsValid(rb->by_txn_last_xid) && rb->by_txn_last_xid == xid)
  {
    txn = rb->by_txn_last_txn;

    if (txn != NULL)
    {
                                    
      if (is_new)
      {
        *is_new = false;
      }
      return txn;
    }

       
                                                                          
              
       
    if (!create)
    {
      return NULL;
    }
                                             
  }

     
                                                                           
                         
     

                               
  ent = (ReorderBufferTXNByIdEnt *)hash_search(rb->by_txn, (void *)&xid, create ? HASH_ENTER : HASH_FIND, &found);
  if (found)
  {
    txn = ent->txn;
  }
  else if (create)
  {
                                                             
    Assert(ent != NULL);
    Assert(lsn != InvalidXLogRecPtr);

    ent->txn = ReorderBufferGetTXN(rb);
    ent->txn->xid = xid;
    txn = ent->txn;
    txn->first_lsn = lsn;
    txn->restart_decoding_lsn = rb->current_restart_decoding_lsn;

    if (create_as_top)
    {
      dlist_push_tail(&rb->toplevel_by_lsn, &txn->node);
      AssertTXNLsnOrder(rb);
    }
  }
  else
  {
    txn = NULL;                                        
  }

                    
  rb->by_txn_last_xid = xid;
  rb->by_txn_last_txn = txn;

  if (is_new)
  {
    *is_new = !found;
  }

  Assert(!create || txn != NULL);
  return txn;
}

   
                                                                        
   
void
ReorderBufferQueueChange(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, ReorderBufferChange *change)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, true, NULL, lsn, true);

  change->lsn = lsn;
  Assert(InvalidXLogRecPtr != lsn);
  dlist_push_tail(&txn->changes, &change->node);
  txn->nentries++;
  txn->nentries_mem++;

  ReorderBufferCheckSerializeTXN(rb, txn);
}

   
                                                                        
   
void
ReorderBufferQueueMessage(ReorderBuffer *rb, TransactionId xid, Snapshot snapshot, XLogRecPtr lsn, bool transactional, const char *prefix, Size message_size, const char *message)
{
  if (transactional)
  {
    MemoryContext oldcontext;
    ReorderBufferChange *change;

    Assert(xid != InvalidTransactionId);

    oldcontext = MemoryContextSwitchTo(rb->context);

    change = ReorderBufferGetChange(rb);
    change->action = REORDER_BUFFER_CHANGE_MESSAGE;
    change->data.msg.prefix = pstrdup(prefix);
    change->data.msg.message_size = message_size;
    change->data.msg.message = palloc(message_size);
    memcpy(change->data.msg.message, message, message_size);

    ReorderBufferQueueChange(rb, xid, lsn, change);

    MemoryContextSwitchTo(oldcontext);
  }
  else
  {
    ReorderBufferTXN *txn = NULL;
    volatile Snapshot snapshot_now = snapshot;

    if (xid != InvalidTransactionId)
    {
      txn = ReorderBufferTXNByXid(rb, xid, true, NULL, lsn, true);
    }

                                                
    SetupHistoricSnapshot(snapshot_now, NULL);
    PG_TRY();
    {
      rb->message(rb, txn, lsn, false, prefix, message_size, message);

      TeardownHistoricSnapshot(false);
    }
    PG_CATCH();
    {
      TeardownHistoricSnapshot(true);
      PG_RE_THROW();
    }
    PG_END_TRY();
  }
}

   
                     
                                                                  
   
                                                 
   
                                       
   
static void
AssertTXNLsnOrder(ReorderBuffer *rb)
{
#ifdef USE_ASSERT_CHECKING
  LogicalDecodingContext *ctx = rb->private_data;
  dlist_iter iter;
  XLogRecPtr prev_first_lsn = InvalidXLogRecPtr;
  XLogRecPtr prev_base_snap_lsn = InvalidXLogRecPtr;

     
                                                                       
                                                                          
                                                                             
                                                                            
                                                                        
                                                                            
                                                                            
              
     
  if (SnapBuildXactNeedsSkip(ctx->snapshot_builder, ctx->reader->EndRecPtr))
  {
    return;
  }

  dlist_foreach(iter, &rb->toplevel_by_lsn)
  {
    ReorderBufferTXN *cur_txn = dlist_container(ReorderBufferTXN, node, iter.cur);

                               
    Assert(cur_txn->first_lsn != InvalidXLogRecPtr);

                                                                  
    if (cur_txn->end_lsn != InvalidXLogRecPtr)
    {
      Assert(cur_txn->first_lsn <= cur_txn->end_lsn);
    }

                                                                   
    if (prev_first_lsn != InvalidXLogRecPtr)
    {
      Assert(prev_first_lsn < cur_txn->first_lsn);
    }

                                                 
    Assert(!cur_txn->is_known_as_subxact);

    prev_first_lsn = cur_txn->first_lsn;
  }

  dlist_foreach(iter, &rb->txns_by_base_snapshot_lsn)
  {
    ReorderBufferTXN *cur_txn = dlist_container(ReorderBufferTXN, base_snapshot_node, iter.cur);

                                                 
    Assert(cur_txn->base_snapshot != NULL);
    Assert(cur_txn->base_snapshot_lsn != InvalidXLogRecPtr);

                                                           
    if (prev_base_snap_lsn != InvalidXLogRecPtr)
    {
      Assert(prev_base_snap_lsn < cur_txn->base_snapshot_lsn);
    }

                                                 
    Assert(!cur_txn->is_known_as_subxact);

    prev_base_snap_lsn = cur_txn->base_snapshot_lsn;
  }
#endif
}

   
                             
                                               
   
ReorderBufferTXN *
ReorderBufferGetOldestTXN(ReorderBuffer *rb)
{
  ReorderBufferTXN *txn;

  AssertTXNLsnOrder(rb);

  if (dlist_is_empty(&rb->toplevel_by_lsn))
  {
    return NULL;
  }

  txn = dlist_head_element(ReorderBufferTXN, node, &rb->toplevel_by_lsn);

  Assert(!txn->is_known_as_subxact);
  Assert(txn->first_lsn != InvalidXLogRecPtr);
  return txn;
}

   
                              
                                        
   
                                                                           
                                                                              
                   
   
                                                                           
                                                 
   
TransactionId
ReorderBufferGetOldestXmin(ReorderBuffer *rb)
{
  ReorderBufferTXN *txn;

  AssertTXNLsnOrder(rb);

  if (dlist_is_empty(&rb->txns_by_base_snapshot_lsn))
  {
    return InvalidTransactionId;
  }

  txn = dlist_head_element(ReorderBufferTXN, base_snapshot_node, &rb->txns_by_base_snapshot_lsn);
  return txn->base_snapshot->xmin;
}

void
ReorderBufferSetRestartPoint(ReorderBuffer *rb, XLogRecPtr ptr)
{
  rb->current_restart_decoding_lsn = ptr;
}

   
                            
   
                                                                             
                  
   
void
ReorderBufferAssignChild(ReorderBuffer *rb, TransactionId xid, TransactionId subxid, XLogRecPtr lsn)
{
  ReorderBufferTXN *txn;
  ReorderBufferTXN *subtxn;
  bool new_top;
  bool new_sub;

  txn = ReorderBufferTXNByXid(rb, xid, true, &new_top, lsn, true);
  subtxn = ReorderBufferTXNByXid(rb, subxid, true, &new_sub, lsn, false);

  if (!new_sub)
  {
    if (subtxn->is_known_as_subxact)
    {
                                             
      return;
    }
    else
    {
         
                                                                        
                                                                       
                               
         
      dlist_delete(&subtxn->node);
    }
  }

  subtxn->is_known_as_subxact = true;
  subtxn->toplevel_xid = xid;
  Assert(subtxn->nsubtxns == 0);

                                  
  dlist_push_tail(&txn->subtxns, &subtxn->node);
  txn->nsubtxns++;

                                                                     
  ReorderBufferTransferSnapToParent(txn, subtxn);

                                     
  AssertTXNLsnOrder(rb);
}

   
                                     
                                                                   
   
                                                                             
                                                                           
                                                                            
                                                                           
                                                                           
                           
   
                                                                       
                                                            
   
                                                                             
                                                                        
                              
   
static void
ReorderBufferTransferSnapToParent(ReorderBufferTXN *txn, ReorderBufferTXN *subtxn)
{
  Assert(subtxn->toplevel_xid == txn->xid);

  if (subtxn->base_snapshot != NULL)
  {
    if (txn->base_snapshot == NULL || subtxn->base_snapshot_lsn < txn->base_snapshot_lsn)
    {
         
                                                                     
                                                  
         
      if (txn->base_snapshot != NULL)
      {
        SnapBuildSnapDecRefcount(txn->base_snapshot);
        dlist_delete(&txn->base_snapshot_node);
      }

         
                                                                     
                                                                        
                                                   
         
      txn->base_snapshot = subtxn->base_snapshot;
      txn->base_snapshot_lsn = subtxn->base_snapshot_lsn;
      dlist_insert_before(&subtxn->base_snapshot_node, &txn->base_snapshot_node);

         
                                                                   
                                  
         
      subtxn->base_snapshot = NULL;
      subtxn->base_snapshot_lsn = InvalidXLogRecPtr;
      dlist_delete(&subtxn->base_snapshot_node);
    }
    else
    {
                                                                     
      SnapBuildSnapDecRefcount(subtxn->base_snapshot);
      dlist_delete(&subtxn->base_snapshot_node);
      subtxn->base_snapshot = NULL;
      subtxn->base_snapshot_lsn = InvalidXLogRecPtr;
    }
  }
}

   
                                                                      
                                                           
   
void
ReorderBufferCommitChild(ReorderBuffer *rb, TransactionId xid, TransactionId subxid, XLogRecPtr commit_lsn, XLogRecPtr end_lsn)
{
  ReorderBufferTXN *subtxn;

  subtxn = ReorderBufferTXNByXid(rb, subxid, false, NULL, InvalidXLogRecPtr, false);

     
                                                                      
     
  if (!subtxn)
  {
    return;
  }

  subtxn->final_lsn = commit_lsn;
  subtxn->end_lsn = end_lsn;

     
                                                                           
            
     
  ReorderBufferAssignChild(rb, xid, subxid, InvalidXLogRecPtr);
}

   
                                                                  
                             
   
                                                                               
                                                                             
                                                                              
             
   
                                                                               
   

   
                                    
   
static int
ReorderBufferIterCompare(Datum a, Datum b, void *arg)
{
  ReorderBufferIterTXNState *state = (ReorderBufferIterTXNState *)arg;
  XLogRecPtr pos_a = state->entries[DatumGetInt32(a)].lsn;
  XLogRecPtr pos_b = state->entries[DatumGetInt32(b)].lsn;

  if (pos_a < pos_b)
  {
    return 1;
  }
  else if (pos_a == pos_b)
  {
    return 0;
  }
  return -1;
}

   
                                                                        
                                            
   
                                                                            
                                                                                
                                                                              
                                                             
   
static void
ReorderBufferIterTXNInit(ReorderBuffer *rb, ReorderBufferTXN *txn, ReorderBufferIterTXNState *volatile *iter_state)
{
  Size nr_txns = 0;
  ReorderBufferIterTXNState *state;
  dlist_iter cur_txn_i;
  int32 off;

  *iter_state = NULL;

     
                                                                            
                                                                         
                                                        
     
  if (txn->nentries > 0)
  {
    nr_txns++;
  }

  dlist_foreach(cur_txn_i, &txn->subtxns)
  {
    ReorderBufferTXN *cur_txn;

    cur_txn = dlist_container(ReorderBufferTXN, node, cur_txn_i.cur);

    if (cur_txn->nentries > 0)
    {
      nr_txns++;
    }
  }

     
                                                                             
                                         
     

                                
  state = (ReorderBufferIterTXNState *)MemoryContextAllocZero(rb->context, sizeof(ReorderBufferIterTXNState) + sizeof(ReorderBufferIterTXNEntry) * nr_txns);

  state->nr_txns = nr_txns;
  dlist_init(&state->old_change);

  for (off = 0; off < state->nr_txns; off++)
  {
    state->entries[off].file.vfd = -1;
    state->entries[off].segno = 0;
  }

                     
  state->heap = binaryheap_allocate(state->nr_txns, ReorderBufferIterCompare, state);

                                                                           
  *iter_state = state;

     
                                                                          
                                                                        
     

  off = 0;

                                                       
  if (txn->nentries > 0)
  {
    ReorderBufferChange *cur_change;

    if (txn->serialized)
    {
                                       
      ReorderBufferSerializeTXN(rb, txn);
      ReorderBufferRestoreChanges(rb, txn, &state->entries[off].file, &state->entries[off].segno);
    }

    cur_change = dlist_head_element(ReorderBufferChange, node, &txn->changes);

    state->entries[off].lsn = cur_change->lsn;
    state->entries[off].change = cur_change;
    state->entries[off].txn = txn;

    binaryheap_add_unordered(state->heap, Int32GetDatum(off++));
  }

                                                   
  dlist_foreach(cur_txn_i, &txn->subtxns)
  {
    ReorderBufferTXN *cur_txn;

    cur_txn = dlist_container(ReorderBufferTXN, node, cur_txn_i.cur);

    if (cur_txn->nentries > 0)
    {
      ReorderBufferChange *cur_change;

      if (cur_txn->serialized)
      {
                                         
        ReorderBufferSerializeTXN(rb, cur_txn);
        ReorderBufferRestoreChanges(rb, cur_txn, &state->entries[off].file, &state->entries[off].segno);
      }
      cur_change = dlist_head_element(ReorderBufferChange, node, &cur_txn->changes);

      state->entries[off].lsn = cur_change->lsn;
      state->entries[off].change = cur_change;
      state->entries[off].txn = cur_txn;

      binaryheap_add_unordered(state->heap, Int32GetDatum(off++));
    }
  }

                                    
  binaryheap_build(state->heap);
}

   
                                                                    
                    
   
                                               
   
static ReorderBufferChange *
ReorderBufferIterTXNNext(ReorderBuffer *rb, ReorderBufferIterTXNState *state)
{
  ReorderBufferChange *change;
  ReorderBufferIterTXNEntry *entry;
  int32 off;

                             
  if (state->heap->bh_size == 0)
  {
    return NULL;
  }

  off = DatumGetInt32(binaryheap_first(state->heap));
  entry = &state->entries[off];

                                                                     
  if (!dlist_is_empty(&state->old_change))
  {
    change = dlist_container(ReorderBufferChange, node, dlist_pop_head_node(&state->old_change));
    ReorderBufferReturnChange(rb, change);
    Assert(dlist_is_empty(&state->old_change));
  }

  change = entry->change;

     
                                                                       
                                  
     

                                   
  if (dlist_has_next(&entry->txn->changes, &entry->change->node))
  {
    dlist_node *next = dlist_next_node(&entry->txn->changes, &change->node);
    ReorderBufferChange *next_change = dlist_container(ReorderBufferChange, node, next);

                            
    state->entries[off].lsn = next_change->lsn;
    state->entries[off].change = next_change;

    binaryheap_replace_first(state->heap, Int32GetDatum(off));
    return change;
  }

                                     
  if (entry->txn->nentries != entry->txn->nentries_mem)
  {
       
                                                                           
                                                                        
       
    dlist_delete(&change->node);
    dlist_push_tail(&state->old_change, &change->node);

    if (ReorderBufferRestoreChanges(rb, entry->txn, &entry->file, &state->entries[off].segno))
    {
                                                   
      ReorderBufferChange *next_change = dlist_head_element(ReorderBufferChange, node, &entry->txn->changes);

      elog(DEBUG2, "restored %u/%u changes from disk", (uint32)entry->txn->nentries_mem, (uint32)entry->txn->nentries);

      Assert(entry->txn->nentries_mem);
                              
      state->entries[off].lsn = next_change->lsn;
      state->entries[off].change = next_change;
      binaryheap_replace_first(state->heap, Int32GetDatum(off));

      return change;
    }
  }

                                            
  binaryheap_remove_first(state->heap);

  return change;
}

   
                           
   
static void
ReorderBufferIterTXNFinish(ReorderBuffer *rb, ReorderBufferIterTXNState *state)
{
  int32 off;

  for (off = 0; off < state->nr_txns; off++)
  {
    if (state->entries[off].file.vfd != -1)
    {
      FileClose(state->entries[off].file.vfd);
    }
  }

                                                                 
  if (!dlist_is_empty(&state->old_change))
  {
    ReorderBufferChange *change;

    change = dlist_container(ReorderBufferChange, node, dlist_pop_head_node(&state->old_change));
    ReorderBufferReturnChange(rb, change);
    Assert(dlist_is_empty(&state->old_change));
  }

  binaryheap_free(state->heap);
  pfree(state);
}

   
                                                                        
                         
   
static void
ReorderBufferCleanupTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  bool found;
  dlist_mutable_iter iter;

                                               
  dlist_foreach_modify(iter, &txn->subtxns)
  {
    ReorderBufferTXN *subtxn;

    subtxn = dlist_container(ReorderBufferTXN, node, iter.cur);

       
                                                                          
                                                                         
                                                   
       
    Assert(subtxn->is_known_as_subxact);
    Assert(subtxn->nsubtxns == 0);

    ReorderBufferCleanupTXN(rb, subtxn);
  }

                                           
  dlist_foreach_modify(iter, &txn->changes)
  {
    ReorderBufferChange *change;

    change = dlist_container(ReorderBufferChange, node, iter.cur);

    ReorderBufferReturnChange(rb, change);
  }

     
                                                                           
                                                         
     
  dlist_foreach_modify(iter, &txn->tuplecids)
  {
    ReorderBufferChange *change;

    change = dlist_container(ReorderBufferChange, node, iter.cur);
    Assert(change->action == REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID);
    ReorderBufferReturnChange(rb, change);
  }

     
                                        
     
  if (txn->base_snapshot != NULL)
  {
    SnapBuildSnapDecRefcount(txn->base_snapshot);
    dlist_delete(&txn->base_snapshot_node);
  }

     
                                          
     
                                                                         
                                                                         
                                                                            
                                                 
     
  dlist_delete(&txn->node);

                                        
  hash_search(rb->by_txn, (void *)&txn->xid, HASH_REMOVE, &found);
  Assert(found);

                                      
  if (txn->serialized)
  {
    ReorderBufferRestoreCleanup(rb, txn);
  }

                  
  ReorderBufferReturnTXN(rb, txn);
}

   
                                                                              
                                   
   
static void
ReorderBufferBuildTupleCidHash(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  dlist_iter iter;
  HASHCTL hash_ctl;

  if (!txn->has_catalog_changes || dlist_is_empty(&txn->tuplecids))
  {
    return;
  }

  memset(&hash_ctl, 0, sizeof(hash_ctl));

  hash_ctl.keysize = sizeof(ReorderBufferTupleCidKey);
  hash_ctl.entrysize = sizeof(ReorderBufferTupleCidEnt);
  hash_ctl.hcxt = rb->context;

     
                                                                          
               
     
  txn->tuplecid_hash = hash_create("ReorderBufferTupleCid", txn->ntuplecids, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  dlist_foreach(iter, &txn->tuplecids)
  {
    ReorderBufferTupleCidKey key;
    ReorderBufferTupleCidEnt *ent;
    bool found;
    ReorderBufferChange *change;

    change = dlist_container(ReorderBufferChange, node, iter.cur);

    Assert(change->action == REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID);

                                  
    memset(&key, 0, sizeof(ReorderBufferTupleCidKey));

    key.relnode = change->data.tuplecid.node;

    ItemPointerCopy(&change->data.tuplecid.tid, &key.tid);

    ent = (ReorderBufferTupleCidEnt *)hash_search(txn->tuplecid_hash, (void *)&key, HASH_ENTER, &found);
    if (!found)
    {
      ent->cmin = change->data.tuplecid.cmin;
      ent->cmax = change->data.tuplecid.cmax;
      ent->combocid = change->data.tuplecid.combocid;
    }
    else
    {
         
                                                                         
                                           
         
      Assert(ent->cmin == change->data.tuplecid.cmin);

         
                                                                       
                                         
         
      Assert((ent->cmax == InvalidCommandId) || ((change->data.tuplecid.cmax != InvalidCommandId) && (change->data.tuplecid.cmax > ent->cmax)));
      ent->cmax = change->data.tuplecid.cmax;
    }
  }
}

   
                                                                             
                                                                          
           
   
static Snapshot
ReorderBufferCopySnap(ReorderBuffer *rb, Snapshot orig_snap, ReorderBufferTXN *txn, CommandId cid)
{
  Snapshot snap;
  dlist_iter iter;
  int i = 0;
  Size size;

  size = sizeof(SnapshotData) + sizeof(TransactionId) * orig_snap->xcnt + sizeof(TransactionId) * (txn->nsubtxns + 1);

  snap = MemoryContextAllocZero(rb->context, size);
  memcpy(snap, orig_snap, sizeof(SnapshotData));

  snap->copied = true;
  snap->active_count = 1;                                        
  snap->regd_count = 0;
  snap->xip = (TransactionId *)(snap + 1);

  memcpy(snap->xip, orig_snap->xip, sizeof(TransactionId) * snap->xcnt);

     
                                                                             
                                                                   
                                   
     
  snap->subxip = snap->xip + snap->xcnt;
  snap->subxip[i++] = txn->xid;

     
                                                                             
                                                                          
            
     
  snap->subxcnt = 1;

  dlist_foreach(iter, &txn->subtxns)
  {
    ReorderBufferTXN *sub_txn;

    sub_txn = dlist_container(ReorderBufferTXN, node, iter.cur);
    snap->subxip[i++] = sub_txn->xid;
    snap->subxcnt++;
  }

                                      
  qsort(snap->subxip, snap->subxcnt, sizeof(TransactionId), xidComparator);

                                             
  snap->curcid = cid;

  return snap;
}

   
                                                       
   
static void
ReorderBufferFreeSnap(ReorderBuffer *rb, Snapshot snap)
{
  if (snap->copied)
  {
    pfree(snap);
  }
  else
  {
    SnapBuildSnapDecRefcount(snap);
  }
}

   
                                                                            
   
                                                      
                                                                           
                                              
   
                                                                         
                                                                          
                                                                                
                                                                           
          
   
void
ReorderBufferCommit(ReorderBuffer *rb, TransactionId xid, XLogRecPtr commit_lsn, XLogRecPtr end_lsn, TimestampTz commit_time, RepOriginId origin_id, XLogRecPtr origin_lsn)
{
  ReorderBufferTXN *txn;
  volatile Snapshot snapshot_now;
  volatile CommandId command_id = FirstCommandId;
  bool using_subtxn;
  ReorderBufferIterTXNState *volatile iterstate = NULL;

  txn = ReorderBufferTXNByXid(rb, xid, false, NULL, InvalidXLogRecPtr, false);

                                              
  if (txn == NULL)
  {
    return;
  }

  txn->final_lsn = commit_lsn;
  txn->end_lsn = end_lsn;
  txn->commit_time = commit_time;
  txn->origin_id = origin_id;
  txn->origin_lsn = origin_lsn;

     
                                                                            
                                                        
                                                                       
                                        
     
  if (txn->base_snapshot == NULL)
  {
    Assert(txn->ninvalidations == 0);
    ReorderBufferCleanupTXN(rb, txn);
    return;
  }

  snapshot_now = txn->base_snapshot;

                                                                        
  ReorderBufferBuildTupleCidHash(rb, txn);

                                  
  SetupHistoricSnapshot(snapshot_now, txn->tuplecid_hash);

     
                                                                  
                                                                             
                                                                          
                                                                            
                                                      
     
                                                                     
                                                         
     
  using_subtxn = IsTransactionOrTransactionBlock();

  PG_TRY();
  {
    ReorderBufferChange *change;
    ReorderBufferChange *specinsert = NULL;

    if (using_subtxn)
    {
      BeginInternalSubTransaction("replay");
    }
    else
    {
      StartTransactionCommand();
    }

    rb->begin(rb, txn);

    ReorderBufferIterTXNInit(rb, txn, &iterstate);
    while ((change = ReorderBufferIterTXNNext(rb, iterstate)) != NULL)
    {
      Relation relation = NULL;
      Oid reloid;

      CHECK_FOR_INTERRUPTS();

      switch (change->action)
      {
      case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_CONFIRM:

           
                                                                  
                                                                  
                                 
           
        if (specinsert == NULL)
        {
          elog(ERROR, "invalid ordering of speculative insertion changes");
        }
        Assert(specinsert->data.tp.oldtuple == NULL);
        change = specinsert;
        change->action = REORDER_BUFFER_CHANGE_INSERT;

                                        
      case REORDER_BUFFER_CHANGE_INSERT:
      case REORDER_BUFFER_CHANGE_UPDATE:
      case REORDER_BUFFER_CHANGE_DELETE:
        Assert(snapshot_now);

        reloid = RelidByRelfilenode(change->data.tp.relnode.spcNode, change->data.tp.relnode.relNode);

           
                                                            
                                                                   
                                                            
                                                                   
                                                             
                                                                  
                                                                  
                                                            
                                                                 
                                                       
           
        if (reloid == InvalidOid && change->data.tp.newtuple == NULL && change->data.tp.oldtuple == NULL)
        {
          goto change_done;
        }
        else if (reloid == InvalidOid)
        {
          elog(ERROR, "could not map filenode \"%s\" to relation OID", relpathperm(change->data.tp.relnode, MAIN_FORKNUM));
        }

        relation = RelationIdGetRelation(reloid);

        if (!RelationIsValid(relation))
        {
          elog(ERROR, "could not open relation with OID %u (for filenode \"%s\")", reloid, relpathperm(change->data.tp.relnode, MAIN_FORKNUM));
        }

        if (!RelationIsLogicallyLogged(relation))
        {
          goto change_done;
        }

           
                                                                
                                      
           
        if (relation->rd_rel->relrewrite && !rb->output_rewrites)
        {
          goto change_done;
        }

           
                                                                 
                                                        
                                                                  
                        
           
        if (relation->rd_rel->relkind == RELKIND_SEQUENCE)
        {
          goto change_done;
        }

                                   
        if (!IsToastRelation(relation))
        {
          ReorderBufferToastReplace(rb, txn, relation, change);
          rb->apply_change(rb, txn, relation, change);

             
                                                               
                                                              
                             
             
          if (change->data.tp.clear_toast_afterwards)
          {
            ReorderBufferToastReset(rb, txn);
          }
        }
                                                     
        else if (change->action == REORDER_BUFFER_CHANGE_INSERT)
        {
             
                                                          
                                                                
                                                        
                                                          
                                                            
                   
             
          Assert(change->data.tp.newtuple != NULL);

          dlist_delete(&change->node);
          ReorderBufferToastAppendChunk(rb, txn, relation, change);
        }

      change_done:

           
                                                                    
                           
           
        if (specinsert != NULL)
        {
          ReorderBufferReturnChange(rb, specinsert);
          specinsert = NULL;
        }

        if (relation != NULL)
        {
          RelationClose(relation);
          relation = NULL;
        }
        break;

      case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_INSERT:

           
                                                                 
                                                                  
                                                                  
                                                                  
                                   
           
                                                                  
                                                                  
                                                         
                                             
           

                                                               
        if (specinsert != NULL)
        {
          ReorderBufferReturnChange(rb, specinsert);
          specinsert = NULL;
        }

                                                
        dlist_delete(&change->node);
        specinsert = change;
        break;

      case REORDER_BUFFER_CHANGE_TRUNCATE:
      {
        int i;
        int nrelids = change->data.truncate.nrelids;
        int nrelations = 0;
        Relation *relations;

        relations = palloc0(nrelids * sizeof(Relation));
        for (i = 0; i < nrelids; i++)
        {
          Oid relid = change->data.truncate.relids[i];
          Relation relation;

          relation = RelationIdGetRelation(relid);

          if (!RelationIsValid(relation))
          {
            elog(ERROR, "could not open relation with OID %u", relid);
          }

          if (!RelationIsLogicallyLogged(relation))
          {
            continue;
          }

          relations[nrelations++] = relation;
        }

        rb->apply_truncate(rb, txn, nrelations, relations, change);

        for (i = 0; i < nrelations; i++)
        {
          RelationClose(relations[i]);
        }

        break;
      }

      case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_ABORT:

           
                                                                   
                                            
           
                                                                 
                                                                   
                                              
           
        if (specinsert != NULL)
        {
             
                                                              
                                                               
                                            
             
          Assert(change->data.tp.clear_toast_afterwards);
          ReorderBufferToastReset(rb, txn);

                                                  
          ReorderBufferReturnChange(rb, specinsert);
          specinsert = NULL;
        }
        break;

      case REORDER_BUFFER_CHANGE_MESSAGE:
        rb->message(rb, txn, change->lsn, true, change->data.msg.prefix, change->data.msg.message_size, change->data.msg.message);
        break;

      case REORDER_BUFFER_CHANGE_INTERNAL_SNAPSHOT:
                                
        TeardownHistoricSnapshot(false);

        if (snapshot_now->copied)
        {
          ReorderBufferFreeSnap(rb, snapshot_now);
          snapshot_now = ReorderBufferCopySnap(rb, change->data.snapshot, txn, command_id);
        }

           
                                                                
                                                                  
                                                         
           
        else if (change->data.snapshot->copied)
        {
          snapshot_now = ReorderBufferCopySnap(rb, change->data.snapshot, txn, command_id);
        }
        else
        {
          snapshot_now = change->data.snapshot;
        }

                                           
        SetupHistoricSnapshot(snapshot_now, txn->tuplecid_hash);
        break;

      case REORDER_BUFFER_CHANGE_INTERNAL_COMMAND_ID:
        Assert(change->data.command_id != InvalidCommandId);

        if (command_id < change->data.command_id)
        {
          command_id = change->data.command_id;

          if (!snapshot_now->copied)
          {
                                                     
            snapshot_now = ReorderBufferCopySnap(rb, snapshot_now, txn, command_id);
          }

          snapshot_now->curcid = command_id;

          TeardownHistoricSnapshot(false);
          SetupHistoricSnapshot(snapshot_now, txn->tuplecid_hash);

             
                                                               
                                                      
                            
             
          ReorderBufferExecuteInvalidations(rb, txn);
        }

        break;

      case REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID:
        elog(ERROR, "tuplecid value in changequeue");
        break;
      }
    }

                                                           
    Assert(!specinsert);

                               
    ReorderBufferIterTXNFinish(rb, iterstate);
    iterstate = NULL;

                              
    rb->commit(rb, txn, commit_lsn);

                                                                         
    if (GetCurrentTransactionIdIfAny() != InvalidTransactionId)
    {
      elog(ERROR, "output plugin used XID %u", GetCurrentTransactionId());
    }

                 
    TeardownHistoricSnapshot(false);

       
                                                                       
                                                                         
                                                                       
                                
       
    AbortCurrentTransaction();

                                              
    ReorderBufferExecuteInvalidations(rb, txn);

    if (using_subtxn)
    {
      RollbackAndReleaseCurrentSubTransaction();
    }

    if (snapshot_now->copied)
    {
      ReorderBufferFreeSnap(rb, snapshot_now);
    }

                                                       
    ReorderBufferCleanupTXN(rb, txn);
  }
  PG_CATCH();
  {
                                                                       
    if (iterstate)
    {
      ReorderBufferIterTXNFinish(rb, iterstate);
    }

    TeardownHistoricSnapshot(true);

       
                                                                         
                                                             
       
    AbortCurrentTransaction();

                                              
    ReorderBufferExecuteInvalidations(rb, txn);

    if (using_subtxn)
    {
      RollbackAndReleaseCurrentSubTransaction();
    }

    if (snapshot_now->copied)
    {
      ReorderBufferFreeSnap(rb, snapshot_now);
    }

                                                       
    ReorderBufferCleanupTXN(rb, txn);

    PG_RE_THROW();
  }
  PG_END_TRY();
}

   
                                                                             
                                                             
   
                                                                          
                                                                              
                                                                           
                                                              
   
                                                                          
         
   
void
ReorderBufferAbort(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, false, NULL, InvalidXLogRecPtr, false);

                                  
  if (txn == NULL)
  {
    return;
  }

                   
  txn->final_lsn = lsn;

                                                     
  ReorderBufferCleanupTXN(rb, txn);
}

   
                                                                           
                     
   
                                                                              
                                                                      
   
void
ReorderBufferAbortOld(ReorderBuffer *rb, TransactionId oldestRunningXid)
{
  dlist_mutable_iter it;

     
                                                                          
                                                                         
                                                                             
                                                                           
                                          
     
  dlist_foreach_modify(it, &rb->toplevel_by_lsn)
  {
    ReorderBufferTXN *txn;

    txn = dlist_container(ReorderBufferTXN, node, it.cur);

    if (TransactionIdPrecedes(txn->xid, oldestRunningXid))
    {
      elog(DEBUG2, "aborting old transaction %u", txn->xid);

                                                                 
      ReorderBufferCleanupTXN(rb, txn);
    }
    else
    {
      return;
    }
  }
}

   
                                                                       
                                                                           
                 
   
                                                                   
                                                                                
                                                  
   
                                                                           
                                                                             
                                                             
   
void
ReorderBufferForget(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, false, NULL, InvalidXLogRecPtr, false);

                                  
  if (txn == NULL)
  {
    return;
  }

                   
  txn->final_lsn = lsn;

     
                                                                             
                                                                             
                                                                 
     
  if (txn->base_snapshot != NULL && txn->ninvalidations > 0)
  {
    ReorderBufferImmediateInvalidation(rb, txn->ninvalidations, txn->invalidations);
  }
  else
  {
    Assert(txn->ninvalidations == 0);
  }

                                                     
  ReorderBufferCleanupTXN(rb, txn);
}

   
                                                                    
                                                                   
                                                                         
                                             
   
void
ReorderBufferImmediateInvalidation(ReorderBuffer *rb, uint32 ninvalidations, SharedInvalidationMessage *invalidations)
{
  bool use_subtxn = IsTransactionOrTransactionBlock();
  int i;

  if (use_subtxn)
  {
    BeginInternalSubTransaction("replay");
  }

     
                                                                             
                                                                           
                                                                       
                                   
     
  if (use_subtxn)
  {
    AbortCurrentTransaction();
  }

  for (i = 0; i < ninvalidations; i++)
  {
    LocalExecuteInvalidationMessage(&invalidations[i]);
  }

  if (use_subtxn)
  {
    RollbackAndReleaseCurrentSubTransaction();
  }
}

   
                                                                               
                                                                           
                                                    
   
                                                                            
                                                                              
                                                                               
                                                               
   
void
ReorderBufferProcessXid(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{
                                                                      
  if (xid != InvalidTransactionId)
  {
    ReorderBufferTXNByXid(rb, xid, true, NULL, lsn, true);
  }
}

   
                                                                             
                                                                            
                   
   
void
ReorderBufferAddSnapshot(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, Snapshot snap)
{
  ReorderBufferChange *change = ReorderBufferGetChange(rb);

  change->data.snapshot = snap;
  change->action = REORDER_BUFFER_CHANGE_INTERNAL_SNAPSHOT;

  ReorderBufferQueueChange(rb, xid, lsn, change);
}

   
                                           
   
                                                                         
                                  
   
void
ReorderBufferSetBaseSnapshot(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, Snapshot snap)
{
  ReorderBufferTXN *txn;
  bool is_new;

  AssertArg(snap != NULL);

     
                                                                             
                                                   
     
  txn = ReorderBufferTXNByXid(rb, xid, true, &is_new, lsn, true);
  if (txn->is_known_as_subxact)
  {
    txn = ReorderBufferTXNByXid(rb, txn->toplevel_xid, false, NULL, InvalidXLogRecPtr, false);
  }
  Assert(txn->base_snapshot == NULL);

  txn->base_snapshot = snap;
  txn->base_snapshot_lsn = lsn;
  dlist_push_tail(&rb->txns_by_base_snapshot_lsn, &txn->base_snapshot_node);

  AssertTXNLsnOrder(rb);
}

   
                                                                             
   
                                          
   
void
ReorderBufferAddNewCommandId(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, CommandId cid)
{
  ReorderBufferChange *change = ReorderBufferGetChange(rb);

  change->data.command_id = cid;
  change->action = REORDER_BUFFER_CHANGE_INTERNAL_COMMAND_ID;

  ReorderBufferQueueChange(rb, xid, lsn, change);
}

   
                                                        
   
void
ReorderBufferAddNewTupleCids(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, RelFileNode node, ItemPointerData tid, CommandId cmin, CommandId cmax, CommandId combocid)
{
  ReorderBufferChange *change = ReorderBufferGetChange(rb);
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, true, NULL, lsn, true);

  change->data.tuplecid.node = node;
  change->data.tuplecid.tid = tid;
  change->data.tuplecid.cmin = cmin;
  change->data.tuplecid.cmax = cmax;
  change->data.tuplecid.combocid = combocid;
  change->lsn = lsn;
  change->action = REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID;

  dlist_push_tail(&txn->tuplecids, &change->node);
  txn->ntuplecids++;
}

   
                                                       
   
                                                               
   
void
ReorderBufferAddInvalidations(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, Size nmsgs, SharedInvalidationMessage *msgs)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, true, NULL, lsn, true);

  if (txn->ninvalidations != 0)
  {
    elog(ERROR, "only ever add one set of invalidations");
  }

  Assert(nmsgs > 0);

  txn->ninvalidations = nmsgs;
  txn->invalidations = (SharedInvalidationMessage *)MemoryContextAlloc(rb->context, sizeof(SharedInvalidationMessage) * nmsgs);
  memcpy(txn->invalidations, msgs, sizeof(SharedInvalidationMessage) * nmsgs);
}

   
                                                                              
                                                          
   
static void
ReorderBufferExecuteInvalidations(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  int i;

  for (i = 0; i < txn->ninvalidations; i++)
  {
    LocalExecuteInvalidationMessage(&txn->invalidations[i]);
  }
}

   
                                                    
   
void
ReorderBufferXidSetCatalogChanges(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, true, NULL, lsn, true);

  txn->has_catalog_changes = true;
}

   
                                                                     
                                                                
   
bool
ReorderBufferXidHasCatalogChanges(ReorderBuffer *rb, TransactionId xid)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, false, NULL, InvalidXLogRecPtr, false);
  if (txn == NULL)
  {
    return false;
  }

  return txn->has_catalog_changes;
}

   
                                   
                                                                    
   
bool
ReorderBufferXidHasBaseSnapshot(ReorderBuffer *rb, TransactionId xid)
{
  ReorderBufferTXN *txn;

  txn = ReorderBufferTXNByXid(rb, xid, false, NULL, InvalidXLogRecPtr, false);

                                                     
  if (txn == NULL)
  {
    return false;
  }

                                                        
  if (txn->is_known_as_subxact)
  {
    txn = ReorderBufferTXNByXid(rb, txn->toplevel_xid, false, NULL, InvalidXLogRecPtr, false);
  }

  return txn->base_snapshot != NULL;
}

   
                                           
                              
                                           
   

   
                                  
   
static void
ReorderBufferSerializeReserve(ReorderBuffer *rb, Size sz)
{
  if (!rb->outbufsize)
  {
    rb->outbuf = MemoryContextAlloc(rb->context, sz);
    rb->outbufsize = sz;
  }
  else if (rb->outbufsize < sz)
  {
    rb->outbuf = repalloc(rb->outbuf, sz);
    rb->outbufsize = sz;
  }
}

   
                                                                   
   
static void
ReorderBufferCheckSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
     
                                                                          
                   
     
  if (txn->nentries_mem >= max_changes_in_memory)
  {
    ReorderBufferSerializeTXN(rb, txn);
    Assert(txn->nentries_mem == 0);
  }
}

   
                                                                        
   
static void
ReorderBufferSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  dlist_iter subtxn_i;
  dlist_mutable_iter change_i;
  int fd = -1;
  XLogSegNo curOpenSegNo = 0;
  Size spilled = 0;

  elog(DEBUG2, "spill %u changes in XID %u to disk", (uint32)txn->nentries_mem, txn->xid);

                                    
  dlist_foreach(subtxn_i, &txn->subtxns)
  {
    ReorderBufferTXN *subtxn;

    subtxn = dlist_container(ReorderBufferTXN, node, subtxn_i.cur);
    ReorderBufferSerializeTXN(rb, subtxn);
  }

                              
  dlist_foreach_modify(change_i, &txn->changes)
  {
    ReorderBufferChange *change;

    change = dlist_container(ReorderBufferChange, node, change_i.cur);

       
                                                                           
                             
       
    if (fd == -1 || !XLByteInSeg(change->lsn, curOpenSegNo, wal_segment_size))
    {
      char path[MAXPGPATH];

      if (fd != -1)
      {
        CloseTransientFile(fd);
      }

      XLByteToSeg(change->lsn, curOpenSegNo, wal_segment_size);

         
                                                                         
                                                         
         
      ReorderBufferSerializedPath(path, MyReplicationSlot, txn->xid, curOpenSegNo);

                                                
      fd = OpenTransientFile(path, O_CREAT | O_WRONLY | O_APPEND | PG_BINARY);

      if (fd < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
      }
    }

    ReorderBufferSerializeChange(rb, txn, fd, change);
    dlist_delete(&change->node);
    ReorderBufferReturnChange(rb, change);

    spilled++;
  }

  Assert(spilled == txn->nentries_mem);
  Assert(dlist_is_empty(&txn->changes));
  txn->nentries_mem = 0;
  txn->serialized = true;

  if (fd != -1)
  {
    CloseTransientFile(fd);
  }
}

   
                                        
   
static void
ReorderBufferSerializeChange(ReorderBuffer *rb, ReorderBufferTXN *txn, int fd, ReorderBufferChange *change)
{
  ReorderBufferDiskChange *ondisk;
  Size sz = sizeof(ReorderBufferDiskChange);

  ReorderBufferSerializeReserve(rb, sz);

  ondisk = (ReorderBufferDiskChange *)rb->outbuf;
  memcpy(&ondisk->change, change, sizeof(ReorderBufferChange));

  switch (change->action)
  {
                                                        
  case REORDER_BUFFER_CHANGE_INSERT:
  case REORDER_BUFFER_CHANGE_UPDATE:
  case REORDER_BUFFER_CHANGE_DELETE:
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_INSERT:
  {
    char *data;
    ReorderBufferTupleBuf *oldtup, *newtup;
    Size oldlen = 0;
    Size newlen = 0;

    oldtup = change->data.tp.oldtuple;
    newtup = change->data.tp.newtuple;

    if (oldtup)
    {
      sz += sizeof(HeapTupleData);
      oldlen = oldtup->tuple.t_len;
      sz += oldlen;
    }

    if (newtup)
    {
      sz += sizeof(HeapTupleData);
      newlen = newtup->tuple.t_len;
      sz += newlen;
    }

                                        
    ReorderBufferSerializeReserve(rb, sz);

    data = ((char *)rb->outbuf) + sizeof(ReorderBufferDiskChange);
                                           
    ondisk = (ReorderBufferDiskChange *)rb->outbuf;

    if (oldlen)
    {
      memcpy(data, &oldtup->tuple, sizeof(HeapTupleData));
      data += sizeof(HeapTupleData);

      memcpy(data, oldtup->tuple.t_data, oldlen);
      data += oldlen;
    }

    if (newlen)
    {
      memcpy(data, &newtup->tuple, sizeof(HeapTupleData));
      data += sizeof(HeapTupleData);

      memcpy(data, newtup->tuple.t_data, newlen);
      data += newlen;
    }
    break;
  }
  case REORDER_BUFFER_CHANGE_MESSAGE:
  {
    char *data;
    Size prefix_size = strlen(change->data.msg.prefix) + 1;

    sz += prefix_size + change->data.msg.message_size + sizeof(Size) + sizeof(Size);
    ReorderBufferSerializeReserve(rb, sz);

    data = ((char *)rb->outbuf) + sizeof(ReorderBufferDiskChange);

                                           
    ondisk = (ReorderBufferDiskChange *)rb->outbuf;

                                             
    memcpy(data, &prefix_size, sizeof(Size));
    data += sizeof(Size);
    memcpy(data, change->data.msg.prefix, prefix_size);
    data += prefix_size;

                                              
    memcpy(data, &change->data.msg.message_size, sizeof(Size));
    data += sizeof(Size);
    memcpy(data, change->data.msg.message, change->data.msg.message_size);
    data += change->data.msg.message_size;

    break;
  }
  case REORDER_BUFFER_CHANGE_INTERNAL_SNAPSHOT:
  {
    Snapshot snap;
    char *data;

    snap = change->data.snapshot;

    sz += sizeof(SnapshotData) + sizeof(TransactionId) * snap->xcnt + sizeof(TransactionId) * snap->subxcnt;

                                        
    ReorderBufferSerializeReserve(rb, sz);
    data = ((char *)rb->outbuf) + sizeof(ReorderBufferDiskChange);
                                           
    ondisk = (ReorderBufferDiskChange *)rb->outbuf;

    memcpy(data, snap, sizeof(SnapshotData));
    data += sizeof(SnapshotData);

    if (snap->xcnt)
    {
      memcpy(data, snap->xip, sizeof(TransactionId) * snap->xcnt);
      data += sizeof(TransactionId) * snap->xcnt;
    }

    if (snap->subxcnt)
    {
      memcpy(data, snap->subxip, sizeof(TransactionId) * snap->subxcnt);
      data += sizeof(TransactionId) * snap->subxcnt;
    }
    break;
  }
  case REORDER_BUFFER_CHANGE_TRUNCATE:
  {
    Size size;
    char *data;

                                                     
    size = sizeof(Oid) * change->data.truncate.nrelids;
    sz += size;

                                        
    ReorderBufferSerializeReserve(rb, sz);

    data = ((char *)rb->outbuf) + sizeof(ReorderBufferDiskChange);
                                           
    ondisk = (ReorderBufferDiskChange *)rb->outbuf;

    memcpy(data, change->data.truncate.relids, size);
    data += size;

    break;
  }
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_CONFIRM:
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_ABORT:
  case REORDER_BUFFER_CHANGE_INTERNAL_COMMAND_ID:
  case REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID:
                                                           
    break;
  }

  ondisk->size = sz;

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_REORDER_BUFFER_WRITE);
  if (write(fd, rb->outbuf, ondisk->size) != ondisk->size)
  {
    int save_errno = errno;

    CloseTransientFile(fd);

                                                                    
    errno = save_errno ? save_errno : ENOSPC;
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to data file for XID %u: %m", txn->xid)));
  }
  pgstat_report_wait_end();

     
                                                                             
                                                                             
                                                                          
                                                                  
     
                                         
     
  if (txn->final_lsn < change->lsn)
  {
    txn->final_lsn = change->lsn;
  }

  Assert(ondisk->change.action == change->action);
}

   
                                                                 
   
static Size
ReorderBufferRestoreChanges(ReorderBuffer *rb, ReorderBufferTXN *txn, TXNEntryFile *file, XLogSegNo *segno)
{
  Size restored = 0;
  XLogSegNo last_segno;
  dlist_mutable_iter cleanup_iter;
  File *fd = &file->vfd;

  Assert(txn->first_lsn != InvalidXLogRecPtr);
  Assert(txn->final_lsn != InvalidXLogRecPtr);

                                                        
  dlist_foreach_modify(cleanup_iter, &txn->changes)
  {
    ReorderBufferChange *cleanup = dlist_container(ReorderBufferChange, node, cleanup_iter.cur);

    dlist_delete(&cleanup->node);
    ReorderBufferReturnChange(rb, cleanup);
  }
  txn->nentries_mem = 0;
  Assert(dlist_is_empty(&txn->changes));

  XLByteToSeg(txn->final_lsn, last_segno, wal_segment_size);

  while (restored < max_changes_in_memory && *segno <= last_segno)
  {
    int readBytes;
    ReorderBufferDiskChange *ondisk;

    CHECK_FOR_INTERRUPTS();

    if (*fd == -1)
    {
      char path[MAXPGPATH];

                         
      if (*segno == 0)
      {
        XLByteToSeg(txn->first_lsn, *segno, wal_segment_size);
      }

      Assert(*segno != 0 || dlist_is_empty(&txn->changes));

         
                                                                         
                                                         
         
      ReorderBufferSerializedPath(path, MyReplicationSlot, txn->xid, *segno);

      *fd = PathNameOpenFile(path, O_RDONLY | PG_BINARY);

                                                                   
      file->curOffset = 0;

      if (*fd < 0 && errno == ENOENT)
      {
        *fd = -1;
        (*segno)++;
        continue;
      }
      else if (*fd < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
      }
    }

       
                                                                        
                                                                        
                         
       
    ReorderBufferSerializeReserve(rb, sizeof(ReorderBufferDiskChange));
    readBytes = FileRead(file->vfd, rb->outbuf, sizeof(ReorderBufferDiskChange), file->curOffset, WAIT_EVENT_REORDER_BUFFER_READ);

             
    if (readBytes == 0)
    {
      FileClose(*fd);
      *fd = -1;
      (*segno)++;
      continue;
    }
    else if (readBytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from reorderbuffer spill file: %m")));
    }
    else if (readBytes != sizeof(ReorderBufferDiskChange))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from reorderbuffer spill file: read %d instead of %u bytes", readBytes, (uint32)sizeof(ReorderBufferDiskChange))));
    }

    file->curOffset += readBytes;

    ondisk = (ReorderBufferDiskChange *)rb->outbuf;

    ReorderBufferSerializeReserve(rb, sizeof(ReorderBufferDiskChange) + ondisk->size);
    ondisk = (ReorderBufferDiskChange *)rb->outbuf;

    readBytes = FileRead(file->vfd, rb->outbuf + sizeof(ReorderBufferDiskChange), ondisk->size - sizeof(ReorderBufferDiskChange), file->curOffset, WAIT_EVENT_REORDER_BUFFER_READ);

    if (readBytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from reorderbuffer spill file: %m")));
    }
    else if (readBytes != ondisk->size - sizeof(ReorderBufferDiskChange))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from reorderbuffer spill file: read %d instead of %u bytes", readBytes, (uint32)(ondisk->size - sizeof(ReorderBufferDiskChange)))));
    }

    file->curOffset += readBytes;

       
                                                                    
                        
       
    ReorderBufferRestoreChange(rb, txn, rb->outbuf);
    restored++;
  }

  return restored;
}

   
                                                                                
                             
   
                                                                    
                                                                        
                                                                   
   
static void
ReorderBufferRestoreChange(ReorderBuffer *rb, ReorderBufferTXN *txn, char *data)
{
  ReorderBufferDiskChange *ondisk;
  ReorderBufferChange *change;

  ondisk = (ReorderBufferDiskChange *)data;

  change = ReorderBufferGetChange(rb);

                        
  memcpy(change, &ondisk->change, sizeof(ReorderBufferChange));

  data += sizeof(ReorderBufferDiskChange);

                                
  switch (change->action)
  {
                                                        
  case REORDER_BUFFER_CHANGE_INSERT:
  case REORDER_BUFFER_CHANGE_UPDATE:
  case REORDER_BUFFER_CHANGE_DELETE:
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_INSERT:
    if (change->data.tp.oldtuple)
    {
      uint32 tuplelen = ((HeapTuple)data)->t_len;

      change->data.tp.oldtuple = ReorderBufferGetTupleBuf(rb, tuplelen - SizeofHeapTupleHeader);

                           
      memcpy(&change->data.tp.oldtuple->tuple, data, sizeof(HeapTupleData));
      data += sizeof(HeapTupleData);

                                                      
      change->data.tp.oldtuple->tuple.t_data = ReorderBufferTupleBufData(change->data.tp.oldtuple);

                                     
      memcpy(change->data.tp.oldtuple->tuple.t_data, data, tuplelen);
      data += tuplelen;
    }

    if (change->data.tp.newtuple)
    {
                                                     
      uint32 tuplelen;

      memcpy(&tuplelen, data + offsetof(HeapTupleData, t_len), sizeof(uint32));

      change->data.tp.newtuple = ReorderBufferGetTupleBuf(rb, tuplelen - SizeofHeapTupleHeader);

                           
      memcpy(&change->data.tp.newtuple->tuple, data, sizeof(HeapTupleData));
      data += sizeof(HeapTupleData);

                                                      
      change->data.tp.newtuple->tuple.t_data = ReorderBufferTupleBufData(change->data.tp.newtuple);

                                     
      memcpy(change->data.tp.newtuple->tuple.t_data, data, tuplelen);
      data += tuplelen;
    }

    break;
  case REORDER_BUFFER_CHANGE_MESSAGE:
  {
    Size prefix_size;

                     
    memcpy(&prefix_size, data, sizeof(Size));
    data += sizeof(Size);
    change->data.msg.prefix = MemoryContextAlloc(rb->context, prefix_size);
    memcpy(change->data.msg.prefix, data, prefix_size);
    Assert(change->data.msg.prefix[prefix_size - 1] == '\0');
    data += prefix_size;

                          
    memcpy(&change->data.msg.message_size, data, sizeof(Size));
    data += sizeof(Size);
    change->data.msg.message = MemoryContextAlloc(rb->context, change->data.msg.message_size);
    memcpy(change->data.msg.message, data, change->data.msg.message_size);
    data += change->data.msg.message_size;

    break;
  }
  case REORDER_BUFFER_CHANGE_INTERNAL_SNAPSHOT:
  {
    Snapshot oldsnap;
    Snapshot newsnap;
    Size size;

    oldsnap = (Snapshot)data;

    size = sizeof(SnapshotData) + sizeof(TransactionId) * oldsnap->xcnt + sizeof(TransactionId) * (oldsnap->subxcnt + 0);

    change->data.snapshot = MemoryContextAllocZero(rb->context, size);

    newsnap = change->data.snapshot;

    memcpy(newsnap, data, size);
    newsnap->xip = (TransactionId *)(((char *)newsnap) + sizeof(SnapshotData));
    newsnap->subxip = newsnap->xip + newsnap->xcnt;
    newsnap->copied = true;
    break;
  }
                                                           
  case REORDER_BUFFER_CHANGE_TRUNCATE:
  {
    Oid *relids;

    relids = ReorderBufferGetRelids(rb, change->data.truncate.nrelids);
    memcpy(relids, data, change->data.truncate.nrelids * sizeof(Oid));
    change->data.truncate.relids = relids;

    break;
  }
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_CONFIRM:
  case REORDER_BUFFER_CHANGE_INTERNAL_SPEC_ABORT:
  case REORDER_BUFFER_CHANGE_INTERNAL_COMMAND_ID:
  case REORDER_BUFFER_CHANGE_INTERNAL_TUPLECID:
    break;
  }

  dlist_push_tail(&txn->changes, &change->node);
  txn->nentries_mem++;
}

   
                                                            
   
static void
ReorderBufferRestoreCleanup(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  XLogSegNo first;
  XLogSegNo cur;
  XLogSegNo last;

  Assert(txn->first_lsn != InvalidXLogRecPtr);
  Assert(txn->final_lsn != InvalidXLogRecPtr);

  XLByteToSeg(txn->first_lsn, first, wal_segment_size);
  XLByteToSeg(txn->final_lsn, last, wal_segment_size);

                                                            
  for (cur = first; cur <= last; cur++)
  {
    char path[MAXPGPATH];

    ReorderBufferSerializedPath(path, MyReplicationSlot, txn->xid, cur);
    if (unlink(path) != 0 && errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
    }
  }
}

   
                                                                                
                                         
   
static void
ReorderBufferCleanupSerializedTXNs(const char *slotname)
{
  DIR *spill_dir;
  struct dirent *spill_de;
  struct stat statbuf;
  char path[MAXPGPATH * 2 + 12];

  sprintf(path, "pg_replslot/%s", slotname);

                                                                   
  if (lstat(path, &statbuf) == 0 && !S_ISDIR(statbuf.st_mode))
  {
    return;
  }

  spill_dir = AllocateDir(path);
  while ((spill_de = ReadDirExtended(spill_dir, path, INFO)) != NULL)
  {
                                             
    if (strncmp(spill_de->d_name, "xid", 3) == 0)
    {
      snprintf(path, sizeof(path), "pg_replslot/%s/%s", slotname, spill_de->d_name);

      if (unlink(path) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove file \"%s\" during removal of pg_replslot/%s/xid*: %m", path, slotname)));
      }
    }
  }
  FreeDir(spill_dir);
}

   
                                                                            
                                                                                
                       
   
static void
ReorderBufferSerializedPath(char *path, ReplicationSlot *slot, TransactionId xid, XLogSegNo segno)
{
  XLogRecPtr recptr;

  XLogSegNoOffsetToRecPtr(segno, 0, wal_segment_size, recptr);

  snprintf(path, MAXPGPATH, "pg_replslot/%s/xid-%u-lsn-%X-%X.spill", NameStr(MyReplicationSlot->data.name), xid, (uint32)(recptr >> 32), (uint32)recptr);
}

   
                                                                             
                                                   
   
void
StartupReorderBuffer(void)
{
  DIR *logical_dir;
  struct dirent *logical_de;

  logical_dir = AllocateDir("pg_replslot");
  while ((logical_de = ReadDir(logical_dir, "pg_replslot")) != NULL)
  {
    if (strcmp(logical_de->d_name, ".") == 0 || strcmp(logical_de->d_name, "..") == 0)
    {
      continue;
    }

                                                    
    if (!ReplicationSlotValidateName(logical_de->d_name, DEBUG2))
    {
      continue;
    }

       
                                                                  
                                      
       
    ReorderBufferCleanupSerializedTXNs(logical_de->d_name);
  }
  FreeDir(logical_dir);
}

                                           
                            
                                           
   

   
                                                      
   
static void
ReorderBufferToastInitHash(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  HASHCTL hash_ctl;

  Assert(txn->toast_hash == NULL);

  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(ReorderBufferToastEnt);
  hash_ctl.hcxt = rb->context;
  txn->toast_hash = hash_create("ReorderBufferToastHash", 5, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

   
                                                     
   
                                                                              
                              
   
static void
ReorderBufferToastAppendChunk(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{
  ReorderBufferToastEnt *ent;
  ReorderBufferTupleBuf *newtup;
  bool found;
  int32 chunksize;
  bool isnull;
  Pointer chunk;
  TupleDesc desc = RelationGetDescr(relation);
  Oid chunk_id;
  int32 chunk_seq;

  if (txn->toast_hash == NULL)
  {
    ReorderBufferToastInitHash(rb, txn);
  }

  Assert(IsToastRelation(relation));

  newtup = change->data.tp.newtuple;
  chunk_id = DatumGetObjectId(fastgetattr(&newtup->tuple, 1, desc, &isnull));
  Assert(!isnull);
  chunk_seq = DatumGetInt32(fastgetattr(&newtup->tuple, 2, desc, &isnull));
  Assert(!isnull);

  ent = (ReorderBufferToastEnt *)hash_search(txn->toast_hash, (void *)&chunk_id, HASH_ENTER, &found);

  if (!found)
  {
    Assert(ent->chunk_id == chunk_id);
    ent->num_chunks = 0;
    ent->last_chunk_seq = 0;
    ent->size = 0;
    ent->reconstructed = NULL;
    dlist_init(&ent->chunks);

    if (chunk_seq != 0)
    {
      elog(ERROR, "got sequence entry %d for toast chunk %u instead of seq 0", chunk_seq, chunk_id);
    }
  }
  else if (found && chunk_seq != ent->last_chunk_seq + 1)
  {
    elog(ERROR, "got sequence entry %d for toast chunk %u instead of seq %d", chunk_seq, chunk_id, ent->last_chunk_seq + 1);
  }

  chunk = DatumGetPointer(fastgetattr(&newtup->tuple, 3, desc, &isnull));
  Assert(!isnull);

                                                                      
  if (!VARATT_IS_EXTENDED(chunk))
  {
    chunksize = VARSIZE(chunk) - VARHDRSZ;
  }
  else if (VARATT_IS_SHORT(chunk))
  {
                                                             
    chunksize = VARSIZE_SHORT(chunk) - VARHDRSZ_SHORT;
  }
  else
  {
    elog(ERROR, "unexpected type of toast chunk");
  }

  ent->size += chunksize;
  ent->last_chunk_seq = chunk_seq;
  ent->num_chunks++;
  dlist_push_tail(&ent->chunks, &change->node);
}

   
                                                                           
                                                                                
   
                                                                              
                          
   
static void
ReorderBufferToastReplace(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{
  TupleDesc desc;
  int natt;
  Datum *attrs;
  bool *isnull;
  bool *free;
  HeapTuple tmphtup;
  Relation toast_rel;
  TupleDesc toast_desc;
  MemoryContext oldcontext;
  ReorderBufferTupleBuf *newtup;

                               
  if (txn->toast_hash == NULL)
  {
    return;
  }

  oldcontext = MemoryContextSwitchTo(rb->context);

                                                               
  Assert(change->data.tp.newtuple);

  desc = RelationGetDescr(relation);

  toast_rel = RelationIdGetRelation(relation->rd_rel->reltoastrelid);
  if (!RelationIsValid(toast_rel))
  {
    elog(ERROR, "could not open toast relation with OID %u (base relation \"%s\")", relation->rd_rel->reltoastrelid, RelationGetRelationName(relation));
  }

  toast_desc = RelationGetDescr(toast_rel);

                                              
  attrs = palloc0(sizeof(Datum) * desc->natts);
  isnull = palloc0(sizeof(bool) * desc->natts);
  free = palloc0(sizeof(bool) * desc->natts);

  newtup = change->data.tp.newtuple;

  heap_deform_tuple(&newtup->tuple, desc, attrs, isnull);

  for (natt = 0; natt < desc->natts; natt++)
  {
    Form_pg_attribute attr = TupleDescAttr(desc, natt);
    ReorderBufferToastEnt *ent;
    struct varlena *varlena;

                                                                          
    struct varatt_external toast_pointer;
    struct varatt_indirect redirect_pointer;
    struct varlena *new_datum = NULL;
    struct varlena *reconstructed;
    dlist_iter it;
    Size data_done = 0;

                                       
    if (attr->attnum < 0)
    {
      continue;
    }

    if (attr->attisdropped)
    {
      continue;
    }

                                
    if (attr->attlen != -1)
    {
      continue;
    }

                 
    if (isnull[natt])
    {
      continue;
    }

                                           
    varlena = (struct varlena *)DatumGetPointer(attrs[natt]);

                                                            
    if (!VARATT_IS_EXTERNAL(varlena))
    {
      continue;
    }

    VARATT_EXTERNAL_GET_POINTER(toast_pointer, varlena);

       
                                                             
       
    ent = (ReorderBufferToastEnt *)hash_search(txn->toast_hash, (void *)&toast_pointer.va_valueid, HASH_FIND, NULL);
    if (ent == NULL)
    {
      continue;
    }

    new_datum = (struct varlena *)palloc0(INDIRECT_POINTER_SIZE);

    free[natt] = true;

    reconstructed = palloc0(toast_pointer.va_rawsize);

    ent->reconstructed = reconstructed;

                                                         
    dlist_foreach(it, &ent->chunks)
    {
      bool isnull;
      ReorderBufferChange *cchange;
      ReorderBufferTupleBuf *ctup;
      Pointer chunk;

      cchange = dlist_container(ReorderBufferChange, node, it.cur);
      ctup = cchange->data.tp.newtuple;
      chunk = DatumGetPointer(fastgetattr(&ctup->tuple, 3, toast_desc, &isnull));

      Assert(!isnull);
      Assert(!VARATT_IS_EXTERNAL(chunk));
      Assert(!VARATT_IS_SHORT(chunk));

      memcpy(VARDATA(reconstructed) + data_done, VARDATA(chunk), VARSIZE(chunk) - VARHDRSZ);
      data_done += VARSIZE(chunk) - VARHDRSZ;
    }
    Assert(data_done == toast_pointer.va_extsize);

                                                   
    if (VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer))
    {
      SET_VARSIZE_COMPRESSED(reconstructed, data_done + VARHDRSZ);
    }
    else
    {
      SET_VARSIZE(reconstructed, data_done + VARHDRSZ);
    }

    memset(&redirect_pointer, 0, sizeof(redirect_pointer));
    redirect_pointer.pointer = reconstructed;

    SET_VARTAG_EXTERNAL(new_datum, VARTAG_INDIRECT);
    memcpy(VARDATA_EXTERNAL(new_datum), &redirect_pointer, sizeof(redirect_pointer));

    attrs[natt] = PointerGetDatum(new_datum);
  }

     
                                                                        
                                                                           
                                                                            
     
  tmphtup = heap_form_tuple(desc, attrs, isnull);
  Assert(newtup->tuple.t_len <= MaxHeapTupleSize);
  Assert(ReorderBufferTupleBufData(newtup) == newtup->tuple.t_data);

  memcpy(newtup->tuple.t_data, tmphtup->t_data, tmphtup->t_len);
  newtup->tuple.t_len = tmphtup->t_len;

     
                                                                         
                                          
     
  RelationClose(toast_rel);
  pfree(tmphtup);
  for (natt = 0; natt < desc->natts; natt++)
  {
    if (free[natt])
    {
      pfree(DatumGetPointer(attrs[natt]));
    }
  }
  pfree(attrs);
  pfree(free);
  pfree(isnull);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                          
   
static void
ReorderBufferToastReset(ReorderBuffer *rb, ReorderBufferTXN *txn)
{
  HASH_SEQ_STATUS hstat;
  ReorderBufferToastEnt *ent;

  if (txn->toast_hash == NULL)
  {
    return;
  }

                                                           
  hash_seq_init(&hstat, txn->toast_hash);
  while ((ent = (ReorderBufferToastEnt *)hash_seq_search(&hstat)) != NULL)
  {
    dlist_mutable_iter it;

    if (ent->reconstructed != NULL)
    {
      pfree(ent->reconstructed);
    }

    dlist_foreach_modify(it, &ent->chunks)
    {
      ReorderBufferChange *change = dlist_container(ReorderBufferChange, node, it.cur);

      dlist_delete(&change->node);
      ReorderBufferReturnChange(rb, change);
    }
  }

  hash_destroy(txn->toast_hash);
  txn->toast_hash = NULL;
}

                                           
                                           
   
   
                                                                         
                                                                    
   
                                                                             
                                                                        
                        
                                                                
                                                                         
                                                                             
                                                                        
                  
   
                                                                      
                                                                       
                                                                    
                                                                      
                                 
   
                                                                  
                                   
   
                                                                
            
                                                                             
   

                                                                     
typedef struct RewriteMappingFile
{
  XLogRecPtr lsn;
  char fname[MAXPGPATH];
} RewriteMappingFile;

#if NOT_USED
static void
DisplayMapping(HTAB *tuplecid_data)
{
  HASH_SEQ_STATUS hstat;
  ReorderBufferTupleCidEnt *ent;

  hash_seq_init(&hstat, tuplecid_data);
  while ((ent = (ReorderBufferTupleCidEnt *)hash_seq_search(&hstat)) != NULL)
  {
    elog(DEBUG3, "mapping: node: %u/%u/%u tid: %u/%u cmin: %u, cmax: %u", ent->key.relnode.dbNode, ent->key.relnode.spcNode, ent->key.relnode.relNode, ItemPointerGetBlockNumber(&ent->key.tid), ItemPointerGetOffsetNumber(&ent->key.tid), ent->cmin, ent->cmax);
  }
}
#endif

   
                                                 
   
                                                                            
                                        
   
static void
ApplyLogicalMappingFile(HTAB *tuplecid_data, Oid relid, const char *fname)
{
  char path[MAXPGPATH];
  int fd;
  int readBytes;
  LogicalRewriteMappingData map;

  sprintf(path, "pg_logical/mappings/%s", fname);
  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

  while (true)
  {
    ReorderBufferTupleCidKey key;
    ReorderBufferTupleCidEnt *ent;
    ReorderBufferTupleCidEnt *new_ent;
    bool found;

                                  
    memset(&key, 0, sizeof(ReorderBufferTupleCidKey));

                                                    
    pgstat_report_wait_start(WAIT_EVENT_REORDER_LOGICAL_MAPPING_READ);
    readBytes = read(fd, &map, sizeof(LogicalRewriteMappingData));
    pgstat_report_wait_end();

    if (readBytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else if (readBytes == 0)          
    {
      break;
    }
    else if (readBytes != sizeof(LogicalRewriteMappingData))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from file \"%s\": read %d instead of %d bytes", path, readBytes, (int32)sizeof(LogicalRewriteMappingData))));
    }

    key.relnode = map.old_node;
    ItemPointerCopy(&map.old_tid, &key.tid);

    ent = (ReorderBufferTupleCidEnt *)hash_search(tuplecid_data, (void *)&key, HASH_FIND, NULL);

                                                
    if (!ent)
    {
      continue;
    }

    key.relnode = map.new_node;
    ItemPointerCopy(&map.new_tid, &key.tid);

    new_ent = (ReorderBufferTupleCidEnt *)hash_search(tuplecid_data, (void *)&key, HASH_ENTER, &found);

    if (found)
    {
         
                                                                        
                                                                      
                                                                   
         
      Assert(ent->cmin == InvalidCommandId || ent->cmin == new_ent->cmin);
      Assert(ent->cmax == InvalidCommandId || ent->cmax == new_ent->cmax);
    }
    else
    {
                          
      new_ent->cmin = ent->cmin;
      new_ent->cmax = ent->cmax;
      new_ent->combocid = ent->combocid;
    }
  }

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }
}

   
                                                                            
   
static bool
TransactionIdInArray(TransactionId xid, TransactionId *xip, Size num)
{
  return bsearch(&xid, xip, num, sizeof(TransactionId), xidComparator) != NULL;
}

   
                                                                    
   
static int
file_sort_by_lsn(const void *a_p, const void *b_p)
{
  RewriteMappingFile *a = *(RewriteMappingFile **)a_p;
  RewriteMappingFile *b = *(RewriteMappingFile **)b_p;

  if (a->lsn < b->lsn)
  {
    return -1;
  }
  else if (a->lsn > b->lsn)
  {
    return 1;
  }
  return 0;
}

   
                                                                               
                          
   
static void
UpdateLogicalMappings(HTAB *tuplecid_data, Oid relid, Snapshot snapshot)
{
  DIR *mapping_dir;
  struct dirent *mapping_de;
  List *files = NIL;
  ListCell *file;
  RewriteMappingFile **files_a;
  size_t off;
  Oid dboid = IsSharedRelation(relid) ? InvalidOid : MyDatabaseId;

  mapping_dir = AllocateDir("pg_logical/mappings");
  while ((mapping_de = ReadDir(mapping_dir, "pg_logical/mappings")) != NULL)
  {
    Oid f_dboid;
    Oid f_relid;
    TransactionId f_mapped_xid;
    TransactionId f_create_xid;
    XLogRecPtr f_lsn;
    uint32 f_hi, f_lo;
    RewriteMappingFile *f;

    if (strcmp(mapping_de->d_name, ".") == 0 || strcmp(mapping_de->d_name, "..") == 0)
    {
      continue;
    }

                                       
    if (strncmp(mapping_de->d_name, "map-", 4) != 0)
    {
      continue;
    }

    if (sscanf(mapping_de->d_name, LOGICAL_REWRITE_FORMAT, &f_dboid, &f_relid, &f_hi, &f_lo, &f_mapped_xid, &f_create_xid) != 6)
    {
      elog(ERROR, "could not parse filename \"%s\"", mapping_de->d_name);
    }

    f_lsn = ((uint64)f_hi) << 32 | f_lo;

                                      
    if (f_dboid != dboid)
    {
      continue;
    }

                                      
    if (f_relid != relid)
    {
      continue;
    }

                                             
    if (!TransactionIdDidCommit(f_create_xid))
    {
      continue;
    }

                                 
    if (!TransactionIdInArray(f_mapped_xid, snapshot->subxip, snapshot->subxcnt))
    {
      continue;
    }

                                       
    f = palloc(sizeof(RewriteMappingFile));
    f->lsn = f_lsn;
    strcpy(f->fname, mapping_de->d_name);
    files = lappend(files, f);
  }
  FreeDir(mapping_dir);

                                      
  files_a = palloc(list_length(files) * sizeof(RewriteMappingFile *));
  off = 0;
  foreach (file, files)
  {
    files_a[off++] = lfirst(file);
  }

                                                
  qsort(files_a, list_length(files), sizeof(RewriteMappingFile *), file_sort_by_lsn);

  for (off = 0; off < list_length(files); off++)
  {
    RewriteMappingFile *f = files_a[off];

    elog(DEBUG1, "applying mapping: \"%s\" in %u", f->fname, snapshot->subxip[0]);
    ApplyLogicalMappingFile(tuplecid_data, relid, f->fname);
    pfree(f);
  }
}

   
                                                                               
              
   
bool
ResolveCminCmaxDuringDecoding(HTAB *tuplecid_data, Snapshot snapshot, HeapTuple htup, Buffer buffer, CommandId *cmin, CommandId *cmax)
{
  ReorderBufferTupleCidKey key;
  ReorderBufferTupleCidEnt *ent;
  ForkNumber forkno;
  BlockNumber blockno;
  bool updated_mapping = false;

                                
  memset(&key, 0, sizeof(key));

  Assert(!BufferIsLocal(buffer));

     
                                                                           
                
     
  BufferGetTag(buffer, &key.relnode, &forkno, &blockno);

                                           
  Assert(forkno == MAIN_FORKNUM);
  Assert(blockno == ItemPointerGetBlockNumber(&htup->t_self));

  ItemPointerCopy(&htup->t_self, &key.tid);

restart:
  ent = (ReorderBufferTupleCidEnt *)hash_search(tuplecid_data, (void *)&key, HASH_FIND, NULL);

     
                                                                         
                                                                      
                                                                       
               
     
  if (ent == NULL && !updated_mapping)
  {
    UpdateLogicalMappings(tuplecid_data, htup->t_tableOid, snapshot);
                                                        
    updated_mapping = true;
    goto restart;
  }
  else if (ent == NULL)
  {
    return false;
  }

  if (cmin)
  {
    *cmin = ent->cmin;
  }
  if (cmax)
  {
    *cmax = ent->cmax;
  }
  return true;
}
