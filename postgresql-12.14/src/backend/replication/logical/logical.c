                                                                            
             
                                               
   
                                                                
   
                  
                                               
   
         
                                                                        
                                                                  
                                                                            
                                                                        
                                                                          
                                                                             
                                                                             
                 
   
                                                                            
                                                                             
                                                                  
                                                                              
                                                                         
                       
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"

#include "access/xact.h"
#include "access/xlog_internal.h"

#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/reorderbuffer.h"
#include "replication/origin.h"
#include "replication/snapbuild.h"

#include "storage/proc.h"
#include "storage/procarray.h"

#include "utils/memutils.h"

                                  
typedef struct LogicalErrorCallbackState
{
  LogicalDecodingContext *ctx;
  const char *callback_name;
  XLogRecPtr report_location;
} LogicalErrorCallbackState;

                                             
static void
output_plugin_error_callback(void *arg);
static void
startup_cb_wrapper(LogicalDecodingContext *ctx, OutputPluginOptions *opt, bool is_init);
static void
shutdown_cb_wrapper(LogicalDecodingContext *ctx);
static void
begin_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn);
static void
commit_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr commit_lsn);
static void
change_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change);
static void
truncate_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, int nrelations, Relation relations[], ReorderBufferChange *change);
static void
message_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr message_lsn, bool transactional, const char *prefix, Size message_size, const char *message);

static void
LoadOutputPlugin(OutputPluginCallbacks *callbacks, char *plugin);

   
                                                                             
             
   
void
CheckLogicalDecodingRequirements(void)
{
  CheckSlotRequirements();

     
                                                                          
                           
     

  if (wal_level < WAL_LEVEL_LOGICAL)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical decoding requires wal_level >= logical")));
  }

  if (MyDatabaseId == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical decoding requires a database connection")));
  }

          
                                                 
     
                                                           
                                                                            
                      
                                                                            
                                               
                                                                       
                                                                     
                                                     
          
     
  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("logical decoding cannot be used while in recovery")));
  }
}

   
                                                       
                                                    
   
static LogicalDecodingContext *
StartupDecodingContext(List *output_plugin_options, XLogRecPtr start_lsn, TransactionId xmin_horizon, bool need_full_snapshot, bool fast_forward, XLogPageReadCB read_page, LogicalOutputPluginWriterPrepareWrite prepare_write, LogicalOutputPluginWriterWrite do_write, LogicalOutputPluginWriterUpdateProgress update_progress)
{
  ReplicationSlot *slot;
  MemoryContext context, old_context;
  LogicalDecodingContext *ctx;

                        
  slot = MyReplicationSlot;

  context = AllocSetContextCreate(CurrentMemoryContext, "Logical decoding context", ALLOCSET_DEFAULT_SIZES);
  old_context = MemoryContextSwitchTo(context);
  ctx = palloc0(sizeof(LogicalDecodingContext));

  ctx->context = context;

     
                                                                          
          
     
  if (!fast_forward)
  {
    LoadOutputPlugin(&ctx->callbacks, NameStr(slot->data.plugin));
  }

     
                                                                           
                                                                            
                                                                      
                        
     
                                                                             
                                                                  
                                                                        
                                                                      
                       
     
  if (!IsTransactionOrTransactionBlock())
  {
    LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
    MyPgXact->vacuumFlags |= PROC_IN_LOGICAL_DECODING;
    LWLockRelease(ProcArrayLock);
  }

  ctx->slot = slot;

  ctx->reader = XLogReaderAllocate(wal_segment_size, read_page, ctx);
  if (!ctx->reader)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }

  ctx->reorder = ReorderBufferAllocate();
  ctx->snapshot_builder = AllocateSnapshotBuilder(ctx->reorder, xmin_horizon, start_lsn, need_full_snapshot);

  ctx->reorder->private_data = ctx;

                                                                             
  ctx->reorder->begin = begin_cb_wrapper;
  ctx->reorder->apply_change = change_cb_wrapper;
  ctx->reorder->apply_truncate = truncate_cb_wrapper;
  ctx->reorder->commit = commit_cb_wrapper;
  ctx->reorder->message = message_cb_wrapper;

  ctx->out = makeStringInfo();
  ctx->prepare_write = prepare_write;
  ctx->write = do_write;
  ctx->update_progress = update_progress;

  ctx->output_plugin_options = output_plugin_options;

  ctx->fast_forward = fast_forward;

  MemoryContextSwitchTo(old_context);

  return ctx;
}

   
                                                          
   
                                                    
                                                                         
                                                                          
                                                                     
                                                                             
                                                                           
                                                                       
                                                                       
                                                    
                                                          
                                                                 
   
                                                                              
                                                                           
              
   
                                                                             
                     
   
