                                                                            
   
                 
                                                    
   
                                                                         
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "miscadmin.h"
#include "storage/dsm.h"
#include "storage/ipc.h"
#include "storage/pg_shmem.h"

   
                                                                             
                                   
                                                                               
                                                         
                                                                            
                                                                               
                                                                              
                                                                        
                                                                             
                                                                            
                                                                           
                                                                              
                                                                             
                                                               
                                                                              
                                                                               
                                                                         
   
#define PROTECTIVE_REGION_SIZE (10 * WIN32_STACK_RLIMIT)
void *ShmemProtectiveRegion = NULL;

HANDLE UsedShmemSegID = INVALID_HANDLE_VALUE;
void *UsedShmemSegAddr = NULL;
static Size UsedShmemSegSize = 0;

static bool
EnableLockPagesPrivilege(int elevel);
static void
pgwin32_SharedMemoryDelete(int status, Datum shmId);

   
                                                                               
                                                                              
                                                                                    
   
                                                                                 
                                                                                      
                                                                               
   
                                                                                   
                                                                                  
                                 
   
static char *
GetSharedMemName(void)
{
  char *retptr;
  DWORD bufsize;
  DWORD r;
  char *cp;

  bufsize = GetFullPathName(DataDir, 0, NULL, NULL);
  if (bufsize == 0)
  {
    elog(FATAL, "could not get size for full pathname of datadir %s: error code %lu", DataDir, GetLastError());
  }

  retptr = malloc(bufsize + 18);                                
  if (retptr == NULL)
  {
    elog(FATAL, "could not allocate memory for shared memory name");
  }

  strcpy(retptr, "Global\\PostgreSQL:");
  r = GetFullPathName(DataDir, bufsize, retptr + 18, NULL);
  if (r == 0 || r > bufsize)
  {
    elog(FATAL, "could not generate full pathname for datadir %s: error code %lu", DataDir, GetLastError());
  }

     
                                                                            
                                                                       
                                                                       
                                
     
  for (cp = retptr; *cp; cp++)
  {
    if (*cp == '\\')
    {
      *cp = '/';
    }
  }

  return retptr;
}

   
                         
   
                                                                     
   
                                                                             
                                                                          
                                                                           
                                                                            
                                            
   
bool
PGSharedMemoryIsInUse(unsigned long id1, unsigned long id2)
{
  char *szShareMem;
  HANDLE hmap;

  szShareMem = GetSharedMemName();

  hmap = OpenFileMapping(FILE_MAP_READ, FALSE, szShareMem);

  free(szShareMem);

  if (hmap == NULL)
  {
    return false;
  }

  CloseHandle(hmap);
  return true;
}

   
                            
   
                                                                   
   
static bool
EnableLockPagesPrivilege(int elevel)
{
  HANDLE hToken;
  TOKEN_PRIVILEGES tp;
  LUID luid;

  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
  {
    ereport(elevel, (errmsg("could not enable Lock Pages in Memory user right: error code %lu", GetLastError()), errdetail("Failed system call was %s.", "OpenProcessToken")));
    return FALSE;
  }

  if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid))
  {
    ereport(elevel, (errmsg("could not enable Lock Pages in Memory user right: error code %lu", GetLastError()), errdetail("Failed system call was %s.", "LookupPrivilegeValue")));
    CloseHandle(hToken);
    return FALSE;
  }
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = luid;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  if (!AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL))
  {
    ereport(elevel, (errmsg("could not enable Lock Pages in Memory user right: error code %lu", GetLastError()), errdetail("Failed system call was %s.", "AdjustTokenPrivileges")));
    CloseHandle(hToken);
    return FALSE;
  }

  if (GetLastError() != ERROR_SUCCESS)
  {
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
      ereport(elevel, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("could not enable Lock Pages in Memory user right"), errhint("Assign Lock Pages in Memory user right to the Windows user account which runs PostgreSQL.")));
    }
    else
    {
      ereport(elevel, (errmsg("could not enable Lock Pages in Memory user right: error code %lu", GetLastError()), errdetail("Failed system call was %s.", "AdjustTokenPrivileges")));
    }
    CloseHandle(hToken);
    return FALSE;
  }

  CloseHandle(hToken);

  return TRUE;
}

   
                        
   
                                                                       
                    
   
