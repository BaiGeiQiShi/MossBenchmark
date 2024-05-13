/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2000-2019, PostgreSQL Global Development Group
 *
 * src/bin/psql/common.c
 */
#include "postgres_fe.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#ifndef WIN32
#include <unistd.h> /* for write() */
#else
#include <io.h> /* for _write() */
#include <win32.h>
#endif

#include "common/logging.h"
#include "fe_utils/mbprint.h"
#include "fe_utils/string_utils.h"
#include "portability/instr_time.h"

#include "command.h"
#include "common.h"
#include "copy.h"
#include "crosstabview.h"
#include "settings.h"

#define PQmblenBounded(s, e) strnlen(s, PQmblen(s, e))

static bool DescribeQuery(const char *query, double *elapsed_msec);
static bool ExecQueryUsingCursor(const char *query, double *elapsed_msec);
static bool command_no_begin(const char *query);
static bool is_select_command(const char *query);

/*
 * openQueryOutputFile --- attempt to open a query output file
 *
 * fname == NULL selects stdout, else an initial '|' selects a pipe,
 * else plain file.
 *
 * Returns output file pointer into *fout, and is-a-pipe flag into *is_pipe.
 * Caller is responsible for adjusting SIGPIPE state if it's a pipe.
 *
 * On error, reports suitable error message and returns false.
 */
bool openQueryOutputFile(const char *fname, FILE **fout, bool *is_pipe) {
  if (!fname || fname[0] == '\0') {
    *fout = stdout;
    *is_pipe = false;
  } else if (*fname == '|') {
    *fout = popen(fname + 1, "w");
    *is_pipe = true;
  } else {
    *fout = fopen(fname, "w");
    *is_pipe = false;
  }

  if (*fout == NULL) {

    return false;
  }

  return true;
}

/*
 * setQFout
 * -- handler for -o command line option and \o command
 *
 * On success, updates pset with the new output file and returns true.
 * On failure, returns false without changing pset state.
 */
bool setQFout(const char *fname) {
  FILE *fout;
  bool is_pipe;

  /* First make sure we can open the new output file/pipe */
  if (!openQueryOutputFile(fname, &fout, &is_pipe)) {
    return false;
  }

  /* Close old file/pipe */
  if (pset.queryFout && pset.queryFout != stdout && pset.queryFout != stderr) {





  }

  pset.queryFout = fout;
  pset.queryFoutPipe = is_pipe;

  /* Adjust SIGPIPE handling appropriately: ignore signal if is_pipe */
  set_sigpipe_trap_state(is_pipe);
  restore_sigpipe_trap();

  return true;
}

/*
 * Variable-fetching callback for flex lexer
 *
 * If the specified variable exists, return its value as a string (malloc'd
 * and expected to be freed by the caller); else return NULL.
 *
 * If "quote" isn't PQUOTE_PLAIN, then return the value suitably quoted and
 * escaped for the specified quoting requirement.  (Failure in escaping
 * should lead to printing an error and returning NULL.)
 *
 * "passthrough" is the pointer previously given to psql_scan_set_passthrough.
 * In psql, passthrough points to a ConditionalStack, which we check to
 * determine whether variable expansion is allowed.
 */
char *psql_get_variable(const char *varname, PsqlScanQuoteType quote,
                        void *passthrough) {













































































}

/*
 * for backend Notice messages (INFO, WARNING, etc)
 */
void NoticeProcessor(void *arg, const char *message) {
  (void)arg; /* not used */
  pg_log_info("%s", message);
}

/*
 * Code to support query cancellation
 *
 * Before we start a query, we enable the SIGINT signal catcher to send a
 * cancel request to the backend. Note that sending the cancel directly from
 * the signal handler is safe because PQcancel() is written to make it
 * so. We use write() to report to stderr because it's better to use simple
 * facilities in a signal handler.
 *
 * On win32, the signal canceling happens on a separate thread, because
 * that's how SetConsoleCtrlHandler works. The PQcancel function is safe
 * for this (unlike PQrequestCancel). However, a CRITICAL_SECTION is required
 * to protect the PGcancel structure against being changed while the signal
 * thread is using it.
 *
 * SIGINT is supposed to abort all long-running psql operations, not only
 * database queries.  In most places, this is accomplished by checking
 * cancel_pressed during long-running loops.  However, that won't work when
 * blocked on user input (in readline() or fgets()).  In those places, we
 * set sigint_interrupt_enabled true while blocked, instructing the signal
 * catcher to longjmp through sigint_interrupt_jmp.  We assume readline and
 * fgets are coded to handle possible interruption.  (XXX currently this does
 * not work on win32, so control-C is less useful there)
 */
