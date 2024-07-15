                                                                            
   
            
                                        
   
                                                                         
                                                                        
   
   
                  
                       
   
                                                                            
   

   
                                                                   
                  
   
#ifdef __NetBSD__

#include "c.h"

#include <sys/stat.h>

   
                                                        
                                                                  
                                  
   

int
fseeko(FILE *stream, off_t offset, int whence)
{
  off_t floc;
  struct stat filestat;

  switch (whence)
  {
  case SEEK_CUR:
    if (fgetpos(stream, &floc) != 0)
    {
      goto failure;
    }
    floc += offset;
    if (fsetpos(stream, &floc) != 0)
    {
      goto failure;
    }
    return 0;
    break;
  case SEEK_SET:
    if (fsetpos(stream, &offset) != 0)
    {
      return -1;
    }
    return 0;
    break;
  case SEEK_END:
    fflush(stream);                                    
    if (fstat(fileno(stream), &filestat) != 0)
    {
      goto failure;
    }
    floc = filestat.st_size;
    floc += offset;
    if (fsetpos(stream, &floc) != 0)
    {
      goto failure;
    }
    return 0;
    break;
  default:
    errno = EINVAL;
    return -1;
  }

failure:
  return -1;
}

off_t
ftello(FILE *stream)
{
  off_t floc;

  if (fgetpos(stream, &floc) != 0)
  {
    return -1;
  }
  return floc;
}

#endif
