/*-------------------------------------------------------------------------
 *
 * ts_utils.c
 *		various support functions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/ts_utils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>

#include "miscadmin.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"

/*
 * Given the base name and extension of a tsearch config file, return
 * its full path name.  The base name is assumed to be user-supplied,
 * and is checked to prevent pathname attacks.  The extension is assumed
 * to be safe.
 *
 * The result is a palloc'd string.
 */
char *
get_tsearch_config_filename(const char *basename, const char *extension)
{






















}

/*
 * Reads a stop-word file. Each word is run through 'wordop'
 * function, if given.  wordop may either modify the input in-place,
 * or palloc a new version.
 */
void
readstoplist(const char *fname, StopList *s, char *(*wordop)(const char *))
{










































































}

bool
searchstoplist(StopList *s, char *key)
{

}