                                                                            
   
                                     
   
                                                
   
   
                                                                         
                                                                        
   
                           
   
                                                                            
   
#include "postgres_fe.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/file_utils.h"
#include "common/logging.h"

                                                                               
#if defined(HAVE_SYNC_FILE_RANGE)
#define PG_FLUSH_DATA_WORKS 1
#elif defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
#define PG_FLUSH_DATA_WORKS 1
#endif

   
                                                     
   
#define MINIMUM_VERSION_FOR_PG_WAL 100000

#ifdef PG_FLUSH_DATA_WORKS
static int
pre_sync_fname(const char *fname, bool isdir);
#endif
static void
walkdir(const char *path, int (*action)(const char *fname, bool isdir), bool process_symlinks);

   
                                                           
   
                                                                           
                                                                          
                                                                           
                                                            
   
                                                                    
   
                                                 
   
void
fsync_pgdata(const char *pg_data, int serverVersion)
{
  bool xlog_is_symlink;
  char pg_wal[MAXPGPATH];
  char pg_tblspc[MAXPGPATH];

                                                                
  snprintf(pg_wal, MAXPGPATH, "%s/%s", pg_data, serverVersion < MINIMUM_VERSION_FOR_PG_WAL ? "pg_xlog" : "pg_wal");
  snprintf(pg_tblspc, MAXPGPATH, "%s/pg_tblspc", pg_data);

     
                                                                       
                                                     
     
  xlog_is_symlink = false;

#ifndef WIN32
  {
    struct stat st;

    if (lstat(pg_wal, &st) < 0)
    {
      pg_log_error("could not stat file \"%s\": %m", pg_wal);
    }
    else if (S_ISLNK(st.st_mode))
    {
      xlog_is_symlink = true;
    }
  }
#else
  if (pgwin32_is_junction(pg_wal))
  {
    xlog_is_symlink = true;
  }
#endif

     
                                                                             
                                 
     
#ifdef PG_FLUSH_DATA_WORKS
  walkdir(pg_data, pre_sync_fname, false);
  if (xlog_is_symlink)
  {
    walkdir(pg_wal, pre_sync_fname, false);
  }
  walkdir(pg_tblspc, pre_sync_fname, true);
#endif

     
                                               
     
                                                                            
                                                                           
                                                                            
                                                                           
                                            
     
  walkdir(pg_data, fsync_fname, false);
  if (xlog_is_symlink)
  {
    walkdir(pg_wal, fsync_fname, false);
  }
  walkdir(pg_tblspc, fsync_fname, true);
}

   
                                                                        
   
                                                     
   
void
fsync_dir_recurse(const char *dir)
{
     
                                                                             
                                 
     
#ifdef PG_FLUSH_DATA_WORKS
  walkdir(dir, pre_sync_fname, false);
#endif

  walkdir(dir, fsync_fname, false);
}

   
                                                                      
                                                                      
   
                                                                          
                                                                           
                                                                         
                                                                      
                                             
   
                                                 
   
                                                                       
   
static void
walkdir(const char *path, int (*action)(const char *fname, bool isdir), bool process_symlinks)
{
  DIR *dir;
  struct dirent *de;

  dir = opendir(path);
  if (dir == NULL)
  {
    pg_log_error("could not open directory \"%s\": %m", path);
    return;
  }

  while (errno = 0, (de = readdir(dir)) != NULL)
  {
    char subpath[MAXPGPATH * 2];
    struct stat fst;
    int sret;

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
      pg_log_error("could not stat file \"%s\": %m", subpath);
      continue;
    }

    if (S_ISREG(fst.st_mode))
    {
      (*action)(subpath, false);
    }
    else if (S_ISDIR(fst.st_mode))
    {
      walkdir(subpath, action, false);
    }
  }

  if (errno)
  {
    pg_log_error("could not read directory \"%s\": %m", path);
  }

  (void)closedir(dir);

     
                                                                            
                                                                          
                                                                          
                                                                    
     
  (*action)(path, true);
}

   
                                                                 
   
                                                                            
                
   
#ifdef PG_FLUSH_DATA_WORKS

static int
pre_sync_fname(const char *fname, bool isdir)
{
  int fd;

  fd = open(fname, O_RDONLY | PG_BINARY, 0);

  if (fd < 0)
  {
    if (errno == EACCES || (isdir && errno == EISDIR))
    {
      return 0;
    }
    pg_log_error("could not open file \"%s\": %m", fname);
    return -1;
  }

     
                                                                       
                                                                        
                                  
     
#if defined(HAVE_SYNC_FILE_RANGE)
  (void)sync_file_range(fd, 0, 0, SYNC_FILE_RANGE_WRITE);
#elif defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
  (void)posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
#else
#error PG_FLUSH_DATA_WORKS should not have been defined
#endif

  (void)close(fd);
  return 0;
}

#endif                          

   
                                                   
   
                                                                      
                                                                      
                             
   
int
fsync_fname(const char *fname, bool isdir)
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

     
                                                                        
                                                                          
                     
     
  fd = open(fname, flags, 0);
  if (fd < 0)
  {
    if (errno == EACCES || (isdir && errno == EISDIR))
    {
      return 0;
    }
    pg_log_error("could not open file \"%s\": %m", fname);
    return -1;
  }

  returncode = fsync(fd);

     
                                                                            
                                                       
     
  if (returncode != 0 && !(isdir && (errno == EBADF || errno == EINVAL)))
  {
    pg_log_error("could not fsync file \"%s\": %m", fname);
    (void)close(fd);
    return -1;
  }

  (void)close(fd);
  return 0;
}

   
                                                                     
   
                                                                         
                                 
   
int
fsync_parent_path(const char *fname)
{
  char parentpath[MAXPGPATH];

  strlcpy(parentpath, fname, MAXPGPATH);
  get_parent_directory(parentpath);

     
                                                                             
                                                                            
                        
     
  if (strlen(parentpath) == 0)
  {
    strlcpy(parentpath, ".", MAXPGPATH);
  }

  if (fsync_fname(parentpath, true) != 0)
  {
    return -1;
  }

  return 0;
}

   
                                                                               
   
                                                          
   
int
durable_rename(const char *oldfile, const char *newfile)
{
  int fd;

     
                                                                             
                                                                     
                                                                         
                                                                           
                    
     
  if (fsync_fname(oldfile, false) != 0)
  {
    return -1;
  }

  fd = open(newfile, PG_BINARY | O_RDWR, 0);
  if (fd < 0)
  {
    if (errno != ENOENT)
    {
      pg_log_error("could not open file \"%s\": %m", newfile);
      return -1;
    }
  }
  else
  {
    if (fsync(fd) != 0)
    {
      pg_log_error("could not fsync file \"%s\": %m", newfile);
      close(fd);
      return -1;
    }
    close(fd);
  }

                                   
  if (rename(oldfile, newfile) != 0)
  {
    pg_log_error("could not rename file \"%s\" to \"%s\": %m", oldfile, newfile);
    return -1;
  }

     
                                                                           
                                             
     
  if (fsync_fname(newfile, false) != 0)
  {
    return -1;
  }

  if (fsync_parent_path(newfile) != 0)
  {
    return -1;
  }

  return 0;
}
