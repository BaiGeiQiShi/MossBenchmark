/*-------------------------------------------------------------------------
 *
 * syslogger.c
 *
 * The system logger (syslogger) appeared in Postgres 8.0. It catches all
 * stderr output from the postmaster, backends, and other subprocesses
 * by redirecting to a pipe, and writes it to a set of logfiles.
 * It's possible to have size and age limits for the logfile configured
 * in postgresql.conf. If these limits are reached or passed, the
 * current logfile is closed and a new one is created (rotated).
 * The logfiles are stored in a subdirectory (configurable in
 * postgresql.conf), using a user-selectable naming scheme.
 *
 * Author: Andreas Pflug <pgadmin@pse-consulting.de>
 *
 * Copyright (c) 2004-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/syslogger.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "common/file_perm.h"
#include "lib/stringinfo.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "pgstat.h"
#include "pgtime.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pg_shmem.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "utils/timestamp.h"

/*
 * We read() into a temp buffer twice as big as a chunk, so that any fragment
 * left after processing can be moved down to the front and we'll still have
 * room to read a full chunk.
 */
#define READ_BUF_SIZE (2 * PIPE_CHUNK_SIZE)

/* Log rotation signal file path, relative to $PGDATA */
#define LOGROTATE_SIGNAL_FILE "logrotate"

/*
 * GUC parameters.  Logging_collector cannot be changed after postmaster
 * start, but the rest can change at SIGHUP.
 */
bool Logging_collector = false;
int Log_RotationAge = HOURS_PER_DAY * MINS_PER_HOUR;
int Log_RotationSize = 10 * 1024;
char *Log_directory = NULL;
char *Log_filename = NULL;
bool Log_truncate_on_rotation = false;
int Log_file_mode = S_IRUSR | S_IWUSR;

/*
 * Globally visible state (used by elog.c)
 */
bool am_syslogger = false;

extern bool redirection_done;

/*
 * Private state
 */
static pg_time_t next_rotation_time;
static bool pipe_eof_seen = false;
static bool rotation_disabled = false;
static FILE *syslogFile = NULL;
static FILE *csvlogFile = NULL;
NON_EXEC_STATIC pg_time_t first_syslogger_file_time = 0;
static char *last_file_name = NULL;
static char *last_csv_file_name = NULL;

/*
 * Buffers for saving partial messages from different backends.
 *
 * Keep NBUFFER_LISTS lists of these, with the entry for a given source pid
 * being in the list numbered (pid % NBUFFER_LISTS), so as to cut down on
 * the number of entries we have to examine for any one incoming message.
 * There must never be more than one entry for the same source pid.
 *
 * An inactive buffer is not removed from its list, just held for re-use.
 * An inactive buffer has pid == 0 and undefined contents of data.
 */
typedef struct
{
  int32 pid;           /* PID of source process */
  StringInfoData data; /* accumulated data, as a StringInfo */
} save_buffer;

#define NBUFFER_LISTS 256
static List *buffer_lists[NBUFFER_LISTS];

/* These must be exported for EXEC_BACKEND case ... annoying */
#ifndef WIN32
int syslogPipe[2] = {-1, -1};
#else
HANDLE syslogPipe[2] = {0, 0};
#endif

#ifdef WIN32
static HANDLE threadHandle = 0;
static CRITICAL_SECTION sysloggerSection;
#endif

/*
 * Flags set by interrupt handlers for later service in the main loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t rotation_requested = false;

/* Local subroutines */
#ifdef EXEC_BACKEND
static pid_t
syslogger_forkexec(void);
static void
syslogger_parseArgs(int argc, char *argv[]);
#endif
NON_EXEC_STATIC void
SysLoggerMain(int argc, char *argv[]) pg_attribute_noreturn();
static void
process_pipe_input(char *logbuffer, int *bytes_in_logbuffer);
static void
flush_pipe_input(char *logbuffer, int *bytes_in_logbuffer);
static FILE *
logfile_open(const char *filename, const char *mode, bool allow_errors);

