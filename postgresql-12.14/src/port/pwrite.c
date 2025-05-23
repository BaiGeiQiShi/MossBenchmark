                                                                            
   
            
                                                              
   
                                                                         
   
                  
                       
   
                                                                           
                                                       
   
                                                                            
   

#include "c.h"

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

ssize_t
pg_pwrite(int fd, const void *buf, size_t size, off_t offset)
{
#ifdef WIN32
  OVERLAPPED overlapped = {0};
  HANDLE handle;
  DWORD result;

  handle = (HANDLE)_get_osfhandle(fd);
  if (handle == INVALID_HANDLE_VALUE)
  {
    errno = EBADF;
    return -1;
  }

  overlapped.Offset = offset;
  if (!WriteFile(handle, buf, size, &result, &overlapped))
  {
    _dosmaperr(GetLastError());
    return -1;
  }

  return result;
#else
  if (lseek(fd, offset, SEEK_SET) < 0)
  {
    return -1;
  }

  return write(fd, buf, size);
#endif
}
