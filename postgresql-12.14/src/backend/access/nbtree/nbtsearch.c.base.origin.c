/*-------------------------------------------------------------------------
 *
 * nbtsearch.c
 *	  Search code for postgres btrees.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtsearch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/nbtree.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/predicate.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

static void
_bt_drop_lock_and_maybe_pin(IndexScanDesc scan, BTScanPos sp);
static OffsetNumber
_bt_binsrch(Relation rel, BTScanInsert key, Buffer buf);
static bool
_bt_readpage(IndexScanDesc scan, ScanDirection dir, OffsetNumber offnum);
static void
_bt_saveitem(BTScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup);
static bool
_bt_steppage(IndexScanDesc scan, ScanDirection dir);
static bool
_bt_readnextpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir);
static bool
_bt_parallel_readpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir);
static Buffer
_bt_walk_left(Relation rel, Buffer buf, Snapshot snapshot);
static bool
_bt_endpoint(IndexScanDesc scan, ScanDirection dir);
static inline void
_bt_initialize_more_data(BTScanOpaque so, ScanDirection dir);

/*
 *	_bt_drop_lock_and_maybe_pin()
 *
 * Unlock the buffer; and if it is safe to release the pin, do that, too.  It
 * is safe if the scan is using an MVCC snapshot and the index is WAL-logged.
 * This will prevent vacuum from stalling in a blocked state trying to read a
 * page when a cursor is sitting on it -- at least in many important cases.
 *
 * Set the buffer to invalid if the pin is released, since the buffer may be
 * re-used.  If we need to go back to this block (for example, to apply
 * LP_DEAD hints) we must get a fresh reference to the buffer.  Hopefully it
 * will remain in shared memory for as long as it takes to scan the index
 * buffer page.
 */
static void
_bt_drop_lock_and_maybe_pin(IndexScanDesc scan, BTScanPos sp)
{







}

/*
 *	_bt_search() -- Search the tree for a particular scankey,
 *		or more precisely for the first leaf page it could be on.
 *
 * The passed scankey is an insertion-type scankey (see nbtree/README),
 * but it can omit the rightmost column(s) of the index.
 *
 * Return value is a stack of parent-page pointers.  *bufP is set to the
 * address of the leaf-page buffer, which is read-locked and pinned.
 * No locks are held on the parent pages, however!
 *
 * If the snapshot parameter is not NULL, "old snapshot" checking will take
 * place during the descent through the tree.  This is not needed when
 * positioning for an insert or delete, so NULL is used for those cases.
 *
 * The returned buffer is locked according to access parameter.  Additionally,
 * access = BT_WRITE will allow an empty root page to be created and returned.
 * When access = BT_READ, an empty index will result in *bufP being set to
 * InvalidBuffer.  Also, in BT_WRITE mode, any incomplete splits encountered
 * during the search will be finished.
 */
BTStack
_bt_search(Relation rel, BTScanInsert key, Buffer *bufP, int access, Snapshot snapshot)
{



















































































































}

/*
 *	_bt_moveright() -- move right in the btree if necessary.
 *
 * When we follow a pointer to reach a page, it is possible that
 * the page has changed in the meanwhile.  If this happens, we're
 * guaranteed that the page has "split right" -- that is, that any
 * data that appeared on the page originally is either on the page
 * or strictly to the right of it.
 *
 * This routine decides whether or not we need to move right in the
 * tree by examining the high key entry on the page.  If that entry is
 * strictly less than the scankey, or <= the scankey in the
 * key.nextkey=true case, then we followed the wrong link and we need
 * to move right.
 *
 * The passed insertion-type scankey can omit the rightmost column(s) of the
 * index. (see nbtree/README)
 *
 * When key.nextkey is false (the usual case), we are looking for the first
 * item >= key.  When key.nextkey is true, we are looking for the first item
 * strictly greater than key.
 *
 * If forupdate is true, we will attempt to finish any incomplete splits
 * that we encounter.  This is required when locking a target page for an
 * insertion, because we don't allow inserting on a page before the split
 * is completed.  'stack' is only used if forupdate is true.
 *
 * On entry, we have the buffer pinned and a lock of the type specified by
 * 'access'.  If we move right, we release the buffer and lock and acquire
 * the same on the right sibling.  Return value is the buffer we stop at.
 *
 * If the snapshot parameter is not NULL, "old snapshot" checking will take
 * place during the descent through the tree.  This is not needed when
 * positioning for an insert or delete, so NULL is used for those cases.
 */
