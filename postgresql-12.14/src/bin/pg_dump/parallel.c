                                                                            
   
              
   
                                               
   
                                                                         
                                                                        
   
                  
                               
   
                                                                            
   

   
                                       
   
                                                                             
                                                                               
   
                                                                              
                                                                            
                                                                 
                                                                            
                                                                          
                                                                            
                                                                             
                                                             
                                                              
                                                                        
                                                         
   
                                                                               
                                                                         
                                                                           
                                                                             
                                                                          
                                                                          
                                                                          
                                                           
   
                                                                            
                         
                                                       
                                          
                                            
                                   
                                                                             
                                            
   

#include "postgres_fe.h"

#ifndef WIN32
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "parallel.h"
#include "pg_backup_utils.h"

#include "fe_utils/string_utils.h"
#include "port/pg_bswap.h"

                                                                   
#define PIPE_READ 0
#define PIPE_WRITE 1

#define NO_SLOT (-1)                                         

                             
typedef enum
{
  WRKR_NOT_STARTED = 0,
  WRKR_IDLE,
  WRKR_WORKING,
  WRKR_TERMINATED
} T_WorkerStatus;

#define WORKER_IS_RUNNING(workerStatus) ((workerStatus) == WRKR_IDLE || (workerStatus) == WRKR_WORKING)

   
                                                                          
   
                                                                            
                                                                              
                                                                
   
struct ParallelSlot
{
  T_WorkerStatus workerStatus;                     

                                                               
  ParallelCompletionPtr callback;                                     
  void *callback_data;                                         

  ArchiveHandle *AH;                                   

  int pipeRead;                                
  int pipeWrite;
  int pipeRevRead;                               
  int pipeRevWrite;

                                           
#ifdef WIN32
  uintptr_t hThread;
  unsigned int threadId;
#else
  pid_t pid;
#endif
};

#ifdef WIN32

   
                                                                              
                                    
   
typedef struct
{
  ArchiveHandle *AH;                                  
  ParallelSlot *slot;                                  
} WorkerInfo;

                                           
static int
pgpipe(int handles[2]);
static int
piperead(int s, char *buf, int len);
#define pipewrite(a, b, c) send(a, b, c, 0)

#else             

                                               
#define pgpipe(a) pipe(a)
#define piperead(a, b, c) read(a, b, c)
#define pipewrite(a, b, c) write(a, b, c)

#endif            

   
                                                                
   
typedef struct ShutdownInformation
{
  ParallelState *pstate;
  Archive *AHX;
} ShutdownInformation;

static ShutdownInformation shutdown_info;

   
                                   
                                                
   
                                                                            
                                                                              
                                                                            
                                                          
   
typedef struct DumpSignalInformation
{
  ArchiveHandle *myAH;                                                
  ParallelState *pstate;                             
  bool handler_set;                                                  
#ifndef WIN32
  bool am_worker;                             
#endif
} DumpSignalInformation;

static volatile DumpSignalInformation signal_info;

#ifdef WIN32
static CRITICAL_SECTION signal_info_lock;
#endif

   
                                                                         
                                                                             
                                                           
   
#define write_stderr(str)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    const char *str_ = (str);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    int rc_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    rc_ = write(fileno(stderr), str_, strlen(str_));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (void)rc_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

#ifdef WIN32
                          
static DWORD tls_index;

                                                        
bool parallel_init_done = false;
DWORD mainThreadId;
#endif            

                               
static ParallelSlot *
GetMyPSlot(ParallelState *pstate);
static void
archive_close_connection(int code, void *arg);
static void
ShutdownWorkersHard(ParallelState *pstate);
static void
WaitForTerminatingWorkers(ParallelState *pstate);
static void
setup_cancel_handler(void);
static void
set_cancel_pstate(ParallelState *pstate);
static void
set_cancel_slot_archive(ParallelSlot *slot, ArchiveHandle *AH);
static void
RunWorker(ArchiveHandle *AH, ParallelSlot *slot);
static int
GetIdleWorker(ParallelState *pstate);
static bool
HasEveryWorkerTerminated(ParallelState *pstate);
static void
lockTableForWorker(ArchiveHandle *AH, TocEntry *te);
static void
WaitForCommands(ArchiveHandle *AH, int pipefd[2]);
static bool
ListenToWorkers(ArchiveHandle *AH, ParallelState *pstate, bool do_wait);
static char *
getMessageFromMaster(int pipefd[2]);
static void
sendMessageToMaster(int pipefd[2], const char *str);
static int
select_loop(int maxFd, fd_set *workerset);
static char *
getMessageFromWorker(ParallelState *pstate, bool do_wait, int *worker);
static void
sendMessageToWorker(ParallelState *pstate, int worker, const char *str);
static char *
readMessageFromPipe(int fd);

