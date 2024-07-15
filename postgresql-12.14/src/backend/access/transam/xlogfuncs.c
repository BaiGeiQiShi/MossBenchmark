                                                                            
   
               
   
                                                               
   
                                                             
   
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "access/htup_details.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xlogutils.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/walreceiver.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/numeric.h"
#include "utils/guc.h"
#include "utils/pg_lsn.h"
#include "utils/timestamp.h"
#include "utils/tuplestore.h"
#include "storage/fd.h"
#include "storage/ipc.h"

   
                                                                     
   
static StringInfo label_file;
static StringInfo tblspc_map_file;

   
                                                             
   
                                                                           
                                                                         
                                                                         
                                                                           
                                       
   
                                                                       
                 
   
Datum
pg_start_backup(PG_FUNCTION_ARGS)
{
  text *backupid = PG_GETARG_TEXT_PP(0);
  bool fast = PG_GETARG_BOOL(1);
  bool exclusive = PG_GETARG_BOOL(2);
  char *backupidstr;
  XLogRecPtr startpoint;
  SessionBackupState status = get_backup_status();

  backupidstr = text_to_cstring(backupid);

  if (status == SESSION_BACKUP_NON_EXCLUSIVE)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("a backup is already in progress in this session")));
  }

  if (exclusive)
  {
    startpoint = do_pg_start_backup(backupidstr, fast, NULL, NULL, NULL, NULL, false, true);
  }
  else
  {
    MemoryContext oldcontext;

       
                                                                       
                                        
       
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    label_file = makeStringInfo();
    tblspc_map_file = makeStringInfo();
    MemoryContextSwitchTo(oldcontext);

    register_persistent_abort_backup_handler();

    startpoint = do_pg_start_backup(backupidstr, fast, NULL, label_file, NULL, tblspc_map_file, false, true);
  }

  PG_RETURN_LSN(startpoint);
}

   
                                                        
   
                                                                          
                                                                        
                                                                             
                                                                            
                                                                              
                                                                             
                                                                              
   
                                                                            
   
                                                                               
                                                                          
                                 
   
                                                                       
                 
   
Datum
pg_stop_backup(PG_FUNCTION_ARGS)
{
  XLogRecPtr stoppoint;
  SessionBackupState status = get_backup_status();

  if (status == SESSION_BACKUP_NON_EXCLUSIVE)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("non-exclusive backup in progress"), errhint("Did you mean to use pg_stop_backup('f')?")));
  }

     
                                                                            
                                                         
                                                                            
                                                           
                        
     
  stoppoint = do_pg_stop_backup(NULL, true, NULL);

  PG_RETURN_LSN(stoppoint);
}

   
                                                                              
   
                                                                                 
                                                                              
              
   
                                                                            
                                                   
   
                                                                        
                                                                             
                                                                     
   
                                                                       
                 
   
Datum
pg_stop_backup_v2(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  Datum values[3];
  bool nulls[3];

  bool exclusive = PG_GETARG_BOOL(0);
  bool waitforarchive = PG_GETARG_BOOL(1);
  XLogRecPtr stoppoint;
  SessionBackupState status = get_backup_status();

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not "
                                                                   "allowed in this context")));
  }

                                                    
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  MemSet(values, 0, sizeof(values));
  MemSet(nulls, 0, sizeof(nulls));

  if (exclusive)
  {
    if (status == SESSION_BACKUP_NON_EXCLUSIVE)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("non-exclusive backup in progress"), errhint("Did you mean to use pg_stop_backup('f')?")));
    }

       
                                                                         
                                                             
       
    stoppoint = do_pg_stop_backup(NULL, waitforarchive, NULL);

    nulls[1] = true;
    nulls[2] = true;
  }
  else
  {
    if (status != SESSION_BACKUP_NON_EXCLUSIVE)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("non-exclusive backup is not in progress"), errhint("Did you mean to use pg_stop_backup('t')?")));
    }

       
                                                                        
                                                                        
       
    stoppoint = do_pg_stop_backup(label_file->data, waitforarchive, NULL);

    values[1] = CStringGetTextDatum(label_file->data);
    values[2] = CStringGetTextDatum(tblspc_map_file->data);

                                                       
    pfree(label_file->data);
    pfree(label_file);
    label_file = NULL;
    pfree(tblspc_map_file->data);
    pfree(tblspc_map_file);
    tblspc_map_file = NULL;
  }

                                                                        
  values[0] = LSNGetDatum(stoppoint);

  tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  tuplestore_donestoring(typstore);

  return (Datum)0;
}

   
                                           
   
                                                                       
                 
   
Datum
pg_switch_wal(PG_FUNCTION_ARGS)
{
  XLogRecPtr switchpoint;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery.")));
  }

  switchpoint = RequestXLogSwitch(false);

     
                                                                    
     
  PG_RETURN_LSN(switchpoint);
}

   
                                                      
   
                                                                       
                 
   
