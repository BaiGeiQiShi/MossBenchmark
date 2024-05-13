/*-------------------------------------------------------------------------
 *
 * dest.c
 *	  support for communication destinations
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/tcop/dest.c
 *
 *-------------------------------------------------------------------------
 */
/*
 *	 INTERFACE ROUTINES
 *		BeginCommand - initialize the destination at start of command
 *		CreateDestReceiver - create tuple receiver object for
 *destination EndCommand - clean up the destination at end of command
 *		NullCommand - tell dest that an empty query string was
 *recognized ReadyForQuery - tell dest that we are ready for a new query
 *
 *	 NOTES
 *		These routines do the appropriate work before and after
 *		tuples are returned by a query to keep the backend and the
 *		"destination" portals synchronized.
 */

#include "postgres.h"

#include "access/printsimple.h"
#include "access/printtup.h"
#include "access/xact.h"
#include "commands/copy.h"
#include "commands/createas.h"
#include "commands/matview.h"
#include "executor/functions.h"
#include "executor/tqueue.h"
#include "executor/tstoreReceiver.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "utils/portal.h"

/* ----------------
 *		dummy DestReceiver functions
 * ----------------
 */
static bool
donothingReceive(TupleTableSlot *slot, DestReceiver *self)
{

}

static void
donothingStartup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
}

static void
donothingCleanup(DestReceiver *self)
{

}

/* ----------------
 *		static DestReceiver structs for dest types needing no local
 *state
 * ----------------
 */
static const DestReceiver donothingDR = {donothingReceive, donothingStartup, donothingCleanup, donothingCleanup, DestNone};

static const DestReceiver debugtupDR = {debugtup, debugStartup, donothingCleanup, donothingCleanup, DestDebug};

static const DestReceiver printsimpleDR = {printsimple, printsimple_startup, donothingCleanup, donothingCleanup, DestRemoteSimple};

static const DestReceiver spi_printtupDR = {spi_printtup, spi_dest_startup, donothingCleanup, donothingCleanup, DestSPI};

/*
 * Globally available receiver for DestNone.
 *
 * It's ok to cast the constness away as any modification of the none receiver
 * would be a bug (which gets easier to catch this way).
 */
DestReceiver *None_Receiver = (DestReceiver *)&donothingDR;

/* ----------------
 *		BeginCommand - initialize the destination at start of command
 * ----------------
 */
void
BeginCommand(const char *commandTag, CommandDest dest)
{

}

/* ----------------
 *		CreateDestReceiver - return appropriate receiver function set
 *for dest
 * ----------------
 */
DestReceiver *
CreateDestReceiver(CommandDest dest)
{












































}

/* ----------------
 *		EndCommand - clean up the destination at end of command
 * ----------------
 */
void
EndCommand(const char *commandTag, CommandDest dest)
{
























}

/* ----------------
 *		NullCommand - tell dest that an empty query string was
 *recognized
 *
 *		In FE/BE protocol version 1.0, this hack is necessary to support
 *		libpq's crufty way of determining whether a multiple-command
 *		query string is done.  In protocol 2.0 it's probably not really
 *		necessary to distinguish empty queries anymore, but we still do
 *it for backwards compatibility with 1.0.  In protocol 3.0 it has some use
 *again, since it ensures that there will be a recognizable end to the response
 *to an Execute message.
 * ----------------
 */
void
NullCommand(CommandDest dest)
{































}

/* ----------------
 *		ReadyForQuery - tell dest that we are ready for a new query
 *
 *		The ReadyForQuery message is sent so that the FE can tell when
 *		we are done processing a query string.
 *		In versions 3.0 and up, it also carries a transaction state
 *indicator.
 *
 *		Note that by flushing the stdio buffer here, we can avoid doing
 *it most other places and thus reduce the number of separate packets sent.
 * ----------------
 */
void
ReadyForQuery(CommandDest dest)
{
































}