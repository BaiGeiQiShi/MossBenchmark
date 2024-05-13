/*-------------------------------------------------------------------------
 *
 * hba.c
 *	  Routines to handle host based authentication (that's the scheme
 *	  wherein you authenticate a user by seeing what IP address the system
 *	  says he comes from and choosing authentication method based on it).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/libpq/hba.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "common/ip.h"
#include "funcapi.h"
#include "libpq/ifaddr.h"
#include "libpq/libpq.h"
#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "regex/regex.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/varlena.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#ifdef USE_LDAP
#ifdef WIN32
#include <winldap.h>
#else
#include <ldap.h>
#endif
#endif

#define MAX_TOKEN 256
#define MAX_LINE 8192

/* callback data for check_network_callback */
typedef struct check_network_data {
  IPCompareMethod method; /* test method */
  SockAddr *raddr;        /* client's actual address */
  bool result;            /* set to true if match */
} check_network_data;

#define token_is_keyword(t, k) (!t->quoted && strcmp(t->string, k) == 0)
#define token_matches(t, k) (strcmp(t->string, k) == 0)

/*
 * A single string token lexed from a config file, together with whether
 * the token had been quoted.
 */
typedef struct HbaToken {
  char *string;
  bool quoted;
} HbaToken;

/*
 * TokenizedLine represents one line lexed from a config file.
 * Each item in the "fields" list is a sub-list of HbaTokens.
 * We don't emit a TokenizedLine for empty or all-comment lines,
 * so "fields" is never NIL (nor are any of its sub-lists).
 * Exception: if an error occurs during tokenization, we might
 * have fields == NIL, in which case err_msg != NULL.
 */
typedef struct TokenizedLine {
  List *fields;   /* List of lists of HbaTokens */
  int line_num;   /* Line number */
  char *raw_line; /* Raw line text */
  char *err_msg;  /* Error message if any */
} TokenizedLine;

/*
 * pre-parsed content of HBA config file: list of HbaLine structs.
 * parsed_hba_context is the memory context where it lives.
 */
static List *parsed_hba_lines = NIL;
static MemoryContext parsed_hba_context = NULL;

/*
 * pre-parsed content of ident mapping file: list of IdentLine structs.
 * parsed_ident_context is the memory context where it lives.
 *
 * NOTE: the IdentLine structs can contain pre-compiled regular expressions
 * that live outside the memory context. Before destroying or resetting the
 * memory context, they need to be explicitly free'd.
 */
static List *parsed_ident_lines = NIL;
static MemoryContext parsed_ident_context = NULL;

/*
 * The following character array represents the names of the authentication
 * methods that are supported by PostgreSQL.
 *
 * Note: keep this in sync with the UserAuth enum in hba.h.
 */
static const char *const UserAuthName[] = {"reject", "implicit reject", /* Not a user-visible option */
    "trust", "ident", "password", "md5", "scram-sha-256", "gss", "sspi", "pam", "bsd", "ldap", "cert", "radius", "peer"};

static MemoryContext
tokenize_file(const char *filename, FILE *file, List **tok_lines, int elevel);
static List *
tokenize_inc_file(List *tokens, const char *outer_filename, const char *inc_filename, int elevel, char **err_msg);
static bool
parse_hba_auth_opt(char *name, char *val, HbaLine *hbaline, int elevel, char **err_msg);
static bool
verify_option_list_length(List *options, const char *optionname, List *masters, const char *mastername, int line_num);
static ArrayType *
gethba_options(HbaLine *hba);
static void
fill_hba_line(Tuplestorestate *tuple_store, TupleDesc tupdesc, int lineno, HbaLine *hba, const char *err_msg);
static void
fill_hba_view(Tuplestorestate *tuple_store, TupleDesc tupdesc);

/*
 * isblank() exists in the ISO C99 spec, but it's not very portable yet,
 * so provide our own version.
 */
bool
pg_isblank(const char c)
{
  return c == ' ' || c == '\t' || c == '\r';
}

/*
 * Grab one token out of the string pointed to by *lineptr.
 *
 * Tokens are strings of non-blank
 * characters bounded by blank characters, commas, beginning of line, and
 * end of line. Blank means space or tab. Tokens can be delimited by
 * double quotes (this allows the inclusion of blanks, but not newlines).
 * Comments (started by an unquoted '#') are skipped.
 *
 * The token, if any, is returned at *buf (a buffer of size bufsz), and
 * *lineptr is advanced past the token.
 *
 * Also, we set *initial_quote to indicate whether there was quoting before
 * the first character.  (We use that to prevent "@x" from being treated
 * as a file inclusion request.  Note that @"x" should be so treated;
 * we want to allow that to support embedded spaces in file paths.)
 *
 * We set *terminating_comma to indicate whether the token is terminated by a
 * comma (which is not returned).
 *
 * In event of an error, log a message at ereport level elevel, and also
 * set *err_msg to a string describing the error.  Currently the only
 * possible error is token too long for buf.
 *
 * If successful: store null-terminated token at *buf and return true.
 * If no more tokens on line: set *buf = '\0' and return false.
 * If error: fill buf with truncated or misformatted token and return false.
 */