LogicalDecodingContext *
CreateInitDecodingContext(char *plugin, List *output_plugin_options, bool need_full_snapshot, XLogRecPtr restart_lsn, XLogPageReadCB read_page, LogicalOutputPluginWriterPrepareWrite prepare_write, LogicalOutputPluginWriterWrite do_write, LogicalOutputPluginWriterUpdateProgress update_progress)
{
  TransactionId xmin_horizon = InvalidTransactionId;
  ReplicationSlot *slot;
  LogicalDecodingContext *ctx;
  MemoryContext old_context;

                        
  slot = MyReplicationSlot;

                                                                 
  if (slot == NULL)
  {
    elog(ERROR, "cannot perform logical decoding without an acquired slot");
  }

  if (plugin == NULL)
  {
    elog(ERROR, "cannot initialize logical decoding without a specified plugin");
  }

                                                                            
  if (SlotIsPhysical(slot))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot use physical replication slot for logical decoding")));
  }

  if (slot->data.database != MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("replication slot \"%s\" was not created in this database", NameStr(slot->data.name))));
  }

  if (IsTransactionState() && GetTopTransactionIdIfAny() != InvalidTransactionId)
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION), errmsg("cannot create logical replication slot in transaction that has performed writes")));
  }

                                             
  SpinLockAcquire(&slot->mutex);
  StrNCpy(NameStr(slot->data.plugin), plugin, NAMEDATALEN);
  SpinLockRelease(&slot->mutex);

  if (XLogRecPtrIsInvalid(restart_lsn))
  {
    ReplicationSlotReserveWal();
  }
  else
  {
    SpinLockAcquire(&slot->mutex);
    slot->data.restart_lsn = restart_lsn;
    SpinLockRelease(&slot->mutex);
  }

          
                                                                             
                                                                            
                                                     
                                                                             
                                                                            
           
     
                                                                           
                                                                            
                                                                  
                                                                
                                
     
                                                                            
                                                                           
                                                                          
                                                                          
                                                                           
                                                                            
                                           
                                                                 
     
          
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

  xmin_horizon = GetOldestSafeDecodingTransactionId(!need_full_snapshot);

  SpinLockAcquire(&slot->mutex);
  slot->effective_catalog_xmin = xmin_horizon;
  slot->data.catalog_xmin = xmin_horizon;
  if (need_full_snapshot)
  {
    slot->effective_xmin = xmin_horizon;
  }
  SpinLockRelease(&slot->mutex);

  ReplicationSlotsComputeRequiredXmin(true);

  LWLockRelease(ProcArrayLock);

  ReplicationSlotMarkDirty();
  ReplicationSlotSave();

  ctx = StartupDecodingContext(NIL, restart_lsn, xmin_horizon, need_full_snapshot, false, read_page, prepare_write, do_write, update_progress);

                                                  
  old_context = MemoryContextSwitchTo(ctx->context);
  if (ctx->callbacks.startup_cb != NULL)
  {
    startup_cb_wrapper(ctx, &ctx->options, true);
  }
  MemoryContextSwitchTo(old_context);

  ctx->reorder->output_rewrites = ctx->options.receive_rewrites;

  return ctx;
}

   
                                                                              
                 
   
             
                                                                       
                                                                         
                                                                         
                      
   
                         
                                         
   
                
                                              
   
                                                       
                                                                        
                 
   
                                                                              
                                                                           
              
   
                                                                             
                     
   
LogicalDecodingContext *
CreateDecodingContext(XLogRecPtr start_lsn, List *output_plugin_options, bool fast_forward, XLogPageReadCB read_page, LogicalOutputPluginWriterPrepareWrite prepare_write, LogicalOutputPluginWriterWrite do_write, LogicalOutputPluginWriterUpdateProgress update_progress)
{
  LogicalDecodingContext *ctx;
  ReplicationSlot *slot;
  MemoryContext old_context;

                        
  slot = MyReplicationSlot;

                                                                 
  if (slot == NULL)
  {
    elog(ERROR, "cannot perform logical decoding without an acquired slot");
  }

                                                                           
  if (SlotIsPhysical(slot))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), (errmsg("cannot use physical replication slot for logical decoding"))));
  }

  if (slot->data.database != MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), (errmsg("replication slot \"%s\" was not created in this database", NameStr(slot->data.name)))));
  }

  if (start_lsn == InvalidXLogRecPtr)
  {
                                     
    start_lsn = slot->data.confirmed_flush;
  }
  else if (start_lsn < slot->data.confirmed_flush)
  {
       
                                                                     
                                                                          
                                                                        
                                                                   
                                                                           
                    
       
    elog(DEBUG1, "cannot stream from %X/%X, minimum is %X/%X, forwarding", (uint32)(start_lsn >> 32), (uint32)start_lsn, (uint32)(slot->data.confirmed_flush >> 32), (uint32)slot->data.confirmed_flush);

    start_lsn = slot->data.confirmed_flush;
  }

  ctx = StartupDecodingContext(output_plugin_options, start_lsn, InvalidTransactionId, false, fast_forward, read_page, prepare_write, do_write, update_progress);

                                                  
  old_context = MemoryContextSwitchTo(ctx->context);
  if (ctx->callbacks.startup_cb != NULL)
  {
    startup_cb_wrapper(ctx, &ctx->options, false);
  }
  MemoryContextSwitchTo(old_context);

  ctx->reorder->output_rewrites = ctx->options.receive_rewrites;

  ereport(LOG, (errmsg("starting logical decoding for slot \"%s\"", NameStr(slot->data.name)), errdetail("Streaming transactions committing after %X/%X, reading WAL from %X/%X.", (uint32)(slot->data.confirmed_flush >> 32), (uint32)slot->data.confirmed_flush, (uint32)(slot->data.restart_lsn >> 32), (uint32)slot->data.restart_lsn)));

  return ctx;
}

   
                                                                          
   
bool
DecodingContextReady(LogicalDecodingContext *ctx)
{
  return SnapBuildCurrentState(ctx->snapshot_builder) == SNAPBUILD_CONSISTENT;
}

   
                                                                               
   
void
DecodingContextFindStartpoint(LogicalDecodingContext *ctx)
{
  XLogRecPtr startptr;
  ReplicationSlot *slot = ctx->slot;

                                                   
  startptr = slot->data.restart_lsn;

  elog(DEBUG1, "searching for logical decoding starting point, starting at %X/%X", (uint32)(slot->data.restart_lsn >> 32), (uint32)slot->data.restart_lsn);

                                            
  for (;;)
  {
    XLogRecord *record;
    char *err = NULL;

                                                  
    record = XLogReadRecord(ctx->reader, startptr, &err);
    if (err)
    {
      elog(ERROR, "%s", err);
    }
    if (!record)
    {
      elog(ERROR, "no record found");                       
    }

    startptr = InvalidXLogRecPtr;

    LogicalDecodingProcessRecord(ctx, ctx->reader);

                                                       
    if (DecodingContextReady(ctx))
    {
      break;
    }

    CHECK_FOR_INTERRUPTS();
  }

  SpinLockAcquire(&slot->mutex);
  slot->data.confirmed_flush = ctx->reader->EndRecPtr;
  SpinLockRelease(&slot->mutex);
}

   
                                                                       
                          
   
