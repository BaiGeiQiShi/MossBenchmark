                                                                            
   
            
                                                        
   
                                                                         
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include "libpq/pqsignal.h"

   
                                                                     
                                                                      
                                                                      
                                                                       
   
volatile int pg_signal_queue;
int pg_signal_mask;

HANDLE pgwin32_signal_event;
HANDLE pgwin32_initial_signal_pipe = INVALID_HANDLE_VALUE;

   
                                                                                
                                                                  
   
static CRITICAL_SECTION pg_signal_crit_sec;

                                                                             
static pqsigfunc pg_signal_array[PG_SIGNAL_COUNT];
static pqsigfunc pg_signal_defaults[PG_SIGNAL_COUNT];

                                      
static DWORD WINAPI
pg_signal_thread(LPVOID param);
static BOOL WINAPI
pg_console_handler(DWORD dwCtrlType);

   
                                                                 
                                     
   
                                                                              
   
void
pg_usleep(long microsec)
{
  Assert(pgwin32_signal_event != NULL);
  if (WaitForSingleObject(pgwin32_signal_event, (microsec < 500 ? 1 : (microsec + 500) / 1000)) == WAIT_OBJECT_0)
  {
    pgwin32_dispatch_queued_signals();
    errno = EINTR;
    return;
  }
}

                    
void
pgwin32_signal_initialize(void)
{
  int i;
  HANDLE signal_thread_handle;

  InitializeCriticalSection(&pg_signal_crit_sec);

  for (i = 0; i < PG_SIGNAL_COUNT; i++)
  {
    pg_signal_array[i] = SIG_DFL;
    pg_signal_defaults[i] = SIG_IGN;
  }
  pg_signal_mask = 0;
  pg_signal_queue = 0;

                                                           
  pgwin32_signal_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (pgwin32_signal_event == NULL)
  {
    ereport(FATAL, (errmsg_internal("could not create signal event: error code %lu", GetLastError())));
  }

                                          
  signal_thread_handle = CreateThread(NULL, 0, pg_signal_thread, NULL, 0, NULL);
  if (signal_thread_handle == NULL)
  {
    ereport(FATAL, (errmsg_internal("could not create signal handler thread")));
  }

                                                           
  if (!SetConsoleCtrlHandler(pg_console_handler, TRUE))
  {
    ereport(FATAL, (errmsg_internal("could not set console control handler")));
  }
}

   
                                                         
                                                                 
                          
   
void
pgwin32_dispatch_queued_signals(void)
{
  int exec_mask;

  Assert(pgwin32_signal_event != NULL);
  EnterCriticalSection(&pg_signal_crit_sec);
  while ((exec_mask = UNBLOCKED_SIGNAL_QUEUE()) != 0)
  {
                                                            
    int i;

    for (i = 1; i < PG_SIGNAL_COUNT; i++)
    {
      if (exec_mask & sigmask(i))
      {
                                 
        pqsigfunc sig = pg_signal_array[i];

        if (sig == SIG_DFL)
        {
          sig = pg_signal_defaults[i];
        }
        pg_signal_queue &= ~sigmask(i);
        if (sig != SIG_ERR && sig != SIG_IGN && sig != SIG_DFL)
        {
          LeaveCriticalSection(&pg_signal_crit_sec);
          sig(i);
          EnterCriticalSection(&pg_signal_crit_sec);
          break;                                               
                                                          
                              
        }
      }
    }
  }
  ResetEvent(pgwin32_signal_event);
  LeaveCriticalSection(&pg_signal_crit_sec);
}

                                                                  
int
pqsigsetmask(int mask)
{
  int prevmask;

  prevmask = pg_signal_mask;
  pg_signal_mask = mask;

     
                                                                          
                                           
     
  pgwin32_dispatch_queued_signals();

  return prevmask;
}

   
                                         
   
                                                
   
pqsigfunc
pqsignal(int signum, pqsigfunc handler)
{
  pqsigfunc prevfunc;

  if (signum >= PG_SIGNAL_COUNT || signum < 0)
  {
    return SIG_ERR;
  }
  prevfunc = pg_signal_array[signum];
  pg_signal_array[signum] = handler;
  return prevfunc;
}

                                                       
HANDLE
pgwin32_create_signal_listener(pid_t pid)
{
  char pipename[128];
  HANDLE pipe;

  snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\pgsignal_%u", (int)pid);

  pipe = CreateNamedPipe(pipename, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 16, 16, 1000, NULL);

  if (pipe == INVALID_HANDLE_VALUE)
  {
    ereport(ERROR, (errmsg("could not create signal listener pipe for PID %d: error code %lu", (int)pid, GetLastError())));
  }

  return pipe;
}

   
                                                            
                                     
                                                      
                    
   

   
                                                                          
   
void
pg_queue_signal(int signum)
{
  Assert(pgwin32_signal_event != NULL);
  if (signum >= PG_SIGNAL_COUNT || signum <= 0)
  {
    return;                                   
  }

  EnterCriticalSection(&pg_signal_crit_sec);
  pg_signal_queue |= sigmask(signum);
  LeaveCriticalSection(&pg_signal_crit_sec);

  SetEvent(pgwin32_signal_event);
}

   
                                                                        
                                                                         
                             
   
static DWORD WINAPI
pg_signal_dispatch_thread(LPVOID param)
{
  HANDLE pipe = (HANDLE)param;
  BYTE sigNum;
  DWORD bytes;

  if (!ReadFile(pipe, &sigNum, 1, &bytes, NULL))
  {
                                    
    CloseHandle(pipe);
    return 0;
  }
  if (bytes != 1)
  {
                                                               
    CloseHandle(pipe);
    return 0;
  }

     
                                                                          
                                                                             
                                                                         
                                                                             
                                                                            
                         
     
  pg_queue_signal(sigNum);

     
                                                                           
                   
     
  WriteFile(pipe, &sigNum, 1, &bytes, NULL);                              
                                                        

     
                                                                          
                                                                       
                                                                  
     
  FlushFileBuffers(pipe);

                                                                        
  DisconnectNamedPipe(pipe);

                       
  CloseHandle(pipe);

  return 0;
}

                            
static DWORD WINAPI
pg_signal_thread(LPVOID param)
{
  char pipename[128];
  HANDLE pipe = pgwin32_initial_signal_pipe;

  snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\pgsignal_%lu", GetCurrentProcessId());

  for (;;)
  {
    BOOL fConnected;
    HANDLE hThread;

                                                          
    if (pipe == INVALID_HANDLE_VALUE)
    {
      pipe = CreateNamedPipe(pipename, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 16, 16, 1000, NULL);

      if (pipe == INVALID_HANDLE_VALUE)
      {
        write_stderr("could not create signal listener pipe: error code %lu; retrying\n", GetLastError());
        SleepEx(500, FALSE);
        continue;
      }
    }

       
                                                                      
                                                                         
                                                           
       
    fConnected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (fConnected)
    {
      HANDLE newpipe;

         
                                                                      
                                                         
         
                                                                    
                                                                    
                                                                       
                                                                        
                                                              
         
      newpipe = CreateNamedPipe(pipename, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 16, 16, 1000, NULL);
      if (newpipe == INVALID_HANDLE_VALUE)
      {
           
                                                                      
                                                                       
                                                                
                                             
           
        write_stderr("could not create signal listener pipe: error code %lu; retrying\n", GetLastError());

           
                                                                      
                                                                      
           
      }
      hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pg_signal_dispatch_thread, (LPVOID)pipe, 0, NULL);
      if (hThread == INVALID_HANDLE_VALUE)
      {
        write_stderr("could not create signal dispatch thread: error code %lu\n", GetLastError());
      }
      else
      {
        CloseHandle(hThread);
      }

         
                                                                        
                                                                        
                             
         
      pipe = newpipe;
    }
    else
    {
         
                                                   
         
                                                                    
                                                            
         
      CloseHandle(pipe);
      pipe = INVALID_HANDLE_VALUE;
    }
  }
  return 0;
}

                                                            
                                         
static BOOL WINAPI
pg_console_handler(DWORD dwCtrlType)
{
  if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT || dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT)
  {
    pg_queue_signal(SIGINT);
    return TRUE;
  }
  return FALSE;
}
