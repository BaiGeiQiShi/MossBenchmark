/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2000-2019, PostgreSQL Global Development Group
 *
 * src/bin/psql/command.c
 */
#include "postgres_fe.h"
#include "command.h"

#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <utime.h>
#ifndef WIN32
#include <sys/stat.h> /* for stat() */
#include <fcntl.h>    /* open() flags */
#include <unistd.h>   /* for geteuid(), getpid(), stat() */
#else
#include <win32.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h> /* for stat() */
#endif

#include "catalog/pg_class_d.h"
#include "portability/instr_time.h"

#include "libpq-fe.h"
#include "pqexpbuffer.h"
#include "common/logging.h"
#include "fe_utils/print.h"
#include "fe_utils/string_utils.h"

#include "common.h"
#include "copy.h"
#include "crosstabview.h"
#include "describe.h"
#include "help.h"
#include "input.h"
#include "large_obj.h"
#include "mainloop.h"
#include "psqlscanslash.h"
#include "settings.h"
#include "variables.h"

/*
 * Editable database object types.
 */
typedef enum EditableObjectType {
  EditableFunction,
  EditableView
} EditableObjectType;

/* local function declarations */
static backslashResult exec_command(const char *cmd, PsqlScanState scan_state,
                                    ConditionalStack cstack,
                                    PQExpBuffer query_buf,
                                    PQExpBuffer previous_buf);
