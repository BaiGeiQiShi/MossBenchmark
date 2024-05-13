/*-------------------------------------------------------------------------
 * logical.c
 *	   PostgreSQL logical decoding coordination
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/logical.c
 *
 * NOTES
 *	  This file coordinates interaction between the various modules that
 *	  together provide logical decoding, primarily by providing so
 *	  called LogicalDecodingContexts. The goal is to encapsulate most of the
 *	  internal complexity for consumers of logical decoding, so they can
 *	  create and consume a changestream with a low amount of code. Builtin
 *	  consumers are the walsender and SQL SRF interface, but it's possible
 *to add further ones without changing core code, e.g. to consume changes in a
 *bgworker.
 *
 *	  The idea is that a consumer provides three callbacks, one to read WAL,
 *	  one to prepare a data write, and a final one for actually writing
 *since their implementation depends on the type of consumer.  Check
 *	  logicalfuncs.c for an example implementation of a fairly simple
 *consumer and an implementation of a WAL reading callback that's suitable for
 *	  simple consumers.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"

#include "access/xact.h"
#include "access/xlog_internal.h"

#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/reorderbuffer.h"
#include "replication/origin.h"
#include "replication/snapbuild.h"

#include "storage/proc.h"
#include "storage/procarray.h"

#include "utils/memutils.h"

/* data for errcontext callback */
typedef struct LogicalErrorCallbackState
{
  LogicalDecodingContext *ctx;
  const char *callback_name;
  XLogRecPtr report_location;
} LogicalErrorCallbackState;

/* wrappers around output plugin callbacks */
static void
output_plugin_error_callback(void *arg);
static void
startup_cb_wrapper(LogicalDecodingContext *ctx, OutputPluginOptions *opt, bool is_init);
static void
shutdown_cb_wrapper(LogicalDecodingContext *ctx);
static void
begin_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn);
static void
commit_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr commit_lsn);
static void
change_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change);
static void
truncate_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, int nrelations, Relation relations[], ReorderBufferChange *change);
static void
message_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr message_lsn, bool transactional, const char *prefix, Size message_size, const char *message);

static void
LoadOutputPlugin(OutputPluginCallbacks *callbacks, char *plugin);

/*
 * Make sure the current settings & environment are capable of doing logical
 * decoding.
 */
void
CheckLogicalDecodingRequirements(void)
{


































}

/*
 * Helper function for CreateInitDecodingContext() and
 * CreateDecodingContext() performing common tasks.
 */
static LogicalDecodingContext *
StartupDecodingContext(List *output_plugin_options, XLogRecPtr start_lsn, TransactionId xmin_horizon, bool need_full_snapshot, bool fast_forward, XLogPageReadCB read_page, LogicalOutputPluginWriterPrepareWrite prepare_write, LogicalOutputPluginWriterWrite do_write, LogicalOutputPluginWriterUpdateProgress update_progress)
{









































































}

/*
 * Create a new decoding context, for a new logical slot.
 *
 * plugin -- contains the name of the output plugin
 * output_plugin_options -- contains options passed to the output plugin
 * need_full_snapshot -- if true, must obtain a snapshot able to read all
 *		tables; if false, one that can read only catalogs is acceptable.
 * restart_lsn -- if given as invalid, it's this routine's responsibility to
 *		mark WAL as reserved by setting a convenient restart_lsn for the
 *slot. Otherwise, we set for decoding to start from the given LSN without
 *		marking WAL reserved beforehand.  In that scenario, it's up to
 *the caller to guarantee that WAL remains available. read_page, prepare_write,
 *do_write, update_progress -- callbacks that perform the use-case dependent,
 *actual, work.
 *
 * Needs to be called while in a memory context that's at least as long lived
 * as the decoding context because further memory contexts will be created
 * inside it.
 *
 * Returns an initialized decoding context after calling the output plugin's
 * startup function.
 */
LogicalDecodingContext *
CreateInitDecodingContext(char *plugin, List *output_plugin_options, bool need_full_snapshot, XLogRecPtr restart_lsn, XLogPageReadCB read_page, LogicalOutputPluginWriterPrepareWrite prepare_write, LogicalOutputPluginWriterWrite do_write, LogicalOutputPluginWriterUpdateProgress update_progress)
{













































































































}