Datum
pg_create_restore_point(PG_FUNCTION_ARGS)
{
  text *restore_name = PG_GETARG_TEXT_PP(0);
  char *restore_name_str;
  XLogRecPtr restorepoint;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), (errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery."))));
  }

  if (!XLogIsNeeded())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("WAL level not sufficient for creating a restore point"), errhint("wal_level must be set to \"replica\" or \"logical\" at server start.")));
  }

  restore_name_str = text_to_cstring(restore_name);

  if (strlen(restore_name_str) >= MAXFNAMELEN)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("value too long for restore point (maximum %d characters)", MAXFNAMELEN - 1)));
  }

  restorepoint = XLogRestorePoint(restore_name_str);

     
                                                                           
     
  PG_RETURN_LSN(restorepoint);
}

   
                                                                              
   
                                                                            
                                                                           
                                                         
   
Datum
pg_current_wal_lsn(PG_FUNCTION_ARGS)
{
  XLogRecPtr current_recptr;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery.")));
  }

  current_recptr = GetXLogWriteRecPtr();

  PG_RETURN_LSN(current_recptr);
}

   
                                                                               
   
                                                   
   
Datum
pg_current_wal_insert_lsn(PG_FUNCTION_ARGS)
{
  XLogRecPtr current_recptr;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery.")));
  }

  current_recptr = GetXLogInsertRecPtr();

  PG_RETURN_LSN(current_recptr);
}

   
                                                                              
   
                                                   
   
Datum
pg_current_wal_flush_lsn(PG_FUNCTION_ARGS)
{
  XLogRecPtr current_recptr;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("WAL control functions cannot be executed during recovery.")));
  }

  current_recptr = GetFlushRecPtr();

  PG_RETURN_LSN(current_recptr);
}

   
                                                                             
   
                                                                               
                                      
   
Datum
pg_last_wal_receive_lsn(PG_FUNCTION_ARGS)
{
  XLogRecPtr recptr;

  recptr = GetWalRcvWriteRecPtr(NULL, NULL);

  if (recptr == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_LSN(recptr);
}

   
                                                                            
   
                                                                          
                                
   
Datum
pg_last_wal_replay_lsn(PG_FUNCTION_ARGS)
{
  XLogRecPtr recptr;

  recptr = GetXLogReplayRecPtr(NULL);

  if (recptr == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_LSN(recptr);
}

   
                                                                           
                                                               
   
                                                                        
                                                                     
                                                                           
   
Datum
pg_walfile_name_offset(PG_FUNCTION_ARGS)
{
  XLogSegNo xlogsegno;
  uint32 xrecoff;
  XLogRecPtr locationpoint = PG_GETARG_LSN(0);
  char xlogfilename[MAXFNAMELEN];
  Datum values[2];
  bool isnull[2];
  TupleDesc resultTupleDesc;
  HeapTuple resultHeapTuple;
  Datum result;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("%s cannot be executed during recovery.", "pg_walfile_name_offset()")));
  }

     
                                                                            
                               
     
  resultTupleDesc = CreateTemplateTupleDesc(2);
  TupleDescInitEntry(resultTupleDesc, (AttrNumber)1, "file_name", TEXTOID, -1, 0);
  TupleDescInitEntry(resultTupleDesc, (AttrNumber)2, "file_offset", INT4OID, -1, 0);

  resultTupleDesc = BlessTupleDesc(resultTupleDesc);

     
                  
     
  XLByteToPrevSeg(locationpoint, xlogsegno, wal_segment_size);
  XLogFileName(xlogfilename, ThisTimeLineID, xlogsegno, wal_segment_size);

  values[0] = CStringGetTextDatum(xlogfilename);
  isnull[0] = false;

     
            
     
  xrecoff = XLogSegmentOffset(locationpoint, wal_segment_size);

  values[1] = UInt32GetDatum(xrecoff);
  isnull[1] = false;

     
                                                                        
     
  resultHeapTuple = heap_form_tuple(resultTupleDesc, values, isnull);

  result = HeapTupleGetDatum(resultHeapTuple);

  PG_RETURN_DATUM(result);
}

   
                                                   
                                                               
   
Datum
pg_walfile_name(PG_FUNCTION_ARGS)
{
  XLogSegNo xlogsegno;
  XLogRecPtr locationpoint = PG_GETARG_LSN(0);
  char xlogfilename[MAXFNAMELEN];

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("%s cannot be executed during recovery.", "pg_walfile_name()")));
  }

  XLByteToPrevSeg(locationpoint, xlogsegno, wal_segment_size);
  XLogFileName(xlogfilename, ThisTimeLineID, xlogsegno, wal_segment_size);

  PG_RETURN_TEXT_P(cstring_to_text(xlogfilename));
}

   
                                            
   
                                                                       
                 
   
