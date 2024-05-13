/* ----------
 * pgstat.c
 *
 *	All the statistics collector stuff hacked up in one big, ugly file.
 *
 *	TODO:	- Separate collector, postmaster and backend stuff
 *			  into different files.
 *
 *			- Add some automatic call for pgstat vacuuming.
 *
 *			- Add a pgstat config column to pg_database, so this
 *			  entire thing can be enabled/disabled on a per db
 *basis.
 *
 *	Copyright (c) 2001-2019, PostgreSQL Global Development Group
 *
 *	src/backend/postmaster/pgstat.c
 * ----------
 */
#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "pgstat.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "catalog/pg_database.h"
#include "catalog/pg_proc.h"
#include "common/ip.h"
#include "libpq/libpq.h"
#include "libpq/pqsignal.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "postmaster/autovacuum.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "replication/walsender.h"
#include "storage/backendid.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lmgr.h"
#include "storage/pg_shmem.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "utils/ascii.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

/* ----------
 * Timer definitions.
 * ----------
 */
#define PGSTAT_STAT_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  500 /* Minimum time between stats file                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
       * updates; in milliseconds. */

#define PGSTAT_RETRY_DELAY                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  10 /* How long to wait between checks for a                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      * new file; in milliseconds. */

#define PGSTAT_MAX_WAIT_TIME                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  10000 /* Maximum time to wait for a stats                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
         * file update; in milliseconds. */

#define PGSTAT_INQ_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  640 /* How often to ping the collector for a                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
       * new file; in milliseconds. */

#define PGSTAT_RESTART_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  60 /* How often to attempt to restart a                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      * failed statistics collector; in                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      * seconds. */

#define PGSTAT_POLL_LOOP_COUNT (PGSTAT_MAX_WAIT_TIME / PGSTAT_RETRY_DELAY)
#define PGSTAT_INQ_LOOP_COUNT (PGSTAT_INQ_INTERVAL / PGSTAT_RETRY_DELAY)

/* Minimum receive buffer size for the collector's socket. */
#define PGSTAT_MIN_RCVBUF (100 * 1024)

/* ----------
 * The initial size hints for the hash tables used in the collector.
 * ----------
 */
#define PGSTAT_DB_HASH_SIZE 16
#define PGSTAT_TAB_HASH_SIZE 512
#define PGSTAT_FUNCTION_HASH_SIZE 512

/* ----------
 * Total number of backends including auxiliary
 *
 * We reserve a slot for each possible BackendId, plus one for each
 * possible auxiliary process type.  (This scheme assumes there is not
 * more than one of any auxiliary process type at a time.) MaxBackends
 * includes autovacuum workers and background workers as well.
 * ----------
 */
#define NumBackendStatSlots (MaxBackends + NUM_AUXPROCTYPES)

/* ----------
 * GUC parameters
 * ----------
 */
bool pgstat_track_activities = false;
bool pgstat_track_counts = false;
int pgstat_track_functions = TRACK_FUNC_OFF;
int pgstat_track_activity_query_size = 1024;

/* ----------
 * Built from GUC parameter
 * ----------
 */
char *pgstat_stat_directory = NULL;
char *pgstat_stat_filename = NULL;
char *pgstat_stat_tmpname = NULL;

/*
 * BgWriter global statistics counters (unused in other processes).
 * Stored directly in a stats message structure so it can be sent
 * without needing to copy things around.  We assume this inits to zeroes.
 */
PgStat_MsgBgWriter BgWriterStats;

/* ----------
 * Local data
 * ----------
 */
NON_EXEC_STATIC pgsocket pgStatSock = PGINVALID_SOCKET;

static struct sockaddr_storage pgStatAddr;

static time_t last_pgstat_start_time;

static bool pgStatRunningInCollector = false;

/*
 * Structures in which backends store per-table info that's waiting to be
 * sent to the collector.
 *
 * NOTE: once allocated, TabStatusArray structures are never moved or deleted
 * for the life of the backend.  Also, we zero out the t_id fields of the
 * contained PgStat_TableStatus structs whenever they are not actively in use.
 * This allows relcache pgstat_info pointers to be treated as long-lived data,
 * avoiding repeated searches in pgstat_initstats() when a relation is
 * repeatedly opened during a transaction.
 */
#define TABSTAT_QUANTUM 100 /* we alloc this many at a time */

typedef struct TabStatusArray
{
  struct TabStatusArray *tsa_next;                 /* link to next array, if any */
  int tsa_used;                                    /* # entries currently used */
  PgStat_TableStatus tsa_entries[TABSTAT_QUANTUM]; /* per-table data */
} TabStatusArray;

static TabStatusArray *pgStatTabList = NULL;

/*
 * pgStatTabHash entry: map from relation OID to PgStat_TableStatus pointer
 */
typedef struct TabStatHashEntry
{
  Oid t_id;
  PgStat_TableStatus *tsa_entry;
} TabStatHashEntry;

/*
 * Hash table for O(1) t_id -> tsa_entry lookup
 */
static HTAB *pgStatTabHash = NULL;

/*
 * Backends store per-function info that's waiting to be sent to the collector
 * in this hash table (indexed by function OID).
 */
static HTAB *pgStatFunctions = NULL;

/*
 * Indicates if backend has some function stats that it hasn't yet
 * sent to the collector.
 */
static bool have_function_stats = false;

/*
 * Tuple insertion/deletion counts for an open transaction can't be propagated
 * into PgStat_TableStatus counters until we know if it is going to commit
 * or abort.  Hence, we keep these counts in per-subxact structs that live
 * in TopTransactionContext.  This data structure is designed on the assumption
 * that subxacts won't usually modify very many tables.
 */
typedef struct PgStat_SubXactStatus
{
  int nest_level;                    /* subtransaction nest level */
  struct PgStat_SubXactStatus *prev; /* higher-level subxact if any */
  PgStat_TableXactStatus *first;     /* head of list for this subxact */
} PgStat_SubXactStatus;

