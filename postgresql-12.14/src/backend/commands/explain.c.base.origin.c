/*-------------------------------------------------------------------------
 *
 * explain.c
 *	  Explain query execution plans
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/commands/explain.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/createas.h"
#include "commands/defrem.h"
#include "commands/prepare.h"
#include "executor/nodeHash.h"
#include "foreign/fdwapi.h"
#include "jit/jit.h"
#include "nodes/extensible.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/guc_tables.h"
#include "utils/json.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/tuplesort.h"
#include "utils/typcache.h"
#include "utils/xml.h"

/* Hook for plugins to get control in ExplainOneQuery() */
ExplainOneQuery_hook_type ExplainOneQuery_hook = NULL;

/* Hook for plugins to get control in explain_get_index_name() */
explain_get_index_name_hook_type explain_get_index_name_hook = NULL;

/* OR-able flags for ExplainXMLTag() */
#define X_OPENING 0
#define X_CLOSING 1
#define X_CLOSE_IMMEDIATE 2
#define X_NOWHITESPACE 4

static void
ExplainOneQuery(Query *query, int cursorOptions, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv);
static void
report_triggers(ResultRelInfo *rInfo, bool show_relname, ExplainState *es);
static double
elapsed_time(instr_time *starttime);
static bool
ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used);
static void
ExplainNode(PlanState *planstate, List *ancestors, const char *relationship, const char *plan_name, ExplainState *es);
static void
show_plan_tlist(PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_expression(Node *node, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es);
static void
show_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es);
static void
show_scan_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_upper_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_sort_keys(SortState *sortstate, List *ancestors, ExplainState *es);
static void
show_merge_append_keys(MergeAppendState *mstate, List *ancestors, ExplainState *es);
static void
show_agg_keys(AggState *astate, List *ancestors, ExplainState *es);
static void
show_grouping_sets(PlanState *planstate, Agg *agg, List *ancestors, ExplainState *es);
static void
show_grouping_set_keys(PlanState *planstate, Agg *aggnode, Sort *sortnode, List *context, bool useprefix, List *ancestors, ExplainState *es);
static void
show_group_keys(GroupState *gstate, List *ancestors, ExplainState *es);
static void
show_sort_group_keys(PlanState *planstate, const char *qlabel, int nkeys, AttrNumber *keycols, Oid *sortOperators, Oid *collations, bool *nullsFirst, List *ancestors, ExplainState *es);
static void
show_sortorder_options(StringInfo buf, Node *sortexpr, Oid sortOperator, Oid collation, bool nullsFirst);
static void
show_tablesample(TableSampleClause *tsc, PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_sort_info(SortState *sortstate, ExplainState *es);
static void
show_hash_info(HashState *hashstate, ExplainState *es);
static void
show_tidbitmap_info(BitmapHeapScanState *planstate, ExplainState *es);
static void
show_instrumentation_count(const char *qlabel, int which, PlanState *planstate, ExplainState *es);
static void
show_foreignscan_info(ForeignScanState *fsstate, ExplainState *es);
static void
show_eval_params(Bitmapset *bms_params, ExplainState *es);
static const char *
explain_get_index_name(Oid indexId);
static void
show_buffer_usage(ExplainState *es, const BufferUsage *usage);
static void
ExplainIndexScanDetails(Oid indexid, ScanDirection indexorderdir, ExplainState *es);
static void
ExplainScanTarget(Scan *plan, ExplainState *es);
static void
ExplainModifyTarget(ModifyTable *plan, ExplainState *es);
static void
ExplainTargetRel(Plan *plan, Index rti, ExplainState *es);
static void
show_modifytable_info(ModifyTableState *mtstate, List *ancestors, ExplainState *es);
static void
ExplainMemberNodes(PlanState **planstates, int nplans, List *ancestors, ExplainState *es);
static void
ExplainMissingMembers(int nplans, int nchildren, ExplainState *es);
static void
ExplainSubPlans(List *plans, List *ancestors, const char *relationship, ExplainState *es);
static void
ExplainCustomChildren(CustomScanState *css, List *ancestors, ExplainState *es);
static void
ExplainProperty(const char *qlabel, const char *unit, const char *value, bool numeric, ExplainState *es);
static void
ExplainDummyGroup(const char *objtype, const char *labelname, ExplainState *es);
static void
ExplainXMLTag(const char *tagname, int flags, ExplainState *es);
static void
ExplainJSONLineEnding(ExplainState *es);
static void
ExplainYAMLLineStarting(ExplainState *es);
static void
escape_yaml(StringInfo buf, const char *str);

/*
 * ExplainQuery -
 *	  execute an EXPLAIN command
 */
void
ExplainQuery(ParseState *pstate, ExplainStmt *stmt, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv, DestReceiver *dest)
{
























































































































































}

/*
 * Create a new ExplainState struct initialized with default options.
 */
ExplainState *
NewExplainState(void)
{








}

/*
 * ExplainResultDesc -
 *	  construct the result tupledesc for an EXPLAIN
 */
TupleDesc
ExplainResultDesc(ExplainStmt *stmt)
{

































}

/*
 * ExplainOneQuery -
 *	  print out the execution plan for one Query
 *
 * "into" is NULL unless we are explaining the contents of a CreateTableAsStmt.
 */
static void
ExplainOneQuery(Query *query, int cursorOptions, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv)
{




























}

/*
 * ExplainOneUtility -
 *	  print out the execution plan for one utility statement
 *	  (In general, utility statements don't have plans, but there are some
 *	  we treat as special cases)
 *
 * "into" is NULL unless we are explaining the contents of a CreateTableAsStmt.
 *
 * This is exported because it's called back from prepare.c in the
 * EXPLAIN EXECUTE case.
 */
void
ExplainOneUtility(Node *utilityStmt, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv)
{






























































}

/*
 * ExplainOnePlan -
 *		given a planned query, execute it if needed, and then print
 *		EXPLAIN output
 *
 * "into" is NULL unless we are explaining the contents of a CreateTableAsStmt,
 * in which case executing the query should result in creating that table.
 *
 * This is exported because it's called back from prepare.c in the
 * EXPLAIN EXECUTE case, and because an index advisor plugin would need
 * to call it.
 */
void
ExplainOnePlan(PlannedStmt *plannedstmt, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv, const instr_time *planduration)
{




























































































































































}

/*
 * ExplainPrintSettings -
 *    Print summary of modified settings affecting query planning.
 */
static void
ExplainPrintSettings(ExplainState *es)
{
































































}

/*
 * ExplainPrintPlan -
 *	  convert a QueryDesc's plan tree to text and append it to es->str
 *
 * The caller should have set up the options fields of *es, as well as
 * initializing the output buffer es->str.  Also, output formatting state
 * such as the indent level is assumed valid.  Plan-tree-specific fields
 * in *es are initialized here.
 *
 * NB: will not work on utility statements
 */
void
ExplainPrintPlan(ExplainState *es, QueryDesc *queryDesc)
{






























}

/*
 * ExplainPrintTriggers -
 *	  convert a QueryDesc's trigger statistics to text and append it to
 *	  es->str
 *
 * The caller should have set up the options fields of *es, as well as
 * initializing the output buffer es->str.  Other fields in *es are
 * initialized here.
 */
void
ExplainPrintTriggers(ExplainState *es, QueryDesc *queryDesc)
{








































}

/*
 * ExplainPrintJITSummary -
 *    Print summarized JIT instrumentation from leader and workers
 */
void
ExplainPrintJITSummary(ExplainState *es, QueryDesc *queryDesc)
{























}

/*
 * ExplainPrintJIT -
 *	  Append information about JITing to es->str.
 *
 * Can be used to print the JIT instrumentation of the backend (worker_num =
 * -1) or that of a specific worker (worker_num = ...).
 */
void
ExplainPrintJIT(ExplainState *es, int jit_flags, JitInstrumentation *ji, int worker_num)
{








































































}

/*
 * ExplainQueryText -
 *	  add a "Query Text" node that contains the actual text of the query
 *
 * The caller should have set up the options fields of *es, as well as
 * initializing the output buffer es->str.
 *
 */
void
ExplainQueryText(ExplainState *es, QueryDesc *queryDesc)
{




}

/*
 * report_triggers -
 *		report execution stats for a single relation's triggers
 */
static void
report_triggers(ResultRelInfo *rInfo, bool show_relname, ExplainState *es)
{























































































}

/* Compute elapsed time in seconds since given timestamp */
static double
elapsed_time(instr_time *starttime)
{





}

/*
 * ExplainPreScanNode -
 *	  Prescan the planstate tree to identify which RTEs are referenced
 *
 * Adds the relid of each referenced RTE to *rels_used.  The result controls
 * which RTEs are assigned aliases by select_rtable_names_for_explain.
 * This ensures that we don't confusingly assign un-suffixed aliases to RTEs
 * that never appear in the EXPLAIN output (such as inheritance parents).
 */
static bool
ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used)
{





































}