static bool
next_token(char **lineptr, char *buf, int bufsz, bool *initial_quote, bool *terminating_comma, int elevel, char **err_msg)
{
  int c;
  char *start_buf = buf;
  char *end_buf = buf + (bufsz - 1);
  bool in_quote = false;
  bool was_quote = false;
  bool saw_quote = false;

  Assert(end_buf > start_buf);

  *initial_quote = false;
  *terminating_comma = false;

  /* Move over any whitespace and commas preceding the next token */
  while ((c = (*(*lineptr)++)) != '\0' && (pg_isblank(c) || c == ','))
    ;

  /*
   * Build a token in buf of next characters up to EOL, unquoted comma, or
   * unquoted whitespace.
   */
  while (c != '\0' && (!pg_isblank(c) || in_quote)) {
    /* skip comments to EOL */
    if (c == '#' && !in_quote) {
      while ((c = (*(*lineptr)++)) != '\0')
        ;
      break;
    }

    if (buf >= end_buf) {



      /* Discard remainder of line */


      /* Un-eat the '\0', in case we're called again */


    }

    /* we do not pass back a terminating comma in the token */
    if (c == ',' && !in_quote) {


    }

    if (c != '"' || was_quote) {
      *buf++ = c;
    }

    /* Literal double-quote is two double-quotes */
    if (in_quote && c == '"') {

    } else {
      was_quote = false;
    }

    if (c == '"') {





    }

    c = *(*lineptr)++;
  }

  /*
   * Un-eat the char right after the token (critical in case it is '\0',
   * else next call will read past end of string).
   */
  (*lineptr)--;

  *buf = '\0';

  return (saw_quote || buf > start_buf);
}

/*
 * Construct a palloc'd HbaToken struct, copying the given string.
 */
static HbaToken *
make_hba_token(const char *token, bool quoted)
{
  HbaToken *hbatoken;
  int toklen;

  toklen = strlen(token);
  /* we copy string into same palloc block as the struct */
  hbatoken = (HbaToken *)palloc(sizeof(HbaToken) + toklen + 1);
  hbatoken->string = (char *)hbatoken + sizeof(HbaToken);
  hbatoken->quoted = quoted;
  memcpy(hbatoken->string, token, toklen + 1);

  return hbatoken;
}

/*
 * Copy a HbaToken struct into freshly palloc'd memory.
 */
static HbaToken *
copy_hba_token(HbaToken *in)
{
  HbaToken *out = make_hba_token(in->string, in->quoted);

  return out;
}

/*
 * Tokenize one HBA field from a line, handling file inclusion and comma lists.
 *
 * filename: current file's pathname (needed to resolve relative pathnames)
 * *lineptr: current line pointer, which will be advanced past field
 *
 * In event of an error, log a message at ereport level elevel, and also
 * set *err_msg to a string describing the error.  Note that the result
 * may be non-NIL anyway, so *err_msg must be tested to determine whether
 * there was an error.
 *
 * The result is a List of HbaToken structs, one for each token in the field,
 * or NIL if we reached EOL.
 */
static List *
next_field_expand(const char *filename, char **lineptr, int elevel, char **err_msg)
{
  char buf[MAX_TOKEN];
  bool trailing_comma;
  bool initial_quote;
  List *tokens = NIL;

  do {
    if (!next_token(lineptr, buf, sizeof(buf), &initial_quote, &trailing_comma, elevel, err_msg)) {
      break;
    }

    /* Is this referencing a file? */
    if (!initial_quote && buf[0] == '@' && buf[1] != '\0') {

    } else {
      tokens = lappend(tokens, make_hba_token(buf, initial_quote));
    }
  } while (trailing_comma && (*err_msg == NULL));

  return tokens;
}

/*
 * tokenize_inc_file
 *		Expand a file included from another file into an hba "field"
 *
 * Opens and tokenises a file included from another HBA config file with @,
 * and returns all values found therein as a flat list of HbaTokens.  If a
 * @-token is found, recursively expand it.  The newly read tokens are
 * appended to "tokens" (so that foo,bar,@baz does what you expect).
 * All new tokens are allocated in caller's memory context.
 *
 * In event of an error, log a message at ereport level elevel, and also
 * set *err_msg to a string describing the error.  Note that the result
 * may be non-NIL anyway, so *err_msg must be tested to determine whether
 * there was an error.
 */
static List *
tokenize_inc_file(List *tokens, const char *outer_filename, const char *inc_filename, int elevel, char **err_msg)
{




























































}

/*
 * Tokenize the given file.
 *
 * The output is a list of TokenizedLine structs; see struct definition above.
 *
 * filename: the absolute path to the target file
 * file: the already-opened target file
 * tok_lines: receives output list
 * elevel: message logging level
 *
 * Errors are reported by logging messages at ereport level elevel and by
 * adding TokenizedLine structs containing non-null err_msg fields to the
 * output list.
 *
 * Return value is a memory context which contains all memory allocated by
 * this function (it's a child of caller's context).
 */
