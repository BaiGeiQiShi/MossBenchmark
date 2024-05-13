/*-------------------------------------------------------------------------
 *
 * xlogfuncs.c
 *
 * PostgreSQL write-ahead log manager user interface functions
 *
 * This file contains WAL control and information functions.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/xlogfuncs.c
 *
 *-------------------------------------------------------------------------
 */
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

/*
 * Store label file and tablespace map during non-exclusive backups.
 */
static StringInfo label_file;
static StringInfo tblspc_map_file;

/*
 * pg_start_backup: set up for taking an on-line backup dump
 *
 * Essentially what this does is to create a backup label file in $PGDATA,
 * where it will be archived as part of the backup dump.  The label file
 * contains the user-supplied label string (typically this would be used
 * to tell where the backup dump will be stored) and the starting time and
 * starting WAL location for the dump.
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_start_backup(PG_FUNCTION_ARGS)
{





































}

/*
 * pg_stop_backup: finish taking an on-line backup dump
 *
 * We write an end-of-backup WAL record, and remove the backup label file
 * created by pg_start_backup, creating a backup history file in pg_wal
 * instead (whence it will immediately be archived). The backup history file
 * contains the same info found in the label file, plus the backup-end time
 * and WAL location. Before 9.0, the backup-end time was read from the backup
 * history file at the beginning of archive recovery, but we now use the WAL
 * record for that and the file is for informational and debug purposes only.
 *
 * Note: different from CancelBackup which just cancels online backup mode.
 *
 * Note: this version is only called to stop an exclusive backup. The function
 *		 pg_stop_backup_v2 (overloaded as pg_stop_backup in SQL) is
 *called to stop non-exclusive backups.
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_stop_backup(PG_FUNCTION_ARGS)
{


















}

/*
 * pg_stop_backup_v2: finish taking exclusive or nonexclusive on-line backup.
 *
 * Works the same as pg_stop_backup, except for non-exclusive backups it returns
 * the backup label and tablespace map files as text fields in as part of the
 * resultset.
 *
 * The first parameter (variable 'exclusive') allows the user to tell us if
 * this is an exclusive or a non-exclusive backup.
 *
 * The second parameter (variable 'waitforarchive'), which is optional,
 * allows the user to choose if they want to wait for the WAL to be archived
 * or if we should just return as soon as the WAL record is written.
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_stop_backup_v2(PG_FUNCTION_ARGS)
{


























































































}

/*
 * pg_switch_wal: switch to next xlog file
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_switch_wal(PG_FUNCTION_ARGS)
{













}

/*
 * pg_create_restore_point: a named point for restore
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_create_restore_point(PG_FUNCTION_ARGS)
{



























}

/*
 * Report the current WAL write location (same format as pg_start_backup etc)
 *
 * This is useful for determining how much of WAL is visible to an external
 * archiving process.  Note that the data before this point is written out
 * to the kernel, but is not necessarily synced to disk.
 */
Datum
pg_current_wal_lsn(PG_FUNCTION_ARGS)
{










}

/*
 * Report the current WAL insert location (same format as pg_start_backup etc)
 *
 * This function is mostly for debugging purposes.
 */
Datum
pg_current_wal_insert_lsn(PG_FUNCTION_ARGS)
{










}

/*
 * Report the current WAL flush location (same format as pg_start_backup etc)
 *
 * This function is mostly for debugging purposes.
 */
Datum
pg_current_wal_flush_lsn(PG_FUNCTION_ARGS)
{










}

/*
 * Report the last WAL receive location (same format as pg_start_backup etc)
 *
 * This is useful for determining how much of WAL is guaranteed to be received
 * and synced to disk by walreceiver.
 */
Datum
pg_last_wal_receive_lsn(PG_FUNCTION_ARGS)
{










}

/*
 * Report the last WAL replay location (same format as pg_start_backup etc)
 *
 * This is useful for determining how much of WAL is visible to read-only
 * connections during recovery.
 */
Datum
pg_last_wal_replay_lsn(PG_FUNCTION_ARGS)
{










}

/*
 * Compute an xlog file name and decimal byte offset given a WAL location,
 * such as is returned by pg_stop_backup() or pg_switch_wal().
 *
 * Note that a location exactly at a segment boundary is taken to be in
 * the previous segment.  This is usually the right thing, since the
 * expected usage is to determine which xlog file(s) are ready to archive.
 */
Datum
pg_walfile_name_offset(PG_FUNCTION_ARGS)
{


















































}

/*
 * Compute an xlog file name given a WAL location,
 * such as is returned by pg_stop_backup() or pg_switch_wal().
 */
Datum
pg_walfile_name(PG_FUNCTION_ARGS)
{













}

/*
 * pg_wal_replay_pause - pause recovery now
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_wal_replay_pause(PG_FUNCTION_ARGS)
{








}

/*
 * pg_wal_replay_resume - resume recovery now
 *
 * Permission checking for this function is managed through the normal
 * GRANT system.
 */
Datum
pg_wal_replay_resume(PG_FUNCTION_ARGS)
{








}

/*
 * pg_is_wal_replay_paused
 */
Datum
pg_is_wal_replay_paused(PG_FUNCTION_ARGS)
{






}

/*
 * Returns timestamp of latest processed commit/abort record.
 *
 * When the server has been started normally without recovery the function
 * returns NULL.
 */
Datum
pg_last_xact_replay_timestamp(PG_FUNCTION_ARGS)
{









}

/*
 * Returns bool with current recovery mode, a global state.
 */
Datum
pg_is_in_recovery(PG_FUNCTION_ARGS)
{

}

/*
 * Compute the difference in bytes between two WAL locations.
 */
Datum
pg_wal_lsn_diff(PG_FUNCTION_ARGS)
{





}

/*
 * Returns bool with current on-line backup mode, a global state.
 */
Datum
pg_is_in_backup(PG_FUNCTION_ARGS)
{

}

/*
 * Returns start time of an online exclusive backup.
 *
 * When there's no exclusive backup in progress, the function
 * returns NULL.
 */
Datum
pg_backup_start_time(PG_FUNCTION_ARGS)
{





















































}

/*
 * Promotes a standby server.
 *
 * A result of "true" means that promotion has been completed if "wait" is
 * "true", or initiated if "wait" is false.
 */
Datum
pg_promote(PG_FUNCTION_ARGS)
{






































































}
