                                                                            
   
          
                               
   
   
                                                                         
   
                   
   
                                                                            
   

#ifdef WIN32

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

static int
openFlagsToCreateFileFlags(int openFlags)
{
  switch (openFlags & (O_CREAT | O_TRUNC | O_EXCL))
  {
                                               
  case 0:
  case O_EXCL:
    return OPEN_EXISTING;

  case O_CREAT:
    return OPEN_ALWAYS;

                                               
  case O_TRUNC:
  case O_TRUNC | O_EXCL:
    return TRUNCATE_EXISTING;

  case O_CREAT | O_TRUNC:
    return CREATE_ALWAYS;

                                             
  case O_CREAT | O_EXCL:
  case O_CREAT | O_TRUNC | O_EXCL:
    return CREATE_NEW;
  }

                           
  return 0;
}

   
                                                 
   
int
pgwin32_open(const char *fileName, int fileFlags, ...)
{
  int fd;
  HANDLE h = INVALID_HANDLE_VALUE;
  SECURITY_ATTRIBUTES sa;
  int loops = 0;

                                            
  assert((fileFlags & ((O_RDONLY | O_WRONLY | O_RDWR) | O_APPEND | (O_RANDOM | O_SEQUENTIAL | O_TEMPORARY) | _O_SHORT_LIVED | O_DSYNC | O_DIRECT | (O_CREAT | O_TRUNC | O_EXCL) | (O_TEXT | O_BINARY))) == fileFlags);
#ifndef FRONTEND
  Assert(pgwin32_signal_event != NULL);                                  
#endif

#ifdef FRONTEND

     
                                                                       
                                                                           
                                                                         
                                                                           
                                                                           
                              
     
  if ((fileFlags & O_BINARY) == 0)
  {
    fileFlags |= O_TEXT;
  }
#endif

  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  while ((h = CreateFile(fileName,
                                                   
              (fileFlags & O_RDWR) ? (GENERIC_WRITE | GENERIC_READ) : ((fileFlags & O_WRONLY) ? GENERIC_WRITE : GENERIC_READ),
                                                              
              (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), &sa, openFlagsToCreateFileFlags(fileFlags), FILE_ATTRIBUTE_NORMAL | ((fileFlags & O_RANDOM) ? FILE_FLAG_RANDOM_ACCESS : 0) | ((fileFlags & O_SEQUENTIAL) ? FILE_FLAG_SEQUENTIAL_SCAN : 0) | ((fileFlags & _O_SHORT_LIVED) ? FILE_ATTRIBUTE_TEMPORARY : 0) | ((fileFlags & O_TEMPORARY) ? FILE_FLAG_DELETE_ON_CLOSE : 0) | ((fileFlags & O_DIRECT) ? FILE_FLAG_NO_BUFFERING : 0) | ((fileFlags & O_DSYNC) ? FILE_FLAG_WRITE_THROUGH : 0), NULL)) == INVALID_HANDLE_VALUE)
  {
       
                                                                         
                                                                        
                                          
       
    DWORD err = GetLastError();

    if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION)
    {
#ifndef FRONTEND
      if (loops == 50)
      {
        ereport(LOG, (errmsg("could not open file \"%s\": %s", fileName, (err == ERROR_SHARING_VIOLATION) ? _("sharing violation") : _("lock violation")), errdetail("Continuing to retry for 30 seconds."), errhint("You might have antivirus, backup, or similar software interfering with the database system.")));
      }
#endif

      if (loops < 300)
      {
        pg_usleep(100000);
        loops++;
        continue;
      }
    }

       
                                                                          
                                                                        
                                                                          
                                                                        
                                                                           
                                                                          
                                                                     
                                                                   
                                                                           
                                                                       
                                                                           
                                                                           
                                                                           
                                        
       
    if (err == ERROR_ACCESS_DENIED)
    {
      if (loops < 10)
      {
        struct stat st;

        if (stat(fileName, &st) != 0)
        {
          pg_usleep(100000);
          loops++;
          continue;
        }
      }
    }

    _dosmaperr(err);
    return -1;
  }

                                                             
  if ((fd = _open_osfhandle((intptr_t)h, fileFlags & O_APPEND)) < 0)
  {
    CloseHandle(h);                            
  }
  else if (fileFlags & (O_TEXT | O_BINARY) && _setmode(fd, fileFlags & (O_TEXT | O_BINARY)) < 0)
  {
    _close(fd);
    return -1;
  }

  return fd;
}

FILE *
pgwin32_fopen(const char *fileName, const char *mode)
{
  int openmode = 0;
  int fd;

  if (strstr(mode, "r+"))
  {
    openmode |= O_RDWR;
  }
  else if (strchr(mode, 'r'))
  {
    openmode |= O_RDONLY;
  }
  if (strstr(mode, "w+"))
  {
    openmode |= O_RDWR | O_CREAT | O_TRUNC;
  }
  else if (strchr(mode, 'w'))
  {
    openmode |= O_WRONLY | O_CREAT | O_TRUNC;
  }
  if (strchr(mode, 'a'))
  {
    openmode |= O_WRONLY | O_CREAT | O_APPEND;
  }

  if (strchr(mode, 'b'))
  {
    openmode |= O_BINARY;
  }
  if (strchr(mode, 't'))
  {
    openmode |= O_TEXT;
  }

  fd = pgwin32_open(fileName, openmode);
  if (fd == -1)
  {
    return NULL;
  }
  return _fdopen(fd, mode);
}

#endif