static PgStat_SubXactStatus *pgStatXactStack = NULL;

static int pgStatXactCommit = 0;
static int pgStatXactRollback = 0;
PgStat_Counter pgStatBlockReadTime = 0;
PgStat_Counter pgStatBlockWriteTime = 0;

/* Record that's written to 2PC state file when pgstat state is persisted */
typedef struct TwoPhasePgStatRecord
{
  PgStat_Counter tuples_inserted;    /* tuples inserted in xact */
  PgStat_Counter tuples_updated;     /* tuples updated in xact */
  PgStat_Counter tuples_deleted;     /* tuples deleted in xact */
  PgStat_Counter inserted_pre_trunc; /* tuples inserted prior to truncate */
  PgStat_Counter updated_pre_trunc;  /* tuples updated prior to truncate */
  PgStat_Counter deleted_pre_trunc;  /* tuples deleted prior to truncate */
  Oid t_id;                          /* table's OID */
  bool t_shared;                     /* is it a shared catalog? */
  bool t_truncated;                  /* was the relation truncated? */
} TwoPhasePgStatRecord;

/*
 * Info about current "snapshot" of stats file
 */
static MemoryContext pgStatLocalContext = NULL;
static HTAB *pgStatDBHash = NULL;

/* Status for backends including auxiliary */
static LocalPgBackendStatus *localBackendStatusTable = NULL;

/* Total number of backends including auxiliary */
static int localNumBackends = 0;

/*
 * Cluster wide statistics, kept in the stats collector.
 * Contains statistics that are not collected per database
 * or per table.
 */
static PgStat_ArchiverStats archiverStats;
static PgStat_GlobalStats globalStats;

/*
 * List of OIDs of databases we need to write out.  If an entry is InvalidOid,
 * it means to write only the shared-catalog stats ("DB 0"); otherwise, we
 * will write both that DB's data and the shared stats.
 */
static List *pending_write_requests = NIL;

/* Signal handler flags */
static volatile bool need_exit = false;
static volatile bool got_SIGHUP = false;

/*
 * Total time charged to functions so far in the current backend.
 * We use this to help separate "self" and "other" time charges.
 * (We assume this initializes to zero.)
 */
static instr_time total_func_time;

/* ----------
 * Local function forward declarations
 * ----------
 */
#ifdef EXEC_BACKEND
static pid_t
pgstat_forkexec(void);
#endif

NON_EXEC_STATIC void
PgstatCollectorMain(int argc, char *argv[]) pg_attribute_noreturn();
static void pgstat_exit(SIGNAL_ARGS);
static void
pgstat_beshutdown_hook(int code, Datum arg);
static void pgstat_sighup_handler(SIGNAL_ARGS);

static PgStat_StatDBEntry *
pgstat_get_db_entry(Oid databaseid, bool create);
static PgStat_StatTabEntry *
pgstat_get_tab_entry(PgStat_StatDBEntry *dbentry, Oid tableoid, bool create);
static void
pgstat_write_statsfiles(bool permanent, bool allDbs);
static void
pgstat_write_db_statsfile(PgStat_StatDBEntry *dbentry, bool permanent);
static HTAB *
pgstat_read_statsfiles(Oid onlydb, bool permanent, bool deep);
static void
pgstat_read_db_statsfile(Oid databaseid, HTAB *tabhash, HTAB *funchash, bool permanent);
static void
backend_read_statsfile(void);
static void
pgstat_read_current_status(void);

static bool
pgstat_write_statsfile_needed(void);
static bool
pgstat_db_requested(Oid databaseid);

static void
pgstat_send_tabstat(PgStat_MsgTabstat *tsmsg);
static void
pgstat_send_funcstats(void);
static HTAB *
pgstat_collect_oids(Oid catalogid, AttrNumber anum_oid);

static PgStat_TableStatus *
get_tabstat_entry(Oid rel_id, bool isshared);

static void
pgstat_setup_memcxt(void);

static const char *
pgstat_get_wait_activity(WaitEventActivity w);
static const char *
pgstat_get_wait_client(WaitEventClient w);
static const char *
pgstat_get_wait_ipc(WaitEventIPC w);
static const char *
pgstat_get_wait_timeout(WaitEventTimeout w);
static const char *
pgstat_get_wait_io(WaitEventIO w);

static void
pgstat_setheader(PgStat_MsgHdr *hdr, StatMsgType mtype);
static void
pgstat_send(void *msg, int len);

static void
pgstat_recv_inquiry(PgStat_MsgInquiry *msg, int len);
static void
pgstat_recv_tabstat(PgStat_MsgTabstat *msg, int len);
static void
pgstat_recv_tabpurge(PgStat_MsgTabpurge *msg, int len);
static void
pgstat_recv_dropdb(PgStat_MsgDropdb *msg, int len);
static void
pgstat_recv_resetcounter(PgStat_MsgResetcounter *msg, int len);
static void
pgstat_recv_resetsharedcounter(PgStat_MsgResetsharedcounter *msg, int len);
static void
pgstat_recv_resetsinglecounter(PgStat_MsgResetsinglecounter *msg, int len);
static void
pgstat_recv_autovac(PgStat_MsgAutovacStart *msg, int len);
static void
pgstat_recv_vacuum(PgStat_MsgVacuum *msg, int len);
static void
pgstat_recv_analyze(PgStat_MsgAnalyze *msg, int len);
static void
pgstat_recv_archiver(PgStat_MsgArchiver *msg, int len);
static void
pgstat_recv_bgwriter(PgStat_MsgBgWriter *msg, int len);
static void
pgstat_recv_funcstat(PgStat_MsgFuncstat *msg, int len);
static void
pgstat_recv_funcpurge(PgStat_MsgFuncpurge *msg, int len);
static void
pgstat_recv_recoveryconflict(PgStat_MsgRecoveryConflict *msg, int len);
static void
pgstat_recv_deadlock(PgStat_MsgDeadlock *msg, int len);
static void
pgstat_recv_checksum_failure(PgStat_MsgChecksumFailure *msg, int len);
static void
pgstat_recv_tempfile(PgStat_MsgTempFile *msg, int len);