#ifdef WIN32
static unsigned int __stdcall pipeThread(void *arg);
#endif
static void
logfile_rotate(bool time_based_rotation, int size_rotation_for);
static char *
logfile_getname(pg_time_t timestamp, const char *suffix);
static void
set_next_rotation_time(void);
static void sigHupHandler(SIGNAL_ARGS);
static void sigUsr1Handler(SIGNAL_ARGS);
static void
update_metainfo_datafile(void);

/*
 * Main entry point for syslogger process
 * argc/argv parameters are valid only in EXEC_BACKEND case.
 */
NON_EXEC_STATIC void
SysLoggerMain(int argc, char *argv[])
{







































































































































































































































































































































































































}

/*
 * Postmaster subroutine to start a syslogger subprocess.
 */
int
SysLogger_Start(void)
{
  pid_t sysloggerPid;
  char *filename;

  if (!Logging_collector)
  {
    return 0;
  }

  /*
   * If first time through, create the pipe which will receive stderr
   * output.
   *
   * If the syslogger crashes and needs to be restarted, we continue to use
   * the same pipe (indeed must do so, since extant backends will be writing
   * into that pipe).
   *
   * This means the postmaster must continue to hold the read end of the
   * pipe open, so we can pass it down to the reincarnated syslogger. This
   * is a bit klugy but we have little choice.
   */
#ifndef WIN32







#else
  if (!syslogPipe[0])
  {
    SECURITY_ATTRIBUTES sa;

    memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&syslogPipe[0], &syslogPipe[1], &sa, 32768))
    {
      ereport(FATAL, (errcode_for_file_access(), (errmsg("could not create pipe for syslog: %m"))));
    }
  }
#endif

  /*
   * Create log directory if not present; ignore errors
   */


  /*
   * The initial logfile is created right in the postmaster, to verify that
   * the Log_directory is writable.  We save the reference time so that the
   * syslogger child process can recompute this file name.
   *
   * It might look a bit strange to re-do this during a syslogger restart,
   * but we must do so since the postmaster closed syslogFile after the
   * previous fork (and remembering that old file wouldn't be right anyway).
   * Note we always append here, we won't overwrite any existing file.  This
   * is consistent with the normal rules, because by definition this is not
   * a time-based rotation.
   */








  /*
   * Likewise for the initial CSV log file, if that's enabled.  (Note that
   * we open syslogFile even when only CSV output is nominally enabled,
   * since some code paths will write to syslogFile anyway.)
   */









#ifdef EXEC_BACKEND
  switch ((sysloggerPid = syslogger_forkexec()))
#else
  switch ((sysloggerPid = fork_process()))
#endif
  {
  case -1:;



#ifndef EXEC_BACKEND
  case 0:;
    /* in postmaster child ... */


    /* Close the postmaster's sockets */


    /* Drop our connection to postmaster's shared memory, as well */



    /* do the work */


#endif

  default:;;
    /* success, in postmaster */

    /* now we redirect stderr, if not done already */





















































    /* postmaster will never write the file(s); close 'em */








  }

  /* we should never reach here */

}

#ifdef EXEC_BACKEND

/*
 * syslogger_forkexec() -
 *
 * Format up the arglist for, then fork and exec, a syslogger process
 */
static pid_t
syslogger_forkexec(void)
{
  char *av[10];
  int ac = 0;
  char filenobuf[32];
  char csvfilenobuf[32];

  av[ac++] = "postgres";
  av[ac++] = "--forklog";
  av[ac++] = NULL; /* filled in by postmaster_forkexec */

  /* static variables (those not passed by write_backend_variables) */
#ifndef WIN32
  if (syslogFile != NULL)
  {
    snprintf(filenobuf, sizeof(filenobuf), "%d", fileno(syslogFile));
  }
  else
  {
    strcpy(filenobuf, "-1");
  }
#else  /* WIN32 */
  if (syslogFile != NULL)
  {
    snprintf(filenobuf, sizeof(filenobuf), "%ld", (long)_get_osfhandle(_fileno(syslogFile)));
  }
  else
  {
    strcpy(filenobuf, "0");
  }
#endif /* WIN32 */
  av[ac++] = filenobuf;

#ifndef WIN32
  if (csvlogFile != NULL)
  {
    snprintf(csvfilenobuf, sizeof(csvfilenobuf), "%d", fileno(csvlogFile));
  }
  else
  {
    strcpy(csvfilenobuf, "-1");
  }
#else  /* WIN32 */
  if (csvlogFile != NULL)
  {
    snprintf(csvfilenobuf, sizeof(csvfilenobuf), "%ld", (long)_get_osfhandle(_fileno(csvlogFile)));
  }
  else
  {
    strcpy(csvfilenobuf, "0");
  }
#endif /* WIN32 */
  av[ac++] = csvfilenobuf;

  av[ac] = NULL;
  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}

