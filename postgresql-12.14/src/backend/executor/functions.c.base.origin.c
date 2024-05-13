/*-------------------------------------------------------------------------
 *
 * functions.c
 *	  Execution of SQL-language functions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/functions.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/functions.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "storage/proc.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/*
 * Specialized DestReceiver for collecting query output in a SQL function
 */
typedef struct
{
  DestReceiver pub;        /* publicly-known function pointers */
  Tuplestorestate *tstore; /* where to put result tuples */
  MemoryContext cxt;       /* context containing tstore */
  JunkFilter *filter;      /* filter to convert tuple type */
} DR_sqlfunction;

/*
 * We have an execution_state record for each query in a function.  Each
 * record contains a plantree for its query.  If the query is currently in
 * F_EXEC_RUN state then there's a QueryDesc too.
 *
 * The "next" fields chain together all the execution_state records generated
 * from a single original parsetree.  (There will only be more than one in
 * case of rule expansion of the original parsetree.)
 */
typedef enum
{
  F_EXEC_START,
  F_EXEC_RUN,
  F_EXEC_DONE
} ExecStatus;

typedef struct execution_state
{
  struct execution_state *next;
  ExecStatus status;
  bool setsResult;   /* true if this query produces func's result */
  bool lazyEval;     /* true if should fetch one row at a time */
  PlannedStmt *stmt; /* plan for this query */
  QueryDesc *qd;     /* null unless status == RUN */
} execution_state;

/*
 * An SQLFunctionCache record is built during the first call,
 * and linked to from the fn_extra field of the FmgrInfo struct.
 *
 * Note that currently this has only the lifespan of the calling query.
 * Someday we should rewrite this code to use plancache.c to save parse/plan
 * results for longer than that.
 *
 * Physically, though, the data has the lifespan of the FmgrInfo that's used
 * to call the function, and there are cases (particularly with indexes)
 * where the FmgrInfo might survive across transactions.  We cannot assume
 * that the parse/plan trees are good for longer than the (sub)transaction in
 * which parsing was done, so we must mark the record with the LXID/subxid of
 * its creation time, and regenerate everything if that's obsolete.  To avoid
 * memory leakage when we do have to regenerate things, all the data is kept
 * in a sub-context of the FmgrInfo's fn_mcxt.
 */
typedef struct
{
  char *fname; /* function name (for error msgs) */
  char *src;   /* function body text (for error msgs) */

  SQLFunctionParseInfoPtr pinfo; /* data for parser callback hooks */

  Oid rettype;        /* actual return type */
  int16 typlen;       /* length of the return type */
  bool typbyval;      /* true if return type is pass by value */
  bool returnsSet;    /* true if returning multiple rows */
  bool returnsTuple;  /* true if returning whole tuple result */
  bool shutdown_reg;  /* true if registered shutdown callback */
  bool readonly_func; /* true to run in "read only" mode */
  bool lazyEval;      /* true if using lazyEval for result query */

  ParamListInfo paramLI; /* Param list representing current args */

  Tuplestorestate *tstore; /* where we accumulate result tuples */

  JunkFilter *junkFilter; /* will be NULL if function returns VOID */

  /*
   * func_state is a List of execution_state records, each of which is the
   * first for its original parsetree, with any additional records chained
   * to it via the "next" fields.  This sublist structure is needed to keep
   * track of where the original query boundaries are.
   */
  List *func_state;

  MemoryContext fcontext; /* memory context holding this struct and all
                           * subsidiary data */

  LocalTransactionId lxid; /* lxid in which cache was made */
  SubTransactionId subxid; /* subxid in which cache was made */
} SQLFunctionCache;

typedef SQLFunctionCache *SQLFunctionCachePtr;

/*
 * Data structure needed by the parser callback hooks to resolve parameter
 * references during parsing of a SQL function's body.  This is separate from
 * SQLFunctionCache since we sometimes do parsing separately from execution.
 */
typedef struct SQLFunctionParseInfo
{
  char *fname;     /* function's name */
  int nargs;       /* number of input arguments */
  Oid *argtypes;   /* resolved types of input arguments */
  char **argnames; /* names of input arguments; NULL if none */
  /* Note that argnames[i] can be NULL, if some args are unnamed */
  Oid collation; /* function's input collation, if known */
} SQLFunctionParseInfo;

