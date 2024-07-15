                                                                            
   
           
                                                         
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   

                                                                            
                                       
   
                                                                        
                                                                       
                         
   
                                                                               
                                                                              
                                                                          
                            
   
                                                                              
                                                             
   
                                                                          
                                                                         
                                                                       
                                                                        
                                                                             
                                                                        
   
                                                                       
                                                                         
                                              
   
                                                                           
                                                                             
                                                                        
                                                                         
                                                                             
                                                                            
                                                        
   
                                                                             
                                                                             
   
                                                                           
                                                                             
                                                                              
                                                                            
                                                                             
                        
   
                                                                          
                                                                             
                                                                            
                                                                              
                                                                           
                                                                           
                     
   
                                                                            
                                                                           
   
                                                                          
                                                                             
   
                                                                          
                                                                            
                                                                           
                                                                             
                                                                             
                                                                         
                                                  
   
                                                                            
                                                                        
                                                                             
                                                         
                                                                          
                                                                           
           
   
                                                                            
                                                                            
                                                                             
                                                                       
                                                                            
                                                                             
                                                                           
                                                                            
                                                                           
                                                          
   
                                                                        
                                                                              
                                                                                
                                                                         
                                                                             
                                                                  
   
                                                                              
                                                                          
                                                                            
                                                           
                                                                            
   

#include "postgres.h"

#include <limits.h>
#include <unistd.h>
#include <signal.h>

#include "access/parallel.h"
#include "access/slru.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/pg_database.h"
#include "commands/async.h"
#include "funcapi.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/sinval.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

   
                                                                       
                                                                        
                                                                          
                                                                          
                                                                          
                 
   
#define NOTIFY_PAYLOAD_MAX_LENGTH (BLCKSZ - NAMEDATALEN - 128)

   
                                                           
   
                                                                             
                                                                               
                                                                             
                                                                          
                                       
   
                                                                         
                                                     
   
typedef struct AsyncQueueEntry
{
  int length;                                             
  Oid dboid;                                    
  TransactionId xid;                   
  int32 srcPid;                        
  char data[NAMEDATALEN + NOTIFY_PAYLOAD_MAX_LENGTH];
} AsyncQueueEntry;

                                                                             
#define QUEUEALIGN(len) INTALIGN(len)

#define AsyncQueueEntryEmptySize (offsetof(AsyncQueueEntry, data) + 2)

   
                                                                               
   
typedef struct QueuePosition
{
  int page;                         
  int offset;                              
} QueuePosition;

#define QUEUE_POS_PAGE(x) ((x).page)
#define QUEUE_POS_OFFSET(x) ((x).offset)

#define SET_QUEUE_POS(x, y, z)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (x).page = (y);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (x).offset = (z);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#define QUEUE_POS_EQUAL(x, y) ((x).page == (y).page && (x).offset == (y).offset)

                                            
#define QUEUE_POS_MIN(x, y) (asyncQueuePagePrecedes((x).page, (y).page) ? (x) : (x).page != (y).page ? (y) : (x).offset < (y).offset ? (x) : (y))

                                           
#define QUEUE_POS_MAX(x, y) (asyncQueuePagePrecedes((x).page, (y).page) ? (y) : (x).page != (y).page ? (x) : (x).offset > (y).offset ? (x) : (y))

   
                                                  
   
typedef struct QueueBackendStatus
{
  int32 pid;                                         
  Oid dboid;                                                    
  QueuePosition pos;                                        
} QueueBackendStatus;

   
                                                                    
   
                                                                          
                        
   
                                                                               
                                                                          
                                                                           
                                             
   
                                                                           
                                                                            
                                                                               
                             
   
                                                                            
                                                                              
                                                                      
   
                                                                       
                                                                               
                        
   
typedef struct AsyncQueueControl
{
  QueuePosition head;                                                       
  QueuePosition tail;                                                           
                                                            
  int stopPage;                                                        
                                                
  TimestampTz lastQueueFillWarn;                                  
  QueueBackendStatus backend[FLEXIBLE_ARRAY_MEMBER];
                                                                          
} AsyncQueueControl;

static AsyncQueueControl *asyncQueueControl;

#define QUEUE_HEAD (asyncQueueControl->head)
#define QUEUE_TAIL (asyncQueueControl->tail)
#define QUEUE_STOP_PAGE (asyncQueueControl->stopPage)
#define QUEUE_BACKEND_PID(i) (asyncQueueControl->backend[i].pid)
#define QUEUE_BACKEND_DBOID(i) (asyncQueueControl->backend[i].dboid)
#define QUEUE_BACKEND_POS(i) (asyncQueueControl->backend[i].pos)

   
                                                                       
   
static SlruCtlData AsyncCtlData;

#define AsyncCtl (&AsyncCtlData)
#define QUEUE_PAGESIZE BLCKSZ
#define QUEUE_FULL_WARN_INTERVAL 5000                                 

   
                                                                          
                                                                  
                                                                     
                                                             
   
                                                                        
                                                         
   
                                                                        
                                                                          
                                                                         
                                                 
   
                                                                             
                                                                       
   
#define QUEUE_MAX_PAGE (SLRU_PAGES_PER_SEGMENT * 0x10000 - 1)

   
                                                                       
                                                                            
                                  
   
static List *listenChannels = NIL;                        

   
                                                                            
                                                                          
                                                                              
   
                                                                        
                                                                         
                                                                         
                                                      
   
typedef enum
{
  LISTEN_LISTEN,
  LISTEN_UNLISTEN,
  LISTEN_UNLISTEN_ALL
} ListenActionKind;

typedef struct
{
  ListenActionKind action;
  char channel[FLEXIBLE_ARRAY_MEMBER];                            
} ListenAction;

