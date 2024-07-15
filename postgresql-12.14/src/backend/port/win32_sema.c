                                                                            
   
                
                                                  
   
                                                                         
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/pg_sema.h"

static HANDLE *mySemSet;                                       
static int numSems;                                               
static int maxSems;                                             

static void
ReleaseSemaphores(int code, Datum arg);

   
                                                        
   
Size
PGSemaphoreShmemSize(int maxSemas)
{
                                          
  return 0;
}

   
                                                        
   
                                                                     
                                                                         
                                                                         
                                                                       
                  
   
void
PGReserveSemaphores(int maxSemas, int port)
{
  mySemSet = (HANDLE *)malloc(maxSemas * sizeof(HANDLE));
  if (mySemSet == NULL)
  {
    elog(PANIC, "out of memory");
  }
  numSems = 0;
  maxSems = maxSemas;

  on_shmem_exit(ReleaseSemaphores, 0);
}

   
                                                            
   
                                                                    
   
static void
ReleaseSemaphores(int code, Datum arg)
{
  int i;

  for (i = 0; i < numSems; i++)
  {
    CloseHandle(mySemSet[i]);
  }
  free(mySemSet);
}

   
                     
   
                                                         
   
PGSemaphore
PGSemaphoreCreate(void)
{
  HANDLE cur_handle;
  SECURITY_ATTRIBUTES sec_attrs;

                                                                        
  Assert(!IsUnderPostmaster);

  if (numSems >= maxSems)
  {
    elog(PANIC, "too many semaphores created");
  }

  ZeroMemory(&sec_attrs, sizeof(sec_attrs));
  sec_attrs.nLength = sizeof(sec_attrs);
  sec_attrs.lpSecurityDescriptor = NULL;
  sec_attrs.bInheritHandle = TRUE;

                                       
  cur_handle = CreateSemaphore(&sec_attrs, 1, 32767, NULL);
  if (cur_handle)
  {
                           
    mySemSet[numSems++] = cur_handle;
  }
  else
  {
    ereport(PANIC, (errmsg("could not create semaphore: error code %lu", GetLastError())));
  }

  return (PGSemaphore)cur_handle;
}

   
                    
   
                                                              
   
void
PGSemaphoreReset(PGSemaphore sema)
{
     
                                                                        
                                                  
     
  while (PGSemaphoreTryLock(sema))
              ;
}

   
                   
   
                                                                       
                                               
   
void
PGSemaphoreLock(PGSemaphore sema)
{
  HANDLE wh[2];
  bool done = false;

     
                                                                          
                                                                       
                                   
     
  wh[0] = pgwin32_signal_event;
  wh[1] = sema;

     
                                                                          
                                                                           
                                                                         
                                      
     
  while (!done)
  {
    DWORD rc;

    CHECK_FOR_INTERRUPTS();

    rc = WaitForMultipleObjectsEx(2, wh, FALSE, INFINITE, TRUE);
    switch (rc)
    {
    case WAIT_OBJECT_0:
                                                             
      pgwin32_dispatch_queued_signals();
      break;
    case WAIT_OBJECT_0 + 1:
                      
      done = true;
      break;
    case WAIT_IO_COMPLETION:

         
                                                           
                                                                   
                                                                   
                                                                   
                              
         
      break;
    case WAIT_FAILED:
      ereport(FATAL, (errmsg("could not lock semaphore: error code %lu", GetLastError())));
      break;
    default:
      elog(FATAL, "unexpected return code from WaitForMultipleObjectsEx(): %lu", rc);
      break;
    }
  }
}

   
                     
   
                                        
   
void
PGSemaphoreUnlock(PGSemaphore sema)
{
  if (!ReleaseSemaphore(sema, 1, NULL))
  {
    ereport(FATAL, (errmsg("could not unlock semaphore: error code %lu", GetLastError())));
  }
}

   
                      
   
                                                           
   
bool
PGSemaphoreTryLock(PGSemaphore sema)
{
  DWORD ret;

  ret = WaitForSingleObject(sema, 0);

  if (ret == WAIT_OBJECT_0)
  {
                    
    return true;
  }
  else if (ret == WAIT_TIMEOUT)
  {
                      
    errno = EAGAIN;
    return false;
  }

                                   
  ereport(FATAL, (errmsg("could not try-lock semaphore: error code %lu", GetLastError())));

                           
  return false;
}
