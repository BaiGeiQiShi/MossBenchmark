                                                                            
   
                
                                                   
   
                                                                       
                                                                      
                                                                     
                                                                       
                                                                         
   
                                                                         
                                                                        
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

#include "miscadmin.h"
#include "portability/mem.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/pg_shmem.h"
#include "utils/guc.h"
#include "utils/pidfile.h"

   
                                                                          
                                                                     
                                                                          
                                                                       
                                                                        
                                                                          
                                                                               
   
                                                                        
                                                                       
                                                                          
                                                                              
                                           
   
                                                                            
                                                                               
                                                                               
                                                                       
                                                                           
                                                                               
   
                                                                               
                                                                               
                                             
   

typedef key_t IpcMemoryKey;                                            
typedef int IpcMemoryId;                                                

   
                                                                   
   
                                                                             
                                                                              
                                                                              
                                                                              
                                                                          
                                                                               
                                                                 
   
typedef enum
{
  SHMSTATE_ANALYSIS_FAILURE,                                           
  SHMSTATE_ATTACHED,                                                      
  SHMSTATE_ENOENT,                                      
  SHMSTATE_FOREIGN,                                                    
  SHMSTATE_UNATTACHED                                                    
} IpcMemoryState;

unsigned long UsedShmemSegID = 0;
void *UsedShmemSegAddr = NULL;

static Size AnonymousShmemSize;
static void *AnonymousShmem = NULL;

static void *
InternalIpcMemoryCreate(IpcMemoryKey memKey, Size size);
static void
IpcMemoryDetach(int status, Datum shmaddr);
static void
IpcMemoryDelete(int status, Datum shmId);
static IpcMemoryState
PGSharedMemoryAttach(IpcMemoryId shmId, void *attachAt, PGShmemHeader **addr);

   
                                         
   
                                                                         
                                                                             
                                                                              
                                                                         
                                                    
   
                                                                              
                                                                             
   
