                                                                            
          
   
                                                  
   
                                                                        
                                                                        
                                                                        
                                                                         
                                                          
   
   
                                                                
                                       
                                                 
   
                                
   
                                                                            
   

#include "postgres.h"

#include "access/clog.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "libpq/pqformat.h"
#include "postmaster/postmaster.h"
#include "storage/lwlock.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

                                                                    
#define MAX_TXID ((uint64)PG_INT64_MAX)

                                     
typedef uint64 txid;

                                    
#define TXID_FMT UINT64_FORMAT

   
                                                                           
                                                       
   
#define USE_BSEARCH_IF_NXIP_GREATER 30

   
                                    
   
typedef struct
{
     
                                                        
     
                                                                          
     
  int32 __varsz;

  uint32 nxip;                                   
  txid xmin;
  txid xmax;
                                                 
  txid xip[FLEXIBLE_ARRAY_MEMBER];
} TxidSnapshot;

#define TXID_SNAPSHOT_SIZE(nxip) (offsetof(TxidSnapshot, xip) + sizeof(txid) * (nxip))
#define TXID_SNAPSHOT_MAX_NXIP ((MaxAllocSize - offsetof(TxidSnapshot, xip)) / sizeof(txid))

   
                            
   
typedef struct
{
  TransactionId last_xid;
  uint32 epoch;
} TxidEpoch;

   
                                 
   
static void
load_xid_epoch(TxidEpoch *state)
{
  FullTransactionId fullXid = ReadNextFullTransactionId();

  state->last_xid = XidFromFullTransactionId(fullXid);
  state->epoch = EpochFromFullTransactionId(fullXid);
}

   
                                                                              
   
                                                                           
                                                                        
                                                                            
                                                                              
           
   
                                                                             
                                                                            
                           
   
static bool
TransactionIdInRecentPast(uint64 xid_with_epoch, TransactionId *extracted_xid)
{
  uint32 xid_epoch = (uint32)(xid_with_epoch >> 32);
  TransactionId xid = (TransactionId)xid_with_epoch;
  uint32 now_epoch;
  TransactionId now_epoch_next_xid;
  FullTransactionId now_fullxid;

  now_fullxid = ReadNextFullTransactionId();
  now_epoch_next_xid = XidFromFullTransactionId(now_fullxid);
  now_epoch = EpochFromFullTransactionId(now_fullxid);

  if (extracted_xid != NULL)
  {
    *extracted_xid = xid;
  }

  if (!TransactionIdIsValid(xid))
  {
    return false;
  }

                                                                
  if (!TransactionIdIsNormal(xid))
  {
    return true;
  }

                                                               
  if (xid_with_epoch >= U64FromFullTransactionId(now_fullxid))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("transaction ID %s is in the future", psprintf(UINT64_FORMAT, xid_with_epoch))));
  }

     
                                                                           
                                                                             
                                                                       
                                                                             
                                                                          
     
  Assert(LWLockHeldByMe(CLogTruncationLock));

     
                                                                          
                                                                   
                                                                         
                                              
     
  if (xid_epoch + 1 < now_epoch || (xid_epoch + 1 == now_epoch && xid < now_epoch_next_xid) || TransactionIdPrecedes(xid, ShmemVariableCache->oldestClogXid))
  {
    return false;
  }

  return true;
}

   
                                                                         
   
static txid
convert_xid(TransactionId xid, const TxidEpoch *state)
{
  uint64 epoch;

                                  
  if (!TransactionIdIsNormal(xid))
  {
    return (txid)xid;
  }

                                                       
  epoch = (uint64)state->epoch;
  if (xid > state->last_xid && TransactionIdPrecedes(xid, state->last_xid))
  {
    epoch--;
  }
  else if (xid < state->last_xid && TransactionIdFollows(xid, state->last_xid))
  {
    epoch++;
  }

  return (epoch << 32) | xid;
}

   
                                     
   
static int
cmp_txid(const void *aa, const void *bb)
{
  txid a = *(const txid *)aa;
  txid b = *(const txid *)bb;

  if (a < b)
  {
    return -1;
  }
  if (a > b)
  {
    return 1;
  }
  return 0;
}

   
                                                                        
                   
   
                                                                             
                     
   
static void
sort_snapshot(TxidSnapshot *snap)
{
  txid last = 0;
  int nxip, idx1, idx2;

  if (snap->nxip > 1)
  {
    qsort(snap->xip, snap->nxip, sizeof(txid), cmp_txid);

                           
    nxip = snap->nxip;
    idx1 = idx2 = 0;
    while (idx1 < nxip)
    {
      if (snap->xip[idx1] != last)
      {
        last = snap->xip[idx2++] = snap->xip[idx1];
      }
      else
      {
        snap->nxip--;
      }
      idx1++;
    }
  }
}

   
                          
   
static bool
is_visible_txid(txid value, const TxidSnapshot *snap)
{
  if (value < snap->xmin)
  {
    return true;
  }
  else if (value >= snap->xmax)
  {
    return false;
  }
#ifdef USE_BSEARCH_IF_NXIP_GREATER
  else if (snap->nxip > USE_BSEARCH_IF_NXIP_GREATER)
  {
    void *res;

    res = bsearch(&value, snap->xip, snap->nxip, sizeof(txid), cmp_txid);
                                                    
    return (res) ? false : true;
  }
#endif
  else
  {
    uint32 i;

    for (i = 0; i < snap->nxip; i++)
    {
      if (value == snap->xip[i])
      {
        return false;
      }
    }
    return true;
  }
}

   
                                                                 
   