static MemoryContext
tokenize_file(const char *filename, FILE *file, List **tok_lines, int elevel)
{
  int line_number = 1;
  MemoryContext linecxt;
  MemoryContext oldcxt;

  linecxt = AllocSetContextCreate(CurrentMemoryContext, "tokenize_file", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(linecxt);

  *tok_lines = NIL;

  while (!feof(file) && !ferror(file)) {
    char rawline[MAX_LINE];
    char *lineptr;
    List *current_line = NIL;
    char *err_msg = NULL;

    if (!fgets(rawline, sizeof(rawline), file)) {
      int save_errno = errno;

      if (!ferror(file)) {
        break; /* normal EOF */
      }
      /* I/O error! */



    }
    if (strlen(rawline) == MAX_LINE - 1) {
      /* Line too long! */


    }

    /* Strip trailing linebreak from rawline */
    lineptr = rawline + strlen(rawline) - 1;
    while (lineptr >= rawline && (*lineptr == '\n' || *lineptr == '\r')) {
      *lineptr-- = '\0';
    }

    /* Parse fields */
    lineptr = rawline;
    while (*lineptr && err_msg == NULL) {
      List *current_field;

      current_field = next_field_expand(filename, &lineptr, elevel, &err_msg);
      /* add field to line, unless we are at EOL or comment start */
      if (current_field != NIL) {
        current_line = lappend(current_line, current_field);
      }
    }

    /* Reached EOL; emit line to TokenizedLine list unless it's boring */
    if (current_line != NIL || err_msg != NULL) {
      TokenizedLine *tok_line;

      tok_line = (TokenizedLine *)palloc(sizeof(TokenizedLine));
      tok_line->fields = current_line;
      tok_line->line_num = line_number;
      tok_line->raw_line = pstrdup(rawline);
      tok_line->err_msg = err_msg;
      *tok_lines = lappend(*tok_lines, tok_line);
    }

    line_number++;
  }

  MemoryContextSwitchTo(oldcxt);

  return linecxt;
}

/*
 * Does user belong to role?
 *
 * userid is the OID of the role given as the attempted login identifier.
 * We check to see if it is a member of the specified role name.
 */
static bool
is_member(Oid userid, const char *role)
{


















}

/*
 * Check HbaToken list for a match to role, allowing group names.
 */
static bool
check_role(const char *role, Oid roleid, List *tokens)
{
  ListCell *cell;
  HbaToken *tok;

  foreach (cell, tokens) {
    tok = lfirst(cell);
    if (!tok->quoted && tok->string[0] == '+') {



    } else if (token_matches(tok, role) || token_is_keyword(tok, "all")) {
      return true;
    }
  }
  return false;
}

/*
 * Check to see if db/role combination matches HbaToken list.
 */
static bool
check_db(const char *dbname, const char *role, Oid roleid, List *tokens)
{
  ListCell *cell;
  HbaToken *tok;

  foreach (cell, tokens) {
    tok = lfirst(cell);
    if (am_walsender && !am_db_walsender) {
      /*
       * physical replication walsender connections can only match
       * replication keyword
       */



    } else if (token_is_keyword(tok, "all")) {
      return true;
    } else if (token_is_keyword(tok, "sameuser")) {












  }
  return false;
}

static bool
ipv4eq(struct sockaddr_in *a, struct sockaddr_in *b)
{

}

#ifdef HAVE_IPV6

static bool
ipv6eq(struct sockaddr_in6 *a, struct sockaddr_in6 *b)
{









}
#endif /* HAVE_IPV6 */

/*
 * Check whether host name matches pattern.
 */
static bool
hostname_match(const char *pattern, const char *actual_hostname)
{













}

/*
 * Check to see if a connecting IP matches a given host name.
 */
static bool
check_hostname(hbaPort *port, const char *hostname)
{












































































}

/*
 * Check to see if a connecting IP matches the given address and netmask.
 */
static bool
check_ip(SockAddr *raddr, struct sockaddr *addr, struct sockaddr *mask)
{




}

/*
 * pg_foreach_ifaddr callback: does client addr match this machine interface?
 */
static void
check_network_callback(struct sockaddr *addr, struct sockaddr *netmask, void *cb_data)
{
















}

/*
 * Use pg_foreach_ifaddr to check a samehost or samenet match
 */
static bool
check_same_host_or_net(SockAddr *raddr, IPCompareMethod method)
{













}

/*
 * Macros used to check and report on invalid configuration options.
 * On error: log a message at level elevel, set *err_msg, and exit the function.
 * These macros are not as general-purpose as they look, because they know
 * what the calling function's error-exit value is.
 *
 * INVALID_AUTH_OPTION = reports when an option is specified for a method where
 *it's not supported. REQUIRE_AUTH_OPTION = same as INVALID_AUTH_OPTION, except
 *it also checks if the method is actually the one specified. Used as a shortcut
 *when the option is only valid for one authentication method.
 * MANDATORY_AUTH_ARG  = check if a required option is set for an authentication
 *method, reporting error if it's not.
 */
#define INVALID_AUTH_OPTION(optname, validmethods)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    ereport(elevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), /* translator: the second %s is a                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                            list of auth methods */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                        errmsg("authentication option \"%s\" is only valid for "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                               "authentication methods %s",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
                            optname, _(validmethods)),                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
                        errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    *err_msg = psprintf("authentication option \"%s\" is only valid for "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
                        "authentication methods %s",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
        optname, validmethods);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    return false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define REQUIRE_AUTH_OPTION(methodval, optname, validmethods)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (hbaline->auth_method != methodval)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      INVALID_AUTH_OPTION(optname, validmethods);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define MANDATORY_AUTH_ARG(argvar, argname, authname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (argvar == NULL) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      ereport(elevel, (errcode(ERRCODE_CONFIG_FILE_ERROR),                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                          errmsg("authentication method \"%s\" requires "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
                                 "argument \"%s\" to be set",                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
                              authname, argname),                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
                          errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      *err_msg = psprintf("authentication method \"%s\" requires argument \"%s\" to be set", authname, argname);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/*
 * Macros for handling pg_ident problems.
 * Much as above, but currently the message level is hardwired as LOG
 * and there is no provision for an err_msg string.
 *
 * IDENT_FIELD_ABSENT:
 * Log a message and exit the function if the given ident field ListCell is
 * not populated.
 *
 * IDENT_MULTI_VALUE:
 * Log a message and exit the function if the given ident token List has more
 * than one element.
 */
#define IDENT_FIELD_ABSENT(field)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (!field) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      ereport(LOG, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("missing entry in file \"%s\" at end of line %d", IdentFileName, line_num)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#define IDENT_MULTI_VALUE(tokens)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (tokens->length > 1) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      ereport(LOG, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("multiple values in ident field"), errcontext("line %d of configuration file \"%s\"", line_num, IdentFileName)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/*
 * Parse one tokenised line from the hba config file and store the result in a
 * HbaLine structure.
 *
 * If parsing fails, log a message at ereport level elevel, store an error
 * string in tok_line->err_msg, and return NULL.  (Some non-error conditions
 * can also result in such messages.)
 *
 * Note: this function leaks memory when an error occurs.  Caller is expected
 * to have set a memory context that will be reset if this function returns
 * NULL.
 */
static HbaLine *
parse_hba_line(TokenizedLine *tok_line, int elevel)
{
  int line_num = tok_line->line_num;
  char **err_msg = &tok_line->err_msg;
  char *str;
  struct addrinfo *gai_result;
  struct addrinfo hints;
  int ret;
  char *cidr_slash;
  char *unsupauth;
  ListCell *field;
  List *tokens;
  ListCell *tokencell;
  HbaToken *token;
  HbaLine *parsedline;

  parsedline = palloc0(sizeof(HbaLine));
  parsedline->linenumber = line_num;
  parsedline->rawline = pstrdup(tok_line->raw_line);

  /* Check the record type. */
  Assert(tok_line->fields != NIL);
  field = list_head(tok_line->fields);
  tokens = lfirst(field);
  if (tokens->length > 1) {



  }
  token = linitial(tokens);
  if (strcmp(token->string, "local") == 0) {
#ifdef HAVE_UNIX_SOCKETS
    parsedline->conntype = ctLocal;
#else
    ereport(elevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("local connections are not supported by this build"), errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));
    *err_msg = "local connections are not supported by this build";
    return NULL;
#endif
  } else if (strcmp(token->string, "host") == 0 || strcmp(token->string, "hostssl") == 0 || strcmp(token->string, "hostnossl") == 0 || strcmp(token->string, "hostgssenc") == 0 || strcmp(token->string, "hostnogssenc") == 0) {

    if (token->string[4] == 's') /* "hostssl" */
    {

      /* Log a warning if SSL support is not active */
#ifdef USE_SSL
      if (!EnableSSL) {
        ereport(elevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("hostssl record cannot match because SSL is disabled"), errhint("Set ssl = on in postgresql.conf."), errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));
        *err_msg = "hostssl record cannot match because SSL is disabled";
      }
#else

                          errmsg("hostssl record cannot match because SSL is not supported by this build"),
                          errhint("Compile with --with-openssl to use SSL connections."), errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));

#endif
    } else if (token->string[4] == 'g') /* "hostgssenc" */
    {

#ifndef ENABLE_GSS

                          errmsg("hostgssenc record cannot match because GSSAPI is not supported by this build"),
                          errhint("Compile with --with-gssapi to use GSSAPI connections."), errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));

#endif
    } else if (token->string[4] == 'n' && token->string[6] == 's') {

    } else if (token->string[4] == 'n' && token->string[6] == 'g') {

    } else {
      /* "host" */
      parsedline->conntype = ctHost;
    }
  } /* record type */
  else {



  }

  /* Get the databases. */
  field = lnext(field);
  if (!field) {



  }
  parsedline->databases = NIL;
  tokens = lfirst(field);
  foreach (tokencell, tokens) {
    parsedline->databases = lappend(parsedline->databases, copy_hba_token(lfirst(tokencell)));
  }

  /* Get the roles. */
  field = lnext(field);
  if (!field) {



  }
  parsedline->roles = NIL;
  tokens = lfirst(field);
  foreach (tokencell, tokens) {
    parsedline->roles = lappend(parsedline->roles, copy_hba_token(lfirst(tokencell)));
  }

  if (parsedline->conntype != ctLocal) {
    /* Read the IP address field. (with or without CIDR netmask) */
    field = lnext(field);
    if (!field) {



    }
    tokens = lfirst(field);
    if (tokens->length > 1) {



    }
    token = linitial(tokens);

    if (token_is_keyword(token, "all")) {

    } else if (token_is_keyword(token, "samehost")) {
      /* Any IP on this host is allowed to connect */

    } else if (token_is_keyword(token, "samenet")) {
      /* Any IP on the host's subnets is allowed to connect */

    } else {
      /* IP and netmask are specified */
      parsedline->ip_cmp_method = ipCmpMask;

      /* need a modifiable copy of token */
      str = pstrdup(token->string);

      /* Check if it has a CIDR suffix and if so isolate it */
      cidr_slash = strchr(str, '/');
      if (cidr_slash) {
        *cidr_slash = '\0';
      }

      /* Get the IP address either way */
      hints.ai_flags = AI_NUMERICHOST;
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = 0;
      hints.ai_protocol = 0;
      hints.ai_addrlen = 0;
      hints.ai_canonname = NULL;
      hints.ai_addr = NULL;
      hints.ai_next = NULL;

      ret = pg_getaddrinfo_all(str, NULL, &hints, &gai_result);
      if (ret == 0 && gai_result) {
        memcpy(&parsedline->addr, gai_result->ai_addr, gai_result->ai_addrlen);
        parsedline->addrlen = gai_result->ai_addrlen;
      } else if (ret == EAI_NONAME) {

      } else {






      }

      pg_freeaddrinfo_all(hints.ai_family, gai_result);

      /* Get the netmask */
      if (cidr_slash) {
        if (parsedline->hostname) {



        }

        if (pg_sockaddr_cidr_mask(&parsedline->mask, cidr_slash + 1, parsedline->addr.ss_family) < 0) {



        }
        parsedline->masklen = parsedline->addrlen;
        pfree(str);
      } else if (!parsedline->hostname) {
        /* Read the mask field. */































        if (parsedline->addr.ss_family != parsedline->mask.ss_family) {



        }
      }
    }
  } /* != ctLocal */

  /* Get the authentication method */
  field = lnext(field);
  if (!field) {



  }
  tokens = lfirst(field);
  if (tokens->length > 1) {



  }
  token = linitial(tokens);

  unsupauth = NULL;
  if (strcmp(token->string, "trust") == 0) {
    parsedline->auth_method = uaTrust;
  } else if (strcmp(token->string, "ident") == 0) {

  } else if (strcmp(token->string, "peer") == 0) {

  } else if (strcmp(token->string, "password") == 0) {

  } else if (strcmp(token->string, "gss") == 0)
