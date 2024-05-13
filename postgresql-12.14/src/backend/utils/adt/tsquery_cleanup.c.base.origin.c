/*-------------------------------------------------------------------------
 *
 * tsquery_cleanup.c
 *	 Cleanup query from NOT values and/or stopword
 *	 Utility functions to correct work.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsquery_cleanup.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "tsearch/ts_utils.h"
#include "miscadmin.h"

typedef struct NODE
{
  struct NODE *left;
  struct NODE *right;
  QueryItem *valnode;
} NODE;

/*
 * make query tree from plain view of query
 */
static NODE *
maketree(QueryItem *in)
{
















}

/*
 * Internal state for plaintree and plainnode
 */
typedef struct
{
  QueryItem *ptr;
  int len; /* allocated size of ptr */
  int cur; /* number of elements in ptr */
} PLAINTREE;

static void
plainnode(PLAINTREE *state, NODE *node)
{





























}

/*
 * make plain view of tree from a NODE-tree representation
 */
static QueryItem *
plaintree(NODE *root, int *len)
{















}

static void
freetree(NODE *node)
{
















}

/*
 * clean tree for ! operator.
 * It's useful for debug, but in
 * other case, such view is used with search in index.
 * Operator ! always return TRUE
 */
static NODE *
clean_NOT_intree(NODE *node)
{

















































}

QueryItem *
clean_NOT(QueryItem *ptr, int *len)
{



}

/*
 * Remove QI_VALSTOP (stopword) nodes from query tree.
 *
 * Returns NULL if the query degenerates to nothing.  Input must not be NULL.
 *
 * When we remove a phrase operator due to removing one or both of its
 * arguments, we might need to adjust the distance of a parent phrase
 * operator.  For example, 'a' is a stopword, so:
 *		(b <-> a) <-> c  should become	b <2> c
 *		b <-> (a <-> c)  should become	b <2> c
 *		(b <-> (a <-> a)) <-> c  should become	b <3> c
 *		b <-> ((a <-> a) <-> c)  should become	b <3> c
 * To handle that, we define two output parameters:
 *		ladd: amount to add to a phrase distance to the left of this
 *node radd: amount to add to a phrase distance to the right of this node We
 *need two outputs because we could need to bubble up adjustments to two
 * different parent phrase operators.  Consider
 *		w <-> (((a <-> x) <2> (y <3> a)) <-> z)
 * After we've removed the two a's and are considering the <2> node (which is
 * now just x <2> y), we have an ladd distance of 1 that needs to propagate
 * up to the topmost (leftmost) <->, and an radd distance of 3 that needs to
 * propagate to the rightmost <->, so that we'll end up with
 *		w <2> ((x <2> y) <4> z)
 * Near the bottom of the tree, we may have subtrees consisting only of
 * stopwords.  The distances of any phrase operators within such a subtree are
 * summed and propagated to both ladd and radd, since we don't know which side
 * of the lowest surviving phrase operator we are in.  The rule is that any
 * subtree that degenerates to NULL must return equal values of ladd and radd,
 * and the parent node dealing with it should incorporate only one of those.
 *
 * Currently, we only implement this adjustment for adjacent phrase operators.
 * Thus for example 'x <-> ((a <-> y) | z)' will become 'x <-> (y | z)', which
 * isn't ideal, but there is no way to represent the really desired semantics
 * without some redesign of the tsquery structure.  Certainly it would not be
 * any better to convert that to 'x <2> (y | z)'.  Since this is such a weird
 * corner case, let it go for now.  But we can fix it in cases where the
 * intervening non-phrase operator also gets removed, for example
 * '((x <-> a) | a) <-> y' will become 'x <2> y'.
 */
static NODE *
clean_stopword_intree(NODE *node, int *ladd, int *radd)
{























































































































}

/*
 * Number of elements in query tree
 */
static int32
calcstrlen(NODE *node)
{


















}

/*
 * Remove QI_VALSTOP (stopword) nodes from TSQuery.
 */
TSQuery
cleanup_tsquery_stopwords(TSQuery in)
{























































}