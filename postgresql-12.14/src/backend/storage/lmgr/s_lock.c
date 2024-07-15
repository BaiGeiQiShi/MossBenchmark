                                                                            
   
            
                                                      
   
                                                                          
                                                                            
                                                                            
                                                                    
                                                                             
                                                                               
                                                                        
                                   
   
                                                                         
                                                                   
                                                                           
                                                                         
                     
   
                                                                      
                                                                           
                                                                          
                                                                    
                                                                         
                                                                         
                                                                          
                                                                           
                                                                         
                                                                        
                                      
   
                                                                        
                                                                           
                                                                           
                                                                     
          
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <time.h>
#include <unistd.h>

#include "storage/s_lock.h"
#include "port/atomics.h"

#define MIN_SPINS_PER_DELAY 10
#define MAX_SPINS_PER_DELAY 1000
#define NUM_DELAYS 1000
#define MIN_DELAY_USEC 1000L
#define MAX_DELAY_USEC 1000000L

slock_t dummy_spinlock;

static int spins_per_delay = DEFAULT_SPINS_PER_DELAY;

   
                                                    
   
static void
s_lock_stuck(const char *file, int line, const char *func)
{
  if (!func)
  {
    func = "(unknown)";
  }
#if defined(S_LOCK_TEST)
  fprintf(stderr, "\nStuck spinlock detected at %s, %s:%d.\n", func, file, line);
  exit(1);
#else
  elog(PANIC, "stuck spinlock detected at %s, %s:%d", func, file, line);
#endif
}

   
                                                                          
   
int
s_lock(volatile slock_t *lock, const char *file, int line, const char *func)
{
  SpinDelayStatus delayStatus;

  init_spin_delay(&delayStatus, file, line, func);

  while (TAS_SPIN(lock))
  {
    perform_spin_delay(&delayStatus);
  }

  finish_spin_delay(&delayStatus);

  return delayStatus.delays;
}

#ifdef USE_DEFAULT_S_UNLOCK
void
s_unlock(volatile slock_t *lock)
{
#ifdef TAS_ACTIVE_WORD
                    
  *TAS_ACTIVE_WORD(lock) = -1;
#else
  *lock = 0;
#endif
}
#endif

   
                                                
   
void
perform_spin_delay(SpinDelayStatus *status)
{
                                                     
  SPIN_DELAY();

                                                     
  if (++(status->spins) >= spins_per_delay)
  {
    if (++(status->delays) > NUM_DELAYS)
    {
      s_lock_stuck(status->file, status->line, status->func);
    }

    if (status->cur_delay == 0)                           
    {
      status->cur_delay = MIN_DELAY_USEC;
    }

    pg_usleep(status->cur_delay);

#if defined(S_LOCK_TEST)
    fprintf(stdout, "*");
    fflush(stdout);
#endif

                                                               
    status->cur_delay += (int)(status->cur_delay * ((double)random() / (double)MAX_RANDOM_VALUE) + 0.5);
                                                         
    if (status->cur_delay > MAX_DELAY_USEC)
    {
      status->cur_delay = MIN_DELAY_USEC;
    }

    status->spins = 0;
  }
}

   
                                                                        
   
                                                                     
                                                                           
                                                                   
                                                                          
                                                                    
                                                                      
                              
   
                                                                         
                                                                   
                                                                          
                                                                         
                                         
   
void
finish_spin_delay(SpinDelayStatus *status)
{
  if (status->cur_delay == 0)
  {
                               
    if (spins_per_delay < MAX_SPINS_PER_DELAY)
    {
      spins_per_delay = Min(spins_per_delay + 100, MAX_SPINS_PER_DELAY);
    }
  }
  else
  {
    if (spins_per_delay > MIN_SPINS_PER_DELAY)
    {
      spins_per_delay = Max(spins_per_delay - 1, MIN_SPINS_PER_DELAY);
    }
  }
}

   
                                                             
   
                                                                           
   
void
set_spins_per_delay(int shared_spins_per_delay)
{
  spins_per_delay = shared_spins_per_delay;
}

   
                                                                  
   
                                                                           
   
int
update_spins_per_delay(int shared_spins_per_delay)
{
     
                                                                          
                                                                             
                                                                           
                                                                             
     
                                                                           
                                                                          
                                         
     
  return (shared_spins_per_delay * 15 + spins_per_delay) / 16;
}

   
                                                                         
                            
                                                                   
   
                                                                              
                                                                               
   