#ifdef ENABLE_GSS
    parsedline->auth_method = uaGSS;
#else

#endif
  else if (strcmp(token->string, "sspi") == 0)
#ifdef ENABLE_SSPI
    parsedline->auth_method = uaSSPI;
#else

#endif
  else if (strcmp(token->string, "reject") == 0) {

  } else if (strcmp(token->string, "md5") == 0) {








  } else if (strcmp(token->string, "scram-sha-256") == 0) {

  } else if (strcmp(token->string, "pam") == 0)
#ifdef USE_PAM
    parsedline->auth_method = uaPAM;
#else

#endif
  else if (strcmp(token->string, "bsd") == 0)
#ifdef USE_BSD_AUTH
    parsedline->auth_method = uaBSD;
#else

#endif
  else if (strcmp(token->string, "ldap") == 0)
#ifdef USE_LDAP
    parsedline->auth_method = uaLDAP;
#else

#endif
  else if (strcmp(token->string, "cert") == 0)
#ifdef USE_SSL
    parsedline->auth_method = uaCert;
#else

#endif
  else if (strcmp(token->string, "radius") == 0) {

  } else {



  }

  if (unsupauth) {

                        errmsg("invalid authentication method \"%s\": not supported by this build",
                            token->string),
                        errcontext("line %d of configuration file \"%s\"", line_num, HbaFileName)));


  }

  /*
   * XXX: When using ident on local connections, change it to peer, for
   * backwards compatibility.
   */
  if (parsedline->conntype == ctLocal && parsedline->auth_method == uaIdent) {

  }

  /* Invalid authentication combinations */
  if (parsedline->conntype == ctLocal && parsedline->auth_method == uaGSS) {



  }

  if (parsedline->conntype != ctLocal && parsedline->auth_method == uaPeer) {



  }

  /*
   * SSPI authentication can never be enabled on ctLocal connections,
   * because it's only supported on Windows, where ctLocal isn't supported.
   */

  if (parsedline->conntype != ctHostSSL && parsedline->auth_method == uaCert) {



  }

  /*
   * For GSS and SSPI, set the default value of include_realm to true.
   * Having include_realm set to false is dangerous in multi-realm
   * situations and is generally considered bad practice.  We keep the
   * capability around for backwards compatibility, but we might want to
   * remove it at some point in the future.  Users who still need to strip
   * the realm off would be better served by using an appropriate regex in a
   * pg_ident.conf mapping.
   */
  if (parsedline->auth_method == uaGSS || parsedline->auth_method == uaSSPI) {

  }

  /*
   * For SSPI, include_realm defaults to the SAM-compatible domain (aka
   * NetBIOS name) and user names instead of the Kerberos principal name for
   * compatibility.
   */
  if (parsedline->auth_method == uaSSPI) {


  }

  /* Parse remaining arguments */
  while ((field = lnext(field)) != NULL) {
























  }

  /*
   * Check if the selected authentication method has any mandatory arguments
   * that are not set.
   */
  if (parsedline->auth_method == uaLDAP) {
#ifndef HAVE_LDAP_INITIALIZE
    /* Not mandatory for OpenLDAP, because it can use DNS SRV records */

#endif

    /*
     * LDAP can operate in two modes: either with a direct bind, using
     * ldapprefix and ldapsuffix, or using a search+bind, using
     * ldapbasedn, ldapbinddn, ldapbindpasswd and one of
     * ldapsearchattribute or ldapsearchfilter.  Disallow mixing these
     * parameters.
     */
















    /*
     * When using search+bind, you can either use a simple attribute
     * (defaulting to "uid") or a fully custom search filter.  You can't
     * do both.
     */
    if (parsedline->ldapsearchattribute && parsedline->ldapsearchfilter) {



    }
  }

  if (parsedline->auth_method == uaRADIUS) {













    /*
     * Verify length of option lists - each can be 0 (except for secrets,
     * but that's already checked above), 1 (use the same value
     * everywhere) or the same as the number of servers.
     */









  }

  /*
   * Enforce any parameters implied by other settings.
   */
  if (parsedline->auth_method == uaCert) {
    /*
     * For auth method cert, client certificate validation is mandatory, and it
     * implies the level of verify-full.
     */

  }

  return parsedline;
}