static backslashResult exec_command_a(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_C(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_connect(PsqlScanState scan_state,
                                            bool active_branch);
static backslashResult exec_command_cd(PsqlScanState scan_state,
                                       bool active_branch, const char *cmd);
static backslashResult exec_command_conninfo(PsqlScanState scan_state,
                                             bool active_branch);
static backslashResult exec_command_copy(PsqlScanState scan_state,
                                         bool active_branch);
static backslashResult exec_command_copyright(PsqlScanState scan_state,
                                              bool active_branch);
static backslashResult exec_command_crosstabview(PsqlScanState scan_state,
                                                 bool active_branch);
static backslashResult exec_command_d(PsqlScanState scan_state,
                                      bool active_branch, const char *cmd);
static backslashResult exec_command_edit(PsqlScanState scan_state,
                                         bool active_branch,
                                         PQExpBuffer query_buf,
                                         PQExpBuffer previous_buf);
static backslashResult exec_command_ef_ev(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf, bool is_func);
static backslashResult exec_command_echo(PsqlScanState scan_state,
                                         bool active_branch, const char *cmd);
static backslashResult exec_command_elif(PsqlScanState scan_state,
                                         ConditionalStack cstack,
                                         PQExpBuffer query_buf);
static backslashResult exec_command_else(PsqlScanState scan_state,
                                         ConditionalStack cstack,
                                         PQExpBuffer query_buf);
static backslashResult exec_command_endif(PsqlScanState scan_state,
                                          ConditionalStack cstack,
                                          PQExpBuffer query_buf);
static backslashResult exec_command_encoding(PsqlScanState scan_state,
                                             bool active_branch);
static backslashResult exec_command_errverbose(PsqlScanState scan_state,
                                               bool active_branch);
static backslashResult exec_command_f(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_g(PsqlScanState scan_state,
                                      bool active_branch, const char *cmd);
static backslashResult exec_command_gdesc(PsqlScanState scan_state,
                                          bool active_branch);
static backslashResult exec_command_gexec(PsqlScanState scan_state,
                                          bool active_branch);
static backslashResult exec_command_gset(PsqlScanState scan_state,
                                         bool active_branch);
static backslashResult exec_command_help(PsqlScanState scan_state,
                                         bool active_branch);
static backslashResult exec_command_html(PsqlScanState scan_state,
                                         bool active_branch);
static backslashResult exec_command_include(PsqlScanState scan_state,
                                            bool active_branch,
                                            const char *cmd);
static backslashResult exec_command_if(PsqlScanState scan_state,
                                       ConditionalStack cstack,
                                       PQExpBuffer query_buf);
static backslashResult exec_command_list(PsqlScanState scan_state,
                                         bool active_branch, const char *cmd);
static backslashResult exec_command_lo(PsqlScanState scan_state,
                                       bool active_branch, const char *cmd);
static backslashResult exec_command_out(PsqlScanState scan_state,
                                        bool active_branch);
static backslashResult exec_command_print(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf,
                                          PQExpBuffer previous_buf);
static backslashResult exec_command_password(PsqlScanState scan_state,
                                             bool active_branch);
static backslashResult exec_command_prompt(PsqlScanState scan_state,
                                           bool active_branch, const char *cmd);
static backslashResult exec_command_pset(PsqlScanState scan_state,
                                         bool active_branch);
static backslashResult exec_command_quit(PsqlScanState scan_state,
                                         bool active_branch);
static backslashResult exec_command_reset(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf);
static backslashResult exec_command_s(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_set(PsqlScanState scan_state,
                                        bool active_branch);
static backslashResult exec_command_setenv(PsqlScanState scan_state,
                                           bool active_branch, const char *cmd);
static backslashResult exec_command_sf_sv(PsqlScanState scan_state,
                                          bool active_branch, const char *cmd,
                                          bool is_func);
static backslashResult exec_command_t(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_T(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_timing(PsqlScanState scan_state,
                                           bool active_branch);
static backslashResult exec_command_unset(PsqlScanState scan_state,
                                          bool active_branch, const char *cmd);
static backslashResult exec_command_write(PsqlScanState scan_state,
                                          bool active_branch, const char *cmd,
                                          PQExpBuffer query_buf,
                                          PQExpBuffer previous_buf);
static backslashResult exec_command_watch(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf,
                                          PQExpBuffer previous_buf);
static backslashResult exec_command_x(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_z(PsqlScanState scan_state,
                                      bool active_branch);
static backslashResult exec_command_shell_escape(PsqlScanState scan_state,
                                                 bool active_branch);
static backslashResult exec_command_slash_command_help(PsqlScanState scan_state,
                                                       bool active_branch);
static char *read_connect_arg(PsqlScanState scan_state);
static PQExpBuffer gather_boolean_expression(PsqlScanState scan_state);
static bool is_true_boolean_expression(PsqlScanState scan_state,
                                       const char *name);
static void ignore_boolean_expression(PsqlScanState scan_state);
static void ignore_slash_options(PsqlScanState scan_state);
static void ignore_slash_filepipe(PsqlScanState scan_state);
static void ignore_slash_whole_line(PsqlScanState scan_state);
static bool is_branching_command(const char *cmd);
static void save_query_text_state(PsqlScanState scan_state,
                                  ConditionalStack cstack,
                                  PQExpBuffer query_buf);
static void discard_query_text(PsqlScanState scan_state,
                               ConditionalStack cstack, PQExpBuffer query_buf);
static void copy_previous_query(PQExpBuffer query_buf,
                                PQExpBuffer previous_buf);
static bool do_connect(enum trivalue reuse_previous_specification, char *dbname,
                       char *user, char *host, char *port);
static bool do_edit(const char *filename_arg, PQExpBuffer query_buf, int lineno,
                    bool *edited);
static bool do_shell(const char *command);
static bool do_watch(PQExpBuffer query_buf, double sleep);
static bool lookup_object_oid(EditableObjectType obj_type, const char *desc,
                              Oid *obj_oid);
static bool get_create_object_cmd(EditableObjectType obj_type, Oid oid,
                                  PQExpBuffer buf);
static int strip_lineno_from_objdesc(char *obj);
static int count_lines_in_buf(PQExpBuffer buf);
static void print_with_linenumbers(FILE *output, char *lines,
                                   const char *header_keyword);
static void minimal_error_message(PGresult *res);

static void printSSLInfo(void);
static void printGSSInfo(void);
static bool printPsetInfo(const char *param, struct printQueryOpt *popt);
static char *pset_value_string(const char *param, struct printQueryOpt *popt);

#ifdef WIN32
static void checkWin32Codepage(void);
#endif

/*----------
 * HandleSlashCmds:
 *
 * Handles all the different commands that start with '\'.
 * Ordinarily called by MainLoop().
 *
 * scan_state is a lexer working state that is set to continue scanning
 * just after the '\'.  The lexer is advanced past the command and all
 * arguments on return.
 *
 * cstack is the current \if stack state.  This will be examined, and
 * possibly modified by conditional commands.
 *
 * query_buf contains the query-so-far, which may be modified by
 * execution of the backslash command (for example, \r clears it).
 *
 * previous_buf contains the query most recently sent to the server
 * (empty if none yet).  This should not be modified here, but some
 * commands copy its content into query_buf.
 *
 * query_buf and previous_buf will be NULL when executing a "-c"
 * command-line option.
 *
 * Returns a status code indicating what action is desired, see command.h.
 *----------
 */

backslashResult HandleSlashCmds(PsqlScanState scan_state,
                                ConditionalStack cstack, PQExpBuffer query_buf,
                                PQExpBuffer previous_buf) {
  backslashResult status;
  char *cmd;
  char *arg;

  Assert(scan_state != NULL);
  Assert(cstack != NULL);

  /* Parse off the command name */
  cmd = psql_scan_slash_command(scan_state);

  /* And try to execute it */
  status = exec_command(cmd, scan_state, cstack, query_buf, previous_buf);

  if (status == PSQL_CMD_UNKNOWN) {





  }

  if (status != PSQL_CMD_ERROR) {
    /*
     * Eat any remaining arguments after a valid command.  We want to
     * suppress evaluation of backticks in this situation, so transiently
     * push an inactive conditional-stack entry.
     */
    bool active_branch = conditional_active(cstack);

    conditional_stack_push(cstack, IFSTATE_IGNORED);
    while ((arg = psql_scan_slash_option(scan_state, OT_NORMAL, NULL, false))) {
      if (active_branch) {
        pg_log_warning("\\%s: extra argument \"%s\" ignored", cmd, arg);
      }
      free(arg);
    }
    conditional_stack_pop(cstack);
  } else {
    /* silently throw away rest of line after an erroneous command */
    while ((
        arg = psql_scan_slash_option(scan_state, OT_WHOLE_LINE, NULL, false))) {
      free(arg);
    }
  }

  /* if there is a trailing \\, swallow it */
  psql_scan_slash_command_end(scan_state);

            

  /* some commands write to queryFout, so make sure output is sent */
  fflush(pset.queryFout);

                
}

/*
 * Subroutine to actually try to execute a backslash command.
 *
 * The typical "success" result code is PSQL_CMD_SKIP_LINE, although some
 * commands return something else.  Failure results are PSQL_CMD_ERROR,
 * unless PSQL_CMD_UNKNOWN is more appropriate.
 */
static backslashResult exec_command(const char *cmd, PsqlScanState scan_state,
                                    ConditionalStack cstack,
                                    PQExpBuffer query_buf,
                                    PQExpBuffer previous_buf) {
  backslashResult status;
  bool active_branch = conditional_active(cstack);

  /*
   * In interactive mode, warn when we're ignoring a command within a false
   * \if-branch.  But we continue on, so as to parse and discard the right
   * amount of parameter text.  Each individual backslash command subroutine
   * is responsible for doing nothing after discarding appropriate
   * arguments, if !active_branch.
   */
  if (pset.cur_cmd_interactive && !active_branch &&
      !is_branching_command(cmd)) {



  }

  if (strcmp(cmd, "a") == 0) {
    status = exec_command_a(scan_state, active_branch);
  } else if (strcmp(cmd, "C") == 0) {
    status = exec_command_C(scan_state, active_branch);
  } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "connect") == 0) {
    status = exec_command_connect(scan_state, active_branch);
  } else if (strcmp(cmd, "cd") == 0) {
    status = exec_command_cd(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "conninfo") == 0) {
    status = exec_command_conninfo(scan_state, active_branch);
  } else if (pg_strcasecmp(cmd, "copy") == 0) {
    status = exec_command_copy(scan_state, active_branch);
  } else if (strcmp(cmd, "copyright") == 0) {
    status = exec_command_copyright(scan_state, active_branch);
  } else if (strcmp(cmd, "crosstabview") == 0) {
    status = exec_command_crosstabview(scan_state, active_branch);
  } else if (cmd[0] == 'd') {
    status = exec_command_d(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "e") == 0 || strcmp(cmd, "edit") == 0) {
    status =
        exec_command_edit(scan_state, active_branch, query_buf, previous_buf);
  } else if (strcmp(cmd, "ef") == 0) {
    status = exec_command_ef_ev(scan_state, active_branch, query_buf, true);
  } else if (strcmp(cmd, "ev") == 0) {
    status = exec_command_ef_ev(scan_state, active_branch, query_buf, false);
  } else if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "qecho") == 0) {
    status = exec_command_echo(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "elif") == 0) {
    status = exec_command_elif(scan_state, cstack, query_buf);
  } else if (strcmp(cmd, "else") == 0) {
    status = exec_command_else(scan_state, cstack, query_buf);
  } else if (strcmp(cmd, "endif") == 0) {
    status = exec_command_endif(scan_state, cstack, query_buf);
  } else if (strcmp(cmd, "encoding") == 0) {
    status = exec_command_encoding(scan_state, active_branch);
  } else if (strcmp(cmd, "errverbose") == 0) {
    status = exec_command_errverbose(scan_state, active_branch);
  } else if (strcmp(cmd, "f") == 0) {
    status = exec_command_f(scan_state, active_branch);
  } else if (strcmp(cmd, "g") == 0 || strcmp(cmd, "gx") == 0) {
    status = exec_command_g(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "gdesc") == 0) {
    status = exec_command_gdesc(scan_state, active_branch);
  } else if (strcmp(cmd, "gexec") == 0) {
    status = exec_command_gexec(scan_state, active_branch);
  } else if (strcmp(cmd, "gset") == 0) {
    status = exec_command_gset(scan_state, active_branch);
  } else if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
    status = exec_command_help(scan_state, active_branch);
  } else if (strcmp(cmd, "H") == 0 || strcmp(cmd, "html") == 0) {
    status = exec_command_html(scan_state, active_branch);
  } else if (strcmp(cmd, "i") == 0 || strcmp(cmd, "include") == 0 ||
             strcmp(cmd, "ir") == 0 || strcmp(cmd, "include_relative") == 0) {
    status = exec_command_include(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "if") == 0) {
    status = exec_command_if(scan_state, cstack, query_buf);
  } else if (strcmp(cmd, "l") == 0 || strcmp(cmd, "list") == 0 ||
             strcmp(cmd, "l+") == 0 || strcmp(cmd, "list+") == 0) {
    status = exec_command_list(scan_state, active_branch, cmd);
  } else if (strncmp(cmd, "lo_", 3) == 0) {
    status = exec_command_lo(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "o") == 0 || strcmp(cmd, "out") == 0) {
    status = exec_command_out(scan_state, active_branch);
  } else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "print") == 0) {
    status =
        exec_command_print(scan_state, active_branch, query_buf, previous_buf);
  } else if (strcmp(cmd, "password") == 0) {
    status = exec_command_password(scan_state, active_branch);
  } else if (strcmp(cmd, "prompt") == 0) {
    status = exec_command_prompt(scan_state, active_branch, cmd);
  } else if (strcmp(cmd, "pset") == 0) {
    status = exec_command_pset(scan_state, active_branch);
  } else if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
    status = exec_command_quit(scan_state, active_branch);






































  /*
   * All the commands that return PSQL_CMD_SEND want to execute previous_buf
   * if query_buf is empty.  For convenience we implement that here, not in
   * the individual command subroutines.
   */
  if (status == PSQL_CMD_SEND) {
    copy_previous_query(query_buf, previous_buf);
  }

                
}