static void *
InternalIpcMemoryCreate(IpcMemoryKey memKey, Size size)
{
  IpcMemoryId shmid;
  void *requestedAddress = NULL;
  void *memAddress;

     
                                                                            
                                                                             
                                                                          
                                                                         
                                                                           
                                                                           
                                                                            
                                                          
     
#ifdef EXEC_BACKEND
  {
    char *pg_shmem_addr = getenv("PG_SHMEM_ADDR");

    if (pg_shmem_addr)
    {
      requestedAddress = (void *)strtoul(pg_shmem_addr, NULL, 0);
    }
    else
    {
#if defined(__darwin__) && SIZEOF_VOID_P == 8
         
                                                                         
                                            
         
      requestedAddress = (void *)0x80000000000;
#endif
    }
  }
#endif

  shmid = shmget(memKey, size, IPC_CREAT | IPC_EXCL | IPCProtection);

  if (shmid < 0)
  {
    int shmget_errno = errno;

       
                                                                          
                                                                         
                                                                       
                                                                       
       
    if (shmget_errno == EEXIST || shmget_errno == EACCES
#ifdef EIDRM
        || shmget_errno == EIDRM
#endif
    )
      return NULL;

       
                                                                           
                                                                          
                                                                   
                                                                        
                                                                        
                                                                          
                                                        
       
    if (shmget_errno == EINVAL)
    {
      shmid = shmget(memKey, 0, IPC_CREAT | IPC_EXCL | IPCProtection);

      if (shmid < 0)
      {
                                                             
        if (errno == EEXIST || errno == EACCES
#ifdef EIDRM
            || errno == EIDRM
#endif
        )
          return NULL;
                                                                  
      }
      else
      {
           
                                                                  
                                                                       
                                                                      
                               
           
        if (shmctl(shmid, IPC_RMID, NULL) < 0)
        {
          elog(LOG, "shmctl(%d, %d, 0) failed: %m", (int)shmid, IPC_RMID);
        }
      }
    }

       
                                
       
                                                                           
                                                                         
                                                                          
                                                                        
                                 
       
    errno = shmget_errno;
    ereport(FATAL, (errmsg("could not create shared memory segment: %m"), errdetail("Failed system call was shmget(key=%lu, size=%zu, 0%o).", (unsigned long)memKey, size, IPC_CREAT | IPC_EXCL | IPCProtection),
                       (shmget_errno == EINVAL) ? errhint("This error usually means that PostgreSQL's request for a shared memory "
                                                          "segment exceeded your kernel's SHMMAX parameter, or possibly that "
                                                          "it is less than "
                                                          "your kernel's SHMMIN parameter.\n"
                                                          "The PostgreSQL documentation contains more information about shared "
                                                          "memory configuration.")
                                                : 0,
                       (shmget_errno == ENOMEM) ? errhint("This error usually means that PostgreSQL's request for a shared "
                                                          "memory segment exceeded your kernel's SHMALL parameter.  You might need "
                                                          "to reconfigure the kernel with larger SHMALL.\n"
                                                          "The PostgreSQL documentation contains more information about shared "
                                                          "memory configuration.")
                                                : 0,
                       (shmget_errno == ENOSPC) ? errhint("This error does *not* mean that you have run out of disk space.  "
                                                          "It occurs either if all available shared memory IDs have been taken, "
                                                          "in which case you need to raise the SHMMNI parameter in your kernel, "
                                                          "or because the system's overall limit for shared memory has been "
                                                          "reached.\n"
                                                          "The PostgreSQL documentation contains more information about shared "
                                                          "memory configuration.")
                                                : 0));
  }

                                                          
  on_shmem_exit(IpcMemoryDelete, Int32GetDatum(shmid));

                                                   
  memAddress = shmat(shmid, requestedAddress, PG_SHMAT_FLAGS);

  if (memAddress == (void *)-1)
  {
    elog(FATAL, "shmat(id=%d, addr=%p, flags=0x%x) failed: %m", shmid, requestedAddress, PG_SHMAT_FLAGS);
  }

                                                                      
  on_shmem_exit(IpcMemoryDetach, PointerGetDatum(memAddress));

     
                                                                          
                                                                         
                                      
     
  {
    char line[64];

    sprintf(line, "%9lu %9lu", (unsigned long)memKey, (unsigned long)shmid);
    AddToDataDirLockFile(LOCK_FILE_LINE_SHMEM_KEY, line);
  }

  return memAddress;
}

                                                                              
                                                                       
                                            
                                                                       
                                                                              
static void
IpcMemoryDetach(int status, Datum shmaddr)
{
                                            
  if (shmdt(DatumGetPointer(shmaddr)) < 0)
  {
    elog(LOG, "shmdt(%p) failed: %m", DatumGetPointer(shmaddr));
  }
}

                                                                              
                                                                      
                                                                       
                                                                              
static void
IpcMemoryDelete(int status, Datum shmId)
{
  if (shmctl(DatumGetInt32(shmId), IPC_RMID, NULL) < 0)
  {
    elog(LOG, "shmctl(%d, %d, 0) failed: %m", DatumGetInt32(shmId), IPC_RMID);
  }
}

   
                         
   
                                                                     
   
                                                                             
                                                                          
                                                                           
                                                                            
                                            
   
bool
PGSharedMemoryIsInUse(unsigned long id1, unsigned long id2)
{
  PGShmemHeader *memAddress;
  IpcMemoryState state;

  state = PGSharedMemoryAttach((IpcMemoryId)id2, NULL, &memAddress);
  if (memAddress && shmdt(memAddress) < 0)
  {
    elog(LOG, "shmdt(%p) failed: %m", memAddress);
  }
  switch (state)
  {
  case SHMSTATE_ENOENT:
  case SHMSTATE_FOREIGN:
  case SHMSTATE_UNATTACHED:
    return false;
  case SHMSTATE_ANALYSIS_FAILURE:
  case SHMSTATE_ATTACHED:
    return true;
  }
  return true;
}

   
                                                                    
   
                                                                        
                                                                
   
                                                                               
   
