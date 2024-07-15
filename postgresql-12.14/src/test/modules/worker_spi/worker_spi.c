                                                                             
   
                
                                                                   
                                                                          
                                                                    
                                                                     
                                                                 
   
                                                                                
                                                                              
                                                                              
                                                                             
                              
   
                                                                
   
                  
                                             
   
                                                                             
   
#include "postgres.h"

                                               
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

                                                             
#include "access/xact.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "tcop/utility.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(worker_spi_launch);

void
_PG_init(void);
void worker_spi_main(Datum) pg_attribute_noreturn();

                                  
static volatile sig_atomic_t got_sighup = false;
static volatile sig_atomic_t got_sigterm = false;

                   
static int worker_spi_naptime = 10;
static int worker_spi_total_workers = 2;
static char *worker_spi_database = NULL;

typedef struct worktable
{
  const char *schema;
  const char *name;
} worktable;

   
                              
                                                                            
           
   
static void
worker_spi_sigterm(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_sigterm = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                             
                                                                        
                             
   
static void
worker_spi_sighup(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_sighup = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                                                                              
                  
   
static void
initialize_worker_spi(worktable *table)
{
  int ret;
  int ntup;
  bool isnull;
  StringInfoData buf;

  SetCurrentStatementStartTimestamp();
  StartTransactionCommand();
  SPI_connect();
  PushActiveSnapshot(GetTransactionSnapshot());
  pgstat_report_activity(STATE_RUNNING, "initializing worker_spi schema");

                                                     
  initStringInfo(&buf);
  appendStringInfo(&buf, "select count(*) from pg_namespace where nspname = '%s'", table->schema);

  ret = SPI_execute(buf.data, true, 0);
  if (ret != SPI_OK_SELECT)
  {
    elog(FATAL, "SPI_execute failed: error code %d", ret);
  }

  if (SPI_processed != 1)
  {
    elog(FATAL, "not a singleton result");
  }

  ntup = DatumGetInt64(SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull));
  if (isnull)
  {
    elog(FATAL, "null result");
  }

  if (ntup == 0)
  {
    resetStringInfo(&buf);
    appendStringInfo(&buf,
        "CREATE SCHEMA \"%s\" "
        "CREATE TABLE \"%s\" ("
        "		type text CHECK (type IN ('total', 'delta')), "
        "		value	integer)"
        "CREATE UNIQUE INDEX \"%s_unique_total\" ON \"%s\" (type) "
        "WHERE type = 'total'",
        table->schema, table->name, table->name, table->name);

                                  
    SetCurrentStatementStartTimestamp();

    ret = SPI_execute(buf.data, false, 0);

    if (ret != SPI_OK_UTILITY)
    {
      elog(FATAL, "failed to create my schema");
    }
  }

  SPI_finish();
  PopActiveSnapshot();
  CommitTransactionCommand();
  pgstat_report_activity(STATE_IDLE, NULL);
}

void
worker_spi_main(Datum main_arg)
{
  int index = DatumGetInt32(main_arg);
  worktable *table;
  StringInfoData buf;
  char name[20];

  table = palloc(sizeof(worktable));
  sprintf(name, "schema%d", index);
  table->schema = pstrdup(name);
  table->name = pstrdup("counted");

                                                            
  pqsignal(SIGHUP, worker_spi_sighup);
  pqsignal(SIGTERM, worker_spi_sigterm);

                                          
  BackgroundWorkerUnblockSignals();

                               
  BackgroundWorkerInitializeConnection(worker_spi_database, NULL, 0);

  elog(LOG, "%s initialized with %s.%s", MyBgworkerEntry->bgw_name, table->schema, table->name);
  initialize_worker_spi(table);

     
                                                                        
                                                                           
             
     
                                            
     
  table->schema = quote_identifier(table->schema);
  table->name = quote_identifier(table->name);

  initStringInfo(&buf);
  appendStringInfo(&buf,
      "WITH deleted AS (DELETE "
      "FROM %s.%s "
      "WHERE type = 'delta' RETURNING value), "
      "total AS (SELECT coalesce(sum(value), 0) as sum "
      "FROM deleted) "
      "UPDATE %s.%s "
      "SET value = %s.value + total.sum "
      "FROM total WHERE type = 'total' "
      "RETURNING %s.value",
      table->schema, table->name, table->schema, table->name, table->name, table->name);

     
                                                                        
     
  while (!got_sigterm)
  {
    int ret;

       
                                                                          
                                                                      
                                                                    
                                                                 
       
    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, worker_spi_naptime * 1000L, PG_WAIT_EXTENSION);
    ResetLatch(MyLatch);

    CHECK_FOR_INTERRUPTS();

       
                                                           
       
    if (got_sighup)
    {
      got_sighup = false;
      ProcessConfigFile(PGC_SIGHUP);
    }

       
                                                                        
                                                              
                                                                          
                                                                       
                                                                          
                                                                          
                                        
       
                                                                           
                                                                      
                                                                    
       
                                                                    
                                 
       
    SetCurrentStatementStartTimestamp();
    StartTransactionCommand();
    SPI_connect();
    PushActiveSnapshot(GetTransactionSnapshot());
    pgstat_report_activity(STATE_RUNNING, buf.data);

                                            
    ret = SPI_execute(buf.data, false, 0);

    if (ret != SPI_OK_UPDATE_RETURNING)
    {
      elog(FATAL, "cannot select from table %s.%s: error code %d", table->schema, table->name, ret);
    }

    if (SPI_processed > 0)
    {
      bool isnull;
      int32 val;

      val = DatumGetInt32(SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull));
      if (!isnull)
      {
        elog(LOG, "%s: count in %s.%s is now %d", MyBgworkerEntry->bgw_name, table->schema, table->name, val);
      }
    }

       
                                   
       
    SPI_finish();
    PopActiveSnapshot();
    CommitTransactionCommand();
    pgstat_report_stat(false);
    pgstat_report_activity(STATE_IDLE, NULL);
  }

  proc_exit(1);
}

   
                              
   
                                                                              
            
   
