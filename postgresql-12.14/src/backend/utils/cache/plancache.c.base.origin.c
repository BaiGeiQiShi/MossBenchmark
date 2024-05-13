/*-------------------------------------------------------------------------
 *
 * plancache.c
 *	  Plan cache management.
 *
 * The plan cache manager has two principal responsibilities: deciding when
 * to use a generic plan versus a custom (parameter-value-specific) plan,
 * and tracking whether cached plans need to be invalidated because of schema
 * changes in the objects they depend on.
 *
 * The logic for choosing generic or custom plans is in choose_custom_plan,
 * which see for comments.
 *
 * Cache invalidation is driven off sinval events.  Any CachedPlanSource
 * that matches the event is marked invalid, as is its generic CachedPlan
 * if it has one.  When (and if) the next demand for a cached plan occurs,
 * parse analysis and rewrite is repeated to build a new valid query tree,
 * and then planning is performed as normal.  We also force re-analysis and
 * re-planning if the active search_path is different from the previous time
 * or, if RLS is involved, if the user changes or the RLS environment changes.
 *
 * Note that if the sinval was a result of user DDL actions, parse analysis
 * could throw an error, for example if a column referenced by the query is
 * no longer present.  Another possibility is for the query's output tupdesc
 * to change (for instance "SELECT *" might expand differently than before).
 * The creator of a cached plan can specify whether it is allowable for the
 * query to change output tupdesc on replan --- if so, it's up to the
 * caller to notice changes and cope with them.
 *
 * Currently, we track exactly the dependencies of plans on relations,
 * user-defined functions, and domains.  On relcache invalidation events or
 * pg_proc or pg_type syscache invalidation events, we invalidate just those
 * plans that depend on the particular object being modified.  (Note: this
 * scheme assumes that any table modification that requires replanning will
 * generate a relcache inval event.)  We also watch for inval events on
 * certain other system catalogs, such as pg_namespace; but for them, our
 * response is just to invalidate all plans.  We expect updates on those
 * catalogs to be infrequent enough that more-detailed tracking is not worth
 * the effort.
 *
 * In addition to full-fledged query plans, we provide a facility for
 * detecting invalidations of simple scalar expressions.  This is fairly
 * bare-bones; it's the caller's responsibility to build a new expression
 * if the old one gets invalidated.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/plancache.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/transam.h"
#include "catalog/namespace.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "storage/lmgr.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/resowner_private.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/*
 * We must skip "overhead" operations that involve database access when the
 * cached plan's subject statement is a transaction control command.
 */
#define IsTransactionStmtPlan(plansource) ((plansource)->raw_parse_tree && IsA((plansource)->raw_parse_tree->stmt, TransactionStmt))

/*
 * This is the head of the backend's list of "saved" CachedPlanSources (i.e.,
 * those that are in long-lived storage and are examined for sinval events).
 * We use a dlist instead of separate List cells so that we can guarantee
 * to save a CachedPlanSource without error.
 */
static dlist_head saved_plan_list = DLIST_STATIC_INIT(saved_plan_list);

/*
 * This is the head of the backend's list of CachedExpressions.
 */
static dlist_head cached_expression_list = DLIST_STATIC_INIT(cached_expression_list);

static void
ReleaseGenericPlan(CachedPlanSource *plansource);
static List *
RevalidateCachedQuery(CachedPlanSource *plansource, QueryEnvironment *queryEnv);
static bool
CheckCachedPlan(CachedPlanSource *plansource);
static CachedPlan *
BuildCachedPlan(CachedPlanSource *plansource, List *qlist, ParamListInfo boundParams, QueryEnvironment *queryEnv);
static bool
choose_custom_plan(CachedPlanSource *plansource, ParamListInfo boundParams);
static double
cached_plan_cost(CachedPlan *plan, bool include_planner);
static Query *
QueryListGetPrimaryStmt(List *stmts);
static void
AcquireExecutorLocks(List *stmt_list, bool acquire);
static void
AcquirePlannerLocks(List *stmt_list, bool acquire);
static void
ScanQueryForLocks(Query *parsetree, bool acquire);
static bool
ScanQueryWalker(Node *node, bool *acquire);
static TupleDesc
PlanCacheComputeResultDesc(List *stmt_list);
static void
PlanCacheRelCallback(Datum arg, Oid relid);
static void
PlanCacheObjectCallback(Datum arg, int cacheid, uint32 hashvalue);
static void
PlanCacheSysCallback(Datum arg, int cacheid, uint32 hashvalue);

