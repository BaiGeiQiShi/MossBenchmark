/*-------------------------------------------------------------------------
 *
 * pqmq.c
 *	  Use the frontend/backend protocol for communication over a shm_mq
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/libpq/pqmq.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqmq.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"

static shm_mq_handle *pq_mq_handle;
static bool pq_mq_busy = false;
static pid_t pq_mq_parallel_master_pid = 0;
static pid_t pq_mq_parallel_master_backend_id = InvalidBackendId;

static void
pq_cleanup_redirect_to_shm_mq(dsm_segment *seg, Datum arg);
static void
mq_comm_reset(void);
static int
mq_flush(void);
static int
mq_flush_if_writable(void);
static bool
mq_is_send_pending(void);
static int
mq_putmessage(char msgtype, const char *s, size_t len);
static void
mq_putmessage_noblock(char msgtype, const char *s, size_t len);
static void
mq_startcopyout(void);
static void
mq_endcopyout(bool errorAbort);

static const PQcommMethods PqCommMqMethods = {mq_comm_reset, mq_flush, mq_flush_if_writable, mq_is_send_pending, mq_putmessage, mq_putmessage_noblock, mq_startcopyout, mq_endcopyout};

/*
 * Arrange to redirect frontend/backend protocol messages to a shared-memory
 * message queue.
 */
void
pq_redirect_to_shm_mq(dsm_segment *seg, shm_mq_handle *mqh)
{





}

/*
 * When the DSM that contains our shm_mq goes away, we need to stop sending
 * messages to it.
 */
static void
pq_cleanup_redirect_to_shm_mq(dsm_segment *seg, Datum arg)
{


}

/*
 * Arrange to SendProcSignal() to the parallel master each time we transmit
 * message data via the shm_mq.
 */
void
pq_set_parallel_master(pid_t pid, BackendId backend_id)
{



}

static void
mq_comm_reset(void)
{
}

static int
mq_flush(void)
{


}

static int
mq_flush_if_writable(void)
{


}

static bool
mq_is_send_pending(void)
{


}

/*
 * Transmit a libpq protocol message to the shared memory message queue
 * selected via pq_mq_handle.  We don't include a length word, because the
 * receiver will know the length of the message from shm_mq_receive().
 */
static int
mq_putmessage(char msgtype, const char *s, size_t len)
{




































































}

static void
mq_putmessage_noblock(char msgtype, const char *s, size_t len)
{








}

static void
mq_startcopyout(void)
{
}

static void
mq_endcopyout(bool errorAbort)
{
}

/*
 * Parse an ErrorResponse or NoticeResponse payload and populate an ErrorData
 * structure with the results.
 */
void
pq_parse_errornotice(StringInfo msg, ErrorData *edata)
{































































































































}