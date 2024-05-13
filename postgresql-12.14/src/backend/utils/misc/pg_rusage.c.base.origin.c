/*-------------------------------------------------------------------------
 *
 * pg_rusage.c
 *	  Resource usage measurement support routines.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/pg_rusage.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "utils/pg_rusage.h"

/*
 * Initialize usage snapshot.
 */
void
pg_rusage_init(PGRUsage *ru0)
{


}

/*
 * Compute elapsed time since ru0 usage snapshot, and format into
 * a displayable string.  Result is in a static string, which is
 * tacky, but no one ever claimed that the Postgres backend is
 * threadable...
 */
const char *
pg_rusage_show(const PGRUsage *ru0)
{
























}