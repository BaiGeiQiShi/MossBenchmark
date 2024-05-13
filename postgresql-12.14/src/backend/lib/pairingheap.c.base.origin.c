/*-------------------------------------------------------------------------
 *
 * pairingheap.c
 *	  A Pairing Heap implementation
 *
 * A pairing heap is a data structure that's useful for implementing
 * priority queues. It is simple to implement, and provides amortized O(1)
 * insert and find-min operations, and amortized O(log n) delete-min.
 *
 * The pairing heap was first described in this paper:
 *
 *	Michael L. Fredman, Robert Sedgewick, Daniel D. Sleator, and Robert E.
 *	 Tarjan. 1986.
 *	The pairing heap: a new form of self-adjusting heap.
 *	Algorithmica 1, 1 (January 1986), pages 111-129. DOI: 10.1007/BF01840439
 *
 * Portions Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/pairingheap.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "lib/pairingheap.h"

static pairingheap_node *
merge(pairingheap *heap, pairingheap_node *a, pairingheap_node *b);
static pairingheap_node *
merge_children(pairingheap *heap, pairingheap_node *children);

/*
 * pairingheap_allocate
 *
 * Returns a pointer to a newly-allocated heap, with the heap property defined
 * by the given comparator function, which will be invoked with the additional
 * argument specified by 'arg'.
 */
pairingheap *
pairingheap_allocate(pairingheap_comparator compare, void *arg)
{









}

/*
 * pairingheap_free
 *
 * Releases memory used by the given pairingheap.
 *
 * Note: The nodes in the heap are not freed!
 */
void
pairingheap_free(pairingheap *heap)
{

}

/*
 * A helper function to merge two subheaps into one.
 *
 * The subheap with smaller value is put as a child of the other one (assuming
 * a max-heap).
 *
 * The next_sibling and prev_or_parent pointers of the input nodes are
 * ignored. On return, the returned node's next_sibling and prev_or_parent
 * pointers are garbage.
 */
static pairingheap_node *
merge(pairingheap *heap, pairingheap_node *a, pairingheap_node *b)
{





























}

/*
 * pairingheap_add
 *
 * Adds the given node to the heap in O(1) time.
 */
void
pairingheap_add(pairingheap *heap, pairingheap_node *node)
{






}

/*
 * pairingheap_first
 *
 * Returns a pointer to the first (root, topmost) node in the heap without
 * modifying the heap. The caller must ensure that this routine is not used on
 * an empty heap. Always O(1).
 */
pairingheap_node *
pairingheap_first(pairingheap *heap)
{



}

/*
 * pairingheap_remove_first
 *
 * Removes the first (root, topmost) node in the heap and returns a pointer to
 * it after rebalancing the heap. The caller must ensure that this routine is
 * not used on an empty heap. O(log n) amortized.
 */
pairingheap_node *
pairingheap_remove_first(pairingheap *heap)
{

















}

/*
 * Remove 'node' from the heap. O(log n) amortized.
 */
void
pairingheap_remove(pairingheap *heap, pairingheap_node *node)
{





























































}

/*
 * Merge a list of subheaps into a single heap.
 *
 * This implements the basic two-pass merging strategy, first forming pairs
 * from left to right, and then merging the pairs.
 */
static pairingheap_node *
merge_children(pairingheap *heap, pairingheap_node *children)
{




















































}

/*
 * A debug function to dump the contents of the heap as a string.
 *
 * The 'dumpfunc' callback appends a string representation of a single node
 * to the StringInfo. 'opaque' can be used to pass more information to the
 * callback.
 */
#ifdef PAIRINGHEAP_DEBUG
static void
pairingheap_dump_recurse(StringInfo buf, pairingheap_node *node, void (*dumpfunc)(pairingheap_node *node, StringInfo buf, void *opaque), void *opaque, int depth, pairingheap_node *prev_or_parent)
{
  while (node)
  {
    Assert(node->prev_or_parent == prev_or_parent);

    appendStringInfoSpaces(buf, depth * 4);
    dumpfunc(node, buf, opaque);
    appendStringInfoChar(buf, '\n');
    if (node->first_child)
    {
      pairingheap_dump_recurse(buf, node->first_child, dumpfunc, opaque, depth + 1, node);
    }
    prev_or_parent = node;
    node = node->next_sibling;
  }
}

char *
pairingheap_dump(pairingheap *heap, void (*dumpfunc)(pairingheap_node *node, StringInfo buf, void *opaque), void *opaque)
{
  StringInfoData buf;

  if (!heap->ph_root)
  {
    return pstrdup("(empty)");
  }

  initStringInfo(&buf);

  pairingheap_dump_recurse(&buf, heap->ph_root, dumpfunc, opaque, 0, NULL);

  return buf.data;
}
#endif