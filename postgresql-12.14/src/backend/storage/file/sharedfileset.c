                                                                            
   
                   
                                       
   
                                                                         
                                                                        
   
                  
                                              
   
                                                                          
                                                                             
                                                      
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_tablespace.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "storage/dsm.h"
#include "storage/sharedfileset.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

static void
SharedFileSetOnDetach(dsm_segment *segment, Datum datum);
static void
SharedFileSetPath(char *path, SharedFileSet *fileset, Oid tablespace);
static void
SharedFilePath(char *path, SharedFileSet *fileset, const char *name);
static Oid
ChooseTablespace(const SharedFileSet *fileset, const char *name);

   
                                                                           
                                                                      
                                                                          
                                                         
   
                                                                
                     
   
                                                                             
                                                   
   
void
SharedFileSetInit(SharedFileSet *fileset, dsm_segment *seg)
{
  static uint32 counter = 0;

  SpinLockInit(&fileset->mutex);
  fileset->refcnt = 1;
  fileset->creator_pid = MyProcPid;
  fileset->number = counter;
  counter = (counter + 1) % INT_MAX;

                                                                       
  PrepareTempTablespaces();
  fileset->ntablespaces = GetTempTablespaces(&fileset->tablespaces[0], lengthof(fileset->tablespaces));
  if (fileset->ntablespaces == 0)
  {
                                                                        
    fileset->tablespaces[0] = MyDatabaseTableSpace;
    fileset->ntablespaces = 1;
  }
  else
  {
    int i;

       
                                                                       
                                                                         
                                              
       
    for (i = 0; i < fileset->ntablespaces; i++)
    {
      if (fileset->tablespaces[i] == InvalidOid)
      {
        fileset->tablespaces[i] = MyDatabaseTableSpace;
      }
    }
  }

                                      
  on_dsm_detach(seg, SharedFileSetOnDetach, PointerGetDatum(fileset));
}

   
                                                                           
   
void
SharedFileSetAttach(SharedFileSet *fileset, dsm_segment *seg)
{
  bool success;

  SpinLockAcquire(&fileset->mutex);
  if (fileset->refcnt == 0)
  {
    success = false;
  }
  else
  {
    ++fileset->refcnt;
    success = true;
  }
  SpinLockRelease(&fileset->mutex);

  if (!success)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not attach to a SharedFileSet that is already destroyed")));
  }

                                      
  on_dsm_detach(seg, SharedFileSetOnDetach, PointerGetDatum(fileset));
}

   
                                       
   
File
SharedFileSetCreate(SharedFileSet *fileset, const char *name)
{
  char path[MAXPGPATH];
  File file;

  SharedFilePath(path, fileset, name);
  file = PathNameCreateTemporaryFile(path, false);

                                                                       
  if (file <= 0)
  {
    char tempdirpath[MAXPGPATH];
    char filesetpath[MAXPGPATH];
    Oid tablespace = ChooseTablespace(fileset, name);

    TempTablespacePath(tempdirpath, tablespace);
    SharedFileSetPath(filesetpath, fileset, tablespace);
    PathNameCreateTemporaryDir(tempdirpath, filesetpath);
    file = PathNameCreateTemporaryFile(path, true);
  }

  return file;
}

   
                                                                        
                    
   
File
SharedFileSetOpen(SharedFileSet *fileset, const char *name)
{
  char path[MAXPGPATH];
  File file;

  SharedFilePath(path, fileset, name);
  file = PathNameOpenTemporaryFile(path);

  return file;
}

   
                                                              
                                                     
   
bool
SharedFileSetDelete(SharedFileSet *fileset, const char *name, bool error_on_failure)
{
  char path[MAXPGPATH];

  SharedFilePath(path, fileset, name);

  return PathNameDeleteTemporaryFile(path, error_on_failure);
}

   
                                
   
void
SharedFileSetDeleteAll(SharedFileSet *fileset)
{
  char dirpath[MAXPGPATH];
  int i;

     
                                                                       
                                                                      
                          
     
  for (i = 0; i < fileset->ntablespaces; ++i)
  {
    SharedFileSetPath(dirpath, fileset, fileset->tablespaces[i]);
    PathNameDeleteTemporaryDir(dirpath);
  }
}

   
                                                                            
                                                                               
                                                                     
                                                                               
                           
   
static void
SharedFileSetOnDetach(dsm_segment *segment, Datum datum)
{
  bool unlink_all = false;
  SharedFileSet *fileset = (SharedFileSet *)DatumGetPointer(datum);

  SpinLockAcquire(&fileset->mutex);
  Assert(fileset->refcnt > 0);
  if (--fileset->refcnt == 0)
  {
    unlink_all = true;
  }
  SpinLockRelease(&fileset->mutex);

     
                                                                  
                                                                            
                                                     
     
  if (unlink_all)
  {
    SharedFileSetDeleteAll(fileset);
  }
}

   
                                                                              
                          
   
static void
SharedFileSetPath(char *path, SharedFileSet *fileset, Oid tablespace)
{
  char tempdirpath[MAXPGPATH];

  TempTablespacePath(tempdirpath, tablespace);
  snprintf(path, MAXPGPATH, "%s/%s%lu.%u.sharedfileset", tempdirpath, PG_TEMP_FILE_PREFIX, (unsigned long)fileset->creator_pid, fileset->number);
}

   
                                                                           
               
   
static Oid
ChooseTablespace(const SharedFileSet *fileset, const char *name)
{
  uint32 hash = hash_any((const unsigned char *)name, strlen(name));

  return fileset->tablespaces[hash % fileset->ntablespaces];
}

   
                                                       
   
static void
SharedFilePath(char *path, SharedFileSet *fileset, const char *name)
{
  char dirpath[MAXPGPATH];

  SharedFileSetPath(dirpath, fileset, ChooseTablespace(fileset, name));
  snprintf(path, MAXPGPATH, "%s/%s", dirpath, name);
}
