                                                                            
   
                
                                         
   
                                                                      
                                                                         
           
   
                                                                         
                                                                      
                                                                    
                                                                    
   
                                                                       
                                                                       
                                                                            
                                                                     
                                              
                                                                   
        
                                                              
   
                                                                           
                                                                              
                                                                             
                                                 
                                
                                    
   
                                                                        
                                                                      
                                                                        
                                                                          
                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "access/heapam.h"
#include "access/reloptions.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_tablespace.h"
#include "commands/comment.h"
#include "commands/seclabel.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "common/file_perm.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/standby.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/varlena.h"

                   
char *default_tablespace = NULL;
char *temp_tablespaces = NULL;
bool allow_in_place_tablespaces = false;

static void
create_tablespace_directories(const char *location, const Oid tablespaceoid);
static bool
destroy_tablespace_directories(Oid tablespaceoid, bool redo);

   
                                                                         
                                                                          
                                                                           
                                         
   
                                                                      
                                                                    
                                                                    
                                                                           
                                                                          
                                                                         
                                                            
   
                                                                         
                                                                          
   
void
TablespaceCreateDbspace(Oid spcNode, Oid dbNode, bool isRedo)
{
  struct stat st;
  char *dir;

     
                                                                        
                           
     
  if (spcNode == GLOBALTABLESPACE_OID)
  {
    return;
  }

  Assert(OidIsValid(spcNode));
  Assert(OidIsValid(dbNode));

  dir = GetDatabasePath(dbNode, spcNode);

  if (stat(dir, &st) < 0)
  {
                                   
    if (errno == ENOENT)
    {
         
                                                                        
                                                             
         
      LWLockAcquire(TablespaceCreateLock, LW_EXCLUSIVE);

         
                                                                       
                           
         
      if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode))
      {
                                   
      }
      else
      {
                                        
        if (MakePGDirectory(dir) < 0)
        {
                                                                   
          if (errno != ENOENT || !isRedo)
          {
            ereport(ERROR, (errcode_for_file_access(), errmsg("could not create directory \"%s\": %m", dir)));
          }

             
                                                                     
                                                                   
                                                                  
                                                                  
                                                                    
                                                                    
                                                                   
                       
             
          if (pg_mkdir_p(dir, pg_dir_create_mode) < 0)
          {
            ereport(ERROR, (errcode_for_file_access(), errmsg("could not create directory \"%s\": %m", dir)));
          }
        }
      }

      LWLockRelease(TablespaceCreateLock);
    }
    else
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat directory \"%s\": %m", dir)));
    }
  }
  else
  {
                                
    if (!S_ISDIR(st.st_mode))
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" exists but is not a directory", dir)));
    }
  }

  pfree(dir);
}

   
                        
   
                                                                                
                                                                           
                                             
   
Oid
CreateTableSpace(CreateTableSpaceStmt *stmt)
{
#ifdef HAVE_SYMLINK
  Relation rel;
  Datum values[Natts_pg_tablespace];
  bool nulls[Natts_pg_tablespace];
  HeapTuple tuple;
  Oid tablespaceoid;
  char *location;
  Oid ownerId;
  Datum newOptions;
  bool in_place;

                          
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create tablespace \"%s\"", stmt->tablespacename), errhint("Must be superuser to create a tablespace.")));
  }

                                                                 
  if (stmt->owner)
  {
    ownerId = get_rolespec_oid(stmt->owner, false);
  }
  else
  {
    ownerId = GetUserId();
  }

                                                                 
  location = pstrdup(stmt->location);
  canonicalize_path(location);

                                                              
  if (strchr(location, '\''))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("tablespace location cannot contain single quotes")));
  }

  in_place = allow_in_place_tablespaces && strlen(location) == 0;

     
                                         
     
                                                                         
                                                             
     
  if (!in_place && !is_absolute_path(location))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("tablespace location must be an absolute path")));
  }

     
                                                                             
                                                                    
                                                                             
            
     
  if (strlen(location) + 1 + strlen(TABLESPACE_VERSION_DIRECTORY) + 1 + OIDCHARS + 1 + OIDCHARS + 1 + FORKNAMECHARS + 1 + OIDCHARS > MAXPGPATH)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("tablespace location \"%s\" is too long", location)));
  }

                                                        
  if (path_is_prefix_of_path(DataDir, location))
  {
    ereport(WARNING, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("tablespace location should not be inside the data directory")));
  }

     
                                                                      
                                    
     
  if (!allowSystemTableMods && IsReservedName(stmt->tablespacename))
  {
    ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("unacceptable tablespace name \"%s\"", stmt->tablespacename), errdetail("The prefix \"pg_\" is reserved for system tablespaces.")));
  }

     
                                                                     
                                                    
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strncmp(stmt->tablespacename, "regress_", 8) != 0)
  {
    elog(WARNING, "tablespaces created by regression test cases should have names starting with \"regress_\"");
  }
