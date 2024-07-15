                                                                            
   
            
   
                                                                         
                                                                        
   
                  
                         
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <unistd.h>
#include <sys/stat.h>

#ifndef FRONTEND
#define pg_log_warning(...) elog(WARNING, __VA_ARGS__)
#else
#include "common/logging.h"
#endif

   
          
   
                                        
                                             
                                  
                                                  
                                                               
                                                               
                                                           
   
bool
rmtree(const char *path, bool rmtopdir)
{
  bool result = true;
  char pathbuf[MAXPGPATH];
  char **filenames;
  char **filename;
  struct stat statbuf;

     
                                                                          
         
     
  filenames = pgfnames(path);

  if (filenames == NULL)
  {
    return false;
  }

                                                          
  for (filename = filenames; *filename; filename++)
  {
    snprintf(pathbuf, MAXPGPATH, "%s/%s", path, *filename);

       
                                                                       
                         
       
                                                                    
                                                                           
                                                             
                                                                         
                                                                           
                                                  
       
    if (lstat(pathbuf, &statbuf) != 0)
    {
      if (errno != ENOENT)
      {
        pg_log_warning("could not stat file or directory \"%s\": %m", pathbuf);
        result = false;
      }
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
                                                      
      if (!rmtree(pathbuf, true))
      {
                                           
        result = false;
      }
    }
    else
    {
      if (unlink(pathbuf) != 0)
      {
        if (errno != ENOENT)
        {
          pg_log_warning("could not remove file or directory \"%s\": %m", pathbuf);
          result = false;
        }
      }
    }
  }

  if (rmtopdir)
  {
    if (rmdir(path) != 0)
    {
      pg_log_warning("could not remove file or directory \"%s\": %m", path);
      result = false;
    }
  }

  pgfnames_cleanup(filenames);

  return result;
}