/* ------------------------------------------------------------
 * Public functions called from postmaster follow
 * ------------------------------------------------------------
 */

/* ----------
 * pgstat_init() -
 *
 *	Called from postmaster at startup. Create the resources required
 *	by the statistics collector process.  If unable to do so, do not
 *	fail --- better to let the postmaster start with stats collection
 *	disabled.
 * ----------
 */
void
pgstat_init(void)
{







































































































































































































































































}

/*
 * subroutine for pgstat_reset_all
 */
static void
pgstat_reset_remove_files(const char *directory)
{










































}

/*
 * pgstat_reset_all() -
 *
 * Remove the stats files.  This is currently used only if WAL
 * recovery is needed after a crash.
 */
void
pgstat_reset_all(void)
{


}

#ifdef EXEC_BACKEND

/*
 * pgstat_forkexec() -
 *
 * Format up the arglist for, then fork and exec, statistics collector process
 */
static pid_t
pgstat_forkexec(void)
{
  char *av[10];
  int ac = 0;

  av[ac++] = "postgres";
  av[ac++] = "--forkcol";
  av[ac++] = NULL; /* filled in by postmaster_forkexec */

  av[ac] = NULL;
  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}
#endif /* EXEC_BACKEND */

/*
 * pgstat_start() -
 *
 *	Called from postmaster at startup or after an existing collector
 *	died.  Attempt to fire up a fresh statistics collector.
 *
 *	Returns PID of child process, or 0 if fail.
 *
 *	Note: if fail, we will be called again from the postmaster main loop.
 */
int
pgstat_start(void)
{




























































}

void
allow_immediate_pgstat_restart(void)
{

}

/* ------------------------------------------------------------
 * Public functions used by backends follow
 *------------------------------------------------------------
 */

/* ----------
 * pgstat_report_stat() -
 *
 *	Must be called by processes that performs DML: tcop/postgres.c, logical
 *	receiver processes, SPI worker, etc. to send the so far collected
 *	per-table and function usage statistics to the collector.  Note that
 *this is called only when not within a transaction, so it is fair to use
 *	transaction stop time as an approximation of current time.
 * ----------
 */
void
pgstat_report_stat(bool force)
{








































































































}

/*
 * Subroutine for pgstat_report_stat: finish and send a tabstat message
 */
static void
pgstat_send_tabstat(PgStat_MsgTabstat *tsmsg)
{





































}

/*
 * Subroutine for pgstat_report_stat: populate and send a function stat message
 */
static void
pgstat_send_funcstats(void)
{


















































}

/* ----------
 * pgstat_vacuum_stat() -
 *
 *	Will tell the collector about objects he can get rid of.
 * ----------
 */
void
pgstat_vacuum_stat(void)
{









































































































































































}

/* ----------
 * pgstat_collect_oids() -
 *
 *	Collect the OIDs of all objects listed in the specified system catalog
 *	into a temporary hash table.  Caller should hash_destroy the result
 *	when done with it.  (However, we make the table in CurrentMemoryContext
 *	so that it will be freed properly in event of an error.)
 * ----------
 */
static HTAB *
pgstat_collect_oids(Oid catalogid, AttrNumber anum_oid)
{

































}

/* ----------
 * pgstat_drop_database() -
 *
 *	Tell the collector that we just dropped a database.
 *	(If the message gets lost, we will still clean the dead DB eventually
 *	via future invocations of pgstat_vacuum_stat().)
 * ----------
 */
void
pgstat_drop_database(Oid databaseid)
{










}

/* ----------
 * pgstat_drop_relation() -
 *
 *	Tell the collector that we just dropped a relation.
 *	(If the message gets lost, we will still clean the dead entry eventually
 *	via future invocations of pgstat_vacuum_stat().)
 *
 *	Currently not used for lack of any good place to call it; we rely
 *	entirely on pgstat_vacuum_stat() to clean out stats for dead rels.
 * ----------
 */
#ifdef NOT_USED
void
pgstat_drop_relation(Oid relid)
{
  PgStat_MsgTabpurge msg;
  int len;

  if (pgStatSock == PGINVALID_SOCKET)
  {
    return;
  }

  msg.m_tableid[0] = relid;
  msg.m_nentries = 1;

  len = offsetof(PgStat_MsgTabpurge, m_tableid[0]) + sizeof(Oid);

  pgstat_setheader(&msg.m_hdr, PGSTAT_MTYPE_TABPURGE);
  msg.m_databaseid = MyDatabaseId;
  pgstat_send(&msg, len);
}
#endif /* NOT_USED */

/* ----------
 * pgstat_reset_counters() -
 *
 *	Tell the statistics collector to reset counters for our database.
 *
 *	Permission checking for this function is managed through the normal
 *	GRANT system.
 * ----------
 */
void
pgstat_reset_counters(void)
{










}

/* ----------
 * pgstat_reset_shared_counters() -
 *
 *	Tell the statistics collector to reset cluster-wide shared counters.
 *
 *	Permission checking for this function is managed through the normal
 *	GRANT system.
 * ----------
 */
void
pgstat_reset_shared_counters(const char *target)
{






















}

/* ----------
 * pgstat_reset_single_counter() -
 *
 *	Tell the statistics collector to reset a single counter.
 *
 *	Permission checking for this function is managed through the normal
 *	GRANT system.
 * ----------
 */
void
pgstat_reset_single_counter(Oid objoid, PgStat_Single_Reset_Type type)
{













}

/* ----------
 * pgstat_report_autovac() -
 *
 *	Called from autovacuum.c to report startup of an autovacuum process.
 *	We are called before InitPostgres is done, so can't rely on
 *MyDatabaseId; the db OID must be passed in, instead.
 * ----------
 */
void
pgstat_report_autovac(Oid dboid)
{












}

