/*-------------------------------------------------------------------------
 *
 * rewriteHandler.c
 *		Primary module of query rewriter.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/rewrite/rewriteHandler.c
 *
 * NOTES
 *	  Some of the terms used in this file are of historic nature: "retrieve"
 *	  was the PostQUEL keyword for what today is SELECT. "RIR" stands for
 *	  "Retrieve-Instead-Retrieve", that is an ON SELECT DO INSTEAD SELECT
 *rule (which has to be unconditional and where only one rule can exist on each
 *	  relation).
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

/* We use a list of these to detect recursion in RewriteQuery */
typedef struct rewrite_event
{
  Oid relation;  /* OID of relation having rules */
  CmdType event; /* type of rule being fired */
} rewrite_event;

typedef struct acquireLocksOnSubLinks_context
{
  bool for_execute; /* AcquireRewriteLocks' forExecute param */
} acquireLocksOnSubLinks_context;

static bool
acquireLocksOnSubLinks(Node *node, acquireLocksOnSubLinks_context *context);
static Query *
rewriteRuleAction(Query *parsetree, Query *rule_action, Node *rule_qual, int rt_index, CmdType event, bool *returning_flag);
static List *
adjustJoinTreeList(Query *parsetree, bool removert, int rt_index);
static List *
rewriteTargetListIU(List *targetList, CmdType commandType, OverridingKind override, Relation target_relation, int result_rti);
static TargetEntry *
process_matched_tle(TargetEntry *src_tle, TargetEntry *prior_tle, const char *attrName);
static Node *
get_assignment_input(Node *node);
static bool
rewriteValuesRTE(Query *parsetree, RangeTblEntry *rte, int rti, Relation target_relation);
static void
rewriteValuesRTEToNulls(Query *parsetree, RangeTblEntry *rte);
static void
markQueryForLocking(Query *qry, Node *jtnode, LockClauseStrength strength, LockWaitPolicy waitPolicy, bool pushedDown);
static List *
matchLocks(CmdType event, RuleLock *rulelocks, int varno, Query *parsetree, bool *hasUpdate);
static Query *
fireRIRrules(Query *parsetree, List *activeRIRs);
static bool
view_has_instead_trigger(Relation view, CmdType event);
static Bitmapset *
adjust_view_column_set(Bitmapset *cols, List *targetlist);

/*
 * AcquireRewriteLocks -
 *	  Acquire suitable locks on all the relations mentioned in the Query.
 *	  These locks will ensure that the relation schemas don't change under
 *us while we are rewriting, planning, and executing the query.
 *
 * Caution: this may modify the querytree, therefore caller should usually
 * have done a copyObject() to make a writable copy of the querytree in the
 * current memory context.
 *
 * forExecute indicates that the query is about to be executed.  If so,
 * we'll acquire the lock modes specified in the RTE rellockmode fields.
 * If forExecute is false, AccessShareLock is acquired on all relations.
 * This case is suitable for ruleutils.c, for example, where we only need
 * schema stability and we don't intend to actually modify any relations.
 *
 * forUpdatePushedDown indicates that a pushed-down FOR [KEY] UPDATE/SHARE
 * applies to the current subquery, requiring all rels to be opened with at
 * least RowShareLock.  This should always be false at the top of the
 * recursion.  When it is true, we adjust RTE rellockmode fields to reflect
 * the higher lock level.  This flag is ignored if forExecute is false.
 *
 * A secondary purpose of this routine is to fix up JOIN RTE references to
 * dropped columns (see details below).  Such RTEs are modified in-place.
 *
 * This processing can, and for efficiency's sake should, be skipped when the
 * querytree has just been built by the parser: parse analysis already got
 * all the same locks we'd get here, and the parser will have omitted dropped
 * columns from JOINs to begin with.  But we must do this whenever we are
 * dealing with a querytree produced earlier than the current command.
 *
 * About JOINs and dropped columns: although the parser never includes an
 * already-dropped column in a JOIN RTE's alias var list, it is possible for
 * such a list in a stored rule to include references to dropped columns.
 * (If the column is not explicitly referenced anywhere else in the query,
 * the dependency mechanism won't consider it used by the rule and so won't
 * prevent the column drop.)  To support get_rte_attribute_is_dropped(), we
 * replace join alias vars that reference dropped columns with null pointers.
 *
 * (In PostgreSQL 8.0, we did not do this processing but instead had
 * get_rte_attribute_is_dropped() recurse to detect dropped columns in joins.
 * That approach had horrible performance unfortunately; in particular
 * construction of a nested join was O(N^2) in the nesting depth.)
 */
