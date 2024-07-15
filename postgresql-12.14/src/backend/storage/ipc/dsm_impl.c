                                                                            
   
              
                                           
   
                                                                        
                                                                          
                                                                          
                                                                        
                                                                        
                                                                     
                       
   
                                                                         
                                                                    
                                                                     
                                                                          
                                                                        
                                                                          
                                                                   
                                                                      
                                                                     
                                                                       
                                                     
   
                                                                         
                                                                         
                                                                    
                                                                        
                                                                        
                                                                   
                                                                       
                                                                          
                                                                        
                         
   
                                                     
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   

#include "postgres.h"
#include "miscadmin.h"

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#ifndef WIN32
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

#include "common/file_perm.h"
#include "libpq/pqsignal.h"
#include "pgstat.h"
#include "portability/mem.h"
#include "storage/dsm_impl.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "postmaster/postmaster.h"

#ifdef USE_DSM_POSIX
static bool
dsm_impl_posix(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel);
static int
dsm_impl_posix_resize(int fd, off_t size);
#endif
#ifdef USE_DSM_SYSV
static bool
dsm_impl_sysv(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel);
#endif
#ifdef USE_DSM_WINDOWS
static bool
dsm_impl_windows(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel);
#endif
#ifdef USE_DSM_MMAP
static bool
dsm_impl_mmap(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel);
#endif
static int
errcode_for_dynamic_shared_memory(void);

const struct config_enum_entry dynamic_shared_memory_options[] = {
#ifdef USE_DSM_POSIX
    {"posix", DSM_IMPL_POSIX, false},
#endif
#ifdef USE_DSM_SYSV
    {"sysv", DSM_IMPL_SYSV, false},
#endif
#ifdef USE_DSM_WINDOWS
    {"windows", DSM_IMPL_WINDOWS, false},
#endif
#ifdef USE_DSM_MMAP
    {"mmap", DSM_IMPL_MMAP, false},
#endif
    {NULL, 0, false}};

                              
int dynamic_shared_memory_type;

                                                 
#define ZBUFFER_SIZE 8192

#define SEGMENT_NAME_PREFIX "Global/PostgreSQL"

         
                                                                           
                                                                       
                                                   
   
                                                                       
           
   
                                                                         
   
                                      
   
                                                                     
            
   
              
                                       
                                                                        
                                             
                                                                        
                                                                            
                                                                             
                                                                          
                            
                                                                         
                                                  
                                                                           
                                    
                                          
   
                                                                             
                                                                           
                                                                       
                          
        
   
bool
dsm_impl_op(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel)
{
  Assert(op == DSM_OP_CREATE || request_size == 0);
  Assert((op != DSM_OP_CREATE && op != DSM_OP_ATTACH) || (*mapped_address == NULL && *mapped_size == 0));

  switch (dynamic_shared_memory_type)
  {
#ifdef USE_DSM_POSIX
  case DSM_IMPL_POSIX:
    return dsm_impl_posix(op, handle, request_size, impl_private, mapped_address, mapped_size, elevel);
#endif
#ifdef USE_DSM_SYSV
  case DSM_IMPL_SYSV:
    return dsm_impl_sysv(op, handle, request_size, impl_private, mapped_address, mapped_size, elevel);
#endif
#ifdef USE_DSM_WINDOWS
  case DSM_IMPL_WINDOWS:
    return dsm_impl_windows(op, handle, request_size, impl_private, mapped_address, mapped_size, elevel);
#endif
#ifdef USE_DSM_MMAP
  case DSM_IMPL_MMAP:
    return dsm_impl_mmap(op, handle, request_size, impl_private, mapped_address, mapped_size, elevel);
#endif
  default:
    elog(ERROR, "unexpected dynamic shared memory type: %d", dynamic_shared_memory_type);
    return false;
  }
}

