                                                                            
   
                
                                                         
   
                                                                       
                                                                      
                                                                      
                                                  
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_db_role_setting.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/dbcommands_xlog.h"
#include "commands/defrem.h"
#include "commands/seclabel.h"
#include "commands/tablespace.h"
#include "common/file_perm.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "replication/slot.h"
#include "storage/copydir.h"
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/ipc.h"
#include "storage/md.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_locale.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

typedef struct
{
  Oid src_dboid;                            
  Oid dest_dboid;                                 
} createdb_failure_params;

typedef struct
{
  Oid dest_dboid;                               
  Oid dest_tsoid;                                          
} movedb_failure_params;

                                    
static void
createdb_failure_callback(int code, Datum arg);
static void
movedb(const char *dbname, const char *tblspcname);
static void
movedb_failure_callback(int code, Datum arg);
static bool
get_db_info(const char *name, LOCKMODE lockmode, Oid *dbIdP, Oid *ownerIdP, int *encodingP, bool *dbIsTemplateP, bool *dbAllowConnP, Oid *dbLastSysOidP, TransactionId *dbFrozenXidP, MultiXactId *dbMinMultiP, Oid *dbTablespace, char **dbCollate, char **dbCtype);
static bool
have_createdb_privilege(void);
static void
remove_dbtablespaces(Oid db_id);
static bool
check_db_file_conflict(Oid db_id);
static int
errdetail_busy_db(int notherbackends, int npreparedxacts);

   
                   
   
Oid
createdb(ParseState *pstate, const CreatedbStmt *stmt)
{
  TableScanDesc scan;
  Relation rel;
  Oid src_dboid;
  Oid src_owner;
  int src_encoding;
  char *src_collate;
  char *src_ctype;
  bool src_istemplate;
  bool src_allowconn;
  Oid src_lastsysoid;
  TransactionId src_frozenxid;
  MultiXactId src_minmxid;
  Oid src_deftablespace;
  volatile Oid dst_deftablespace;
  Relation pg_database_rel;
  HeapTuple tuple;
  Datum new_record[Natts_pg_database];
  bool new_record_nulls[Natts_pg_database];
  Oid dboid;
  Oid datdba;
  ListCell *option;
  DefElem *dtablespacename = NULL;
  DefElem *downer = NULL;
  DefElem *dtemplate = NULL;
  DefElem *dencoding = NULL;
  DefElem *dcollate = NULL;
  DefElem *dctype = NULL;
  DefElem *distemplate = NULL;
  DefElem *dallowconnections = NULL;
  DefElem *dconnlimit = NULL;
  char *dbname = stmt->dbname;
  char *dbowner = NULL;
  const char *dbtemplate = NULL;
  char *dbcollate = NULL;
  char *dbctype = NULL;
  char *canonname;
  int encoding = -1;
  bool dbistemplate = false;
  bool dballowconnections = true;
  int dbconnlimit = -1;
  int notherbackends;
  int npreparedxacts;
  createdb_failure_params fparms;

                                                    
  foreach (option, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(option);

    if (strcmp(defel->defname, "tablespace") == 0)
    {
      if (dtablespacename)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dtablespacename = defel;
    }
    else if (strcmp(defel->defname, "owner") == 0)
    {
      if (downer)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      downer = defel;
    }
    else if (strcmp(defel->defname, "template") == 0)
    {
      if (dtemplate)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dtemplate = defel;
    }
    else if (strcmp(defel->defname, "encoding") == 0)
    {
      if (dencoding)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dencoding = defel;
    }
    else if (strcmp(defel->defname, "lc_collate") == 0)
    {
      if (dcollate)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dcollate = defel;
    }
    else if (strcmp(defel->defname, "lc_ctype") == 0)
    {
      if (dctype)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dctype = defel;
    }
    else if (strcmp(defel->defname, "is_template") == 0)
    {
      if (distemplate)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      distemplate = defel;
    }
    else if (strcmp(defel->defname, "allow_connections") == 0)
    {
      if (dallowconnections)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dallowconnections = defel;
    }
    else if (strcmp(defel->defname, "connection_limit") == 0)
    {
      if (dconnlimit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dconnlimit = defel;
    }
    else if (strcmp(defel->defname, "location") == 0)
    {
      ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("LOCATION is not supported anymore"), errhint("Consider using tablespaces instead."), parser_errposition(pstate, defel->location)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("option \"%s\" not recognized", defel->defname), parser_errposition(pstate, defel->location)));
    }
  }

  if (downer && downer->arg)
  {
    dbowner = defGetString(downer);
  }
  if (dtemplate && dtemplate->arg)
  {
    dbtemplate = defGetString(dtemplate);
  }
  if (dencoding && dencoding->arg)
  {
    const char *encoding_name;

    if (IsA(dencoding->arg, Integer))
    {
      encoding = defGetInt32(dencoding);
      encoding_name = pg_encoding_to_char(encoding);
      if (strcmp(encoding_name, "") == 0 || pg_valid_server_encoding(encoding_name) < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("%d is not a valid encoding code", encoding), parser_errposition(pstate, dencoding->location)));
      }
    }
    else
    {
      encoding_name = defGetString(dencoding);
      encoding = pg_valid_server_encoding(encoding_name);
      if (encoding < 0)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("%s is not a valid encoding name", encoding_name), parser_errposition(pstate, dencoding->location)));
      }
    }
  }
  if (dcollate && dcollate->arg)
  {
    dbcollate = defGetString(dcollate);
  }
  if (dctype && dctype->arg)
  {
    dbctype = defGetString(dctype);
  }
  if (distemplate && distemplate->arg)
  {
    dbistemplate = defGetBoolean(distemplate);
  }
  if (dallowconnections && dallowconnections->arg)
  {
    dballowconnections = defGetBoolean(dallowconnections);
  }
  if (dconnlimit && dconnlimit->arg)
  {
    dbconnlimit = defGetInt32(dconnlimit);
    if (dbconnlimit < -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid connection limit: %d", dbconnlimit)));
    }
  }

                                    
  if (dbowner)
  {
    datdba = get_role_oid(dbowner, false);
  }
  else
  {
    datdba = GetUserId();
  }

     
                                                                            
                                                                             
                                                                         
                                                                         
                                  
     
  if (!have_createdb_privilege())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create database")));
  }

  check_is_member_of_role(GetUserId(), datdba);

     
                                                                           
                                                                          
                                                                         
                                                                       
                                                                             
                                                                         
                                 
     
  if (!dbtemplate)
  {
    dbtemplate = "template1";                                     
  }

  if (!get_db_info(dbtemplate, ShareLock, &src_dboid, &src_owner, &src_encoding, &src_istemplate, &src_allowconn, &src_lastsysoid, &src_frozenxid, &src_minmxid, &src_deftablespace, &src_collate, &src_ctype))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("template database \"%s\" does not exist", dbtemplate)));
  }

     
                                                                         
                                             
     
  if (!src_istemplate)
  {
    if (!pg_database_ownercheck(src_dboid, GetUserId()))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to copy database \"%s\"", dbtemplate)));
    }
  }

                                                                  
  if (encoding < 0)
  {
    encoding = src_encoding;
  }
  if (dbcollate == NULL)
  {
    dbcollate = src_collate;
  }
  if (dbctype == NULL)
  {
    dbctype = src_ctype;
  }

                                      
  if (!PG_VALID_BE_ENCODING(encoding))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("invalid server encoding %d", encoding)));
  }

                                                                            
  if (!check_locale(LC_COLLATE, dbcollate, &canonname))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("invalid locale name: \"%s\"", dbcollate)));
  }
  dbcollate = canonname;
  if (!check_locale(LC_CTYPE, dbctype, &canonname))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("invalid locale name: \"%s\"", dbctype)));
  }
  dbctype = canonname;

  check_encoding_locale_matches(encoding, dbcollate, dbctype);

     
                                                                      
                                                                             
                                                                         
                                                    
     
                                                                          
                                                                            
                                                                           
     
  if (strcmp(dbtemplate, "template0") != 0)
  {
    if (encoding != src_encoding)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("new encoding (%s) is incompatible with the encoding of the template database (%s)", pg_encoding_to_char(encoding), pg_encoding_to_char(src_encoding)), errhint("Use the same encoding as in the template database, or use template0 as template.")));
    }

    if (strcmp(dbcollate, src_collate) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("new collation (%s) is incompatible with the collation of the template database (%s)", dbcollate, src_collate), errhint("Use the same collation as in the template database, or use template0 as template.")));
    }

    if (strcmp(dbctype, src_ctype) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("new LC_CTYPE (%s) is incompatible with the LC_CTYPE of the template database (%s)", dbctype, src_ctype), errhint("Use the same LC_CTYPE as in the template database, or use template0 as template.")));
    }
  }

                                                   
  if (dtablespacename && dtablespacename->arg)
  {
    char *tablespacename;
    AclResult aclresult;

    tablespacename = defGetString(dtablespacename);
    dst_deftablespace = get_tablespace_oid(tablespacename, false);
                           
    aclresult = pg_tablespace_aclcheck(dst_deftablespace, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_TABLESPACE, tablespacename);
    }

                                                        
    if (dst_deftablespace == GLOBALTABLESPACE_OID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_global cannot be used as default tablespace")));
    }

       
                                                                          
                                                                          
                                                                   
                                                                      
                                                                           
                                                                        
                                                                         
                                                                          
                                                                        
                                     
       
    if (dst_deftablespace != src_deftablespace)
    {
      char *srcpath;
      struct stat st;

      srcpath = GetDatabasePath(src_dboid, dst_deftablespace);

      if (stat(srcpath, &st) == 0 && S_ISDIR(st.st_mode) && !directory_is_empty(srcpath))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot assign new default tablespace \"%s\"", tablespacename), errdetail("There is a conflict because database \"%s\" already has some tables in this tablespace.", dbtemplate)));
      }
      pfree(srcpath);
    }
  }
  else
  {
                                                    
    dst_deftablespace = src_deftablespace;
                                                                   
  }

     
                                                                     
                                                                             
             
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (IsUnderPostmaster && strstr(dbname, "regression") == NULL)
  {
    elog(WARNING, "databases created by regression test cases should have names including \"regression\"");
  }
