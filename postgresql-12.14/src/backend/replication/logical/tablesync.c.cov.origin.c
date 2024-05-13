/*-------------------------------------------------------------------------
 * tablesync.c
 *	  PostgreSQL logical replication
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/tablesync.c
 *
 * NOTES
 *	  This file contains code for initial table data synchronization for
 *	  logical replication.
 *
 *	  The initial data synchronization is done separately for each table,
 *	  in a separate apply worker that only fetches the initial snapshot data
 *	  from the publisher and then synchronizes the position in the stream
 *with the main apply worker.
 *
 *	  There are several reasons for doing the synchronization this way:
 *	   - It allows us to parallelize the initial data synchronization
 *		 which lowers the time needed for it to happen.
 *	   - The initial synchronization does not have to hold the xid and LSN
 *		 for the time it takes to copy data of all tables, causing less
 *		 bloat and lower disk consumption compared to doing the
 *		 synchronization in a single process for the whole database.
 *	   - It allows us to synchronize any tables added after the initial
 *		 synchronization has finished.
 *
 *	  The stream position synchronization works in multiple steps.
 *	   - Sync finishes copy and sets worker state as SYNCWAIT and waits for
 *		 state to change in a loop.
 *	   - Apply periodically checks tables that are synchronizing for
 *SYNCWAIT. When the desired state appears, it will set the worker state to
 *		 CATCHUP and starts loop-waiting until either the table state is
 *set to SYNCDONE or the sync worker exits.
 *	   - After the sync worker has seen the state change to CATCHUP, it will
 *		 read the stream and apply changes (acting like an apply worker)
 *until it catches up to the specified stream position.  Then it sets the state
 *to SYNCDONE.  There might be zero changes applied between CATCHUP and
 *SYNCDONE, because the sync worker might be ahead of the apply worker.
 *	   - Once the state was set to SYNCDONE, the apply will continue
 *tracking the table until it reaches the SYNCDONE stream position, at which
 *		 point it sets state to READY and stops tracking.  Again, there
 *might be zero changes in between.
 *
 *	  So the state progression is always: INIT -> DATASYNC -> SYNCWAIT ->
 *CATCHUP -> SYNCDONE -> READY.
 *
 *	  The catalog pg_subscription_rel is used to keep information about
 *	  subscribed tables and their state.  Some transient state during data
 *	  synchronization is kept in shared memory.  The states SYNCWAIT and
 *	  CATCHUP only appear in memory.
 *
 *	  Example flows look like this:
 *	   - Apply is in front:
 *		  sync:8
 *			-> set in memory SYNCWAIT
 *		  apply:10
 *			-> set in memory CATCHUP
 *			-> enter wait-loop
 *		  sync:10
 *			-> set in catalog SYNCDONE
 *			-> exit
 *		  apply:10
 *			-> exit wait-loop
 *			-> continue rep
 *		  apply:11
 *			-> set in catalog READY
 *	   - Sync in front:
 *		  sync:10
 *			-> set in memory SYNCWAIT
 *		  apply:8
 *			-> set in memory CATCHUP
 *			-> continue per-table filtering
 *		  sync:10
 *			-> set in catalog SYNCDONE
 *			-> exit
 *		  apply:10
 *			-> set in catalog READY
 *			-> stop per-table filtering
 *			-> continue rep
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"

#include "access/table.h"
#include "access/xact.h"

#include "catalog/pg_subscription_rel.h"
#include "catalog/pg_type.h"

#include "commands/copy.h"

#include "parser/parse_relation.h"

#include "replication/logicallauncher.h"
#include "replication/logicalrelation.h"
#include "replication/walreceiver.h"
#include "replication/worker_internal.h"

#include "utils/snapmgr.h"
#include "storage/ipc.h"

#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static bool table_states_valid = false;

StringInfo copybuf = NULL;

/*
 * Exit routine for synchronization worker.
 */
static void
pg_attribute_noreturn() finish_sync_worker(void)
{






















}

/*
 * Wait until the relation synchronization state is set in the catalog to the
 * expected one.
 *
 * Used when transitioning from CATCHUP state to SYNCDONE.
 *
 * Returns false if the synchronization worker has disappeared or the table
 * state has been reset.
 */
static bool
wait_for_relation_state_change(Oid relid, char expected_state)
{









































}

/*
 * Wait until the apply worker changes the state of our synchronization
 * worker to the expected one.
 *
 * Used when transitioning from SYNCWAIT state to CATCHUP.
 *
 * Returns false if the apply worker has disappeared.
 */
static bool
wait_for_worker_state_change(char expected_state)
{














































}

/*
 * Callback from syscache invalidation.
 */
void
invalidate_syncing_table_states(Datum arg, int cacheid, uint32 hashvalue)
{

}

/*
 * Handle table synchronization cooperation from the synchronization
 * worker.
 *
 * If the sync worker is in CATCHUP state and reached (or passed) the
 * predetermined synchronization point in the WAL stream, mark the table as
 * SYNCDONE and finish.
 */
static void
process_syncing_tables_for_sync(XLogRecPtr current_lsn)
{






















}

/*
 * Handle table synchronization cooperation from the apply worker.
 *
 * Walk over all subscription tables that are individually tracked by the
 * apply process (currently, all that have state other than
 * SUBREL_STATE_READY) and manage synchronization for them.
 *
 * If there are tables that need synchronizing and are not being synchronized
 * yet, start sync workers for them (if there are free slots for sync
 * workers).  To prevent starting the sync worker for the same relation at a
 * high frequency after a failure, we store its last start time with each sync
 * state info.  We start the sync worker for the same relation after waiting
 * at least wal_retrieve_retry_interval.
 *
 * For tables that are being synchronized already, check if sync workers
 * either need action from the apply worker or have finished.  This is the
 * SYNCWAIT to CATCHUP transition.
 *
 * If the synchronization position is reached (SYNCDONE), then the table can
 * be marked as READY and is no longer tracked.
 */
static void
process_syncing_tables_for_apply(XLogRecPtr current_lsn)
{































































































































































































}

/*
 * Process possible state change(s) of tables that are being synchronized.
 */
void
process_syncing_tables(XLogRecPtr current_lsn)
{








}

/*
 * Create list of columns for COPY based on logical relation mapping.
 */
static List *
make_copy_attnamelist(LogicalRepRelMapEntry *rel)
{









}

/*
 * Data source callback for the COPY FROM, which reads from the remote
 * connection and passes the data back to our local COPY.
 */
static int
copy_read_data(void *outbuf, int minread, int maxread)
{








































































}

/*
 * Get information about remote relation in similar fashion the RELATION
 * message provides during replication.
 */
static void
fetch_remote_table_info(char *nspname, char *relname, LogicalRepRelation *lrel)
{


























































































}

/*
 * Copy existing data of a table from publisher.
 *
 * Caller is responsible for locking the local relation.
 */
static void
copy_table(Relation rel)
{









































}

/*
 * Start syncing the table in the sync worker.
 *
 * The returned slot name is palloc'ed in current memory context.
 */
char *
LogicalRepSyncTableStart(XLogRecPtr *origin_startpos)
{




























































































































































}
