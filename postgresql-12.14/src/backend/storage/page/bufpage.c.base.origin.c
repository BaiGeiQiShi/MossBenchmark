/*-------------------------------------------------------------------------
 *
 * bufpage.c
 *	  POSTGRES standard buffer page code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/page/bufpage.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/itup.h"
#include "access/xlog.h"
#include "pgstat.h"
#include "storage/checksum.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

/* GUC variable */
bool ignore_checksum_failure = false;

/* ----------------------------------------------------------------
 *						Page support functions
 * ----------------------------------------------------------------
 */

/*
 * PageInit
 *		Initializes the contents of a page.
 *		Note that we don't calculate an initial checksum here; that's
 *not done until it's time to write.
 */
void
PageInit(Page page, Size pageSize, Size specialSize)
{
















}

/*
 * PageIsVerified
 *		Utility wrapper for PageIsVerifiedExtended().
 */
bool
PageIsVerified(Page page, BlockNumber blkno)
{

}

/*
 * PageIsVerifiedExtended
 *		Check that the page header and checksum (if any) appear valid.
 *
 * This is called when a page has just been read in from disk.  The idea is
 * to cheaply detect trashed pages before we go nuts following bogus line
 * pointers, testing invalid transaction identifiers, etc.
 *
 * It turns out to be necessary to allow zeroed pages here too.  Even though
 * this routine is *not* called when deliberately adding a page to a relation,
 * there are scenarios in which a zeroed page might be found in a table.
 * (Example: a backend extends a relation, then crashes before it can write
 * any WAL entry about the new page.  The kernel will already have the
 * zeroed page in the file, and it will stay that way after restart.)  So we
 * allow zeroed pages here, and are careful that the page access macros
 * treat such a page as empty and without free space.  Eventually, VACUUM
 * will clean up such a page and make it usable.
 *
 * If flag PIV_LOG_WARNING is set, a WARNING is logged in the event of
 * a checksum failure.
 *
 * If flag PIV_REPORT_STAT is set, a checksum failure is reported directly
 * to pgstat.
 */
bool
PageIsVerifiedExtended(Page page, BlockNumber blkno, int flags)
{






















































































}

/*
 *	PageAddItemExtended
 *
 *	Add an item to a page.  Return value is the offset at which it was
 *	inserted, or InvalidOffsetNumber if the item is not inserted for any
 *	reason.  A WARNING is issued indicating the reason for the refusal.
 *
 *	offsetNumber must be either InvalidOffsetNumber to specify finding a
 *	free line pointer, or a value between FirstOffsetNumber and one past
 *	the last existing item, to specify using that particular line pointer.
 *
 *	If offsetNumber is valid and flag PAI_OVERWRITE is set, we just store
 *	the item at the specified offsetNumber, which must be either a
 *	currently-unused line pointer, or one past the last existing item.
 *
 *	If offsetNumber is valid and flag PAI_OVERWRITE is not set, insert
 *	the item at the specified offsetNumber, moving existing items later
 *	in the array to make room.
 *
 *	If offsetNumber is not valid, then assign a slot by finding the first
 *	one that is both unused and deallocated.
 *
 *	If flag PAI_IS_HEAP is set, we enforce that there can't be more than
 *	MaxHeapTuplesPerPage line pointers on the page.
 *
 *	!!! EREPORT(ERROR) IS DISALLOWED HERE !!!
 */
OffsetNumber
PageAddItemExtended(Page page, Item item, Size size, OffsetNumber offsetNumber, int flags)
{






















































































































































}

/*
 * PageGetTempPage
 *		Get a temporary page in local memory for special processing.
 *		The returned page is not initialized at all; caller must do
 *that.
 */
Page
PageGetTempPage(Page page)
{







}

/*
 * PageGetTempPageCopy
 *		Get a temporary page in local memory for special processing.
 *		The page is initialized by copying the contents of the given
 *page.
 */
Page
PageGetTempPageCopy(Page page)
{









}

/*
 * PageGetTempPageCopySpecial
 *		Get a temporary page in local memory for special processing.
 *		The page is PageInit'd with the same special-space size as the
 *		given page, and the special space is copied from the given page.
 */
Page
PageGetTempPageCopySpecial(Page page)
{










}

/*
 * PageRestoreTempPage
 *		Copy temporary page back to permanent page after special
 *processing and release the temporary page.
 */
void
PageRestoreTempPage(Page tempPage, Page oldPage)
{






}

/*
 * sorting support for PageRepairFragmentation and PageIndexMultiDelete
 */
typedef struct itemIdSortData
{
  uint16 offsetindex; /* linp array index */
  int16 itemoff;      /* page offset of item data */
  uint16 alignedlen;  /* MAXALIGN(item data len) */
} itemIdSortData;
typedef itemIdSortData *itemIdSort;

static int
itemoffcompare(const void *itemidp1, const void *itemidp2)
{


}

