/*-------------------------------------------------------------------------
 *
 * tstoreReceiver.c
 *	  An implementation of DestReceiver that stores the result tuples in
 *	  a Tuplestore.
 *
 * Optionally, we can force detoasting (but not decompression) of out-of-line
 * toasted values.  This is to support cursors WITH HOLD, which must retain
 * data even if the underlying table is dropped.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/tstoreReceiver.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/tuptoaster.h"
#include "executor/tstoreReceiver.h"

typedef struct
{
  DestReceiver pub;
  /* parameters: */
  Tuplestorestate *tstore; /* where to put the data */
  MemoryContext cxt;       /* context containing tstore */
  bool detoast;            /* were we told to detoast? */
  /* workspace: */
  Datum *outvalues; /* values array for result tuple */
  Datum *tofree;    /* temp values to be pfree'd */
} TStoreState;

static bool
tstoreReceiveSlot_notoast(TupleTableSlot *slot, DestReceiver *self);
static bool
tstoreReceiveSlot_detoast(TupleTableSlot *slot, DestReceiver *self);

/*
 * Prepare to receive tuples from executor.
 */
static void
tstoreStartupReceiver(DestReceiver *self, int operation, TupleDesc typeinfo)
{






































}

/*
 * Receive a tuple from the executor and store it in the tuplestore.
 * This is for the easy case where we don't have to detoast.
 */
static bool
tstoreReceiveSlot_notoast(TupleTableSlot *slot, DestReceiver *self)
{





}

/*
 * Receive a tuple from the executor and store it in the tuplestore.
 * This is for the case where we have to detoast any toasted values.
 */
static bool
tstoreReceiveSlot_detoast(TupleTableSlot *slot, DestReceiver *self)
{















































}

/*
 * Clean up at end of an executor run
 */
static void
tstoreShutdownReceiver(DestReceiver *self)
{













}

/*
 * Destroy receiver when done with it
 */
static void
tstoreDestroyReceiver(DestReceiver *self)
{

}

/*
 * Initially create a DestReceiver object.
 */
DestReceiver *
CreateTuplestoreDestReceiver(void)
{











}

/*
 * Set parameters for a TuplestoreDestReceiver
 */
void
SetTuplestoreDestReceiverParams(DestReceiver *self, Tuplestorestate *tStore, MemoryContext tContext, bool detoast)
{






}