#define messageStartsWith(msg, prefix) (strncmp(msg, prefix, strlen(prefix)) == 0)

   
                                                                          
                                                                          
              
   
void
init_parallel_dump_utils(void)
{
#ifdef WIN32
  if (!parallel_init_done)
  {
    WSADATA wsaData;
    int err;

                                        
    tls_index = TlsAlloc();
    mainThreadId = GetCurrentThreadId();

                                  
    err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0)
    {
      pg_log_error("WSAStartup failed: %d", err);
      exit_nicely(1);
    }

    parallel_init_done = true;
  }
#endif
}

   
                                                                   
   
                                                                              
   
static ParallelSlot *
GetMyPSlot(ParallelState *pstate)
{
  int i;

  for (i = 0; i < pstate->numWorkers; i++)
  {
#ifdef WIN32
    if (pstate->parallelSlot[i].threadId == GetCurrentThreadId())
#else
    if (pstate->parallelSlot[i].pid == getpid())
#endif
      return &(pstate->parallelSlot[i]);
  }

  return NULL;
}

   
                                                    
   
                                                                          
                                                                        
   
#ifdef WIN32
static PQExpBuffer
getThreadLocalPQExpBuffer(void)
{
     
                                                                           
                                                                            
                                                                    
     
  static PQExpBuffer s_id_return = NULL;
  PQExpBuffer id_return;

  if (parallel_init_done)
  {
    id_return = (PQExpBuffer)TlsGetValue(tls_index);
  }
  else
  {
    id_return = s_id_return;
  }

  if (id_return)                          
  {
                                         
    resetPQExpBuffer(id_return);
  }
  else
  {
                    
    id_return = createPQExpBuffer();
    if (parallel_init_done)
    {
      TlsSetValue(tls_index, id_return);
    }
    else
    {
      s_id_return = id_return;
    }
  }

  return id_return;
}
#endif            

   
                                                                    
                                                 
   
void
on_exit_close_archive(Archive *AHX)
{
  shutdown_info.AHX = AHX;
  on_exit_nicely(archive_close_connection, &shutdown_info);
}

   
                                                                     
                             
   
static void
archive_close_connection(int code, void *arg)
{
  ShutdownInformation *si = (ShutdownInformation *)arg;

  if (si->pstate)
  {
                                                      
    ParallelSlot *slot = GetMyPSlot(si->pstate);

    if (!slot)
    {
         
                                                                       
                                          
         
      ShutdownWorkersHard(si->pstate);

      if (si->AHX)
      {
        DisconnectDatabase(si->AHX);
      }
    }
    else
    {
         
                                                                      
                                                                      
                                                                         
                                                                      
                                                                      
                                     
         
      if (slot->AH)
      {
        DisconnectDatabase(&(slot->AH->public));
      }

#ifdef WIN32
      closesocket(slot->pipeRevRead);
      closesocket(slot->pipeRevWrite);
#endif
    }
  }
  else
  {
                                                                    
    if (si->AHX)
    {
      DisconnectDatabase(si->AHX);
    }
  }
}

   
                                                                         
   
                                                                          
                                                                          
                                                                  
                
   