void
AcquireRewriteLocks(Query *parsetree, bool forExecute, bool forUpdatePushedDown)
{



























































































































































}

/*
 * Walker to find sublink subqueries for AcquireRewriteLocks
 */
static bool
acquireLocksOnSubLinks(Node *node, acquireLocksOnSubLinks_context *context)
{


















}

/*
 * rewriteRuleAction -
 *	  Rewrite the rule action with appropriate qualifiers (taken from
 *	  the triggering query).
 *
 * Input arguments:
 *	parsetree - original query
 *	rule_action - one action (query) of a rule
 *	rule_qual - WHERE condition of rule, or NULL if unconditional
 *	rt_index - RT index of result relation in original query
 *	event - type of rule event
 * Output arguments:
 *	*returning_flag - set true if we rewrite RETURNING clause in rule_action
 *					(must be initialized to false)
 * Return value:
 *	rewritten form of rule_action
 */
static Query *
rewriteRuleAction(Query *parsetree, Query *rule_action, Node *rule_qual, int rt_index, CmdType event, bool *returning_flag)
{
































































































































































































































































































}

/*
 * Copy the query's jointree list, and optionally attempt to remove any
 * occurrence of the given rt_index as a top-level join item (we do not look
 * for it within join items; this is OK because we are only expecting to find
 * it as an UPDATE or DELETE target relation, which will be at the top level
 * of the join).  Returns modified jointree list --- this is a separate copy
 * sharing no nodes with the original.
 */
static List *
adjustJoinTreeList(Query *parsetree, bool removert, int rt_index)
{





















}

/*
 * rewriteTargetListIU - rewrite INSERT/UPDATE targetlist into standard form
 *
 * This has the following responsibilities:
 *
 * 1. For an INSERT, add tlist entries to compute default values for any
 * attributes that have defaults and are not assigned to in the given tlist.
 * (We do not insert anything for default-less attributes, however.  The
 * planner will later insert NULLs for them, but there's no reason to slow
 * down rewriter processing with extra tlist nodes.)  Also, for both INSERT
 * and UPDATE, replace explicit DEFAULT specifications with column default
 * expressions.
 *
 * 2. For an UPDATE on a trigger-updatable view, add tlist entries for any
 * unassigned-to attributes, assigning them their old values.  These will
 * later get expanded to the output values of the view.  (This is equivalent
 * to what the planner's expand_targetlist() will do for UPDATE on a regular
 * table, but it's more convenient to do it here while we still have easy
 * access to the view's original RT index.)  This is only necessary for
 * trigger-updatable views, for which the view remains the result relation of
 * the query.  For auto-updatable views we must not do this, since it might
 * add assignments to non-updatable view columns.  For rule-updatable views it
 * is unnecessary extra work, since the query will be rewritten with a
 * different result relation which will be processed when we recurse via
 * RewriteQuery.
 *
 * 3. Merge multiple entries for the same target attribute, or declare error
 * if we can't.  Multiple entries are only allowed for INSERT/UPDATE of
 * portions of an array or record field, for example
 *			UPDATE table SET foo[2] = 42, foo[4] = 43;
 * We can merge such operations into a single assignment op.  Essentially,
 * the expression we want to produce in this case is like
 *		foo = array_set_element(array_set_element(foo, 2, 42), 4, 43)
 *
 * 4. Sort the tlist into standard order: non-junk fields in order by resno,
 * then junk fields (these in no particular order).
 *
 * We must do items 1,2,3 before firing rewrite rules, else rewritten
 * references to NEW.foo will produce wrong or incomplete results.  Item 4
 * is not needed for rewriting, but will be needed by the planner, and we
 * can do it essentially for free while handling the other items.
 *
 * Note that for an inheritable UPDATE, this processing is only done once,
 * using the parent relation as reference.  It must not do anything that
 * will not be correct when transposed to the child relation(s).  (Step 4
 * is incorrect by this light, since child relations might have different
 * column ordering, but the planner will fix things by re-sorting the tlist
 * for each child.)
 */
static List *
rewriteTargetListIU(List *targetList, CmdType commandType, OverridingKind override, Relation target_relation, int result_rti)
{






















































































































































































}

/*
 * Convert a matched TLE from the original tlist into a correct new TLE.
 *
 * This routine detects and handles multiple assignments to the same target
 * attribute.  (The attribute name is needed only for error messages.)
 */
static TargetEntry *
process_matched_tle(TargetEntry *src_tle, TargetEntry *prior_tle, const char *attrName)
{






































































































































}

/*
 * If node is an assignment node, return its input; else return NULL
 */
static Node *
get_assignment_input(Node *node)
{























}

