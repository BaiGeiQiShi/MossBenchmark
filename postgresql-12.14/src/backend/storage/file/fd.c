                                                                            
   
        
                                   
   
                                                                         
                                                                        
   
                  
                                   
   
          
   
                                                                   
                                                                    
                                                                   
                                                                     
                                                                       
                                                                 
                                                   
   
                                                                    
                                                                  
                                                                      
                                                                 
                
   
                                                                      
                                                                       
                                                                    
                                                             
   
                      
   
                                                                          
                                                                          
                                                                            
                                                                           
                                                                           
                                                                    
   
                                                                      
                                                                      
                                                                        
                                                                               
                                                                               
                                                                             
                                             
   
                                                                       
                                                                             
                                                                               
                                                                            
                                                                        
                                                                              
                                                        
   
                                                                        
                                                                      
                                                                            
                                                                             
                                   
   
                                                                            
   

#include "postgres.h"

#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/mman.h>
#endif
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>                    
#endif

#include "miscadmin.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_tablespace.h"
#include "common/file_perm.h"
#include "pgstat.h"
#include "portability/mem.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "utils/guc.h"
#include "utils/resowner_private.h"

                                                                               
#if defined(HAVE_SYNC_FILE_RANGE)
#define PG_FLUSH_DATA_WORKS 1
#elif !defined(WIN32) && defined(MS_ASYNC)
#define PG_FLUSH_DATA_WORKS 1
#elif defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
#define PG_FLUSH_DATA_WORKS 1
#endif

   
                                                                              
                                                                          
                                                                       
                                                                       
                                                                            
                                                                      
   
                                                                          
                                                                           
                                                                         
                                                                         
                                                                      
                                  
   
#define NUM_RESERVED_FDS 10

   
                                                                              
                
   
#define FD_MINFREE 10

   
                                                                            
                                                                         
                                                                             
                                                           
   
int max_files_per_process = 1000;

   
                                                                        
                                                                               
                                                                              
                                                                             
                                                                            
                                                        
   
                                                                        
                                                                
   
int max_safe_fds = 32;                             

                                                                 
bool data_sync_retry = false;

                   

#ifdef FDDEBUG
#define DO_DB(A)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int _do_db_save_errno = errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    A;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    errno = _do_db_save_errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)
#else
#define DO_DB(A) ((void)0)
#endif

#define VFD_CLOSED (-1)

#define FileIsValid(file) ((file) > 0 && (file) < (int)SizeVfdCache && VfdCache[file].fileName != NULL)

#define FileIsNotOpen(file) (VfdCache[file].fd == VFD_CLOSED)

                                                   
#define FD_DELETE_AT_CLOSE (1 << 0)                             
#define FD_CLOSE_AT_EOXACT (1 << 1)                          
#define FD_TEMP_FILE_LIMIT (1 << 2)                                  

typedef struct vfd
{
  int fd;                                                        
  unsigned short fdstate;                               
  ResourceOwner resowner;                                   
  File nextFree;                                                     
  File lruMoreRecently;                                          
  File lruLessRecently;
  off_t fileSize;                                                
  char *fileName;                                           
                                                                         
  int fileFlags;                                               
  mode_t fileMode;                              
} Vfd;

   
                                                                  
                                                       
                                                                  
   
static Vfd *VfdCache;
static Size SizeVfdCache = 0;

   
                                                                 
   
static int nfile = 0;

   
                                                                            
            
   
static bool have_xact_temporary_files = false;

   
                                                                             
                                                                          
                                                                       
                                
   
static uint64 temporary_files_size = 0;

   
                                                                
                      
   
typedef enum
{
  AllocateDescFile,
  AllocateDescPipe,
  AllocateDescDir,
  AllocateDescRawFD
} AllocateDescKind;

typedef struct
{
  AllocateDescKind kind;
  SubTransactionId create_subid;
  union
  {
    FILE *file;
    DIR *dir;
    int fd;
  } desc;
} AllocateDesc;

static int numAllocatedDescs = 0;
static int maxAllocatedDescs = 0;
static AllocateDesc *allocatedDescs = NULL;

   
                                                                
                                                 
   
static long tempFileCounter = 0;

   
                                                                        
                                                                              
                                                                       
                
   
static Oid *tempTableSpaces = NULL;
static int numTempTableSpaces = -1;
static int nextTempTableSpace = 0;

                       
   
                    
   
                                                
                                                                   
                                                        
                                                                      
                                                                             
                                                                            
                                                                     
                                    
   
                                                                        
                                                                          
                                                                            
                                                       
                                                                              
                                                                          
                                        
   
            
   
                                     
                          
                                               
                    
                                             
                              
   
                       
   
static void
Delete(File file);
static void
LruDelete(File file);
static void
Insert(File file);
static int
LruInsert(File file);
static bool
ReleaseLruFile(void);
static void
ReleaseLruFiles(void);
static File
AllocateVfd(void);
static void
FreeVfd(File file);

static int
FileAccess(File file);
static File
OpenTemporaryFileInTablespace(Oid tblspcOid, bool rejectError);
static bool
reserveAllocatedDesc(void);
static int
FreeDesc(AllocateDesc *desc);

static void
AtProcExit_Files(int code, Datum arg);
static void
CleanupTempFiles(bool isCommit, bool isProcExit);
static void
RemovePgTempFilesInDir(const char *tmpdirname, bool missing_ok, bool unlink_all);
static void
RemovePgTempRelationFiles(const char *tsdirname);
static void
RemovePgTempRelationFilesInDbspace(const char *dbspacedirname);

static void
walkdir(const char *path, void (*action)(const char *fname, bool isdir, int elevel), bool process_symlinks, int elevel);
#ifdef PG_FLUSH_DATA_WORKS
static void
pre_sync_fname(const char *fname, bool isdir, int elevel);
#endif
static void
datadir_fsync_fname(const char *fname, bool isdir, int elevel);
static void
unlink_if_exists_fname(const char *fname, bool isdir, int elevel);

static int
fsync_fname_ext(const char *fname, bool isdir, bool ignore_perm, int elevel);
static int
fsync_parent_path(const char *fname, int elevel);

   
                                                      
   
int
pg_fsync(int fd)
{
                                                                     
#if defined(HAVE_FSYNC_WRITETHROUGH) && !defined(FSYNC_WRITETHROUGH_IS_FSYNC)
  if (sync_method == SYNC_METHOD_FSYNC_WRITETHROUGH)
  {
    return pg_fsync_writethrough(fd);
  }
  else
#endif
    return pg_fsync_no_writethrough(fd);
}

   
                                                                     
                      
   