static void
ShutdownWorkersHard(ParallelState *pstate)
{
  int i;

     
                                                                        
                                                                             
                                                                             
                          
     
  for (i = 0; i < pstate->numWorkers; i++)
  {
    closesocket(pstate->parallelSlot[i].pipeWrite);
  }

     
                                                                    
     
#ifndef WIN32
                                                            
  for (i = 0; i < pstate->numWorkers; i++)
  {
    pid_t pid = pstate->parallelSlot[i].pid;

    if (pid != 0)
    {
      kill(pid, SIGTERM);
    }
  }
#else

     
                                                                            
                                                                     
     
  EnterCriticalSection(&signal_info_lock);
  for (i = 0; i < pstate->numWorkers; i++)
  {
    ArchiveHandle *AH = pstate->parallelSlot[i].AH;
    char errbuf[1];

    if (AH != NULL && AH->connCancel != NULL)
    {
      (void)PQcancel(AH->connCancel, errbuf, sizeof(errbuf));
    }
  }
  LeaveCriticalSection(&signal_info_lock);
#endif

                                       
  WaitForTerminatingWorkers(pstate);
}

   
                                      
   
static void
WaitForTerminatingWorkers(ParallelState *pstate)
{
  while (!HasEveryWorkerTerminated(pstate))
  {
    ParallelSlot *slot = NULL;
    int j;

#ifndef WIN32
                                                                   
    int status;
    pid_t pid = wait(&status);

                                                          
    for (j = 0; j < pstate->numWorkers; j++)
    {
      slot = &(pstate->parallelSlot[j]);
      if (slot->pid == pid)
      {
        slot->pid = 0;
        break;
      }
    }
#else             
                                                          
    HANDLE *lpHandles = pg_malloc(sizeof(HANDLE) * pstate->numWorkers);
    int nrun = 0;
    DWORD ret;
    uintptr_t hThread;

    for (j = 0; j < pstate->numWorkers; j++)
    {
      if (WORKER_IS_RUNNING(pstate->parallelSlot[j].workerStatus))
      {
        lpHandles[nrun] = (HANDLE)pstate->parallelSlot[j].hThread;
        nrun++;
      }
    }
    ret = WaitForMultipleObjects(nrun, lpHandles, false, INFINITE);
    Assert(ret != WAIT_FAILED);
    hThread = (uintptr_t)lpHandles[ret - WAIT_OBJECT_0];
    free(lpHandles);

                                                              
    for (j = 0; j < pstate->numWorkers; j++)
    {
      slot = &(pstate->parallelSlot[j]);
      if (slot->hThread == hThread)
      {
                                                             
        CloseHandle((HANDLE)slot->hThread);
        slot->hThread = (uintptr_t)INVALID_HANDLE_VALUE;
        break;
      }
    }
#endif            

                                                                
    Assert(j < pstate->numWorkers);
    slot->workerStatus = WRKR_TERMINATED;
    pstate->te[j] = NULL;
  }
}

   
                                                                     
   
                                                                        
                                                                    
   
                                                                             
                                                                           
                                                                            
                                                                          
                                                                             
                                                        
   
                                                                            
                                                                              
                                                                           
                                                                             
                                                                          
                                                                            
                                                
   
                                                                            
                                                                          
                                                                              
                                                                        
                                                                          
                
   

#ifndef WIN32

   
                              
   
static void
sigTermHandler(SIGNAL_ARGS)
{
  int i;
  char errbuf[1];

     
                                                                         
                                                                          
                                                            
     
  pqsignal(SIGINT, SIG_IGN);
  pqsignal(SIGTERM, SIG_IGN);
  pqsignal(SIGQUIT, SIG_IGN);

     
                                                                            
                                                                            
                                                                        
                                                          
     
  if (signal_info.pstate != NULL)
  {
    for (i = 0; i < signal_info.pstate->numWorkers; i++)
    {
      pid_t pid = signal_info.pstate->parallelSlot[i].pid;

      if (pid != 0)
      {
        kill(pid, SIGTERM);
      }
    }
  }

     
                                                                          
                                                   
     
  if (signal_info.myAH != NULL && signal_info.myAH->connCancel != NULL)
  {
    (void)PQcancel(signal_info.myAH->connCancel, errbuf, sizeof(errbuf));
  }

     
                                                                          
                                                                         
     
  if (!signal_info.am_worker)
  {
    if (progname)
    {
      write_stderr(progname);
      write_stderr(": ");
    }
    write_stderr("terminated by user\n");
  }

     
                                                                             
                                                            
     
  _exit(1);
}

   
                                                         
   
static void
setup_cancel_handler(void)
{
     
                                                                       
                                                                         
     
  if (!signal_info.handler_set)
  {
    signal_info.handler_set = true;

    pqsignal(SIGINT, sigTermHandler);
    pqsignal(SIGTERM, sigTermHandler);
    pqsignal(SIGQUIT, sigTermHandler);
  }
}

#else            

   
                                                                 
   
                                                                        
                                                                           
                       
   
static BOOL WINAPI
consoleHandler(DWORD dwCtrlType)
{
  int i;
  char errbuf[1];

  if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT)
  {
                                                                 
    EnterCriticalSection(&signal_info_lock);

       
                                                                        
                                                                        
                                                                           
                                                                          
                                                                   
                                                                          
                                                                       
                                                     
       
    if (signal_info.pstate != NULL)
    {
      for (i = 0; i < signal_info.pstate->numWorkers; i++)
      {
        ParallelSlot *slot = &(signal_info.pstate->parallelSlot[i]);
        ArchiveHandle *AH = slot->AH;
        HANDLE hThread = (HANDLE)slot->hThread;

           
                                                                       
                                                                    
                    
           
        if (hThread != INVALID_HANDLE_VALUE)
        {
          TerminateThread(hThread, 0);
        }

        if (AH != NULL && AH->connCancel != NULL)
        {
          (void)PQcancel(AH->connCancel, errbuf, sizeof(errbuf));
        }
      }
    }

       
                                                                          
                                                     
       
    if (signal_info.myAH != NULL && signal_info.myAH->connCancel != NULL)
    {
      (void)PQcancel(signal_info.myAH->connCancel, errbuf, sizeof(errbuf));
    }

    LeaveCriticalSection(&signal_info_lock);

       
                                                                  
                                                                      
                                                                       
                                                      
       
    if (progname)
    {
      write_stderr(progname);
      write_stderr(": ");
    }
    write_stderr("terminated by user\n");
  }

                                                                
  return FALSE;
}

   
                                                         
   
static void
setup_cancel_handler(void)
{
  if (!signal_info.handler_set)
  {
    signal_info.handler_set = true;

    InitializeCriticalSection(&signal_info_lock);

    SetConsoleCtrlHandler(consoleHandler, TRUE);
  }
}