void
FreeDecodingContext(LogicalDecodingContext *ctx)
{
  if (ctx->callbacks.shutdown_cb != NULL)
  {
    shutdown_cb_wrapper(ctx);
  }

  ReorderBufferFree(ctx->reorder);
  FreeSnapshotBuilder(ctx->snapshot_builder);
  XLogReaderFree(ctx->reader);
  MemoryContextDelete(ctx->context);
}

   
                                                       
   
void
OutputPluginPrepareWrite(struct LogicalDecodingContext *ctx, bool last_write)
{
  if (!ctx->accept_writes)
  {
    elog(ERROR, "writes are only accepted in commit, begin and change callbacks");
  }

  ctx->prepare_write(ctx, ctx->write_location, ctx->write_xid, last_write);
  ctx->prepared_write = true;
}

   
                                                       
   
void
OutputPluginWrite(struct LogicalDecodingContext *ctx, bool last_write)
{
  if (!ctx->prepared_write)
  {
    elog(ERROR, "OutputPluginPrepareWrite needs to be called before OutputPluginWrite");
  }

  ctx->write(ctx, ctx->write_location, ctx->write_xid, last_write);
  ctx->prepared_write = false;
}

   
                                            
   
void
OutputPluginUpdateProgress(struct LogicalDecodingContext *ctx)
{
  if (!ctx->update_progress)
  {
    return;
  }

  ctx->update_progress(ctx, ctx->write_location, ctx->write_xid);
}

   
                                                                             
                                            
   
