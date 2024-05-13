/*-------------------------------------------------------------------------
 *
 * analyze.c
 *	  transform the raw parse tree into a query tree
 *
 * For optimizable statements, we are careful to obtain a suitable lock on
 * each referenced table, and other modules of the backend preserve or
 * re-obtain these locks before depending on the results.  It is therefore
 * okay to do significant semantic analysis of these statements.  For
 * utility commands, no locks are obtained here (and if they were, we could
 * not be sure we'd still have them at execution).  Hence the general rule
 * for utility commands is to just dump them into a Query node untransformed.
 * DECLARE CURSOR, EXPLAIN, and CREATE TABLE AS are exceptions because they
 * contain optimizable statements, which we should transform.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/parser/analyze.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_cte.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_param.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "utils/rel.h"

/* Hook for plugins to get control at end of parse analysis */
post_parse_analyze_hook_type post_parse_analyze_hook = NULL;

static Query *
transformOptionalSelectInto(ParseState *pstate, Node *parseTree);
static Query *
transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
static Query *
transformInsertStmt(ParseState *pstate, InsertStmt *stmt);
static List *
transformInsertRow(ParseState *pstate, List *exprlist, List *stmtcols, List *icolumns, List *attrnos, bool strip_indirection);
static OnConflictExpr *
transformOnConflictClause(ParseState *pstate, OnConflictClause *onConflictClause);
static int
count_rowexpr_columns(ParseState *pstate, Node *expr);
static Query *
transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
static Query *
transformValuesClause(ParseState *pstate, SelectStmt *stmt);
static Query *
transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt);
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt, bool isTopLevel, List **targetlist);
static void
determineRecursiveColTypes(ParseState *pstate, Node *larg, List *nrtargetlist);
static Query *
transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
static List *
transformReturningList(ParseState *pstate, List *returningList);
static List *
transformUpdateTargetList(ParseState *pstate, List *targetList);
static Query *
transformDeclareCursorStmt(ParseState *pstate, DeclareCursorStmt *stmt);
static Query *
transformExplainStmt(ParseState *pstate, ExplainStmt *stmt);
static Query *
transformCreateTableAsStmt(ParseState *pstate, CreateTableAsStmt *stmt);
static Query *
transformCallStmt(ParseState *pstate, CallStmt *stmt);
static void
transformLockingClause(ParseState *pstate, Query *qry, LockingClause *lc, bool pushedDown);
#ifdef RAW_EXPRESSION_COVERAGE_TEST
static bool
test_raw_expression_coverage(Node *node, void *context);
#endif

/*
 * parse_analyze
 *		Analyze a raw parse tree and transform it to Query form.
 *
 * Optionally, information about $n parameter types can be supplied.
 * References to $n indexes not defined by paramTypes[] are disallowed.
 *
 * The result is a Query node.  Optimizable statements require considerable
 * transformation, while utility-type statements are simply hung off
 * a dummy CMD_UTILITY Query node.
 */
Query *
parse_analyze(RawStmt *parseTree, const char *sourceText, Oid *paramTypes, int numParams, QueryEnvironment *queryEnv)
{
























}

/*
 * parse_analyze_varparams
 *
 * This variant is used when it's okay to deduce information about $n
 * symbol datatypes from context.  The passed-in paramTypes[] array can
 * be modified or enlarged (via repalloc).
 */
Query *
parse_analyze_varparams(RawStmt *parseTree, const char *sourceText, Oid **paramTypes, int *numParams)
{






















}

/*
 * parse_sub_analyze
 *		Entry point for recursively analyzing a sub-statement.
 */
Query *
parse_sub_analyze(Node *parseTree, ParseState *parentParseState, CommonTableExpr *parentCTE, bool locked_from_parent, bool resolve_unknowns)
{












}

/*
 * transformTopLevelStmt -
 *	  transform a Parse tree into a Query tree.
 *
 * This function is just responsible for transferring statement location data
 * from the RawStmt into the finished Query.
 */
Query *
transformTopLevelStmt(ParseState *pstate, RawStmt *parseTree)
{









}

/*
 * transformOptionalSelectInto -
 *	  If SELECT has INTO, convert it to CREATE TABLE AS.
 *
 * The only thing we do here that we don't do in transformStmt() is to
 * convert SELECT ... INTO into CREATE TABLE AS.  Since utility statements
 * aren't allowed within larger statements, this is only allowed at the top
 * of the parse tree, and so we only try it before entering the recursive
 * transformStmt() processing.
 */
