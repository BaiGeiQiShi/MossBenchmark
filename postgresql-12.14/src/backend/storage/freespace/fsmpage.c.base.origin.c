/*-------------------------------------------------------------------------
 *
 * fsmpage.c
 *	  routines to search and manipulate one FSM page.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/freespace/fsmpage.c
 *
 * NOTES:
 *
 *	The public functions in this file form an API that hides the internal
 *	structure of a FSM page. This allows freespace.c to treat each FSM page
 *	as a black box with SlotsPerPage "slots". fsm_set_avail() and
 *	fsm_get_avail() let you get/set the value of a slot, and
 *	fsm_search_avail() lets you search for a slot with value >= X.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "storage/bufmgr.h"
#include "storage/fsm_internals.h"

/* Macros to navigate the tree within a page. Root has index zero. */
#define leftchild(x) (2 * (x) + 1)
#define rightchild(x) (2 * (x) + 2)
#define parentof(x) (((x)-1) / 2)

/*
 * Find right neighbor of x, wrapping around within the level
 */
static int
rightneighbor(int x)
{


















}

/*
 * Sets the value of a slot on page. Returns true if the page was modified.
 *
 * The caller must hold an exclusive lock on the page.
 */
bool
fsm_set_avail(Page page, int slot, uint8 value)
{























































}

/*
 * Returns the value of given slot on page.
 *
 * Since this is just a read-only access of a single byte, the page doesn't
 * need to be locked.
 */
uint8
fsm_get_avail(Page page, int slot)
{





}

/*
 * Returns the value at the root of a page.
 *
 * Since this is just a read-only access of a single byte, the page doesn't
 * need to be locked.
 */
uint8
fsm_get_max_avail(Page page)
{



}

/*
 * Searches for a slot with category at least minvalue.
 * Returns slot number, or -1 if none found.
 *
 * The caller must hold at least a shared lock on the page, and this
 * function can unlock and lock the page again in exclusive mode if it
 * needs to be updated. exclusive_lock_held should be set to true if the
 * caller is already holding an exclusive lock, to avoid extra work.
 *
 * If advancenext is false, fp_next_slot is set to point to the returned
 * slot, and if it's true, to the slot after the returned slot.
 */
int
fsm_search_avail(Buffer buf, uint8 minvalue, bool advancenext, bool exclusive_lock_held)
{




















































































































































}

/*
 * Sets the available space to zero for all slots numbered >= nslots.
 * Returns true if the page was modified.
 */
bool
fsm_truncate_avail(Page page, int nslots)
{
























}

/*
 * Reconstructs the upper levels of a page. Returns true if the page
 * was modified.
 */
bool
fsm_rebuild_page(Page page)
{

































}