static StringInfo
buf_init(txid xmin, txid xmax)
{
  TxidSnapshot snap;
  StringInfo buf;

  snap.xmin = xmin;
  snap.xmax = xmax;
  snap.nxip = 0;

  buf = makeStringInfo();
  appendBinaryStringInfo(buf, (char *)&snap, TXID_SNAPSHOT_SIZE(0));
  return buf;
}

static void
buf_add_txid(StringInfo buf, txid xid)
{
  TxidSnapshot *snap = (TxidSnapshot *)buf->data;

                                       
  snap->nxip++;

  appendBinaryStringInfo(buf, (char *)&xid, sizeof(xid));
}

static TxidSnapshot *
buf_finalize(StringInfo buf)
{
  TxidSnapshot *snap = (TxidSnapshot *)buf->data;

  SET_VARSIZE(snap, buf->len);

                                 
  buf->data = NULL;
  pfree(buf);

  return snap;
}

   
                         
   
                                                          
   
static txid
str2txid(const char *s, const char **endp)
{
  txid val = 0;
  txid cutoff = MAX_TXID / 10;
  txid cutlim = MAX_TXID % 10;

  for (; *s; s++)
  {
    unsigned d;

    if (*s < '0' || *s > '9')
    {
      break;
    }
    d = *s - '0';

       
                          
       
    if (val > cutoff || (val == cutoff && d > cutlim))
    {
      val = 0;
      break;
    }

    val = val * 10 + d;
  }
  if (endp)
  {
    *endp = s;
  }
  return val;
}

   
                               
   
static TxidSnapshot *
parse_snapshot(const char *str)
{
  txid xmin;
  txid xmax;
  txid last_val = 0, val;
  const char *str_start = str;
  const char *endp;
  StringInfo buf;

  xmin = str2txid(str, &endp);
  if (*endp != ':')
  {
    goto bad_format;
  }
  str = endp + 1;

  xmax = str2txid(str, &endp);
  if (*endp != ':')
  {
    goto bad_format;
  }
  str = endp + 1;

                           
  if (xmin == 0 || xmax == 0 || xmin > xmax)
  {
    goto bad_format;
  }

                       
  buf = buf_init(xmin, xmax);

                        
  while (*str != '\0')
  {
                         
    val = str2txid(str, &endp);
    str = endp;

                                          
    if (val < xmin || val >= xmax || val < last_val)
    {
      goto bad_format;
    }

                         
    if (val != last_val)
    {
      buf_add_txid(buf, val);
    }
    last_val = val;

    if (*str == ',')
    {
      str++;
    }
    else if (*str != '\0')
    {
      goto bad_format;
    }
  }

  return buf_finalize(buf);

bad_format:
  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "txid_snapshot", str_start)));
  return NULL;                          
}

   
                     
   
                                                                     
                                                                     
                     
   

   
                               
   
                                                      
                                                                  
   
                                                                   
                       
   
Datum
txid_current(PG_FUNCTION_ARGS)
{
  txid val;
  TxidEpoch state;

     
                                                                           
                                                                             
                                                                           
                                         
     
  PreventCommandDuringRecovery("txid_current()");

  load_xid_epoch(&state);

  val = convert_xid(GetTopTransactionId(), &state);

  PG_RETURN_INT64(val);
}

   
                                                                          
        
   