static IpcMemoryState
PGSharedMemoryAttach(IpcMemoryId shmId, void *attachAt, PGShmemHeader **addr)
{
  struct shmid_ds shmStat;
  struct stat statbuf;
  PGShmemHeader *hdr;

  *addr = NULL;

     
                                                                        
     
  if (shmctl(shmId, IPC_STAT, &shmStat) < 0)
  {
       
                                                                      
                                                                         
               
       
    if (errno == EINVAL)
    {
      return SHMSTATE_ENOENT;
    }

       
                                                                          
                                                                        
                            
       
    if (errno == EACCES)
    {
      return SHMSTATE_FOREIGN;
    }

       
                                                                         
                                                                        
                                                                    
                                                                       
                                              
       
#ifdef HAVE_LINUX_EIDRM_BUG
    if (errno == EIDRM)
    {
      return SHMSTATE_ENOENT;
    }
#endif

       
                                                                        
                                                                      
                                                                        
                                       
       
    return SHMSTATE_ANALYSIS_FAILURE;
  }

     
                                                                            
                                                                            
                                                                         
                                                                        
                                                                            
                                      
     
  if (stat(DataDir, &statbuf) < 0)
  {
    return SHMSTATE_ANALYSIS_FAILURE;                                  
  }

  hdr = (PGShmemHeader *)shmat(shmId, attachAt, PG_SHMAT_FLAGS);
  if (hdr == (PGShmemHeader *)-1)
  {
       
                                                                         
                                                                         
                                                                        
                             
       
                                                                    
                                                                         
                                                        
       
    if (errno == EINVAL)
    {
      return SHMSTATE_ENOENT;                          
    }
    if (errno == EACCES)
    {
      return SHMSTATE_FOREIGN;                           
    }
#ifdef HAVE_LINUX_EIDRM_BUG
    if (errno == EIDRM)
    {
      return SHMSTATE_ENOENT;                          
    }
#endif
                                     
    return SHMSTATE_ANALYSIS_FAILURE;
  }
  *addr = hdr;

  if (hdr->magic != PGShmemMagic || hdr->device != statbuf.st_dev || hdr->inode != statbuf.st_ino)
  {
       
                                                                  
                  
       
    return SHMSTATE_FOREIGN;
  }

     
                                                                             
                                                                            
                                
     
  return shmStat.shm_nattch == 0 ? SHMSTATE_UNATTACHED : SHMSTATE_ATTACHED;
}

#ifdef MAP_HUGETLB

   
                                       
   
                                                                            
                                                                            
                                                                           
                                                                       
                                                                           
                          
   
                                                                           
                                                                           
                                                                            
                                                                           
                                                                          
                                                                
   
                                                               
                                                                
   
                                                                          
                                                                           
                               
   
static void
GetHugePageSize(Size *hugepagesize, int *mmap_flags)
{
     
                                                                           
                                                                         
                                                                     
                                                                          
                                     
     
  *hugepagesize = 2 * 1024 * 1024;
  *mmap_flags = MAP_HUGETLB;

     
                                                                   
     
                                                                         
                                                                         
     
#ifdef __linux__
  {
    FILE *fp = AllocateFile("/proc/meminfo", "r");
    char buf[128];
    unsigned int sz;
    char ch;

    if (fp)
    {
      while (fgets(buf, sizeof(buf), fp))
      {
        if (sscanf(buf, "Hugepagesize: %u %c", &sz, &ch) == 2)
        {
          if (ch == 'k')
          {
            *hugepagesize = sz * (Size)1024;
            break;
          }
                                                                 
        }
      }
      FreeFile(fp);
    }
  }
#endif                
}

#endif                  

   
                                                        
   
                                                                             
                                                                             
                          
   
static void *
CreateAnonymousSegment(Size *size)
{
  Size allocsize = *size;
  void *ptr = MAP_FAILED;
  int mmap_errno = 0;

#ifndef MAP_HUGETLB
                                                             
  Assert(huge_pages != HUGE_PAGES_ON);
#else
  if (huge_pages == HUGE_PAGES_ON || huge_pages == HUGE_PAGES_TRY)
  {
       
                                                            
       
    Size hugepagesize;
    int mmap_flags;

    GetHugePageSize(&hugepagesize, &mmap_flags);

    if (allocsize % hugepagesize != 0)
    {
      allocsize += hugepagesize - (allocsize % hugepagesize);
    }

    ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, PG_MMAP_FLAGS | mmap_flags, -1, 0);
    mmap_errno = errno;
    if (huge_pages == HUGE_PAGES_TRY && ptr == MAP_FAILED)
    {
      elog(DEBUG1, "mmap(%zu) with MAP_HUGETLB failed, huge pages disabled: %m", allocsize);
    }
  }
