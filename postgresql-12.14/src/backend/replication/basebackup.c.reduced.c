/*-------------------------------------------------------------------------
 *
 * basebackup.c
 *	  code for taking a base backup and streaming it to a standby
 *
 * Portions Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/basebackup.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "access/xlog_internal.h" /* for pg_start/stop_backup */
#include "catalog/pg_type.h"
#include "common/file_perm.h"
#include "lib/stringinfo.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "pgtar.h"
#include "pgstat.h"
#include "port.h"
#include "postmaster/syslogger.h"
#include "replication/basebackup.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/bufpage.h"
#include "storage/checksum.h"
#include "storage/dsm_impl.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/reinit.h"
#include "utils/builtins.h"
#include "utils/ps_status.h"
#include "utils/relcache.h"
#include "utils/timestamp.h"

typedef struct {
  const char *label;
  bool progress;
  bool fastcheckpoint;
  bool nowait;
  bool includewal;
  uint32 maxrate;
  bool sendtblspcmapfile;
} basebackup_options;

static int64
sendDir(const char *path, int basepathlen, bool sizeonly, List *tablespaces,
        bool sendtblspclinks);
static bool
sendFile(const char *readfilename, const char *tarfilename,
         struct stat *statbuf, bool missing_ok, Oid dboid);
static void
sendFileWithContent(const char *filename, const char *content);
static int64
_tarWriteHeader(const char *filename, const char *linktarget,
                struct stat *statbuf, bool sizeonly);
static int64
_tarWriteDir(const char *pathbuf, int basepathlen, struct stat *statbuf,
             bool sizeonly);
static void
send_int8_string(StringInfoData *buf, int64 intval);
static void
SendBackupHeader(List *tablespaces);
static void
perform_base_backup(basebackup_options *opt);
static void
parse_basebackup_options(List *options, basebackup_options *opt);
static void
SendXlogRecPtrResult(XLogRecPtr ptr, TimeLineID tli);
static int
compareWalFileNames(const void *a, const void *b);
static void
throttle(size_t increment);
static bool
is_checksummed_file(const char *fullpath, const char *filename);

/* Was the backup currently in-progress initiated in recovery mode? */
static bool backup_started_in_recovery = false;

/* Relative path of temporary statistics directory */
static char *statrelpath = NULL;

/*
 * Size of each block sent into the tar stream for larger files.
 */
#define TAR_SEND_SIZE 32768

/*
 * How frequently to throttle, as a fraction of the specified rate-second.
 */


/*
 * Checks whether we encountered any error in fread().  fread() doesn't give
 * any clue what has happened, so we check with ferror().  Also, neither
 * fread() nor ferror() set errno, so we just throw a generic error.
 */
#define CHECK_FREAD_ERROR(fp, filename)                                        \





/* The actual number of bytes, transfer of which may cause sleep. */
static uint64 throttling_sample;

/* Amount of data already transferred but not yet throttled.  */
static int64 throttling_counter;

/* The minimum time required to transfer throttling_sample bytes. */
static TimeOffset elapsed_min_unit;

/* The last check of the transfer rate. */
static TimestampTz throttled_last;

/* The starting XLOG position of the base backup. */
static XLogRecPtr startptr;

/* Total number of checksum failures during base backup. */
static int64 total_checksum_failures;

/* Do not verify checksums. */
static bool noverify_checksums = false;

/*
 * Definition of one element part of an exclusion list, used for paths part
 * of checksum validation or base backups.  "name" is the name of the file
 * or path to check for exclusion.  If "match_prefix" is true, any items
 * matching the name as prefix are excluded.
 */
struct exclude_list_item {
  const char *name;
  bool match_prefix;
};

/*
 * The contents of these directories are removed or recreated during server
 * start so they are not included in backups.  The directories themselves are
 * kept and included as empty to preserve access permissions.
 *
 * Note: this list should be kept in sync with the filter lists in pg_rewind's
 * filemap.c.
 */
static const char *excludeDirContents[] = {
    /*
     * Skip temporary statistics files. PG_STAT_TMP_DIR must be skipped even
     * when stats_temp_directory is set because PGSS_TEXT_FILE is always
     * created there.
     */
    PG_STAT_TMP_DIR,

    /*
     * It is generally not useful to backup the contents of this directory
     * even if the intention is to restore to another master. See backup.sgml
     * for a more detailed description.
     */
    "pg_replslot",

    /* Contents removed on startup, see dsm_cleanup_for_mmap(). */
    PG_DYNSHMEM_DIR,

    /* Contents removed on startup, see AsyncShmemInit(). */
    "pg_notify",

    /*
     * Old contents are loaded for possible debugging but are not required for
     * normal operation, see OldSerXidInit().
     */
    "pg_serial",

    /* Contents removed on startup, see DeleteAllExportedSnapshotFiles(). */
    "pg_snapshots",

    /* Contents zeroed on startup, see StartupSUBTRANS(). */
    "pg_subtrans",

    /* end of list */
    NULL};

/*
 * List of files excluded from backups.
 */
