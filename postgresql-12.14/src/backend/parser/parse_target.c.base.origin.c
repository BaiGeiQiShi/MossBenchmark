/*-------------------------------------------------------------------------
 *
 * parse_target.c
 *	  handle target lists
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_target.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/typcache.h"

static void
markTargetListOrigin(ParseState *pstate, TargetEntry *tle, Var *var, int levelsup);
static Node *
transformAssignmentIndirection(ParseState *pstate, Node *basenode, const char *targetName, bool targetIsSubscripting, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, ListCell *indirection, Node *rhs, int location);
static Node *
transformAssignmentSubscripts(ParseState *pstate, Node *basenode, const char *targetName, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, List *subscripts, bool isSlice, ListCell *next_indirection, Node *rhs, int location);
static List *
ExpandColumnRefStar(ParseState *pstate, ColumnRef *cref, bool make_target_entry);
static List *
ExpandAllTables(ParseState *pstate, int location);
static List *
ExpandIndirectionStar(ParseState *pstate, A_Indirection *ind, bool make_target_entry, ParseExprKind exprKind);
static List *
ExpandSingleTable(ParseState *pstate, RangeTblEntry *rte, int location, bool make_target_entry);
static List *
ExpandRowReference(ParseState *pstate, Node *expr, bool make_target_entry);
static int
FigureColnameInternal(Node *node, char **name);

/*
 * transformTargetEntry()
 *	Transform any ordinary "expression-type" node into a targetlist entry.
 *	This is exported so that parse_clause.c can generate targetlist entries
 *	for ORDER/GROUP BY items that are not already in the targetlist.
 *
 * node		the (untransformed) parse tree for the value expression.
 * expr		the transformed expression, or NULL if caller didn't do it yet.
 * exprKind expression kind (EXPR_KIND_SELECT_TARGET, etc)
 * colname	the column name to be assigned, or NULL if none yet set.
 * resjunk	true if the target should be marked resjunk, ie, it is not
 *			wanted in the final projected tuple.
 */
TargetEntry *
transformTargetEntry(ParseState *pstate, Node *node, Node *expr, ParseExprKind exprKind, char *colname, bool resjunk)
{




























}

/*
 * transformTargetList()
 * Turns a list of ResTarget's into a list of TargetEntry's.
 *
 * This code acts mostly the same for SELECT, UPDATE, or RETURNING lists;
 * the main thing is to transform the given expressions (the "val" fields).
 * The exprKind parameter distinguishes these cases when necessary.
 */
List *
transformTargetList(ParseState *pstate, List *targetlist, ParseExprKind exprKind)
{


































































}

/*
 * transformExpressionList()
 *
 * This is the identical transformation to transformTargetList, except that
 * the input list elements are bare expressions without ResTarget decoration,
 * and the output elements are likewise just expressions without TargetEntry
 * decoration.  Also, we don't expect any multiassign constructs within the
 * list, so there's nothing to do for that.  We use this for ROW() and
 * VALUES() constructs.
 *
 * exprKind is not enough to tell us whether to allow SetToDefault, so
 * an additional flag is needed for that.
 */
List *
transformExpressionList(ParseState *pstate, List *exprlist, ParseExprKind exprKind, bool allowDefault)
{




















































}

/*
 * resolveTargetListUnknowns()
 *		Convert any unknown-type targetlist entries to type TEXT.
 *
 * We do this after we've exhausted all other ways of identifying the output
 * column types of a query.
 */
void
resolveTargetListUnknowns(ParseState *pstate, List *targetlist)
{












}

/*
 * markTargetListOrigins()
 *		Mark targetlist columns that are simple Vars with the source
 *		table's OID and column number.
 *
 * Currently, this is done only for SELECT targetlists and RETURNING lists,
 * since we only need the info if we are going to send it to the frontend.
 */
void
markTargetListOrigins(ParseState *pstate, List *targetlist)
{








}

/*
 * markTargetListOrigin()
 *		If 'var' is a Var of a plain relation, mark 'tle' with its
 *origin
 *
 * levelsup is an extra offset to interpret the Var's varlevelsup correctly.
 *
 * This is split out so it can recurse for join references.  Note that we
 * do not drill down into views, but report the view as the column owner.
 */