volatile bool sigint_interrupt_enabled = false;

sigjmp_buf sigint_interrupt_jmp;

static PGcancel *volatile cancelConn = NULL;

#ifdef WIN32
static CRITICAL_SECTION cancelConnLock;
#endif

/*
 * Write a simple string to stderr --- must be safe in a signal handler.
 * We ignore the write() result since there's not much we could do about it.
 * Certain compilers make that harder than it ought to be.
 */
#define write_stderr(str)                                                      \
  do {                                                                         \
    const char *str_ = (str);                                                  \
    int rc_;                                                                   \
    rc_ = write(fileno(stderr), str_, strlen(str_));                           \
    (void)rc_;                                                                 \
  } while (0)

#ifndef WIN32

static void handle_sigint(SIGNAL_ARGS) {























}

void setup_cancel_handler(void) { pqsignal(SIGINT, handle_sigint); }
#else  /* WIN32 */

static BOOL WINAPI consoleHandler(DWORD dwCtrlType) {
  char errbuf[256];

  if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
    /*
     * Can't longjmp here, because we are in wrong thread :-(
     */

    /* set cancel flag to stop any long-running loops */
    cancel_pressed = true;

    /* and send QueryCancel if we are processing a database query */
    EnterCriticalSection(&cancelConnLock);
    if (cancelConn != NULL) {
      if (PQcancel(cancelConn, errbuf, sizeof(errbuf))) {
        write_stderr("Cancel request sent\n");
      } else {
        write_stderr("Could not send cancel request: ");
        write_stderr(errbuf);
      }
    }
    LeaveCriticalSection(&cancelConnLock);

    return TRUE;
  } else {
    /* Return FALSE for any signals not being handled */
    return FALSE;
  }
}

void setup_cancel_handler(void) {
  InitializeCriticalSection(&cancelConnLock);

  SetConsoleCtrlHandler(consoleHandler, TRUE);
}
#endif /* WIN32 */

/* ConnectionUp
 *
 * Returns whether our backend connection is still there.
 */
static bool ConnectionUp(void) { return PQstatus(pset.db) != CONNECTION_BAD; }

/* CheckConnection
 *
 * Verify that we still have a good connection to the backend, and if not,
 * see if it can be restored.
 *
 * Returns true if either the connection was still there, or it could be
 * restored successfully; false otherwise.  If, however, there was no
 * connection and the session is non-interactive, this will exit the program
 * with a code of EXIT_BADCONN.
 */
static bool CheckConnection(void) {
  bool OK;

  OK = ConnectionUp();
  if (!OK) {





    fprintf(stderr,
            _("The connection to the server was lost. Attempting reset: "));
    PQreset(pset.db);
    OK = ConnectionUp();





















  }

  return OK;
}

/*
 * SetCancelConn
 *
 * Set cancelConn to point to the current database connection.
 */
void SetCancelConn(void) {
  PGcancel *oldCancelConn;

#ifdef WIN32
  EnterCriticalSection(&cancelConnLock);
#endif

  /* Free the old one if we have one */
  oldCancelConn = cancelConn;
  /* be sure handle_sigint doesn't use pointer while freeing */
  cancelConn = NULL;

  if (oldCancelConn != NULL) {
    PQfreeCancel(oldCancelConn);
  }

  cancelConn = PQgetCancel(pset.db);

#ifdef WIN32
  LeaveCriticalSection(&cancelConnLock);
#endif
}

/*
 * ResetCancelConn
 *
 * Free the current cancel connection, if any, and set to NULL.
 */
void ResetCancelConn(void) {
  PGcancel *oldCancelConn;

#ifdef WIN32
  EnterCriticalSection(&cancelConnLock);
#endif

  oldCancelConn = cancelConn;
  /* be sure handle_sigint doesn't use pointer while freeing */
  cancelConn = NULL;

  if (oldCancelConn != NULL) {
    PQfreeCancel(oldCancelConn);
  }

#ifdef WIN32
  LeaveCriticalSection(&cancelConnLock);
#endif
}