static Query *
transformOptionalSelectInto(ParseState *pstate, Node *parseTree)
{
































}

/*
 * transformStmt -
 *	  recursively transform a Parse tree into a Query tree.
 */
Query *
transformStmt(ParseState *pstate, Node *parseTree)
{





























































































}

/*
 * analyze_requires_snapshot
 *		Returns true if a snapshot must be set before doing parse
 *analysis on the given raw parse tree.
 *
 * Classification here should match transformStmt().
 */
bool
analyze_requires_snapshot(RawStmt *parseTree)
{































}

/*
 * transformDeleteStmt -
 *	  transforms a Delete Statement
 */
static Query *
transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt)
{





























































}

/*
 * transformInsertStmt -
 *	  transform an Insert Statement
 */
static Query *
transformInsertStmt(ParseState *pstate, InsertStmt *stmt)
{



























































































































































































































































































































































































}

/*
 * Prepare an INSERT row for assignment to the target table.
 *
 * exprlist: transformed expressions for source values; these might come from
 * a VALUES row, or be Vars referencing a sub-SELECT or VALUES RTE output.
 * stmtcols: original target-columns spec for INSERT (we just test for NIL)
 * icolumns: effective target-columns spec (list of ResTarget)
 * attrnos: integer column numbers (must be same length as icolumns)
 * strip_indirection: if true, remove any field/array assignment nodes
 */
static List *
transformInsertRow(ParseState *pstate, List *exprlist, List *stmtcols, List *icolumns, List *attrnos, bool strip_indirection)
{










































































}

/*
 * transformOnConflictClause -
 *	  transforms an OnConflictClause in an INSERT
 */
static OnConflictExpr *
transformOnConflictClause(ParseState *pstate, OnConflictClause *onConflictClause)
{




































































}

/*
 * BuildOnConflictExcludedTargetlist
 *		Create target list for the EXCLUDED pseudo-relation of ON
 *CONFLICT, representing the columns of targetrel with varno exclRelIndex.
 *
 * Note: Exported for use in the rewriter.
 */
List *
BuildOnConflictExcludedTargetlist(Relation targetrel, Index exclRelIndex)
{














































}

/*
 * count_rowexpr_columns -
 *	  get number of columns contained in a ROW() expression;
 *	  return -1 if expression isn't a RowExpr or a Var referencing one.
 *
 * This is currently used only for hint purposes, so we aren't terribly
 * tense about recognizing all possible cases.  The Var case is interesting
 * because that's what we'll get in the INSERT ... SELECT (...) case.
 */
static int
count_rowexpr_columns(ParseState *pstate, Node *expr)
{




































}

/*
 * transformSelectStmt -
 *	  transforms a Select Statement
 *
 * Note: this covers only cases with no set operations and no VALUES lists;
 * see below for the other cases.
 */
static Query *
transformSelectStmt(ParseState *pstate, SelectStmt *stmt)
{








































































































}

/*
 * transformValuesClause -
 *	  transforms a VALUES clause that's being used as a standalone SELECT
 *
 * We build a Query containing a VALUES RTE, rather as if one had written
 *			SELECT * FROM (VALUES ...) AS "*VALUES*"
 */
static Query *
transformValuesClause(ParseState *pstate, SelectStmt *stmt)
{










































































































































































































}

/*
 * transformSetOperationStmt -
 *	  transforms a set-operations tree
 *
 * A set-operation tree is just a SELECT, but with UNION/INTERSECT/EXCEPT
 * structure to it.  We must transform each leaf SELECT and build up a top-
 * level Query that contains the leaf SELECTs as subqueries in its rangetable.
 * The tree of set operations is converted into the setOperations field of
 * the top-level Query.
 */
static Query *
transformSetOperationStmt(ParseState *pstate, SelectStmt *stmt)
{




























































































































































































}

/*
 * transformSetOperationTree
 *		Recursively transform leaves and internal nodes of a set-op tree
 *
 * In addition to returning the transformed node, if targetlist isn't NULL
 * then we return a list of its non-resjunk TargetEntry nodes.  For a leaf
 * set-op node these are the actual targetlist entries; otherwise they are
 * dummy entries created to carry the type, typmod, collation, and location
 * (for error messages) of each output column of the set-op node.  This info
 * is needed only during the internal recursion of this function, so outside
 * callers pass NULL for targetlist.  Note: the reason for passing the
 * actual targetlist entries of a leaf node is so that upper levels can
 * replace UNKNOWN Consts with properly-coerced constants.
 */
