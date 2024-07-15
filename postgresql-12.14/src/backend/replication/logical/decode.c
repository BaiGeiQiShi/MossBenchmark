                                                                             
   
            
                                                                           
                                                              
                                                                    
                                                                        
                                                       
   
         
                                                                
                                                                        
                                                                        
                                                               
                                                                     
                                                                       
            
   
                                                                         
                                                                        
   
                  
                                              
   
                                                                             
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/xlogutils.h"
#include "access/xlogreader.h"
#include "access/xlogrecord.h"

#include "catalog/pg_control.h"

#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/message.h"
#include "replication/reorderbuffer.h"
#include "replication/origin.h"
#include "replication/snapbuild.h"

#include "storage/standby.h"

typedef struct XLogRecordBuffer
{
  XLogRecPtr origptr;
  XLogRecPtr endptr;
  XLogReaderState *record;
} XLogRecordBuffer;

                   
static void
DecodeXLogOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeHeapOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeHeap2Op(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeXactOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeStandbyOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeLogicalMsgOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);

                                         
static void
DecodeInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeUpdate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeDelete(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeTruncate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeMultiInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeSpecConfirm(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);

static void
DecodeCommit(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_commit *parsed, TransactionId xid);
static void
DecodeAbort(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_abort *parsed, TransactionId xid);

                                      
static void
DecodeXLogTuple(char *data, Size len, ReorderBufferTupleBuf *tup);

   
                                                                            
                                                                           
            
   
                                                                           
                                                                              
                                                                    
                                                                              
                                                                         
                                                                           
                   
   
                                                                           
                                                                      
   
void
LogicalDecodingProcessRecord(LogicalDecodingContext *ctx, XLogReaderState *record)
{
  XLogRecordBuffer buf;

  buf.origptr = ctx->reader->ReadRecPtr;
  buf.endptr = ctx->reader->EndRecPtr;
  buf.record = record;

                                                         
  switch ((RmgrIds)XLogRecGetRmid(record))
  {
       
                                                                  
                           
       
  case RM_XLOG_ID:
    DecodeXLogOp(ctx, &buf);
    break;

  case RM_XACT_ID:
    DecodeXactOp(ctx, &buf);
    break;

  case RM_STANDBY_ID:
    DecodeStandbyOp(ctx, &buf);
    break;

  case RM_HEAP2_ID:
    DecodeHeap2Op(ctx, &buf);
    break;

  case RM_HEAP_ID:
    DecodeHeapOp(ctx, &buf);
    break;

  case RM_LOGICALMSG_ID:
    DecodeLogicalMsgOp(ctx, &buf);
    break;

       
                                                                      
                                                                      
              
       
  case RM_SMGR_ID:
  case RM_CLOG_ID:
  case RM_DBASE_ID:
  case RM_TBLSPC_ID:
  case RM_MULTIXACT_ID:
  case RM_RELMAP_ID:
  case RM_BTREE_ID:
  case RM_HASH_ID:
  case RM_GIN_ID:
  case RM_GIST_ID:
  case RM_SEQ_ID:
  case RM_SPGIST_ID:
  case RM_BRIN_ID:
  case RM_COMMIT_TS_ID:
  case RM_REPLORIGIN_ID:
  case RM_GENERIC_ID:
                                      
    ReorderBufferProcessXid(ctx->reorder, XLogRecGetXid(record), buf.origptr);
    break;
  case RM_NEXT_ID:
    elog(ERROR, "unexpected RM_NEXT_ID rmgr_id: %u", (RmgrIds)XLogRecGetRmid(buf.record));
  }
}

   
                                                                    
   
static void
DecodeXLogOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  SnapBuild *builder = ctx->snapshot_builder;
  uint8 info = XLogRecGetInfo(buf->record) & ~XLR_INFO_MASK;

  ReorderBufferProcessXid(ctx->reorder, XLogRecGetXid(buf->record), buf->origptr);

  switch (info)
  {
                                                          
  case XLOG_CHECKPOINT_SHUTDOWN:
  case XLOG_END_OF_RECOVERY:
    SnapBuildSerializationPoint(builder, buf->origptr);

    break;
  case XLOG_CHECKPOINT_ONLINE:

       
                                                                     
                               
       
    break;
  case XLOG_NOOP:
  case XLOG_NEXTOID:
  case XLOG_SWITCH:
  case XLOG_BACKUP_END:
  case XLOG_PARAMETER_CHANGE:
  case XLOG_RESTORE_POINT:
  case XLOG_FPW_CHANGE:
  case XLOG_FPI_FOR_HINT:
  case XLOG_FPI:
  case XLOG_OVERWRITE_CONTRECORD:
    break;
  default:
    elog(ERROR, "unexpected RM_XLOG_ID record type: %u", info);
  }
}

   
                                                                    
   
static void
DecodeXactOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  SnapBuild *builder = ctx->snapshot_builder;
  ReorderBuffer *reorder = ctx->reorder;
  XLogReaderState *r = buf->record;
  uint8 info = XLogRecGetInfo(r) & XLOG_XACT_OPMASK;

     
                                                                          
               
     
                                                                         
                                                                           
                                                   
     
  if (SnapBuildCurrentState(builder) < SNAPBUILD_FULL_SNAPSHOT && info != XLOG_XACT_ASSIGNMENT)
  {
    return;
  }

  switch (info)
  {
  case XLOG_XACT_COMMIT:
  case XLOG_XACT_COMMIT_PREPARED:
  {
    xl_xact_commit *xlrec;
    xl_xact_parsed_commit parsed;
    TransactionId xid;

    xlrec = (xl_xact_commit *)XLogRecGetData(r);
    ParseCommitRecord(XLogRecGetInfo(buf->record), xlrec, &parsed);

    if (!TransactionIdIsValid(parsed.twophase_xid))
    {
      xid = XLogRecGetXid(r);
    }
    else
    {
      xid = parsed.twophase_xid;
    }

    DecodeCommit(ctx, buf, &parsed, xid);
    break;
  }
  case XLOG_XACT_ABORT:
  case XLOG_XACT_ABORT_PREPARED:
  {
    xl_xact_abort *xlrec;
    xl_xact_parsed_abort parsed;
    TransactionId xid;

    xlrec = (xl_xact_abort *)XLogRecGetData(r);
    ParseAbortRecord(XLogRecGetInfo(buf->record), xlrec, &parsed);

    if (!TransactionIdIsValid(parsed.twophase_xid))
    {
      xid = XLogRecGetXid(r);
    }
    else
    {
      xid = parsed.twophase_xid;
    }

    DecodeAbort(ctx, buf, &parsed, xid);
    break;
  }
  case XLOG_XACT_ASSIGNMENT:
  {
    xl_xact_assignment *xlrec;
    int i;
    TransactionId *sub_xid;

    xlrec = (xl_xact_assignment *)XLogRecGetData(r);

    sub_xid = &xlrec->xsub[0];

    for (i = 0; i < xlrec->nsubxacts; i++)
    {
      ReorderBufferAssignChild(reorder, xlrec->xtop, *(sub_xid++), buf->origptr);
    }
    break;
  }
  case XLOG_XACT_PREPARE:

       
                                                                    
                                                                  
                                                                      
                                                                       
                                                               
                        
       
    ReorderBufferProcessXid(reorder, XLogRecGetXid(r), buf->origptr);
    break;
  default:
    elog(ERROR, "unexpected RM_XACT_ID record type: %u", info);
  }
}

   
                                                                       
   
static void
DecodeStandbyOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  SnapBuild *builder = ctx->snapshot_builder;
  XLogReaderState *r = buf->record;
  uint8 info = XLogRecGetInfo(r) & ~XLR_INFO_MASK;

  ReorderBufferProcessXid(ctx->reorder, XLogRecGetXid(r), buf->origptr);

  switch (info)
  {
  case XLOG_RUNNING_XACTS:
  {
    xl_running_xacts *running = (xl_running_xacts *)XLogRecGetData(r);

    SnapBuildProcessRunningXacts(builder, buf->origptr, running);

       
                                                              
                                                                  
                                                                   
                                                                 
                                                              
                                                                 
                                     
       
    ReorderBufferAbortOld(ctx->reorder, running->oldestRunningXid);
  }
  break;
  case XLOG_STANDBY_LOCK:
    break;
  case XLOG_INVALIDATIONS:
  {
    xl_invalidations *invalidations = (xl_invalidations *)XLogRecGetData(r);

    if (!ctx->fast_forward)
    {
      ReorderBufferImmediateInvalidation(ctx->reorder, invalidations->nmsgs, invalidations->msgs);
    }
  }
  break;
  default:
    elog(ERROR, "unexpected RM_STANDBY_ID record type: %u", info);
  }
}

   
                                                                     
   
static void
DecodeHeap2Op(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  uint8 info = XLogRecGetInfo(buf->record) & XLOG_HEAP_OPMASK;
  TransactionId xid = XLogRecGetXid(buf->record);
  SnapBuild *builder = ctx->snapshot_builder;

  ReorderBufferProcessXid(ctx->reorder, xid, buf->origptr);

     
                                                                           
                                
     
  if (SnapBuildCurrentState(builder) < SNAPBUILD_FULL_SNAPSHOT || ctx->fast_forward)
  {
    return;
  }

  switch (info)
  {
  case XLOG_HEAP2_MULTI_INSERT:
    if (!ctx->fast_forward && SnapBuildProcessChange(builder, xid, buf->origptr))
    {
      DecodeMultiInsert(ctx, buf);
    }
    break;
  case XLOG_HEAP2_NEW_CID:
  {
    xl_heap_new_cid *xlrec;

    xlrec = (xl_heap_new_cid *)XLogRecGetData(buf->record);
    SnapBuildProcessNewCid(builder, xid, buf->origptr, xlrec);

    break;
  }
  case XLOG_HEAP2_REWRITE:

       
                                                                       
                                                                  
                                                       
       
    break;

       
                                                                       
                      
       
  case XLOG_HEAP2_FREEZE_PAGE:
  case XLOG_HEAP2_CLEAN:
  case XLOG_HEAP2_CLEANUP_INFO:
  case XLOG_HEAP2_VISIBLE:
  case XLOG_HEAP2_LOCK_UPDATED:
    break;
  default:
    elog(ERROR, "unexpected RM_HEAP2_ID record type: %u", info);
  }
}

   
                                                                    
   
static void
DecodeHeapOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  uint8 info = XLogRecGetInfo(buf->record) & XLOG_HEAP_OPMASK;
  TransactionId xid = XLogRecGetXid(buf->record);
  SnapBuild *builder = ctx->snapshot_builder;

  ReorderBufferProcessXid(ctx->reorder, xid, buf->origptr);

     
                                                                           
                                     
     
  if (SnapBuildCurrentState(builder) < SNAPBUILD_FULL_SNAPSHOT || ctx->fast_forward)
  {
    return;
  }

  switch (info)
  {
  case XLOG_HEAP_INSERT:
    if (SnapBuildProcessChange(builder, xid, buf->origptr))
    {
      DecodeInsert(ctx, buf);
    }
    break;

       
                                                              
                                                                  
                                                 
       
  case XLOG_HEAP_HOT_UPDATE:
  case XLOG_HEAP_UPDATE:
    if (SnapBuildProcessChange(builder, xid, buf->origptr))
    {
      DecodeUpdate(ctx, buf);
    }
    break;

  case XLOG_HEAP_DELETE:
    if (SnapBuildProcessChange(builder, xid, buf->origptr))
    {
      DecodeDelete(ctx, buf);
    }
    break;

  case XLOG_HEAP_TRUNCATE:
    if (SnapBuildProcessChange(builder, xid, buf->origptr))
    {
      DecodeTruncate(ctx, buf);
    }
    break;

  case XLOG_HEAP_INPLACE:

       
                                                                     
                                                                   
                                                                
                          
       
                                                                       
                                                           
                                                                    
                                                               
                                                               
                                                                      
                                                                       
       
    if (!TransactionIdIsValid(xid))
    {
      break;
    }

    SnapBuildProcessChange(builder, xid, buf->origptr);
    ReorderBufferXidSetCatalogChanges(ctx->reorder, xid, buf->origptr);
    break;

  case XLOG_HEAP_CONFIRM:
    if (SnapBuildProcessChange(builder, xid, buf->origptr))
    {
      DecodeSpecConfirm(ctx, buf);
    }
    break;

  case XLOG_HEAP_LOCK:
                                                     
    break;

  default:
    elog(ERROR, "unexpected RM_HEAP_ID record type: %u", info);
    break;
  }
}