int
pg_fsync_no_writethrough(int fd)
{
  if (enableFsync)
  {
    return fsync(fd);
  }
  else
  {
    return 0;
  }
}

   
                         
   
int
pg_fsync_writethrough(int fd)
{
  if (enableFsync)
  {
#ifdef WIN32
    return _commit(fd);
#elif defined(F_FULLFSYNC)
    return (fcntl(fd, F_FULLFSYNC, 0) == -1) ? -1 : 0;
#else
    errno = ENOSYS;
    return -1;
#endif
  }
  else
  {
    return 0;
  }
}

   
                                                                                
   
                                                                      
   
int
pg_fdatasync(int fd)
{
  if (enableFsync)
  {
#ifdef HAVE_FDATASYNC
    return fdatasync(fd);
#else
    return fsync(fd);
#endif
  }
  else
  {
    return 0;
  }
}

   
                                                                               
   
                                                                          
   
void
pg_flush_data(int fd, off_t offset, off_t nbytes)
{
     
                                                                     
                                                                            
                                                                      
                                 
     
  if (!enableFsync)
  {
    return;
  }

     
                                                                             
                                               
     
#if defined(HAVE_SYNC_FILE_RANGE)
  {
    int rc;
    static bool not_implemented_by_kernel = false;

    if (not_implemented_by_kernel)
    {
      return;
    }

       
                                                                         
                                                                      
                                                                          
                                                                         
                                                                       
                                                                           
                                        
       
    rc = sync_file_range(fd, offset, nbytes, SYNC_FILE_RANGE_WRITE);
    if (rc != 0)
    {
      int elevel;

         
                                                          
                                                                  
                                                                         
         
      if (errno == ENOSYS)
      {
        elevel = WARNING;
        not_implemented_by_kernel = true;
      }
      else
      {
        elevel = data_sync_elevel(WARNING);
      }

      ereport(elevel, (errcode_for_file_access(), errmsg("could not flush dirty data: %m")));
    }

    return;
  }
#endif
#if !defined(WIN32) && defined(MS_ASYNC)
  {
    void *p;
    static int pagesize = 0;

       
                                                                 
                                                                        
                                                                          
                                                                
                                                              
       
                                                                           
                                                                
       

                                                                 
    if (offset == 0 && nbytes == 0)
    {
      nbytes = lseek(fd, 0, SEEK_END);
      if (nbytes < 0)
      {
        ereport(WARNING, (errcode_for_file_access(), errmsg("could not determine dirty data size: %m")));
        return;
      }
    }

       
                                                                         
                                                                         
                                                               
       

                                  
    if (pagesize == 0)
    {
      pagesize = sysconf(_SC_PAGESIZE);
    }

                                                                
    if (pagesize > 0)
    {
      nbytes = (nbytes / pagesize) * pagesize;
    }

                                            
    if (nbytes <= 0)
    {
      return;
    }

       
                                                                          
                                                                     
                                           
       
    if (nbytes <= (off_t)SSIZE_MAX)
    {
      p = mmap(NULL, nbytes, PROT_READ, MAP_SHARED, fd, offset);
    }
    else
    {
      p = MAP_FAILED;
    }

    if (p != MAP_FAILED)
    {
      int rc;

      rc = msync(p, (size_t)nbytes, MS_ASYNC);
      if (rc != 0)
      {
        ereport(data_sync_elevel(WARNING), (errcode_for_file_access(), errmsg("could not flush dirty data: %m")));
                                                   
      }

      rc = munmap(p, (size_t)nbytes);
      if (rc != 0)
      {
                                                      
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not munmap() while flushing data: %m")));
      }

      return;
    }
  }
#endif
#if defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
  {
    int rc;

       
                                                                       
                                                                        
                                                                         
                                                                     
                          
       

    rc = posix_fadvise(fd, offset, nbytes, POSIX_FADV_DONTNEED);

    if (rc != 0)
    {
                                                                    
      ereport(WARNING, (errcode_for_file_access(), errmsg("could not flush dirty data: %m")));
    }

    return;
  }
#endif
}

   
                                                                      
   
                                                                               
                                                                    
   
void
fsync_fname(const char *fname, bool isdir)
{
  fsync_fname_ext(fname, isdir, false, data_sync_elevel(ERROR));
}

   
                                                                               
   
                                                                           
                                                                           
                                                                            
                                                             
   
                                                                           
                                                                               
   
                                                                           
                                                                        
                                        
   
                                                  
   
                                                                              
                      
   
int
durable_rename(const char *oldfile, const char *newfile, int elevel)
{
  int fd;

     
                                                                             
                                                                     
                                                                         
                                                                           
                    
     
  if (fsync_fname_ext(oldfile, false, false, elevel) != 0)
  {
    return -1;
  }

  fd = OpenTransientFile(newfile, PG_BINARY | O_RDWR);
  if (fd < 0)
  {
    if (errno != ENOENT)
    {
      ereport(elevel, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", newfile)));
      return -1;
    }
  }
  else
  {
    if (pg_fsync(fd) != 0)
    {
      int save_errno;

                                                                      
      save_errno = errno;
      CloseTransientFile(fd);
      errno = save_errno;

      ereport(elevel, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", newfile)));
      return -1;
    }

    if (CloseTransientFile(fd))
    {
      ereport(elevel, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", newfile)));
      return -1;
    }
  }

                                   
  if (rename(oldfile, newfile) < 0)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", oldfile, newfile)));
    return -1;
  }

     
                                                                           
                                             
     
  if (fsync_fname_ext(newfile, false, false, elevel) != 0)
  {
    return -1;
  }

  if (fsync_parent_path(newfile, elevel) != 0)
  {
    return -1;
  }

  return 0;
}

   
                                                       
   
                                                                           
                                                                           
                                       
   
                                                                           
                           
   
                                                     
   
                                                                              
                      
   
int
durable_unlink(const char *fname, int elevel)
{
  if (unlink(fname) < 0)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", fname)));
    return -1;
  }

     
                                                                        
                       
     
  if (fsync_parent_path(fname, elevel) != 0)
  {
    return -1;
  }

  return 0;
}

   
                                                                
   
                                                                             
                                                
   
                                                                              
                    
   
                                                  
   
                                                                              
                      
   
int
durable_link_or_rename(const char *oldfile, const char *newfile, int elevel)
{
     
                                                                          
                                         
     
  if (fsync_fname_ext(oldfile, false, false, elevel) != 0)
  {
    return -1;
  }

#if HAVE_WORKING_LINK
  if (link(oldfile, newfile) < 0)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not link file \"%s\" to \"%s\": %m", oldfile, newfile), (AmCheckpointerProcess() ? errhint("This is known to fail occasionally during archive recovery, where it is harmless.") : 0)));
    return -1;
  }
  unlink(oldfile);
#else
                                           
  if (rename(oldfile, newfile) < 0)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", oldfile, newfile), (AmCheckpointerProcess() ? errhint("This is known to fail occasionally during archive recovery, where it is harmless.") : 0)));
    return -1;
  }
#endif

     
                                                                           
                                              
     
  if (fsync_fname_ext(newfile, false, false, elevel) != 0)
  {
    return -1;
  }

                                 
  if (fsync_parent_path(newfile, elevel) != 0)
  {
    return -1;
  }

  return 0;
}

   
                                                                    
   
                                                                    
                                         
   
void
InitFileAccess(void)
{
  Assert(SizeVfdCache == 0);                        

                                     
  VfdCache = (Vfd *)malloc(sizeof(Vfd));
  if (VfdCache == NULL)
  {
    ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }

  MemSet((char *)&(VfdCache[0]), 0, sizeof(Vfd));
  VfdCache->fd = VFD_CLOSED;

  SizeVfdCache = 1;

                                                                        
  on_proc_exit(AtProcExit_Files, 0);
}

   
                                                                        
                                            
   
                                                                       
                                                                           
                                                                            
                                                                          
                                                                 
   
                                                   
   
static void
count_usable_fds(int max_to_probe, int *usable_fds, int *already_open)
{
  int *fd;
  int size;
  int used = 0;
  int highestfd = 0;
  int j;

#ifdef HAVE_GETRLIMIT
  struct rlimit rlim;
  int getrlimit_status;
#endif

  size = 1024;
  fd = (int *)palloc(size * sizeof(int));

#ifdef HAVE_GETRLIMIT
#ifdef RLIMIT_NOFILE                                       
  getrlimit_status = getrlimit(RLIMIT_NOFILE, &rlim);
#else                           
  getrlimit_status = getrlimit(RLIMIT_OFILE, &rlim);
#endif                    
  if (getrlimit_status != 0)
  {
    ereport(WARNING, (errmsg("getrlimit failed: %m")));
  }
#endif                     

                                                
  for (;;)
  {
    int thisfd;

#ifdef HAVE_GETRLIMIT

       
                                                                       
                      
       
    if (getrlimit_status == 0 && highestfd >= rlim.rlim_cur - 1)
    {
      break;
    }
#endif

    thisfd = dup(0);
    if (thisfd < 0)
    {
                                                    
      if (errno != EMFILE && errno != ENFILE)
      {
        elog(WARNING, "dup(0) failed after %d successes: %m", used);
      }
      break;
    }

    if (used >= size)
    {
      size *= 2;
      fd = (int *)repalloc(fd, size * sizeof(int));
    }
    fd[used++] = thisfd;

    if (highestfd < thisfd)
    {
      highestfd = thisfd;
    }

    if (used >= max_to_probe)
    {
      break;
    }
  }

                                   
  for (j = 0; j < used; j++)
  {
    close(fd[j]);
  }

  pfree(fd);

     
                                                                           
                                                                           
                                                              
     
  *usable_fds = used;
  *already_open = highestfd + 1 - used;
}

   
                    
                                                                    
   
void
set_max_safe_fds(void)
{
  int usable_fds;
  int already_open;

               
                                    
                                                             
                                                                       
                                                                           
                                                    
               
     
  count_usable_fds(max_files_per_process, &usable_fds, &already_open);

  max_safe_fds = Min(usable_fds, max_files_per_process - already_open);

     
                                                 
     
  max_safe_fds -= NUM_RESERVED_FDS;

     
                                               
     
  if (max_safe_fds < FD_MINFREE)
  {
    ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("insufficient file descriptors available to start server process"), errdetail("System allows %d, we need at least %d.", max_safe_fds + NUM_RESERVED_FDS, FD_MINFREE + NUM_RESERVED_FDS)));
  }

  elog(DEBUG2, "max_safe_fds = %d, usable_fds = %d, already_open = %d", max_safe_fds, usable_fds, already_open);
}

   
                                                                           
                       
   
