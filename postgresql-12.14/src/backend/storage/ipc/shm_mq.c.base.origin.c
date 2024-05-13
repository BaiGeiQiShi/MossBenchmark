/*-------------------------------------------------------------------------
 *
 * shm_mq.c
 *	  single-reader, single-writer shared memory message queue
 *
 * Both the sender and the receiver must have a PGPROC; their respective
 * process latches are used for synchronization.  Only the sender may send,
 * and only the receiver may receive.  This is intended to allow a user
 * backend to communicate with worker backends that it has registered.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/storage/ipc/shm_mq.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/procsignal.h"
#include "storage/shm_mq.h"
#include "storage/spin.h"
#include "utils/memutils.h"

/*
 * This structure represents the actual queue, stored in shared memory.
 *
 * Some notes on synchronization:
 *
 * mq_receiver and mq_bytes_read can only be changed by the receiver; and
 * mq_sender and mq_bytes_written can only be changed by the sender.
 * mq_receiver and mq_sender are protected by mq_mutex, although, importantly,
 * they cannot change once set, and thus may be read without a lock once this
 * is known to be the case.
 *
 * mq_bytes_read and mq_bytes_written are not protected by the mutex.  Instead,
 * they are written atomically using 8 byte loads and stores.  Memory barriers
 * must be carefully used to synchronize reads and writes of these values with
 * reads and writes of the actual data in mq_ring.
 *
 * mq_detached needs no locking.  It can be set by either the sender or the
 * receiver, but only ever from false to true, so redundant writes don't
 * matter.  It is important that if we set mq_detached and then set the
 * counterparty's latch, the counterparty must be certain to see the change
 * after waking up.  Since SetLatch begins with a memory barrier and ResetLatch
 * ends with one, this should be OK.
 *
 * mq_ring_size and mq_ring_offset never change after initialization, and
 * can therefore be read without the lock.
 *
 * Importantly, mq_ring can be safely read and written without a lock.
 * At any given time, the difference between mq_bytes_read and
 * mq_bytes_written defines the number of bytes within mq_ring that contain
 * unread data, and mq_bytes_read defines the position where those bytes
 * begin.  The sender can increase the number of unread bytes at any time,
 * but only the receiver can give license to overwrite those bytes, by
 * incrementing mq_bytes_read.  Therefore, it's safe for the receiver to read
 * the unread bytes it knows to be present without the lock.  Conversely,
 * the sender can write to the unused portion of the ring buffer without
 * the lock, because nobody else can be reading or writing those bytes.  The
 * receiver could be making more bytes unused by incrementing mq_bytes_read,
 * but that's OK.  Note that it would be unsafe for the receiver to read any
 * data it's already marked as read, or to write any data; and it would be
 * unsafe for the sender to reread any data after incrementing
 * mq_bytes_written, but fortunately there's no need for any of that.
 */
struct shm_mq
{
  slock_t mq_mutex;
  PGPROC *mq_receiver;
  PGPROC *mq_sender;
  pg_atomic_uint64 mq_bytes_read;
  pg_atomic_uint64 mq_bytes_written;
  Size mq_ring_size;
  bool mq_detached;
  uint8 mq_ring_offset;
  char mq_ring[FLEXIBLE_ARRAY_MEMBER];
};

/*
 * This structure is a backend-private handle for access to a queue.
 *
 * mqh_queue is a pointer to the queue we've attached, and mqh_segment is
 * an optional pointer to the dynamic shared memory segment that contains it.
 * (If mqh_segment is provided, we register an on_dsm_detach callback to
 * make sure we detach from the queue before detaching from DSM.)
 *
 * If this queue is intended to connect the current process with a background
 * worker that started it, the user can pass a pointer to the worker handle
 * to shm_mq_attach(), and we'll store it in mqh_handle.  The point of this
 * is to allow us to begin sending to or receiving from that queue before the
 * process we'll be communicating with has even been started.  If it fails
 * to start, the handle will allow us to notice that and fail cleanly, rather
 * than waiting forever; see shm_mq_wait_internal.  This is mostly useful in
 * simple cases - e.g. where there are just 2 processes communicating; in
 * more complex scenarios, every process may not have a BackgroundWorkerHandle
 * available, or may need to watch for the failure of more than one other
 * process at a time.
 *
 * When a message exists as a contiguous chunk of bytes in the queue - that is,
 * it is smaller than the size of the ring buffer and does not wrap around
 * the end - we return the message to the caller as a pointer into the buffer.
 * For messages that are larger or happen to wrap, we reassemble the message
 * locally by copying the chunks into a backend-local buffer.  mqh_buffer is
 * the buffer, and mqh_buflen is the number of bytes allocated for it.
 *
 * mqh_partial_bytes, mqh_expected_bytes, and mqh_length_word_complete
 * are used to track the state of non-blocking operations.  When the caller
 * attempts a non-blocking operation that returns SHM_MQ_WOULD_BLOCK, they
 * are expected to retry the call at a later time with the same argument;
 * we need to retain enough state to pick up where we left off.
 * mqh_length_word_complete tracks whether we are done sending or receiving
 * (whichever we're doing) the entire length word.  mqh_partial_bytes tracks
 * the number of bytes read or written for either the length word or the
 * message itself, and mqh_expected_bytes - which is used only for reads -
 * tracks the expected total size of the payload.
 *
 * mqh_counterparty_attached tracks whether we know the counterparty to have
 * attached to the queue at some previous point.  This lets us avoid some
 * mutex acquisitions.
 *
 * mqh_context is the memory context in effect at the time we attached to
 * the shm_mq.  The shm_mq_handle itself is allocated in this context, and
 * we make sure any other allocations we do happen in this context as well,
 * to avoid nasty surprises.
 */