static inline bool
FilterByOrigin(LogicalDecodingContext *ctx, RepOriginId origin_id)
{
  if (ctx->callbacks.filter_by_origin_cb == NULL)
  {
    return false;
  }

  return filter_by_origin_cb_wrapper(ctx, origin_id);
}

   
                                                                          
   
static void
DecodeLogicalMsgOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  SnapBuild *builder = ctx->snapshot_builder;
  XLogReaderState *r = buf->record;
  TransactionId xid = XLogRecGetXid(r);
  uint8 info = XLogRecGetInfo(r) & ~XLR_INFO_MASK;
  RepOriginId origin_id = XLogRecGetOrigin(r);
  Snapshot snapshot;
  xl_logical_message *message;

  if (info != XLOG_LOGICAL_MESSAGE)
  {
    elog(ERROR, "unexpected RM_LOGICALMSG_ID record type: %u", info);
  }

  ReorderBufferProcessXid(ctx->reorder, XLogRecGetXid(r), buf->origptr);

     
                                                                           
                                 
     
  if (SnapBuildCurrentState(builder) < SNAPBUILD_FULL_SNAPSHOT || ctx->fast_forward)
  {
    return;
  }

  message = (xl_logical_message *)XLogRecGetData(r);

  if (message->dbId != ctx->slot->data.database || FilterByOrigin(ctx, origin_id))
  {
    return;
  }

  if (message->transactional && !SnapBuildProcessChange(builder, xid, buf->origptr))
  {
    return;
  }
  else if (!message->transactional && (SnapBuildCurrentState(builder) != SNAPBUILD_CONSISTENT || SnapBuildXactNeedsSkip(builder, buf->origptr)))
  {
    return;
  }

  snapshot = SnapBuildGetOrBuildSnapshot(builder, xid);
  ReorderBufferQueueMessage(ctx->reorder, xid, snapshot, buf->endptr, message->transactional, message->message,                             
                                                                                                                            
      message->message_size, message->message + message->prefix_size);
}

   
                                                                            
            
   
