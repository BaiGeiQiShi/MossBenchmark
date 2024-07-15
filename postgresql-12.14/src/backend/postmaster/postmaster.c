                                                                            
   
                
                                                               
                                                                
                                                               
                                         
   
                                                                
                                                                  
                                                                        
                                                                     
                           
   
                                                                    
                                                                     
                                                                       
                                                                       
                                                                        
                                                                         
                                                                     
                                                                          
                              
   
                                                                    
                                                                   
                                                                      
                                                                        
                                                                    
                                                                      
                                                                       
              
   
   
                                                                         
                                                                        
   
   
                  
                                         
   
         
   
                   
                                                         
                      
   
                    
                                                                    
                                                                    
                                                                        
                                                          
   
                       
                                                                      
                           
   
                    
                                                               
                                                                  
                                           
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/param.h>
#include <netdb.h>
#include <limits.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef USE_BONJOUR
#include <dns_sd.h>
#endif

#ifdef USE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#ifdef HAVE_PTHREAD_IS_THREADED_NP
#include <pthread.h>
#endif

#include "access/transam.h"
#include "access/xlog.h"
#include "bootstrap/bootstrap.h"
#include "catalog/pg_control.h"
#include "common/file_perm.h"
#include "common/ip.h"
#include "common/string.h"
#include "lib/ilist.h"
#include "libpq/auth.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pg_getopt.h"
#include "pgstat.h"
#include "port/pg_bswap.h"
#include "postmaster/autovacuum.h"
#include "postmaster/bgworker_internals.h"
#include "postmaster/fork_process.h"
#include "postmaster/pgarch.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "replication/logicallauncher.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/memutils.h"
#include "utils/pidfile.h"
#include "utils/ps_status.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"
#include "utils/varlena.h"

#ifdef EXEC_BACKEND
#include "storage/spin.h"
#endif

   
                                                                               
                                                                              
                        
   
#define BACKEND_TYPE_NORMAL 0x0001                       
#define BACKEND_TYPE_AUTOVAC 0x0002                                 
#define BACKEND_TYPE_WALSND 0x0004                          
#define BACKEND_TYPE_BGWORKER 0x0008                       
#define BACKEND_TYPE_ALL 0x000F                               

   
                                                                         
                                                                      
                                                                        
                                                                      
   
                                                                            
                                                                           
                                                                             
                                                                             
                                                                              
                                                                               
                
   
                                             
   
typedef struct bkend
{
  pid_t pid;                                   
  int32 cancel_key;                                              
  int child_slot;                                             

     
                                                                            
                                                                          
                                                                     
     
  int bkend_type;
  bool dead_end;                                                    
  bool bgworker_notify;                                             
  dlist_node elem;                                    
} Backend;

static dlist_head BackendList = DLIST_STATIC_INIT(BackendList);

#ifdef EXEC_BACKEND
static Backend *ShmemBackendArray;
#endif

BackgroundWorker *MyBgworkerEntry = NULL;

                                                           
int PostPortNumber;

                                            
char *Unix_socket_directories;

                                
char *ListenAddresses;

   
                                                                          
                                                                        
                                                          
                                                                        
                                                                            
                                                                          
                            
   
int ReservedBackends;

                                       
#define MAXLISTEN 64
static pgsocket ListenSocket[MAXLISTEN];

   
                        
   
static char ExtraOptions[MAXPGPATH];

   
                                                                     
                                                                         
                                                                         
                                                                     
                                                                     
   
static bool Reinit = true;
static int SendStop = false;

                                 
bool EnableSSL = false;

int PreAuthDelay = 0;
int AuthenticationTimeout = 60;

bool log_hostname;                                 
bool Log_connections = false;
bool Db_user_namespace = false;

bool enable_bonjour = false;
char *bonjour_name;
bool restart_after_crash = true;

                                                         
static pid_t StartupPID = 0, BgWriterPID = 0, CheckpointerPID = 0, WalWriterPID = 0, WalReceiverPID = 0, AutoVacPID = 0, PgArchPID = 0, PgStatPID = 0, SysLoggerPID = 0;

                              
typedef enum
{
  STARTUP_NOT_RUNNING,
  STARTUP_RUNNING,
  STARTUP_SIGNALED,                                      
  STARTUP_CRASHED
} StartupStatusEnum;

static StartupStatusEnum StartupStatus = STARTUP_NOT_RUNNING;

                            
#define NoShutdown 0
#define SmartShutdown 1
#define FastShutdown 2
#define ImmediateShutdown 3

static int Shutdown = NoShutdown;

static bool FatalError = false;                                         

   
                                                                   
                                                                       
   
                                                                           
                                                                            
                                                                        
                                                                           
                                                                              
                                                                                
                                        
   
                                                                               
                                                                             
                                                                                
                                                                             
                                                                           
                                                                             
                                                                            
                                  
   
                                                                       
                                                                      
                                                                         
                                                                           
                                                                           
                                                                             
                                                                             
                                                                      
                                                                          
                                                                       
                                                                           
                           
   
                                                                         
                                                                          
                                                                               
                                                                             
                                                                              
                                                                               
   
typedef enum
{
  PM_INIT,                                   
  PM_STARTUP,                                           
  PM_RECOVERY,                                    
  PM_HOT_STANDBY,                            
  PM_RUN,                                                 
  PM_STOP_BACKENDS,                                      
  PM_WAIT_BACKENDS,                                        
  PM_SHUTDOWN,                                                 
                              
  PM_SHUTDOWN_2,                                              
                                
  PM_WAIT_DEAD_END,                                            
  PM_NO_CHILDREN                                            
} PMState;

static PMState pmState = PM_INIT;

   
                                                                             
                                                                             
                                                                         
                                                                    
   
typedef enum
{
  ALLOW_ALL_CONNS,                                           
  ALLOW_SUPERUSER_CONNS,                                  
  ALLOW_NO_CONNS                                                 
} ConnsAllowedState;

static ConnsAllowedState connsAllowed = ALLOW_ALL_CONNS;

                                                                            
                                       
static time_t AbortStartTime = 0;

                            
#define SIGKILL_CHILDREN_AFTER_SECS 5

static bool ReachedNormalRunning = false;                                

bool ClientAuthInProgress = false;                        
                                                       

bool redirection_done = false;                                       

                                            
static volatile sig_atomic_t start_autovac_launcher = false;

                                                                      
static volatile bool avlauncher_needs_signal = false;

                                       
static volatile sig_atomic_t WalReceiverRequested = false;

                                                           
static volatile bool StartWorkerNeeded = true;
static volatile bool HaveCrashedWorker = false;

#ifdef USE_SSL
                                                       
static bool LoadedSSL = false;
#endif

#ifdef USE_BONJOUR
static DNSServiceRef bonjour_sdref = NULL;
#endif

   
                                      
   
static void
CloseServerPorts(int status, Datum arg);
static void
unlink_external_pid_file(int status, Datum arg);
static void
getInstallationPaths(const char *argv0);
static void
checkControlFile(void);
static Port *
ConnCreate(int serverFd);
static void
ConnFree(Port *port);
static void
reset_shared(int port);
static void SIGHUP_handler(SIGNAL_ARGS);
static void pmdie(SIGNAL_ARGS);
static void reaper(SIGNAL_ARGS);
static void sigusr1_handler(SIGNAL_ARGS);
static void process_startup_packet_die(SIGNAL_ARGS);
static void process_startup_packet_quickdie(SIGNAL_ARGS);
static void dummy_handler(SIGNAL_ARGS);
static void
StartupPacketTimeoutHandler(void);
static void
CleanupBackend(int pid, int exitstatus);
static bool
CleanupBackgroundWorker(int pid, int exitstatus);
static void
HandleChildCrash(int pid, int exitstatus, const char *procname);
static void
LogChildExit(int lev, const char *procname, int pid, int exitstatus);
static void
PostmasterStateMachine(void);
static void
BackendInitialize(Port *port);
static void
BackendRun(Port *port) pg_attribute_noreturn();
static void
ExitPostmaster(int status) pg_attribute_noreturn();
static int
ServerLoop(void);
static int
BackendStartup(Port *port);
static int
ProcessStartupPacket(Port *port, bool ssl_done, bool gss_done);
static void
SendNegotiateProtocolVersion(List *unrecognized_protocol_options);
static void
processCancelRequest(Port *port, void *pkt);
static int
initMasks(fd_set *rmask);
static void
report_fork_failure_to_client(Port *port, int errnum);
static CAC_state
canAcceptConnections(int backend_type);
static bool
RandomCancelKey(int32 *cancel_key);
static void
signal_child(pid_t pid, int signal);
static bool
SignalSomeChildren(int signal, int targets);
static void
TerminateChildren(int signal);

#define SignalChildren(sig) SignalSomeChildren(sig, BACKEND_TYPE_ALL)

static int
CountChildren(int target);
static bool
assign_backendlist_entry(RegisteredBgWorker *rw);
static void
maybe_start_bgworkers(void);
static bool
CreateOptsFile(int argc, char *argv[], char *fullprogname);
static pid_t
StartChildProcess(AuxProcType type);
static void
StartAutovacuumWorker(void);
static void
MaybeStartWalReceiver(void);
static void
InitPostmasterDeathWatchHandle(void);

   
                                                                    
   
                                                                        
                         
   
#define PgArchStartupAllowed() ((XLogArchivingActive() && pmState == PM_RUN) || (XLogArchivingAlways() && (pmState == PM_RECOVERY || pmState == PM_HOT_STANDBY)))

#ifdef EXEC_BACKEND

#ifdef WIN32
#define WNOHANG 0                                            

