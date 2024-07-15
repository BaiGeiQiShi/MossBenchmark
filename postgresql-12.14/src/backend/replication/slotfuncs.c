                                                                            
   
               
                                              
   
                                                                
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xlog_internal.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "replication/decode.h"
#include "replication/slot.h"
#include "replication/logical.h"
#include "replication/logicalfuncs.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/pg_lsn.h"
#include "utils/resowner.h"

static void
check_permissions(void)
{
  if (!superuser() && !has_rolreplication(GetUserId()))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser or replication role to use replication slots"))));
  }
}

   
                                                                     
                                                                        
         
   
                                                                      
                                                                
   
static void
create_physical_replication_slot(char *name, bool immediately_reserve, bool temporary, XLogRecPtr restart_lsn)
{
  Assert(!MyReplicationSlot);

                                                                       
  ReplicationSlotCreate(name, false, temporary ? RS_TEMPORARY : RS_PERSISTENT);

  if (immediately_reserve)
  {
                                              
    if (XLogRecPtrIsInvalid(restart_lsn))
    {
      ReplicationSlotReserveWal();
    }
    else
    {
      MyReplicationSlot->data.restart_lsn = restart_lsn;
    }

                                 
    ReplicationSlotMarkDirty();
    ReplicationSlotSave();
  }
}

   
                                                                    
                     
   
Datum
pg_create_physical_replication_slot(PG_FUNCTION_ARGS)
{
  Name name = PG_GETARG_NAME(0);
  bool immediately_reserve = PG_GETARG_BOOL(1);
  bool temporary = PG_GETARG_BOOL(2);
  Datum values[2];
  bool nulls[2];
  TupleDesc tupdesc;
  HeapTuple tuple;
  Datum result;

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  check_permissions();

  CheckSlotRequirements();

  create_physical_replication_slot(NameStr(*name), immediately_reserve, temporary, InvalidXLogRecPtr);

  values[0] = NameGetDatum(&MyReplicationSlot->data.name);
  nulls[0] = false;

  if (immediately_reserve)
  {
    values[1] = LSNGetDatum(MyReplicationSlot->data.restart_lsn);
    nulls[1] = false;
  }
  else
  {
    nulls[1] = true;
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);
  result = HeapTupleGetDatum(tuple);

  ReplicationSlotRelease();

  PG_RETURN_DATUM(result);
}

   
                                                                    
                                                                        
         
   
                                                                              
                                                                     
   
