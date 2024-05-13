/*-------------------------------------------------------------------------
 *
 * gistget.c
 *	  fetch tuples from a GiST scan.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/gist/gistget.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/gist_private.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "pgstat.h"
#include "lib/pairingheap.h"
#include "utils/float.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * gistkillitems() -- set LP_DEAD state for items an indexscan caller has
 * told us were killed.
 *
 * We re-read page here, so it's important to check page LSN. If the page
 * has been modified since the last read (as determined by LSN), we cannot
 * flag any entries because it is possible that the old entry was vacuumed
 * away and the TID was re-used by a completely different heap tuple.
 */
static void
gistkillitems(IndexScanDesc scan)
{





























































}

/*
 * gistindex_keytest() -- does this index tuple satisfy the scan key(s)?
 *
 * The index tuple might represent either a heap tuple or a lower index page,
 * depending on whether the containing page is a leaf page or not.
 *
 * On success return for a heap tuple, *recheck_p is set to indicate whether
 * the quals need to be rechecked.  We recheck if any of the consistent()
 * functions request it.  recheck is not interesting when examining a non-leaf
 * entry, since we must visit the lower index page if there's any doubt.
 * Similarly, *recheck_distances_p is set to indicate whether the distances
 * need to be rechecked, and it is also ignored for non-leaf entries.
 *
 * If we are doing an ordered scan, so->distances[] is filled with distance
 * data from the distance() functions before returning success.
 *
 * We must decompress the key in the IndexTuple before passing it to the
 * sk_funcs (which actually are the opclass Consistent or Distance methods).
 *
 * Note that this function is always invoked in a short-lived memory context,
 * so we don't need to worry about cleaning up allocated memory, either here
 * or in the implementation of any Consistent or Distance methods.
 */
static bool
gistindex_keytest(IndexScanDesc scan, IndexTuple tuple, Page page, OffsetNumber offset, bool *recheck_p, bool *recheck_distances_p)
{





























































































































































}

/*
 * Scan all items on the GiST index page identified by *pageItem, and insert
 * them into the queue (or directly to output areas)
 *
 * scan: index scan we are executing
 * pageItem: search queue item identifying an index page to scan
 * myDistances: distances array associated with pageItem, or NULL at the root
 * tbm: if not NULL, gistgetbitmap's output bitmap
 * ntids: if not NULL, gistgetbitmap's output tuple counter
 *
 * If tbm/ntids aren't NULL, we are doing an amgetbitmap scan, and heap
 * tuples should be reported directly into the bitmap.  If they are NULL,
 * we're doing a plain or ordered indexscan.  For a plain indexscan, heap
 * tuple TIDs are returned into so->pageData[].  For an ordered indexscan,
 * heap tuple TIDs are pushed into individual search queue items.  In an
 * index-only scan, reconstructed index tuples are returned along with the
 * TIDs.
 *
 * If we detect that the index page has split since we saw its downlink
 * in the parent, we push its new right sibling onto the queue so the
 * sibling will be processed next.
 */
static void
gistScanPage(IndexScanDesc scan, GISTSearchItem *pageItem, IndexOrderByDistance *myDistances, TIDBitmap *tbm, int64 *ntids)
{









































































































































































































}

/*
 * Extract next item (in order) from search queue
 *
 * Returns a GISTSearchItem or NULL.  Caller must pfree item when done with it.
 */
static GISTSearchItem *
getNextGISTSearchItem(GISTScanOpaque so)
{














}

/*
 * Fetch next heap tuple in an ordered search
 */
static bool
getNextNearest(IndexScanDesc scan)
{














































}

/*
 * gistgettuple() -- Get the next tuple in the scan
 */
bool
gistgettuple(IndexScanDesc scan, ScanDirection dir)
{

































































































































}

/*
 * gistgetbitmap() -- Get a bitmap of all heap tuple locations
 */
int64
gistgetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{












































}

/*
 * Can we do index-only scans on the given index column?
 *
 * Opclasses that implement a fetch function support index-only scans.
 * Opclasses without compression functions also support index-only scans.
 * Included attributes always can be fetched for index-only scans.
 */
bool
gistcanreturn(Relation index, int attno)
{








}