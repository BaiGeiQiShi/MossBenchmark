/* -------------------------------------------------------------------------
 *
 * decode.c
 *		This module decodes WAL records read using xlogreader.h's APIs
 *for the purpose of logical decoding by passing information to the
 *		reorderbuffer module (containing the actual changes) and to the
 *		snapbuild module to build a fitting catalog snapshot (to be able
 *to properly decode the changes in the reorderbuffer).
 *
 * NOTE:
 *		This basically tries to handle all low level xlog stuff for
 *		reorderbuffer.c and snapbuild.c. There's some minor leakage
 *where a specific record's struct is used to pass data along, but those just
 *		happen to contain the right amount of data in a convenient
 *		format. There isn't and shouldn't be much intelligence about the
 *		contents of records in here except turning them into a more
 *usable format.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/decode.c
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/xlogutils.h"
#include "access/xlogreader.h"
#include "access/xlogrecord.h"

#include "catalog/pg_control.h"

#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/message.h"
#include "replication/reorderbuffer.h"
#include "replication/origin.h"
#include "replication/snapbuild.h"

#include "storage/standby.h"

typedef struct XLogRecordBuffer
{
  XLogRecPtr origptr;
  XLogRecPtr endptr;
  XLogReaderState *record;
} XLogRecordBuffer;

/* RMGR Handlers */
static void
DecodeXLogOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeHeapOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeHeap2Op(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeXactOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeStandbyOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeLogicalMsgOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);

/* individual record(group)'s handlers */
static void
DecodeInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeUpdate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeDelete(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeTruncate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeMultiInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);
static void
DecodeSpecConfirm(LogicalDecodingContext *ctx, XLogRecordBuffer *buf);

static void
DecodeCommit(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_commit *parsed, TransactionId xid);
static void
DecodeAbort(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_abort *parsed, TransactionId xid);

/* common function to decode tuples */
static void
DecodeXLogTuple(char *data, Size len, ReorderBufferTupleBuf *tup);

/*
 * Take every XLogReadRecord()ed record and perform the actions required to
 * decode it using the output plugin already setup in the logical decoding
 * context.
 *
 * NB: Note that every record's xid needs to be processed by reorderbuffer
 * (xids contained in the content of records are not relevant for this rule).
 * That means that for records which'd otherwise not go through the
 * reorderbuffer ReorderBufferProcessXid() has to be called. We don't want to
 * call ReorderBufferProcessXid for each record type by default, because
 * e.g. empty xacts can be handled more efficiently if there's no previous
 * state for them.
 *
 * We also support the ability to fast forward thru records, skipping some
 * record types completely - see individual record types for details.
 */
void
LogicalDecodingProcessRecord(LogicalDecodingContext *ctx, XLogReaderState *record)
{
































































}

/*
 * Handle rmgr XLOG_ID records for DecodeRecordIntoReorderBuffer().
 */
static void
DecodeXLogOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{


































}

/*
 * Handle rmgr XACT_ID records for DecodeRecordIntoReorderBuffer().
 */
static void
DecodeXactOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{































































































}

/*
 * Handle rmgr STANDBY_ID records for DecodeRecordIntoReorderBuffer().
 */
static void
DecodeStandbyOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{









































}

/*
 * Handle rmgr HEAP2_ID records for DecodeRecordIntoReorderBuffer().
 */
static void
DecodeHeap2Op(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{






















































}

/*
 * Handle rmgr HEAP_ID records for DecodeRecordIntoReorderBuffer().
 */
static void
DecodeHeapOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{



























































































}

static inline bool
FilterByOrigin(LogicalDecodingContext *ctx, RepOriginId origin_id)
{






}

/*
 * Handle rmgr LOGICALMSG_ID records for DecodeRecordIntoReorderBuffer().
 */
static void
DecodeLogicalMsgOp(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{












































}

/*
 * Consolidated commit record handling between the different form of commit
 * records.
 */
static void
DecodeCommit(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_commit *parsed, TransactionId xid)
{



















































































}

/*
 * Get the data from the various forms of abort records and pass it on to
 * snapbuild.c and reorderbuffer.c
 */
static void
DecodeAbort(LogicalDecodingContext *ctx, XLogRecordBuffer *buf, xl_xact_parsed_abort *parsed, TransactionId xid)
{








}

/*
 * Parse XLOG_HEAP_INSERT (not MULTI_INSERT!) records into tuplebufs.
 *
 * Deletes can contain the new tuple.
 */
static void
DecodeInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{























































}

/*
 * Parse XLOG_HEAP_UPDATE and XLOG_HEAP_HOT_UPDATE, which have the same layout
 * in the record, from wal into proper tuplebufs.
 *
 * Updates can possibly contain a new tuple and the old primary key.
 */
static void
DecodeUpdate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{


























































}

/*
 * Parse XLOG_HEAP_DELETE from wal into proper tuplebufs.
 *
 * Deletes can possibly contain the old primary key.
 */
static void
DecodeDelete(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{



















































}

/*
 * Parse XLOG_HEAP_TRUNCATE from wal
 */
static void
DecodeTruncate(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{

































}

/*
 * Decode XLOG_HEAP2_MULTI_INSERT_insert record into multiple tuplebufs.
 *
 * Currently MULTI_INSERT will always contain the full tuples.
 */
static void
DecodeMultiInsert(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{

































































































}

/*
 * Parse XLOG_HEAP_CONFIRM from wal into a confirmation change.
 *
 * This is pretty trivial, all the state essentially already setup by the
 * speculative insertion.
 */
static void
DecodeSpecConfirm(LogicalDecodingContext *ctx, XLogRecordBuffer *buf)
{


























}

/*
 * Read a HeapTuple as WAL logged by heap_insert, heap_update and heap_delete
 * (but not by heap_multi_insert) into a tuplebuf.
 *
 * The size 'len' and the pointer 'data' in the record need to be
 * computed outside as they are record specific.
 */
static void
DecodeXLogTuple(char *data, Size len, ReorderBufferTupleBuf *tuple)
{

























}