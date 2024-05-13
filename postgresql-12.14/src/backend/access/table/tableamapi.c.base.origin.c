/*----------------------------------------------------------------------
 *
 * tableamapi.c
 *		Support routines for API for Postgres table access methods
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/table/tableamapi.c
 *----------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

/*
 * GetTableAmRoutine
 *		Call the specified access method handler routine to get its
 *		TableAmRoutine struct, which will be palloc'd in the caller's
 *		memory context.
 */
const TableAmRoutine *
GetTableAmRoutine(Oid amhandler)
{







































































}

/* check_hook: validate new default_table_access_method */
bool
check_default_table_access_method(char **newval, void **extra, GucSource source)
{







































}