#endif

     
                                                                             
                                                                          
                                                                     
     
  if (OidIsValid(get_database_oid(dbname, true)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_DATABASE), errmsg("database \"%s\" already exists", dbname)));
  }

     
                                                                   
                                                                     
                                                
     
                                                                           
                                                                           
                
     
  if (CountOtherDBBackends(src_dboid, &notherbackends, &npreparedxacts))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("source database \"%s\" is being accessed by other users", dbtemplate), errdetail_busy_db(notherbackends, npreparedxacts)));
  }

     
                                                                         
                                                                        
                  
     
  pg_database_rel = table_open(DatabaseRelationId, RowExclusiveLock);

  do
  {
    dboid = GetNewOidWithIndex(pg_database_rel, DatabaseOidIndexId, Anum_pg_database_oid);
  } while (check_db_file_conflict(dboid));

     
                                                                             
                                                                            
                                                           
     

                  
  MemSet(new_record, 0, sizeof(new_record));
  MemSet(new_record_nulls, false, sizeof(new_record_nulls));

  new_record[Anum_pg_database_oid - 1] = ObjectIdGetDatum(dboid);
  new_record[Anum_pg_database_datname - 1] = DirectFunctionCall1(namein, CStringGetDatum(dbname));
  new_record[Anum_pg_database_datdba - 1] = ObjectIdGetDatum(datdba);
  new_record[Anum_pg_database_encoding - 1] = Int32GetDatum(encoding);
  new_record[Anum_pg_database_datcollate - 1] = DirectFunctionCall1(namein, CStringGetDatum(dbcollate));
  new_record[Anum_pg_database_datctype - 1] = DirectFunctionCall1(namein, CStringGetDatum(dbctype));
  new_record[Anum_pg_database_datistemplate - 1] = BoolGetDatum(dbistemplate);
  new_record[Anum_pg_database_datallowconn - 1] = BoolGetDatum(dballowconnections);
  new_record[Anum_pg_database_datconnlimit - 1] = Int32GetDatum(dbconnlimit);
  new_record[Anum_pg_database_datlastsysoid - 1] = ObjectIdGetDatum(src_lastsysoid);
  new_record[Anum_pg_database_datfrozenxid - 1] = TransactionIdGetDatum(src_frozenxid);
  new_record[Anum_pg_database_datminmxid - 1] = TransactionIdGetDatum(src_minmxid);
  new_record[Anum_pg_database_dattablespace - 1] = ObjectIdGetDatum(dst_deftablespace);

     
                                                                          
                                                                          
                                                    
     
  new_record_nulls[Anum_pg_database_datacl - 1] = true;

  tuple = heap_form_tuple(RelationGetDescr(pg_database_rel), new_record, new_record_nulls);

  CatalogTupleInsert(pg_database_rel, tuple);

     
                                                                        
     

                                 
  recordDependencyOnOwner(DatabaseRelationId, dboid, datdba);

                                                              
  copyTemplateDependencies(src_dboid, dboid);

                                           
  InvokeObjectPostCreateHook(DatabaseRelationId, dboid, 0);

     
                                                                            
                                                                         
                                                         
                                                                        
                                                                     
                                                                           
                                                                           
                  
     
  RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT | CHECKPOINT_FLUSH_ALL);

     
                                                                           
                                                                           
                                                                          
                                                                          
                      
     
  fparms.src_dboid = src_dboid;
  fparms.dest_dboid = dboid;
  PG_ENSURE_ERROR_CLEANUP(createdb_failure_callback, PointerGetDatum(&fparms));
  {
       
                                                                          
                                     
       
    rel = table_open(TableSpaceRelationId, AccessShareLock);
    scan = table_beginscan_catalog(rel, 0, NULL);
    while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
      Form_pg_tablespace spaceform = (Form_pg_tablespace)GETSTRUCT(tuple);
      Oid srctablespace = spaceform->oid;
      Oid dsttablespace;
      char *srcpath;
      char *dstpath;
      struct stat st;

                                             
      if (srctablespace == GLOBALTABLESPACE_OID)
      {
        continue;
      }

      srcpath = GetDatabasePath(src_dboid, srctablespace);

      if (stat(srcpath, &st) < 0 || !S_ISDIR(st.st_mode) || directory_is_empty(srcpath))
      {
                                     
        pfree(srcpath);
        continue;
      }

      if (srctablespace == src_deftablespace)
      {
        dsttablespace = dst_deftablespace;
      }
      else
      {
        dsttablespace = srctablespace;
      }

      dstpath = GetDatabasePath(dboid, dsttablespace);

         
                                                    
         
                                              
         
      copydir(srcpath, dstpath, false);

                                                
      {
        xl_dbase_create_rec xlrec;

        xlrec.db_id = dboid;
        xlrec.tablespace_id = dsttablespace;
        xlrec.src_db_id = src_dboid;
        xlrec.src_tablespace_id = srctablespace;

        XLogBeginInsert();
        XLogRegisterData((char *)&xlrec, sizeof(xl_dbase_create_rec));

        (void)XLogInsert(RM_DBASE_ID, XLOG_DBASE_CREATE | XLR_SPECIAL_REL_UPDATE);
      }
    }
    table_endscan(scan);
    table_close(rel, AccessShareLock);

       
                                                                        
                                                                         
                                                                           
                                                                     
                                   
       
                                                                         
                                                                         
                                                       
       
                                                                           
                                                                  
                                                                           
                                                                         
                                                                      
              
       
                                                                     
       
                                                                         
                                                                     
                                                                      
                                                                          
                           
       
                                                                          
                          
       
    RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

       
                                                     
       
    table_close(pg_database_rel, NoLock);

       
                                                                    
                                                                           
                                                                         
                                                           
       
    ForceSyncCommit();
  }
  PG_END_ENSURE_ERROR_CLEANUP(createdb_failure_callback, PointerGetDatum(&fparms));

  return dboid;
}

   
                                                                       
                                                                        
                                                                        
                                 
   
                                                                          
                                  
   
                                                                       
                                                                 
   
                                                                       
                                                                         
                                         
   
                                                                           
                                                                 
                                
   
                                                         
   