/*
 * After removing or marking some line pointers unused, move the tuples to
 * remove the gaps caused by the removed items.
 */
static void
compactify_tuples(itemIdSort itemidbase, int nitems, Page page)
{




















}

/*
 * PageRepairFragmentation
 *
 * Frees fragmented space on a page.
 * It doesn't remove unused line pointers! Please don't change this.
 *
 * This routine is usable for heap pages only, but see PageIndexMultiDelete.
 *
 * As a side effect, the page's PD_HAS_FREE_LINES hint bit is updated.
 */
void
PageRepairFragmentation(Page page)
{
















































































}

/*
 * PageGetFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		reduced by the space needed for a new line pointer.
 *
 * Note: this should usually only be used on index pages.  Use
 * PageGetHeapFreeSpace on heap pages.
 */
Size
PageGetFreeSpace(Page page)
{















}

/*
 * PageGetFreeSpaceForMultipleTuples
 *		Returns the size of the free (allocatable) space on a page,
 *		reduced by the space needed for multiple new line pointers.
 *
 * Note: this should usually only be used on index pages.  Use
 * PageGetHeapFreeSpace on heap pages.
 */
Size
PageGetFreeSpaceForMultipleTuples(Page page, int ntups)
{















}

/*
 * PageGetExactFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		without any consideration for adding/removing line pointers.
 */
Size
PageGetExactFreeSpace(Page page)
{














}

/*
 * PageGetHeapFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		reduced by the space needed for a new line pointer.
 *
 * The difference between this and PageGetFreeSpace is that this will return
 * zero if there are already MaxHeapTuplesPerPage line pointers in the page
 * and none are free.  We use this to enforce that no more than
 * MaxHeapTuplesPerPage line pointers are created on a heap page.  (Although
 * no more tuples than that could fit anyway, in the presence of redirected
 * or dead line pointers it'd be possible to have too many line pointers.
 * To avoid breaking code that assumes MaxHeapTuplesPerPage is a hard limit
 * on the number of line pointers, we make this extra check.)
 */
Size
PageGetHeapFreeSpace(Page page)
{

















































}

/*
 * PageIndexTupleDelete
 *
 * This routine does the work of removing a tuple from an index page.
 *
 * Unlike heap pages, we compact out the line pointer for the removed tuple.
 */
void
PageIndexTupleDelete(Page page, OffsetNumber offnum)
{





























































































}

/*
 * PageIndexMultiDelete
 *
 * This routine handles the case of deleting multiple tuples from an
 * index page at once.  It is considerably faster than a loop around
 * PageIndexTupleDelete ... however, the caller *must* supply the array
 * of item numbers to be deleted in item number order!
 */
void
PageIndexMultiDelete(Page page, OffsetNumber *itemnos, int nitems)
{




































































































}

/*
 * PageIndexTupleDeleteNoCompact
 *
 * Remove the specified tuple from an index page, but set its line pointer
 * to "unused" instead of compacting it out, except that it can be removed
 * if it's the last line pointer on the page.
 *
 * This is used for index AMs that require that existing TIDs of live tuples
 * remain unchanged, and are willing to allow unused line pointers instead.
 */
void
PageIndexTupleDeleteNoCompact(Page page, OffsetNumber offnum)
{























































































}

/*
 * PageIndexTupleOverwrite
 *
 * Replace a specified tuple on an index page.
 *
 * The new tuple is placed exactly where the old one had been, shifting
 * other tuples' data up or down as needed to keep the page compacted.
 * This is better than deleting and reinserting the tuple, because it
 * avoids any data shifting when the tuple size doesn't change; and
 * even when it does, we avoid moving the line pointers around.
 * Conceivably this could also be of use to an index AM that cares about
 * the physical order of tuples as well as their ItemId order.
 *
 * If there's insufficient space for the new tuple, return false.  Other
 * errors represent data-corruption problems, so we just elog.
 */
bool
PageIndexTupleOverwrite(Page page, OffsetNumber offnum, Item newtup, Size newsize)
{


















































































}

/*
 * Set checksum for a page in shared buffers.
 *
 * If checksums are disabled, or if the page is not initialized, just return
 * the input.  Otherwise, we must make a copy of the page before calculating
 * the checksum, to prevent concurrent modifications (e.g. setting hint bits)
 * from making the final checksum invalid.  It doesn't matter if we include or
 * exclude hints during the copy, as long as we write a valid page and
 * associated checksum.
 *
 * Returns a pointer to the block-sized data that needs to be written. Uses
 * statically-allocated memory, so the caller must immediately write the
 * returned page and not refer to it again.
 */
char *
PageSetChecksumCopy(Page page, BlockNumber blkno)
{






















}

/*
 * Set checksum for a page in private memory.
 *
 * This must only be used when we know that no other process can be modifying
 * the page buffer.
 */
void
PageSetChecksumInplace(Page page, BlockNumber blkno)
{







}