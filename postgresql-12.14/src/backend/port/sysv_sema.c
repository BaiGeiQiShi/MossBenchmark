                                                                            
   
               
                                                            
   
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/file.h>
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SEM_H
#include <sys/sem.h>
#endif

#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/pg_sema.h"
#include "storage/shmem.h"

typedef struct PGSemaphoreData
{
  int semId;                                
  int semNum;                                  
} PGSemaphoreData;

#ifndef HAVE_UNION_SEMUN
union semun
{
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};
#endif

typedef key_t IpcSemaphoreKey;                                        
typedef int IpcSemaphoreId;                                            

   
                                                                          
                                                                             
                                                                         
                                                                     
   
#define SEMAS_PER_SET 16

#define IPCProtection (0600)                                 

#define PGSemaMagic 537                               

static PGSemaphore sharedSemas;                                                    
static int numSharedSemas;                                                      
static int maxSharedSemas;                                                       
static IpcSemaphoreId *mySemaSets;                                        
static int numSemaSets;                                                      
static int maxSemaSets;                                                     
static IpcSemaphoreKey nextSemaKey;                            
static int nextSemaNumber;                                                  

static IpcSemaphoreId
InternalIpcSemaphoreCreate(IpcSemaphoreKey semKey, int numSems);
static void
IpcSemaphoreInitialize(IpcSemaphoreId semId, int semNum, int value);
static void
IpcSemaphoreKill(IpcSemaphoreId semId);
static int
IpcSemaphoreGetValue(IpcSemaphoreId semId, int semNum);
static pid_t
IpcSemaphoreGetLastPID(IpcSemaphoreId semId, int semNum);
static IpcSemaphoreId
IpcSemaphoreCreate(int numSems);
static void
ReleaseSemaphores(int status, Datum arg);

   
                              
   
                                                                 
                                                       
   
                                                                          
                                                                               
             
   