#endif

     
                                                                        
                                                                        
               
     
  if (OidIsValid(get_tablespace_oid(stmt->tablespacename, true)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("tablespace \"%s\" already exists", stmt->tablespacename)));
  }

     
                                                                             
                                                                      
                                                         
     
  rel = table_open(TableSpaceRelationId, RowExclusiveLock);

  MemSet(nulls, false, sizeof(nulls));

  tablespaceoid = GetNewOidWithIndex(rel, TablespaceOidIndexId, Anum_pg_tablespace_oid);
  values[Anum_pg_tablespace_oid - 1] = ObjectIdGetDatum(tablespaceoid);
  values[Anum_pg_tablespace_spcname - 1] = DirectFunctionCall1(namein, CStringGetDatum(stmt->tablespacename));
  values[Anum_pg_tablespace_spcowner - 1] = ObjectIdGetDatum(ownerId);
  nulls[Anum_pg_tablespace_spcacl - 1] = true;

                                                     
  newOptions = transformRelOptions((Datum)0, stmt->options, NULL, NULL, false, false);
  (void)tablespace_reloptions(newOptions, true);
  if (newOptions != (Datum)0)
  {
    values[Anum_pg_tablespace_spcoptions - 1] = newOptions;
  }
  else
  {
    nulls[Anum_pg_tablespace_spcoptions - 1] = true;
  }

  tuple = heap_form_tuple(rel->rd_att, values, nulls);

  CatalogTupleInsert(rel, tuple);

  heap_freetuple(tuple);

                                  
  recordDependencyOnOwner(TableSpaceRelationId, tablespaceoid, ownerId);

                                             
  InvokeObjectPostCreateHook(TableSpaceRelationId, tablespaceoid, 0);

  create_tablespace_directories(location, tablespaceoid);

                                            
  {
    xl_tblspc_create_rec xlrec;

    xlrec.ts_id = tablespaceoid;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, offsetof(xl_tblspc_create_rec, ts_path));
    XLogRegisterData((char *)location, strlen(location) + 1);

    (void)XLogInsert(RM_TBLSPC_ID, XLOG_TBLSPC_CREATE);
  }

     
                                                                           
                                                                            
                                                                           
                               
     
  ForceSyncCommit();

  pfree(location);

                                                      
  table_close(rel, NoLock);

  return tablespaceoid;
#else                     
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tablespaces are not supported on this platform")));
  return InvalidOid;                          