/*
 * AcceptResult
 *
 * Checks whether a result is valid, giving an error message if necessary;
 * and ensures that the connection to the backend is still up.
 *
 * Returns true for valid result, false for error state.
 */
static bool AcceptResult(const PGresult *result) {
  bool OK;

  if (!result) {
    OK = false;
  } else {
    switch (PQresultStatus(result)) {
    case PGRES_COMMAND_OK:
    case PGRES_TUPLES_OK:
    case PGRES_EMPTY_QUERY:
    case PGRES_COPY_IN:
    case PGRES_COPY_OUT:
      /* Fine, do nothing */
      OK = true;
      break;

    case PGRES_BAD_RESPONSE:
    case PGRES_NONFATAL_ERROR:
    case PGRES_FATAL_ERROR:
      OK = false;
      break;

    default:;
      OK = false;
      pg_log_error("unexpected PQresultStatus: %d", PQresultStatus(result));
      break;
    }
  }

  if (!OK) {
    const char *error = PQerrorMessage(pset.db);

    if (strlen(error)) {
      pg_log_info("%s", error);
    }

    CheckConnection();
  }

  return OK;
}

/*
 * Set special variables from a query result
 * - ERROR: true/false, whether an error occurred on this query
 * - SQLSTATE: code of error, or "00000" if no error, or "" if unknown
 * - ROW_COUNT: how many rows were returned or affected, or "0"
 * - LAST_ERROR_SQLSTATE: same for last error
 * - LAST_ERROR_MESSAGE: message of last error
 *
 * Note: current policy is to apply this only to the results of queries
 * entered by the user, not queries generated by slash commands.
 */
static void SetResultVariables(PGresult *results, bool success) {
  if (success) {
    const char *ntuples = PQcmdTuples(results);

    SetVariable(pset.vars, "ERROR", "false");
    SetVariable(pset.vars, "SQLSTATE", "00000");
    SetVariable(pset.vars, "ROW_COUNT", *ntuples ? ntuples : "0");
  } else {
    const char *code = PQresultErrorField(results, PG_DIAG_SQLSTATE);
    const char *mesg = PQresultErrorField(results, PG_DIAG_MESSAGE_PRIMARY);

    SetVariable(pset.vars, "ERROR", "true");

    /*
     * If there is no SQLSTATE code, use an empty string.  This can happen
     * for libpq-detected errors (e.g., lost connection, ENOMEM).
     */
    if (code == NULL) {

    }
    SetVariable(pset.vars, "SQLSTATE", code);
    SetVariable(pset.vars, "ROW_COUNT", "0");
    SetVariable(pset.vars, "LAST_ERROR_SQLSTATE", code);
    SetVariable(pset.vars, "LAST_ERROR_MESSAGE", mesg ? mesg : "");
  }
}

/*
 * ClearOrSaveResult
 *
 * If the result represents an error, remember it for possible display by
 * \errverbose.  Otherwise, just PQclear() it.
 *
 * Note: current policy is to apply this to the results of all queries,
 * including "back door" queries, for debugging's sake.  It's OK to use
 * PQclear() directly on results known to not be error results, however.
 */
static void ClearOrSaveResult(PGresult *result) {
  if (result) {
    switch (PQresultStatus(result)) {
    case PGRES_NONFATAL_ERROR:
    case PGRES_FATAL_ERROR:
      if (pset.last_error_result) {
        PQclear(pset.last_error_result);
      }
      pset.last_error_result = result;
      break;

    default:;
      PQclear(result);
      break;
    }
  }
}

/*
 * Print microtiming output.  Always print raw milliseconds; if the interval
 * is >= 1 second, also break it down into days/hours/minutes/seconds.
 */
static void PrintTiming(double elapsed_msec) {






































}

/*
 * PSQLexec
 *
 * This is the way to send "backdoor" queries (those not directly entered
 * by the user). It is subject to -E but not -e.
 *
 * Caller is responsible for handling the ensuing processing if a COPY
 * command is sent.
 *
 * Note: we don't bother to check PQclientEncoding; it is assumed that no
 * caller uses this path to issue "SET CLIENT_ENCODING".
 */