PGShmemHeader *
PGSharedMemoryCreate(Size size, int port, PGShmemHeader **shim)
{
  void *memAddress;
  PGShmemHeader *hdr;
  HANDLE hmap, hmap2;
  char *szShareMem;
  int i;
  DWORD size_high;
  DWORD size_low;
  SIZE_T largePageSize = 0;
  Size orig_size = size;
  DWORD flProtect = PAGE_READWRITE;

  ShmemProtectiveRegion = VirtualAlloc(NULL, PROTECTIVE_REGION_SIZE, MEM_RESERVE, PAGE_NOACCESS);
  if (ShmemProtectiveRegion == NULL)
  {
    elog(FATAL, "could not reserve memory region: error code %lu", GetLastError());
  }

                          
  Assert(size > MAXALIGN(sizeof(PGShmemHeader)));

  szShareMem = GetSharedMemName();

  UsedShmemSegAddr = NULL;

  if (huge_pages == HUGE_PAGES_ON || huge_pages == HUGE_PAGES_TRY)
  {
                                                 
    largePageSize = GetLargePageMinimum();
    if (largePageSize == 0)
    {
      ereport(huge_pages == HUGE_PAGES_ON ? FATAL : DEBUG1, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("the processor does not support large pages")));
      ereport(DEBUG1, (errmsg("disabling huge pages")));
    }
    else if (!EnableLockPagesPrivilege(huge_pages == HUGE_PAGES_ON ? FATAL : DEBUG1))
    {
      ereport(DEBUG1, (errmsg("disabling huge pages")));
    }
    else
    {
                                                                  
      flProtect = PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES;

                                         
      if (size % largePageSize != 0)
      {
        size += largePageSize - (size % largePageSize);
      }
    }
  }

retry:
#ifdef _WIN64
  size_high = size >> 32;
#else
  size_high = 0;
#endif
  size_low = (DWORD)size;

     
                                                                       
                                                                       
                                                                        
                                                              
     
  for (i = 0; i < 10; i++)
  {
       
                                                                      
               
       
    SetLastError(0);

    hmap = CreateFileMapping(INVALID_HANDLE_VALUE,                       
        NULL,                                                                  
        flProtect, size_high,                                              
        size_low,                                                          
        szShareMem);

    if (!hmap)
    {
      if (GetLastError() == ERROR_NO_SYSTEM_RESOURCES && huge_pages == HUGE_PAGES_TRY && (flProtect & SEC_LARGE_PAGES) != 0)
      {
        elog(DEBUG1,
            "CreateFileMapping(%zu) with SEC_LARGE_PAGES failed, "
            "huge pages disabled",
            size);

           
                                                                 
                                           
           
        size = orig_size;
        flProtect = PAGE_READWRITE;
        goto retry;
      }
      else
      {
        ereport(FATAL, (errmsg("could not create shared memory segment: error code %lu", GetLastError()), errdetail("Failed system call was CreateFileMapping(size=%zu, name=%s).", size, szShareMem)));
      }
    }

       
                                                                         
                                                                
       
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
      CloseHandle(hmap);                                               
                                                       
      hmap = NULL;
      Sleep(1000);
      continue;
    }
    break;
  }

     
                                                                            
                                                                             
     
  if (!hmap)
  {
    ereport(FATAL, (errmsg("pre-existing shared memory block is still in use"), errhint("Check if there are any old server processes still running, and terminate them.")));
  }

  free(szShareMem);

     
                                 
     
  if (!DuplicateHandle(GetCurrentProcess(), hmap, GetCurrentProcess(), &hmap2, 0, TRUE, DUPLICATE_SAME_ACCESS))
  {
    ereport(FATAL, (errmsg("could not create shared memory segment: error code %lu", GetLastError()), errdetail("Failed system call was DuplicateHandle.")));
  }

     
                                                                          
           
     
  if (!CloseHandle(hmap))
  {
    elog(LOG, "could not close handle to shared memory: error code %lu", GetLastError());
  }

     
                                                                           
                                                                
     
  memAddress = MapViewOfFileEx(hmap2, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0, NULL);
  if (!memAddress)
  {
    ereport(FATAL, (errmsg("could not create shared memory segment: error code %lu", GetLastError()), errdetail("Failed system call was MapViewOfFileEx.")));
  }

     
                                                                            
                                                                            
                                                                    
     
  hdr = (PGShmemHeader *)memAddress;
  hdr->creatorPID = getpid();
  hdr->magic = PGShmemMagic;

     
                                                     
     
  hdr->totalsize = size;
  hdr->freeoffset = MAXALIGN(sizeof(PGShmemHeader));
  hdr->dsm_control = 0;

                                         
  UsedShmemSegAddr = memAddress;
  UsedShmemSegSize = size;
  UsedShmemSegID = hmap2;

                                                          
  on_shmem_exit(pgwin32_SharedMemoryDelete, PointerGetDatum(hmap2));

  *shim = hdr;
  return hdr;
}

   
                          
   
                                                                               
                                                                              
                   
   
                                                                           
                                                                              
                            
   
void
PGSharedMemoryReAttach(void)
{
  PGShmemHeader *hdr;
  void *origUsedShmemSegAddr = UsedShmemSegAddr;

  Assert(ShmemProtectiveRegion != NULL);
  Assert(UsedShmemSegAddr != NULL);
  Assert(IsUnderPostmaster);

     
                                                               
     
  if (VirtualFree(ShmemProtectiveRegion, 0, MEM_RELEASE) == 0)
  {
    elog(FATAL, "failed to release reserved memory region (addr=%p): error code %lu", ShmemProtectiveRegion, GetLastError());
  }
  if (VirtualFree(UsedShmemSegAddr, 0, MEM_RELEASE) == 0)
  {
    elog(FATAL, "failed to release reserved memory region (addr=%p): error code %lu", UsedShmemSegAddr, GetLastError());
  }

  hdr = (PGShmemHeader *)MapViewOfFileEx(UsedShmemSegID, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0, UsedShmemSegAddr);
  if (!hdr)
  {
    elog(FATAL, "could not reattach to shared memory (key=%p, addr=%p): error code %lu", UsedShmemSegID, UsedShmemSegAddr, GetLastError());
  }
  if (hdr != origUsedShmemSegAddr)
  {
    elog(FATAL, "reattaching to shared memory returned unexpected address (got %p, expected %p)", hdr, origUsedShmemSegAddr);
  }
  if (hdr->magic != PGShmemMagic)
  {
    elog(FATAL, "reattaching to shared memory returned non-PostgreSQL memory");
  }
  dsm_set_control_handle(hdr->dsm_control);

  UsedShmemSegAddr = hdr;                         
}

   
                            
   
                                                                              
                                                                               
                                             
   
                                                                                
                                                            
   
                                                                           
                                                                              
                            
   
