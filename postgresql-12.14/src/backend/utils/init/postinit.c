                                                                            
   
              
                                       
   
                                                                         
                                                                        
   
   
                  
                                       
   
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/session.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_db_role_setting.h"
#include "catalog/pg_tablespace.h"
#include "libpq/auth.h"
#include "libpq/libpq-be.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/postmaster.h"
#include "replication/walsender.h"
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/proc.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "storage/sync.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/portal.h"
#include "utils/ps_status.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/timeout.h"

static HeapTuple
GetDatabaseTuple(const char *dbname);
static HeapTuple
GetDatabaseTupleByOid(Oid dboid);
static void
PerformAuthentication(Port *port);
static void
CheckMyDatabase(const char *name, bool am_superuser, bool override_allow_connections);
static void
InitCommunication(void);
static void
ShutdownPostgres(int code, Datum arg);
static void
StatementTimeoutHandler(void);
static void
LockTimeoutHandler(void);
static void
IdleInTransactionSessionTimeoutHandler(void);
static bool
ThereIsAtLeastOneRole(void);
static void
process_startup_options(Port *port, bool am_superuser);
static void
process_settings(Oid databaseid, Oid roleid);

                              

   
                                                                
   
                                                                            
                                                                              
                                                                           
                                                                    
                                                                             
                                                                            
                                                
   
static HeapTuple
GetDatabaseTuple(const char *dbname)
{
  HeapTuple tuple;
  Relation relation;
  SysScanDesc scan;
  ScanKeyData key[1];

     
                     
     
  ScanKeyInit(&key[0], Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(dbname));

     
                                                                            
                                                                         
                                            
     
  relation = table_open(DatabaseRelationId, AccessShareLock);
  scan = systable_beginscan(relation, DatabaseNameIndexId, criticalSharedRelcachesBuilt, NULL, 1, key);

  tuple = systable_getnext(scan);

                                               
  if (HeapTupleIsValid(tuple))
  {
    tuple = heap_copytuple(tuple);
  }

                
  systable_endscan(scan);
  table_close(relation, AccessShareLock);

  return tuple;
}

   
                                                                 
   
static HeapTuple
GetDatabaseTupleByOid(Oid dboid)
{
  HeapTuple tuple;
  Relation relation;
  SysScanDesc scan;
  ScanKeyData key[1];

     
                     
     
  ScanKeyInit(&key[0], Anum_pg_database_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(dboid));

     
                                                                            
                                                                         
                                            
     
  relation = table_open(DatabaseRelationId, AccessShareLock);
  scan = systable_beginscan(relation, DatabaseOidIndexId, criticalSharedRelcachesBuilt, NULL, 1, key);

  tuple = systable_getnext(scan);

                                               
  if (HeapTupleIsValid(tuple))
  {
    tuple = heap_copytuple(tuple);
  }

                
  systable_endscan(scan);
  table_close(relation, AccessShareLock);

  return tuple;
}

   
                                                         
   
                                                                     
   
static void
PerformAuthentication(Port *port)
{
                                                       
  ClientAuthInProgress = true;                                       

     
                                                                         
                                                                    
     
                                                                   
     
#ifdef EXEC_BACKEND

     
                                                                            
                                                                           
                                   
     
  if (PostmasterContext == NULL)
  {
    PostmasterContext = AllocSetContextCreate(TopMemoryContext, "Postmaster", ALLOCSET_DEFAULT_SIZES);
  }

  if (!load_hba())
  {
       
                                                                      
                                                                      
       
    ereport(FATAL, (errmsg("could not load pg_hba.conf")));
  }

  if (!load_ident())
  {
       
                                                                           
                                                                    
                                                                          
                                        
       
  }
#endif

     
                                                                           
                                                                           
                                                                           
     
  enable_timeout_after(STATEMENT_TIMEOUT, AuthenticationTimeout * 1000);

     
                                          
     
  ClientAuthentication(port);                                   

     
                                                                        
     
  disable_timeout(STATEMENT_TIMEOUT, false);

  if (Log_connections)
  {
    StringInfoData logmsg;

    initStringInfo(&logmsg);
    if (am_walsender)
    {
      appendStringInfo(&logmsg, _("replication connection authorized: user=%s"), port->user_name);
    }
    else
    {
      appendStringInfo(&logmsg, _("connection authorized: user=%s"), port->user_name);
    }
    if (!am_walsender)
    {
      appendStringInfo(&logmsg, _(" database=%s"), port->database_name);
    }

    if (port->application_name != NULL)
    {
      appendStringInfo(&logmsg, _(" application_name=%s"), port->application_name);
    }

#ifdef USE_SSL
    if (port->ssl_in_use)
    {
      appendStringInfo(&logmsg, _(" SSL enabled (protocol=%s, cipher=%s, bits=%d, compression=%s)"), be_tls_get_version(port), be_tls_get_cipher(port), be_tls_get_cipher_bits(port), be_tls_get_compression(port) ? _("on") : _("off"));
    }
#endif
#ifdef ENABLE_GSS
    if (port->gss)
    {
      const char *princ = be_gssapi_get_princ(port);

      if (princ)
      {
        appendStringInfo(&logmsg, _(" GSS (authenticated=%s, encrypted=%s, principal=%s)"), be_gssapi_get_auth(port) ? _("yes") : _("no"), be_gssapi_get_enc(port) ? _("yes") : _("no"), princ);
      }
      else
      {
        appendStringInfo(&logmsg, _(" GSS (authenticated=%s, encrypted=%s)"), be_gssapi_get_auth(port) ? _("yes") : _("no"), be_gssapi_get_enc(port) ? _("yes") : _("no"));
      }
    }
#endif

    ereport(LOG, errmsg_internal("%s", logmsg.data));
    pfree(logmsg.data);
  }

  set_ps_display("startup", false);

  ClientAuthInProgress = false;                                        
}

   
                                                                              
   
static void
CheckMyDatabase(const char *name, bool am_superuser, bool override_allow_connections)
{
  HeapTuple tup;
  Form_pg_database dbform;
  char *collate;
  char *ctype;

                                                        
  tup = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(MyDatabaseId));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for database %u", MyDatabaseId);
  }
  dbform = (Form_pg_database)GETSTRUCT(tup);

                                         
  if (strcmp(name, NameStr(dbform->datname)) != 0)
  {
    ereport(FATAL, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" has disappeared from pg_database", name), errdetail("Database OID %u now seems to belong to \"%s\".", MyDatabaseId, NameStr(dbform->datname))));
  }

     
                                                   
     
                                                                             
                                                                      
                                                             
     
                                                                    
     
  if (IsUnderPostmaster && !IsAutoVacuumWorkerProcess())
  {
       
                                                                  
       
    if (!dbform->datallowconn && !override_allow_connections)
    {
      ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("database \"%s\" is not currently accepting connections", name)));
    }

       
                                                                           
                                                                        
                               
       
    if (!am_superuser && pg_database_aclcheck(MyDatabaseId, GetUserId(), ACL_CONNECT) != ACLCHECK_OK)
    {
      ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for database \"%s\"", name), errdetail("User does not have CONNECT privilege.")));
    }

       
                                                 
       
                                                                      
                                                                          
                                                                        
                                                                      
                                                                        
                                                               
       
    if (dbform->datconnlimit >= 0 && !am_superuser && CountDBConnections(MyDatabaseId) > dbform->datconnlimit)
    {
      ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("too many connections for database \"%s\"", name)));
    }
  }

     
                                                                            
                            
     
  SetDatabaseEncoding(dbform->encoding);
                                               
  SetConfigOption("server_encoding", GetDatabaseEncodingName(), PGC_INTERNAL, PGC_S_OVERRIDE);
                                                                          
  SetConfigOption("client_encoding", GetDatabaseEncodingName(), PGC_BACKEND, PGC_S_DYNAMIC_DEFAULT);

                               
  collate = NameStr(dbform->datcollate);
  ctype = NameStr(dbform->datctype);

  if (pg_perm_setlocale(LC_COLLATE, collate) == NULL)
  {
    ereport(FATAL, (errmsg("database locale is incompatible with operating system"),
                       errdetail("The database was initialized with LC_COLLATE \"%s\", "
                                 " which is not recognized by setlocale().",
                           collate),
                       errhint("Recreate the database with another locale or install the missing locale.")));
  }

  if (pg_perm_setlocale(LC_CTYPE, ctype) == NULL)
  {
    ereport(FATAL, (errmsg("database locale is incompatible with operating system"),
                       errdetail("The database was initialized with LC_CTYPE \"%s\", "
                                 " which is not recognized by setlocale().",
                           ctype),
                       errhint("Recreate the database with another locale or install the missing locale.")));
  }

                                                              
  SetConfigOption("lc_collate", collate, PGC_INTERNAL, PGC_S_OVERRIDE);
  SetConfigOption("lc_ctype", ctype, PGC_INTERNAL, PGC_S_OVERRIDE);

  check_strxfrm_bug();

  ReleaseSysCache(tup);
}

                                    
                      
   
                                                                 
                                                    
                                    
   
static void
InitCommunication(void)
{
     
                                                            
     
  if (!IsUnderPostmaster)                                  
  {
       
                                                                           
                                                                          
                                                                    
                                
       
    CreateSharedMemoryAndSemaphores(PostPortNumber);
  }
}

   
                                                                             
   
                                                                               
                                                                 
                             
   
                                                                         
                                                          
   