static void
DecodeCommit(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_commit *parsed, TransactionId xid)
{
  XLogRecPtr origin_lsn = InvalidXLogRecPtr;
  TimestampTz commit_time = parsed->xact_time;
  RepOriginId origin_id = XLogRecGetOrigin(buf->record);
  int i;

  if (parsed->xinfo & XACT_XINFO_HAS_ORIGIN)
  {
    origin_lsn = parsed->origin_lsn;
    commit_time = parsed->origin_timestamp;
  }

     
                                                                        
                                                                        
                 
     
  if (parsed->nmsgs > 0)
  {
    if (!ctx->fast_forward)
    {
      ReorderBufferAddInvalidations(ctx->reorder, xid, buf->origptr, parsed->nmsgs, parsed->msgs);
    }
       
                                                                             
                                                                        
                                                                           
                                                                            
                                                                             
                       
       
                                                                            
                                                    
       
    SnapBuildXidSetCatalogChanges(ctx->snapshot_builder, xid, parsed->nsubxacts, parsed->subxacts, buf->origptr);
  }

  SnapBuildCommitTxn(ctx->snapshot_builder, buf->origptr, xid, parsed->nsubxacts, parsed->subxacts);

          
                                                                            
                                                                       
             
     
                                                                     
                  
                                                                       
                                                                          
                                                                            
                                                      
                                                           
                                     
     
                                                                             
                                                                         
                                                                           
                                                                             
                                                                          
                                                                              
                                                                       
                                                                          
                                                                         
                         
         
     
  if (SnapBuildXactNeedsSkip(ctx->snapshot_builder, buf->origptr) || (parsed->dbId != InvalidOid && parsed->dbId != ctx->slot->data.database) || ctx->fast_forward || FilterByOrigin(ctx, origin_id))
  {
    for (i = 0; i < parsed->nsubxacts; i++)
    {
      ReorderBufferForget(ctx->reorder, parsed->subxacts[i], buf->origptr);
    }
    ReorderBufferForget(ctx->reorder, xid, buf->origptr);

    return;
  }

                                                                  
  for (i = 0; i < parsed->nsubxacts; i++)
  {
    ReorderBufferCommitChild(ctx->reorder, xid, parsed->subxacts[i], buf->origptr, buf->endptr);
  }

                                                                    
  ReorderBufferCommit(ctx->reorder, xid, buf->origptr, buf->endptr, commit_time, origin_id, origin_lsn);
}

   
                                                                          
                                   
   
