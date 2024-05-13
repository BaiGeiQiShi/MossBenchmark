/*-------------------------------------------------------------------------
 *
 * gistvacuum.c
 *	  vacuuming routines for the postgres GiST index access method.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/gist/gistvacuum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/gist_private.h"
#include "access/transam.h"
#include "commands/vacuum.h"
#include "lib/integerset.h"
#include "miscadmin.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "utils/memutils.h"

/*
 * State kept across vacuum stages.
 */
typedef struct
{
  IndexBulkDeleteResult stats; /* must be first */

  /*
   * These are used to memorize all internal and empty leaf pages in the 1st
   * vacuum stage.  They are used in the 2nd stage, to delete all the empty
   * pages.
   */
  IntegerSet *internal_page_set;
  IntegerSet *empty_leaf_set;
  MemoryContext page_set_context;
} GistBulkDeleteResult;

/* Working state needed by gistbulkdelete */
typedef struct
{
  IndexVacuumInfo *info;
  GistBulkDeleteResult *stats;
  IndexBulkDeleteCallback callback;
  void *callback_state;
  GistNSN startNSN;
} GistVacState;

static void
gistvacuumscan(IndexVacuumInfo *info, GistBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state);
static void
gistvacuumpage(GistVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno);
static void
gistvacuum_delete_empty_pages(IndexVacuumInfo *info, GistBulkDeleteResult *stats);
static bool
gistdeletepage(IndexVacuumInfo *info, GistBulkDeleteResult *stats, Buffer buffer, OffsetNumber downlink, Buffer leafBuffer);

/* allocate the 'stats' struct that's kept over vacuum stages */
static GistBulkDeleteResult *
create_GistBulkDeleteResult(void)
{






}

/*
 * VACUUM bulkdelete stage: remove index entries.
 */
IndexBulkDeleteResult *
gistbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{











}

/*
 * VACUUM cleanup stage: delete empty pages, and update index statistics.
 */
IndexBulkDeleteResult *
gistvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{














































}

/*
 * gistvacuumscan --- scan the index for VACUUMing purposes
 *
 * This scans the index for leaf tuples that are deletable according to the
 * vacuum callback, and updates the stats.  Both btbulkdelete and
 * btvacuumcleanup invoke this (the latter only if no btbulkdelete call
 * occurred).
 *
 * This also makes note of any empty leaf pages, as well as all internal
 * pages.  The second stage, gistvacuum_delete_empty_pages(), needs that
 * information.  Any deleted pages are added directly to the free space map.
 * (They should've been added there when they were originally deleted, already,
 * but it's possible that the FSM was lost at a crash, for example.)
 *
 * The caller is responsible for initially allocating/zeroing a stats struct.
 */
static void
gistvacuumscan(IndexVacuumInfo *info, GistBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
















































































































}

/*
 * gistvacuumpage --- VACUUM one page
 *
 * This processes a single page for gistbulkdelete().  In some cases we
 * must go back and re-examine previously-scanned pages; this routine
 * recurses when necessary to handle that case.
 *
 * blkno is the page to process.  orig_blkno is the highest block number
 * reached by the outer gistvacuumscan loop (the same as blkno, unless we
 * are recursing to re-examine a previous page).
 */
static void
gistvacuumpage(GistVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno)
{




















































































































































































}

/*
 * Scan all internal pages, and try to delete their empty child pages.
 */
static void
gistvacuum_delete_empty_pages(IndexVacuumInfo *info, GistBulkDeleteResult *stats)
{













































































































}

/*
 * gistdeletepage takes a leaf page, and its parent, and tries to delete the
 * leaf.  Both pages must be locked.
 *
 * Even if the page was empty when we first saw it, a concurrent inserter might
 * have added a tuple to it since.  Similarly, the downlink might have moved.
 * We re-check all the conditions, to make sure the page is still deletable,
 * before modifying anything.
 *
 * Returns true, if the page was deleted, and false if a concurrent update
 * prevented it.
 */
static bool
gistdeletepage(IndexVacuumInfo *info, GistBulkDeleteResult *stats, Buffer parentBuffer, OffsetNumber downlink, Buffer leafBuffer)
{


























































































}