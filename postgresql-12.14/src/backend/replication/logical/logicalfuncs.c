                                                                            
   
                  
   
                                                                     
                                         
   
   
                                                                
   
                  
                                            
                                                                            
   

#include "postgres.h"

#include <unistd.h>

#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "access/xlog_internal.h"
#include "access/xlogutils.h"

#include "access/xact.h"

#include "catalog/pg_type.h"

#include "nodes/makefuncs.h"

#include "mb/pg_wchar.h"

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/pg_lsn.h"
#include "utils/regproc.h"
#include "utils/resowner.h"
#include "utils/lsyscache.h"

#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/logicalfuncs.h"
#include "replication/message.h"

#include "storage/fd.h"

                                       
typedef struct DecodingOutputState
{
  Tuplestorestate *tupstore;
  TupleDesc tupdesc;
  bool binary_output;
  int64 returned_rows;
} DecodingOutputState;

   
                                       
   
static void
LogicalOutputPrepareWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write)
{
  resetStringInfo(ctx->out);
}

   
                                                
   
static void
LogicalOutputWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write)
{
  Datum values[3];
  bool nulls[3];
  DecodingOutputState *p;

                                                     
  if (ctx->out->len > MaxAllocSize - VARHDRSZ)
  {
    elog(ERROR, "too much output for sql interface");
  }

  p = (DecodingOutputState *)ctx->output_writer_private;

  memset(nulls, 0, sizeof(nulls));
  values[0] = LSNGetDatum(lsn);
  values[1] = TransactionIdGetDatum(xid);

     
                                                                        
             
     
  if (!p->binary_output)
  {
    Assert(pg_verify_mbstr(GetDatabaseEncoding(), ctx->out->data, ctx->out->len, false));
  }

                                                                        
  values[2] = PointerGetDatum(cstring_to_text_with_len(ctx->out->data, ctx->out->len));

  tuplestore_putvalues(p->tupstore, p->tupdesc, values, nulls);
  p->returned_rows++;
}

static void
check_permissions(void)
{
  if (!superuser() && !has_rolreplication(GetUserId()))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser or replication role to use replication slots"))));
  }
}