static void
create_logical_replication_slot(char *name, char *plugin, bool temporary, XLogRecPtr restart_lsn, bool find_startpoint)
{
  LogicalDecodingContext *ctx = NULL;

  Assert(!MyReplicationSlot);

     
                                                                             
                                                                       
                                                                             
                                                                            
                                                                             
                    
     
  ReplicationSlotCreate(name, true, temporary ? RS_TEMPORARY : RS_EPHEMERAL);

     
                                                                         
                                                                             
     
                                                                          
                                                     
     
  ctx = CreateInitDecodingContext(plugin, NIL, false,                          
      restart_lsn, logical_read_local_xlog_page, NULL, NULL, NULL);

     
                                                                          
                              
     
  if (find_startpoint)
  {
    DecodingContextFindStartpoint(ctx);
  }

                                               
  FreeDecodingContext(ctx);
}

   
                                                             
   
Datum
pg_create_logical_replication_slot(PG_FUNCTION_ARGS)
{
  Name name = PG_GETARG_NAME(0);
  Name plugin = PG_GETARG_NAME(1);
  bool temporary = PG_GETARG_BOOL(2);
  Datum result;
  TupleDesc tupdesc;
  HeapTuple tuple;
  Datum values[2];
  bool nulls[2];

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  check_permissions();

  CheckLogicalDecodingRequirements();

  create_logical_replication_slot(NameStr(*name), NameStr(*plugin), temporary, InvalidXLogRecPtr, true);

  values[0] = NameGetDatum(&MyReplicationSlot->data.name);
  values[1] = LSNGetDatum(MyReplicationSlot->data.confirmed_flush);

  memset(nulls, 0, sizeof(nulls));

  tuple = heap_form_tuple(tupdesc, values, nulls);
  result = HeapTupleGetDatum(tuple);

                                                                      
  if (!temporary)
  {
    ReplicationSlotPersist();
  }
  ReplicationSlotRelease();

  PG_RETURN_DATUM(result);
}

   
                                                 
   
Datum
pg_drop_replication_slot(PG_FUNCTION_ARGS)
{
  Name name = PG_GETARG_NAME(0);

  check_permissions();

  CheckSlotRequirements();

  ReplicationSlotDrop(NameStr(*name), true);

  PG_RETURN_VOID();
}

   
                                                                        
   
Datum
pg_get_replication_slots(PG_FUNCTION_ARGS)
{
#define PG_GET_REPLICATION_SLOTS_COLS 11
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  int slotno;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not "
                                                                   "allowed in this context")));
  }

                                                    
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

     
                                                                         
                                                                           
                                                                    
     

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (slotno = 0; slotno < max_replication_slots; slotno++)
  {
    ReplicationSlot *slot = &ReplicationSlotCtl->replication_slots[slotno];
    ReplicationSlot slot_contents;
    Datum values[PG_GET_REPLICATION_SLOTS_COLS];
    bool nulls[PG_GET_REPLICATION_SLOTS_COLS];
    int i;

    if (!slot->in_use)
    {
      continue;
    }

                                                                            
    SpinLockAcquire(&slot->mutex);
    slot_contents = *slot;
    SpinLockRelease(&slot->mutex);

    memset(values, 0, sizeof(values));
    memset(nulls, 0, sizeof(nulls));

    i = 0;
    values[i++] = NameGetDatum(&slot_contents.data.name);

    if (slot_contents.data.database == InvalidOid)
    {
      nulls[i++] = true;
    }
    else
    {
      values[i++] = NameGetDatum(&slot_contents.data.plugin);
    }

    if (slot_contents.data.database == InvalidOid)
    {
      values[i++] = CStringGetTextDatum("physical");
    }
    else
    {
      values[i++] = CStringGetTextDatum("logical");
    }

    if (slot_contents.data.database == InvalidOid)
    {
      nulls[i++] = true;
    }
    else
    {
      values[i++] = ObjectIdGetDatum(slot_contents.data.database);
    }

    values[i++] = BoolGetDatum(slot_contents.data.persistency == RS_TEMPORARY);
    values[i++] = BoolGetDatum(slot_contents.active_pid != 0);

    if (slot_contents.active_pid != 0)
    {
      values[i++] = Int32GetDatum(slot_contents.active_pid);
    }
    else
    {
      nulls[i++] = true;
    }

    if (slot_contents.data.xmin != InvalidTransactionId)
    {
      values[i++] = TransactionIdGetDatum(slot_contents.data.xmin);
    }
    else
    {
      nulls[i++] = true;
    }

    if (slot_contents.data.catalog_xmin != InvalidTransactionId)
    {
      values[i++] = TransactionIdGetDatum(slot_contents.data.catalog_xmin);
    }
    else
    {
      nulls[i++] = true;
    }

    if (slot_contents.data.restart_lsn != InvalidXLogRecPtr)
    {
      values[i++] = LSNGetDatum(slot_contents.data.restart_lsn);
    }
    else
    {
      nulls[i++] = true;
    }

    if (slot_contents.data.confirmed_flush != InvalidXLogRecPtr)
    {
      values[i++] = LSNGetDatum(slot_contents.data.confirmed_flush);
    }
    else
    {
      nulls[i++] = true;
    }

    Assert(i == PG_GET_REPLICATION_SLOTS_COLS);

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  LWLockRelease(ReplicationSlotControlLock);

  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

   
                                                                        
   
                                                                             
                                                                            
                
   
static XLogRecPtr
pg_physical_replication_slot_advance(XLogRecPtr moveto)
{
  XLogRecPtr startlsn = MyReplicationSlot->data.restart_lsn;
  XLogRecPtr retlsn = startlsn;

  if (startlsn < moveto)
  {
    SpinLockAcquire(&MyReplicationSlot->mutex);
    MyReplicationSlot->data.restart_lsn = moveto;
    SpinLockRelease(&MyReplicationSlot->mutex);
    retlsn = moveto;

       
                                                                      
                                                                    
                                                                    
                       
       
    ReplicationSlotMarkDirty();
  }

  return retlsn;
}

   
                                                                       
   
                                                                            
                                                                   
   
                                                                               
                                                                            
                                                                               
                                          
   
static XLogRecPtr
pg_logical_replication_slot_advance(XLogRecPtr moveto)
{
  LogicalDecodingContext *ctx;
  ResourceOwner old_resowner = CurrentResourceOwner;
  XLogRecPtr startlsn;
  XLogRecPtr retlsn;

  PG_TRY();
  {
       
                                                                           
                                                                        
                        
       
    ctx = CreateDecodingContext(InvalidXLogRecPtr, NIL, true,                   
        logical_read_local_xlog_page, NULL, NULL, NULL);

       
                                                                          
                       
       
    startlsn = MyReplicationSlot->data.restart_lsn;

                                                                  
    retlsn = MyReplicationSlot->data.confirmed_flush;

                                           
    InvalidateSystemCaches();

                                                                 
    while ((!XLogRecPtrIsInvalid(startlsn) && startlsn < moveto) || (!XLogRecPtrIsInvalid(ctx->reader->EndRecPtr) && ctx->reader->EndRecPtr < moveto))
    {
      char *errm = NULL;
      XLogRecord *record;

         
                                                                       
                                                             
         
      record = XLogReadRecord(ctx->reader, startlsn, &errm);
      if (errm)
      {
        elog(ERROR, "%s", errm);
      }

                                         
      startlsn = InvalidXLogRecPtr;

         
                                                                   
                                                                    
                                                  
         
      if (record)
      {
        LogicalDecodingProcessRecord(ctx, ctx->reader);
      }

                                                           
      if (moveto <= ctx->reader->EndRecPtr)
      {
        break;
      }

      CHECK_FOR_INTERRUPTS();
    }

       
                                                                         
                                                                          
                                                           
       
    CurrentResourceOwner = old_resowner;

    if (ctx->reader->EndRecPtr != InvalidXLogRecPtr)
    {
      LogicalConfirmReceivedLocation(moveto);

         
                                                                        
                                                                
                                                                        
                                                                   
                                                                       
                                                                    
                                     
         
                                                                     
                                                                   
                                                                    
         
      ReplicationSlotMarkDirty();
    }

    retlsn = MyReplicationSlot->data.confirmed_flush;

                                              
    FreeDecodingContext(ctx);

    InvalidateSystemCaches();
  }
  PG_CATCH();
  {
                                      
    InvalidateSystemCaches();

    PG_RE_THROW();
  }
  PG_END_TRY();

  return retlsn;
}

   
                                                               
   
Datum
pg_replication_slot_advance(PG_FUNCTION_ARGS)
{
  Name slotname = PG_GETARG_NAME(0);
  XLogRecPtr moveto = PG_GETARG_LSN(1);
  XLogRecPtr endlsn;
  XLogRecPtr minlsn;
  TupleDesc tupdesc;
  Datum values[2];
  bool nulls[2];
  HeapTuple tuple;
  Datum result;

  Assert(!MyReplicationSlot);

  check_permissions();

  if (XLogRecPtrIsInvalid(moveto))
  {
    ereport(ERROR, (errmsg("invalid target WAL LSN")));
  }

                                                    
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

     
                                                                       
                                  
     
  if (!RecoveryInProgress())
  {
    moveto = Min(moveto, GetFlushRecPtr());
  }
  else
  {
    moveto = Min(moveto, GetXLogReplayRecPtr(&ThisTimeLineID));
  }

                                       
  ReplicationSlotAcquire(NameStr(*slotname), true);

                                                                           
  if (XLogRecPtrIsInvalid(MyReplicationSlot->data.restart_lsn))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot advance replication slot that has not previously reserved WAL")));
  }

     
                                                                            
                                                                           
                                                                        
                                               
     
  if (OidIsValid(MyReplicationSlot->data.database))
  {
    minlsn = MyReplicationSlot->data.confirmed_flush;
  }
  else
  {
    minlsn = MyReplicationSlot->data.restart_lsn;
  }

  if (moveto < minlsn)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot advance replication slot to %X/%X, minimum is %X/%X", (uint32)(moveto >> 32), (uint32)moveto, (uint32)(minlsn >> 32), (uint32)minlsn)));
  }

                                                             
  if (OidIsValid(MyReplicationSlot->data.database))
  {
    endlsn = pg_logical_replication_slot_advance(moveto);
  }
  else
  {
    endlsn = pg_physical_replication_slot_advance(moveto);
  }

  values[0] = NameGetDatum(&MyReplicationSlot->data.name);
  nulls[0] = false;

     
                                                                            
                                 
     
  ReplicationSlotsComputeRequiredXmin(false);
  ReplicationSlotsComputeRequiredLSN();

  ReplicationSlotRelease();

                                    
  values[1] = LSNGetDatum(endlsn);
  nulls[1] = false;

  tuple = heap_form_tuple(tupdesc, values, nulls);
  result = HeapTupleGetDatum(tuple);

  PG_RETURN_DATUM(result);
}

   
                                                  
   
static Datum
copy_replication_slot(FunctionCallInfo fcinfo, bool logical_slot)
{
  Name src_name = PG_GETARG_NAME(0);
  Name dst_name = PG_GETARG_NAME(1);
  ReplicationSlot *src = NULL;
  ReplicationSlot first_slot_contents;
  ReplicationSlot second_slot_contents;
  XLogRecPtr src_restart_lsn;
  bool src_islogical;
  bool temporary;
  char *plugin;
  Datum values[2];
  bool nulls[2];
  Datum result;
  TupleDesc tupdesc;
  HeapTuple tuple;

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  check_permissions();

  if (logical_slot)
  {
    CheckLogicalDecodingRequirements();
  }
  else
  {
    CheckSlotRequirements();
  }

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

     
                                                                           
                                                                           
                                                                          
                                                                           
                                                                     
                                                                            
                                                                       
                                                                           
                                                                          
     
  for (int i = 0; i < max_replication_slots; i++)
  {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];

    if (s->in_use && strcmp(NameStr(s->data.name), NameStr(*src_name)) == 0)
    {
                                                         
      SpinLockAcquire(&s->mutex);
      first_slot_contents = *s;
      SpinLockRelease(&s->mutex);
      src = s;
      break;
    }
  }

  LWLockRelease(ReplicationSlotControlLock);

  if (src == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("replication slot \"%s\" does not exist", NameStr(*src_name))));
  }

  src_islogical = SlotIsLogical(&first_slot_contents);
  src_restart_lsn = first_slot_contents.data.restart_lsn;
  temporary = (first_slot_contents.data.persistency == RS_TEMPORARY);
  plugin = logical_slot ? NameStr(first_slot_contents.data.plugin) : NULL;

                                      
  if (src_islogical != logical_slot)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), src_islogical ? errmsg("cannot copy physical replication slot \"%s\" as a logical replication slot", NameStr(*src_name)) : errmsg("cannot copy logical replication slot \"%s\" as a physical replication slot", NameStr(*src_name))));
  }

                                                    
  if (XLogRecPtrIsInvalid(src_restart_lsn))
  {
    Assert(!logical_slot);
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), (errmsg("cannot copy a replication slot that doesn't reserve WAL"))));
  }

                                                
  if (PG_NARGS() >= 3)
  {
    temporary = PG_GETARG_BOOL(2);
  }
  if (PG_NARGS() >= 4)
  {
    Assert(logical_slot);
    plugin = NameStr(*(PG_GETARG_NAME(3)));
  }

                                      
  if (logical_slot)
  {
       
                                                                        
                                                                      
                                               
       
    create_logical_replication_slot(NameStr(*dst_name), plugin, temporary, src_restart_lsn, false);
  }
  else
  {
    create_physical_replication_slot(NameStr(*dst_name), true, temporary, src_restart_lsn);
  }

     
                                                                       
                                                                      
     
  {
    TransactionId copy_effective_xmin;
    TransactionId copy_effective_catalog_xmin;
    TransactionId copy_xmin;
    TransactionId copy_catalog_xmin;
    XLogRecPtr copy_restart_lsn;
    XLogRecPtr copy_confirmed_flush;
    bool copy_islogical;
    char *copy_name;

                                        
    SpinLockAcquire(&src->mutex);
    second_slot_contents = *src;
    SpinLockRelease(&src->mutex);

    copy_effective_xmin = second_slot_contents.effective_xmin;
    copy_effective_catalog_xmin = second_slot_contents.effective_catalog_xmin;

    copy_xmin = second_slot_contents.data.xmin;
    copy_catalog_xmin = second_slot_contents.data.catalog_xmin;
    copy_restart_lsn = second_slot_contents.data.restart_lsn;
    copy_confirmed_flush = second_slot_contents.data.confirmed_flush;

                             
    copy_name = NameStr(second_slot_contents.data.name);
    copy_islogical = SlotIsLogical(&second_slot_contents);

       
                                                                           
                                                                         
                                                                       
                                                                        
                                                       
       
                                                                        
                                      
       
    if (copy_restart_lsn < src_restart_lsn || src_islogical != copy_islogical || strcmp(copy_name, NameStr(*src_name)) != 0)
    {
      ereport(ERROR, (errmsg("could not copy replication slot \"%s\"", NameStr(*src_name)), errdetail("The source replication slot was modified incompatibly during the copy operation.")));
    }

                                                         
    if (src_islogical && XLogRecPtrIsInvalid(copy_confirmed_flush))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot copy unfinished logical replication slot \"%s\"", NameStr(*src_name)), errhint("Retry when the source replication slot's confirmed_flush_lsn is valid.")));
    }

                                     
    SpinLockAcquire(&MyReplicationSlot->mutex);
    MyReplicationSlot->effective_xmin = copy_effective_xmin;
    MyReplicationSlot->effective_catalog_xmin = copy_effective_catalog_xmin;

    MyReplicationSlot->data.xmin = copy_xmin;
    MyReplicationSlot->data.catalog_xmin = copy_catalog_xmin;
    MyReplicationSlot->data.restart_lsn = copy_restart_lsn;
    MyReplicationSlot->data.confirmed_flush = copy_confirmed_flush;
    SpinLockRelease(&MyReplicationSlot->mutex);

    ReplicationSlotMarkDirty();
    ReplicationSlotsComputeRequiredXmin(false);
    ReplicationSlotsComputeRequiredLSN();
    ReplicationSlotSave();