void
check_encoding_locale_matches(int encoding, const char *collate, const char *ctype)
{
  int ctype_encoding = pg_get_encoding_from_locale(ctype, true);
  int collate_encoding = pg_get_encoding_from_locale(collate, true);

  if (!(ctype_encoding == encoding || ctype_encoding == PG_SQL_ASCII || ctype_encoding == -1 ||
#ifdef WIN32
          encoding == PG_UTF8 ||
#endif
          (encoding == PG_SQL_ASCII && superuser())))
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("encoding \"%s\" does not match locale \"%s\"", pg_encoding_to_char(encoding), ctype), errdetail("The chosen LC_CTYPE setting requires encoding \"%s\".", pg_encoding_to_char(ctype_encoding))));

  if (!(collate_encoding == encoding || collate_encoding == PG_SQL_ASCII || collate_encoding == -1 ||
#ifdef WIN32
          encoding == PG_UTF8 ||
#endif
          (encoding == PG_SQL_ASCII && superuser())))
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("encoding \"%s\" does not match locale \"%s\"", pg_encoding_to_char(encoding), collate), errdetail("The chosen LC_COLLATE setting requires encoding \"%s\".", pg_encoding_to_char(collate_encoding))));
}

                                         
static void
createdb_failure_callback(int code, Datum arg)
{
  createdb_failure_params *fparms = (createdb_failure_params *)DatumGetPointer(arg);

     
                                                                            
                                                                         
               
     
  UnlockSharedObject(DatabaseRelationId, fparms->src_dboid, 0, ShareLock);

                                                         
  remove_dbtablespaces(fparms->dest_dboid);
}

   
                 
   
void
dropdb(const char *dbname, bool missing_ok)
{
  Oid db_id;
  bool db_istemplate;
  Relation pgdbrel;
  HeapTuple tup;
  int notherbackends;
  int npreparedxacts;
  int nslots, nslots_active;
  int nsubscriptions;

     
                                                                         
                                                                     
                                                                            
                                                                       
                 
     
  pgdbrel = table_open(DatabaseRelationId, RowExclusiveLock);

  if (!get_db_info(dbname, AccessExclusiveLock, &db_id, NULL, NULL, &db_istemplate, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
    }
    else
    {
                                                                         
      table_close(pgdbrel, RowExclusiveLock);
      ereport(NOTICE, (errmsg("database \"%s\" does not exist, skipping", dbname)));
      return;
    }
  }

     
                       
     
  if (!pg_database_ownercheck(db_id, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, dbname);
  }

                                                
  InvokeObjectDropHook(DatabaseRelationId, db_id, 0);

     
                                                                        
                                                                            
                                                
     
  if (db_istemplate)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot drop a template database")));
  }

                                            
  if (db_id == MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("cannot drop the currently open database")));
  }

     
                                                                    
                                                                           
                                                                         
             
     
  (void)ReplicationSlotsCountDBSlots(db_id, &nslots, &nslots_active);
  if (nslots_active)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("database \"%s\" is used by an active logical replication slot", dbname), errdetail_plural("There is %d active slot.", "There are %d active slots.", nslots_active, nslots_active)));
  }

     
                                                                            
                                                       
     
                                                                     
     
  if (CountOtherDBBackends(db_id, &notherbackends, &npreparedxacts))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("database \"%s\" is being accessed by other users", dbname), errdetail_busy_db(notherbackends, npreparedxacts)));
  }

     
                                                                      
     
                                                                    
                                             
     
  if ((nsubscriptions = CountDBSubscriptions(db_id)) > 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("database \"%s\" is being used by logical replication subscription", dbname), errdetail_plural("There is %d subscription.", "There are %d subscriptions.", nsubscriptions, nsubscriptions)));
  }

     
                                                   
     
  tup = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(db_id));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for database %u", db_id);
  }

  CatalogTupleDelete(pgdbrel, &tup->t_self);

  ReleaseSysCache(tup);

     
                                                                          
     
  DeleteSharedComments(db_id, DatabaseRelationId);
  DeleteSharedSecurityLabel(db_id, DatabaseRelationId);

     
                                                   
     
  DropSetting(db_id, InvalidOid);

     
                                                           
     
  dropDatabaseDependencies(db_id);

     
                                         
     
  ReplicationSlotsDropDBSlots(db_id);

     
                                                                            
                                                                           
                                                
     
  DropDatabaseBuffers(db_id);

     
                                                             
     
  pgstat_drop_database(db_id);

     
                                                                           
                                                                             
                                                                         
                        
     
  ForgetDatabaseSyncRequests(db_id);

     
                                                                       
                                                                       
                                                                          
                            
     
  RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

     
                                                              
     
  remove_dbtablespaces(db_id);

     
                                                   
     
  table_close(pgdbrel, NoLock);

     
                                                                             
                                                                             
                                                                     
                                                  
     
  ForceSyncCommit();
}

   
                   
   
ObjectAddress
RenameDatabase(const char *oldname, const char *newname)
{
  Oid db_id;
  HeapTuple newtup;
  Relation rel;
  int notherbackends;
  int npreparedxacts;
  ObjectAddress address;

     
                                                                         
                                                      
     
  rel = table_open(DatabaseRelationId, RowExclusiveLock);

  if (!get_db_info(oldname, AccessExclusiveLock, &db_id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", oldname)));
  }

                     
  if (!pg_database_ownercheck(db_id, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, oldname);
  }

                                 
  if (!have_createdb_privilege())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to rename database")));
  }

     
                                                                     
                                                  
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strstr(newname, "regression") == NULL)
  {
    elog(WARNING, "databases created by regression test cases should have names including \"regression\"");
  }