/*
 * Make an expression tree for the default value for a column.
 *
 * If there is no default, return a NULL instead.
 */
Node *
build_column_default(Relation rel, int attrno)
{





































































}

/* Does VALUES RTE contain any SetToDefault items? */
static bool
searchForDefault(RangeTblEntry *rte)
{


















}

/*
 * When processing INSERT ... VALUES with a VALUES RTE (ie, multiple VALUES
 * lists), we have to replace any DEFAULT items in the VALUES lists with
 * the appropriate default expressions.  The other aspects of targetlist
 * rewriting need be applied only to the query's targetlist proper.
 *
 * For an auto-updatable view, each DEFAULT item in the VALUES list is
 * replaced with the default from the view, if it has one.  Otherwise it is
 * left untouched so that the underlying base relation's default can be
 * applied instead (when we later recurse to here after rewriting the query
 * to refer to the base relation instead of the view).
 *
 * For other types of relation, including rule- and trigger-updatable views,
 * all DEFAULT items are replaced, and if the target relation doesn't have a
 * default, the value is explicitly set to NULL.
 *
 * Note that we may have subscripted or field assignment targetlist entries,
 * as well as more complex expressions from already-replaced DEFAULT items if
 * we have recursed to here for an auto-updatable view. However, it ought to
 * be impossible for such entries to have DEFAULTs assigned to them --- we
 * should only have to replace DEFAULT items for targetlist entries that
 * contain simple Vars referencing the VALUES RTE.
 *
 * Returns true if all DEFAULT items were replaced, and false if some were
 * left untouched.
 */
static bool
rewriteValuesRTE(Query *parsetree, RangeTblEntry *rte, int rti, Relation target_relation)
{
































































































































































}

/*
 * Mop up any remaining DEFAULT items in the given VALUES RTE by
 * replacing them with NULL constants.
 *
 * This is used for the product queries generated by DO ALSO rules attached to
 * an auto-updatable view.  The action can't depend on the "target relation"
 * since the product query might not have one (it needn't be an INSERT).
 * Essentially, such queries are treated as being attached to a rule-updatable
 * view.
 */
static void
rewriteValuesRTEToNulls(Query *parsetree, RangeTblEntry *rte)
{





























}

/*
 * rewriteTargetListUD - rewrite UPDATE/DELETE targetlist as needed
 *
 * This function adds a "junk" TLE that is needed to allow the executor to
 * find the original row for the update or delete.  When the target relation
 * is a regular table, the junk TLE emits the ctid attribute of the original
 * row.  When the target relation is a foreign table, we let the FDW decide
 * what to add.
 *
 * We used to do this during RewriteQuery(), but now that inheritance trees
 * can contain a mix of regular and foreign tables, we must postpone it till
 * planning, after the inheritance tree has been expanded.  In that way we
 * can do the right thing for each child table.
 */
void
rewriteTargetListUD(Query *parsetree, RangeTblEntry *target_rte, Relation target_relation)
{














































}

/*
 * Record in target_rte->extraUpdatedCols the indexes of any generated columns
 * that depend on any columns mentioned in target_rte->updatedCols.
 */
void
fill_extraUpdatedCols(RangeTblEntry *target_rte, Relation target_relation)
{





























}

/*
 * matchLocks -
 *	  match the list of locks and returns the matching rules
 */
static List *
matchLocks(CmdType event, RuleLock *rulelocks, int varno, Query *parsetree, bool *hasUpdate)
{






























































}

/*
 * ApplyRetrieveRule - expand an ON SELECT rule
 */
static Query *
ApplyRetrieveRule(Query *parsetree, RewriteRule *rule, int rt_index, Relation relation, List *activeRIRs)
{






































































































































































}

/*
 * Recursively mark all relations used by a view as FOR [KEY] UPDATE/SHARE.
 *
 * This may generate an invalid query, eg if some sub-query uses an
 * aggregate.  We leave it to the planner to detect that.
 *
 * NB: this must agree with the parser's transformLockingClause() routine.
 * However, unlike the parser we have to be careful not to mark a view's
 * OLD and NEW rels for updating.  The best way to handle that seems to be
 * to scan the jointree to determine which rels are used.
 */
static void
markQueryForLocking(Query *qry, Node *jtnode, LockClauseStrength strength, LockWaitPolicy waitPolicy, bool pushedDown)
{











































}

/*
 * fireRIRonSubLink -
 *	Apply fireRIRrules() to each SubLink (subselect in expression) found
 *	in the given tree.
 *
 * NOTE: although this has the form of a walker, we cheat and modify the
 * SubLink nodes in-place.  It is caller's responsibility to ensure that
 * no unwanted side-effects occur!
 *
 * This is unlike most of the other routines that recurse into subselects,
 * because we must take control at the SubLink node in order to replace
 * the SubLink's subselect link with the possibly-rewritten subquery.
 */
