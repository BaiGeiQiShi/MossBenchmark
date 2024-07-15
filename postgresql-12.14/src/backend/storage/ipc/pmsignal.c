                                                                            
   
              
                                                                    
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "replication/walsender.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"
#include "utils/memutils.h"

   
                                                                       
                                                                        
                                                                        
                                                                      
                                                                     
                                          
   
                                                                          
                                                                      
                                                                         
   
                                                                            
                                                                          
                                                                           
                                                                            
                                                                               
                                                                            
                                                                            
                                                                       
                                                                           
                                  
   
                                                                           
                                                                     
                                                                            
                                                                          
   

#define PM_CHILD_UNUSED 0                                            
#define PM_CHILD_ASSIGNED 1
#define PM_CHILD_ACTIVE 2
#define PM_CHILD_WALSENDER 3

                                                                      
struct PMSignalData
{
                        
  sig_atomic_t PMSignalFlags[NUM_PMSIGNALS];
                               
  int num_child_flags;                                     
  sig_atomic_t PMChildFlags[FLEXIBLE_ARRAY_MEMBER];
};

                                                                           
NON_EXEC_STATIC volatile PMSignalData *PMSignalState = NULL;

   
                                                                       
                                                                         
                                                                         
   
static int num_child_inuse;                                      
static int next_child_inuse;                                 
static bool *PMChildInUse;                                           

   
                                                     
   
#ifdef USE_POSTMASTER_DEATH_SIGNAL
volatile sig_atomic_t postmaster_possibly_dead = false;

static void
postmaster_death_handler(int signo)
{
  postmaster_possibly_dead = true;
}

   
                                                                            
                                                 
   
                                                                       
                                                                        
                                                      
   
#if defined(SIGINFO)
#define POSTMASTER_DEATH_SIGNAL SIGINFO
#elif defined(SIGPWR)
#define POSTMASTER_DEATH_SIGNAL SIGPWR
#else
#error "cannot find a signal to use for postmaster death"
#endif

#endif                                  

   
                     
                                                        
   
Size
PMSignalShmemSize(void)
{
  Size size;

  size = offsetof(PMSignalData, PMChildFlags);
  size = add_size(size, mul_size(MaxLivePostmasterChildren(), sizeof(sig_atomic_t)));

  return size;
}

   
                                                                
   
void
PMSignalShmemInit(void)
{
  bool found;

  PMSignalState = (PMSignalData *)ShmemInitStruct("PMSignalState", PMSignalShmemSize(), &found);

  if (!found)
  {
    MemSet(unvolatize(PMSignalData *, PMSignalState), 0, PMSignalShmemSize());
    num_child_inuse = MaxLivePostmasterChildren();
    PMSignalState->num_child_flags = num_child_inuse;

       
                                                                    
                                                                       
                                                                        
                                                                           
                                                                
       
    if (PostmasterContext != NULL)
    {
      if (PMChildInUse)
      {
        pfree(PMChildInUse);
      }
      PMChildInUse = (bool *)MemoryContextAllocZero(PostmasterContext, num_child_inuse * sizeof(bool));
    }
    next_child_inuse = 0;
  }
}

   
                                                                     
   
void
SendPostmasterSignal(PMSignalReason reason)
{
                                                     
  if (!IsUnderPostmaster)
  {
    return;
  }
                                      
  PMSignalState->PMSignalFlags[reason] = true;
                                 
  kill(PostmasterPid, SIGUSR1);
}

   
                                                                        
                                                                        
                            
   
bool
CheckPostmasterSignal(PMSignalReason reason)
{
                                                                   
  if (PMSignalState->PMSignalFlags[reason])
  {
    PMSignalState->PMSignalFlags[reason] = false;
    return true;
  }
  return false;
}

   
                                                                          
                                                                        
               
   
                                                                         
                    
   
int
AssignPostmasterChildSlot(void)
{
  int slot = next_child_inuse;
  int n;

     
                                                                            
                                                                             
                                                                        
                                    
     
  for (n = num_child_inuse; n > 0; n--)
  {
    if (--slot < 0)
    {
      slot = num_child_inuse - 1;
    }
    if (!PMChildInUse[slot])
    {
      PMChildInUse[slot] = true;
      PMSignalState->PMChildFlags[slot] = PM_CHILD_ASSIGNED;
      next_child_inuse = slot;
      return slot + 1;
    }
  }

                                                                         
  elog(FATAL, "no free slots in PMChildFlags array");
  return 0;                          
}

   
                                                                           
                                                                  
   
                                                                            
                                                                        
   
bool
ReleasePostmasterChildSlot(int slot)
{
  bool result;

  Assert(slot > 0 && slot <= num_child_inuse);
  slot--;

     
                                                                        
                                                                        
                                                                   
     
  result = (PMSignalState->PMChildFlags[slot] == PM_CHILD_ASSIGNED);
  PMSignalState->PMChildFlags[slot] = PM_CHILD_UNUSED;
  PMChildInUse[slot] = false;
  return result;
}

   
                                                                   
                                                              
   