int
BasicOpenFile(const char *fileName, int fileFlags)
{
  return BasicOpenFilePerm(fileName, fileFlags, pg_file_create_mode);
}

   
                                                                             
   
                                                                          
                                                                         
                                                                           
                                                                         
                                                                      
                                                                         
                                                                    
   
                                                                           
                                                                         
                                                                          
                                                                           
   
int
BasicOpenFilePerm(const char *fileName, int fileFlags, mode_t fileMode)
{
  int fd;

tryAgain:
  fd = open(fileName, fileFlags, fileMode);

  if (fd >= 0)
  {
    return fd;               
  }

  if (errno == EMFILE || errno == ENFILE)
  {
    int save_errno = errno;

    ereport(LOG, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("out of file descriptors: %m; release and retry")));
    errno = 0;
    if (ReleaseLruFile())
    {
      goto tryAgain;
    }
    errno = save_errno;
  }

  return -1;              
}

#if defined(FDDEBUG)

static void
_dump_lru(void)
{
  int mru = VfdCache[0].lruLessRecently;
  Vfd *vfdP = &VfdCache[mru];
  char buf[2048];

  snprintf(buf, sizeof(buf), "LRU: MOST %d ", mru);
  while (mru != 0)
  {
    mru = vfdP->lruLessRecently;
    vfdP = &VfdCache[mru];
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%d ", mru);
  }
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "LEAST");
  elog(LOG, "%s", buf);
}
#endif              

static void
Delete(File file)
{
  Vfd *vfdP;

  Assert(file != 0);

  DO_DB(elog(LOG, "Delete %d (%s)", file, VfdCache[file].fileName));
  DO_DB(_dump_lru());

  vfdP = &VfdCache[file];

  VfdCache[vfdP->lruLessRecently].lruMoreRecently = vfdP->lruMoreRecently;
  VfdCache[vfdP->lruMoreRecently].lruLessRecently = vfdP->lruLessRecently;

  DO_DB(_dump_lru());
}

static void
LruDelete(File file)
{
  Vfd *vfdP;

  Assert(file != 0);

  DO_DB(elog(LOG, "LruDelete %d (%s)", file, VfdCache[file].fileName));

  vfdP = &VfdCache[file];

     
                                                                           
                                                        
     
  if (close(vfdP->fd))
  {
    elog(vfdP->fdstate & FD_TEMP_FILE_LIMIT ? LOG : data_sync_elevel(LOG), "could not close file \"%s\": %m", vfdP->fileName);
  }
  vfdP->fd = VFD_CLOSED;
  --nfile;

                                               
  Delete(file);
}

static void
Insert(File file)
{
  Vfd *vfdP;

  Assert(file != 0);

  DO_DB(elog(LOG, "Insert %d (%s)", file, VfdCache[file].fileName));
  DO_DB(_dump_lru());

  vfdP = &VfdCache[file];

  vfdP->lruMoreRecently = 0;
  vfdP->lruLessRecently = VfdCache[0].lruLessRecently;
  VfdCache[0].lruLessRecently = file;
  VfdCache[vfdP->lruLessRecently].lruMoreRecently = file;

  DO_DB(_dump_lru());
}

                                                                  
static int
LruInsert(File file)
{
  Vfd *vfdP;

  Assert(file != 0);

  DO_DB(elog(LOG, "LruInsert %d (%s)", file, VfdCache[file].fileName));

  vfdP = &VfdCache[file];

  if (FileIsNotOpen(file))
  {
                                  
    ReleaseLruFiles();

       
                                                                         
                                                                         
                                  
       
    vfdP->fd = BasicOpenFilePerm(vfdP->fileName, vfdP->fileFlags, vfdP->fileMode);
    if (vfdP->fd < 0)
    {
      DO_DB(elog(LOG, "re-open failed: %m"));
      return -1;
    }
    else
    {
      ++nfile;
    }
  }

     
                                        
     

  Insert(file);

  return 0;
}

   
                                                                 
   
static bool
ReleaseLruFile(void)
{
  DO_DB(elog(LOG, "ReleaseLruFile. Opened %d", nfile));

  if (nfile > 0)
  {
       
                                                                           
                    
       
    Assert(VfdCache[0].lruMoreRecently != 0);
    LruDelete(VfdCache[0].lruMoreRecently);
    return true;                   
  }
  return false;                                 
}

   
                                                                     
                                                            
   
static void
ReleaseLruFiles(void)
{
  while (nfile + numAllocatedDescs >= max_safe_fds)
  {
    if (!ReleaseLruFile())
    {
      break;
    }
  }
}

static File
AllocateVfd(void)
{
  Index i;
  File file;

  DO_DB(elog(LOG, "AllocateVfd. Size %zu", SizeVfdCache));

  Assert(SizeVfdCache > 0);                                 

  if (VfdCache[0].nextFree == 0)
  {
       
                                                                        
                                                                       
                                                        
       
    Size newCacheSize = SizeVfdCache * 2;
    Vfd *newVfdCache;

    if (newCacheSize < 32)
    {
      newCacheSize = 32;
    }

       
                                                                
       
    newVfdCache = (Vfd *)realloc(VfdCache, sizeof(Vfd) * newCacheSize);
    if (newVfdCache == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
    VfdCache = newVfdCache;

       
                                                                    
       
    for (i = SizeVfdCache; i < newCacheSize; i++)
    {
      MemSet((char *)&(VfdCache[i]), 0, sizeof(Vfd));
      VfdCache[i].nextFree = i + 1;
      VfdCache[i].fd = VFD_CLOSED;
    }
    VfdCache[newCacheSize - 1].nextFree = 0;
    VfdCache[0].nextFree = SizeVfdCache;

       
                           
       
    SizeVfdCache = newCacheSize;
  }

  file = VfdCache[0].nextFree;

  VfdCache[0].nextFree = VfdCache[file].nextFree;

  return file;
}

static void
FreeVfd(File file)
{
  Vfd *vfdP = &VfdCache[file];

  DO_DB(elog(LOG, "FreeVfd: %d (%s)", file, vfdP->fileName ? vfdP->fileName : ""));

  if (vfdP->fileName != NULL)
  {
    free(vfdP->fileName);
    vfdP->fileName = NULL;
  }
  vfdP->fdstate = 0x0;

  vfdP->nextFree = VfdCache[0].nextFree;
  VfdCache[0].nextFree = file;
}

                                                                  
static int
FileAccess(File file)
{
  int returnValue;

  DO_DB(elog(LOG, "FileAccess %d (%s)", file, VfdCache[file].fileName));

     
                                                                          
                                                                        
     

  if (FileIsNotOpen(file))
  {
    returnValue = LruInsert(file);
    if (returnValue != 0)
    {
      return returnValue;
    }
  }
  else if (VfdCache[0].lruLessRecently != file)
  {
       
                                                                         
                                                                    
       

    Delete(file);
    Insert(file);
  }

  return 0;
}

   
                                                                   
   
static void
ReportTemporaryFileUsage(const char *path, off_t size)
{
  pgstat_report_tempfile(size);

  if (log_temp_files >= 0)
  {
    if ((size / 1024) >= log_temp_files)
    {
      ereport(LOG, (errmsg("temporary file: path \"%s\", size %lu", path, (unsigned long)size)));
    }
  }
}

   
                                                            
                                                                         
                               
   
static void
RegisterTemporaryFile(File file)
{
  ResourceOwnerRememberFile(CurrentResourceOwner, file);
  VfdCache[file].resowner = CurrentResourceOwner;

                                                    
  VfdCache[file].fdstate |= FD_CLOSE_AT_EOXACT;
  have_xact_temporary_files = true;
}

   
                                                                      
   
#ifdef NOT_USED
void
FileInvalidate(File file)
{
  Assert(FileIsValid(file));
  if (!FileIsNotOpen(file))
  {
    LruDelete(file);
  }
}
#endif

   
                                                                              
                       
   
File
PathNameOpenFile(const char *fileName, int fileFlags)
{
  return PathNameOpenFilePerm(fileName, fileFlags, pg_file_create_mode);
}

   
                                         
   
                                                                 
                                                                     
                                                               
   
File
PathNameOpenFilePerm(const char *fileName, int fileFlags, mode_t fileMode)
{
  char *fnamecopy;
  File file;
  Vfd *vfdP;

  DO_DB(elog(LOG, "PathNameOpenFilePerm: %s %x %o", fileName, fileFlags, fileMode));

     
                                                                        
     
  fnamecopy = strdup(fileName);
  if (fnamecopy == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }

  file = AllocateVfd();
  vfdP = &VfdCache[file];

                                
  ReleaseLruFiles();

  vfdP->fd = BasicOpenFilePerm(fileName, fileFlags, fileMode);

  if (vfdP->fd < 0)
  {
    int save_errno = errno;

    FreeVfd(file);
    free(fnamecopy);
    errno = save_errno;
    return -1;
  }
  ++nfile;
  DO_DB(elog(LOG, "PathNameOpenFile: success %d", vfdP->fd));

  vfdP->fileName = fnamecopy;
                                                             
  vfdP->fileFlags = fileFlags & ~(O_CREAT | O_TRUNC | O_EXCL);
  vfdP->fileMode = fileMode;
  vfdP->fileSize = 0;
  vfdP->fdstate = 0x0;
  vfdP->resowner = NULL;

  Insert(file);

  return file;
}

   
                                                                             
                                                                           
                                                                            
                                               
   
                                                                             
                                                                             
                                                                            
                                           
   
void
PathNameCreateTemporaryDir(const char *basedir, const char *directory)
{
  if (MakePGDirectory(directory) < 0)
  {
    if (errno == EEXIST)
    {
      return;
    }

       
                                                                           
                                                                         
                  
       
    if (MakePGDirectory(basedir) < 0 && errno != EEXIST)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("cannot create temporary directory \"%s\": %m", basedir)));
    }

                    
    if (MakePGDirectory(directory) < 0 && errno != EEXIST)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("cannot create temporary subdirectory \"%s\": %m", directory)));
    }
  }
}

   
                                                          
   
void
PathNameDeleteTemporaryDir(const char *dirname)
{
  struct stat statbuf;

                                          
  if (stat(dirname, &statbuf) != 0 && errno == ENOENT)
  {
    return;
  }

     
                                                                          
                                                                          
                                                                          
                                                                            
                        
     
  walkdir(dirname, unlink_if_exists_fname, false, LOG);
}

   
                                                               
   
                                                                       
                                                                       
                                                
   
                                                                            
                                                                              
                                                                            
                                                                             
                                                                             
                                                                 
   