void
pg_split_opts(char **argv, int *argcp, const char *optstr)
{
  StringInfoData s;

  initStringInfo(&s);

  while (*optstr)
  {
    bool last_was_escape = false;

    resetStringInfo(&s);

                                 
    while (isspace((unsigned char)*optstr))
    {
      optstr++;
    }

    if (*optstr == '\0')
    {
      break;
    }

       
                                                                       
                
       
    while (*optstr)
    {
      if (isspace((unsigned char)*optstr) && !last_was_escape)
      {
        break;
      }

      if (!last_was_escape && *optstr == '\\')
      {
        last_was_escape = true;
      }
      else
      {
        last_was_escape = false;
        appendStringInfoChar(&s, *optstr);
      }

      optstr++;
    }

                                                          
    argv[(*argcp)++] = pstrdup(s.data);
  }

  pfree(s.data);
}

   
                                                     
   
                                                                                
                                                                         
               
   
                                                                        
                                                                               
                                                                            
         
   
void
InitializeMaxBackends(void)
{
  Assert(MaxBackends == 0);

                                                           
  MaxBackends = MaxConnections + autovacuum_max_workers + 1 + max_worker_processes + max_wal_senders;

                                                                     
  if (MaxBackends > MAX_BACKENDS)
  {
    elog(ERROR, "too many backends configured");
  }
}

   
                                                                              
                                          
   
                                                                             
                                                                        
                        
   
