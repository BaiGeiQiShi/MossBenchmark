/*-------------------------------------------------------------------------
 *
 * slotfuncs.c
 *	   Support functions for replication slots
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/slotfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xlog_internal.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "replication/decode.h"
#include "replication/slot.h"
#include "replication/logical.h"
#include "replication/logicalfuncs.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/pg_lsn.h"
#include "utils/resowner.h"

static void
check_permissions(void)
{





}

/*
 * Helper function for creating a new physical replication slot with
 * given arguments. Note that this function doesn't release the created
 * slot.
 *
 * If restart_lsn is a valid value, we use it without WAL reservation
 * routine. So the caller must guarantee that WAL is available.
 */
static void
create_physical_replication_slot(char *name, bool immediately_reserve,
                                 bool temporary, XLogRecPtr restart_lsn)
{

















}

/*
 * SQL function for creating a new physical (streaming replication)
 * replication slot.
 */
Datum
pg_create_physical_replication_slot(PG_FUNCTION_ARGS)
{




































}

/*
 * Helper function for creating a new logical replication slot with
 * given arguments. Note that this function doesn't release the created
 * slot.
 *
 * When find_startpoint is false, the slot's confirmed_flush is not set; it's
 * caller's responsibility to ensure it's set to something sensible.
 */
static void
create_logical_replication_slot(char *name, char *plugin, bool temporary,
                                XLogRecPtr restart_lsn, bool find_startpoint)
{



































}

/*
 * SQL function for creating a new logical replication slot.
 */
Datum
pg_create_logical_replication_slot(PG_FUNCTION_ARGS)
{



































}

/*
 * SQL function for dropping a replication slot.
 */
Datum
pg_drop_replication_slot(PG_FUNCTION_ARGS)
{









}

/*
 * pg_get_replication_slots - SQL SRF showing active replication slots.
 */
Datum
pg_get_replication_slots(PG_FUNCTION_ARGS)
{





























































































































}

/*
 * Helper function for advancing our physical replication slot forward.
 *
 * The LSN position to move to is compared simply to the slot's restart_lsn,
 * knowing that any position older than that would be removed by successive
 * checkpoints.
 */
static XLogRecPtr
pg_physical_replication_slot_advance(XLogRecPtr moveto)
{



















}

/*
 * Helper function for advancing our logical replication slot forward.
 *
 * The slot's restart_lsn is used as start point for reading records, while
 * confirmed_flush is used as base point for the decoding context.
 *
 * We cannot just do LogicalConfirmReceivedLocation to update confirmed_flush,
 * because we need to digest WAL to advance restart_lsn allowing to recycle
 * WAL and removal of old catalog tuples.  As decoding is done in fast_forward
 * mode, no changes are generated anyway.
 */
static XLogRecPtr
pg_logical_replication_slot_advance(XLogRecPtr moveto)
{










































































































}

/*
 * SQL function for moving the position in a replication slot.
 */
Datum
pg_replication_slot_advance(PG_FUNCTION_ARGS)
{



























































































}

/*
 * Helper function of copying a replication slot.
 */
static Datum
copy_replication_slot(FunctionCallInfo fcinfo, bool logical_slot)
{

























































































































































































































}

/* The wrappers below are all to appease opr_sanity */
Datum
pg_copy_logical_replication_slot_a(PG_FUNCTION_ARGS)
{

}

Datum
pg_copy_logical_replication_slot_b(PG_FUNCTION_ARGS)
{

}

Datum
pg_copy_logical_replication_slot_c(PG_FUNCTION_ARGS)
{

}

Datum
pg_copy_physical_replication_slot_a(PG_FUNCTION_ARGS)
{

}

Datum
pg_copy_physical_replication_slot_b(PG_FUNCTION_ARGS)
{

}
