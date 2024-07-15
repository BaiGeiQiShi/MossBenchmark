                                                                            
   
          
                                                                       
   
                                                                         
                                                                        
   
                            
   
                                                                            
   

#include "postgres.h"

#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqmq.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"

static shm_mq_handle *pq_mq_handle;
static bool pq_mq_busy = false;
static pid_t pq_mq_parallel_master_pid = 0;
static pid_t pq_mq_parallel_master_backend_id = InvalidBackendId;

static void
pq_cleanup_redirect_to_shm_mq(dsm_segment *seg, Datum arg);
static void
mq_comm_reset(void);
static int
mq_flush(void);
static int
mq_flush_if_writable(void);
static bool
mq_is_send_pending(void);
static int
mq_putmessage(char msgtype, const char *s, size_t len);
static void
mq_putmessage_noblock(char msgtype, const char *s, size_t len);
static void
mq_startcopyout(void);
static void
mq_endcopyout(bool errorAbort);

static const PQcommMethods PqCommMqMethods = {mq_comm_reset, mq_flush, mq_flush_if_writable, mq_is_send_pending, mq_putmessage, mq_putmessage_noblock, mq_startcopyout, mq_endcopyout};

   
                                                                             
                  
   
void
pq_redirect_to_shm_mq(dsm_segment *seg, shm_mq_handle *mqh)
{
  PqCommMethods = &PqCommMqMethods;
  pq_mq_handle = mqh;
  whereToSendOutput = DestRemote;
  FrontendProtocol = PG_PROTOCOL_LATEST;
  on_dsm_detach(seg, pq_cleanup_redirect_to_shm_mq, (Datum)0);
}

   
                                                                            
                   
   
static void
pq_cleanup_redirect_to_shm_mq(dsm_segment *seg, Datum arg)
{
  pq_mq_handle = NULL;
  whereToSendOutput = DestNone;
}

   
                                                                            
                                
   
void
pq_set_parallel_master(pid_t pid, BackendId backend_id)
{
  Assert(PqCommMethods == &PqCommMqMethods);
  pq_mq_parallel_master_pid = pid;
  pq_mq_parallel_master_backend_id = backend_id;
}

static void
mq_comm_reset(void)
{
                      
}

static int
mq_flush(void)
{
                      
  return 0;
}

static int
mq_flush_if_writable(void)
{
                      
  return 0;
}

static bool
mq_is_send_pending(void)
{
                                       
  return 0;
}

   
                                                                        
                                                                           
                                                                       
   
static int
mq_putmessage(char msgtype, const char *s, size_t len)
{
  shm_mq_iovec iov[2];
  shm_mq_result result;

     
                                                                          
                                                                             
                                                                             
                                                                         
                                                                      
                                                        
     
  if (pq_mq_busy)
  {
    if (pq_mq_handle != NULL)
    {
      shm_mq_detach(pq_mq_handle);
    }
    pq_mq_handle = NULL;
    return EOF;
  }

     
                                                                         
                                                                             
                                                                             
                    
     
  if (pq_mq_handle == NULL)
  {
    return 0;
  }

  pq_mq_busy = true;

  iov[0].data = &msgtype;
  iov[0].len = 1;
  iov[1].data = s;
  iov[1].len = len;

  Assert(pq_mq_handle != NULL);

  for (;;)
  {
    result = shm_mq_sendv(pq_mq_handle, iov, 2, true);

    if (pq_mq_parallel_master_pid != 0)
    {
      SendProcSignal(pq_mq_parallel_master_pid, PROCSIG_PARALLEL_MESSAGE, pq_mq_parallel_master_backend_id);
    }

    if (result != SHM_MQ_WOULD_BLOCK)
    {
      break;
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, WAIT_EVENT_MQ_PUT_MESSAGE);
    ResetLatch(MyLatch);
    CHECK_FOR_INTERRUPTS();
  }

  pq_mq_busy = false;

  Assert(result == SHM_MQ_SUCCESS || result == SHM_MQ_DETACHED);
  if (result != SHM_MQ_SUCCESS)
  {
    return EOF;
  }
  return 0;
}