/* GUC parameter */
int plan_cache_mode;

/*
 * InitPlanCache: initialize module during InitPostgres.
 *
 * All we need to do is hook into inval.c's callback lists.
 */
void
InitPlanCache(void)
{








}

/*
 * CreateCachedPlan: initially create a plan cache entry.
 *
 * Creation of a cached plan is divided into two steps, CreateCachedPlan and
 * CompleteCachedPlan.  CreateCachedPlan should be called after running the
 * query through raw_parser, but before doing parse analysis and rewrite;
 * CompleteCachedPlan is called after that.  The reason for this arrangement
 * is that it can save one round of copying of the raw parse tree, since
 * the parser will normally scribble on the raw parse tree.  Callers would
 * otherwise need to make an extra copy of the parse tree to ensure they
 * still had a clean copy to present at plan cache creation time.
 *
 * All arguments presented to CreateCachedPlan are copied into a memory
 * context created as a child of the call-time CurrentMemoryContext, which
 * should be a reasonably short-lived working context that will go away in
 * event of an error.  This ensures that the cached plan data structure will
 * likewise disappear if an error occurs before we have fully constructed it.
 * Once constructed, the cached plan can be made longer-lived, if needed,
 * by calling SaveCachedPlan.
 *
 * raw_parse_tree: output of raw_parser(), or NULL if empty query
 * query_string: original query text
 * commandTag: compile-time-constant tag for query, or NULL if empty query
 */
CachedPlanSource *
CreateCachedPlan(RawStmt *raw_parse_tree, const char *query_string, const char *commandTag)
{
























































}

/*
 * CreateOneShotCachedPlan: initially create a one-shot plan cache entry.
 *
 * This variant of CreateCachedPlan creates a plan cache entry that is meant
 * to be used only once.  No data copying occurs: all data structures remain
 * in the caller's memory context (which typically should get cleared after
 * completing execution).  The CachedPlanSource struct itself is also created
 * in that context.
 *
 * A one-shot plan cannot be saved or copied, since we make no effort to
 * preserve the raw parse tree unmodified.  There is also no support for
 * invalidation, so plan use must be completed in the current transaction,
 * and DDL that might invalidate the querytree_list must be avoided as well.
 *
 * raw_parse_tree: output of raw_parser(), or NULL if empty query
 * query_string: original query text
 * commandTag: compile-time-constant tag for query, or NULL if empty query
 */
CachedPlanSource *
CreateOneShotCachedPlan(RawStmt *raw_parse_tree, const char *query_string, const char *commandTag)
{








































}

