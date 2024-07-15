                                                                            
   
              
                                                
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <sys/param.h>
#include <signal.h>
#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#include "access/htup_details.h"
#include "catalog/pg_authid.h"
#include "common/file_perm.h"
#include "libpq/libpq.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/postmaster.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/pidfile.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

#define DIRECTORY_LOCK_FILE "postmaster.pid"

ProcessingMode Mode = InitProcessing;

                                                   
static List *lock_files = NIL;

static Latch LocalLatchData;

                                                                    
                                          
   
                                                                          
                                                                             
                                                                    
                         
                                                                    
   

bool IgnoreSystemIndexes = false;

                                                                    
                                         
                                                                    
   

void
SetDatabasePath(const char *path)
{
                                                
  Assert(!DatabasePath);
  DatabasePath = MemoryContextStrdup(TopMemoryContext, path);
}

   
                                         
   
                                                                  
   
void
checkDataDir(void)
{
  struct stat stat_buf;

  Assert(DataDir);

  if (stat(DataDir, &stat_buf) != 0)
  {
    if (errno == ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("data directory \"%s\" does not exist", DataDir)));
    }
    else
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not read permissions of directory \"%s\": %m", DataDir)));
    }
  }

                                                            
  if (!S_ISDIR(stat_buf.st_mode))
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("specified data directory \"%s\" is not a directory", DataDir)));
  }

     
                                                                    
     
                                                                        
                                                                             
                                 
     
                                                     
     
#if !defined(WIN32) && !defined(__CYGWIN__)
  if (stat_buf.st_uid != geteuid())
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("data directory \"%s\" has wrong ownership", DataDir), errhint("The server must be started by the user that owns the data directory.")));
  }
#endif

     
                                                                      
     
                                                                          
                                                                      
                                    
     
                                                                           
                                                                        
                                           
     
#if !defined(WIN32) && !defined(__CYGWIN__)
  if (stat_buf.st_mode & PG_MODE_MASK_GROUP)
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("data directory \"%s\" has invalid permissions", DataDir), errdetail("Permissions should be u=rwx (0700) or u=rwx,g=rx (0750).")));
  }
#endif

     
                                                                            
     
                                                                          
                                                                          
                                                                             
                                                                            
                                      
     
                                                                           
                              
     
#if !defined(WIN32) && !defined(__CYGWIN__)
  SetDataDirectoryCreatePerm(stat_buf.st_mode);

  umask(pg_mode_mask);
  data_directory_mode = pg_dir_create_mode;
#endif

                            
  ValidatePgVersion(DataDir);
}

   
                                                                       
                               
   
void
SetDataDir(const char *dir)
{
  char *new;

  AssertArg(dir);

                                                          
  new = make_absolute_path(dir);

  if (DataDir)
  {
    free(DataDir);
  }
  DataDir = new;
}

   
                                                                            
                                                                              
                                                                       
                                                                        
   
void
ChangeToDataDir(void)
{
  AssertState(DataDir);

  if (chdir(DataDir) < 0)
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not change directory to \"%s\": %m", DataDir)));
  }
}

                                                                    
                 
   
                                                                         
                 
   
                                                                            
   
                                                                          
                                                                           
                                                             
   
                                                                              
                                                                               
                                                                      
                                                             
   
                                                                          
                                                                           
                                                                       
                                                                       
   
                                                                            
                                                                          
                                                                      
                            
                                                                    
   
static Oid AuthenticatedUserId = InvalidOid;
static Oid SessionUserId = InvalidOid;
static Oid OuterUserId = InvalidOid;
static Oid CurrentUserId = InvalidOid;

                                                                          
static bool AuthenticatedUserIsSuperuser = false;
static bool SessionUserIsSuperuser = false;