/*
 * \a -- toggle field alignment
 *
 * This makes little sense but we keep it around.
 */
static backslashResult exec_command_a(PsqlScanState scan_state,
                                      bool active_branch) {











}

/*
 * \C -- override table title (formerly change HTML caption)
 */
static backslashResult exec_command_C(PsqlScanState scan_state,
                                      bool active_branch) {












}

/*
 * \c or \connect -- connect to database using the specified parameters.
 *
 * \c [-reuse-previous=BOOL] dbname user host port
 *
 * Specifying a parameter as '-' is equivalent to omitting it.  Examples:
 *
 * \c - - hst		Connect to current database on current port of
 *					host "hst" as current user.
 * \c - usr - prt	Connect to current database on port "prt" of current
 *host as user "usr". \c dbs			Connect to database "dbs" on
 *current port of current host as current user.
 */
static backslashResult exec_command_connect(PsqlScanState scan_state,
                                            bool active_branch) {






































}

/*
 * \cd -- change directory
 */
static backslashResult exec_command_cd(PsqlScanState scan_state,
                                       bool active_branch, const char *cmd) {













































}

/*
 * \conninfo -- display information about the current connection
 */
static backslashResult exec_command_conninfo(PsqlScanState scan_state,
                                             bool active_branch) {








































}