/*
 * CompleteCachedPlan: second step of creating a plan cache entry.
 *
 * Pass in the analyzed-and-rewritten form of the query, as well as the
 * required subsidiary data about parameters and such.  All passed values will
 * be copied into the CachedPlanSource's memory, except as specified below.
 * After this is called, GetCachedPlan can be called to obtain a plan, and
 * optionally the CachedPlanSource can be saved using SaveCachedPlan.
 *
 * If querytree_context is not NULL, the querytree_list must be stored in that
 * context (but the other parameters need not be).  The querytree_list is not
 * copied, rather the given context is kept as the initial query_context of
 * the CachedPlanSource.  (It should have been created as a child of the
 * caller's working memory context, but it will now be reparented to belong
 * to the CachedPlanSource.)  The querytree_context is normally the context in
 * which the caller did raw parsing and parse analysis.  This approach saves
 * one tree copying step compared to passing NULL, but leaves lots of extra
 * cruft in the query_context, namely whatever extraneous stuff parse analysis
 * created, as well as whatever went unused from the raw parse tree.  Using
 * this option is a space-for-time tradeoff that is appropriate if the
 * CachedPlanSource is not expected to survive long.
 *
 * plancache.c cannot know how to copy the data referenced by parserSetupArg,
 * and it would often be inappropriate to do so anyway.  When using that
 * option, it is caller's responsibility that the referenced data remains
 * valid for as long as the CachedPlanSource exists.
 *
 * If the CachedPlanSource is a "oneshot" plan, then no querytree copying
 * occurs at all, and querytree_context is ignored; it is caller's
 * responsibility that the passed querytree_list is sufficiently long-lived.
 *
 * plansource: structure returned by CreateCachedPlan
 * querytree_list: analyzed-and-rewritten form of query (list of Query nodes)
 * querytree_context: memory context containing querytree_list,
 *					  or NULL to copy querytree_list into a
 *fresh context param_types: array of fixed parameter type OIDs, or NULL if none
 * num_params: number of fixed parameters
 * parserSetup: alternate method for handling query parameters
 * parserSetupArg: data to pass to parserSetup
 * cursor_options: options bitmask to pass to planner
 * fixed_result: true to disallow future changes in query's result tupdesc
 */
void
CompleteCachedPlan(CachedPlanSource *plansource, List *querytree_list, MemoryContext querytree_context, Oid *param_types, int num_params, ParserSetupHook parserSetup, void *parserSetupArg, int cursor_options, bool fixed_result)
{





















































































}

/*
 * SaveCachedPlan: save a cached plan permanently
 *
 * This function moves the cached plan underneath CacheMemoryContext (making
 * it live for the life of the backend, unless explicitly dropped), and adds
 * it to the list of cached plans that are checked for invalidation when an
 * sinval event occurs.
 *
 * This is guaranteed not to throw error, except for the caller-error case
 * of trying to save a one-shot plan.  Callers typically depend on that
 * since this is called just before or just after adding a pointer to the
 * CachedPlanSource to some permanent data structure of their own.  Up until
 * this is done, a CachedPlanSource is just transient data that will go away
 * automatically on transaction abort.
 */
void
SaveCachedPlan(CachedPlanSource *plansource)
{

































}

/*
 * DropCachedPlan: destroy a cached plan.
 *
 * Actually this only destroys the CachedPlanSource: any referenced CachedPlan
 * is released, but not destroyed until its refcount goes to zero.  That
 * handles the situation where DropCachedPlan is called while the plan is
 * still in use.
 */
void
DropCachedPlan(CachedPlanSource *plansource)
{























}

/*
 * ReleaseGenericPlan: release a CachedPlanSource's generic plan, if any.
 */
static void
ReleaseGenericPlan(CachedPlanSource *plansource)
{









}

/*
 * RevalidateCachedQuery: ensure validity of analyzed-and-rewritten query tree.
 *
 * What we do here is re-acquire locks and redo parse analysis if necessary.
 * On return, the query_list is valid and we have sufficient locks to begin
 * planning.
 *
 * If any parse analysis activity is required, the caller's memory context is
 * used for that work.
 *
 * The result value is the transient analyzed-and-rewritten query tree if we
 * had to do re-analysis, and NIL otherwise.  (This is returned just to save
 * a tree copying step in a subsequent BuildCachedPlan call.)
 */
static List *
RevalidateCachedQuery(CachedPlanSource *plansource, QueryEnvironment *queryEnv)
{

































































































































































































































}

/*
 * CheckCachedPlan: see if the CachedPlanSource's generic plan is valid.
 *
 * Caller must have already called RevalidateCachedQuery to verify that the
 * querytree is up to date.
 *
 * On a "true" return, we have acquired the locks needed to run the plan.
 * (We must do this for the "true" result to be race-condition-free.)
 */
