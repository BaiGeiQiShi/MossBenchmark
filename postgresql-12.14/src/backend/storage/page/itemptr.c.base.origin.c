/*-------------------------------------------------------------------------
 *
 * itemptr.c
 *	  POSTGRES disk item pointer code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/page/itemptr.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "storage/itemptr.h"

/*
 * ItemPointerEquals
 *	Returns true if both item pointers point to the same item,
 *	 otherwise returns false.
 *
 * Note:
 *	Asserts that the disk item pointers are both valid!
 */
bool
ItemPointerEquals(ItemPointer pointer1, ItemPointer pointer2)
{














}

/*
 * ItemPointerCompare
 *		Generic btree-style comparison for item pointers.
 */
int32
ItemPointerCompare(ItemPointer arg1, ItemPointer arg2)
{



























}