/*
 * \copy -- run a COPY command
 */
static backslashResult exec_command_copy(PsqlScanState scan_state,
                                         bool active_branch) {












}

/*
 * \copyright -- print copyright notice
 */
static backslashResult exec_command_copyright(PsqlScanState scan_state,
                                              bool active_branch) {





}

/*
 * \crosstabview -- execute a query and display results in crosstab
 */
static backslashResult exec_command_crosstabview(PsqlScanState scan_state,
                                                 bool active_branch) {
















}

/*
 * \d* commands
 */
static backslashResult exec_command_d(PsqlScanState scan_state,
                                      bool active_branch, const char *cmd) {






















































































































































































































}

/*
 * \e or \edit -- edit the current query buffer, or edit a file and
 * make it the query buffer
 */
static backslashResult exec_command_edit(PsqlScanState scan_state,
                                         bool active_branch,
                                         PQExpBuffer query_buf,
                                         PQExpBuffer previous_buf) {


























































}

/*
 * \ef/\ev -- edit the named function/view, or
 * present a blank CREATE FUNCTION/VIEW template if no argument is given
 */
static backslashResult exec_command_ef_ev(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf, bool is_func) {





































































































}

/*
 * \echo and \qecho -- echo arguments to stdout or query output
 */
static backslashResult exec_command_echo(PsqlScanState scan_state,
                                         bool active_branch, const char *cmd) {



































}

/*
 * \encoding -- set/show client side encoding
 */
static backslashResult exec_command_encoding(PsqlScanState scan_state,
                                             bool active_branch) {

























}

/*
 * \errverbose -- display verbose message from last failed query
 */
static backslashResult exec_command_errverbose(PsqlScanState scan_state,
                                               bool active_branch) {


















}

/*
 * \f -- change field separator
 */
static backslashResult exec_command_f(PsqlScanState scan_state,
                                      bool active_branch) {












}

/*
 * \g [filename] -- send query, optionally with output to file/pipe
 * \gx [filename] -- same as \g, with expanded mode forced
 */
static backslashResult exec_command_g(PsqlScanState scan_state,
                                      bool active_branch, const char *cmd) {





















}

/*
 * \gdesc -- describe query result
 */
static backslashResult exec_command_gdesc(PsqlScanState scan_state,
                                          bool active_branch) {








}

/*
 * \gexec -- send query and execute each field of result
 */
static backslashResult exec_command_gexec(PsqlScanState scan_state,
                                          bool active_branch) {








}

/*
 * \gset [prefix] -- send query and store result into variables
 */
static backslashResult exec_command_gset(PsqlScanState scan_state,
                                         bool active_branch) {


















}

/*
 * \help [topic] -- print help about SQL commands
 */
static backslashResult exec_command_help(PsqlScanState scan_state,
                                         bool active_branch) {




















}

/*
 * \H and \html -- toggle HTML formatting
 */
static backslashResult exec_command_html(PsqlScanState scan_state,
                                         bool active_branch) {











}

/*
 * \i and \ir -- include a file
 */
