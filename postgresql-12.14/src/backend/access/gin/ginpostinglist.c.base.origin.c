/*-------------------------------------------------------------------------
 *
 * ginpostinglist.c
 *	  routines for dealing with posting lists.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginpostinglist.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"

#ifdef USE_ASSERT_CHECKING
#define CHECK_ENCODING_ROUNDTRIP
#endif

/*
 * For encoding purposes, item pointers are represented as 64-bit unsigned
 * integers. The lowest 11 bits represent the offset number, and the next
 * lowest 32 bits are the block number. That leaves 21 bits unused, i.e.
 * only 43 low bits are used.
 *
 * 11 bits is enough for the offset number, because MaxHeapTuplesPerPage <
 * 2^11 on all supported block sizes. We are frugal with the bits, because
 * smaller integers use fewer bytes in the varbyte encoding, saving disk
 * space. (If we get a new table AM in the future that wants to use the full
 * range of possible offset numbers, we'll need to change this.)
 *
 * These 43-bit integers are encoded using varbyte encoding. In each byte,
 * the 7 low bits contain data, while the highest bit is a continuation bit.
 * When the continuation bit is set, the next byte is part of the same
 * integer, otherwise this is the last byte of this integer. 43 bits need
 * at most 7 bytes in this encoding:
 *
 * 0XXXXXXX
 * 1XXXXXXX 0XXXXYYY
 * 1XXXXXXX 1XXXXYYY 0YYYYYYY
 * 1XXXXXXX 1XXXXYYY 1YYYYYYY 0YYYYYYY
 * 1XXXXXXX 1XXXXYYY 1YYYYYYY 1YYYYYYY 0YYYYYYY
 * 1XXXXXXX 1XXXXYYY 1YYYYYYY 1YYYYYYY 1YYYYYYY 0YYYYYYY
 * 1XXXXXXX 1XXXXYYY 1YYYYYYY 1YYYYYYY 1YYYYYYY 1YYYYYYY 0uuuuuuY
 *
 * X = bits used for offset number
 * Y = bits used for block number
 * u = unused bit
 *
 * The bytes are in stored in little-endian order.
 *
 * An important property of this encoding is that removing an item from list
 * never increases the size of the resulting compressed posting list. Proof:
 *
 * Removing number is actually replacement of two numbers with their sum. We
 * have to prove that varbyte encoding of a sum can't be longer than varbyte
 * encoding of its summands. Sum of two numbers is at most one bit wider than
 * the larger of the summands. Widening a number by one bit enlarges its length
 * in varbyte encoding by at most one byte. Therefore, varbyte encoding of sum
 * is at most one byte longer than varbyte encoding of larger summand. Lesser
 * summand is at least one byte, so the sum cannot take more space than the
 * summands, Q.E.D.
 *
 * This property greatly simplifies VACUUM, which can assume that posting
 * lists always fit on the same page after vacuuming. Note that even though
 * that holds for removing items from a posting list, you must also be
 * careful to not cause expansion e.g. when merging uncompressed items on the
 * page into the compressed lists, when vacuuming.
 */

/*
 * How many bits do you need to encode offset number? OffsetNumber is a 16-bit
 * integer, but you can't fit that many items on a page. 11 ought to be more
 * than enough. It's tempting to derive this from MaxHeapTuplesPerPage, and
 * use the minimum number of bits, but that would require changing the on-disk
 * format if MaxHeapTuplesPerPage changes. Better to leave some slack.
 */
#define MaxHeapTuplesPerPageBits 11

/* Max. number of bytes needed to encode the largest supported integer. */
#define MaxBytesPerInteger 7

static inline uint64
itemptr_to_uint64(const ItemPointer iptr)
{










}

static inline void
uint64_to_itemptr(uint64 val, ItemPointer iptr)
{





}

/*
 * Varbyte-encode 'val' into *ptr. *ptr is incremented to next integer.
 */
static void
encode_varbyte(uint64 val, unsigned char **ptr)
{










}

/*
 * Decode varbyte-encoded integer at *ptr. *ptr is incremented to next integer.
 */
static uint64
decode_varbyte(unsigned char **ptr)
{
















































}

/*
 * Encode a posting list.
 *
 * The encoded list is returned in a palloc'd struct, which will be at most
 * 'maxsize' bytes in size.  The number items in the returned segment is
 * returned in *nwritten. If it's not equal to nipd, not all the items fit
 * in 'maxsize', and only the first *nwritten were encoded.
 *
 * The allocated size of the returned struct is short-aligned, and the padding
 * byte at the end, if any, is zero.
 */
GinPostingList *
ginCompressPostingList(const ItemPointer ipd, int nipd, int maxsize, int *nwritten)
{

























































































}

/*
 * Decode a compressed posting list into an array of item pointers.
 * The number of items is returned in *ndecoded.
 */
ItemPointer
ginPostingListDecode(GinPostingList *plist, int *ndecoded)
{

}

/*
 * Decode multiple posting list segments into an array of item pointers.
 * The number of items is returned in *ndecoded_out. The segments are stored
 * one after each other, with total size 'len' bytes.
 */
ItemPointer
ginPostingListDecodeAllSegments(GinPostingList *segment, int len, int *ndecoded_out)
{























































}

/*
 * Add all item pointers from a bunch of posting lists to a TIDBitmap.
 */
int
ginPostingListDecodeAllSegmentsToTbm(GinPostingList *ptr, int len, TIDBitmap *tbm)
{








}

/*
 * Merge two ordered arrays of itempointers, eliminating any duplicates.
 *
 * Returns a palloc'd array, and *nmerged is set to the number of items in
 * the result, after eliminating duplicates.
 */
ItemPointer
ginMergeItemPointers(ItemPointerData *a, uint32 na, ItemPointerData *b, uint32 nb, int *nmerged)
{




























































}