/* non-export function prototypes */
static Node *
sql_fn_param_ref(ParseState *pstate, ParamRef *pref);
static Node *
sql_fn_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var);
static Node *
sql_fn_make_param(SQLFunctionParseInfoPtr pinfo, int paramno, int location);
static Node *
sql_fn_resolve_param_name(SQLFunctionParseInfoPtr pinfo, const char *paramname, int location);
static List *
init_execution_state(List *queryTree_list, SQLFunctionCachePtr fcache, bool lazyEvalOK);
static void
init_sql_fcache(FmgrInfo *finfo, Oid collation, bool lazyEvalOK);
static void
postquel_start(execution_state *es, SQLFunctionCachePtr fcache);
static bool
postquel_getnext(execution_state *es, SQLFunctionCachePtr fcache);
static void
postquel_end(execution_state *es);
static void
postquel_sub_params(SQLFunctionCachePtr fcache, FunctionCallInfo fcinfo);
static Datum
postquel_get_single_result(TupleTableSlot *slot, FunctionCallInfo fcinfo, SQLFunctionCachePtr fcache, MemoryContext resultcontext);
static void
sql_exec_error_callback(void *arg);
static void
ShutdownSQLFunction(Datum arg);
static void
sqlfunction_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static bool
sqlfunction_receive(TupleTableSlot *slot, DestReceiver *self);
static void
sqlfunction_shutdown(DestReceiver *self);
static void
sqlfunction_destroy(DestReceiver *self);

/*
 * Prepare the SQLFunctionParseInfo struct for parsing a SQL function body
 *
 * This includes resolving actual types of polymorphic arguments.
 *
 * call_expr can be passed as NULL, but then we will fail if there are any
 * polymorphic arguments.
 */
SQLFunctionParseInfoPtr
prepare_sql_fn_parse_info(HeapTuple procedureTuple, Node *call_expr, Oid inputCollation)
{















































































}

/*
 * Parser setup hook for parsing a SQL function body.
 */
void
sql_fn_parser_setup(struct ParseState *pstate, SQLFunctionParseInfoPtr pinfo)
{





}

/*
 * sql_fn_post_column_ref		parser callback for ColumnRefs
 */
static Node *
sql_fn_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var)
{


















































































































}

/*
 * sql_fn_param_ref		parser callback for ParamRefs ($n symbols)
 */
static Node *
sql_fn_param_ref(ParseState *pstate, ParamRef *pref)
{










}

/*
 * sql_fn_make_param		construct a Param node for the given paramno
 */
static Node *
sql_fn_make_param(SQLFunctionParseInfoPtr pinfo, int paramno, int location)
{





















}

/*
 * Search for a function parameter of the given name; if there is one,
 * construct and return a Param node for it.  If not, return NULL.
 * Helper function for sql_fn_post_column_ref.
 */
static Node *
sql_fn_resolve_param_name(SQLFunctionParseInfoPtr pinfo, const char *paramname, int location)
{
















}

/*
 * Set up the per-query execution_state records for a SQL function.
 *
 * The input is a List of Lists of parsed and rewritten, but not planned,
 * querytrees.  The sublist structure denotes the original query boundaries.
 */
static List *
init_execution_state(List *queryTree_list, SQLFunctionCachePtr fcache, bool lazyEvalOK)
{

















































































































}

/*
 * Initialize the SQLFunctionCache for a SQL function
 */
static void
init_sql_fcache(FmgrInfo *finfo, Oid collation, bool lazyEvalOK)
{









































































































































































}

/* Start up execution of one execution_state node */
static void
postquel_start(execution_state *es, SQLFunctionCachePtr fcache)
{






















































}

/* Run one execution_state; either to completion or to first result row */
/* Returns true if we ran to completion */
static bool
postquel_getnext(execution_state *es, SQLFunctionCachePtr fcache)
{






















}

/* Shut down execution of one execution_state node */
static void
postquel_end(execution_state *es)
{














}