void
PGSharedMemoryNoReAttach(void)
{
  Assert(ShmemProtectiveRegion != NULL);
  Assert(UsedShmemSegAddr != NULL);
  Assert(IsUnderPostmaster);

     
                                                                            
                                                                         
     
  UsedShmemSegAddr = NULL;

     
                                                                           
                                                                         
                                                                         
     
  PGSharedMemoryDetach();
}

   
                        
   
                                                                          
                                                                               
                                                                           
                                                                               
                     
   
                                                                           
                               
   
void
PGSharedMemoryDetach(void)
{
     
                                                                          
                                 
     
  if (ShmemProtectiveRegion != NULL)
  {
    if (VirtualFree(ShmemProtectiveRegion, 0, MEM_RELEASE) == 0)
    {
      elog(LOG, "failed to release reserved memory region (addr=%p): error code %lu", ShmemProtectiveRegion, GetLastError());
    }

    ShmemProtectiveRegion = NULL;
  }

                                      
  if (UsedShmemSegAddr != NULL)
  {
    if (!UnmapViewOfFile(UsedShmemSegAddr))
    {
      elog(LOG, "could not unmap view of shared memory: error code %lu", GetLastError());
    }

    UsedShmemSegAddr = NULL;
  }

                                                  
  if (UsedShmemSegID != INVALID_HANDLE_VALUE)
  {
    if (!CloseHandle(UsedShmemSegID))
    {
      elog(LOG, "could not close handle to shared memory: error code %lu", GetLastError());
    }

    UsedShmemSegID = INVALID_HANDLE_VALUE;
  }
}

   
                              
   
                                                    
                                                                    
   
static void
pgwin32_SharedMemoryDelete(int status, Datum shmId)
{
  Assert(DatumGetPointer(shmId) == UsedShmemSegID);
  PGSharedMemoryDetach();
}

   
                                             
   
                                                                            
                                                                           
                        
   
                                                                             
                                                                         
                                                                        
                                                                             
                                                                          
                                                          
   
                                                                       
                                                                         
   
int
pgwin32_ReserveSharedMemoryRegion(HANDLE hChild)
{
  void *address;

  Assert(ShmemProtectiveRegion != NULL);
  Assert(UsedShmemSegAddr != NULL);
  Assert(UsedShmemSegSize != 0);

                             
  address = VirtualAllocEx(hChild, ShmemProtectiveRegion, PROTECTIVE_REGION_SIZE, MEM_RESERVE, PAGE_NOACCESS);
  if (address == NULL)
  {
                                                               
    elog(LOG, "could not reserve shared memory region (addr=%p) for child %p: error code %lu", ShmemProtectiveRegion, hChild, GetLastError());
    return false;
  }
  if (address != ShmemProtectiveRegion)
  {
       
                                                                        
                                                        
       
                                                              
       
    elog(LOG, "reserved shared memory region got incorrect address %p, expected %p", address, ShmemProtectiveRegion);
    return false;
  }

                        
  address = VirtualAllocEx(hChild, UsedShmemSegAddr, UsedShmemSegSize, MEM_RESERVE, PAGE_READWRITE);
  if (address == NULL)
  {
    elog(LOG, "could not reserve shared memory region (addr=%p) for child %p: error code %lu", UsedShmemSegAddr, hChild, GetLastError());
    return false;
  }
  if (address != UsedShmemSegAddr)
  {
    elog(LOG, "reserved shared memory region got incorrect address %p, expected %p", address, UsedShmemSegAddr);
    return false;
  }

  return true;
}
