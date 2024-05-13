/*-------------------------------------------------------------------------
 *
 * tsquery_util.c
 *	  Utilities for tsquery datatype
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsquery_util.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "tsearch/ts_utils.h"
#include "miscadmin.h"

/*
 * Build QTNode tree for a tsquery given in QueryItem array format.
 */
QTNode *
QT2QTN(QueryItem *in, char *operand)
{






























}

/*
 * Free a QTNode tree.
 *
 * Referenced "word" and "valnode" items are freed if marked as transient
 * by flags.
 */
void
QTNFree(QTNode *in)
{

































}

/*
 * Sort comparator for QTNodes.
 *
 * The sort order is somewhat arbitrary.
 */
int
QTNodeCompare(QTNode *an, QTNode *bn)
{



























































}

/*
 * qsort comparator for QTNode pointers.
 */
static int
cmpQTN(const void *a, const void *b)
{

}

/*
 * Canonicalize a QTNode tree by sorting the children of AND/OR nodes
 * into an arbitrary but well-defined order.
 */
void
QTNSort(QTNode *in)
{


















}

/*
 * Are two QTNode trees equal according to QTNodeCompare?
 */
bool
QTNEq(QTNode *a, QTNode *b)
{








}

/*
 * Remove unnecessary intermediate nodes. For example:
 *
 *	OR			OR
 * a  OR	-> a b c
 *	 b	c
 */
void
QTNTernary(QTNode *in)
{















































}

/*
 * Convert a tree to binary tree by inserting intermediate nodes.
 * (Opposite of QTNTernary)
 */
void
QTNBinary(QTNode *in)
{




































}

/*
 * Count the total length of operand strings in tree (including '\0'-
 * terminators) and the total number of nodes.
 * Caller must initialize *sumlen and *nnode to zeroes.
 */
static void
cntsize(QTNode *in, int *sumlen, int *nnode)
{

















}

typedef struct
{
  QueryItem *curitem;
  char *operand;
  char *curoperand;
} QTN2QTState;

/*
 * Recursively convert a QTNode tree into flat tsquery format.
 * Caller must have allocated arrays of the correct size.
 */
static void
fillQT(QTN2QTState *state, QTNode *in)
{
































}

/*
 * Build flat tsquery from a QTNode tree.
 */
TSQuery
QTN2QT(QTNode *in)
{






















}

/*
 * Copy a QTNode tree.
 *
 * Modifiable copies of the words and valnodes are made, too.
 */
QTNode *
QTNCopy(QTNode *in)
{
































}

/*
 * Clear the specified flag bit(s) in all nodes of a QTNode tree.
 */
void
QTNClearFlags(QTNode *in, uint32 flags)
{














}