/*
 * syslogger_parseArgs() -
 *
 * Extract data from the arglist for exec'ed syslogger process
 */
static void
syslogger_parseArgs(int argc, char *argv[])
{
  int fd;

  Assert(argc == 5);
  argv += 3;

  /*
   * Re-open the error output files that were opened by SysLogger_Start().
   *
   * We expect this will always succeed, which is too optimistic, but if it
   * fails there's not a lot we can do to report the problem anyway.  As
   * coded, we'll just crash on a null pointer dereference after failure...
   */
#ifndef WIN32
  fd = atoi(*argv++);
  if (fd != -1)
  {
    syslogFile = fdopen(fd, "a");
    setvbuf(syslogFile, NULL, PG_IOLBF, 0);
  }
  fd = atoi(*argv++);
  if (fd != -1)
  {
    csvlogFile = fdopen(fd, "a");
    setvbuf(csvlogFile, NULL, PG_IOLBF, 0);
  }
#else  /* WIN32 */
  fd = atoi(*argv++);
  if (fd != 0)
  {
    fd = _open_osfhandle(fd, _O_APPEND | _O_TEXT);
    if (fd > 0)
    {
      syslogFile = fdopen(fd, "a");
      setvbuf(syslogFile, NULL, PG_IOLBF, 0);
    }
  }
  fd = atoi(*argv++);
  if (fd != 0)
  {
    fd = _open_osfhandle(fd, _O_APPEND | _O_TEXT);
    if (fd > 0)
    {
      csvlogFile = fdopen(fd, "a");
      setvbuf(csvlogFile, NULL, PG_IOLBF, 0);
    }
  }
#endif /* WIN32 */
}
#endif /* EXEC_BACKEND */

/* --------------------------------
 *		pipe protocol handling
 * --------------------------------
 */

/*
 * Process data received through the syslogger pipe.
 *
 * This routine interprets the log pipe protocol which sends log messages as
 * (hopefully atomic) chunks - such chunks are detected and reassembled here.
 *
 * The protocol has a header that starts with two nul bytes, then has a 16 bit
 * length, the pid of the sending process, and a flag to indicate if it is
 * the last chunk in a message. Incomplete chunks are saved until we read some
 * more, and non-final chunks are accumulated until we get the final chunk.
 *
 * All of this is to avoid 2 problems:
 * . partial messages being written to logfiles (messes rotation), and
 * . messages from different backends being interleaved (messages garbled).
 *
 * Any non-protocol messages are written out directly. These should only come
 * from non-PostgreSQL sources, however (e.g. third party libraries writing to
 * stderr).
 *
 * logbuffer is the data input buffer, and *bytes_in_logbuffer is the number
 * of bytes present.  On exit, any not-yet-eaten data is left-justified in
 * logbuffer, and *bytes_in_logbuffer is updated.
 */
static void
process_pipe_input(char *logbuffer, int *bytes_in_logbuffer)
{







































































































































}

/*
 * Force out any buffered data
 *
 * This is currently used only at syslogger shutdown, but could perhaps be
 * useful at other times, so it is careful to leave things in a clean state.
 */
static void
flush_pipe_input(char *logbuffer, int *bytes_in_logbuffer)
{

































}

/* --------------------------------
 *		logfile routines
 * --------------------------------
 */

/*
 * Write text to the currently open logfile
 *
 * This is exported so that elog.c can call it when am_syslogger is true.
 * This allows the syslogger process to record elog messages of its own,
 * even though its stderr does not point at the syslog pipe.
 */
void
write_syslogger_file(const char *buffer, int count, int destination)
{





























}

#ifdef WIN32

