                                                                            
   
                
                                                         
   
                                                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "datapagemap.h"
#include "fetch.h"
#include "file_ops.h"
#include "filemap.h"
#include "pg_rewind.h"

static void
recurse_dir(const char *datadir, const char *path, process_file_callback_t callback);

static void
execute_pagemap(datapagemap_t *pagemap, const char *path);

   
                                                                      
                  
   
void
traverse_datadir(const char *datadir, process_file_callback_t callback)
{
  recurse_dir(datadir, NULL, callback);
}

   
                                      
   
                                                                      
                             
   
static void
recurse_dir(const char *datadir, const char *parentpath, process_file_callback_t callback)
{
  DIR *xldir;
  struct dirent *xlde;
  char fullparentpath[MAXPGPATH];

  if (parentpath)
  {
    snprintf(fullparentpath, MAXPGPATH, "%s/%s", datadir, parentpath);
  }
  else
  {
    snprintf(fullparentpath, MAXPGPATH, "%s", datadir);
  }

  xldir = opendir(fullparentpath);
  if (xldir == NULL)
  {
    pg_fatal("could not open directory \"%s\": %m", fullparentpath);
  }

  while (errno = 0, (xlde = readdir(xldir)) != NULL)
  {
    struct stat fst;
    char fullpath[MAXPGPATH * 2];
    char path[MAXPGPATH * 2];

    if (strcmp(xlde->d_name, ".") == 0 || strcmp(xlde->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(fullpath, sizeof(fullpath), "%s/%s", fullparentpath, xlde->d_name);

    if (lstat(fullpath, &fst) < 0)
    {
      if (errno == ENOENT)
      {
           
                                                                     
                                                                      
                                                                    
                                                             
           
                                                                  
           
      }
      else
      {
        pg_fatal("could not stat file \"%s\": %m", fullpath);
      }
    }

    if (parentpath)
    {
      snprintf(path, sizeof(path), "%s/%s", parentpath, xlde->d_name);
    }
    else
    {
      snprintf(path, sizeof(path), "%s", xlde->d_name);
    }

    if (S_ISREG(fst.st_mode))
    {
      callback(path, FILE_TYPE_REGULAR, fst.st_size, NULL);
    }
    else if (S_ISDIR(fst.st_mode))
    {
      callback(path, FILE_TYPE_DIRECTORY, 0, NULL);
                                            
      recurse_dir(datadir, path, callback);
    }
#ifndef WIN32
    else if (S_ISLNK(fst.st_mode))
#else
    else if (pgwin32_is_junction(fullpath))
#endif
    {
#if defined(HAVE_READLINK) || defined(WIN32)
      char link_target[MAXPGPATH];
      int len;

      len = readlink(fullpath, link_target, sizeof(link_target));
      if (len < 0)
      {
        pg_fatal("could not read symbolic link \"%s\": %m", fullpath);
      }
      if (len >= sizeof(link_target))
      {
        pg_fatal("symbolic link \"%s\" target is too long", fullpath);
      }
      link_target[len] = '\0';

      callback(path, FILE_TYPE_SYMLINK, 0, link_target);

         
                                                                         
                                                                      
                                                           
         
      if ((parentpath && strcmp(parentpath, "pg_tblspc") == 0) || strcmp(path, "pg_wal") == 0)
      {
        recurse_dir(datadir, path, callback);
      }
#else
      pg_fatal("\"%s\" is a symbolic link, but symbolic links are not supported on this platform", fullpath);
#endif                    
    }
  }

  if (errno)
  {
    pg_fatal("could not read directory \"%s\": %m", fullparentpath);
  }

  if (closedir(xldir))
  {
    pg_fatal("could not close directory \"%s\": %m", fullparentpath);
  }
}

   
                                                                         
   
                                                                          
   
static void
rewind_copy_file_range(const char *path, off_t begin, off_t end, bool trunc)
{
  PGAlignedBlock buf;
  char srcpath[MAXPGPATH];
  int srcfd;

  snprintf(srcpath, sizeof(srcpath), "%s/%s", datadir_source, path);

  srcfd = open(srcpath, O_RDONLY | PG_BINARY, 0);
  if (srcfd < 0)
  {
    pg_fatal("could not open source file \"%s\": %m", srcpath);
  }

  if (lseek(srcfd, begin, SEEK_SET) == -1)
  {
    pg_fatal("could not seek in source file: %m");
  }

  open_target_file(path, trunc);

  while (end - begin > 0)
  {
    int readlen;
    int len;

    if (end - begin > sizeof(buf))
    {
      len = sizeof(buf);
    }
    else
    {
      len = end - begin;
    }

    readlen = read(srcfd, buf.data, len);

    if (readlen < 0)
    {
      pg_fatal("could not read file \"%s\": %m", srcpath);
    }
    else if (readlen == 0)
    {
      pg_fatal("unexpected EOF while reading file \"%s\"", srcpath);
    }

    write_target_range(buf.data, begin, readlen);
    begin += readlen;
  }

  if (close(srcfd) != 0)
  {
    pg_fatal("could not close file \"%s\": %m", srcpath);
  }
}

   
                                                                             
                                          
   
void
copy_executeFileMap(filemap_t *map)
{
  file_entry_t *entry;
  int i;

  for (i = 0; i < map->narray; i++)
  {
    entry = map->array[i];
    execute_pagemap(&entry->pagemap, entry->path);

    switch (entry->action)
    {
    case FILE_ACTION_NONE:
                            
      break;

    case FILE_ACTION_COPY:
      rewind_copy_file_range(entry->path, 0, entry->newsize, true);
      break;

    case FILE_ACTION_TRUNCATE:
      truncate_target_file(entry->path, entry->newsize);
      break;

    case FILE_ACTION_COPY_TAIL:
      rewind_copy_file_range(entry->path, entry->oldsize, entry->newsize, false);
      break;

    case FILE_ACTION_CREATE:
      create_target(entry);
      break;

    case FILE_ACTION_REMOVE:
      remove_target(entry);
      break;
    }
  }

  close_target_file();
}

static void
execute_pagemap(datapagemap_t *pagemap, const char *path)
{
  datapagemap_iterator_t *iter;
  BlockNumber blkno;
  off_t offset;

  iter = datapagemap_iterate(pagemap);
  while (datapagemap_next(iter, &blkno))
  {
    offset = blkno * BLCKSZ;
    rewind_copy_file_range(path, offset, offset + BLCKSZ, false);
                                                                     
  }
  pg_free(iter);
}