static bool
verify_option_list_length(List *options, const char *optionname, List *masters, const char *mastername, int line_num)
{









}

/*
 * Parse one name-value pair as an authentication option into the given
 * HbaLine.  Return true if we successfully parse the option, false if we
 * encounter an error.  In the event of an error, also log a message at
 * ereport level elevel, and store a message string into *err_msg.
 */
static bool
parse_hba_auth_opt(char *name, char *val, HbaLine *hbaline, int elevel, char **err_msg)
{





































































































































































































































































}

/*
 *	Scan the pre-parsed hba file, looking for a match to the port's
 *connection request.
 */
static void
check_hba(hbaPort *port)
{
  Oid roleid;
  ListCell *line;
  HbaLine *hba;

  /* Get the target role's OID.  Note we do not error out for bad role. */
  roleid = get_role_oid(port->user_name, true);

  foreach (line, parsed_hba_lines) {
    hba = (HbaLine *)lfirst(line);

    /* Check connection type */
    if (hba->conntype == ctLocal) {
      if (!IS_AF_UNIX(port->raddr.addr.ss_family)) {

      }
    } else {




      /* Check SSL state */












      /* Check GSSAPI state */
#ifdef ENABLE_GSS
      if (port->gss && port->gss->enc && hba->conntype == ctHostNoGSS) {
        continue;
      } else if (!(port->gss && port->gss->enc) && hba->conntype == ctHostGSS) {
        continue;
      }
#else



#endif

      /* Check IP address */
      switch (hba->ip_cmp_method) {




















        /* shouldn't get here, but deem it no-match if so */

      }
    } /* != ctLocal */

    /* Check database and role */
    if (!check_db(port->database_name, port->user_name, roleid, hba->databases)) {

    }

    if (!check_role(port->user_name, roleid, hba->roles)) {

    }

    /* Found a record that matched! */
    port->hba = hba;
    return;
  }

  /* If no matching entry was found, then implicitly reject. */
  hba = palloc0(sizeof(HbaLine));


}