void
_PG_init(void)
{
  BackgroundWorker worker;
  unsigned int i;

                             
  DefineCustomIntVariable("worker_spi.naptime", "Duration between each check (in seconds).", NULL, &worker_spi_naptime, 10, 1, INT_MAX, PGC_SIGHUP, 0, NULL, NULL, NULL);

  if (!process_shared_preload_libraries_in_progress)
  {
    return;
  }

  DefineCustomIntVariable("worker_spi.total_workers", "Number of workers.", NULL, &worker_spi_total_workers, 2, 1, 100, PGC_POSTMASTER, 0, NULL, NULL, NULL);

  DefineCustomStringVariable("worker_spi.database", "Database to connect to.", NULL, &worker_spi_database, "postgres", PGC_POSTMASTER, 0, NULL, NULL, NULL);

                                              
  memset(&worker, 0, sizeof(worker));
  worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
  worker.bgw_restart_time = BGW_NEVER_RESTART;
  sprintf(worker.bgw_library_name, "worker_spi");
  sprintf(worker.bgw_function_name, "worker_spi_main");
  worker.bgw_notify_pid = 0;

     
                                                                        
     
  for (i = 1; i <= worker_spi_total_workers; i++)
  {
    snprintf(worker.bgw_name, BGW_MAXLEN, "worker_spi worker %d", i);
    snprintf(worker.bgw_type, BGW_MAXLEN, "worker_spi");
    worker.bgw_main_arg = Int32GetDatum(i);

    RegisterBackgroundWorker(&worker);
  }
}

   
                                     
   
Datum
worker_spi_launch(PG_FUNCTION_ARGS)
{
  int32 i = PG_GETARG_INT32(0);
  BackgroundWorker worker;
  BackgroundWorkerHandle *handle;
  BgwHandleStatus status;
  pid_t pid;

  memset(&worker, 0, sizeof(worker));
  worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
  worker.bgw_restart_time = BGW_NEVER_RESTART;
  sprintf(worker.bgw_library_name, "worker_spi");
  sprintf(worker.bgw_function_name, "worker_spi_main");
  snprintf(worker.bgw_name, BGW_MAXLEN, "worker_spi worker %d", i);
  snprintf(worker.bgw_type, BGW_MAXLEN, "worker_spi");
  worker.bgw_main_arg = Int32GetDatum(i);
                                                                            
  worker.bgw_notify_pid = MyProcPid;

  if (!RegisterDynamicBackgroundWorker(&worker, &handle))
  {
    PG_RETURN_NULL();
  }

  status = WaitForBackgroundWorkerStartup(handle, &pid);

  if (status == BGWH_STOPPED)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("could not start background process"), errhint("More details may be available in the server log.")));
  }
  if (status == BGWH_POSTMASTER_DIED)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_RESOURCES), errmsg("cannot start background processes without postmaster"), errhint("Kill all remaining database processes and restart the database.")));
  }
  Assert(status == BGWH_STARTED);

  PG_RETURN_INT32(pid);
}