#endif            

   
                           
   
                                                                         
                                            
   
void
set_archive_cancel_info(ArchiveHandle *AH, PGconn *conn)
{
  PGcancel *oldConnCancel;

     
                                                                          
                                                                     
                                                                     
              
     
  setup_cancel_handler();

     
                                                                            
                                                                            
     

#ifdef WIN32
  EnterCriticalSection(&signal_info_lock);
#endif

                                       
  oldConnCancel = AH->connCancel;
                                                                   
  AH->connCancel = NULL;

  if (oldConnCancel != NULL)
  {
    PQfreeCancel(oldConnCancel);
  }

                                    
  if (conn)
  {
    AH->connCancel = PQgetCancel(conn);
  }

     
                                                                            
                                                                         
                                                                     
                                                                      
                  
     
#ifndef WIN32
  signal_info.myAH = AH;
#else
  if (mainThreadId == GetCurrentThreadId())
  {
    signal_info.myAH = AH;
  }
#endif

#ifdef WIN32
  LeaveCriticalSection(&signal_info_lock);
#endif
}

   
                     
   
                                                                           
                                                                           
   
static void
set_cancel_pstate(ParallelState *pstate)
{
#ifdef WIN32
  EnterCriticalSection(&signal_info_lock);
#endif

  signal_info.pstate = pstate;

#ifdef WIN32
  LeaveCriticalSection(&signal_info_lock);
#endif
}

   
                           
   
                                                                          
                                                                           
   
