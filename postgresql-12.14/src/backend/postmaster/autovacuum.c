                                                                            
   
                
   
                                           
   
                                                                                
                                                                      
                                                                             
                                                                              
                                                                             
                                                                            
                                                                            
   
                                                                        
                                                                           
                                                                        
                                                                             
                                                                                
   
                                                                        
                                                                          
                                                                              
                                                                                
                                                                         
                                                                                
                        
   
                                                                            
                                                                        
                                                                         
                                                                             
                                                                        
                                                                              
                                                                                
                                                       
   
                                                                            
                                                                                
                                                                          
                                                                             
                                    
   
                                                                           
                                                                               
                                                                               
                                                                             
                                                                              
                                                                          
                                                                             
                                                                         
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/reloptions.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/pg_database.h"
#include "commands/dbcommands.h"
#include "commands/vacuum.h"
#include "lib/ilist.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lmgr.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"

   
                  
   
bool autovacuum_start_daemon = false;
int autovacuum_max_workers;
int autovacuum_work_mem = -1;
int autovacuum_naptime;
int autovacuum_vac_thresh;
double autovacuum_vac_scale;
int autovacuum_anl_thresh;
double autovacuum_anl_scale;
int autovacuum_freeze_max_age;
int autovacuum_multixact_freeze_max_age;

double autovacuum_vac_cost_delay;
int autovacuum_vac_cost_limit;

int Log_autovacuum_min_duration = -1;

                                                                   
#define STATS_READ_DELAY 1000

                                                                     
#define MIN_AUTOVAC_SLEEPTIME 100.0                   
#define MAX_AUTOVAC_SLEEPTIME 300                

                                                      
static bool am_autovacuum_launcher = false;
static bool am_autovacuum_worker = false;

                                  
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGUSR2 = false;
static volatile sig_atomic_t got_SIGTERM = false;

                                                                          
static TransactionId recentXid;
static MultiXactId recentMulti;

                                                                    
static int default_freeze_min_age;
static int default_freeze_table_age;
static int default_multixact_freeze_min_age;
static int default_multixact_freeze_table_age;

                                        
static MemoryContext AutovacMemCxt;

                                                   
typedef struct avl_dbase
{
  Oid adl_datid;                                
  TimestampTz adl_next_worker;
  int adl_score;
  dlist_node adl_node;
} avl_dbase;

                                                 
typedef struct avw_dbase
{
  Oid adw_datid;
  char *adw_name;
  TransactionId adw_frozenxid;
  MultiXactId adw_minmulti;
  PgStat_StatDBEntry *adw_entry;
} avw_dbase;

                                                                          
typedef struct av_relation
{
  Oid ar_toastrelid;                               
  Oid ar_relid;
  bool ar_hasrelopts;
  AutoVacOpts ar_reloptions;                                              
                                                              
} av_relation;

                                                                               
typedef struct autovac_table
{
  Oid at_relid;
  VacuumParams at_params;
  double at_vacuum_cost_delay;
  int at_vacuum_cost_limit;
  bool at_dobalance;
  bool at_sharedrel;
  char *at_relname;
  char *at_nspname;
  char *at_datname;
} autovac_table;

                
                                                                               
                                                          
                           
   
                                                  
                                                                    
                                                                 
                                                                    
                                                                         
                                                        
                                                                       
   
                                                                          
                                                                          
                                                                     
                
   
typedef struct WorkerInfoData
{
  dlist_node wi_links;
  Oid wi_dboid;
  Oid wi_tableoid;
  PGPROC *wi_proc;
  TimestampTz wi_launchtime;
  bool wi_dobalance;
  bool wi_sharedrel;
  double wi_cost_delay;
  int wi_cost_limit;
  int wi_cost_limit_base;
} WorkerInfoData;

typedef struct WorkerInfoData *WorkerInfo;

   
                                                                               
                                                                           
                    
   
typedef enum
{
  AutoVacForkFailed,                                      
  AutoVacRebalance,                                 
  AutoVacNumSignals                    
} AutoVacuumSignal;

   
                                                                             
                                                                         
                                                                        
            
   
typedef struct AutoVacuumWorkItem
{
  AutoVacuumWorkItemType avw_type;
  bool avw_used;                            
  bool avw_active;                      
  Oid avw_database;
  Oid avw_relation;
  BlockNumber avw_blockNumber;
} AutoVacuumWorkItem;

#define NUM_WORKITEMS 256

                
                                                                          
                                                                   
   
                                                                    
                                                     
                                          
                                                   
                                                                               
                                                         
                                 
   
                                                                              
                                   
                
   
typedef struct
{
  sig_atomic_t av_signal[AutoVacNumSignals];
  pid_t av_launcherpid;
  dlist_head av_freeWorkers;
  dlist_head av_runningWorkers;
  WorkerInfo av_startingWorker;
  AutoVacuumWorkItem av_workItems[NUM_WORKITEMS];
} AutoVacuumShmemStruct;

static AutoVacuumShmemStruct *AutoVacuumShmem;

   
                                                                              
                    
   
static dlist_head DatabaseList = DLIST_STATIC_INIT(DatabaseList);
static MemoryContext DatabaseListCxt = NULL;

                                                        
static WorkerInfo MyWorkerInfo = NULL;

                                                               
int AutovacuumLauncherPid = 0;

#ifdef EXEC_BACKEND
static pid_t
avlauncher_forkexec(void);
static pid_t
avworker_forkexec(void);
#endif
NON_EXEC_STATIC void
AutoVacWorkerMain(int argc, char *argv[]) pg_attribute_noreturn();
NON_EXEC_STATIC void
AutoVacLauncherMain(int argc, char *argv[]) pg_attribute_noreturn();

static Oid
do_start_worker(void);
static void
launcher_determine_sleep(bool canlaunch, bool recursing, struct timeval *nap);
static void
launch_worker(TimestampTz now);
static List *
get_database_list(void);
static void
rebuild_database_list(Oid newdb);
static int
db_comparator(const void *a, const void *b);
static void
autovac_balance_cost(void);

static void
do_autovacuum(void);
static void
FreeWorkerInfo(int code, Datum arg);

static autovac_table *
table_recheck_autovac(Oid relid, HTAB *table_toast_map, TupleDesc pg_class_desc, int effective_multixact_freeze_max_age);
static void
relation_needs_vacanalyze(Oid relid, AutoVacOpts *relopts, Form_pg_class classForm, PgStat_StatTabEntry *tabentry, int effective_multixact_freeze_max_age, bool *dovacuum, bool *doanalyze, bool *wraparound);

static void
autovacuum_do_vac_analyze(autovac_table *tab, BufferAccessStrategy bstrategy);
static AutoVacOpts *
extract_autovac_opts(HeapTuple tup, TupleDesc pg_class_desc);
static PgStat_StatTabEntry *
get_pgstat_tabentry_relid(Oid relid, bool isshared, PgStat_StatDBEntry *shared, PgStat_StatDBEntry *dbentry);
static void
perform_work_item(AutoVacuumWorkItem *workitem);
static void
autovac_report_activity(autovac_table *tab);
static void
autovac_report_workitem(AutoVacuumWorkItem *workitem, const char *nspname, const char *relname);
static void av_sighup_handler(SIGNAL_ARGS);
static void avl_sigusr2_handler(SIGNAL_ARGS);
static void avl_sigterm_handler(SIGNAL_ARGS);
static void
autovac_refresh_stats(void);

                                                                      
                                  
                                                                      

#ifdef EXEC_BACKEND
   
                                                         
   
                                              
   
static pid_t
avlauncher_forkexec(void)
{
  char *av[10];
  int ac = 0;

  av[ac++] = "postgres";
  av[ac++] = "--forkavlauncher";
  av[ac++] = NULL;                                       
  av[ac] = NULL;

  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}

   
                                                                   
   
void
AutovacuumLauncherIAm(void)
{
  am_autovacuum_launcher = true;
}
#endif

   
                                                                           
               
   
int
StartAutoVacLauncher(void)
{
  pid_t AutoVacPID;

#ifdef EXEC_BACKEND
  switch ((AutoVacPID = avlauncher_forkexec()))
#else
  switch ((AutoVacPID = fork_process()))
#endif
  {
  case -1:
    ereport(LOG, (errmsg("could not fork autovacuum launcher process: %m")));
    return 0;

#ifndef EXEC_BACKEND
  case 0:
                                 
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

    AutoVacLauncherMain(0, NULL);
    break;
#endif
  default:
    return (int)AutoVacPID;
  }

                          
  return 0;
}

   
                                                  
   
NON_EXEC_STATIC void
AutoVacLauncherMain(int argc, char *argv[])
{
  sigjmp_buf local_sigjmp_buf;

  am_autovacuum_launcher = true;

                              
  init_ps_display(pgstat_get_backend_desc(B_AUTOVAC_LAUNCHER), "", "", "");

  ereport(DEBUG1, (errmsg("autovacuum launcher started")));

  if (PostAuthDelay)
  {
    pg_usleep(PostAuthDelay * 1000000L);
  }

  SetProcessingMode(InitProcessing);

     
                                                                          
                                                                          
                      
     
  pqsignal(SIGHUP, av_sighup_handler);
  pqsignal(SIGINT, StatementCancelHandler);
  pqsignal(SIGTERM, avl_sigterm_handler);

  pqsignal(SIGQUIT, quickdie);
  InitializeTimeouts();                                  

  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, procsignal_sigusr1_handler);
  pqsignal(SIGUSR2, avl_sigusr2_handler);
  pqsignal(SIGFPE, FloatExceptionHandler);
  pqsignal(SIGCHLD, SIG_DFL);

                            
  BaseInit();

     
                                                                        
                                                                            
                                                                             
                                         
     