static void
markTargetListOrigin(ParseState *pstate, TargetEntry *tle, Var *var, int levelsup)
{













































































}

/*
 * transformAssignedExpr()
 *	This is used in INSERT and UPDATE statements only.  It prepares an
 *	expression for assignment to a column of the target table.
 *	This includes coercing the given value to the target column's type
 *	(if necessary), and dealing with any subfield names or subscripts
 *	attached to the target column itself.  The input expression has
 *	already been through transformExpr().
 *
 * pstate		parse state
 * expr			expression to be modified
 * exprKind		indicates which type of statement we're dealing with
 * colname		target column name (ie, name of attribute to be assigned
 *to) attrno		target attribute number indirection	subscripts/field
 *names for target column, if any location		error cursor position
 *for the target column, or -1
 *
 * Returns the modified expression.
 *
 * Note: location points at the target column name (SET target or INSERT
 * column name list entry), and must therefore be -1 in an INSERT that
 * omits the column name list.  So we should usually prefer to use
 * exprLocation(expr) for errors that can happen in a default INSERT.
 */
Expr *
transformAssignedExpr(ParseState *pstate, Expr *expr, ParseExprKind exprKind, const char *colname, int attrno, List *indirection, int location)
{








































































































}

/*
 * updateTargetListEntry()
 *	This is used in UPDATE statements (and ON CONFLICT DO UPDATE)
 *	only.  It prepares an UPDATE TargetEntry for assignment to a
 *	column of the target table.  This includes coercing the given
 *	value to the target column's type (if necessary), and dealing with
 *	any subfield names or subscripts attached to the target column
 *	itself.
 *
 * pstate		parse state
 * tle			target list entry to be modified
 * colname		target column name (ie, name of attribute to be assigned
 *to) attrno		target attribute number indirection	subscripts/field
 *names for target column, if any location		error cursor position
 *(should point at column name), or -1
 */
void
updateTargetListEntry(ParseState *pstate, TargetEntry *tle, char *colname, int attrno, List *indirection, int location)
{











}

/*
 * Process indirection (field selection or subscripting) of the target
 * column in INSERT/UPDATE.  This routine recurses for multiple levels
 * of indirection --- but note that several adjacent A_Indices nodes in
 * the indirection list are treated as a single multidimensional subscript
 * operation.
 *
 * In the initial call, basenode is a Var for the target column in UPDATE,
 * or a null Const of the target's type in INSERT.  In recursive calls,
 * basenode is NULL, indicating that a substitute node should be consed up if
 * needed.
 *
 * targetName is the name of the field or subfield we're assigning to, and
 * targetIsSubscripting is true if we're subscripting it.  These are just for
 * error reporting.
 *
 * targetTypeId, targetTypMod, targetCollation indicate the datatype and
 * collation of the object to be assigned to (initially the target column,
 * later some subobject).
 *
 * indirection is the sublist remaining to process.  When it's NULL, we're
 * done recursing and can just coerce and return the RHS.
 *
 * rhs is the already-transformed value to be assigned; note it has not been
 * coerced to any particular type.
 *
 * location is the cursor error position for any errors.  (Note: this points
 * to the head of the target clause, eg "foo" in "foo.bar[baz]".  Later we
 * might want to decorate indirection cells with their own location info,
 * in which case the location argument could probably be dropped.)
 */
static Node *
transformAssignmentIndirection(ParseState *pstate, Node *basenode, const char *targetName, bool targetIsSubscripting, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, ListCell *indirection, Node *rhs, int location)
{





































































































































}

/*
 * helper for transformAssignmentIndirection: process container assignment
 */
static Node *
transformAssignmentSubscripts(ParseState *pstate, Node *basenode, const char *targetName, Oid targetTypeId, int32 targetTypMod, Oid targetCollation, List *subscripts, bool isSlice, ListCell *next_indirection, Node *rhs, int location)
{



















































}

/*
 * checkInsertTargets -
 *	  generate a list of INSERT column targets if not supplied, or
 *	  test supplied column names to make sure they are in target table.
 *	  Also return an integer list of the columns' attribute numbers.
 */
List *
checkInsertTargets(ParseState *pstate, List *cols, List **attrnos)
{


















































































}