static void
DecodeAbort(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_abort *parsed, TransactionId xid)
{
  int i;

  for (i = 0; i < parsed->nsubxacts; i++)
  {
    ReorderBufferAbort(ctx->reorder, parsed->subxacts[i], buf->record->EndRecPtr);
  }

  ReorderBufferAbort(ctx->reorder, xid, buf->record->EndRecPtr);
}

   
                                                                      
   
                                      
   
static void
DecodeInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  Size datalen;
  char *tupledata;
  Size tuplelen;
  XLogReaderState *r = buf->record;
  xl_heap_insert *xlrec;
  ReorderBufferChange *change;
  RelFileNode target_node;

  xlrec = (xl_heap_insert *)XLogRecGetData(r);

     
                                                                     
                                                                        
     
  if (!(xlrec->flags & XLH_INSERT_CONTAINS_NEW_TUPLE))
  {
    return;
  }

                                       
  XLogRecGetBlockTag(r, 0, &target_node, NULL, NULL);
  if (target_node.dbNode != ctx->slot->data.database)
  {
    return;
  }

                                                                    
  if (FilterByOrigin(ctx, XLogRecGetOrigin(r)))
  {
    return;
  }

  change = ReorderBufferGetChange(ctx->reorder);
  if (!(xlrec->flags & XLH_INSERT_IS_SPECULATIVE))
  {
    change->action = REORDER_BUFFER_CHANGE_INSERT;
  }
  else
  {
    change->action = REORDER_BUFFER_CHANGE_INTERNAL_SPEC_INSERT;
  }
  change->origin_id = XLogRecGetOrigin(r);

  memcpy(&change->data.tp.relnode, &target_node, sizeof(RelFileNode));

  tupledata = XLogRecGetBlockData(r, 0, &datalen);
  tuplelen = datalen - SizeOfHeapHeader;

  change->data.tp.newtuple = ReorderBufferGetTupleBuf(ctx->reorder, tuplelen);

  DecodeXLogTuple(tupledata, datalen, change->data.tp.newtuple);

  change->data.tp.clear_toast_afterwards = true;

  ReorderBufferQueueChange(ctx->reorder, XLogRecGetXid(r), buf->origptr, change);
}

   
                                                                               
                                                  
   
                                                                     
   
