/*-------------------------------------------------------------------------
 *
 * pgstatfuncs.c
 *	  Functions for accessing the statistics collector data
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/pgstatfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xlog.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_type.h"
#include "common/ip.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker_internals.h"
#include "postmaster/postmaster.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/inet.h"
#include "utils/timestamp.h"

#define UINT32_ACCESS_ONCE(var) ((uint32)(*((volatile uint32 *)&(var))))

#define HAS_PGSTAT_PERMISSIONS(role) (is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_STATS) || has_privs_of_role(GetUserId(), role))

/* Global bgwriter statistics, from bgwriter.c */
extern PgStat_MsgBgWriter bgwriterStats;

Datum
pg_stat_get_numscans(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_tuples_returned(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_tuples_fetched(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_tuples_inserted(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_tuples_updated(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_tuples_deleted(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_tuples_hot_updated(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_live_tuples(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_dead_tuples(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_mod_since_analyze(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_blocks_fetched(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_blocks_hit(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_last_vacuum_time(PG_FUNCTION_ARGS)
{





















}

Datum
pg_stat_get_last_autovacuum_time(PG_FUNCTION_ARGS)
{





















}

Datum
pg_stat_get_last_analyze_time(PG_FUNCTION_ARGS)
{





















}

Datum
pg_stat_get_last_autoanalyze_time(PG_FUNCTION_ARGS)
{





















}

Datum
pg_stat_get_vacuum_count(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_autovacuum_count(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_analyze_count(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_autoanalyze_count(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_function_calls(PG_FUNCTION_ARGS)
{








}

Datum
pg_stat_get_function_total_time(PG_FUNCTION_ARGS)
{









}

Datum
pg_stat_get_function_self_time(PG_FUNCTION_ARGS)
{









}

Datum
pg_stat_get_backend_idset(PG_FUNCTION_ARGS)
{


































}

/*
 * Returns command progress information for the named command.
 */
Datum
pg_stat_get_progress_info(PG_FUNCTION_ARGS)
{

















































































































}

/*
 * Returns activity of PG backends.
 */
Datum
pg_stat_get_activity(PG_FUNCTION_ARGS)
{
#define PG_STAT_GET_ACTIVITY_COLS 29
  int num_backends = pgstat_fetch_stat_numbackends();
  int curr_backend;
  int pid = PG_ARGISNULL(0) ? -1 : PG_GETARG_INT32(0);
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;

  /* check to see if caller supports us returning a tuplestore */
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {

  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {

  }

  /* Build a tuple descriptor for our result type */
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {

  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  /* 1-based index */
  for (curr_backend = 1; curr_backend <= num_backends; curr_backend++)
  {
    /* for each row */
    Datum values[PG_STAT_GET_ACTIVITY_COLS];
    bool nulls[PG_STAT_GET_ACTIVITY_COLS];
    LocalPgBackendStatus *local_beentry;
    PgBackendStatus *beentry;
    PGPROC *proc;
    const char *wait_event_type = NULL;
    const char *wait_event = NULL;

    MemSet(values, 0, sizeof(values));
    MemSet(nulls, 0, sizeof(nulls));

    /* Get the next one in the list */
    local_beentry = pgstat_fetch_stat_local_beentry(curr_backend);
    if (!local_beentry)
    {
      int i;

      /* Ignore missing entries if looking for specific PID */















    }

    beentry = &local_beentry->backendStatus;

    /* If looking for specific PID, ignore all the others */
    if (pid != -1 && beentry->st_procpid != pid)
    {

    }

    /* Values available to all callers */
    if (beentry->st_databaseid != InvalidOid)
    {
      values[0] = ObjectIdGetDatum(beentry->st_databaseid);
    }
    else
    {
      nulls[0] = true;
    }

    values[1] = Int32GetDatum(beentry->st_procpid);

    if (beentry->st_userid != InvalidOid)
    {
      values[2] = ObjectIdGetDatum(beentry->st_userid);
    }
    else
    {
      nulls[2] = true;
    }

    if (beentry->st_appname)
    {
      values[3] = CStringGetTextDatum(beentry->st_appname);
    }
    else
    {

    }

    if (TransactionIdIsValid(local_beentry->backend_xid))
    {
      values[15] = TransactionIdGetDatum(local_beentry->backend_xid);
    }
    else
    {
      nulls[15] = true;
    }

    if (TransactionIdIsValid(local_beentry->backend_xmin))
    {
      values[16] = TransactionIdGetDatum(local_beentry->backend_xmin);
    }
    else
    {
      nulls[16] = true;
    }

    /* Values only available to role member or pg_read_all_stats */
    if (HAS_PGSTAT_PERMISSIONS(beentry->st_userid))
    {
      SockAddr zero_clientaddr;
      char *clipped_activity;

      switch (beentry->st_state)
      {
      case STATE_IDLE:;;
        values[4] = CStringGetTextDatum("idle");
        break;
      case STATE_RUNNING:;;
        values[4] = CStringGetTextDatum("active");
        break;
      case STATE_IDLEINTRANSACTION:;;
        values[4] = CStringGetTextDatum("idle in transaction");
        break;
      case STATE_FASTPATH:;;


      case STATE_IDLEINTRANSACTION_ABORTED:;;


      case STATE_DISABLED:;;


      case STATE_UNDEFINED:;;
        nulls[4] = true;
        break;
      }

      clipped_activity = pgstat_clip_activity(beentry->st_activity_raw);
      values[5] = CStringGetTextDatum(clipped_activity);
      pfree(clipped_activity);

      proc = BackendPidGetProc(beentry->st_procpid);
      if (proc != NULL)
      {
        uint32 raw_wait_event;

        raw_wait_event = UINT32_ACCESS_ONCE(proc->wait_event_info);
        wait_event_type = pgstat_get_wait_event_type(raw_wait_event);
        wait_event = pgstat_get_wait_event(raw_wait_event);
      }
      else if (beentry->st_backendType != B_BACKEND)
      {
        /*
         * For an auxiliary process, retrieve process info from
         * AuxiliaryProcs stored in shared-memory.
         */
        proc = AuxiliaryPidGetProc(beentry->st_procpid);

        if (proc != NULL)
        {
          uint32 raw_wait_event;

          raw_wait_event = UINT32_ACCESS_ONCE(proc->wait_event_info);
          wait_event_type = pgstat_get_wait_event_type(raw_wait_event);
          wait_event = pgstat_get_wait_event(raw_wait_event);
        }
      }

      if (wait_event_type)
      {
        values[6] = CStringGetTextDatum(wait_event_type);
      }
      else
      {
        nulls[6] = true;
      }

      if (wait_event)
      {
        values[7] = CStringGetTextDatum(wait_event);
      }
      else
      {
        nulls[7] = true;
      }

      /*
       * Don't expose transaction time for walsenders; it confuses
       * monitoring, particularly because we don't keep the time up-to-
       * date.
       */
      if (beentry->st_xact_start_timestamp != 0 && beentry->st_backendType != B_WAL_SENDER)
      {
        values[8] = TimestampTzGetDatum(beentry->st_xact_start_timestamp);
      }
      else
      {
        nulls[8] = true;
      }

      if (beentry->st_activity_start_timestamp != 0)
      {
        values[9] = TimestampTzGetDatum(beentry->st_activity_start_timestamp);
      }
      else
      {
        nulls[9] = true;
      }

      if (beentry->st_proc_start_timestamp != 0)
      {
        values[10] = TimestampTzGetDatum(beentry->st_proc_start_timestamp);
      }
      else
      {

      }

      if (beentry->st_state_start_timestamp != 0)
      {
        values[11] = TimestampTzGetDatum(beentry->st_state_start_timestamp);
      }
      else
      {
        nulls[11] = true;
      }

      /* A zeroed client addr means we don't know */
      memset(&zero_clientaddr, 0, sizeof(zero_clientaddr));
      if (memcmp(&(beentry->st_clientaddr), &zero_clientaddr, sizeof(zero_clientaddr)) == 0)
      {
        nulls[12] = true;
        nulls[13] = true;
        nulls[14] = true;
      }
      else
      {
        if (beentry->st_clientaddr.addr.ss_family == AF_INET
#ifdef HAVE_IPV6
            || beentry->st_clientaddr.addr.ss_family == AF_INET6
#endif
        )
        {
          char remote_host[NI_MAXHOST];
          char remote_port[NI_MAXSERV];
          int ret;
























        }
        else if (beentry->st_clientaddr.addr.ss_family == AF_UNIX)
        {
          /*
           * Unix sockets always reports NULL for host and -1 for
           * port, so it's possible to tell the difference to
           * connections we have no permissions to view, or with
           * errors.
           */
          nulls[12] = true;
          nulls[13] = true;
          values[14] = Int32GetDatum(-1);
        }
        else
        {
          /* Unknown address type, should never happen */



        }
      }
      /* Add backend type */
      if (beentry->st_backendType == B_BG_WORKER)
      {
        const char *bgw_type;

        bgw_type = GetBackgroundWorkerTypeByPid(beentry->st_procpid);
        if (bgw_type)
        {
          values[17] = CStringGetTextDatum(bgw_type);
        }
        else
        {

        }
      }
      else
      {
        values[17] = CStringGetTextDatum(pgstat_get_backend_desc(beentry->st_backendType));
      }

      /* SSL information */
      if (beentry->st_ssl)
      {
































      }
      else
      {
        values[18] = BoolGetDatum(false); /* ssl */
        nulls[19] = nulls[20] = nulls[21] = nulls[22] = nulls[23] = nulls[24] = nulls[25] = true;
      }

      /* GSSAPI information */
      if (beentry->st_gss)
      {



      }
      else
      {
        values[26] = BoolGetDatum(false); /* gss_auth */
        nulls[27] = true;                 /* No GSS principal */
        values[28] = BoolGetDatum(false); /* GSS Encryption not in
                                           * use */
      }
    }
    else
    {
      /* No permissions to view data about this session */























    }

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);

    /* If only a single backend was requested, and we found it, break. */
    if (pid != -1)
    {

    }
  }

  /* clean up and return the tuplestore */
  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

Datum
pg_backend_pid(PG_FUNCTION_ARGS)
{
  PG_RETURN_INT32(MyProcPid);
}

Datum
pg_stat_get_backend_pid(PG_FUNCTION_ARGS)
{









}

Datum
pg_stat_get_backend_dbid(PG_FUNCTION_ARGS)
{









}

Datum
pg_stat_get_backend_userid(PG_FUNCTION_ARGS)
{









}

Datum
pg_stat_get_backend_activity(PG_FUNCTION_ARGS)
{




























}

Datum
pg_stat_get_backend_wait_event_type(PG_FUNCTION_ARGS)
{
























}

Datum
pg_stat_get_backend_wait_event(PG_FUNCTION_ARGS)
{
























}

Datum
pg_stat_get_backend_activity_start(PG_FUNCTION_ARGS)
{


























}

Datum
pg_stat_get_backend_xact_start(PG_FUNCTION_ARGS)
{






















}

Datum
pg_stat_get_backend_start(PG_FUNCTION_ARGS)
{






















}

Datum
pg_stat_get_backend_client_addr(PG_FUNCTION_ARGS)
{












































}

Datum
pg_stat_get_backend_client_port(PG_FUNCTION_ARGS)
{












































}

Datum
pg_stat_get_db_numbackends(PG_FUNCTION_ARGS)
{

















}

Datum
pg_stat_get_db_xact_commit(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_xact_rollback(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_blocks_fetched(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_blocks_hit(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_tuples_returned(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_tuples_fetched(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_tuples_inserted(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_tuples_updated(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_tuples_deleted(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_stat_reset_time(PG_FUNCTION_ARGS)
{





















}

Datum
pg_stat_get_db_temp_files(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_temp_bytes(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_conflict_tablespace(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_conflict_lock(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_conflict_snapshot(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_conflict_bufferpin(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_conflict_startup_deadlock(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_conflict_all(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_deadlocks(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_db_checksum_failures(PG_FUNCTION_ARGS)
{



















}

Datum
pg_stat_get_db_checksum_last_failure(PG_FUNCTION_ARGS)
{


























}

Datum
pg_stat_get_db_blk_read_time(PG_FUNCTION_ARGS)
{















}

Datum
pg_stat_get_db_blk_write_time(PG_FUNCTION_ARGS)
{















}

Datum
pg_stat_get_bgwriter_timed_checkpoints(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_bgwriter_requested_checkpoints(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_bgwriter_buf_written_checkpoints(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_bgwriter_buf_written_clean(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_bgwriter_maxwritten_clean(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_checkpoint_write_time(PG_FUNCTION_ARGS)
{


}

Datum
pg_stat_get_checkpoint_sync_time(PG_FUNCTION_ARGS)
{


}

Datum
pg_stat_get_bgwriter_stat_reset_time(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_buf_written_backend(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_buf_fsync_backend(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_buf_alloc(PG_FUNCTION_ARGS)
{

}

Datum
pg_stat_get_xact_numscans(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_xact_tuples_returned(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_xact_tuples_fetched(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_xact_tuples_inserted(PG_FUNCTION_ARGS)
{




















}

Datum
pg_stat_get_xact_tuples_updated(PG_FUNCTION_ARGS)
{




















}

Datum
pg_stat_get_xact_tuples_deleted(PG_FUNCTION_ARGS)
{




















}

Datum
pg_stat_get_xact_tuples_hot_updated(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_xact_blocks_fetched(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_xact_blocks_hit(PG_FUNCTION_ARGS)
{














}

Datum
pg_stat_get_xact_function_calls(PG_FUNCTION_ARGS)
{








}

Datum
pg_stat_get_xact_function_total_time(PG_FUNCTION_ARGS)
{








}

Datum
pg_stat_get_xact_function_self_time(PG_FUNCTION_ARGS)
{








}

/* Get the timestamp of the current statistics snapshot */
Datum
pg_stat_get_snapshot_timestamp(PG_FUNCTION_ARGS)
{

}

/* Discard the active statistics snapshot */
Datum
pg_stat_clear_snapshot(PG_FUNCTION_ARGS)
{



}

/* Reset all counters for the current database */
Datum
pg_stat_reset(PG_FUNCTION_ARGS)
{



}

/* Reset some shared cluster-wide counters */
Datum
pg_stat_reset_shared(PG_FUNCTION_ARGS)
{





}

/* Reset a single counter in the current database */
Datum
pg_stat_reset_single_table_counters(PG_FUNCTION_ARGS)
{





}

Datum
pg_stat_reset_single_function_counters(PG_FUNCTION_ARGS)
{





}

Datum
pg_stat_get_archiver(PG_FUNCTION_ARGS)
{










































































}