static int SecurityRestrictionContext = 0;

                                                        
static bool SetRoleIsActive = false;

   
                                                           
   
                                                                    
   
void
InitPostmasterChild(void)
{
  IsUnderPostmaster = true;                                         

     
                                                                    
                                                                            
                                                                           
                           
     
  (void)set_stack_base();

  InitProcessGlobals();

     
                                                                        
                                                                          
                                                                            
                                              
     
#ifdef WIN32
  _setmode(fileno(stderr), _O_BINARY);
#endif

                                                           
  on_exit_reset();

                                              
  InitializeLatchSupport();
  MyLatch = &LocalLatchData;
  InitLatch(MyLatch);

     
                                                                           
                                                                     
                                                                             
           
     
#ifdef HAVE_SETSID
  if (setsid() < 0)
  {
    elog(FATAL, "setsid() failed: %m");
  }
#endif

                                                             
  PostmasterDeathSignalInit();
}

   
                                                              
   
                                                              
   
void
InitStandaloneProcess(const char *argv0)
{
  Assert(!IsPostmasterEnvironment);

  InitProcessGlobals();

                                              
  InitializeLatchSupport();
  MyLatch = &LocalLatchData;
  InitLatch(MyLatch);

                                                    
  if (my_exec_path[0] == '\0')
  {
    if (find_my_exec(argv0, my_exec_path) < 0)
    {
      elog(FATAL, "%s: could not locate my own executable path", argv0);
    }
  }

  if (pkglib_path[0] == '\0')
  {
    get_pkglib_path(my_exec_path, pkglib_path);
  }
}

void
SwitchToSharedLatch(void)
{
  Assert(MyLatch == &LocalLatchData);
  Assert(MyProc != NULL);

  MyLatch = &MyProc->procLatch;

  if (FeBeWaitSet)
  {
    ModifyWaitEvent(FeBeWaitSet, 1, WL_LATCH_SET, MyLatch);
  }

     
                                                                     
                                                                      
                                                                        
     
  SetLatch(MyLatch);
}

void
SwitchBackToLocalLatch(void)
{
  Assert(MyLatch != &LocalLatchData);
  Assert(MyProc != NULL && MyLatch == &MyProc->procLatch);

  MyLatch = &LocalLatchData;

  if (FeBeWaitSet)
  {
    ModifyWaitEvent(FeBeWaitSet, 1, WL_LATCH_SET, MyLatch);
  }

  SetLatch(MyLatch);
}

   
                                                  
   
                                                                       
   
Oid
GetUserId(void)
{
  AssertState(OidIsValid(CurrentUserId));
  return CurrentUserId;
}

   
                                                                    
   
Oid
GetOuterUserId(void)
{
  AssertState(OidIsValid(OuterUserId));
  return OuterUserId;
}

static void
SetOuterUserId(Oid userid)
{
  AssertState(SecurityRestrictionContext == 0);
  AssertArg(OidIsValid(userid));
  OuterUserId = userid;

                                                    
  CurrentUserId = userid;
}

   
                                                                    
   
Oid
GetSessionUserId(void)
{
  AssertState(OidIsValid(SessionUserId));
  return SessionUserId;
}

static void
SetSessionUserId(Oid userid, bool is_superuser)
{
  AssertState(SecurityRestrictionContext == 0);
  AssertArg(OidIsValid(userid));
  SessionUserId = userid;
  SessionUserIsSuperuser = is_superuser;
  SetRoleIsActive = false;

                                                     
  OuterUserId = userid;
  CurrentUserId = userid;
}

   
                                                          
   
Oid
GetAuthenticatedUserId(void)
{
  AssertState(OidIsValid(AuthenticatedUserId));
  return AuthenticatedUserId;
}

   
                                                                               
                                             
   
                                                                       
   
                                                                          
                                                                            
                                                                            
                                                                  
   
                                                                           
                                                                          
                                                                              
                                                                            
                                                                               
                                                                               
                                                                            
                                                                            
                                                                           
                                                                               
                                                                             
                                                                               
                                                                              
                                                                            
                                                                              
                                                  
   
                                                                               
                                                                              
                                                                         
                                                                             
                                                                       
                      
   
                                                                               
                                                                            
                                                                      
                                                                   
                                                                       
                                                                            
                                                                      
                                                                          
   