File
OpenTemporaryFile(bool interXact)
{
  File file = 0;

     
                                                                            
                                                
     
  if (!interXact)
  {
    ResourceOwnerEnlargeFiles(CurrentResourceOwner);
  }

     
                                                                           
                                                                          
                                        
     
                                                                         
                                                                          
                                                         
     
  if (numTempTableSpaces > 0 && !interXact)
  {
    Oid tblspcOid = GetNextTempTableSpace();

    if (OidIsValid(tblspcOid))
    {
      file = OpenTemporaryFileInTablespace(tblspcOid, false);
    }
  }

     
                                                                   
                                                                            
                                                                          
     
  if (file <= 0)
  {
    file = OpenTemporaryFileInTablespace(MyDatabaseTableSpace ? MyDatabaseTableSpace : DEFAULTTABLESPACE_OID, true);
  }

                                                                   
  VfdCache[file].fdstate |= FD_DELETE_AT_CLOSE | FD_TEMP_FILE_LIMIT;

                                                   
  if (!interXact)
  {
    RegisterTemporaryFile(file);
  }

  return file;
}

   
                                                                
   
void
TempTablespacePath(char *path, Oid tablespace)
{
     
                                                          
     
                                                                    
     
  if (tablespace == InvalidOid || tablespace == DEFAULTTABLESPACE_OID || tablespace == GLOBALTABLESPACE_OID)
  {
    snprintf(path, MAXPGPATH, "base/%s", PG_TEMP_FILES_DIR);
  }
  else
  {
                                                         
    snprintf(path, MAXPGPATH, "pg_tblspc/%u/%s/%s", tablespace, TABLESPACE_VERSION_DIRECTORY, PG_TEMP_FILES_DIR);
  }
}

   
                                                   
                                                            
   
static File
OpenTemporaryFileInTablespace(Oid tblspcOid, bool rejectError)
{
  char tempdirpath[MAXPGPATH];
  char tempfilepath[MAXPGPATH];
  File file;

  TempTablespacePath(tempdirpath, tblspcOid);

     
                                                                       
                        
     
  snprintf(tempfilepath, sizeof(tempfilepath), "%s/%s%d.%ld", tempdirpath, PG_TEMP_FILE_PREFIX, MyProcPid, tempFileCounter++);

     
                                                                             
                                   
     
  file = PathNameOpenFile(tempfilepath, O_RDWR | O_CREAT | O_TRUNC | PG_BINARY);
  if (file <= 0)
  {
       
                                                                          
                            
       
                                                                       
                                                                      
                                                             
       
    (void)MakePGDirectory(tempdirpath);

    file = PathNameOpenFile(tempfilepath, O_RDWR | O_CREAT | O_TRUNC | PG_BINARY);
    if (file <= 0 && rejectError)
    {
      elog(ERROR, "could not create temporary file \"%s\": %m", tempfilepath);
    }
  }

  return file;
}

   
                                                                              
                                                                         
                                                                            
                                                                        
   
                                                                            
                                                                            
                                                                            
                                                                               
                            
   
File
PathNameCreateTemporaryFile(const char *path, bool error_on_failure)
{
  File file;

  ResourceOwnerEnlargeFiles(CurrentResourceOwner);

     
                                                                             
                                   
     
  file = PathNameOpenFile(path, O_RDWR | O_CREAT | O_TRUNC | PG_BINARY);
  if (file <= 0)
  {
    if (error_on_failure)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not create temporary file \"%s\": %m", path)));
    }
    else
    {
      return file;
    }
  }

                                               
  VfdCache[file].fdstate |= FD_TEMP_FILE_LIMIT;

                                        
  RegisterTemporaryFile(file);

  return file;
}

   
                                                                              
                                                                   
                                                                             
                                                               
   
File
PathNameOpenTemporaryFile(const char *path)
{
  File file;

  ResourceOwnerEnlargeFiles(CurrentResourceOwner);

                                   
  file = PathNameOpenFile(path, O_RDONLY | PG_BINARY);

                                                      
  if (file <= 0 && errno != ENOENT)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open temporary file \"%s\": %m", path)));
  }

  if (file > 0)
  {
                                          
    RegisterTemporaryFile(file);
  }

  return file;
}

   
                                                                         
           
   