static bool
fireRIRonSubLink(Node *node, List *activeRIRs)
{


















}

/*
 * fireRIRrules -
 *	Apply all RIR rules on each rangetable entry in the given query
 *
 * activeRIRs is a list of the OIDs of views we're already processing RIR
 * rules for, used to detect/reject recursion.
 */
static Query *
fireRIRrules(Query *parsetree, List *activeRIRs)
{
































































































































































































































































}

/*
 * Modify the given query by adding 'AND rule_qual IS NOT TRUE' to its
 * qualification.  This is used to generate suitable "else clauses" for
 * conditional INSTEAD rules.  (Unfortunately we must use "x IS NOT TRUE",
 * not just "NOT x" which the planner is much smarter about, else we will
 * do the wrong thing when the qual evaluates to NULL.)
 *
 * The rule_qual may contain references to OLD or NEW.  OLD references are
 * replaced by references to the specified rt_index (the relation that the
 * rule applies to).  NEW references are only possible for INSERT and UPDATE
 * queries on the relation itself, and so they should be replaced by copies
 * of the related entries in the query's own targetlist.
 */
static Query *
CopyAndAddInvertedQual(Query *parsetree, Node *rule_qual, int rt_index, CmdType event)
{

























}

/*
 *	fireRules -
 *	   Iterate through rule locks applying rules.
 *
 * Input arguments:
 *	parsetree - original query
 *	rt_index - RT index of result relation in original query
 *	event - type of rule event
 *	locks - list of rules to fire
 * Output arguments:
 *	*instead_flag - set true if any unqualified INSTEAD rule is found
 *					(must be initialized to false)
 *	*returning_flag - set true if we rewrite RETURNING clause in any rule
 *					(must be initialized to false)
 *	*qual_product - filled with modified original query if any qualified
 *					INSTEAD rule is found (must be
 *initialized to NULL) Return value: list of rule actions adjusted for use with
 *this query
 *
 * Qualified INSTEAD rules generate their action with the qualification
 * condition added.  They also generate a modified version of the original
 * query with the negated qualification added, so that it will run only for
 * rows that the qualified action doesn't act on.  (If there are multiple
 * qualified INSTEAD rules, we AND all the negated quals onto a single
 * modified original query.)  We won't execute the original, unmodified
 * query if we find either qualified or unqualified INSTEAD rules.  If
 * we find both, the modified original query is discarded too.
 */
static List *
fireRules(Query *parsetree, int rt_index, CmdType event, List *locks, bool *instead_flag, bool *returning_flag, Query **qual_product)
{









































































}

/*
 * get_view_query - get the Query from a view's _RETURN rule.
 *
 * Caller should have verified that the relation is a view, and therefore
 * we should find an ON SELECT action.
 *
 * Note that the pointer returned is into the relcache and therefore must
 * be treated as read-only to the caller and not modified or scribbled on.
 */
Query *
get_view_query(Relation view)
{






















}

/*
 * view_has_instead_trigger - does view have an INSTEAD OF trigger for event?
 *
 * If it does, we don't want to treat it as auto-updatable.  This test can't
 * be folded into view_query_is_auto_updatable because it's not an error
 * condition.
 */
static bool
view_has_instead_trigger(Relation view, CmdType event)
{



























}

/*
 * view_col_is_auto_updatable - test whether the specified column of a view
 * is auto-updatable. Returns NULL (if the column can be updated) or a message
 * string giving the reason that it cannot be.
 *
 * The returned string has not been translated; if it is shown as an error
 * message, the caller should apply _() to translate it.
 *
 * Note that the checks performed here are local to this view. We do not check
 * whether the referenced column of the underlying base relation is updatable.
 */
static const char *
view_col_is_auto_updatable(RangeTblRef *rtr, TargetEntry *tle)
{
































}

/*
 * view_query_is_auto_updatable - test whether the specified view definition
 * represents an auto-updatable view. Returns NULL (if the view can be updated)
 * or a message string giving the reason that it cannot be.

 * The returned string has not been translated; if it is shown as an error
 * message, the caller should apply _() to translate it.
 *
 * If check_cols is true, the view is required to have at least one updatable
 * column (necessary for INSERT/UPDATE). Otherwise the view's columns are not
 * checked for updatability. See also view_cols_are_auto_updatable.
 *
 * Note that the checks performed here are only based on the view definition.
 * We do not check whether any base relations referred to by the view are
 * updatable.
 */