static List *pendingActions = NIL;                           

static List *upperPendingActions = NIL;                               

   
                                                                           
                                                                            
                                                                           
                                                      
   
                                                                        
                                                                         
                                                                         
                                                      
   
                                                                           
                                                                           
                                                                           
                                                   
   
typedef struct Notification
{
  char *channel;                   
  char *payload;                                    
} Notification;

static List *pendingNotifies = NIL;                            

static List *upperPendingNotifies = NIL;                               

   
                                                                             
                                                           
                                                    
                                                                             
                                     
   
volatile sig_atomic_t notifyInterruptPending = false;

                                                       
static bool unlistenExitRegistered = false;

                                                                           
static bool amRegisteredListener = false;

                                                                     
static bool backendHasSentNotifications = false;

                   
bool Trace_notify = false;

                               
static bool
asyncQueuePagePrecedes(int p, int q);
static void
queue_listen(ListenActionKind action, const char *channel);
static void
Async_UnlistenOnExit(int code, Datum arg);
static void
Exec_ListenPreCommit(void);
static void
Exec_ListenCommit(const char *channel);
static void
Exec_UnlistenCommit(const char *channel);
static void
Exec_UnlistenAllCommit(void);
static bool
IsListeningOn(const char *channel);
static void
asyncQueueUnregister(void);
static bool
asyncQueueIsFull(void);
static bool
asyncQueueAdvance(volatile QueuePosition *position, int entryLength);
static void
asyncQueueNotificationToEntry(Notification *n, AsyncQueueEntry *qe);
static ListCell *
asyncQueueAddEntries(ListCell *nextNotify);
static double
asyncQueueUsage(void);
static void
asyncQueueFillWarning(void);
static bool
SignalBackends(void);
static void
asyncQueueReadAllNotifications(void);
static bool
asyncQueueProcessPageEntries(volatile QueuePosition *current, QueuePosition stop, char *page_buffer, Snapshot snapshot);
static void
asyncQueueAdvanceTail(void);
static void
ProcessIncomingNotify(void);
static bool
AsyncExistsPendingNotify(const char *channel, const char *payload);
static void
ClearPendingActionsAndNotifies(void);

   
                                                        
   
                                                                             
                                                          
   
static bool
asyncQueuePagePrecedes(int p, int q)
{
  int diff;

     
                                                                            
                                     
     
  Assert(p >= 0 && p <= QUEUE_MAX_PAGE);
  Assert(q >= 0 && q <= QUEUE_MAX_PAGE);

  diff = p - q;
  if (diff >= ((QUEUE_MAX_PAGE + 1) / 2))
  {
    diff -= QUEUE_MAX_PAGE + 1;
  }
  else if (diff < -((QUEUE_MAX_PAGE + 1) / 2))
  {
    diff += QUEUE_MAX_PAGE + 1;
  }
  return diff < 0;
}

   
                                                  
   
Size
AsyncShmemSize(void)
{
  Size size;

                                            
  size = mul_size(MaxBackends + 1, sizeof(QueueBackendStatus));
  size = add_size(size, offsetof(AsyncQueueControl, backend));

  size = add_size(size, SimpleLruShmemSize(NUM_ASYNC_BUFFERS, 0));

  return size;
}

   
                                     
   
void
AsyncShmemInit(void)
{
  bool found;
  int slotno;
  Size size;

     
                                                          
     
                                                                            
                                                    
     
  size = mul_size(MaxBackends + 1, sizeof(QueueBackendStatus));
  size = add_size(size, offsetof(AsyncQueueControl, backend));

  asyncQueueControl = (AsyncQueueControl *)ShmemInitStruct("Async Queue Control", size, &found);

  if (!found)
  {
                                              
    int i;

    SET_QUEUE_POS(QUEUE_HEAD, 0, 0);
    SET_QUEUE_POS(QUEUE_TAIL, 0, 0);
    QUEUE_STOP_PAGE = 0;
    asyncQueueControl->lastQueueFillWarn = 0;
                                                                     
    for (i = 0; i <= MaxBackends; i++)
    {
      QUEUE_BACKEND_PID(i) = InvalidPid;
      QUEUE_BACKEND_DBOID(i) = InvalidOid;
      SET_QUEUE_POS(QUEUE_BACKEND_POS(i), 0, 0);
    }
  }

     
                                                   
     
  AsyncCtl->PagePrecedes = asyncQueuePagePrecedes;
  SimpleLruInit(AsyncCtl, "async", NUM_ASYNC_BUFFERS, 0, AsyncCtlLock, "pg_notify", LWTRANCHE_ASYNC_BUFFERS);
                                                                 
  AsyncCtl->do_fsync = false;

  if (!found)
  {
       
                                                                  
       
    (void)SlruScanDirectory(AsyncCtl, SlruScanDirCbDeleteAll, NULL);

                                           
    LWLockAcquire(AsyncCtlLock, LW_EXCLUSIVE);
    slotno = SimpleLruZeroPage(AsyncCtl, QUEUE_POS_PAGE(QUEUE_HEAD));
                                                                  
    SimpleLruWritePage(AsyncCtl, slotno);
    LWLockRelease(AsyncCtlLock);
  }
}

   
               
                                               
   