bool
PathNameDeleteTemporaryFile(const char *path, bool error_on_failure)
{
  struct stat filestats;
  int stat_errno;

                                                
  if (stat(path, &filestats) != 0)
  {
    stat_errno = errno;
  }
  else
  {
    stat_errno = 0;
  }

     
                                                                  
                                                                         
                                                       
     
  if (stat_errno == ENOENT)
  {
    return false;
  }

  if (unlink(path) < 0)
  {
    if (errno != ENOENT)
    {
      ereport(error_on_failure ? ERROR : LOG, (errcode_for_file_access(), errmsg("could not unlink temporary file \"%s\": %m", path)));
    }
    return false;
  }

  if (stat_errno == 0)
  {
    ReportTemporaryFileUsage(path, filestats.st_size);
  }
  else
  {
    errno = stat_errno;
    ereport(LOG, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", path)));
  }

  return true;
}

   
                                  
   
void
FileClose(File file)
{
  Vfd *vfdP;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileClose: %d (%s)", file, VfdCache[file].fileName));

  vfdP = &VfdCache[file];

  if (!FileIsNotOpen(file))
  {
                        
    if (close(vfdP->fd))
    {
         
                                                                       
                        
         
      elog(vfdP->fdstate & FD_TEMP_FILE_LIMIT ? LOG : data_sync_elevel(LOG), "could not close file \"%s\": %m", vfdP->fileName);
    }

    --nfile;
    vfdP->fd = VFD_CLOSED;

                                           
    Delete(file);
  }

  if (vfdP->fdstate & FD_TEMP_FILE_LIMIT)
  {
                                                                          
    temporary_files_size -= vfdP->fileSize;
    vfdP->fileSize = 0;
  }

     
                                                                         
     
  if (vfdP->fdstate & FD_DELETE_AT_CLOSE)
  {
    struct stat filestats;
    int stat_errno;

       
                                                                          
                                                                       
                                                                          
                                                                           
                                                               
       
    vfdP->fdstate &= ~FD_DELETE_AT_CLOSE;

                              
    if (stat(vfdP->fileName, &filestats))
    {
      stat_errno = errno;
    }
    else
    {
      stat_errno = 0;
    }

                                   
    if (unlink(vfdP->fileName))
    {
      elog(LOG, "could not unlink file \"%s\": %m", vfdP->fileName);
    }

                                          
    if (stat_errno == 0)
    {
      ReportTemporaryFileUsage(vfdP->fileName, filestats.st_size);
    }
    else
    {
      errno = stat_errno;
      elog(LOG, "could not stat file \"%s\": %m", vfdP->fileName);
    }
  }

                                             
  if (vfdP->resowner)
  {
    ResourceOwnerForgetFile(vfdP->resowner, file);
  }

     
                                          
     
  FreeVfd(file);
}

   
                                                                           
   
                                                                             
                                                                        
                                                                            
                                                                               
                 
   
int
FilePrefetch(File file, off_t offset, int amount, uint32 wait_event_info)
{
#if defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
  int returnCode;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FilePrefetch: %d (%s) " INT64_FORMAT " %d", file, VfdCache[file].fileName, (int64)offset, amount));

  returnCode = FileAccess(file);
  if (returnCode < 0)
  {
    return returnCode;
  }

  pgstat_report_wait_start(wait_event_info);
  returnCode = posix_fadvise(VfdCache[file].fd, offset, amount, POSIX_FADV_WILLNEED);
  pgstat_report_wait_end();

  return returnCode;
#else
  Assert(FileIsValid(file));
  return 0;
#endif
}

void
FileWriteback(File file, off_t offset, off_t nbytes, uint32 wait_event_info)
{
  int returnCode;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileWriteback: %d (%s) " INT64_FORMAT " " INT64_FORMAT, file, VfdCache[file].fileName, (int64)offset, (int64)nbytes));

  if (nbytes <= 0)
  {
    return;
  }

  returnCode = FileAccess(file);
  if (returnCode < 0)
  {
    return;
  }

  pgstat_report_wait_start(wait_event_info);
  pg_flush_data(VfdCache[file].fd, offset, nbytes);
  pgstat_report_wait_end();
}

int
FileRead(File file, char *buffer, int amount, off_t offset, uint32 wait_event_info)
{
  int returnCode;
  Vfd *vfdP;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileRead: %d (%s) " INT64_FORMAT " %d %p", file, VfdCache[file].fileName, (int64)offset, amount, buffer));

  returnCode = FileAccess(file);
  if (returnCode < 0)
  {
    return returnCode;
  }

  vfdP = &VfdCache[file];

retry:
  pgstat_report_wait_start(wait_event_info);
  returnCode = pg_pread(vfdP->fd, buffer, amount, offset);
  pgstat_report_wait_end();

  if (returnCode < 0)
  {
       
                                                                      
                                                                   
       
                                                                           
                                                   
       
#ifdef WIN32
    DWORD error = GetLastError();

    switch (error)
    {
    case ERROR_NO_SYSTEM_RESOURCES:
      pg_usleep(1000L);
      errno = EINTR;
      break;
    default:
      _dosmaperr(error);
      break;
    }
#endif
                                    
    if (errno == EINTR)
    {
      goto retry;
    }
  }

  return returnCode;
}

int
FileWrite(File file, char *buffer, int amount, off_t offset, uint32 wait_event_info)
{
  int returnCode;
  Vfd *vfdP;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileWrite: %d (%s) " INT64_FORMAT " %d %p", file, VfdCache[file].fileName, (int64)offset, amount, buffer));

  returnCode = FileAccess(file);
  if (returnCode < 0)
  {
    return returnCode;
  }

  vfdP = &VfdCache[file];

     
                                                                            
                                                                             
                                                                            
                                                                        
                                                                        
                                                     
     
  if (temp_file_limit >= 0 && (vfdP->fdstate & FD_TEMP_FILE_LIMIT))
  {
    off_t past_write = offset + amount;

    if (past_write > vfdP->fileSize)
    {
      uint64 newTotal = temporary_files_size;

      newTotal += past_write - vfdP->fileSize;
      if (newTotal > (uint64)temp_file_limit * (uint64)1024)
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("temporary file size exceeds temp_file_limit (%dkB)", temp_file_limit)));
      }
    }
  }

retry:
  errno = 0;
  pgstat_report_wait_start(wait_event_info);
  returnCode = pg_pwrite(VfdCache[file].fd, buffer, amount, offset);
  pgstat_report_wait_end();

                                                                  
  if (returnCode != amount && errno == 0)
  {
    errno = ENOSPC;
  }

  if (returnCode >= 0)
  {
       
                                                                       
       
                                                                           
                                                                           
                         
       
    if (vfdP->fdstate & FD_TEMP_FILE_LIMIT)
    {
      off_t past_write = offset + amount;

      if (past_write > vfdP->fileSize)
      {
        temporary_files_size += past_write - vfdP->fileSize;
        vfdP->fileSize = past_write;
      }
    }
  }
  else
  {
       
                                  
       
#ifdef WIN32
    DWORD error = GetLastError();

    switch (error)
    {
    case ERROR_NO_SYSTEM_RESOURCES:
      pg_usleep(1000L);
      errno = EINTR;
      break;
    default:
      _dosmaperr(error);
      break;
    }
#endif
                                    
    if (errno == EINTR)
    {
      goto retry;
    }
  }

  return returnCode;
}

int
FileSync(File file, uint32 wait_event_info)
{
  int returnCode;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileSync: %d (%s)", file, VfdCache[file].fileName));

  returnCode = FileAccess(file);
  if (returnCode < 0)
  {
    return returnCode;
  }

  pgstat_report_wait_start(wait_event_info);
  returnCode = pg_fsync(VfdCache[file].fd);
  pgstat_report_wait_end();

  return returnCode;
}

off_t
FileSize(File file)
{
  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileSize %d (%s)", file, VfdCache[file].fileName));

  if (FileIsNotOpen(file))
  {
    if (FileAccess(file) < 0)
    {
      return (off_t)-1;
    }
  }

  return lseek(VfdCache[file].fd, 0, SEEK_END);
}

int
FileTruncate(File file, off_t offset, uint32 wait_event_info)
{
  int returnCode;

  Assert(FileIsValid(file));

  DO_DB(elog(LOG, "FileTruncate %d (%s)", file, VfdCache[file].fileName));

  returnCode = FileAccess(file);
  if (returnCode < 0)
  {
    return returnCode;
  }

  pgstat_report_wait_start(wait_event_info);
  returnCode = ftruncate(VfdCache[file].fd, offset);
  pgstat_report_wait_end();

  if (returnCode == 0 && VfdCache[file].fileSize > offset)
  {
                                                        
    Assert(VfdCache[file].fdstate & FD_TEMP_FILE_LIMIT);
    temporary_files_size -= VfdCache[file].fileSize - offset;
    VfdCache[file].fileSize = offset;
  }

  return returnCode;
}

   
                                                     
   
                                                                          
                       
   
char *
FilePathName(File file)
{
  Assert(FileIsValid(file));

  return VfdCache[file].fileName;
}

   
                                                     
   
                                                                            
                                                                              
                                                                           
                             
   
int
FileGetRawDesc(File file)
{
  Assert(FileIsValid(file));
  return VfdCache[file].fd;
}

   
                                                       
   
int
FileGetRawFlags(File file)
{
  Assert(FileIsValid(file));
  return VfdCache[file].fileFlags;
}

   
                                                               
   
mode_t
FileGetRawMode(File file)
{
  Assert(FileIsValid(file));
  return VfdCache[file].fileMode;
}

   
                                                                              
                                                  
   
static bool
reserveAllocatedDesc(void)
{
  AllocateDesc *newDescs;
  int newMax;

                                                   
  if (numAllocatedDescs < maxAllocatedDescs)
  {
    return true;
  }

     
                                                                             
                                                                            
                                                                       
                                                                  
     
  if (allocatedDescs == NULL)
  {
    newMax = FD_MINFREE / 2;
    newDescs = (AllocateDesc *)malloc(newMax * sizeof(AllocateDesc));
                                                       
    if (newDescs == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
    allocatedDescs = newDescs;
    maxAllocatedDescs = newMax;
    return true;
  }

     
                                                                            
                                                                        
     
                                                                            
                                                                            
                                                                        
                                                          
     
  newMax = max_safe_fds / 2;
  if (newMax > maxAllocatedDescs)
  {
    newDescs = (AllocateDesc *)realloc(allocatedDescs, newMax * sizeof(AllocateDesc));
                                                   
    if (newDescs == NULL)
    {
      return false;
    }
    allocatedDescs = newDescs;
    maxAllocatedDescs = newMax;
    return true;
  }

                                                
  return false;
}

   
                                                                       
                                                                       
                                                                             
   
                                                                        
                                                                       
                                                                           
                                                                        
                                                                           
   
                                                                       
                                                                      
                                                                        
   
                                                                            
   
FILE *
AllocateFile(const char *name, const char *mode)
{
  FILE *file;

  DO_DB(elog(LOG, "AllocateFile: Allocated %d (%s)", numAllocatedDescs, name));

                                               
  if (!reserveAllocatedDesc())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("exceeded maxAllocatedDescs (%d) while trying to open file \"%s\"", maxAllocatedDescs, name)));
  }

                                
  ReleaseLruFiles();