/*
 * Worker thread to transfer data from the pipe to the current logfile.
 *
 * We need this because on Windows, WaitforMultipleObjects does not work on
 * unnamed pipes: it always reports "signaled", so the blocking ReadFile won't
 * allow for SIGHUP; and select is for sockets only.
 */
static unsigned int __stdcall pipeThread(void *arg)
{
  char logbuffer[READ_BUF_SIZE];
  int bytes_in_logbuffer = 0;

  for (;;)
  {
    DWORD bytesRead;
    BOOL result;

    result = ReadFile(syslogPipe[0], logbuffer + bytes_in_logbuffer, sizeof(logbuffer) - bytes_in_logbuffer, &bytesRead, 0);

    /*
     * Enter critical section before doing anything that might touch
     * global state shared by the main thread. Anything that uses
     * palloc()/pfree() in particular are not safe outside the critical
     * section.
     */
    EnterCriticalSection(&sysloggerSection);
    if (!result)
    {
      DWORD error = GetLastError();

      if (error == ERROR_HANDLE_EOF || error == ERROR_BROKEN_PIPE)
      {
        break;
      }
      _dosmaperr(error);
      ereport(LOG, (errcode_for_file_access(), errmsg("could not read from logger pipe: %m")));
    }
    else if (bytesRead > 0)
    {
      bytes_in_logbuffer += bytesRead;
      process_pipe_input(logbuffer, &bytes_in_logbuffer);
    }

    /*
     * If we've filled the current logfile, nudge the main thread to do a
     * log rotation.
     */
    if (Log_RotationSize > 0)
    {
      if (ftell(syslogFile) >= Log_RotationSize * 1024L || (csvlogFile != NULL && ftell(csvlogFile) >= Log_RotationSize * 1024L))
      {
        SetLatch(MyLatch);
      }
    }
    LeaveCriticalSection(&sysloggerSection);
  }

  /* We exit the above loop only upon detecting pipe EOF */
  pipe_eof_seen = true;

  /* if there's any data left then force it out now */
  flush_pipe_input(logbuffer, &bytes_in_logbuffer);

  /* set the latch to waken the main thread, which will quit */
  SetLatch(MyLatch);

  LeaveCriticalSection(&sysloggerSection);
  _endthread();
  return 0;
}
#endif /* WIN32 */

/*
 * Open a new logfile with proper permissions and buffering options.
 *
 * If allow_errors is true, we just log any open failure and return NULL
 * (with errno still correct for the fopen failure).
 * Otherwise, errors are treated as fatal.
 */
static FILE *
logfile_open(const char *filename, const char *mode, bool allow_errors)
{





























}

/*
 * perform logfile rotation
 */
static void
logfile_rotate(bool time_based_rotation, int size_rotation_for)
{



































































































































































}

/*
 * construct logfile name using timestamp information
 *
 * If suffix isn't NULL, append it to the name, replacing any ".log"
 * that may be in the pattern.
 *
 * Result is palloc'd.
 */
static char *
logfile_getname(pg_time_t timestamp, const char *suffix)
{























}

/*
 * Determine the next planned rotation time, and store in next_rotation_time.
 */
static void
set_next_rotation_time(void)
{
























}

/*
 * Store the name of the file(s) where the log collector, when enabled, writes
 * log messages.  Useful for finding the name(s) of the current log file(s)
 * when there is time-based logfile rotation.  Filenames are stored in a
 * temporary file and which is renamed into the final destination for
 * atomicity.  The file is opened with the same permissions as what gets
 * created in the data directory and has proper buffering options.
 */
static void
update_metainfo_datafile(void)
{

























































}

/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */

/*
 * Check to see if a log rotation request has arrived.  Should be
 * called by postmaster after receiving SIGUSR1.
 */
bool
CheckLogrotateSignal(void)
{








}

/*
 * Remove the file signaling a log rotation request.
 */
void
RemoveLogrotateSignalFiles(void)
{
  unlink(LOGROTATE_SIGNAL_FILE);
}

/* SIGHUP: set flag to reload config file */
static void
sigHupHandler(SIGNAL_ARGS)
{






}

/* SIGUSR1: set flag to rotate logfile */
static void
sigUsr1Handler(SIGNAL_ARGS)
{






}
