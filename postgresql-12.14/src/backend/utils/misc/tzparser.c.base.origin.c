/*-------------------------------------------------------------------------
 *
 * tzparser.c
 *	  Functions for parsing timezone offset files
 *
 * Note: this code is invoked from the check_hook for the GUC variable
 * timezone_abbreviations.  Therefore, it should report problems using
 * GUC_check_errmsg() and related functions, and try to avoid throwing
 * elog(ERROR).  This is not completely bulletproof at present --- in
 * particular out-of-memory will throw an error.  Could probably fix with
 * PG_TRY if necessary.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/tzparser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>

#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/tzparser.h"

#define WHITESPACE " \t\n\r"

static bool
validateTzEntry(tzEntry *tzentry);
static bool
splitTzLine(const char *filename, int lineno, char *line, tzEntry *tzentry);
static int
addToArray(tzEntry **base, int *arraysize, int n, tzEntry *entry, bool override);
static int
ParseTzFile(const char *filename, int depth, tzEntry **base, int *arraysize, int n);

/*
 * Apply additional validation checks to a tzEntry
 *
 * Returns true if OK, else false
 */
static bool
validateTzEntry(tzEntry *tzentry)
{






























}

/*
 * Attempt to parse the line as a timezone abbrev spec
 *
 * Valid formats are:
 *	name  zone
 *	name  offset  dst
 *
 * Returns true if OK, else false; data is stored in *tzentry
 */
static bool
splitTzLine(const char *filename, int lineno, char *line, tzEntry *tzentry)
{








































































}

/*
 * Insert entry into sorted array
 *
 * *base: base address of array (changeable if must enlarge array)
 * *arraysize: allocated length of array (changeable if must enlarge array)
 * n: current number of valid elements in array
 * entry: new data to insert
 * override: true if OK to override
 *
 * Returns the new array length (new value for n), or -1 if error
 */
static int
addToArray(tzEntry **base, int *arraysize, int n, tzEntry *entry, bool override)
{




































































}

/*
 * Parse a single timezone abbrev file --- can recurse to handle @INCLUDE
 *
 * filename: user-specified file name (does not include path)
 * depth: current recursion depth
 * *base: array for results (changeable if must enlarge array)
 * *arraysize: allocated length of array (changeable if must enlarge array)
 * n: current number of valid elements in array
 *
 * Returns the new array length (new value for n), or -1 if error
 */
static int
ParseTzFile(const char *filename, int depth, tzEntry **base, int *arraysize, int n)
{

































































































































































}

/*
 * load_tzoffsets --- read and parse the specified timezone offset file
 *
 * On success, return a filled-in TimeZoneAbbrevTable, which must have been
 * malloc'd not palloc'd.  On failure, return NULL, using GUC_check_errmsg
 * and friends to give details of the problem.
 */
TimeZoneAbbrevTable *
load_tzoffsets(const char *filename)
{




































}