/* ---------
 * pgstat_report_vacuum() -
 *
 *	Tell the collector about the table we just vacuumed.
 * ---------
 */
void
pgstat_report_vacuum(Oid tableoid, bool shared, PgStat_Counter livetuples, PgStat_Counter deadtuples)
{















}

/* --------
 * pgstat_report_analyze() -
 *
 *	Tell the collector about the table we just analyzed.
 *
 * Caller must provide new live- and dead-tuples estimates, as well as a
 * flag indicating whether to reset the changes_since_analyze counter.
 * --------
 */
void
pgstat_report_analyze(Relation rel, PgStat_Counter livetuples, PgStat_Counter deadtuples, bool resetcounter)
{










































}

/* --------
 * pgstat_report_recovery_conflict() -
 *
 *	Tell the collector about a Hot Standby recovery conflict.
 * --------
 */
void
pgstat_report_recovery_conflict(int reason)
{











}

/* --------
 * pgstat_report_deadlock() -
 *
 *	Tell the collector about a deadlock detected.
 * --------
 */
void
pgstat_report_deadlock(void)
{










}

/* --------
 * pgstat_report_checksum_failures_in_db() -
 *
 *	Tell the collector about one or more checksum failures.
 * --------
 */
void
pgstat_report_checksum_failures_in_db(Oid dboid, int failurecount)
{













}

/* --------
 * pgstat_report_checksum_failure() -
 *
 *	Tell the collector about a checksum failure.
 * --------
 */
void
pgstat_report_checksum_failure(void)
{

}

/* --------
 * pgstat_report_tempfile() -
 *
 *	Tell the collector about a temporary file.
 * --------
 */
void
pgstat_report_tempfile(size_t filesize)
{











}

/* ----------
 * pgstat_ping() -
 *
 *	Send some junk data to the collector to increase traffic.
 * ----------
 */
void
pgstat_ping(void)
{









}

/* ----------
 * pgstat_send_inquiry() -
 *
 *	Notify collector that we need fresh data.
 * ----------
 */
static void
pgstat_send_inquiry(TimestampTz clock_time, TimestampTz cutoff_time, Oid databaseid)
{







}

/*
 * Initialize function call usage data.
 * Called by the executor before invoking a function.
 */
void
pgstat_init_function_usage(FunctionCallInfo fcinfo, PgStat_FunctionCallUsage *fcu)
{






































}

/*
 * find_funcstat_entry - find any existing PgStat_BackendFunctionEntry entry
 *		for specified function
 *
 * If no entry, return NULL, don't create a new one
 */
PgStat_BackendFunctionEntry *
find_funcstat_entry(Oid func_id)
{






}

/*
 * Calculate function call usage and update stat counters.
 * Called by the executor after invoking a function.
 *
 * In the case of a set-returning function that runs in value-per-call mode,
 * we will see multiple pgstat_init_function_usage/pgstat_end_function_usage
 * calls for what the user considers a single call of the function.  The
 * finalize flag should be TRUE on the last call.
 */
void
pgstat_end_function_usage(PgStat_FunctionCallUsage *fcu, bool finalize)
{











































}

/* ----------
 * pgstat_initstats() -
 *
 *	Initialize a relcache entry to count access statistics.
 *	Called whenever a relation is opened.
 *
 *	We assume that a relcache entry's pgstat_info field is zeroed by
 *	relcache.c when the relcache entry is made; thereafter it is long-lived
 *	data.  We can avoid repeated searches of the TabStatus arrays when the
 *	same relation is touched repeatedly within a transaction.
 * ----------
 */
void
pgstat_initstats(Relation rel)
{




























}

/*
 * get_tabstat_entry - find or create a PgStat_TableStatus entry for rel
 */
static PgStat_TableStatus *
get_tabstat_entry(Oid rel_id, bool isshared)
{








































































}

/*
 * find_tabstat_entry - find any existing PgStat_TableStatus entry for rel
 *
 * If no entry, return NULL, don't create a new one
 *
 * Note: if we got an error in the most recent execution of pgstat_report_stat,
 * it's possible that an entry exists but there's no hashtable entry for it.
 * That's okay, we'll treat this case as "doesn't exist".
 */
PgStat_TableStatus *
find_tabstat_entry(Oid rel_id)
{
















}

/*
 * get_tabstat_stack_level - add a new (sub)transaction stack entry if needed
 */
static PgStat_SubXactStatus *
get_tabstat_stack_level(int nest_level)
{












}

/*
 * add_tabstat_xact_level - add a new (sub)transaction state record
 */
static void
add_tabstat_xact_level(PgStat_TableStatus *pgstat_info, int nest_level)
{

















}

/*
 * pgstat_count_heap_insert - count a tuple insertion of n tuples
 */
void
pgstat_count_heap_insert(Relation rel, PgStat_Counter n)
{














}

/*
 * pgstat_count_heap_update - count a tuple update
 */
void
pgstat_count_heap_update(Relation rel, bool hot)
{




















}

/*
 * pgstat_count_heap_delete - count a tuple deletion
 */
void
pgstat_count_heap_delete(Relation rel)
{














}

/*
 * pgstat_truncate_save_counters
 *
 * Whenever a table is truncated, we save its i/u/d counters so that they can
 * be cleared, and if the (sub)xact that executed the truncate later aborts,
 * the counters can be restored to the saved (pre-truncate) values.  Note we do
 * this on the first truncate in any particular subxact level only.
 */
static void
pgstat_truncate_save_counters(PgStat_TableXactStatus *trans)
{







}

/*
 * pgstat_truncate_restore_counters - restore counters when a truncate aborts
 */
static void
pgstat_truncate_restore_counters(PgStat_TableXactStatus *trans)
{






}

/*
 * pgstat_count_truncate - update tuple counters due to truncate
 */
void
pgstat_count_truncate(Relation rel)
{

















}

/*
 * pgstat_update_heap_dead_tuples - update dead-tuples count
 *
 * The semantics of this are that we are reporting the nontransactional
 * recovery of "delta" dead tuples; so t_delta_dead_tuples decreases
 * rather than increasing, and the change goes straight into the per-table
 * counter, not into transactional state.
 */