static void
LoadOutputPlugin(OutputPluginCallbacks *callbacks, char *plugin)
{
  LogicalOutputPluginInit plugin_init;

  plugin_init = (LogicalOutputPluginInit)load_external_function(plugin, "_PG_output_plugin_init", false, NULL);

  if (plugin_init == NULL)
  {
    elog(ERROR, "output plugins have to declare the _PG_output_plugin_init symbol");
  }

                                                         
  plugin_init(callbacks);

  if (callbacks->begin_cb == NULL)
  {
    elog(ERROR, "output plugins have to register a begin callback");
  }
  if (callbacks->change_cb == NULL)
  {
    elog(ERROR, "output plugins have to register a change callback");
  }
  if (callbacks->commit_cb == NULL)
  {
    elog(ERROR, "output plugins have to register a commit callback");
  }
}

static void
output_plugin_error_callback(void *arg)
{
  LogicalErrorCallbackState *state = (LogicalErrorCallbackState *)arg;

                                                 
  if (state->report_location != InvalidXLogRecPtr)
  {
    errcontext("slot \"%s\", output plugin \"%s\", in the %s callback, associated LSN %X/%X", NameStr(state->ctx->slot->data.name), NameStr(state->ctx->slot->data.plugin), state->callback_name, (uint32)(state->report_location >> 32), (uint32)state->report_location);
  }
  else
  {
    errcontext("slot \"%s\", output plugin \"%s\", in the %s callback", NameStr(state->ctx->slot->data.name), NameStr(state->ctx->slot->data.plugin), state->callback_name);
  }
}

static void
startup_cb_wrapper(LogicalDecodingContext *ctx, OutputPluginOptions *opt, bool is_init)
{
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

                                                       
  state.ctx = ctx;
  state.callback_name = "startup";
  state.report_location = InvalidXLogRecPtr;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = false;
  ctx->end_xact = false;

                                         
  ctx->callbacks.startup_cb(ctx, opt, is_init);

                                   
  error_context_stack = errcallback.previous;
}

static void
shutdown_cb_wrapper(LogicalDecodingContext *ctx)
{
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

                                                       
  state.ctx = ctx;
  state.callback_name = "shutdown";
  state.report_location = InvalidXLogRecPtr;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = false;
  ctx->end_xact = false;

                                         
  ctx->callbacks.shutdown_cb(ctx);

                                   
  error_context_stack = errcallback.previous;
}

   
                                                                                
                            
   
static void
begin_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn)
{
  LogicalDecodingContext *ctx = cache->private_data;
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

                                                       
  state.ctx = ctx;
  state.callback_name = "begin";
  state.report_location = txn->first_lsn;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = true;
  ctx->write_xid = txn->xid;
  ctx->write_location = txn->first_lsn;
  ctx->end_xact = false;

                                         
  ctx->callbacks.begin_cb(ctx, txn);

                                   
  error_context_stack = errcallback.previous;
}

static void
commit_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr commit_lsn)
{
  LogicalDecodingContext *ctx = cache->private_data;
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

                                                       
  state.ctx = ctx;
  state.callback_name = "commit";
  state.report_location = txn->final_lsn;                                 
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = true;
  ctx->write_xid = txn->xid;
  ctx->write_location = txn->end_lsn;                                      
  ctx->end_xact = true;

                                         
  ctx->callbacks.commit_cb(ctx, txn, commit_lsn);

                                   
  error_context_stack = errcallback.previous;
}

static void
change_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{
  LogicalDecodingContext *ctx = cache->private_data;
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

                                                       
  state.ctx = ctx;
  state.callback_name = "change";
  state.report_location = change->lsn;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = true;
  ctx->write_xid = txn->xid;

     
                                                                          
                                                                      
                                                                           
                                              
     
  ctx->write_location = change->lsn;

  ctx->end_xact = false;

  ctx->callbacks.change_cb(ctx, txn, relation, change);

                                   
  error_context_stack = errcallback.previous;
}