static void
set_cancel_slot_archive(ParallelSlot *slot, ArchiveHandle *AH)
{
#ifdef WIN32
  EnterCriticalSection(&signal_info_lock);
#endif

  slot->AH = AH;

#ifdef WIN32
  LeaveCriticalSection(&signal_info_lock);
#endif
}

   
                                                                       
                                                                         
                
   
static void
RunWorker(ArchiveHandle *AH, ParallelSlot *slot)
{
  int pipefd[2];

                                 
  pipefd[PIPE_READ] = slot->pipeRevRead;
  pipefd[PIPE_WRITE] = slot->pipeRevWrite;

     
                                                                          
                                             
     
                                                                           
                                                                     
                                                                             
                                                            
     
  AH = CloneArchive(AH);

                                                                
  set_cancel_slot_archive(slot, AH);

     
                                                                         
     
  (AH->SetupWorkerPtr)((Archive *)AH);

     
                                  
     
  WaitForCommands(AH, pipefd);

     
                                            
     
  set_cancel_slot_archive(slot, NULL);
  DisconnectDatabase(&(AH->public));
  DeCloneArchive(AH);
}

   
                                    
   
#ifdef WIN32
static unsigned __stdcall init_spawned_worker_win32(WorkerInfo *wi)
{
  ArchiveHandle *AH = wi->AH;
  ParallelSlot *slot = wi->slot;

                                     
  free(wi);

                          
  RunWorker(AH, slot);

                       
  _endthreadex(0);
  return 0;
}
#endif            

   
                                                                              
                                                                        
                                    
   
ParallelState *
ParallelBackupStart(ArchiveHandle *AH)
{
  ParallelState *pstate;
  int i;

  Assert(AH->public.numWorkers > 0);

  pstate = (ParallelState *)pg_malloc(sizeof(ParallelState));

  pstate->numWorkers = AH->public.numWorkers;
  pstate->te = NULL;
  pstate->parallelSlot = NULL;

  if (AH->public.numWorkers == 1)
  {
    return pstate;
  }

                                                                      
  pstate->te = (TocEntry **)pg_malloc0(pstate->numWorkers * sizeof(TocEntry *));
  pstate->parallelSlot = (ParallelSlot *)pg_malloc0(pstate->numWorkers * sizeof(ParallelSlot));

#ifdef WIN32
                                                                  
  getLocalPQExpBuffer = getThreadLocalPQExpBuffer;
#endif

     
                                                                            
                                                                             
                                                                           
                                          
     
  shutdown_info.pstate = pstate;

     
                                                                            
                                                                     
                                                                       
                                                                          
                                                         
     
  set_archive_cancel_info(AH, NULL);

                                                     
  fflush(NULL);

                                        
  for (i = 0; i < pstate->numWorkers; i++)
  {
#ifdef WIN32
    WorkerInfo *wi;
    uintptr_t handle;
#else
    pid_t pid;
#endif
    ParallelSlot *slot = &(pstate->parallelSlot[i]);
    int pipeMW[2], pipeWM[2];

                                                    
    if (pgpipe(pipeMW) < 0 || pgpipe(pipeWM) < 0)
    {
      fatal("could not create communication channels: %m");
    }

                                    
    slot->pipeRead = pipeWM[PIPE_READ];
    slot->pipeWrite = pipeMW[PIPE_WRITE];
                                   
    slot->pipeRevRead = pipeMW[PIPE_READ];
    slot->pipeRevWrite = pipeWM[PIPE_WRITE];

#ifdef WIN32
                                                                    
    wi = (WorkerInfo *)pg_malloc(sizeof(WorkerInfo));

    wi->AH = AH;
    wi->slot = slot;

    handle = _beginthreadex(NULL, 0, (void *)&init_spawned_worker_win32, wi, 0, &(slot->threadId));
    slot->hThread = handle;
    slot->workerStatus = WRKR_IDLE;
#else              
    pid = fork();
    if (pid == 0)
    {
                             
      int j;

                                           
      slot->pid = getpid();

                                                              
      signal_info.am_worker = true;

                                              
      closesocket(pipeWM[PIPE_READ]);
                                               
      closesocket(pipeMW[PIPE_WRITE]);

         
                                                                      
                                    
         
      for (j = 0; j < i; j++)
      {
        closesocket(pstate->parallelSlot[j].pipeRead);
        closesocket(pstate->parallelSlot[j].pipeWrite);
      }

                              
      RunWorker(AH, slot);

                                         
      exit(0);
    }
    else if (pid < 0)
    {
                       
      fatal("could not create worker process: %m");
    }

                                         
    slot->pid = pid;
    slot->workerStatus = WRKR_IDLE;

                                            
    closesocket(pipeMW[PIPE_READ]);
                                             
    closesocket(pipeWM[PIPE_WRITE]);
#endif            
  }

     
                                                                         
                                                                           
                                                  
     
#ifndef WIN32
  pqsignal(SIGPIPE, SIG_IGN);
#endif

     
                                                               
     
  set_archive_cancel_info(AH, AH->connection);

     
                                                                            
                                                                           
                                                                           
                                                                         
     
  set_cancel_pstate(pstate);

  return pstate;
}

   
                                          
   
void
ParallelBackupEnd(ArchiveHandle *AH, ParallelState *pstate)
{
  int i;

                               
  if (pstate->numWorkers == 1)
  {
    return;
  }

                                               
  Assert(IsEveryWorkerIdle(pstate));

                                                                
  for (i = 0; i < pstate->numWorkers; i++)
  {
    closesocket(pstate->parallelSlot[i].pipeRead);
    closesocket(pstate->parallelSlot[i].pipeWrite);
  }

                             
  WaitForTerminatingWorkers(pstate);

     
                                                                           
                                                   
     
  shutdown_info.pstate = NULL;
  set_cancel_pstate(NULL);

                                                                        
  free(pstate->te);
  free(pstate->parallelSlot);
  free(pstate);
}

   
                                                                            
                                                      
   
                                                                              
                                                                              
                                                                   
   

   
                                                                    
   
                                                                     
   
static void
buildWorkerCommand(ArchiveHandle *AH, TocEntry *te, T_Action act, char *buf, int buflen)
{
  if (act == ACT_DUMP)
  {
    snprintf(buf, buflen, "DUMP %d", te->dumpId);
  }
  else if (act == ACT_RESTORE)
  {
    snprintf(buf, buflen, "RESTORE %d", te->dumpId);
  }
  else
  {
    Assert(false);
  }
}

   
                                                               
   
static void
parseWorkerCommand(ArchiveHandle *AH, TocEntry **te, T_Action *act, const char *msg)
{
  DumpId dumpId;
  int nBytes;

  if (messageStartsWith(msg, "DUMP "))
  {
    *act = ACT_DUMP;
    sscanf(msg, "DUMP %d%n", &dumpId, &nBytes);
    Assert(nBytes == strlen(msg));
    *te = getTocEntryByDumpId(AH, dumpId);
    Assert(*te != NULL);
  }
  else if (messageStartsWith(msg, "RESTORE "))
  {
    *act = ACT_RESTORE;
    sscanf(msg, "RESTORE %d%n", &dumpId, &nBytes);
    Assert(nBytes == strlen(msg));
    *te = getTocEntryByDumpId(AH, dumpId);
    Assert(*te != NULL);
  }
  else
  {
    fatal("unrecognized command received from master: \"%s\"", msg);
  }
}

   
                                                                        
   
                                                                     
   
static void
buildWorkerResponse(ArchiveHandle *AH, TocEntry *te, T_Action act, int status, char *buf, int buflen)
{
  snprintf(buf, buflen, "OK %d %d %d", te->dumpId, status, status == WORKER_IGNORED_ERRORS ? AH->public.n_errors : 0);
}

   
                                                                       
   
                                                                           
   
static int
parseWorkerResponse(ArchiveHandle *AH, TocEntry *te, const char *msg)
{
  DumpId dumpId;
  int nBytes, n_errors;
  int status = 0;

  if (messageStartsWith(msg, "OK "))
  {
    sscanf(msg, "OK %d %d %d%n", &dumpId, &status, &n_errors, &nBytes);

    Assert(dumpId == te->dumpId);
    Assert(nBytes == strlen(msg));

    AH->public.n_errors += n_errors;
  }
  else
  {
    fatal("invalid message received from worker: \"%s\"", msg);
  }

  return status;
}

   
                                       
   
                                                                            
                                                              
   
                                                                        
                                                
   
void
DispatchJobForTocEntry(ArchiveHandle *AH, ParallelState *pstate, TocEntry *te, T_Action act, ParallelCompletionPtr callback, void *callback_data)
{
  int worker;
  char buf[256];

                                              
  while ((worker = GetIdleWorker(pstate)) == NO_SLOT)
  {
    WaitForWorkers(AH, pstate, WFW_ONE_IDLE);
  }

                                         
  buildWorkerCommand(AH, te, act, buf, sizeof(buf));

  sendMessageToWorker(pstate, worker, buf);

                                                                   
  pstate->parallelSlot[worker].workerStatus = WRKR_WORKING;
  pstate->parallelSlot[worker].callback = callback;
  pstate->parallelSlot[worker].callback_data = callback_data;
  pstate->te[worker] = te;
}

   
                                                   
                                    
   