int
logical_read_local_xlog_page(XLogReaderState *state, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *cur_page, TimeLineID *pageTLI)
{
  return read_local_xlog_page(state, targetPagePtr, reqLen, targetRecPtr, cur_page, pageTLI);
}

   
                                                                            
   
static Datum
pg_logical_slot_get_changes_guts(FunctionCallInfo fcinfo, bool confirm, bool binary)
{
  Name name;
  XLogRecPtr upto_lsn;
  int32 upto_nchanges;
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  XLogRecPtr end_of_wal;
  XLogRecPtr startptr;
  LogicalDecodingContext *ctx;
  ResourceOwner old_resowner = CurrentResourceOwner;
  ArrayType *arr;
  Size ndim;
  List *options = NIL;
  DecodingOutputState *p;

  check_permissions();

  CheckLogicalDecodingRequirements();

  if (PG_ARGISNULL(0))
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("slot name must not be null")));
  }
  name = PG_GETARG_NAME(0);

  if (PG_ARGISNULL(1))
  {
    upto_lsn = InvalidXLogRecPtr;
  }
  else
  {
    upto_lsn = PG_GETARG_LSN(1);
  }

  if (PG_ARGISNULL(2))
  {
    upto_nchanges = InvalidXLogRecPtr;
  }
  else
  {
    upto_nchanges = PG_GETARG_INT32(2);
  }

  if (PG_ARGISNULL(3))
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("options array must not be null")));
  }
  arr = PG_GETARG_ARRAYTYPE_P(3);

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }

                                
  p = palloc0(sizeof(DecodingOutputState));

  p->binary_output = binary;

                                                    
  if (get_call_result_type(fcinfo, NULL, &p->tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

                                 
  ndim = ARR_NDIM(arr);
  if (ndim > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("array must be one-dimensional")));
  }
  else if (array_contains_nulls(arr))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("array must not contain nulls")));
  }
  else if (ndim == 1)
  {
    int nelems;
    Datum *datum_opts;
    int i;

    Assert(ARR_ELEMTYPE(arr) == TEXTOID);

    deconstruct_array(arr, TEXTOID, -1, false, 'i', &datum_opts, NULL, &nelems);

    if (nelems % 2 != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("array must have even number of elements")));
    }

    for (i = 0; i < nelems; i += 2)
    {
      char *name = TextDatumGetCString(datum_opts[i]);
      char *opt = TextDatumGetCString(datum_opts[i + 1]);

      options = lappend(options, makeDefElem(name, (Node *)makeString(opt), -1));
    }
  }

  p->tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = p->tupstore;
  rsinfo->setDesc = p->tupdesc;

     
                                                                 
                                                                   
     
  if (!RecoveryInProgress())
  {
    end_of_wal = GetFlushRecPtr();
  }
  else
  {
    end_of_wal = GetXLogReplayRecPtr(&ThisTimeLineID);
  }

  ReplicationSlotAcquire(NameStr(*name), true);

  PG_TRY();
  {
                                           
    ctx = CreateDecodingContext(InvalidXLogRecPtr, options, false, logical_read_local_xlog_page, LogicalOutputPrepareWrite, LogicalOutputWrite, NULL);

    MemoryContextSwitchTo(oldcontext);

       
                                                                       
                     
       
    if (!binary && ctx->options.output_type != OUTPUT_PLUGIN_TEXTUAL_OUTPUT)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("logical decoding output plugin \"%s\" produces binary output, but function \"%s\" expects textual data", NameStr(MyReplicationSlot->data.plugin), format_procedure(fcinfo->flinfo->fn_oid))));
    }

    ctx->output_writer_private = p;

       
                                                                         
                                                                    
                                         
       
    startptr = MyReplicationSlot->data.restart_lsn;

                                           
    InvalidateSystemCaches();

                                            
    while ((startptr != InvalidXLogRecPtr && startptr < end_of_wal) || (ctx->reader->EndRecPtr != InvalidXLogRecPtr && ctx->reader->EndRecPtr < end_of_wal))
    {
      XLogRecord *record;
      char *errm = NULL;

      record = XLogReadRecord(ctx->reader, startptr, &errm);
      if (errm)
      {
        elog(ERROR, "%s", errm);
      }

         
                                                                       
                                                                   
         
      startptr = InvalidXLogRecPtr;

         
                                                                        
                                                    
         
      if (record != NULL)
      {
        LogicalDecodingProcessRecord(ctx, ctx->reader);
      }

                        
      if (upto_lsn != InvalidXLogRecPtr && upto_lsn <= ctx->reader->EndRecPtr)
      {
        break;
      }
      if (upto_nchanges != 0 && upto_nchanges <= p->returned_rows)
      {
        break;
      }
      CHECK_FOR_INTERRUPTS();
    }

    tuplestore_donestoring(tupstore);

       
                                                                         
                                                                          
                                                           
       
    CurrentResourceOwner = old_resowner;

       
                                                                       
                   
       
    if (ctx->reader->EndRecPtr != InvalidXLogRecPtr && confirm)
    {
      LogicalConfirmReceivedLocation(ctx->reader->EndRecPtr);

         
                                                                        
                                                                
                                                                        
                                                                   
                                                                       
                                                                    
                                     
         
                                                                    
                                                                         
                                                                       
         
      ReplicationSlotMarkDirty();
    }

                                              
    FreeDecodingContext(ctx);

    ReplicationSlotRelease();
    InvalidateSystemCaches();
  }
  PG_CATCH();
  {
                                      
    InvalidateSystemCaches();

    PG_RE_THROW();
  }
  PG_END_TRY();

  return (Datum)0;
}

   
                                                                        
   
Datum
pg_logical_slot_get_changes(PG_FUNCTION_ARGS)
{
  return pg_logical_slot_get_changes_guts(fcinfo, true, false);
}

   
                                                                        
   
Datum
pg_logical_slot_peek_changes(PG_FUNCTION_ARGS)
{
  return pg_logical_slot_get_changes_guts(fcinfo, false, false);
}

   
                                                                          
   
Datum
pg_logical_slot_get_binary_changes(PG_FUNCTION_ARGS)
{
  return pg_logical_slot_get_changes_guts(fcinfo, true, true);
}

   
                                                                          
   
Datum
pg_logical_slot_peek_binary_changes(PG_FUNCTION_ARGS)
{
  return pg_logical_slot_get_changes_guts(fcinfo, false, true);
}

   
                                                               
   
Datum
pg_logical_emit_message_bytea(PG_FUNCTION_ARGS)
{
  bool transactional = PG_GETARG_BOOL(0);
  char *prefix = text_to_cstring(PG_GETARG_TEXT_PP(1));
  bytea *data = PG_GETARG_BYTEA_PP(2);
  XLogRecPtr lsn;

  lsn = LogLogicalMessage(prefix, VARDATA_ANY(data), VARSIZE_ANY_EXHDR(data), transactional);
  PG_RETURN_LSN(lsn);
}

Datum
pg_logical_emit_message_text(PG_FUNCTION_ARGS)
{
                                     
  return pg_logical_emit_message_bytea(fcinfo);
}