static void
truncate_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, int nrelations, Relation relations[], ReorderBufferChange *change)
{
  LogicalDecodingContext *ctx = cache->private_data;
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

  if (!ctx->callbacks.truncate_cb)
  {
    return;
  }

                                                       
  state.ctx = ctx;
  state.callback_name = "truncate";
  state.report_location = change->lsn;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = true;
  ctx->write_xid = txn->xid;

     
                                                                          
                                                                      
                                                                           
                                              
     
  ctx->write_location = change->lsn;

  ctx->end_xact = false;

  ctx->callbacks.truncate_cb(ctx, txn, nrelations, relations, change);

                                   
  error_context_stack = errcallback.previous;
}

bool
filter_by_origin_cb_wrapper(LogicalDecodingContext *ctx, RepOriginId origin_id)
{
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;
  bool ret;

  Assert(!ctx->fast_forward);

                                                       
  state.ctx = ctx;
  state.callback_name = "filter_by_origin";
  state.report_location = InvalidXLogRecPtr;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = false;
  ctx->end_xact = false;

                                         
  ret = ctx->callbacks.filter_by_origin_cb(ctx, origin_id);

                                   
  error_context_stack = errcallback.previous;

  return ret;
}

static void
message_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr message_lsn, bool transactional, const char *prefix, Size message_size, const char *message)
{
  LogicalDecodingContext *ctx = cache->private_data;
  LogicalErrorCallbackState state;
  ErrorContextCallback errcallback;

  Assert(!ctx->fast_forward);

  if (ctx->callbacks.message_cb == NULL)
  {
    return;
  }

                                                       
  state.ctx = ctx;
  state.callback_name = "message";
  state.report_location = message_lsn;
  errcallback.callback = output_plugin_error_callback;
  errcallback.arg = (void *)&state;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                        
  ctx->accept_writes = true;
  ctx->write_xid = txn != NULL ? txn->xid : InvalidTransactionId;
  ctx->write_location = message_lsn;
  ctx->end_xact = false;

                                         
  ctx->callbacks.message_cb(ctx, txn, message_lsn, transactional, prefix, message_size, message);

                                   
  error_context_stack = errcallback.previous;
}

   
                                                                               
                     
   
                                                                             
                                                                               
                                                                
   