static void
DecodeUpdate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  XLogReaderState *r = buf->record;
  xl_heap_update *xlrec;
  ReorderBufferChange *change;
  char *data;
  RelFileNode target_node;

  xlrec = (xl_heap_update *)XLogRecGetData(r);

                                       
  XLogRecGetBlockTag(r, 0, &target_node, NULL, NULL);
  if (target_node.dbNode != ctx->slot->data.database)
  {
    return;
  }

                                                                    
  if (FilterByOrigin(ctx, XLogRecGetOrigin(r)))
  {
    return;
  }

  change = ReorderBufferGetChange(ctx->reorder);
  change->action = REORDER_BUFFER_CHANGE_UPDATE;
  change->origin_id = XLogRecGetOrigin(r);
  memcpy(&change->data.tp.relnode, &target_node, sizeof(RelFileNode));

  if (xlrec->flags & XLH_UPDATE_CONTAINS_NEW_TUPLE)
  {
    Size datalen;
    Size tuplelen;

    data = XLogRecGetBlockData(r, 0, &datalen);

    tuplelen = datalen - SizeOfHeapHeader;

    change->data.tp.newtuple = ReorderBufferGetTupleBuf(ctx->reorder, tuplelen);

    DecodeXLogTuple(data, datalen, change->data.tp.newtuple);
  }

  if (xlrec->flags & XLH_UPDATE_CONTAINS_OLD)
  {
    Size datalen;
    Size tuplelen;

                                                          
    data = XLogRecGetData(r) + SizeOfHeapUpdate;
    datalen = XLogRecGetDataLen(r) - SizeOfHeapUpdate;
    tuplelen = datalen - SizeOfHeapHeader;

    change->data.tp.oldtuple = ReorderBufferGetTupleBuf(ctx->reorder, tuplelen);

    DecodeXLogTuple(data, datalen, change->data.tp.oldtuple);
  }

  change->data.tp.clear_toast_afterwards = true;

  ReorderBufferQueueChange(ctx->reorder, XLogRecGetXid(r), buf->origptr, change);
}

   
                                                          
   
                                                     
   
static void
DecodeDelete(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  XLogReaderState *r = buf->record;
  xl_heap_delete *xlrec;
  ReorderBufferChange *change;
  RelFileNode target_node;

  xlrec = (xl_heap_delete *)XLogRecGetData(r);

                                       
  XLogRecGetBlockTag(r, 0, &target_node, NULL, NULL);
  if (target_node.dbNode != ctx->slot->data.database)
  {
    return;
  }

                                                                    
  if (FilterByOrigin(ctx, XLogRecGetOrigin(r)))
  {
    return;
  }

  change = ReorderBufferGetChange(ctx->reorder);

  if (xlrec->flags & XLH_DELETE_IS_SUPER)
  {
    change->action = REORDER_BUFFER_CHANGE_INTERNAL_SPEC_ABORT;
  }
  else
  {
    change->action = REORDER_BUFFER_CHANGE_DELETE;
  }

  change->origin_id = XLogRecGetOrigin(r);

  memcpy(&change->data.tp.relnode, &target_node, sizeof(RelFileNode));

                              
  if (xlrec->flags & XLH_DELETE_CONTAINS_OLD)
  {
    Size datalen = XLogRecGetDataLen(r) - SizeOfHeapDelete;
    Size tuplelen = datalen - SizeOfHeapHeader;

    Assert(XLogRecGetDataLen(r) > (SizeOfHeapDelete + SizeOfHeapHeader));

    change->data.tp.oldtuple = ReorderBufferGetTupleBuf(ctx->reorder, tuplelen);

    DecodeXLogTuple((char *)xlrec + SizeOfHeapDelete, datalen, change->data.tp.oldtuple);
  }

  change->data.tp.clear_toast_afterwards = true;

  ReorderBufferQueueChange(ctx->reorder, XLogRecGetXid(r), buf->origptr, change);
}

   
                                     
   
static void
DecodeTruncate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  XLogReaderState *r = buf->record;
  xl_heap_truncate *xlrec;
  ReorderBufferChange *change;

  xlrec = (xl_heap_truncate *)XLogRecGetData(r);

                                       
  if (xlrec->dbId != ctx->slot->data.database)
  {
    return;
  }

                                                                    
  if (FilterByOrigin(ctx, XLogRecGetOrigin(r)))
  {
    return;
  }

  change = ReorderBufferGetChange(ctx->reorder);
  change->action = REORDER_BUFFER_CHANGE_TRUNCATE;
  change->origin_id = XLogRecGetOrigin(r);
  if (xlrec->flags & XLH_TRUNCATE_CASCADE)
  {
    change->data.truncate.cascade = true;
  }
  if (xlrec->flags & XLH_TRUNCATE_RESTART_SEQS)
  {
    change->data.truncate.restart_seqs = true;
  }
  change->data.truncate.nrelids = xlrec->nrelids;
  change->data.truncate.relids = ReorderBufferGetRelids(ctx->reorder, xlrec->nrelids);
  memcpy(change->data.truncate.relids, xlrec->relids, xlrec->nrelids * sizeof(Oid));
  ReorderBufferQueueChange(ctx->reorder, XLogRecGetXid(r), buf->origptr, change);
}

   
                                                                         
   
                                                               
   
static void
DecodeMultiInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  XLogReaderState *r = buf->record;
  xl_heap_multi_insert *xlrec;
  int i;
  char *data;
  char *tupledata;
  Size tuplelen;
  RelFileNode rnode;

  xlrec = (xl_heap_multi_insert *)XLogRecGetData(r);

                                       
  XLogRecGetBlockTag(r, 0, &rnode, NULL, NULL);
  if (rnode.dbNode != ctx->slot->data.database)
  {
    return;
  }

                                                                    
  if (FilterByOrigin(ctx, XLogRecGetOrigin(r)))
  {
    return;
  }

  tupledata = XLogRecGetBlockData(r, 0, &tuplelen);

  data = tupledata;
  for (i = 0; i < xlrec->ntuples; i++)
  {
    ReorderBufferChange *change;
    xl_multi_insert_tuple *xlhdr;
    int datalen;
    ReorderBufferTupleBuf *tuple;

    change = ReorderBufferGetChange(ctx->reorder);
    change->action = REORDER_BUFFER_CHANGE_INSERT;
    change->origin_id = XLogRecGetOrigin(r);

    memcpy(&change->data.tp.relnode, &rnode, sizeof(RelFileNode));

       
                                                                       
                                                            
       
                                                                           
                                                                         
       
    if (xlrec->flags & XLH_INSERT_CONTAINS_NEW_TUPLE)
    {
      HeapTupleHeader header;

      xlhdr = (xl_multi_insert_tuple *)SHORTALIGN(data);
      data = ((char *)xlhdr) + SizeOfMultiInsertTuple;
      datalen = xlhdr->datalen;

      change->data.tp.newtuple = ReorderBufferGetTupleBuf(ctx->reorder, datalen);

      tuple = change->data.tp.newtuple;
      header = tuple->tuple.t_data;

                                  
      ItemPointerSetInvalid(&tuple->tuple.t_self);

         
                                                            
                       
         
      tuple->tuple.t_tableOid = InvalidOid;

      tuple->tuple.t_len = datalen + SizeofHeapTupleHeader;

      memset(header, 0, SizeofHeapTupleHeader);

      memcpy((char *)tuple->tuple.t_data + SizeofHeapTupleHeader, (char *)data, datalen);
      data += datalen;

      header->t_infomask = xlhdr->t_infomask;
      header->t_infomask2 = xlhdr->t_infomask2;
      header->t_hoff = xlhdr->t_hoff;
    }

       
                                                                        
                                                                       
             
       
    if (xlrec->flags & XLH_INSERT_LAST_IN_MULTI && (i + 1) == xlrec->ntuples)
    {
      change->data.tp.clear_toast_afterwards = true;
    }
    else
    {
      change->data.tp.clear_toast_afterwards = false;
    }

    ReorderBufferQueueChange(ctx->reorder, XLogRecGetXid(r), buf->origptr, change);
  }
  Assert(data == tupledata + tuplelen);
}

   
                                                                
   
                                                                          
                          
   
