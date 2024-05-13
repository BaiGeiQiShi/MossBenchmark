/*-------------------------------------------------------------------------
 *
 * freespace.c
 *	  POSTGRES free space map for quickly finding free space in relations
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/freespace/freespace.c
 *
 *
 * NOTES:
 *
 *	Free Space Map keeps track of the amount of free space on pages, and
 *	allows quickly searching for a page with enough free space. The FSM is
 *	stored in a dedicated relation fork of all heap relations, and those
 *	index access methods that need it (see also indexfsm.c). See README for
 *	more information.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "storage/freespace.h"
#include "storage/fsm_internals.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"

/*
 * We use just one byte to store the amount of free space on a page, so we
 * divide the amount of free space a page can have into 256 different
 * categories. The highest category, 255, represents a page with at least
 * MaxFSMRequestSize bytes of free space, and the second highest category
 * represents the range from 254 * FSM_CAT_STEP, inclusive, to
 * MaxFSMRequestSize, exclusive.
 *
 * MaxFSMRequestSize depends on the architecture and BLCKSZ, but assuming
 * default 8k BLCKSZ, and that MaxFSMRequestSize is 8164 bytes, the
 * categories look like this:
 *
 *
 * Range	 Category
 * 0	- 31   0
 * 32	- 63   1
 * ...    ...  ...
 * 8096 - 8127 253
 * 8128 - 8163 254
 * 8164 - 8192 255
 *
 * The reason that MaxFSMRequestSize is special is that if MaxFSMRequestSize
 * isn't equal to a range boundary, a page with exactly MaxFSMRequestSize
 * bytes of free space wouldn't satisfy a request for MaxFSMRequestSize
 * bytes. If there isn't more than MaxFSMRequestSize bytes of free space on a
 * completely empty page, that would mean that we could never satisfy a
 * request of exactly MaxFSMRequestSize bytes.
 */
#define FSM_CATEGORIES 256
#define FSM_CAT_STEP (BLCKSZ / FSM_CATEGORIES)
#define MaxFSMRequestSize MaxHeapTupleSize

/*
 * Depth of the on-disk tree. We need to be able to address 2^32-1 blocks,
 * and 1626 is the smallest number that satisfies X^3 >= 2^32-1. Likewise,
 * 216 is the smallest number that satisfies X^4 >= 2^32-1. In practice,
 * this means that 4096 bytes is the smallest BLCKSZ that we can get away
 * with a 3-level tree, and 512 is the smallest we support.
 */
#define FSM_TREE_DEPTH ((SlotsPerFSMPage >= 1626) ? 3 : 4)

#define FSM_ROOT_LEVEL (FSM_TREE_DEPTH - 1)
#define FSM_BOTTOM_LEVEL 0

/*
 * The internal FSM routines work on a logical addressing scheme. Each
 * level of the tree can be thought of as a separately addressable file.
 */
typedef struct
{
  int level;     /* level */
  int logpageno; /* page number within the level */
} FSMAddress;

/* Address of the root page. */
static const FSMAddress FSM_ROOT_ADDRESS = {FSM_ROOT_LEVEL, 0};

/* functions to navigate the tree */
static FSMAddress
fsm_get_child(FSMAddress parent, uint16 slot);
static FSMAddress
fsm_get_parent(FSMAddress child, uint16 *slot);
static FSMAddress
fsm_get_location(BlockNumber heapblk, uint16 *slot);
static BlockNumber
fsm_get_heap_blk(FSMAddress addr, uint16 slot);
static BlockNumber
fsm_logical_to_physical(FSMAddress addr);

static Buffer
fsm_readbuf(Relation rel, FSMAddress addr, bool extend);
static void
fsm_extend(Relation rel, BlockNumber fsm_nblocks);

/* functions to convert amount of free space to a FSM category */
static uint8
fsm_space_avail_to_cat(Size avail);
static uint8
fsm_space_needed_to_cat(Size needed);
static Size
fsm_space_cat_to_avail(uint8 cat);

/* workhorse functions for various operations */
static int
fsm_set_and_search(Relation rel, FSMAddress addr, uint16 slot, uint8 newValue, uint8 minValue);
static BlockNumber
fsm_search(Relation rel, uint8 min_cat);
static uint8
fsm_vacuum_page(Relation rel, FSMAddress addr, BlockNumber start, BlockNumber end, bool *eof);

/******** Public API ********/

/*
 * GetPageWithFreeSpace - try to find a page in the given relation with
 *		at least the specified amount of free space.
 *
 * If successful, return the block number; if not, return InvalidBlockNumber.
 *
 * The caller must be prepared for the possibility that the returned page
 * will turn out to have too little space available by the time the caller
 * gets a lock on it.  In that case, the caller should report the actual
 * amount of free space available on that page and then try again (see
 * RecordAndGetPageWithFreeSpace).  If InvalidBlockNumber is returned,
 * extend the relation.
 */
BlockNumber
GetPageWithFreeSpace(Relation rel, Size spaceNeeded)
{



}

/*
 * RecordAndGetPageWithFreeSpace - update info about a page and try again.
 *
 * We provide this combo form to save some locking overhead, compared to
 * separate RecordPageWithFreeSpace + GetPageWithFreeSpace calls. There's
 * also some effort to return a page close to the old page; if there's a
 * page with enough free space on the same FSM page where the old one page
 * is located, it is preferred.
 */