Datum
pg_notify(PG_FUNCTION_ARGS)
{
  const char *channel;
  const char *payload;

  if (PG_ARGISNULL(0))
  {
    channel = "";
  }
  else
  {
    channel = text_to_cstring(PG_GETARG_TEXT_PP(0));
  }

  if (PG_ARGISNULL(1))
  {
    payload = "";
  }
  else
  {
    payload = text_to_cstring(PG_GETARG_TEXT_PP(1));
  }

                                                                    
  PreventCommandDuringRecovery("NOTIFY");

  Async_Notify(channel, payload);

  PG_RETURN_VOID();
}

   
                
   
                                                
   
                                                      
                                                           
                                                           
   
void
Async_Notify(const char *channel, const char *payload)
{
  Notification *n;
  MemoryContext oldcontext;

  if (IsParallelWorker())
  {
    elog(ERROR, "cannot send notifications from a parallel worker");
  }

  if (Trace_notify)
  {
    elog(DEBUG1, "Async_Notify(%s)", channel);
  }

                                        
  if (!channel || !strlen(channel))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("channel name cannot be empty")));
  }

  if (strlen(channel) >= NAMEDATALEN)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("channel name too long")));
  }

  if (payload)
  {
    if (strlen(payload) >= NOTIFY_PAYLOAD_MAX_LENGTH)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("payload string too long")));
    }
  }

                                                            
  if (AsyncExistsPendingNotify(channel, payload))
  {
    return;
  }

     
                                                                            
                                    
     
  oldcontext = MemoryContextSwitchTo(CurTransactionContext);

  n = (Notification *)palloc(sizeof(Notification));
  n->channel = pstrdup(channel);
  if (payload)
  {
    n->payload = pstrdup(payload);
  }
  else
  {
    n->payload = "";
  }

     
                                                                            
                                                 
     
  pendingNotifies = lappend(pendingNotifies, n);

  MemoryContextSwitchTo(oldcontext);
}

   
                
                                                             
   
                                                     
                                                                        
            
   
static void
queue_listen(ListenActionKind action, const char *channel)
{
  MemoryContext oldcontext;
  ListenAction *actrec;

     
                                                                            
                                                                   
                                                                            
                                                                   
     
  oldcontext = MemoryContextSwitchTo(CurTransactionContext);

                                                                      
  actrec = (ListenAction *)palloc(offsetof(ListenAction, channel) + strlen(channel) + 1);
  actrec->action = action;
  strcpy(actrec->channel, channel);

  pendingActions = lappend(pendingActions, actrec);

  MemoryContextSwitchTo(oldcontext);
}

   
                
   
                                                
   
void
Async_Listen(const char *channel)
{
  if (Trace_notify)
  {
    elog(DEBUG1, "Async_Listen(%s,%d)", channel, MyProcPid);
  }

  queue_listen(LISTEN_LISTEN, channel);
}

   
                  
   
                                                  
   
void
Async_Unlisten(const char *channel)
{
  if (Trace_notify)
  {
    elog(DEBUG1, "Async_Unlisten(%s,%d)", channel, MyProcPid);
  }

                                                                       
  if (pendingActions == NIL && !unlistenExitRegistered)
  {
    return;
  }

  queue_listen(LISTEN_UNLISTEN, channel);
}

   
                     
   
                                                                     
   
void
Async_UnlistenAll(void)
{
  if (Trace_notify)
  {
    elog(DEBUG1, "Async_UnlistenAll(%d)", MyProcPid);
  }

                                                                       
  if (pendingActions == NIL && !unlistenExitRegistered)
  {
    return;
  }

  queue_listen(LISTEN_UNLISTEN_ALL, "");
}

   
                                                                            
                 
   
                                                                            
                                
   
Datum
pg_listening_channels(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  ListCell **lcp;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcontext;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

                                                                          
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                          
    lcp = (ListCell **)palloc(sizeof(ListCell *));
    *lcp = list_head(listenChannels);
    funcctx->user_fctx = (void *)lcp;

    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();
  lcp = (ListCell **)funcctx->user_fctx;

  while (*lcp != NULL)
  {
    char *channel = (char *)lfirst(*lcp);

    *lcp = lnext(*lcp);
    SRF_RETURN_NEXT(funcctx, CStringGetTextDatum(channel));
  }

  SRF_RETURN_DONE(funcctx);
}

   
                        
   
                                                                        
                                                                       
                                                     
   
static void
Async_UnlistenOnExit(int code, Datum arg)
{
  Exec_UnlistenAllCommit();
  asyncQueueUnregister();
}

   
                    
   
                                                       
                                                            
   