static IpcSemaphoreId
InternalIpcSemaphoreCreate(IpcSemaphoreKey semKey, int numSems)
{
  int semId;

  semId = semget(semKey, numSems, IPC_CREAT | IPC_EXCL | IPCProtection);

  if (semId < 0)
  {
    int saved_errno = errno;

       
                                                                          
                                                                        
                                                                          
                                                                 
       
    if (saved_errno == EEXIST || saved_errno == EACCES
#ifdef EIDRM
        || saved_errno == EIDRM
#endif
    )
      return -1;

       
                               
       
    ereport(FATAL, (errmsg("could not create semaphores: %m"), errdetail("Failed system call was semget(%lu, %d, 0%o).", (unsigned long)semKey, numSems, IPC_CREAT | IPC_EXCL | IPCProtection),
                       (saved_errno == ENOSPC) ? errhint("This error does *not* mean that you have run out of disk space.  "
                                                         "It occurs when either the system limit for the maximum number of "
                                                         "semaphore sets (SEMMNI), or the system wide maximum number of "
                                                         "semaphores (SEMMNS), would be exceeded.  You need to raise the "
                                                         "respective kernel parameter.  Alternatively, reduce PostgreSQL's "
                                                         "consumption of semaphores by reducing its max_connections parameter.\n"
                                                         "The PostgreSQL documentation contains more information about "
                                                         "configuring your system for PostgreSQL.")
                                               : 0));
  }

  return semId;
}

   
                                                  
   
static void
IpcSemaphoreInitialize(IpcSemaphoreId semId, int semNum, int value)
{
  union semun semun;

  semun.val = value;
  if (semctl(semId, semNum, SETVAL, semun) < 0)
  {
    int saved_errno = errno;

    ereport(FATAL, (errmsg_internal("semctl(%d, %d, SETVAL, %d) failed: %m", semId, semNum, value), (saved_errno == ERANGE) ? errhint("You possibly need to raise your kernel's SEMVMX value to be at least "
                                                                                                                                      "%d.  Look into the PostgreSQL documentation for details.",
                                                                                                                                  value)
                                                                                                                            : 0));
  }
}

   
                                                     
   
static void
IpcSemaphoreKill(IpcSemaphoreId semId)
{
  union semun semun;

  semun.val = 0;                                      

  if (semctl(semId, 0, IPC_RMID, semun) < 0)
  {
    elog(LOG, "semctl(%d, 0, IPC_RMID, ...) failed: %m", semId);
  }
}

                                                     
static int
IpcSemaphoreGetValue(IpcSemaphoreId semId, int semNum)
{
  union semun dummy;                  

  dummy.val = 0;             

  return semctl(semId, semNum, GETVAL, dummy);
}

                                                                    
static pid_t
IpcSemaphoreGetLastPID(IpcSemaphoreId semId, int semNum)
{
  union semun dummy;                  

  dummy.val = 0;             

  return semctl(semId, semNum, GETPID, dummy);
}

   
                                                                     
                                                                      
                                                                     
                                               
   
                                                                          
                                       
   
static IpcSemaphoreId
IpcSemaphoreCreate(int numSems)
{
  IpcSemaphoreId semId;
  union semun semun;
  PGSemaphoreData mysema;

                                        
  for (nextSemaKey++;; nextSemaKey++)
  {
    pid_t creatorPID;

                                         
    semId = InternalIpcSemaphoreCreate(nextSemaKey, numSems + 1);
    if (semId >= 0)
    {
      break;                        
    }

                                                                     
    semId = semget(nextSemaKey, numSems + 1, 0);
    if (semId < 0)
    {
      continue;                                       
    }
    if (IpcSemaphoreGetValue(semId, numSems) != PGSemaMagic)
    {
      continue;                                         
    }

       
                                                                         
                                     
       
    creatorPID = IpcSemaphoreGetLastPID(semId, numSems);
    if (creatorPID <= 0)
    {
      continue;                          
    }
    if (creatorPID != getpid())
    {
      if (kill(creatorPID, 0) == 0 || errno != ESRCH)
      {
        continue;                                     
      }
    }

       
                                                                          
                                                                          
                                                                         
                                                                
       
    semun.val = 0;                                      
    if (semctl(semId, 0, IPC_RMID, semun) < 0)
    {
      continue;
    }

       
                                             
       
    semId = InternalIpcSemaphoreCreate(nextSemaKey, numSems + 1);
    if (semId >= 0)
    {
      break;                        
    }

       
                                                                          
                                                                          
                 
       
  }

     
                                                                            
                                                                      
                                                                          
                                          
     
  IpcSemaphoreInitialize(semId, numSems, PGSemaMagic - 1);
  mysema.semId = semId;
  mysema.semNum = numSems;
  PGSemaphoreUnlock(&mysema);

  return semId;
}

   
                                                        
   
Size
PGSemaphoreShmemSize(int maxSemas)
{
  return mul_size(maxSemas, sizeof(PGSemaphoreData));
}

   
                                                        
   
                                                                             
                                                                        
                                                                      
                                                                        
                             
   
                                                                         
                                                                         
                        
   
                                                                        
                                                                          
                                                                             
                                                                         
                                                                            
                                                                               
                    
   
void
PGReserveSemaphores(int maxSemas, int port)
{
     
                                                                     
                                                                           
                                               
     
  sharedSemas = (PGSemaphore)ShmemAllocUnlocked(PGSemaphoreShmemSize(maxSemas));
  numSharedSemas = 0;
  maxSharedSemas = maxSemas;

  maxSemaSets = (maxSemas + SEMAS_PER_SET - 1) / SEMAS_PER_SET;
  mySemaSets = (IpcSemaphoreId *)malloc(maxSemaSets * sizeof(IpcSemaphoreId));
  if (mySemaSets == NULL)
  {
    elog(PANIC, "out of memory");
  }
  numSemaSets = 0;
  nextSemaKey = port * 1000;
  nextSemaNumber = SEMAS_PER_SET;                                       

  on_shmem_exit(ReleaseSemaphores, 0);
}

   
                                                            
   
                                                                    
   
static void
ReleaseSemaphores(int status, Datum arg)
{
  int i;

  for (i = 0; i < numSemaSets; i++)
  {
    IpcSemaphoreKill(mySemaSets[i]);
  }
  free(mySemaSets);
}

   
                     
   
                                                         
   
PGSemaphore
PGSemaphoreCreate(void)
{
  PGSemaphore sema;

                                                                        
  Assert(!IsUnderPostmaster);

  if (nextSemaNumber >= SEMAS_PER_SET)
  {
                                                
    if (numSemaSets >= maxSemaSets)
    {
      elog(PANIC, "too many semaphores created");
    }
    mySemaSets[numSemaSets] = IpcSemaphoreCreate(SEMAS_PER_SET);
    numSemaSets++;
    nextSemaNumber = 0;
  }
                                           
  if (numSharedSemas >= maxSharedSemas)
  {
    elog(PANIC, "too many semaphores created");
  }
  sema = &sharedSemas[numSharedSemas++];
                                                         
  sema->semId = mySemaSets[numSemaSets - 1];
  sema->semNum = nextSemaNumber++;
                                
  IpcSemaphoreInitialize(sema->semId, sema->semNum, 1);

  return sema;
}

   
                    
   
                                                              
   
void
PGSemaphoreReset(PGSemaphore sema)
{
  IpcSemaphoreInitialize(sema->semId, sema->semNum, 0);
}

   
                   
   
                                                                      
   
void
PGSemaphoreLock(PGSemaphore sema)
{
  int errStatus;
  struct sembuf sops;

  sops.sem_op = -1;                
  sops.sem_flg = 0;
  sops.sem_num = sema->semNum;

     
                                                                           
                                                                          
                                       
     
                                                                   
                                                                          
                   
     
  do
  {
    errStatus = semop(sema->semId, &sops, 1);
  } while (errStatus < 0 && errno == EINTR);

  if (errStatus < 0)
  {
    elog(FATAL, "semop(id=%d) failed: %m", sema->semId);
  }
}

   
                     
   
                                        
   
void
PGSemaphoreUnlock(PGSemaphore sema)
{
  int errStatus;
  struct sembuf sops;

  sops.sem_op = 1;                
  sops.sem_flg = 0;
  sops.sem_num = sema->semNum;

     
                                                                           
                                                                          
                                                                           
                             
     
  do
  {
    errStatus = semop(sema->semId, &sops, 1);
  } while (errStatus < 0 && errno == EINTR);

  if (errStatus < 0)
  {
    elog(FATAL, "semop(id=%d) failed: %m", sema->semId);
  }
}

   
                      
   
                                                           
   
bool
PGSemaphoreTryLock(PGSemaphore sema)
{
  int errStatus;
  struct sembuf sops;

  sops.sem_op = -1;                         
  sops.sem_flg = IPC_NOWAIT;                      
  sops.sem_num = sema->semNum;

     
                                                                           
                                                                          
                                       
     
  do
  {
    errStatus = semop(sema->semId, &sops, 1);
  } while (errStatus < 0 && errno == EINTR);

  if (errStatus < 0)
  {
                                                           
#ifdef EAGAIN
    if (errno == EAGAIN)
    {
      return false;                        
    }
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    if (errno == EWOULDBLOCK)
    {
      return false;                        
    }
#endif
                                  
    elog(FATAL, "semop(id=%d) failed: %m", sema->semId);
  }

  return true;
}