#endif                   
}

   
                      
   
                                                     
   
void
DropTableSpace(DropTableSpaceStmt *stmt)
{
#ifdef HAVE_SYMLINK
  char *tablespacename = stmt->tablespacename;
  TableScanDesc scandesc;
  Relation rel;
  HeapTuple tuple;
  Form_pg_tablespace spcform;
  ScanKeyData entry[1];
  Oid tablespaceoid;
  char *detail;
  char *detail_log;

     
                           
     
  rel = table_open(TableSpaceRelationId, RowExclusiveLock);

  ScanKeyInit(&entry[0], Anum_pg_tablespace_spcname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(tablespacename));
  scandesc = table_beginscan_catalog(rel, 1, entry);
  tuple = heap_getnext(scandesc, ForwardScanDirection);

  if (!HeapTupleIsValid(tuple))
  {
    if (!stmt->missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace \"%s\" does not exist", tablespacename)));
    }
    else
    {
      ereport(NOTICE, (errmsg("tablespace \"%s\" does not exist, skipping", tablespacename)));
                                                                   
      table_endscan(scandesc);
      table_close(rel, NoLock);
    }
    return;
  }

  spcform = (Form_pg_tablespace)GETSTRUCT(tuple);
  tablespaceoid = spcform->oid;

                                
  if (!pg_tablespace_ownercheck(tablespaceoid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_TABLESPACE, tablespacename);
  }

                                                                    
  if (tablespaceoid == GLOBALTABLESPACE_OID || tablespaceoid == DEFAULTTABLESPACE_OID)
  {
    aclcheck_error(ACLCHECK_NO_PRIV, OBJECT_TABLESPACE, tablespacename);
  }

                                                                  
  if (checkSharedDependencies(TableSpaceRelationId, tablespaceoid, &detail, &detail_log))
  {
    ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("tablespace \"%s\" cannot be dropped because some objects depend on it", tablespacename), errdetail_internal("%s", detail), errdetail_log("%s", detail_log)));
  }

                                                  
  InvokeObjectDropHook(TableSpaceRelationId, tablespaceoid, 0);

     
                                                                           
     
  CatalogTupleDelete(rel, &tuple->t_self);

  table_endscan(scandesc);

     
                                                                
     
  DeleteSharedComments(tablespaceoid, TableSpaceRelationId);
  DeleteSharedSecurityLabel(tablespaceoid, TableSpaceRelationId);

     
                                 
     
  deleteSharedDependencyRecordsFor(TableSpaceRelationId, tablespaceoid, 0);

     
                                                                            
                              
     
  LWLockAcquire(TablespaceCreateLock, LW_EXCLUSIVE);

     
                                                
     
  if (!destroy_tablespace_directories(tablespaceoid, false))
  {
       
                                                                           
                                                                       
                                                                         
                                                                           
                                                                      
                                                                           
                                               
       
                                                                          
                                                                      
                                                                          
                                                                      
                                                                      
                                                               
       
    RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);
    if (!destroy_tablespace_directories(tablespaceoid, false))
    {
                                                             
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("tablespace \"%s\" is not empty", tablespacename)));
    }
  }

                                            
  {
    xl_tblspc_drop_rec xlrec;

    xlrec.ts_id = tablespaceoid;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, sizeof(xl_tblspc_drop_rec));

    (void)XLogInsert(RM_TBLSPC_ID, XLOG_TBLSPC_DROP);
  }

     
                                                                             
                                                                      
                                              
     

     
                                                                           
                                                                          
                                                                           
                               
     
  ForceSyncCommit();

     
                                          
     
  LWLockRelease(TablespaceCreateLock);

                                                      
  table_close(rel, NoLock);
#else                     
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tablespaces are not supported on this platform")));
#endif                   
}

   
                                 
   
                                                                          
                              
   