void
BaseInit(void)
{
     
                                                                
                                              
     
  InitCommunication();
  DebugFileOpen();

                                                                    
  InitFileAccess();
  InitSync();
  smgrinit();
  InitBufferPoolAccess();
}

                                    
                
                         
   
                                                                               
                                                                            
                                                                          
                                                        
   
                                                                                
                                          
   
                                                                              
                                                                            
                                                                            
                                            
   
                                                                           
                                                                           
   
         
                                                                          
                                    
   
void
InitPostgres(const char *in_dbname, Oid dboid, const char *username, Oid useroid, char *out_dbname, bool override_allow_connections)
{
  bool bootstrap = IsBootstrapProcessingMode();
  bool am_superuser;
  char *fullpath;
  char dbname[NAMEDATALEN];

  elog(DEBUG3, "InitPostgres");

     
                                            
     
                                                            
     
  InitProcessPhase2();

     
                                                                       
                       
     
                                                       
     
  MyBackendId = InvalidBackendId;

  SharedInvalBackendInit(false);

  if (MyBackendId > MaxBackends || MyBackendId <= 0)
  {
    elog(FATAL, "bad backend ID: %d", MyBackendId);
  }

                                                                      
  ProcSignalInit(MyBackendId);

     
                                                                         
                                           
     
  if (!bootstrap)
  {
    RegisterTimeout(DEADLOCK_TIMEOUT, CheckDeadLockAlert);
    RegisterTimeout(STATEMENT_TIMEOUT, StatementTimeoutHandler);
    RegisterTimeout(LOCK_TIMEOUT, LockTimeoutHandler);
    RegisterTimeout(IDLE_IN_TRANSACTION_SESSION_TIMEOUT, IdleInTransactionSessionTimeoutHandler);
  }

     
                                                  
     
  InitBufferPoolBackend();

     
                                                
     
  if (IsUnderPostmaster)
  {
       
                                                                         
                                                                       
                                                                      
               
       
    (void)RecoveryInProgress();
  }
  else
  {
       
                                                                         
                                                                        
                     
       
                                                                        
                                                                          
                                                                         
       
    CreateAuxProcessResourceOwner();

    StartupXLOG();
                                                                        
    ReleaseAuxProcessResources(true);
                                                              
    CurrentResourceOwner = NULL;

    on_shmem_exit(ShutdownXLOG, 0);
  }

     
                                                                             
                                                                             
                                                                             
                                          
     
  RelationCacheInitialize();
  InitCatalogCache();
  InitPlanCache();

                                 
  EnablePortalManager();

                                                                     
  if (!bootstrap)
  {
    pgstat_initialize();
  }

     
                                                                             
                                                                            
     
  RelationCacheInitializePhase2();

     
                                                                           
                                                                          
                                                                             
                                                                             
                                                                             
                                                                       
     
  before_shmem_exit(ShutdownPostgres, 0);

                                            
  if (IsAutoVacuumLauncherProcess())
  {
                                                          
    pgstat_bestart();

    return;
  }

     
                                                                       
                                                                       
                                                                             
                                                                            
                                                                     
              
     
  if (!bootstrap)
  {
                                                                        
    SetCurrentStatementStartTimestamp();
    StartTransactionCommand();

       
                                                                      
                                                                   
                                                                       
                                                            
       
    XactIsoLevel = XACT_READ_COMMITTED;

    (void)GetTransactionSnapshot();
  }

     
                                                                     
                                                      
     
                                                                           
                                                                      
     
  if (bootstrap || IsAutoVacuumWorkerProcess())
  {
    InitializeSessionUserIdStandalone();
    am_superuser = true;
  }
  else if (!IsUnderPostmaster)
  {
    InitializeSessionUserIdStandalone();
    am_superuser = true;
    if (!ThereIsAtLeastOneRole())
    {
      ereport(WARNING, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("no roles are defined in this database system"), errhint("You should immediately run CREATE USER \"%s\" SUPERUSER;.", username != NULL ? username : "postgres")));
    }
  }
  else if (IsBackgroundWorker)
  {
    if (username == NULL && !OidIsValid(useroid))
    {
      InitializeSessionUserIdStandalone();
      am_superuser = true;
    }
    else
    {
      InitializeSessionUserId(username, useroid);
      am_superuser = superuser();
    }
  }
  else
  {
                               
    Assert(MyProcPort != NULL);
    PerformAuthentication(MyProcPort);
    InitializeSessionUserId(username, useroid);
    am_superuser = superuser();
  }

     
                                                                        
                                              
     
  if ((!am_superuser || am_walsender) && MyProcPort != NULL && MyProcPort->canAcceptConnections == CAC_SUPERUSER)
  {
    if (am_walsender)
    {
      ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("new replication connections are not allowed during database shutdown")));
    }
    else
    {
      ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to connect during database shutdown")));
    }
  }

     
                                                         
     
  if (IsBinaryUpgrade && !am_superuser)
  {
    ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to connect in binary upgrade mode")));
  }

     
                                                                             
                                                                            
                                                                   
     
  if (!am_superuser && !am_walsender && ReservedBackends > 0 && !HaveNFreeProcs(ReservedBackends))
  {
    ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("remaining connection slots are reserved for non-replication superuser connections")));
  }

                                                                     
  if (am_walsender)
  {
    Assert(!bootstrap);

    if (!superuser() && !has_rolreplication(GetUserId()))
    {
      ereport(FATAL, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser or replication role to start walsender")));
    }
  }

     
                                                                           
                                                                       
                                                                            
                 
     
  if (am_walsender && !am_db_walsender)
  {
                                                          
    if (MyProcPort != NULL)
    {
      process_startup_options(MyProcPort, am_superuser);
    }

                                                               
    if (PostAuthDelay > 0)
    {
      pg_usleep(PostAuthDelay * 1000000L);
    }

                                    
    InitializeClientEncoding();

                                                          
    pgstat_bestart();

                                                
    CommitTransactionCommand();

    return;
  }

     
                                                                             
                                                                    
     
                                                                            
                                    
     
  if (bootstrap)
  {
    MyDatabaseId = TemplateDbOid;
    MyDatabaseTableSpace = DEFAULTTABLESPACE_OID;
  }
  else if (in_dbname != NULL)
  {
    HeapTuple tuple;
    Form_pg_database dbform;

    tuple = GetDatabaseTuple(in_dbname);
    if (!HeapTupleIsValid(tuple))
    {
      ereport(FATAL, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", in_dbname)));
    }
    dbform = (Form_pg_database)GETSTRUCT(tuple);
    MyDatabaseId = dbform->oid;
    MyDatabaseTableSpace = dbform->dattablespace;
                                                               
    strlcpy(dbname, in_dbname, sizeof(dbname));
  }
  else if (OidIsValid(dboid))
  {
                                          
    HeapTuple tuple;
    Form_pg_database dbform;

    tuple = GetDatabaseTupleByOid(dboid);
    if (!HeapTupleIsValid(tuple))
    {
      ereport(FATAL, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database %u does not exist", dboid)));
    }
    dbform = (Form_pg_database)GETSTRUCT(tuple);
    MyDatabaseId = dbform->oid;
    MyDatabaseTableSpace = dbform->dattablespace;
    Assert(MyDatabaseId == dboid);
    strlcpy(dbname, NameStr(dbform->datname), sizeof(dbname));
                                                   
    if (out_dbname)
    {
      strcpy(out_dbname, dbname);
    }
  }
  else
  {
       
                                                                  
                                                                           
                                                                        
                                                
       
    if (!bootstrap)
    {
      pgstat_bestart();
      CommitTransactionCommand();
    }
    return;
  }

     
                                                                            
                                                                             
                                                                      
                   
     
                                                                             
                                                                     
                                                                             
                                                                             
                                                                             
                                                                        
                                                                            
                                           
     
                                                                          
                                                                            
                                                                            
                                                                        
                      
     
  if (!bootstrap)
  {
    LockSharedObject(DatabaseRelationId, MyDatabaseId, 0, RowExclusiveLock);
  }

     
                                                            
     
                                                                             
                                                                             
                                                                             
                                                                             
                                                                             
                                                                            
                                                                           
                                          
     
  MyProc->databaseId = MyDatabaseId;

     
                                                                      
                                                                           
                                                                            
                                                                         
     
  InvalidateCatalogSnapshot();

     
                                                                            
                                                                       
                                      
     
  if (!bootstrap)
  {
    HeapTuple tuple;

    tuple = GetDatabaseTuple(dbname);
    if (!HeapTupleIsValid(tuple) || MyDatabaseId != ((Form_pg_database)GETSTRUCT(tuple))->oid || MyDatabaseTableSpace != ((Form_pg_database)GETSTRUCT(tuple))->dattablespace)
    {
      ereport(FATAL, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname), errdetail("It seems to have just been dropped or renamed.")));
    }
  }

     
                                                                           
                                      
     
  fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);

  if (!bootstrap)
  {
    if (access(fullpath, F_OK) == -1)
    {
      if (errno == ENOENT)
      {
        ereport(FATAL, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname), errdetail("The database subdirectory \"%s\" is missing.", fullpath)));
      }
      else
      {
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not access directory \"%s\": %m", fullpath)));
      }
    }

    ValidatePgVersion(fullpath);
  }

  SetDatabasePath(fullpath);

     
                                                                 
     
                                                                         
                                                         
     
  RelationCacheInitializePhase3();

                                                                       
  initialize_acl();

     
                                                                             
                                                                        
                                                                           
                                                                   
     
  if (!bootstrap)
  {
    CheckMyDatabase(dbname, am_superuser, override_allow_connections);
  }

     
                                                                           
                                                                         
                                                      
     
  if (MyProcPort != NULL)
  {
    process_startup_options(MyProcPort, am_superuser);
  }

                                          
  process_settings(MyDatabaseId, GetSessionUserId());

                                                             
  if (PostAuthDelay > 0)
  {
    pg_usleep(PostAuthDelay * 1000000L);
  }

     
                                                                        
                                                                 
     

                                         
  InitializeSearchPath();

                                  
  InitializeClientEncoding();

                                                
  InitializeSession();

                                                        
  if (!bootstrap)
  {
    pgstat_bestart();
  }

                                              
  if (!bootstrap)
  {
    CommitTransactionCommand();
  }
}

   
                                                                     
                                          
   