void
LogicalIncreaseXminForSlot(XLogRecPtr current_lsn, TransactionId xmin)
{
  bool updated_xmin = false;
  ReplicationSlot *slot;

  slot = MyReplicationSlot;

  Assert(slot != NULL);

  SpinLockAcquire(&slot->mutex);

     
                                                                            
                                 
     
  if (TransactionIdPrecedesOrEquals(xmin, slot->data.catalog_xmin))
  {
  }

     
                                                                         
                                                                        
           
     
  else if (current_lsn <= slot->data.confirmed_flush)
  {
    slot->candidate_catalog_xmin = xmin;
    slot->candidate_xmin_lsn = current_lsn;

                                            
    updated_xmin = true;
  }

     
                                                                          
                                                                  
     
  else if (slot->candidate_xmin_lsn == InvalidXLogRecPtr)
  {
    slot->candidate_catalog_xmin = xmin;
    slot->candidate_xmin_lsn = current_lsn;
  }
  SpinLockRelease(&slot->mutex);

                                                                      
  if (updated_xmin)
  {
    LogicalConfirmReceivedLocation(slot->data.confirmed_flush);
  }
}

   
                                                                    
                                                            
   
                                                                        
                                                      
   
void
LogicalIncreaseRestartDecodingForSlot(XLogRecPtr current_lsn, XLogRecPtr restart_lsn)
{
  bool updated_lsn = false;
  ReplicationSlot *slot;

  slot = MyReplicationSlot;

  Assert(slot != NULL);
  Assert(restart_lsn != InvalidXLogRecPtr);
  Assert(current_lsn != InvalidXLogRecPtr);

  SpinLockAcquire(&slot->mutex);

                                                   
  if (restart_lsn <= slot->data.restart_lsn)
  {
  }

     
                                                                           
                                                                        
     
  else if (current_lsn <= slot->data.confirmed_flush)
  {
    slot->candidate_restart_valid = current_lsn;
    slot->candidate_restart_lsn = restart_lsn;

                                            
    updated_lsn = true;
  }

     
                                                                          
                                                                           
                                                                      
     
  if (slot->candidate_restart_valid == InvalidXLogRecPtr)
  {
    slot->candidate_restart_valid = current_lsn;
    slot->candidate_restart_lsn = restart_lsn;
    SpinLockRelease(&slot->mutex);

    elog(DEBUG1, "got new restart lsn %X/%X at %X/%X", (uint32)(restart_lsn >> 32), (uint32)restart_lsn, (uint32)(current_lsn >> 32), (uint32)current_lsn);
  }
  else
  {
    XLogRecPtr candidate_restart_lsn;
    XLogRecPtr candidate_restart_valid;
    XLogRecPtr confirmed_flush;

    candidate_restart_lsn = slot->candidate_restart_lsn;
    candidate_restart_valid = slot->candidate_restart_valid;
    confirmed_flush = slot->data.confirmed_flush;
    SpinLockRelease(&slot->mutex);

    elog(DEBUG1, "failed to increase restart lsn: proposed %X/%X, after %X/%X, current candidate %X/%X, current after %X/%X, flushed up to %X/%X", (uint32)(restart_lsn >> 32), (uint32)restart_lsn, (uint32)(current_lsn >> 32), (uint32)current_lsn, (uint32)(candidate_restart_lsn >> 32), (uint32)candidate_restart_lsn, (uint32)(candidate_restart_valid >> 32), (uint32)candidate_restart_valid, (uint32)(confirmed_flush >> 32), (uint32)confirmed_flush);
  }

                                                                           
  if (updated_lsn)
  {
    LogicalConfirmReceivedLocation(slot->data.confirmed_flush);
  }
}

   
                                                                           
   
void
LogicalConfirmReceivedLocation(XLogRecPtr lsn)
{
  Assert(lsn != InvalidXLogRecPtr);

                                                     
  if (MyReplicationSlot->candidate_xmin_lsn != InvalidXLogRecPtr || MyReplicationSlot->candidate_restart_valid != InvalidXLogRecPtr)
  {
    bool updated_xmin = false;
    bool updated_restart = false;

    SpinLockAcquire(&MyReplicationSlot->mutex);

    MyReplicationSlot->data.confirmed_flush = lsn;

                                                                     
    if (MyReplicationSlot->candidate_xmin_lsn != InvalidXLogRecPtr && MyReplicationSlot->candidate_xmin_lsn <= lsn)
    {
         
                                                                      
                                                                       
                                                                   
         
                                                                
                                                                        
                                                  
         
      if (TransactionIdIsValid(MyReplicationSlot->candidate_catalog_xmin) && MyReplicationSlot->data.catalog_xmin != MyReplicationSlot->candidate_catalog_xmin)
      {
        MyReplicationSlot->data.catalog_xmin = MyReplicationSlot->candidate_catalog_xmin;
        MyReplicationSlot->candidate_catalog_xmin = InvalidTransactionId;
        MyReplicationSlot->candidate_xmin_lsn = InvalidXLogRecPtr;
        updated_xmin = true;
      }
    }

    if (MyReplicationSlot->candidate_restart_valid != InvalidXLogRecPtr && MyReplicationSlot->candidate_restart_valid <= lsn)
    {
      Assert(MyReplicationSlot->candidate_restart_lsn != InvalidXLogRecPtr);

      MyReplicationSlot->data.restart_lsn = MyReplicationSlot->candidate_restart_lsn;
      MyReplicationSlot->candidate_restart_lsn = InvalidXLogRecPtr;
      MyReplicationSlot->candidate_restart_valid = InvalidXLogRecPtr;
      updated_restart = true;
    }

    SpinLockRelease(&MyReplicationSlot->mutex);

                                                                          
    if (updated_xmin || updated_restart)
    {
      ReplicationSlotMarkDirty();
      ReplicationSlotSave();
      elog(DEBUG1, "updated xmin: %u restart: %u", updated_xmin, updated_restart);
    }

       
                                                                       
                                                                      
                                                                        
                                 
       
    if (updated_xmin)
    {
      SpinLockAcquire(&MyReplicationSlot->mutex);
      MyReplicationSlot->effective_catalog_xmin = MyReplicationSlot->data.catalog_xmin;
      SpinLockRelease(&MyReplicationSlot->mutex);

      ReplicationSlotsComputeRequiredXmin(false);
      ReplicationSlotsComputeRequiredLSN();
    }
  }
  else
  {
    SpinLockAcquire(&MyReplicationSlot->mutex);
    MyReplicationSlot->data.confirmed_flush = lsn;
    SpinLockRelease(&MyReplicationSlot->mutex);
  }
}