static void
create_tablespace_directories(const char *location, const Oid tablespaceoid)
{
  char *linkloc;
  char *location_with_version_dir;
  struct stat st;
  bool in_place;

  linkloc = psprintf("pg_tblspc/%u", tablespaceoid);

     
                                                                           
                                                                             
                                                       
     
  in_place = strlen(location) == 0;

  if (in_place)
  {
    if (MakePGDirectory(linkloc) < 0 && errno != EEXIST)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not create directory \"%s\": %m", linkloc)));
    }
  }

  location_with_version_dir = psprintf("%s/%s", in_place ? linkloc : location, TABLESPACE_VERSION_DIRECTORY);

     
                                                                             
                                                                             
                                                                    
                  
     
  if (!in_place && chmod(location, pg_dir_create_mode) != 0)
  {
    if (errno == ENOENT)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FILE), errmsg("directory \"%s\" does not exist", location),
                         InRecovery ? errhint("Create this directory for the tablespace before "
                                              "restarting the server.")
                                    : 0));
    }
    else
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not set permissions on directory \"%s\": %m", location)));
    }
  }

     
                                                                             
                                                                            
                                                                            
                                                                          
                           
     
  if (stat(location_with_version_dir, &st) < 0)
  {
    if (errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat directory \"%s\": %m", location_with_version_dir)));
    }
    else if (MakePGDirectory(location_with_version_dir) < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not create directory \"%s\": %m", location_with_version_dir)));
    }
  }
  else if (!S_ISDIR(st.st_mode))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" exists but is not a directory", location_with_version_dir)));
  }
  else if (!InRecovery)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("directory \"%s\" already in use as a tablespace", location_with_version_dir)));
  }

     
                                                                            
     
  if (!in_place && InRecovery)
  {
    remove_tablespace_symlink(linkloc);
  }

     
                                     
     
  if (!in_place && symlink(location, linkloc) < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create symbolic link \"%s\": %m", linkloc)));
  }

  pfree(linkloc);
  pfree(location_with_version_dir);
}

   
                                  
   
                                                                   
   
                                                                            
                                                                             
                                                                              
                                                                             
                                      
   
                                                                       
   
static bool
destroy_tablespace_directories(Oid tablespaceoid, bool redo)
{
  char *linkloc;
  char *linkloc_with_version_dir;
  DIR *dirdesc;
  struct dirent *de;
  char *subfile;
  struct stat st;

  linkloc_with_version_dir = psprintf("pg_tblspc/%u/%s", tablespaceoid, TABLESPACE_VERSION_DIRECTORY);

     
                                                                             
                                                                            
                                                                             
                                                                           
                                                                    
                                                
     
                                                                            
                                                                         
                                                                          
                                                                
     
                                                                           
                                                                          
                                                                           
                                                                           
                                                                           
                                                                        
                                                                        
                                        
     
  dirdesc = AllocateDir(linkloc_with_version_dir);
  if (dirdesc == NULL)
  {
    if (errno == ENOENT)
    {
      if (!redo)
      {
        ereport(WARNING, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", linkloc_with_version_dir)));
      }
                                                                 
      goto remove_symlink;
    }
    else if (redo)
    {
                                                  
      ereport(LOG, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", linkloc_with_version_dir)));
      pfree(linkloc_with_version_dir);
      return false;
    }
                                           
  }

  while ((de = ReadDir(dirdesc, linkloc_with_version_dir)) != NULL)
  {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
    {
      continue;
    }

    subfile = psprintf("%s/%s", linkloc_with_version_dir, de->d_name);

                                                                  
    if (!redo && !directory_is_empty(subfile))
    {
      FreeDir(dirdesc);
      pfree(subfile);
      pfree(linkloc_with_version_dir);
      return false;
    }

                                
    if (rmdir(subfile) < 0)
    {
      ereport(redo ? LOG : ERROR, (errcode_for_file_access(), errmsg("could not remove directory \"%s\": %m", subfile)));
    }

    pfree(subfile);
  }

  FreeDir(dirdesc);

                                
  if (rmdir(linkloc_with_version_dir) < 0)
  {
    ereport(redo ? LOG : ERROR, (errcode_for_file_access(), errmsg("could not remove directory \"%s\": %m", linkloc_with_version_dir)));
    pfree(linkloc_with_version_dir);
    return false;
  }

     
                                                                           
                                                                             
                                                                          
                                                           
     
                                                                         
                                                                           
                     
     
remove_symlink:
  linkloc = pstrdup(linkloc_with_version_dir);
  get_parent_directory(linkloc);
  if (lstat(linkloc, &st) < 0)
  {
    int saved_errno = errno;

    ereport(redo ? LOG : (saved_errno == ENOENT ? WARNING : ERROR), (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", linkloc)));
  }
  else if (S_ISDIR(st.st_mode))
  {
    if (rmdir(linkloc) < 0)
    {
      int saved_errno = errno;

      ereport(redo ? LOG : (saved_errno == ENOENT ? WARNING : ERROR), (errcode_for_file_access(), errmsg("could not remove directory \"%s\": %m", linkloc)));
    }
  }
#ifdef S_ISLNK
  else if (S_ISLNK(st.st_mode))
  {
    if (unlink(linkloc) < 0)
    {
      int saved_errno = errno;

      ereport(redo ? LOG : (saved_errno == ENOENT ? WARNING : ERROR), (errcode_for_file_access(), errmsg("could not remove symbolic link \"%s\": %m", linkloc)));
    }
  }
#endif
  else
  {
                                                                     
    ereport(redo ? LOG : ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("\"%s\" is not a directory or symbolic link", linkloc)));
  }

  pfree(linkloc_with_version_dir);
  pfree(linkloc);

  return true;
}

   
                                  
   
                                                               
   
bool
directory_is_empty(const char *path)
{
  DIR *dirdesc;
  struct dirent *de;

  dirdesc = AllocateDir(path);

  while ((de = ReadDir(dirdesc, path)) != NULL)
  {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
    {
      continue;
    }
    FreeDir(dirdesc);
    return false;
  }

  FreeDir(dirdesc);
  return true;
}

   
                             
   
                                                                             
                                                                          
                                                                          
                                                                              
                            
   
void
remove_tablespace_symlink(const char *linkloc)
{
  struct stat st;

  if (lstat(linkloc, &st) < 0)
  {
    if (errno == ENOENT)
    {
      return;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", linkloc)));
  }

  if (S_ISDIR(st.st_mode))
  {
       
                                                                      
                       
       
    if (rmdir(linkloc) < 0 && errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove directory \"%s\": %m", linkloc)));
    }
  }
#ifdef S_ISLNK
  else if (S_ISLNK(st.st_mode))
  {
    if (unlink(linkloc) < 0 && errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove symbolic link \"%s\": %m", linkloc)));
    }
  }