Buffer
_bt_moveright(Relation rel, BTScanInsert key, Buffer buf, bool forupdate, BTStack stack, int access, Snapshot snapshot)
{
















































































}

/*
 *	_bt_binsrch() -- Do a binary search for a key on a particular page.
 *
 * On a leaf page, _bt_binsrch() returns the OffsetNumber of the first
 * key >= given scankey, or > scankey if nextkey is true.  (NOTE: in
 * particular, this means it is possible to return a value 1 greater than the
 * number of keys on the page, if the scankey is > all keys on the page.)
 *
 * On an internal (non-leaf) page, _bt_binsrch() returns the OffsetNumber
 * of the last key < given scankey, or last key <= given scankey if nextkey
 * is true.  (Since _bt_compare treats the first data key of such a page as
 * minus infinity, there will be at least one key < scankey, so the result
 * always points at one of the keys on the page.)  This key indicates the
 * right place to descend to be sure we find all leaf keys >= given scankey
 * (or leaf keys > given scankey when nextkey is true).
 *
 * This procedure is not responsible for walking right, it just examines
 * the given page.  _bt_binsrch() has no lock or refcount side effects
 * on the buffer.
 */
static OffsetNumber
_bt_binsrch(Relation rel, BTScanInsert key, Buffer buf)
{















































































}

/*
 *
 *	_bt_binsrch_insert() -- Cacheable, incremental leaf page binary search.
 *
 * Like _bt_binsrch(), but with support for caching the binary search
 * bounds.  Only used during insertion, and only on the leaf page that it
 * looks like caller will insert tuple on.  Exclusive-locked and pinned
 * leaf page is contained within insertstate.
 *
 * Caches the bounds fields in insertstate so that a subsequent call can
 * reuse the low and strict high bounds of original binary search.  Callers
 * that use these fields directly must be prepared for the case where low
 * and/or stricthigh are not on the same page (one or both exceed maxoff
 * for the page).  The case where there are no items on the page (high <
 * low) makes bounds invalid.
 *
 * Caller is responsible for invalidating bounds when it modifies the page
 * before calling here a second time.
 */
OffsetNumber
_bt_binsrch_insert(Relation rel, BTInsertState insertstate)
{


























































































}

/*----------
 *	_bt_compare() -- Compare insertion-type scankey to tuple on a page.
 *
 *	page/offnum: location of btree item to be compared to.
 *
 *		This routine returns:
 *			<0 if scankey < tuple at offnum;
 *			 0 if scankey == tuple at offnum;
 *			>0 if scankey > tuple at offnum.
 *		NULLs in the keys are treated as sortable values.  Therefore
 *		"equality" does not necessarily mean that the item should be
 *		returned to the caller as a matching key!
 *
 * CRUCIAL NOTE: on a non-leaf page, the first data key is assumed to be
 * "minus infinity": this routine will always claim it is less than the
 * scankey.  The actual key value stored is explicitly truncated to 0
 * attributes (explicitly minus infinity) with version 3+ indexes, but
 * that isn't relied upon.  This allows us to implement the Lehman and
 * Yao convention that the first down-link pointer is before the first
 * key.  See backend/access/nbtree/README for details.
 *----------
 */
int32
_bt_compare(Relation rel, BTScanInsert key, Page page, OffsetNumber offnum)
{

















































































































































































}

/*
 *	_bt_first() -- Find the first item in a scan.
 *
 *		We need to be clever about the direction of scan, the search
 *		conditions, and the tree ordering.  We find the first item (or,
 *		if backwards scan, the last item) in the tree that satisfies the
 *		qualifications in the scan key.  On success exit, the page
 *containing the current index tuple is pinned but not locked, and data about
 *		the matching tuple(s) on the page has been loaded into
 *so->currPos. scan->xs_ctup.t_self is set to the heap TID of the current tuple,
 *		and if requested, scan->xs_itup points to a copy of the index
 *tuple.
 *
 * If there are no matching items in the index, we return false, with no
 * pins or locks held.
 *
 * Note that scan->keyData[], and the so->keyData[] scankey built from it,
 * are both search-type scankeys (see nbtree/README for more about this).
 * Within this routine, we build a temporary insertion-type scankey to use
 * in locating the scan start position.
 */
bool
_bt_first(IndexScanDesc scan, ScanDirection dir)
{







































































































































































































































































































































































































































































































































































































}

