/*-------------------------------------------------------------------------
 *
 * pgfnames.c
 *	  directory handling functions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/common/pgfnames.c
 *
 *-------------------------------------------------------------------------
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <dirent.h>

#ifndef FRONTEND
#define pg_log_warning(...) elog(WARNING, __VA_ARGS__)
#else
#include "common/logging.h"
#endif

/*
 * pgfnames
 *
 * return a list of the names of objects in the argument directory.  Caller
 * must call pgfnames_cleanup later to free the memory allocated by this
 * function.
 */
char **
pgfnames(const char *path)
{









































}

/*
 *	pgfnames_cleanup
 *
 *	deallocate memory used for filenames
 */
void
pgfnames_cleanup(char **filenames)
{








}