static backslashResult exec_command_include(PsqlScanState scan_state,
                                            bool active_branch,
                                            const char *cmd) {






















}

/*
 * \if <expr> -- beginning of an \if..\endif block
 *
 * <expr> is parsed as a boolean expression.  Invalid expressions will emit a
 * warning and be treated as false.  Statements that follow a false expression
 * will be parsed but ignored.  Note that in the case where an \if statement
 * is itself within an inactive section of a block, then the entire inner
 * \if..\endif block will be parsed but ignored.
 */
static backslashResult exec_command_if(PsqlScanState scan_state,
                                       ConditionalStack cstack,
                                       PQExpBuffer query_buf) {

































}

/*
 * \elif <expr> -- alternative branch in an \if..\endif block
 *
 * <expr> is evaluated the same as in \if <expr>.
 */
static backslashResult exec_command_elif(PsqlScanState scan_state,
                                         ConditionalStack cstack,
                                         PQExpBuffer query_buf) {































































}

/*
 * \else -- final alternative in an \if..\endif block
 *
 * Statements within an \else branch will only be executed if
 * all previous \if and \elif expressions evaluated to false
 * and the block was not itself being ignored.
 */
static backslashResult exec_command_else(PsqlScanState scan_state,
                                         ConditionalStack cstack,
                                         PQExpBuffer query_buf) {























































}

/*
 * \endif -- ends an \if...\endif block
 */
static backslashResult exec_command_endif(PsqlScanState scan_state,
                                          ConditionalStack cstack,
                                          PQExpBuffer query_buf) {






























}

/*
 * \l -- list databases
 */
static backslashResult exec_command_list(PsqlScanState scan_state,
                                         bool active_branch, const char *cmd) {




















}

/*
 * \lo_* -- large object operations
 */
static backslashResult exec_command_lo(PsqlScanState scan_state,
                                       bool active_branch, const char *cmd) {

























































}

/*
 * \o -- set query output
 */
static backslashResult exec_command_out(PsqlScanState scan_state,
                                        bool active_branch) {













}

/*
 * \p -- print the current query buffer
 */
static backslashResult exec_command_print(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf,
                                          PQExpBuffer previous_buf) {

















}

/*
 * \password -- set user password
 */
static backslashResult exec_command_password(PsqlScanState scan_state,
                                             bool active_branch) {




























































}

/*
 * \prompt -- prompt and set variable
 */
static backslashResult exec_command_prompt(PsqlScanState scan_state,
                                           bool active_branch,
                                           const char *cmd) {






















































}

/*
 * \pset -- set printing parameters
 */
static backslashResult exec_command_pset(PsqlScanState scan_state,
                                         bool active_branch) {
  bool success = true;

  if (active_branch) {
    char *opt0 = psql_scan_slash_option(scan_state, OT_NORMAL, NULL, false);
    char *opt1 = psql_scan_slash_option(scan_state, OT_NORMAL, NULL, false);

    if (!opt0) {
      /* list all variables */

























      for (i = 0; my_list[i] != NULL; i++) {
        char *val = pset_value_string(my_list[i], &pset.popt);

        printf("%-24s %s\n", my_list[i], val);
        free(val);
      }

      success = true;
    } else {
      success = do_pset(opt0, opt1, &pset.popt, pset.quiet);
    }

    free(opt0);
    free(opt1);
  } else {
    ignore_slash_options(scan_state);
  }

  return success ? PSQL_CMD_SKIP_LINE : PSQL_CMD_ERROR;
}

/*
 * \q or \quit -- exit psql
 */
static backslashResult exec_command_quit(PsqlScanState scan_state,
                                         bool active_branch) {







}

/*
 * \r -- reset (clear) the query buffer
 */
static backslashResult exec_command_reset(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf) {









}

/*
 * \s -- save history in a file or show it on the screen
 */
static backslashResult exec_command_s(PsqlScanState scan_state,
                                      bool active_branch) {



















}

/*
 * \set -- set variable
 */
static backslashResult exec_command_set(PsqlScanState scan_state,
                                        bool active_branch) {







































}

/*
 * \setenv -- set environment variable
 */
static backslashResult exec_command_setenv(PsqlScanState scan_state,
                                           bool active_branch,
                                           const char *cmd) {






































}

/*
 * \sf/\sv -- show a function/view's source code
 */
static backslashResult exec_command_sf_sv(PsqlScanState scan_state,
                                          bool active_branch, const char *cmd,
                                          bool is_func) {



















































































}

/*
 * \t -- turn off table headers and row count
 */
static backslashResult exec_command_t(PsqlScanState scan_state,
                                      bool active_branch) {












}

/*
 * \T -- define html <table ...> attributes
 */
static backslashResult exec_command_T(PsqlScanState scan_state,
                                      bool active_branch) {












}

/*
 * \timing -- enable/disable timing of queries
 */
static backslashResult exec_command_timing(PsqlScanState scan_state,
                                           bool active_branch) {























}

/*
 * \unset -- unset variable
 */
static backslashResult exec_command_unset(PsqlScanState scan_state,
                                          bool active_branch, const char *cmd) {


















}