const char *
view_query_is_auto_updatable(Query *viewquery, bool check_cols)
{















































































































































}

/*
 * view_cols_are_auto_updatable - test whether all of the required columns of
 * an auto-updatable view are actually updatable. Returns NULL (if all the
 * required columns can be updated) or a message string giving the reason that
 * they cannot be.
 *
 * The returned string has not been translated; if it is shown as an error
 * message, the caller should apply _() to translate it.
 *
 * This should be used for INSERT/UPDATE to ensure that we don't attempt to
 * assign to any non-updatable columns.
 *
 * Additionally it may be used to retrieve the set of updatable columns in the
 * view, or if one or more of the required columns is not updatable, the name
 * of the first offending non-updatable column.
 *
 * The caller must have already verified that this is an auto-updatable view
 * using view_query_is_auto_updatable.
 *
 * Note that the checks performed here are only based on the view definition.
 * We do not check whether the referenced columns of the base relation are
 * updatable.
 */
static const char *
view_cols_are_auto_updatable(Query *viewquery, Bitmapset *required_cols, Bitmapset **updatable_cols, char **non_updatable_col)
{



















































}

/*
 * relation_is_updatable - determine which update events the specified
 * relation supports.
 *
 * Note that views may contain a mix of updatable and non-updatable columns.
 * For a view to support INSERT/UPDATE it must have at least one updatable
 * column, but there is no such restriction for DELETE. If include_cols is
 * non-NULL, then only the specified columns are considered when testing for
 * updatability.
 *
 * Unlike the preceding functions, this does recurse to look at a view's
 * base relations, so it needs to detect recursion.  To do that, we pass
 * a list of currently-considered outer relations.  External callers need
 * only pass NIL.
 *
 * This is used for the information_schema views, which have separate concepts
 * of "updatable" and "trigger updatable".  A relation is "updatable" if it
 * can be updated without the need for triggers (either because it has a
 * suitable RULE, or because it is simple enough to be automatically updated).
 * A relation is "trigger updatable" if it has a suitable INSTEAD OF trigger.
 * The SQL standard regards this as not necessarily updatable, presumably
 * because there is no way of knowing what the trigger will actually do.
 * The information_schema views therefore call this function with
 * include_triggers = false.  However, other callers might only care whether
 * data-modifying SQL will work, so they can pass include_triggers = true
 * to have trigger updatability included in the result.
 *
 * The return value is a bitmask of rule event numbers indicating which of
 * the INSERT, UPDATE and DELETE operations are supported.  (We do it this way
 * so that we can test for UPDATE plus DELETE support in a single call.)
 */
int
relation_is_updatable(Oid reloid, List *outer_reloids, bool include_triggers, Bitmapset *include_cols)
{

















































































































































































}

/*
 * adjust_view_column_set - map a set of column numbers according to targetlist
 *
 * This is used with simply-updatable views to map column-permissions sets for
 * the view columns onto the matching columns in the underlying base relation.
 * The targetlist is expected to be a list of plain Vars of the underlying
 * relation (as per the checks above in view_query_is_auto_updatable).
 */
static Bitmapset *
adjust_view_column_set(Bitmapset *cols, List *targetlist)
{
























































}

/*
 * rewriteTargetView -
 *	  Attempt to rewrite a query where the target relation is a view, so
 *that the view's base relation becomes the target relation.
 *
 * Note that the base relation here may itself be a view, which may or may not
 * have INSTEAD OF triggers or rules to handle the update.  That is handled by
 * the recursion in RewriteQuery.
 */
static Query *
rewriteTargetView(Query *parsetree, Relation view)
{









































































































































































































































































































































































































































































































}

/*
 * RewriteQuery -
 *	  rewrites the query and apply the rules again on the queries rewritten
 *
 * rewrite_events is a list of open query-rewrite actions, so we can detect
 * infinite recursion.
 *
 * orig_rt_length is the length of the originating query's rtable, for product
 * queries created by fireRules(), and 0 otherwise.  This is used to skip any
 * already-processed VALUES RTEs from the original query.
 */
static List *
RewriteQuery(Query *parsetree, List *rewrite_events, int orig_rt_length)
{






















































































































































































































































































































































































































































}

/*
 * QueryRewrite -
 *	  Primary entry point to the query rewriter.
 *	  Rewrite one query via query rewrite system, possibly returning 0
 *	  or many queries.
 *
 * NOTE: the parsetree must either have come straight from the parser,
 * or have been scanned by AcquireRewriteLocks to acquire suitable locks.
 */
List *
QueryRewrite(Query *parsetree)
{

























































































}