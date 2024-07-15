                                                                            
   
             
                                                                            
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <sys/time.h>

#include "miscadmin.h"
#include "storage/proc.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"

                                       
typedef struct timeout_params
{
  TimeoutId index;                                   

                                                                  
  volatile bool indicator;                                   

                                                                        
  timeout_handler_proc timeout_handler;

  TimestampTz start_time;                                           
  TimestampTz fin_time;                                             
} timeout_params;

   
                                                                    
   
static timeout_params all_timeouts[MAX_TIMEOUTS];
static bool all_timeouts_initialized = false;

   
                                                                   
                                                                              
   
static volatile int num_active_timeouts = 0;
static timeout_params *volatile active_timeouts[MAX_TIMEOUTS];

   
                                                                          
                                                                            
   
                                                                          
                                                                          
                                                                           
                                                                     
   
static volatile sig_atomic_t alarm_enabled = false;

#define disable_alarm() (alarm_enabled = false)
#define enable_alarm() (alarm_enabled = true)

                                                                               
                             
   
                                                                        
                                                                        
                                                                   
                                                                        
                                                                               

   
                                                                 
                                 
   
static int
find_active_timeout(TimeoutId id)
{
  int i;

  for (i = 0; i < num_active_timeouts; i++)
  {
    if (active_timeouts[i]->index == id)
    {
      return i;
    }
  }

  return -1;
}

   
                                                                    
                       
   
static void
insert_timeout(TimeoutId id, int index)
{
  int i;

  if (index < 0 || index > num_active_timeouts)
  {
    elog(FATAL, "timeout index %d out of range 0..%d", index, num_active_timeouts);
  }

  for (i = num_active_timeouts - 1; i >= index; i--)
  {
    active_timeouts[i + 1] = active_timeouts[i];
  }

  active_timeouts[index] = &all_timeouts[id];

  num_active_timeouts++;
}

   
                                                      
   
static void
remove_timeout_index(int index)
{
  int i;

  if (index < 0 || index >= num_active_timeouts)
  {
    elog(FATAL, "timeout index %d out of range 0..%d", index, num_active_timeouts - 1);
  }

  for (i = index + 1; i < num_active_timeouts; i++)
  {
    active_timeouts[i - 1] = active_timeouts[i];
  }

  num_active_timeouts--;
}

   
                                       
   
static void
enable_timeout(TimeoutId id, TimestampTz now, TimestampTz fin_time)
{
  int i;

                              
  Assert(all_timeouts_initialized);
  Assert(all_timeouts[id].timeout_handler != NULL);

     
                                                                     
                                                                  
     
  i = find_active_timeout(id);
  if (i >= 0)
  {
    remove_timeout_index(i);
  }

     
                                                                     
                                                   
     
  for (i = 0; i < num_active_timeouts; i++)
  {
    timeout_params *old_timeout = active_timeouts[i];

    if (fin_time < old_timeout->fin_time)
    {
      break;
    }
    if (fin_time == old_timeout->fin_time && id < old_timeout->index)
    {
      break;
    }
  }

     
                                                                  
     
  all_timeouts[id].indicator = false;
  all_timeouts[id].start_time = now;
  all_timeouts[id].fin_time = fin_time;

  insert_timeout(id, i);
}

   
                                                      
   
                                                                         
                  
   
static void
schedule_alarm(TimestampTz now)
{
  if (num_active_timeouts > 0)
  {
    struct itimerval timeval;
    long secs;
    int usecs;

    MemSet(&timeval, 0, sizeof(struct itimerval));

                                                                 
    TimestampDifference(now, active_timeouts[0]->fin_time, &secs, &usecs);

       
                                                                     
                                                               
       
    if (secs == 0 && usecs == 0)
    {
      usecs = 1;
    }

    timeval.it_value.tv_sec = secs;
    timeval.it_value.tv_usec = usecs;

       
                                                                           
                                                                         
                                                                          
                                                 
       
                                                                       
                                                                       
                                                                          
                              
       
                                                                      
                                                                        
                                                                         
                                                                  
                                                                        
                                                              
       
                                                                      
                                                                      
                                                                           
                                                                          
                                                                         
                                                                 
                                               
       
                                                                      
                                                                         
                                                     
       
    enable_alarm();

                             
    if (setitimer(ITIMER_REAL, &timeval, NULL) != 0)
    {
      elog(FATAL, "could not enable SIGALRM timer: %m");
    }
  }
}

                                                                               
                  
                                                                               

   
                              
   
                                                                        
              
   