void
GetUserIdAndSecContext(Oid *userid, int *sec_context)
{
  *userid = CurrentUserId;
  *sec_context = SecurityRestrictionContext;
}

void
SetUserIdAndSecContext(Oid userid, int sec_context)
{
  CurrentUserId = userid;
  SecurityRestrictionContext = sec_context;
}

   
                                                                        
   
bool
InLocalUserIdChange(void)
{
  return (SecurityRestrictionContext & SECURITY_LOCAL_USERID_CHANGE) != 0;
}

   
                                                                                
   
bool
InSecurityRestrictedOperation(void)
{
  return (SecurityRestrictionContext & SECURITY_RESTRICTED_OPERATION) != 0;
}

   
                                                                      
   
bool
InNoForceRLSOperation(void)
{
  return (SecurityRestrictionContext & SECURITY_NOFORCE_RLS) != 0;
}

   
                                                                      
                                                                        
                                                                      
                                 
   
void
GetUserIdAndContext(Oid *userid, bool *sec_def_context)
{
  *userid = CurrentUserId;
  *sec_def_context = InLocalUserIdChange();
}

void
SetUserIdAndContext(Oid userid, bool sec_def_context)
{
                                               
  if (InSecurityRestrictedOperation())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("cannot set parameter \"%s\" within security-restricted operation", "role")));
  }
  CurrentUserId = userid;
  if (sec_def_context)
  {
    SecurityRestrictionContext |= SECURITY_LOCAL_USERID_CHANGE;
  }
  else
  {
    SecurityRestrictionContext &= ~SECURITY_LOCAL_USERID_CHANGE;
  }
}

   
                                                                   
   
bool
has_rolreplication(Oid roleid)
{
  bool result = false;
  HeapTuple utup;

  utup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (HeapTupleIsValid(utup))
  {
    result = ((Form_pg_authid)GETSTRUCT(utup))->rolreplication;
    ReleaseSysCache(utup);
  }
  return result;
}

   
                                                          
   
void
InitializeSessionUserId(const char *rolename, Oid roleid)
{
  HeapTuple roleTup;
  Form_pg_authid rform;
  char *rname;

     
                                                                        
                                                             
     
  AssertState(!IsBootstrapProcessingMode());

                      
  AssertState(!OidIsValid(AuthenticatedUserId));

     
                                                                             
                                                                 
                     
     
  AcceptInvalidationMessages();

  if (rolename != NULL)
  {
    roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(rolename));
    if (!HeapTupleIsValid(roleTup))
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("role \"%s\" does not exist", rolename)));
    }
  }
  else
  {
    roleTup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
    if (!HeapTupleIsValid(roleTup))
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("role with OID %u does not exist", roleid)));
    }
  }

  rform = (Form_pg_authid)GETSTRUCT(roleTup);
  roleid = rform->oid;
  rname = NameStr(rform->rolname);

  AuthenticatedUserId = roleid;
  AuthenticatedUserIsSuperuser = rform->rolsuper;

                                               
  SetSessionUserId(roleid, AuthenticatedUserIsSuperuser);

                                                                 
                                                                
  MyProc->roleId = roleid;

     
                                                                         
                                                                           
                            
     
  if (IsUnderPostmaster)
  {
       
                                        
       
    if (!rform->rolcanlogin)
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("role \"%s\" is not permitted to log in", rname)));
    }

       
                                             
       
                                                                      
                                                                          
                                                                        
                                                                      
                                                                        
                                                               
       
    if (rform->rolconnlimit >= 0 && !AuthenticatedUserIsSuperuser && CountUserBackends(roleid) > rform->rolconnlimit)
    {
      ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("too many connections for role \"%s\"", rname)));
    }
  }

                                                                
  SetConfigOption("session_authorization", rname, PGC_BACKEND, PGC_S_OVERRIDE);
  SetConfigOption("is_superuser", AuthenticatedUserIsSuperuser ? "on" : "off", PGC_INTERNAL, PGC_S_OVERRIDE);

  ReleaseSysCache(roleTup);
}

   
                                                           
   
void
InitializeSessionUserIdStandalone(void)
{
     
                                                                            
                                         
     
  AssertState(!IsUnderPostmaster || IsAutoVacuumWorkerProcess() || IsBackgroundWorker);

                      
  AssertState(!OidIsValid(AuthenticatedUserId));

  AuthenticatedUserId = BOOTSTRAP_SUPERUSERID;
  AuthenticatedUserIsSuperuser = true;

  SetSessionUserId(BOOTSTRAP_SUPERUSERID, true);
}

   
                                        
   
                                                                           
                                                                            
                                                                            
                                                                    
   
                                                                           
                                                                       
                                                                       
                                                         
   
void
SetSessionAuthorization(Oid userid, bool is_superuser)
{
                                                                         
  AssertState(OidIsValid(AuthenticatedUserId));

  if (userid != AuthenticatedUserId && !AuthenticatedUserIsSuperuser)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to set session authorization")));
  }

  SetSessionUserId(userid, is_superuser);

  SetConfigOption("is_superuser", is_superuser ? "on" : "off", PGC_INTERNAL, PGC_S_OVERRIDE);
}

   
                          
                                                                         
                                                                         
                                
   