static int
GetIdleWorker(ParallelState *pstate)
{
  int i;

  for (i = 0; i < pstate->numWorkers; i++)
  {
    if (pstate->parallelSlot[i].workerStatus == WRKR_IDLE)
    {
      return i;
    }
  }
  return NO_SLOT;
}

   
                                         
   
static bool
HasEveryWorkerTerminated(ParallelState *pstate)
{
  int i;

  for (i = 0; i < pstate->numWorkers; i++)
  {
    if (WORKER_IS_RUNNING(pstate->parallelSlot[i].workerStatus))
    {
      return false;
    }
  }
  return true;
}

   
                                                           
   
bool
IsEveryWorkerIdle(ParallelState *pstate)
{
  int i;

  for (i = 0; i < pstate->numWorkers; i++)
  {
    if (pstate->parallelSlot[i].workerStatus != WRKR_IDLE)
    {
      return false;
    }
  }
  return true;
}

   
                                                             
   
                                                                           
                                                                             
                                                    
   
                                                                         
                                                                              
                                                                
                                                                             
                                                                      
                                                                          
                                                          
   
                                                                               
                                                                           
                                                                              
                                                                 
   
static void
lockTableForWorker(ArchiveHandle *AH, TocEntry *te)
{
  const char *qualId;
  PQExpBuffer query;
  PGresult *res;

                               
  if (strcmp(te->desc, "BLOBS") == 0)
  {
    return;
  }

  query = createPQExpBuffer();

  qualId = fmtQualifiedId(te->namespace, te->tag);

  appendPQExpBuffer(query, "LOCK TABLE %s IN ACCESS SHARE MODE NOWAIT", qualId);

  res = PQexec(AH->connection, query->data);

  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fatal("could not obtain lock on relation \"%s\"\n"
          "This usually means that someone requested an ACCESS EXCLUSIVE lock "
          "on the table after the pg_dump parent process had gotten the "
          "initial ACCESS SHARE lock on the table.",
        qualId);
  }

  PQclear(res);
  destroyPQExpBuffer(query);
}

   
                                                       
   
                                                                           
   
static void
WaitForCommands(ArchiveHandle *AH, int pipefd[2])
{
  char *command;
  TocEntry *te;
  T_Action act;
  int status = 0;
  char buf[256];

  for (;;)
  {
    if (!(command = getMessageFromMaster(pipefd)))
    {
                        
      return;
    }

                            
    parseWorkerCommand(AH, &te, &act, command);

    if (act == ACT_DUMP)
    {
                                                                  
      lockTableForWorker(AH, te);

                                    
      status = (AH->WorkerJobDumpPtr)(AH, te);
    }
    else if (act == ACT_RESTORE)
    {
                                       
      status = (AH->WorkerJobRestorePtr)(AH, te);
    }
    else
    {
      Assert(false);
    }

                                 
    buildWorkerResponse(AH, te, act, status, buf, sizeof(buf));

    sendMessageToMaster(pipefd, buf);

                                                                          
    free(command);
  }
}

   
                                           
   
                                                                            
                                           
   
                                                                         
                                                                         
                          
   
                                                              
   
                                                                       
                                                                            
                  
   
