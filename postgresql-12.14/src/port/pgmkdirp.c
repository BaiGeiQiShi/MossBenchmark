   
                                                                     
                                   
   
                                  
                                                                      
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

#include "c.h"

#include <sys/stat.h>

   
                                                                           
   
                                                                           
                             
   
                                                                           
   
                                                                               
                                                                               
                                                                            
                                                                               
   
                                                         
   
                                                                               
                                         
   
int
pg_mkdir_p(char *path, int omode)
{
  struct stat sb;
  mode_t numask, oumask;
  int last, retval;
  char *p;

  retval = 0;
  p = path;

#ifdef WIN32
                                                   
  if (strlen(p) >= 2)
  {
    if (p[0] == '/' && p[1] == '/')
    {
                         
      p = strstr(p + 2, "/");
      if (p == NULL)
      {
        errno = EINVAL;
        return -1;
      }
    }
    else if (p[1] == ':' && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')))
    {
                       
      p += 2;
    }
  }
#endif

     
                                                                       
                                                                            
                  
     
                                                                        
     
                                                                      
                                                          
     
  oumask = umask(0);
  numask = oumask & ~(S_IWUSR | S_IXUSR);
  (void)umask(numask);

  if (p[0] == '/')                        
  {
    ++p;
  }
  for (last = 0; !last; ++p)
  {
    if (p[0] == '\0')
    {
      last = 1;
    }
    else if (p[0] != '/')
    {
      continue;
    }
    *p = '\0';
    if (!last && p[1] == '\0')
    {
      last = 1;
    }

    if (last)
    {
      (void)umask(oumask);
    }

                                          
    if (stat(path, &sb) == 0)
    {
      if (!S_ISDIR(sb.st_mode))
      {
        if (last)
        {
          errno = EEXIST;
        }
        else
        {
          errno = ENOTDIR;
        }
        retval = -1;
        break;
      }
    }
    else if (mkdir(path, last ? omode : S_IRWXU | S_IRWXG | S_IRWXO) < 0)
    {
      retval = -1;
      break;
    }
    if (!last)
    {
      *p = '/';
    }
  }

                                
  (void)umask(oumask);

  return retval;
}