struct shm_mq_handle
{
  shm_mq *mqh_queue;
  dsm_segment *mqh_segment;
  BackgroundWorkerHandle *mqh_handle;
  char *mqh_buffer;
  Size mqh_buflen;
  Size mqh_consume_pending;
  Size mqh_partial_bytes;
  Size mqh_expected_bytes;
  bool mqh_length_word_complete;
  bool mqh_counterparty_attached;
  MemoryContext mqh_context;
};

static void
shm_mq_detach_internal(shm_mq *mq);
static shm_mq_result
shm_mq_send_bytes(shm_mq_handle *mqh, Size nbytes, const void *data, bool nowait, Size *bytes_written);
static shm_mq_result
shm_mq_receive_bytes(shm_mq_handle *mqh, Size bytes_needed, bool nowait, Size *nbytesp, void **datap);
static bool
shm_mq_counterparty_gone(shm_mq *mq, BackgroundWorkerHandle *handle);
static bool
shm_mq_wait_internal(shm_mq *mq, PGPROC **ptr, BackgroundWorkerHandle *handle);
static void
shm_mq_inc_bytes_read(shm_mq *mq, Size n);
static void
shm_mq_inc_bytes_written(shm_mq *mq, Size n);
static void
shm_mq_detach_callback(dsm_segment *seg, Datum arg);

/* Minimum queue size is enough for header and at least one chunk of data. */
const Size shm_mq_minimum_size = MAXALIGN(offsetof(shm_mq, mq_ring)) + MAXIMUM_ALIGNOF;

#define MQH_INITIAL_BUFSIZE 8192

/*
 * Initialize a new shared message queue.
 */
shm_mq *
shm_mq_create(void *address, Size size)
{




















}

/*
 * Set the identity of the process that will receive from a shared message
 * queue.
 */
void
shm_mq_set_receiver(shm_mq *mq, PGPROC *proc)
{












}

/*
 * Set the identity of the process that will send to a shared message queue.
 */
void
shm_mq_set_sender(shm_mq *mq, PGPROC *proc)
{












}

/*
 * Get the configured receiver.
 */
PGPROC *
shm_mq_get_receiver(shm_mq *mq)
{







}

/*
 * Get the configured sender.
 */
PGPROC *
shm_mq_get_sender(shm_mq *mq)
{







}

/*
 * Attach to a shared message queue so we can send or receive messages.
 *
 * The memory context in effect at the time this function is called should
 * be one which will last for at least as long as the message queue itself.
 * We'll allocate the handle in that context, and future allocations that
 * are needed to buffer incoming data will happen in that context as well.
 *
 * If seg != NULL, the queue will be automatically detached when that dynamic
 * shared memory segment is detached.
 *
 * If handle != NULL, the queue can be read or written even before the
 * other process has attached.  We'll wait for it to do so if needed.  The
 * handle must be for a background worker initialized with bgw_notify_pid
 * equal to our PID.
 *
 * shm_mq_detach() should be called when done.  This will free the
 * shm_mq_handle and mark the queue itself as detached, so that our
 * counterpart won't get stuck waiting for us to fill or drain the queue
 * after we've already lost interest.
 */
shm_mq_handle *
shm_mq_attach(shm_mq *mq, dsm_segment *seg, BackgroundWorkerHandle *handle)
{





















}

/*
 * Associate a BackgroundWorkerHandle with a shm_mq_handle just as if it had
 * been passed to shm_mq_attach.
 */
void
shm_mq_set_handle(shm_mq_handle *mqh, BackgroundWorkerHandle *handle)
{


}

/*
 * Write a message into a shared message queue.
 */
shm_mq_result
shm_mq_send(shm_mq_handle *mqh, Size nbytes, const void *data, bool nowait)
{






}

/*
 * Write a message into a shared message queue, gathered from multiple
 * addresses.
 *
 * When nowait = false, we'll wait on our process latch when the ring buffer
 * fills up, and then continue writing once the receiver has drained some data.
 * The process latch is reset after each wait.
 *
 * When nowait = true, we do not manipulate the state of the process latch;
 * instead, if the buffer becomes full, we return SHM_MQ_WOULD_BLOCK.  In
 * this case, the caller should call this function again, with the same
 * arguments, each time the process latch is set.  (Once begun, the sending
 * of a message cannot be aborted except by detaching from the queue; changing
 * the length or payload will corrupt the queue.)
 */