void
pgstat_update_heap_dead_tuples(Relation rel, int delta)
{






}

/* ----------
 * AtEOXact_PgStat
 *
 *	Called from access/transam/xact.c at top-level transaction commit/abort.
 * ----------
 */
void
AtEOXact_PgStat(bool isCommit, bool parallel)
{













































































}

/* ----------
 * AtEOSubXact_PgStat
 *
 *	Called from access/transam/xact.c at subtransaction commit/abort.
 * ----------
 */
void
AtEOSubXact_PgStat(bool isCommit, int nestDepth)
{




















































































}

/*
 * AtPrepare_PgStat
 *		Save the transactional stats state at 2PC transaction prepare.
 *
 * In this phase we just generate 2PC records for all the pending
 * transaction-dependent stats work.
 */
void
AtPrepare_PgStat(void)
{
































}

/*
 * PostPrepare_PgStat
 *		Clean up after successful PREPARE.
 *
 * All we need do here is unlink the transaction stats state from the
 * nontransactional state.  The nontransactional action counts will be
 * reported to the stats collector immediately, while the effects on live
 * and dead tuple counts are preserved in the 2PC state file.
 *
 * Note: AtEOXact_PgStat is not called during PREPARE.
 */
void
PostPrepare_PgStat(void)
{























}

/*
 * 2PC processing routine for COMMIT PREPARED case.
 *
 * Load the saved counts into our local pgstats state.
 */
void
pgstat_twophase_postcommit(TransactionId xid, uint16 info, void *recdata, uint32 len)
{




















}

/*
 * 2PC processing routine for ROLLBACK PREPARED case.
 *
 * Load the saved counts into our local pgstats state, but treat them
 * as aborted.
 */
void
pgstat_twophase_postabort(TransactionId xid, uint16 info, void *recdata, uint32 len)
{

















}

/* ----------
 * pgstat_fetch_stat_dbentry() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	the collected statistics for one database or NULL. NULL doesn't mean
 *	that the database doesn't exist, it is just not yet known by the
 *	collector, so the caller is better off to report ZERO instead.
 * ----------
 */
PgStat_StatDBEntry *
pgstat_fetch_stat_dbentry(Oid dbid)
{










}

/* ----------
 * pgstat_fetch_stat_tabentry() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	the collected statistics for one table or NULL. NULL doesn't mean
 *	that the table doesn't exist, it is just not yet known by the
 *	collector, so the caller is better off to report ZERO instead.
 * ----------
 */
PgStat_StatTabEntry *
pgstat_fetch_stat_tabentry(Oid relid)
{







































}

/* ----------
 * pgstat_fetch_stat_funcentry() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	the collected statistics for one function or NULL.
 * ----------
 */
PgStat_StatFuncEntry *
pgstat_fetch_stat_funcentry(Oid func_id)
{














}

/* ----------
 * pgstat_fetch_stat_beentry() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	our local copy of the current-activity entry for one backend.
 *
 *	NB: caller is responsible for a check if the user is permitted to see
 *	this info (especially the querystring).
 * ----------
 */
PgBackendStatus *
pgstat_fetch_stat_beentry(int beid)
{








}

/* ----------
 * pgstat_fetch_stat_local_beentry() -
 *
 *	Like pgstat_fetch_stat_beentry() but with locally computed additions
 *(like xid and xmin values of the backend)
 *
 *	NB: caller is responsible for a check if the user is permitted to see
 *	this info (especially the querystring).
 * ----------
 */
LocalPgBackendStatus *
pgstat_fetch_stat_local_beentry(int beid)
{








}

/* ----------
 * pgstat_fetch_stat_numbackends() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	the maximum current backend id.
 * ----------
 */
int
pgstat_fetch_stat_numbackends(void)
{



}

/*
 * ---------
 * pgstat_fetch_stat_archiver() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	a pointer to the archiver statistics struct.
 * ---------
 */
PgStat_ArchiverStats *
pgstat_fetch_stat_archiver(void)
{



}

/*
 * ---------
 * pgstat_fetch_global() -
 *
 *	Support function for the SQL-callable pgstat* functions. Returns
 *	a pointer to the global statistics struct.
 * ---------
 */
PgStat_GlobalStats *
pgstat_fetch_global(void)
{



}

/* ------------------------------------------------------------
 * Functions for management of the shared-memory PgBackendStatus array
 * ------------------------------------------------------------
 */

static PgBackendStatus *BackendStatusArray = NULL;
static PgBackendStatus *MyBEEntry = NULL;
static char *BackendAppnameBuffer = NULL;
static char *BackendClientHostnameBuffer = NULL;
static char *BackendActivityBuffer = NULL;
static Size BackendActivityBufferSize = 0;
#ifdef USE_SSL
static PgBackendSSLStatus *BackendSslStatusBuffer = NULL;
#endif
#ifdef ENABLE_GSS
static PgBackendGSSStatus *BackendGssStatusBuffer = NULL;
#endif

/*
 * Report shared-memory space needed by CreateSharedBackendStatus.
 */
Size
BackendStatusShmemSize(void)
{



















}

/*
 * Initialize the shared status array and several string buffers
 * during postmaster startup.
 */
void
CreateSharedBackendStatus(void)
{













































































































}

/* ----------
 * pgstat_initialize() -
 *
 *	Initialize pgstats state, and set up our on-proc-exit hook.
 *	Called from InitPostgres and AuxiliaryProcessMain. For auxiliary
 *process, MyBackendId is invalid. Otherwise, MyBackendId must be set, but we
 *must not have started any transaction yet (since the exit hook must run after
 *the last transaction exit). NOTE: MyDatabaseId isn't set yet; so the shutdown
 *hook has to be careful.
 * ----------
 */
void
pgstat_initialize(void)
{
























}