static void
mq_putmessage_noblock(char msgtype, const char *s, size_t len)
{
     
                                                                  
                                                                             
                                                                    
                                                                         
                    
     
  elog(ERROR, "not currently supported");
}

static void
mq_startcopyout(void)
{
                      
}

static void
mq_endcopyout(bool errorAbort)
{
                      
}

   
                                                                              
                               
   
void
pq_parse_errornotice(StringInfo msg, ErrorData *edata)
{
                                                  
  MemSet(edata, 0, sizeof(ErrorData));
  edata->elevel = ERROR;
  edata->assoc_context = CurrentMemoryContext;

                                              
  for (;;)
  {
    char code = pq_getmsgbyte(msg);
    const char *value;

    if (code == '\0')
    {
      pq_getmsgend(msg);
      break;
    }
    value = pq_getmsgrawstring(msg);

    switch (code)
    {
    case PG_DIAG_SEVERITY:
                                                             
      break;
    case PG_DIAG_SEVERITY_NONLOCALIZED:
      if (strcmp(value, "DEBUG") == 0)
      {
           
                                                           
                                                               
                                                            
           
        edata->elevel = DEBUG1;
      }
      else if (strcmp(value, "LOG") == 0)
      {
           
                                                               
                                                            
           
        edata->elevel = LOG;
      }
      else if (strcmp(value, "INFO") == 0)
      {
        edata->elevel = INFO;
      }
      else if (strcmp(value, "NOTICE") == 0)
      {
        edata->elevel = NOTICE;
      }
      else if (strcmp(value, "WARNING") == 0)
      {
        edata->elevel = WARNING;
      }
      else if (strcmp(value, "ERROR") == 0)
      {
        edata->elevel = ERROR;
      }
      else if (strcmp(value, "FATAL") == 0)
      {
        edata->elevel = FATAL;
      }
      else if (strcmp(value, "PANIC") == 0)
      {
        edata->elevel = PANIC;
      }
      else
      {
        elog(ERROR, "unrecognized error severity: \"%s\"", value);
      }
      break;
    case PG_DIAG_SQLSTATE:
      if (strlen(value) != 5)
      {
        elog(ERROR, "invalid SQLSTATE: \"%s\"", value);
      }
      edata->sqlerrcode = MAKE_SQLSTATE(value[0], value[1], value[2], value[3], value[4]);
      break;
    case PG_DIAG_MESSAGE_PRIMARY:
      edata->message = pstrdup(value);
      break;
    case PG_DIAG_MESSAGE_DETAIL:
      edata->detail = pstrdup(value);
      break;
    case PG_DIAG_MESSAGE_HINT:
      edata->hint = pstrdup(value);
      break;
    case PG_DIAG_STATEMENT_POSITION:
      edata->cursorpos = pg_strtoint32(value);
      break;
    case PG_DIAG_INTERNAL_POSITION:
      edata->internalpos = pg_strtoint32(value);
      break;
    case PG_DIAG_INTERNAL_QUERY:
      edata->internalquery = pstrdup(value);
      break;
    case PG_DIAG_CONTEXT:
      edata->context = pstrdup(value);
      break;
    case PG_DIAG_SCHEMA_NAME:
      edata->schema_name = pstrdup(value);
      break;
    case PG_DIAG_TABLE_NAME:
      edata->table_name = pstrdup(value);
      break;
    case PG_DIAG_COLUMN_NAME:
      edata->column_name = pstrdup(value);
      break;
    case PG_DIAG_DATATYPE_NAME:
      edata->datatype_name = pstrdup(value);
      break;
    case PG_DIAG_CONSTRAINT_NAME:
      edata->constraint_name = pstrdup(value);
      break;
    case PG_DIAG_SOURCE_FILE:
      edata->filename = pstrdup(value);
      break;
    case PG_DIAG_SOURCE_LINE:
      edata->lineno = pg_strtoint32(value);
      break;
    case PG_DIAG_SOURCE_FUNCTION:
      edata->funcname = pstrdup(value);
      break;
    default:
      elog(ERROR, "unrecognized error field code: %d", (int)code);
      break;
    }
  }
}