static bool
CheckCachedPlan(CachedPlanSource *plansource)
{


































































}

/*
 * BuildCachedPlan: construct a new CachedPlan from a CachedPlanSource.
 *
 * qlist should be the result value from a previous RevalidateCachedQuery,
 * or it can be set to NIL if we need to re-copy the plansource's query_list.
 *
 * To build a generic, parameter-value-independent plan, pass NULL for
 * boundParams.  To build a custom plan, pass the actual parameter values via
 * boundParams.  For best effect, the PARAM_FLAG_CONST flag should be set on
 * each parameter value; otherwise the planner will treat the value as a
 * hint rather than a hard constant.
 *
 * Planning work is done in the caller's memory context.  The finished plan
 * is in a child memory context, which typically should get reparented
 * (unless this is a one-shot plan, in which case we don't copy the plan).
 */
static CachedPlan *
BuildCachedPlan(CachedPlanSource *plansource, List *qlist, ParamListInfo boundParams, QueryEnvironment *queryEnv)
{














































































































































}

/*
 * choose_custom_plan: choose whether to use custom or generic plan
 *
 * This defines the policy followed by GetCachedPlan.
 */
static bool
choose_custom_plan(CachedPlanSource *plansource, ParamListInfo boundParams)
{































































}

/*
 * cached_plan_cost: calculate estimated cost of a plan
 *
 * If include_planner is true, also include the estimated cost of constructing
 * the plan.  (We must factor that into the cost of using a custom plan, but
 * we don't count it for a generic plan.)
 */
static double
cached_plan_cost(CachedPlan *plan, bool include_planner)
{












































}

/*
 * GetCachedPlan: get a cached plan from a CachedPlanSource.
 *
 * This function hides the logic that decides whether to use a generic
 * plan or a custom plan for the given parameters: the caller does not know
 * which it will get.
 *
 * On return, the plan is valid and we have sufficient locks to begin
 * execution.
 *
 * On return, the refcount of the plan has been incremented; a later
 * ReleaseCachedPlan() call is expected.  The refcount has been reported
 * to the CurrentResourceOwner if useResOwner is true (note that that must
 * only be true if it's a "saved" CachedPlanSource).
 *
 * Note: if any replanning activity is required, the caller's memory context
 * is used for that work.
 */
CachedPlan *
GetCachedPlan(CachedPlanSource *plansource, ParamListInfo boundParams, bool useResOwner, QueryEnvironment *queryEnv)
{













































































































}

/*
 * ReleaseCachedPlan: release active use of a cached plan.
 *
 * This decrements the reference count, and frees the plan if the count
 * has thereby gone to zero.  If useResOwner is true, it is assumed that
 * the reference count is managed by the CurrentResourceOwner.
 *
 * Note: useResOwner = false is used for releasing references that are in
 * persistent data structures, such as the parent CachedPlanSource or a
 * Portal.  Transient references should be protected by a resource owner.
 */
void
ReleaseCachedPlan(CachedPlan *plan, bool useResOwner)
{



















}

/*
 * CachedPlanSetParentContext: move a CachedPlanSource to a new memory context
 *
 * This can only be applied to unsaved plans; once saved, a plan always
 * lives underneath CacheMemoryContext.
 */
void
CachedPlanSetParentContext(CachedPlanSource *plansource, MemoryContext newcontext)
{



























}

/*
 * CopyCachedPlan: make a copy of a CachedPlanSource
 *
 * This is a convenience routine that does the equivalent of
 * CreateCachedPlan + CompleteCachedPlan, using the data stored in the
 * input CachedPlanSource.  The result is therefore "unsaved" (regardless
 * of the state of the source), and we don't copy any generic plan either.
 * The result will be currently valid, or not, the same as the source.
 */
CachedPlanSource *
CopyCachedPlan(CachedPlanSource *plansource)
{

















































































}

