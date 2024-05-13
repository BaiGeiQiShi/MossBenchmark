/*-------------------------------------------------------------------------
 *
 * binaryheap.c
 *	  A simple binary heap implementation
 *
 * Portions Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/binaryheap.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "lib/binaryheap.h"

static void
sift_down(binaryheap *heap, int node_off);
static void
sift_up(binaryheap *heap, int node_off);
static inline void
swap_nodes(binaryheap *heap, int a, int b);

/*
 * binaryheap_allocate
 *
 * Returns a pointer to a newly-allocated heap that has the capacity to
 * store the given number of nodes, with the heap property defined by
 * the given comparator function, which will be invoked with the additional
 * argument specified by 'arg'.
 */
binaryheap *
binaryheap_allocate(int capacity, binaryheap_comparator compare, void *arg)
{













}

/*
 * binaryheap_reset
 *
 * Resets the heap to an empty state, losing its data content but not the
 * parameters passed at allocation.
 */
void
binaryheap_reset(binaryheap *heap)
{


}

/*
 * binaryheap_free
 *
 * Releases memory used by the given binaryheap.
 */
void
binaryheap_free(binaryheap *heap)
{

}

/*
 * These utility functions return the offset of the left child, right
 * child, and parent of the node at the given index, respectively.
 *
 * The heap is represented as an array of nodes, with the root node
 * stored at index 0. The left child of node i is at index 2*i+1, and
 * the right child at 2*i+2. The parent of node i is at index (i-1)/2.
 */

static inline int
left_offset(int i)
{

}

static inline int
right_offset(int i)
{

}

static inline int
parent_offset(int i)
{

}

/*
 * binaryheap_add_unordered
 *
 * Adds the given datum to the end of the heap's list of nodes in O(1) without
 * preserving the heap property. This is a convenience to add elements quickly
 * to a new heap. To obtain a valid heap, one must call binaryheap_build()
 * afterwards.
 */
void
binaryheap_add_unordered(binaryheap *heap, Datum d)
{







}

/*
 * binaryheap_build
 *
 * Assembles a valid heap in O(n) from the nodes added by
 * binaryheap_add_unordered(). Not needed otherwise.
 */
void
binaryheap_build(binaryheap *heap)
{







}

/*
 * binaryheap_add
 *
 * Adds the given datum to the heap in O(log n) time, while preserving
 * the heap property.
 */
void
binaryheap_add(binaryheap *heap, Datum d)
{







}

/*
 * binaryheap_first
 *
 * Returns a pointer to the first (root, topmost) node in the heap
 * without modifying the heap. The caller must ensure that this
 * routine is not used on an empty heap. Always O(1).
 */
Datum
binaryheap_first(binaryheap *heap)
{


}

/*
 * binaryheap_remove_first
 *
 * Removes the first (root, topmost) node in the heap and returns a
 * pointer to it after rebalancing the heap. The caller must ensure
 * that this routine is not used on an empty heap. O(log n) worst
 * case.
 */
Datum
binaryheap_remove_first(binaryheap *heap)
{


















}

/*
 * binaryheap_replace_first
 *
 * Replace the topmost element of a non-empty heap, preserving the heap
 * property.  O(1) in the best case, or O(log n) if it must fall back to
 * sifting the new node down.
 */
void
binaryheap_replace_first(binaryheap *heap, Datum d)
{








}

/*
 * Swap the contents of two nodes.
 */
static inline void
swap_nodes(binaryheap *heap, int a, int b)
{





}

/*
 * Sift a node up to the highest position it can hold according to the
 * comparator.
 */
static void
sift_up(binaryheap *heap, int node_off)
{























}

/*
 * Sift a node down from its current position to satisfy the heap
 * property.
 */
static void
sift_down(binaryheap *heap, int node_off)
{






































}