static pid_t
waitpid(pid_t pid, int *exitstatus, int options);
static void WINAPI
pgwin32_deadchild_callback(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

static HANDLE win32ChildQueue;

typedef struct
{
  HANDLE waitHandle;
  HANDLE procHandle;
  DWORD procId;
} win32_deadchild_waitinfo;
#endif            

static pid_t
backend_forkexec(Port *port);
static pid_t
internal_forkexec(int argc, char *argv[], Port *port);

                                                                 
#ifdef WIN32
typedef struct
{
  SOCKET origsocket;                                               
                                          
  WSAPROTOCOL_INFO wsainfo;
} InheritableSocket;
#else
typedef int InheritableSocket;
#endif

   
                                                               
   
typedef struct
{
  Port port;
  InheritableSocket portsocket;
  char DataDir[MAXPGPATH];
  pgsocket ListenSocket[MAXLISTEN];
  int32 MyCancelKey;
  int MyPMChildSlot;
#ifndef WIN32
  unsigned long UsedShmemSegID;
#else
  void *ShmemProtectiveRegion;
  HANDLE UsedShmemSegID;
#endif
  void *UsedShmemSegAddr;
  slock_t *ShmemLock;
  VariableCache ShmemVariableCache;
  Backend *ShmemBackendArray;
#ifndef HAVE_SPINLOCKS
  PGSemaphore *SpinlockSemaArray;
#endif
  int NamedLWLockTrancheRequests;
  NamedLWLockTranche *NamedLWLockTrancheArray;
  LWLockPadded *MainLWLockArray;
  slock_t *ProcStructLock;
  PROC_HDR *ProcGlobal;
  PGPROC *AuxiliaryProcs;
  PGPROC *PreparedXactProcs;
  PMSignalData *PMSignalState;
  InheritableSocket pgStatSock;
  pid_t PostmasterPid;
  TimestampTz PgStartTime;
  TimestampTz PgReloadTime;
  pg_time_t first_syslogger_file_time;
  bool redirection_done;
  bool IsBinaryUpgrade;
  int max_safe_fds;
  int MaxBackends;
#ifdef WIN32
  HANDLE PostmasterHandle;
  HANDLE initial_signal_pipe;
  HANDLE syslogPipe[2];
#else
  int postmaster_alive_fds[2];
  int syslogPipe[2];
#endif
  char my_exec_path[MAXPGPATH];
  char pkglib_path[MAXPGPATH];
  char ExtraOptions[MAXPGPATH];
} BackendParameters;

static void
read_backend_variables(char *id, Port *port);
static void
restore_backend_variables(BackendParameters *param, Port *port);

#ifndef WIN32
static bool
save_backend_variables(BackendParameters *param, Port *port);
#else
static bool
save_backend_variables(BackendParameters *param, Port *port, HANDLE childProcess, pid_t childPid);
#endif

static void
ShmemBackendArrayAdd(Backend *bn);
static void
ShmemBackendArrayRemove(Backend *bn);
#endif                   

#define StartupDataBase() StartChildProcess(StartupProcess)
#define StartBackgroundWriter() StartChildProcess(BgWriterProcess)
#define StartCheckpointer() StartChildProcess(CheckpointerProcess)
#define StartWalWriter() StartChildProcess(WalWriterProcess)
#define StartWalReceiver() StartChildProcess(WalReceiverProcess)

                                                    
#define EXIT_STATUS_0(st) ((st) == 0)
#define EXIT_STATUS_1(st) (WIFEXITED(st) && WEXITSTATUS(st) == 1)
#define EXIT_STATUS_3(st) (WIFEXITED(st) && WEXITSTATUS(st) == 3)

#ifndef WIN32
   
                                                                     
                                                              
   
int postmaster_alive_fds[2] = {-1, -1};
#else
                                                                       
HANDLE PostmasterHandle;
#endif

   
                               
   
void
PostmasterMain(int argc, char *argv[])
{
  int opt;
  int status;
  char *userDoption = NULL;
  bool listen_addr_saved = false;
  int i;
  char *output_config_variable = NULL;

  InitProcessGlobals();

  PostmasterPid = MyProcPid;

  IsPostmasterEnvironment = true;

     
                                                                            
                                                                            
                                                    
     
                                                                     
                  
     
  umask(PG_MODE_MASK_OWNER);

     
                                                                          
                                                                             
                                                                     
                                    
     
  PostmasterContext = AllocSetContextCreate(TopMemoryContext, "Postmaster", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(PostmasterContext);

                                              
  getInstallationPaths(argv[0]);

     
                                                        
     
                                                                           
                                                                       
                                  
     
                                                                            
                                                                           
                                                                            
                                                                         
     
                                                                            
                                                                             
                                                                           
                                                                           
                                                                      
                           
     
                                                                             
                                                                   
                         
     
                                                                            
                                                              
                                                                           
                                                                        
                                                       
                                
     
  pqinitmask();
  PG_SETMASK(&BlockSig);

  pqsignal_pm(SIGHUP, SIGHUP_handler);                                  
                                                               
  pqsignal_pm(SIGINT, pmdie);                                            
  pqsignal_pm(SIGQUIT, pmdie);                                     
  pqsignal_pm(SIGTERM, pmdie);                                                
  pqsignal_pm(SIGALRM, SIG_IGN);                      
  pqsignal_pm(SIGPIPE, SIG_IGN);                      
  pqsignal_pm(SIGUSR1, sigusr1_handler);                                 
  pqsignal_pm(SIGUSR2, dummy_handler);                                     
  pqsignal_pm(SIGCHLD, reaper);                                        

     
                                                                           
                                                                           
                                                                            
                                                                             
                                                                        
     
#ifdef SIGTTIN
  pqsignal_pm(SIGTTIN, SIG_IGN);              
#endif
#ifdef SIGTTOU
  pqsignal_pm(SIGTTOU, SIG_IGN);              
#endif

                                                                     
#ifdef SIGXFSZ
  pqsignal_pm(SIGXFSZ, SIG_IGN);              
#endif

     
                   
     
  InitializeGUCOptions();

  opterr = 1;

     
                                                                  
                                                                        
                                            
     
  while ((opt = getopt(argc, argv, "B:bc:C:D:d:EeFf:h:ijk:lN:nOo:Pp:r:S:sTt:W:-:")) != -1)
  {
    switch (opt)
    {
    case 'B':
      SetConfigOption("shared_buffers", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'b':
                                                      
      IsBinaryUpgrade = true;
      break;

    case 'C':
      output_config_variable = strdup(optarg);
      break;

    case 'D':
      userDoption = strdup(optarg);
      break;

    case 'd':
      set_debug_options(atoi(optarg), PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'E':
      SetConfigOption("log_statement", "all", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'e':
      SetConfigOption("datestyle", "euro", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'F':
      SetConfigOption("fsync", "false", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'f':
      if (!set_plan_disabling_options(optarg, PGC_POSTMASTER, PGC_S_ARGV))
      {
        write_stderr("%s: invalid argument for option -f: \"%s\"\n", progname, optarg);
        ExitPostmaster(1);
      }
      break;

    case 'h':
      SetConfigOption("listen_addresses", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'i':
      SetConfigOption("listen_addresses", "*", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'j':
                                            
      break;

    case 'k':
      SetConfigOption("unix_socket_directories", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'l':
      SetConfigOption("ssl", "true", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'N':
      SetConfigOption("max_connections", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'n':
                                                       
      Reinit = false;
      break;

    case 'O':
      SetConfigOption("allow_system_table_mods", "true", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'o':
                                                                    
      snprintf(ExtraOptions + strlen(ExtraOptions), sizeof(ExtraOptions) - strlen(ExtraOptions), " %s", optarg);
      break;

    case 'P':
      SetConfigOption("ignore_system_indexes", "true", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'p':
      SetConfigOption("port", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'r':
                                            
      break;

    case 'S':
      SetConfigOption("work_mem", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 's':
      SetConfigOption("log_statement_stats", "true", PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'T':

         
                                                                  
                                                                    
                                                       
         
      SendStop = true;
      break;

    case 't':
    {
      const char *tmp = get_stats_option_name(optarg);

      if (tmp)
      {
        SetConfigOption(tmp, "true", PGC_POSTMASTER, PGC_S_ARGV);
      }
      else
      {
        write_stderr("%s: invalid argument for option -t: \"%s\"\n", progname, optarg);
        ExitPostmaster(1);
      }
      break;
    }

    case 'W':
      SetConfigOption("post_auth_delay", optarg, PGC_POSTMASTER, PGC_S_ARGV);
      break;

    case 'c':
    case '-':
    {
      char *name, *value;

      ParseLongOption(optarg, &name, &value);
      if (!value)
      {
        if (opt == '-')
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("--%s requires a value", optarg)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("-c %s requires a value", optarg)));
        }
      }

      SetConfigOption(name, value, PGC_POSTMASTER, PGC_S_ARGV);
      free(name);
      if (value)
      {
        free(value);
      }
      break;
    }

    default:
      write_stderr("Try \"%s --help\" for more information.\n", progname);
      ExitPostmaster(1);
    }
  }

     
                                                        
     
  if (optind < argc)
  {
    write_stderr("%s: invalid argument: \"%s\"\n", progname, argv[optind]);
    write_stderr("Try \"%s --help\" for more information.\n", progname);
    ExitPostmaster(1);
  }

     
                                                                        
                                         
     
  if (!SelectConfigFiles(userDoption, progname))
  {
    ExitPostmaster(2);
  }

  if (output_config_variable != NULL)
  {
       
                                                                        
                                                                         
                 
       
    const char *config_val = GetConfigOption(output_config_variable, false, false);

    puts(config_val ? config_val : "");
    ExitPostmaster(0);
  }

                                            
  checkDataDir();

                                    
  checkControlFile();

                                            
  ChangeToDataDir();

     
                                                     
     
  if (ReservedBackends >= MaxConnections)
  {
    write_stderr("%s: superuser_reserved_connections (%d) must be less than max_connections (%d)\n", progname, ReservedBackends, MaxConnections);
    ExitPostmaster(1);
  }
  if (XLogArchiveMode > ARCHIVE_MODE_OFF && wal_level == WAL_LEVEL_MINIMAL)
  {
    ereport(ERROR, (errmsg("WAL archival cannot be enabled when wal_level is \"minimal\"")));
  }
  if (max_wal_senders > 0 && wal_level == WAL_LEVEL_MINIMAL)
  {
    ereport(ERROR, (errmsg("WAL streaming (max_wal_senders > 0) requires wal_level \"replica\" or \"logical\"")));
  }

     
                                                                          
                                                                            
     
  if (!CheckDateTokenTables())
  {
    write_stderr("%s: invalid datetoken tables, please fix\n", progname);
    ExitPostmaster(1);
  }

     
                                                                     
                                                                       
     
  optind = 1;
#ifdef HAVE_INT_OPTRESET
  optreset = 1;                                 
#endif

                                                     
  {
    extern char **environ;
    char **p;

    ereport(DEBUG3, (errmsg_internal("%s: PostmasterMain: initial environment dump:", progname)));
    ereport(DEBUG3, (errmsg_internal("-----------------------------------------")));
    for (p = environ; *p; ++p)
    {
      ereport(DEBUG3, (errmsg_internal("\t%s", *p)));
    }
    ereport(DEBUG3, (errmsg_internal("-----------------------------------------")));
  }

     
                                         
     
                                                                             
                                                                    
                                                                            
                                                                         
                     
     
                                                                           
                                                                           
                                                                          
                                                    
     
  CreateDataDirLockFile(true);

     
                                                                 
     
                                                                           
                                                                             
                                                                         
                                                                         
                      
     
  LocalProcessControlFile(false);

     
                                           
     
#ifdef USE_SSL
  if (EnableSSL)
  {
    (void)secure_initialize(true);
    LoadedSSL = true;
  }
#endif

     
                                                                           
                                                                             
                                                                      
                              
     
  ApplyLauncherRegister();

     
                                                                        
     
  process_shared_preload_libraries();

     
                                                                            
                                     
     
  InitializeMaxBackends();

                                    
  ereport(LOG, (errmsg("starting %s", PG_VERSION_STR)));

     
                              
     
                                                                             
                                                                    
     
  for (i = 0; i < MAXLISTEN; i++)
  {
    ListenSocket[i] = PGINVALID_SOCKET;
  }

  on_proc_exit(CloseServerPorts, 0);

  if (ListenAddresses)
  {
    char *rawstring;
    List *elemlist;
    ListCell *l;
    int success = 0;

                                                   
    rawstring = pstrdup(ListenAddresses);

                                             
    if (!SplitIdentifierString(rawstring, ',', &elemlist))
    {
                                
      ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid list syntax in parameter \"%s\"", "listen_addresses")));
    }

    foreach (l, elemlist)
    {
      char *curhost = (char *)lfirst(l);

      if (strcmp(curhost, "*") == 0)
      {
        status = StreamServerPort(AF_UNSPEC, NULL, (unsigned short)PostPortNumber, NULL, ListenSocket, MAXLISTEN);
      }
      else
      {
        status = StreamServerPort(AF_UNSPEC, curhost, (unsigned short)PostPortNumber, NULL, ListenSocket, MAXLISTEN);
      }

      if (status == STATUS_OK)
      {
        success++;
                                                               
        if (!listen_addr_saved)
        {
          AddToDataDirLockFile(LOCK_FILE_LINE_LISTEN_ADDR, curhost);
          listen_addr_saved = true;
        }
      }
      else
      {
        ereport(WARNING, (errmsg("could not create listen socket for \"%s\"", curhost)));
      }
    }

    if (!success && elemlist != NIL)
    {
      ereport(FATAL, (errmsg("could not create any TCP/IP sockets")));
    }

    list_free(elemlist);
    pfree(rawstring);
  }

#ifdef USE_BONJOUR
                                                            
  if (enable_bonjour && ListenSocket[0] != PGINVALID_SOCKET)
  {
    DNSServiceErrorType err;

       
                                                                          
                                                                      
                                                                         
                                                          
       
    err = DNSServiceRegister(&bonjour_sdref, 0, 0, bonjour_name, "_postgresql._tcp.", NULL, NULL, pg_hton16(PostPortNumber), 0, NULL, NULL, NULL);
    if (err != kDNSServiceErr_NoError)
    {
      elog(LOG, "DNSServiceRegister() failed: error code %ld", (long)err);
    }

       
                                                                           
                                                                           
                                                                        
                                                                     
                                                             
       
  }
#endif

#ifdef HAVE_UNIX_SOCKETS
  if (Unix_socket_directories)
  {
    char *rawstring;
    List *elemlist;
    ListCell *l;
    int success = 0;

                                                           
    rawstring = pstrdup(Unix_socket_directories);

                                               
    if (!SplitDirectoriesString(rawstring, ',', &elemlist))
    {
                                
      ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid list syntax in parameter \"%s\"", "unix_socket_directories")));
    }

    foreach (l, elemlist)
    {
      char *socketdir = (char *)lfirst(l);

      status = StreamServerPort(AF_UNIX, NULL, (unsigned short)PostPortNumber, socketdir, ListenSocket, MAXLISTEN);

      if (status == STATUS_OK)
      {
        success++;
                                                                 
        if (success == 1)
        {
          AddToDataDirLockFile(LOCK_FILE_LINE_SOCKET_DIR, socketdir);
        }
      }
      else
      {
        ereport(WARNING, (errmsg("could not create Unix-domain socket in directory \"%s\"", socketdir)));
      }
    }

    if (!success && elemlist != NIL)
    {
      ereport(FATAL, (errmsg("could not create any Unix-domain sockets")));
    }

    list_free_deep(elemlist);
    pfree(rawstring);
  }
#endif

     
                                                 
     
  if (ListenSocket[0] == PGINVALID_SOCKET)
  {
    ereport(FATAL, (errmsg("no socket created for listening")));
  }

     
                                                                    
                                                                          
                                                                
     
  if (!listen_addr_saved)
  {
    AddToDataDirLockFile(LOCK_FILE_LINE_LISTEN_ADDR, "");
  }

     
                                          
     
  reset_shared(PostPortNumber);

     
                                                                           
                                                                           
     
  set_max_safe_fds();

     
                                                   
     
  (void)set_stack_base();

     
                                                                            
                                             
     
  InitPostmasterDeathWatchHandle();

#ifdef WIN32

     
                                                                           
     
  win32ChildQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
  if (win32ChildQueue == NULL)
  {
    ereport(FATAL, (errmsg("could not create I/O completion port for child queue")));
  }
#endif

     
                                                                           
                                                                 
     
  if (!CreateOptsFile(argc, argv, my_exec_path))
  {
    ExitPostmaster(1);
  }

#ifdef EXEC_BACKEND
                                                                    
  write_nondefault_variables(PGC_POSTMASTER);
#endif

     
                                              
     
  if (external_pid_file)
  {
    FILE *fpidfile = fopen(external_pid_file, "w");

    if (fpidfile)
    {
      fprintf(fpidfile, "%d\n", MyProcPid);
      fclose(fpidfile);

                                        
      if (chmod(external_pid_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0)
      {
        write_stderr("%s: could not change permissions of external PID file \"%s\": %s\n", progname, external_pid_file, strerror(errno));
      }
    }
    else
    {
      write_stderr("%s: could not write external PID file \"%s\": %s\n", progname, external_pid_file, strerror(errno));
    }

    on_proc_exit(unlink_external_pid_file, 0);
  }

     
                                                                      
                                                                           
     
  RemovePgTempFiles();

     
                                                                      
                                                                             
                                       
     
                                                                         
                                                                            
                                                                           
                                                                            
                                                                        
                                                                      
                                                                   
                
     
                                                                            
                                                                  
                                          
     
  RemovePromoteSignalFiles();

                                             
  RemoveLogrotateSignalFiles();

                                                                   
  if (unlink(LOG_METAINFO_DATAFILE) < 0 && errno != ENOENT)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", LOG_METAINFO_DATAFILE)));
  }

     
                                                          
     
  SysLoggerPID = SysLogger_Start();

     
                                                                    
                                                                             
                                                                        
                                                                       
             
     
                                                                             
                                                                             
                                                            
     
  if (!(Log_destination & LOG_DESTINATION_STDERR))
  {
    ereport(LOG, (errmsg("ending log output to stderr"), errhint("Future log output will go to log destination \"%s\".", Log_destination_string)));
  }

  whereToSendOutput = DestNone;

     
                                                                    
                         
     
  pgstat_init();

     
                                                                       
     
  autovac_init();

     
                                                         
     
  if (!load_hba())
  {
       
                                                                      
                                                                      
       
    ereport(FATAL, (errmsg("could not load pg_hba.conf")));
  }
  if (!load_ident())
  {
       
                                                                          
                                                                         
                                                                           
                   
       
  }

#ifdef HAVE_PTHREAD_IS_THREADED_NP

     
                                                                      
                                                                             
                                                                          
                                                                        
                                                                            
                                                                             
                                                                            
     
  if (pthread_is_threaded_np() != 0)
  {
    ereport(FATAL, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("postmaster became multithreaded during startup"), errhint("Set the LC_ALL environment variable to a valid locale.")));
  }
#endif

     
                                      
     
  PgStartTime = GetCurrentTimestamp();

     
                                                                             
                           
     
  AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_STARTING);

     
                                     
     
  StartupPID = StartupDataBase();
  Assert(StartupPID != 0);
  StartupStatus = STARTUP_RUNNING;
  pmState = PM_STARTUP;

                                                  
  maybe_start_bgworkers();

  status = ServerLoop();

     
                                                                            
     
  ExitPostmaster(status != STATUS_OK);

  abort();                  
}

   
                                                          
   
static void
CloseServerPorts(int status, Datum arg)
{
  int i;

     
                                                                           
                                                                         
                                                                            
                                                                        
     
  for (i = 0; i < MAXLISTEN; i++)
  {
    if (ListenSocket[i] != PGINVALID_SOCKET)
    {
      StreamClose(ListenSocket[i]);
      ListenSocket[i] = PGINVALID_SOCKET;
    }
  }

     
                                                                          
                                                                             
                                                 
     
  RemoveSocketFiles();

     
                                                                      
                                               
     
}

   
                                                     
   
static void
unlink_external_pid_file(int status, Datum arg)
{
  if (external_pid_file)
  {
    unlink(external_pid_file);
  }
}

   
                                                                       
                                                                         
   
static void
getInstallationPaths(const char *argv0)
{
  DIR *pdir;

                                             
  if (find_my_exec(argv0, my_exec_path) < 0)
  {
    elog(FATAL, "%s: could not locate my own executable path", argv0);
  }

#ifdef EXEC_BACKEND
                                                                    
  if (find_other_exec(argv0, "postgres", PG_BACKEND_VERSIONSTR, postgres_exec_path) < 0)
  {
    ereport(FATAL, (errmsg("%s: could not locate matching postgres executable", argv0)));
  }
#endif

     
                                                                             
                                                                         
     
  get_pkglib_path(my_exec_path, pkglib_path);

     
                                                                            
                                                                      
                                                                             
                                                                  
                 
     
  pdir = AllocateDir(pkglib_path);
  if (pdir == NULL)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", pkglib_path), errhint("This may indicate an incomplete PostgreSQL installation, or that the file \"%s\" has been moved away from its proper location.", my_exec_path)));
  }
  FreeDir(pdir);

     
                                                                           
                                                      
     
}

   
                                                                               
   
                                                                            
                                                                          
   
static void
checkControlFile(void)
{
  char path[MAXPGPATH];
  FILE *fp;

  snprintf(path, sizeof(path), "%s/global/pg_control", DataDir);

  fp = AllocateFile(path, PG_BINARY_R);
  if (fp == NULL)
  {
    write_stderr("%s: could not find the database system\n"
                 "Expected to find it in the directory \"%s\",\n"
                 "but could not open file \"%s\": %s\n",
        progname, DataDir, path, strerror(errno));
    ExitPostmaster(2);
  }
  FreeFile(fp);
}

   
                                                      
   
                                                                             
                                                                             
                                                                              
                                                                               
                                   
   
static void
DetermineSleepTime(struct timeval *timeout)
{
  TimestampTz next_wakeup = 0;

     
                                                                             
                                                                        
     
  if (Shutdown > NoShutdown || (!StartWorkerNeeded && !HaveCrashedWorker))
  {
    if (AbortStartTime != 0)
    {
                                                                     
      timeout->tv_sec = SIGKILL_CHILDREN_AFTER_SECS - (time(NULL) - AbortStartTime);
      timeout->tv_sec = Max(timeout->tv_sec, 0);
      timeout->tv_usec = 0;
    }
    else
    {
      timeout->tv_sec = 60;
      timeout->tv_usec = 0;
    }
    return;
  }

  if (StartWorkerNeeded)
  {
    timeout->tv_sec = 0;
    timeout->tv_usec = 0;
    return;
  }

  if (HaveCrashedWorker)
  {
    slist_mutable_iter siter;

       
                                                                        
                                                                     
                                                                          
                                                  
       
    slist_foreach_modify(siter, &BackgroundWorkerList)
    {
      RegisteredBgWorker *rw;
      TimestampTz this_wakeup;

      rw = slist_container(RegisteredBgWorker, rw_lnode, siter.cur);

      if (rw->rw_crashed_at == 0)
      {
        continue;
      }

      if (rw->rw_worker.bgw_restart_time == BGW_NEVER_RESTART || rw->rw_terminate)
      {
        ForgetBackgroundWorker(&siter);
        continue;
      }

      this_wakeup = TimestampTzPlusMilliseconds(rw->rw_crashed_at, 1000L * rw->rw_worker.bgw_restart_time);
      if (next_wakeup == 0 || this_wakeup < next_wakeup)
      {
        next_wakeup = this_wakeup;
      }
    }
  }

  if (next_wakeup != 0)
  {
    long secs;
    int microsecs;

    TimestampDifference(GetCurrentTimestamp(), next_wakeup, &secs, &microsecs);
    timeout->tv_sec = secs;
    timeout->tv_usec = microsecs;

                                           
    if (timeout->tv_sec > 60)
    {
      timeout->tv_sec = 60;
      timeout->tv_usec = 0;
    }
  }
  else
  {
    timeout->tv_sec = 60;
    timeout->tv_usec = 0;
  }
}

   
                                
   
                                               
   
static int
ServerLoop(void)
{
  fd_set readmask;
  int nSockets;
  time_t last_lockfile_recheck_time, last_touch_time;

  last_lockfile_recheck_time = last_touch_time = time(NULL);

  nSockets = initMasks(&readmask);

  for (;;)
  {
    fd_set rmask;
    int selres;
    time_t now;

       
                                                
       
                                                                          
                                                                          
                           
       
                                                                         
                                                                       
       
    memcpy((char *)&rmask, (char *)&readmask, sizeof(fd_set));

    if (pmState == PM_WAIT_DEAD_END)
    {
      PG_SETMASK(&UnBlockSig);

      pg_usleep(100000L);                                
      selres = 0;

      PG_SETMASK(&BlockSig);
    }
    else
    {
                                                            
      struct timeval timeout;

                                              
      DetermineSleepTime(&timeout);

      PG_SETMASK(&UnBlockSig);

      selres = select(nSockets, &rmask, NULL, NULL, &timeout);

      PG_SETMASK(&BlockSig);
    }

                                       
    if (selres < 0)
    {
      if (errno != EINTR && errno != EWOULDBLOCK)
      {
        ereport(LOG, (errcode_for_socket_access(), errmsg("select() failed in postmaster: %m")));
        return STATUS_ERROR;
      }
    }

       
                                                                         
                                
       
    if (selres > 0)
    {
      int i;

      for (i = 0; i < MAXLISTEN; i++)
      {
        if (ListenSocket[i] == PGINVALID_SOCKET)
        {
          break;
        }
        if (FD_ISSET(ListenSocket[i], &rmask))
        {
          Port *port;

          port = ConnCreate(ListenSocket[i]);
          if (port)
          {
            BackendStartup(port);

               
                                                                   
                               
               
            StreamClose(port->sock);
            ConnFree(port);
          }
        }
      }
    }

                                                                   
    if (SysLoggerPID == 0 && Logging_collector)
    {
      SysLoggerPID = SysLogger_Start();
    }

       
                                                                       
                                                                     
                                                                          
       
    if (pmState == PM_RUN || pmState == PM_RECOVERY || pmState == PM_HOT_STANDBY)
    {
      if (CheckpointerPID == 0)
      {
        CheckpointerPID = StartCheckpointer();
      }
      if (BgWriterPID == 0)
      {
        BgWriterPID = StartBackgroundWriter();
      }
    }

       
                                                                           
                                                                         
                                
       
    if (WalWriterPID == 0 && pmState == PM_RUN)
    {
      WalWriterPID = StartWalWriter();
    }

       
                                                                           
                                                                   
                                                                        
                                        
       
    if (!IsBinaryUpgrade && AutoVacPID == 0 && (AutoVacuumingActive() || start_autovac_launcher) && pmState == PM_RUN)
    {
      AutoVacPID = StartAutoVacLauncher();
      if (AutoVacPID != 0)
      {
        start_autovac_launcher = false;                       
      }
    }

                                                                     
    if (PgStatPID == 0 && (pmState == PM_RUN || pmState == PM_HOT_STANDBY))
    {
      PgStatPID = pgstat_start();
    }

                                                               
    if (PgArchPID == 0 && PgArchStartupAllowed())
    {
      PgArchPID = pgarch_start();
    }

                                                                 
    if (avlauncher_needs_signal)
    {
      avlauncher_needs_signal = false;
      if (AutoVacPID != 0)
      {
        kill(AutoVacPID, SIGUSR2);
      }
    }

                                                                
    if (WalReceiverRequested)
    {
      MaybeStartWalReceiver();
    }

                                                       
    if (StartWorkerNeeded || HaveCrashedWorker)
    {
      maybe_start_bgworkers();
    }

#ifdef HAVE_PTHREAD_IS_THREADED_NP

       
                                                                  
                                                                
       
    Assert(pthread_is_threaded_np() == 0);
#endif

       
                                                                         
                                                                        
                                                                        
                                                                          
                                                                        
                                 
       
    now = time(NULL);

       
                                                                        
                                                                  
                                                                           
                                                                    
       
                                                                  
       
    if ((Shutdown >= ImmediateShutdown || (FatalError && !SendStop)) && AbortStartTime != 0 && (now - AbortStartTime) >= SIGKILL_CHILDREN_AFTER_SECS)
    {
                                                        
      TerminateChildren(SIGKILL);
                                                
      AbortStartTime = 0;
    }

       
                                                                        
                                                                         
                                                                           
                                                                        
                                                                    
                                                                    
                                                                           
                                                                     
       
    if (now - last_lockfile_recheck_time >= 1 * SECS_PER_MINUTE)
    {
      if (!RecheckDataDirLockFile())
      {
        ereport(LOG, (errmsg("performing immediate shutdown because data directory lock file is invalid")));
        kill(MyProcPid, SIGQUIT);
      }
      last_lockfile_recheck_time = now;
    }

       
                                                                         
                                                                           
                                                                       
       
    if (now - last_touch_time >= 58 * SECS_PER_MINUTE)
    {
      TouchSocketFiles();
      TouchSocketLockFiles();
      last_touch_time = now;
    }
  }
}

   
                                                                        
                                              
   
static int
initMasks(fd_set *rmask)
{
  int maxsock = -1;
  int i;

  FD_ZERO(rmask);

  for (i = 0; i < MAXLISTEN; i++)
  {
    int fd = ListenSocket[i];

    if (fd == PGINVALID_SOCKET)
    {
      break;
    }
    FD_SET(fd, rmask);

    if (fd > maxsock)
    {
      maxsock = fd;
    }
  }

  return maxsock + 1;
}

   
                                                                    
   
                                                                       
                      
   
                                                                         
                                                                      
                                                                     
                                           
   
                                                                       
                                                                               
                                                                              
                                                                           
                                                                               
             
   
static int
ProcessStartupPacket(Port *port, bool ssl_done, bool gss_done)
{
  int32 len;
  void *buf;
  ProtocolVersion proto;
  MemoryContext oldcontext;

  pq_startmsgread();

     
                                                                            
                                                                          
                                                                     
                
     
  if (pq_getbytes((char *)&len, 1) == EOF)
  {
       
                                                                         
                                                                          
                                                                           
                                                                     
                                                                          
                                                                  
                                                                          
                      
       
    return STATUS_ERROR;
  }

  if (pq_getbytes(((char *)&len) + 1, 3) == EOF)
  {
                                                        
    if (!ssl_done && !gss_done)
    {
      ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("incomplete startup packet")));
    }
    return STATUS_ERROR;
  }

  len = pg_ntoh32(len);
  len -= 4;

  if (len < (int32)sizeof(ProtocolVersion) || len > MAX_STARTUP_PACKET_LENGTH)
  {
    ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid length of startup packet")));
    return STATUS_ERROR;
  }

     
                                                                         
                                                                          
                                                                         
                     
     
  if (len <= (int32)sizeof(StartupPacket))
  {
    buf = palloc0(sizeof(StartupPacket) + 1);
  }
  else
  {
    buf = palloc0(len + 1);
  }

  if (pq_getbytes(buf, len) == EOF)
  {
    ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("incomplete startup packet")));
    return STATUS_ERROR;
  }
  pq_endmsgread();

     
                                                                      
                   
     
  port->proto = proto = pg_ntoh32(*((ProtocolVersion *)buf));

  if (proto == CANCEL_REQUEST_CODE)
  {
    if (len != sizeof(CancelRequestPacket))
    {
      ereport(COMMERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid length of startup packet")));
      return STATUS_ERROR;
    }
    processCancelRequest(port, buf);
                                                                   
    return STATUS_ERROR;
  }

  if (proto == NEGOTIATE_SSL_CODE && !ssl_done)
  {
    char SSLok;

#ifdef USE_SSL
                                                 
    if (!LoadedSSL || IS_AF_UNIX(port->laddr.addr.ss_family))
    {
      SSLok = 'N';
    }
    else
    {
      SSLok = 'S';                      
    }
#else
    SSLok = 'N';                         
#endif

  retry1:
    if (send(port->sock, &SSLok, 1, 0) != 1)
    {
      if (errno == EINTR)
      {
        goto retry1;                                 
      }
      ereport(COMMERROR, (errcode_for_socket_access(), errmsg("failed to send SSL negotiation response: %m")));
      return STATUS_ERROR;                           
    }

#ifdef USE_SSL
    if (SSLok == 'S' && secure_open_server(port) == -1)
    {
      return STATUS_ERROR;
    }
#endif

       
                                                                         
                                                                           
                                                                           
                                          
       
    if (pq_buffer_has_data())
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("received unencrypted data after SSL request"), errdetail("This could be either a client-software bug or evidence of an attempted man-in-the-middle attack.")));
    }

       
                                                                         
                                                                      
                                                                         
       
    return ProcessStartupPacket(port, true, SSLok == 'S');
  }
  else if (proto == NEGOTIATE_GSS_CODE && !gss_done)
  {
    char GSSok = 'N';

#ifdef ENABLE_GSS
                                                  
    if (!IS_AF_UNIX(port->laddr.addr.ss_family))
    {
      GSSok = 'G';
    }
#endif

    while (send(port->sock, &GSSok, 1, 0) != 1)
    {
      if (errno == EINTR)
      {
        continue;
      }
      ereport(COMMERROR, (errcode_for_socket_access(), errmsg("failed to send GSSAPI negotiation response: %m")));
      return STATUS_ERROR;                           
    }

#ifdef ENABLE_GSS
    if (GSSok == 'G' && secure_open_gssapi(port) == -1)
    {
      return STATUS_ERROR;
    }
#endif

       
                                                                         
                                                                           
                                                                           
                                          
       
    if (pq_buffer_has_data())
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("received unencrypted data after GSSAPI encryption request"), errdetail("This could be either a client-software bug or evidence of an attempted man-in-the-middle attack.")));
    }

       
                                                                         
                                                                       
                                                                         
       
    return ProcessStartupPacket(port, GSSok == 'G', true);
  }

                                                      

     
                                                                             
                             
     
  FrontendProtocol = proto;

                                                          
  if (PG_PROTOCOL_MAJOR(proto) < PG_PROTOCOL_MAJOR(PG_PROTOCOL_EARLIEST) || PG_PROTOCOL_MAJOR(proto) > PG_PROTOCOL_MAJOR(PG_PROTOCOL_LATEST))
  {
    ereport(FATAL, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unsupported frontend protocol %u.%u: server supports %u.0 to %u.%u", PG_PROTOCOL_MAJOR(proto), PG_PROTOCOL_MINOR(proto), PG_PROTOCOL_MAJOR(PG_PROTOCOL_EARLIEST), PG_PROTOCOL_MAJOR(PG_PROTOCOL_LATEST), PG_PROTOCOL_MINOR(PG_PROTOCOL_LATEST))));
  }

     
                                                                            
                                                                         
                                                                           
                                                                           
                                                                             
                                 
     
  oldcontext = MemoryContextSwitchTo(TopMemoryContext);

  if (PG_PROTOCOL_MAJOR(proto) >= 3)
  {
    int32 offset = sizeof(ProtocolVersion);
    List *unrecognized_protocol_options = NIL;

       
                                                                         
                                                                      
                                 
       
    port->guc_options = NIL;

    while (offset < len)
    {
      char *nameptr = ((char *)buf) + offset;
      int32 valoffset;
      char *valptr;

      if (*nameptr == '\0')
      {
        break;                              
      }
      valoffset = offset + strlen(nameptr) + 1;
      if (valoffset >= len)
      {
        break;                                         
      }
      valptr = ((char *)buf) + valoffset;

      if (strcmp(nameptr, "database") == 0)
      {
        port->database_name = pstrdup(valptr);
      }
      else if (strcmp(nameptr, "user") == 0)
      {
        port->user_name = pstrdup(valptr);
      }
      else if (strcmp(nameptr, "options") == 0)
      {
        port->cmdline_options = pstrdup(valptr);
      }
      else if (strcmp(nameptr, "replication") == 0)
      {
           
                                                                  
                                                                    
                                                               
                                                                      
                                   
           
        if (strcmp(valptr, "database") == 0)
        {
          am_walsender = true;
          am_db_walsender = true;
        }
        else if (!parse_bool(valptr, &am_walsender))
        {
          ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid value for parameter \"%s\": \"%s\"", "replication", valptr), errhint("Valid values are: \"false\", 0, \"true\", 1, \"database\".")));
        }
      }
      else if (strncmp(nameptr, "_pq_.", 5) == 0)
      {
           
                                                                    
                                                                     
                    
           
        unrecognized_protocol_options = lappend(unrecognized_protocol_options, pstrdup(nameptr));
      }
      else
      {
                                              
        port->guc_options = lappend(port->guc_options, pstrdup(nameptr));
        port->guc_options = lappend(port->guc_options, pstrdup(valptr));

           
                                                                     
                                                             
                                                                      
                                                              
           
        if (strcmp(nameptr, "application_name") == 0)
        {
          char *tmp_app_name = pstrdup(valptr);

          pg_clean_ascii(tmp_app_name);

          port->application_name = tmp_app_name;
        }
      }
      offset = valoffset + strlen(valptr) + 1;
    }

       
                                                                       
                                      
       
    if (offset != len - 1)
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid startup packet layout: expected terminator as last byte")));
    }

       
                                                                         
                                                                         
                                                                        
                                 
       
    if (PG_PROTOCOL_MINOR(proto) > PG_PROTOCOL_MINOR(PG_PROTOCOL_LATEST) || unrecognized_protocol_options != NIL)
    {
      SendNegotiateProtocolVersion(unrecognized_protocol_options);
    }
  }
  else
  {
       
                                                                         
                                                                           
                                                                         
                                                                
       
    StartupPacket *packet = (StartupPacket *)buf;

    port->database_name = pstrdup(packet->database);
    if (strlen(port->database_name) > sizeof(packet->database))
    {
      port->database_name[sizeof(packet->database)] = '\0';
    }
    port->user_name = pstrdup(packet->user);
    if (strlen(port->user_name) > sizeof(packet->user))
    {
      port->user_name[sizeof(packet->user)] = '\0';
    }
    port->cmdline_options = pstrdup(packet->options);
    if (strlen(port->cmdline_options) > sizeof(packet->options))
    {
      port->cmdline_options[sizeof(packet->options)] = '\0';
    }
    port->guc_options = NIL;
  }

                                    
  if (port->user_name == NULL || port->user_name[0] == '\0')
  {
    ereport(FATAL, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("no PostgreSQL user name specified in startup packet")));
  }

                                               
  if (port->database_name == NULL || port->database_name[0] == '\0')
  {
    port->database_name = pstrdup(port->user_name);
  }

  if (Db_user_namespace)
  {
       
                                                                          
                                                                          
                                                                           
                 
       
    if (strchr(port->user_name, '@') == port->user_name + strlen(port->user_name) - 1)
    {
      *strchr(port->user_name, '@') = '\0';
    }
    else
    {
                                 
      port->user_name = psprintf("%s@%s", port->user_name, port->database_name);
    }
  }

     
                                                                          
                                                                  
     
  if (strlen(port->database_name) >= NAMEDATALEN)
  {
    port->database_name[NAMEDATALEN - 1] = '\0';
  }
  if (strlen(port->user_name) >= NAMEDATALEN)
  {
    port->user_name[NAMEDATALEN - 1] = '\0';
  }

     
                                                                        
                                                                         
                                                                            
                                                                             
                                                                       
                         
     
  if (am_walsender && !am_db_walsender)
  {
    port->database_name[0] = '\0';
  }

     
                                             
     
  MemoryContextSwitchTo(oldcontext);

     
                                                                           
                                                                             
                                              
     
  switch (port->canAcceptConnections)
  {
  case CAC_STARTUP:
    ereport(FATAL, (errcode(ERRCODE_CANNOT_CONNECT_NOW), errmsg("the database system is starting up")));
    break;
  case CAC_SHUTDOWN:
    ereport(FATAL, (errcode(ERRCODE_CANNOT_CONNECT_NOW), errmsg("the database system is shutting down")));
    break;
  case CAC_RECOVERY:
    ereport(FATAL, (errcode(ERRCODE_CANNOT_CONNECT_NOW), errmsg("the database system is in recovery mode")));
    break;
  case CAC_TOOMANY:
    ereport(FATAL, (errcode(ERRCODE_TOO_MANY_CONNECTIONS), errmsg("sorry, too many clients already")));
    break;
  case CAC_SUPERUSER:
                                                
    break;
  case CAC_OK:
    break;
  }

  return STATUS_OK;
}

   
                                                                             
                                                                            
                                                                             
                                                          
   
                                                                        
                                                                              
                                                                        
                                                                           
                                                                              
                                            
   
static void
SendNegotiateProtocolVersion(List *unrecognized_protocol_options)
{
  StringInfoData buf;
  ListCell *lc;

  pq_beginmessage(&buf, 'v');                               
  pq_sendint32(&buf, PG_PROTOCOL_LATEST);
  pq_sendint32(&buf, list_length(unrecognized_protocol_options));
  foreach (lc, unrecognized_protocol_options)
  {
    pq_sendstring(&buf, lfirst(lc));
  }
  pq_endmessage(&buf);

                                                        
}

   
                                                             
                                                                     
                                       
   
static void
processCancelRequest(Port *port, void *pkt)
{
  CancelRequestPacket *canc = (CancelRequestPacket *)pkt;
  int backendPID;
  int32 cancelAuthCode;
  Backend *bp;

#ifndef EXEC_BACKEND
  dlist_iter iter;
#else
  int i;
#endif

  backendPID = (int)pg_ntoh32(canc->backendPID);
  cancelAuthCode = (int32)pg_ntoh32(canc->cancelAuthCode);

     
                                                                             
                                                                           
                                       
     
#ifndef EXEC_BACKEND
  dlist_foreach(iter, &BackendList)
  {
    bp = dlist_container(Backend, elem, iter.cur);
#else
  for (i = MaxLivePostmasterChildren() - 1; i >= 0; i--)
  {
    bp = (Backend *)&ShmemBackendArray[i];
#endif
    if (bp->pid == backendPID)
    {
      if (bp->cancel_key == cancelAuthCode)
      {
                                                                     
        ereport(DEBUG2, (errmsg_internal("processing cancel request: sending SIGINT to process %d", backendPID)));
        signal_child(bp->pid, SIGINT);
      }
      else
      {
                                                
        ereport(LOG, (errmsg("wrong key in cancel request for process %d", backendPID)));
      }
      return;
    }
#ifndef EXEC_BACKEND                                            
  }
#else
  }
#endif

                           
  ereport(LOG, (errmsg("PID %d in cancel request did not match any process", backendPID)));
}

   
                                                                              
                                                                    
                                                                            
                                                                  
   