static Node *
transformSetOperationTree(ParseState *pstate, SelectStmt *stmt, bool isTopLevel, List **targetlist)
{























































































































































































































































































































}

/*
 * Process the outputs of the non-recursive term of a recursive union
 * to set up the parent CTE's columns
 */
static void
determineRecursiveColTypes(ParseState *pstate, Node *larg, List *nrtargetlist)
{











































}

/*
 * transformUpdateStmt -
 *	  transforms an update statement
 */
static Query *
transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt)
{





















































}

/*
 * transformUpdateTargetList -
 *	handle SET clause in UPDATE/INSERT ... ON CONFLICT UPDATE
 */
static List *
transformUpdateTargetList(ParseState *pstate, List *origTlist)
{




























































}

/*
 * transformReturningList -
 *	handle a RETURNING clause in INSERT/UPDATE/DELETE
 */
static List *
transformReturningList(ParseState *pstate, List *returningList)
{










































}

/*
 * transformDeclareCursorStmt -
 *	transform a DECLARE CURSOR Statement
 *
 * DECLARE CURSOR is like other utility statements in that we emit it as a
 * CMD_UTILITY Query node; however, we must first transform the contained
 * query.  We used to postpone that until execution, but it's really necessary
 * to do it during the normal parse analysis phase to ensure that side effects
 * of parser hooks happen at the expected time.
 */
static Query *
transformDeclareCursorStmt(ParseState *pstate, DeclareCursorStmt *stmt)
{























































}

/*
 * transformExplainStmt -
 *	transform an EXPLAIN Statement
 *
 * EXPLAIN is like other utility statements in that we emit it as a
 * CMD_UTILITY Query node; however, we must first transform the contained
 * query.  We used to postpone that until execution, but it's really necessary
 * to do it during the normal parse analysis phase to ensure that side effects
 * of parser hooks happen at the expected time.
 */
static Query *
transformExplainStmt(ParseState *pstate, ExplainStmt *stmt)
{











}

/*
 * transformCreateTableAsStmt -
 *	transform a CREATE TABLE AS, SELECT ... INTO, or CREATE MATERIALIZED
 *VIEW Statement
 *
 * As with DECLARE CURSOR and EXPLAIN, transform the contained statement now.
 */
static Query *
transformCreateTableAsStmt(ParseState *pstate, CreateTableAsStmt *stmt)
{



































































}

/*
 * transform a CallStmt
 *
 * We need to do parse analysis on the procedure call and its arguments.
 */
static Query *
transformCallStmt(ParseState *pstate, CallStmt *stmt)
{






















}

/*
 * Produce a string representation of a LockClauseStrength value.
 * This should only be applied to valid values (not LCS_NONE).
 */
const char *
LCS_asString(LockClauseStrength strength)
{















}

/*
 * Check for features that are not supported with FOR [KEY] UPDATE/SHARE.
 *
 * exported so planner can check again after rewriting, query pullup, etc
 */
void
CheckSelectLocking(Query *qry, LockClauseStrength strength)
{






























}

/*
 * Transform a FOR [KEY] UPDATE/SHARE clause
 *
 * This basically involves replacing names by integer relids.
 *
 * NB: if you need to change this, see also markQueryForLocking()
 * in rewriteHandler.c, and isLockedRefname() in parse_relation.c.
 */
static void
transformLockingClause(ParseState *pstate, Query *qry, LockingClause *lc, bool pushedDown)
{















































































































































}

/*
 * Record locking info for a single rangetable item
 */
void
applyLockingClause(Query *qry, Index rtindex, LockClauseStrength strength, LockWaitPolicy waitPolicy, bool pushedDown)
{













































}

/*
 * Coverage testing for raw_expression_tree_walker().
 *
 * When enabled, we run raw_expression_tree_walker() over every DML statement
 * submitted to parse analysis.  Without this provision, that function is only
 * applied in limited cases involving CTEs, and we don't really want to have
 * to test everything inside as well as outside a CTE.
 */
#ifdef RAW_EXPRESSION_COVERAGE_TEST

static bool
test_raw_expression_coverage(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  return raw_expression_tree_walker(node, test_raw_expression_coverage, context);
}

#endif /* RAW_EXPRESSION_COVERAGE_TEST */