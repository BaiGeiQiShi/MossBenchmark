/*-------------------------------------------------------------------------
 *
 * parse_cte.c
 *	  handle CTEs (common table expressions) in parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_cte.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "parser/analyze.h"
#include "parser/parse_cte.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

/* Enumeration of contexts in which a self-reference is disallowed */
typedef enum
{
  RECURSION_OK,
  RECURSION_NONRECURSIVETERM, /* inside the left-hand term */
  RECURSION_SUBLINK,          /* inside a sublink */
  RECURSION_OUTERJOIN,        /* inside nullable side of an outer join */
  RECURSION_INTERSECT,        /* underneath INTERSECT (ALL) */
  RECURSION_EXCEPT            /* underneath EXCEPT (ALL) */
} RecursionContext;

/* Associated error messages --- each must have one %s for CTE name */
static const char *const recursion_errormsgs[] = {
    /* RECURSION_OK */
    NULL,
    /* RECURSION_NONRECURSIVETERM */
    gettext_noop("recursive reference to query \"%s\" must not appear within its non-recursive term"),
    /* RECURSION_SUBLINK */
    gettext_noop("recursive reference to query \"%s\" must not appear within a subquery"),
    /* RECURSION_OUTERJOIN */
    gettext_noop("recursive reference to query \"%s\" must not appear within an outer join"),
    /* RECURSION_INTERSECT */
    gettext_noop("recursive reference to query \"%s\" must not appear within INTERSECT"),
    /* RECURSION_EXCEPT */
    gettext_noop("recursive reference to query \"%s\" must not appear within EXCEPT")};

/*
 * For WITH RECURSIVE, we have to find an ordering of the clause members
 * with no forward references, and determine which members are recursive
 * (i.e., self-referential).  It is convenient to do this with an array
 * of CteItems instead of a list of CommonTableExprs.
 */
typedef struct CteItem
{
  CommonTableExpr *cte;  /* One CTE to examine */
  int id;                /* Its ID number for dependencies */
  Bitmapset *depends_on; /* CTEs depended on (not including self) */
} CteItem;

/* CteState is what we need to pass around in the tree walkers */
typedef struct CteState
{
  /* global state: */
  ParseState *pstate; /* global parse state */
  CteItem *items;     /* array of CTEs and extra data */
  int numitems;       /* number of CTEs */
  /* working state during a tree walk: */
  int curitem;      /* index of item currently being examined */
  List *innerwiths; /* list of lists of CommonTableExpr */
  /* working state for checkWellFormedRecursion walk only: */
  int selfrefcount;         /* number of self-references detected */
  RecursionContext context; /* context to allow or disallow self-ref */
} CteState;

static void
analyzeCTE(ParseState *pstate, CommonTableExpr *cte);

/* Dependency processing functions */
static void
makeDependencyGraph(CteState *cstate);
static bool
makeDependencyGraphWalker(Node *node, CteState *cstate);
static void
TopologicalSort(ParseState *pstate, CteItem *items, int numitems);

/* Recursion validity checker functions */
static void
checkWellFormedRecursion(CteState *cstate);
static bool
checkWellFormedRecursionWalker(Node *node, CteState *cstate);
static void
checkWellFormedSelectStmt(SelectStmt *stmt, CteState *cstate);

/*
 * transformWithClause -
 *	  Transform the list of WITH clause "common table expressions" into
 *	  Query nodes.
 *
 * The result is the list of transformed CTEs to be put into the output
 * Query.  (This is in fact the same as the ending value of p_ctenamespace,
 * but it seems cleaner to not expose that in the function's API.)
 */
List *
transformWithClause(ParseState *pstate, WithClause *withClause)
{





















































































































}

/*
 * Perform the actual parse analysis transformation of one CTE.  All
 * CTEs it depends on have already been loaded into pstate->p_ctenamespace,
 * and have been marked with the correct output column names/types.
 */
static void
analyzeCTE(ParseState *pstate, CommonTableExpr *cte)
{


























































































}

/*
 * Compute derived fields of a CTE, given the transformed output targetlist
 *
 * For a nonrecursive CTE, this is called after transforming the CTE's query.
 * For a recursive CTE, we call it after transforming the non-recursive term,
 * and pass the targetlist emitted by the non-recursive term only.
 *
 * Note: in the recursive case, the passed pstate is actually the one being
 * used to analyze the CTE's query, so it is one level lower down than in
 * the nonrecursive case.  This doesn't matter since we only use it for
 * error message context anyway.
 */
void
analyzeCTETargetList(ParseState *pstate, CommonTableExpr *cte, List *tlist)
{





































































}

/*
 * Identify the cross-references of a list of WITH RECURSIVE items,
 * and sort into an order that has no forward references.
 */
static void
makeDependencyGraph(CteState *cstate)
{













}

/*
 * Tree walker function to detect cross-references and self-references of the
 * CTEs in a WITH RECURSIVE list.
 */
static bool
makeDependencyGraphWalker(Node *node, CteState *cstate)
{



















































































































}

/*
 * Sort by dependencies, using a standard topological sort operation
 */
static void
TopologicalSort(ParseState *pstate, CteItem *items, int numitems)
{










































}

/*
 * Check that recursive queries are well-formed.
 */
static void
checkWellFormedRecursion(CteState *cstate)
{


















































































}

/*
 * Tree walker function to detect invalid self-references in a recursive query.
 */
static bool
checkWellFormedRecursionWalker(Node *node, CteState *cstate)
{













































































































































































}

/*
 * subroutine for checkWellFormedRecursionWalker: process a SelectStmt
 * without worrying about its WITH clause
 */
static void
checkWellFormedSelectStmt(SelectStmt *stmt, CteState *cstate)
{
















































}