static void
process_startup_options(Port *port, bool am_superuser)
{
  GucContext gucctx;
  ListCell *gucopts;

  gucctx = am_superuser ? PGC_SU_BACKEND : PGC_BACKEND;

     
                                                                       
                                                     
     
  if (port->cmdline_options != NULL)
  {
       
                                                                       
                                                                
                        
       
    char **av;
    int maxac;
    int ac;

    maxac = 2 + (strlen(port->cmdline_options) + 1) / 2;

    av = (char **)palloc(maxac * sizeof(char *));
    ac = 0;

    av[ac++] = "postgres";

    pg_split_opts(av, &ac, port->cmdline_options);

    av[ac] = NULL;

    Assert(ac < maxac);

    (void)process_postgres_switches(ac, av, gucctx, NULL);
  }

     
                                                                            
                                                            
     
  gucopts = list_head(port->guc_options);
  while (gucopts)
  {
    char *name;
    char *value;

    name = lfirst(gucopts);
    gucopts = lnext(gucopts);

    value = lfirst(gucopts);
    gucopts = lnext(gucopts);

    SetConfigOption(name, value, gucctx, PGC_S_CLIENT);
  }
}

   
                                              
   
                                                                          
                                                
   
static void
process_settings(Oid databaseid, Oid roleid)
{
  Relation relsetting;
  Snapshot snapshot;

  if (!IsUnderPostmaster)
  {
    return;
  }

  relsetting = table_open(DbRoleSettingRelationId, AccessShareLock);

                                                                    
  snapshot = RegisterSnapshot(GetCatalogSnapshot(DbRoleSettingRelationId));

                                                  
  ApplySetting(snapshot, databaseid, roleid, relsetting, PGC_S_DATABASE_USER);
  ApplySetting(snapshot, InvalidOid, roleid, relsetting, PGC_S_USER);
  ApplySetting(snapshot, databaseid, InvalidOid, relsetting, PGC_S_DATABASE);
  ApplySetting(snapshot, InvalidOid, InvalidOid, relsetting, PGC_S_GLOBAL);

  UnregisterSnapshot(snapshot);
  table_close(relsetting, AccessShareLock);
}

   
                                                                          
                                                                        
                        
   
                                                                           
                                                                              
                                                                        
                  
   