#endif

     
                                                                        
                      
     
  if (OidIsValid(get_database_oid(newname, true)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_DATABASE), errmsg("database \"%s\" already exists", newname)));
  }

     
                                                                            
                                                                             
                                                                          
                 
     
  if (db_id == MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("current database cannot be renamed")));
  }

     
                                                                             
                                                      
     
                                                                     
     
  if (CountOtherDBBackends(db_id, &notherbackends, &npreparedxacts))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("database \"%s\" is being accessed by other users", oldname), errdetail_busy_db(notherbackends, npreparedxacts)));
  }

              
  newtup = SearchSysCacheCopy1(DATABASEOID, ObjectIdGetDatum(db_id));
  if (!HeapTupleIsValid(newtup))
  {
    elog(ERROR, "cache lookup failed for database %u", db_id);
  }
  namestrcpy(&(((Form_pg_database)GETSTRUCT(newtup))->datname), newname);
  CatalogTupleUpdate(rel, &newtup->t_self, newtup);

  InvokeObjectPostAlterHook(DatabaseRelationId, db_id, 0);

  ObjectAddressSet(address, DatabaseRelationId, db_id);

     
                                                   
     
  table_close(rel, NoLock);

  return address;
}

   
                                 
   
static void
movedb(const char *dbname, const char *tblspcname)
{
  Oid db_id;
  Relation pgdbrel;
  int notherbackends;
  int npreparedxacts;
  HeapTuple oldtuple, newtuple;
  Oid src_tblspcoid, dst_tblspcoid;
  Datum new_record[Natts_pg_database];
  bool new_record_nulls[Natts_pg_database];
  bool new_record_repl[Natts_pg_database];
  ScanKeyData scankey;
  SysScanDesc sysscan;
  AclResult aclresult;
  char *src_dbpath;
  char *dst_dbpath;
  DIR *dstdir;
  struct dirent *xlde;
  movedb_failure_params fparms;

     
                                                                         
                                                                             
                                                                        
                                      
     
  pgdbrel = table_open(DatabaseRelationId, RowExclusiveLock);

  if (!get_db_info(dbname, AccessExclusiveLock, &db_id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &src_tblspcoid, NULL, NULL))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
  }

     
                                                                           
                                                                           
                                                                       
                                                                            
     
  LockSharedObjectForSession(DatabaseRelationId, db_id, 0, AccessExclusiveLock);

     
                       
     
  if (!pg_database_ownercheck(db_id, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, dbname);
  }

     
                                                        
     
  if (db_id == MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("cannot change the tablespace of the currently open database")));
  }

     
                          
     
  dst_tblspcoid = get_tablespace_oid(tblspcname, false);

     
                       
     
  aclresult = pg_tablespace_aclcheck(dst_tblspcoid, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_TABLESPACE, tblspcname);
  }

     
                                                    
     
  if (dst_tblspcoid == GLOBALTABLESPACE_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_global cannot be used as default tablespace")));
  }

     
                              
     
  if (src_tblspcoid == dst_tblspcoid)
  {
    table_close(pgdbrel, NoLock);
    UnlockSharedObjectForSession(DatabaseRelationId, db_id, 0, AccessExclusiveLock);
    return;
  }

     
                                                                            
                                                       
     
                                                                     
     
  if (CountOtherDBBackends(db_id, &notherbackends, &npreparedxacts))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("database \"%s\" is being accessed by other users", dbname), errdetail_busy_db(notherbackends, npreparedxacts)));
  }

     
                                    
     
  src_dbpath = GetDatabasePath(db_id, src_tblspcoid);
  dst_dbpath = GetDatabasePath(db_id, dst_tblspcoid);

     
                                                                     
                                                                         
                                                         
                                                                        
                                                                            
                                                                            
                                                                            
                                                                             
                                               
     
  RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT | CHECKPOINT_FLUSH_ALL);

     
                                                                           
                                                        
     
                                                                          
                                                                           
                                                                            
                                                                          
                                                                            
                                                                            
                                                                     
     
                                                                       
                                                                     
     
  DropDatabaseBuffers(db_id);

     
                                                                            
                                                                        
                                                                          
                                                                          
                                           
     
  dstdir = AllocateDir(dst_dbpath);
  if (dstdir != NULL)
  {
    while ((xlde = ReadDir(dstdir, dst_dbpath)) != NULL)
    {
      if (strcmp(xlde->d_name, ".") == 0 || strcmp(xlde->d_name, "..") == 0)
      {
        continue;
      }

      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("some relations of database \"%s\" are already in tablespace \"%s\"", dbname, tblspcname), errhint("You must move them back to the database's default tablespace before using this command.")));
    }

    FreeDir(dstdir);

       
                                                                         
                             
       
    if (rmdir(dst_dbpath) != 0)
    {
      elog(ERROR, "could not remove directory \"%s\": %m", dst_dbpath);
    }
  }

     
                                                                             
                                                                           
                                                                            
                            
     
  fparms.dest_dboid = db_id;
  fparms.dest_tsoid = dst_tblspcoid;
  PG_ENSURE_ERROR_CLEANUP(movedb_failure_callback, PointerGetDatum(&fparms));
  {
       
                                                         
       
    copydir(src_dbpath, dst_dbpath, false);

       
                                            
       
    {
      xl_dbase_create_rec xlrec;

      xlrec.db_id = db_id;
      xlrec.tablespace_id = dst_tblspcoid;
      xlrec.src_db_id = db_id;
      xlrec.src_tablespace_id = src_tblspcoid;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, sizeof(xl_dbase_create_rec));

      (void)XLogInsert(RM_DBASE_ID, XLOG_DBASE_CREATE | XLR_SPECIAL_REL_UPDATE);
    }

       
                                               
       
    ScanKeyInit(&scankey, Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(dbname));
    sysscan = systable_beginscan(pgdbrel, DatabaseNameIndexId, true, NULL, 1, &scankey);
    oldtuple = systable_getnext(sysscan);
    if (!HeapTupleIsValid(oldtuple))                          
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
    }

    MemSet(new_record, 0, sizeof(new_record));
    MemSet(new_record_nulls, false, sizeof(new_record_nulls));
    MemSet(new_record_repl, false, sizeof(new_record_repl));

    new_record[Anum_pg_database_dattablespace - 1] = ObjectIdGetDatum(dst_tblspcoid);
    new_record_repl[Anum_pg_database_dattablespace - 1] = true;

    newtuple = heap_modify_tuple(oldtuple, RelationGetDescr(pgdbrel), new_record, new_record_nulls, new_record_repl);
    CatalogTupleUpdate(pgdbrel, &oldtuple->t_self, newtuple);

    InvokeObjectPostAlterHook(DatabaseRelationId, db_id, 0);

    systable_endscan(sysscan);

       
                                                                         
                                                                         
                                                                       
                                                                 
       
    RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

       
                                                                    
                                                                          
                                                                        
                                                     
       
    ForceSyncCommit();

       
                                                     
       
    table_close(pgdbrel, NoLock);
  }
  PG_END_ENSURE_ERROR_CLEANUP(movedb_failure_callback, PointerGetDatum(&fparms));

     
                                                                            
                                                                         
                                                          
     
                                                                        
     
                                                                         
                                                                          
                     
     
  PopActiveSnapshot();
  CommitTransactionCommand();

                                                                           
  StartTransactionCommand();

     
                                          
     
  if (!rmtree(src_dbpath, true))
  {
    ereport(WARNING, (errmsg("some useless files may be left behind in old database directory \"%s\"", src_dbpath)));
  }

     
                                          
     
  {
    xl_dbase_drop_rec xlrec;

    xlrec.db_id = db_id;
    xlrec.tablespace_id = src_tblspcoid;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, sizeof(xl_dbase_drop_rec));

    (void)XLogInsert(RM_DBASE_ID, XLOG_DBASE_DROP | XLR_SPECIAL_REL_UPDATE);
  }

                                                  
  UnlockSharedObjectForSession(DatabaseRelationId, db_id, 0, AccessExclusiveLock);
}

                                       
static void
movedb_failure_callback(int code, Datum arg)
{
  movedb_failure_params *fparms = (movedb_failure_params *)DatumGetPointer(arg);
  char *dstpath;

                                                                      
  dstpath = GetDatabasePath(fparms->dest_dboid, fparms->dest_tsoid);

  (void)rmtree(dstpath, true);
}

   
                           
   
Oid
AlterDatabase(ParseState *pstate, AlterDatabaseStmt *stmt, bool isTopLevel)
{
  Relation rel;
  Oid dboid;
  HeapTuple tuple, newtuple;
  Form_pg_database datform;
  ScanKeyData scankey;
  SysScanDesc scan;
  ListCell *option;
  bool dbistemplate = false;
  bool dballowconnections = true;
  int dbconnlimit = -1;
  DefElem *distemplate = NULL;
  DefElem *dallowconnections = NULL;
  DefElem *dconnlimit = NULL;
  DefElem *dtablespace = NULL;
  Datum new_record[Natts_pg_database];
  bool new_record_nulls[Natts_pg_database];
  bool new_record_repl[Natts_pg_database];

                                                    
  foreach (option, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(option);

    if (strcmp(defel->defname, "is_template") == 0)
    {
      if (distemplate)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      distemplate = defel;
    }
    else if (strcmp(defel->defname, "allow_connections") == 0)
    {
      if (dallowconnections)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dallowconnections = defel;
    }
    else if (strcmp(defel->defname, "connection_limit") == 0)
    {
      if (dconnlimit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dconnlimit = defel;
    }
    else if (strcmp(defel->defname, "tablespace") == 0)
    {
      if (dtablespace)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dtablespace = defel;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("option \"%s\" not recognized", defel->defname), parser_errposition(pstate, defel->location)));
    }
  }

  if (dtablespace)
  {
       
                                                                        
                                                                     
                                                  
       
    if (list_length(stmt->options) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("option \"%s\" cannot be specified with other options", dtablespace->defname), parser_errposition(pstate, dtablespace->location)));
    }
                                                            
    PreventInTransactionBlock(isTopLevel, "ALTER DATABASE SET TABLESPACE");
    movedb(stmt->dbname, defGetString(dtablespace));
    return InvalidOid;
  }

  if (distemplate && distemplate->arg)
  {
    dbistemplate = defGetBoolean(distemplate);
  }
  if (dallowconnections && dallowconnections->arg)
  {
    dballowconnections = defGetBoolean(dallowconnections);
  }
  if (dconnlimit && dconnlimit->arg)
  {
    dbconnlimit = defGetInt32(dconnlimit);
    if (dbconnlimit < -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid connection limit: %d", dbconnlimit)));
    }
  }

     
                                                                      
                                                                        
                  
     
  rel = table_open(DatabaseRelationId, RowExclusiveLock);
  ScanKeyInit(&scankey, Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(stmt->dbname));
  scan = systable_beginscan(rel, DatabaseNameIndexId, true, NULL, 1, &scankey);
  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", stmt->dbname)));
  }

  datform = (Form_pg_database)GETSTRUCT(tuple);
  dboid = datform->oid;

  if (!pg_database_ownercheck(dboid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, stmt->dbname);
  }

     
                                                                   
                                                                        
                                                                             
                                                                            
     
  if (!dballowconnections && dboid == MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot disallow connections for current database")));
  }

     
                                                                    
     
  MemSet(new_record, 0, sizeof(new_record));
  MemSet(new_record_nulls, false, sizeof(new_record_nulls));
  MemSet(new_record_repl, false, sizeof(new_record_repl));

  if (distemplate)
  {
    new_record[Anum_pg_database_datistemplate - 1] = BoolGetDatum(dbistemplate);
    new_record_repl[Anum_pg_database_datistemplate - 1] = true;
  }
  if (dallowconnections)
  {
    new_record[Anum_pg_database_datallowconn - 1] = BoolGetDatum(dballowconnections);
    new_record_repl[Anum_pg_database_datallowconn - 1] = true;
  }
  if (dconnlimit)
  {
    new_record[Anum_pg_database_datconnlimit - 1] = Int32GetDatum(dbconnlimit);
    new_record_repl[Anum_pg_database_datconnlimit - 1] = true;
  }

  newtuple = heap_modify_tuple(tuple, RelationGetDescr(rel), new_record, new_record_nulls, new_record_repl);
  CatalogTupleUpdate(rel, &tuple->t_self, newtuple);

  InvokeObjectPostAlterHook(DatabaseRelationId, dboid, 0);

  systable_endscan(scan);

                                                    
  table_close(rel, NoLock);

  return dboid;
}

   
                               
   
Oid
AlterDatabaseSet(AlterDatabaseSetStmt *stmt)
{
  Oid datid = get_database_oid(stmt->dbname, false);

     
                                                                          
               
     
  shdepLockAndCheckObject(DatabaseRelationId, datid);

  if (!pg_database_ownercheck(datid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, stmt->dbname);
  }

  AlterSetting(datid, InvalidOid, stmt->setstmt);

  UnlockSharedObject(DatabaseRelationId, datid, 0, AccessShareLock);

  return datid;
}

   
                                         
   
ObjectAddress
AlterDatabaseOwner(const char *dbname, Oid newOwnerId)
{
  Oid db_id;
  HeapTuple tuple;
  Relation rel;
  ScanKeyData scankey;
  SysScanDesc scan;
  Form_pg_database datForm;
  ObjectAddress address;

     
                                                                      
                                                                        
                  
     
  rel = table_open(DatabaseRelationId, RowExclusiveLock);
  ScanKeyInit(&scankey, Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(dbname));
  scan = systable_beginscan(rel, DatabaseNameIndexId, true, NULL, 1, &scankey);
  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
  }

  datForm = (Form_pg_database)GETSTRUCT(tuple);
  db_id = datForm->oid;

     
                                                                      
                                                                     
              
     
  if (datForm->datdba != newOwnerId)
  {
    Datum repl_val[Natts_pg_database];
    bool repl_null[Natts_pg_database];
    bool repl_repl[Natts_pg_database];
    Acl *newAcl;
    Datum aclDatum;
    bool isNull;
    HeapTuple newtuple;

                                                         
    if (!pg_database_ownercheck(db_id, GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, dbname);
    }

                                          
    check_is_member_of_role(GetUserId(), newOwnerId);

       
                                 
       
                                                                         
                                                                      
                                                                       
                                                                           
                                 
       
    if (!have_createdb_privilege())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to change owner of database")));
    }

    memset(repl_null, false, sizeof(repl_null));
    memset(repl_repl, false, sizeof(repl_repl));

    repl_repl[Anum_pg_database_datdba - 1] = true;
    repl_val[Anum_pg_database_datdba - 1] = ObjectIdGetDatum(newOwnerId);

       
                                                                   
                                           
       
    aclDatum = heap_getattr(tuple, Anum_pg_database_datacl, RelationGetDescr(rel), &isNull);
    if (!isNull)
    {
      newAcl = aclnewowner(DatumGetAclP(aclDatum), datForm->datdba, newOwnerId);
      repl_repl[Anum_pg_database_datacl - 1] = true;
      repl_val[Anum_pg_database_datacl - 1] = PointerGetDatum(newAcl);
    }

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(rel), repl_val, repl_null, repl_repl);
    CatalogTupleUpdate(rel, &newtuple->t_self, newtuple);

    heap_freetuple(newtuple);

                                           
    changeDependencyOnOwner(DatabaseRelationId, db_id, newOwnerId);
  }

  InvokeObjectPostAlterHook(DatabaseRelationId, db_id, 0);

  ObjectAddressSet(address, DatabaseRelationId, db_id);

  systable_endscan(scan);

                                                    
  table_close(rel, NoLock);

  return address;
}

   
                    
   

   
                                                                          
                                                                      
                                                                       
                 
   
