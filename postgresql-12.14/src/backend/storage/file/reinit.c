                                                                            
   
            
                                            
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>

#include "common/relpath.h"
#include "storage/copydir.h"
#include "storage/fd.h"
#include "storage/reinit.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

static void
ResetUnloggedRelationsInTablespaceDir(const char *tsdirname, int op);
static void
ResetUnloggedRelationsInDbspaceDir(const char *dbspacedirname, int op);

typedef struct
{
  char oid[OIDCHARS + 1];
} unlogged_relation_entry;

   
                                                          
   
                                                                        
                                                                    
   
                                                                              
         
   
void
ResetUnloggedRelations(int op)
{
  char temp_path[MAXPGPATH + 10 + sizeof(TABLESPACE_VERSION_DIRECTORY)];
  DIR *spc_dir;
  struct dirent *spc_de;
  MemoryContext tmpctx, oldctx;

               
  elog(DEBUG1, "resetting unlogged relations: cleanup %d init %d", (op & UNLOGGED_RELATION_CLEANUP) != 0, (op & UNLOGGED_RELATION_INIT) != 0);

     
                                                                        
                                        
     
  tmpctx = AllocSetContextCreate(CurrentMemoryContext, "ResetUnloggedRelations", ALLOCSET_DEFAULT_SIZES);
  oldctx = MemoryContextSwitchTo(tmpctx);

     
                                                               
     
  ResetUnloggedRelationsInTablespaceDir("base", op);

     
                                                                
     
  spc_dir = AllocateDir("pg_tblspc");

  while ((spc_de = ReadDir(spc_dir, "pg_tblspc")) != NULL)
  {
    if (strcmp(spc_de->d_name, ".") == 0 || strcmp(spc_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(temp_path, sizeof(temp_path), "pg_tblspc/%s/%s", spc_de->d_name, TABLESPACE_VERSION_DIRECTORY);
    ResetUnloggedRelationsInTablespaceDir(temp_path, op);
  }

  FreeDir(spc_dir);

     
                             
     
  MemoryContextSwitchTo(oldctx);
  MemoryContextDelete(tmpctx);
}

   
                                                               
   
static void
ResetUnloggedRelationsInTablespaceDir(const char *tsdirname, int op)
{
  DIR *ts_dir;
  struct dirent *de;
  char dbspace_path[MAXPGPATH * 2];

  ts_dir = AllocateDir(tsdirname);

     
                                                                          
                                                                           
                                                                           
                                                                         
                                                                        
                                  
     
  if (ts_dir == NULL && errno == ENOENT)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", tsdirname)));
    return;
  }

  while ((de = ReadDir(ts_dir, tsdirname)) != NULL)
  {
       
                                                                         
                                                                           
                 
       
    if (strspn(de->d_name, "0123456789") != strlen(de->d_name))
    {
      continue;
    }

    snprintf(dbspace_path, sizeof(dbspace_path), "%s/%s", tsdirname, de->d_name);
    ResetUnloggedRelationsInDbspaceDir(dbspace_path, op);
  }

  FreeDir(ts_dir);
}

   
                                                                
   
static void
ResetUnloggedRelationsInDbspaceDir(const char *dbspacedirname, int op)
{
  DIR *dbspace_dir;
  struct dirent *de;
  char rm_path[MAXPGPATH * 2];

                                                   
  Assert((op & (UNLOGGED_RELATION_CLEANUP | UNLOGGED_RELATION_INIT)) != 0);

     
                                                                             
                                                                    
                                                        
     
  if ((op & UNLOGGED_RELATION_CLEANUP) != 0)
  {
    HTAB *hash;
    HASHCTL ctl;

       
                                                                           
                                                                          
                                                                        
                                                                     
               
       
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(unlogged_relation_entry);
    ctl.entrysize = sizeof(unlogged_relation_entry);
    hash = hash_create("unlogged hash", 32, &ctl, HASH_ELEM);

                             
    dbspace_dir = AllocateDir(dbspacedirname);
    while ((de = ReadDir(dbspace_dir, dbspacedirname)) != NULL)
    {
      ForkNumber forkNum;
      int oidchars;
      unlogged_relation_entry ent;

                                                                      
      if (!parse_filename_for_nontemp_relation(de->d_name, &oidchars, &forkNum))
      {
        continue;
      }

                                                      
      if (forkNum != INIT_FORKNUM)
      {
        continue;
      }

         
                                                                    
                        
         
      memset(ent.oid, 0, sizeof(ent.oid));
      memcpy(ent.oid, de->d_name, oidchars);
      hash_search(hash, &ent, HASH_ENTER, NULL);
    }

                                   
    FreeDir(dbspace_dir);

       
                                                                         
                            
       
    if (hash_get_num_entries(hash) == 0)
    {
      hash_destroy(hash);
      return;
    }

       
                                                                 
       
    dbspace_dir = AllocateDir(dbspacedirname);
    while ((de = ReadDir(dbspace_dir, dbspacedirname)) != NULL)
    {
      ForkNumber forkNum;
      int oidchars;
      bool found;
      unlogged_relation_entry ent;

                                                                      
      if (!parse_filename_for_nontemp_relation(de->d_name, &oidchars, &forkNum))
      {
        continue;
      }

                                          
      if (forkNum == INIT_FORKNUM)
      {
        continue;
      }

         
                                                                      
                
         
      memset(ent.oid, 0, sizeof(ent.oid));
      memcpy(ent.oid, de->d_name, oidchars);
      hash_search(hash, &ent, HASH_FIND, &found);

                           
      if (found)
      {
        snprintf(rm_path, sizeof(rm_path), "%s/%s", dbspacedirname, de->d_name);
        if (unlink(rm_path) < 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", rm_path)));
        }
        else
        {
          elog(DEBUG2, "unlinked file \"%s\"", rm_path);
        }
      }
    }

                              
    FreeDir(dbspace_dir);
    hash_destroy(hash);
  }

     
                                                                         
                                                                         
                                                                      
                                                                           
                                                 
     
  if ((op & UNLOGGED_RELATION_INIT) != 0)
  {
                             
    dbspace_dir = AllocateDir(dbspacedirname);
    while ((de = ReadDir(dbspace_dir, dbspacedirname)) != NULL)
    {
      ForkNumber forkNum;
      int oidchars;
      char oidbuf[OIDCHARS + 1];
      char srcpath[MAXPGPATH * 2];
      char dstpath[MAXPGPATH];

                                                                      
      if (!parse_filename_for_nontemp_relation(de->d_name, &oidchars, &forkNum))
      {
        continue;
      }

                                                      
      if (forkNum != INIT_FORKNUM)
      {
        continue;
      }

                                      
      snprintf(srcpath, sizeof(srcpath), "%s/%s", dbspacedirname, de->d_name);

                                           
      memcpy(oidbuf, de->d_name, oidchars);
      oidbuf[oidchars] = '\0';
      snprintf(dstpath, sizeof(dstpath), "%s/%s%s", dbspacedirname, oidbuf, de->d_name + oidchars + 1 + strlen(forkNames[INIT_FORKNUM]));

                                                       
      elog(DEBUG2, "copying %s to %s", srcpath, dstpath);
      copy_file(srcpath, dstpath);
    }

    FreeDir(dbspace_dir);

       
                                                                         
                                                                          
                                                                   
                                                                    
                                               
       
    dbspace_dir = AllocateDir(dbspacedirname);
    while ((de = ReadDir(dbspace_dir, dbspacedirname)) != NULL)
    {
      ForkNumber forkNum;
      int oidchars;
      char oidbuf[OIDCHARS + 1];
      char mainpath[MAXPGPATH];

                                                                      
      if (!parse_filename_for_nontemp_relation(de->d_name, &oidchars, &forkNum))
      {
        continue;
      }

                                                      
      if (forkNum != INIT_FORKNUM)
      {
        continue;
      }

                                         
      memcpy(oidbuf, de->d_name, oidchars);
      oidbuf[oidchars] = '\0';
      snprintf(mainpath, sizeof(mainpath), "%s/%s%s", dbspacedirname, oidbuf, de->d_name + oidchars + 1 + strlen(forkNames[INIT_FORKNUM]));

      fsync_fname(mainpath, false);
    }

    FreeDir(dbspace_dir);

       
                                                                 
                                                                         
                                                              
                                                                        
                                                                        
                                        
       
    fsync_fname(dbspacedirname, true);
  }
}

   
                                                 
   
                                                                              
                                                     
   
                                                                            
                                                                         
                                                                        
                                                                            
                   
   
bool
parse_filename_for_nontemp_relation(const char *name, int *oidchars, ForkNumber *fork)
{
  int pos;

                                                                    
  for (pos = 0; isdigit((unsigned char)name[pos]); ++pos)
    ;
  if (pos == 0 || pos > OIDCHARS)
  {
    return false;
  }
  *oidchars = pos;

                              
  if (name[pos] != '_')
  {
    *fork = MAIN_FORKNUM;
  }
  else
  {
    int forkchar;

    forkchar = forkname_chars(&name[pos + 1], fork);
    if (forkchar <= 0)
    {
      return false;
    }
    pos += forkchar + 1;
  }

                                   
  if (name[pos] == '.')
  {
    int segchar;

    for (segchar = 1; isdigit((unsigned char)name[pos + segchar]); ++segchar)
      ;
    if (segchar <= 1)
    {
      return false;
    }
    pos += segchar;
  }

                                    
  if (name[pos] != '\0')
  {
    return false;
  }
  return true;
}