bool
IsPostmasterChildWalSender(int slot)
{
  Assert(slot > 0 && slot <= num_child_inuse);
  slot--;

  if (PMSignalState->PMChildFlags[slot] == PM_CHILD_WALSENDER)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                                         
                                                                       
   
void
MarkPostmasterChildActive(void)
{
  int slot = MyPMChildSlot;

  Assert(slot > 0 && slot <= PMSignalState->num_child_flags);
  slot--;
  Assert(PMSignalState->PMChildFlags[slot] == PM_CHILD_ASSIGNED);
  PMSignalState->PMChildFlags[slot] = PM_CHILD_ACTIVE;
}

   
                                                                          
                                                                             
                    
   
void
MarkPostmasterChildWalSender(void)
{
  int slot = MyPMChildSlot;

  Assert(am_walsender);

  Assert(slot > 0 && slot <= PMSignalState->num_child_flags);
  slot--;
  Assert(PMSignalState->PMChildFlags[slot] == PM_CHILD_ACTIVE);
  PMSignalState->PMChildFlags[slot] = PM_CHILD_WALSENDER;
}

   
                                                                       
                                                        
   
void
MarkPostmasterChildInactive(void)
{
  int slot = MyPMChildSlot;

  Assert(slot > 0 && slot <= PMSignalState->num_child_flags);
  slot--;
  Assert(PMSignalState->PMChildFlags[slot] == PM_CHILD_ACTIVE || PMSignalState->PMChildFlags[slot] == PM_CHILD_WALSENDER);
  PMSignalState->PMChildFlags[slot] = PM_CHILD_ASSIGNED;
}

   
                                                                               
   
                                                                              
                                                                         
                                                                              
   
bool
PostmasterIsAliveInternal(void)
{
#ifdef USE_POSTMASTER_DEATH_SIGNAL
     
                                                                       
                                                                            
                                                 
     
  postmaster_possibly_dead = false;
#endif

#ifndef WIN32
  {
    char c;
    ssize_t rc;

    rc = read(postmaster_alive_fds[POSTMASTER_FD_WATCH], &c, 1);

       
                                                                         
                         
       
    if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
      return true;
    }
    else
    {
         
                                                                     
               
         

#ifdef USE_POSTMASTER_DEATH_SIGNAL
      postmaster_possibly_dead = true;
#endif

      if (rc < 0)
      {
        elog(FATAL, "read on postmaster death monitoring pipe failed: %m");
      }
      else if (rc > 0)
      {
        elog(FATAL, "unexpected data in postmaster death monitoring pipe");
      }

      return false;
    }
  }

#else            
  if (WaitForSingleObject(PostmasterHandle, 0) == WAIT_TIMEOUT)
  {
    return true;
  }
  else
  {
#ifdef USE_POSTMASTER_DEATH_SIGNAL
    postmaster_possibly_dead = true;
#endif
    return false;
  }
#endif            
}

   
                                                                              
   
void
PostmasterDeathSignalInit(void)
{
#ifdef USE_POSTMASTER_DEATH_SIGNAL
  int signum = POSTMASTER_DEATH_SIGNAL;

                                    
  pqsignal(signum, postmaster_death_handler);

                                        
#if defined(PR_SET_PDEATHSIG)
  if (prctl(PR_SET_PDEATHSIG, signum) < 0)
  {
    elog(ERROR, "could not request parent death signal: %m");
  }
#elif defined(PROC_PDEATHSIG_CTL)
  if (procctl(P_PID, 0, PROC_PDEATHSIG_CTL, &signum) < 0)
  {
    elog(ERROR, "could not request parent death signal: %m");
  }
#else
#error "USE_POSTMASTER_DEATH_SIGNAL set, but there is no mechanism to request the signal"
#endif

     
                                                                            
                                           
     
  postmaster_possibly_dead = true;
#endif                                  
}