/*
 * \w -- write query buffer to file
 */
static backslashResult exec_command_write(PsqlScanState scan_state,
                                          bool active_branch, const char *cmd,
                                          PQExpBuffer query_buf,
                                          PQExpBuffer previous_buf) {




































































}

/*
 * \watch -- execute a query every N seconds
 */
static backslashResult exec_command_watch(PsqlScanState scan_state,
                                          bool active_branch,
                                          PQExpBuffer query_buf,
                                          PQExpBuffer previous_buf) {




























}

/*
 * \x -- set or toggle expanded table representation
 */
static backslashResult exec_command_x(PsqlScanState scan_state,
                                      bool active_branch) {












}

/*
 * \z -- list table privileges (equivalent to \dp)
 */
static backslashResult exec_command_z(PsqlScanState scan_state,
                                      bool active_branch) {














}

/*
 * \! -- execute shell command
 */
static backslashResult exec_command_shell_escape(PsqlScanState scan_state,
                                                 bool active_branch) {












}

/*
 * \? -- print help about backslash commands
 */
static backslashResult exec_command_slash_command_help(PsqlScanState scan_state,
                                                       bool active_branch) {





















}

/*
 * Read and interpret an argument to the \connect slash command.
 *
 * Returns a malloc'd string, or NULL if no/empty argument.
 */
static char *read_connect_arg(PsqlScanState scan_state) {




























}

/*
 * Read a boolean expression, return it as a PQExpBuffer string.
 *
 * Note: anything more or less than one token will certainly fail to be
 * parsed by ParseVariableBool, so we don't worry about complaining here.
 * This routine's return data structure will need to be rethought anyway
 * to support likely future extensions such as "\if defined VARNAME".
 */
static PQExpBuffer gather_boolean_expression(PsqlScanState scan_state) {

















}

/*
 * Read a boolean expression, return true if the expression
 * was a valid boolean expression that evaluated to true.
 * Otherwise return false.
 *
 * Note: conditional stack's top state must be active, else lexer will
 * fail to expand variables and backticks.
 */
static bool is_true_boolean_expression(PsqlScanState scan_state,
                                       const char *name) {






}

/*
 * Read a boolean expression, but do nothing with it.
 *
 * Note: conditional stack's top state must be INACTIVE, else lexer will
 * expand variables and backticks, which we do not want here.
 */
static void ignore_boolean_expression(PsqlScanState scan_state) {



}

/*
 * Read and discard "normal" slash command options.
 *
 * This should be used for inactive-branch processing of any slash command
 * that eats one or more OT_NORMAL, OT_SQLID, or OT_SQLIDHACK parameters.
 * We don't need to worry about exactly how many it would eat, since the
 * cleanup logic in HandleSlashCmds would silently discard any extras anyway.
 */
static void ignore_slash_options(PsqlScanState scan_state) {






}

/*
 * Read and discard FILEPIPE slash command argument.
 *
 * This *MUST* be used for inactive-branch processing of any slash command
 * that takes an OT_FILEPIPE option.  Otherwise we might consume a different
 * amount of option text in active and inactive cases.
 */
static void ignore_slash_filepipe(PsqlScanState scan_state) {





}

/*
 * Read and discard whole-line slash command argument.
 *
 * This *MUST* be used for inactive-branch processing of any slash command
 * that takes an OT_WHOLE_LINE option.  Otherwise we might consume a different
 * amount of option text in active and inactive cases.
 */
static void ignore_slash_whole_line(PsqlScanState scan_state) {





}

/*
 * Return true if the command given is a branching command.
 */
static bool is_branching_command(const char *cmd) {


}

/*
 * Prepare to possibly restore query buffer to its current state
 * (cf. discard_query_text).
 *
 * We need to remember the length of the query buffer, and the lexer's
 * notion of the parenthesis nesting depth.
 */
static void save_query_text_state(PsqlScanState scan_state,
                                  ConditionalStack cstack,
                                  PQExpBuffer query_buf) {





}

/*
 * Discard any query text absorbed during an inactive conditional branch.
 *
 * We must discard data that was appended to query_buf during an inactive
 * \if branch.  We don't have to do anything there if there's no query_buf.
 *
 * Also, reset the lexer state to the same paren depth there was before.
 * (The rest of its state doesn't need attention, since we could not be
 * inside a comment or literal or partial token.)
 */
static void discard_query_text(PsqlScanState scan_state,
                               ConditionalStack cstack, PQExpBuffer query_buf) {









}

/*
 * If query_buf is empty, copy previous_buf into it.
 *
 * This is used by various slash commands for which re-execution of a
 * previous query is a common usage.  For convenience, we allow the
 * case of query_buf == NULL (and do nothing).
 */
static void copy_previous_query(PQExpBuffer query_buf,
                                PQExpBuffer previous_buf) {



}

/*
 * Ask the user for a password; 'username' is the username the
 * password is for, if one has been explicitly specified. Returns a
 * malloc'd string.
 */
static char *prompt_for_password(const char *username) {












}

static bool param_is_newly_set(const char *old_val, const char *new_val) {









}

