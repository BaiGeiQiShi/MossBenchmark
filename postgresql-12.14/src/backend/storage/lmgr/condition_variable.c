                                                                            
   
                        
                                                                         
                                                                      
                                                                      
                                                                   
                                                                     
                                                   
   
                                                                         
                                                                        
   
                                                 
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"
#include "storage/condition_variable.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/proclist.h"
#include "storage/spin.h"
#include "utils/memutils.h"

                                                                        
static ConditionVariable *cv_sleep_target = NULL;

                            
static WaitEventSet *cv_wait_event_set = NULL;

   
                                    
   
void
ConditionVariableInit(ConditionVariable *cv)
{
  SpinLockInit(&cv->mutex);
  proclist_init(&cv->wakeup);
}

   
                                                  
   
                                                                    
                                                                    
                                                                          
                                                                         
                                                           
   
                                                                      
                                                                         
                                                                  
                                    
   
void
ConditionVariablePrepareToSleep(ConditionVariable *cv)
{
  int pgprocno = MyProc->pgprocno;

     
                                                                         
                                                    
     
  if (cv_wait_event_set == NULL)
  {
    WaitEventSet *new_event_set;

    new_event_set = CreateWaitEventSet(TopMemoryContext, 2);
    AddWaitEventToSet(new_event_set, WL_LATCH_SET, PGINVALID_SOCKET, MyLatch, NULL);
    AddWaitEventToSet(new_event_set, WL_EXIT_ON_PM_DEATH, PGINVALID_SOCKET, NULL, NULL);
                                                                  
    cv_wait_event_set = new_event_set;
  }

     
                                                                           
                                                                           
                                                                       
                                                                            
                                                                          
                       
     
  if (cv_sleep_target != NULL)
  {
    ConditionVariableCancelSleep();
  }

                                                             
  cv_sleep_target = cv;

     
                                                                         
                                                  
     
  ResetLatch(MyLatch);

                                     
  SpinLockAcquire(&cv->mutex);
  proclist_push_tail(&cv->wakeup, pgprocno, cvWaitLink);
  SpinLockRelease(&cv->mutex);
}

   
                                                         
   
                                                                            
                                            
   
                                                      
                                                           
                                                  
                                    
   
                                                                        
                                                                          
                                                         
   
void
ConditionVariableSleep(ConditionVariable *cv, uint32 wait_event_info)
{
  WaitEvent event;
  bool done = false;

     
                                                                          
                                                                         
                                                                           
                                                                             
                                                                          
                                                                             
                                                                            
                                                                             
                                                                       
                                                  
     
                                                                            
                                                                     
     
  if (cv_sleep_target != cv)
  {
    ConditionVariablePrepareToSleep(cv);
    return;
  }

  do
  {
    CHECK_FOR_INTERRUPTS();

       
                                                                    
                                                 
       
    (void)WaitEventSetWait(cv_wait_event_set, -1, &event, 1, wait_event_info);

                                                                  
    ResetLatch(MyLatch);

       
                                                                         
                                                                
                                                                           
                                                                           
                                                                         
                                                                           
                                                                           
                                                
                                     
       
                                                                          
                                                                        
                                                                          
       
    SpinLockAcquire(&cv->mutex);
    if (!proclist_contains(&cv->wakeup, MyProc->pgprocno, cvWaitLink))
    {
      done = true;
      proclist_push_tail(&cv->wakeup, MyProc->pgprocno, cvWaitLink);
    }
    SpinLockRelease(&cv->mutex);
  } while (!done);
}

   
                                       
   
                                                                         
                                                           
   
                                                                            
                                                                 
   
void
ConditionVariableCancelSleep(void)
{
  ConditionVariable *cv = cv_sleep_target;

  if (cv == NULL)
  {
    return;
  }

  SpinLockAcquire(&cv->mutex);
  if (proclist_contains(&cv->wakeup, MyProc->pgprocno, cvWaitLink))
  {
    proclist_delete(&cv->wakeup, MyProc->pgprocno, cvWaitLink);
  }
  SpinLockRelease(&cv->mutex);

  cv_sleep_target = NULL;
}

   
                                                                   
   
                                                                          
                                                                        
                                                                          
                                            
   
void
ConditionVariableSignal(ConditionVariable *cv)
{
  PGPROC *proc = NULL;

                                                                
  SpinLockAcquire(&cv->mutex);
  if (!proclist_is_empty(&cv->wakeup))
  {
    proc = proclist_pop_head_node(&cv->wakeup, cvWaitLink);
  }
  SpinLockRelease(&cv->mutex);

                                                                      
  if (proc != NULL)
  {
    SetLatch(&proc->procLatch);
  }
}

   
                                                   
   
                                                                      
                                                                           
                                    
   
void
ConditionVariableBroadcast(ConditionVariable *cv)
{
  int pgprocno = MyProc->pgprocno;
  PGPROC *proc = NULL;
  bool have_sentinel = false;

     
                                                                           
                                                                            
                                                                           
                                                                             
                                                                           
                                                                 
     
                                                                           
                                                                           
                                                                           
                                                                          
                                                                       
                                        
     
                                                                            
                                                                            
                                                                        
                                                                           
                                             
     
  if (cv_sleep_target != NULL)
  {
    ConditionVariableCancelSleep();
  }

     
                                                                            
                                                                       
                                                                        
     
  SpinLockAcquire(&cv->mutex);
                                                             
  Assert(!proclist_contains(&cv->wakeup, pgprocno, cvWaitLink));

  if (!proclist_is_empty(&cv->wakeup))
  {
    proc = proclist_pop_head_node(&cv->wakeup, cvWaitLink);
    if (!proclist_is_empty(&cv->wakeup))
    {
      proclist_push_tail(&cv->wakeup, pgprocno, cvWaitLink);
      have_sentinel = true;
    }
  }
  SpinLockRelease(&cv->mutex);

                                              
  if (proc != NULL)
  {
    SetLatch(&proc->procLatch);
  }

  while (have_sentinel)
  {
       
                                                                           
                                                                           
                            
       
                                                                           
                                                                          
                                                                        
                                                                      
                                                                    
                                                                   
       
    proc = NULL;
    SpinLockAcquire(&cv->mutex);
    if (!proclist_is_empty(&cv->wakeup))
    {
      proc = proclist_pop_head_node(&cv->wakeup, cvWaitLink);
    }
    have_sentinel = proclist_contains(&cv->wakeup, pgprocno, cvWaitLink);
    SpinLockRelease(&cv->mutex);

    if (proc != NULL && proc != MyProc)
    {
      SetLatch(&proc->procLatch);
    }
  }
}