shm_mq_result
shm_mq_sendv(shm_mq_handle *mqh, shm_mq_iovec *iov, int iovcnt, bool nowait)
{































































































































































































}

/*
 * Receive a message from a shared message queue.
 *
 * We set *nbytes to the message length and *data to point to the message
 * payload.  If the entire message exists in the queue as a single,
 * contiguous chunk, *data will point directly into shared memory; otherwise,
 * it will point to a temporary buffer.  This mostly avoids data copying in
 * the hoped-for case where messages are short compared to the buffer size,
 * while still allowing longer messages.  In either case, the return value
 * remains valid until the next receive operation is performed on the queue.
 *
 * When nowait = false, we'll wait on our process latch when the ring buffer
 * is empty and we have not yet received a full message.  The sender will
 * set our process latch after more data has been written, and we'll resume
 * processing.  Each call will therefore return a complete message
 * (unless the sender detaches the queue).
 *
 * When nowait = true, we do not manipulate the state of the process latch;
 * instead, whenever the buffer is empty and we need to read from it, we
 * return SHM_MQ_WOULD_BLOCK.  In this case, the caller should call this
 * function again after the process latch has been set.
 */
shm_mq_result
shm_mq_receive(shm_mq_handle *mqh, Size *nbytesp, void **datap, bool nowait)
{


























































































































































































































































}

/*
 * Wait for the other process that's supposed to use this queue to attach
 * to it.
 *
 * The return value is SHM_MQ_DETACHED if the worker has already detached or
 * if it dies; it is SHM_MQ_SUCCESS if we detect that the worker has attached.
 * Note that we will only be able to detect that the worker has died before
 * attaching if a background worker handle was passed to shm_mq_attach().
 */
shm_mq_result
shm_mq_wait_for_attach(shm_mq_handle *mqh)
{





















}

/*
 * Detach from a shared message queue, and destroy the shm_mq_handle.
 */
void
shm_mq_detach(shm_mq_handle *mqh)
{















}

/*
 * Notify counterparty that we're detaching from shared message queue.
 *
 * The purpose of this function is to make sure that the process
 * with which we're communicating doesn't block forever waiting for us to
 * fill or drain the queue once we've lost interest.  When the sender
 * detaches, the receiver can read any messages remaining in the queue;
 * further reads will return SHM_MQ_DETACHED.  If the receiver detaches,
 * further attempts to send messages will likewise return SHM_MQ_DETACHED.
 *
 * This is separated out from shm_mq_detach() because if the on_dsm_detach
 * callback fires, we only want to do this much.  We do not try to touch
 * the local shm_mq_handle, as it may have been pfree'd already.
 */
static void
shm_mq_detach_internal(shm_mq *mq)
{



















}

/*
 * Get the shm_mq from handle.
 */
shm_mq *
shm_mq_get_queue(shm_mq_handle *mqh)
{

}

/*
 * Write bytes into a shared message queue.
 */
static shm_mq_result
shm_mq_send_bytes(shm_mq_handle *mqh, Size nbytes, const void *data, bool nowait, Size *bytes_written)
{









































































































































}

/*
 * Wait until at least *nbytesp bytes are available to be read from the
 * shared message queue, or until the buffer wraps around.  If the queue is
 * detached, returns SHM_MQ_DETACHED.  If nowait is specified and a wait
 * would be required, returns SHM_MQ_WOULD_BLOCK.  Otherwise, *datap is set
 * to the location at which data bytes can be read, *nbytesp is set to the
 * number of bytes which can be read at that address, and the return value
 * is SHM_MQ_SUCCESS.
 */
static shm_mq_result
shm_mq_receive_bytes(shm_mq_handle *mqh, Size bytes_needed, bool nowait, Size *nbytesp, void **datap)
{





























































































}

/*
 * Test whether a counterparty who may not even be alive yet is definitely gone.
 */
static bool
shm_mq_counterparty_gone(shm_mq *mq, BackgroundWorkerHandle *handle)
{

























}

/*
 * This is used when a process is waiting for its counterpart to attach to the
 * queue.  We exit when the other process attaches as expected, or, if
 * handle != NULL, when the referenced background process or the postmaster
 * dies.  Note that if handle == NULL, and the process fails to attach, we'll
 * potentially get stuck here forever waiting for a process that may never
 * start.  We do check for interrupts, though.
 *
 * ptr is a pointer to the memory address that we're expecting to become
 * non-NULL when our counterpart attaches to the queue.
 */
static bool
shm_mq_wait_internal(shm_mq *mq, PGPROC **ptr, BackgroundWorkerHandle *handle)
{













































}

/*
 * Increment the number of bytes read.
 */
static void
shm_mq_inc_bytes_read(shm_mq *mq, Size n)
{
























}

/*
 * Increment the number of bytes written.
 */
static void
shm_mq_inc_bytes_written(shm_mq *mq, Size n)
{













}

/* Shim for on_dsm_callback. */
static void
shm_mq_detach_callback(dsm_segment *seg, Datum arg)
{



}