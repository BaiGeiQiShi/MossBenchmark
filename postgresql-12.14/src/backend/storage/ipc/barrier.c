                                                                            
   
             
                                                       
   
                                                                         
                                                                        
   
                                                                     
                                                                             
                                                                               
                                                                  
   
                                                                          
                                                                               
                                                                          
                                                                        
                                                                            
                                                                    
                                                                            
                                                                               
                                                                               
                                      
   
                                                                              
                                                                             
              
   
                                                                         
                                                                               
                                                                          
                                                                            
                                                                            
                                                                              
                                                                              
                                                                             
                                                                         
   
                    
                                            
                    
                                            
                    
                                            
   
                                                                               
                                                                          
                                                                           
                                                                             
                                                                         
                                                                            
                                                                          
                                                                              
                                                                              
                                                                            
                                                                            
                                                                            
                                        
   
                                        
                      
         
                     
                        
                                                
                     
                        
                                                
                     
                        
                                                
         
                                
   
                                                                           
                                                                    
   
                                                                
   
                  
                                       
   
                                                                            
   

#include "postgres.h"
#include "storage/barrier.h"

static inline bool
BarrierDetachImpl(Barrier *barrier, bool arrive);

   
                                                                               
                                                                         
                                                                               
                                         
                                                                       
                            
   
void
BarrierInit(Barrier *barrier, int participants)
{
  SpinLockInit(&barrier->mutex);
  barrier->participants = participants;
  barrier->arrived = 0;
  barrier->phase = 0;
  barrier->elected = 0;
  barrier->static_party = participants > 0;
  ConditionVariableInit(&barrier->condition_variable);
}

   
                                                                              
                                                                           
             
   
                                                                          
                                                                             
                                                      
   
                                                                           
                                                                              
                                                                           
   
bool
BarrierArriveAndWait(Barrier *barrier, uint32 wait_event_info)
{
  bool release = false;
  bool elected;
  int start_phase;
  int next_phase;

  SpinLockAcquire(&barrier->mutex);
  start_phase = barrier->phase;
  next_phase = start_phase + 1;
  ++barrier->arrived;
  if (barrier->arrived == barrier->participants)
  {
    release = true;
    barrier->arrived = 0;
    barrier->phase = next_phase;
    barrier->elected = next_phase;
  }
  SpinLockRelease(&barrier->mutex);

     
                                                                            
                                                                             
                              
     
  if (release)
  {
    ConditionVariableBroadcast(&barrier->condition_variable);

    return true;
  }

     
                                                                      
                        
     
  elected = false;
  ConditionVariablePrepareToSleep(&barrier->condition_variable);
  for (;;)
  {
       
                                                                         
                                                                     
                                                                           
                                                                       
                                                                       
                                 
       
    SpinLockAcquire(&barrier->mutex);
    Assert(barrier->phase == start_phase || barrier->phase == next_phase);
    release = barrier->phase == next_phase;
    if (release && barrier->elected != next_phase)
    {
         
                                                                      
                                                                        
                                                                    
                                                                         
                                                                     
         
      barrier->elected = barrier->phase;
      elected = true;
    }
    SpinLockRelease(&barrier->mutex);
    if (release)
    {
      break;
    }
    ConditionVariableSleep(&barrier->condition_variable, wait_event_info);
  }
  ConditionVariableCancelSleep();

  return elected;
}

   
                                                                            
                                      
   
bool
BarrierArriveAndDetach(Barrier *barrier)
{
  return BarrierDetachImpl(barrier, true);
}

   
                                                                         
                                                                  
                                                        
   
int
BarrierAttach(Barrier *barrier)
{
  int phase;

  Assert(!barrier->static_party);

  SpinLockAcquire(&barrier->mutex);
  ++barrier->participants;
  phase = barrier->phase;
  SpinLockRelease(&barrier->mutex);

  return phase;
}

   
                                                               
                                                                              
                                                                          
   
bool
BarrierDetach(Barrier *barrier)
{
  return BarrierDetachImpl(barrier, false);
}

   
                                                                        
   
int
BarrierPhase(Barrier *barrier)
{
     
                                                                       
                                                                         
                                                                         
           
     
  return barrier->phase;
}

   
                                                                            
                                                           
   
int
BarrierParticipants(Barrier *barrier)
{
  int participants;

  SpinLockAcquire(&barrier->mutex);
  participants = barrier->participants;
  SpinLockRelease(&barrier->mutex);

  return participants;
}

   
                                                                             
                                                                        
                                                                            
                                                                              
                   
   
static inline bool
BarrierDetachImpl(Barrier *barrier, bool arrive)
{
  bool release;
  bool last;

  Assert(!barrier->static_party);

  SpinLockAcquire(&barrier->mutex);
  Assert(barrier->participants > 0);
  --barrier->participants;

     
                                                                            
                                                                          
                                                                          
     
  if ((arrive || barrier->participants > 0) && barrier->arrived == barrier->participants)
  {
    release = true;
    barrier->arrived = 0;
    ++barrier->phase;
  }
  else
  {
    release = false;
  }

  last = barrier->participants == 0;
  SpinLockRelease(&barrier->mutex);

  if (release)
  {
    ConditionVariableBroadcast(&barrier->condition_variable);
  }

  return last;
}