#ifdef USE_DSM_POSIX
   
                                                               
   
                                                                          
                                                                     
                                                                       
   
                                                                       
                                                                            
                                                                         
                                                                          
                                                                        
                                             
   
static bool
dsm_impl_posix(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel)
{
  char name[64];
  int flags;
  int fd;
  char *address;

  snprintf(name, 64, "/PostgreSQL.%u", handle);

                              
  if (op == DSM_OP_DETACH || op == DSM_OP_DESTROY)
  {
    if (*mapped_address != NULL && munmap(*mapped_address, *mapped_size) != 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not unmap shared memory segment \"%s\": %m", name)));
      return false;
    }
    *mapped_address = NULL;
    *mapped_size = 0;
    if (op == DSM_OP_DESTROY && shm_unlink(name) != 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not remove shared memory segment \"%s\": %m", name)));
      return false;
    }
    return true;
  }

     
                                                            
     
                                                                         
                                                                          
                                                                       
                
     
  flags = O_RDWR | (op == DSM_OP_CREATE ? O_CREAT | O_EXCL : 0);
  if ((fd = shm_open(name, flags, PG_FILE_MODE_OWNER)) == -1)
  {
    if (op == DSM_OP_ATTACH || errno != EEXIST)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not open shared memory segment \"%s\": %m", name)));
    }
    return false;
  }

     
                                                                           
                                                                
     
  if (op == DSM_OP_ATTACH)
  {
    struct stat st;

    if (fstat(fd, &st) != 0)
    {
      int save_errno;

                                              
      save_errno = errno;
      close(fd);
      errno = save_errno;

      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not stat shared memory segment \"%s\": %m", name)));
      return false;
    }
    request_size = st.st_size;
  }
  else if (dsm_impl_posix_resize(fd, request_size) != 0)
  {
    int save_errno;

                                            
    save_errno = errno;
    close(fd);
    shm_unlink(name);
    errno = save_errno;

    ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not resize shared memory segment \"%s\" to %zu bytes: %m", name, request_size)));
    return false;
  }

               
  address = mmap(NULL, request_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HASSEMAPHORE | MAP_NOSYNC, fd, 0);
  if (address == MAP_FAILED)
  {
    int save_errno;

                                            
    save_errno = errno;
    close(fd);
    if (op == DSM_OP_CREATE)
    {
      shm_unlink(name);
    }
    errno = save_errno;

    ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not map shared memory segment \"%s\": %m", name)));
    return false;
  }
  *mapped_address = address;
  *mapped_size = request_size;
  close(fd);

  return true;
}

   
                                                                              
                                                                              
                                                     
   
                                                                              
   
static int
dsm_impl_posix_resize(int fd, off_t size)
{
  int rc;
  int save_errno;
  sigset_t save_sigmask;

     
                                                                             
                                                                       
                                                                              
                                                     
     
  if (IsUnderPostmaster)
  {
    sigprocmask(SIG_SETMASK, &BlockSig, &save_sigmask);
  }

                                                            
  do
  {
    rc = ftruncate(fd, size);
  } while (rc < 0 && errno == EINTR);

     
                                                                             
                                                                           
                                                                           
                                                                          
                                                                         
                   
     
#if defined(HAVE_POSIX_FALLOCATE) && defined(__linux__)
  if (rc == 0)
  {
       
                                                                      
                                                                          
                                              
       
    do
    {
      rc = posix_fallocate(fd, 0, size);
    } while (rc == EINTR);

       
                                                                         
                                                                          
                                                                        
       
    errno = rc;
  }
#endif                                        

  if (IsUnderPostmaster)
  {
    save_errno = errno;
    sigprocmask(SIG_SETMASK, &save_sigmask, NULL);
    errno = save_errno;
  }

  return rc;
}

#endif                    

