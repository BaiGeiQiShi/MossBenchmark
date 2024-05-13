/*-------------------------------------------------------------------------
 *
 * trigfuncs.c
 *	  Builtin functions for useful trigger support.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/utils/adt/trigfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "commands/trigger.h"
#include "utils/builtins.h"
#include "utils/rel.h"

/*
 * suppress_redundant_updates_trigger
 *
 * This trigger function will inhibit an update from being done
 * if the OLD and NEW records are identical.
 */
Datum
suppress_redundant_updates_trigger(PG_FUNCTION_ARGS)
{











































}