Datum
txid_current_if_assigned(PG_FUNCTION_ARGS)
{
  txid val;
  TxidEpoch state;
  TransactionId topxid = GetTopTransactionIdIfAny();

  if (topxid == InvalidTransactionId)
  {
    PG_RETURN_NULL();
  }

  load_xid_epoch(&state);

  val = convert_xid(topxid, &state);

  PG_RETURN_INT64(val);
}

   
                                                 
   
                                           
   
                                                                     
   
Datum
txid_current_snapshot(PG_FUNCTION_ARGS)
{
  TxidSnapshot *snap;
  uint32 nxip, i;
  TxidEpoch state;
  Snapshot cur;

  cur = GetActiveSnapshot();
  if (cur == NULL)
  {
    elog(ERROR, "no active snapshot set");
  }

  load_xid_epoch(&state);

     
                                                                       
                                                                            
     
  StaticAssertStmt(MAX_BACKENDS * 2 <= TXID_SNAPSHOT_MAX_NXIP, "possible overflow in txid_current_snapshot()");

                
  nxip = cur->xcnt;
  snap = palloc(TXID_SNAPSHOT_SIZE(nxip));

            
  snap->xmin = convert_xid(cur->xmin, &state);
  snap->xmax = convert_xid(cur->xmax, &state);
  snap->nxip = nxip;
  for (i = 0; i < nxip; i++)
  {
    snap->xip[i] = convert_xid(cur->xip[i], &state);
  }

     
                                                                          
                                                                       
                                                                           
                                                                       
                                                                           
     
  sort_snapshot(snap);

                                                                          
  SET_VARSIZE(snap, TXID_SNAPSHOT_SIZE(snap->nxip));

  PG_RETURN_POINTER(snap);
}

   
                                                   
   
                                          
   
Datum
txid_snapshot_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  TxidSnapshot *snap;

  snap = parse_snapshot(str);

  PG_RETURN_POINTER(snap);
}

   
                                                    
   
                                           
   
Datum
txid_snapshot_out(PG_FUNCTION_ARGS)
{
  TxidSnapshot *snap = (TxidSnapshot *)PG_GETARG_VARLENA_P(0);
  StringInfoData str;
  uint32 i;

  initStringInfo(&str);

  appendStringInfo(&str, TXID_FMT ":", snap->xmin);
  appendStringInfo(&str, TXID_FMT ":", snap->xmax);

  for (i = 0; i < snap->nxip; i++)
  {
    if (i > 0)
    {
      appendStringInfoChar(&str, ',');
    }
    appendStringInfo(&str, TXID_FMT, snap->xip[i]);
  }

  PG_RETURN_CSTRING(str.data);
}

   
                                                      
   
                                                 
   
                                                      
   
Datum
txid_snapshot_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  TxidSnapshot *snap;
  txid last = 0;
  int nxip;
  int i;
  txid xmin, xmax;

                              
  nxip = pq_getmsgint(buf, 4);
  if (nxip < 0 || nxip > TXID_SNAPSHOT_MAX_NXIP)
  {
    goto bad_format;
  }

  xmin = pq_getmsgint64(buf);
  xmax = pq_getmsgint64(buf);
  if (xmin == 0 || xmax == 0 || xmin > xmax || xmax > MAX_TXID)
  {
    goto bad_format;
  }

  snap = palloc(TXID_SNAPSHOT_SIZE(nxip));
  snap->xmin = xmin;
  snap->xmax = xmax;

  for (i = 0; i < nxip; i++)
  {
    txid cur = pq_getmsgint64(buf);

    if (cur < last || cur < xmin || cur >= xmax)
    {
      goto bad_format;
    }

                             
    if (cur == last)
    {
      i--;
      nxip--;
      continue;
    }

    snap->xip[i] = cur;
    last = cur;
  }
  snap->nxip = nxip;
  SET_VARSIZE(snap, TXID_SNAPSHOT_SIZE(nxip));
  PG_RETURN_POINTER(snap);