/*
 * ExplainNode -
 *	  Appends a description of a plan tree to es->str
 *
 * planstate points to the executor state node for the current plan node.
 * We need to work from a PlanState node, not just a Plan node, in order to
 * get at the instrumentation data (if any) as well as the list of subplans.
 *
 * ancestors is a list of parent PlanState nodes, most-closely-nested first.
 * These are needed in order to interpret PARAM_EXEC Params.
 *
 * relationship describes the relationship of this plan node to its parent
 * (eg, "Outer", "Inner"); it can be null at top level.  plan_name is an
 * optional name to be attached to the node.
 *
 * In text format, es->indent is controlled in this function since we only
 * want it to change at plan-node boundaries.  In non-text formats, es->indent
 * corresponds to the nesting depth of logical output groups, and therefore
 * is controlled by ExplainOpenGroup/ExplainCloseGroup.
 */
static void
ExplainNode(PlanState *planstate, List *ancestors, const char *relationship, const char *plan_name, ExplainState *es)
{




















































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































}

/*
 * Show the targetlist of a plan node
 */
static void
show_plan_tlist(PlanState *planstate, List *ancestors, ExplainState *es)
{























































}

/*
 * Show a generic expression
 */
static void
show_expression(Node *node, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es)
{











}

/*
 * Show a qualifier expression (which is a List with implicit AND semantics)
 */
