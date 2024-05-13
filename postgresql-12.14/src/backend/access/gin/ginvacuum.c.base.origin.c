/*-------------------------------------------------------------------------
 *
 * ginvacuum.c
 *	  delete & vacuum routines for the postgres GIN
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginvacuum.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "postmaster/autovacuum.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/memutils.h"

struct GinVacuumState
{
  Relation index;
  IndexBulkDeleteResult *result;
  IndexBulkDeleteCallback callback;
  void *callback_state;
  GinState ginstate;
  BufferAccessStrategy strategy;
  MemoryContext tmpCxt;
};

/*
 * Vacuums an uncompressed posting list. The size of the must can be specified
 * in number of items (nitems).
 *
 * If none of the items need to be removed, returns NULL. Otherwise returns
 * a new palloc'd array with the remaining items. The number of remaining
 * items is returned in *nremaining.
 */
ItemPointer
ginVacuumItemPointers(GinVacuumState *gvs, ItemPointerData *items, int nitem, int *nremaining)
{


































}

/*
 * Create a WAL record for vacuuming entry tree leaf page.
 */
static void
xlogVacuumPage(Relation index, Buffer buffer)
{





















}

typedef struct DataPageDeleteStack
{
  struct DataPageDeleteStack *child;
  struct DataPageDeleteStack *parent;

  BlockNumber blkno; /* current block number */
  Buffer leftBuffer; /* pinned and locked rightest non-deleted page
                      * on left */
  bool isRoot;
} DataPageDeleteStack;

/*
 * Delete a posting tree page.
 */
static void
ginDeletePage(GinVacuumState *gvs, BlockNumber deleteBlkno, BlockNumber leftBlkno, BlockNumber parentBlkno, OffsetNumber myoff, bool isParentRoot)
{


































































































}

/*
 * Scans posting tree and deletes empty pages.  Caller must lock root page for
 * cleanup.  During scan path from root to current page is kept exclusively
 * locked.  Also keep left page exclusively locked, because ginDeletePage()
 * needs it.  If we try to relock left page later, it could deadlock with
 * ginStepRight().
 */
static bool
ginScanToDelete(GinVacuumState *gvs, BlockNumber blkno, bool isRoot, DataPageDeleteStack *parent, OffsetNumber myoff)
{






































































































}

/*
 * Scan through posting tree leafs, delete empty tuples.  Returns true if there
 * is at least one empty page.
 */
static bool
ginVacuumPostingTreeLeaves(GinVacuumState *gvs, BlockNumber blkno)
{




























































}

static void
ginVacuumPostingTree(GinVacuumState *gvs, BlockNumber rootBlkno)
{


































}

/*
 * returns modified page or NULL if page isn't modified.
 * Function works with original page until first change is occurred,
 * then page is copied into temporary one.
 */
static Page
ginVacuumEntryPage(GinVacuumState *gvs, Buffer buffer, BlockNumber *roots, uint32 *nroot)
{









































































































}

IndexBulkDeleteResult *
ginbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{

















































































































}

IndexBulkDeleteResult *
ginvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{

















































































































}