static bool
get_db_info(const char *name, LOCKMODE lockmode, Oid *dbIdP, Oid *ownerIdP, int *encodingP, bool *dbIsTemplateP, bool *dbAllowConnP, Oid *dbLastSysOidP, TransactionId *dbFrozenXidP, MultiXactId *dbMinMultiP, Oid *dbTablespace, char **dbCollate, char **dbCtype)
{
  bool result = false;
  Relation relation;

  AssertArg(name);

                                                                          
  relation = table_open(DatabaseRelationId, AccessShareLock);

     
                                                                           
                                                                           
           
     
  for (;;)
  {
    ScanKeyData scanKey;
    SysScanDesc scan;
    HeapTuple tuple;
    Oid dbOid;

       
                                                                           
                
       
    ScanKeyInit(&scanKey, Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(name));

    scan = systable_beginscan(relation, DatabaseNameIndexId, true, NULL, 1, &scanKey);

    tuple = systable_getnext(scan);

    if (!HeapTupleIsValid(tuple))
    {
                                               
      systable_endscan(scan);
      break;
    }

    dbOid = ((Form_pg_database)GETSTRUCT(tuple))->oid;

    systable_endscan(scan);

       
                                                                   
       
    if (lockmode != NoLock)
    {
      LockSharedObject(DatabaseRelationId, dbOid, 0, lockmode);
    }

       
                                                                          
                                                                       
              
       
    tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(dbOid));
    if (HeapTupleIsValid(tuple))
    {
      Form_pg_database dbform = (Form_pg_database)GETSTRUCT(tuple);

      if (strcmp(name, NameStr(dbform->datname)) == 0)
      {
                                 
        if (dbIdP)
        {
          *dbIdP = dbOid;
        }
                              
        if (ownerIdP)
        {
          *ownerIdP = dbform->datdba;
        }
                                
        if (encodingP)
        {
          *encodingP = dbform->encoding;
        }
                                  
        if (dbIsTemplateP)
        {
          *dbIsTemplateP = dbform->datistemplate;
        }
                                   
        if (dbAllowConnP)
        {
          *dbAllowConnP = dbform->datallowconn;
        }
                                              
        if (dbLastSysOidP)
        {
          *dbLastSysOidP = dbform->datlastsysoid;
        }
                                  
        if (dbFrozenXidP)
        {
          *dbFrozenXidP = dbform->datfrozenxid;
        }
                                 
        if (dbMinMultiP)
        {
          *dbMinMultiP = dbform->datminmxid;
        }
                                                  
        if (dbTablespace)
        {
          *dbTablespace = dbform->dattablespace;
        }
                                                       
        if (dbCollate)
        {
          *dbCollate = pstrdup(NameStr(dbform->datcollate));
        }
        if (dbCtype)
        {
          *dbCtype = pstrdup(NameStr(dbform->datctype));
        }
        ReleaseSysCache(tuple);
        result = true;
        break;
      }
                                                    
      ReleaseSysCache(tuple);
    }

    if (lockmode != NoLock)
    {
      UnlockSharedObject(DatabaseRelationId, dbOid, 0, lockmode);
    }
  }

  table_close(relation, AccessShareLock);

  return result;
}

                                                   
static bool
have_createdb_privilege(void)
{
  bool result = false;
  HeapTuple utup;

                                           
  if (superuser())
  {
    return true;
  }

  utup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(GetUserId()));
  if (HeapTupleIsValid(utup))
  {
    result = ((Form_pg_authid)GETSTRUCT(utup))->rolcreatedb;
    ReleaseSysCache(utup);
  }
  return result;
}

   
                                 
   
                                                                         
                                           
   