static CAC_state
canAcceptConnections(int backend_type)
{
  CAC_state result = CAC_OK;

     
                                                                         
                                                                         
                                                                         
                                                                           
     
  if (pmState != PM_RUN && pmState != PM_HOT_STANDBY && backend_type != BACKEND_TYPE_BGWORKER)
  {
    if (Shutdown > NoShutdown)
    {
      return CAC_SHUTDOWN;                          
    }
    else if (!FatalError && (pmState == PM_STARTUP || pmState == PM_RECOVERY))
    {
      return CAC_STARTUP;                     
    }
    else
    {
      return CAC_RECOVERY;                                  
    }
  }

     
                                                                           
                                                                             
                                                                            
                                                                      
                                                           
     
  if (connsAllowed != ALLOW_ALL_CONNS && backend_type == BACKEND_TYPE_NORMAL)
  {
    if (connsAllowed == ALLOW_SUPERUSER_CONNS)
    {
      result = CAC_SUPERUSER;                            
    }
    else
    {
      return CAC_SHUTDOWN;                          
    }
  }

     
                                    
     
                                                                           
                                                                           
                                                                       
                                                                        
                                 
     
                                                                          
                                                   
     
  if (CountChildren(BACKEND_TYPE_ALL) >= MaxLivePostmasterChildren())
  {
    result = CAC_TOOMANY;
  }

  return result;
}

   
                                                          
   
                                                                     
   
static Port *
ConnCreate(int serverFd)
{
  Port *port;

  if (!(port = (Port *)calloc(1, sizeof(Port))))
  {
    ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    ExitPostmaster(1);
  }

  if (StreamConnection(serverFd, port) != STATUS_OK)
  {
    if (port->sock != PGINVALID_SOCKET)
    {
      StreamClose(port->sock);
    }
    ConnFree(port);
    return NULL;
  }

  return port;
}

   
                                                      
   
                                                                    
               
   