#ifdef USE_ASSERT_CHECKING
                                                 
    {
      XLogSegNo segno;

      XLByteToSeg(copy_restart_lsn, segno, wal_segment_size);
      Assert(XLogGetLastRemovedSegno() < segno);
    }
#endif
  }

                                                               
  if (logical_slot && !temporary)
  {
    ReplicationSlotPersist();
  }

                                           
  values[0] = NameGetDatum(dst_name);
  nulls[0] = false;
  if (!XLogRecPtrIsInvalid(MyReplicationSlot->data.confirmed_flush))
  {
    values[1] = LSNGetDatum(MyReplicationSlot->data.confirmed_flush);
    nulls[1] = false;
  }
  else
  {
    nulls[1] = true;
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);
  result = HeapTupleGetDatum(tuple);

  ReplicationSlotRelease();

  PG_RETURN_DATUM(result);
}

                                                      
Datum
pg_copy_logical_replication_slot_a(PG_FUNCTION_ARGS)
{
  return copy_replication_slot(fcinfo, true);
}

Datum
pg_copy_logical_replication_slot_b(PG_FUNCTION_ARGS)
{
  return copy_replication_slot(fcinfo, true);
}

Datum
pg_copy_logical_replication_slot_c(PG_FUNCTION_ARGS)
{
  return copy_replication_slot(fcinfo, true);
}

Datum
pg_copy_physical_replication_slot_a(PG_FUNCTION_ARGS)
{
  return copy_replication_slot(fcinfo, false);
}

Datum
pg_copy_physical_replication_slot_b(PG_FUNCTION_ARGS)
{
  return copy_replication_slot(fcinfo, false);
}