static void
remove_dbtablespaces(Oid db_id)
{
  Relation rel;
  TableScanDesc scan;
  HeapTuple tuple;

  rel = table_open(TableSpaceRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);
  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_tablespace spcform = (Form_pg_tablespace)GETSTRUCT(tuple);
    Oid dsttablespace = spcform->oid;
    char *dstpath;
    struct stat st;

                                               
    if (dsttablespace == GLOBALTABLESPACE_OID)
    {
      continue;
    }

    dstpath = GetDatabasePath(db_id, dsttablespace);

    if (lstat(dstpath, &st) < 0 || !S_ISDIR(st.st_mode))
    {
                                   
      pfree(dstpath);
      continue;
    }

    if (!rmtree(dstpath, true))
    {
      ereport(WARNING, (errmsg("some useless files may be left behind in old database directory \"%s\"", dstpath)));
    }

                                              
    {
      xl_dbase_drop_rec xlrec;

      xlrec.db_id = db_id;
      xlrec.tablespace_id = dsttablespace;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, sizeof(xl_dbase_drop_rec));

      (void)XLogInsert(RM_DBASE_ID, XLOG_DBASE_DROP | XLR_SPECIAL_REL_UPDATE);
    }

    pfree(dstpath);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);
}

   
                                                                      
                                
   
                                                                            
                                                                              
                                                                          
                                                                          
                                                                          
                                                                             
                       
   