Oid
GetCurrentRoleId(void)
{
  if (SetRoleIsActive)
  {
    return OuterUserId;
  }
  else
  {
    return InvalidOid;
  }
}

   
                                           
   
                                                                      
                                                                       
               
   
                                                                       
                                                                          
                                                                             
                                                       
   
void
SetCurrentRoleId(Oid roleid, bool is_superuser)
{
     
                                            
     
                                                                            
                                                                         
                                                
     
  if (!OidIsValid(roleid))
  {
    if (!OidIsValid(SessionUserId))
    {
      return;
    }

    roleid = SessionUserId;
    is_superuser = SessionUserIsSuperuser;

    SetRoleIsActive = false;
  }
  else
  {
    SetRoleIsActive = true;
  }

  SetOuterUserId(roleid);

  SetConfigOption("is_superuser", is_superuser ? "on" : "off", PGC_INTERNAL, PGC_S_OVERRIDE);
}

   
                                                                             
            
   
char *
GetUserNameFromId(Oid roleid, bool noerr)
{
  HeapTuple tuple;
  char *result;

  tuple = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (!HeapTupleIsValid(tuple))
  {
    if (!noerr)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid role OID: %u", roleid)));
    }
    result = NULL;
  }
  else
  {
    result = pstrdup(NameStr(((Form_pg_authid)GETSTRUCT(tuple))->rolname));
    ReleaseSysCache(tuple);
  }
  return result;
}

                                                                            
                             
   
                                                                    
                                                                              
                                                                            
                                                                           
                                                                            
                       
   
                                                                       
                                      
                                                                            
   

   
                                           
   
static void
UnlinkLockFiles(int status, Datum arg)
{
  ListCell *l;

  foreach (l, lock_files)
  {
    char *curfile = (char *)lfirst(l);

    unlink(curfile);
                                                 
  }
                                                             
  lock_files = NIL;

     
                                                                           
                                                                            
                                                                         
                                                                        
                                                                            
                                          
     
  ereport(IsPostmasterEnvironment ? LOG : NOTICE, (errmsg("database system is shut down")));
}

   
                      
   
                                                        
                                                                   
                                                                            
                                                                             
   
