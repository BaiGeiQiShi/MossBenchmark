                                                                            
   
              
                                  
   
                                                                         
                                                                        
   
                  
                           
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <dirent.h>

#ifndef FRONTEND
#define pg_log_warning(...) elog(WARNING, __VA_ARGS__)
#else
#include "common/logging.h"
#endif

   
            
   
                                                                            
                                                                         
             
   
char **
pgfnames(const char *path)
{
  DIR *dir;
  struct dirent *file;
  char **filenames;
  int numnames = 0;
  int fnsize = 200;                                

  dir = opendir(path);
  if (dir == NULL)
  {
    pg_log_warning("could not open directory \"%s\": %m", path);
    return NULL;
  }

  filenames = (char **)palloc(fnsize * sizeof(char *));

  while (errno = 0, (file = readdir(dir)) != NULL)
  {
    if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0)
    {
      if (numnames + 1 >= fnsize)
      {
        fnsize *= 2;
        filenames = (char **)repalloc(filenames, fnsize * sizeof(char *));
      }
      filenames[numnames++] = pstrdup(file->d_name);
    }
  }

  if (errno)
  {
    pg_log_warning("could not read directory \"%s\": %m", path);
  }

  filenames[numnames] = NULL;

  if (closedir(dir))
  {
    pg_log_warning("could not close directory \"%s\": %m", path);
  }

  return filenames;
}

   
                    
   
                                        
   
void
pgfnames_cleanup(char **filenames)
{
  char **fn;

  for (fn = filenames; *fn; fn++)
  {
    pfree(*fn);
  }

  pfree(filenames);
}