PGresult *PSQLexec(const char *query) {







































}

/*
 * PSQLexecWatch
 *
 * This function is used for \watch command to send the query to
 * the server and print out the results.
 *
 * Returns 1 if the query executed successfully, 0 if it cannot be repeated,
 * e.g., because of the interrupt, -1 on error.
 */
int PSQLexecWatch(const char *query, const printQueryOpt *opt) {














































































}

/*
 * PrintNotifications: check for asynchronous notifications, and print them out
 */
static void PrintNotifications(void) {
  PGnotify *notify;

  PQconsumeInput(pset.db);
  while ((notify = PQnotifies(pset.db)) != NULL) {
    /* for backward compatibility, only show payload if nonempty */
    if (notify->extra[0]) {
      fprintf(pset.queryFout,
              _("Asynchronous notification \"%s\" with payload \"%s\" received "
                "from server process with PID %d.\n"),
              notify->relname, notify->extra, notify->be_pid);
    } else {
      fprintf(pset.queryFout,
              _("Asynchronous notification \"%s\" received from server process "
                "with PID %d.\n"),
              notify->relname, notify->be_pid);
    }
    fflush(pset.queryFout);
    PQfreemem(notify);
    PQconsumeInput(pset.db);
  }
}

/*
 * PrintQueryTuples: assuming query result is OK, print its tuples
 *
 * Returns true if successful, false otherwise.
 */
static bool PrintQueryTuples(const PGresult *results) {
  printQueryOpt my_popt = pset.popt;

  /* one-shot expanded output requested via \gx */
  if (pset.g_expanded) {
    my_popt.topt.expanded = 1;
  }

  /* write output to \g argument, if any */
  if (pset.gfname) {
    FILE *fout;









    printQuery(results, &my_popt, fout, false, pset.logfile);







  } else {
    printQuery(results, &my_popt, pset.queryFout, false, pset.logfile);
  }

  return true;
}

/*
 * StoreQueryTuple: assuming query result is OK, save data into variables
 *
 * Returns true if successful, false otherwise.
 */
static bool StoreQueryTuple(const PGresult *result) {












































}

/*
 * ExecQueryTuples: assuming query result is OK, execute each query
 * result field as a SQL statement
 *
 * Returns true if successful, false otherwise.
 */
static bool ExecQueryTuples(const PGresult *result) {






















































}

/*
 * ProcessResult: utility function for use by SendQuery() only
 *
 * When our command string contained a COPY FROM STDIN or COPY TO STDOUT,
 * PQexec() has stopped at the PGresult associated with the first such
 * command.  In that event, we'll marshal data for the COPY and then cycle
 * through any subsequent PGresult objects.
 *
 * When the command string contained no such COPY command, this function
 * degenerates to an AcceptResult() call.
 *
 * Changes its argument to point to the last PGresult of the command string,
 * or NULL if that result was for a COPY TO STDOUT.  (Returning NULL prevents
 * the command status from being printed, which we want in that case so that
 * the status line doesn't get taken as part of the COPY data.)
 *
 * Returns true on complete success, false otherwise.  Possible failure modes
 * include purely client-side problems; check the transaction status for the
 * server-side opinion.
 */