/*
 * CachedPlanIsValid: test whether the rewritten querytree within a
 * CachedPlanSource is currently valid (that is, not marked as being in need
 * of revalidation).
 *
 * This result is only trustworthy (ie, free from race conditions) if
 * the caller has acquired locks on all the relations used in the plan.
 */
bool
CachedPlanIsValid(CachedPlanSource *plansource)
{


}

/*
 * CachedPlanGetTargetList: return tlist, if any, describing plan's output
 *
 * The result is guaranteed up-to-date.  However, it is local storage
 * within the cached plan, and may disappear next time the plan is updated.
 */
List *
CachedPlanGetTargetList(CachedPlanSource *plansource, QueryEnvironment *queryEnv)
{






















}

/*
 * GetCachedExpression: construct a CachedExpression for an expression.
 *
 * This performs the same transformations on the expression as
 * expression_planner(), ie, convert an expression as emitted by parse
 * analysis to be ready to pass to the executor.
 *
 * The result is stashed in a private, long-lived memory context.
 * (Note that this might leak a good deal of memory in the caller's
 * context before that.)  The passed-in expr tree is not modified.
 */
CachedExpression *
GetCachedExpression(Node *expr)
{












































}

/*
 * FreeCachedExpression
 *		Delete a CachedExpression.
 */
void
FreeCachedExpression(CachedExpression *cexpr)
{






}

/*
 * QueryListGetPrimaryStmt
 *		Get the "primary" stmt within a list, ie, the one marked
 *canSetTag.
 *
 * Returns NULL if no such stmt.  If multiple queries within the list are
 * marked canSetTag, returns the first one.  Neither of these cases should
 * occur in present usages of this function.
 */
static Query *
QueryListGetPrimaryStmt(List *stmts)
{












}

/*
 * AcquireExecutorLocks: acquire locks needed for execution of a cached plan;
 * or release them if acquire is false.
 */
static void
AcquireExecutorLocks(List *stmt_list, bool acquire)
{


















































}

/*
 * AcquirePlannerLocks: acquire locks needed for planning of a querytree list;
 * or release them if acquire is false.
 *
 * Note that we don't actually try to open the relations, and hence will not
 * fail if one has been dropped entirely --- we'll just transiently acquire
 * a non-conflicting lock.
 */
static void
AcquirePlannerLocks(List *stmt_list, bool acquire)
{



















}

/*
 * ScanQueryForLocks: recursively scan one Query for AcquirePlannerLocks.
 */
static void
ScanQueryForLocks(Query *parsetree, bool acquire)
{





















































}

/*
 * Walker to find sublink subqueries for ScanQueryForLocks
 */
static bool
ScanQueryWalker(Node *node, bool *acquire)
{


















}

/*
 * PlanCacheComputeResultDesc: given a list of analyzed-and-rewritten Queries,
 * determine the result tupledesc it will produce.  Returns NULL if the
 * execution will not return tuples.
 *
 * Note: the result is created or copied into current memory context.
 */
static TupleDesc
PlanCacheComputeResultDesc(List *stmt_list)
{
























}

/*
 * PlanCacheRelCallback
 *		Relcache inval callback function
 *
 * Invalidate all plans mentioning the given rel, or all plans mentioning
 * any rel at all if relid == InvalidOid.
 */
static void
PlanCacheRelCallback(Datum arg, Oid relid)
{













































































}

/*
 * PlanCacheObjectCallback
 *		Syscache inval callback function for PROCOID and TYPEOID caches
 *
 * Invalidate all plans mentioning the object with the specified hash value,
 * or all plans mentioning any member of this cache if hashvalue == 0.
 */
static void
PlanCacheObjectCallback(Datum arg, int cacheid, uint32 hashvalue)
{















































































































}

/*
 * PlanCacheSysCallback
 *		Syscache inval callback function for other caches
 *
 * Just invalidate everything...
 */
static void
PlanCacheSysCallback(Datum arg, int cacheid, uint32 hashvalue)
{

}

/*
 * ResetPlanCache: invalidate all cached plans.
 */
void
ResetPlanCache(void)
{



























































}