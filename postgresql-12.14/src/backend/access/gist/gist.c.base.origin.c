/*-------------------------------------------------------------------------
 *
 * gist.c
 *	  interface routines for the postgres GiST index access method.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/gist/gist.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/gist_private.h"
#include "access/gistscan.h"
#include "catalog/pg_collation.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "nodes/execnodes.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/* non-export function prototypes */
static void
gistfixsplit(GISTInsertState *state, GISTSTATE *giststate);
static bool
gistinserttuple(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple tuple, OffsetNumber oldoffnum);
static bool
gistinserttuples(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple *tuples, int ntup, OffsetNumber oldoffnum, Buffer leftchild, Buffer rightchild, bool unlockbuf, bool unlockleftchild);
static void
gistfinishsplit(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, List *splitinfo, bool releasebuf);
static void
gistprunepage(Relation rel, Page page, Buffer buffer, Relation heapRel);

#define ROTATEDIST(d)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    SplitedPageLayout *tmp = (SplitedPageLayout *)palloc0(sizeof(SplitedPageLayout));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    tmp->block.blkno = InvalidBlockNumber;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    tmp->buffer = InvalidBuffer;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    tmp->next = (d);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (d) = tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

/*
 * GiST handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
gisthandler(PG_FUNCTION_ARGS)
{










































}

/*
 * Create and return a temporary memory context for use by GiST. We
 * _always_ invoke user-provided methods in a temporary memory
 * context, so that memory leaks in those functions cannot cause
 * problems. Also, we use some additional temporary contexts in the
 * GiST code itself, to avoid the need to do some awkward manual
 * memory management.
 */
MemoryContext
createTempGistContext(void)
{

}

/*
 *	gistbuildempty() -- build an empty gist index in the initialization fork
 */
void
gistbuildempty(Relation index)
{















}

/*
 *	gistinsert -- wrapper for GiST tuple insertion.
 *
 *	  This is the public interface routine for tuple insertion in GiSTs.
 *	  It doesn't do any work; just locks the relation and passes the buck.
 */