/*
 * Read the config file and create a List of HbaLine records for the contents.
 *
 * The configuration is read into a temporary list, and if any parse error
 * occurs the old list is kept in place and false is returned.  Only if the
 * whole file parses OK is the list replaced, and the function returns true.
 *
 * On a false result, caller will take care of reporting a FATAL error in case
 * this is the initial startup.  If it happens on reload, we just keep running
 * with the old data.
 */
bool
load_hba(void)
{
  FILE *file;
  List *hba_lines = NIL;
  ListCell *line;
  List *new_parsed_lines = NIL;
  bool ok = true;
  MemoryContext linecxt;
  MemoryContext oldcxt;
  MemoryContext hbacxt;

  file = AllocateFile(HbaFileName, "r");
  if (file == NULL) {


  }

  linecxt = tokenize_file(HbaFileName, file, &hba_lines, LOG);
  FreeFile(file);

  /* Now parse all the lines */
  Assert(PostmasterContext);
  hbacxt = AllocSetContextCreate(PostmasterContext, "hba parser context", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(hbacxt);
  foreach (line, hba_lines) {
    TokenizedLine *tok_line = (TokenizedLine *)lfirst(line);
    HbaLine *newline;

    /* don't parse lines that already have errors */
    if (tok_line->err_msg != NULL) {


    }

    if ((newline = parse_hba_line(tok_line, LOG)) == NULL) {
      /* Parse error; remember there's trouble */


      /*
       * Keep parsing the rest of the file so we can report errors on
       * more than the first line.  Error has already been logged, no
       * need for more chatter here.
       */

    }

    new_parsed_lines = lappend(new_parsed_lines, newline);
  }

  /*
   * A valid HBA file must have at least one entry; else there's no way to
   * connect to the postmaster.  But only complain about this if we didn't
   * already have parsing errors.
   */
  if (ok && new_parsed_lines == NIL) {


  }

  /* Free tokenizer memory */
  MemoryContextDelete(linecxt);
  MemoryContextSwitchTo(oldcxt);

  if (!ok) {
    /* File contained one or more errors, so bail out */


  }

  /* Loaded new file successfully, replace the one we use */
  if (parsed_hba_context != NULL) {

  }
  parsed_hba_context = hbacxt;
  parsed_hba_lines = new_parsed_lines;

  return true;
}

/*
 * This macro specifies the maximum number of authentication options
 * that are possible with any given authentication method that is supported.
 * Currently LDAP supports 11, and there are 3 that are not dependent on
 * the auth method here.  It may not actually be possible to set all of them
 * at the same time, but we'll set the macro value high enough to be
 * conservative and avoid warnings from static analysis tools.
 */
#define MAX_HBA_OPTIONS 14

/*
 * Create a text array listing the options specified in the HBA line.
 * Return NULL if no options are specified.
 */
static ArrayType *
gethba_options(HbaLine *hba)
{
  int noptions;
  Datum options[MAX_HBA_OPTIONS];

  noptions = 0;

  if (hba->auth_method == uaGSS || hba->auth_method == uaSSPI) {







  }

  if (hba->usermap) {

  }

  if (hba->clientcert != clientCertOff) {

  }

  if (hba->pamservice) {

  }

  if (hba->auth_method == uaLDAP) {











































  }

  if (hba->auth_method == uaRADIUS) {















  }

  /* If you add more options, consider increasing MAX_HBA_OPTIONS. */
  Assert(noptions <= MAX_HBA_OPTIONS);

  if (noptions > 0) {

  } else {
    return NULL;
  }
}

/* Number of columns in pg_hba_file_rules view */
#define NUM_PG_HBA_FILE_RULES_ATTS 9

/*
 * fill_hba_line: build one row of pg_hba_file_rules view, add it to tuplestore
 *
 * tuple_store: where to store data
 * tupdesc: tuple descriptor for the view
 * lineno: pg_hba.conf line number (must always be valid)
 * hba: parsed line data (can be NULL, in which case err_msg should be set)
 * err_msg: error message (NULL if none)
 *
 * Note: leaks memory, but we don't care since this is run in a short-lived
 * memory context.
 */
static void
fill_hba_line(Tuplestorestate *tuple_store, TupleDesc tupdesc, int lineno, HbaLine *hba, const char *err_msg)
{
  Datum values[NUM_PG_HBA_FILE_RULES_ATTS];
  bool nulls[NUM_PG_HBA_FILE_RULES_ATTS];
  char buffer[NI_MAXHOST];
  HeapTuple tuple;
  int index;
  ListCell *lc;
  const char *typestr;
  const char *addrstr;
  const char *maskstr;
  ArrayType *options;

  Assert(tupdesc->natts == NUM_PG_HBA_FILE_RULES_ATTS);

  memset(values, 0, sizeof(values));
  memset(nulls, 0, sizeof(nulls));
  index = 0;

  /* line_number */
  values[index++] = Int32GetDatum(lineno);

  if (hba != NULL) {
    /* type */
    /* Avoid a default:; case so compiler will warn about missing cases */
    typestr = NULL;
    switch (hba->conntype) {
    case ctLocal:
      typestr = "local";
      break;
    case ctHost:
      typestr = "host";
      break;
    case ctHostSSL:


    case ctHostNoSSL:


    case ctHostGSS:


    case ctHostNoGSS:


    }
    if (typestr) {
      values[index++] = CStringGetTextDatum(typestr);
    } else {

    }

    /* database */
    if (hba->databases) {
      /*
       * Flatten HbaToken list to string list.  It might seem that we
       * should re-quote any quoted tokens, but that has been rejected
       * on the grounds that it makes it harder to compare the array
       * elements to other system catalogs.  That makes entries like
       * "all" or "samerole" formally ambiguous ... but users who name
       * databases/roles that way are inflicting their own pain.
       */
      List *names = NIL;

      foreach (lc, hba->databases) {
        HbaToken *tok = lfirst(lc);

        names = lappend(names, tok->string);
      }
      values[index++] = PointerGetDatum(strlist_to_textarray(names));
    } else {

    }

    /* user */
    if (hba->roles) {
      /* Flatten HbaToken list to string list; see comment above */
      List *roles = NIL;

      foreach (lc, hba->roles) {
        HbaToken *tok = lfirst(lc);

        roles = lappend(roles, tok->string);
      }
      values[index++] = PointerGetDatum(strlist_to_textarray(roles));
    } else {

    }

    /* address and netmask */
    /* Avoid a default:; case so compiler will warn about missing cases */
    addrstr = maskstr = NULL;
    switch (hba->ip_cmp_method) {
    case ipCmpMask:
      if (hba->hostname) {

      } else {
        /*
         * Note: if pg_getnameinfo_all fails, it'll set buffer to
         * "???", which we want to return.
         */
        if (hba->addrlen > 0) {
          if (pg_getnameinfo_all(&hba->addr, hba->addrlen, buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) == 0) {
            clean_ipv6_addr(hba->addr.ss_family, buffer);
          }
          addrstr = pstrdup(buffer);
        }
        if (hba->masklen > 0) {
          if (pg_getnameinfo_all(&hba->mask, hba->masklen, buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) == 0) {
            clean_ipv6_addr(hba->mask.ss_family, buffer);
          }
          maskstr = pstrdup(buffer);
        }
      }
      break;
    case ipCmpAll:


    case ipCmpSameHost:


    case ipCmpSameNet:


    }
    if (addrstr) {
      values[index++] = CStringGetTextDatum(addrstr);
    } else {
      nulls[index++] = true;
    }
    if (maskstr) {
      values[index++] = CStringGetTextDatum(maskstr);
    } else {
      nulls[index++] = true;
    }

    /*
     * Make sure UserAuthName[] tracks additions to the UserAuth enum
     */
    StaticAssertStmt(lengthof(UserAuthName) == USER_AUTH_LAST + 1, "UserAuthName[] must match the UserAuth enum");

    /* auth_method */
    values[index++] = CStringGetTextDatum(UserAuthName[hba->auth_method]);

    /* options */
    options = gethba_options(hba);
    if (options) {

    } else {
      nulls[index++] = true;
    }
  } else {
    /* no parsing result, so set relevant fields to nulls */

  }

  /* error */
  if (err_msg) {

  } else {
    nulls[NUM_PG_HBA_FILE_RULES_ATTS - 1] = true;
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);
  tuplestore_puttuple(tuple_store, tuple);
}

/*
 * Read the pg_hba.conf file and fill the tuplestore with view records.
 */
static void
fill_hba_view(Tuplestorestate *tuple_store, TupleDesc tupdesc)
{
  FILE *file;
  List *hba_lines = NIL;
  ListCell *line;
  MemoryContext linecxt;
  MemoryContext hbacxt;
  MemoryContext oldcxt;

  /*
   * In the unlikely event that we can't open pg_hba.conf, we throw an
   * error, rather than trying to report it via some sort of view entry.
   * (Most other error conditions should result in a message in a view
   * entry.)
   */
  file = AllocateFile(HbaFileName, "r");
  if (file == NULL) {

  }

  linecxt = tokenize_file(HbaFileName, file, &hba_lines, DEBUG3);
  FreeFile(file);

  /* Now parse all the lines */
  hbacxt = AllocSetContextCreate(CurrentMemoryContext, "hba parser context", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(hbacxt);
  foreach (line, hba_lines) {
    TokenizedLine *tok_line = (TokenizedLine *)lfirst(line);
    HbaLine *hbaline = NULL;

    /* don't parse lines that already have errors */
    if (tok_line->err_msg == NULL) {
      hbaline = parse_hba_line(tok_line, DEBUG3);
    }

    fill_hba_line(tuple_store, tupdesc, tok_line->line_num, hbaline, tok_line->err_msg);
  }

  /* Free tokenizer memory */
  MemoryContextDelete(linecxt);
  /* Free parse_hba_line memory */
  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(hbacxt);
}

/*
 * SQL-accessible SRF to return all the entries in the pg_hba.conf file.
 */
Datum
pg_hba_file_rules(PG_FUNCTION_ARGS)
{
  Tuplestorestate *tuple_store;
  TupleDesc tupdesc;
  MemoryContext old_cxt;
  ReturnSetInfo *rsi;

  /*
   * We must use the Materialize mode to be safe against HBA file changes
   * while the cursor is open. It's also more efficient than having to look
   * up our current position in the parsed list every time.
   */
  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  /* Check to see if caller supports us returning a tuplestore */
  if (rsi == NULL || !IsA(rsi, ReturnSetInfo)) {

  }
  if (!(rsi->allowedModes & SFRM_Materialize)) {

  }

  rsi->returnMode = SFRM_Materialize;

  /* Build a tuple descriptor for our result type */
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {

  }

  /* Build tuplestore to hold the result rows */
  old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);

  tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);
  rsi->setDesc = tupdesc;
  rsi->setResult = tuple_store;

  MemoryContextSwitchTo(old_cxt);

  /* Fill the tuplestore */
  fill_hba_view(tuple_store, tupdesc);

  PG_RETURN_NULL();
}

