/*-------------------------------------------------------------------------
 *
 * execCurrent.c
 *	  executor support for WHERE CURRENT OF cursor
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/executor/execCurrent.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/portal.h"
#include "utils/rel.h"

static char *
fetch_cursor_param_value(ExprContext *econtext, int paramId);
static ScanState *
search_plan_tree(PlanState *node, Oid table_oid, bool *pending_rescan);

/*
 * execCurrentOf
 *
 * Given a CURRENT OF expression and the OID of a table, determine which row
 * of the table is currently being scanned by the cursor named by CURRENT OF,
 * and return the row's TID into *current_tid.
 *
 * Returns true if a row was identified.  Returns false if the cursor is valid
 * for the table but is not currently scanning a row of the table (this is a
 * legal situation in inheritance cases).  Raises error if cursor is not a
 * valid updatable scan of the specified table.
 */
bool
execCurrentOf(CurrentOfExpr *cexpr, ExprContext *econtext, Oid table_oid, ItemPointer current_tid)
{




































































































































































































}

/*
 * fetch_cursor_param_value
 *
 * Fetch the string value of a param, verifying it is of type REFCURSOR.
 */
static char *
fetch_cursor_param_value(ExprContext *econtext, int paramId)
{
































}

/*
 * search_plan_tree
 *
 * Search through a PlanState tree for a scan node on the specified table.
 * Return NULL if not found or multiple candidates.
 *
 * CAUTION: this function is not charged simply with finding some candidate
 * scan, but with ensuring that that scan returned the plan tree's current
 * output row.  That's why we must reject multiple-match cases.
 *
 * If a candidate is found, set *pending_rescan to true if that candidate
 * or any node above it has a pending rescan action, i.e. chgParam != NULL.
 * That indicates that we shouldn't consider the node to be positioned on a
 * valid tuple, even if its own state would indicate that it is.  (Caller
 * must initialize *pending_rescan to false, and should not trust its state
 * if multiple candidates are found.)
 */
static ScanState *
search_plan_tree(PlanState *node, Oid table_oid, bool *pending_rescan)
{















































































































}