static void
ShutdownPostgres(int code, Datum arg)
{
                                                     
  AbortOutOfAnyTransaction();

     
                                                                           
                      
     
  LockReleaseAll(USER_LOCKMETHOD, true);
}

   
                                                                
   
static void
StatementTimeoutHandler(void)
{
  int sig = SIGINT;

     
                                                            
                                                                            
     
  if (ClientAuthInProgress)
  {
    sig = SIGTERM;
  }

#ifdef HAVE_SETSID
                                         
  kill(-MyProcPid, sig);
#endif
  kill(MyProcPid, sig);
}

   
                                                           
   
static void
LockTimeoutHandler(void)
{
#ifdef HAVE_SETSID
                                         
  kill(-MyProcPid, SIGINT);
#endif
  kill(MyProcPid, SIGINT);
}

static void
IdleInTransactionSessionTimeoutHandler(void)
{
  IdleInTransactionSessionTimeoutPending = true;
  InterruptPending = true;
  SetLatch(MyLatch);
}

   
                                                                          
   
static bool
ThereIsAtLeastOneRole(void)
{
  Relation pg_authid_rel;
  TableScanDesc scan;
  bool result;

  pg_authid_rel = table_open(AuthIdRelationId, AccessShareLock);

  scan = table_beginscan_catalog(pg_authid_rel, 0, NULL);
  result = (heap_getnext(scan, ForwardScanDirection) != NULL);

  table_endscan(scan);
  table_close(pg_authid_rel, AccessShareLock);

  return result;
}