static void
ConnFree(Port *conn)
{
  free(conn);
}

   
                                                                   
   
                                                                           
                                                                        
                         
   
                                                                        
                                                
   
void
ClosePostmasterPorts(bool am_syslogger)
{
  int i;

#ifndef WIN32

     
                                                                           
                                                                            
                                                                        
     
  if (close(postmaster_alive_fds[POSTMASTER_FD_OWN]))
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg_internal("could not close postmaster death monitoring pipe in child process: %m")));
  }
  postmaster_alive_fds[POSTMASTER_FD_OWN] = -1;
#endif

                                
  for (i = 0; i < MAXLISTEN; i++)
  {
    if (ListenSocket[i] != PGINVALID_SOCKET)
    {
      StreamClose(ListenSocket[i]);
      ListenSocket[i] = PGINVALID_SOCKET;
    }
  }

                                                           
  if (!am_syslogger)
  {
#ifndef WIN32
    if (syslogPipe[0] >= 0)
    {
      close(syslogPipe[0]);
    }
    syslogPipe[0] = -1;
#else
    if (syslogPipe[0])
    {
      CloseHandle(syslogPipe[0]);
    }
    syslogPipe[0] = 0;
#endif
  }

#ifdef USE_BONJOUR
                                                                 
  if (bonjour_sdref)
  {
    close(DNSServiceRefSockFD(bonjour_sdref));
  }
#endif
}

   
                                                                         
   
                                                     
   
void
InitProcessGlobals(void)
{
  unsigned int rseed;

  MyProcPid = getpid();
  MyStartTimestamp = GetCurrentTimestamp();
  MyStartTime = timestamptz_to_time_t(MyStartTimestamp);

     
                                                                            
                                                                         
                                                                       
     
  if (!pg_strong_random(&rseed, sizeof(rseed)))
  {
       
                                                                         
                                                                          
                                                                       
                                                                       
                                     
       
    rseed = ((uint64)MyProcPid) ^ ((uint64)MyStartTimestamp << 12) ^ ((uint64)MyStartTimestamp >> 20);
  }
  srandom(rseed);
}

   
                                                      
   
static void
reset_shared(int port)
{
     
                                                       
     
                                                                             
                                                                          
                                                                           
                                                         
     
  CreateSharedMemoryAndSemaphores(port);
}

   
                                                               
   
static void
SIGHUP_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                                                            
                                                                        
     
#ifdef WIN32
  PG_SETMASK(&BlockSig);
#endif

  if (Shutdown <= SmartShutdown)
  {
    ereport(LOG, (errmsg("received SIGHUP, reloading configuration files")));
    ProcessConfigFile(PGC_SIGHUP);
    SignalChildren(SIGHUP);
    if (StartupPID != 0)
    {
      signal_child(StartupPID, SIGHUP);
    }
    if (BgWriterPID != 0)
    {
      signal_child(BgWriterPID, SIGHUP);
    }
    if (CheckpointerPID != 0)
    {
      signal_child(CheckpointerPID, SIGHUP);
    }
    if (WalWriterPID != 0)
    {
      signal_child(WalWriterPID, SIGHUP);
    }
    if (WalReceiverPID != 0)
    {
      signal_child(WalReceiverPID, SIGHUP);
    }
    if (AutoVacPID != 0)
    {
      signal_child(AutoVacPID, SIGHUP);
    }
    if (PgArchPID != 0)
    {
      signal_child(PgArchPID, SIGHUP);
    }
    if (SysLoggerPID != 0)
    {
      signal_child(SysLoggerPID, SIGHUP);
    }
    if (PgStatPID != 0)
    {
      signal_child(PgStatPID, SIGHUP);
    }

                                                
    if (!load_hba())
    {
      ereport(LOG,
                                                      
          (errmsg("%s was not reloaded", "pg_hba.conf")));
    }

    if (!load_ident())
    {
      ereport(LOG, (errmsg("%s was not reloaded", "pg_ident.conf")));
    }

#ifdef USE_SSL
                                          
    if (EnableSSL)
    {
      if (secure_initialize(false) == 0)
      {
        LoadedSSL = true;
      }
      else
      {
        ereport(LOG, (errmsg("SSL configuration was not reloaded")));
      }
    }
    else
    {
      secure_destroy();
      LoadedSSL = false;
    }
#endif

#ifdef EXEC_BACKEND
                                                            
    write_nondefault_variables(PGC_SIGHUP);
#endif
  }

#ifdef WIN32
  PG_SETMASK(&UnBlockSig);
#endif

  errno = save_errno;
}

   
                                                                      
   
static void
pmdie(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                                                            
                                                                        
     
#ifdef WIN32
  PG_SETMASK(&BlockSig);
#endif

  ereport(DEBUG2, (errmsg_internal("postmaster received signal %d", postgres_signal_arg)));

  switch (postgres_signal_arg)
  {
  case SIGTERM:

       
                       
       
                                                            
       
    if (Shutdown >= SmartShutdown)
    {
      break;
    }
    Shutdown = SmartShutdown;
    ereport(LOG, (errmsg("received smart shutdown request")));

                       
    AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_STOPPING);
#ifdef USE_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

       
                                                                    
                                                                       
                                                                       
                                                                      
                                                                       
                                        
       
    if (pmState == PM_RUN)
    {
      connsAllowed = ALLOW_SUPERUSER_CONNS;
    }
    else if (pmState == PM_HOT_STANDBY)
    {
      connsAllowed = ALLOW_NO_CONNS;
    }
    else if (pmState == PM_STARTUP || pmState == PM_RECOVERY)
    {
                                                                   
      pmState = PM_STOP_BACKENDS;
    }

       
                                                                       
                                                                      
                  
       
    PostmasterStateMachine();
    break;

  case SIGINT:

       
                      
       
                                                                     
                                                   
       
    if (Shutdown >= FastShutdown)
    {
      break;
    }
    Shutdown = FastShutdown;
    ereport(LOG, (errmsg("received fast shutdown request")));

                       
    AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_STOPPING);
#ifdef USE_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    if (pmState == PM_STARTUP || pmState == PM_RECOVERY)
    {
                                                        
      pmState = PM_STOP_BACKENDS;
    }
    else if (pmState == PM_RUN || pmState == PM_HOT_STANDBY)
    {
                                                               
      ereport(LOG, (errmsg("aborting any active transactions")));
      pmState = PM_STOP_BACKENDS;
    }

       
                                                                   
                                                                   
       
    PostmasterStateMachine();
    break;

  case SIGQUIT:

       
                           
       
                                                               
                                                                
                                                           
       
    if (Shutdown >= ImmediateShutdown)
    {
      break;
    }
    Shutdown = ImmediateShutdown;
    ereport(LOG, (errmsg("received immediate shutdown request")));

                       
    AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_STOPPING);
#ifdef USE_SYSTEMD
    sd_notify(0, "STOPPING=1");
#endif

    TerminateChildren(SIGQUIT);
    pmState = PM_WAIT_BACKENDS;

                                       
    AbortStartTime = time(NULL);

       
                                                          
                                                       
       
    PostmasterStateMachine();
    break;
  }

#ifdef WIN32
  PG_SETMASK(&UnBlockSig);
#endif

  errno = save_errno;
}

   
                                                                   
   
static void
reaper(SIGNAL_ARGS)
{
  int save_errno = errno;
  int pid;                                              
  int exitstatus;                      

     
                                                                            
                                                                        
     
#ifdef WIN32
  PG_SETMASK(&BlockSig);
#endif

  ereport(DEBUG4, (errmsg_internal("reaping dead processes")));

  while ((pid = waitpid(-1, &exitstatus, WNOHANG)) > 0)
  {
       
                                                  
       
    if (pid == StartupPID)
    {
      StartupPID = 0;

         
                                                                         
                                                                 
         
      if (Shutdown > NoShutdown && (EXIT_STATUS_0(exitstatus) || EXIT_STATUS_1(exitstatus)))
      {
        StartupStatus = STARTUP_NOT_RUNNING;
        pmState = PM_WAIT_BACKENDS;
                                                        
        continue;
      }

      if (EXIT_STATUS_3(exitstatus))
      {
        ereport(LOG, (errmsg("shutdown at recovery target")));
        StartupStatus = STARTUP_NOT_RUNNING;
        Shutdown = Max(Shutdown, SmartShutdown);
        TerminateChildren(SIGTERM);
        pmState = PM_WAIT_BACKENDS;
                                                        
        continue;
      }

         
                                                                   
                                                                    
                                                           
         
      if (pmState == PM_STARTUP && !EXIT_STATUS_0(exitstatus))
      {
        LogChildExit(LOG, _("startup process"), pid, exitstatus);
        ereport(LOG, (errmsg("aborting startup due to startup process failure")));
        ExitPostmaster(1);
      }

         
                                                                         
                                                                      
                                                                     
                                                                         
                                                                   
                                                                      
                               
         
      if (!EXIT_STATUS_0(exitstatus))
      {
        if (StartupStatus == STARTUP_SIGNALED)
        {
          StartupStatus = STARTUP_NOT_RUNNING;
        }
        else
        {
          StartupStatus = STARTUP_CRASHED;
        }
        HandleChildCrash(pid, exitstatus, _("startup process"));
        continue;
      }

         
                                                       
         
      StartupStatus = STARTUP_NOT_RUNNING;
      FatalError = false;
      Assert(AbortStartTime == 0);
      ReachedNormalRunning = true;
      pmState = PM_RUN;
      connsAllowed = ALLOW_ALL_CONNS;

         
                                                                     
                                                                       
                                                    
         
      if (CheckpointerPID == 0)
      {
        CheckpointerPID = StartCheckpointer();
      }
      if (BgWriterPID == 0)
      {
        BgWriterPID = StartBackgroundWriter();
      }
      if (WalWriterPID == 0)
      {
        WalWriterPID = StartWalWriter();
      }

         
                                                                         
                                                       
         
      if (!IsBinaryUpgrade && AutoVacuumingActive() && AutoVacPID == 0)
      {
        AutoVacPID = StartAutoVacLauncher();
      }
      if (PgArchStartupAllowed() && PgArchPID == 0)
      {
        PgArchPID = pgarch_start();
      }
      if (PgStatPID == 0)
      {
        PgStatPID = pgstat_start();
      }

                                                 
      maybe_start_bgworkers();

                                                         
      ereport(LOG, (errmsg("database system is ready to accept connections")));

                         
      AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_READY);
#ifdef USE_SYSTEMD
      sd_notify(0, "READY=1");
#endif

      continue;
    }

       
                                                                           
                                                                   
                                                                   
       
    if (pid == BgWriterPID)
    {
      BgWriterPID = 0;
      if (!EXIT_STATUS_0(exitstatus))
      {
        HandleChildCrash(pid, exitstatus, _("background writer process"));
      }
      continue;
    }

       
                                
       
    if (pid == CheckpointerPID)
    {
      CheckpointerPID = 0;
      if (EXIT_STATUS_0(exitstatus) && pmState == PM_SHUTDOWN)
      {
           
                                                                      
                                                                  
                                                                     
                                            
           
                                                                   
                                                                     
                                               
           
                                                                   
                                                                  
                                                                    
           
        Assert(Shutdown > NoShutdown);

                                              
        if (PgArchPID != 0)
        {
          signal_child(PgArchPID, SIGUSR2);
        }

           
                                                                   
                                     
           
        SignalChildren(SIGUSR2);

        pmState = PM_SHUTDOWN_2;

           
                                                                  
                                      
           
        if (PgStatPID != 0)
        {
          signal_child(PgStatPID, SIGQUIT);
        }
      }
      else
      {
           
                                                                    
                                        
           
        HandleChildCrash(pid, exitstatus, _("checkpointer process"));
      }

      continue;
    }

       
                                                                         
                                                                       
                                                                   
       
    if (pid == WalWriterPID)
    {
      WalWriterPID = 0;
      if (!EXIT_STATUS_0(exitstatus))
      {
        HandleChildCrash(pid, exitstatus, _("WAL writer process"));
      }
      continue;
    }

       
                                                                        
                                                                        
                                                                         
                                                      
       
    if (pid == WalReceiverPID)
    {
      WalReceiverPID = 0;
      if (!EXIT_STATUS_0(exitstatus) && !EXIT_STATUS_1(exitstatus))
      {
        HandleChildCrash(pid, exitstatus, _("WAL receiver process"));
      }
      continue;
    }

       
                                                                         
                                                                      
                                                                     
              
       
    if (pid == AutoVacPID)
    {
      AutoVacPID = 0;
      if (!EXIT_STATUS_0(exitstatus))
      {
        HandleChildCrash(pid, exitstatus, _("autovacuum launcher process"));
      }
      continue;
    }

       
                                                                         
                                                                      
                                                                          
                                                               
                                                                        
       
    if (pid == PgArchPID)
    {
      PgArchPID = 0;
      if (!EXIT_STATUS_0(exitstatus))
      {
        LogChildExit(LOG, _("archiver process"), pid, exitstatus);
      }
      if (PgArchStartupAllowed())
      {
        PgArchPID = pgarch_start();
      }
      continue;
    }

       
                                                                        
                                                                         
                                                           
       
    if (pid == PgStatPID)
    {
      PgStatPID = 0;
      if (!EXIT_STATUS_0(exitstatus))
      {
        LogChildExit(LOG, _("statistics collector process"), pid, exitstatus);
      }
      if (pmState == PM_RUN || pmState == PM_HOT_STANDBY)
      {
        PgStatPID = pgstat_start();
      }
      continue;
    }

                                                                  
    if (pid == SysLoggerPID)
    {
      SysLoggerPID = 0;
                                                        
      SysLoggerPID = SysLogger_Start();
      if (!EXIT_STATUS_0(exitstatus))
      {
        LogChildExit(LOG, _("system logger process"), pid, exitstatus);
      }
      continue;
    }

                                               
    if (CleanupBackgroundWorker(pid, exitstatus))
    {
                                
      HaveCrashedWorker = true;
      continue;
    }

       
                                               
       
    CleanupBackend(pid, exitstatus);
  }                                            

     
                                                                            
                         
     
  PostmasterStateMachine();

                                
#ifdef WIN32
  PG_SETMASK(&UnBlockSig);