static bool
check_db_file_conflict(Oid db_id)
{
  bool result = false;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tuple;

  rel = table_open(TableSpaceRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);
  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_tablespace spcform = (Form_pg_tablespace)GETSTRUCT(tuple);
    Oid dsttablespace = spcform->oid;
    char *dstpath;
    struct stat st;

                                               
    if (dsttablespace == GLOBALTABLESPACE_OID)
    {
      continue;
    }

    dstpath = GetDatabasePath(db_id, dsttablespace);

    if (lstat(dstpath, &st) == 0)
    {
                                                             
      pfree(dstpath);
      result = true;
      break;
    }

    pfree(dstpath);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  return result;
}

   
                                                          
   
static int
errdetail_busy_db(int notherbackends, int npreparedxacts)
{
  if (notherbackends > 0 && npreparedxacts > 0)
  {

       
                                                                     
                                                       
       
    errdetail("There are %d other session(s) and %d prepared transaction(s) using the database.", notherbackends, npreparedxacts);
  }
  else if (notherbackends > 0)
  {
    errdetail_plural("There is %d other session using the database.", "There are %d other sessions using the database.", notherbackends, notherbackends);
  }
  else
  {
    errdetail_plural("There is %d prepared transaction using the database.", "There are %d prepared transactions using the database.", npreparedxacts, npreparedxacts);
  }
  return 0;                                       
}

   
                                                             
   
                                                                          
                                 
   