/* ----------
 * pgstat_bestart() -
 *
 *	Initialize this backend's entry in the PgBackendStatus array.
 *	Called from InitPostgres.
 *
 *	Apart from auxiliary processes, MyBackendId, MyDatabaseId,
 *	session userid, and application_name must be set for a
 *	backend (hence, this cannot be combined with pgstat_initialize).
 *	Note also that we must be inside a transaction if this isn't an aux
 *	process, as we may need to do encoding conversion on some strings.
 * ----------
 */
void
pgstat_bestart(void)
{




























































































































































































































}

/*
 * Shut down a single backend's statistics reporting at process exit.
 *
 * Flush any remaining statistics counts out to the collector.
 * Without this, operations triggered during backend exit (such as
 * temp table deletions) won't be counted.
 *
 * Lastly, clear out our entry in the PgBackendStatus array.
 */
static void
pgstat_beshutdown_hook(int code, Datum arg)
{























}

/* ----------
 * pgstat_report_activity() -
 *
 *	Called from tcop/postgres.c to report what the backend is actually doing
 *	(but note cmd_str can be NULL for certain cases).
 *
 * All updates of the status entry follow the protocol of bumping
 * st_changecount before and after.  We use a volatile pointer here to
 * ensure the compiler doesn't try to get cute.
 * ----------
 */
void
pgstat_report_activity(BackendState state, const char *cmd_str)
{




































































}

/*-----------
 * pgstat_progress_start_command() -
 *
 * Set st_progress_command (and st_progress_command_target) in own backend
 * entry.  Also, zero-initialize st_progress_param array.
 *-----------
 */
void
pgstat_progress_start_command(ProgressCommandType cmdtype, Oid relid)
{












}

/*-----------
 * pgstat_progress_update_param() -
 *
 * Update index'th member in st_progress_param[] of own backend entry.
 *-----------
 */
void
pgstat_progress_update_param(int index, int64 val)
{












}

/*-----------
 * pgstat_progress_update_multi_param() -
 *
 * Update multiple members in st_progress_param[] of own backend entry.
 * This is atomic; readers won't see intermediate states.
 *-----------
 */
void
pgstat_progress_update_multi_param(int nparam, const int *index, const int64 *val)
{


















}

/*-----------
 * pgstat_progress_end_command() -
 *
 * Reset st_progress_command (and st_progress_command_target) in own backend
 * entry.  This signals the end of the command.
 *-----------
 */
void
pgstat_progress_end_command(void)
{
















}

/* ----------
 * pgstat_report_appname() -
 *
 *	Called to update our application name.
 * ----------
 */
void
pgstat_report_appname(const char *appname)
{






















}

/*
 * Report current transaction start timestamp as the specified value.
 * Zero means there is no active transaction.
 */
void
pgstat_report_xact_timestamp(TimestampTz tstamp)
{

















}

/* ----------
 * pgstat_read_current_status() -
 *
 *	Copy the current contents of the PgBackendStatus array to local memory,
 *	if not already done in this transaction.
 * ----------
 */
static void
pgstat_read_current_status(void)
{
































































































































}

/* ----------
 * pgstat_get_wait_event_type() -
 *
 *	Return a string representing the current wait event type, backend is
 *	waiting on.
 */
const char *
pgstat_get_wait_event_type(uint32 wait_event_info)
{














































}

/* ----------
 * pgstat_get_wait_event() -
 *
 *	Return a string representing the current wait event, backend is
 *	waiting on.
 */
const char *
pgstat_get_wait_event(uint32 wait_event_info)
{




































































}

/* ----------
 * pgstat_get_wait_activity() -
 *
 * Convert WaitEventActivity to string.
 * ----------
 */
static const char *
pgstat_get_wait_activity(WaitEventActivity w)
{


















































}

/* ----------
 * pgstat_get_wait_client() -
 *
 * Convert WaitEventClient to string.
 * ----------
 */
static const char *
pgstat_get_wait_client(WaitEventClient w)
{



































}

/* ----------
 * pgstat_get_wait_ipc() -
 *
 * Convert WaitEventIPC to string.
 * ----------
 */
static const char *
pgstat_get_wait_ipc(WaitEventIPC w)
{























































































































}

/* ----------
 * pgstat_get_wait_timeout() -
 *
 * Convert WaitEventTimeout to string.
 * ----------
 */
static const char *
pgstat_get_wait_timeout(WaitEventTimeout w)
{




















}

/* ----------
 * pgstat_get_wait_io() -
 *
 * Convert WaitEventIO to string.
 * ----------
 */
static const char *
pgstat_get_wait_io(WaitEventIO w)
{





















































































































































































































}

/* ----------
 * pgstat_get_backend_current_activity() -
 *
 *	Return a string representing the current activity of the backend with
 *	the specified PID.  This looks directly at the BackendStatusArray,
 *	and so will provide current information regardless of the age of our
 *	transaction's snapshot of the status array.
 *
 *	It is the caller's responsibility to invoke this only for backends whose
 *	state is expected to remain stable while the result is in use.  The
 *	only current use is in deadlock reporting, where we can expect that
 *	the target backend is blocked on a lock.  (There are corner cases
 *	where the target's wait could get aborted while we are looking at it,
 *	but the very worst consequence is to return a pointer to a string
 *	that's been changed, so we won't worry too much.)
 *
 *	Note: return strings for special cases match
 *pg_stat_get_backend_activity.
 * ----------
 */
const char *
pgstat_get_backend_current_activity(int pid, bool checkUser)
{






























































}

/* ----------
 * pgstat_get_crashed_backend_activity() -
 *
 *	Return a string representing the current activity of the backend with
 *	the specified PID.  Like the function above, but reads shared memory
 *with the expectation that it may be corrupt.  On success, copy the string into
 *the "buffer" argument and return that pointer.  On failure, return NULL.
 *
 *	This function is only intended to be used by the postmaster to report
 *the query that crashed a backend.  In particular, no attempt is made to follow
 *the correct concurrency protocol when accessing the BackendStatusArray.  But
 *that's OK, in the worst case we'll return a corrupted message.  We also must
 *take care not to trip on ereport(ERROR).
 * ----------
 */
