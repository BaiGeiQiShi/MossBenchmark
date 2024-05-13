/*-------------------------------------------------------------------------
 *
 * hio.c
 *	  POSTGRES heap access method input/output code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/heap/hio.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/hio.h"
#include "access/htup_details.h"
#include "access/visibilitymap.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"

/*
 * RelationPutHeapTuple - place tuple at specified page
 *
 * !!! EREPORT(ERROR) IS DISALLOWED HERE !!!  Must PANIC on failure!!!
 *
 * Note - caller must hold BUFFER_LOCK_EXCLUSIVE on the buffer.
 */
void
RelationPutHeapTuple(Relation relation, Buffer buffer, HeapTuple tuple, bool token)
{


































}

/*
 * Read in a buffer in mode, using bulk-insert strategy if bistate isn't NULL.
 */
static Buffer
ReadBufferBI(Relation relation, BlockNumber targetBlock, ReadBufferMode mode, BulkInsertState bistate)
{



































}

/*
 * For each heap page which is all-visible, acquire a pin on the appropriate
 * visibility map page, if we haven't already got one.
 *
 * buffer2 may be InvalidBuffer, if only one buffer is involved.  buffer1
 * must not be InvalidBuffer.  If both buffers are specified, block1 must
 * be less than block2.
 */
static void
GetVisibilityMapPins(Relation relation, Buffer buffer1, Buffer buffer2, BlockNumber block1, BlockNumber block2, Buffer *vmbuffer1, Buffer *vmbuffer2)
{



















































}

/*
 * Extend a relation by multiple blocks to avoid future contention on the
 * relation extension lock.  Our goal is to pre-extend the relation by an
 * amount which ramps up as the degree of contention ramps up, but limiting
 * the result to some sane overall value.
 */
static void
RelationAddExtraBlocks(Relation relation, BulkInsertState bistate)
{











































































}

/*
 * RelationGetBufferForTuple
 *
 *	Returns pinned and exclusive-locked buffer of a page in given relation
 *	with free space >= given len.
 *
 *	If otherBuffer is not InvalidBuffer, then it references a previously
 *	pinned buffer of another page in the same relation; on return, this
 *	buffer will also be exclusive-locked.  (This case is used by
 *heap_update; the otherBuffer contains the tuple being updated.)
 *
 *	The reason for passing otherBuffer is that if two backends are doing
 *	concurrent heap_update operations, a deadlock could occur if they try
 *	to lock the same two buffers in opposite orders.  To ensure that this
 *	can't happen, we impose the rule that buffers of a relation must be
 *	locked in increasing page number order.  This is most conveniently done
 *	by having RelationGetBufferForTuple lock them both, with suitable care
 *	for ordering.
 *
 *	NOTE: it is unlikely, but not quite impossible, for otherBuffer to be
 *the same buffer we select for insertion of the new tuple (this could only
 *	happen if space is freed in that page after heap_update finds there's
 *not enough there).  In that case, the page will be pinned and locked only
 *once.
 *
 *	We also handle the possibility that the all-visible flag will need to be
 *	cleared on one or both pages.  If so, pin on the associated visibility
 *map page must be acquired before acquiring buffer lock(s), to avoid possibly
 *	doing I/O while holding buffer locks.  The pins are passed back to the
 *	caller using the input-output arguments vmbuffer and vmbuffer_other.
 *	Note that in some cases the caller might have already acquired such
 *pins, which is indicated by these arguments not being InvalidBuffer on entry.
 *
 *	We normally use FSM to help us find free space.  However,
 *	if HEAP_INSERT_SKIP_FSM is specified, we just append a new empty page to
 *	the end of the relation if the tuple won't fit on the current target
 *page. This can save some cycles when we know the relation is new and doesn't
 *	contain useful amounts of free space.
 *
 *	HEAP_INSERT_SKIP_FSM is also useful for non-WAL-logged additions to a
 *	relation, if the caller holds exclusive lock and is careful to
 *invalidate relation's smgr_targblock before the first insertion --- that
 *ensures that all insertions will occur into newly added pages and not be
 *intermixed with tuples from other transactions.  That way, a crash can't risk
 *losing any committed data of other transactions.  (See heap_insert's comments
 *	for additional constraints needed for safe usage of this behavior.)
 *
 *	The caller can also provide a BulkInsertState object to optimize many
 *	insertions into the same relation.  This keeps a pin on the current
 *	insertion target page (to save pin/unpin cycles) and also passes a
 *	BULKWRITE buffer selection strategy object to the buffer manager.
 *	Passing NULL for bistate selects the default behavior.
 *
 *	We always try to avoid filling existing pages further than the
 *fillfactor. This is OK since this routine is not consulted when updating a
 *tuple and keeping it on the same page, which is the scenario fillfactor is
 *meant to reserve space for.
 *
 *	ereport(ERROR) is allowed here, so this routine *must* be called
 *	before any (unlogged) changes are made in buffer pool.
 */
Buffer
RelationGetBufferForTuple(Relation relation, Size len, Buffer otherBuffer, int options, BulkInsertState bistate, Buffer *vmbuffer, Buffer *vmbuffer_other)
{



























































































































































































































































































































































































}