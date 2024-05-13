/*-------------------------------------------------------------------------
 *
 * tqueue.c
 *	  Use shm_mq to send & receive tuples between parallel backends
 *
 * A DestReceiver of type DestTupleQueue, which is a TQueueDestReceiver
 * under the hood, writes tuples from the executor to a shm_mq.
 *
 * A TupleQueueReader reads tuples from a shm_mq and returns the tuples.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/tqueue.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "executor/tqueue.h"

/*
 * DestReceiver object's private contents
 *
 * queue is a pointer to data supplied by DestReceiver's caller.
 */
typedef struct TQueueDestReceiver
{
  DestReceiver pub;     /* public fields */
  shm_mq_handle *queue; /* shm_mq to send to */
} TQueueDestReceiver;

/*
 * TupleQueueReader object's private contents
 *
 * queue is a pointer to data supplied by reader's caller.
 *
 * "typedef struct TupleQueueReader TupleQueueReader" is in tqueue.h
 */
struct TupleQueueReader
{
  shm_mq_handle *queue; /* shm_mq to receive from */
};

/*
 * Receive a tuple from a query, and send it to the designated shm_mq.
 *
 * Returns true if successful, false if shm_mq has been detached.
 */
static bool
tqueueReceiveSlot(TupleTableSlot *slot, DestReceiver *self)
{

























}

/*
 * Prepare to receive tuples from executor.
 */
static void
tqueueStartupReceiver(DestReceiver *self, int operation, TupleDesc typeinfo)
{

}

/*
 * Clean up at end of an executor run
 */
static void
tqueueShutdownReceiver(DestReceiver *self)
{







}

/*
 * Destroy receiver when done with it
 */
static void
tqueueDestroyReceiver(DestReceiver *self)
{








}

/*
 * Create a DestReceiver that writes tuples to a tuple queue.
 */
DestReceiver *
CreateTupleQueueDestReceiver(shm_mq_handle *handle)
{












}

/*
 * Create a tuple queue reader.
 */
TupleQueueReader *
CreateTupleQueueReader(shm_mq_handle *handle)
{





}

/*
 * Destroy a tuple queue reader.
 *
 * Note: cleaning up the underlying shm_mq is the caller's responsibility.
 * We won't access it here, as it may be detached already.
 */
void
DestroyTupleQueueReader(TupleQueueReader *reader)
{

}

/*
 * Fetch a tuple from a tuple queue reader.
 *
 * The return value is NULL if there are no remaining tuples or if
 * nowait = true and no tuple is ready to return.  *done, if not NULL,
 * is set to true when there are no remaining tuples and otherwise to false.
 *
 * The returned tuple, if any, is allocated in CurrentMemoryContext.
 * Note that this routine must not leak memory!  (We used to allow that,
 * but not any more.)
 *
 * Even when shm_mq_receive() returns SHM_MQ_WOULD_BLOCK, this can still
 * accumulate bytes from a partially-read message, so it's useful to call
 * this with nowait = true even if nothing is returned.
 */
HeapTuple
TupleQueueReaderNext(TupleQueueReader *reader, bool nowait, bool *done)
{








































}