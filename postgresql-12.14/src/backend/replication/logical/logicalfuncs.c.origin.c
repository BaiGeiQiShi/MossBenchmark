/*-------------------------------------------------------------------------
 *
 * logicalfuncs.c
 *
 *	   Support functions for using logical decoding and management of
 *	   logical replication slots via SQL.
 *
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logicalfuncs.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>

#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "access/xlog_internal.h"
#include "access/xlogutils.h"

#include "access/xact.h"

#include "catalog/pg_type.h"

#include "nodes/makefuncs.h"

#include "mb/pg_wchar.h"

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/pg_lsn.h"
#include "utils/regproc.h"
#include "utils/resowner.h"
#include "utils/lsyscache.h"

#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/logicalfuncs.h"
#include "replication/message.h"

#include "storage/fd.h"

/* private date for writing out data */
typedef struct DecodingOutputState
{
  Tuplestorestate *tupstore;
  TupleDesc tupdesc;
  bool binary_output;
  int64 returned_rows;
} DecodingOutputState;

/*
 * Prepare for an output plugin write.
 */
static void
LogicalOutputPrepareWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write)
{

}

/*
 * Perform output plugin write into tuplestore.
 */
static void
LogicalOutputWrite(LogicalDecodingContext *ctx, XLogRecPtr lsn, TransactionId xid, bool last_write)
{






























}

static void
check_permissions(void)
{




}

int
logical_read_local_xlog_page(XLogReaderState *state, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *cur_page, TimeLineID *pageTLI)
{

}

/*
 * Helper function for the various SQL callable logical decoding functions.
 */
static Datum
pg_logical_slot_get_changes_guts(FunctionCallInfo fcinfo, bool confirm, bool binary)
{



















































































































































































































































}

/*
 * SQL function returning the changestream as text, consuming the data.
 */
Datum
pg_logical_slot_get_changes(PG_FUNCTION_ARGS)
{

}

/*
 * SQL function returning the changestream as text, only peeking ahead.
 */
Datum
pg_logical_slot_peek_changes(PG_FUNCTION_ARGS)
{

}

/*
 * SQL function returning the changestream in binary, consuming the data.
 */
Datum
pg_logical_slot_get_binary_changes(PG_FUNCTION_ARGS)
{

}

/*
 * SQL function returning the changestream in binary, only peeking ahead.
 */
Datum
pg_logical_slot_peek_binary_changes(PG_FUNCTION_ARGS)
{

}

/*
 * SQL function for writing logical decoding message into WAL.
 */
Datum
pg_logical_emit_message_bytea(PG_FUNCTION_ARGS)
{







}

Datum
pg_logical_emit_message_text(PG_FUNCTION_ARGS)
{


}