/*
 * Create a new decoding context, for a logical slot that has previously been
 * used already.
 *
 * start_lsn
 *		The LSN at which to start decoding.  If InvalidXLogRecPtr,
 *restart from the slot's confirmed_flush; otherwise, start from the specified
 *		location (but move it forwards to confirmed_flush if it's older
 *than that, see below).
 *
 * output_plugin_options
 *		options passed to the output plugin.
 *
 * fast_forward
 *		bypass the generation of logical changes.
 *
 * read_page, prepare_write, do_write, update_progress
 *		callbacks that have to be filled to perform the use-case
 *dependent, actual work.
 *
 * Needs to be called while in a memory context that's at least as long lived
 * as the decoding context because further memory contexts will be created
 * inside it.
 *
 * Returns an initialized decoding context after calling the output plugin's
 * startup function.
 */
LogicalDecodingContext *
CreateDecodingContext(XLogRecPtr start_lsn, List *output_plugin_options, bool fast_forward, XLogPageReadCB read_page, LogicalOutputPluginWriterPrepareWrite prepare_write, LogicalOutputPluginWriterWrite do_write, LogicalOutputPluginWriterUpdateProgress update_progress)
{



























































}

/*
 * Returns true if a consistent initial decoding snapshot has been built.
 */
bool
DecodingContextReady(LogicalDecodingContext *ctx)
{

}

/*
 * Read from the decoding slot, until it is ready to start extracting changes.
 */
void
DecodingContextFindStartpoint(LogicalDecodingContext *ctx)
{









































}

/*
 * Free a previously allocated decoding context, invoking the shutdown
 * callback if necessary.
 */
void
FreeDecodingContext(LogicalDecodingContext *ctx)
{









}

/*
 * Prepare a write using the context's output routine.
 */
void
OutputPluginPrepareWrite(struct LogicalDecodingContext *ctx, bool last_write)
{







}

/*
 * Perform a write using the context's output routine.
 */
void
OutputPluginWrite(struct LogicalDecodingContext *ctx, bool last_write)
{







}

/*
 * Update progress tracking (if supported).
 */
void
OutputPluginUpdateProgress(struct LogicalDecodingContext *ctx)
{






}

/*
 * Load the output plugin, lookup its output plugin init function, and check
 * that it provides the required callbacks.
 */
static void
LoadOutputPlugin(OutputPluginCallbacks *callbacks, char *plugin)
{
























}

static void
output_plugin_error_callback(void *arg)
{











}

static void
startup_cb_wrapper(LogicalDecodingContext *ctx, OutputPluginOptions *opt, bool is_init)
{























}

static void
shutdown_cb_wrapper(LogicalDecodingContext *ctx)
{























}

/*
 * Callbacks for ReorderBuffer which add in some more information and then call
 * output_plugin.h plugins.
 */
static void
begin_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn)
{


























}

static void
commit_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr commit_lsn)
{


























}

static void
change_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{

































}

static void
truncate_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, int nrelations, Relation relations[], ReorderBufferChange *change)
{






































}

bool
filter_by_origin_cb_wrapper(LogicalDecodingContext *ctx, RepOriginId origin_id)
{


























}

static void
message_cb_wrapper(ReorderBuffer *cache, ReorderBufferTXN *txn, XLogRecPtr message_lsn, bool transactional, const char *prefix, Size message_size, const char *message)
{































}

/*
 * Set the required catalog xmin horizon for historic snapshots in the current
 * replication slot.
 *
 * Note that in the most cases, we won't be able to immediately use the xmin
 * to increase the xmin horizon: we need to wait till the client has confirmed
 * receiving current_lsn with LogicalConfirmReceivedLocation().
 */
void
LogicalIncreaseXminForSlot(XLogRecPtr current_lsn, TransactionId xmin)
{















































}

/*
 * Mark the minimal LSN (restart_lsn) we need to read to replay all
 * transactions that have not yet committed at current_lsn.
 *
 * Just like LogicalIncreaseXminForSlot this only takes effect when the
 * client has confirmed to have received current_lsn.
 */
void
LogicalIncreaseRestartDecodingForSlot(XLogRecPtr current_lsn, XLogRecPtr restart_lsn)
{





























































}

/*
 * Handle a consumer's confirmation having received all changes up to lsn.
 */
void
LogicalConfirmReceivedLocation(XLogRecPtr lsn)
{











































































}