static void
show_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es)
{













}

/*
 * Show a qualifier expression for a scan plan node
 */
static void
show_scan_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es)
{




}

/*
 * Show a qualifier expression for an upper-level plan node
 */
static void
show_upper_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es)
{




}

/*
 * Show the sort keys for a Sort node.
 */
static void
show_sort_keys(SortState *sortstate, List *ancestors, ExplainState *es)
{



}

/*
 * Likewise, for a MergeAppend node.
 */
static void
show_merge_append_keys(MergeAppendState *mstate, List *ancestors, ExplainState *es)
{



}

/*
 * Show the grouping keys for an Agg node.
 */
static void
show_agg_keys(AggState *astate, List *ancestors, ExplainState *es)
{


















}

static void
show_grouping_sets(PlanState *planstate, Agg *agg, List *ancestors, ExplainState *es)
{





















}

static void
show_grouping_set_keys(PlanState *planstate, Agg *aggnode, Sort *sortnode, List *context, bool useprefix, List *ancestors, ExplainState *es)
{







































































}

/*
 * Show the grouping keys for a Group node.
 */
static void
show_group_keys(GroupState *gstate, List *ancestors, ExplainState *es)
{






}

/*
 * Common code to show sort/group keys, which are represented in plan nodes
 * as arrays of targetlist indexes.  If it's a sort key rather than a group
 * key, also pass sort operators/collations/nullsFirst arrays.
 */
static void
show_sort_group_keys(PlanState *planstate, const char *qlabel, int nkeys, AttrNumber *keycols, Oid *sortOperators, Oid *collations, bool *nullsFirst, List *ancestors, ExplainState *es)
{











































}

/*
 * Append nondefault characteristics of the sort ordering of a column to buf
 * (collation, direction, NULLS FIRST/LAST)
 */
static void
show_sortorder_options(StringInfo buf, Node *sortexpr, Oid sortOperator, Oid collation, bool nullsFirst)
{




















































}

/*
 * Show TABLESAMPLE properties
 */
static void
show_tablesample(TableSampleClause *tsc, PlanState *planstate, List *ancestors, ExplainState *es)
{






























































}

/*
 * If it's EXPLAIN ANALYZE, show tuplesort stats for a sort node
 */
static void
show_sort_info(SortState *sortstate, ExplainState *es)
{













































































}

/*
 * Show information on hash buckets/batches.
 */
static void
show_hash_info(HashState *hashstate, ExplainState *es)
{

















































































}