#ifdef USE_DSM_SYSV
   
                                                                  
   
                                                                            
                                                                         
                                                                    
                                                       
   
static bool
dsm_impl_sysv(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel)
{
  key_t key;
  int ident;
  char *address;
  char name[64];
  int *ident_cache;

     
                                                                             
                                                                             
               
     
  snprintf(name, 64, "%u", handle);

     
                                                                           
                                                                             
                                                                             
                                                                            
                                                                          
                                                                      
                                                                             
                                                                       
                                                                          
                                                                             
             
     
                                                                            
               
     
  key = (key_t)handle;
  if (key < 1)                                                 
  {
    key = -key;
  }

     
                                                                           
                                                                             
                                                                            
                                                                     
                                                        
     
  if (key == IPC_PRIVATE)
  {
    if (op != DSM_OP_CREATE)
    {
      elog(DEBUG4, "System V shared memory key may not be IPC_PRIVATE");
    }
    errno = EEXIST;
    return false;
  }

     
                                                                            
                                                                            
                                                                  
     
  if (*impl_private != NULL)
  {
    ident_cache = *impl_private;
    ident = *ident_cache;
  }
  else
  {
    int flags = IPCProtection;
    size_t segsize;

       
                                                                           
                                                     
       
    ident_cache = MemoryContextAlloc(TopMemoryContext, sizeof(int));

       
                                                                       
                                                                     
                                          
       
    segsize = 0;

    if (op == DSM_OP_CREATE)
    {
      flags |= IPC_CREAT | IPC_EXCL;
      segsize = request_size;
    }

    if ((ident = shmget(key, segsize, flags)) == -1)
    {
      if (op == DSM_OP_ATTACH || errno != EEXIST)
      {
        int save_errno = errno;

        pfree(ident_cache);
        errno = save_errno;
        ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not get shared memory segment: %m")));
      }
      return false;
    }

    *ident_cache = ident;
    *impl_private = ident_cache;
  }

                              
  if (op == DSM_OP_DETACH || op == DSM_OP_DESTROY)
  {
    pfree(ident_cache);
    *impl_private = NULL;
    if (*mapped_address != NULL && shmdt(*mapped_address) != 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not unmap shared memory segment \"%s\": %m", name)));
      return false;
    }
    *mapped_address = NULL;
    *mapped_size = 0;
    if (op == DSM_OP_DESTROY && shmctl(ident, IPC_RMID, NULL) < 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not remove shared memory segment \"%s\": %m", name)));
      return false;
    }
    return true;
  }

                                                                          
  if (op == DSM_OP_ATTACH)
  {
    struct shmid_ds shm;

    if (shmctl(ident, IPC_STAT, &shm) != 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not stat shared memory segment \"%s\": %m", name)));
      return false;
    }
    request_size = shm.shm_segsz;
  }

               
  address = shmat(ident, NULL, PG_SHMAT_FLAGS);
  if (address == (void *)-1)
  {
    int save_errno;

                                            
    save_errno = errno;
    if (op == DSM_OP_CREATE)
    {
      shmctl(ident, IPC_RMID, NULL);
    }
    errno = save_errno;

    ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not map shared memory segment \"%s\": %m", name)));
    return false;
  }
  *mapped_address = address;
  *mapped_size = request_size;

  return true;
}
#endif

#ifdef USE_DSM_WINDOWS
   
                                                                 
   
                                                                   
                                                                      
                                                                   
                                                                              
                                          
   
                                                                             
                                                                                
                                                    
   
