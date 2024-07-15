                                                                            
   
           
                                                                  
   
                                                                      
                                                                          
                                                                          
                                                                   
                              
   
   
                                                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <sys/stat.h>
#include <unistd.h>

#include "pg_rewind.h"
#include "fetch.h"
#include "file_ops.h"
#include "filemap.h"

void
fetchSourceFileList(void)
{
  if (datadir_source)
  {
    traverse_datadir(datadir_source, &process_source_file);
  }
  else
  {
    libpqProcessFileList();
  }
}

   
                                                                             
   
void
executeFileMap(void)
{
  if (datadir_source)
  {
    copy_executeFileMap(filemap);
  }
  else
  {
    libpq_executeFileMap(filemap);
  }
}

   
                                                                         
                                                                         
                         
   
char *
fetchFile(const char *filename, size_t *filesize)
{
  if (datadir_source)
  {
    return slurpFile(datadir_source, filename, filesize);
  }
  else
  {
    return libpqGetFile(filename, filesize);
  }
}