/*
 * Parse one tokenised line from the ident config file and store the result in
 * an IdentLine structure.
 *
 * If parsing fails, log a message and return NULL.
 *
 * If ident_user is a regular expression (ie. begins with a slash), it is
 * compiled and stored in IdentLine structure.
 *
 * Note: this function leaks memory when an error occurs.  Caller is expected
 * to have set a memory context that will be reset if this function returns
 * NULL.
 */
static IdentLine *
parse_ident_line(TokenizedLine *tok_line)
{




























































}

/*
 *	Process one line from the parsed ident config lines.
 *
 *	Compare input parsed ident line to the needed map, pg_role and
 *ident_user. *found_p and *error_p are set according to our results.
 */
static void
check_ident_usermap(IdentLine *identLine, const char *usermap_name, const char *pg_role, const char *ident_user, bool case_insensitive, bool *found_p, bool *error_p)
{
































































































}

/*
 *	Scan the (pre-parsed) ident usermap file line by line, looking for a
 *match
 *
 *	See if the user with ident username "auth_user" is allowed to act
 *	as Postgres user "pg_role" according to usermap "usermap_name".
 *
 *	Special case: Usermap NULL, equivalent to what was previously called
 *	"sameuser" or "samerole", means don't look in the usermap file.
 *	That's an implied map wherein "pg_role" must be identical to
 *	"auth_user" in order to be authorized.
 *
 *	Iff authorized, return STATUS_OK, otherwise return STATUS_ERROR.
 */
