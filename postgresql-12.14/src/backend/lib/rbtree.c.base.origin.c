/*-------------------------------------------------------------------------
 *
 * rbtree.c
 *	  implementation for PostgreSQL generic Red-Black binary tree package
 *	  Adopted from http://algolist.manual.ru/ds/rbtree.php
 *
 * This code comes from Thomas Niemann's "Sorting and Searching Algorithms:
 * a Cookbook".
 *
 * See http://www.cs.auckland.ac.nz/software/AlgAnim/niemann/s_man.htm for
 * license terms: "Source code, when part of a software project, may be used
 * freely without reference to the author."
 *
 * Red-black trees are a type of balanced binary tree wherein (1) any child of
 * a red node is always black, and (2) every path from root to leaf traverses
 * an equal number of black nodes.  From these properties, it follows that the
 * longest path from root to leaf is only about twice as long as the shortest,
 * so lookups are guaranteed to run in O(lg n) time.
 *
 * Copyright (c) 2009-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/rbtree.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/rbtree.h"

/*
 * Colors of nodes (values of RBTNode.color)
 */
#define RBTBLACK (0)
#define RBTRED (1)

/*
 * RBTree control structure
 */
struct RBTree
{
  RBTNode *root; /* root node, or RBTNIL if tree is empty */

  /* Remaining fields are constant after rbt_create */

  Size node_size; /* actual size of tree nodes */
  /* The caller-supplied manipulation functions */
  rbt_comparator comparator;
  rbt_combiner combiner;
  rbt_allocfunc allocfunc;
  rbt_freefunc freefunc;
  /* Passthrough arg passed to all manipulation functions */
  void *arg;
};

/*
 * all leafs are sentinels, use customized NIL name to prevent
 * collision with system-wide constant NIL which is actually NULL
 */
#define RBTNIL (&sentinel)

static RBTNode sentinel = {RBTBLACK, RBTNIL, RBTNIL, NULL};

/*
 * rbt_create: create an empty RBTree
 *
 * Arguments are:
 *	node_size: actual size of tree nodes (> sizeof(RBTNode))
 *	The manipulation functions:
 *	comparator: compare two RBTNodes for less/equal/greater
 *	combiner: merge an existing tree entry with a new one
 *	allocfunc: allocate a new RBTNode
 *	freefunc: free an old RBTNode
 *	arg: passthrough pointer that will be passed to the manipulation
 *functions
 *
 * Note that the combiner's righthand argument will be a "proposed" tree node,
 * ie the input to rbt_insert, in which the RBTNode fields themselves aren't
 * valid.  Similarly, either input to the comparator may be a "proposed" node.
 * This shouldn't matter since the functions aren't supposed to look at the
 * RBTNode fields, only the extra fields of the struct the RBTNode is embedded
 * in.
 *
 * The freefunc should just be pfree or equivalent; it should NOT attempt
 * to free any subsidiary data, because the node passed to it may not contain
 * valid data!	freefunc can be NULL if caller doesn't require retail
 * space reclamation.
 *
 * The RBTree node is palloc'd in the caller's memory context.  Note that
 * all contents of the tree are actually allocated by the caller, not here.
 *
 * Since tree contents are managed by the caller, there is currently not
 * an explicit "destroy" operation; typically a tree would be freed by
 * resetting or deleting the memory context it's stored in.  You can pfree
 * the RBTree node if you feel the urge.
 */
RBTree *
rbt_create(Size node_size, rbt_comparator comparator, rbt_combiner combiner, rbt_allocfunc allocfunc, rbt_freefunc freefunc, void *arg)
{














}

/* Copy the additional data fields from one RBTNode to another */
static inline void
rbt_copy_data(RBTree *rbt, RBTNode *dest, const RBTNode *src)
{

}

/**********************************************************************
 *						  Search
 **
 **********************************************************************/

/*
 * rbt_find: search for a value in an RBTree
 *
 * data represents the value to try to find.  Its RBTNode fields need not
 * be valid, it's the extra data in the larger struct that is of interest.
 *
 * Returns the matching tree entry, or NULL if no match is found.
 */