void
AtPrepare_Notify(void)
{
                                                                           
  if (pendingActions || pendingNotifies)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE a transaction that has executed LISTEN, UNLISTEN, or NOTIFY")));
  }
}

   
                    
   
                                                                        
          
   
                                                                        
                                                                     
                                                                    
                     
   
                                                                       
                                                                    
                                                           
   
void
PreCommit_Notify(void)
{
  ListCell *p;

  if (pendingActions == NIL && pendingNotifies == NIL)
  {
    return;                                          
  }

  if (Trace_notify)
  {
    elog(DEBUG1, "PreCommit_Notify");
  }

                                                         
  foreach (p, pendingActions)
  {
    ListenAction *actrec = (ListenAction *)lfirst(p);

    switch (actrec->action)
    {
    case LISTEN_LISTEN:
      Exec_ListenPreCommit();
      break;
    case LISTEN_UNLISTEN:
                                                
      break;
    case LISTEN_UNLISTEN_ALL:
                                                   
      break;
    }
  }

                                                                
  if (pendingNotifies)
  {
    ListCell *nextNotify;

       
                                                                          
                                                                           
                                                                       
                               
       
    (void)GetCurrentTransactionId();

       
                                                                       
                                                                       
                                                                       
                                                                      
                                                          
       
                                                                         
                                                                       
                                                                      
       
                                                                        
                                                                      
                                                                           
                                         
       
    LockSharedObject(DatabaseRelationId, InvalidOid, 0, AccessExclusiveLock);

                                                   
    backendHasSentNotifications = true;

    nextNotify = list_head(pendingNotifies);
    while (nextNotify != NULL)
    {
         
                                                                     
                                                                       
                                                                     
         
                                                                     
                                                                       
                                                                       
                                                                  
                                                                        
                                                               
         
      LWLockAcquire(AsyncQueueLock, LW_EXCLUSIVE);
      asyncQueueFillWarning();
      if (asyncQueueIsFull())
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many notifications in the NOTIFY queue")));
      }
      nextNotify = asyncQueueAddEntries(nextNotify);
      LWLockRelease(AsyncQueueLock);
    }
  }
}

   
                   
   
                                                                    
   
                                                             
   
void
AtCommit_Notify(void)
{
  ListCell *p;

     
                                                                         
                                
     
  if (!pendingActions && !pendingNotifies)
  {
    return;
  }

  if (Trace_notify)
  {
    elog(DEBUG1, "AtCommit_Notify");
  }

                                                   
  foreach (p, pendingActions)
  {
    ListenAction *actrec = (ListenAction *)lfirst(p);

    switch (actrec->action)
    {
    case LISTEN_LISTEN:
      Exec_ListenCommit(actrec->channel);
      break;
    case LISTEN_UNLISTEN:
      Exec_UnlistenCommit(actrec->channel);
      break;
    case LISTEN_UNLISTEN_ALL:
      Exec_UnlistenAllCommit();
      break;
    }
  }

                                                                     
  if (amRegisteredListener && listenChannels == NIL)
  {
    asyncQueueUnregister();
  }

                    
  ClearPendingActionsAndNotifies();
}

   
                                                            
   
                                                                             
   
static void
Exec_ListenPreCommit(void)
{
  QueuePosition head;
  QueuePosition max;
  int i;

     
                                                                       
                                                   
     
  if (amRegisteredListener)
  {
    return;
  }

  if (Trace_notify)
  {
    elog(DEBUG1, "Exec_ListenPreCommit(%d)", MyProcPid);
  }

     
                                                                         
                                                         
     
  if (!unlistenExitRegistered)
  {
    before_shmem_exit(Async_UnlistenOnExit, 0);
    unlistenExitRegistered = true;
  }

     
                                                         
     
                                                                            
                                                                            
                                                                        
                   
     
                                                                            
                                                                             
                                                                      
                                                                           
                                                                           
                                                                          
                                                                            
                                                                           
                                                                  
                                                                            
     
                                                                            
     
  LWLockAcquire(AsyncQueueLock, LW_EXCLUSIVE);
  head = QUEUE_HEAD;
  max = QUEUE_TAIL;
  if (QUEUE_POS_PAGE(max) != QUEUE_POS_PAGE(head))
  {
    for (i = 1; i <= MaxBackends; i++)
    {
      if (QUEUE_BACKEND_DBOID(i) == MyDatabaseId)
      {
        max = QUEUE_POS_MAX(max, QUEUE_BACKEND_POS(i));
      }
    }
  }
  QUEUE_BACKEND_POS(MyBackendId) = max;
  QUEUE_BACKEND_PID(MyBackendId) = MyProcPid;
  QUEUE_BACKEND_DBOID(MyBackendId) = MyDatabaseId;
  LWLockRelease(AsyncQueueLock);

                                                                          
  amRegisteredListener = true;

     
                                                                             
                                                                             
                                                         
     
                                                                             
                                                                         
                                                                          
                            
     
                                                                 
     
  if (!QUEUE_POS_EQUAL(max, head))
  {
    asyncQueueReadAllNotifications();
  }
}

   
                                                        
   
                                                                
   
static void
Exec_ListenCommit(const char *channel)
{
  MemoryContext oldcontext;

                                                              
  if (IsListeningOn(channel))
  {
    return;
  }

     
                                                 
     
                                                                            
                                                                         
                                                                             
            
     
  oldcontext = MemoryContextSwitchTo(TopMemoryContext);
  listenChannels = lappend(listenChannels, pstrdup(channel));
  MemoryContextSwitchTo(oldcontext);
}

   
                                                          
   
                                                          
   
static void
Exec_UnlistenCommit(const char *channel)
{
  ListCell *q;
  ListCell *prev;

  if (Trace_notify)
  {
    elog(DEBUG1, "Exec_UnlistenCommit(%s,%d)", channel, MyProcPid);
  }

  prev = NULL;
  foreach (q, listenChannels)
  {
    char *lchan = (char *)lfirst(q);

    if (strcmp(lchan, channel) == 0)
    {
      listenChannels = list_delete_cell(listenChannels, q, prev);
      pfree(lchan);
      break;
    }
    prev = q;
  }

     
                                                                        
                
     
}

   
                                                             
   
                                               
   
static void
Exec_UnlistenAllCommit(void)
{
  if (Trace_notify)
  {
    elog(DEBUG1, "Exec_UnlistenAllCommit(%d)", MyProcPid);
  }

  list_free_deep(listenChannels);
  listenChannels = NIL;
}

   
                                                                   
   
                                                                           
                                                                           
                                                                         
                                                                     
   
                                                                        
                                                                            
                                                                      
                                                                            
                                                                              
                                                         
   
                                                                           
                                                                          
                       
   
                                                 
   
void
ProcessCompletedNotifies(void)
{
  MemoryContext caller_context;
  bool signalled;

                                                         
  if (!backendHasSentNotifications)
  {
    return;
  }

     
                                                                           
                                                                             
                                          
     
  backendHasSentNotifications = false;

     
                                                                            
                                        
     
  caller_context = CurrentMemoryContext;

  if (Trace_notify)
  {
    elog(DEBUG1, "ProcessCompletedNotifies");
  }

     
                                                                           
                                            
     
  StartTransactionCommand();

                                      
  signalled = SignalBackends();

  if (listenChannels != NIL)
  {
                                                                           
    asyncQueueReadAllNotifications();
  }
  else if (!signalled)
  {
       
                                                                        
                                                                          
                                                                          
                                                                      
                                                                     
                  
       
    asyncQueueAdvanceTail();
  }

  CommitTransactionCommand();

  MemoryContextSwitchTo(caller_context);

                                                                          
}

   
                                                                     
   
                                                                              
                                                                             
                                                                           
                         
   