/*
 * ExpandColumnRefStar()
 *		Transforms foo.* into a list of expressions or targetlist
 *entries.
 *
 * This handles the case where '*' appears as the last or only item in a
 * ColumnRef.  The code is shared between the case of foo.* at the top level
 * in a SELECT target list (where we want TargetEntry nodes in the result)
 * and foo.* in a ROW() or VALUES() construct (where we want just bare
 * expressions).
 *
 * The referenced columns are marked as requiring SELECT access.
 */
static List *
ExpandColumnRefStar(ParseState *pstate, ColumnRef *cref, bool make_target_entry)
{









































































































































}

/*
 * ExpandAllTables()
 *		Transforms '*' (in the target list) into a list of targetlist
 *entries.
 *
 * tlist entries are generated for each relation visible for unqualified
 * column name access.  We do not consider qualified-name-only entries because
 * that would include input tables of aliasless JOINs, NEW/OLD pseudo-entries,
 * etc.
 *
 * The referenced relations/columns are marked as requiring SELECT access.
 */
static List *
ExpandAllTables(ParseState *pstate, int location)
{

































}

/*
 * ExpandIndirectionStar()
 *		Transforms foo.* into a list of expressions or targetlist
 *entries.
 *
 * This handles the case where '*' appears as the last item in A_Indirection.
 * The code is shared between the case of foo.* at the top level in a SELECT
 * target list (where we want TargetEntry nodes in the result) and foo.* in
 * a ROW() or VALUES() construct (where we want just bare expressions).
 * For robustness, we use a separate "make_target_entry" flag to control
 * this rather than relying on exprKind.
 */
static List *
ExpandIndirectionStar(ParseState *pstate, A_Indirection *ind, bool make_target_entry, ParseExprKind exprKind)
{











}

/*
 * ExpandSingleTable()
 *		Transforms foo.* into a list of expressions or targetlist
 *entries.
 *
 * This handles the case where foo has been determined to be a simple
 * reference to an RTE, so we can just generate Vars for the expressions.
 *
 * The referenced columns are marked as requiring SELECT access.
 */
static List *
ExpandSingleTable(ParseState *pstate, RangeTblEntry *rte, int location, bool make_target_entry)
{


































}

/*
 * ExpandRowReference()
 *		Transforms foo.* into a list of expressions or targetlist
 *entries.
 *
 * This handles the case where foo is an arbitrary expression of composite
 * type.
 */
static List *
ExpandRowReference(ParseState *pstate, Node *expr, bool make_target_entry)
{

















































































}

/*
 * expandRecordVariable
 *		Get the tuple descriptor for a Var of type RECORD, if possible.
 *
 * Since no actual table or view column is allowed to have type RECORD, such
 * a Var must refer to a JOIN or FUNCTION RTE or to a subquery output.  We
 * drill down to find the ultimate defining expression and attempt to infer
 * the tupdesc from it.  We ereport if we can't determine the tupdesc.
 *
 * levelsup is an extra offset to interpret the Var's varlevelsup correctly.
 */
TupleDesc
expandRecordVariable(ParseState *pstate, Var *var, int levelsup)
{


























































































































































}

/*
 * FigureColname -
 *	  if the name of the resulting column is not specified in the target
 *	  list, we have to guess a suitable name.  The SQL spec provides some
 *	  guidance, but not much...
 *
 * Note that the argument is the *untransformed* parse tree for the target
 * item.  This is a shade easier to work with than the transformed tree.
 */
char *
FigureColname(Node *node)
{









}

/*
 * FigureIndexColname -
 *	  choose the name for an expression column in an index
 *
 * This is actually just like FigureColname, except we return NULL if
 * we can't pick a good name.
 */
char *
FigureIndexColname(Node *node)
{




}

/*
 * FigureColnameInternal -
 *	  internal workhorse for FigureColname
 *
 * Return value indicates strength of confidence in result:
 *		0 - no information
 *		1 - second-best name choice
 *		2 - good name choice
 * The return value is actually only used internally.
 * If the result isn't zero, *name is set to the chosen name.
 */
static int
FigureColnameInternal(Node *node, char **name)
{




















































































































































































































































}