#endif

  if (ptr == MAP_FAILED && huge_pages != HUGE_PAGES_ON)
  {
       
                                                                          
                          
       
    allocsize = *size;
    ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, PG_MMAP_FLAGS, -1, 0);
    mmap_errno = errno;
  }

  if (ptr == MAP_FAILED)
  {
    errno = mmap_errno;
    ereport(FATAL, (errmsg("could not map anonymous shared memory: %m"), (mmap_errno == ENOMEM) ? errhint("This error usually means that PostgreSQL's request "
                                                                                                          "for a shared memory segment exceeded available memory, "
                                                                                                          "swap space, or huge pages. To reduce the request size "
                                                                                                          "(currently %zu bytes), reduce PostgreSQL's shared "
                                                                                                          "memory usage, perhaps by reducing shared_buffers or "
                                                                                                          "max_connections.",
                                                                                                      *size)
                                                                                                : 0));
  }

  *size = allocsize;
  return ptr;
}

   
                                                                  
                                                                    
   
static void
AnonymousShmemDetach(int status, Datum arg)
{
                                                      
  if (AnonymousShmem != NULL)
  {
    if (munmap(AnonymousShmem, AnonymousShmemSize) < 0)
    {
      elog(LOG, "munmap(%p, %zu) failed: %m", AnonymousShmem, AnonymousShmemSize);
    }
    AnonymousShmem = NULL;
  }
}

   
                        
   
                                                                       
                                                                         
                
   
                                                                               
                                                                             
                                                                         
                          
   
                                                                         
                                           
   
PGShmemHeader *
PGSharedMemoryCreate(Size size, int port, PGShmemHeader **shim)
{
  IpcMemoryKey NextShmemSegID;
  void *memAddress;
  PGShmemHeader *hdr;
  struct stat statbuf;
  Size sysvsize;

                                                                         
#if !defined(MAP_HUGETLB)
  if (huge_pages == HUGE_PAGES_ON)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("huge pages not supported on this platform")));
  }
#endif

                                                           
  if (huge_pages == HUGE_PAGES_ON && shared_memory_type != SHMEM_TYPE_MMAP)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("huge pages not supported with the current shared_memory_type setting")));
  }

                          
  Assert(size > MAXALIGN(sizeof(PGShmemHeader)));

  if (shared_memory_type == SHMEM_TYPE_MMAP)
  {
    AnonymousShmem = CreateAnonymousSegment(&size);
    AnonymousShmemSize = size;

                                                                 
    on_shmem_exit(AnonymousShmemDetach, (Datum)0);

                                                                     
    sysvsize = sizeof(PGShmemHeader);
  }
  else
  {
    sysvsize = size;
  }

     
                                                                         
                                                                          
                                                                          
                                             
     
  NextShmemSegID = 1 + port * 1000;

  for (;;)
  {
    IpcMemoryId shmid;
    PGShmemHeader *oldhdr;
    IpcMemoryState state;

                                   
    memAddress = InternalIpcMemoryCreate(NextShmemSegID, sysvsize);
    if (memAddress)
    {
      break;                                   
    }

                                                              

       
                                                                     
                                                                          
                                                           
       
    shmid = shmget(NextShmemSegID, sizeof(PGShmemHeader), 0);
    if (shmid < 0)
    {
      oldhdr = NULL;
      state = SHMSTATE_FOREIGN;
    }
    else
    {
      state = PGSharedMemoryAttach(shmid, NULL, &oldhdr);
    }

    switch (state)
    {
    case SHMSTATE_ANALYSIS_FAILURE:
    case SHMSTATE_ATTACHED:
      ereport(FATAL, (errcode(ERRCODE_LOCK_FILE_EXISTS), errmsg("pre-existing shared memory block (key %lu, ID %lu) is still in use", (unsigned long)NextShmemSegID, (unsigned long)shmid), errhint("Terminate any old server processes associated with data directory \"%s\".", DataDir)));
      break;
    case SHMSTATE_ENOENT:

         
                                                                    
                                                                    
                                                         
         
      elog(LOG, "shared memory block (key %lu, ID %lu) deleted during startup", (unsigned long)NextShmemSegID, (unsigned long)shmid);
      break;
    case SHMSTATE_FOREIGN:
      NextShmemSegID++;
      break;
    case SHMSTATE_UNATTACHED:

         
                                                                     
                                                                     
                                                                   
                                                                    
                                                                
                                                                    
                                                                    
                                                   
         
      if (oldhdr->dsm_control != 0)
      {
        dsm_cleanup_using_control_segment(oldhdr->dsm_control);
      }
      if (shmctl(shmid, IPC_RMID, NULL) < 0)
      {
        NextShmemSegID++;
      }
      break;
    }

    if (oldhdr && shmdt(oldhdr) < 0)
    {
      elog(LOG, "shmdt(%p) failed: %m", oldhdr);
    }
  }

                               
  hdr = (PGShmemHeader *)memAddress;
  hdr->creatorPID = getpid();
  hdr->magic = PGShmemMagic;
  hdr->dsm_control = 0;

                                               
  if (stat(DataDir, &statbuf) < 0)
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not stat data directory \"%s\": %m", DataDir)));
  }
  hdr->device = statbuf.st_dev;
  hdr->inode = statbuf.st_ino;

     
                                                     
     
  hdr->totalsize = size;
  hdr->freeoffset = MAXALIGN(sizeof(PGShmemHeader));
  *shim = hdr;

                                         
  UsedShmemSegAddr = memAddress;
  UsedShmemSegID = (unsigned long)NextShmemSegID;

     
                                                                           
                                                                       
                                                                            
                                                 
     
  if (AnonymousShmem == NULL)
  {
    return hdr;
  }
  memcpy(AnonymousShmem, hdr, sizeof(PGShmemHeader));
  return (PGShmemHeader *)AnonymousShmem;
}