TryAgain:
  if ((file = fopen(name, mode)) != NULL)
  {
    AllocateDesc *desc = &allocatedDescs[numAllocatedDescs];

    desc->kind = AllocateDescFile;
    desc->desc.file = file;
    desc->create_subid = GetCurrentSubTransactionId();
    numAllocatedDescs++;
    return desc->desc.file;
  }

  if (errno == EMFILE || errno == ENFILE)
  {
    int save_errno = errno;

    ereport(LOG, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("out of file descriptors: %m; release and retry")));
    errno = 0;
    if (ReleaseLruFile())
    {
      goto TryAgain;
    }
    errno = save_errno;
  }

  return NULL;
}

   
                                                                           
                           
   
int
OpenTransientFile(const char *fileName, int fileFlags)
{
  return OpenTransientFilePerm(fileName, fileFlags, pg_file_create_mode);
}

   
                                                                
   
int
OpenTransientFilePerm(const char *fileName, int fileFlags, mode_t fileMode)
{
  int fd;

  DO_DB(elog(LOG, "OpenTransientFile: Allocated %d (%s)", numAllocatedDescs, fileName));

                                               
  if (!reserveAllocatedDesc())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("exceeded maxAllocatedDescs (%d) while trying to open file \"%s\"", maxAllocatedDescs, fileName)));
  }

                                
  ReleaseLruFiles();

  fd = BasicOpenFilePerm(fileName, fileFlags, fileMode);

  if (fd >= 0)
  {
    AllocateDesc *desc = &allocatedDescs[numAllocatedDescs];

    desc->kind = AllocateDescRawFD;
    desc->desc.fd = fd;
    desc->create_subid = GetCurrentSubTransactionId();
    numAllocatedDescs++;

    return fd;
  }

  return -1;              
}

   
                                                                          
                                                                       
                                                                   
   
                                                                           
                                                                            
                                                                             
   
FILE *
OpenPipeStream(const char *command, const char *mode)
{
  FILE *file;
  int save_errno;

  DO_DB(elog(LOG, "OpenPipeStream: Allocated %d (%s)", numAllocatedDescs, command));

                                               
  if (!reserveAllocatedDesc())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("exceeded maxAllocatedDescs (%d) while trying to execute command \"%s\"", maxAllocatedDescs, command)));
  }

                                
  ReleaseLruFiles();

TryAgain:
  fflush(stdout);
  fflush(stderr);
  pqsignal(SIGPIPE, SIG_DFL);
  errno = 0;
  file = popen(command, mode);
  save_errno = errno;
  pqsignal(SIGPIPE, SIG_IGN);
  errno = save_errno;
  if (file != NULL)
  {
    AllocateDesc *desc = &allocatedDescs[numAllocatedDescs];

    desc->kind = AllocateDescPipe;
    desc->desc.file = file;
    desc->create_subid = GetCurrentSubTransactionId();
    numAllocatedDescs++;
    return desc->desc.file;
  }

  if (errno == EMFILE || errno == ENFILE)
  {
    ereport(LOG, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("out of file descriptors: %m; release and retry")));
    if (ReleaseLruFile())
    {
      goto TryAgain;
    }
    errno = save_errno;
  }

  return NULL;
}

   
                                     
   
                                                              
   
static int
FreeDesc(AllocateDesc *desc)
{
  int result;

                                   
  switch (desc->kind)
  {
  case AllocateDescFile:
    result = fclose(desc->desc.file);
    break;
  case AllocateDescPipe:
    result = pclose(desc->desc.file);
    break;
  case AllocateDescDir:
    result = closedir(desc->desc.dir);
    break;
  case AllocateDescRawFD:
    result = close(desc->desc.fd);
    break;
  default:
    elog(ERROR, "AllocateDesc kind not recognized");
    result = 0;                          
    break;
  }

                                                   
  numAllocatedDescs--;
  *desc = allocatedDescs[numAllocatedDescs];

  return result;
}

   
                                          
   
                                                                         
                           
   
int
FreeFile(FILE *file)
{
  int i;

  DO_DB(elog(LOG, "FreeFile: Allocated %d", numAllocatedDescs));

                                                                 
  for (i = numAllocatedDescs; --i >= 0;)
  {
    AllocateDesc *desc = &allocatedDescs[i];

    if (desc->kind == AllocateDescFile && desc->desc.file == file)
    {
      return FreeDesc(desc);
    }
  }

                                                                       
  elog(WARNING, "file passed to FreeFile was not obtained from AllocateFile");

  return fclose(file);
}

   
                                               
   
                                                                        
                           
   
int
CloseTransientFile(int fd)
{
  int i;

  DO_DB(elog(LOG, "CloseTransientFile: Allocated %d", numAllocatedDescs));

                                                               
  for (i = numAllocatedDescs; --i >= 0;)
  {
    AllocateDesc *desc = &allocatedDescs[i];

    if (desc->kind == AllocateDescRawFD && desc->desc.fd == fd)
    {
      return FreeDesc(desc);
    }
  }

                                                                       
  elog(WARNING, "fd passed to CloseTransientFile was not obtained from OpenTransientFile");

  return close(fd);
}

   
                                                                          
                                                                         
                                                                       
                                                 
   
                                                                          
                                                                         
                                 
   
                                                                              
   
DIR *
AllocateDir(const char *dirname)
{
  DIR *dir;

  DO_DB(elog(LOG, "AllocateDir: Allocated %d (%s)", numAllocatedDescs, dirname));

                                               
  if (!reserveAllocatedDesc())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("exceeded maxAllocatedDescs (%d) while trying to open directory \"%s\"", maxAllocatedDescs, dirname)));
  }

                                
  ReleaseLruFiles();

TryAgain:
  if ((dir = opendir(dirname)) != NULL)
  {
    AllocateDesc *desc = &allocatedDescs[numAllocatedDescs];

    desc->kind = AllocateDescDir;
    desc->desc.dir = dir;
    desc->create_subid = GetCurrentSubTransactionId();
    numAllocatedDescs++;
    return desc->desc.dir;
  }

  if (errno == EMFILE || errno == ENFILE)
  {
    int save_errno = errno;

    ereport(LOG, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("out of file descriptors: %m; release and retry")));
    errno = 0;
    if (ReleaseLruFile())
    {
      goto TryAgain;
    }
    errno = save_errno;
  }

  return NULL;
}

   
                                                                    
   
                                                                        
                                                                          
                                                                          
                   
   
                             
                                                  
                     
                  
   
                                                                         
                                                                         
                       
   
                                                                          
                                            
   
struct dirent *
ReadDir(DIR *dir, const char *dirname)
{
  return ReadDirExtended(dir, dirname, ERROR);
}

   
                                                                         
                                                                      
                                                        
   
                                                                            
                                                                       
                                                     
   
struct dirent *
ReadDirExtended(DIR *dir, const char *dirname, int elevel)
{
  struct dirent *dent;

                                                                        
  if (dir == NULL)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", dirname)));
    return NULL;
  }

  errno = 0;
  if ((dent = readdir(dir)) != NULL)
  {
    return dent;
  }

  if (errno)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not read directory \"%s\": %m", dirname)));
  }
  return NULL;
}

   
                                              
   
                                                                   
                                                                    
                                     
   
                                                                          
                                
   
int
FreeDir(DIR *dir)
{
  int i;

                                           
  if (dir == NULL)
  {
    return 0;
  }

  DO_DB(elog(LOG, "FreeDir: Allocated %d", numAllocatedDescs));

                                                               
  for (i = numAllocatedDescs; --i >= 0;)
  {
    AllocateDesc *desc = &allocatedDescs[i];

    if (desc->kind == AllocateDescDir && desc->desc.dir == dir)
    {
      return FreeDesc(desc);
    }
  }

                                                                      
  elog(WARNING, "dir passed to FreeDir was not obtained from AllocateDir");

  return closedir(dir);
}

   
                                                   
   
int
ClosePipeStream(FILE *file)
{
  int i;

  DO_DB(elog(LOG, "ClosePipeStream: Allocated %d", numAllocatedDescs));

                                                                 
  for (i = numAllocatedDescs; --i >= 0;)
  {
    AllocateDesc *desc = &allocatedDescs[i];

    if (desc->kind == AllocateDescPipe && desc->desc.file == file)
    {
      return FreeDesc(desc);
    }
  }

                                                                       
  elog(WARNING, "file passed to ClosePipeStream was not obtained from OpenPipeStream");

  return pclose(file);
}

   
                
   
                                                                       
                                                                       
                                            
   
void
closeAllVfds(void)
{
  Index i;

  if (SizeVfdCache > 0)
  {
    Assert(FileIsNotOpen(0));                                   
    for (i = 1; i < SizeVfdCache; i++)
    {
      if (!FileIsNotOpen(i))
      {
        LruDelete(i);
      }
    }
  }
}

   
                      
   
                                                                       
                                                                      
                                                                     
                                                                            
                                                
   
                                                                            
                                                 
   
