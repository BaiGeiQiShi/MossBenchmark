/*-------------------------------------------------------------------------
 *
 * subtrans.c
 *		PostgreSQL subtransaction-log manager
 *
 * The pg_subtrans manager is a pg_xact-like manager that stores the parent
 * transaction Id for each transaction.  It is a fundamental part of the
 * nested transactions implementation.  A main transaction has a parent
 * of InvalidTransactionId, and each subtransaction has its immediate parent.
 * The tree can easily be walked from child to parent, but not in the
 * opposite direction.
 *
 * This code is based on xact.c, but the robustness requirements
 * are completely different from pg_xact, because we only need to remember
 * pg_subtrans information for currently-open transactions.  Thus, there is
 * no need to preserve data over a crash and restart.
 *
 * There are no XLOG interactions since we do not care about preserving
 * data across crashes.  During database startup, we simply force the
 * currently-active page of SUBTRANS to zeroes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/subtrans.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/slru.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "pg_trace.h"
#include "utils/snapmgr.h"

/*
 * Defines for SubTrans page sizes.  A page is the same BLCKSZ as is used
 * everywhere else in Postgres.
 *
 * Note: because TransactionIds are 32 bits and wrap around at 0xFFFFFFFF,
 * SubTrans page numbering also wraps around at
 * 0xFFFFFFFF/SUBTRANS_XACTS_PER_PAGE, and segment numbering at
 * 0xFFFFFFFF/SUBTRANS_XACTS_PER_PAGE/SLRU_PAGES_PER_SEGMENT.  We need take no
 * explicit notice of that fact in this module, except when comparing segment
 * and page numbers in TruncateSUBTRANS (see SubTransPagePrecedes) and zeroing
 * them in StartupSUBTRANS.
 */

/* We need four bytes per xact */
#define SUBTRANS_XACTS_PER_PAGE (BLCKSZ / sizeof(TransactionId))

#define TransactionIdToPage(xid) ((xid) / (TransactionId)SUBTRANS_XACTS_PER_PAGE)
#define TransactionIdToEntry(xid) ((xid) % (TransactionId)SUBTRANS_XACTS_PER_PAGE)

/*
 * Link to shared-memory data structures for SUBTRANS control
 */
static SlruCtlData SubTransCtlData;

#define SubTransCtl (&SubTransCtlData)

static int
ZeroSUBTRANSPage(int pageno);
static bool
SubTransPagePrecedes(int page1, int page2);

/*
 * Record the parent of a subtransaction in the subtrans log.
 */
void
SubTransSetParent(TransactionId xid, TransactionId parent)
{



























}

/*
 * Interrogate the parent of a transaction in the subtrans log.
 */
TransactionId
SubTransGetParent(TransactionId xid)
{


























}

/*
 * SubTransGetTopmostTransaction
 *
 * Returns the topmost transaction of the given transaction id.
 *
 * Because we cannot look back further than TransactionXmin, it is possible
 * that this function will lie and return an intermediate subtransaction ID
 * instead of the true topmost parent ID.  This is OK, because in practice
 * we only care about detecting whether the topmost parent is still running
 * or is part of a current snapshot's list of still-running transactions.
 * Therefore, any XID before TransactionXmin is as good as any other.
 */
TransactionId
SubTransGetTopmostTransaction(TransactionId xid)
{




























}

/*
 * Initialization of shared memory for SUBTRANS
 */
Size
SUBTRANSShmemSize(void)
{

}

void
SUBTRANSShmemInit(void)
{





}

/*
 * This func must be called ONCE on system install.  It creates
 * the initial SUBTRANS segment.  (The SUBTRANS directory is assumed to
 * have been created by the initdb shell script, and SUBTRANSShmemInit
 * must have been called already.)
 *
 * Note: it's not really necessary to create the initial segment now,
 * since slru.c would create it on first write anyway.  But we may as well
 * do it to be sure the directory is set up correctly.
 */
void
BootStrapSUBTRANS(void)
{












}

/*
 * Initialize (or reinitialize) a page of SUBTRANS to zeroes.
 *
 * The page is not actually written, just set up in shared memory.
 * The slot number of the new page is returned.
 *
 * Control lock must be held at entry, and will be held at exit.
 */
static int
ZeroSUBTRANSPage(int pageno)
{

}

/*
 * This must be called ONCE during postmaster or standalone-backend startup,
 * after StartupXLOG has initialized ShmemVariableCache->nextFullXid.
 *
 * oldestActiveXID is the oldest XID of any prepared transaction, or nextFullXid
 * if there are none.
 */
void
StartupSUBTRANS(TransactionId oldestActiveXID)
{





























}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
ShutdownSUBTRANS(void)
{









}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 */
void
CheckPointSUBTRANS(void)
{










}

/*
 * Make sure that SUBTRANS has room for a newly-allocated XID.
 *
 * NB: this is called while holding XidGenLock.  We want it to be very fast
 * most of the time; even when it's not so fast, no actual I/O need happen
 * unless we're forced to write out a dirty subtrans page to make room
 * in shared memory.
 */
void
ExtendSUBTRANS(TransactionId newestXact)
{



















}

/*
 * Remove all SUBTRANS segments before the one holding the passed transaction ID
 *
 * oldestXact is the oldest TransactionXmin of any running transaction.  This
 * is called only during checkpoint.
 */
void
TruncateSUBTRANS(TransactionId oldestXact)
{














}

/*
 * Decide whether a SUBTRANS page number is "older" for truncation purposes.
 * Analogous to CLOGPagePrecedes().
 */
static bool
SubTransPagePrecedes(int page1, int page2)
{









}