#ifdef EXEC_BACKEND

   
                          
   
                                                                               
                                                                          
                                                                              
                                  
   
                                                                       
                                                                            
           
   
void
PGSharedMemoryReAttach(void)
{
  IpcMemoryId shmid;
  PGShmemHeader *hdr;
  IpcMemoryState state;
  void *origUsedShmemSegAddr = UsedShmemSegAddr;

  Assert(UsedShmemSegAddr != NULL);
  Assert(IsUnderPostmaster);

#ifdef __CYGWIN__
                                                         
  PGSharedMemoryDetach();
  UsedShmemSegAddr = origUsedShmemSegAddr;
#endif

  elog(DEBUG3, "attaching to %p", UsedShmemSegAddr);
  shmid = shmget(UsedShmemSegID, sizeof(PGShmemHeader), 0);
  if (shmid < 0)
  {
    state = SHMSTATE_FOREIGN;
  }
  else
  {
    state = PGSharedMemoryAttach(shmid, UsedShmemSegAddr, &hdr);
  }
  if (state != SHMSTATE_ATTACHED)
  {
    elog(FATAL, "could not reattach to shared memory (key=%d, addr=%p): %m", (int)UsedShmemSegID, UsedShmemSegAddr);
  }
  if (hdr != origUsedShmemSegAddr)
  {
    elog(FATAL, "reattaching to shared memory returned unexpected address (got %p, expected %p)", hdr, origUsedShmemSegAddr);
  }
  dsm_set_control_handle(hdr->dsm_control);

  UsedShmemSegAddr = hdr;                         
}

   
                            
   
                                                                              
                                                                               
                                                                          
                              
   
                                                                                
                                                            
   
                                                                       
                                                                            
           
   
void
PGSharedMemoryNoReAttach(void)
{
  Assert(UsedShmemSegAddr != NULL);
  Assert(IsUnderPostmaster);

#ifdef __CYGWIN__
                                                         
  PGSharedMemoryDetach();
#endif

                                                                           
  UsedShmemSegAddr = NULL;
                                        
  UsedShmemSegID = 0;
}

#endif                   

   
                        
   
                                                                          
                                                                               
                                                                           
                                                                               
                     
   
                                                                       
                                                        
   
void
PGSharedMemoryDetach(void)
{
  if (UsedShmemSegAddr != NULL)
  {
    if ((shmdt(UsedShmemSegAddr) < 0)
#if defined(EXEC_BACKEND) && defined(__CYGWIN__)
                                             
        && shmdt(NULL) < 0
#endif
    )
      elog(LOG, "shmdt(%p) failed: %m", UsedShmemSegAddr);
    UsedShmemSegAddr = NULL;
  }

  if (AnonymousShmem != NULL)
  {
    if (munmap(AnonymousShmem, AnonymousShmemSize) < 0)
    {
      elog(LOG, "munmap(%p, %zu) failed: %m", AnonymousShmem, AnonymousShmemSize);
    }
    AnonymousShmem = NULL;
  }
}
