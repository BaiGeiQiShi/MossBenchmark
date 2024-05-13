/*
 * brin_pageops.c
 *		Page-handling routines for BRIN indexes
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_pageops.c
 */
#include "postgres.h"

#include "access/brin_pageops.h"
#include "access/brin_page.h"
#include "access/brin_revmap.h"
#include "access/brin_xlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/rel.h"

/*
 * Maximum size of an entry in a BRIN_PAGETYPE_REGULAR page.  We can tolerate
 * a single item per page, unlike other index AMs.
 */
#define BrinMaxItemSize MAXALIGN_DOWN(BLCKSZ - (MAXALIGN(SizeOfPageHeaderData + sizeof(ItemIdData)) + MAXALIGN(sizeof(BrinSpecialSpace))))

static Buffer
brin_getinsertbuffer(Relation irel, Buffer oldbuf, Size itemsz, bool *extended);
static Size
br_page_get_freespace(Page page);
static void
brin_initialize_empty_new_buffer(Relation idxrel, Buffer buffer);

/*
 * Update tuple origtup (size origsz), located in offset oldoff of buffer
 * oldbuf, to newtup (size newsz) as summary tuple for the page range starting
 * at heapBlk.  oldbuf must not be locked on entry, and is not locked at exit.
 *
 * If samepage is true, attempt to put the new tuple in the same page, but if
 * there's no room, use some other one.
 *
 * If the update is successful, return true; the revmap is updated to point to
 * the new tuple.  If the update is not done for whatever reason, return false.
 * Caller may retry the update if this happens.
 */
bool
brin_doupdate(Relation idxrel, BlockNumber pagesPerRange, BrinRevmap *revmap, BlockNumber heapBlk, Buffer oldbuf, OffsetNumber oldoff, const BrinTuple *origtup, Size origsz, const BrinTuple *newtup, Size newsz, bool samepage)
{
















































































































































































































































































}

/*
 * Return whether brin_doupdate can do a samepage update.
 */
bool
brin_can_do_samepage_update(Buffer buffer, Size origsz, Size newsz)
{

}

/*
 * Insert an index tuple into the index relation.  The revmap is updated to
 * mark the range containing the given page as pointing to the inserted entry.
 * A WAL record is written.
 *
 * The buffer, if valid, is first checked for free space to insert the new
 * entry; if there isn't enough, a new buffer is obtained and pinned.  No
 * buffer lock must be held on entry, no buffer lock is held on exit.
 *
 * Return value is the offset number where the tuple was inserted.
 */
OffsetNumber
brin_doinsert(Relation idxrel, BlockNumber pagesPerRange, BrinRevmap *revmap, Buffer *buffer, BlockNumber heapBlk, BrinTuple *tup, Size itemsz)
{





























































































































}

/*
 * Initialize a page with the given type.
 *
 * Caller is responsible for marking it dirty, as appropriate.
 */
void
brin_page_init(Page page, uint16 type)
{



}

/*
 * Initialize a new BRIN index's metapage.
 */
void
brin_metapage_init(Page page, BlockNumber pagesPerRange, uint16 version)
{























}

/*
 * Initiate page evacuation protocol.
 *
 * The page must be locked in exclusive mode by the caller.
 *
 * If the page is not yet initialized or empty, return false without doing
 * anything; it can be used for revmap without any further changes.  If it
 * contains tuples, mark it for evacuation and return true.
 */
bool
brin_start_evacuating_page(Relation idxRel, Buffer buf)
{
































}

/*
 * Move all tuples out of a page.
 *
 * The caller must hold lock on the page. The lock and pin are released.
 */
void
brin_evacuate_page(Relation idxRel, BlockNumber pagesPerRange, BrinRevmap *revmap, Buffer buf)
{












































}

/*
 * Given a BRIN index page, initialize it if necessary, and record its
 * current free space in the FSM.
 *
 * The main use for this is when, during vacuuming, an uninitialized page is
 * found, which could be the result of relation extension followed by a crash
 * before the page can be used.
 *
 * Here, we don't bother to update upper FSM pages, instead expecting that our
 * caller (brin_vacuum_scan) will fix them at the end of the scan.  Elsewhere
 * in this file, it's generally a good idea to propagate additions of free
 * space into the upper FSM pages immediately.
 */
void
brin_page_cleanup(Relation idxrel, Buffer buf)
{



































}

/*
 * Return a pinned and exclusively locked buffer which can be used to insert an
 * index item of size itemsz (caller must ensure not to request sizes
 * impossible to fulfill).  If oldbuf is a valid buffer, it is also locked (in
 * an order determined to avoid deadlocks).
 *
 * If we find that the old page is no longer a regular index page (because
 * of a revmap extension), the old buffer is unlocked and we return
 * InvalidBuffer.
 *
 * If there's no existing page with enough free space to accommodate the new
 * item, the relation is extended.  If this happens, *extended is set to true,
 * and it is the caller's responsibility to initialize the page (and WAL-log
 * that fact) prior to use.  The caller should also update the FSM with the
 * page's remaining free space after the insertion.
 *
 * Note that the caller is not expected to update FSM unless *extended is set
 * true.  This policy means that we'll update FSM when a page is created, and
 * when it's found to have too little space for a desired tuple insertion,
 * but not every single time we add a tuple to the page.
 *
 * Note that in some corner cases it is possible for this routine to extend
 * the relation and then not return the new page.  It is this routine's
 * responsibility to WAL-log the page initialization and to record the page in
 * FSM if that happens, since the caller certainly can't do it.
 */
static Buffer
brin_getinsertbuffer(Relation irel, Buffer oldbuf, Size itemsz, bool *extended)
{





















































































































































































}

/*
 * Initialize a page as an empty regular BRIN page, WAL-log this, and record
 * the page in FSM.
 *
 * There are several corner situations in which we extend the relation to
 * obtain a new page and later find that we cannot use it immediately.  When
 * that happens, we don't want to leave the page go unrecorded in FSM, because
 * there is no mechanism to get the space back and the index would bloat.
 * Also, because we would not WAL-log the action that would initialize the
 * page, the page would go uninitialized in a standby (or after recovery).
 *
 * While we record the page in FSM here, caller is responsible for doing FSM
 * upper-page update if that seems appropriate.
 */
static void
brin_initialize_empty_new_buffer(Relation idxrel, Buffer buffer)
{

















}

/*
 * Return the amount of free space on a regular BRIN index page.
 *
 * If the page is not a regular page, or has been marked with the
 * BRIN_EVACUATE_PAGE flag, returns 0.
 */
static Size
br_page_get_freespace(Page page)
{








}