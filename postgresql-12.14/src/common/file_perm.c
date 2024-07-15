                                                                            
   
                                          
   
   
                                                                         
                                                                        
   
                          
   
                                                                            
   
#include "c.h"

#include "common/file_perm.h"

                                                                    
int pg_dir_create_mode = PG_DIR_MODE_OWNER;
int pg_file_create_mode = PG_FILE_MODE_OWNER;

   
                                                                               
                                                                                
   
int pg_mode_mask = PG_MODE_MASK_OWNER;

   
                                                                             
                                                                               
                                                                            
                                        
   
void
SetDataDirectoryCreatePerm(int dataDirMode)
{
                                                   
  if ((PG_DIR_MODE_GROUP & dataDirMode) == PG_DIR_MODE_GROUP)
  {
    pg_dir_create_mode = PG_DIR_MODE_GROUP;
    pg_file_create_mode = PG_FILE_MODE_GROUP;
    pg_mode_mask = PG_MODE_MASK_GROUP;
  }
                                    
  else
  {
    pg_dir_create_mode = PG_DIR_MODE_OWNER;
    pg_file_create_mode = PG_FILE_MODE_OWNER;
    pg_mode_mask = PG_MODE_MASK_OWNER;
  }
}

#ifdef FRONTEND

   
                                                                                
                                                                          
   
                                                                              
                      
   
                                                                                
                     
   
bool
GetDataDirectoryCreatePerm(const char *dataDir)
{
#if !defined(WIN32) && !defined(__CYGWIN__)
  struct stat statBuf;

     
                                                                           
                                                                             
                                               
     
  if (stat(dataDir, &statBuf) == -1)
  {
    return false;
  }

                       
  SetDataDirectoryCreatePerm(statBuf.st_mode);
  return true;
#else                                              
     
                                                                         
                         
     
  return true;
#endif
}

#endif               
