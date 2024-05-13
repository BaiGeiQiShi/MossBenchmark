/*-------------------------------------------------------------------------
 *
 * ginbtree.c
 *	  page utilities routines for the postgres inverted index access method.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginbtree.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xloginsert.h"
#include "storage/predicate.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static void
ginFindParents(GinBtree btree, GinBtreeStack *stack);
static bool
ginPlaceToPage(GinBtree btree, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, Buffer childbuf, GinStatsData *buildStats);
static void
ginFinishSplit(GinBtree btree, GinBtreeStack *stack, bool freestack, GinStatsData *buildStats);

/*
 * Lock buffer by needed method for search.
 */
int
ginTraverseLock(Buffer buffer, bool searchMode)
{




























}

/*
 * Descend the tree to the leaf page that contains or would contain the key
 * we're searching for. The key should already be filled in 'btree', in
 * tree-type specific manner. If btree->fullScan is true, descends to the
 * leftmost leaf page.
 *
 * If 'searchmode' is false, on return stack->buffer is exclusively locked,
 * and the stack represents the full path to the root. Otherwise stack->buffer
 * is share-locked, and stack->parent is NULL.
 *
 * If 'rootConflictCheck' is true, tree root is checked for serialization
 * conflict.
 */
GinBtreeStack *
ginFindLeafPage(GinBtree btree, bool searchMode, bool rootConflictCheck, Snapshot snapshot)
{

























































































}

/*
 * Step right from current page.
 *
 * The next page is locked first, before releasing the current page. This is
 * crucial to protect from concurrent page deletion (see comment in
 * ginDeletePage).
 */
Buffer
ginStepRight(Buffer buffer, Relation index, int lockmode)
{


















}

void
freeGinBtreeStack(GinBtreeStack *stack)
{












}

/*
 * Try to find parent for current stack position. Returns correct parent and
 * child's offset in stack->parent. The root page is never released, to
 * prevent conflict with vacuum process.
 */
static void
ginFindParents(GinBtree btree, GinBtreeStack *stack)
{

































































































}

/*
 * Insert a new item to a page.
 *
 * Returns true if the insertion was finished. On false, the page was split and
 * the parent needs to be updated. (A root split returns true as it doesn't
 * need any further action by the caller to complete.)
 *
 * When inserting a downlink to an internal page, 'childbuf' contains the
 * child page that was split. Its GIN_INCOMPLETE_SPLIT flag will be cleared
 * atomically with the insert. Also, the existing item at offset stack->off
 * in the target page is updated to point to updateblkno.
 *
 * stack->buffer is locked on entry, and is kept locked.
 * Likewise for childbuf, if given.
 */
static bool
ginPlaceToPage(GinBtree btree, GinBtreeStack *stack, void *insertdata, BlockNumber updateblkno, Buffer childbuf, GinStatsData *buildStats)
{








































































































































































































































































































































}

/*
 * Finish a split by inserting the downlink for the new page to parent.
 *
 * On entry, stack->buffer is exclusively locked.
 *
 * If freestack is true, all the buffers are released and unlocked as we
 * crawl up the tree, and 'stack' is freed. Otherwise stack->buffer is kept
 * locked, and stack is unmodified, except for possibly moving right to find
 * the correct parent of page.
 */
static void
ginFinishSplit(GinBtree btree, GinBtreeStack *stack, bool freestack, GinStatsData *buildStats)
{


































































































}

/*
 * Insert a value to tree described by stack.
 *
 * The value to be inserted is given in 'insertdata'. Its format depends
 * on whether this is an entry or data tree, ginInsertValue just passes it
 * through to the tree-specific callback function.
 *
 * During an index build, buildStats is non-null and the counters it contains
 * are incremented as needed.
 *
 * NB: the passed-in stack is freed, as though by freeGinBtreeStack.
 */
void
ginInsertValue(GinBtree btree, GinBtreeStack *stack, void *insertdata, GinStatsData *buildStats)
{


















}