static void
CreateLockFile(const char *filename, bool amPostmaster, const char *socketDir, bool isDDLock, const char *refName)
{
  int fd;
  char buffer[MAXPGPATH * 2 + 256];
  int ntries;
  int len;
  int encoded_pid;
  pid_t other_pid;
  pid_t my_pid, my_p_pid, my_gp_pid;
  const char *envvar;

     
                                                                  
                                                                             
                                                                          
                                                                            
                                                                          
                                                                           
                                                                          
                                                                      
                                                                    
                                                                 
                                                                           
                                                                          
                                                                        
                                                                          
                                                                        
               
     
  my_pid = getpid();

#ifndef WIN32
  my_p_pid = getppid();
#else

     
                                                                            
                           
     
  my_p_pid = 0;
#endif

  envvar = getenv("PG_GRANDPARENT_PID");
  if (envvar)
  {
    my_gp_pid = atoi(envvar);
  }
  else
  {
    my_gp_pid = 0;
  }

     
                                                                             
                                                                          
                                                        
     
  for (ntries = 0;; ntries++)
  {
       
                                                                 
       
                                                                         
                       
       
    fd = open(filename, O_RDWR | O_CREAT | O_EXCL, pg_file_create_mode);
    if (fd >= 0)
    {
      break;                                   
    }

       
                                                                 
       
    if ((errno != EEXIST && errno != EACCES) || ntries > 100)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not create lock file \"%s\": %m", filename)));
    }

       
                                                                      
                                                                       
       
    fd = open(filename, O_RDONLY, pg_file_create_mode);
    if (fd < 0)
    {
      if (errno == ENOENT)
      {
        continue;                                
      }
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not open lock file \"%s\": %m", filename)));
    }
    pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_CREATE_READ);
    if ((len = read(fd, buffer, sizeof(buffer) - 1)) < 0)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not read lock file \"%s\": %m", filename)));
    }
    pgstat_report_wait_end();
    close(fd);

    if (len == 0)
    {
      ereport(FATAL, (errcode(ERRCODE_LOCK_FILE_EXISTS), errmsg("lock file \"%s\" is empty", filename), errhint("Either another server is starting, or the lock file is the remnant of a previous server startup crash.")));
    }

    buffer[len] = '\0';
    encoded_pid = atoi(buffer);

                                                             
    other_pid = (pid_t)(encoded_pid < 0 ? -encoded_pid : encoded_pid);

    if (other_pid <= 0)
    {
      elog(FATAL, "bogus data in lock file \"%s\": \"%s\"", filename, buffer);
    }

       
                                                      
       
                                                                    
                                 
       
                                                                     
              
       
                                                                    
                                                                        
                                                                          
                                                                        
                                                                         
                                                                          
                                                                         
                                                                     
                                                                      
                                                                     
                                                                   
                                                    
       
    if (other_pid != my_pid && other_pid != my_p_pid && other_pid != my_gp_pid)
    {
      if (kill(other_pid, 0) == 0 || (errno != ESRCH && errno != EPERM))
      {
                                                
        ereport(FATAL, (errcode(ERRCODE_LOCK_FILE_EXISTS), errmsg("lock file \"%s\" already exists", filename), isDDLock ? (encoded_pid < 0 ? errhint("Is another postgres (PID %d) running in data directory \"%s\"?", (int)other_pid, refName) : errhint("Is another postmaster (PID %d) running in data directory \"%s\"?", (int)other_pid, refName)) : (encoded_pid < 0 ? errhint("Is another postgres (PID %d) using socket file \"%s\"?", (int)other_pid, refName) : errhint("Is another postmaster (PID %d) using socket file \"%s\"?", (int)other_pid, refName))));
      }
    }

       
                                                                          
                                                                          
                                                                      
                                                                      
                     
       
                                                                           
                                                                     
              
       
    if (isDDLock)
    {
      char *ptr = buffer;
      unsigned long id1, id2;
      int lineno;

      for (lineno = 1; lineno < LOCK_FILE_LINE_SHMEM_KEY; lineno++)
      {
        if ((ptr = strchr(ptr, '\n')) == NULL)
        {
          break;
        }
        ptr++;
      }

      if (ptr != NULL && sscanf(ptr, "%lu %lu", &id1, &id2) == 2)
      {
        if (PGSharedMemoryIsInUse(id1, id2))
        {
          ereport(FATAL, (errcode(ERRCODE_LOCK_FILE_EXISTS), errmsg("pre-existing shared memory block (key %lu, ID %lu) is still in use", id1, id2), errhint("Terminate any old server processes associated with data directory \"%s\".", refName)));
        }
      }
    }

       
                                                                          
                                                                         
                          
       
    if (unlink(filename) < 0)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not remove old lock file \"%s\": %m", filename),
                         errhint("The file seems accidentally left over, but "
                                 "it could not be removed. Please remove the file "
                                 "by hand and try again.")));
    }
  }

     
                                                                           
                                                                            
                                                                             
                                 
     
  snprintf(buffer, sizeof(buffer), "%d\n%s\n%ld\n%d\n%s\n", amPostmaster ? (int)my_pid : -((int)my_pid), DataDir, (long)MyStartTime, PostPortNumber, socketDir);

     
                                                                         
                                                          
     
  if (isDDLock && !amPostmaster)
  {
    strlcat(buffer, "\n", sizeof(buffer));
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_CREATE_WRITE);
  if (write(fd, buffer, strlen(buffer)) != strlen(buffer))
  {
    int save_errno = errno;

    close(fd);
    unlink(filename);
                                                                    
    errno = save_errno ? save_errno : ENOSPC;
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not write lock file \"%s\": %m", filename)));
  }
  pgstat_report_wait_end();

  pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_CREATE_SYNC);
  if (pg_fsync(fd) != 0)
  {
    int save_errno = errno;

    close(fd);
    unlink(filename);
    errno = save_errno;
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not write lock file \"%s\": %m", filename)));
  }
  pgstat_report_wait_end();
  if (close(fd) != 0)
  {
    int save_errno = errno;

    unlink(filename);
    errno = save_errno;
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not write lock file \"%s\": %m", filename)));
  }

     
                                                                            
                                                                             
                                     
     
  if (lock_files == NIL)
  {
    on_proc_exit(UnlinkLockFiles, 0);
  }

     
                                                                       
                                 
     
  lock_files = lcons(pstrdup(filename), lock_files);
}

   
                                       
   
                                                                  
                                                                   
                                                                
   
                                                                           
                                                                     
   