/*
 * If it's EXPLAIN ANALYZE, show exact/lossy pages for a BitmapHeapScan node
 */
static void
show_tidbitmap_info(BitmapHeapScanState *planstate, ExplainState *es)
{






















}

/*
 * If it's EXPLAIN ANALYZE, show instrumentation information for a plan node
 *
 * "which" identifies which instrumentation counter to print
 */
static void
show_instrumentation_count(const char *qlabel, int which, PlanState *planstate, ExplainState *es)
{






























}

/*
 * Show extra information for a ForeignScan node.
 */
static void
show_foreignscan_info(ForeignScanState *fsstate, ExplainState *es)
{

















}

/*
 * Show initplan params evaluated at Gather or Gather Merge node.
 */
static void
show_eval_params(Bitmapset *bms_params, ExplainState *es)
{

















}

/*
 * Fetch the name of an index in an EXPLAIN
 *
 * We allow plugins to get control here so that plans involving hypothetical
 * indexes can be explained.
 *
 * Note: names returned by this function should be "raw"; the caller will
 * apply quoting if needed.  Formerly the convention was to do quoting here,
 * but we don't want that in non-text output formats.
 */
static const char *
explain_get_index_name(Oid indexId)
{




















}

/*
 * Show buffer usage details.
 */
static void
show_buffer_usage(ExplainState *es, const BufferUsage *usage)
{














































































































}

/*
 * Add some additional details about an IndexScan or IndexOnlyScan
 */
static void
ExplainIndexScanDetails(Oid indexid, ScanDirection indexorderdir, ExplainState *es)
{
































}

/*
 * Show the target of a Scan node
 */
static void
ExplainScanTarget(Scan *plan, ExplainState *es)
{

}

/*
 * Show the target of a ModifyTable node
 *
 * Here we show the nominal target (ie, the relation that was named in the
 * original query).  If the actual target(s) is/are different, we'll show them
 * in show_modifytable_info().
 */
static void
ExplainModifyTarget(ModifyTable *plan, ExplainState *es)
{

}

/*
 * Show the target relation of a scan or modify node
 */
static void
ExplainTargetRel(Plan *plan, Index rti, ExplainState *es)
{
























































































































}

/*
 * Show extra information for a ModifyTable node
 *
 * We have three objectives here.  First, if there's more than one target
 * table or it's different from the nominal target, identify the actual
 * target(s).  Second, give FDWs a chance to display extra info about foreign
 * targets.  Third, show information about ON CONFLICT.
 */
static void
show_modifytable_info(ModifyTableState *mtstate, List *ancestors, ExplainState *es)
{










































































































































}

/*
 * Explain the constituent plans of a ModifyTable, Append, MergeAppend,
 * BitmapAnd, or BitmapOr node.
 *
 * The ancestors list should already contain the immediate parent of these
 * plans.
 */
static void
ExplainMemberNodes(PlanState **planstates, int nplans, List *ancestors, ExplainState *es)
{






}

/*
 * Report about any pruned subnodes of an Append or MergeAppend node.
 *
 * nplans indicates the number of live subplans.
 * nchildren indicates the original number of subnodes in the Plan;
 * some of these may have been pruned by the run-time pruning code.
 */
static void
ExplainMissingMembers(int nplans, int nchildren, ExplainState *es)
{




}

/*
 * Explain a list of SubPlans (or initPlans, which also use SubPlan nodes).
 *
 * The ancestors list should already contain the immediate parent of these
 * SubPlanStates.
 */
static void
ExplainSubPlans(List *plans, List *ancestors, const char *relationship, ExplainState *es)
{

























}

/*
 * Explain a list of children of a CustomScan.
 */
static void
ExplainCustomChildren(CustomScanState *css, List *ancestors, ExplainState *es)
{







}

/*
 * Explain a property, such as sort keys or targets, that takes the form of
 * a list of unlabeled items.  "data" is a list of C strings.
 */
void
ExplainPropertyList(const char *qlabel, List *data, ExplainState *es)
{

































































}

/*
 * Explain a property that takes the form of a list of unlabeled items within
 * another list.  "data" is a list of C strings.
 */
void
ExplainPropertyListNested(const char *qlabel, List *data, ExplainState *es)
{









































}