/*
 * do_connect -- handler for \connect
 *
 * Connects to a database with given parameters. Absent an established
 * connection, all parameters are required. Given -reuse-previous=off or a
 * connection string without -reuse-previous=on, NULL values will pass through
 * to PQconnectdbParams(), so the libpq defaults will be used. Otherwise, NULL
 * values will be replaced with the ones in the current connection.
 *
 * In interactive mode, if connection fails with the given parameters,
 * the old connection will be kept.
 */
static bool do_connect(enum trivalue reuse_previous_specification, char *dbname,
                       char *user, char *host, char *port) {





























































































































































































































































































































































































































}

void connection_warnings(bool in_startup) {









































}

/*
 * printSSLInfo
 *
 * Prints information about the current SSL connection, if SSL is in use
 */
static void printSSLInfo(void) {



















}

/*
 * printGSSInfo
 *
 * Prints information about the current GSSAPI connection, if GSSAPI encryption
 * is in use
 */
static void printGSSInfo(void) {





}

/*
 * checkWin32Codepage
 *
 * Prints a warning when win32 console codepage differs from Windows codepage
 */
#ifdef WIN32
static void checkWin32Codepage(void) {
  unsigned int wincp, concp;

  wincp = GetACP();
  concp = GetConsoleCP();
  if (wincp != concp) {
    printf(_("WARNING: Console code page (%u) differs from Windows code page "
             "(%u)\n"
             "         8-bit characters might not work correctly. See psql "
             "reference\n"
             "         page \"Notes for Windows users\" for details.\n"),
           concp, wincp);
  }
}
#endif

/*
 * SyncVariables
 *
 * Make psql's internal variables agree with connection state upon
 * establishing a new connection.
 */
void SyncVariables(void) {
  char vbuf[32];
  const char *server_version;

  /* get stuff from connection */
  pset.encoding = PQclientEncoding(pset.db);
  pset.popt.topt.encoding = pset.encoding;
  pset.sversion = PQserverVersion(pset.db);

  SetVariable(pset.vars, "DBNAME", PQdb(pset.db));
  SetVariable(pset.vars, "USER", PQuser(pset.db));
  SetVariable(pset.vars, "HOST", PQhost(pset.db));
  SetVariable(pset.vars, "PORT", PQport(pset.db));
  SetVariable(pset.vars, "ENCODING", pg_encoding_to_char(pset.encoding));

  /* this bit should match connection_warnings(): */
  /* Try to get full text form of version, might include "devel" etc */
  server_version = PQparameterStatus(pset.db, "server_version");
  /* Otherwise fall back on pset.sversion */
  if (!server_version) {
    formatPGVersionNumber(pset.sversion, true, vbuf, sizeof(vbuf));

  }
  SetVariable(pset.vars, "SERVER_VERSION_NAME", server_version);

  snprintf(vbuf, sizeof(vbuf), "%d", pset.sversion);
  SetVariable(pset.vars, "SERVER_VERSION_NUM", vbuf);

  /* send stuff to it, too */
  PQsetErrorVerbosity(pset.db, pset.verbosity);
  PQsetErrorContextVisibility(pset.db, pset.show_context);
}

/*
 * UnsyncVariables
 *
 * Clear variables that should be not be set when there is no connection.
 */
void UnsyncVariables(void) {







}

/*
 * do_edit -- handler for \e
 *
 * If you do not specify a filename, the current query buffer will be copied
 * into a temporary one.
 */
static bool editFile(const char *fname, int lineno) {

































































}

/* call this one */
static bool do_edit(const char *filename_arg, PQExpBuffer query_buf, int lineno,
                    bool *edited) {





















































































































































}

/*
 * process_file
 *
 * Reads commands from filename and passes them to the main processing loop.
 * Handler for \i and \ir, but can be used for other things as well.  Returns
 * MainLoop() error code.
 *
 * If use_relative_path is true and filename is not an absolute path, then open
 * the file from where the currently processed file (if any) is located.
 */
int process_file(char *filename, bool use_relative_path) {
  FILE *fd;
  int result;
  char *oldfilename;
  char relpath[MAXPGPATH];

  if (!filename) {
    fd = stdin;
    filename = NULL;
  } else if (strcmp(filename, "-") != 0) {


    /*
     * If we were asked to resolve the pathname relative to the location
     * of the currently executing script, and there is one, and this is a
     * relative pathname, then prepend all but the last pathname component
     * of the current script to this pathname.
     */










    fd = fopen(filename, PG_BINARY_R);





  } else {
    fd = stdin;

  }

  oldfilename = pset.inputfile;
  pset.inputfile = filename;

  pg_logging_config(pset.inputfile ? 0 : PG_LOG_FLAG_TERSE);

  result = MainLoop(fd);

  if (fd != stdin) {
    fclose(fd);
  }

  pset.inputfile = oldfilename;

  pg_logging_config(pset.inputfile ? 0 : PG_LOG_FLAG_TERSE);

  return result;
}

static const char *_align2string(enum printFormat in) {

































}