bool
gistinsert(Relation r, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{


























}

/*
 * Place tuples from 'itup' to 'buffer'. If 'oldoffnum' is valid, the tuple
 * at that offset is atomically removed along with inserting the new tuples.
 * This is used to replace a tuple with a new one.
 *
 * If 'leftchildbuf' is valid, we're inserting the downlink for the page
 * to the right of 'leftchildbuf', or updating the downlink for 'leftchildbuf'.
 * F_FOLLOW_RIGHT flag on 'leftchildbuf' is cleared and NSN is set.
 *
 * If 'markfollowright' is true and the page is split, the left child is
 * marked with F_FOLLOW_RIGHT flag. That is the normal case. During buffered
 * index build, however, there is no concurrent access and the page splitting
 * is done in a slightly simpler fashion, and false is passed.
 *
 * If there is not enough room on the page, it is split. All the split
 * pages are kept pinned and locked and returned in *splitinfo, the caller
 * is responsible for inserting the downlinks for them. However, if
 * 'buffer' is the root page and it needs to be split, gistplacetopage()
 * performs the split as one atomic operation, and *splitinfo is set to NIL.
 * In that case, we continue to hold the root page locked, and the child
 * pages are released; note that new tuple(s) are *not* on the root page
 * but in one of the new child pages.
 *
 * If 'newblkno' is not NULL, returns the block number of page the first
 * new/updated tuple was inserted to. Usually it's the given page, but could
 * be its right sibling if the page was split.
 *
 * Returns 'true' if the page was split, 'false' otherwise.
 */
bool
gistplacetopage(Relation rel, Size freespace, GISTSTATE *giststate, Buffer buffer, IndexTuple *itup, int ntup, OffsetNumber oldoffnum, BlockNumber *newblkno, Buffer leftchildbuf, List **splitinfo, bool markfollowright, Relation heapRel, bool is_build)
{















































































































































































































































































































































































































































}

/*
 * Workhouse routine for doing insertion into a GiST index. Note that
 * this routine assumes it is invoked in a short-lived memory context,
 * so it does not bother releasing palloc'd allocations.
 */
void
gistdoinsert(Relation r, IndexTuple itup, Size freespace, GISTSTATE *giststate, Relation heapRel, bool is_build)
{



































































































































































































































































}

/*
 * Traverse the tree to find path from root page to specified "child" block.
 *
 * returns a new insertion stack, starting from the parent of "child", up
 * to the root. *downlinkoffnum is set to the offset of the downlink in the
 * direct parent of child.
 *
 * To prevent deadlocks, this should lock only one page at a time.
 */
static GISTInsertStack *
gistFindPath(Relation r, BlockNumber child, OffsetNumber *downlinkoffnum)
{





































































































}

/*
 * Updates the stack so that child->parent is the correct parent of the
 * child. child->parent must be exclusively locked on entry, and will
 * remain so at exit, but it might not be the same page anymore.
 */
static void
gistFindCorrectParent(Relation r, GISTInsertStack *child)
{















































































}

/*
 * Form a downlink pointer for the page in 'buf'.
 */
static IndexTuple
gistformdownlink(Relation rel, Buffer buf, GISTSTATE *giststate, GISTInsertStack *stack)
{




















































}

/*
 * Complete the incomplete split of state->stack->page.
 */
static void
gistfixsplit(GISTInsertState *state, GISTSTATE *giststate)
{













































}

/*
 * Insert or replace a tuple in stack->buffer. If 'oldoffnum' is valid, the
 * tuple at 'oldoffnum' is replaced, otherwise the tuple is inserted as new.
 * 'stack' represents the path from the root to the page being updated.
 *
 * The caller must hold an exclusive lock on stack->buffer.  The lock is still
 * held on return, but the page might not contain the inserted tuple if the
 * page was split. The function returns true if the page was split, false
 * otherwise.
 */
static bool
gistinserttuple(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple tuple, OffsetNumber oldoffnum)
{

}

/* ----------------
 * An extended workhorse version of gistinserttuple(). This version allows
 * inserting multiple tuples, or replacing a single tuple with multiple tuples.
 * This is used to recursively update the downlinks in the parent when a page
 * is split.
 *
 * If leftchild and rightchild are valid, we're inserting/replacing the
 * downlink for rightchild, and leftchild is its left sibling. We clear the
 * F_FOLLOW_RIGHT flag and update NSN on leftchild, atomically with the
 * insertion of the downlink.
 *
 * To avoid holding locks for longer than necessary, when recursing up the
 * tree to update the parents, the locking is a bit peculiar here. On entry,
 * the caller must hold an exclusive lock on stack->buffer, as well as
 * leftchild and rightchild if given. On return:
 *
 *	- Lock on stack->buffer is released, if 'unlockbuf' is true. The page is
 *	  always kept pinned, however.
 *	- Lock on 'leftchild' is released, if 'unlockleftchild' is true. The
 *page is kept pinned.
 *	- Lock and pin on 'rightchild' are always released.
 *
 * Returns 'true' if the page had to be split. Note that if the page was
 * split, the inserted/updated tuples might've been inserted to a right
 * sibling of stack->buffer instead of stack->buffer itself.
 */
static bool
gistinserttuples(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple *tuples, int ntup, OffsetNumber oldoffnum, Buffer leftchild, Buffer rightchild, bool unlockbuf, bool unlockleftchild)
{










































}

/*
 * Finish an incomplete split by inserting/updating the downlinks in parent
 * page. 'splitinfo' contains all the child pages involved in the split,
 * from left-to-right.
 *
 * On entry, the caller must hold a lock on stack->buffer and all the child
 * pages in 'splitinfo'. If 'unlockbuf' is true, the lock on stack->buffer is
 * released on return. The child pages are always unlocked and unpinned.
 */
static void
gistfinishsplit(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, List *splitinfo, bool unlockbuf)
{























































































}

/*
 * gistSplit -- split a page in the tree and fill struct
 * used for XLOG and real writes buffers. Function is recursive, ie
 * it will split page until keys will fit in every page.
 */
SplitedPageLayout *
gistSplit(Relation r, Page page, IndexTuple *itup, /* contains compressed entry */
    int len, GISTSTATE *giststate)
{











































































}

/*
 * Create a GISTSTATE and fill it with information about the index
 */
GISTSTATE *
initGISTstate(Relation index)
{



























































































































}

void
freeGISTstate(GISTSTATE *giststate)
{


}

/*
 * gistprunepage() -- try to remove LP_DEAD items from the given page.
 * Function assumes that buffer is exclusively locked.
 */
static void
gistprunepage(Relation rel, Page page, Buffer buffer, Relation heapRel)
{



































































}