#endif

  errno = save_errno;
}

   
                                                                            
                                                                                
                           
   
                                                                              
                                                                                
                           
   
static bool
CleanupBackgroundWorker(int pid, int exitstatus)                          
{
  char namebuf[MAXPGPATH];
  slist_mutable_iter iter;

  slist_foreach_modify(iter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;

    rw = slist_container(RegisteredBgWorker, rw_lnode, iter.cur);

    if (rw->rw_pid != pid)
    {
      continue;
    }

#ifdef WIN32
                            
    if (exitstatus == ERROR_WAIT_NO_CHILDREN)
    {
      exitstatus = 0;
    }
#endif

    snprintf(namebuf, MAXPGPATH, _("background worker \"%s\""), rw->rw_worker.bgw_type);

    if (!EXIT_STATUS_0(exitstatus))
    {
                                                                    
      rw->rw_crashed_at = GetCurrentTimestamp();
    }
    else
    {
                                            
      rw->rw_crashed_at = 0;
      rw->rw_terminate = true;
    }

       
                                                                      
                                                                        
                                         
       
    if ((rw->rw_worker.bgw_flags & BGWORKER_SHMEM_ACCESS) != 0)
    {
      if (!EXIT_STATUS_0(exitstatus) && !EXIT_STATUS_1(exitstatus))
      {
        HandleChildCrash(pid, exitstatus, namebuf);
        return true;
      }
    }

       
                                                                        
                                                                          
                                   
       
    if (!ReleasePostmasterChildSlot(rw->rw_child_slot) && (rw->rw_worker.bgw_flags & BGWORKER_SHMEM_ACCESS) != 0)
    {
      HandleChildCrash(pid, exitstatus, namebuf);
      return true;
    }

                                                                    
    dlist_delete(&rw->rw_backend->elem);
#ifdef EXEC_BACKEND
    ShmemBackendArrayRemove(rw->rw_backend);
#endif

       
                                                                    
                                                                           
                                                                     
                         
       
    if (rw->rw_backend->bgworker_notify)
    {
      BackgroundWorkerStopNotifications(rw->rw_pid);
    }
    free(rw->rw_backend);
    rw->rw_backend = NULL;
    rw->rw_pid = 0;
    rw->rw_child_slot = 0;
    ReportBackgroundWorkerExit(&iter);                         

    LogChildExit(EXIT_STATUS_0(exitstatus) ? DEBUG1 : LOG, namebuf, pid, exitstatus);

    return true;
  }

  return false;
}

   
                                                       
   
                                                   
   
                                                         
   
static void
CleanupBackend(int pid, int exitstatus)                           
{
  dlist_mutable_iter iter;

  LogChildExit(DEBUG2, _("server process"), pid, exitstatus);

     
                                                                             
                                                                           
                                                                           
                              
     

#ifdef WIN32

     
                                                                         
                                                                             
                                                                             
                                     
                                                                       
     
  if (exitstatus == ERROR_WAIT_NO_CHILDREN)
  {
    LogChildExit(LOG, _("server process"), pid, exitstatus);
    exitstatus = 0;
  }
#endif

  if (!EXIT_STATUS_0(exitstatus) && !EXIT_STATUS_1(exitstatus))
  {
    HandleChildCrash(pid, exitstatus, _("server process"));
    return;
  }

  dlist_foreach_modify(iter, &BackendList)
  {
    Backend *bp = dlist_container(Backend, elem, iter.cur);

    if (bp->pid == pid)
    {
      if (!bp->dead_end)
      {
        if (!ReleasePostmasterChildSlot(bp->child_slot))
        {
             
                                                                     
                              
             
          HandleChildCrash(pid, exitstatus, _("server process"));
          return;
        }
#ifdef EXEC_BACKEND
        ShmemBackendArrayRemove(bp);
#endif
      }
      if (bp->bgworker_notify)
      {
           
                                                                     
                                                                    
                                                                       
                                                                    
                                                                       
                               
           
        BackgroundWorkerStopNotifications(bp->pid);
      }
      dlist_delete(iter.cur);
      free(bp);
      break;
    }
  }
}

   
                                                                             
                                                
   
                                                                       
                                                                    
   
static void
HandleChildCrash(int pid, int exitstatus, const char *procname)
{
  dlist_mutable_iter iter;
  slist_iter siter;
  Backend *bp;
  bool take_action;

     
                                                                        
                                                                            
                                                                             
                                                                         
                  
     
  take_action = !FatalError && Shutdown != ImmediateShutdown;

  if (take_action)
  {
    LogChildExit(LOG, procname, pid, exitstatus);
    ereport(LOG, (errmsg("terminating any other active server processes")));
  }

                                   
  slist_foreach(siter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;

    rw = slist_container(RegisteredBgWorker, rw_lnode, siter.cur);
    if (rw->rw_pid == 0)
    {
      continue;                  
    }
    if (rw->rw_pid == pid)
    {
         
                                                            
         
      (void)ReleasePostmasterChildSlot(rw->rw_child_slot);
      dlist_delete(&rw->rw_backend->elem);
#ifdef EXEC_BACKEND
      ShmemBackendArrayRemove(rw->rw_backend);
#endif
      free(rw->rw_backend);
      rw->rw_backend = NULL;
      rw->rw_pid = 0;
      rw->rw_child_slot = 0;
                                  
                                           
                                                           
    }
    else
    {
         
                                                                        
                              
         
                                                                        
                                                                       
                                                                        
                                                       
         
      if (take_action)
      {
        ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)rw->rw_pid)));
        signal_child(rw->rw_pid, (SendStop ? SIGSTOP : SIGQUIT));
      }
    }
  }

                                
  dlist_foreach_modify(iter, &BackendList)
  {
    bp = dlist_container(Backend, elem, iter.cur);

    if (bp->pid == pid)
    {
         
                                                             
         
      if (!bp->dead_end)
      {
        (void)ReleasePostmasterChildSlot(bp->child_slot);
#ifdef EXEC_BACKEND
        ShmemBackendArrayRemove(bp);
#endif
      }
      dlist_delete(iter.cur);
      free(bp);
                                                            
    }
    else
    {
         
                                                                         
                              
         
                                                                        
                                                                       
                                                                        
                                                       
         
                                                                      
                                                       
         
                                                                      
               
         
      if (bp->bkend_type == BACKEND_TYPE_BGWORKER)
      {
        continue;
      }

      if (take_action)
      {
        ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)bp->pid)));
        signal_child(bp->pid, (SendStop ? SIGSTOP : SIGQUIT));
      }
    }
  }

                                            
  if (pid == StartupPID)
  {
    StartupPID = 0;
    StartupStatus = STARTUP_CRASHED;
  }
  else if (StartupPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)StartupPID)));
    signal_child(StartupPID, (SendStop ? SIGSTOP : SIGQUIT));
    StartupStatus = STARTUP_SIGNALED;
  }

                                     
  if (pid == BgWriterPID)
  {
    BgWriterPID = 0;
  }
  else if (BgWriterPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)BgWriterPID)));
    signal_child(BgWriterPID, (SendStop ? SIGSTOP : SIGQUIT));
  }

                                         
  if (pid == CheckpointerPID)
  {
    CheckpointerPID = 0;
  }
  else if (CheckpointerPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)CheckpointerPID)));
    signal_child(CheckpointerPID, (SendStop ? SIGSTOP : SIGQUIT));
  }

                                      
  if (pid == WalWriterPID)
  {
    WalWriterPID = 0;
  }
  else if (WalWriterPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)WalWriterPID)));
    signal_child(WalWriterPID, (SendStop ? SIGSTOP : SIGQUIT));
  }

                                        
  if (pid == WalReceiverPID)
  {
    WalReceiverPID = 0;
  }
  else if (WalReceiverPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)WalReceiverPID)));
    signal_child(WalReceiverPID, (SendStop ? SIGSTOP : SIGQUIT));
  }

                                                
  if (pid == AutoVacPID)
  {
    AutoVacPID = 0;
  }
  else if (AutoVacPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", (SendStop ? "SIGSTOP" : "SIGQUIT"), (int)AutoVacPID)));
    signal_child(AutoVacPID, (SendStop ? SIGSTOP : SIGQUIT));
  }

     
                                                                            
                                                                     
                                                                             
                                       
     
  if (PgArchPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", "SIGQUIT", (int)PgArchPID)));
    signal_child(PgArchPID, SIGQUIT);
  }

     
                                                                            
                                                                     
                                                                             
                                       
     
  if (PgStatPID != 0 && take_action)
  {
    ereport(DEBUG2, (errmsg_internal("sending %s to process %d", "SIGQUIT", (int)PgStatPID)));
    signal_child(PgStatPID, SIGQUIT);
    allow_immediate_pgstat_restart();
  }

                                       

  if (Shutdown != ImmediateShutdown)
  {
    FatalError = true;
  }

                                                                  
  if (pmState == PM_RECOVERY || pmState == PM_HOT_STANDBY || pmState == PM_RUN || pmState == PM_STOP_BACKENDS || pmState == PM_SHUTDOWN)
  {
    pmState = PM_WAIT_BACKENDS;
  }

     
                                                                            
                                        
     
  if (AbortStartTime == 0)
  {
    AbortStartTime = time(NULL);
  }
}

   
                                     
   
static void
LogChildExit(int lev, const char *procname, int pid, int exitstatus)
{
     
                                                                    
                               
     
  char activity_buffer[1024];
  const char *activity = NULL;

  if (!EXIT_STATUS_0(exitstatus))
  {
    activity = pgstat_get_crashed_backend_activity(pid, activity_buffer, sizeof(activity_buffer));
  }

  if (WIFEXITED(exitstatus))
  {
    ereport(lev,

                 
                                                                              
                             
        (errmsg("%s (PID %d) exited with exit code %d", procname, pid, WEXITSTATUS(exitstatus)), activity ? errdetail("Failed process was running: %s", activity) : 0));
  }
  else if (WIFSIGNALED(exitstatus))
  {
#if defined(WIN32)
    ereport(lev,

                 
                                                                              
                             
        (errmsg("%s (PID %d) was terminated by exception 0x%X", procname, pid, WTERMSIG(exitstatus)), errhint("See C include file \"ntstatus.h\" for a description of the hexadecimal value."), activity ? errdetail("Failed process was running: %s", activity) : 0));
#else
    ereport(lev,

                 
                                                                              
                             
        (errmsg("%s (PID %d) was terminated by signal %d: %s", procname, pid, WTERMSIG(exitstatus), pg_strsignal(WTERMSIG(exitstatus))), activity ? errdetail("Failed process was running: %s", activity) : 0));
#endif
  }
  else
  {
    ereport(lev,

                 
                                                                              
                             
        (errmsg("%s (PID %d) exited with unrecognized status %d", procname, pid, exitstatus), activity ? errdetail("Failed process was running: %s", activity) : 0));
  }
}

   
                                                                          
   
                                                                          
                                                                
   
static void
PostmasterStateMachine(void)
{
                                                                   
  if (pmState == PM_RUN || pmState == PM_HOT_STANDBY)
  {
    if (connsAllowed == ALLOW_SUPERUSER_CONNS)
    {
         
                                                                        
                        
         
      if (!BackupInProgress())
      {
        connsAllowed = ALLOW_NO_CONNS;
      }
    }

    if (connsAllowed == ALLOW_NO_CONNS)
    {
         
                                                                 
                                                                     
         
      if (CountChildren(BACKEND_TYPE_NORMAL) == 0)
      {
        pmState = PM_STOP_BACKENDS;
      }
    }
  }

     
                                                                          
                                                                            
                                                                 
     
  if (pmState == PM_STOP_BACKENDS)
  {
       
                                                                          
                                                                          
                                                              
       
    ForgetUnstartedBackgroundWorkers();

                                                       
    SignalSomeChildren(SIGTERM, BACKEND_TYPE_ALL - BACKEND_TYPE_WALSND);
                                      
    if (AutoVacPID != 0)
    {
      signal_child(AutoVacPID, SIGTERM);
    }
                              
    if (BgWriterPID != 0)
    {
      signal_child(BgWriterPID, SIGTERM);
    }
                               
    if (WalWriterPID != 0)
    {
      signal_child(WalWriterPID, SIGTERM);
    }
                                                                       
    if (StartupPID != 0)
    {
      signal_child(StartupPID, SIGTERM);
    }
    if (WalReceiverPID != 0)
    {
      signal_child(WalReceiverPID, SIGTERM);
    }
                                                                           

                                                                          
    pmState = PM_WAIT_BACKENDS;
  }

     
                                                                             
                                                            
     
  if (pmState == PM_WAIT_BACKENDS)
  {
       
                                                                    
                                                                        
                                                                         
                                                                        
                                                                         
                                                                  
                                                                       
                                                                      
                                                                    
                
       
    if (CountChildren(BACKEND_TYPE_ALL - BACKEND_TYPE_WALSND) == 0 && StartupPID == 0 && WalReceiverPID == 0 && BgWriterPID == 0 && (CheckpointerPID == 0 || (!FatalError && Shutdown < ImmediateShutdown)) && WalWriterPID == 0 && AutoVacPID == 0)
    {
      if (Shutdown >= ImmediateShutdown || FatalError)
      {
           
                                                                   
                                                               
           
        pmState = PM_WAIT_DEAD_END;

           
                                                                     
                                                              
                             
           
      }
      else
      {
           
                                                                       
                                                                    
                                                     
           
        Assert(Shutdown > NoShutdown);
                                                   
        if (CheckpointerPID == 0)
        {
          CheckpointerPID = StartCheckpointer();
        }
                                      
        if (CheckpointerPID != 0)
        {
          signal_child(CheckpointerPID, SIGUSR2);
          pmState = PM_SHUTDOWN;
        }
        else
        {
             
                                                                  
                                                                  
                                                                   
                                       
             
          FatalError = true;
          pmState = PM_WAIT_DEAD_END;

                                                                     
          SignalChildren(SIGQUIT);
          if (PgArchPID != 0)
          {
            signal_child(PgArchPID, SIGQUIT);
          }
          if (PgStatPID != 0)
          {
            signal_child(PgStatPID, SIGQUIT);
          }
        }
      }
    }
  }

  if (pmState == PM_SHUTDOWN_2)
  {
       
                                                                    
                                                                       
                                                                           
                 
       
    if (PgArchPID == 0 && CountChildren(BACKEND_TYPE_ALL) == 0)
    {
      pmState = PM_WAIT_DEAD_END;
    }
  }

  if (pmState == PM_WAIT_DEAD_END)
  {
       
                                                                          
                                                                     
                               
       
                                                                         
                                                                   
                                                         
                                                                       
                                                                       
                                                                         
                              
       
    if (dlist_is_empty(&BackendList) && PgArchPID == 0 && PgStatPID == 0)
    {
                                                   
      Assert(StartupPID == 0);
      Assert(WalReceiverPID == 0);
      Assert(BgWriterPID == 0);
      Assert(CheckpointerPID == 0);
      Assert(WalWriterPID == 0);
      Assert(AutoVacPID == 0);
                                            
      pmState = PM_NO_CHILDREN;
    }
  }

     
                                                                      
                                                                          
                                                                         
                                                                            
                                                                          
                                         
     
                                                                          
                                                                          
                
     
  if (Shutdown > NoShutdown && pmState == PM_NO_CHILDREN)
  {
    if (FatalError)
    {
      ereport(LOG, (errmsg("abnormal database system shutdown")));
      ExitPostmaster(1);
    }
    else
    {
         
                                                                         
                                                                     
                                                                    
                                                                         
                                                                        
                                                                         
                                                 
         
      if (ReachedNormalRunning)
      {
        CancelBackup();
      }

                                                   
      ExitPostmaster(0);
    }
  }

     
                                                                           
                                                                           
                                                                            
                                                                             
                                      
     
  if (pmState == PM_NO_CHILDREN && (StartupStatus == STARTUP_CRASHED || !restart_after_crash))
  {
    ExitPostmaster(1);
  }

     
                                                                             
                                                    
     
  if (FatalError && pmState == PM_NO_CHILDREN)
  {
    ereport(LOG, (errmsg("all server processes terminated; reinitializing")));

                                                         
    ResetBackgroundWorkerCrashTimes();

    shmem_exit(1);

                                                
    LocalProcessControlFile(true);

    reset_shared(PostPortNumber);

    StartupPID = StartupDataBase();
    Assert(StartupPID != 0);
    StartupStatus = STARTUP_RUNNING;
    pmState = PM_STARTUP;
                                                    
    AbortStartTime = 0;
  }
}

   
                                               
   
                                                                         
                                                                            
                                                                        
                                                                            
                                                                          
             
   
                                                                          
                                                                           
                                                                           
                                                                         
                                            
   
static void
signal_child(pid_t pid, int signal)
{
  if (kill(pid, signal) < 0)
  {
    elog(DEBUG3, "kill(%ld,%d) failed: %m", (long)pid, signal);
  }
#ifdef HAVE_SETSID
  switch (signal)
  {
  case SIGINT:
  case SIGTERM:
  case SIGQUIT:
  case SIGSTOP:
  case SIGKILL:
    if (kill(-pid, signal) < 0)
    {
      elog(DEBUG3, "kill(%ld,%d) failed: %m", (long)(-pid), signal);
    }
    break;
  default:
    break;
  }
#endif
}

   
                                                                     
                                                  
   
static bool
SignalSomeChildren(int signal, int target)
{
  dlist_iter iter;
  bool signaled = false;

  dlist_foreach(iter, &BackendList)
  {
    Backend *bp = dlist_container(Backend, elem, iter.cur);

    if (bp->dead_end)
    {
      continue;
    }

       
                                                                         
                                                                  
       
    if (target != BACKEND_TYPE_ALL)
    {
         
                                                                 
                    
         
      if (bp->bkend_type == BACKEND_TYPE_NORMAL && IsPostmasterChildWalSender(bp->child_slot))
      {
        bp->bkend_type = BACKEND_TYPE_WALSND;
      }

      if (!(target & bp->bkend_type))
      {
        continue;
      }
    }

    ereport(DEBUG4, (errmsg_internal("sending signal %d to process %d", signal, (int)bp->pid)));
    signal_child(bp->pid, signal);
    signaled = true;
  }
  return signaled;
}

   
                                                                              
                                                      
   
static void
TerminateChildren(int signal)
{
  SignalChildren(signal);
  if (StartupPID != 0)
  {
    signal_child(StartupPID, signal);
    if (signal == SIGQUIT || signal == SIGKILL)
    {
      StartupStatus = STARTUP_SIGNALED;
    }
  }
  if (BgWriterPID != 0)
  {
    signal_child(BgWriterPID, signal);
  }
  if (CheckpointerPID != 0)
  {
    signal_child(CheckpointerPID, signal);
  }
  if (WalWriterPID != 0)
  {
    signal_child(WalWriterPID, signal);
  }
  if (WalReceiverPID != 0)
  {
    signal_child(WalReceiverPID, signal);
  }
  if (AutoVacPID != 0)
  {
    signal_child(AutoVacPID, signal);
  }
  if (PgArchPID != 0)
  {
    signal_child(PgArchPID, signal);
  }
  if (PgStatPID != 0)
  {
    signal_child(PgStatPID, signal);
  }
}

   
                                           
   
                                                                  
   
                                                                       
   
static int
BackendStartup(Port *port)
{
  Backend *bn;                          
  pid_t pid;

     
                                                                        
                             
     
  bn = (Backend *)malloc(sizeof(Backend));
  if (!bn)
  {
    ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    return STATUS_ERROR;
  }

     
                                                                       
                                                                        
                                                                   
     
  if (!RandomCancelKey(&MyCancelKey))
  {
    free(bn);
    ereport(LOG, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("could not generate random cancel key")));
    return STATUS_ERROR;
  }

  bn->cancel_key = MyCancelKey;

                                            
  port->canAcceptConnections = canAcceptConnections(BACKEND_TYPE_NORMAL);
  bn->dead_end = (port->canAcceptConnections != CAC_OK && port->canAcceptConnections != CAC_SUPERUSER);

     
                                                                 
     
  if (!bn->dead_end)
  {
    bn->child_slot = MyPMChildSlot = AssignPostmasterChildSlot();
  }
  else
  {
    bn->child_slot = 0;
  }

                                                           
  bn->bgworker_notify = false;

