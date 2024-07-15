                                                                            
   
                
                                                             
   
                                                                      
                                                                      
   
                                                                             
                                                                         
                                                                              
                                                                             
                                                                             
                                                                         
                                                                       
   
   
                                                                         
                                                                        
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/pg_sema.h"
#include "storage/shmem.h"

                             
#if defined(USE_NAMED_POSIX_SEMAPHORES) && defined(EXEC_BACKEND)
#error cannot use named POSIX semaphores with EXEC_BACKEND
#endif

typedef union SemTPadded
{
  sem_t pgsem;
  char pad[PG_CACHE_LINE_SIZE];
} SemTPadded;

                                                           
typedef struct PGSemaphoreData
{
  SemTPadded sem_padded;
} PGSemaphoreData;

#define PG_SEM_REF(x) (&(x)->sem_padded.pgsem)

#define IPCProtection (0600)                                 

#ifdef USE_NAMED_POSIX_SEMAPHORES
static sem_t **mySemPointers;                                       
#else
static PGSemaphore sharedSemas;                                                
#endif
static int numSems;                                         
static int maxSems;                                        
static int nextSemKey;                       

static void
ReleaseSemaphores(int status, Datum arg);

#ifdef USE_NAMED_POSIX_SEMAPHORES

   
                        
   
                                            
   
                                                                           
                                                                               
             
   
static sem_t *
PosixSemaphoreCreate(void)
{
  int semKey;
  char semname[64];
  sem_t *mySem;

  for (;;)
  {
    semKey = nextSemKey++;

    snprintf(semname, sizeof(semname), "/pgsql-%d", semKey);

    mySem = sem_open(semname, O_CREAT | O_EXCL, (mode_t)IPCProtection, (unsigned)1);

#ifdef SEM_FAILED
    if (mySem != (sem_t *)SEM_FAILED)
    {
      break;
    }
#else
    if (mySem != (sem_t *)(-1))
    {
      break;
    }
#endif

                                             
    if (errno == EEXIST || errno == EACCES || errno == EINTR)
    {
      continue;
    }

       
                               
       
    elog(FATAL, "sem_open(\"%s\") failed: %m", semname);
  }

     
                                                                           
                                                         
     
  sem_unlink(semname);

  return mySem;
}
#else                                   

   
                        
   
                                              
   
static void
PosixSemaphoreCreate(sem_t *sem)
{
  if (sem_init(sem, 1, 1) < 0)
  {
    elog(FATAL, "sem_init failed: %m");
  }
}
#endif                                 

   
                                            
   
static void
PosixSemaphoreKill(sem_t *sem)
{
#ifdef USE_NAMED_POSIX_SEMAPHORES
                                                 
  if (sem_close(sem) < 0)
  {
    elog(LOG, "sem_close failed: %m");
  }
#else
                                                     
  if (sem_destroy(sem) < 0)
  {
    elog(LOG, "sem_destroy failed: %m");
  }
#endif
}

   
                                                        
   
Size
PGSemaphoreShmemSize(int maxSemas)
{
#ifdef USE_NAMED_POSIX_SEMAPHORES
                                            
  return 0;
#else
                                            
  return mul_size(maxSemas, sizeof(PGSemaphoreData));
#endif
}

   
                                                        
   
                                                                             
                                                                        
                                                                      
                                                                        
                             
   
                                                                          
                                                                          
                        
   
                                                                     
                                                                    
                                                                              
                                                                             
                                                       
                                                                       
                                                                           
                                                                         
                                                             
   
void
PGReserveSemaphores(int maxSemas, int port)
{
#ifdef USE_NAMED_POSIX_SEMAPHORES
  mySemPointers = (sem_t **)malloc(maxSemas * sizeof(sem_t *));
  if (mySemPointers == NULL)
  {
    elog(PANIC, "out of memory");
  }
#else

     
                                                                     
                                                                           
                                               
     
  sharedSemas = (PGSemaphore)ShmemAllocUnlocked(PGSemaphoreShmemSize(maxSemas));
#endif

  numSems = 0;
  maxSems = maxSemas;
  nextSemKey = port * 1000;

  on_shmem_exit(ReleaseSemaphores, 0);
}

   
                                                            
   
                                                                    
   
static void
ReleaseSemaphores(int status, Datum arg)
{
  int i;

#ifdef USE_NAMED_POSIX_SEMAPHORES
  for (i = 0; i < numSems; i++)
  {
    PosixSemaphoreKill(mySemPointers[i]);
  }
  free(mySemPointers);
#endif

#ifdef USE_UNNAMED_POSIX_SEMAPHORES
  for (i = 0; i < numSems; i++)
  {
    PosixSemaphoreKill(PG_SEM_REF(sharedSemas + i));
  }
#endif
}

   
                     
   
                                                         
   
PGSemaphore
PGSemaphoreCreate(void)
{
  PGSemaphore sema;
  sem_t *newsem;

                                                                        
  Assert(!IsUnderPostmaster);

  if (numSems >= maxSems)
  {
    elog(PANIC, "too many semaphores created");
  }

#ifdef USE_NAMED_POSIX_SEMAPHORES
  newsem = PosixSemaphoreCreate();
                                               
  mySemPointers[numSems] = newsem;
  sema = (PGSemaphore)newsem;
#else
  sema = &sharedSemas[numSems];
  newsem = PG_SEM_REF(sema);
  PosixSemaphoreCreate(newsem);
#endif

  numSems++;

  return sema;
}

   
                    
   
                                                              
   
void
PGSemaphoreReset(PGSemaphore sema)
{
     
                                                                        
                                                  
     
  for (;;)
  {
    if (sem_trywait(PG_SEM_REF(sema)) < 0)
    {
      if (errno == EAGAIN || errno == EDEADLK)
      {
        break;                       
      }
      if (errno == EINTR)
      {
        continue;                       
      }
      elog(FATAL, "sem_trywait failed: %m");
    }
  }
}

   
                   
   
                                                                      
   
void
PGSemaphoreLock(PGSemaphore sema)
{
  int errStatus;

                                                                     
  do
  {
    errStatus = sem_wait(PG_SEM_REF(sema));
  } while (errStatus < 0 && errno == EINTR);

  if (errStatus < 0)
  {
    elog(FATAL, "sem_wait failed: %m");
  }
}

   
                     
   
                                        
   
void
PGSemaphoreUnlock(PGSemaphore sema)
{
  int errStatus;

     
                                                                           
                                                                          
                                                                           
                             
     
  do
  {
    errStatus = sem_post(PG_SEM_REF(sema));
  } while (errStatus < 0 && errno == EINTR);

  if (errStatus < 0)
  {
    elog(FATAL, "sem_post failed: %m");
  }
}

   
                      
   
                                                           
   
bool
PGSemaphoreTryLock(PGSemaphore sema)
{
  int errStatus;

     
                                                                           
                                                                          
                                       
     
  do
  {
    errStatus = sem_trywait(PG_SEM_REF(sema));
  } while (errStatus < 0 && errno == EINTR);

  if (errStatus < 0)
  {
    if (errno == EAGAIN || errno == EDEADLK)
    {
      return false;                        
    }
                                  
    elog(FATAL, "sem_trywait failed: %m");
  }

  return true;
}