void
CreateDataDirLockFile(bool amPostmaster)
{
  CreateLockFile(DIRECTORY_LOCK_FILE, amPostmaster, "", true, DataDir);
}

   
                                                         
   
void
CreateSocketLockFile(const char *socketfile, bool amPostmaster, const char *socketDir)
{
  char lockfile[MAXPGPATH];

  snprintf(lockfile, sizeof(lockfile), "%s.lock", socketfile);
  CreateLockFile(lockfile, amPostmaster, socketDir, false, socketfile);
}

   
                                                                       
   
                                                                          
                                                                 
                                                                          
                                                                        
   
void
TouchSocketLockFiles(void)
{
  ListCell *l;

  foreach (l, lock_files)
  {
    char *socketLockFile = (char *)lfirst(l);

                                                                 
    if (strcmp(socketLockFile, DIRECTORY_LOCK_FILE) == 0)
    {
      continue;
    }

       
                                                                          
                                                                        
                                                                       
                                                     
       
#ifdef HAVE_UTIME
    utime(socketLockFile, NULL);
#else                  
#ifdef HAVE_UTIMES
    utimes(socketLockFile, NULL);
#else                    
    int fd;
    char buffer[1];

    fd = open(socketLockFile, O_RDONLY | PG_BINARY, 0);
    if (fd >= 0)
    {
      read(fd, buffer, sizeof(buffer));
      close(fd);
    }
#endif                  
#endif                 
  }
}

   
                                                            
                                                           
   
                                                                          
                                                                            
                                                                             
                                                                            
                                                                   
   