/*
 * Parse entered Unicode linestyle.  If ok, update *linestyle and return
 * true, else return false.
 */
static bool set_unicode_line_style(const char *value, size_t vallen,
                                   unicode_linestyle *linestyle) {








}

static const char *_unicode_linestyle2string(int linestyle) {









}

/*
 * do_pset
 *
 */
bool do_pset(const char *param, const char *value, printQueryOpt *popt,
             bool quiet) {
  size_t vallen = 0;

  Assert(param != NULL);

  if (value) {
    vallen = strlen(value);
  }

  /* set format */
  if (strcmp(param, "format") == 0) {













    if (!value)

    else {





























    }
  }

  /* set table line style */
  else if (strcmp(param, "linestyle") == 0) {












  }

  /* set unicode border line style */
  else if (strcmp(param, "unicode_border_linestyle") == 0) {










  }

  /* set unicode column line style */
  else if (strcmp(param, "unicode_column_linestyle") == 0) {










  }

  /* set unicode header line style */
  else if (strcmp(param, "unicode_header_linestyle") == 0) {










  }

  /* set border style/width */
  else if (strcmp(param, "border") == 0) {



  }

  /* set expanded/vertical mode */
  else if (strcmp(param, "x") == 0 || strcmp(param, "expanded") == 0 ||
           strcmp(param, "vertical") == 0) {














  }

  /* field separator for CSV format */
  else if (strcmp(param, "csv_fieldsep") == 0) {














  }

  /* locale-aware numeric output */
  else if (strcmp(param, "numericlocale") == 0) {





  }

  /* null display */
  else if (strcmp(param, "null") == 0) {
    if (value) {
      free(popt->nullPrint);
      popt->nullPrint = pg_strdup(value);
    }
  }

  /* field separator for unaligned text */







































































































  if (!quiet) {
    printPsetInfo(param, &pset.popt);
  }

  return true;
}

static bool printPsetInfo(const char *param, struct printQueryOpt *popt) {




































































































































































}

static const char *pset_bool_string(bool val) {
}

static char *pset_quoted_string(const char *str) {





















}

/*
 * Return a malloc'ed string for the \pset value.
 *
 * Note that for some string parameters, print.c distinguishes between unset
 * and empty string, but for others it doesn't.  This function should produce
 * output that produces the correct setting when fed back into \pset.
 */
static char *pset_value_string(const char *param, struct printQueryOpt *popt) {























































}

#ifndef WIN32
#define DEFAULT_SHELL "/bin/sh"
#else
/*
 *	CMD.EXE is in different places in different Win32 releases so we
 *	have to rely on the path to find it.
 */
#define DEFAULT_SHELL "cmd.exe"
#endif

static bool do_shell(const char *command) {

































}

/*
 * do_watch -- handler for \watch
 *
 * We break this out of exec_command to avoid having to plaster "volatile"
 * onto a bunch of exec_command's variables to silence stupider compilers.
 */
static bool do_watch(PQExpBuffer query_buf, double sleep) {









































































































}

/*
 * a little code borrowed from PSQLexec() to manage ECHO_HIDDEN output.
 * returns true unless we have ECHO_HIDDEN_NOEXEC.
 */
static bool echo_hidden_command(const char *query) {




















}

/*
 * Look up the object identified by obj_type and desc.  If successful,
 * store its OID in *obj_oid and return true, else return false.
 *
 * Note that we'll fail if the object doesn't exist OR if there are multiple
 * matching candidates OR if there's something syntactically wrong with the
 * object description; unfortunately it can be hard to tell the difference.
 */
static bool lookup_object_oid(EditableObjectType obj_type, const char *desc,
                              Oid *obj_oid) {















































}

/*
 * Construct a "CREATE OR REPLACE ..." command that describes the specified
 * database object.  If successful, the result is stored in buf.
 */
static bool get_create_object_cmd(EditableObjectType obj_type, Oid oid,
                                  PQExpBuffer buf) {
















































































































































}

/*
 * If the given argument of \ef or \ev ends with a line number, delete the line
 * number from the argument string and return it as an integer.  (We need
 * this kluge because we're too lazy to parse \ef's function or \ev's view
 * argument carefully --- we just slop it up in OT_WHOLE_LINE mode.)
 *
 * Returns -1 if no line number is present, 0 on error, or a positive value
 * on success.
 */
static int strip_lineno_from_objdesc(char *obj) {





















































}

/*
 * Count number of lines in the buffer.
 * This is used to test if pager is needed or not.
 */
static int count_lines_in_buf(PQExpBuffer buf) {














}

/*
 * Write text at *lines to output with line numbers.
 *
 * If header_keyword isn't NULL, then line 1 should be the first line beginning
 * with header_keyword; lines before that are unnumbered.
 *
 * Caution: this scribbles on *lines.
 */
static void print_with_linenumbers(FILE *output, char *lines,
                                   const char *header_keyword) {



































}

/*
 * Report just the primary error; this is to avoid cluttering the output
 * with, for instance, a redisplay of the internally generated query
 */
static void minimal_error_message(PGresult *res) {






















}
