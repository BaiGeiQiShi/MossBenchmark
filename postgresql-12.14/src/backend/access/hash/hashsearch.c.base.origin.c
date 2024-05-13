/*-------------------------------------------------------------------------
 *
 * hashsearch.c
 *	  search code for postgres hash tables
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashsearch.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/rel.h"
#include "storage/predicate.h"

static bool
_hash_readpage(IndexScanDesc scan, Buffer *bufP, ScanDirection dir);
static int
_hash_load_qualified_items(IndexScanDesc scan, Page page, OffsetNumber offnum, ScanDirection dir);
static inline void
_hash_saveitem(HashScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup);
static void
_hash_readnext(IndexScanDesc scan, Buffer *bufp, Page *pagep, HashPageOpaque *opaquep);

/*
 *	_hash_next() -- Get the next item in a scan.
 *
 *		On entry, so->currPos describes the current page, which may
 *		be pinned but not locked, and so->currPos.itemIndex identifies
 *		which item was previously returned.
 *
 *		On successful exit, scan->xs_ctup.t_self is set to the TID
 *		of the next heap tuple. so->currPos is updated as needed.
 *
 *		On failure exit (no more tuples), we return false with pin
 *		held on bucket page but no pins or locks held on overflow
 *		page.
 */
bool
_hash_next(IndexScanDesc scan, ScanDirection dir)
{























































































}

/*
 * Advance to next page in a bucket, if any.  If we are scanning the bucket
 * being populated during split operation then this function advances to the
 * bucket being split after the last bucket page of bucket being populated.
 */
static void
_hash_readnext(IndexScanDesc scan, Buffer *bufp, Page *pagep, HashPageOpaque *opaquep)
{




























































}

/*
 * Advance to previous page in a bucket, if any.  If the current scan has
 * started during split operation then this function advances to bucket
 * being populated after the first bucket page of bucket being split.
 */
static void
_hash_readprev(IndexScanDesc scan, Buffer *bufp, Page *pagep, HashPageOpaque *opaquep)
{









































































}

/*
 *	_hash_first() -- Find the first item in a scan.
 *
 *		We find the first item (or, if backward scan, the last item) in
 *the index that satisfies the qualification associated with the scan
 *		descriptor.
 *
 *		On successful exit, if the page containing current index tuple
 *is an overflow page, both pin and lock are released whereas if it is a bucket
 *		page then it is pinned but not locked and data about the
 *matching tuple(s) on the page has been loaded into so->currPos,
 *		scan->xs_ctup.t_self is set to the heap TID of the current
 *tuple.
 *
 *		On failure exit (no more tuples), we return false, with pin held
 *on bucket page but no pins or locks held on overflow page.
 */
bool
_hash_first(IndexScanDesc scan, ScanDirection dir)
{


























































































































































}

/*
 *	_hash_readpage() -- Load data from current index page into so->currPos
 *
 *	We scan all the items in the current index page and save them into
 *	so->currPos if it satisfies the qualification. If no matching items
 *	are found in the current page, we move to the next or previous page
 *	in a bucket chain as indicated by the direction.
 *
 *	Return true if any matching items are found else return false.
 */
static bool
_hash_readpage(IndexScanDesc scan, Buffer *bufP, ScanDirection dir)
{






























































































































































}

/*
 * Load all the qualified items from a current index page
 * into so->currPos. Helper function for _hash_readpage.
 */
static int
_hash_load_qualified_items(IndexScanDesc scan, Page page, OffsetNumber offnum, ScanDirection dir)
{



























































































}

/* Save an index item into so->currPos.items[itemIndex] */
static inline void
_hash_saveitem(HashScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup)
{




}