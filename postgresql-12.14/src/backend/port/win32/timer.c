                                                                            
   
           
                                                  
   
                                         
   
                                                            
                                 
   
                                                                         
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

                                                       
typedef struct timerCA
{
  struct itimerval value;
  HANDLE event;
  CRITICAL_SECTION crit_sec;
} timerCA;

static timerCA timerCommArea;
static HANDLE timerThreadHandle = INVALID_HANDLE_VALUE;

                             
static DWORD WINAPI
pg_timer_thread(LPVOID param)
{
  DWORD waittime;

  Assert(param == NULL);

  waittime = INFINITE;

  for (;;)
  {
    int r;

    r = WaitForSingleObjectEx(timerCommArea.event, waittime, FALSE);
    if (r == WAIT_OBJECT_0)
    {
                                                              
      EnterCriticalSection(&timerCommArea.crit_sec);
      if (timerCommArea.value.it_value.tv_sec == 0 && timerCommArea.value.it_value.tv_usec == 0)
      {
        waittime = INFINITE;                           
      }
      else
      {
                                                                 
        waittime = (timerCommArea.value.it_value.tv_usec + 999) / 1000 + timerCommArea.value.it_value.tv_sec * 1000;
      }
      ResetEvent(timerCommArea.event);
      LeaveCriticalSection(&timerCommArea.crit_sec);
    }
    else if (r == WAIT_TIMEOUT)
    {
                                                           
      pg_queue_signal(SIGALRM);
      waittime = INFINITE;
    }
    else
    {
                               
      Assert(false);
    }
  }

  return 0;
}

   
                                                             
                                                              
   
int
setitimer(int which, const struct itimerval *value, struct itimerval *ovalue)
{
  Assert(value != NULL);
  Assert(value->it_interval.tv_sec == 0 && value->it_interval.tv_usec == 0);
  Assert(which == ITIMER_REAL);

  if (timerThreadHandle == INVALID_HANDLE_VALUE)
  {
                                                                       
    timerCommArea.event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (timerCommArea.event == NULL)
    {
      ereport(FATAL, (errmsg_internal("could not create timer event: error code %lu", GetLastError())));
    }

    MemSet(&timerCommArea.value, 0, sizeof(struct itimerval));

    InitializeCriticalSection(&timerCommArea.crit_sec);

    timerThreadHandle = CreateThread(NULL, 0, pg_timer_thread, NULL, 0, NULL);
    if (timerThreadHandle == INVALID_HANDLE_VALUE)
    {
      ereport(FATAL, (errmsg_internal("could not create timer thread: error code %lu", GetLastError())));
    }
  }

                                                   
  EnterCriticalSection(&timerCommArea.crit_sec);
  if (ovalue)
  {
    *ovalue = timerCommArea.value;
  }
  timerCommArea.value = *value;
  LeaveCriticalSection(&timerCommArea.crit_sec);
  SetEvent(timerCommArea.event);

  return 0;
}