RBTNode *
rbt_find(RBTree *rbt, const RBTNode *data)
{





















}

/*
 * rbt_leftmost: fetch the leftmost (smallest-valued) tree node.
 * Returns NULL if tree is empty.
 *
 * Note: in the original implementation this included an unlink step, but
 * that's a bit awkward.  Just call rbt_delete on the result if that's what
 * you want.
 */
RBTNode *
rbt_leftmost(RBTree *rbt)
{















}

/**********************************************************************
 *							  Insertion
 **
 **********************************************************************/

/*
 * Rotate node x to left.
 *
 * x's right child takes its place in the tree, and x becomes the left
 * child of that node.
 */
static void
rbt_rotate_left(RBTree *rbt, RBTNode *x)
{




































}

/*
 * Rotate node x to right.
 *
 * x's left right child takes its place in the tree, and x becomes the right
 * child of that node.
 */
static void
rbt_rotate_right(RBTree *rbt, RBTNode *x)
{




































}

/*
 * Maintain Red-Black tree balance after inserting node x.
 *
 * The newly inserted node is always initially marked red.  That may lead to
 * a situation where a red node has a red child, which is prohibited.  We can
 * always fix the problem by a series of color changes and/or "rotations",
 * which move the problem progressively higher up in the tree.  If one of the
 * two red nodes is the root, we can always fix the problem by changing the
 * root from red to black.
 *
 * (This does not work lower down in the tree because we must also maintain
 * the invariant that every leaf has equal black-height.)
 */
static void
rbt_insert_fixup(RBTree *rbt, RBTNode *x)
{























































































}

/*
 * rbt_insert: insert a new value into the tree.
 *
 * data represents the value to insert.  Its RBTNode fields need not
 * be valid, it's the extra data in the larger struct that is of interest.
 *
 * If the value represented by "data" is not present in the tree, then
 * we copy "data" into a new tree entry and return that node, setting *isNew
 * to true.
 *
 * If the value represented by "data" is already present, then we call the
 * combiner function to merge data into the existing node, and return the
 * existing node, setting *isNew to false.
 *
 * "data" is unmodified in either case; it's typically just a local
 * variable in the caller.
 */
RBTNode *
rbt_insert(RBTree *rbt, const RBTNode *data, bool *isNew)
{


























































}

/**********************************************************************
 *							Deletion
 **
 **********************************************************************/

/*
 * Maintain Red-Black tree balance after deleting a black node.
 */
static void
rbt_delete_fixup(RBTree *rbt, RBTNode *x)
{


























































































}

/*
 * Delete node z from tree.
 */
static void
rbt_delete_node(RBTree *rbt, RBTNode *z)
{















































































}

/*
 * rbt_delete: remove the given tree entry
 *
 * "node" must have previously been found via rbt_find or rbt_leftmost.
 * It is caller's responsibility to free any subsidiary data attached
 * to the node before calling rbt_delete.  (Do *not* try to push that
 * responsibility off to the freefunc, as some other physical node
 * may be the one actually freed!)
 */
void
rbt_delete(RBTree *rbt, RBTNode *node)
{

}

/**********************************************************************
 *						  Traverse
 **
 **********************************************************************/

static RBTNode *
rbt_left_right_iterator(RBTreeIterator *iter)
{











































}

static RBTNode *
rbt_right_left_iterator(RBTreeIterator *iter)
{











































}

/*
 * rbt_begin_iterate: prepare to traverse the tree in any of several orders
 *
 * After calling rbt_begin_iterate, call rbt_iterate repeatedly until it
 * returns NULL or the traversal stops being of interest.
 *
 * If the tree is changed during traversal, results of further calls to
 * rbt_iterate are unspecified.  Multiple concurrent iterators on the same
 * tree are allowed.
 *
 * The iterator state is stored in the 'iter' struct.  The caller should
 * treat it as an opaque struct.
 */
void
rbt_begin_iterate(RBTree *rbt, RBTOrderControl ctrl, RBTreeIterator *iter)
{
















}

/*
 * rbt_iterate: return the next node in traversal order, or NULL if no more
 */
RBTNode *
rbt_iterate(RBTreeIterator *iter)
{






}