static bool ProcessResult(PGresult **results) {
  bool success = true;
  bool first_cycle = true;

  for (;;) {
    ExecStatusType result_status;
    bool is_copy;
    PGresult *next_result;

    if (!AcceptResult(*results)) {
      /*
       * Failure at this point is always a server-side failure or a
       * failure to submit the command string.  Either way, we're
       * finished with this command string.
       */
      success = false;
      break;
    }

    result_status = PQresultStatus(*results);
    switch (result_status) {
    case PGRES_EMPTY_QUERY:
    case PGRES_COMMAND_OK:
    case PGRES_TUPLES_OK:
      is_copy = false;
      break;

    case PGRES_COPY_OUT:
    case PGRES_COPY_IN:
      is_copy = true;
      break;

    default:;
      /* AcceptResult() should have caught anything else. */
      is_copy = false;
      pg_log_error("unexpected PQresultStatus: %d", result_status);
      break;
    }

    if (is_copy) {
      /*
       * Marshal the COPY data.  Either subroutine will get the
       * connection out of its COPY state, then call PQresultStatus()
       * once and report any error.
       *
       * For COPY OUT, direct the output to pset.copyStream if it's set,
       * otherwise to pset.gfname if it's set, otherwise to queryFout.
       * For COPY IN, use pset.copyStream as data source if it's set,
       * otherwise cur_cmd_source.
       */
      FILE *copystream;
      PGresult *copy_result;























































      /*
       * Replace the PGRES_COPY_OUT/IN result with COPY command's exit
       * status, or with NULL if we want to suppress printing anything.
       */
      PQclear(*results);
      *results = copy_result;
    } else if (first_cycle) {
      /* fast path: no COPY commands; PQexec visited all results */
      break;
    }

    /*
     * Check PQgetResult() again.  In the typical case of a single-command
     * string, it will return NULL.  Otherwise, we'll have other results
     * to process that may include other COPYs.  We keep the last result.
     */
    next_result = PQgetResult(pset.db);




    PQclear(*results);
    *results = next_result;
    first_cycle = false;
  }

  SetResultVariables(*results, success);

  /* may need this to recover from conn loss during COPY */
  if (!first_cycle && !CheckConnection()) {
    return false;
  }

  return success;
}

/*
 * PrintQueryStatus: report command status as required
 *
 * Note: Utility function for use by PrintQueryResults() only.
 */
static void PrintQueryStatus(PGresult *results) {
  char buf[16];

  if (!pset.quiet) {
    if (pset.popt.topt.format == PRINT_HTML) {
      fputs("<p>", pset.queryFout);
      html_escaped_print(PQcmdStatus(results), pset.queryFout);
      fputs("</p>\n", pset.queryFout);
    } else {
      fprintf(pset.queryFout, "%s\n", PQcmdStatus(results));
    }
  }

  if (pset.logfile) {
    fprintf(pset.logfile, "%s\n", PQcmdStatus(results));
  }

  snprintf(buf, sizeof(buf), "%u", (unsigned int)PQoidValue(results));
  SetVariable(pset.vars, "LASTOID", buf);
}

/*
 * PrintQueryResults: print out (or store or execute) query results as required
 *
 * Note: Utility function for use by SendQuery() only.
 *
 * Returns true if the query executed successfully, false otherwise.
 */
static bool PrintQueryResults(PGresult *results) {
  bool success;
  const char *cmdstatus;

  if (!results) {
    return false;
  }

  switch (PQresultStatus(results)) {
  case PGRES_TUPLES_OK:
    /* store or execute or print the data ... */
    if (pset.gset_prefix) {
      success = StoreQueryTuple(results);
    } else if (pset.gexec_flag) {
      success = ExecQueryTuples(results);
    } else if (pset.crosstab_flag) {
      success = PrintResultsInCrosstab(results);
    } else {
      success = PrintQueryTuples(results);
    }
    /* if it's INSERT/UPDATE/DELETE RETURNING, also print status */
    cmdstatus = PQcmdStatus(results);
    if (strncmp(cmdstatus, "INSERT", 6) == 0 ||
        strncmp(cmdstatus, "UPDATE", 6) == 0 ||
        strncmp(cmdstatus, "DELETE", 6) == 0) {
      PrintQueryStatus(results);
    }
    break;

  case PGRES_COMMAND_OK:
    PrintQueryStatus(results);
    success = true;
    break;

  case PGRES_EMPTY_QUERY:
    success = true;
    break;

  case PGRES_COPY_OUT:
  case PGRES_COPY_IN:
    /* nothing to do here */
    success = true;
    break;

  case PGRES_BAD_RESPONSE:
  case PGRES_NONFATAL_ERROR:
  case PGRES_FATAL_ERROR:
    success = false;
    break;

  default:;
    success = false;
    pg_log_error("unexpected PQresultStatus: %d", PQresultStatus(results));
    break;
  }

  fflush(pset.queryFout);

  return success;
}

/*
 * SendQuery: send the query string to the backend
 * (and print out results)
 *
 * Note: This is the "front door" way to send a query. That is, use it to
 * send queries actually entered by the user. These queries will be subject to
 * single step mode.
 * To send "back door" queries (generated by slash commands, etc.) in a
 * controlled way, use PSQLexec().
 *
 * Returns true if the query executed successfully, false otherwise.
 */