void
SetTempTablespaces(Oid *tableSpaces, int numSpaces)
{
  Assert(numSpaces >= 0);
  tempTableSpaces = tableSpaces;
  numTempTableSpaces = numSpaces;

     
                                                                      
                                                                           
                                                                             
                                                                          
                                                                          
                            
     
  if (numSpaces > 1)
  {
    nextTempTableSpace = random() % numSpaces;
  }
  else
  {
    nextTempTableSpace = 0;
  }
}

   
                         
   
                                                                              
                                                                            
           
   
bool
TempTablespacesAreSet(void)
{
  return (numTempTableSpaces >= 0);
}

   
                      
   
                                                                              
                                                                          
                                                                             
                           
                                                                      
   
int
GetTempTablespaces(Oid *tableSpaces, int numSpaces)
{
  int i;

  Assert(TempTablespacesAreSet());
  for (i = 0; i < numTempTableSpaces && i < numSpaces; ++i)
  {
    tableSpaces[i] = tempTableSpaces[i];
  }

  return i;
}

   
                         
   
                                                                         
                                                     
   
Oid
GetNextTempTableSpace(void)
{
  if (numTempTableSpaces > 0)
  {
                                                            
    if (++nextTempTableSpace >= numTempTableSpaces)
    {
      nextTempTableSpace = 0;
    }
    return tempTableSpaces[nextTempTableSpace];
  }
  return InvalidOid;
}

   
                     
   
                                                                            
                                                                        
                                                        
   
void
AtEOSubXact_Files(bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid)
{
  Index i;

  for (i = 0; i < numAllocatedDescs; i++)
  {
    if (allocatedDescs[i].create_subid == mySubid)
    {
      if (isCommit)
      {
        allocatedDescs[i].create_subid = parentSubid;
      }
      else
      {
                                                            
        FreeDesc(&allocatedDescs[i--]);
      }
    }
  }
}

   
                  
   
                                                                              
                                                                         
                                                                               
                                                                               
                                                                      
   
                                                                           
                   
   
void
AtEOXact_Files(bool isCommit)
{
  CleanupTempFiles(isCommit, false);
  tempTableSpaces = NULL;
  numTempTableSpaces = -1;
}

   
                    
   
                                                                     
                                                                        
   
static void
AtProcExit_Files(int code, Datum arg)
{
  CleanupTempFiles(false, true);
}

   
                                                            
   
                                                                      
                                                       
   
                                                                       
                                                                         
                                                                         
                                                                         
                                                        
   
static void
CleanupTempFiles(bool isCommit, bool isProcExit)
{
  Index i;

     
                                                                
                           
     
  if (isProcExit || have_xact_temporary_files)
  {
    Assert(FileIsNotOpen(0));                                   
    for (i = 1; i < SizeVfdCache; i++)
    {
      unsigned short fdstate = VfdCache[i].fdstate;

      if (((fdstate & FD_DELETE_AT_CLOSE) || (fdstate & FD_CLOSE_AT_EOXACT)) && VfdCache[i].fileName != NULL)
      {
           
                                                                       
                                                                      
                                                                      
                                                                  
                                  
           
        if (isProcExit)
        {
          FileClose(i);
        }
        else if (fdstate & FD_CLOSE_AT_EOXACT)
        {
          elog(WARNING, "temporary file %s not closed at end-of-transaction", VfdCache[i].fileName);
          FileClose(i);
        }
      }
    }

    have_xact_temporary_files = false;
  }

                                                              
  if (isCommit && numAllocatedDescs > 0)
  {
    elog(WARNING, "%d temporary files and directories not closed at end-of-transaction", numAllocatedDescs);
  }

                                                       
  while (numAllocatedDescs > 0)
  {
    FreeDesc(&allocatedDescs[0]);
  }
}

   
                                                                        
                      
   
                                                                      
                                                                           
                                                 
   
                                                                            
                                                                               
                                                                       
                                                                          
              
   
                                                                             
                                                                             
                                                                  
   