void
AddToDataDirLockFile(int target_line, const char *str)
{
  int fd;
  int len;
  int lineno;
  char *srcptr;
  char *destptr;
  char srcbuffer[BLCKSZ];
  char destbuffer[BLCKSZ];

  fd = open(DIRECTORY_LOCK_FILE, O_RDWR | PG_BINARY, 0);
  if (fd < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", DIRECTORY_LOCK_FILE)));
    return;
  }
  pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_ADDTODATADIR_READ);
  len = read(fd, srcbuffer, sizeof(srcbuffer) - 1);
  pgstat_report_wait_end();
  if (len < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not read from file \"%s\": %m", DIRECTORY_LOCK_FILE)));
    close(fd);
    return;
  }
  srcbuffer[len] = '\0';

     
                                                                          
                 
     
  srcptr = srcbuffer;
  for (lineno = 1; lineno < target_line; lineno++)
  {
    char *eol = strchr(srcptr, '\n');

    if (eol == NULL)
    {
      break;                                   
    }
    srcptr = eol + 1;
  }
  memcpy(destbuffer, srcbuffer, srcptr - srcbuffer);
  destptr = destbuffer + (srcptr - srcbuffer);

     
                                                                         
                                     
     
  for (; lineno < target_line; lineno++)
  {
    if (destptr < destbuffer + sizeof(destbuffer))
    {
      *destptr++ = '\n';
    }
  }

     
                                       
     
  snprintf(destptr, destbuffer + sizeof(destbuffer) - destptr, "%s\n", str);
  destptr += strlen(destptr);

     
                                                                         
     
  if ((srcptr = strchr(srcptr, '\n')) != NULL)
  {
    srcptr++;
    snprintf(destptr, destbuffer + sizeof(destbuffer) - destptr, "%s", srcptr);
  }

     
                                                                         
                                               
     
  len = strlen(destbuffer);
  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_ADDTODATADIR_WRITE);
  if (lseek(fd, (off_t)0, SEEK_SET) != 0 || (int)write(fd, destbuffer, len) != len)
  {
    pgstat_report_wait_end();
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", DIRECTORY_LOCK_FILE)));
    close(fd);
    return;
  }
  pgstat_report_wait_end();
  pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_ADDTODATADIR_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", DIRECTORY_LOCK_FILE)));
  }
  pgstat_report_wait_end();
  if (close(fd) != 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", DIRECTORY_LOCK_FILE)));
  }
}

   
                                                                        
                                                                         
   
                                                                         
                                                                           
                                                                            
                                                                         
                                                                        
                                                                              
   
bool
RecheckDataDirLockFile(void)
{
  int fd;
  int len;
  long file_pid;
  char buffer[BLCKSZ];

  fd = open(DIRECTORY_LOCK_FILE, O_RDWR | PG_BINARY, 0);
  if (fd < 0)
  {
       
                                                                        
                                                                  
                   
       
    switch (errno)
    {
    case ENOENT:
    case ENOTDIR:
                    
      ereport(LOG, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", DIRECTORY_LOCK_FILE)));
      return false;
    default:
                                       
      ereport(LOG, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m; continuing anyway", DIRECTORY_LOCK_FILE)));
      return true;
    }
  }
  pgstat_report_wait_start(WAIT_EVENT_LOCK_FILE_RECHECKDATADIR_READ);
  len = read(fd, buffer, sizeof(buffer) - 1);
  pgstat_report_wait_end();
  if (len < 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not read from file \"%s\": %m", DIRECTORY_LOCK_FILE)));
    close(fd);
    return true;                                     
  }
  buffer[len] = '\0';
  close(fd);
  file_pid = atol(buffer);
  if (file_pid == getpid())
  {
    return true;                  
  }

                                                    
  ereport(LOG, (errmsg("lock file \"%s\" contains wrong PID: %ld instead of %ld", DIRECTORY_LOCK_FILE, file_pid, (long)getpid())));
  return false;
}

                                                                            
                               
                                                                            
   

   
                                                                       
                                                               
   
                                                     
   