static bool
ListenToWorkers(ArchiveHandle *AH, ParallelState *pstate, bool do_wait)
{
  int worker;
  char *msg;

                                       
  msg = getMessageFromWorker(pstate, do_wait, &worker);

  if (!msg)
  {
                                                                      
    if (do_wait)
    {
      fatal("a worker process died unexpectedly");
    }
    return false;
  }

                                                             
  if (messageStartsWith(msg, "OK "))
  {
    ParallelSlot *slot = &pstate->parallelSlot[worker];
    TocEntry *te = pstate->te[worker];
    int status;

    status = parseWorkerResponse(AH, te, msg);
    slot->callback(AH, te, status, slot->callback_data);
    slot->workerStatus = WRKR_IDLE;
    pstate->te[worker] = NULL;
  }
  else
  {
    fatal("invalid message received from worker: \"%s\"", msg);
  }

                                                          
  free(msg);

  return true;
}

   
                                                                
   
                             
                                                           
                                                               
                                                         
                                                 
   
                                                                
                           
   
                                                    
   
void
WaitForWorkers(ArchiveHandle *AH, ParallelState *pstate, WFW_WaitOption mode)
{
  bool do_wait = false;

     
                                                                            
                                                                             
                            
     
  if (mode == WFW_GOT_STATUS)
  {
                                                  
    Assert(!IsEveryWorkerIdle(pstate));
    do_wait = true;
  }

  for (;;)
  {
       
                                                                         
                                                                       
                                             
       
    if (ListenToWorkers(AH, pstate, do_wait))
    {
         
                                                                       
                                                                         
                                                          
         
      if (mode != WFW_ALL_IDLE)
      {
        return;
      }
    }

                                                            
    switch (mode)
    {
    case WFW_NO_WAIT:
      return;                 
    case WFW_GOT_STATUS:
      Assert(false);                                        
      break;
    case WFW_ONE_IDLE:
      if (GetIdleWorker(pstate) != NO_SLOT)
      {
        return;
      }
      break;
    case WFW_ALL_IDLE:
      if (IsEveryWorkerIdle(pstate))
      {
        return;
      }
      break;
    }

                                                               
    do_wait = true;
  }
}

   
                                                                   
                                                               
                        
   
                                                  
   
static char *
getMessageFromMaster(int pipefd[2])
{
  return readMessageFromPipe(pipefd[PIPE_READ]);
}

   
                                        
   
                                                  
   
static void
sendMessageToMaster(int pipefd[2], const char *str)
{
  int len = strlen(str) + 1;

  if (pipewrite(pipefd[PIPE_WRITE], str, len) != len)
  {
    fatal("could not write to the communication channel: %m");
  }
}

   
                                                               
                                                                 
   