bool SendQuery(const char *query) {
  PGresult *results;
  PGTransactionStatusType transaction_status;
  double elapsed_msec = 0;
  bool OK = false;
  int i;
  bool on_error_rollback_savepoint = false;
  static bool on_error_rollback_warning = false;

  if (!pset.db) {


  }

  if (pset.singlestep) {


    fflush(stderr);






    fflush(stdout);








  } else if (pset.echo == PSQL_ECHO_QUERIES) {

    fflush(stdout);
  }

  if (pset.logfile) {
    fprintf(pset.logfile,
            _("********* QUERY **********\n"
              "%s\n"
              "**************************\n\n"),
            query);
    fflush(pset.logfile);
  }

  SetCancelConn();

  transaction_status = PQtransactionStatus(pset.db);

  if (transaction_status == PQTRANS_IDLE && !pset.autocommit &&
      !command_no_begin(query)) {
    results = PQexec(pset.db, "BEGIN");






    ClearOrSaveResult(results);
    transaction_status = PQtransactionStatus(pset.db);
  }

  if (transaction_status == PQTRANS_INTRANS &&
      pset.on_error_rollback != PSQL_ERROR_ROLLBACK_OFF &&
      (pset.cur_cmd_interactive ||
       pset.on_error_rollback == PSQL_ERROR_ROLLBACK_ON)) {



















  }

  if (pset.gdesc_flag) {
    /* Describe query's result columns, without executing it */
    OK = DescribeQuery(query, &elapsed_msec);

    results = NULL; /* PQclear(NULL) does nothing */
  } else if (pset.fetch_count <= 0 || pset.gexec_flag || pset.crosstab_flag ||
             !is_select_command(query)) {
    /* Default fetch-it-all-and-print mode */
    instr_time before, after;

    if (pset.timing) {
      INSTR_TIME_SET_CURRENT(before);
    }

    results = PQexec(pset.db, query);

    /* these operations are included in the timing result: */
    ResetCancelConn();
    OK = ProcessResult(&results);

    if (pset.timing) {
      INSTR_TIME_SET_CURRENT(after);
      INSTR_TIME_SUBTRACT(after, before);
      elapsed_msec = INSTR_TIME_GET_MILLISEC(after);
    }

    /* but printing results isn't: */
    if (OK && results) {
      OK = PrintQueryResults(results);
    }
  } else {
    /* Fetch-in-segments mode */
    OK = ExecQueryUsingCursor(query, &elapsed_msec);

    results = NULL; /* PQclear(NULL) does nothing */
  }

  if (!OK && pset.echo == PSQL_ECHO_ERRORS) {

  }

  /* If we made a temporary savepoint, possibly release/rollback */
  if (on_error_rollback_savepoint) {


    transaction_status = PQtransactionStatus(pset.db);

    switch (transaction_status) {
    case PQTRANS_INERROR:
      /* We always rollback on an error */
      svptcmd = "ROLLBACK TO pg_psql_temporary_savepoint";
      break;

    case PQTRANS_IDLE:
      /* If they are no longer in a transaction, then do nothing */
      break;

    case PQTRANS_INTRANS:

      /*
       * Do nothing if they are messing with savepoints themselves:
       * If the user did COMMIT AND CHAIN, RELEASE or ROLLBACK, our
       * savepoint is gone. If they issued a SAVEPOINT, releasing
       * ours would remove theirs.
       */
      if (results && (strcmp(PQcmdStatus(results), "COMMIT") == 0 ||
                      strcmp(PQcmdStatus(results), "SAVEPOINT") == 0 ||
                      strcmp(PQcmdStatus(results), "RELEASE") == 0 ||
                      strcmp(PQcmdStatus(results), "ROLLBACK") == 0)) {
        svptcmd = NULL;
      } else {
        svptcmd = "RELEASE pg_psql_temporary_savepoint";
      }
      break;

    case PQTRANS_ACTIVE:
    case PQTRANS_UNKNOWN:
    default:;
      OK = false;
      /* PQTRANS_UNKNOWN is expected given a broken connection. */
      if (transaction_status != PQTRANS_UNKNOWN || ConnectionUp()) {
        pg_log_error("unexpected transaction status (%d)", transaction_status);
      }
      break;
    }
















  }

  ClearOrSaveResult(results);

  /* Possible microtiming output */
  if (pset.timing) {

  }

  /* check for events that may occur during query execution */

  if (pset.encoding != PQclientEncoding(pset.db) &&
      PQclientEncoding(pset.db) >= 0) {
    /* track effects of SET CLIENT_ENCODING */
    pset.encoding = PQclientEncoding(pset.db);
    pset.popt.topt.encoding = pset.encoding;
    SetVariable(pset.vars, "ENCODING", pg_encoding_to_char(pset.encoding));
  }

  PrintNotifications();

  /* perform cleanup that should occur after any attempted query */

sendquery_cleanup:;

  /* reset \g's output-to-filename trigger */
  if (pset.gfname) {

    pset.gfname = NULL;
  }

  /* reset \gx's expanded-mode flag */
  pset.g_expanded = false;

  /* reset \gset trigger */
  if (pset.gset_prefix) {

    pset.gset_prefix = NULL;
  }

  /* reset \gdesc trigger */
  pset.gdesc_flag = false;

  /* reset \gexec trigger */
  pset.gexec_flag = false;

  /* reset \crosstabview trigger */
  pset.crosstab_flag = false;
  for (i = 0; i < lengthof(pset.ctv_args); i++) {
    pg_free(pset.ctv_args[i]);
    pset.ctv_args[i] = NULL;
  }

  return OK;
}