#ifndef EXEC_BACKEND
  InitProcess();
#endif

  InitPostgres(NULL, InvalidOid, NULL, InvalidOid, NULL, false);

  SetProcessingMode(NormalProcessing);

     
                                                                             
                                                                           
                            
     
  AutovacMemCxt = AllocSetContextCreate(TopMemoryContext, "Autovacuum Launcher", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(AutovacMemCxt);

     
                                                              
     
                                                                          
     
  if (sigsetjmp(local_sigjmp_buf, 1) != 0)
  {
                                                                
    error_context_stack = NULL;

                                               
    HOLD_INTERRUPTS();

                                                           
    disable_all_timeouts(false);
    QueryCancelPending = false;                                     

                                            
    EmitErrorReport();

                                                           
    AbortCurrentTransaction();

       
                                                                        
                    
       
    LWLockReleaseAll();
    pgstat_report_wait_end();
    AbortBufferIO();
    UnlockBuffers();
                                                        
    if (AuxProcessResourceOwner)
    {
      ReleaseAuxProcessResources(false);
    }
    AtEOXact_Buffers(false);
    AtEOXact_SMgr();
    AtEOXact_Files(false);
    AtEOXact_HashTables(false);

       
                                                                         
                  
       
    MemoryContextSwitchTo(AutovacMemCxt);
    FlushErrorState();

                                                        
    MemoryContextResetAndDeleteChildren(AutovacMemCxt);

                                                       
    DatabaseListCxt = NULL;
    dlist_init(&DatabaseList);

       
                                                                        
                                               
       
    pgstat_clear_snapshot();

                                           
    RESUME_INTERRUPTS();

                                                                         
    if (got_SIGTERM)
    {
      goto shutdown;
    }

       
                                                                     
                                                 
       
    pg_usleep(1000000L);
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

                                                                 
  PG_SETMASK(&UnBlockSig);

     
                                                                             
                            
     
  SetConfigOption("search_path", "", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                            
                                                                             
                                
     
  SetConfigOption("zero_damaged_pages", "false", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                         
                                              
     
  SetConfigOption("statement_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);
  SetConfigOption("lock_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);
  SetConfigOption("idle_in_transaction_session_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                           
                                                                           
                                               
     
  SetConfigOption("default_transaction_isolation", "read committed", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                            
                  
     
  if (!AutoVacuumingActive())
  {
    if (!got_SIGTERM)
    {
      do_start_worker();
    }
    proc_exit(0);           
  }

  AutoVacuumShmem->av_launcherpid = MyProcPid;

     
                                                                           
                                                                             
                                                                          
                                                                            
                                        
     
  rebuild_database_list(InvalidOid);

                                   
  while (!got_SIGTERM)
  {
    struct timeval nap;
    TimestampTz current_time = 0;
    bool can_launch;

       
                                                                      
                                                                     
                                                                    
                            
       

    launcher_determine_sleep(!dlist_is_empty(&AutoVacuumShmem->av_freeWorkers), false, &nap);

       
                                                                         
                                                          
       
    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, (nap.tv_sec * 1000L) + (nap.tv_usec / 1000L), WAIT_EVENT_AUTOVACUUM_MAIN);

    ResetLatch(MyLatch);

                                                                        
    ProcessCatchupInterrupt();

                                  
    if (got_SIGTERM)
    {
      break;
    }

    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);

                                              
      if (!AutoVacuumingActive())
      {
        break;
      }

                                                                 
      LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);
      autovac_balance_cost();
      LWLockRelease(AutovacuumLock);

                                                        
      rebuild_database_list(InvalidOid);
    }

       
                                                                     
              
       
    if (got_SIGUSR2)
    {
      got_SIGUSR2 = false;

                                            
      if (AutoVacuumShmem->av_signal[AutoVacRebalance])
      {
        LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);
        AutoVacuumShmem->av_signal[AutoVacRebalance] = false;
        autovac_balance_cost();
        LWLockRelease(AutovacuumLock);
      }

      if (AutoVacuumShmem->av_signal[AutoVacForkFailed])
      {
           
                                                                    
                                                                       
                                                                   
                                           
           
                                                                      
                                                                     
                                                              
           
        AutoVacuumShmem->av_signal[AutoVacForkFailed] = false;
        pg_usleep(1000000L);         
        SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_WORKER);
        continue;
      }
    }

       
                                                                        
                                                                           
                                                                          
                                 
       

    current_time = GetCurrentTimestamp();
    LWLockAcquire(AutovacuumLock, LW_SHARED);

    can_launch = !dlist_is_empty(&AutoVacuumShmem->av_freeWorkers);

    if (AutoVacuumShmem->av_startingWorker != NULL)
    {
      int waittime;
      WorkerInfo worker = AutoVacuumShmem->av_startingWorker;

         
                                                                  
                                                                         
                                                                        
                                                                       
                                                                       
                                                                    
                                                                   
                                                                     
                                                                        
                                                                     
                                                                       
                                                           
                                 
         
      waittime = Min(autovacuum_naptime, 60) * 1000;
      if (TimestampDifferenceExceeds(worker->wi_launchtime, current_time, waittime))
      {
        LWLockRelease(AutovacuumLock);
        LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

           
                                                                     
                                                                      
                                                                 
                                     
           
        if (AutoVacuumShmem->av_startingWorker != NULL)
        {
          worker = AutoVacuumShmem->av_startingWorker;
          worker->wi_dboid = InvalidOid;
          worker->wi_tableoid = InvalidOid;
          worker->wi_sharedrel = false;
          worker->wi_proc = NULL;
          worker->wi_launchtime = 0;
          dlist_push_head(&AutoVacuumShmem->av_freeWorkers, &worker->wi_links);
          AutoVacuumShmem->av_startingWorker = NULL;
          elog(WARNING, "worker took too long to start; canceled");
        }
      }
      else
      {
        can_launch = false;
      }
    }
    LWLockRelease(AutovacuumLock);                                 

                                                        
    if (!can_launch)
    {
      continue;
    }

                                        

    if (dlist_is_empty(&DatabaseList))
    {
         
                                                                         
                                                                      
                                                                 
                                                                    
                                                                         
                 
         
      launch_worker(current_time);
    }
    else
    {
         
                                                                   
                                                                        
                           
         
      avl_dbase *avdb;

      avdb = dlist_tail_element(avl_dbase, adl_node, &DatabaseList);

         
                                                                     
              
         
      if (TimestampDifferenceExceeds(avdb->adl_next_worker, current_time, 0))
      {
        launch_worker(current_time);
      }
    }
  }

                                                     
shutdown:
  ereport(DEBUG1, (errmsg("autovacuum launcher shutting down")));
  AutoVacuumShmem->av_launcherpid = 0;

  proc_exit(0);           
}

   
                                                            
   
                                                                                
                                                                             
                                                                      
   
static void
launcher_determine_sleep(bool canlaunch, bool recursing, struct timeval *nap)
{
     
                                                                       
                                                                           
                                                                             
                                                           
     
  if (!canlaunch)
  {
    nap->tv_sec = autovacuum_naptime;
    nap->tv_usec = 0;
  }
  else if (!dlist_is_empty(&DatabaseList))
  {
    TimestampTz current_time = GetCurrentTimestamp();
    TimestampTz next_wakeup;
    avl_dbase *avdb;
    long secs;
    int usecs;

    avdb = dlist_tail_element(avl_dbase, adl_node, &DatabaseList);

    next_wakeup = avdb->adl_next_worker;
    TimestampDifference(current_time, next_wakeup, &secs, &usecs);

    nap->tv_sec = secs;
    nap->tv_usec = usecs;
  }
  else
  {
                                                                    
    nap->tv_sec = autovacuum_naptime;
    nap->tv_usec = 0;
  }

     
                                                                          
                                                                          
                                                                            
                                                                             
                                               
     
                                                                             
                                                                     
     
  if (nap->tv_sec == 0 && nap->tv_usec == 0 && !recursing)
  {
    rebuild_database_list(InvalidOid);
    launcher_determine_sleep(canlaunch, true, nap);
    return;
  }

                                                            
  if (nap->tv_sec <= 0 && nap->tv_usec <= MIN_AUTOVAC_SLEEPTIME * 1000)
  {
    nap->tv_sec = 0;
    nap->tv_usec = MIN_AUTOVAC_SLEEPTIME * 1000;
  }

     
                                                                            
                                                                          
                                                                             
                
     
  if (nap->tv_sec > MAX_AUTOVAC_SLEEPTIME)
  {
    nap->tv_sec = MAX_AUTOVAC_SLEEPTIME;
  }
}

   
                                                                              
                                                                         
                                                                      
   
                                                                              
                                                                             
                                                                      
                                                                       
                                                                               
                                                                              
                      
   
static void
rebuild_database_list(Oid newdb)
{
  List *dblist;
  ListCell *cell;
  MemoryContext newcxt;
  MemoryContext oldcxt;
  MemoryContext tmpcxt;
  HASHCTL hctl;
  int score;
  int nelems;
  HTAB *dbhash;
  dlist_iter iter;

                       
  autovac_refresh_stats();

  newcxt = AllocSetContextCreate(AutovacMemCxt, "AV dblist", ALLOCSET_DEFAULT_SIZES);
  tmpcxt = AllocSetContextCreate(newcxt, "tmp AV dblist", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(tmpcxt);

     
                                                                             
                                                                           
                                                                        
                                                        
     
                                                                            
                                                                   
                                                                             
                                                           
                                                           
     
                                                                             
                                                                          
           
     
  hctl.keysize = sizeof(Oid);
  hctl.entrysize = sizeof(avl_dbase);
  hctl.hcxt = tmpcxt;
  dbhash = hash_create("db hash", 20, &hctl,                              
      HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

                                           
  score = 0;
  if (OidIsValid(newdb))
  {
    avl_dbase *db;
    PgStat_StatDBEntry *entry;

                                                              
    entry = pgstat_fetch_stat_dbentry(newdb);
    if (entry != NULL)
    {
                                                                      
      db = hash_search(dbhash, &newdb, HASH_ENTER, NULL);

                                                 
      db->adl_score = score++;
                                          
    }
  }

                                                       
  dlist_foreach(iter, &DatabaseList)
  {
    avl_dbase *avdb = dlist_container(avl_dbase, adl_node, iter.cur);
    avl_dbase *db;
    bool found;
    PgStat_StatDBEntry *entry;

       
                                                                           
                            
       
    entry = pgstat_fetch_stat_dbentry(avdb->adl_datid);
    if (entry == NULL)
    {
      continue;
    }

    db = hash_search(dbhash, &(avdb->adl_datid), HASH_ENTER, &found);

    if (!found)
    {
                                                 
      db->adl_score = score++;
                                          
    }
  }

                                                                        
  dblist = get_database_list();
  foreach (cell, dblist)
  {
    avw_dbase *avdb = lfirst(cell);
    avl_dbase *db;
    bool found;
    PgStat_StatDBEntry *entry;

                                                     
    entry = pgstat_fetch_stat_dbentry(avdb->adw_datid);
    if (entry == NULL)
    {
      continue;
    }

    db = hash_search(dbhash, &(avdb->adw_datid), HASH_ENTER, &found);
                                                                           
    if (!found)
    {
                                                 
      db->adl_score = score++;
                                          
    }
  }
  nelems = score;

                                                                  
  MemoryContextSwitchTo(newcxt);
  dlist_init(&DatabaseList);

  if (nelems > 0)
  {
    TimestampTz current_time;
    int millis_increment;
    avl_dbase *dbary;
    avl_dbase *db;
    HASH_SEQ_STATUS seq;
    int i;

                                                 
    dbary = palloc(nelems * sizeof(avl_dbase));

    i = 0;
    hash_seq_init(&seq, dbhash);
    while ((db = hash_seq_search(&seq)) != NULL)
    {
      memcpy(&(dbary[i++]), db, sizeof(avl_dbase));
    }

                        
    qsort(dbary, nelems, sizeof(avl_dbase), db_comparator);

       
                                                                         
                                                                       
                                                                        
                                                                           
                          
       
    millis_increment = 1000.0 * autovacuum_naptime / nelems;
    if (millis_increment <= MIN_AUTOVAC_SLEEPTIME)
    {
      millis_increment = MIN_AUTOVAC_SLEEPTIME * 1.1;
    }

    current_time = GetCurrentTimestamp();

       
                                                                     
                                           
       
    for (i = 0; i < nelems; i++)
    {
      avl_dbase *db = &(dbary[i]);

      current_time = TimestampTzPlusMilliseconds(current_time, millis_increment);
      db->adl_next_worker = current_time;

                                                                   
      dlist_push_head(&DatabaseList, &db->adl_node);
    }
  }

                                 
  if (DatabaseListCxt != NULL)
  {
    MemoryContextDelete(DatabaseListCxt);
  }
  MemoryContextDelete(tmpcxt);
  DatabaseListCxt = newcxt;
  MemoryContextSwitchTo(oldcxt);
}

                                                     
static int
db_comparator(const void *a, const void *b)
{
  if (((const avl_dbase *)a)->adl_score == ((const avl_dbase *)b)->adl_score)
  {
    return 0;
  }
  else
  {
    return (((const avl_dbase *)a)->adl_score < ((const avl_dbase *)b)->adl_score) ? 1 : -1;
  }
}

   
                   
   
                                                                             
                                                                           
                                                                                
                                          
   
                                                                                
                                                    
   
static Oid
do_start_worker(void)
{
  List *dblist;
  ListCell *cell;
  TransactionId xidForceLimit;
  MultiXactId multiForceLimit;
  bool for_xid_wrap;
  bool for_multi_wrap;
  avw_dbase *avdb;
  TimestampTz current_time;
  bool skipit = false;
  Oid retval = InvalidOid;
  MemoryContext tmpcxt, oldcxt;

                                                     
  LWLockAcquire(AutovacuumLock, LW_SHARED);
  if (dlist_is_empty(&AutoVacuumShmem->av_freeWorkers))
  {
    LWLockRelease(AutovacuumLock);
    return InvalidOid;
  }
  LWLockRelease(AutovacuumLock);

     
                                                                          
                                      
     
  tmpcxt = AllocSetContextCreate(CurrentMemoryContext, "Start worker tmp cxt", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(tmpcxt);

                       
  autovac_refresh_stats();

                               
  dblist = get_database_list();

     
                                                                          
                                                                      
                                           
     
  recentXid = ReadNewTransactionId();
  xidForceLimit = recentXid - autovacuum_freeze_max_age;
                                                                         
                                                                    
  if (xidForceLimit < FirstNormalTransactionId)
  {
    xidForceLimit -= FirstNormalTransactionId;
  }

                                                              
  recentMulti = ReadNextMultiXactId();
  multiForceLimit = recentMulti - MultiXactMemberFreezeThreshold();
  if (multiForceLimit < FirstMultiXactId)
  {
    multiForceLimit -= FirstMultiXactId;
  }

     
                                                                           
                                                                        
                                                                           
                                                                       
                                                                            
                                                                             
                                                                           
     
                                                                            
                                                                     
                                                                            
                
     
                                                                           
                                                                          
                                                                       
                                                                        
                                                                            
                                         
     
  avdb = NULL;
  for_xid_wrap = false;
  for_multi_wrap = false;
  current_time = GetCurrentTimestamp();
  foreach (cell, dblist)
  {
    avw_dbase *tmp = lfirst(cell);
    dlist_iter iter;

                                                           
    if (TransactionIdPrecedes(tmp->adw_frozenxid, xidForceLimit))
    {
      if (avdb == NULL || TransactionIdPrecedes(tmp->adw_frozenxid, avdb->adw_frozenxid))
      {
        avdb = tmp;
      }
      for_xid_wrap = true;
      continue;
    }
    else if (for_xid_wrap)
    {
      continue;                             
    }
    else if (MultiXactIdPrecedes(tmp->adw_minmulti, multiForceLimit))
    {
      if (avdb == NULL || MultiXactIdPrecedes(tmp->adw_minmulti, avdb->adw_minmulti))
      {
        avdb = tmp;
      }
      for_multi_wrap = true;
      continue;
    }
    else if (for_multi_wrap)
    {
      continue;                             
    }

                                  
    tmp->adw_entry = pgstat_fetch_stat_dbentry(tmp->adw_datid);

       
                                                                         
                 
       
    if (!tmp->adw_entry)
    {
      continue;
    }

       
                                                                         
                                                                           
                                                                   
                                                                           
                            
       
    skipit = false;

    dlist_reverse_foreach(iter, &DatabaseList)
    {
      avl_dbase *dbp = dlist_container(avl_dbase, adl_node, iter.cur);

      if (dbp->adl_datid == tmp->adw_datid)
      {
           
                                                                     
                                                               
           
        if (!TimestampDifferenceExceeds(dbp->adl_next_worker, current_time, 0) && !TimestampDifferenceExceeds(current_time, dbp->adl_next_worker, autovacuum_naptime * 1000))
        {
          skipit = true;
        }

        break;
      }
    }
    if (skipit)
    {
      continue;
    }

       
                                                                        
                                                   
       
    if (avdb == NULL || tmp->adw_entry->last_autovac_time < avdb->adw_entry->last_autovac_time)
    {
      avdb = tmp;
    }
  }

                                      
  if (avdb != NULL)
  {
    WorkerInfo worker;
    dlist_node *wptr;

    LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

       
                                                                         
                                     
       
    wptr = dlist_pop_head_node(&AutoVacuumShmem->av_freeWorkers);

    worker = dlist_container(WorkerInfoData, wi_links, wptr);
    worker->wi_dboid = avdb->adw_datid;
    worker->wi_proc = NULL;
    worker->wi_launchtime = GetCurrentTimestamp();

    AutoVacuumShmem->av_startingWorker = worker;

    LWLockRelease(AutovacuumLock);

    SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_WORKER);

    retval = avdb->adw_datid;
  }
  else if (skipit)
  {
       
                                                                       
                                             
       
    rebuild_database_list(InvalidOid);
  }

  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(tmpcxt);

  return retval;
}

   
                 
   
                                                                               
                                                                               
                                                                               
                            
   
                                                                              
                                                              
   
static void
launch_worker(TimestampTz now)
{
  Oid dbid;
  dlist_iter iter;

  dbid = do_start_worker();
  if (OidIsValid(dbid))
  {
    bool found = false;

       
                                                                          
                                                             
       
    dlist_foreach(iter, &DatabaseList)
    {
      avl_dbase *avdb = dlist_container(avl_dbase, adl_node, iter.cur);

      if (avdb->adl_datid == dbid)
      {
        found = true;

           
                                                                       
                                                                  
           
        avdb->adl_next_worker = TimestampTzPlusMilliseconds(now, autovacuum_naptime * 1000);

        dlist_move_head(&DatabaseList, iter.cur);
        break;
      }
    }

       
                                                                        
                                                                        
                                                                       
                                                                        
                                                          
       
    if (!found)
    {
      rebuild_database_list(dbid);
    }
  }
}

   
                                                                          
                                                                     
                                
   
void
AutoVacWorkerFailed(void)
{
  AutoVacuumShmem->av_signal[AutoVacForkFailed] = true;
}

                                                                     
static void
av_sighup_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                                              
static void
avl_sigusr2_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGUSR2 = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                          
static void
avl_sigterm_handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGTERM = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                                      
                                
                                                                      

#ifdef EXEC_BACKEND
   
                                                
   
                                              
   
static pid_t
avworker_forkexec(void)
{
  char *av[10];
  int ac = 0;

  av[ac++] = "postgres";
  av[ac++] = "--forkavworker";
  av[ac++] = NULL;                                       
  av[ac] = NULL;

  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}

   
                                                                   
   
void
AutovacuumWorkerIAm(void)
{
  am_autovacuum_worker = true;
}
#endif

   
                                                   
   
                                                
   
int
StartAutoVacWorker(void)
{
  pid_t worker_pid;

#ifdef EXEC_BACKEND
  switch ((worker_pid = avworker_forkexec()))
#else
  switch ((worker_pid = fork_process()))
#endif
  {
  case -1:
    ereport(LOG, (errmsg("could not fork autovacuum worker process: %m")));
    return 0;

#ifndef EXEC_BACKEND
  case 0:
                                 
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

    AutoVacWorkerMain(0, NULL);
    break;
#endif
  default:
    return (int)worker_pid;
  }

                          
  return 0;
}

   
                     
   
NON_EXEC_STATIC void
AutoVacWorkerMain(int argc, char *argv[])
{
  sigjmp_buf local_sigjmp_buf;
  Oid dbid;

  am_autovacuum_worker = true;

                              
  init_ps_display(pgstat_get_backend_desc(B_AUTOVAC_WORKER), "", "", "");

  SetProcessingMode(InitProcessing);

     
                                                                          
                                                                          
                      
     
  pqsignal(SIGHUP, av_sighup_handler);

     
                                                                            
                                                                   
     
  pqsignal(SIGINT, StatementCancelHandler);
  pqsignal(SIGTERM, die);
  pqsignal(SIGQUIT, quickdie);
  InitializeTimeouts();                                  

  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, procsignal_sigusr1_handler);
  pqsignal(SIGUSR2, SIG_IGN);
  pqsignal(SIGFPE, FloatExceptionHandler);
  pqsignal(SIGCHLD, SIG_DFL);

                            
  BaseInit();

     
                                                                        
                                                                            
                                                                             
                                         
     
#ifndef EXEC_BACKEND
  InitProcess();
#endif

     
                                                              
     
                                                              
     
  if (sigsetjmp(local_sigjmp_buf, 1) != 0)
  {
                                                                
    error_context_stack = NULL;

                                               
    HOLD_INTERRUPTS();

                                            
    EmitErrorReport();

       
                                                                       
                                                                   
                        
       
    proc_exit(0);
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

  PG_SETMASK(&UnBlockSig);

     
                                                                           
                                                          
                                                                         
                                                                
     
  SetConfigOption("search_path", "", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                            
                                                                             
                                
     
  SetConfigOption("zero_damaged_pages", "false", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                         
                                              
     
  SetConfigOption("statement_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);
  SetConfigOption("lock_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);
  SetConfigOption("idle_in_transaction_session_timeout", "0", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                           
                                                                           
                                               
     
  SetConfigOption("default_transaction_isolation", "read committed", PGC_SUSET, PGC_S_OVERRIDE);

     
                                                                            
                                                                            
                                                           
     
  if (synchronous_commit > SYNCHRONOUS_COMMIT_LOCAL_FLUSH)
  {
    SetConfigOption("synchronous_commit", "local", PGC_SUSET, PGC_S_OVERRIDE);
  }

     
                                                             
     
  LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

     
                                                                      
                                                                      
                                                                       
            
     
  if (AutoVacuumShmem->av_startingWorker != NULL)
  {
    MyWorkerInfo = AutoVacuumShmem->av_startingWorker;
    dbid = MyWorkerInfo->wi_dboid;
    MyWorkerInfo->wi_proc = MyProc;

                                      
    dlist_push_head(&AutoVacuumShmem->av_runningWorkers, &MyWorkerInfo->wi_links);

       
                                                                          
                                
       
    AutoVacuumShmem->av_startingWorker = NULL;
    LWLockRelease(AutovacuumLock);

    on_shmem_exit(FreeWorkerInfo, 0);

                              
    if (AutoVacuumShmem->av_launcherpid != 0)
    {
      kill(AutoVacuumShmem->av_launcherpid, SIGUSR2);
    }
  }
  else
  {
                                         
    elog(WARNING, "autovacuum worker started without a worker entry");
    dbid = InvalidOid;
    LWLockRelease(AutovacuumLock);
  }

  if (OidIsValid(dbid))
  {
    char dbname[NAMEDATALEN];

       
                                                                          
                                                                        
                                                                         
                                                                       
                                                                         
           
       
    pgstat_report_autovac(dbid);

       
                                        
       
                                                                       
                                                    
       
    InitPostgres(NULL, dbid, NULL, InvalidOid, dbname, false);
    SetProcessingMode(NormalProcessing);
    set_ps_display(dbname, false);
    ereport(DEBUG1, (errmsg("autovacuum: processing database \"%s\"", dbname)));

    if (PostAuthDelay)
    {
      pg_usleep(PostAuthDelay * 1000000L);
    }

                                              
    recentXid = ReadNewTransactionId();
    recentMulti = ReadNextMultiXactId();
    do_autovacuum();
  }

     
                                                                            
                                 
     

                         
  proc_exit(0);
}

   
                                        
   
static void
FreeWorkerInfo(int code, Datum arg)
{
  if (MyWorkerInfo != NULL)
  {
    LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

       
                                                                           
                                                                           
                                                                         
                                                                           
                                               
       
                                                                     
                                                                           
                                                                           
                                      
       
    AutovacuumLauncherPid = AutoVacuumShmem->av_launcherpid;

    dlist_delete(&MyWorkerInfo->wi_links);
    MyWorkerInfo->wi_dboid = InvalidOid;
    MyWorkerInfo->wi_tableoid = InvalidOid;
    MyWorkerInfo->wi_sharedrel = false;
    MyWorkerInfo->wi_proc = NULL;
    MyWorkerInfo->wi_launchtime = 0;
    MyWorkerInfo->wi_dobalance = false;
    MyWorkerInfo->wi_cost_delay = 0;
    MyWorkerInfo->wi_cost_limit = 0;
    MyWorkerInfo->wi_cost_limit_base = 0;
    dlist_push_head(&AutoVacuumShmem->av_freeWorkers, &MyWorkerInfo->wi_links);
                          
    MyWorkerInfo = NULL;

       
                                                                     
               
       
    AutoVacuumShmem->av_signal[AutoVacRebalance] = true;
    LWLockRelease(AutovacuumLock);
  }
}

   
                                                                            
                                               
   
void
AutoVacuumUpdateDelay(void)
{
  if (MyWorkerInfo)
  {
    VacuumCostDelay = MyWorkerInfo->wi_cost_delay;
    VacuumCostLimit = MyWorkerInfo->wi_cost_limit;
  }
}

   
                        
                                                               
   
                                                          
   
static void
autovac_balance_cost(void)
{
     
                                                                         
                                                                             
                                                                      
     
                                                                            
                                
     
  int vac_cost_limit = (autovacuum_vac_cost_limit > 0 ? autovacuum_vac_cost_limit : VacuumCostLimit);
  double vac_cost_delay = (autovacuum_vac_cost_delay >= 0 ? autovacuum_vac_cost_delay : VacuumCostDelay);
  double cost_total;
  double cost_avail;
  dlist_iter iter;

                              
  if (vac_cost_limit <= 0 || vac_cost_delay <= 0)
  {
    return;
  }

                                                                           
  cost_total = 0.0;
  dlist_foreach(iter, &AutoVacuumShmem->av_runningWorkers)
  {
    WorkerInfo worker = dlist_container(WorkerInfoData, wi_links, iter.cur);

    if (worker->wi_proc != NULL && worker->wi_dobalance && worker->wi_cost_limit_base > 0 && worker->wi_cost_delay > 0)
    {
      cost_total += (double)worker->wi_cost_limit_base / worker->wi_cost_delay;
    }
  }

                                                 
  if (cost_total <= 0)
  {
    return;
  }

     
                                                                          
                                            
     
  cost_avail = (double)vac_cost_limit / vac_cost_delay;
  dlist_foreach(iter, &AutoVacuumShmem->av_runningWorkers)
  {
    WorkerInfo worker = dlist_container(WorkerInfoData, wi_links, iter.cur);

    if (worker->wi_proc != NULL && worker->wi_dobalance && worker->wi_cost_limit_base > 0 && worker->wi_cost_delay > 0)
    {
      int limit = (int)(cost_avail * worker->wi_cost_limit_base / cost_total);

         
                                                                         
                                                                        
                                                                
                                                 
         
      worker->wi_cost_limit = Max(Min(limit, worker->wi_cost_limit_base), 1);
    }

    if (worker->wi_proc != NULL)
    {
      elog(DEBUG2, "autovac_balance_cost(pid=%u db=%u, rel=%u, dobalance=%s cost_limit=%d, cost_limit_base=%d, cost_delay=%g)", worker->wi_proc->pid, worker->wi_dboid, worker->wi_tableoid, worker->wi_dobalance ? "yes" : "no", worker->wi_cost_limit, worker->wi_cost_limit_base, worker->wi_cost_delay);
    }
  }
}

   
                     
                                                         
   
                                                                             
                                                                            
   
                                                                           
                                                                            
                                                                          
                                   
   
static List *
get_database_list(void)
{
  List *dblist = NIL;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tup;
  MemoryContext resultcxt;

                                                                    
  resultcxt = CurrentMemoryContext;

     
                                                                           
                                                                          
                                                                            
                                                                         
                                                                     
     
  StartTransactionCommand();
  (void)GetTransactionSnapshot();

  rel = table_open(DatabaseRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);

  while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
  {
    Form_pg_database pgdatabase = (Form_pg_database)GETSTRUCT(tup);
    avw_dbase *avdb;
    MemoryContext oldcxt;

       
                                                             
                                                                           
                                                                        
                                                       
       
    oldcxt = MemoryContextSwitchTo(resultcxt);

    avdb = (avw_dbase *)palloc(sizeof(avw_dbase));

    avdb->adw_datid = pgdatabase->oid;
    avdb->adw_name = pstrdup(NameStr(pgdatabase->datname));
    avdb->adw_frozenxid = pgdatabase->datfrozenxid;
    avdb->adw_minmulti = pgdatabase->datminmxid;
                              
    avdb->adw_entry = NULL;

    dblist = lappend(dblist, avdb);
    MemoryContextSwitchTo(oldcxt);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  CommitTransactionCommand();

                                                  
  MemoryContextSwitchTo(resultcxt);

  return dblist;
}

   
                                     
   
                                                                             
                                                       
   
static void
do_autovacuum(void)
{
  Relation classRel;
  HeapTuple tuple;
  TableScanDesc relScan;
  Form_pg_database dbForm;
  List *table_oids = NIL;
  List *orphan_oids = NIL;
  HASHCTL ctl;
  HTAB *table_toast_map;
  ListCell *volatile cell;
  PgStat_StatDBEntry *shared;
  PgStat_StatDBEntry *dbentry;
  BufferAccessStrategy bstrategy;
  ScanKeyData key;
  TupleDesc pg_class_desc;
  int effective_multixact_freeze_max_age;
  bool did_vacuum = false;
  bool found_concurrent_worker = false;
  int i;

     
                                                                             
                                                                     
                                                      
     
  AutovacMemCxt = AllocSetContextCreate(TopMemoryContext, "AV worker", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(AutovacMemCxt);

     
                                                                      
                                               
     
  dbentry = pgstat_fetch_stat_dbentry(MyDatabaseId);

                                                                  
  StartTransactionCommand();

     
                                                                           
                                                                           
                                              
     
  pgstat_vacuum_stat();

     
                                                                      
                                                                             
                                      
     
  effective_multixact_freeze_max_age = MultiXactMemberFreezeThreshold();

     
                                                                           
                                                                         
              
     
  tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(MyDatabaseId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for database %u", MyDatabaseId);
  }
  dbForm = (Form_pg_database)GETSTRUCT(tuple);

  if (dbForm->datistemplate || !dbForm->datallowconn)
  {
    default_freeze_min_age = 0;
    default_freeze_table_age = 0;
    default_multixact_freeze_min_age = 0;
    default_multixact_freeze_table_age = 0;
  }
  else
  {
    default_freeze_min_age = vacuum_freeze_min_age;
    default_freeze_table_age = vacuum_freeze_table_age;
    default_multixact_freeze_min_age = vacuum_multixact_freeze_min_age;
    default_multixact_freeze_table_age = vacuum_multixact_freeze_table_age;
  }

  ReleaseSysCache(tuple);

                                                 
  MemoryContextSwitchTo(AutovacMemCxt);

                                                             
  shared = pgstat_fetch_stat_dbentry(InvalidOid);

  classRel = table_open(RelationRelationId, AccessShareLock);

                                                             
  pg_class_desc = CreateTupleDescCopy(RelationGetDescr(classRel));

                                                          
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(av_relation);

  table_toast_map = hash_create("TOAST to main relid map", 100, &ctl, HASH_ELEM | HASH_BLOBS);

     
                                                        
     
                                                                             
                                                                        
                                                                             
                                                                            
                                                                     
                                           
     
                                                                           
                                                                         
                                     
     
  relScan = table_beginscan_catalog(classRel, 0, NULL);

     
                                                                            
                                         
     
  while ((tuple = heap_getnext(relScan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class classForm = (Form_pg_class)GETSTRUCT(tuple);
    PgStat_StatTabEntry *tabentry;
    AutoVacOpts *relopts;
    Oid relid;
    bool dovacuum;
    bool doanalyze;
    bool wraparound;

    if (classForm->relkind != RELKIND_RELATION && classForm->relkind != RELKIND_MATVIEW)
    {
      continue;
    }

    relid = classForm->oid;

       
                                                                          
                                                             
       
    if (classForm->relpersistence == RELPERSISTENCE_TEMP)
    {
         
                                                                     
                                                                         
                                                                      
         
      if (checkTempNamespaceStatus(classForm->relnamespace) == TEMP_NAMESPACE_IDLE)
      {
           
                                                                       
                                                                     
                                                                
                                                                      
                                                         
           
        orphan_oids = lappend_oid(orphan_oids, relid);
      }
      continue;
    }

                                                              
    relopts = extract_autovac_opts(tuple, pg_class_desc);
    tabentry = get_pgstat_tabentry_relid(relid, classForm->relisshared, shared, dbentry);

                                             
    relation_needs_vacanalyze(relid, relopts, classForm, tabentry, effective_multixact_freeze_max_age, &dovacuum, &doanalyze, &wraparound);

                                                          
    if (dovacuum || doanalyze)
    {
      table_oids = lappend_oid(table_oids, relid);
    }

       
                                                                          
                                                                         
                                                                       
       
    if (OidIsValid(classForm->reltoastrelid))
    {
      av_relation *hentry;
      bool found;

      hentry = hash_search(table_toast_map, &classForm->reltoastrelid, HASH_ENTER, &found);

      if (!found)
      {
                                                   
        hentry->ar_relid = relid;
        hentry->ar_hasrelopts = false;
        if (relopts != NULL)
        {
          hentry->ar_hasrelopts = true;
          memcpy(&hentry->ar_reloptions, relopts, sizeof(AutoVacOpts));
        }
      }
    }
  }

  table_endscan(relScan);

                                       
  ScanKeyInit(&key, Anum_pg_class_relkind, BTEqualStrategyNumber, F_CHAREQ, CharGetDatum(RELKIND_TOASTVALUE));

  relScan = table_beginscan_catalog(classRel, 1, &key);
  while ((tuple = heap_getnext(relScan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class classForm = (Form_pg_class)GETSTRUCT(tuple);
    PgStat_StatTabEntry *tabentry;
    Oid relid;
    AutoVacOpts *relopts = NULL;
    bool dovacuum;
    bool doanalyze;
    bool wraparound;

       
                                                                          
       
    if (classForm->relpersistence == RELPERSISTENCE_TEMP)
    {
      continue;
    }

    relid = classForm->oid;

       
                                                                           
                
       
    relopts = extract_autovac_opts(tuple, pg_class_desc);
    if (relopts == NULL)
    {
      av_relation *hentry;
      bool found;

      hentry = hash_search(table_toast_map, &relid, HASH_FIND, &found);
      if (found && hentry->ar_hasrelopts)
      {
        relopts = &hentry->ar_reloptions;
      }
    }

                                               
    tabentry = get_pgstat_tabentry_relid(relid, classForm->relisshared, shared, dbentry);

    relation_needs_vacanalyze(relid, relopts, classForm, tabentry, effective_multixact_freeze_max_age, &dovacuum, &doanalyze, &wraparound);

                                         
    if (dovacuum)
    {
      table_oids = lappend_oid(table_oids, relid);
    }
  }

  table_endscan(relScan);
  table_close(classRel, AccessShareLock);

     
                                                                            
                                                                        
                                                                        
                                                                       
                                                                        
                                                                             
                                                                         
     
  foreach (cell, orphan_oids)
  {
    Oid relid = lfirst_oid(cell);
    Form_pg_class classForm;
    ObjectAddress object;

       
                                       
       
    CHECK_FOR_INTERRUPTS();

       
                                                                     
                                                                       
                                                                         
       
    if (!ConditionalLockRelationOid(relid, AccessExclusiveLock))
    {
      continue;
    }

       
                                                                          
                                                                           
                            
       
    tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(tuple))
    {
                                                                     
      UnlockRelationOid(relid, AccessExclusiveLock);
      continue;
    }
    classForm = (Form_pg_class)GETSTRUCT(tuple);

       
                                                                        
                                                                   
                                                      
       
    if (!((classForm->relkind == RELKIND_RELATION || classForm->relkind == RELKIND_MATVIEW) && classForm->relpersistence == RELPERSISTENCE_TEMP))
    {
      UnlockRelationOid(relid, AccessExclusiveLock);
      continue;
    }

    if (checkTempNamespaceStatus(classForm->relnamespace) != TEMP_NAMESPACE_IDLE)
    {
      UnlockRelationOid(relid, AccessExclusiveLock);
      continue;
    }

                             
    ereport(LOG, (errmsg("autovacuum: dropping orphan temp table \"%s.%s.%s\"", get_database_name(MyDatabaseId), get_namespace_name(classForm->relnamespace), NameStr(classForm->relname))));

    object.classId = RelationRelationId;
    object.objectId = relid;
    object.objectSubId = 0;
    performDeletion(&object, DROP_CASCADE, PERFORM_DELETION_INTERNAL | PERFORM_DELETION_QUIETLY | PERFORM_DELETION_SKIP_EXTENSIONS);

       
                                                                       
                                                       
       
    CommitTransactionCommand();
    StartTransactionCommand();

                                                                
    MemoryContextSwitchTo(AutovacMemCxt);
  }

     
                                                                           
                                                                             
                                                           
     
  bstrategy = GetAccessStrategy(BAS_VACUUM);

     
                                                                       
                                                                        
     
  PortalContext = AllocSetContextCreate(AutovacMemCxt, "Autovacuum Portal", ALLOCSET_DEFAULT_SIZES);

     
                                             
     
  foreach (cell, table_oids)
  {
    Oid relid = lfirst_oid(cell);
    HeapTuple classTup;
    autovac_table *tab;
    bool isshared;
    bool skipit;
    double stdVacuumCostDelay;
    int stdVacuumCostLimit;
    dlist_iter iter;

    CHECK_FOR_INTERRUPTS();

       
                                                                        
       
    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);

         
                                                                      
                                                                   
                                                                      
                                 
         
    }

       
                                                                    
                                                                          
                                                                     
                                                                        
                                                                    
                                                                     
       
    classTup = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(classTup))
    {
      continue;                                          
    }
    isshared = ((Form_pg_class)GETSTRUCT(classTup))->relisshared;
    ReleaseSysCache(classTup);

       
                                                                       
                                                                           
                                  
       
    LWLockAcquire(AutovacuumScheduleLock, LW_EXCLUSIVE);
    LWLockAcquire(AutovacuumLock, LW_SHARED);

       
                                                                         
               
       
    skipit = false;
    dlist_foreach(iter, &AutoVacuumShmem->av_runningWorkers)
    {
      WorkerInfo worker = dlist_container(WorkerInfoData, wi_links, iter.cur);

                         
      if (worker == MyWorkerInfo)
      {
        continue;
      }

                                                                      
      if (!worker->wi_sharedrel && worker->wi_dboid != MyDatabaseId)
      {
        continue;
      }

      if (worker->wi_tableoid == relid)
      {
        skipit = true;
        found_concurrent_worker = true;
        break;
      }
    }
    LWLockRelease(AutovacuumLock);
    if (skipit)
    {
      LWLockRelease(AutovacuumScheduleLock);
      continue;
    }

       
                                                                   
                                                                   
                                                          
                                                           
       
    MyWorkerInfo->wi_tableoid = relid;
    MyWorkerInfo->wi_sharedrel = isshared;
    LWLockRelease(AutovacuumScheduleLock);

       
                                                                          
                                                                         
                           
       
                                                                      
                                                                         
                                                                        
                                                              
       
    MemoryContextSwitchTo(AutovacMemCxt);
    tab = table_recheck_autovac(relid, table_toast_map, pg_class_desc, effective_multixact_freeze_max_age);
    if (tab == NULL)
    {
                                                            
      LWLockAcquire(AutovacuumScheduleLock, LW_EXCLUSIVE);
      MyWorkerInfo->wi_tableoid = InvalidOid;
      MyWorkerInfo->wi_sharedrel = false;
      LWLockRelease(AutovacuumScheduleLock);
      continue;
    }

       
                                                                           
                                                                         
                                                               
       
    stdVacuumCostDelay = VacuumCostDelay;
    stdVacuumCostLimit = VacuumCostLimit;

                                                                       
    LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

                                                                        
    MyWorkerInfo->wi_dobalance = tab->at_dobalance;
    MyWorkerInfo->wi_cost_delay = tab->at_vacuum_cost_delay;
    MyWorkerInfo->wi_cost_limit = tab->at_vacuum_cost_limit;
    MyWorkerInfo->wi_cost_limit_base = tab->at_vacuum_cost_limit;

                      
    autovac_balance_cost();

                                                                
    AutoVacuumUpdateDelay();

              
    LWLockRelease(AutovacuumLock);

                                               
    MemoryContextResetAndDeleteChildren(PortalContext);

       
                                                                       
                                                                         
                                                                          
                                                                           
                                                     
       

    tab->at_relname = get_rel_name(tab->at_relid);
    tab->at_nspname = get_namespace_name(get_rel_namespace(tab->at_relid));
    tab->at_datname = get_database_name(MyDatabaseId);
    if (!tab->at_relname || !tab->at_nspname || !tab->at_datname)
    {
      goto deleted;
    }

       
                                                                          
                                                                       
                                                  
       
    PG_TRY();
    {
                                                           
      MemoryContextSwitchTo(PortalContext);

                      
      autovacuum_do_vac_analyze(tab, bstrategy);

         
                                                                        
                                                                  
                                                                         
                                
         
      QueryCancelPending = false;
    }
    PG_CATCH();
    {
         
                                                                      
                                 
         
      HOLD_INTERRUPTS();
      if (tab->at_params.options & VACOPT_VACUUM)
      {
        errcontext("automatic vacuum of table \"%s.%s.%s\"", tab->at_datname, tab->at_nspname, tab->at_relname);
      }
      else
      {
        errcontext("automatic analyze of table \"%s.%s.%s\"", tab->at_datname, tab->at_nspname, tab->at_relname);
      }
      EmitErrorReport();

                                            
      AbortOutOfAnyTransaction();
      FlushErrorState();
      MemoryContextResetAndDeleteChildren(PortalContext);

                                                                
      StartTransactionCommand();
      RESUME_INTERRUPTS();
    }
    PG_END_TRY();

                                               
    MemoryContextSwitchTo(AutovacMemCxt);

    did_vacuum = true;

                                                                   

                 
  deleted:
    if (tab->at_datname != NULL)
    {
      pfree(tab->at_datname);
    }
    if (tab->at_nspname != NULL)
    {
      pfree(tab->at_nspname);
    }
    if (tab->at_relname != NULL)
    {
      pfree(tab->at_relname);
    }
    pfree(tab);

       
                                                                       
                                                                 
                                                                     
                                                                         
                                                             
       
    LWLockAcquire(AutovacuumScheduleLock, LW_EXCLUSIVE);
    MyWorkerInfo->wi_tableoid = InvalidOid;
    MyWorkerInfo->wi_sharedrel = false;
    LWLockRelease(AutovacuumScheduleLock);

                                                         
    VacuumCostDelay = stdVacuumCostDelay;
    VacuumCostLimit = stdVacuumCostLimit;
  }

     
                                                              
     
  LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);
  for (i = 0; i < NUM_WORKITEMS; i++)
  {
    AutoVacuumWorkItem *workitem = &AutoVacuumShmem->av_workItems[i];

    if (!workitem->avw_used)
    {
      continue;
    }
    if (workitem->avw_active)
    {
      continue;
    }
    if (workitem->avw_database != MyDatabaseId)
    {
      continue;
    }

                                                              
    workitem->avw_active = true;
    LWLockRelease(AutovacuumLock);

    perform_work_item(workitem);

       
                                                                        
       
    CHECK_FOR_INTERRUPTS();
    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
    }

    LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

                          
    workitem->avw_active = false;
    workitem->avw_used = false;
  }
  LWLockRelease(AutovacuumLock);

     
                                                                        
                                          
     

     
                                                                           
                                                      
     
                                                                        
                                                                          
                                                                         
                                                                        
                                                                           
                                                               
     
                                                                       
                                                                            
                                                                           
                                                                            
                                                                           
                                                                        
                                                                           
                                                                 
     
  if (did_vacuum || !found_concurrent_worker)
  {
    vac_update_datfrozenxid();
  }

                                               
  CommitTransactionCommand();
}

   
                                              
   
static void
perform_work_item(AutoVacuumWorkItem *workitem)
{
  char *cur_datname = NULL;
  char *cur_nspname = NULL;
  char *cur_relname = NULL;

     
                                                                        
                       
     

     
                                                                             
                                                                        
                                                               
     
  Assert(CurrentMemoryContext == AutovacMemCxt);

  cur_relname = get_rel_name(workitem->avw_relation);
  cur_nspname = get_namespace_name(get_rel_namespace(workitem->avw_relation));
  cur_datname = get_database_name(MyDatabaseId);
  if (!cur_relname || !cur_nspname || !cur_datname)
  {
    goto deleted2;
  }

  autovac_report_workitem(workitem, cur_nspname, cur_relname);

                                             
  MemoryContextResetAndDeleteChildren(PortalContext);

     
                                                                      
                                                                       
                                                                            
                   
     
  PG_TRY();
  {
                                                             
    MemoryContextSwitchTo(PortalContext);

                    
    switch (workitem->avw_type)
    {
    case AVW_BRINSummarizeRange:
      DirectFunctionCall2(brin_summarize_range, ObjectIdGetDatum(workitem->avw_relation), Int64GetDatum((int64)workitem->avw_blockNumber));
      break;
    default:
      elog(WARNING, "unrecognized work item found: type %d", workitem->avw_type);
      break;
    }

       
                                                                         
                                                                           
                                                                        
               
       
    QueryCancelPending = false;
  }
  PG_CATCH();
  {
       
                                                                         
                          
       
    HOLD_INTERRUPTS();
    errcontext("processing work entry for relation \"%s.%s.%s\"", cur_datname, cur_nspname, cur_relname);
    EmitErrorReport();

                                          
    AbortOutOfAnyTransaction();
    FlushErrorState();
    MemoryContextResetAndDeleteChildren(PortalContext);

                                                              
    StartTransactionCommand();
    RESUME_INTERRUPTS();
  }
  PG_END_TRY();

                                             
  MemoryContextSwitchTo(AutovacMemCxt);

                                                   

               
deleted2:
  if (cur_datname)
  {
    pfree(cur_datname);
  }
  if (cur_nspname)
  {
    pfree(cur_nspname);
  }
  if (cur_relname)
  {
    pfree(cur_relname);
  }
}

   
                        
   
                                                                        
                                               
   
static AutoVacOpts *
extract_autovac_opts(HeapTuple tup, TupleDesc pg_class_desc)
{
  bytea *relopts;
  AutoVacOpts *av;

  Assert(((Form_pg_class)GETSTRUCT(tup))->relkind == RELKIND_RELATION || ((Form_pg_class)GETSTRUCT(tup))->relkind == RELKIND_MATVIEW || ((Form_pg_class)GETSTRUCT(tup))->relkind == RELKIND_TOASTVALUE);

  relopts = extractRelOptions(tup, pg_class_desc, NULL);
  if (relopts == NULL)
  {
    return NULL;
  }

  av = palloc(sizeof(AutoVacOpts));
  memcpy(av, &(((StdRdOptions *)relopts)->autovacuum), sizeof(AutoVacOpts));
  pfree(relopts);

  return av;
}

   
                             
   
                                                                            
   
static PgStat_StatTabEntry *
get_pgstat_tabentry_relid(Oid relid, bool isshared, PgStat_StatDBEntry *shared, PgStat_StatDBEntry *dbentry)
{
  PgStat_StatTabEntry *tabentry = NULL;

  if (isshared)
  {
    if (PointerIsValid(shared))
    {
      tabentry = hash_search(shared->tables, &relid, HASH_FIND, NULL);
    }
  }
  else if (PointerIsValid(dbentry))
  {
    tabentry = hash_search(dbentry->tables, &relid, HASH_FIND, NULL);
  }

  return tabentry;
}

   
                         
   
                                                                             
                                                           
   
                                                                           
   
static autovac_table *
table_recheck_autovac(Oid relid, HTAB *table_toast_map, TupleDesc pg_class_desc, int effective_multixact_freeze_max_age)
{
  Form_pg_class classForm;
  HeapTuple classTup;
  bool dovacuum;
  bool doanalyze;
  autovac_table *tab = NULL;
  PgStat_StatTabEntry *tabentry;
  PgStat_StatDBEntry *shared;
  PgStat_StatDBEntry *dbentry;
  bool wraparound;
  AutoVacOpts *avopts;

                       
  autovac_refresh_stats();

  shared = pgstat_fetch_stat_dbentry(InvalidOid);
  dbentry = pgstat_fetch_stat_dbentry(MyDatabaseId);

                                           
  classTup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(classTup))
  {
    return NULL;
  }
  classForm = (Form_pg_class)GETSTRUCT(classTup);

     
                                                                            
                                                                   
     
  avopts = extract_autovac_opts(classTup, pg_class_desc);
  if (classForm->relkind == RELKIND_TOASTVALUE && avopts == NULL && table_toast_map != NULL)
  {
    av_relation *hentry;
    bool found;

    hentry = hash_search(table_toast_map, &relid, HASH_FIND, &found);
    if (found && hentry->ar_hasrelopts)
    {
      avopts = &hentry->ar_reloptions;
    }
  }

                                    
  tabentry = get_pgstat_tabentry_relid(relid, classForm->relisshared, shared, dbentry);

  relation_needs_vacanalyze(relid, avopts, classForm, tabentry, effective_multixact_freeze_max_age, &dovacuum, &doanalyze, &wraparound);

                                       
  if (classForm->relkind == RELKIND_TOASTVALUE)
  {
    doanalyze = false;
  }

                                   
  if (doanalyze || dovacuum)
  {
    int freeze_min_age;
    int freeze_table_age;
    int multixact_freeze_min_age;
    int multixact_freeze_table_age;
    int vac_cost_limit;
    double vac_cost_delay;
    int log_min_duration;

       
                                                                           
                                                                          
                                                                   
                                                                 
       

                                                                 
    vac_cost_delay = (avopts && avopts->vacuum_cost_delay >= 0) ? avopts->vacuum_cost_delay : (autovacuum_vac_cost_delay >= 0) ? autovacuum_vac_cost_delay : VacuumCostDelay;

                                                                      
    vac_cost_limit = (avopts && avopts->vacuum_cost_limit > 0) ? avopts->vacuum_cost_limit : (autovacuum_vac_cost_limit > 0) ? autovacuum_vac_cost_limit : VacuumCostLimit;

                                                                     
    log_min_duration = (avopts && avopts->log_min_duration >= 0) ? avopts->log_min_duration : Log_autovacuum_min_duration;

                                                        
    freeze_min_age = (avopts && avopts->freeze_min_age >= 0) ? avopts->freeze_min_age : default_freeze_min_age;

    freeze_table_age = (avopts && avopts->freeze_table_age >= 0) ? avopts->freeze_table_age : default_freeze_table_age;

    multixact_freeze_min_age = (avopts && avopts->multixact_freeze_min_age >= 0) ? avopts->multixact_freeze_min_age : default_multixact_freeze_min_age;

    multixact_freeze_table_age = (avopts && avopts->multixact_freeze_table_age >= 0) ? avopts->multixact_freeze_table_age : default_multixact_freeze_table_age;

    tab = palloc(sizeof(autovac_table));
    tab->at_relid = relid;
    tab->at_sharedrel = classForm->relisshared;
    tab->at_params.options = VACOPT_SKIPTOAST | (dovacuum ? VACOPT_VACUUM : 0) | (doanalyze ? VACOPT_ANALYZE : 0) | (!wraparound ? VACOPT_SKIP_LOCKED : 0);
    tab->at_params.index_cleanup = VACOPT_TERNARY_DEFAULT;
    tab->at_params.truncate = VACOPT_TERNARY_DEFAULT;
    tab->at_params.freeze_min_age = freeze_min_age;
    tab->at_params.freeze_table_age = freeze_table_age;
    tab->at_params.multixact_freeze_min_age = multixact_freeze_min_age;
    tab->at_params.multixact_freeze_table_age = multixact_freeze_table_age;
    tab->at_params.is_wraparound = wraparound;
    tab->at_params.log_min_duration = log_min_duration;
    tab->at_vacuum_cost_limit = vac_cost_limit;
    tab->at_vacuum_cost_delay = vac_cost_delay;
    tab->at_relname = NULL;
    tab->at_nspname = NULL;
    tab->at_datname = NULL;

       
                                                                         
                                                    
       
    tab->at_dobalance = !(avopts && (avopts->vacuum_cost_limit > 0 || avopts->vacuum_cost_delay > 0));
  }

  heap_freetuple(classTup);

  return tab;
}

   
                             
   
                                                                               
                                                                                
                                                        
   
                                                                             
                                                                               
                                                                                
         
   
                                                                       
                                               
   
                                                              
   
                                                                         
                                                                             
                                                                          
                                                                            
                                                    
   
                                                                                
                                                         
                                             
   
                                                       
                                                                              
                                                                             
                                                                   
   
                                                                            
                                                                            
                                              
                                                                    
   
static void
relation_needs_vacanalyze(Oid relid, AutoVacOpts *relopts, Form_pg_class classForm, PgStat_StatTabEntry *tabentry, int effective_multixact_freeze_max_age,
                             
    bool *dovacuum, bool *doanalyze, bool *wraparound)
{
  bool force_vacuum;
  bool av_enabled;
  float4 reltuples;                         

                                                  
  int vac_base_thresh, anl_base_thresh;
  float4 vac_scale_factor, anl_scale_factor;

                                                  
  float4 vacthresh, anlthresh;

                                                            
  float4 vactuples, anltuples;

                         
  int freeze_max_age;
  int multixact_freeze_max_age;
  TransactionId xidForceLimit;
  MultiXactId multiForceLimit;

  AssertArg(classForm != NULL);
  AssertArg(OidIsValid(relid));

     
                                                                         
                                                                            
                                              
     

                                                                 
  vac_scale_factor = (relopts && relopts->vacuum_scale_factor >= 0) ? relopts->vacuum_scale_factor : autovacuum_vac_scale;

  vac_base_thresh = (relopts && relopts->vacuum_threshold >= 0) ? relopts->vacuum_threshold : autovacuum_vac_thresh;

  anl_scale_factor = (relopts && relopts->analyze_scale_factor >= 0) ? relopts->analyze_scale_factor : autovacuum_anl_scale;

  anl_base_thresh = (relopts && relopts->analyze_threshold >= 0) ? relopts->analyze_threshold : autovacuum_anl_thresh;

  freeze_max_age = (relopts && relopts->freeze_max_age >= 0) ? Min(relopts->freeze_max_age, autovacuum_freeze_max_age) : autovacuum_freeze_max_age;

  multixact_freeze_max_age = (relopts && relopts->multixact_freeze_max_age >= 0) ? Min(relopts->multixact_freeze_max_age, effective_multixact_freeze_max_age) : effective_multixact_freeze_max_age;

  av_enabled = (relopts ? relopts->enabled : true);

                                                      
  xidForceLimit = recentXid - freeze_max_age;
  if (xidForceLimit < FirstNormalTransactionId)
  {
    xidForceLimit -= FirstNormalTransactionId;
  }
  force_vacuum = (TransactionIdIsNormal(classForm->relfrozenxid) && TransactionIdPrecedes(classForm->relfrozenxid, xidForceLimit));
  if (!force_vacuum)
  {
    multiForceLimit = recentMulti - multixact_freeze_max_age;
    if (multiForceLimit < FirstMultiXactId)
    {
      multiForceLimit -= FirstMultiXactId;
    }
    force_vacuum = MultiXactIdIsValid(classForm->relminmxid) && MultiXactIdPrecedes(classForm->relminmxid, multiForceLimit);
  }
  *wraparound = force_vacuum;

                                                                         
  if (!av_enabled && !force_vacuum)
  {
    *doanalyze = false;
    *dovacuum = false;
    return;
  }

     
                                                                          
                                                                       
                                                                        
                                                                           
                              
     
  if (PointerIsValid(tabentry) && AutoVacuumingActive())
  {
    reltuples = classForm->reltuples;
    vactuples = tabentry->n_dead_tuples;
    anltuples = tabentry->changes_since_analyze;

    vacthresh = (float4)vac_base_thresh + vac_scale_factor * reltuples;
    anlthresh = (float4)anl_base_thresh + anl_scale_factor * reltuples;

       
                                                                      
                                                                          
                          
       
    elog(DEBUG3, "%s: vac: %.0f (threshold %.0f), anl: %.0f (threshold %.0f)", NameStr(classForm->relname), vactuples, vacthresh, anltuples, anlthresh);

                                                          
    *dovacuum = force_vacuum || (vactuples > vacthresh);
    *doanalyze = (anltuples > anlthresh);
  }
  else
  {
       
                                                                           
                                                                           
                  
       
    *dovacuum = force_vacuum;
    *doanalyze = false;
  }

                                                 
  if (relid == StatisticRelationId)
  {
    *doanalyze = false;
  }
}

   
                             
                                              
   
static void
autovacuum_do_vac_analyze(autovac_table *tab, BufferAccessStrategy bstrategy)
{
  RangeVar *rangevar;
  VacuumRelation *rel;
  List *rel_list;

                                        
  autovac_report_activity(tab);

                                                                         
  rangevar = makeRangeVar(tab->at_nspname, tab->at_relname, -1);
  rel = makeVacuumRelation(rangevar, tab->at_relid, NIL);
  rel_list = list_make1(rel);

  vacuum(rel_list, &tab->at_params, bstrategy, true);
}

   
                           
                                              
   
                                                                        
                                                 
   
                                                                                
                                                                             
                                           
   
static void
autovac_report_activity(autovac_table *tab)
{
#define MAX_AUTOVAC_ACTIV_LEN (NAMEDATALEN * 2 + 56)
  char activity[MAX_AUTOVAC_ACTIV_LEN];
  int len;

                                               
  if (tab->at_params.options & VACOPT_VACUUM)
  {
    snprintf(activity, MAX_AUTOVAC_ACTIV_LEN, "autovacuum: VACUUM%s", tab->at_params.options & VACOPT_ANALYZE ? " ANALYZE" : "");
  }
  else
  {
    snprintf(activity, MAX_AUTOVAC_ACTIV_LEN, "autovacuum: ANALYZE");
  }

     
                                                
     
  len = strlen(activity);

  snprintf(activity + len, MAX_AUTOVAC_ACTIV_LEN - len, " %s.%s%s", tab->at_nspname, tab->at_relname, tab->at_params.is_wraparound ? " (to prevent wraparound)" : "");

                                                                      
  SetCurrentStatementStartTimestamp();

  pgstat_report_activity(STATE_RUNNING, activity);
}

   
                           
                                                               
   
static void
autovac_report_workitem(AutoVacuumWorkItem *workitem, const char *nspname, const char *relname)
{
  char activity[MAX_AUTOVAC_ACTIV_LEN + 12 + 2];
  char blk[12 + 2];
  int len;

  switch (workitem->avw_type)
  {
  case AVW_BRINSummarizeRange:
    snprintf(activity, MAX_AUTOVAC_ACTIV_LEN, "autovacuum: BRIN summarize");
    break;
  }

     
                                                                            
     
  len = strlen(activity);

  if (BlockNumberIsValid(workitem->avw_blockNumber))
  {
    snprintf(blk, sizeof(blk), " %u", workitem->avw_blockNumber);
  }
  else
  {
    blk[0] = '\0';
  }

  snprintf(activity + len, MAX_AUTOVAC_ACTIV_LEN - len, " %s.%s%s", nspname, relname, blk);

                                                                      
  SetCurrentStatementStartTimestamp();

  pgstat_report_activity(STATE_RUNNING, activity);
}

   
                       
                                                                       
             
   
bool
AutoVacuumingActive(void)
{
  if (!autovacuum_start_daemon || !pgstat_track_counts)
  {
    return false;
  }
  return true;
}

   
                                                                             
                                                  
   
bool
AutoVacuumRequestWork(AutoVacuumWorkItemType type, Oid relationId, BlockNumber blkno)
{
  int i;
  bool result = false;

  LWLockAcquire(AutovacuumLock, LW_EXCLUSIVE);

     
                                                                 
     
  for (i = 0; i < NUM_WORKITEMS; i++)
  {
    AutoVacuumWorkItem *workitem = &AutoVacuumShmem->av_workItems[i];

    if (workitem->avw_used)
    {
      continue;
    }

    workitem->avw_used = true;
    workitem->avw_active = false;
    workitem->avw_type = type;
    workitem->avw_database = MyDatabaseId;
    workitem->avw_relation = relationId;
    workitem->avw_blockNumber = blkno;
    result = true;

              
    break;
  }

  LWLockRelease(AutovacuumLock);

  return result;
}

   
                
                                                 
   
                                                        
   
void
autovac_init(void)
{
  if (autovacuum_start_daemon && !pgstat_track_counts)
  {
    ereport(WARNING, (errmsg("autovacuum not started because of misconfiguration"), errhint("Enable the \"track_counts\" option.")));
  }
}

   
                          
                                                                            
             
   
bool
IsAutoVacuumLauncherProcess(void)
{
  return am_autovacuum_launcher;
}

bool
IsAutoVacuumWorkerProcess(void)
{
  return am_autovacuum_worker;
}

   
                       
                                                              
   
Size
AutoVacuumShmemSize(void)
{
  Size size;

     
                                                            
     
  size = sizeof(AutoVacuumShmemStruct);
  size = MAXALIGN(size);
  size = add_size(size, mul_size(autovacuum_max_workers, sizeof(WorkerInfoData)));
  return size;
}

   
                       
                                                             
   
void
AutoVacuumShmemInit(void)
{
  bool found;

  AutoVacuumShmem = (AutoVacuumShmemStruct *)ShmemInitStruct("AutoVacuum Data", AutoVacuumShmemSize(), &found);

  if (!IsUnderPostmaster)
  {
    WorkerInfo worker;
    int i;

    Assert(!found);

    AutoVacuumShmem->av_launcherpid = 0;
    dlist_init(&AutoVacuumShmem->av_freeWorkers);
    dlist_init(&AutoVacuumShmem->av_runningWorkers);
    AutoVacuumShmem->av_startingWorker = NULL;
    memset(AutoVacuumShmem->av_workItems, 0, sizeof(AutoVacuumWorkItem) * NUM_WORKITEMS);

    worker = (WorkerInfo)((char *)AutoVacuumShmem + MAXALIGN(sizeof(AutoVacuumShmemStruct)));

                                             
    for (i = 0; i < autovacuum_max_workers; i++)
    {
      dlist_push_head(&AutoVacuumShmem->av_freeWorkers, &worker[i].wi_links);
    }
  }
  else
  {
    Assert(found);
  }
}

   
                         
                                                   
   
                                                                            
                                                                        
                                                                             
                       
   
                                                                   
                                           
   
static void
autovac_refresh_stats(void)
{
  if (IsAutoVacuumLauncherProcess())
  {
    static TimestampTz last_read = 0;
    TimestampTz current_time;

    current_time = GetCurrentTimestamp();

    if (!TimestampDifferenceExceeds(last_read, current_time, STATS_READ_DELAY))
    {
      return;
    }

    last_read = current_time;
  }

  pgstat_clear_snapshot();
}