#endif
  else
  {
                                                                     
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("\"%s\" is not a directory or symbolic link", linkloc)));
  }
}

   
                       
   
ObjectAddress
RenameTableSpace(const char *oldname, const char *newname)
{
  Oid tspId;
  Relation rel;
  ScanKeyData entry[1];
  TableScanDesc scan;
  HeapTuple tup;
  HeapTuple newtuple;
  Form_pg_tablespace newform;
  ObjectAddress address;

                            
  rel = table_open(TableSpaceRelationId, RowExclusiveLock);

  ScanKeyInit(&entry[0], Anum_pg_tablespace_spcname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(oldname));
  scan = table_beginscan_catalog(rel, 1, entry);
  tup = heap_getnext(scan, ForwardScanDirection);
  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace \"%s\" does not exist", oldname)));
  }

  newtuple = heap_copytuple(tup);
  newform = (Form_pg_tablespace)GETSTRUCT(newtuple);
  tspId = newform->oid;

  table_endscan(scan);

                     
  if (!pg_tablespace_ownercheck(tspId, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NO_PRIV, OBJECT_TABLESPACE, oldname);
  }

                         
  if (!allowSystemTableMods && IsReservedName(newname))
  {
    ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("unacceptable tablespace name \"%s\"", newname), errdetail("The prefix \"pg_\" is reserved for system tablespaces.")));
  }

     
                                                                     
                                                    
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strncmp(newname, "regress_", 8) != 0)
  {
    elog(WARNING, "tablespaces created by regression test cases should have names starting with \"regress_\"");
  }