#ifdef EXEC_BACKEND
  pid = backend_forkexec(port);
#else                     
  pid = fork_process();
  if (pid == 0)            
  {
    free(bn);

                                  
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

                                                                      
    BackendInitialize(port);

                             
    BackendRun(port);
  }
#endif                   

  if (pid < 0)
  {
                                
    int save_errno = errno;

    if (!bn->dead_end)
    {
      (void)ReleasePostmasterChildSlot(bn->child_slot);
    }
    free(bn);
    errno = save_errno;
    ereport(LOG, (errmsg("could not fork new process for connection: %m")));
    report_fork_failure_to_client(port, save_errno);
    return STATUS_ERROR;
  }

                                  
  ereport(DEBUG2, (errmsg_internal("forked new backend, pid=%d socket=%d", (int)pid, (int)port->sock)));

     
                                                                             
                  
     
  bn->pid = pid;
  bn->bkend_type = BACKEND_TYPE_NORMAL;                                 
  dlist_push_head(&BackendList, &bn->elem);

#ifdef EXEC_BACKEND
  if (!bn->dead_end)
  {
    ShmemBackendArrayAdd(bn);
  }
#endif

  return STATUS_OK;
}

   
                                                                      
                                                                        
                                                                             
   
                                                                          
                            
   
static void
report_fork_failure_to_client(Port *port, int errnum)
{
  char buffer[1000];
  int rc;

                                                            
  snprintf(buffer, sizeof(buffer), "E%s%s\n", _("could not fork new process for connection: "), strerror(errnum));

                                                                
  if (!pg_set_noblock(port->sock))
  {
    return;
  }

                                                              
  do
  {
    rc = send(port->sock, buffer, strlen(buffer) + 1, 0);
  } while (rc < 0 && errno == EINTR);
}

   
                                                                     
                                                                
   
                                                                     
   
                                                                          
                                                                         
                                                                           
   
static void
BackendInitialize(Port *port)
{
  int status;
  int ret;
  char remote_host[NI_MAXHOST];
  char remote_port[NI_MAXSERV];
  char remote_ps_data[NI_MAXHOST];

                                    
  MyProcPort = port;

     
                                                                       
                                                                             
                                                                    
                                                                             
                                                 
     
  if (PreAuthDelay > 0)
  {
    pg_usleep(PreAuthDelay * 1000000L);
  }

                                                                            
  ClientAuthInProgress = true;                                       

                                                                        
  port->remote_host = "";
  port->remote_port = "";

     
                                                                            
                                                                          
     
  pq_init();                                                              
  whereToSendOutput = DestRemote;                                    

     
                                                                          
                                                                    
                                                                           
                                                                           
     
                                                                            
                                                                        
                                                                         
                                                                            
                                                                           
                        
     
                                                                             
                                                                           
                                                                       
                                                                      
                                                                      
                                                                       
                                                                       
                                                                           
                                                                             
     
  pqsignal(SIGTERM, process_startup_packet_die);
  pqsignal(SIGQUIT, process_startup_packet_quickdie);
  InitializeTimeouts();                                  
  PG_SETMASK(&StartupBlockSig);

     
                                                                       
     
  remote_host[0] = '\0';
  remote_port[0] = '\0';
  if ((ret = pg_getnameinfo_all(&port->raddr.addr, port->raddr.salen, remote_host, sizeof(remote_host), remote_port, sizeof(remote_port), (log_hostname ? 0 : NI_NUMERICHOST) | NI_NUMERICSERV)) != 0)
  {
    ereport(WARNING, (errmsg_internal("pg_getnameinfo_all() failed: %s", gai_strerror(ret))));
  }
  if (remote_port[0] == '\0')
  {
    snprintf(remote_ps_data, sizeof(remote_ps_data), "%s", remote_host);
  }
  else
  {
    snprintf(remote_ps_data, sizeof(remote_ps_data), "%s(%s)", remote_host, remote_port);
  }

     
                                                                          
                                                            
     
  port->remote_host = strdup(remote_host);
  port->remote_port = strdup(remote_port);

                                                                   
  if (Log_connections)
  {
    if (remote_port[0])
    {
      ereport(LOG, (errmsg("connection received: host=%s port=%s", remote_host, remote_port)));
    }
    else
    {
      ereport(LOG, (errmsg("connection received: host=%s", remote_host)));
    }
  }

     
                                                                           
                                                                      
     
                                                                             
                                                                           
                                                                            
                                                                          
                                                                    
                                                                     
     
  if (log_hostname && ret == 0 && strspn(remote_host, "0123456789.") < strlen(remote_host) && strspn(remote_host, "0123456789ABCDEFabcdef:") < strlen(remote_host))
  {
    port->remote_hostname = strdup(remote_host);
  }

     
                                                                          
                                                                        
                                                                            
                             
     
                                                                       
                                                                            
                                                                      
                                                                            
     
                                                                        
                                                                        
                                                      
     
  RegisterTimeout(STARTUP_PACKET_TIMEOUT, StartupPacketTimeoutHandler);
  enable_timeout_after(STARTUP_PACKET_TIMEOUT, AuthenticationTimeout * 1000);

     
                                                                             
              
     
  status = ProcessStartupPacket(port, false, false);

     
                                                             
     
  disable_timeout(STARTUP_PACKET_TIMEOUT, false);
  PG_SETMASK(&BlockSig);

     
                                                                       
                                                  
     
  if (status != STATUS_OK)
  {
    proc_exit(0);
  }

     
                                                                         
                                                                          
     
                                                                   
     
                                                  
     
                                                                             
                                                            
                                                                  
     
  if (am_walsender)
  {
    init_ps_display(pgstat_get_backend_desc(B_WAL_SENDER), port->user_name, remote_ps_data, update_process_title ? "authentication" : "");
  }
  else
  {
    init_ps_display(port->user_name, port->database_name, remote_ps_data, update_process_title ? "authentication" : "");
  }
}

   
                                                                              
   
            
                             
                                            
   
static void
BackendRun(Port *port)
{
  char **av;
  int maxac;
  int ac;
  int i;

     
                                                                    
     
                                                                          
                                                              
                      
     
  maxac = 2;                                    
  maxac += (strlen(ExtraOptions) + 1) / 2;

  av = (char **)MemoryContextAlloc(TopMemoryContext, maxac * sizeof(char *));
  ac = 0;

  av[ac++] = "postgres";

     
                                                                         
                                                
     
  pg_split_opts(av, &ac, ExtraOptions);

  av[ac] = NULL;

  Assert(ac < maxac);

     
                                                    
     
  ereport(DEBUG3, (errmsg_internal("%s child[%d]: starting with (", progname, (int)getpid())));
  for (i = 0; i < ac; ++i)
  {
    ereport(DEBUG3, (errmsg_internal("\t%s", av[i])));
  }
  ereport(DEBUG3, (errmsg_internal(")")));

     
                                                                            
                                                                     
     
  MemoryContextSwitchTo(TopMemoryContext);

  PostgresMain(ac, av, port->database_name, port->user_name);
}

#ifdef EXEC_BACKEND

   
                                                                
   
                                                                          
                                                                 
   
                                                                          
                                        
   
                                                                      
                  
   
pid_t
postmaster_forkexec(int argc, char *argv[])
{
  Port port;

                                                                   
  memset(&port, 0, sizeof(port));
  return internal_forkexec(argc, argv, &port);
}

   
                                                       
   
                                                                           
                                                                    
                                    
   
                                                                
   
static pid_t
backend_forkexec(Port *port)
{
  char *av[4];
  int ac = 0;

  av[ac++] = "postgres";
  av[ac++] = "--forkbackend";
  av[ac++] = NULL;                                     

  av[ac] = NULL;
  Assert(ac < lengthof(av));

  return internal_forkexec(ac, av, port);
}

#ifndef WIN32

   
                                              
   
                                                        
                                                   
   
static pid_t
internal_forkexec(int argc, char *argv[], Port *port)
{
  static unsigned long tmpBackendFileNum = 0;
  pid_t pid;
  char tmpfilename[MAXPGPATH];
  BackendParameters param;
  FILE *fp;

  if (!save_backend_variables(&param, port))
  {
    return -1;                                         
  }

                                    
  snprintf(tmpfilename, MAXPGPATH, "%s/%s.backend_var.%d.%lu", PG_TEMP_FILES_DIR, PG_TEMP_FILE_PREFIX, MyProcPid, ++tmpBackendFileNum);

                 
  fp = AllocateFile(tmpfilename, PG_BINARY_W);
  if (!fp)
  {
       
                                                                      
                                   
       
    (void)MakePGDirectory(PG_TEMP_FILES_DIR);

    fp = AllocateFile(tmpfilename, PG_BINARY_W);
    if (!fp)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmpfilename)));
      return -1;
    }
  }

  if (fwrite(&param, sizeof(param), 1, fp) != 1)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmpfilename)));
    FreeFile(fp);
    return -1;
  }

                    
  if (FreeFile(fp))
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmpfilename)));
    return -1;
  }

                                             
  Assert(argc >= 3);
  Assert(argv[argc] == NULL);
  Assert(strncmp(argv[1], "--fork", 6) == 0);
  Assert(argv[2] == NULL);

                                                   
  argv[2] = tmpfilename;

                               
  if ((pid = fork_process()) == 0)
  {
    if (execv(postgres_exec_path, argv) < 0)
    {
      ereport(LOG, (errmsg("could not execute server process \"%s\": %m", postgres_exec_path)));
                                                                 
      exit(1);
    }
  }

  return pid;                                                
}
#else            

   
                                          
   
                                                              
                                                        
                                                              
                                      
                                                                     
                      
   
