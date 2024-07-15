                                                                            
   
          
                                                        
   
   
                                                                       
                                                                       
                                                                       
                                                                     
                                     
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "storage/pg_sema.h"
#include "storage/shmem.h"
#include "storage/spin.h"

#ifndef HAVE_SPINLOCKS

   
                                                         
   

#ifndef HAVE_ATOMICS
#define NUM_EMULATION_SEMAPHORES (NUM_SPINLOCK_SEMAPHORES + NUM_ATOMICS_SEMAPHORES)
#else
#define NUM_EMULATION_SEMAPHORES (NUM_SPINLOCK_SEMAPHORES)
#endif                      

PGSemaphore *SpinlockSemaArray;

#else                      

#define NUM_EMULATION_SEMAPHORES 0

#endif                     

   
                                                                              
            
   
Size
SpinlockSemaSize(void)
{
  return NUM_EMULATION_SEMAPHORES * sizeof(PGSemaphore);
}

   
                                                            
   
int
SpinlockSemas(void)
{
  return NUM_EMULATION_SEMAPHORES;
}

#ifndef HAVE_SPINLOCKS

   
                                  
   
                                                    
   
void
SpinlockSemaInit(void)
{
  PGSemaphore *spinsemas;
  int nsemas = SpinlockSemas();
  int i;

     
                                                                     
                                                
     
  spinsemas = (PGSemaphore *)ShmemAllocUnlocked(SpinlockSemaSize());
  for (i = 0; i < nsemas; ++i)
  {
    spinsemas[i] = PGSemaphoreCreate();
  }
  SpinlockSemaArray = spinsemas;
}

   
                                                         
   
                                                                                
                                                                            
                                                                            
                                                      
   
                                                                               
                                                                               
                                                                               
                                                                       
                                                                      
   
                                                                         
                                                                            
                                                                      
                                     
   

static inline void
s_check_valid(int lockndx)
{
  if (unlikely(lockndx <= 0 || lockndx > NUM_EMULATION_SEMAPHORES))
  {
    elog(ERROR, "invalid spinlock number: %d", lockndx);
  }
}

void
s_init_lock_sema(volatile slock_t *lock, bool nested)
{
  static uint32 counter = 0;
  uint32 offset;
  uint32 sema_total;
  uint32 idx;

  if (nested)
  {
       
                                                                  
                                              
       
    offset = 1 + NUM_SPINLOCK_SEMAPHORES;
    sema_total = NUM_ATOMICS_SEMAPHORES;
  }
  else
  {
    offset = 1;
    sema_total = NUM_SPINLOCK_SEMAPHORES;
  }

  idx = (counter++ % sema_total) + offset;

                                            
  s_check_valid(idx);

  *lock = idx;
}

void
s_unlock_sema(volatile slock_t *lock)
{
  int lockndx = *lock;

  s_check_valid(lockndx);

  PGSemaphoreUnlock(SpinlockSemaArray[lockndx - 1]);
}

bool
s_lock_free_sema(volatile slock_t *lock)
{
                                                 
  elog(ERROR, "spin.c does not support S_LOCK_FREE()");
  return false;
}

int
tas_sema(volatile slock_t *lock)
{
  int lockndx = *lock;

  s_check_valid(lockndx);

                                                  
  return !PGSemaphoreTryLock(SpinlockSemaArray[lockndx - 1]);
}

#endif                      