const char *
pgstat_get_crashed_backend_activity(int pid, char *buffer, int buflen)
{

























































}

const char *
pgstat_get_backend_desc(BackendType backendType)
{





































}

/* ------------------------------------------------------------
 * Local support functions follow
 * ------------------------------------------------------------
 */

/* ----------
 * pgstat_setheader() -
 *
 *		Set common header fields in a statistics message
 * ----------
 */
static void
pgstat_setheader(PgStat_MsgHdr *hdr, StatMsgType mtype)
{

}

/* ----------
 * pgstat_send() -
 *
 *		Send out one statistics message to the collector
 * ----------
 */
static void
pgstat_send(void *msg, int len)
{






















}

/* ----------
 * pgstat_send_archiver() -
 *
 *	Tell the collector about the WAL file that we successfully
 *	archived or failed to archive.
 * ----------
 */
void
pgstat_send_archiver(const char *xlog, bool failed)
{










}

/* ----------
 * pgstat_send_bgwriter() -
 *
 *		Send bgwriter statistics to the collector
 * ----------
 */
void
pgstat_send_bgwriter(void)
{























}

/* ----------
 * PgstatCollectorMain() -
 *
 *	Start up the statistics collector process.  This is the body of the
 *	postmaster child process.
 *
 *	The argc/argv parameters are valid only in EXEC_BACKEND case.
 * ----------
 */
NON_EXEC_STATIC void
PgstatCollectorMain(int argc, char *argv[])
{





















































































































































































































































}

/* SIGQUIT signal handler for collector process */
static void
pgstat_exit(SIGNAL_ARGS)
{






}

/* SIGHUP handler for collector process */
static void
pgstat_sighup_handler(SIGNAL_ARGS)
{






}

/*
 * Subroutine to clear stats in a database entry
 *
 * Tables and functions hashes are initialized to empty.
 */
static void
reset_dbentry_counters(PgStat_StatDBEntry *dbentry)
{




































}

/*
 * Lookup the hash table entry for the specified database. If no hash
 * table entry exists, initialize it, if the create parameter is true.
 * Else, return NULL.
 */
static PgStat_StatDBEntry *
pgstat_get_db_entry(Oid databaseid, bool create)
{






















}

/*
 * Lookup the hash table entry for the specified table. If no hash
 * table entry exists, initialize it, if the create parameter is true.
 * Else, return NULL.
 */
static PgStat_StatTabEntry *
pgstat_get_tab_entry(PgStat_StatDBEntry *dbentry, Oid tableoid, bool create)
{






































}

/* ----------
 * pgstat_write_statsfiles() -
 *		Write the global statistics file, as well as requested DB files.
 *
 *	'permanent' specifies writing to the permanent files not temporary ones.
 *	When true (happens only when the collector is shutting down), also
 *remove the temporary files so that backends starting up under a new postmaster
 *	can't read old data before the new collector is ready.
 *
 *	When 'allDbs' is false, only the requested databases (listed in
 *	pending_write_requests) will be written; otherwise, all databases
 *	will be written.
 * ----------
 */
static void
pgstat_write_statsfiles(bool permanent, bool allDbs)
{










































































































}

/*
 * return the filename for a DB stat file; filename is the output buffer,
 * of length len.
 */
static void
get_dbstat_filename(bool permanent, bool tempname, Oid databaseid, char *filename, int len)
{








}

/* ----------
 * pgstat_write_db_statsfile() -
 *		Write the stat file for a single database.
 *
 *	If writing to the permanent file (happens when the collector is
 *	shutting down only), remove the temporary file so that backends
 *	starting up under a new postmaster can't read the old data before
 *	the new collector is ready.
 * ----------
 */
static void
pgstat_write_db_statsfile(PgStat_StatDBEntry *dbentry, bool permanent)
{






















































































}

/* ----------
 * pgstat_read_statsfiles() -
 *
 *	Reads in some existing statistics collector files and returns the
 *	databases hash table that is the top level of the data.
 *
 *	If 'onlydb' is not InvalidOid, it means we only want data for that DB
 *	plus the shared catalogs ("DB 0").  We'll still populate the DB hash
 *	table for all databases, but we don't bother even creating
 *table/function hash tables for other databases.
 *
 *	'permanent' specifies reading from the permanent files not temporary
 *ones. When true (happens only when the collector is starting up), remove the
 *	files after reading; the in-memory status is now authoritative, and the
 *	files would be out of date in case somebody else reads them.
 *
 *	If a 'deep' read is requested, table/function stats are read, otherwise
 *	the table/function hash tables remain empty.
 * ----------
 */
static HTAB *
pgstat_read_statsfiles(Oid onlydb, bool permanent, bool deep)
{


































































































































































































}

/* ----------
 * pgstat_read_db_statsfile() -
 *
 *	Reads in the existing statistics collector file for the given database,
 *	filling the passed-in tables and functions hash tables.
 *
 *	As in pgstat_read_statsfiles, if the permanent file is requested, it is
 *	removed after reading.
 *
 *	Note: this code has the ability to skip storing per-table or
 *per-function data, if NULL is passed for the corresponding hashtable.  That's
 *not used at the moment though.
 * ----------
 */
static void
pgstat_read_db_statsfile(Oid databaseid, HTAB *tabhash, HTAB *funchash, bool permanent)
{




























































































































}

/* ----------
 * pgstat_read_db_statsfile_timestamp() -
 *
 *	Attempt to determine the timestamp of the last db statfile write.
 *	Returns true if successful; the timestamp is stored in *ts.
 *
 *	This needs to be careful about handling databases for which no stats
 *file exists, such as databases without a stat entry or those not yet written:
 *
 *	- if there's a database entry in the global file, return the
 *corresponding stats_timestamp value.
 *
 *	- if there's no db stat entry (e.g. for a new or inactive database),
 *	there's no stats_timestamp value, but also nothing to write so we return
 *	the timestamp of the global statfile.
 * ----------
 */