static bool
IsListeningOn(const char *channel)
{
  ListCell *p;

  foreach (p, listenChannels)
  {
    char *lchan = (char *)lfirst(p);

    if (strcmp(lchan, channel) == 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                                             
                                                                      
   
static void
asyncQueueUnregister(void)
{
  bool advanceTail;

  Assert(listenChannels == NIL);                        

  if (!amRegisteredListener)                    
  {
    return;
  }

  LWLockAcquire(AsyncQueueLock, LW_SHARED);
                                              
  advanceTail = (MyProcPid == QUEUE_BACKEND_PID(MyBackendId)) && QUEUE_POS_EQUAL(QUEUE_BACKEND_POS(MyBackendId), QUEUE_TAIL);
                                
  QUEUE_BACKEND_PID(MyBackendId) = InvalidPid;
  QUEUE_BACKEND_DBOID(MyBackendId) = InvalidOid;
  LWLockRelease(AsyncQueueLock);

                                                              
  amRegisteredListener = false;

                                                                       
  if (advanceTail)
  {
    asyncQueueAdvanceTail();
  }
}

   
                                                                    
   
                                                    
   
static bool
asyncQueueIsFull(void)
{
  int nexthead;
  int boundary;

     
                                                                            
                                                                      
                                                                            
                                                                             
                                                             
                                                                            
     
                                                                          
                                                                            
                                                         
     
  nexthead = QUEUE_POS_PAGE(QUEUE_HEAD) + 1;
  if (nexthead > QUEUE_MAX_PAGE)
  {
    nexthead = 0;                  
  }
  boundary = QUEUE_STOP_PAGE;
  boundary -= boundary % SLRU_PAGES_PER_SEGMENT;
  return asyncQueuePagePrecedes(nexthead, boundary);
}

   
                                                                          
                                                                          
                             
   
static bool
asyncQueueAdvance(volatile QueuePosition *position, int entryLength)
{
  int pageno = QUEUE_POS_PAGE(*position);
  int offset = QUEUE_POS_OFFSET(*position);
  bool pageJump = false;

     
                                                                          
                      
     
  offset += entryLength;
  Assert(offset <= QUEUE_PAGESIZE);

     
                                                                            
                                                                             
                                          
     
  if (offset + QUEUEALIGN(AsyncQueueEntryEmptySize) > QUEUE_PAGESIZE)
  {
    pageno++;
    if (pageno > QUEUE_MAX_PAGE)
    {
      pageno = 0;                  
    }
    offset = 0;
    pageJump = true;
  }

  SET_QUEUE_POS(*position, pageno, offset);
  return pageJump;
}

   
                                                                          
   
static void
asyncQueueNotificationToEntry(Notification *n, AsyncQueueEntry *qe)
{
  size_t channellen = strlen(n->channel);
  size_t payloadlen = strlen(n->payload);
  int entryLength;

  Assert(channellen < NAMEDATALEN);
  Assert(payloadlen < NOTIFY_PAYLOAD_MAX_LENGTH);

                                                                        
  entryLength = AsyncQueueEntryEmptySize + payloadlen + channellen;
  entryLength = QUEUEALIGN(entryLength);
  qe->length = entryLength;
  qe->dboid = MyDatabaseId;
  qe->xid = GetCurrentTransactionId();
  qe->srcPid = MyProcPid;
  memcpy(qe->data, n->channel, channellen + 1);
  memcpy(qe->data + channellen + 1, n->payload, payloadlen + 1);
}

   
                                           
   
                                                                              
                                                                              
                                                                               
                                                                              
                                                          
   
                                                                         
                                                                              
                                
   
                                                                               
                             
   
static ListCell *
asyncQueueAddEntries(ListCell *nextNotify)
{
  AsyncQueueEntry qe;
  QueuePosition queue_head;
  int pageno;
  int offset;
  int slotno;

                                                                          
  LWLockAcquire(AsyncCtlLock, LW_EXCLUSIVE);

     
                                                                            
                                                                             
                                                                         
                                                                            
                                                                            
                                                                          
                                                                           
                                                                        
                 
     
  queue_head = QUEUE_HEAD;

                              
  pageno = QUEUE_POS_PAGE(queue_head);
  slotno = SimpleLruReadPage(AsyncCtl, pageno, true, InvalidTransactionId);
                                                        
  AsyncCtl->shared->page_dirty[slotno] = true;

  while (nextNotify != NULL)
  {
    Notification *n = (Notification *)lfirst(nextNotify);

                                                            
    asyncQueueNotificationToEntry(n, &qe);

    offset = QUEUE_POS_OFFSET(queue_head);

                                                                 
    if (offset + qe.length <= QUEUE_PAGESIZE)
    {
                                                    
      nextNotify = lnext(nextNotify);
    }
    else
    {
         
                                                                        
                                                                         
                                                       
         
      qe.length = QUEUE_PAGESIZE - offset;
      qe.dboid = InvalidOid;
      qe.data[0] = '\0';                    
      qe.data[1] = '\0';                    
    }

                                                 
    memcpy(AsyncCtl->shared->page_buffer[slotno] + offset, &qe, qe.length);

                                                                      
    if (asyncQueueAdvance(&(queue_head), qe.length))
    {
         
                                                                        
                                                                        
                                                                        
                                                              
                                                                      
                                             
         
      slotno = SimpleLruZeroPage(AsyncCtl, QUEUE_POS_PAGE(queue_head));
                             
      break;
    }
  }

                                                
  QUEUE_HEAD = queue_head;

  LWLockRelease(AsyncCtlLock);

  return nextNotify;
}

   
                                                                           
             
   
Datum
pg_notification_queue_usage(PG_FUNCTION_ARGS)
{
  double usage;

  LWLockAcquire(AsyncQueueLock, LW_SHARED);
  usage = asyncQueueUsage();
  LWLockRelease(AsyncQueueLock);

  PG_RETURN_FLOAT8(usage);
}

   
                                                                
   
                                                                  
   
                                                                            
                                                                            
                                                                         
                                                            
   
static double
asyncQueueUsage(void)
{
  int headPage = QUEUE_POS_PAGE(QUEUE_HEAD);
  int tailPage = QUEUE_POS_PAGE(QUEUE_TAIL);
  int occupied;

  occupied = headPage - tailPage;

  if (occupied == 0)
  {
    return (double)0;                                
  }

  if (occupied < 0)
  {
                                               
    occupied += QUEUE_MAX_PAGE + 1;
  }

  return (double)occupied / (double)((QUEUE_MAX_PAGE + 1) / 2);
}

   
                                                                            
   
                                                               
                                                                     
   
                                              
   
static void
asyncQueueFillWarning(void)
{
  double fillDegree;
  TimestampTz t;

  fillDegree = asyncQueueUsage();
  if (fillDegree < 0.5)
  {
    return;
  }

  t = GetCurrentTimestamp();

  if (TimestampDifferenceExceeds(asyncQueueControl->lastQueueFillWarn, t, QUEUE_FULL_WARN_INTERVAL))
  {
    QueuePosition min = QUEUE_HEAD;
    int32 minPid = InvalidPid;
    int i;

    for (i = 1; i <= MaxBackends; i++)
    {
      if (QUEUE_BACKEND_PID(i) != InvalidPid)
      {
        min = QUEUE_POS_MIN(min, QUEUE_BACKEND_POS(i));
        if (QUEUE_POS_EQUAL(min, QUEUE_BACKEND_POS(i)))
        {
          minPid = QUEUE_BACKEND_PID(i);
        }
      }
    }

    ereport(WARNING, (errmsg("NOTIFY queue is %.0f%% full", fillDegree * 100), (minPid != InvalidPid ? errdetail("The server process with PID %d is among those with the oldest transactions.", minPid) : 0), (minPid != InvalidPid ? errhint("The NOTIFY queue cannot be emptied until that process ends its current transaction.") : 0)));

    asyncQueueControl->lastQueueFillWarn = t;
  }
}

   
                                                            
   
                                                
   
                                                                               
                                                                      
                                                                               
                                                                              
         
   
                                                                          
   
static bool
SignalBackends(void)
{
  bool signalled = false;
  int32 *pids;
  BackendId *ids;
  int count;
  int i;
  int32 pid;

     
                                                                             
                                                                             
                                  
     
                                                                          
                                                                         
                                                              
     
  pids = (int32 *)palloc(MaxBackends * sizeof(int32));
  ids = (BackendId *)palloc(MaxBackends * sizeof(BackendId));
  count = 0;

  LWLockAcquire(AsyncQueueLock, LW_EXCLUSIVE);
  for (i = 1; i <= MaxBackends; i++)
  {
    pid = QUEUE_BACKEND_PID(i);
    if (pid != InvalidPid && pid != MyProcPid)
    {
      QueuePosition pos = QUEUE_BACKEND_POS(i);

      if (!QUEUE_POS_EQUAL(pos, QUEUE_HEAD))
      {
        pids[count] = pid;
        ids[count] = i;
        count++;
      }
    }
  }
  LWLockRelease(AsyncQueueLock);

                        
  for (i = 0; i < count; i++)
  {
    pid = pids[i];

       
                                                                        
                                                                 
                                                                       
                                                         
       
    if (SendProcSignal(pid, PROCSIG_NOTIFY_INTERRUPT, ids[i]) < 0)
    {
      elog(DEBUG3, "could not signal backend with PID %d: %m", pid);
    }
    else
    {
      signalled = true;
    }
  }

  pfree(pids);
  pfree(ids);

  return signalled;
}

   
                  
   
                                        
   
                                                                        
                                              
   
void
AtAbort_Notify(void)
{
     
                                                                             
                                                                     
                                                      
     
  if (amRegisteredListener && listenChannels == NIL)
  {
    asyncQueueUnregister();
  }

                    
  ClearPendingActionsAndNotifies();
}

   
                                                              
   
                                                
   
void
AtSubStart_Notify(void)
{
  MemoryContext old_cxt;

                                                                      
  old_cxt = MemoryContextSwitchTo(TopTransactionContext);

  upperPendingActions = lcons(pendingActions, upperPendingActions);

  Assert(list_length(upperPendingActions) == GetCurrentTransactionNestLevel() - 1);

  pendingActions = NIL;

  upperPendingNotifies = lcons(pendingNotifies, upperPendingNotifies);

  Assert(list_length(upperPendingNotifies) == GetCurrentTransactionNestLevel() - 1);

  pendingNotifies = NIL;

  MemoryContextSwitchTo(old_cxt);
}

   
                                                                
   
                                                                      
   
void
AtSubCommit_Notify(void)
{
  List *parentPendingActions;
  List *parentPendingNotifies;

  parentPendingActions = linitial_node(List, upperPendingActions);
  upperPendingActions = list_delete_first(upperPendingActions);

  Assert(list_length(upperPendingActions) == GetCurrentTransactionNestLevel() - 2);

     
                                                                     
     
  pendingActions = list_concat(parentPendingActions, pendingActions);

  parentPendingNotifies = linitial_node(List, upperPendingNotifies);
  upperPendingNotifies = list_delete_first(upperPendingNotifies);

  Assert(list_length(upperPendingNotifies) == GetCurrentTransactionNestLevel() - 2);

     
                                                                             
     
  pendingNotifies = list_concat(parentPendingNotifies, pendingNotifies);
}

   
                                                              
   
void
AtSubAbort_Notify(void)
{
  int my_level = GetCurrentTransactionNestLevel();

     
                                                                         
                                                                         
                                             
     
                                                                             
                                                                         
                                                                           
                     
     
  while (list_length(upperPendingActions) > my_level - 2)
  {
    pendingActions = linitial_node(List, upperPendingActions);
    upperPendingActions = list_delete_first(upperPendingActions);
  }

  while (list_length(upperPendingNotifies) > my_level - 2)
  {
    pendingNotifies = linitial_node(List, upperPendingNotifies);
    upperPendingNotifies = list_delete_first(upperPendingNotifies);
  }
}

   
                         
   
                                                                       
                                                                        
                                                      
                                                                     
   
void
HandleNotifyInterrupt(void)
{
     
                                                                          
                  
     

                                         
  notifyInterruptPending = true;

                                                      
  SetLatch(MyLatch);
}

   
                          
   
                                                                     
                                                                     
                                                                    
                                                                  
                                                               
                                                                 
                                   
   
void
ProcessNotifyInterrupt(void)
{
  if (IsTransactionOrTransactionBlock())
  {
    return;                      
  }

  while (notifyInterruptPending)
  {
    ProcessIncomingNotify();
  }
}

   
                                                                          
                                                                         
                 
   
static void
asyncQueueReadAllNotifications(void)
{
  volatile QueuePosition pos;
  QueuePosition oldpos;
  QueuePosition head;
  Snapshot snapshot;
  bool advanceTail;

                                                              
  union
  {
    char buf[QUEUE_PAGESIZE];
    AsyncQueueEntry align;
  } page_buffer;

                           
  LWLockAcquire(AsyncQueueLock, LW_SHARED);
                                                      
  Assert(MyProcPid == QUEUE_BACKEND_PID(MyBackendId));
  pos = oldpos = QUEUE_BACKEND_POS(MyBackendId);
  head = QUEUE_HEAD;
  LWLockRelease(AsyncQueueLock);

  if (QUEUE_POS_EQUAL(pos, head))
  {
                                                                
    return;
  }

                                                                          
  snapshot = RegisterSnapshot(GetLatestSnapshot());

               
                                                                       
                                            
                                                                    
                                     
     
                                
     
                        
                 
                   
                                
                         
                           
                    
                            
     
                                                                            
                                                                        
                                                                   
     
                                                                          
                                                                
     
                                                                       
                                                                     
                                                                     
                                                                      
                                                                         
                                                                         
                                                                        
                                             
               
     
  PG_TRY();
  {
    bool reachedStop;

    do
    {
      int curpage = QUEUE_POS_PAGE(pos);
      int curoffset = QUEUE_POS_OFFSET(pos);
      int slotno;
      int copysize;

         
                                                                        
                                                                         
                                                                         
                                               
         
      slotno = SimpleLruReadPage_ReadOnly(AsyncCtl, curpage, InvalidTransactionId);
      if (curpage == QUEUE_POS_PAGE(head))
      {
                                                 
        copysize = QUEUE_POS_OFFSET(head) - curoffset;
        if (copysize < 0)
        {
          copysize = 0;                      
        }
      }
      else
      {
                                            
        copysize = QUEUE_PAGESIZE - curoffset;
      }
      memcpy(page_buffer.buf + curoffset, AsyncCtl->shared->page_buffer[slotno] + curoffset, copysize);
                                                                      
      LWLockRelease(AsyncCtlLock);

         
                                                                      
                              
         
                                                                      
                                                                       
                                                                      
                                                     
         
                                                                    
                                                                     
                                                                  
                                                                         
                                                          
         
      reachedStop = asyncQueueProcessPageEntries(&pos, head, page_buffer.buf, snapshot);
    } while (!reachedStop);
  }
  PG_CATCH();
  {
                             
    LWLockAcquire(AsyncQueueLock, LW_SHARED);
    QUEUE_BACKEND_POS(MyBackendId) = pos;
    advanceTail = QUEUE_POS_EQUAL(oldpos, QUEUE_TAIL);
    LWLockRelease(AsyncQueueLock);

                                                                         
    if (advanceTail)
    {
      asyncQueueAdvanceTail();
    }

    PG_RE_THROW();
  }
  PG_END_TRY();

                           
  LWLockAcquire(AsyncQueueLock, LW_SHARED);
  QUEUE_BACKEND_POS(MyBackendId) = pos;
  advanceTail = QUEUE_POS_EQUAL(oldpos, QUEUE_TAIL);
  LWLockRelease(AsyncQueueLock);

                                                                       
  if (advanceTail)
  {
    asyncQueueAdvanceTail();
  }

                          
  UnregisterSnapshot(snapshot);
}

   
                                                                             
                                             
   
                                                                        
                                                                       
                                                                  
   
                                                                            
                                                          
   
                                                                          
                                                                          
                                                                          
                                                                       
   
static bool
asyncQueueProcessPageEntries(volatile QueuePosition *current, QueuePosition stop, char *page_buffer, Snapshot snapshot)
{
  bool reachedStop = false;
  bool reachedEndOfPage;
  AsyncQueueEntry *qe;

  do
  {
    QueuePosition thisentry = *current;

    if (QUEUE_POS_EQUAL(thisentry, stop))
    {
      break;
    }

    qe = (AsyncQueueEntry *)(page_buffer + QUEUE_POS_OFFSET(thisentry));

       
                                                                         
                                                                         
                                                                     
       
    reachedEndOfPage = asyncQueueAdvance(current, qe->length);

                                                      
    if (qe->dboid == MyDatabaseId)
    {
      if (XidInMVCCSnapshot(qe->xid, snapshot))
      {
           
                                                                    
                                                                       
                                                                  
                                                               
                                                                    
                                                                   
                                 
           
                                                                   
                                                                       
                                                                       
                                                            
           
                                                                     
                                                                     
                                                                
                     
           
        *current = thisentry;
        reachedStop = true;
        break;
      }
      else if (TransactionIdDidCommit(qe->xid))
      {
                                                          
        char *channel = qe->data;

        if (IsListeningOn(channel))
        {
                                            
          char *payload = qe->data + strlen(channel) + 1;

          NotifyMyFrontEnd(channel, payload, qe->srcPid);
        }
      }
      else
      {
           
                                                                 
                                     
           
      }
    }

                                               
  } while (!reachedEndOfPage);

  if (QUEUE_POS_EQUAL(*current, stop))
  {
    reachedStop = true;
  }

  return reachedStop;
}

   
                                                                    
                                                                     
   
static void
asyncQueueAdvanceTail(void)
{
  QueuePosition min;
  int i;
  int oldtailpage;
  int newtailpage;
  int boundary;

                                                                          
  LWLockAcquire(NotifyQueueTailLock, LW_EXCLUSIVE);

     
                                                                             
                                                                           
                                                                             
                                                                           
         
     
                                                                             
                                                                            
                                                                             
                                                                             
                                                                        
     
                                                                        
                                                                           
                                                            
     
  LWLockAcquire(AsyncQueueLock, LW_EXCLUSIVE);
  min = QUEUE_HEAD;
  for (i = 1; i <= MaxBackends; i++)
  {
    if (QUEUE_BACKEND_PID(i) != InvalidPid)
    {
      min = QUEUE_POS_MIN(min, QUEUE_BACKEND_POS(i));
    }
  }
  QUEUE_TAIL = min;
  oldtailpage = QUEUE_STOP_PAGE;
  LWLockRelease(AsyncQueueLock);

     
                                                                          
                       
     
                                                                             
                                           
     
  newtailpage = QUEUE_POS_PAGE(min);
  boundary = newtailpage - (newtailpage % SLRU_PAGES_PER_SEGMENT);
  if (asyncQueuePagePrecedes(oldtailpage, boundary))
  {
       
                                                                           
                       
       
    SimpleLruTruncate(AsyncCtl, newtailpage);

       
                                                                          
                                                                         
                               
       
    LWLockAcquire(AsyncQueueLock, LW_EXCLUSIVE);
    QUEUE_STOP_PAGE = newtailpage;
    LWLockRelease(AsyncQueueLock);
  }

  LWLockRelease(NotifyQueueTailLock);
}

   
                         
   
                                                                           
                                                                    
                                       
   
                                                                          
         
   
                                                                        
   
static void
ProcessIncomingNotify(void)
{
                                
  notifyInterruptPending = false;

                                                       
  if (listenChannels == NIL)
  {
    return;
  }

  if (Trace_notify)
  {
    elog(DEBUG1, "ProcessIncomingNotify");
  }

  set_ps_display("notify interrupt", false);

     
                                                                           
                                            
     
  StartTransactionCommand();

  asyncQueueReadAllNotifications();

  CommitTransactionCommand();

     
                                                                           
     
  pq_flush();

  set_ps_display("idle", false);

  if (Trace_notify)
  {
    elog(DEBUG1, "ProcessIncomingNotify: done");
  }
}

   
                                        
   
void
NotifyMyFrontEnd(const char *channel, const char *payload, int32 srcPid)
{
  if (whereToSendOutput == DestRemote)
  {
    StringInfoData buf;

    pq_beginmessage(&buf, 'A');
    pq_sendint32(&buf, srcPid);
    pq_sendstring(&buf, channel);
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
    {
      pq_sendstring(&buf, payload);
    }
    pq_endmessage(&buf);

       
                                                                       
                                                                       
                                                                        
       
  }
  else
  {
    elog(INFO, "NOTIFY for \"%s\" payload \"%s\"", channel, payload);
  }
}

                                                             
static bool
AsyncExistsPendingNotify(const char *channel, const char *payload)
{
  ListCell *p;
  Notification *n;

  if (pendingNotifies == NIL)
  {
    return false;
  }

  if (payload == NULL)
  {
    payload = "";
  }

               
                                                                            
                                                                       
                                                                            
                                                                           
                                                                        
                                             
     
                                                                            
                                                   
     
            
                     
                    
                     
             
               
     
  n = (Notification *)llast(pendingNotifies);
  if (strcmp(n->channel, channel) == 0 && strcmp(n->payload, payload) == 0)
  {
    return true;
  }

  foreach (p, pendingNotifies)
  {
    n = (Notification *)lfirst(p);

    if (strcmp(n->channel, channel) == 0 && strcmp(n->payload, payload) == 0)
    {
      return true;
    }
  }

  return false;
}

                                                         
static void
ClearPendingActionsAndNotifies(void)
{
     
                                                                          
                                                                          
                                                                    
                                                                          
               
     
  pendingActions = NIL;
  pendingNotifies = NIL;
}