Datum
pg_wal_replay_pause(PG_FUNCTION_ARGS)
{
  if (!RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is not in progress"), errhint("Recovery control functions can only be executed during recovery.")));
  }

  SetRecoveryPause(true);

  PG_RETURN_VOID();
}

   
                                              
   
                                                                       
                 
   
Datum
pg_wal_replay_resume(PG_FUNCTION_ARGS)
{
  if (!RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is not in progress"), errhint("Recovery control functions can only be executed during recovery.")));
  }

  SetRecoveryPause(false);

  PG_RETURN_VOID();
}

   
                           
   
Datum
pg_is_wal_replay_paused(PG_FUNCTION_ARGS)
{
  if (!RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is not in progress"), errhint("Recovery control functions can only be executed during recovery.")));
  }

  PG_RETURN_BOOL(RecoveryIsPaused());
}

   
                                                              
   
                                                                           
                 
   
Datum
pg_last_xact_replay_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz xtime;

  xtime = GetLatestXTime();
  if (xtime == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TIMESTAMPTZ(xtime);
}

   
                                                            
   
Datum
pg_is_in_recovery(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(RecoveryInProgress());
}

   
                                                              
   
Datum
pg_wal_lsn_diff(PG_FUNCTION_ARGS)
{
  Datum result;

  result = DirectFunctionCall2(pg_lsn_mi, PG_GETARG_DATUM(0), PG_GETARG_DATUM(1));

  PG_RETURN_NUMERIC(result);
}

   
                                                                  
   
Datum
pg_is_in_backup(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(BackupInProgress());
}

   
                                                     
   
                                                              
                 
   
Datum
pg_backup_start_time(PG_FUNCTION_ARGS)
{
  Datum xtime;
  FILE *lfp;
  char fline[MAXPGPATH];
  char backup_start_time[30];

     
                                  
     
  lfp = AllocateFile(BACKUP_LABEL_FILE, "r");
  if (lfp == NULL)
  {
    if (errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", BACKUP_LABEL_FILE)));
    }
    PG_RETURN_NULL();
  }

     
                                                 
     
  backup_start_time[0] = '\0';
  while (fgets(fline, sizeof(fline), lfp) != NULL)
  {
    if (sscanf(fline, "START TIME: %25[^\n]\n", backup_start_time) == 1)
    {
      break;
    }
  }

                               
  if (ferror(lfp))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", BACKUP_LABEL_FILE)));
  }

                                    
  if (FreeFile(lfp))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", BACKUP_LABEL_FILE)));
  }

  if (strlen(backup_start_time) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
  }

     
                                                                 
     
  xtime = DirectFunctionCall3(timestamptz_in, CStringGetDatum(backup_start_time), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));

  PG_RETURN_DATUM(xtime);
}

   
                              
   
                                                                           
                                            
   
Datum
pg_promote(PG_FUNCTION_ARGS)
{
  bool wait = PG_GETARG_BOOL(0);
  int wait_seconds = PG_GETARG_INT32(1);
  FILE *promote_file;
  int i;

  if (!RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is not in progress"), errhint("Recovery control functions can only be executed during recovery.")));
  }

  if (wait_seconds <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("\"wait_seconds\" must not be negative or zero")));
  }

                                      
  promote_file = AllocateFile(PROMOTE_SIGNAL_FILE, "w");
  if (!promote_file)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", PROMOTE_SIGNAL_FILE)));
  }

  if (FreeFile(promote_file))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", PROMOTE_SIGNAL_FILE)));
  }

                             
  if (kill(PostmasterPid, SIGUSR1) != 0)
  {
    ereport(WARNING, (errmsg("failed to send signal to postmaster: %m")));
    (void)unlink(PROMOTE_SIGNAL_FILE);
    PG_RETURN_BOOL(false);
  }

                                                       
  if (!wait)
  {
    PG_RETURN_BOOL(true);
  }

                                                          
#define WAITS_PER_SECOND 10
  for (i = 0; i < WAITS_PER_SECOND * wait_seconds; i++)
  {
    int rc;

    ResetLatch(MyLatch);

    if (!RecoveryInProgress())
    {
      PG_RETURN_BOOL(true);
    }

    CHECK_FOR_INTERRUPTS();

    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, 1000L / WAITS_PER_SECOND, WAIT_EVENT_PROMOTE);

       
                                                                       
                                                                
       
    if (rc & WL_POSTMASTER_DEATH)
    {
      PG_RETURN_BOOL(false);
    }
  }

  ereport(WARNING, (errmsg("server did not promote within %d seconds", wait_seconds)));
  PG_RETURN_BOOL(false);
}
