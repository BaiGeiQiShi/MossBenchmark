/*-------------------------------------------------------------------------
 *
 * ts_locale.c
 *		locale compatibility layer for tsearch
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/ts_locale.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "storage/fd.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_public.h"

static void
tsearch_readline_callback(void *arg);

/*
 * The reason these functions use a 3-wchar_t output buffer, not 2 as you
 * might expect, is that on Windows "wchar_t" is 16 bits and what we'll be
 * getting from char2wchar() is UTF16 not UTF32.  A single input character
 * may therefore produce a surrogate pair rather than just one wchar_t;
 * we also need room for a trailing null.  When we do get a surrogate pair,
 * we pass just the first code to iswdigit() etc, so that these functions will
 * always return false for characters outside the Basic Multilingual Plane.
 */
#define WC_BUF_LEN 3

int
t_isdigit(const char *ptr)
{













}

int
t_isspace(const char *ptr)
{













}

int
t_isalpha(const char *ptr)
{













}

int
t_isprint(const char *ptr)
{













}

/*
 * Set up to read a file using tsearch_readline().  This facility is
 * better than just reading the file directly because it provides error
 * context pointing to the specific line where a problem is detected.
 *
 * Expected usage is:
 *
 *		tsearch_readline_state trst;
 *
 *		if (!tsearch_readline_begin(&trst, filename))
 *			ereport(ERROR,
 *					(errcode(ERRCODE_CONFIG_FILE_ERROR),
 *					 errmsg("could not open stop-word file
 *\"%s\": %m", filename))); while ((line = tsearch_readline(&trst)) != NULL)
 *			process line;
 *		tsearch_readline_end(&trst);
 *
 * Note that the caller supplies the ereport() for file open failure;
 * this is so that a custom message can be provided.  The filename string
 * passed to tsearch_readline_begin() must remain valid through
 * tsearch_readline_end().
 */
bool
tsearch_readline_begin(tsearch_readline_state *stp, const char *filename)
{













}

/*
 * Read the next line from a tsearch data file (expected to be in UTF-8), and
 * convert it to database encoding if needed. The returned string is palloc'd.
 * NULL return means EOF.
 */
char *
tsearch_readline(tsearch_readline_state *stp)
{



























}

/*
 * Close down after reading a file with tsearch_readline()
 */
void
tsearch_readline_end(tsearch_readline_state *stp)
{












}

/*
 * Error context callback for errors occurring while reading a tsearch
 * configuration file.
 */
static void
tsearch_readline_callback(void *arg)
{

















}

/*
 * Read the next line from a tsearch data file (expected to be in UTF-8), and
 * convert it to database encoding if needed. The returned string is palloc'd.
 * NULL return means EOF.
 *
 * Note: direct use of this function is now deprecated.  Go through
 * tsearch_readline() to provide better error reporting.
 */
char *
t_readline(FILE *fp)
{


























}

/*
 * lowerstr --- fold null-terminated string to lower case
 *
 * Returned string is palloc'd
 */
char *
lowerstr(const char *str)
{

}

/*
 * lowerstr_with_len --- fold string to lower case
 *
 * Input string need not be null-terminated.
 *
 * Returned string is palloc'd
 */
char *
lowerstr_with_len(const char *str, int len)
{



































































}