/*
 *	_bt_next() -- Get the next item in a scan.
 *
 *		On entry, so->currPos describes the current page, which may be
 *pinned but is not locked, and so->currPos.itemIndex identifies which item was
 *		previously returned.
 *
 *		On successful exit, scan->xs_ctup.t_self is set to the TID of
 *the next heap tuple, and if requested, scan->xs_itup points to a copy of the
 *index tuple.  so->currPos is updated as needed.
 *
 *		On failure exit (no more tuples), we release pin and set
 *		so->currPos.buf to InvalidBuffer.
 */
bool
_bt_next(IndexScanDesc scan, ScanDirection dir)
{





































}

/*
 *	_bt_readpage() -- Load data from current index page into so->currPos
 *
 * Caller must have pinned and read-locked so->currPos.buf; the buffer's state
 * is not changed here.  Also, currPos.moreLeft and moreRight must be valid;
 * they are updated as appropriate.  All other fields of so->currPos are
 * initialized from scratch here.
 *
 * We scan the current page starting at offnum and moving in the indicated
 * direction.  All items matching the scan keys are loaded into currPos.items.
 * moreLeft or moreRight (as appropriate) is cleared if _bt_checkkeys reports
 * that there can be no more matching tuples in the current scan direction.
 *
 * In the case of a parallel scan, caller must have called _bt_parallel_seize
 * prior to calling this function; this function will invoke
 * _bt_parallel_release before returning.
 *
 * Returns true if any matching items found on the page, false if none.
 */
static bool
_bt_readpage(IndexScanDesc scan, ScanDirection dir, OffsetNumber offnum)
{









































































































































































































}

/* Save an index item into so->currPos.items[itemIndex] */
static void
_bt_saveitem(BTScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup)
{












}

/*
 *	_bt_steppage() -- Step to next page containing valid data for scan
 *
 * On entry, if so->currPos.buf is valid the buffer is pinned but not locked;
 * if pinned, we'll drop the pin before moving to next page.  The buffer is
 * not locked on entry.
 *
 * For success on a scan using a non-MVCC snapshot we hold a pin, but not a
 * read lock, on that page.  If we do not hold the pin, we set so->currPos.buf
 * to InvalidBuffer.  We return true to indicate success.
 */
static bool
_bt_steppage(IndexScanDesc scan, ScanDirection dir)
{

































































































}

/*
 *	_bt_readnextpage() -- Read next page containing valid data for scan
 *
 * On success exit, so->currPos is updated to contain data from the next
 * interesting page.  Caller is responsible to release lock and pin on
 * buffer on success.  We return true to indicate success.
 *
 * If there are no more matching records in the given direction, we drop all
 * locks and pins, set so->currPos.buf to InvalidBuffer, and return false.
 */
static bool
_bt_readnextpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir)
{














































































































































































}

/*
 *	_bt_parallel_readpage() -- Read current page containing valid data for
 *scan
 *
 * On success, release lock and maybe pin on buffer.  We return true to
 * indicate success.
 */
static bool
_bt_parallel_readpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir)
{













}

/*
 * _bt_walk_left() -- step left one page, if possible
 *
 * The given buffer must be pinned and read-locked.  This will be dropped
 * before stepping left.  On return, we have pin and read lock on the
 * returned page, instead.
 *
 * Returns InvalidBuffer if there is no page to the left (no lock is held
 * in that case).
 *
 * When working on a non-leaf level, it is possible for the returned page
 * to be half-dead; the caller should check that condition and step left
 * again if it's important.
 */
static Buffer
_bt_walk_left(Relation rel, Buffer buf, Snapshot snapshot)
{
















































































































}

/*
 * _bt_get_endpoint() -- Find the first or last page on a given tree level
 *
 * If the index is empty, we will return InvalidBuffer; any other failure
 * condition causes ereport().  We will not return a dead page.
 *
 * The returned buffer is pinned and read-locked.
 */
Buffer
_bt_get_endpoint(Relation rel, uint32 level, bool rightmost, Snapshot snapshot)
{
















































































}

/*
 *	_bt_endpoint() -- Find the first or last page in the index, and scan
 * from there to the first key satisfying all the quals.
 *
 * This is used by _bt_first() to set up a scan when we've determined
 * that the scan must start at the beginning or end of the index (for
 * a forward or backward scan respectively).  Exit conditions are the
 * same as for _bt_first().
 */
static bool
_bt_endpoint(IndexScanDesc scan, ScanDirection dir)
{





















































































}

/*
 * _bt_initialize_more_data() -- initialize moreLeft/moreRight appropriately
 * for scan direction
 */
static inline void
_bt_initialize_more_data(BTScanOpaque so, ScanDirection dir)
{













}