BlockNumber
RecordAndGetPageWithFreeSpace(Relation rel, BlockNumber oldPage, Size oldSpaceAvail, Size spaceNeeded)
{























}

/*
 * RecordPageWithFreeSpace - update info about a page.
 *
 * Note that if the new spaceAvail value is higher than the old value stored
 * in the FSM, the space might not become visible to searchers until the next
 * FreeSpaceMapVacuum call, which updates the upper level pages.
 */
void
RecordPageWithFreeSpace(Relation rel, BlockNumber heapBlk, Size spaceAvail)
{








}

/*
 * XLogRecordPageWithFreeSpace - like RecordPageWithFreeSpace, for use in
 *		WAL replay
 */
void
XLogRecordPageWithFreeSpace(RelFileNode rnode, BlockNumber heapBlk, Size spaceAvail)
{


























}

/*
 * GetRecordedFreePage - return the amount of free space on a particular page,
 *		according to the FSM.
 */
Size
GetRecordedFreeSpace(Relation rel, BlockNumber heapBlk)
{

















}

/*
 * FreeSpaceMapTruncateRel - adjust for truncation of a relation.
 *
 * The caller must hold AccessExclusiveLock on the relation, to ensure that
 * other backends receive the smgr invalidation event that this function sends
 * before they access the FSM again.
 *
 * nblocks is the new size of the heap.
 */
void
FreeSpaceMapTruncateRel(Relation rel, BlockNumber nblocks)
{























































































}

/*
 * FreeSpaceMapVacuum - update upper-level pages in the rel's FSM
 *
 * We assume that the bottom-level pages have already been updated with
 * new free-space information.
 */
void
FreeSpaceMapVacuum(Relation rel)
{




}

/*
 * FreeSpaceMapVacuumRange - update upper-level pages in the rel's FSM
 *
 * As above, but assume that only heap pages between start and end-1 inclusive
 * have new free-space information, so update only the upper-level slots
 * covering that block range.  end == InvalidBlockNumber is equivalent to
 * "all the rest of the relation".
 */
void
FreeSpaceMapVacuumRange(Relation rel, BlockNumber start, BlockNumber end)
{







}

/******** Internal routines ********/

/*
 * Return category corresponding x bytes of free space
 */
static uint8
fsm_space_avail_to_cat(Size avail)
{





















}

/*
 * Return the lower bound of the range of free space represented by given
 * category.
 */
static Size
fsm_space_cat_to_avail(uint8 cat)
{









}

/*
 * Which category does a page need to have, to accommodate x bytes of data?
 * While fsm_size_to_avail_cat() rounds down, this needs to round up.
 */
static uint8
fsm_space_needed_to_cat(Size needed)
{





















}

/*
 * Returns the physical block number of a FSM page
 */
static BlockNumber
fsm_logical_to_physical(FSMAddress addr)
{






























}

/*
 * Return the FSM location corresponding to given heap block.
 */
static FSMAddress
fsm_get_location(BlockNumber heapblk, uint16 *slot)
{







}

/*
 * Return the heap block number corresponding to given location in the FSM.
 */
static BlockNumber
fsm_get_heap_blk(FSMAddress addr, uint16 slot)
{


}

/*
 * Given a logical address of a child page, get the logical page number of
 * the parent, and the slot within the parent corresponding to the child.
 */
static FSMAddress
fsm_get_parent(FSMAddress child, uint16 *slot)
{









}

/*
 * Given a logical address of a parent page and a slot number, get the
 * logical address of the corresponding child page.
 */
static FSMAddress
fsm_get_child(FSMAddress parent, uint16 slot)
{








}

/*
 * Read a FSM page.
 *
 * If the page doesn't exist, InvalidBuffer is returned, or if 'extend' is
 * true, the FSM file is extended.
 */
static Buffer
fsm_readbuf(Relation rel, FSMAddress addr, bool extend)
{










































































}

/*
 * Ensure that the FSM fork is at least fsm_nblocks long, extending
 * it if necessary with empty pages. And by empty, I mean pages filled
 * with zeros, meaning there's no free space.
 */
static void
fsm_extend(Relation rel, BlockNumber fsm_nblocks)
{
















































}

/*
 * Set value in given FSM page and slot.
 *
 * If minValue > 0, the updated page is also searched for a page with at
 * least minValue of free space. If one is found, its slot number is
 * returned, -1 otherwise.
 */
static int
fsm_set_and_search(Relation rel, FSMAddress addr, uint16 slot, uint8 newValue, uint8 minValue)
{























}

/*
 * Search the tree for a heap page with at least min_cat of free space
 */
static BlockNumber
fsm_search(Relation rel, uint8 min_cat)
{





















































































}

/*
 * Recursive guts of FreeSpaceMapVacuum
 *
 * Examine the FSM page indicated by addr, as well as its children, updating
 * upper-level nodes that cover the heap block range from start to end-1.
 * (It's okay if end is beyond the actual end of the map.)
 * Return the maximum freespace value on this page.
 *
 * If addr is past the end of the FSM, set *eof_p to true and return 0.
 *
 * This traverses the tree in depth-first order.  The tree is stored
 * physically in depth-first order, so this should be pretty I/O efficient.
 */
static uint8
fsm_vacuum_page(Relation rel, FSMAddress addr, BlockNumber start, BlockNumber end, bool *eof_p)
{

















































































































}