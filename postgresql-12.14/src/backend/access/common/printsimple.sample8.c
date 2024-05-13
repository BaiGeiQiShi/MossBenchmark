/*-------------------------------------------------------------------------
 *
 * printsimple.c
 *	  Routines to print out tuples containing only a limited range of
 *	  builtin types without catalog access.  This is intended for
 *	  backends that don't have catalog access because they are not bound
 *	  to a specific database, such as some walsender processes.  It
 *	  doesn't handle standalone backends or protocol versions other than
 *	  3.0, because we don't need such handling for current applications.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/common/printsimple.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/printsimple.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"

/*
 * At startup time, send a RowDescription message.
 */
void
printsimple_startup(DestReceiver *self, int operation, TupleDesc tupdesc)
{




















}

/*
 * For each tuple, send a DataRow message.
 */
bool
printsimple(TupleTableSlot *slot, DestReceiver *self)
{



































































}