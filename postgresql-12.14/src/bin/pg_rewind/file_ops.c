                                                                            
   
              
                                              
   
                                                                          
                                                                          
                                                                           
                                                                   
   
                                                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/file_perm.h"
#include "file_ops.h"
#include "filemap.h"
#include "pg_rewind.h"

   
                               
   
static int dstfd = -1;
static char dstpath[MAXPGPATH] = "";

static void
create_target_dir(const char *path);
static void
remove_target_dir(const char *path);
static void
create_target_symlink(const char *path, const char *link);
static void
remove_target_symlink(const char *path);

   
                                                                           
                                 
   
void
open_target_file(const char *path, bool trunc)
{
  int mode;

  if (dry_run)
  {
    return;
  }

  if (dstfd != -1 && !trunc && strcmp(path, &dstpath[strlen(datadir_target) + 1]) == 0)
  {
    return;                   
  }

  close_target_file();

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);

  mode = O_WRONLY | O_CREAT | PG_BINARY;
  if (trunc)
  {
    mode |= O_TRUNC;
  }
  dstfd = open(dstpath, mode, pg_file_create_mode);
  if (dstfd < 0)
  {
    pg_fatal("could not open target file \"%s\": %m", dstpath);
  }
}

   
                                    
   
void
close_target_file(void)
{
  if (dstfd == -1)
  {
    return;
  }

  if (close(dstfd) != 0)
  {
    pg_fatal("could not close target file \"%s\": %m", dstpath);
  }

  dstfd = -1;
}

void
write_target_range(char *buf, off_t begin, size_t size)
{
  int writeleft;
  char *p;

                              
  fetch_done += size;
  progress_report(false);

  if (dry_run)
  {
    return;
  }

  if (lseek(dstfd, begin, SEEK_SET) == -1)
  {
    pg_fatal("could not seek in target file \"%s\": %m", dstpath);
  }

  writeleft = size;
  p = buf;
  while (writeleft > 0)
  {
    int writelen;

    errno = 0;
    writelen = write(dstfd, p, writeleft);
    if (writelen < 0)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      pg_fatal("could not write file \"%s\": %m", dstpath);
    }

    p += writelen;
    writeleft -= writelen;
  }

                                                                     
}

void
remove_target(file_entry_t *entry)
{
  Assert(entry->action == FILE_ACTION_REMOVE);

  switch (entry->type)
  {
  case FILE_TYPE_DIRECTORY:
    remove_target_dir(entry->path);
    break;

  case FILE_TYPE_REGULAR:
    remove_target_file(entry->path, false);
    break;

  case FILE_TYPE_SYMLINK:
    remove_target_symlink(entry->path);
    break;
  }
}

void
create_target(file_entry_t *entry)
{
  Assert(entry->action == FILE_ACTION_CREATE);

  switch (entry->type)
  {
  case FILE_TYPE_DIRECTORY:
    create_target_dir(entry->path);
    break;

  case FILE_TYPE_SYMLINK:
    create_target_symlink(entry->path, entry->link_target);
    break;

  case FILE_TYPE_REGULAR:
                                                                        
    pg_fatal("invalid action (CREATE) for regular file");
    break;
  }
}

   
                                                                        
                                             
   
void
remove_target_file(const char *path, bool missing_ok)
{
  char dstpath[MAXPGPATH];

  if (dry_run)
  {
    return;
  }

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);
  if (unlink(dstpath) != 0)
  {
    if (errno == ENOENT && missing_ok)
    {
      return;
    }

    pg_fatal("could not remove file \"%s\": %m", dstpath);
  }
}

void
truncate_target_file(const char *path, off_t newsize)
{
  char dstpath[MAXPGPATH];
  int fd;

  if (dry_run)
  {
    return;
  }

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);

  fd = open(dstpath, O_WRONLY, pg_file_create_mode);
  if (fd < 0)
  {
    pg_fatal("could not open file \"%s\" for truncation: %m", dstpath);
  }

  if (ftruncate(fd, newsize) != 0)
  {
    pg_fatal("could not truncate file \"%s\" to %u: %m", dstpath, (unsigned int)newsize);
  }

  close(fd);
}

static void
create_target_dir(const char *path)
{
  char dstpath[MAXPGPATH];

  if (dry_run)
  {
    return;
  }

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);
  if (mkdir(dstpath, pg_dir_create_mode) != 0)
  {
    pg_fatal("could not create directory \"%s\": %m", dstpath);
  }
}

static void
remove_target_dir(const char *path)
{
  char dstpath[MAXPGPATH];

  if (dry_run)
  {
    return;
  }

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);
  if (rmdir(dstpath) != 0)
  {
    pg_fatal("could not remove directory \"%s\": %m", dstpath);
  }
}

static void
create_target_symlink(const char *path, const char *link)
{
  char dstpath[MAXPGPATH];

  if (dry_run)
  {
    return;
  }

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);
  if (symlink(link, dstpath) != 0)
  {
    pg_fatal("could not create symbolic link at \"%s\": %m", dstpath);
  }
}

static void
remove_target_symlink(const char *path)
{
  char dstpath[MAXPGPATH];

  if (dry_run)
  {
    return;
  }

  snprintf(dstpath, sizeof(dstpath), "%s/%s", datadir_target, path);
  if (unlink(dstpath) != 0)
  {
    pg_fatal("could not remove symbolic link \"%s\": %m", dstpath);
  }
}

   
                                                                     
                                                                      
                                     
   
                                                                           
                                                                            
                                                                        
                                            
   
                                                                            
                                                         
   
char *
slurpFile(const char *datadir, const char *path, size_t *filesize)
{
  int fd;
  char *buffer;
  struct stat statbuf;
  char fullpath[MAXPGPATH];
  int len;
  int r;

  snprintf(fullpath, sizeof(fullpath), "%s/%s", datadir, path);

  if ((fd = open(fullpath, O_RDONLY | PG_BINARY, 0)) == -1)
  {
    pg_fatal("could not open file \"%s\" for reading: %m", fullpath);
  }

  if (fstat(fd, &statbuf) < 0)
  {
    pg_fatal("could not open file \"%s\" for reading: %m", fullpath);
  }

  len = statbuf.st_size;

  buffer = pg_malloc(len + 1);

  r = read(fd, buffer, len);
  if (r != len)
  {
    if (r < 0)
    {
      pg_fatal("could not read file \"%s\": %m", fullpath);
    }
    else
    {
      pg_fatal("could not read file \"%s\": read %d of %zu", fullpath, r, (Size)len);
    }
  }
  close(fd);

                                  
  buffer[len] = '\0';

  if (filesize)
  {
    *filesize = len;
  }
  return buffer;
}