static void
handle_sig_alarm(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                                                         
                                                                       
                                                    
     
  HOLD_INTERRUPTS();

     
                                                                        
            
     
  SetLatch(MyLatch);

     
                                                                    
     
  if (alarm_enabled)
  {
       
                                                                         
                                                                    
                    
       
    disable_alarm();

    if (num_active_timeouts > 0)
    {
      TimestampTz now = GetCurrentTimestamp();

                                                                
      while (num_active_timeouts > 0 && now >= active_timeouts[0]->fin_time)
      {
        timeout_params *this_timeout = active_timeouts[0];

                                            
        remove_timeout_index(0);

                              
        this_timeout->indicator = true;

                                           
        this_timeout->timeout_handler();

           
                                                                     
                                                                      
                                 
           
        now = GetCurrentTimestamp();
      }

                                                                     
      schedule_alarm(now);
    }
  }

  RESUME_INTERRUPTS();

  errno = save_errno;
}

                                                                               
              
                                                                               

   
                              
   
                                                                    
   
                                                                       
                                                                          
                                                                     
   
void
InitializeTimeouts(void)
{
  int i;

                                                     
  disable_alarm();

  num_active_timeouts = 0;

  for (i = 0; i < MAX_TIMEOUTS; i++)
  {
    all_timeouts[i].index = i;
    all_timeouts[i].indicator = false;
    all_timeouts[i].timeout_handler = NULL;
    all_timeouts[i].start_time = 0;
    all_timeouts[i].fin_time = 0;
  }

  all_timeouts_initialized = true;

                                        
  pqsignal(SIGALRM, handle_sig_alarm);
}

   
                             
   
                                                                       
   
                                                                            
                        
   