Oid
get_database_oid(const char *dbname, bool missing_ok)
{
  Relation pg_database;
  ScanKeyData entry[1];
  SysScanDesc scan;
  HeapTuple dbtuple;
  Oid oid;

     
                                                                          
                   
     
  pg_database = table_open(DatabaseRelationId, AccessShareLock);
  ScanKeyInit(&entry[0], Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(dbname));
  scan = systable_beginscan(pg_database, DatabaseNameIndexId, true, NULL, 1, entry);

  dbtuple = systable_getnext(scan);

                                                              
  if (HeapTupleIsValid(dbtuple))
  {
    oid = ((Form_pg_database)GETSTRUCT(dbtuple))->oid;
  }
  else
  {
    oid = InvalidOid;
  }

  systable_endscan(scan);
  table_close(pg_database, AccessShareLock);

  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
  }

  return oid;
}

   
                                                              
   
                                                           
   
char *
get_database_name(Oid dbid)
{
  HeapTuple dbtuple;
  char *result;

  dbtuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(dbid));
  if (HeapTupleIsValid(dbtuple))
  {
    result = pstrdup(NameStr(((Form_pg_database)GETSTRUCT(dbtuple))->datname));
    ReleaseSysCache(dbtuple);
  }
  else
  {
    result = NULL;
  }

  return result;
}

   
                           
   
                                                                              
                                                                          
                                                                           
                                                                               
                                                                                
                                                                            
                                                    
   
                                                                              
   
static void
recovery_create_dbdir(char *path, bool only_tblspc)
{
  struct stat st;

  Assert(RecoveryInProgress());

  if (stat(path, &st) == 0)
  {
    return;
  }

  if (only_tblspc && strstr(path, "pg_tblspc/") == NULL)
  {
    elog(PANIC, "requested to created invalid directory: %s", path);
  }

  if (reachedConsistency && !allow_in_place_tablespaces)
  {
    ereport(PANIC, errmsg("missing directory \"%s\"", path));
  }

  elog(reachedConsistency ? WARNING : DEBUG1, "creating missing directory: %s", path);

  if (pg_mkdir_p(path, pg_dir_create_mode) != 0)
  {
    ereport(PANIC, errmsg("could not create missing directory \"%s\": %m", path));
  }
}

   
                                        
   
void
dbase_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                   
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_DBASE_CREATE)
  {
    xl_dbase_create_rec *xlrec = (xl_dbase_create_rec *)XLogRecGetData(record);
    char *src_path;
    char *dst_path;
    char *parent_path;
    struct stat st;

    src_path = GetDatabasePath(xlrec->src_db_id, xlrec->src_tablespace_id);
    dst_path = GetDatabasePath(xlrec->db_id, xlrec->tablespace_id);

       
                                                                        
                                                                          
                                                             
       
    if (stat(dst_path, &st) == 0 && S_ISDIR(st.st_mode))
    {
      if (!rmtree(dst_path, true))
      {
                                                                
        ereport(WARNING, (errmsg("some useless files may be left behind in old database directory \"%s\"", dst_path)));
      }
    }

       
                                                                           
                                                                       
                                                                       
                                               
       
    parent_path = pstrdup(dst_path);
    get_parent_directory(parent_path);
    if (stat(parent_path, &st) < 0)
    {
      if (errno != ENOENT)
      {
        ereport(FATAL, errmsg("could not stat directory \"%s\": %m", dst_path));
      }

      recovery_create_dbdir(parent_path, true);
    }
    pfree(parent_path);

       
                                                                         
                                                                     
                                                                          
                 
       
    if (stat(src_path, &st) < 0 && errno == ENOENT)
    {
      recovery_create_dbdir(src_path, false);
    }

       
                                                                     
                                
       
    FlushDatabaseBuffers(xlrec->src_db_id);

       
                                                  
       
                                            
       
    copydir(src_path, dst_path, false);
  }
  else if (info == XLOG_DBASE_DROP)
  {
    xl_dbase_drop_rec *xlrec = (xl_dbase_drop_rec *)XLogRecGetData(record);
    char *dst_path;

    dst_path = GetDatabasePath(xlrec->db_id, xlrec->tablespace_id);

    if (InHotStandby)
    {
         
                                                                 
                                                                   
                                                                       
                                         
         
                                                                        
                                                                     
                
         
      LockSharedObjectForSession(DatabaseRelationId, xlrec->db_id, 0, AccessExclusiveLock);
      ResolveRecoveryConflictWithDatabase(xlrec->db_id);
    }

                                                      
    ReplicationSlotsDropDBSlots(xlrec->db_id);

                                                                          
    DropDatabaseBuffers(xlrec->db_id);

                                                                          
    ForgetDatabaseSyncRequests(xlrec->db_id);

                                         
    XLogDropDatabase(xlrec->db_id);

                                       
    if (!rmtree(dst_path, true))
    {
      ereport(WARNING, (errmsg("some useless files may be left behind in old database directory \"%s\"", dst_path)));
    }

    if (InHotStandby)
    {
         
                                                                      
                                                                       
                                                                         
                                                                      
                                                            
         
      UnlockSharedObjectForSession(DatabaseRelationId, xlrec->db_id, 0, AccessExclusiveLock);
    }
  }
  else
  {
    elog(PANIC, "dbase_redo: unknown op code %u", info);
  }
}