#endif

                                            
  ScanKeyInit(&entry[0], Anum_pg_tablespace_spcname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(newname));
  scan = table_beginscan_catalog(rel, 1, entry);
  tup = heap_getnext(scan, ForwardScanDirection);
  if (HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("tablespace \"%s\" already exists", newname)));
  }

  table_endscan(scan);

                            
  namestrcpy(&(newform->spcname), newname);

  CatalogTupleUpdate(rel, &newtuple->t_self, newtuple);

  InvokeObjectPostAlterHook(TableSpaceRelationId, tspId, 0);

  ObjectAddressSet(address, TableSpaceRelationId, tspId);

  table_close(rel, NoLock);

  return address;
}

   
                             
   
Oid
AlterTableSpaceOptions(AlterTableSpaceOptionsStmt *stmt)
{
  Relation rel;
  ScanKeyData entry[1];
  TableScanDesc scandesc;
  HeapTuple tup;
  Oid tablespaceoid;
  Datum datum;
  Datum newOptions;
  Datum repl_val[Natts_pg_tablespace];
  bool isnull;
  bool repl_null[Natts_pg_tablespace];
  bool repl_repl[Natts_pg_tablespace];
  HeapTuple newtuple;

                            
  rel = table_open(TableSpaceRelationId, RowExclusiveLock);

  ScanKeyInit(&entry[0], Anum_pg_tablespace_spcname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(stmt->tablespacename));
  scandesc = table_beginscan_catalog(rel, 1, entry);
  tup = heap_getnext(scandesc, ForwardScanDirection);
  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace \"%s\" does not exist", stmt->tablespacename)));
  }

  tablespaceoid = ((Form_pg_tablespace)GETSTRUCT(tup))->oid;

                                            
  if (!pg_tablespace_ownercheck(tablespaceoid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_TABLESPACE, stmt->tablespacename);
  }

                                                     
  datum = heap_getattr(tup, Anum_pg_tablespace_spcoptions, RelationGetDescr(rel), &isnull);
  newOptions = transformRelOptions(isnull ? (Datum)0 : datum, stmt->options, NULL, NULL, false, stmt->isReset);
  (void)tablespace_reloptions(newOptions, true);

                        
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));
  if (newOptions != (Datum)0)
  {
    repl_val[Anum_pg_tablespace_spcoptions - 1] = newOptions;
  }
  else
  {
    repl_null[Anum_pg_tablespace_spcoptions - 1] = true;
  }
  repl_repl[Anum_pg_tablespace_spcoptions - 1] = true;
  newtuple = heap_modify_tuple(tup, RelationGetDescr(rel), repl_val, repl_null, repl_repl);

                              
  CatalogTupleUpdate(rel, &newtuple->t_self, newtuple);

  InvokeObjectPostAlterHook(TableSpaceRelationId, tablespaceoid, 0);

  heap_freetuple(newtuple);

                           
  table_endscan(scandesc);
  table_close(rel, NoLock);

  return tablespaceoid;
}

   
                                                                
   

                                                 
bool
check_default_tablespace(char **newval, void **extra, GucSource source)
{
     
                                                                       
                                                                        
                                
     
  if (IsTransactionState() && MyDatabaseId != InvalidOid)
  {
    if (**newval != '\0' && !OidIsValid(get_tablespace_oid(*newval, true)))
    {
         
                                                                   
                                                                        
         
      if (source == PGC_S_TEST)
      {
        ereport(NOTICE, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace \"%s\" does not exist", *newval)));
      }
      else
      {
        GUC_check_errdetail("Tablespace \"%s\" does not exist.", *newval);
        return false;
      }
    }
  }

  return true;
}

   
                                                                         
   
                                                                   
                                                                              
                                                                              
        
   
                                                                              
   
                                                                         
                                 
   
                                                              
                                    
   
Oid
GetDefaultTablespace(char relpersistence, bool partitioned)
{
  Oid result;

                                                
  if (relpersistence == RELPERSISTENCE_TEMP)
  {
    PrepareTempTablespaces();
    return GetNextTempTableSpace();
  }

                                              
  if (default_tablespace == NULL || default_tablespace[0] == '\0')
  {
    return InvalidOid;
  }

     
                                                                           
                                                                            
                                                                            
                                                                             
                                                                        
     
  result = get_tablespace_oid(default_tablespace, true);

     
                                                                      
                                                                            
                                                                           
                          
     
  if (result == MyDatabaseTableSpace)
  {
    if (partitioned)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot specify default tablespace for partitioned relations")));
    }
    result = InvalidOid;
  }
  return result;
}

   
                                                              
   

