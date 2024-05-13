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


















































































































































































































































































































































































































































}

Datum
pg_backend_pid(PG_FUNCTION_ARGS)
{

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