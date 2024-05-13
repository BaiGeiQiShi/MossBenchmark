/*-------------------------------------------------------------------------
 *
 * pg_controldata.c
 *
 * Routines to expose the contents of the control data file via
 * a set of SQL functions.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/pg_controldata.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/transam.h"
#include "access/xlog_internal.h"
#include "access/xlog.h"
#include "catalog/pg_control.h"
#include "catalog/pg_type.h"
#include "common/controldata_utils.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "utils/timestamp.h"

Datum
pg_control_system(PG_FUNCTION_ARGS)
{








































}

Datum
pg_control_checkpoint(PG_FUNCTION_ARGS)
{










































































































}

Datum
pg_control_recovery(PG_FUNCTION_ARGS)
{












































}

Datum
pg_control_init(PG_FUNCTION_ARGS)
{








































































}