/*
 * DescribeQuery: describe the result columns of a query, without executing it
 *
 * Returns true if the operation executed successfully, false otherwise.
 *
 * If pset.timing is on, total query time (exclusive of result-printing) is
 * stored into *elapsed_msec.
 */
static bool DescribeQuery(const char *query, double *elapsed_msec) {





























































































}

/*
 * ExecQueryUsingCursor: run a SELECT-like query using a cursor
 *
 * This feature allows result sets larger than RAM to be dealt with.
 *
 * Returns true if the query executed successfully, false otherwise.
 *
 * If pset.timing is on, total query time (exclusive of result-printing) is
 * stored into *elapsed_msec.
 */
static bool ExecQueryUsingCursor(const char *query, double *elapsed_msec) {












































































































































































































































}

/*
 * Advance the given char pointer over white space and SQL comments.
 */
static const char *skip_white_space(const char *query) {











































}

/*
 * Check whether a command is one of those for which we should NOT start
 * a new transaction block (ie, send a preceding BEGIN).
 *
 * These include the transaction control statements themselves, plus
 * certain statements that the backend disallows inside transaction blocks.
 */
static bool command_no_begin(const char *query) {

































































































































































































































}

/*
 * Check whether the specified command is a SELECT (or VALUES).
 */
static bool is_select_command(const char *query) {































}

/*
 * Test if the current user is a database superuser.
 *
 * Note: this will correctly detect superuserness only with a protocol-3.0
 * or newer backend; otherwise it will always say "false".
 */
bool is_superuser(void) {













}

/*
 * Test if the current session uses standard string literals.
 *
 * Note: With a pre-protocol-3.0 connection this will always say "false",
 * which should be the right answer.
 */
bool standard_strings(void) {
  const char *val;

  if (!pset.db) {
    return false;
  }

  val = PQparameterStatus(pset.db, "standard_conforming_strings");

  if (val && strcmp(val, "on") == 0) {
    return true;
  }

  return false;
}

/*
 * Return the session user of the current connection.
 *
 * Note: this will correctly detect the session user only with a
 * protocol-3.0 or newer backend; otherwise it will return the
 * connection user.
 */
const char *session_username(void) {












}

/* expand_tilde
 *
 * substitute '~' with HOME or '~username' with username's home dir
 *
 */
void expand_tilde(char **filename) {















































}

/*
 * Checks if connection string starts with either of the valid URI prefix
 * designators.
 *
 * Returns the URI prefix length, 0 if the string doesn't contain a URI prefix.
 *
 * XXX This is a duplicate of the eponymous libpq function.
 */
static int uri_prefix_length(const char *connstr) {














}

/*
 * Recognized connection string either starts with a valid URI prefix or
 * contains a "=" in it.
 *
 * Must be consistent with parse_connection_string: anything for which this
 * returns true should at least look like it's parseable by that routine.
 *
 * XXX This is a duplicate of the eponymous libpq function.
 */
bool recognized_connection_string(const char *connstr) {

}