static pid_t
internal_forkexec(int argc, char *argv[], Port *port)
{
  int retry_count = 0;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  int i;
  int j;
  char cmdLine[MAXPGPATH * 2];
  HANDLE paramHandle;
  BackendParameters *param;
  SECURITY_ATTRIBUTES sa;
  char paramHandleStr[32];
  win32_deadchild_waitinfo *childinfo;

                                             
  Assert(argc >= 3);
  Assert(argv[argc] == NULL);
  Assert(strncmp(argv[1], "--fork", 6) == 0);
  Assert(argv[2] == NULL);

                                       
retry:

                                                  
  ZeroMemory(&sa, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  paramHandle = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(BackendParameters), NULL);
  if (paramHandle == INVALID_HANDLE_VALUE)
  {
    elog(LOG, "could not create backend parameter file mapping: error code %lu", GetLastError());
    return -1;
  }

  param = MapViewOfFile(paramHandle, FILE_MAP_WRITE, 0, 0, sizeof(BackendParameters));
  if (!param)
  {
    elog(LOG, "could not map backend parameter memory: error code %lu", GetLastError());
    CloseHandle(paramHandle);
    return -1;
  }

                                                   
#ifdef _WIN64
  sprintf(paramHandleStr, "%llu", (LONG_PTR)paramHandle);
#else
  sprintf(paramHandleStr, "%lu", (DWORD)paramHandle);
#endif
  argv[2] = paramHandleStr;

                           
  cmdLine[sizeof(cmdLine) - 1] = '\0';
  cmdLine[sizeof(cmdLine) - 2] = '\0';
  snprintf(cmdLine, sizeof(cmdLine) - 1, "\"%s\"", postgres_exec_path);
  i = 0;
  while (argv[++i] != NULL)
  {
    j = strlen(cmdLine);
    snprintf(cmdLine + j, sizeof(cmdLine) - 1 - j, " \"%s\"", argv[i]);
  }
  if (cmdLine[sizeof(cmdLine) - 2] != '\0')
  {
    elog(LOG, "subprocess command line too long");
    return -1;
  }

  memset(&pi, 0, sizeof(pi));
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);

     
                                                                             
                                                  
     
  if (!CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
  {
    elog(LOG, "CreateProcess call failed: %m (error code %lu)", GetLastError());
    return -1;
  }

  if (!save_backend_variables(param, port, pi.hProcess, pi.dwProcessId))
  {
       
                                                                       
                                          
       
    if (!TerminateProcess(pi.hProcess, 255))
    {
      ereport(LOG, (errmsg_internal("could not terminate unstarted process: error code %lu", GetLastError())));
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return -1;                                         
  }

                                                                             
  if (!UnmapViewOfFile(param))
  {
    elog(LOG, "could not unmap view of backend parameter file: error code %lu", GetLastError());
  }
  if (!CloseHandle(paramHandle))
  {
    elog(LOG, "could not close handle to backend parameter file: error code %lu", GetLastError());
  }

     
                                                                             
                                                                             
                                                                            
                                                                      
                        
     
  if (!pgwin32_ReserveSharedMemoryRegion(pi.hProcess))
  {
                                                                    
    if (!TerminateProcess(pi.hProcess, 255))
    {
      ereport(LOG, (errmsg_internal("could not terminate process that failed to reserve memory: error code %lu", GetLastError())));
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (++retry_count < 100)
    {
      goto retry;
    }
    ereport(LOG, (errmsg("giving up after too many tries to reserve shared memory"), errhint("This might be caused by ASLR or antivirus software.")));
    return -1;
  }

     
                                                                        
                                                                         
                   
     
  if (ResumeThread(pi.hThread) == -1)
  {
    if (!TerminateProcess(pi.hProcess, 255))
    {
      ereport(LOG, (errmsg_internal("could not terminate unstartable process: error code %lu", GetLastError())));
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
      return -1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ereport(LOG, (errmsg_internal("could not resume thread of unstarted process: error code %lu", GetLastError())));
    return -1;
  }

     
                                                                             
                                                       
     
                                                                           
                                                                       
                       
     
  childinfo = malloc(sizeof(win32_deadchild_waitinfo));
  if (!childinfo)
  {
    ereport(FATAL, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }

  childinfo->procHandle = pi.hProcess;
  childinfo->procId = pi.dwProcessId;

  if (!RegisterWaitForSingleObject(&childinfo->waitHandle, pi.hProcess, pgwin32_deadchild_callback, childinfo, INFINITE, WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD))
  {
    ereport(FATAL, (errmsg_internal("could not register process for wait: error code %lu", GetLastError())));
  }

                                                                         

  CloseHandle(pi.hThread);

  return pi.dwProcessId;
}
#endif            

   
                                                                            
                                                                 
                                        
   
                                                                       
                                                                      
                                                                         
                                                                         
                                 
   
void
SubPostmasterMain(int argc, char *argv[])
{
  Port port;

                                                                      
  IsPostmasterEnvironment = true;
  whereToSendOutput = DestNone;

                                 
  InitPostmasterChild();

                                                                    
  InitializeGUCOptions();

                                     
  if (argc < 3)
  {
    elog(FATAL, "invalid subpostmaster invocation");
  }

                                  
  memset(&port, 0, sizeof(Port));
  read_backend_variables(argv[2], &port);

                                                                
  ClosePostmasterPorts(strcmp(argv[1], "--forklog") == 0);

     
                                                  
     
  set_stack_base();

     
                                                                            
                                                                             
                                                                            
                                                    
     
                                                                          
                              
     
                                           
     
                                                                            
                                                                             
                                                                             
                                                                             
     
  if (strcmp(argv[1], "--forkbackend") == 0 || strcmp(argv[1], "--forkavlauncher") == 0 || strcmp(argv[1], "--forkavworker") == 0 || strcmp(argv[1], "--forkboot") == 0 || strncmp(argv[1], "--forkbgworker=", 15) == 0)
  {
    PGSharedMemoryReAttach();
  }
  else
  {
    PGSharedMemoryNoReAttach();
  }

                                                            
  if (strcmp(argv[1], "--forkavlauncher") == 0)
  {
    AutovacuumLauncherIAm();
  }
  if (strcmp(argv[1], "--forkavworker") == 0)
  {
    AutovacuumWorkerIAm();
  }

     
                                                                         
                                                                            
                              
     
#ifdef WIN32
  pgwin32_signal_initialize();
#endif

                                                                      
  pqinitmask();
  PG_SETMASK(&BlockSig);

                                       
  read_nondefault_variables();

     
                                                                          
                                                                          
                                                                           
                                                
     
  checkDataDir();

     
                                                                        
                                                                       
     
  LocalProcessControlFile(false);

     
                                                                           
                                                                            
                                                                         
                                
     
  process_shared_preload_libraries();

                                        
  if (strcmp(argv[1], "--forkbackend") == 0)
  {
    Assert(argc == 3);                                 

       
                                                                      
                                                                         
                                   
       
                                                                        
                                                                  
                                        
       
                                                                          
                                            
       
#ifdef USE_SSL
    if (EnableSSL)
    {
      if (secure_initialize(false) == 0)
      {
        LoadedSSL = true;
      }
      else
      {
        ereport(LOG, (errmsg("SSL configuration could not be loaded in child process")));
      }
    }
#endif

       
                                                                     
       
                                                                           
                                                                      
                                                                     
                                                                       
                                       
       
    BackendInitialize(&port);

                                              
    InitShmemAccess(UsedShmemSegAddr);

                                                              
    InitProcess();

                                                  
    CreateSharedMemoryAndSemaphores(0);

                             
    BackendRun(&port);                      
  }
  if (strcmp(argv[1], "--forkboot") == 0)
  {
                                              
    InitShmemAccess(UsedShmemSegAddr);

                                                              
    InitAuxiliaryProcess();

                                                  
    CreateSharedMemoryAndSemaphores(0);

    AuxiliaryProcessMain(argc - 2, argv + 2);                      
  }
  if (strcmp(argv[1], "--forkavlauncher") == 0)
  {
                                              
    InitShmemAccess(UsedShmemSegAddr);

                                                              
    InitProcess();

                                                  
    CreateSharedMemoryAndSemaphores(0);

    AutoVacLauncherMain(argc - 2, argv + 2);                      
  }
  if (strcmp(argv[1], "--forkavworker") == 0)
  {
                                              
    InitShmemAccess(UsedShmemSegAddr);

                                                              
    InitProcess();

                                                  
    CreateSharedMemoryAndSemaphores(0);

    AutoVacWorkerMain(argc - 2, argv + 2);                      
  }
  if (strncmp(argv[1], "--forkbgworker=", 15) == 0)
  {
    int shmem_slot;

                                                                           
    IsBackgroundWorker = true;

                                              
    InitShmemAccess(UsedShmemSegAddr);

                                                              
    InitProcess();

                                                  
    CreateSharedMemoryAndSemaphores(0);

                                                  
    shmem_slot = atoi(argv[1] + 15);
    MyBgworkerEntry = BackgroundWorkerEntry(shmem_slot);

    StartBackgroundWorker();
  }
  if (strcmp(argv[1], "--forkarch") == 0)
  {
                                                

    PgArchiverMain(argc, argv);                      
  }
  if (strcmp(argv[1], "--forkcol") == 0)
  {
                                                

    PgstatCollectorMain(argc, argv);                      
  }
  if (strcmp(argv[1], "--forklog") == 0)
  {
                                                

    SysLoggerMain(argc, argv);                      
  }

  abort();                         
}
#endif                   

   
                             
   
                                                           
   
static void
ExitPostmaster(int status)
{
#ifdef HAVE_PTHREAD_IS_THREADED_NP

     
                                                                            
                                                                         
                                                                            
                                                                  
     
  if (pthread_is_threaded_np() != 0)
  {
    ereport(LOG, (errcode(ERRCODE_INTERNAL_ERROR), errmsg_internal("postmaster became multithreaded"), errdetail("Please report this to <pgsql-bugs@lists.postgresql.org>.")));
  }
#endif

                                                          

     
                                                                           
                                           
     
                               
     

  proc_exit(status);
}

   
                                                                   
   
static void
sigusr1_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                                                            
                                                                        
     
#ifdef WIN32
  PG_SETMASK(&BlockSig);
#endif

     
                                                                   
                                                                            
                                                                        
                                                               
     
  if (CheckPostmasterSignal(PMSIGNAL_RECOVERY_STARTED) && pmState == PM_STARTUP && Shutdown == NoShutdown)
  {
                                                              
    FatalError = false;
    Assert(AbortStartTime == 0);

       
                                                                        
                                   
       
    Assert(CheckpointerPID == 0);
    CheckpointerPID = StartCheckpointer();
    Assert(BgWriterPID == 0);
    BgWriterPID = StartBackgroundWriter();

       
                                                                           
              
       
    Assert(PgArchPID == 0);
    if (XLogArchivingAlways())
    {
      PgArchPID = pgarch_start();
    }

       
                                                                    
                                                                           
                    
       
    if (!EnableHotStandby)
    {
      AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_STANDBY);
#ifdef USE_SYSTEMD
      sd_notify(0, "READY=1");
#endif
    }

    pmState = PM_RECOVERY;
  }

  if (CheckPostmasterSignal(PMSIGNAL_BEGIN_HOT_STANDBY) && pmState == PM_RECOVERY && Shutdown == NoShutdown)
  {
       
                                                         
       
    Assert(PgStatPID == 0);
    PgStatPID = pgstat_start();

    ereport(LOG, (errmsg("database system is ready to accept read only connections")));

                       
    AddToDataDirLockFile(LOCK_FILE_LINE_PM_STATUS, PM_STATUS_READY);
#ifdef USE_SYSTEMD
    sd_notify(0, "READY=1");
#endif

    pmState = PM_HOT_STANDBY;
    connsAllowed = ALLOW_ALL_CONNS;

                                                    
    StartWorkerNeeded = true;
  }

                                                
  if (CheckPostmasterSignal(PMSIGNAL_BACKGROUND_WORKER_CHANGE))
  {
                                                          
    BackgroundWorkerStateChange(pmState < PM_STOP_BACKENDS);
    StartWorkerNeeded = true;
  }

  if (StartWorkerNeeded || HaveCrashedWorker)
  {
    maybe_start_bgworkers();
  }

  if (CheckPostmasterSignal(PMSIGNAL_WAKEN_ARCHIVER) && PgArchPID != 0)
  {
       
                                                                           
                      
       
    signal_child(PgArchPID, SIGUSR1);
  }

                                                     
  if (SysLoggerPID != 0)
  {
    if (CheckLogrotateSignal())
    {
      signal_child(SysLoggerPID, SIGUSR1);
      RemoveLogrotateSignalFiles();
    }
    else if (CheckPostmasterSignal(PMSIGNAL_ROTATE_LOGFILE))
    {
      signal_child(SysLoggerPID, SIGUSR1);
    }
  }

  if (CheckPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER) && Shutdown <= SmartShutdown && pmState < PM_STOP_BACKENDS)
  {
       
                                                                           
                                                                           
                                                                           
                                                                         
                                                                         
                                                                      
                  
       
    start_autovac_launcher = true;
  }

  if (CheckPostmasterSignal(PMSIGNAL_START_AUTOVAC_WORKER) && Shutdown <= SmartShutdown && pmState < PM_STOP_BACKENDS)
  {
                                                                     
    StartAutovacuumWorker();
  }

  if (CheckPostmasterSignal(PMSIGNAL_START_WALRECEIVER))
  {
                                                                    
                                                                         
    WalReceiverRequested = true;
    MaybeStartWalReceiver();
  }

     
                                                                        
     
                                                                             
                                                                            
                                                                        
                                                                        
                                                                       
                                        
     
  if (CheckPostmasterSignal(PMSIGNAL_ADVANCE_STATE_MACHINE))
  {
    PostmasterStateMachine();
  }

  if (StartupPID != 0 && (pmState == PM_STARTUP || pmState == PM_RECOVERY || pmState == PM_HOT_STANDBY) && CheckPromoteSignal())
  {
                                                 
    signal_child(StartupPID, SIGUSR2);
  }

#ifdef WIN32
  PG_SETMASK(&UnBlockSig);
#endif

  errno = save_errno;
}

   
                                            
                         
   
                                                                        
                                                                         
                                                                        
                                                                      
                                                                       
   
                                                                       
                                                                          
                                                                       
                                                       
   
static void
process_startup_packet_die(SIGNAL_ARGS)
{
  proc_exit(1);
}

   
                                            
   
                                     
                                                 
   
static void
process_startup_packet_quickdie(SIGNAL_ARGS)
{
     
                                                                            
                                                                           
                      
     
                                                                             
                                                                        
                                                                          
                                                                           
                        
     
  _exit(2);
}

   
                        
   
                                                                         
                                                                         
                                                                             
                                                                         
                     
   
static void
dummy_handler(SIGNAL_ARGS)
{
}

   
                                            
                                                                 
   
                                                                               
                                                                         
                                   
   
static void
StartupPacketTimeoutHandler(void)
{
  proc_exit(1);
}

   
                                 
   
static bool
RandomCancelKey(int32 *cancel_key)
{
  return pg_strong_random(cancel_key, sizeof(int32));
}

   
                                                                            
                         
   
static int
CountChildren(int target)
{
  dlist_iter iter;
  int cnt = 0;

  dlist_foreach(iter, &BackendList)
  {
    Backend *bp = dlist_container(Backend, elem, iter.cur);

    if (bp->dead_end)
    {
      continue;
    }

       
                                                                         
                                                                  
       
    if (target != BACKEND_TYPE_ALL)
    {
         
                                                                 
                    
         
      if (bp->bkend_type == BACKEND_TYPE_NORMAL && IsPostmasterChildWalSender(bp->child_slot))
      {
        bp->bkend_type = BACKEND_TYPE_WALSND;
      }

      if (!(target & bp->bkend_type))
      {
        continue;
      }
    }

    cnt++;
  }
  return cnt;
}

   
                                                                      
   
                                                                          
                                                                         
   
                                                                        
                        
   
static pid_t
StartChildProcess(AuxProcType type)
{
  pid_t pid;
  char *av[10];
  int ac = 0;
  char typebuf[32];

     
                                                  
     
  av[ac++] = "postgres";

#ifdef EXEC_BACKEND
  av[ac++] = "--forkboot";
  av[ac++] = NULL;                                       
#endif

  snprintf(typebuf, sizeof(typebuf), "-x%d", type);
  av[ac++] = typebuf;

  av[ac] = NULL;
  Assert(ac < lengthof(av));

#ifdef EXEC_BACKEND
  pid = postmaster_forkexec(ac, av);
#else                     
  pid = fork_process();

  if (pid == 0)            
  {
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

                                                     
    MemoryContextSwitchTo(TopMemoryContext);
    MemoryContextDelete(PostmasterContext);
    PostmasterContext = NULL;

    AuxiliaryProcessMain(ac, av);
    ExitPostmaster(0);
  }
#endif                   

  if (pid < 0)
  {
                                
    int save_errno = errno;

    errno = save_errno;
    switch (type)
    {
    case StartupProcess:
      ereport(LOG, (errmsg("could not fork startup process: %m")));
      break;
    case BgWriterProcess:
      ereport(LOG, (errmsg("could not fork background writer process: %m")));
      break;
    case CheckpointerProcess:
      ereport(LOG, (errmsg("could not fork checkpointer process: %m")));
      break;
    case WalWriterProcess:
      ereport(LOG, (errmsg("could not fork WAL writer process: %m")));
      break;
    case WalReceiverProcess:
      ereport(LOG, (errmsg("could not fork WAL receiver process: %m")));
      break;
    default:
      ereport(LOG, (errmsg("could not fork process: %m")));
      break;
    }

       
                                                                          
                                                        
       
    if (type == StartupProcess)
    {
      ExitPostmaster(1);
    }
    return 0;
  }

     
                                
     
  return pid;
}

   
                         
                                     
   
                                                                      
                                       
   
                                                        
   
static void
StartAutovacuumWorker(void)
{
  Backend *bn;

     
                                                                           
                                                                            
                                                                           
                                                                       
              
     
  if (canAcceptConnections(BACKEND_TYPE_AUTOVAC) == CAC_OK)
  {
       
                                                                        
                                                                     
                                                                       
                                            
       
    if (!RandomCancelKey(&MyCancelKey))
    {
      ereport(LOG, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("could not generate random cancel key")));
      return;
    }

    bn = (Backend *)malloc(sizeof(Backend));
    if (bn)
    {
      bn->cancel_key = MyCancelKey;

                                                                  
      bn->dead_end = false;
      bn->child_slot = MyPMChildSlot = AssignPostmasterChildSlot();
      bn->bgworker_notify = false;

      bn->pid = StartAutoVacWorker();
      if (bn->pid > 0)
      {
        bn->bkend_type = BACKEND_TYPE_AUTOVAC;
        dlist_push_head(&BackendList, &bn->elem);
#ifdef EXEC_BACKEND
        ShmemBackendArrayAdd(bn);
#endif
                    
        return;
      }

         
                                                                         
                                      
         
      (void)ReleasePostmasterChildSlot(bn->child_slot);
      free(bn);
    }
    else
    {
      ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }
  }

     
                                                                            
                                                                        
                                                                           
                                                                           
                                                                           
                                                                          
                      
     
  if (AutoVacPID != 0)
  {
    AutoVacWorkerFailed();
    avlauncher_needs_signal = true;
  }
}

   
                         
                                                                         
   
                                                                            
                                                                         
                                                                             
                                                                             
                                                                    
                                                                          
                                                                 
   
static void
MaybeStartWalReceiver(void)
{
  if (WalReceiverPID == 0 && (pmState == PM_STARTUP || pmState == PM_RECOVERY || pmState == PM_HOT_STANDBY) && Shutdown <= SmartShutdown)
  {
    WalReceiverPID = StartWalReceiver();
    if (WalReceiverPID != 0)
    {
      WalReceiverRequested = false;
    }
                                                           
  }
}

   
                        
   
static bool
CreateOptsFile(int argc, char *argv[], char *fullprogname)
{
  FILE *fp;
  int i;

#define OPTS_FILE "postmaster.opts"

  if ((fp = fopen(OPTS_FILE, "w")) == NULL)
  {
    elog(LOG, "could not create file \"%s\": %m", OPTS_FILE);
    return false;
  }

  fprintf(fp, "%s", fullprogname);
  for (i = 1; i < argc; i++)
  {
    fprintf(fp, " \"%s\"", argv[i]);
  }
  fputs("\n", fp);

  if (fclose(fp))
  {
    elog(LOG, "could not write file \"%s\": %m", OPTS_FILE);
    return false;
  }

  return true;
}

   
                             
   
                                                                         
                                                                        
                                                                      
                                                                           
                                                                        
                                                                                
                                                             
   
int
MaxLivePostmasterChildren(void)
{
  return 2 * (MaxConnections + autovacuum_max_workers + 1 + max_wal_senders + max_worker_processes);
}

   
                                            
   
void
BackgroundWorkerInitializeConnection(const char *dbname, const char *username, uint32 flags)
{
  BackgroundWorker *worker = MyBgworkerEntry;

                                      
  if (!(worker->bgw_flags & BGWORKER_BACKEND_DATABASE_CONNECTION))
  {
    ereport(FATAL, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("database connection requirement not indicated during registration")));
  }

  InitPostgres(dbname, InvalidOid, username, InvalidOid, NULL, (flags & BGWORKER_BYPASS_ALLOWCONN) != 0);

                                                       
  if (!IsInitProcessingMode())
  {
    ereport(ERROR, (errmsg("invalid processing mode in background worker")));
  }
  SetProcessingMode(NormalProcessing);
}

   
                                                       
   
void
BackgroundWorkerInitializeConnectionByOid(Oid dboid, Oid useroid, uint32 flags)
{
  BackgroundWorker *worker = MyBgworkerEntry;

                                      
  if (!(worker->bgw_flags & BGWORKER_BACKEND_DATABASE_CONNECTION))
  {
    ereport(FATAL, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("database connection requirement not indicated during registration")));
  }

  InitPostgres(NULL, dboid, NULL, useroid, NULL, (flags & BGWORKER_BYPASS_ALLOWCONN) != 0);

                                                       
  if (!IsInitProcessingMode())
  {
    ereport(ERROR, (errmsg("invalid processing mode in background worker")));
  }
  SetProcessingMode(NormalProcessing);
}

   
                                                
   
void
BackgroundWorkerBlockSignals(void)
{
  PG_SETMASK(&BlockSig);
}

void
BackgroundWorkerUnblockSignals(void)
{
  PG_SETMASK(&UnBlockSig);
}

#ifdef EXEC_BACKEND
static pid_t
bgworker_forkexec(int shmem_slot)
{
  char *av[10];
  int ac = 0;
  char forkav[MAXPGPATH];

  snprintf(forkav, MAXPGPATH, "--forkbgworker=%d", shmem_slot);

  av[ac++] = "postgres";
  av[ac++] = forkav;
  av[ac++] = NULL;                                       
  av[ac] = NULL;

  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}
#endif

   
                         
                                                            
   
                                              
                                                                        
   
                                                    
   
static bool
do_start_bgworker(RegisteredBgWorker *rw)
{
  pid_t worker_pid;

  Assert(rw->rw_pid == 0);

     
                                                                           
                                                                             
                     
     
                                                                    
                                                                           
                                                                         
                                         
     
  if (!assign_backendlist_entry(rw))
  {
    rw->rw_crashed_at = GetCurrentTimestamp();
    return false;
  }

  ereport(DEBUG1, (errmsg("starting background worker process \"%s\"", rw->rw_worker.bgw_name)));

#ifdef EXEC_BACKEND
  switch ((worker_pid = bgworker_forkexec(rw->rw_shmem_slot)))
#else
  switch ((worker_pid = fork_process()))
#endif
  {
  case -1:
                                        
    ereport(LOG, (errmsg("could not fork worker process: %m")));
                                                
    ReleasePostmasterChildSlot(rw->rw_child_slot);
    rw->rw_child_slot = 0;
    free(rw->rw_backend);
    rw->rw_backend = NULL;
                                                         
    rw->rw_crashed_at = GetCurrentTimestamp();
    break;

#ifndef EXEC_BACKEND
  case 0:
                                 
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

       
                                                                   
                                  
       
    MyBgworkerEntry = (BackgroundWorker *)MemoryContextAlloc(TopMemoryContext, sizeof(BackgroundWorker));
    memcpy(MyBgworkerEntry, &rw->rw_worker, sizeof(BackgroundWorker));

                                                     
    MemoryContextSwitchTo(TopMemoryContext);
    MemoryContextDelete(PostmasterContext);
    PostmasterContext = NULL;

    StartBackgroundWorker();

    exit(1);                          
    break;
#endif
  default:
                                            
    rw->rw_pid = worker_pid;
    rw->rw_backend->pid = rw->rw_pid;
    ReportBackgroundWorkerPID(rw);
                                             
    dlist_push_head(&BackendList, &rw->rw_backend->elem);
#ifdef EXEC_BACKEND
    ShmemBackendArrayAdd(rw->rw_backend);
#endif
    return true;
  }

  return false;
}

   
                                                                        
                         
   