static void
DecodeSpecConfirm(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{
  XLogReaderState *r = buf->record;
  ReorderBufferChange *change;
  RelFileNode target_node;

                                       
  XLogRecGetBlockTag(r, 0, &target_node, NULL, NULL);
  if (target_node.dbNode != ctx->slot->data.database)
  {
    return;
  }

                                                                    
  if (FilterByOrigin(ctx, XLogRecGetOrigin(r)))
  {
    return;
  }

  change = ReorderBufferGetChange(ctx->reorder);
  change->action = REORDER_BUFFER_CHANGE_INTERNAL_SPEC_CONFIRM;
  change->origin_id = XLogRecGetOrigin(r);

  memcpy(&change->data.tp.relnode, &target_node, sizeof(RelFileNode));

  change->data.tp.clear_toast_afterwards = true;

  ReorderBufferQueueChange(ctx->reorder, XLogRecGetXid(r), buf->origptr, change);
}

   
                                                                              
                                                   
   
                                                                  
                                                 
   
static void
DecodeXLogTuple(char *data, Size len, ReorderBufferTupleBuf *tuple)
{
  xl_heap_header xlhdr;
  int datalen = len - SizeOfHeapHeader;
  HeapTupleHeader header;

  Assert(datalen >= 0);

  tuple->tuple.t_len = datalen + SizeofHeapTupleHeader;
  header = tuple->tuple.t_data;

                              
  ItemPointerSetInvalid(&tuple->tuple.t_self);

                                                                       
  tuple->tuple.t_tableOid = InvalidOid;

                                                           
  memcpy((char *)&xlhdr, data, SizeOfHeapHeader);

  memset(header, 0, SizeofHeapTupleHeader);

  memcpy(((char *)tuple->tuple.t_data) + SizeofHeapTupleHeader, data + SizeOfHeapHeader, datalen);

  header->t_infomask = xlhdr.t_infomask;
  header->t_infomask2 = xlhdr.t_infomask2;
  header->t_hoff = xlhdr.t_hoff;
}
