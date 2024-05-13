/*-------------------------------------------------------------------------
 *
 * xlogarchive.c
 *		Functions for archiving WAL files and restoring from the
 *archive.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/xlogarchive.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "miscadmin.h"
#include "postmaster/startup.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"

/*
 * Attempt to retrieve the specified file from off-line archival storage.
 * If successful, fill "path" with its complete path (note that this will be
 * a temp file name that doesn't follow the normal naming convention), and
 * return true.
 *
 * If not successful, fill "path" with the name of the normal on-line file
 * (which may or may not actually exist, but we'll try to use it), and return
 * false.
 *
 * For fixed-size files, the caller may pass the expected size as an
 * additional crosscheck on successful recovery.  If the file size is not
 * known, set expectedSize = 0.
 *
 * When 'cleanupEnabled' is false, refrain from deleting any old WAL segments
 * in the archive. This is used when fetching the initial checkpoint record,
 * when we are not yet sure how far back we need the WAL.
 */
bool
RestoreArchivedFile(char *path, const char *xlogfname, const char *recovername, off_t expectedSize, bool cleanupEnabled)
{











































































































































































































































































}

/*
 * Attempt to execute an external shell command during recovery.
 *
 * 'command' is the shell command to be executed, 'commandName' is a
 * human-readable name describing the command emitted in the logs. If
 * 'failOnSignal' is true and the command is killed by a signal, a FATAL
 * error is thrown. Otherwise a WARNING is emitted.
 *
 * This is currently used for recovery_end_command and archive_cleanup_command.
 */
void
ExecuteRecoveryCommand(const char *command, const char *commandName, bool failOnSignal)
{

















































































}

/*
 * A file was restored from the archive under a temporary filename (path),
 * and now we want to keep it. Rename it under the permanent filename in
 * pg_wal (xlogfname), replacing any existing file with the same name.
 */
void
KeepFileRestoredFromArchive(const char *path, const char *xlogfname)
{








































































}

/*
 * XLogArchiveNotify
 *
 * Create an archive notification file
 *
 * The name of the notification file is the message that will be picked up
 * by the archiver, e.g. we write 0000000100000001000000C6.ready
 * and the archiver then knows to archive XLOGDIR/0000000100000001000000C6,
 * then when complete, rename it to 0000000100000001000000C6.done
 */
void
XLogArchiveNotify(const char *xlog)
{






















}

/*
 * Convenience routine to notify using segment number representation of filename
 */
void
XLogArchiveNotifySeg(XLogSegNo segno)
{




}

/*
 * XLogArchiveForceDone
 *
 * Emit notification forcibly that an XLOG segment file has been successfully
 * archived, by creating <XLOG>.done regardless of whether <XLOG>.ready
 * exists or not.
 */
void
XLogArchiveForceDone(const char *xlog)
{
































}

/*
 * XLogArchiveCheckDone
 *
 * This is called when we are ready to delete or recycle an old XLOG segment
 * file or backup history file.  If it is okay to delete it then return true.
 * If it is not time to delete it, make sure a .ready file exists, and return
 * false.
 *
 * If <XLOG>.done exists, then return true; else if <XLOG>.ready exists,
 * then return false; else create <XLOG>.ready and return false.
 *
 * The reason we do things this way is so that if the original attempt to
 * create <XLOG>.ready fails, we'll retry during subsequent checkpoints.
 */
bool
XLogArchiveCheckDone(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];
  struct stat stat_buf;

  /* The file is always deletable if archive_mode is "off". */
  if (!XLogArchivingActive())
  {
    return true;
  }

  /*
   * During archive recovery, the file is deletable if archive_mode is not
   * "always".
   */





  /*
   * At this point of the logic, note that we are either a primary with
   * archive_mode set to "on" or "always", or a standby with archive_mode
   * set to "always".
   */

  /* First check for .done --- this means archiver is done with it */






  /* check for .ready --- this means archiver is still busy with it */






  /* Race condition --- maybe archiver just finished, so recheck */






  /* Retry creation of the .ready file */


}

/*
 * XLogArchiveIsBusy
 *
 * Check to see if an XLOG segment file is still unarchived.
 * This is almost but not quite the inverse of XLogArchiveCheckDone: in
 * the first place we aren't chartered to recreate the .ready file, and
 * in the second place we should consider that if the file is already gone
 * then it's not busy.  (This check is needed to handle the race condition
 * that a checkpoint already deleted the no-longer-needed file.)
 */
bool
XLogArchiveIsBusy(const char *xlog)
{




































}

/*
 * XLogArchiveIsReadyOrDone
 *
 * Check to see if an XLOG segment file has a .ready or .done file.
 * This is similar to XLogArchiveIsBusy(), but returns true if the file
 * is already archived or is about to be archived.
 *
 * This is currently only used at recovery.  During normal operation this
 * would be racy: the file might get removed or marked with .ready as we're
 * checking it, or immediately after we return.
 */
bool
XLogArchiveIsReadyOrDone(const char *xlog)
{

























}

/*
 * XLogArchiveIsReady
 *
 * Check to see if an XLOG segment file has an archive notification (.ready)
 * file.
 */
bool
XLogArchiveIsReady(const char *xlog)
{










}

/*
 * XLogArchiveCleanup
 *
 * Cleanup archive notification file(s) for a particular xlog segment
 */
void
XLogArchiveCleanup(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];

  /* Remove the .done file */
  StatusFilePath(archiveStatusPath, xlog, ".done");
  unlink(archiveStatusPath);
  /* should we complain about failure? */

  /* Remove the .ready file if present --- normally it shouldn't be */
  StatusFilePath(archiveStatusPath, xlog, ".ready");
  unlink(archiveStatusPath);
  /* should we complain about failure? */
}