void
RemovePgTempFiles(void)
{
  char temp_path[MAXPGPATH + 10 + sizeof(TABLESPACE_VERSION_DIRECTORY) + sizeof(PG_TEMP_FILES_DIR)];
  DIR *spc_dir;
  struct dirent *spc_de;

     
                                                           
     
  snprintf(temp_path, sizeof(temp_path), "base/%s", PG_TEMP_FILES_DIR);
  RemovePgTempFilesInDir(temp_path, true, false);
  RemovePgTempRelationFiles("base");

     
                                                                     
     
  spc_dir = AllocateDir("pg_tblspc");

  while ((spc_de = ReadDirExtended(spc_dir, "pg_tblspc", LOG)) != NULL)
  {
    if (strcmp(spc_de->d_name, ".") == 0 || strcmp(spc_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(temp_path, sizeof(temp_path), "pg_tblspc/%s/%s/%s", spc_de->d_name, TABLESPACE_VERSION_DIRECTORY, PG_TEMP_FILES_DIR);
    RemovePgTempFilesInDir(temp_path, true, false);

    snprintf(temp_path, sizeof(temp_path), "pg_tblspc/%s/%s", spc_de->d_name, TABLESPACE_VERSION_DIRECTORY);
    RemovePgTempRelationFiles(temp_path);
  }

  FreeDir(spc_dir);

     
                                                                             
                      
     
#ifdef EXEC_BACKEND
  RemovePgTempFilesInDir(PG_TEMP_FILES_DIR, true, false);
#endif
}

   
                                                          
   
                                                                               
                                                                              
                                                                             
   
                                                                            
                                                                         
                                                                           
                                          
   
                                                                           
                   
   
static void
RemovePgTempFilesInDir(const char *tmpdirname, bool missing_ok, bool unlink_all)
{
  DIR *temp_dir;
  struct dirent *temp_de;
  char rm_path[MAXPGPATH * 2];

  temp_dir = AllocateDir(tmpdirname);

  if (temp_dir == NULL && errno == ENOENT && missing_ok)
  {
    return;
  }

  while ((temp_de = ReadDirExtended(temp_dir, tmpdirname, LOG)) != NULL)
  {
    if (strcmp(temp_de->d_name, ".") == 0 || strcmp(temp_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(rm_path, sizeof(rm_path), "%s/%s", tmpdirname, temp_de->d_name);

    if (unlink_all || strncmp(temp_de->d_name, PG_TEMP_FILE_PREFIX, strlen(PG_TEMP_FILE_PREFIX)) == 0)
    {
      struct stat statbuf;

      if (lstat(rm_path, &statbuf) < 0)
      {
        ereport(LOG, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", rm_path)));
        continue;
      }

      if (S_ISDIR(statbuf.st_mode))
      {
                                                                
        RemovePgTempFilesInDir(rm_path, false, true);

        if (rmdir(rm_path) < 0)
        {
          ereport(LOG, (errcode_for_file_access(), errmsg("could not remove directory \"%s\": %m", rm_path)));
        }
      }
      else
      {
        if (unlink(rm_path) < 0)
        {
          ereport(LOG, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", rm_path)));
        }
      }
    }
    else
    {
      ereport(LOG, (errmsg("unexpected file found in temporary-files directory: \"%s\"", rm_path)));
    }
  }

  FreeDir(temp_dir);
}

                                                                      
static void
RemovePgTempRelationFiles(const char *tsdirname)
{
  DIR *ts_dir;
  struct dirent *de;
  char dbspace_path[MAXPGPATH * 2];

  ts_dir = AllocateDir(tsdirname);

  while ((de = ReadDirExtended(ts_dir, tsdirname, LOG)) != NULL)
  {
       
                                                                         
                                                                           
                 
       
    if (strspn(de->d_name, "0123456789") != strlen(de->d_name))
    {
      continue;
    }

    snprintf(dbspace_path, sizeof(dbspace_path), "%s/%s", tsdirname, de->d_name);
    RemovePgTempRelationFilesInDbspace(dbspace_path);
  }

  FreeDir(ts_dir);
}

                                                                     
static void
RemovePgTempRelationFilesInDbspace(const char *dbspacedirname)
{
  DIR *dbspace_dir;
  struct dirent *de;
  char rm_path[MAXPGPATH * 2];

  dbspace_dir = AllocateDir(dbspacedirname);

  while ((de = ReadDirExtended(dbspace_dir, dbspacedirname, LOG)) != NULL)
  {
    if (!looks_like_temp_rel_name(de->d_name))
    {
      continue;
    }

    snprintf(rm_path, sizeof(rm_path), "%s/%s", dbspacedirname, de->d_name);

    if (unlink(rm_path) < 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", rm_path)));
    }
  }

  FreeDir(dbspace_dir);
}

                                                          
bool
looks_like_temp_rel_name(const char *name)
{
  int pos;
  int savepos;

                            
  if (name[0] != 't')
  {
    return false;
  }

                                                                        
  for (pos = 1; isdigit((unsigned char)name[pos]); ++pos)
    ;
  if (pos == 1 || name[pos] != '_')
  {
    return false;
  }

                                                      
  for (savepos = ++pos; isdigit((unsigned char)name[pos]); ++pos)
    ;
  if (savepos == pos)
  {
    return false;
  }

                                                    
  if (name[pos] == '_')
  {
    int forkchar = forkname_chars(&name[pos + 1], NULL);

    if (forkchar <= 0)
    {
      return false;
    }
    pos += forkchar + 1;
  }
  if (name[pos] == '.')
  {
    int segchar;

    for (segchar = 1; isdigit((unsigned char)name[pos + segchar]); ++segchar)
      ;
    if (segchar <= 1)
    {
      return false;
    }
    pos += segchar;
  }

                                    
  if (name[pos] != '\0')
  {
    return false;
  }
  return true;
}

   
                                                           
   
                                                                    
                                                                    
                                                                       
                                                                
   
                                                                           
                                                                             
                                                                              
                                                                             
                                                                         
                                                                            
                    
   
                                                                          
                                                
   
                                                           
   
void
SyncDataDirectory(void)
{
  bool xlog_is_symlink;

                                                          
  if (!enableFsync)
  {
    return;
  }

     
                                                                       
                                                     
     
  xlog_is_symlink = false;

#ifndef WIN32
  {
    struct stat st;

    if (lstat("pg_wal", &st) < 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", "pg_wal")));
    }
    else if (S_ISLNK(st.st_mode))
    {
      xlog_is_symlink = true;
    }
  }
#else
  if (pgwin32_is_junction("pg_wal"))
  {
    xlog_is_symlink = true;
  }
#endif

     
                                                                             
                                                                    
                                                          
     
#ifdef PG_FLUSH_DATA_WORKS
  walkdir(".", pre_sync_fname, false, DEBUG1);
  if (xlog_is_symlink)
  {
    walkdir("pg_wal", pre_sync_fname, false, DEBUG1);
  }
  walkdir("pg_tblspc", pre_sync_fname, true, DEBUG1);
#endif

     
                                               
     
                                                                            
                                                                           
                                                                            
                                                                           
                                            
     
  walkdir(".", datadir_fsync_fname, false, LOG);
  if (xlog_is_symlink)
  {
    walkdir("pg_wal", datadir_fsync_fname, false, LOG);
  }
  walkdir("pg_tblspc", datadir_fsync_fname, true, LOG);
}

   
                                                                      
                                                                      
   
                                                                          
                                                                           
                                                                         
                                                                      
                                             
   
                                                                      
   
                                                                            
   
static void
walkdir(const char *path, void (*action)(const char *fname, bool isdir, int elevel), bool process_symlinks, int elevel)
{
  DIR *dir;
  struct dirent *de;

  dir = AllocateDir(path);

  while ((de = ReadDirExtended(dir, path, elevel)) != NULL)
  {
    char subpath[MAXPGPATH * 2];
    struct stat fst;
    int sret;

    CHECK_FOR_INTERRUPTS();

    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(subpath, sizeof(subpath), "%s/%s", path, de->d_name);

    if (process_symlinks)
    {
      sret = stat(subpath, &fst);
    }
    else
    {
      sret = lstat(subpath, &fst);
    }

    if (sret < 0)
    {
      ereport(elevel, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", subpath)));
      continue;
    }

    if (S_ISREG(fst.st_mode))
    {
      (*action)(subpath, false, elevel);
    }
    else if (S_ISDIR(fst.st_mode))
    {
      walkdir(subpath, action, false, elevel);
    }
  }

  FreeDir(dir);                               

     
                                                                            
                                                                          
                                                                            
                                       
     
  if (dir)
  {
    (*action)(path, true, elevel);
  }
}

   
                                                                 
   
                                                                              
                           
   
#ifdef PG_FLUSH_DATA_WORKS

static void
pre_sync_fname(const char *fname, bool isdir, int elevel)
{
  int fd;

                                                              
  if (isdir)
  {
    return;
  }

  fd = OpenTransientFile(fname, O_RDONLY | PG_BINARY);

  if (fd < 0)
  {
    if (errno == EACCES)
    {
      return;
    }
    ereport(elevel, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", fname)));
    return;
  }

     
                                                                        
           
     
  pg_flush_data(fd, 0, 0);

  if (CloseTransientFile(fd))
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", fname)));
  }
}

#endif                          

static void
datadir_fsync_fname(const char *fname, bool isdir, int elevel)
{
     
                                                                            
                                     
     
  fsync_fname_ext(fname, isdir, true, elevel);
}

static void
unlink_if_exists_fname(const char *fname, bool isdir, int elevel)
{
  if (isdir)
  {
    if (rmdir(fname) != 0 && errno != ENOENT)
    {
      ereport(elevel, (errcode_for_file_access(), errmsg("could not remove directory \"%s\": %m", fname)));
    }
  }
  else
  {
                                                            
    PathNameDeleteTemporaryFile(fname, false);
  }
}

   
                                                       
   
                                                                        
                                                         
   
                                                       
   
static int
fsync_fname_ext(const char *fname, bool isdir, bool ignore_perm, int elevel)
{
  int fd;
  int flags;
  int returncode;

     
                                                                       
                                                                             
                                                                             
                                                          
     
  flags = PG_BINARY;
  if (!isdir)
  {
    flags |= O_RDWR;
  }
  else
  {
    flags |= O_RDONLY;
  }

  fd = OpenTransientFile(fname, flags);

     
                                                                         
                                                                            
                                                         
     
  if (fd < 0 && isdir && (errno == EISDIR || errno == EACCES))
  {
    return 0;
  }
  else if (fd < 0 && ignore_perm && errno == EACCES)
  {
    return 0;
  }
  else if (fd < 0)
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", fname)));
    return -1;
  }

  returncode = pg_fsync(fd);

     
                                                                            
                                                     
     
  if (returncode != 0 && !(isdir && (errno == EBADF || errno == EINVAL)))
  {
    int save_errno;

                                                                    
    save_errno = errno;
    (void)CloseTransientFile(fd);
    errno = save_errno;

    ereport(elevel, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", fname)));
    return -1;
  }

  if (CloseTransientFile(fd))
  {
    ereport(elevel, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", fname)));
    return -1;
  }

  return 0;
}

   
                                                                     
   
                                                                         
                                 
   
static int
fsync_parent_path(const char *fname, int elevel)
{
  char parentpath[MAXPGPATH];

  strlcpy(parentpath, fname, MAXPGPATH);
  get_parent_directory(parentpath);

     
                                                                             
                                                                            
                        
     
  if (strlen(parentpath) == 0)
  {
    strlcpy(parentpath, ".", MAXPGPATH);
  }

  if (fsync_fname_ext(parentpath, true, false, elevel) != 0)
  {
    return -1;
  }

  return 0;
}

   
                                          
   
                                                                              
                                                                            
                                                                             
                                                                                 
                                                                                
                                          
   
                                                                             
                                        
   
                                                                             
                                                                              
                                                                            
                      
   
int
MakePGDirectory(const char *directoryName)
{
  return mkdir(directoryName, pg_dir_create_mode);
}

   
                                                                         
   
                                                                       
                                                                            
                                                                             
                                                                            
                                                                               
                                                                              
                                                                          
                                                                          
                                                                           
                                           
   
                                                                            
                                              
   
int
data_sync_elevel(int elevel)
{
  return data_sync_retry ? elevel : PANIC;
}