static const struct exclude_list_item excludeFiles[] = {
    /* Skip auto conf temporary file. */
    {PG_AUTOCONF_FILENAME ".tmp", false},

    /* Skip current log file temporary file */
    {LOG_METAINFO_DATAFILE_TMP, false},

    /*
     * Skip relation cache because it is rebuilt on startup.  This includes
     * temporary files.
     */
    {RELCACHE_INIT_FILENAME, true},

    /*
     * If there's a backup_label or tablespace_map file, it belongs to a
     * backup started by the user with pg_start_backup().  It is *not* correct
     * for this backup.  Our backup_label/tablespace_map is injected into the
     * tar separately.
     */
    {BACKUP_LABEL_FILE, false},
    {TABLESPACE_MAP, false},

    {"postmaster.pid", false},
    {"postmaster.opts", false},

    /* end of list */
    {NULL, false}};

/*
 * List of files excluded from checksum validation.
 *
 * Note: this list should be kept in sync with what pg_checksums.c
 * includes.
 */
static const struct exclude_list_item noChecksumFiles[] = {
    {"pg_control", false},
    {"pg_filenode.map", false},
    {"pg_internal.init", true},
    {"PG_VERSION", false},
#ifdef EXEC_BACKEND
    {"config_exec_params", true},
#endif
    {NULL, false}};

/*
 * Actually do a base backup for the specified tablespaces.
 *
 * This is split out mainly to avoid complaints about "variable might be
 * clobbered by longjmp" from stupider versions of gcc.
 */
static void
perform_base_backup(basebackup_options *opt)
{















































































































































































































































































































































































}

/*
 * qsort comparison function, to compare log/seg portion of WAL segment
 * filenames, ignoring the timeline portion.
 */
static int
compareWalFileNames(const void *a, const void *b)
{




}

/*
 * Parse the base backup options passed down by the parser
 */
static void
parse_basebackup_options(List *options, basebackup_options *opt)
{

























































































}

/*
 * SendBaseBackup() - send a complete base backup.
 *
 * The function will put the system into backup mode like pg_start_backup()
 * does, so that the backup is consistent even though we read directly from
 * the filesystem, bypassing the buffer cache.
 */
void
SendBaseBackup(BaseBackupCmd *cmd)
{





















}

static void
send_int8_string(StringInfoData *buf, int64 intval)
{





}

static void
SendBackupHeader(List *tablespaces)
{


































































}

/*
 * Send a single resultset containing just a single
 * XLogRecPtr record (in text format)
 */
static void
SendXlogRecPtrResult(XLogRecPtr ptr, TimeLineID tli)
{














































}

/*
 * Inject a file with given name and content in the output tar stream.
 */
static void
sendFileWithContent(const char *filename, const char *content)
{

































}

/*
 * Include the tablespace directory pointed to by 'path' in the output tar
 * stream.  If 'sizeonly' is true, we just calculate a total length and return
 * it, without actually sending anything.
 *
 * Only used to send auxiliary tablespaces, not PGDATA.
 */
int64
sendTablespace(char *path, bool sizeonly)
{

































}

/*
 * Include all files from the given directory in the output tar stream. If
 * 'sizeonly' is true, we just calculate a total length and return it, without
 * actually sending anything.
 *
 * Omit any directory in the tablespaces list, to avoid backing up
 * tablespaces twice when they were created inside PGDATA.
 *
 * If sendtblspclinks is true, we need to include symlink
 * information in the tar file. If not, we can skip that
 * as it will be sent separately in the tablespace_map file.
 */
static int64
sendDir(const char *path, int basepathlen, bool sizeonly, List *tablespaces,
        bool sendtblspclinks)
{

































































































































































































































































































}

/*
 * Check if a file should have its checksum validated.
 * We validate checksums on files in regular tablespaces
 * (including global and default) only, and in those there
 * are some files that are explicitly excluded.
 */
static bool
is_checksummed_file(const char *fullpath, const char *filename)
{






















}

/*****
 * Functions for handling tar file format
 *
 * Copied from pg_dump, but modified to work with libpq for sending
 */

/*
 * Given the member, write the TAR header & send the file.
 *
 * If 'missing_ok' is true, will not throw an error if the file is not found.
 *
 * If dboid is anything other than InvalidOid then any checksum failures
 * detected will get reported to the stats collector.
 *
 * Returns true if the file was successfully sent, false if 'missing_ok',
 * and the file did not exist.
 */
static bool
sendFile(const char *readfilename, const char *tarfilename,
         struct stat *statbuf, bool missing_ok, Oid dboid)
{






























































































































































































































}

static int64
_tarWriteHeader(const char *filename, const char *linktarget,
                struct stat *statbuf, bool sizeonly)
{




























}

/*
 * Write tar header for a directory.  If the entry in statbuf is a link then
 * write it as a directory anyway.
 */
static int64
_tarWriteDir(const char *pathbuf, int basepathlen, struct stat *statbuf,
             bool sizeonly)
{









}

/*
 * Increment the network transfer counter by the given number of bytes,
 * and sleep if necessary to comply with the requested network transfer
 * rate.
 */
static void
throttle(size_t increment)
{

































































}