static bool
bgworker_should_start_now(BgWorkerStartTime start_time)
{
  switch (pmState)
  {
  case PM_NO_CHILDREN:
  case PM_WAIT_DEAD_END:
  case PM_SHUTDOWN_2:
  case PM_SHUTDOWN:
  case PM_WAIT_BACKENDS:
  case PM_STOP_BACKENDS:
    break;

  case PM_RUN:
    if (start_time == BgWorkerStart_RecoveryFinished)
    {
      return true;
    }
                      

  case PM_HOT_STANDBY:
    if (start_time == BgWorkerStart_ConsistentState)
    {
      return true;
    }
                      

  case PM_RECOVERY:
  case PM_STARTUP:
  case PM_INIT:
    if (start_time == BgWorkerStart_PostmasterStart)
    {
      return true;
    }
                      
  }

  return false;
}

   
                                                                            
                                            
   
                                                               
   
                                                            
   
static bool
assign_backendlist_entry(RegisteredBgWorker *rw)
{
  Backend *bn;

     
                                                                         
                                                                           
                                                                  
     
  if (canAcceptConnections(BACKEND_TYPE_BGWORKER) != CAC_OK)
  {
    ereport(LOG, (errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED), errmsg("no slot available for new worker process")));
    return false;
  }

     
                                                                      
                                                                             
                                                                          
                              
     
  if (!RandomCancelKey(&MyCancelKey))
  {
    ereport(LOG, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("could not generate random cancel key")));
    return false;
  }

  bn = malloc(sizeof(Backend));
  if (bn == NULL)
  {
    ereport(LOG, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    return false;
  }

  bn->cancel_key = MyCancelKey;
  bn->child_slot = MyPMChildSlot = AssignPostmasterChildSlot();
  bn->bkend_type = BACKEND_TYPE_BGWORKER;
  bn->dead_end = false;
  bn->bgworker_notify = false;

  rw->rw_backend = bn;
  rw->rw_child_slot = bn->child_slot;

  return true;
}

   
                                                     
   
                                                                     
                                                             
   
                                                                           
                                                                            
                                                                            
                                                                 
   
static void
maybe_start_bgworkers(void)
{
#define MAX_BGWORKERS_TO_LAUNCH 100
  int num_launched = 0;
  TimestampTz now = 0;
  slist_mutable_iter iter;

     
                                                                         
                                 
     
  if (FatalError)
  {
    StartWorkerNeeded = false;
    HaveCrashedWorker = false;
    return;
  }

                                                                          
  StartWorkerNeeded = false;
  HaveCrashedWorker = false;

  slist_foreach_modify(iter, &BackgroundWorkerList)
  {
    RegisteredBgWorker *rw;

    rw = slist_container(RegisteredBgWorker, rw_lnode, iter.cur);

                                   
    if (rw->rw_pid != 0)
    {
      continue;
    }

                                                            
    if (rw->rw_terminate)
    {
      ForgetBackgroundWorker(&iter);
      continue;
    }

       
                                                                   
                                                                         
                                                                          
                                                                          
                                                 
       
    if (rw->rw_crashed_at != 0)
    {
      if (rw->rw_worker.bgw_restart_time == BGW_NEVER_RESTART)
      {
        int notify_pid;

        notify_pid = rw->rw_worker.bgw_notify_pid;

        ForgetBackgroundWorker(&iter);

                                        
        if (notify_pid != 0)
        {
          kill(notify_pid, SIGUSR1);
        }

        continue;
      }

                                             
      if (now == 0)
      {
        now = GetCurrentTimestamp();
      }

      if (!TimestampDifferenceExceeds(rw->rw_crashed_at, now, rw->rw_worker.bgw_restart_time * 1000))
      {
                                                                      
        HaveCrashedWorker = true;
        continue;
      }
    }

    if (bgworker_should_start_now(rw->rw_worker.bgw_start_time))
    {
                                                          
      rw->rw_crashed_at = 0;

         
                                  
         
                                                                 
                                                                         
                                                                      
                                                                        
                                                                    
                                                                   
                                
         
      if (!do_start_bgworker(rw))
      {
        StartWorkerNeeded = true;
        return;
      }

         
                                                                      
                                                                      
                                                                       
                      
         
      if (++num_launched >= MAX_BGWORKERS_TO_LAUNCH)
      {
        StartWorkerNeeded = true;
        return;
      }
    }
  }
}

   
                                                                     
                                                                           
                                    
   
bool
PostmasterMarkPIDForWorkerNotify(int pid)
{
  dlist_iter iter;
  Backend *bp;

  dlist_foreach(iter, &BackendList)
  {
    bp = dlist_container(Backend, elem, iter.cur);
    if (bp->pid == pid)
    {
      bp->bgworker_notify = true;
      return true;
    }
  }
  return false;
}

#ifdef EXEC_BACKEND

   
                                                                            
                                                                      
   
extern slock_t *ShmemLock;
extern slock_t *ProcStructLock;
extern PGPROC *AuxiliaryProcs;
extern PMSignalData *PMSignalState;
extern pgsocket pgStatSock;
extern pg_time_t first_syslogger_file_time;

#ifndef WIN32
#define write_inheritable_socket(dest, src, childpid) ((*(dest) = (src)), true)
#define read_inheritable_socket(dest, src) (*(dest) = *(src))
#else
static bool
write_duplicated_handle(HANDLE *dest, HANDLE src, HANDLE child);
static bool
write_inheritable_socket(InheritableSocket *dest, SOCKET src, pid_t childPid);
static void
read_inheritable_socket(SOCKET *dest, InheritableSocket *src);
#endif

                                                                       
#ifndef WIN32
static bool
save_backend_variables(BackendParameters *param, Port *port)
#else
static bool
save_backend_variables(BackendParameters *param, Port *port, HANDLE childProcess, pid_t childPid)
#endif
{
  memcpy(&param->port, port, sizeof(Port));
  if (!write_inheritable_socket(&param->portsocket, port->sock, childPid))
  {
    return false;
  }

  strlcpy(param->DataDir, DataDir, MAXPGPATH);

  memcpy(&param->ListenSocket, &ListenSocket, sizeof(ListenSocket));

  param->MyCancelKey = MyCancelKey;
  param->MyPMChildSlot = MyPMChildSlot;

#ifdef WIN32
  param->ShmemProtectiveRegion = ShmemProtectiveRegion;
#endif
  param->UsedShmemSegID = UsedShmemSegID;
  param->UsedShmemSegAddr = UsedShmemSegAddr;

  param->ShmemLock = ShmemLock;
  param->ShmemVariableCache = ShmemVariableCache;
  param->ShmemBackendArray = ShmemBackendArray;

#ifndef HAVE_SPINLOCKS
  param->SpinlockSemaArray = SpinlockSemaArray;
#endif
  param->NamedLWLockTrancheRequests = NamedLWLockTrancheRequests;
  param->NamedLWLockTrancheArray = NamedLWLockTrancheArray;
  param->MainLWLockArray = MainLWLockArray;
  param->ProcStructLock = ProcStructLock;
  param->ProcGlobal = ProcGlobal;
  param->AuxiliaryProcs = AuxiliaryProcs;
  param->PreparedXactProcs = PreparedXactProcs;
  param->PMSignalState = PMSignalState;
  if (!write_inheritable_socket(&param->pgStatSock, pgStatSock, childPid))
  {
    return false;
  }

  param->PostmasterPid = PostmasterPid;
  param->PgStartTime = PgStartTime;
  param->PgReloadTime = PgReloadTime;
  param->first_syslogger_file_time = first_syslogger_file_time;

  param->redirection_done = redirection_done;
  param->IsBinaryUpgrade = IsBinaryUpgrade;
  param->max_safe_fds = max_safe_fds;

  param->MaxBackends = MaxBackends;

#ifdef WIN32
  param->PostmasterHandle = PostmasterHandle;
  if (!write_duplicated_handle(&param->initial_signal_pipe, pgwin32_create_signal_listener(childPid), childProcess))
  {
    return false;
  }
#else
  memcpy(&param->postmaster_alive_fds, &postmaster_alive_fds, sizeof(postmaster_alive_fds));
#endif

  memcpy(&param->syslogPipe, &syslogPipe, sizeof(syslogPipe));

  strlcpy(param->my_exec_path, my_exec_path, MAXPGPATH);

  strlcpy(param->pkglib_path, pkglib_path, MAXPGPATH);

  strlcpy(param->ExtraOptions, ExtraOptions, MAXPGPATH);

  return true;
}

#ifdef WIN32
   
                                                                        
                                                         
   
static bool
write_duplicated_handle(HANDLE *dest, HANDLE src, HANDLE childProcess)
{
  HANDLE hChild = INVALID_HANDLE_VALUE;

  if (!DuplicateHandle(GetCurrentProcess(), src, childProcess, &hChild, 0, TRUE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
  {
    ereport(LOG, (errmsg_internal("could not duplicate handle to be written to backend parameter file: error code %lu", GetLastError())));
    return false;
  }

  *dest = hChild;
  return true;
}

   
                                                                            
                                    
                                                                              
                                                                         
                                
   
static bool
write_inheritable_socket(InheritableSocket *dest, SOCKET src, pid_t childpid)
{
  dest->origsocket = src;
  if (src != 0 && src != PGINVALID_SOCKET)
  {
                       
    if (WSADuplicateSocket(src, childpid, &dest->wsainfo) != 0)
    {
      ereport(LOG, (errmsg("could not duplicate socket %d for use in backend: error code %d", (int)src, WSAGetLastError())));
      return false;
    }
  }
  return true;
}

   
                                                                          
   
static void
read_inheritable_socket(SOCKET *dest, InheritableSocket *src)
{
  SOCKET s;

  if (src->origsocket == PGINVALID_SOCKET || src->origsocket == 0)
  {
                            
    *dest = src->origsocket;
  }
  else
  {
                                                 
    s = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &src->wsainfo, 0, 0);
    if (s == INVALID_SOCKET)
    {
      write_stderr("could not create inherited socket: error code %d\n", WSAGetLastError());
      exit(1);
    }
    *dest = s;

       
                                                                          
                                                                      
               
       
    closesocket(src->origsocket);
  }
}
#endif

static void
read_backend_variables(char *id, Port *port)
{
  BackendParameters param;

#ifndef WIN32
                                                
  FILE *fp;

                 
  fp = AllocateFile(id, PG_BINARY_R);
  if (!fp)
  {
    write_stderr("could not open backend variables file \"%s\": %s\n", id, strerror(errno));
    exit(1);
  }

  if (fread(&param, sizeof(param), 1, fp) != 1)
  {
    write_stderr("could not read from backend variables file \"%s\": %s\n", id, strerror(errno));
    exit(1);
  }

                    
  FreeFile(fp);
  if (unlink(id) != 0)
  {
    write_stderr("could not remove file \"%s\": %s\n", id, strerror(errno));
    exit(1);
  }
#else
                                      
  HANDLE paramHandle;
  BackendParameters *paramp;

#ifdef _WIN64
  paramHandle = (HANDLE)_atoi64(id);
#else
  paramHandle = (HANDLE)atol(id);
#endif
  paramp = MapViewOfFile(paramHandle, FILE_MAP_READ, 0, 0, 0);
  if (!paramp)
  {
    write_stderr("could not map view of backend variables: error code %lu\n", GetLastError());
    exit(1);
  }

  memcpy(&param, paramp, sizeof(BackendParameters));

  if (!UnmapViewOfFile(paramp))
  {
    write_stderr("could not unmap view of backend variables: error code %lu\n", GetLastError());
    exit(1);
  }

  if (!CloseHandle(paramHandle))
  {
    write_stderr("could not close handle to backend parameter variables: error code %lu\n", GetLastError());
    exit(1);
  }
#endif

  restore_backend_variables(&param, port);
}

                                                                          
static void
restore_backend_variables(BackendParameters *param, Port *port)
{
  memcpy(port, &param->port, sizeof(Port));
  read_inheritable_socket(&port->sock, &param->portsocket);

  SetDataDir(param->DataDir);

  memcpy(&ListenSocket, &param->ListenSocket, sizeof(ListenSocket));

  MyCancelKey = param->MyCancelKey;
  MyPMChildSlot = param->MyPMChildSlot;

#ifdef WIN32
  ShmemProtectiveRegion = param->ShmemProtectiveRegion;
#endif
  UsedShmemSegID = param->UsedShmemSegID;
  UsedShmemSegAddr = param->UsedShmemSegAddr;

  ShmemLock = param->ShmemLock;
  ShmemVariableCache = param->ShmemVariableCache;
  ShmemBackendArray = param->ShmemBackendArray;

#ifndef HAVE_SPINLOCKS
  SpinlockSemaArray = param->SpinlockSemaArray;
#endif
  NamedLWLockTrancheRequests = param->NamedLWLockTrancheRequests;
  NamedLWLockTrancheArray = param->NamedLWLockTrancheArray;
  MainLWLockArray = param->MainLWLockArray;
  ProcStructLock = param->ProcStructLock;
  ProcGlobal = param->ProcGlobal;
  AuxiliaryProcs = param->AuxiliaryProcs;
  PreparedXactProcs = param->PreparedXactProcs;
  PMSignalState = param->PMSignalState;
  read_inheritable_socket(&pgStatSock, &param->pgStatSock);

  PostmasterPid = param->PostmasterPid;
  PgStartTime = param->PgStartTime;
  PgReloadTime = param->PgReloadTime;
  first_syslogger_file_time = param->first_syslogger_file_time;

  redirection_done = param->redirection_done;
  IsBinaryUpgrade = param->IsBinaryUpgrade;
  max_safe_fds = param->max_safe_fds;

  MaxBackends = param->MaxBackends;

#ifdef WIN32
  PostmasterHandle = param->PostmasterHandle;
  pgwin32_initial_signal_pipe = param->initial_signal_pipe;
#else
  memcpy(&postmaster_alive_fds, &param->postmaster_alive_fds, sizeof(postmaster_alive_fds));
#endif

  memcpy(&syslogPipe, &param->syslogPipe, sizeof(syslogPipe));

  strlcpy(my_exec_path, param->my_exec_path, MAXPGPATH);

  strlcpy(pkglib_path, param->pkglib_path, MAXPGPATH);

  strlcpy(ExtraOptions, param->ExtraOptions, MAXPGPATH);
}

Size
ShmemBackendArraySize(void)
{
  return mul_size(MaxLivePostmasterChildren(), sizeof(Backend));
}

void
ShmemBackendArrayAllocation(void)
{
  Size size = ShmemBackendArraySize();

  ShmemBackendArray = (Backend *)ShmemAlloc(size);
                               
  memset(ShmemBackendArray, 0, size);
}

static void
ShmemBackendArrayAdd(Backend *bn)
{
                                                                     
  int i = bn->child_slot - 1;

  Assert(ShmemBackendArray[i].pid == 0);
  ShmemBackendArray[i] = *bn;
}

static void
ShmemBackendArrayRemove(Backend *bn)
{
  int i = bn->child_slot - 1;

  Assert(ShmemBackendArray[i].pid == bn->pid);
                              
  ShmemBackendArray[i].pid = 0;
}
#endif                   

#ifdef WIN32

   
                                                                        
                                                                             
   
static pid_t
waitpid(pid_t pid, int *exitstatus, int options)
{
  DWORD dwd;
  ULONG_PTR key;
  OVERLAPPED *ovl;

     
                                                                           
                              
     
  if (GetQueuedCompletionStatus(win32ChildQueue, &dwd, &key, &ovl, 0))
  {
    *exitstatus = (int)key;
    return dwd;
  }

  return -1;
}

   
                                                                   
                                                                    
   
static void WINAPI
pgwin32_deadchild_callback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
  win32_deadchild_waitinfo *childinfo = (win32_deadchild_waitinfo *)lpParameter;
  DWORD exitcode;

  if (TimerOrWaitFired)
  {
    return;                                               
                                            
  }

     
                                                                          
          
     
  UnregisterWaitEx(childinfo->waitHandle, NULL);

  if (!GetExitCodeProcess(childinfo->procHandle, &exitcode))
  {
       
                                                                  
       
    write_stderr("could not read exit code for process\n");
    exitcode = 255;
  }

  if (!PostQueuedCompletionStatus(win32ChildQueue, childinfo->procId, (ULONG_PTR)exitcode, NULL))
  {
    write_stderr("could not post child completion status\n");
  }

     
                                                                  
                        
     
  CloseHandle(childinfo->procHandle);

     
                                                       
                                   
     
  free(childinfo);

                            
  pg_queue_signal(SIGCHLD);
}
#endif            

   
                                                                   
   
                                                                           
                                    
   
static void
InitPostmasterDeathWatchHandle(void)
{
#ifndef WIN32

     
                                                                    
                                                                            
                                                                        
                                                                             
                                                                        
                                                                           
                                                                
     
  Assert(MyProcPid == PostmasterPid);
  if (pipe(postmaster_alive_fds) < 0)
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg_internal("could not create pipe to monitor postmaster death: %m")));
  }

     
                                                                         
           
     
  if (fcntl(postmaster_alive_fds[POSTMASTER_FD_WATCH], F_SETFL, O_NONBLOCK) == -1)
  {
    ereport(FATAL, (errcode_for_socket_access(), errmsg_internal("could not set postmaster death monitoring pipe to nonblocking mode: %m")));
  }
#else

     
                                                               
     
  if (DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &PostmasterHandle, 0, TRUE, DUPLICATE_SAME_ACCESS) == 0)
  {
    ereport(FATAL, (errmsg_internal("could not duplicate postmaster handle: error code %lu", GetLastError())));
  }
#endif            
}
