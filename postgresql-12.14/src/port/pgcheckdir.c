                                                                            
   
                         
   
                                                                                
                                          
   
                                                                         
                                                                        
   
                                                                            
   

#include "c.h"

#include <dirent.h>

   
                                                          
   
            
                     
                          
                                              
                                           
                              
                                                                 
   
int
pg_check_dir(const char *dir)
{
  int result = 1;
  DIR *chkdir;
  struct dirent *file;
  bool dot_found = false;
  bool mount_found = false;
  int readdir_errno;

  chkdir = opendir(dir);
  if (chkdir == NULL)
  {
    return (errno == ENOENT) ? 0 : -1;
  }

  while (errno = 0, (file = readdir(chkdir)) != NULL)
  {
    if (strcmp(".", file->d_name) == 0 || strcmp("..", file->d_name) == 0)
    {
                                          
      continue;
    }
#ifndef WIN32
                              
    else if (file->d_name[0] == '.')
    {
      dot_found = true;
    }
                              
    else if (strcmp("lost+found", file->d_name) == 0)
    {
      mount_found = true;
    }
#endif
    else
    {
      result = 4;                
      break;
    }
  }

  if (errno)
  {
    result = -1;                              
  }

                                                                       
  readdir_errno = errno;
  if (closedir(chkdir))
  {
    result = -1;                               
  }
  else
  {
    errno = readdir_errno;
  }

                                                                  
  if (result == 1 && mount_found)
  {
    result = 3;
  }

                                                          
  if (result == 1 && dot_found)
  {
    result = 2;
  }

  return result;
}