bad_format:
  ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid external txid_snapshot data")));
  PG_RETURN_POINTER(NULL);                          
}

   
                                                   
   
                                                  
   
                                                      
   
Datum
txid_snapshot_send(PG_FUNCTION_ARGS)
{
  TxidSnapshot *snap = (TxidSnapshot *)PG_GETARG_VARLENA_P(0);
  StringInfoData buf;
  uint32 i;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, snap->nxip);
  pq_sendint64(&buf, snap->xmin);
  pq_sendint64(&buf, snap->xmax);
  for (i = 0; i < snap->nxip; i++)
  {
    pq_sendint64(&buf, snap->xip[i]);
  }
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                              
   
                                  
   
Datum
txid_visible_in_snapshot(PG_FUNCTION_ARGS)
{
  txid value = PG_GETARG_INT64(0);
  TxidSnapshot *snap = (TxidSnapshot *)PG_GETARG_VARLENA_P(1);

  PG_RETURN_BOOL(is_visible_txid(value, snap));
}

   
                                                  
   
                           
   
Datum
txid_snapshot_xmin(PG_FUNCTION_ARGS)
{
  TxidSnapshot *snap = (TxidSnapshot *)PG_GETARG_VARLENA_P(0);

  PG_RETURN_INT64(snap->xmin);
}

   
                                                  
   
                           
   
Datum
txid_snapshot_xmax(PG_FUNCTION_ARGS)
{
  TxidSnapshot *snap = (TxidSnapshot *)PG_GETARG_VARLENA_P(0);

  PG_RETURN_INT64(snap->xmax);
}

   
                                                       
   
                                          
   
Datum
txid_snapshot_xip(PG_FUNCTION_ARGS)
{
  FuncCallContext *fctx;
  TxidSnapshot *snap;
  txid value;

                                                                    
  if (SRF_IS_FIRSTCALL())
  {
    TxidSnapshot *arg = (TxidSnapshot *)PG_GETARG_VARLENA_P(0);

    fctx = SRF_FIRSTCALL_INIT();

                                      
    snap = MemoryContextAlloc(fctx->multi_call_memory_ctx, VARSIZE(arg));
    memcpy(snap, arg, VARSIZE(arg));

    fctx->user_fctx = snap;
  }

                                
  fctx = SRF_PERCALL_SETUP();
  snap = fctx->user_fctx;
  if (fctx->call_cntr < snap->nxip)
  {
    value = snap->xip[fctx->call_cntr];
    SRF_RETURN_NEXT(fctx, Int64GetDatum(value));
  }
  else
  {
    SRF_RETURN_DONE(fctx);
  }
}

   
                                                                      
                                             
   
                                                                    
                 
   
                                                                             
                                                                        
   
Datum
txid_status(PG_FUNCTION_ARGS)
{
  const char *status;
  uint64 xid_with_epoch = PG_GETARG_INT64(0);
  TransactionId xid;

     
                                                                            
                                  
     
  LWLockAcquire(CLogTruncationLock, LW_SHARED);
  if (TransactionIdInRecentPast(xid_with_epoch, &xid))
  {
    Assert(TransactionIdIsValid(xid));

       
                                                                    
                                                                      
                                                                           
                                                                       
                                                                          
                                                    
       
    if (TransactionIdIsInProgress(xid))
    {
      status = "in progress";
    }
    else if (TransactionIdDidCommit(xid))
    {
      status = "committed";
    }
    else
    {
                                           
      status = "aborted";
    }
  }
  else
  {
    status = NULL;
  }
  LWLockRelease(CLogTruncationLock);

  if (status == NULL)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_TEXT_P(cstring_to_text(status));
  }
}