typedef struct
{
                                                          
  int numSpcs;
  Oid tblSpcs[FLEXIBLE_ARRAY_MEMBER];
} temp_tablespaces_extra;

                                               
bool
check_temp_tablespaces(char **newval, void **extra, GucSource source)
{
  char *rawname;
  List *namelist;

                                        
  rawname = pstrdup(*newval);

                                             
  if (!SplitIdentifierString(rawname, ',', &namelist))
  {
                                   
    GUC_check_errdetail("List syntax is invalid.");
    pfree(rawname);
    list_free(namelist);
    return false;
  }

     
                                                                       
                                                                        
                                                                          
                            
     
  if (IsTransactionState() && MyDatabaseId != InvalidOid)
  {
    temp_tablespaces_extra *myextra;
    Oid *tblSpcs;
    int numSpcs;
    ListCell *l;

                                                                  
    tblSpcs = (Oid *)palloc(list_length(namelist) * sizeof(Oid));
    numSpcs = 0;
    foreach (l, namelist)
    {
      char *curname = (char *)lfirst(l);
      Oid curoid;
      AclResult aclresult;

                                                               
      if (curname[0] == '\0')
      {
                                                                
        tblSpcs[numSpcs++] = InvalidOid;
        continue;
      }

         
                                                                       
                                                              
                                                                        
         
      curoid = get_tablespace_oid(curname, source <= PGC_S_TEST);
      if (curoid == InvalidOid)
      {
        if (source == PGC_S_TEST)
        {
          ereport(NOTICE, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace \"%s\" does not exist", curname)));
        }
        continue;
      }

         
                                                                       
                                                                    
         
      if (curoid == MyDatabaseTableSpace)
      {
                                                                
        tblSpcs[numSpcs++] = InvalidOid;
        continue;
      }

                                                                        
      aclresult = pg_tablespace_aclcheck(curoid, GetUserId(), ACL_CREATE);
      if (aclresult != ACLCHECK_OK)
      {
        if (source >= PGC_S_INTERACTIVE)
        {
          aclcheck_error(aclresult, OBJECT_TABLESPACE, curname);
        }
        continue;
      }

      tblSpcs[numSpcs++] = curoid;
    }

                                                                   
    myextra = malloc(offsetof(temp_tablespaces_extra, tblSpcs) + numSpcs * sizeof(Oid));
    if (!myextra)
    {
      return false;
    }
    myextra->numSpcs = numSpcs;
    memcpy(myextra->tblSpcs, tblSpcs, numSpcs * sizeof(Oid));
    *extra = (void *)myextra;

    pfree(tblSpcs);
  }

  pfree(rawname);
  list_free(namelist);

  return true;
}

                                             
void
assign_temp_tablespaces(const char *newval, void *extra)
{
  temp_tablespaces_extra *myextra = (temp_tablespaces_extra *)extra;

     
                                                                            
                                                                         
                                                                             
                                                                             
                       
     
  if (myextra)
  {
    SetTempTablespaces(myextra->tblSpcs, myextra->numSpcs);
  }
  else
  {
    SetTempTablespaces(NULL, 0);
  }
}

   
                                                             
   
                                                                        
                                                                          
                   
   
void
PrepareTempTablespaces(void)
{
  char *rawname;
  List *namelist;
  Oid *tblSpcs;
  int numSpcs;
  ListCell *l;

                                                      
  if (TempTablespacesAreSet())
  {
    return;
  }

     
                                                                          
                                                                         
                                                                           
                                                                           
                                            
     
  if (!IsTransactionState())
  {
    return;
  }

                                        
  rawname = pstrdup(temp_tablespaces);

                                             
  if (!SplitIdentifierString(rawname, ',', &namelist))
  {
                                   
    SetTempTablespaces(NULL, 0);
    pfree(rawname);
    list_free(namelist);
    return;
  }

                                                                  
  tblSpcs = (Oid *)MemoryContextAlloc(TopTransactionContext, list_length(namelist) * sizeof(Oid));
  numSpcs = 0;
  foreach (l, namelist)
  {
    char *curname = (char *)lfirst(l);
    Oid curoid;
    AclResult aclresult;

                                                             
    if (curname[0] == '\0')
    {
                                                              
      tblSpcs[numSpcs++] = InvalidOid;
      continue;
    }

                                                          
    curoid = get_tablespace_oid(curname, true);
    if (curoid == InvalidOid)
    {
                                      
      continue;
    }

       
                                                                        
                                                               
       
    if (curoid == MyDatabaseTableSpace)
    {
                                                              
      tblSpcs[numSpcs++] = InvalidOid;
      continue;
    }

                                     
    aclresult = pg_tablespace_aclcheck(curoid, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      continue;
    }

    tblSpcs[numSpcs++] = curoid;
  }

  SetTempTablespaces(tblSpcs, numSpcs);

  pfree(rawname);
  list_free(namelist);
}

   
                                                                 
   
                                                                            
                                 
   