static bool
pgstat_read_db_statsfile_timestamp(Oid databaseid, bool permanent, TimestampTz *ts)
{
































































































}

/*
 * If not already done, read the statistics collector stats file into
 * some hash tables.  The results will be kept until pgstat_clear_snapshot()
 * is called (typically, at end of transaction).
 */
static void
backend_read_statsfile(void)
{









































































































































}

/* ----------
 * pgstat_setup_memcxt() -
 *
 *	Create pgStatLocalContext, if not already done.
 * ----------
 */
static void
pgstat_setup_memcxt(void)
{




}

/* ----------
 * pgstat_clear_snapshot() -
 *
 *	Discard any data collected in the current transaction.  Any subsequent
 *	request will cause new snapshots to be read.
 *
 *	This is also invoked during transaction commit or abort to discard
 *	the no-longer-wanted snapshot.
 * ----------
 */
void
pgstat_clear_snapshot(void)
{











}

/* ----------
 * pgstat_recv_inquiry() -
 *
 *	Process stat inquiry requests.
 * ----------
 */
static void
pgstat_recv_inquiry(PgStat_MsgInquiry *msg, int len)
{
















































































}

/* ----------
 * pgstat_recv_tabstat() -
 *
 *	Count what the backend has done.
 * ----------
 */
static void
pgstat_recv_tabstat(PgStat_MsgTabstat *msg, int len)
{





























































































}

/* ----------
 * pgstat_recv_tabpurge() -
 *
 *	Arrange for dead table removal.
 * ----------
 */
static void
pgstat_recv_tabpurge(PgStat_MsgTabpurge *msg, int len)
{





















}

/* ----------
 * pgstat_recv_dropdb() -
 *
 *	Arrange for dead database removal
 * ----------
 */
static void
pgstat_recv_dropdb(PgStat_MsgDropdb *msg, int len)
{


































}

/* ----------
 * pgstat_recv_resetcounter() -
 *
 *	Reset the statistics for the specified database.
 * ----------
 */
static void
pgstat_recv_resetcounter(PgStat_MsgResetcounter *msg, int len)
{

































}

/* ----------
 * pgstat_recv_resetshared() -
 *
 *	Reset some shared statistics of the cluster.
 * ----------
 */
static void
pgstat_recv_resetsharedcounter(PgStat_MsgResetsharedcounter *msg, int len)
{

















}

/* ----------
 * pgstat_recv_resetsinglecounter() -
 *
 *	Reset a statistics for a single object
 * ----------
 */
static void
pgstat_recv_resetsinglecounter(PgStat_MsgResetsinglecounter *msg, int len)
{





















}

/* ----------
 * pgstat_recv_autovac() -
 *
 *	Process an autovacuum signalling message.
 * ----------
 */
static void
pgstat_recv_autovac(PgStat_MsgAutovacStart *msg, int len)
{








}

/* ----------
 * pgstat_recv_vacuum() -
 *
 *	Process a VACUUM message.
 * ----------
 */
static void
pgstat_recv_vacuum(PgStat_MsgVacuum *msg, int len)
{























}

/* ----------
 * pgstat_recv_analyze() -
 *
 *	Process an ANALYZE message.
 * ----------
 */
static void
pgstat_recv_analyze(PgStat_MsgAnalyze *msg, int len)
{

































}

/* ----------
 * pgstat_recv_archiver() -
 *
 *	Process a ARCHIVER message.
 * ----------
 */
static void
pgstat_recv_archiver(PgStat_MsgArchiver *msg, int len)
{














}

/* ----------
 * pgstat_recv_bgwriter() -
 *
 *	Process a BGWRITER message.
 * ----------
 */
static void
pgstat_recv_bgwriter(PgStat_MsgBgWriter *msg, int len)
{










}

/* ----------
 * pgstat_recv_recoveryconflict() -
 *
 *	Process a RECOVERYCONFLICT message.
 * ----------
 */
static void
pgstat_recv_recoveryconflict(PgStat_MsgRecoveryConflict *msg, int len)
{





























}

/* ----------
 * pgstat_recv_deadlock() -
 *
 *	Process a DEADLOCK message.
 * ----------
 */
static void
pgstat_recv_deadlock(PgStat_MsgDeadlock *msg, int len)
{





}

/* ----------
 * pgstat_recv_checksum_failure() -
 *
 *	Process a CHECKSUMFAILURE message.
 * ----------
 */
static void
pgstat_recv_checksum_failure(PgStat_MsgChecksumFailure *msg, int len)
{






}

/* ----------
 * pgstat_recv_tempfile() -
 *
 *	Process a TEMPFILE message.
 * ----------
 */
static void
pgstat_recv_tempfile(PgStat_MsgTempFile *msg, int len)
{






}

/* ----------
 * pgstat_recv_funcstat() -
 *
 *	Count what the backend has done.
 * ----------
 */
static void
pgstat_recv_funcstat(PgStat_MsgFuncstat *msg, int len)
{



































}

/* ----------
 * pgstat_recv_funcpurge() -
 *
 *	Arrange for dead function removal.
 * ----------
 */
static void
pgstat_recv_funcpurge(PgStat_MsgFuncpurge *msg, int len)
{





















}

/* ----------
 * pgstat_write_statsfile_needed() -
 *
 *	Do we need to write out any stats files?
 * ----------
 */
static bool
pgstat_write_statsfile_needed(void)
{







}

/* ----------
 * pgstat_db_requested() -
 *
 *	Checks whether stats for a particular DB need to be written to a file.
 * ----------
 */
static bool
pgstat_db_requested(Oid databaseid)
{


















}

/*
 * Convert a potentially unsafely truncated activity string (see
 * PgBackendStatus.st_activity_raw's documentation) into a correctly truncated
 * one.
 *
 * The returned string is allocated in the caller's memory context and may be
 * freed.
 */
char *
pgstat_clip_activity(const char *raw_activity)
{






























}