static bool
dsm_impl_windows(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel)
{
  char *address;
  HANDLE hmap;
  char name[64];
  MEMORY_BASIC_INFORMATION info;

     
                                                                           
                                                                           
                                                                           
                                                                            
                                                                            
                                      
     
  snprintf(name, 64, "%s.%u", SEGMENT_NAME_PREFIX, handle);

     
                                                                             
                                                                    
     
  if (op == DSM_OP_DETACH || op == DSM_OP_DESTROY)
  {
    if (*mapped_address != NULL && UnmapViewOfFile(*mapped_address) == 0)
    {
      _dosmaperr(GetLastError());
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not unmap shared memory segment \"%s\": %m", name)));
      return false;
    }
    if (*impl_private != NULL && CloseHandle(*impl_private) == 0)
    {
      _dosmaperr(GetLastError());
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not remove shared memory segment \"%s\": %m", name)));
      return false;
    }

    *impl_private = NULL;
    *mapped_address = NULL;
    *mapped_size = 0;
    return true;
  }

                                                              
  if (op == DSM_OP_CREATE)
  {
    DWORD size_high;
    DWORD size_low;
    DWORD errcode;

                                                        
#ifdef _WIN64
    size_high = request_size >> 32;
#else
    size_high = 0;
#endif
    size_low = (DWORD)request_size;

                                                                     
    SetLastError(0);

    hmap = CreateFileMapping(INVALID_HANDLE_VALUE,                       
        NULL,                                                                  
        PAGE_READWRITE,                                                      
        size_high,                                                            
        size_low,                                                             
        name);

    errcode = GetLastError();
    if (errcode == ERROR_ALREADY_EXISTS || errcode == ERROR_ACCESS_DENIED)
    {
         
                                                                       
                                                                
                                                                      
                                                                   
                                                      
         
      if (hmap)
      {
        CloseHandle(hmap);
      }
      return false;
    }

    if (!hmap)
    {
      _dosmaperr(errcode);
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not create shared memory segment \"%s\": %m", name)));
      return false;
    }
  }
  else
  {
    hmap = OpenFileMapping(FILE_MAP_WRITE | FILE_MAP_READ, FALSE,                              
        name);                                                                                
    if (!hmap)
    {
      _dosmaperr(GetLastError());
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not open shared memory segment \"%s\": %m", name)));
      return false;
    }
  }

               
  address = MapViewOfFile(hmap, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);
  if (!address)
  {
    int save_errno;

    _dosmaperr(GetLastError());
                                            
    save_errno = errno;
    CloseHandle(hmap);
    errno = save_errno;

    ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not map shared memory segment \"%s\": %m", name)));
    return false;
  }

     
                                                                             
                                                                           
                                                                
                                      
     
  if (VirtualQuery(address, &info, sizeof(info)) == 0)
  {
    int save_errno;

    _dosmaperr(GetLastError());
                                            
    save_errno = errno;
    UnmapViewOfFile(address);
    CloseHandle(hmap);
    errno = save_errno;

    ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not stat shared memory segment \"%s\": %m", name)));
    return false;
  }

  *mapped_address = address;
  *mapped_size = info.RegionSize;
  *impl_private = hmap;

  return true;
}
#endif