/* Build ParamListInfo array representing current arguments */
static void
postquel_sub_params(SQLFunctionCachePtr fcache, FunctionCallInfo fcinfo)
{












































}

/*
 * Extract the SQL function's value from a single result row.  This is used
 * both for scalar (non-set) functions and for each row of a lazy-eval set
 * result.
 */
static Datum
postquel_get_single_result(TupleTableSlot *slot, FunctionCallInfo fcinfo, SQLFunctionCachePtr fcache, MemoryContext resultcontext)
{


































}

/*
 * fmgr_sql: function call manager for SQL functions
 */
Datum
fmgr_sql(PG_FUNCTION_ARGS)
{



























































































































































































































































































































































































}

/*
 * error context callback to let us supply a call-stack traceback
 */
static void
sql_exec_error_callback(void *arg)
{











































































}

/*
 * callback function in case a function-returning-set needs to be shut down
 * before it has been run to completion
 */
static void
ShutdownSQLFunction(Datum arg)
{









































}

/*
 * check_sql_fn_statements
 *
 * Check statements in an SQL function.  Error out if there is anything that
 * is not acceptable.
 */
void
check_sql_fn_statements(List *queryTreeList)
{








































}

/*
 * check_sql_fn_retval() -- check return value of a list of sql parse trees.
 *
 * The return value of a sql function is the value returned by the last
 * canSetTag query in the function.  We do some ad-hoc type checking here
 * to be sure that the user is returning the type he claims.  There are
 * also a couple of strange-looking features to assist callers in dealing
 * with allowed special cases, such as binary-compatible result types.
 *
 * For a polymorphic function the passed rettype must be the actual resolved
 * output type of the function; we should never see a polymorphic pseudotype
 * such as ANYELEMENT as rettype.  (This means we can't check the type during
 * function definition of a polymorphic function.)
 *
 * This function returns true if the sql function returns the entire tuple
 * result of its final statement, or false if it returns just the first column
 * result of that statement.  It throws an error if the final statement doesn't
 * return the right type at all.
 *
 * Note that because we allow "SELECT rowtype_expression", the result can be
 * false even when the declared function return type is a rowtype.
 *
 * If modifyTargetList isn't NULL, the function will modify the final
 * statement's targetlist in two cases:
 * (1) if the tlist returns values that are binary-coercible to the expected
 * type rather than being exactly the expected type.  RelabelType nodes will
 * be inserted to make the result types match exactly.
 * (2) if there are dropped columns in the declared result rowtype.  NULL
 * output columns will be inserted in the tlist to match them.
 * (Obviously the caller must pass a parsetree that is okay to modify when
 * using this flag.)  Note that this flag does not affect whether the tlist is
 * considered to be a legal match to the result type, only how we react to
 * allowed not-exact-match cases.  *modifyTargetList will be set true iff
 * we had to make any "dangerous" changes that could modify the semantics of
 * the statement.  If it is set true, the caller should not use the modified
 * statement, but for simplicity we apply the changes anyway.
 *
 * If junkFilter isn't NULL, then *junkFilter is set to a JunkFilter defined
 * to convert the function's tuple result to the correct output tuple type.
 * Exception: if the function is defined to return VOID then *junkFilter is
 * set to NULL.
 */
bool
check_sql_fn_retval(Oid func_id, Oid rettype, List *queryTreeList, bool *modifyTargetList, JunkFilter **junkFilter)
{










































































































































































































































































































































}

/*
 * CreateSQLFunctionDestReceiver -- create a suitable DestReceiver object
 */
DestReceiver *
CreateSQLFunctionDestReceiver(void)
{











}

/*
 * sqlfunction_startup --- executor startup
 */
static void
sqlfunction_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{

}

/*
 * sqlfunction_receive --- receive one tuple
 */
static bool
sqlfunction_receive(TupleTableSlot *slot, DestReceiver *self)
{









}

/*
 * sqlfunction_shutdown --- executor end
 */
static void
sqlfunction_shutdown(DestReceiver *self)
{
}

/*
 * sqlfunction_destroy --- release DestReceiver object
 */
static void
sqlfunction_destroy(DestReceiver *self)
{

}