TimeoutId
RegisterTimeout(TimeoutId id, timeout_handler_proc handler)
{
  Assert(all_timeouts_initialized);

                                                           

  if (id >= USER_TIMEOUT)
  {
                                                
    for (id = USER_TIMEOUT; id < MAX_TIMEOUTS; id++)
    {
      if (all_timeouts[id].timeout_handler == NULL)
      {
        break;
      }
    }
    if (id >= MAX_TIMEOUTS)
    {
      ereport(FATAL, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("cannot add more timeout reasons")));
    }
  }

  Assert(all_timeouts[id].timeout_handler == NULL);

  all_timeouts[id].timeout_handler = handler;

  return id;
}

   
                                             
   
                                                                                
                                                                            
                                                                         
                                                                           
                                                       
   
void
reschedule_timeouts(void)
{
                                                                          
  if (!all_timeouts_initialized)
  {
    return;
  }

                                              
  disable_alarm();

                                                                
  if (num_active_timeouts > 0)
  {
    schedule_alarm(GetCurrentTimestamp());
  }
}

   
                                                                   
   
                                   
   
void
enable_timeout_after(TimeoutId id, int delay_ms)
{
  TimestampTz now;
  TimestampTz fin_time;

                                              
  disable_alarm();

                                                  
  now = GetCurrentTimestamp();
  fin_time = TimestampTzPlusMilliseconds(now, delay_ms);
  enable_timeout(id, now, fin_time);

                                
  schedule_alarm(now);
}

   
                                                               
   
                                                                         
                                                                             
                                                                             
   
void
enable_timeout_at(TimeoutId id, TimestampTz fin_time)
{
  TimestampTz now;

                                              
  disable_alarm();

                                                  
  now = GetCurrentTimestamp();
  enable_timeout(id, now, fin_time);

                                
  schedule_alarm(now);
}

   
                                     
   
                                                                             
                                                                           
                                                                
   
void
enable_timeouts(const EnableTimeoutParams *timeouts, int count)
{
  TimestampTz now;
  int i;

                                              
  disable_alarm();

                                                      
  now = GetCurrentTimestamp();

  for (i = 0; i < count; i++)
  {
    TimeoutId id = timeouts[i].id;
    TimestampTz fin_time;

    switch (timeouts[i].type)
    {
    case TMPARAM_AFTER:
      fin_time = TimestampTzPlusMilliseconds(now, timeouts[i].delay_ms);
      enable_timeout(id, now, fin_time);
      break;

    case TMPARAM_AT:
      enable_timeout(id, now, timeouts[i].fin_time);
      break;

    default:
      elog(ERROR, "unrecognized timeout type %d", (int)timeouts[i].type);
      break;
    }
  }

                                
  schedule_alarm(now);
}

   
                                 
   
                                                     
                                  
   
                                                                          
                                                               
   
void
disable_timeout(TimeoutId id, bool keep_indicator)
{
  int i;

                              
  Assert(all_timeouts_initialized);
  Assert(all_timeouts[id].timeout_handler != NULL);

                                              
  disable_alarm();

                                                            
  i = find_active_timeout(id);
  if (i >= 0)
  {
    remove_timeout_index(i);
  }

                                                       
  if (!keep_indicator)
  {
    all_timeouts[id].indicator = false;
  }

                                                                
  if (num_active_timeouts > 0)
  {
    schedule_alarm(GetCurrentTimestamp());
  }
}

   
                                     
   
                                                       
                                              
   
                                                             
                                                          
                                                             
   
void
disable_timeouts(const DisableTimeoutParams *timeouts, int count)
{
  int i;

  Assert(all_timeouts_initialized);

                                              
  disable_alarm();

                              
  for (i = 0; i < count; i++)
  {
    TimeoutId id = timeouts[i].id;
    int idx;

    Assert(all_timeouts[id].timeout_handler != NULL);

    idx = find_active_timeout(id);
    if (idx >= 0)
    {
      remove_timeout_index(idx);
    }

    if (!timeouts[i].keep_indicator)
    {
      all_timeouts[id].indicator = false;
    }
  }

                                                                
  if (num_active_timeouts > 0)
  {
    schedule_alarm(GetCurrentTimestamp());
  }
}

   
                                                                 
                                                  
   
void
disable_all_timeouts(bool keep_indicators)
{
  disable_alarm();

     
                                                                            
                                                                            
                                                           
     
  if (num_active_timeouts > 0)
  {
    struct itimerval timeval;

    MemSet(&timeval, 0, sizeof(struct itimerval));
    if (setitimer(ITIMER_REAL, &timeval, NULL) != 0)
    {
      elog(FATAL, "could not disable SIGALRM timer: %m");
    }
  }

  num_active_timeouts = 0;

  if (!keep_indicators)
  {
    int i;

    for (i = 0; i < MAX_TIMEOUTS; i++)
    {
      all_timeouts[i].indicator = false;
    }
  }
}

   
                                                  
   
                                                                        
                                                                           
                                             
   
bool
get_timeout_indicator(TimeoutId id, bool reset_indicator)
{
  if (all_timeouts[id].indicator)
  {
    if (reset_indicator)
    {
      all_timeouts[id].indicator = false;
    }
    return true;
  }
  return false;
}

   
                                                                
   
                                                                            
                                                                          
                                                                        
                             
   
TimestampTz
get_timeout_start_time(TimeoutId id)
{
  return all_timeouts[id].start_time;
}

   
                                                                          
   
                                                                            
                                                                        
                                                                        
                             
   
TimestampTz
get_timeout_finish_time(TimeoutId id)
{
  return all_timeouts[id].fin_time;
}