#ifdef HAVE_SPINLOCKS                                  

#if defined(__GNUC__)

   
                                            
   

   
                                                                        
                                                                        
                                                                       
   
#if defined(__m68k__) && !defined(__linux__)
                                                    
static void
tas_dummy()
{
  __asm__ __volatile__(
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(__ELF__)
                                                       
      "\
.global		tas 				\n\
tas:							\n\
			movel	%sp@(0x4),%a0	\n\
			tas 	%a0@		\n\
			beq 	_success	\n\
			moveq	#-128,%d0	\n\
			rts 				\n\
_success:						\n\
			moveq	#0,%d0		\n\
			rts 				\n"
#else
      "\
.global		_tas				\n\
_tas:							\n\
			movel	sp@(0x4),a0	\n\
			tas 	a0@			\n\
			beq 	_success	\n\
			moveq 	#-128,d0	\n\
			rts					\n\
_success:						\n\
			moveq 	#0,d0		\n\
			rts					\n"
#endif                                             
  );
}
#endif                             
#endif                   
#endif                     

                                                                               
#if defined(S_LOCK_TEST)

   
                                                         
   

struct test_lock_struct
{
  char pad1;
  slock_t lock;
  char pad2;
};

volatile struct test_lock_struct test_lock;

int
main()
{
  srandom((unsigned int)time(NULL));

  test_lock.pad1 = test_lock.pad2 = 0x44;

  S_INIT_LOCK(&test_lock.lock);

  if (test_lock.pad1 != 0x44 || test_lock.pad2 != 0x44)
  {
    printf("S_LOCK_TEST: failed, declared datatype is wrong size\n");
    return 1;
  }

  if (!S_LOCK_FREE(&test_lock.lock))
  {
    printf("S_LOCK_TEST: failed, lock not initialized\n");
    return 1;
  }

  S_LOCK(&test_lock.lock);

  if (test_lock.pad1 != 0x44 || test_lock.pad2 != 0x44)
  {
    printf("S_LOCK_TEST: failed, declared datatype is wrong size\n");
    return 1;
  }

  if (S_LOCK_FREE(&test_lock.lock))
  {
    printf("S_LOCK_TEST: failed, lock not locked\n");
    return 1;
  }

  S_UNLOCK(&test_lock.lock);

  if (test_lock.pad1 != 0x44 || test_lock.pad2 != 0x44)
  {
    printf("S_LOCK_TEST: failed, declared datatype is wrong size\n");
    return 1;
  }

  if (!S_LOCK_FREE(&test_lock.lock))
  {
    printf("S_LOCK_TEST: failed, lock not unlocked\n");
    return 1;
  }

  S_LOCK(&test_lock.lock);

  if (test_lock.pad1 != 0x44 || test_lock.pad2 != 0x44)
  {
    printf("S_LOCK_TEST: failed, declared datatype is wrong size\n");
    return 1;
  }

  if (S_LOCK_FREE(&test_lock.lock))
  {
    printf("S_LOCK_TEST: failed, lock not re-locked\n");
    return 1;
  }

  printf("S_LOCK_TEST: this will print %d stars and then\n", NUM_DELAYS);
  printf("             exit with a 'stuck spinlock' message\n");
  printf("             if S_LOCK() and TAS() are working.\n");
  fflush(stdout);

  s_lock(&test_lock.lock, __FILE__, __LINE__);

  printf("S_LOCK_TEST: failed, lock not locked\n");
  return 1;
}

#endif                  