#ifdef USE_DSM_MMAP
   
                                                                    
   
                                                                        
                                                                         
                                                                
                                                                        
                                                                        
                                                               
   
static bool
dsm_impl_mmap(dsm_op op, dsm_handle handle, Size request_size, void **impl_private, void **mapped_address, Size *mapped_size, int elevel)
{
  char name[64];
  int flags;
  int fd;
  char *address;

  snprintf(name, 64, PG_DYNSHMEM_DIR "/" PG_DYNSHMEM_MMAP_FILE_PREFIX "%u", handle);

                              
  if (op == DSM_OP_DETACH || op == DSM_OP_DESTROY)
  {
    if (*mapped_address != NULL && munmap(*mapped_address, *mapped_size) != 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not unmap shared memory segment \"%s\": %m", name)));
      return false;
    }
    *mapped_address = NULL;
    *mapped_size = 0;
    if (op == DSM_OP_DESTROY && unlink(name) != 0)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not remove shared memory segment \"%s\": %m", name)));
      return false;
    }
    return true;
  }

                                                              
  flags = O_RDWR | (op == DSM_OP_CREATE ? O_CREAT | O_EXCL : 0);
  if ((fd = OpenTransientFile(name, flags)) == -1)
  {
    if (op == DSM_OP_ATTACH || errno != EEXIST)
    {
      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not open shared memory segment \"%s\": %m", name)));
    }
    return false;
  }

     
                                                                           
                                                                
     
  if (op == DSM_OP_ATTACH)
  {
    struct stat st;

    if (fstat(fd, &st) != 0)
    {
      int save_errno;

                                              
      save_errno = errno;
      CloseTransientFile(fd);
      errno = save_errno;

      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not stat shared memory segment \"%s\": %m", name)));
      return false;
    }
    request_size = st.st_size;
  }
  else
  {
       
                                        
       
                                                                          
                                                                        
                                        
       
    char *zbuffer = (char *)palloc0(ZBUFFER_SIZE);
    uint32 remaining = request_size;
    bool success = true;

       
                                                                          
                                                                      
                                                                          
                 
       
    while (success && remaining > 0)
    {
      Size goal = remaining;

      if (goal > ZBUFFER_SIZE)
      {
        goal = ZBUFFER_SIZE;
      }
      pgstat_report_wait_start(WAIT_EVENT_DSM_FILL_ZERO_WRITE);
      if (write(fd, zbuffer, goal) == goal)
      {
        remaining -= goal;
      }
      else
      {
        success = false;
      }
      pgstat_report_wait_end();
    }

    if (!success)
    {
      int save_errno;

                                              
      save_errno = errno;
      CloseTransientFile(fd);
      unlink(name);
      errno = save_errno ? save_errno : ENOSPC;

      ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not resize shared memory segment \"%s\" to %zu bytes: %m", name, request_size)));
      return false;
    }
  }

               
  address = mmap(NULL, request_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HASSEMAPHORE | MAP_NOSYNC, fd, 0);
  if (address == MAP_FAILED)
  {
    int save_errno;

                                            
    save_errno = errno;
    CloseTransientFile(fd);
    if (op == DSM_OP_CREATE)
    {
      unlink(name);
    }
    errno = save_errno;

    ereport(elevel, (errcode_for_dynamic_shared_memory(), errmsg("could not map shared memory segment \"%s\": %m", name)));
    return false;
  }
  *mapped_address = address;
  *mapped_size = request_size;

  if (CloseTransientFile(fd))
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not close shared memory segment \"%s\": %m", name)));
    return false;
  }

  return true;
}
#endif

   
                                                                               
                                                      
   
                                                                              
                                                                            
                                                                           
                                                                          
   
void
dsm_impl_pin_segment(dsm_handle handle, void *impl_private, void **impl_private_pm_handle)
{
  switch (dynamic_shared_memory_type)
  {
#ifdef USE_DSM_WINDOWS
  case DSM_IMPL_WINDOWS:
  {
    HANDLE hmap;

    if (!DuplicateHandle(GetCurrentProcess(), impl_private, PostmasterHandle, &hmap, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
      char name[64];

      snprintf(name, 64, "%s.%u", SEGMENT_NAME_PREFIX, handle);
      _dosmaperr(GetLastError());
      ereport(ERROR, (errcode_for_dynamic_shared_memory(), errmsg("could not duplicate handle for \"%s\": %m", name)));
    }

       
                                                           
                                                                 
                                                               
                                                                   
                                                         
       
    *impl_private_pm_handle = hmap;
    break;
  }
#endif
  default:
    break;
  }
}

   
                                                                               
                                                                           
                          
   
                                                                            
                                                                   
                               
   
void
dsm_impl_unpin_segment(dsm_handle handle, void **impl_private)
{
  switch (dynamic_shared_memory_type)
  {
#ifdef USE_DSM_WINDOWS
  case DSM_IMPL_WINDOWS:
  {
    if (*impl_private && !DuplicateHandle(PostmasterHandle, *impl_private, NULL, NULL, 0, FALSE, DUPLICATE_CLOSE_SOURCE))
    {
      char name[64];

      snprintf(name, 64, "%s.%u", SEGMENT_NAME_PREFIX, handle);
      _dosmaperr(GetLastError());
      ereport(ERROR, (errcode_for_dynamic_shared_memory(), errmsg("could not duplicate handle for \"%s\": %m", name)));
    }

    *impl_private = NULL;
    break;
  }
#endif
  default:
    break;
  }
}

static int
errcode_for_dynamic_shared_memory(void)
{
  if (errno == EFBIG || errno == ENOMEM)
  {
    return errcode(ERRCODE_OUT_OF_MEMORY);
  }
  else
  {
    return errcode_for_file_access();
  }
}