/*
 * Explain a simple property.
 *
 * If "numeric" is true, the value is a number (or other value that
 * doesn't need quoting in JSON).
 *
 * If unit is non-NULL the text format will display it after the value.
 *
 * This usually should not be invoked directly, but via one of the datatype
 * specific routines ExplainPropertyText, ExplainPropertyInteger, etc.
 */
static void
ExplainProperty(const char *qlabel, const char *unit, const char *value, bool numeric, ExplainState *es)
{
























































}

/*
 * Explain a string-valued property.
 */
void
ExplainPropertyText(const char *qlabel, const char *value, ExplainState *es)
{

}

/*
 * Explain an integer-valued property.
 */
void
ExplainPropertyInteger(const char *qlabel, const char *unit, int64 value, ExplainState *es)
{




}

/*
 * Explain a float-valued property, using the specified number of
 * fractional digits.
 */
void
ExplainPropertyFloat(const char *qlabel, const char *unit, double value, int ndigits, ExplainState *es)
{





}

/*
 * Explain a bool-valued property.
 */
void
ExplainPropertyBool(const char *qlabel, bool value, ExplainState *es)
{

}

/*
 * Open a group of related objects.
 *
 * objtype is the type of the group object, labelname is its label within
 * a containing object (if any).
 *
 * If labeled is true, the group members will be labeled properties,
 * while if it's false, they'll be unlabeled objects.
 */
void
ExplainOpenGroup(const char *objtype, const char *labelname, bool labeled, ExplainState *es)
{





















































}

/*
 * Close a group of related objects.
 * Parameters must match the corresponding ExplainOpenGroup call.
 */
void
ExplainCloseGroup(const char *objtype, const char *labelname, bool labeled, ExplainState *es)
{
























}

/*
 * Emit a "dummy" group that never has any members.
 *
 * objtype is the type of the group object, labelname is its label within
 * a containing object (if any).
 */
static void
ExplainDummyGroup(const char *objtype, const char *labelname, ExplainState *es)
{



































}

/*
 * Emit the start-of-output boilerplate.
 *
 * This is just enough different from processing a subgroup that we need
 * a separate pair of subroutines.
 */
void
ExplainBeginOutput(ExplainState *es)
{






















}

/*
 * Emit the end-of-output boilerplate.
 */
void
ExplainEndOutput(ExplainState *es)
{





















}

/*
 * Put an appropriate separator between multiple plans
 */
void
ExplainSeparatePlans(ExplainState *es)
{













}

/*
 * Emit opening or closing XML tag.
 *
 * "flags" must contain X_OPENING, X_CLOSING, or X_CLOSE_IMMEDIATE.
 * Optionally, OR in X_NOWHITESPACE to suppress the whitespace we'd normally
 * add.
 *
 * XML restricts tag names more than our other output formats, eg they can't
 * contain white space or slashes.  Replace invalid characters with dashes,
 * so that for example "I/O Read Time" becomes "I-O-Read-Time".
 */
static void
ExplainXMLTag(const char *tagname, int flags, ExplainState *es)
{

























}

/*
 * Emit a JSON line ending.
 *
 * JSON requires a comma after each property but the last.  To facilitate this,
 * in JSON format, the text emitted for each property begins just prior to the
 * preceding line-break (and comma, if applicable).
 */
static void
ExplainJSONLineEnding(ExplainState *es)
{










}

/*
 * Indent a YAML line.
 *
 * YAML lines are ordinarily indented by two spaces per indentation level.
 * The text emitted for each property begins just prior to the preceding
 * line-break, except for the first property in an unlabelled group, for which
 * it begins immediately after the "- " that introduces the group.  The first
 * property of the group appears on the same line as the opening "- ".
 */
static void
ExplainYAMLLineStarting(ExplainState *es)
{










}

/*
 * YAML is a superset of JSON; unfortunately, the YAML quoting rules are
 * ridiculously complicated -- as documented in sections 5.3 and 7.3.3 of
 * http://yaml.org/spec/1.2/spec.html -- so we chose to just quote everything.
 * Empty strings, strings with leading or trailing whitespace, and strings
 * containing a variety of special characters must certainly be quoted or the
 * output is invalid; and other seemingly harmless strings like "0xa" or
 * "true" must be quoted, lest they be interpreted as a hexadecimal or Boolean
 * constant rather than a string.
 */
static void
escape_yaml(StringInfo buf, const char *str)
{

}