int
check_usermap(const char *usermap_name, const char *pg_role, const char *auth_user, bool case_insensitive)
{






























}

/*
 * Read the ident config file and create a List of IdentLine records for
 * the contents.
 *
 * This works the same as load_hba(), but for the user config file.
 */
bool
load_ident(void)
{
  FILE *file;
  List *ident_lines = NIL;
  ListCell *line_cell, *parsed_line_cell;
  List *new_parsed_lines = NIL;
  bool ok = true;
  MemoryContext linecxt;
  MemoryContext oldcxt;
  MemoryContext ident_context;
  IdentLine *newline;

  file = AllocateFile(IdentFileName, "r");
  if (file == NULL) {
    /* not fatal ... we just won't do any special ident maps */


  }

  linecxt = tokenize_file(IdentFileName, file, &ident_lines, LOG);
  FreeFile(file);

  /* Now parse all the lines */
  Assert(PostmasterContext);
  ident_context = AllocSetContextCreate(PostmasterContext, "ident parser context", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(ident_context);
  foreach (line_cell, ident_lines) {


    /* don't parse lines that already have errors */


















  }

  /* Free tokenizer memory */
  MemoryContextDelete(linecxt);
  MemoryContextSwitchTo(oldcxt);

  if (!ok) {
    /*
     * File contained one or more errors, so bail out, first being careful
     * to clean up whatever we allocated.  Most stuff will go away via
     * MemoryContextDelete, but we have to clean up regexes explicitly.
     */








  }

  /* Loaded new file successfully, replace the one we use */
  if (parsed_ident_lines != NIL) {






  }
  if (parsed_ident_context != NULL) {

  }

  parsed_ident_context = ident_context;
  parsed_ident_lines = new_parsed_lines;

  return true;
}

/*
 *	Determine what authentication method should be used when accessing
 *database "database" from frontend "raddr", user "user".  Return the method and
 *	an optional argument (stored in fields of *port), and STATUS_OK.
 *
 *	If the file does not contain any entry matching the request, we return
 *	method = uaImplicitReject.
 */
void
hba_getauthmethod(hbaPort *port)
{
  check_hba(port);
}