void
ValidatePgVersion(const char *path)
{
  char full_path[MAXPGPATH];
  FILE *file;
  int ret;
  long file_major;
  long my_major;
  char *endptr;
  char file_version_string[64];
  const char *my_version_string = PG_VERSION;

  my_major = strtol(my_version_string, &endptr, 10);

  snprintf(full_path, sizeof(full_path), "%s/PG_VERSION", path);

  file = AllocateFile(full_path, "r");
  if (!file)
  {
    if (errno == ENOENT)
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is not a valid data directory", path), errdetail("File \"%s\" is missing.", full_path)));
    }
    else
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", full_path)));
    }
  }

  file_version_string[0] = '\0';
  ret = fscanf(file, "%63s", file_version_string);
  file_major = strtol(file_version_string, &endptr, 10);

  if (ret != 1 || endptr == file_version_string)
  {
    ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is not a valid data directory", path), errdetail("File \"%s\" does not contain valid data.", full_path), errhint("You might need to initdb.")));
  }

  FreeFile(file);

  if (my_major != file_major)
  {
    ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("database files are incompatible with server"),
                       errdetail("The data directory was initialized by PostgreSQL version %s, "
                                 "which is not compatible with this version %s.",
                           file_version_string, my_version_string)));
  }
}

                                                                            
                              
                                                                            
   

   
                                                                       
                              
   
char *session_preload_libraries_string = NULL;
char *shared_preload_libraries_string = NULL;
char *local_preload_libraries_string = NULL;

                                                               
bool process_shared_preload_libraries_in_progress = false;

   
                                                   
   
                                                      
                                                                    
   
static void
load_libraries(const char *libraries, const char *gucname, bool restricted)
{
  char *rawstring;
  List *elemlist;
  ListCell *l;

  if (libraries == NULL || libraries[0] == '\0')
  {
    return;                    
  }

                                        
  rawstring = pstrdup(libraries);

                                                
  if (!SplitDirectoriesString(rawstring, ',', &elemlist))
  {
                              
    list_free_deep(elemlist);
    pfree(rawstring);
    ereport(LOG, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid list syntax in parameter \"%s\"", gucname)));
    return;
  }

  foreach (l, elemlist)
  {
                                                      
    char *filename = (char *)lfirst(l);
    char *expanded = NULL;

                                                                         
    if (restricted && first_dir_separator(filename) == NULL)
    {
      expanded = psprintf("$libdir/plugins/%s", filename);
      filename = expanded;
    }
    load_file(filename, restricted);
    ereport(DEBUG1, (errmsg("loaded library \"%s\"", filename)));
    if (expanded)
    {
      pfree(expanded);
    }
  }

  list_free_deep(elemlist);
  pfree(rawstring);
}

   
                                                                      
   
void
process_shared_preload_libraries(void)
{
  process_shared_preload_libraries_in_progress = true;
  load_libraries(shared_preload_libraries_string, "shared_preload_libraries", false);
  process_shared_preload_libraries_in_progress = false;
}

   
                                                                   
   
void
process_session_preload_libraries(void)
{
  load_libraries(session_preload_libraries_string, "session_preload_libraries", false);
  load_libraries(local_preload_libraries_string, "local_preload_libraries", true);
}

void
pg_bindtextdomain(const char *domain)
{
#ifdef ENABLE_NLS
  if (my_exec_path[0] != '\0')
  {
    char locale_path[MAXPGPATH];

    get_locale_path(my_exec_path, locale_path);
    bindtextdomain(domain, locale_path);
    pg_bind_textdomain_codeset(domain);
  }
#endif
}
