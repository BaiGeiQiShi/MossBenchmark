/*-------------------------------------------------------------------------
 *
 * print.c
 *	  various print routines (used mostly for debugging)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/print.c
 *
 * HISTORY
 *	  AUTHOR			DATE			MAJOR EVENT
 *	  Andrew Yu			Oct 26, 1994	file creation
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/printtup.h"
#include "lib/stringinfo.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "nodes/print.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"

/*
 * print
 *	  print contents of Node to stdout
 */
void
print(const void *obj)
{









}

/*
 * pprint
 *	  pretty-print contents of Node to stdout
 */
void
pprint(const void *obj)
{









}

/*
 * elog_node_display
 *	  send pretty-printed contents of Node to postmaster log
 */
void
elog_node_display(int lev, const char *title, const void *obj, bool pretty)
{















}

/*
 * Format a nodeToString output for display on a terminal.
 *
 * The result is a palloc'd string.
 *
 * This version just tries to break at whitespace.
 */
char *
format_node_dump(const char *dump)
{


















































}

/*
 * Format a nodeToString output for display on a terminal.
 *
 * The result is a palloc'd string.
 *
 * This version tries to indent intelligently.
 */
char *
pretty_format_node_dump(const char *dump)
{










































































































}

/*
 * print_rt
 *	  print contents of range table
 */
void
print_rt(const List *rtable)
{













































}

/*
 * print_expr
 *	  print an expression
 */
void
print_expr(const Node *expr, const List *rtable)
{


































































































}

/*
 * print_pathkeys -
 *	  pathkeys list of PathKeys
 */
void
print_pathkeys(const List *pathkeys, const List *rtable)
{







































}

/*
 * print_tl
 *	  print targetlist in a more legible way.
 */
void
print_tl(const List *tlist, const List *rtable)
{




















}

/*
 * print_slot
 *	  print out the tuple with the given TupleTableSlot
 */
void
print_slot(TupleTableSlot *slot)
{












}
