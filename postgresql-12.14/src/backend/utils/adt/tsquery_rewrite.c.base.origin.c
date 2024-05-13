/*-------------------------------------------------------------------------
 *
 * tsquery_rewrite.c
 *	  Utilities for reconstructing tsquery
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsquery_rewrite.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

/*
 * If "node" is equal to "ex", return a copy of "subs" instead.
 * If "ex" matches a subset of node's children, return a modified version
 * of "node" in which those children are replaced with a copy of "subs".
 * Otherwise return "node" unmodified.
 *
 * The QTN_NOCHANGE bit is set in successfully modified nodes, so that
 * we won't uselessly recurse into them.
 * Also, set *isfind true if we make a replacement.
 */
static QTNode *
findeq(QTNode *node, QTNode *ex, QTNode *subs, bool *isfind)
{



































































































































































}

/*
 * Recursive guts of findsubquery(): attempt to replace "ex" with "subs"
 * at the root node, and if we failed to do so, recursively match against
 * child nodes.
 *
 * Delete any void subtrees resulting from the replacement.
 * In the following example '5' is replaced by empty operand:
 *
 *	  AND		->	  6
 *	 /	 \
 *	5	 OR
 *		/  \
 *	   6	5
 */
static QTNode *
dofindsubquery(QTNode *root, QTNode *ex, QTNode *subs, bool *isfind)
{
















































}

/*
 * Substitute "subs" for "ex" throughout the QTNode tree at root.
 *
 * If isfind isn't NULL, set *isfind to show whether we made any substitution.
 *
 * Both "root" and "ex" must have been through QTNTernary and QTNSort
 * to ensure reliable matching.
 */
QTNode *
findsubquery(QTNode *root, QTNode *ex, QTNode *subs, bool *isfind)
{










}

Datum
tsquery_rewrite_query(PG_FUNCTION_ARGS)
{









































































































































}

Datum
tsquery_rewrite(PG_FUNCTION_ARGS)
{


















































}