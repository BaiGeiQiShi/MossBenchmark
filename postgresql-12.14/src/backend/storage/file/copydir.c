                                                                            
   
             
                        
   
                                                                         
                                                                        
   
                                                                            
                                                                           
                 
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "storage/copydir.h"
#include "storage/fd.h"
#include "miscadmin.h"
#include "pgstat.h"

   
                             
   
                                                                         
                                             
   
void
copydir(char *fromdir, char *todir, bool recurse)
{
  DIR *xldir;
  struct dirent *xlde;
  char fromfile[MAXPGPATH * 2];
  char tofile[MAXPGPATH * 2];

  if (MakePGDirectory(todir) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create directory \"%s\": %m", todir)));
  }

  xldir = AllocateDir(fromdir);

  while ((xlde = ReadDir(xldir, fromdir)) != NULL)
  {
    struct stat fst;

                                                                          
    CHECK_FOR_INTERRUPTS();

    if (strcmp(xlde->d_name, ".") == 0 || strcmp(xlde->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(fromfile, sizeof(fromfile), "%s/%s", fromdir, xlde->d_name);
    snprintf(tofile, sizeof(tofile), "%s/%s", todir, xlde->d_name);

    if (lstat(fromfile, &fst) < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", fromfile)));
    }

    if (S_ISDIR(fst.st_mode))
    {
                                            
      if (recurse)
      {
        copydir(fromfile, tofile, true);
      }
    }
    else if (S_ISREG(fst.st_mode))
    {
      copy_file(fromfile, tofile);
    }
  }
  FreeDir(xldir);

     
                                                                             
                                           
     
  if (!enableFsync)
  {
    return;
  }

  xldir = AllocateDir(todir);

  while ((xlde = ReadDir(xldir, todir)) != NULL)
  {
    struct stat fst;

    if (strcmp(xlde->d_name, ".") == 0 || strcmp(xlde->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(tofile, sizeof(tofile), "%s/%s", todir, xlde->d_name);

       
                                                                     
                                            
       
    if (lstat(tofile, &fst) < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", tofile)));
    }

    if (S_ISREG(fst.st_mode))
    {
      fsync_fname(tofile, false);
    }
  }
  FreeDir(xldir);

     
                                                                            
                                                                          
                                                                         
                                                                
     
  fsync_fname(todir, true);
}

   
                 
   
void
copy_file(char *fromfile, char *tofile)
{
  char *buffer;
  int srcfd;
  int dstfd;
  int nbytes;
  off_t offset;
  off_t flush_offset;

                                                     
#define COPY_BUF_SIZE (8 * BLCKSZ)

     
                                                                            
                                                                          
                                                                             
                      
     
#if defined(__darwin__)
#define FLUSH_DISTANCE (32 * 1024 * 1024)
#else
#define FLUSH_DISTANCE (1024 * 1024)
#endif

                                                       
  buffer = palloc(COPY_BUF_SIZE);

     
                    
     
  srcfd = OpenTransientFile(fromfile, O_RDONLY | PG_BINARY);
  if (srcfd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", fromfile)));
  }

  dstfd = OpenTransientFile(tofile, O_RDWR | O_CREAT | O_EXCL | PG_BINARY);
  if (dstfd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tofile)));
  }

     
                          
     
  flush_offset = 0;
  for (offset = 0;; offset += nbytes)
  {
                                                                     
    CHECK_FOR_INTERRUPTS();

       
                                                                          
                                                                         
                                                      
       
    if (offset - flush_offset >= FLUSH_DISTANCE)
    {
      pg_flush_data(dstfd, flush_offset, offset - flush_offset);
      flush_offset = offset;
    }

    pgstat_report_wait_start(WAIT_EVENT_COPY_FILE_READ);
    nbytes = read(srcfd, buffer, COPY_BUF_SIZE);
    pgstat_report_wait_end();
    if (nbytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", fromfile)));
    }
    if (nbytes == 0)
    {
      break;
    }
    errno = 0;
    pgstat_report_wait_start(WAIT_EVENT_COPY_FILE_WRITE);
    if ((int)write(dstfd, buffer, nbytes) != nbytes)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tofile)));
    }
    pgstat_report_wait_end();
  }

  if (offset > flush_offset)
  {
    pg_flush_data(dstfd, flush_offset, offset - flush_offset);
  }

  if (CloseTransientFile(dstfd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tofile)));
  }

  if (CloseTransientFile(srcfd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", fromfile)));
  }

  pfree(buffer);
}