static int
select_loop(int maxFd, fd_set *workerset)
{
  int i;
  fd_set saveSet = *workerset;

  for (;;)
  {
    *workerset = saveSet;
    i = select(maxFd + 1, workerset, NULL, NULL, NULL);

#ifndef WIN32
    if (i < 0 && errno == EINTR)
    {
      continue;
    }
#else
    if (i == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
    {
      continue;
    }
#endif
    break;
  }

  return i;
}

   
                                             
   
                                                                          
                                           
   
                                                                         
   
                                                                           
                                                                           
                                     
   
                                                    
   
static char *
getMessageFromWorker(ParallelState *pstate, bool do_wait, int *worker)
{
  int i;
  fd_set workerset;
  int maxFd = -1;
  struct timeval nowait = {0, 0};

                                                           
  FD_ZERO(&workerset);
  for (i = 0; i < pstate->numWorkers; i++)
  {
    if (!WORKER_IS_RUNNING(pstate->parallelSlot[i].workerStatus))
    {
      continue;
    }
    FD_SET(pstate->parallelSlot[i].pipeRead, &workerset);
    if (pstate->parallelSlot[i].pipeRead > maxFd)
    {
      maxFd = pstate->parallelSlot[i].pipeRead;
    }
  }

  if (do_wait)
  {
    i = select_loop(maxFd, &workerset);
    Assert(i != 0);
  }
  else
  {
    if ((i = select(maxFd + 1, &workerset, NULL, NULL, &nowait)) == 0)
    {
      return NULL;
    }
  }

  if (i < 0)
  {
    fatal("select() failed: %m");
  }

  for (i = 0; i < pstate->numWorkers; i++)
  {
    char *msg;

    if (!WORKER_IS_RUNNING(pstate->parallelSlot[i].workerStatus))
    {
      continue;
    }
    if (!FD_ISSET(pstate->parallelSlot[i].pipeRead, &workerset))
    {
      continue;
    }

       
                                                                        
                                                                         
                                
       
                                                                         
                                                                        
                                                                         
                                                         
       
    msg = readMessageFromPipe(pstate->parallelSlot[i].pipeRead);
    *worker = i;
    return msg;
  }
  Assert(false);
  return NULL;
}

   
                                                           
   
                                                    
   
static void
sendMessageToWorker(ParallelState *pstate, int worker, const char *str)
{
  int len = strlen(str) + 1;

  if (pipewrite(pstate->parallelSlot[worker].pipeWrite, str, len) != len)
  {
    fatal("could not write to the communication channel: %m");
  }
}

   
                                                                        
                                                               
                        
   
                                                                
   
static char *
readMessageFromPipe(int fd)
{
  char *msg;
  int msgsize, bufsize;
  int ret;

     
                                                                           
                                                                             
                                                                           
                                                                           
                                                                           
                                                                            
                                                      
     
  bufsize = 64;                          
  msg = (char *)pg_malloc(bufsize);
  msgsize = 0;
  for (;;)
  {
    Assert(msgsize < bufsize);
    ret = piperead(fd, msg + msgsize, 1);
    if (ret <= 0)
    {
      break;                                  
    }

    Assert(ret == 1);

    if (msg[msgsize] == '\0')
    {
      return msg;                              
    }

    msgsize++;
    if (msgsize == bufsize)                               
    {
      bufsize += 16;                          
      msg = (char *)pg_realloc(msg, bufsize);
    }
  }

                                           
  pg_free(msg);
  return NULL;
}

#ifdef WIN32

   
                                                                              
                                   
   
                                                                        
   
                                                                       
                                                                       
                                                    
   
static int
pgpipe(int handles[2])
{
  pgsocket s, tmp_sock;
  struct sockaddr_in serv_addr;
  int len = sizeof(serv_addr);

                                                                          
  handles[0] = handles[1] = -1;

     
                         
     
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) == PGINVALID_SOCKET)
  {
    pg_log_error("pgpipe: could not create socket: error code %d", WSAGetLastError());
    return -1;
  }

  memset((void *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = pg_hton16(0);
  serv_addr.sin_addr.s_addr = pg_hton32(INADDR_LOOPBACK);
  if (bind(s, (SOCKADDR *)&serv_addr, len) == SOCKET_ERROR)
  {
    pg_log_error("pgpipe: could not bind: error code %d", WSAGetLastError());
    closesocket(s);
    return -1;
  }
  if (listen(s, 1) == SOCKET_ERROR)
  {
    pg_log_error("pgpipe: could not listen: error code %d", WSAGetLastError());
    closesocket(s);
    return -1;
  }
  if (getsockname(s, (SOCKADDR *)&serv_addr, &len) == SOCKET_ERROR)
  {
    pg_log_error("pgpipe: getsockname() failed: error code %d", WSAGetLastError());
    closesocket(s);
    return -1;
  }

     
                        
     
  if ((tmp_sock = socket(AF_INET, SOCK_STREAM, 0)) == PGINVALID_SOCKET)
  {
    pg_log_error("pgpipe: could not create second socket: error code %d", WSAGetLastError());
    closesocket(s);
    return -1;
  }
  handles[1] = (int)tmp_sock;

  if (connect(handles[1], (SOCKADDR *)&serv_addr, len) == SOCKET_ERROR)
  {
    pg_log_error("pgpipe: could not connect socket: error code %d", WSAGetLastError());
    closesocket(handles[1]);
    handles[1] = -1;
    closesocket(s);
    return -1;
  }
  if ((tmp_sock = accept(s, (SOCKADDR *)&serv_addr, &len)) == PGINVALID_SOCKET)
  {
    pg_log_error("pgpipe: could not accept connection: error code %d", WSAGetLastError());
    closesocket(handles[1]);
    handles[1] = -1;
    closesocket(s);
    return -1;
  }
  handles[0] = (int)tmp_sock;

  closesocket(s);
  return 0;
}

   
                                                  
   
static int
piperead(int s, char *buf, int len)
{
  int ret = recv(s, buf, len, 0);

  if (ret < 0 && WSAGetLastError() == WSAECONNRESET)
  {
                          
    ret = 0;
  }
  return ret;
}

#endif            