Oid
get_tablespace_oid(const char *tablespacename, bool missing_ok)
{
  Oid result;
  Relation rel;
  TableScanDesc scandesc;
  HeapTuple tuple;
  ScanKeyData entry[1];

     
                                                                           
                                                                            
                                                                  
     
  rel = table_open(TableSpaceRelationId, AccessShareLock);

  ScanKeyInit(&entry[0], Anum_pg_tablespace_spcname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(tablespacename));
  scandesc = table_beginscan_catalog(rel, 1, entry);
  tuple = heap_getnext(scandesc, ForwardScanDirection);

                                                              
  if (HeapTupleIsValid(tuple))
  {
    result = ((Form_pg_tablespace)GETSTRUCT(tuple))->oid;
  }
  else
  {
    result = InvalidOid;
  }

  table_endscan(scandesc);
  table_close(rel, AccessShareLock);

  if (!OidIsValid(result) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace \"%s\" does not exist", tablespacename)));
  }

  return result;
}

   
                                                                  
   
                                                             
   
char *
get_tablespace_name(Oid spc_oid)
{
  char *result;
  Relation rel;
  TableScanDesc scandesc;
  HeapTuple tuple;
  ScanKeyData entry[1];

     
                                                                           
                                                                             
                                                                
     
  rel = table_open(TableSpaceRelationId, AccessShareLock);

  ScanKeyInit(&entry[0], Anum_pg_tablespace_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(spc_oid));
  scandesc = table_beginscan_catalog(rel, 1, entry);
  tuple = heap_getnext(scandesc, ForwardScanDirection);

                                                              
  if (HeapTupleIsValid(tuple))
  {
    result = pstrdup(NameStr(((Form_pg_tablespace)GETSTRUCT(tuple))->spcname));
  }
  else
  {
    result = NULL;
  }

  table_endscan(scandesc);
  table_close(rel, AccessShareLock);

  return result;
}

   
                                          
   
void
tblspc_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                    
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_TBLSPC_CREATE)
  {
    xl_tblspc_create_rec *xlrec = (xl_tblspc_create_rec *)XLogRecGetData(record);
    char *location = xlrec->ts_path;

    create_tablespace_directories(location, xlrec->ts_id);
  }
  else if (info == XLOG_TBLSPC_DROP)
  {
    xl_tblspc_drop_rec *xlrec = (xl_tblspc_drop_rec *)XLogRecGetData(record);

       
                                                                       
                                                                           
                                                                
       
                                                                         
                                                                       
                                                                     
                
       
                                                                
                                                                           
                                                                           
                            
       
    if (!destroy_tablespace_directories(xlrec->ts_id, true))
    {
      ResolveRecoveryConflictWithTablespace(xlrec->ts_id);

         
                                                                       
                                                                        
                                                                         
                                                                     
                                                                       
                                                               
         
      if (!destroy_tablespace_directories(xlrec->ts_id, true))
      {
        ereport(LOG, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("directories for tablespace %u could not be removed", xlrec->ts_id), errhint("You can remove the directories manually if necessary.")));
      }
    }
  }
  else
  {
    elog(PANIC, "tblspc_redo: unknown op code %u", info);
  }
}
