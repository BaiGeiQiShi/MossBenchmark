                                                                            
   
               
                                                                
                 
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "access/amapi.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_am.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_language.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_partitioned_table.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/tablespace.h"
#include "common/keywords.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_node.h"
#include "parser/parse_agg.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parser.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rewriteSupport.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "utils/varlena.h"
#include "utils/xml.h"

              
                               
              
   

                   
#define PRETTYINDENT_STD 8
#define PRETTYINDENT_JOIN 4
#define PRETTYINDENT_VAR 4

#define PRETTYINDENT_LIMIT 40                 

                  
#define PRETTYFLAG_PAREN 0x0001
#define PRETTYFLAG_INDENT 0x0002
#define PRETTYFLAG_SCHEMA 0x0004

                                                                        
#define WRAP_COLUMN_DEFAULT 0

                                            
#define PRETTY_PAREN(context) ((context)->prettyFlags & PRETTYFLAG_PAREN)
#define PRETTY_INDENT(context) ((context)->prettyFlags & PRETTYFLAG_INDENT)
#define PRETTY_SCHEMA(context) ((context)->prettyFlags & PRETTYFLAG_SCHEMA)

              
                    
              
   

                                                                            
typedef struct
{
  StringInfo buf;                                                 
  List *namespaces;                                                    
  List *windowClause;                                                      
  List *windowTList;                                                          
  int prettyFlags;                                                        
  int wrapColumn;                                                          
  int indentLevel;                                                          
  bool varprefix;                                                     
  ParseExprKind special_exprkind;                                           
                                                
} deparse_context;

   
                                                                                
                                                                           
                                          
   
                                                                          
                                        
   
                                                                         
                                                             
                                                                        
   
                                                                           
                                                                          
                                                                              
                     
   
                                                                        
                                                                      
                                                                        
                                                                      
                                                                             
                                                                              
                                                                               
                                                                               
                                               
   
typedef struct
{
  List *rtable;                                          
  List *rtable_names;                                        
  List *rtable_columns;                                               
  List *ctes;                                              
                                              
  bool unique_using;                                                
  List *using_names;                                               
                                                                  
  PlanState *planstate;                                                   
  List *ancestors;                                        
  PlanState *outer_planstate;                                           
  PlanState *inner_planstate;                                           
  List *outer_tlist;                                           
  List *inner_tlist;                                           
  List *index_tlist;                                           
} deparse_namespace;

   
                                               
   
                                                                             
                                                                               
                                                                                
                                                                               
   
                                                                          
                                                                             
                                                                         
                                                                            
                                                                 
   
                                                                              
                                                                            
                                                                               
                                                                            
                                                                               
                                                                           
                                                                              
                                                       
   
                                                                          
                                                                         
                                                                          
                                                                      
                                                                           
                                                                               
                                                                               
                                                                             
                                                                        
   
                                                                              
                                                                            
                                                                               
                                                                              
                                                                               
                                                                           
                                                                           
   
typedef struct
{
     
                                                                            
                                                                            
                                                                         
     
                                                                          
                                                                            
                              
     
                                                                            
                                                                             
                                                                            
                                                                             
                            
     
  int num_cols;                                    
  char **colnames;                                   

     
                                                                           
                                                                     
                                                                          
                                                                           
                                                                           
                                                                             
                                                                             
                                                                            
                                                                     
                                                                          
                                                                     
                                            
     
  int num_new_cols;                                        
  char **new_colnames;                         
  bool *is_new_col;                             

                                                                            
  bool printaliases;

                                                                           
  List *parentUsing;                                              

     
                                                                       
                                                                            
     
                                                                       
                                                                           
                                                                          
                                                                        
                                                                         
                                                                            
                                                          
     
                                                                             
                                                                            
                                                       
     
  int leftrti;                                          
  int rightrti;                                          
  int *leftattnos;                                               
  int *rightattnos;                                               
  List *usingNames;                                       
} deparse_columns;

                                                                            
#define deparse_columns_fetch(rangetable_index, dpns) ((deparse_columns *)list_nth((dpns)->rtable_columns, (rangetable_index)-1))

   
                                         
   
typedef struct
{
  char name[NAMEDATALEN];                                 
  int counter;                                                       
} NameHashEntry;

              
               
              
   
static SPIPlanPtr plan_getrulebyoid = NULL;
static const char *query_getrulebyoid = "SELECT * FROM pg_catalog.pg_rewrite WHERE oid = $1";
static SPIPlanPtr plan_getviewrule = NULL;
static const char *query_getviewrule = "SELECT * FROM pg_catalog.pg_rewrite WHERE ev_class = $1 AND rulename = $2";

                    
bool quote_all_identifiers = false;

              
                   
   
                                                                         
                                                                       
                                                                 
              
   
static char *
deparse_expression_pretty(Node *expr, List *dpcontext, bool forceprefix, bool showimplicit, int prettyFlags, int startIndent);
static char *
pg_get_viewdef_worker(Oid viewoid, int prettyFlags, int wrapColumn);
static char *
pg_get_triggerdef_worker(Oid trigid, bool pretty);
static int
decompile_column_index_array(Datum column_index_array, Oid relId, StringInfo buf);
static char *
pg_get_ruledef_worker(Oid ruleoid, int prettyFlags);
static char *
pg_get_indexdef_worker(Oid indexrelid, int colno, const Oid *excludeOps, bool attrsOnly, bool keysOnly, bool showTblSpc, bool inherits, int prettyFlags, bool missing_ok);
static char *
pg_get_statisticsobj_worker(Oid statextid, bool missing_ok);
static char *
pg_get_partkeydef_worker(Oid relid, int prettyFlags, bool attrsOnly, bool missing_ok);
static char *
pg_get_constraintdef_worker(Oid constraintId, bool fullCommand, int prettyFlags, bool missing_ok);
static text *
pg_get_expr_worker(text *expr, Oid relid, const char *relname, int prettyFlags);
static int
print_function_arguments(StringInfo buf, HeapTuple proctup, bool print_table_args, bool print_defaults);
static void
print_function_rettype(StringInfo buf, HeapTuple proctup);
static void
print_function_trftypes(StringInfo buf, HeapTuple proctup);
static void
set_rtable_names(deparse_namespace *dpns, List *parent_namespaces, Bitmapset *rels_used);
static void
set_deparse_for_query(deparse_namespace *dpns, Query *query, List *parent_namespaces);
static void
set_simple_column_names(deparse_namespace *dpns);
static bool
has_dangerous_join_using(deparse_namespace *dpns, Node *jtnode);
static void
set_using_names(deparse_namespace *dpns, Node *jtnode, List *parentUsing);
static void
set_relation_column_names(deparse_namespace *dpns, RangeTblEntry *rte, deparse_columns *colinfo);
static void
set_join_column_names(deparse_namespace *dpns, RangeTblEntry *rte, deparse_columns *colinfo);
static bool
colname_is_unique(const char *colname, deparse_namespace *dpns, deparse_columns *colinfo);
static char *
make_colname_unique(char *colname, deparse_namespace *dpns, deparse_columns *colinfo);
static void
expand_colnames_array_to(deparse_columns *colinfo, int n);
static void
identify_join_columns(JoinExpr *j, RangeTblEntry *jrte, deparse_columns *colinfo);
static void
flatten_join_using_qual(Node *qual, List **leftvars, List **rightvars);
static char *
get_rtable_name(int rtindex, deparse_context *context);
static void
set_deparse_planstate(deparse_namespace *dpns, PlanState *ps);
static void
push_child_plan(deparse_namespace *dpns, PlanState *ps, deparse_namespace *save_dpns);
static void
pop_child_plan(deparse_namespace *dpns, deparse_namespace *save_dpns);
static void
push_ancestor_plan(deparse_namespace *dpns, ListCell *ancestor_cell, deparse_namespace *save_dpns);
static void
pop_ancestor_plan(deparse_namespace *dpns, deparse_namespace *save_dpns);
static void
make_ruledef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc, int prettyFlags);
static void
make_viewdef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc, int prettyFlags, int wrapColumn);
static void
get_query_def(Query *query, StringInfo buf, List *parentnamespace, TupleDesc resultDesc, bool colNamesVisible, int prettyFlags, int wrapColumn, int startIndent);
static void
get_values_def(List *values_lists, deparse_context *context);
static void
get_with_clause(Query *query, deparse_context *context);
static void
get_select_query_def(Query *query, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible);
static void
get_insert_query_def(Query *query, deparse_context *context, bool colNamesVisible);
static void
get_update_query_def(Query *query, deparse_context *context, bool colNamesVisible);
static void
get_update_query_targetlist_def(Query *query, List *targetList, deparse_context *context, RangeTblEntry *rte);
static void
get_delete_query_def(Query *query, deparse_context *context, bool colNamesVisible);
static void
get_utility_query_def(Query *query, deparse_context *context);
static void
get_basic_select_query(Query *query, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible);
static void
get_target_list(List *targetList, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible);
static void
get_setop_query(Node *setOp, Query *query, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible);
static Node *
get_rule_sortgroupclause(Index ref, List *tlist, bool force_colno, deparse_context *context);
static void
get_rule_groupingset(GroupingSet *gset, List *targetlist, bool omit_parens, deparse_context *context);
static void
get_rule_orderby(List *orderList, List *targetList, bool force_colno, deparse_context *context);
static void
get_rule_windowclause(Query *query, deparse_context *context);
static void
get_rule_windowspec(WindowClause *wc, List *targetList, deparse_context *context);
static char *
get_variable(Var *var, int levelsup, bool istoplevel, deparse_context *context);
static void
get_special_variable(Node *node, deparse_context *context, void *private);
static void
resolve_special_varno(Node *node, deparse_context *context, void *private, void (*callback)(Node *, deparse_context *, void *));
static Node *
find_param_referent(Param *param, deparse_context *context, deparse_namespace **dpns_p, ListCell **ancestor_cell_p);
static void
get_parameter(Param *param, deparse_context *context);
static const char *
get_simple_binary_op_name(OpExpr *expr);
static bool
isSimpleNode(Node *node, Node *parentNode, int prettyFlags);
static void
appendContextKeyword(deparse_context *context, const char *str, int indentBefore, int indentAfter, int indentPlus);
static void
removeStringInfoSpaces(StringInfo str);
static void
get_rule_expr(Node *node, deparse_context *context, bool showimplicit);
static void
get_rule_expr_toplevel(Node *node, deparse_context *context, bool showimplicit);
static void
get_rule_list_toplevel(List *lst, deparse_context *context, bool showimplicit);
static void
get_rule_expr_funccall(Node *node, deparse_context *context, bool showimplicit);
static bool
looks_like_function(Node *node);
static void
get_oper_expr(OpExpr *expr, deparse_context *context);
static void
get_func_expr(FuncExpr *expr, deparse_context *context, bool showimplicit);
static void
get_agg_expr(Aggref *aggref, deparse_context *context, Aggref *original_aggref);
static void
get_agg_combine_expr(Node *node, deparse_context *context, void *private);
static void
get_windowfunc_expr(WindowFunc *wfunc, deparse_context *context);
static void
get_coercion_expr(Node *arg, deparse_context *context, Oid resulttype, int32 resulttypmod, Node *parentNode);
static void
get_const_expr(Const *constval, deparse_context *context, int showtype);
static void
get_const_collation(Const *constval, deparse_context *context);
static void
simple_quote_literal(StringInfo buf, const char *val);
static void
get_sublink_expr(SubLink *sublink, deparse_context *context);
static void
get_tablefunc(TableFunc *tf, deparse_context *context, bool showimplicit);
static void
get_from_clause(Query *query, const char *prefix, deparse_context *context);
static void
get_from_clause_item(Node *jtnode, Query *query, deparse_context *context);
static void
get_column_alias_list(deparse_columns *colinfo, deparse_context *context);
static void
get_from_clause_coldeflist(RangeTblFunction *rtfunc, deparse_columns *colinfo, deparse_context *context);
static void
get_tablesample_def(TableSampleClause *tablesample, deparse_context *context);
static void
get_opclass_name(Oid opclass, Oid actual_datatype, StringInfo buf);
static Node *
processIndirection(Node *node, deparse_context *context);
static void
printSubscripts(SubscriptingRef *sbsref, deparse_context *context);
static char *
get_relation_name(Oid relid);
static char *
generate_relation_name(Oid relid, List *namespaces);
static char *
generate_qualified_relation_name(Oid relid);
static char *
generate_function_name(Oid funcid, int nargs, List *argnames, Oid *argtypes, bool has_variadic, bool *use_variadic_p, ParseExprKind special_exprkind);
static char *
generate_operator_name(Oid operid, Oid arg1, Oid arg2);
static void
add_cast_to(StringInfo buf, Oid typid);
static char *
generate_qualified_type_name(Oid typid);
static text *
string_to_text(char *str);
static char *
flatten_reloptions(Oid relid);

#define only_marker(rte) ((rte)->inh ? "" : "ONLY ")

              
                                               
                                          
                             
              
   
Datum
pg_get_ruledef(PG_FUNCTION_ARGS)
{
  Oid ruleoid = PG_GETARG_OID(0);
  int prettyFlags;
  char *res;

  prettyFlags = PRETTYFLAG_INDENT;

  res = pg_get_ruledef_worker(ruleoid, prettyFlags);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_ruledef_ext(PG_FUNCTION_ARGS)
{
  Oid ruleoid = PG_GETARG_OID(0);
  bool pretty = PG_GETARG_BOOL(1);
  int prettyFlags;
  char *res;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  res = pg_get_ruledef_worker(ruleoid, prettyFlags);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

static char *
pg_get_ruledef_worker(Oid ruleoid, int prettyFlags)
{
  Datum args[1];
  char nulls[1];
  int spirc;
  HeapTuple ruletup;
  TupleDesc rulettc;
  StringInfoData buf;

     
                                                                         
     
  initStringInfo(&buf);

     
                            
     
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                      
                                                                         
                                            
     
  if (plan_getrulebyoid == NULL)
  {
    Oid argtypes[1];
    SPIPlanPtr plan;

    argtypes[0] = OIDOID;
    plan = SPI_prepare(query_getrulebyoid, 1, argtypes);
    if (plan == NULL)
    {
      elog(ERROR, "SPI_prepare failed for \"%s\"", query_getrulebyoid);
    }
    SPI_keepplan(plan);
    plan_getrulebyoid = plan;
  }

     
                                            
     
  args[0] = ObjectIdGetDatum(ruleoid);
  nulls[0] = ' ';
  spirc = SPI_execute_plan(plan_getrulebyoid, args, nulls, true, 0);
  if (spirc != SPI_OK_SELECT)
  {
    elog(ERROR, "failed to get pg_rewrite tuple for rule %u", ruleoid);
  }
  if (SPI_processed != 1)
  {
       
                                                                          
              
       
  }
  else
  {
       
                                                                   
       
    ruletup = SPI_tuptable->vals[0];
    rulettc = SPI_tuptable->tupdesc;
    make_ruledef(&buf, ruletup, rulettc, prettyFlags);
  }

     
                                 
     
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  if (buf.len == 0)
  {
    return NULL;
  }

  return buf.data;
}

              
                                                 
                                              
              
   
Datum
pg_get_viewdef(PG_FUNCTION_ARGS)
{
              
  Oid viewoid = PG_GETARG_OID(0);
  int prettyFlags;
  char *res;

  prettyFlags = PRETTYFLAG_INDENT;

  res = pg_get_viewdef_worker(viewoid, prettyFlags, WRAP_COLUMN_DEFAULT);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_viewdef_ext(PG_FUNCTION_ARGS)
{
              
  Oid viewoid = PG_GETARG_OID(0);
  bool pretty = PG_GETARG_BOOL(1);
  int prettyFlags;
  char *res;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  res = pg_get_viewdef_worker(viewoid, prettyFlags, WRAP_COLUMN_DEFAULT);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_viewdef_wrap(PG_FUNCTION_ARGS)
{
              
  Oid viewoid = PG_GETARG_OID(0);
  int wrap = PG_GETARG_INT32(1);
  int prettyFlags;
  char *res;

                                                    
  prettyFlags = PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA;

  res = pg_get_viewdef_worker(viewoid, prettyFlags, wrap);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_viewdef_name(PG_FUNCTION_ARGS)
{
                         
  text *viewname = PG_GETARG_TEXT_PP(0);
  int prettyFlags;
  RangeVar *viewrel;
  Oid viewoid;
  char *res;

  prettyFlags = PRETTYFLAG_INDENT;

                                                                         
  viewrel = makeRangeVarFromNameList(textToQualifiedNameList(viewname));
  viewoid = RangeVarGetRelid(viewrel, NoLock, false);

  res = pg_get_viewdef_worker(viewoid, prettyFlags, WRAP_COLUMN_DEFAULT);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_viewdef_name_ext(PG_FUNCTION_ARGS)
{
                         
  text *viewname = PG_GETARG_TEXT_PP(0);
  bool pretty = PG_GETARG_BOOL(1);
  int prettyFlags;
  RangeVar *viewrel;
  Oid viewoid;
  char *res;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

                                                                         
  viewrel = makeRangeVarFromNameList(textToQualifiedNameList(viewname));
  viewoid = RangeVarGetRelid(viewrel, NoLock, false);

  res = pg_get_viewdef_worker(viewoid, prettyFlags, WRAP_COLUMN_DEFAULT);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

   
                                                                 
   
static char *
pg_get_viewdef_worker(Oid viewoid, int prettyFlags, int wrapColumn)
{
  Datum args[2];
  char nulls[2];
  int spirc;
  HeapTuple ruletup;
  TupleDesc rulettc;
  StringInfoData buf;

     
                                                                         
     
  initStringInfo(&buf);

     
                            
     
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                      
                                                                         
                                            
     
  if (plan_getviewrule == NULL)
  {
    Oid argtypes[2];
    SPIPlanPtr plan;

    argtypes[0] = OIDOID;
    argtypes[1] = NAMEOID;
    plan = SPI_prepare(query_getviewrule, 2, argtypes);
    if (plan == NULL)
    {
      elog(ERROR, "SPI_prepare failed for \"%s\"", query_getviewrule);
    }
    SPI_keepplan(plan);
    plan_getviewrule = plan;
  }

     
                                                         
     
  args[0] = ObjectIdGetDatum(viewoid);
  args[1] = DirectFunctionCall1(namein, CStringGetDatum(ViewSelectRuleName));
  nulls[0] = ' ';
  nulls[1] = ' ';
  spirc = SPI_execute_plan(plan_getviewrule, args, nulls, true, 0);
  if (spirc != SPI_OK_SELECT)
  {
    elog(ERROR, "failed to get pg_rewrite tuple for view %u", viewoid);
  }
  if (SPI_processed != 1)
  {
       
                                                                          
              
       
  }
  else
  {
       
                                                                   
       
    ruletup = SPI_tuptable->vals[0];
    rulettc = SPI_tuptable->tupdesc;
    make_viewdef(&buf, ruletup, rulettc, prettyFlags, wrapColumn);
  }

     
                                 
     
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  if (buf.len == 0)
  {
    return NULL;
  }

  return buf.data;
}

              
                                                      
              
   
Datum
pg_get_triggerdef(PG_FUNCTION_ARGS)
{
  Oid trigid = PG_GETARG_OID(0);
  char *res;

  res = pg_get_triggerdef_worker(trigid, false);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_triggerdef_ext(PG_FUNCTION_ARGS)
{
  Oid trigid = PG_GETARG_OID(0);
  bool pretty = PG_GETARG_BOOL(1);
  char *res;

  res = pg_get_triggerdef_worker(trigid, pretty);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

static char *
pg_get_triggerdef_worker(Oid trigid, bool pretty)
{
  HeapTuple ht_trig;
  Form_pg_trigger trigrec;
  StringInfoData buf;
  Relation tgrel;
  ScanKeyData skey[1];
  SysScanDesc tgscan;
  int findx = 0;
  char *tgname;
  char *tgoldtable;
  char *tgnewtable;
  Oid argtypes[1];            
  Datum value;
  bool isnull;

     
                                                          
     
  tgrel = table_open(TriggerRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_trigger_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(trigid));

  tgscan = systable_beginscan(tgrel, TriggerOidIndexId, true, NULL, 1, skey);

  ht_trig = systable_getnext(tgscan);

  if (!HeapTupleIsValid(ht_trig))
  {
    systable_endscan(tgscan);
    table_close(tgrel, AccessShareLock);
    return NULL;
  }

  trigrec = (Form_pg_trigger)GETSTRUCT(ht_trig);

     
                                                                             
                                                             
     
  initStringInfo(&buf);

  tgname = NameStr(trigrec->tgname);
  appendStringInfo(&buf, "CREATE %sTRIGGER %s ", OidIsValid(trigrec->tgconstraint) ? "CONSTRAINT " : "", quote_identifier(tgname));

  if (TRIGGER_FOR_BEFORE(trigrec->tgtype))
  {
    appendStringInfoString(&buf, "BEFORE");
  }
  else if (TRIGGER_FOR_AFTER(trigrec->tgtype))
  {
    appendStringInfoString(&buf, "AFTER");
  }
  else if (TRIGGER_FOR_INSTEAD(trigrec->tgtype))
  {
    appendStringInfoString(&buf, "INSTEAD OF");
  }
  else
  {
    elog(ERROR, "unexpected tgtype value: %d", trigrec->tgtype);
  }

  if (TRIGGER_FOR_INSERT(trigrec->tgtype))
  {
    appendStringInfoString(&buf, " INSERT");
    findx++;
  }
  if (TRIGGER_FOR_DELETE(trigrec->tgtype))
  {
    if (findx > 0)
    {
      appendStringInfoString(&buf, " OR DELETE");
    }
    else
    {
      appendStringInfoString(&buf, " DELETE");
    }
    findx++;
  }
  if (TRIGGER_FOR_UPDATE(trigrec->tgtype))
  {
    if (findx > 0)
    {
      appendStringInfoString(&buf, " OR UPDATE");
    }
    else
    {
      appendStringInfoString(&buf, " UPDATE");
    }
    findx++;
                                                                   
    if (trigrec->tgattr.dim1 > 0)
    {
      int i;

      appendStringInfoString(&buf, " OF ");
      for (i = 0; i < trigrec->tgattr.dim1; i++)
      {
        char *attname;

        if (i > 0)
        {
          appendStringInfoString(&buf, ", ");
        }
        attname = get_attname(trigrec->tgrelid, trigrec->tgattr.values[i], false);
        appendStringInfoString(&buf, quote_identifier(attname));
      }
    }
  }
  if (TRIGGER_FOR_TRUNCATE(trigrec->tgtype))
  {
    if (findx > 0)
    {
      appendStringInfoString(&buf, " OR TRUNCATE");
    }
    else
    {
      appendStringInfoString(&buf, " TRUNCATE");
    }
    findx++;
  }

     
                                                                         
                                                                  
     
  appendStringInfo(&buf, " ON %s ", pretty ? generate_relation_name(trigrec->tgrelid, NIL) : generate_qualified_relation_name(trigrec->tgrelid));

  if (OidIsValid(trigrec->tgconstraint))
  {
    if (OidIsValid(trigrec->tgconstrrelid))
    {
      appendStringInfo(&buf, "FROM %s ", generate_relation_name(trigrec->tgconstrrelid, NIL));
    }
    if (!trigrec->tgdeferrable)
    {
      appendStringInfoString(&buf, "NOT ");
    }
    appendStringInfoString(&buf, "DEFERRABLE INITIALLY ");
    if (trigrec->tginitdeferred)
    {
      appendStringInfoString(&buf, "DEFERRED ");
    }
    else
    {
      appendStringInfoString(&buf, "IMMEDIATE ");
    }
  }

  value = fastgetattr(ht_trig, Anum_pg_trigger_tgoldtable, tgrel->rd_att, &isnull);
  if (!isnull)
  {
    tgoldtable = NameStr(*DatumGetName(value));
  }
  else
  {
    tgoldtable = NULL;
  }
  value = fastgetattr(ht_trig, Anum_pg_trigger_tgnewtable, tgrel->rd_att, &isnull);
  if (!isnull)
  {
    tgnewtable = NameStr(*DatumGetName(value));
  }
  else
  {
    tgnewtable = NULL;
  }
  if (tgoldtable != NULL || tgnewtable != NULL)
  {
    appendStringInfoString(&buf, "REFERENCING ");
    if (tgoldtable != NULL)
    {
      appendStringInfo(&buf, "OLD TABLE AS %s ", quote_identifier(tgoldtable));
    }
    if (tgnewtable != NULL)
    {
      appendStringInfo(&buf, "NEW TABLE AS %s ", quote_identifier(tgnewtable));
    }
  }

  if (TRIGGER_FOR_ROW(trigrec->tgtype))
  {
    appendStringInfoString(&buf, "FOR EACH ROW ");
  }
  else
  {
    appendStringInfoString(&buf, "FOR EACH STATEMENT ");
  }

                                                         
  value = fastgetattr(ht_trig, Anum_pg_trigger_tgqual, tgrel->rd_att, &isnull);
  if (!isnull)
  {
    Node *qual;
    char relkind;
    deparse_context context;
    deparse_namespace dpns;
    RangeTblEntry *oldrte;
    RangeTblEntry *newrte;

    appendStringInfoString(&buf, "WHEN (");

    qual = stringToNode(TextDatumGetCString(value));

    relkind = get_rel_relkind(trigrec->tgrelid);

                                                    
    oldrte = makeNode(RangeTblEntry);
    oldrte->rtekind = RTE_RELATION;
    oldrte->relid = trigrec->tgrelid;
    oldrte->relkind = relkind;
    oldrte->rellockmode = AccessShareLock;
    oldrte->alias = makeAlias("old", NIL);
    oldrte->eref = oldrte->alias;
    oldrte->lateral = false;
    oldrte->inh = false;
    oldrte->inFromCl = true;

    newrte = makeNode(RangeTblEntry);
    newrte->rtekind = RTE_RELATION;
    newrte->relid = trigrec->tgrelid;
    newrte->relkind = relkind;
    newrte->rellockmode = AccessShareLock;
    newrte->alias = makeAlias("new", NIL);
    newrte->eref = newrte->alias;
    newrte->lateral = false;
    newrte->inh = false;
    newrte->inFromCl = true;

                                  
    memset(&dpns, 0, sizeof(dpns));
    dpns.rtable = list_make2(oldrte, newrte);
    dpns.ctes = NIL;
    set_rtable_names(&dpns, NIL, NULL);
    set_simple_column_names(&dpns);

                                                      
    context.buf = &buf;
    context.namespaces = list_make1(&dpns);
    context.windowClause = NIL;
    context.windowTList = NIL;
    context.varprefix = true;
    context.prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;
    context.wrapColumn = WRAP_COLUMN_DEFAULT;
    context.indentLevel = PRETTYINDENT_STD;
    context.special_exprkind = EXPR_KIND_NONE;

    get_rule_expr(qual, &context, false);

    appendStringInfoString(&buf, ") ");
  }

  appendStringInfo(&buf, "EXECUTE FUNCTION %s(", generate_function_name(trigrec->tgfoid, 0, NIL, argtypes, false, NULL, EXPR_KIND_NONE));

  if (trigrec->tgnargs > 0)
  {
    char *p;
    int i;

    value = fastgetattr(ht_trig, Anum_pg_trigger_tgargs, tgrel->rd_att, &isnull);
    if (isnull)
    {
      elog(ERROR, "tgargs is null for trigger %u", trigid);
    }
    p = (char *)VARDATA_ANY(DatumGetByteaPP(value));
    for (i = 0; i < trigrec->tgnargs; i++)
    {
      if (i > 0)
      {
        appendStringInfoString(&buf, ", ");
      }
      simple_quote_literal(&buf, p);
                                                       
      while (*p)
      {
        p++;
      }
      p++;
    }
  }

                                                    
  appendStringInfoChar(&buf, ')');

                
  systable_endscan(tgscan);

  table_close(tgrel, AccessShareLock);

  return buf.data;
}

              
                                                   
   
                                                                              
                                                       
                                                                          
   
                                                                       
                                                                            
                                                                   
              
   
Datum
pg_get_indexdef(PG_FUNCTION_ARGS)
{
  Oid indexrelid = PG_GETARG_OID(0);
  int prettyFlags;
  char *res;

  prettyFlags = PRETTYFLAG_INDENT;

  res = pg_get_indexdef_worker(indexrelid, 0, NULL, false, false, false, false, prettyFlags, true);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_indexdef_ext(PG_FUNCTION_ARGS)
{
  Oid indexrelid = PG_GETARG_OID(0);
  int32 colno = PG_GETARG_INT32(1);
  bool pretty = PG_GETARG_BOOL(2);
  int prettyFlags;
  char *res;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  res = pg_get_indexdef_worker(indexrelid, colno, NULL, colno != 0, false, false, false, prettyFlags, true);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

   
                                            
                                               
                                                    
   
char *
pg_get_indexdef_string(Oid indexrelid)
{
  return pg_get_indexdef_worker(indexrelid, 0, NULL, false, false, true, true, 0, false);
}

                                                                   
char *
pg_get_indexdef_columns(Oid indexrelid, bool pretty)
{
  int prettyFlags;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  return pg_get_indexdef_worker(indexrelid, 0, NULL, true, true, false, false, prettyFlags, false);
}

   
                                                        
   
                                                                            
                                                               
   
static char *
pg_get_indexdef_worker(Oid indexrelid, int colno, const Oid *excludeOps, bool attrsOnly, bool keysOnly, bool showTblSpc, bool inherits, int prettyFlags, bool missing_ok)
{
                                                          
  bool isConstraint = (excludeOps != NULL);
  HeapTuple ht_idx;
  HeapTuple ht_idxrel;
  HeapTuple ht_am;
  Form_pg_index idxrec;
  Form_pg_class idxrelrec;
  Form_pg_am amrec;
  IndexAmRoutine *amroutine;
  List *indexprs;
  ListCell *indexpr_item;
  List *context;
  Oid indrelid;
  int keyno;
  Datum indcollDatum;
  Datum indclassDatum;
  Datum indoptionDatum;
  bool isnull;
  oidvector *indcollation;
  oidvector *indclass;
  int2vector *indoption;
  StringInfoData buf;
  char *str;
  char *sep;

     
                                                      
     
  ht_idx = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexrelid));
  if (!HeapTupleIsValid(ht_idx))
  {
    if (missing_ok)
    {
      return NULL;
    }
    elog(ERROR, "cache lookup failed for index %u", indexrelid);
  }
  idxrec = (Form_pg_index)GETSTRUCT(ht_idx);

  indrelid = idxrec->indrelid;
  Assert(indexrelid == idxrec->indexrelid);

                                                                   
  indcollDatum = SysCacheGetAttr(INDEXRELID, ht_idx, Anum_pg_index_indcollation, &isnull);
  Assert(!isnull);
  indcollation = (oidvector *)DatumGetPointer(indcollDatum);

  indclassDatum = SysCacheGetAttr(INDEXRELID, ht_idx, Anum_pg_index_indclass, &isnull);
  Assert(!isnull);
  indclass = (oidvector *)DatumGetPointer(indclassDatum);

  indoptionDatum = SysCacheGetAttr(INDEXRELID, ht_idx, Anum_pg_index_indoption, &isnull);
  Assert(!isnull);
  indoption = (int2vector *)DatumGetPointer(indoptionDatum);

     
                                                    
     
  ht_idxrel = SearchSysCache1(RELOID, ObjectIdGetDatum(indexrelid));
  if (!HeapTupleIsValid(ht_idxrel))
  {
    elog(ERROR, "cache lookup failed for relation %u", indexrelid);
  }
  idxrelrec = (Form_pg_class)GETSTRUCT(ht_idxrel);

     
                                                       
     
  ht_am = SearchSysCache1(AMOID, ObjectIdGetDatum(idxrelrec->relam));
  if (!HeapTupleIsValid(ht_am))
  {
    elog(ERROR, "cache lookup failed for access method %u", idxrelrec->relam);
  }
  amrec = (Form_pg_am)GETSTRUCT(ht_am);

                                       
  amroutine = GetIndexAmRoutine(amrec->amhandler);

     
                                                                           
                                                                           
                                    
     
  if (!heap_attisnull(ht_idx, Anum_pg_index_indexprs, NULL))
  {
    Datum exprsDatum;
    bool isnull;
    char *exprsString;

    exprsDatum = SysCacheGetAttr(INDEXRELID, ht_idx, Anum_pg_index_indexprs, &isnull);
    Assert(!isnull);
    exprsString = TextDatumGetCString(exprsDatum);
    indexprs = (List *)stringToNode(exprsString);
    pfree(exprsString);
  }
  else
  {
    indexprs = NIL;
  }

  indexpr_item = list_head(indexprs);

  context = deparse_context_for(get_relation_name(indrelid), indrelid);

     
                                                                             
                                                          
     
  initStringInfo(&buf);

  if (!attrsOnly)
  {
    if (!isConstraint)
    {
      appendStringInfo(&buf, "CREATE %sINDEX %s ON %s%s USING %s (", idxrec->indisunique ? "UNIQUE " : "", quote_identifier(NameStr(idxrelrec->relname)), idxrelrec->relkind == RELKIND_PARTITIONED_INDEX && !inherits ? "ONLY " : "", (prettyFlags & PRETTYFLAG_SCHEMA) ? generate_relation_name(indrelid, NIL) : generate_qualified_relation_name(indrelid), quote_identifier(NameStr(amrec->amname)));
    }
    else                                            
    {
      appendStringInfo(&buf, "EXCLUDE USING %s (", quote_identifier(NameStr(amrec->amname)));
    }
  }

     
                                   
     
  sep = "";
  for (keyno = 0; keyno < idxrec->indnatts; keyno++)
  {
    AttrNumber attnum = idxrec->indkey.values[keyno];
    Oid keycoltype;
    Oid keycolcollation;

       
                                             
       
    if (keysOnly && keyno >= idxrec->indnkeyatts)
    {
      break;
    }

                                                                   
    if (!colno && keyno == idxrec->indnkeyatts)
    {
      appendStringInfoString(&buf, ") INCLUDE (");
      sep = "";
    }

    if (!colno)
    {
      appendStringInfoString(&buf, sep);
    }
    sep = ", ";

    if (attnum != 0)
    {
                               
      char *attname;
      int32 keycoltypmod;

      attname = get_attname(indrelid, attnum, false);
      if (!colno || colno == keyno + 1)
      {
        appendStringInfoString(&buf, quote_identifier(attname));
      }
      get_atttypetypmodcoll(indrelid, attnum, &keycoltype, &keycoltypmod, &keycolcollation);
    }
    else
    {
                              
      Node *indexkey;

      if (indexpr_item == NULL)
      {
        elog(ERROR, "too few entries in indexprs list");
      }
      indexkey = (Node *)lfirst(indexpr_item);
      indexpr_item = lnext(indexpr_item);
                   
      str = deparse_expression_pretty(indexkey, context, false, false, prettyFlags, 0);
      if (!colno || colno == keyno + 1)
      {
                                                          
        if (looks_like_function(indexkey))
        {
          appendStringInfoString(&buf, str);
        }
        else
        {
          appendStringInfo(&buf, "(%s)", str);
        }
      }
      keycoltype = exprType(indexkey);
      keycolcollation = exprCollation(indexkey);
    }

                                                                
    if (!attrsOnly && keyno < idxrec->indnkeyatts && (!colno || colno == keyno + 1))
    {
      int16 opt = indoption->values[keyno];
      Oid indcoll = indcollation->values[keyno];

                                                    
      if (OidIsValid(indcoll) && indcoll != keycolcollation)
      {
        appendStringInfo(&buf, " COLLATE %s", generate_collation_name((indcoll)));
      }

                                                       
      get_opclass_name(indclass->values[keyno], keycoltype, &buf);

                                   
      if (amroutine->amcanorder)
      {
                                                                      
        if (opt & INDOPTION_DESC)
        {
          appendStringInfoString(&buf, " DESC");
                                                       
          if (!(opt & INDOPTION_NULLS_FIRST))
          {
            appendStringInfoString(&buf, " NULLS LAST");
          }
        }
        else
        {
          if (opt & INDOPTION_NULLS_FIRST)
          {
            appendStringInfoString(&buf, " NULLS FIRST");
          }
        }
      }

                                                  
      if (excludeOps != NULL)
      {
        appendStringInfo(&buf, " WITH %s", generate_operator_name(excludeOps[keyno], keycoltype, keycoltype));
      }
    }
  }

  if (!attrsOnly)
  {
    appendStringInfoChar(&buf, ')');

       
                                                  
       
    str = flatten_reloptions(indexrelid);
    if (str)
    {
      appendStringInfo(&buf, " WITH (%s)", str);
      pfree(str);
    }

       
                                               
       
    if (showTblSpc)
    {
      Oid tblspc;

      tblspc = get_rel_tablespace(indexrelid);
      if (OidIsValid(tblspc))
      {
        if (isConstraint)
        {
          appendStringInfoString(&buf, " USING INDEX");
        }
        appendStringInfo(&buf, " TABLESPACE %s", quote_identifier(get_tablespace_name(tblspc)));
      }
    }

       
                                                                   
       
    if (!heap_attisnull(ht_idx, Anum_pg_index_indpred, NULL))
    {
      Node *node;
      Datum predDatum;
      bool isnull;
      char *predString;

                                            
      predDatum = SysCacheGetAttr(INDEXRELID, ht_idx, Anum_pg_index_indpred, &isnull);
      Assert(!isnull);
      predString = TextDatumGetCString(predDatum);
      node = (Node *)stringToNode(predString);
      pfree(predString);

                   
      str = deparse_expression_pretty(node, context, false, false, prettyFlags, 0);
      if (isConstraint)
      {
        appendStringInfo(&buf, " WHERE (%s)", str);
      }
      else
      {
        appendStringInfo(&buf, " WHERE %s", str);
      }
    }
  }

                
  ReleaseSysCache(ht_idx);
  ReleaseSysCache(ht_idxrel);
  ReleaseSysCache(ht_am);

  return buf.data;
}

   
                           
                                                        
   
Datum
pg_get_statisticsobjdef(PG_FUNCTION_ARGS)
{
  Oid statextid = PG_GETARG_OID(0);
  char *res;

  res = pg_get_statisticsobj_worker(statextid, true);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

   
                                                                  
   
static char *
pg_get_statisticsobj_worker(Oid statextid, bool missing_ok)
{
  Form_pg_statistic_ext statextrec;
  HeapTuple statexttup;
  StringInfoData buf;
  int colno;
  char *nsp;
  ArrayType *arr;
  char *enabled;
  Datum datum;
  bool isnull;
  bool ndistinct_enabled;
  bool dependencies_enabled;
  bool mcv_enabled;
  int i;

  statexttup = SearchSysCache1(STATEXTOID, ObjectIdGetDatum(statextid));

  if (!HeapTupleIsValid(statexttup))
  {
    if (missing_ok)
    {
      return NULL;
    }
    elog(ERROR, "cache lookup failed for statistics object %u", statextid);
  }

  statextrec = (Form_pg_statistic_ext)GETSTRUCT(statexttup);

  initStringInfo(&buf);

  nsp = get_namespace_name(statextrec->stxnamespace);
  appendStringInfo(&buf, "CREATE STATISTICS %s", quote_qualified_identifier(nsp, NameStr(statextrec->stxname)));

     
                                                                           
     
  datum = SysCacheGetAttr(STATEXTOID, statexttup, Anum_pg_statistic_ext_stxkind, &isnull);
  Assert(!isnull);
  arr = DatumGetArrayTypeP(datum);
  if (ARR_NDIM(arr) != 1 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != CHAROID)
  {
    elog(ERROR, "stxkind is not a 1-D char array");
  }
  enabled = (char *)ARR_DATA_PTR(arr);

  ndistinct_enabled = false;
  dependencies_enabled = false;
  mcv_enabled = false;

  for (i = 0; i < ARR_DIMS(arr)[0]; i++)
  {
    if (enabled[i] == STATS_EXT_NDISTINCT)
    {
      ndistinct_enabled = true;
    }
    if (enabled[i] == STATS_EXT_DEPENDENCIES)
    {
      dependencies_enabled = true;
    }
    if (enabled[i] == STATS_EXT_MCV)
    {
      mcv_enabled = true;
    }
  }

     
                                                                           
                                                                             
                                                                           
                                                                             
                                              
     
  if (!ndistinct_enabled || !dependencies_enabled || !mcv_enabled)
  {
    bool gotone = false;

    appendStringInfoString(&buf, " (");

    if (ndistinct_enabled)
    {
      appendStringInfoString(&buf, "ndistinct");
      gotone = true;
    }

    if (dependencies_enabled)
    {
      appendStringInfo(&buf, "%sdependencies", gotone ? ", " : "");
      gotone = true;
    }

    if (mcv_enabled)
    {
      appendStringInfo(&buf, "%smcv", gotone ? ", " : "");
    }

    appendStringInfoChar(&buf, ')');
  }

  appendStringInfoString(&buf, " ON ");

  for (colno = 0; colno < statextrec->stxkeys.dim1; colno++)
  {
    AttrNumber attnum = statextrec->stxkeys.values[colno];
    char *attname;

    if (colno > 0)
    {
      appendStringInfoString(&buf, ", ");
    }

    attname = get_attname(statextrec->stxrelid, attnum, false);

    appendStringInfoString(&buf, quote_identifier(attname));
  }

  appendStringInfo(&buf, " FROM %s", generate_relation_name(statextrec->stxrelid, NIL));

  ReleaseSysCache(statexttup);

  return buf.data;
}

   
                     
   
                                                               
   
                                                                                   
   
Datum
pg_get_partkeydef(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  char *res;

  res = pg_get_partkeydef_worker(relid, PRETTYFLAG_INDENT, false, true);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

                                                               
char *
pg_get_partkeydef_columns(Oid relid, bool pretty)
{
  int prettyFlags;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  return pg_get_partkeydef_worker(relid, prettyFlags, true, false);
}

   
                                                               
   
static char *
pg_get_partkeydef_worker(Oid relid, int prettyFlags, bool attrsOnly, bool missing_ok)
{
  Form_pg_partitioned_table form;
  HeapTuple tuple;
  oidvector *partclass;
  oidvector *partcollation;
  List *partexprs;
  ListCell *partexpr_item;
  List *context;
  Datum datum;
  bool isnull;
  StringInfoData buf;
  int keyno;
  char *str;
  char *sep;

  tuple = SearchSysCache1(PARTRELID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    if (missing_ok)
    {
      return NULL;
    }
    elog(ERROR, "cache lookup failed for partition key of %u", relid);
  }

  form = (Form_pg_partitioned_table)GETSTRUCT(tuple);

  Assert(form->partrelid == relid);

                                                         
  datum = SysCacheGetAttr(PARTRELID, tuple, Anum_pg_partitioned_table_partclass, &isnull);
  Assert(!isnull);
  partclass = (oidvector *)DatumGetPointer(datum);

  datum = SysCacheGetAttr(PARTRELID, tuple, Anum_pg_partitioned_table_partcollation, &isnull);
  Assert(!isnull);
  partcollation = (oidvector *)DatumGetPointer(datum);

     
                                                                     
                                                             
                                    
     
  if (!heap_attisnull(tuple, Anum_pg_partitioned_table_partexprs, NULL))
  {
    Datum exprsDatum;
    bool isnull;
    char *exprsString;

    exprsDatum = SysCacheGetAttr(PARTRELID, tuple, Anum_pg_partitioned_table_partexprs, &isnull);
    Assert(!isnull);
    exprsString = TextDatumGetCString(exprsDatum);
    partexprs = (List *)stringToNode(exprsString);

    if (!IsA(partexprs, List))
    {
      elog(ERROR, "unexpected node type found in partexprs: %d", (int)nodeTag(partexprs));
    }

    pfree(exprsString);
  }
  else
  {
    partexprs = NIL;
  }

  partexpr_item = list_head(partexprs);
  context = deparse_context_for(get_relation_name(relid), relid);

  initStringInfo(&buf);

  switch (form->partstrat)
  {
  case PARTITION_STRATEGY_HASH:
    if (!attrsOnly)
    {
      appendStringInfo(&buf, "HASH");
    }
    break;
  case PARTITION_STRATEGY_LIST:
    if (!attrsOnly)
    {
      appendStringInfoString(&buf, "LIST");
    }
    break;
  case PARTITION_STRATEGY_RANGE:
    if (!attrsOnly)
    {
      appendStringInfoString(&buf, "RANGE");
    }
    break;
  default:
    elog(ERROR, "unexpected partition strategy: %d", (int)form->partstrat);
  }

  if (!attrsOnly)
  {
    appendStringInfoString(&buf, " (");
  }
  sep = "";
  for (keyno = 0; keyno < form->partnatts; keyno++)
  {
    AttrNumber attnum = form->partattrs.values[keyno];
    Oid keycoltype;
    Oid keycolcollation;
    Oid partcoll;

    appendStringInfoString(&buf, sep);
    sep = ", ";
    if (attnum != 0)
    {
                                      
      char *attname;
      int32 keycoltypmod;

      attname = get_attname(relid, attnum, false);
      appendStringInfoString(&buf, quote_identifier(attname));
      get_atttypetypmodcoll(relid, attnum, &keycoltype, &keycoltypmod, &keycolcollation);
    }
    else
    {
                      
      Node *partkey;

      if (partexpr_item == NULL)
      {
        elog(ERROR, "too few entries in partexprs list");
      }
      partkey = (Node *)lfirst(partexpr_item);
      partexpr_item = lnext(partexpr_item);

                   
      str = deparse_expression_pretty(partkey, context, false, false, prettyFlags, 0);
                                                        
      if (looks_like_function(partkey))
      {
        appendStringInfoString(&buf, str);
      }
      else
      {
        appendStringInfo(&buf, "(%s)", str);
      }

      keycoltype = exprType(partkey);
      keycolcollation = exprCollation(partkey);
    }

                                                  
    partcoll = partcollation->values[keyno];
    if (!attrsOnly && OidIsValid(partcoll) && partcoll != keycolcollation)
    {
      appendStringInfo(&buf, " COLLATE %s", generate_collation_name((partcoll)));
    }

                                                     
    if (!attrsOnly)
    {
      get_opclass_name(partclass->values[keyno], keycoltype, &buf);
    }
  }

  if (!attrsOnly)
  {
    appendStringInfoChar(&buf, ')');
  }

                
  ReleaseSysCache(tuple);

  return buf.data;
}

   
                                  
   
                                                                              
   
Datum
pg_get_partition_constraintdef(PG_FUNCTION_ARGS)
{
  Oid relationId = PG_GETARG_OID(0);
  Expr *constr_expr;
  int prettyFlags;
  List *context;
  char *consrc;

  constr_expr = get_partition_qual_relid(relationId);

                                             
  if (constr_expr == NULL)
  {
    PG_RETURN_NULL();
  }

     
                                                   
     
  prettyFlags = PRETTYFLAG_INDENT;
  context = deparse_context_for(get_relation_name(relationId), relationId);
  consrc = deparse_expression_pretty((Node *)constr_expr, context, false, false, prettyFlags, 0);

  PG_RETURN_TEXT_P(string_to_text(consrc));
}

   
                               
   
                                                                               
                                         
   
char *
pg_get_partconstrdef_string(Oid partitionId, char *aliasname)
{
  Expr *constr_expr;
  List *context;

  constr_expr = get_partition_qual_relid(partitionId);
  context = deparse_context_for(aliasname, partitionId);

  return deparse_expression((Node *)constr_expr, context, true, false);
}

   
                        
   
                                                                           
                                                                   
   
Datum
pg_get_constraintdef(PG_FUNCTION_ARGS)
{
  Oid constraintId = PG_GETARG_OID(0);
  int prettyFlags;
  char *res;

  prettyFlags = PRETTYFLAG_INDENT;

  res = pg_get_constraintdef_worker(constraintId, false, prettyFlags, true);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

Datum
pg_get_constraintdef_ext(PG_FUNCTION_ARGS)
{
  Oid constraintId = PG_GETARG_OID(0);
  bool pretty = PG_GETARG_BOOL(1);
  int prettyFlags;
  char *res;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  res = pg_get_constraintdef_worker(constraintId, false, prettyFlags, true);

  if (res == NULL)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(string_to_text(res));
}

   
                                                                               
   
char *
pg_get_constraintdef_command(Oid constraintId)
{
  return pg_get_constraintdef_worker(constraintId, true, 0, false);
}

   
                                                    
   
static char *
pg_get_constraintdef_worker(Oid constraintId, bool fullCommand, int prettyFlags, bool missing_ok)
{
  HeapTuple tup;
  Form_pg_constraint conForm;
  StringInfoData buf;
  SysScanDesc scandesc;
  ScanKeyData scankey[1];
  Snapshot snapshot = RegisterSnapshot(GetTransactionSnapshot());
  Relation relation = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&scankey[0], Anum_pg_constraint_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(constraintId));

  scandesc = systable_beginscan(relation, ConstraintOidIndexId, true, snapshot, 1, scankey);

     
                                                                            
                                           
     
  tup = systable_getnext(scandesc);

  UnregisterSnapshot(snapshot);

  if (!HeapTupleIsValid(tup))
  {
    if (missing_ok)
    {
      systable_endscan(scandesc);
      table_close(relation, AccessShareLock);
      return NULL;
    }
    elog(ERROR, "could not find tuple for constraint %u", constraintId);
  }

  conForm = (Form_pg_constraint)GETSTRUCT(tup);

  initStringInfo(&buf);

  if (fullCommand)
  {
    if (OidIsValid(conForm->conrelid))
    {
         
                                                                      
                                                                   
                                                                         
                                                                         
                  
         
      appendStringInfo(&buf, "ALTER TABLE %s ADD CONSTRAINT %s ", generate_qualified_relation_name(conForm->conrelid), quote_identifier(NameStr(conForm->conname)));
    }
    else
    {
                                       
      Assert(OidIsValid(conForm->contypid));
      appendStringInfo(&buf, "ALTER DOMAIN %s ADD CONSTRAINT %s ", generate_qualified_type_name(conForm->contypid), quote_identifier(NameStr(conForm->conname)));
    }
  }

  switch (conForm->contype)
  {
  case CONSTRAINT_FOREIGN:
  {
    Datum val;
    bool isnull;
    const char *string;

                                             
    appendStringInfoString(&buf, "FOREIGN KEY (");

                                                 
    val = SysCacheGetAttr(CONSTROID, tup, Anum_pg_constraint_conkey, &isnull);
    if (isnull)
    {
      elog(ERROR, "null conkey for constraint %u", constraintId);
    }

    decompile_column_index_array(val, conForm->conrelid, &buf);

                                   
    appendStringInfo(&buf, ") REFERENCES %s(", generate_relation_name(conForm->confrelid, NIL));

                                                
    val = SysCacheGetAttr(CONSTROID, tup, Anum_pg_constraint_confkey, &isnull);
    if (isnull)
    {
      elog(ERROR, "null confkey for constraint %u", constraintId);
    }

    decompile_column_index_array(val, conForm->confrelid, &buf);

    appendStringInfoChar(&buf, ')');

                        
    switch (conForm->confmatchtype)
    {
    case FKCONSTR_MATCH_FULL:
      string = " MATCH FULL";
      break;
    case FKCONSTR_MATCH_PARTIAL:
      string = " MATCH PARTIAL";
      break;
    case FKCONSTR_MATCH_SIMPLE:
      string = "";
      break;
    default:
      elog(ERROR, "unrecognized confmatchtype: %d", conForm->confmatchtype);
      string = "";                          
      break;
    }
    appendStringInfoString(&buf, string);

                                                        
    switch (conForm->confupdtype)
    {
    case FKCONSTR_ACTION_NOACTION:
      string = NULL;                       
      break;
    case FKCONSTR_ACTION_RESTRICT:
      string = "RESTRICT";
      break;
    case FKCONSTR_ACTION_CASCADE:
      string = "CASCADE";
      break;
    case FKCONSTR_ACTION_SETNULL:
      string = "SET NULL";
      break;
    case FKCONSTR_ACTION_SETDEFAULT:
      string = "SET DEFAULT";
      break;
    default:
      elog(ERROR, "unrecognized confupdtype: %d", conForm->confupdtype);
      string = NULL;                          
      break;
    }
    if (string)
    {
      appendStringInfo(&buf, " ON UPDATE %s", string);
    }

    switch (conForm->confdeltype)
    {
    case FKCONSTR_ACTION_NOACTION:
      string = NULL;                       
      break;
    case FKCONSTR_ACTION_RESTRICT:
      string = "RESTRICT";
      break;
    case FKCONSTR_ACTION_CASCADE:
      string = "CASCADE";
      break;
    case FKCONSTR_ACTION_SETNULL:
      string = "SET NULL";
      break;
    case FKCONSTR_ACTION_SETDEFAULT:
      string = "SET DEFAULT";
      break;
    default:
      elog(ERROR, "unrecognized confdeltype: %d", conForm->confdeltype);
      string = NULL;                          
      break;
    }
    if (string)
    {
      appendStringInfo(&buf, " ON DELETE %s", string);
    }

    break;
  }
  case CONSTRAINT_PRIMARY:
  case CONSTRAINT_UNIQUE:
  {
    Datum val;
    bool isnull;
    Oid indexId;
    int keyatts;
    HeapTuple indtup;

                                             
    if (conForm->contype == CONSTRAINT_PRIMARY)
    {
      appendStringInfoString(&buf, "PRIMARY KEY (");
    }
    else
    {
      appendStringInfoString(&buf, "UNIQUE (");
    }

                                            
    val = SysCacheGetAttr(CONSTROID, tup, Anum_pg_constraint_conkey, &isnull);
    if (isnull)
    {
      elog(ERROR, "null conkey for constraint %u", constraintId);
    }

    keyatts = decompile_column_index_array(val, conForm->conrelid, &buf);

    appendStringInfoChar(&buf, ')');

    indexId = get_constraint_index(constraintId);

                                                             
    indtup = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexId));
    if (!HeapTupleIsValid(indtup))
    {
      elog(ERROR, "cache lookup failed for index %u", indexId);
    }
    val = SysCacheGetAttr(INDEXRELID, indtup, Anum_pg_index_indnatts, &isnull);
    if (isnull)
    {
      elog(ERROR, "null indnatts for index %u", indexId);
    }
    if (DatumGetInt32(val) > keyatts)
    {
      Datum cols;
      Datum *keys;
      int nKeys;
      int j;

      appendStringInfoString(&buf, " INCLUDE (");

      cols = SysCacheGetAttr(INDEXRELID, indtup, Anum_pg_index_indkey, &isnull);
      if (isnull)
      {
        elog(ERROR, "null indkey for index %u", indexId);
      }

      deconstruct_array(DatumGetArrayTypeP(cols), INT2OID, 2, true, 's', &keys, NULL, &nKeys);

      for (j = keyatts; j < nKeys; j++)
      {
        char *colName;

        colName = get_attname(conForm->conrelid, DatumGetInt16(keys[j]), false);
        if (j > keyatts)
        {
          appendStringInfoString(&buf, ", ");
        }
        appendStringInfoString(&buf, quote_identifier(colName));
      }

      appendStringInfoChar(&buf, ')');
    }
    ReleaseSysCache(indtup);

                                                             
    if (fullCommand && OidIsValid(indexId))
    {
      char *options = flatten_reloptions(indexId);
      Oid tblspc;

      if (options)
      {
        appendStringInfo(&buf, " WITH (%s)", options);
        pfree(options);
      }

         
                                                                 
                                                             
                                                             
                
         
      tblspc = get_rel_tablespace(indexId);
      if (OidIsValid(tblspc))
      {
        appendStringInfo(&buf, " USING INDEX TABLESPACE %s", quote_identifier(get_tablespace_name(tblspc)));
      }
    }

    break;
  }
  case CONSTRAINT_CHECK:
  {
    Datum val;
    bool isnull;
    char *conbin;
    char *consrc;
    Node *expr;
    List *context;

                                                       
    val = SysCacheGetAttr(CONSTROID, tup, Anum_pg_constraint_conbin, &isnull);
    if (isnull)
    {
      elog(ERROR, "null conbin for constraint %u", constraintId);
    }

    conbin = TextDatumGetCString(val);
    expr = stringToNode(conbin);

                                                              
    if (conForm->conrelid != InvalidOid)
    {
                               
      context = deparse_context_for(get_relation_name(conForm->conrelid), conForm->conrelid);
    }
    else
    {
                                                 
      context = NIL;
    }

    consrc = deparse_expression_pretty(expr, context, false, false, prettyFlags, 0);

       
                                                                
                  
       
                                                               
                                                                  
                                                           
                             
       
                                                                  
                                                                 
       
    appendStringInfo(&buf, "CHECK (%s)%s", consrc, conForm->connoinherit ? " NO INHERIT" : "");
    break;
  }
  case CONSTRAINT_TRIGGER:

       
                                                                     
                                                                       
                                                                     
                                                       
       
    appendStringInfoString(&buf, "TRIGGER");
    break;
  case CONSTRAINT_EXCLUSION:
  {
    Oid indexOid = conForm->conindid;
    Datum val;
    bool isnull;
    Datum *elems;
    int nElems;
    int i;
    Oid *operators;

                                                            
    val = SysCacheGetAttr(CONSTROID, tup, Anum_pg_constraint_conexclop, &isnull);
    if (isnull)
    {
      elog(ERROR, "null conexclop for constraint %u", constraintId);
    }

    deconstruct_array(DatumGetArrayTypeP(val), OIDOID, sizeof(Oid), true, 'i', &elems, NULL, &nElems);

    operators = (Oid *)palloc(nElems * sizeof(Oid));
    for (i = 0; i < nElems; i++)
    {
      operators[i] = DatumGetObjectId(elems[i]);
    }

                                              
                                                               
    appendStringInfoString(&buf, pg_get_indexdef_worker(indexOid, 0, operators, false, false, false, false, prettyFlags, false));
    break;
  }
  default:
    elog(ERROR, "invalid constraint type \"%c\"", conForm->contype);
    break;
  }

  if (conForm->condeferrable)
  {
    appendStringInfoString(&buf, " DEFERRABLE");
  }
  if (conForm->condeferred)
  {
    appendStringInfoString(&buf, " INITIALLY DEFERRED");
  }
  if (!conForm->convalidated)
  {
    appendStringInfoString(&buf, " NOT VALID");
  }

               
  systable_endscan(scandesc);
  table_close(relation, AccessShareLock);

  return buf.data;
}

   
                                                                        
                                                                           
            
   
static int
decompile_column_index_array(Datum column_index_array, Oid relId, StringInfo buf)
{
  Datum *keys;
  int nKeys;
  int j;

                                        
  deconstruct_array(DatumGetArrayTypeP(column_index_array), INT2OID, 2, true, 's', &keys, NULL, &nKeys);

  for (j = 0; j < nKeys; j++)
  {
    char *colName;

    colName = get_attname(relId, DatumGetInt16(keys[j]), false);

    if (j == 0)
    {
      appendStringInfoString(buf, quote_identifier(colName));
    }
    else
    {
      appendStringInfo(buf, ", %s", quote_identifier(colName));
    }
  }

  return nKeys;
}

              
                                             
   
                                                                      
   
                                     
   
                                                                         
                                                                      
                                                                      
                                                              
              
   
Datum
pg_get_expr(PG_FUNCTION_ARGS)
{
  text *expr = PG_GETARG_TEXT_PP(0);
  Oid relid = PG_GETARG_OID(1);
  int prettyFlags;
  char *relname;

  prettyFlags = PRETTYFLAG_INDENT;

  if (OidIsValid(relid))
  {
                                       
    relname = get_rel_name(relid);

       
                                                                          
                                                                   
                                                                  
                                                             
       
    if (relname == NULL)
    {
      PG_RETURN_NULL();
    }
  }
  else
  {
    relname = NULL;
  }

  PG_RETURN_TEXT_P(pg_get_expr_worker(expr, relid, relname, prettyFlags));
}

Datum
pg_get_expr_ext(PG_FUNCTION_ARGS)
{
  text *expr = PG_GETARG_TEXT_PP(0);
  Oid relid = PG_GETARG_OID(1);
  bool pretty = PG_GETARG_BOOL(2);
  int prettyFlags;
  char *relname;

  prettyFlags = pretty ? (PRETTYFLAG_PAREN | PRETTYFLAG_INDENT | PRETTYFLAG_SCHEMA) : PRETTYFLAG_INDENT;

  if (OidIsValid(relid))
  {
                                       
    relname = get_rel_name(relid);
                         
    if (relname == NULL)
    {
      PG_RETURN_NULL();
    }
  }
  else
  {
    relname = NULL;
  }

  PG_RETURN_TEXT_P(pg_get_expr_worker(expr, relid, relname, prettyFlags));
}

static text *
pg_get_expr_worker(text *expr, Oid relid, const char *relname, int prettyFlags)
{
  Node *node;
  List *context;
  char *exprstr;
  char *str;

                                             
  exprstr = text_to_cstring(expr);

                                       
  node = (Node *)stringToNode(exprstr);

  pfree(exprstr);

                                         
  if (OidIsValid(relid))
  {
    context = deparse_context_for(relname, relid);
  }
  else
  {
    context = NIL;
  }

               
  str = deparse_expression_pretty(node, context, false, false, prettyFlags, 0);

  return string_to_text(str);
}

              
                                                  
                                      
              
   
Datum
pg_get_userbyid(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Name result;
  HeapTuple roletup;
  Form_pg_authid role_rec;

     
                                   
     
  result = (Name)palloc(NAMEDATALEN);
  memset(NameStr(*result), 0, NAMEDATALEN);

     
                                                  
     
  roletup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (HeapTupleIsValid(roletup))
  {
    role_rec = (Form_pg_authid)GETSTRUCT(roletup);
    StrNCpy(NameStr(*result), NameStr(role_rec->rolname), NAMEDATALEN);
    ReleaseSysCache(roletup);
  }
  else
  {
    sprintf(NameStr(*result), "unknown (OID=%u)", roleid);
  }

  PG_RETURN_NAME(result);
}

   
                          
                                                                       
                                                                  
                                                                      
                                         
   
Datum
pg_get_serial_sequence(PG_FUNCTION_ARGS)
{
  text *tablename = PG_GETARG_TEXT_PP(0);
  text *columnname = PG_GETARG_TEXT_PP(1);
  RangeVar *tablerv;
  Oid tableOid;
  char *column;
  AttrNumber attnum;
  Oid sequenceId = InvalidOid;
  Relation depRel;
  ScanKeyData key[3];
  SysScanDesc scan;
  HeapTuple tup;

                                                                          
  tablerv = makeRangeVarFromNameList(textToQualifiedNameList(tablename));
  tableOid = RangeVarGetRelid(tablerv, NoLock, false);

                                    
  column = text_to_cstring(columnname);

  attnum = get_attnum(tableOid, column);
  if (attnum == InvalidAttrNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", column, tablerv->relname)));
  }

                                                              
  depRel = table_open(DependRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_depend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(tableOid));
  ScanKeyInit(&key[2], Anum_pg_depend_refobjsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(attnum));

  scan = systable_beginscan(depRel, DependReferenceIndexId, true, NULL, 3, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_depend deprec = (Form_pg_depend)GETSTRUCT(tup);

       
                                                                          
                                                                          
                                                                         
       
    if (deprec->classid == RelationRelationId && deprec->objsubid == 0 && (deprec->deptype == DEPENDENCY_AUTO || deprec->deptype == DEPENDENCY_INTERNAL) && get_rel_relkind(deprec->objid) == RELKIND_SEQUENCE)
    {
      sequenceId = deprec->objid;
      break;
    }
  }

  systable_endscan(scan);
  table_close(depRel, AccessShareLock);

  if (OidIsValid(sequenceId))
  {
    char *result;

    result = generate_qualified_relation_name(sequenceId);

    PG_RETURN_TEXT_P(string_to_text(result));
  }

  PG_RETURN_NULL();
}

   
                      
                                                                        
                            
   
                                                                          
                                                                           
                                                                          
                                                          
   
Datum
pg_get_functiondef(PG_FUNCTION_ARGS)
{
  Oid funcid = PG_GETARG_OID(0);
  StringInfoData buf;
  StringInfoData dq;
  HeapTuple proctup;
  Form_pg_proc proc;
  bool isfunction;
  Datum tmp;
  bool isnull;
  const char *prosrc;
  const char *name;
  const char *nsp;
  float4 procost;
  int oldlen;

  initStringInfo(&buf);

                            
  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    PG_RETURN_NULL();
  }

  proc = (Form_pg_proc)GETSTRUCT(proctup);
  name = NameStr(proc->proname);

  if (proc->prokind == PROKIND_AGGREGATE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an aggregate function", name)));
  }

  isfunction = (proc->prokind != PROKIND_PROCEDURE);

     
                                                                            
               
     
  nsp = get_namespace_name(proc->pronamespace);
  appendStringInfo(&buf, "CREATE OR REPLACE %s %s(", isfunction ? "FUNCTION" : "PROCEDURE", quote_qualified_identifier(nsp, name));
  (void)print_function_arguments(&buf, proctup, false, true);
  appendStringInfoString(&buf, ")\n");
  if (isfunction)
  {
    appendStringInfoString(&buf, " RETURNS ");
    print_function_rettype(&buf, proctup);
    appendStringInfoChar(&buf, '\n');
  }

  print_function_trftypes(&buf, proctup);

  appendStringInfo(&buf, " LANGUAGE %s\n", quote_identifier(get_language_name(proc->prolang, false)));

                                                   
  oldlen = buf.len;

  if (proc->prokind == PROKIND_WINDOW)
  {
    appendStringInfoString(&buf, " WINDOW");
  }
  switch (proc->provolatile)
  {
  case PROVOLATILE_IMMUTABLE:
    appendStringInfoString(&buf, " IMMUTABLE");
    break;
  case PROVOLATILE_STABLE:
    appendStringInfoString(&buf, " STABLE");
    break;
  case PROVOLATILE_VOLATILE:
    break;
  }

  switch (proc->proparallel)
  {
  case PROPARALLEL_SAFE:
    appendStringInfoString(&buf, " PARALLEL SAFE");
    break;
  case PROPARALLEL_RESTRICTED:
    appendStringInfoString(&buf, " PARALLEL RESTRICTED");
    break;
  case PROPARALLEL_UNSAFE:
    break;
  }

  if (proc->proisstrict)
  {
    appendStringInfoString(&buf, " STRICT");
  }
  if (proc->prosecdef)
  {
    appendStringInfoString(&buf, " SECURITY DEFINER");
  }
  if (proc->proleakproof)
  {
    appendStringInfoString(&buf, " LEAKPROOF");
  }

                                                                           
  if (proc->prolang == INTERNALlanguageId || proc->prolang == ClanguageId)
  {
    procost = 1;
  }
  else
  {
    procost = 100;
  }
  if (proc->procost != procost)
  {
    appendStringInfo(&buf, " COST %g", proc->procost);
  }

  if (proc->prorows > 0 && proc->prorows != 1000)
  {
    appendStringInfo(&buf, " ROWS %g", proc->prorows);
  }

  if (proc->prosupport)
  {
    Oid argtypes[1];

       
                                                                       
                                                      
       
    argtypes[0] = INTERNALOID;
    appendStringInfo(&buf, " SUPPORT %s", generate_function_name(proc->prosupport, 1, NIL, argtypes, false, NULL, EXPR_KIND_NONE));
  }

  if (oldlen != buf.len)
  {
    appendStringInfoChar(&buf, '\n');
  }

                                                
  tmp = SysCacheGetAttr(PROCOID, proctup, Anum_pg_proc_proconfig, &isnull);
  if (!isnull)
  {
    ArrayType *a = DatumGetArrayTypeP(tmp);
    int i;

    Assert(ARR_ELEMTYPE(a) == TEXTOID);
    Assert(ARR_NDIM(a) == 1);
    Assert(ARR_LBOUND(a)[0] == 1);

    for (i = 1; i <= ARR_DIMS(a)[0]; i++)
    {
      Datum d;

      d = array_ref(a, 1, &i, -1                  , -1 /* TEXT's typlen */, false /* TEXT's typbyval */, 'i' /* TEXT's typalign */, &isnull);
      if (!isnull)
      {
        char *configitem = TextDatumGetCString(d);
        char *pos;

        pos = strchr(configitem, '=');
        if (pos == NULL)
        {
          continue;
        }
        *pos++ = '\0';

        appendStringInfo(&buf, " SET %s TO ", quote_identifier(configitem));

           
                                                                       
                                                                      
                                                                   
                                                                  
                                                                     
                                                                       
                                                                      
                                                                 
                                     
           
                                                                      
                                                                    
                                                             
                                                   
           
        if (GetConfigOptionFlags(configitem, true) & GUC_LIST_QUOTE)
        {
          List *namelist;
          ListCell *lc;

                                                     
          if (!SplitGUCList(pos, ',', &namelist))
          {
                                            
            elog(ERROR, "invalid list syntax in proconfig item");
          }
          foreach (lc, namelist)
          {
            char *curname = (char *)lfirst(lc);

            simple_quote_literal(&buf, curname);
            if (lnext(lc))
            {
              appendStringInfoString(&buf, ", ");
            }
          }
        }
        else
        {
          simple_quote_literal(&buf, pos);
        }
        appendStringInfoChar(&buf, '\n');
      }
    }
  }

                                               
  appendStringInfoString(&buf, "AS ");

  tmp = SysCacheGetAttr(PROCOID, proctup, Anum_pg_proc_probin, &isnull);
  if (!isnull)
  {
    simple_quote_literal(&buf, TextDatumGetCString(tmp));
    appendStringInfoString(&buf, ", ");                               
  }

  tmp = SysCacheGetAttr(PROCOID, proctup, Anum_pg_proc_prosrc, &isnull);
  if (isnull)
  {
    elog(ERROR, "null prosrc");
  }
  prosrc = TextDatumGetCString(tmp);

     
                                                                     
     
                                                                         
                                                                            
                                                                           
     
  initStringInfo(&dq);
  appendStringInfoChar(&dq, '$');
  appendStringInfoString(&dq, (isfunction ? "function" : "procedure"));
  while (strstr(prosrc, dq.data) != NULL)
  {
    appendStringInfoChar(&dq, 'x');
  }
  appendStringInfoChar(&dq, '$');

  appendStringInfoString(&buf, dq.data);
  appendStringInfoString(&buf, prosrc);
  appendStringInfoString(&buf, dq.data);

  appendStringInfoChar(&buf, '\n');

  ReleaseSysCache(proctup);

  PG_RETURN_TEXT_P(string_to_text(buf.data));
}

   
                             
                                                             
                                                                
                     
   
Datum
pg_get_function_arguments(PG_FUNCTION_ARGS)
{
  Oid funcid = PG_GETARG_OID(0);
  StringInfoData buf;
  HeapTuple proctup;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    PG_RETURN_NULL();
  }

  initStringInfo(&buf);

  (void)print_function_arguments(&buf, proctup, false, true);

  ReleaseSysCache(proctup);

  PG_RETURN_TEXT_P(string_to_text(buf.data));
}

   
                                      
                                                      
                                                                
                                                               
   
Datum
pg_get_function_identity_arguments(PG_FUNCTION_ARGS)
{
  Oid funcid = PG_GETARG_OID(0);
  StringInfoData buf;
  HeapTuple proctup;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    PG_RETURN_NULL();
  }

  initStringInfo(&buf);

  (void)print_function_arguments(&buf, proctup, false, false);

  ReleaseSysCache(proctup);

  PG_RETURN_TEXT_P(string_to_text(buf.data));
}

   
                          
                                                                     
                                                                
   
Datum
pg_get_function_result(PG_FUNCTION_ARGS)
{
  Oid funcid = PG_GETARG_OID(0);
  StringInfoData buf;
  HeapTuple proctup;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    PG_RETURN_NULL();
  }

  if (((Form_pg_proc)GETSTRUCT(proctup))->prokind == PROKIND_PROCEDURE)
  {
    ReleaseSysCache(proctup);
    PG_RETURN_NULL();
  }

  initStringInfo(&buf);

  print_function_rettype(&buf, proctup);

  ReleaseSysCache(proctup);

  PG_RETURN_TEXT_P(string_to_text(buf.data));
}

   
                                                                     
                            
   
static void
print_function_rettype(StringInfo buf, HeapTuple proctup)
{
  Form_pg_proc proc = (Form_pg_proc)GETSTRUCT(proctup);
  int ntabargs = 0;
  StringInfoData rbuf;

  initStringInfo(&rbuf);

  if (proc->proretset)
  {
                                                                  
    appendStringInfoString(&rbuf, "TABLE(");
    ntabargs = print_function_arguments(&rbuf, proctup, true, false);
    if (ntabargs > 0)
    {
      appendStringInfoChar(&rbuf, ')');
    }
    else
    {
      resetStringInfo(&rbuf);
    }
  }

  if (ntabargs == 0)
  {
                                                      
    if (proc->proretset)
    {
      appendStringInfoString(&rbuf, "SETOF ");
    }
    appendStringInfoString(&rbuf, format_type_be(proc->prorettype));
  }

  appendStringInfoString(buf, rbuf.data);
}

   
                                                                         
                                                                       
                                                                                
                                                              
                                                             
   
static int
print_function_arguments(StringInfo buf, HeapTuple proctup, bool print_table_args, bool print_defaults)
{
  Form_pg_proc proc = (Form_pg_proc)GETSTRUCT(proctup);
  int numargs;
  Oid *argtypes;
  char **argnames;
  char *argmodes;
  int insertorderbyat = -1;
  int argsprinted;
  int inputargno;
  int nlackdefaults;
  ListCell *nextargdefault = NULL;
  int i;

  numargs = get_func_arg_info(proctup, &argtypes, &argnames, &argmodes);

  nlackdefaults = numargs;
  if (print_defaults && proc->pronargdefaults > 0)
  {
    Datum proargdefaults;
    bool isnull;

    proargdefaults = SysCacheGetAttr(PROCOID, proctup, Anum_pg_proc_proargdefaults, &isnull);
    if (!isnull)
    {
      char *str;
      List *argdefaults;

      str = TextDatumGetCString(proargdefaults);
      argdefaults = castNode(List, stringToNode(str));
      pfree(str);
      nextargdefault = list_head(argdefaults);
                                                                        
      nlackdefaults = proc->pronargs - list_length(argdefaults);
    }
  }

                                                             
  if (proc->prokind == PROKIND_AGGREGATE)
  {
    HeapTuple aggtup;
    Form_pg_aggregate agg;

    aggtup = SearchSysCache1(AGGFNOID, proc->oid);
    if (!HeapTupleIsValid(aggtup))
    {
      elog(ERROR, "cache lookup failed for aggregate %u", proc->oid);
    }
    agg = (Form_pg_aggregate)GETSTRUCT(aggtup);
    if (AGGKIND_IS_ORDERED_SET(agg->aggkind))
    {
      insertorderbyat = agg->aggnumdirectargs;
    }
    ReleaseSysCache(aggtup);
  }

  argsprinted = 0;
  inputargno = 0;
  for (i = 0; i < numargs; i++)
  {
    Oid argtype = argtypes[i];
    char *argname = argnames ? argnames[i] : NULL;
    char argmode = argmodes ? argmodes[i] : PROARGMODE_IN;
    const char *modename;
    bool isinput;

    switch (argmode)
    {
    case PROARGMODE_IN:
      modename = "";
      isinput = true;
      break;
    case PROARGMODE_INOUT:
      modename = "INOUT ";
      isinput = true;
      break;
    case PROARGMODE_OUT:
      modename = "OUT ";
      isinput = false;
      break;
    case PROARGMODE_VARIADIC:
      modename = "VARIADIC ";
      isinput = true;
      break;
    case PROARGMODE_TABLE:
      modename = "";
      isinput = false;
      break;
    default:
      elog(ERROR, "invalid parameter mode '%c'", argmode);
      modename = NULL;                          
      isinput = false;
      break;
    }
    if (isinput)
    {
      inputargno++;                                
    }

    if (print_table_args != (argmode == PROARGMODE_TABLE))
    {
      continue;
    }

    if (argsprinted == insertorderbyat)
    {
      if (argsprinted)
      {
        appendStringInfoChar(buf, ' ');
      }
      appendStringInfoString(buf, "ORDER BY ");
    }
    else if (argsprinted)
    {
      appendStringInfoString(buf, ", ");
    }

    appendStringInfoString(buf, modename);
    if (argname && argname[0])
    {
      appendStringInfo(buf, "%s ", quote_identifier(argname));
    }
    appendStringInfoString(buf, format_type_be(argtype));
    if (print_defaults && isinput && inputargno > nlackdefaults)
    {
      Node *expr;

      Assert(nextargdefault != NULL);
      expr = (Node *)lfirst(nextargdefault);
      nextargdefault = lnext(nextargdefault);

      appendStringInfo(buf, " DEFAULT %s", deparse_expression(expr, NIL, false, false));
    }
    argsprinted++;

                                                                           
    if (argsprinted == insertorderbyat && i == numargs - 1)
    {
      i--;
                                                                        
      print_defaults = false;
    }
  }

  return argsprinted;
}

static bool
is_input_argument(int nth, const char *argmodes)
{
  return (!argmodes || argmodes[nth] == PROARGMODE_IN || argmodes[nth] == PROARGMODE_INOUT || argmodes[nth] == PROARGMODE_VARIADIC);
}

   
                                                     
   
static void
print_function_trftypes(StringInfo buf, HeapTuple proctup)
{
  Oid *trftypes;
  int ntypes;

  ntypes = get_func_trftypes(proctup, &trftypes);
  if (ntypes > 0)
  {
    int i;

    appendStringInfoString(buf, " TRANSFORM ");
    for (i = 0; i < ntypes; i++)
    {
      if (i != 0)
      {
        appendStringInfoString(buf, ", ");
      }
      appendStringInfo(buf, "FOR TYPE %s", format_type_be(trftypes[i]));
    }
    appendStringInfoChar(buf, '\n');
  }
}

   
                                                                           
                                                                               
                                                                             
                                       
   
Datum
pg_get_function_arg_default(PG_FUNCTION_ARGS)
{
  Oid funcid = PG_GETARG_OID(0);
  int32 nth_arg = PG_GETARG_INT32(1);
  HeapTuple proctup;
  Form_pg_proc proc;
  int numargs;
  Oid *argtypes;
  char **argnames;
  char *argmodes;
  int i;
  List *argdefaults;
  Node *node;
  char *str;
  int nth_inputarg;
  Datum proargdefaults;
  bool isnull;
  int nth_default;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    PG_RETURN_NULL();
  }

  numargs = get_func_arg_info(proctup, &argtypes, &argnames, &argmodes);
  if (nth_arg < 1 || nth_arg > numargs || !is_input_argument(nth_arg - 1, argmodes))
  {
    ReleaseSysCache(proctup);
    PG_RETURN_NULL();
  }

  nth_inputarg = 0;
  for (i = 0; i < nth_arg; i++)
  {
    if (is_input_argument(i, argmodes))
    {
      nth_inputarg++;
    }
  }

  proargdefaults = SysCacheGetAttr(PROCOID, proctup, Anum_pg_proc_proargdefaults, &isnull);
  if (isnull)
  {
    ReleaseSysCache(proctup);
    PG_RETURN_NULL();
  }

  str = TextDatumGetCString(proargdefaults);
  argdefaults = castNode(List, stringToNode(str));
  pfree(str);

  proc = (Form_pg_proc)GETSTRUCT(proctup);

     
                                                                            
                                                        
     
  nth_default = nth_inputarg - 1 - (proc->pronargs - proc->pronargdefaults);

  if (nth_default < 0 || nth_default >= list_length(argdefaults))
  {
    ReleaseSysCache(proctup);
    PG_RETURN_NULL();
  }
  node = list_nth(argdefaults, nth_default);
  str = deparse_expression(node, NIL, false, false);

  ReleaseSysCache(proctup);

  PG_RETURN_TEXT_P(string_to_text(str));
}

   
                                                                    
   
                                                                    
   
char *
deparse_expression(Node *expr, List *dpcontext, bool forceprefix, bool showimplicit)
{
  return deparse_expression_pretty(expr, dpcontext, forceprefix, showimplicit, 0, 0);
}

              
                                                                         
   
                                                                              
                                            
   
                                                                           
                                                                         
             
   
                                                                                
   
                                                                            
   
                                                                           
   
                                    
              
   
static char *
deparse_expression_pretty(Node *expr, List *dpcontext, bool forceprefix, bool showimplicit, int prettyFlags, int startIndent)
{
  StringInfoData buf;
  deparse_context context;

  initStringInfo(&buf);
  context.buf = &buf;
  context.namespaces = dpcontext;
  context.windowClause = NIL;
  context.windowTList = NIL;
  context.varprefix = forceprefix;
  context.prettyFlags = prettyFlags;
  context.wrapColumn = WRAP_COLUMN_DEFAULT;
  context.indentLevel = startIndent;
  context.special_exprkind = EXPR_KIND_NONE;

  get_rule_expr(expr, &context, showimplicit);

  return buf.data;
}

              
                                                                       
   
                                                                           
                                                                         
                                                                            
              
   
List *
deparse_context_for(const char *aliasname, Oid relid)
{
  deparse_namespace *dpns;
  RangeTblEntry *rte;

  dpns = (deparse_namespace *)palloc0(sizeof(deparse_namespace));

                                       
  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RELATION;
  rte->relid = relid;
  rte->relkind = RELKIND_RELATION;                                 
  rte->rellockmode = AccessShareLock;
  rte->alias = makeAlias(aliasname, NIL);
  rte->eref = rte->alias;
  rte->lateral = false;
  rte->inh = false;
  rte->inFromCl = true;

                                
  dpns->rtable = list_make1(rte);
  dpns->ctes = NIL;
  set_rtable_names(dpns, NIL, NULL);
  set_simple_column_names(dpns);

                                         
  return list_make1(dpns);
}

   
                                                                               
   
                                                                             
                                                                            
                                                                              
                                                                           
                                                                           
                                                                          
   
                                                                           
                                                                   
   
List *
deparse_context_for_plan_rtable(List *rtable, List *rtable_names)
{
  deparse_namespace *dpns;

  dpns = (deparse_namespace *)palloc0(sizeof(deparse_namespace));

                                                                       
  dpns->rtable = rtable;
  dpns->rtable_names = rtable_names;
  dpns->ctes = NIL;

     
                                                                            
                                                                             
                 
     
  set_simple_column_names(dpns);

                                         
  return list_make1(dpns);
}

   
                                                                           
   
                                                                         
                                                                               
                                                                               
                                                                         
                                                                       
                                                                            
                                                                            
                                                                          
                                                                        
                            
   
                                                                            
                                                                   
   
                                                                          
                                                                            
                                                                 
   
                                                                             
                                                                         
                                                                            
                                                  
   
                                                                            
   
List *
set_deparse_context_planstate(List *dpcontext, Node *planstate, List *ancestors)
{
  deparse_namespace *dpns;

                                                                      
  Assert(list_length(dpcontext) == 1);
  dpns = (deparse_namespace *)linitial(dpcontext);

                                                             
  set_deparse_planstate(dpns, (PlanState *)planstate);
  dpns->ancestors = ancestors;

  return dpcontext;
}

   
                                                                    
   
                                                                         
                                                                               
                                                                            
   
List *
select_rtable_names_for_explain(List *rtable, Bitmapset *rels_used)
{
  deparse_namespace dpns;

  memset(&dpns, 0, sizeof(dpns));
  dpns.rtable = rtable;
  dpns.ctes = NIL;
  set_rtable_names(&dpns, NIL, rels_used);
                                                      

  return dpns.rtable_names;
}

   
                                                                       
   
                                                                               
                                                                              
                                                               
                      
   
                                                                             
   
                                                                             
          
   
static void
set_rtable_names(deparse_namespace *dpns, List *parent_namespaces, Bitmapset *rels_used)
{
  HASHCTL hash_ctl;
  HTAB *names_hash;
  NameHashEntry *hentry;
  bool found;
  int rtindex;
  ListCell *lc;

  dpns->rtable_names = NIL;
                                          
  if (dpns->rtable == NIL)
  {
    return;
  }

     
                                                                           
                             
     
  MemSet(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = NAMEDATALEN;
  hash_ctl.entrysize = sizeof(NameHashEntry);
  hash_ctl.hcxt = CurrentMemoryContext;
  names_hash = hash_create("set_rtable_names names", list_length(dpns->rtable), &hash_ctl, HASH_ELEM | HASH_CONTEXT);
                                                                        
  foreach (lc, parent_namespaces)
  {
    deparse_namespace *olddpns = (deparse_namespace *)lfirst(lc);
    ListCell *lc2;

    foreach (lc2, olddpns->rtable_names)
    {
      char *oldname = (char *)lfirst(lc2);

      if (oldname == NULL)
      {
        continue;
      }
      hentry = (NameHashEntry *)hash_search(names_hash, oldname, HASH_ENTER, &found);
                                                                         
      hentry->counter = 0;
    }
  }

                                  
  rtindex = 1;
  foreach (lc, dpns->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);
    char *refname;

                                                                    
    CHECK_FOR_INTERRUPTS();

    if (rels_used && !bms_is_member(rtindex, rels_used))
    {
                                   
      refname = NULL;
    }
    else if (rte->alias)
    {
                                                        
      refname = rte->alias->aliasname;
    }
    else if (rte->rtekind == RTE_RELATION)
    {
                                                       
      refname = get_rel_name(rte->relid);
    }
    else if (rte->rtekind == RTE_JOIN)
    {
                                       
      refname = NULL;
    }
    else
    {
                                                      
      refname = rte->eref->aliasname;
    }

       
                                                                           
                                                                         
                                                                      
                    
       
    if (refname)
    {
      hentry = (NameHashEntry *)hash_search(names_hash, refname, HASH_ENTER, &found);
      if (found)
      {
                                                        
        int refnamelen = strlen(refname);
        char *modname = (char *)palloc(refnamelen + 16);
        NameHashEntry *hentry2;

        do
        {
          hentry->counter++;
          for (;;)
          {
               
                                                                 
                                                                   
                                    
               
            memcpy(modname, refname, refnamelen);
            sprintf(modname + refnamelen, "_%d", hentry->counter);
            if (strlen(modname) < NAMEDATALEN)
            {
              break;
            }
                                                                
            refnamelen = pg_mbcliplen(refname, refnamelen, refnamelen - 1);
          }
          hentry2 = (NameHashEntry *)hash_search(names_hash, modname, HASH_ENTER, &found);
        } while (found);
        hentry2->counter = 0;                          
        refname = modname;
      }
      else
      {
                                                                   
        hentry->counter = 0;
      }
    }

    dpns->rtable_names = lappend(dpns->rtable_names, refname);
    rtindex++;
  }

  hash_destroy(names_hash);
}

   
                                                                              
   
                                                                               
                 
   
static void
set_deparse_for_query(deparse_namespace *dpns, Query *query, List *parent_namespaces)
{
  ListCell *lc;
  ListCell *lc2;

                                                   
  memset(dpns, 0, sizeof(deparse_namespace));
  dpns->rtable = query->rtable;
  dpns->ctes = query->cteList;

                                                  
  set_rtable_names(dpns, parent_namespaces, NULL);

                                                                 
  dpns->rtable_columns = NIL;
  while (list_length(dpns->rtable_columns) < list_length(dpns->rtable))
  {
    dpns->rtable_columns = lappend(dpns->rtable_columns, palloc0(sizeof(deparse_columns)));
  }

                                                         
  if (query->jointree)
  {
                                                                   
    dpns->unique_using = has_dangerous_join_using(dpns, (Node *)query->jointree);

       
                                                                           
                           
       
    set_using_names(dpns, (Node *)query->jointree, NIL);
  }

     
                                                                        
                                                                          
                                                                          
                                                                          
                                                                           
                                              
     
  forboth(lc, dpns->rtable, lc2, dpns->rtable_columns)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);
    deparse_columns *colinfo = (deparse_columns *)lfirst(lc2);

    if (rte->rtekind == RTE_JOIN)
    {
      set_join_column_names(dpns, rte, colinfo);
    }
    else
    {
      set_relation_column_names(dpns, rte, colinfo);
    }
  }
}

   
                                                                            
   
                                                                             
                                                                         
                                                                        
                                                                            
   
static void
set_simple_column_names(deparse_namespace *dpns)
{
  ListCell *lc;
  ListCell *lc2;

                                                                 
  dpns->rtable_columns = NIL;
  while (list_length(dpns->rtable_columns) < list_length(dpns->rtable))
  {
    dpns->rtable_columns = lappend(dpns->rtable_columns, palloc0(sizeof(deparse_columns)));
  }

                                                    
  forboth(lc, dpns->rtable, lc2, dpns->rtable_columns)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);
    deparse_columns *colinfo = (deparse_columns *)lfirst(lc2);

    set_relation_column_names(dpns, rte, colinfo);
  }
}

   
                                                                    
   
                                                                               
                                                                             
                                                                            
                                                                               
                                                                             
                                                                             
                                                                               
                                                                       
                                                                            
                                                                            
                                                                        
   
                                                                            
                                                                        
                                                                          
   
static bool
has_dangerous_join_using(deparse_namespace *dpns, Node *jtnode)
{
  if (IsA(jtnode, RangeTblRef))
  {
                            
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *lc;

    foreach (lc, f->fromlist)
    {
      if (has_dangerous_join_using(dpns, (Node *)lfirst(lc)))
      {
        return true;
      }
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

                                           
    if (j->alias == NULL && j->usingClause)
    {
         
                                                                         
                                                                    
                                                           
         
      RangeTblEntry *jrte = rt_fetch(j->rtindex, dpns->rtable);
      ListCell *lc;

      foreach (lc, jrte->joinaliasvars)
      {
        Var *aliasvar = (Var *)lfirst(lc);

        if (aliasvar != NULL && !IsA(aliasvar, Var))
        {
          return true;
        }
      }
    }

                                    
    if (has_dangerous_join_using(dpns, j->larg))
    {
      return true;
    }
    if (has_dangerous_join_using(dpns, j->rarg))
    {
      return true;
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
  return false;
}

   
                                                                              
   
                                                                
                                                                            
   
                                                                         
                                                                 
   
                                                                          
                                                                          
   
static void
set_using_names(deparse_namespace *dpns, Node *jtnode, List *parentUsing)
{
  if (IsA(jtnode, RangeTblRef))
  {
                           
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *lc;

    foreach (lc, f->fromlist)
    {
      set_using_names(dpns, (Node *)lfirst(lc), parentUsing);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    RangeTblEntry *rte = rt_fetch(j->rtindex, dpns->rtable);
    deparse_columns *colinfo = deparse_columns_fetch(j->rtindex, dpns);
    int *leftattnos;
    int *rightattnos;
    deparse_columns *leftcolinfo;
    deparse_columns *rightcolinfo;
    int i;
    ListCell *lc;

                                              
    identify_join_columns(j, rte, colinfo);
    leftattnos = colinfo->leftattnos;
    rightattnos = colinfo->rightattnos;

                                                                     
    leftcolinfo = deparse_columns_fetch(colinfo->leftrti, dpns);
    rightcolinfo = deparse_columns_fetch(colinfo->rightrti, dpns);

       
                                                                         
                                                                        
                                          
       
    if (rte->alias == NULL)
    {
      for (i = 0; i < colinfo->num_cols; i++)
      {
        char *colname = colinfo->colnames[i];

        if (colname == NULL)
        {
          continue;
        }

                                                                   
        if (leftattnos[i] > 0)
        {
          expand_colnames_array_to(leftcolinfo, leftattnos[i]);
          leftcolinfo->colnames[leftattnos[i] - 1] = colname;
        }

                                        
        if (rightattnos[i] > 0)
        {
          expand_colnames_array_to(rightcolinfo, rightattnos[i]);
          rightcolinfo->colnames[rightattnos[i] - 1] = colname;
        }
      }
    }

       
                                                                         
                                                                  
       
                                                                     
                                                                         
                                                                          
                                                                          
                                                                      
                                                                         
                                                                     
                                                                          
                                                                      
                                                                        
                                                                           
                
       
                                                                      
                                                                 
                                  
       
                                                                           
                                                                         
                                                     
       
    if (j->usingClause)
    {
                                                                 
      parentUsing = list_copy(parentUsing);

                                                                        
      expand_colnames_array_to(colinfo, list_length(j->usingClause));
      i = 0;
      foreach (lc, j->usingClause)
      {
        char *colname = strVal(lfirst(lc));

                                         
        Assert(leftattnos[i] != 0 && rightattnos[i] != 0);

                                                                    
        if (colinfo->colnames[i] != NULL)
        {
          colname = colinfo->colnames[i];
        }
        else
        {
                                                       
          if (rte->alias && i < list_length(rte->alias->colnames))
          {
            colname = strVal(list_nth(rte->alias->colnames, i));
          }
                                            
          colname = make_colname_unique(colname, dpns, colinfo);
          if (dpns->unique_using)
          {
            dpns->using_names = lappend(dpns->using_names, colname);
          }
                                                  
          colinfo->colnames[i] = colname;
        }

                                                   
        colinfo->usingNames = lappend(colinfo->usingNames, colname);
        parentUsing = lappend(parentUsing, colname);

                                                                   
        if (leftattnos[i] > 0)
        {
          expand_colnames_array_to(leftcolinfo, leftattnos[i]);
          leftcolinfo->colnames[leftattnos[i] - 1] = colname;
        }

                                        
        if (rightattnos[i] > 0)
        {
          expand_colnames_array_to(rightcolinfo, rightattnos[i]);
          rightcolinfo->colnames[rightattnos[i] - 1] = colname;
        }

        i++;
      }
    }

                                                                          
    leftcolinfo->parentUsing = parentUsing;
    rightcolinfo->parentUsing = parentUsing;

                                                               
    set_using_names(dpns, j->larg, parentUsing);
    set_using_names(dpns, j->rarg, parentUsing);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                                                                       
   
                                                                              
                                                                       
            
   
static void
set_relation_column_names(deparse_namespace *dpns, RangeTblEntry *rte, deparse_columns *colinfo)
{
  int ncolumns;
  char **real_colnames;
  bool changed_any;
  int noldcolumns;
  int i;
  int j;

     
                                                                       
                                                                          
                                  
     
  if (rte->rtekind == RTE_RELATION)
  {
                                                                      
    Relation rel;
    TupleDesc tupdesc;

    rel = relation_open(rte->relid, AccessShareLock);
    tupdesc = RelationGetDescr(rel);

    ncolumns = tupdesc->natts;
    real_colnames = (char **)palloc(ncolumns * sizeof(char *));

    for (i = 0; i < ncolumns; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

      if (attr->attisdropped)
      {
        real_colnames[i] = NULL;
      }
      else
      {
        real_colnames[i] = pstrdup(NameStr(attr->attname));
      }
    }
    relation_close(rel, AccessShareLock);
  }
  else
  {
                                                                 
    List *colnames;
    ListCell *lc;

       
                                                                           
                                                                         
                                                                      
                                                                     
                                                                      
                                                                          
                                                                           
                                                                   
                                                                          
       
                                                                           
                                                                       
                                                               
       
    if (rte->rtekind == RTE_FUNCTION && rte->functions != NIL)
    {
                                                                    
      expandRTE(rte, 1, 0, -1, true                      , &colnames, NULL);
    }
    else
    {
      colnames = rte->eref->colnames;
    }

    ncolumns = list_length(colnames);
    real_colnames = (char **)palloc(ncolumns * sizeof(char *));

    i = 0;
    foreach (lc, colnames)
    {
         
                                                                         
                                            
         
      char *cname = strVal(lfirst(lc));

      if (cname[0] == '\0')
      {
        cname = NULL;
      }
      real_colnames[i] = cname;
      i++;
    }
  }

     
                                                                             
                                                                           
                                                                            
                                                                             
                                                                            
                                                             
     
  expand_colnames_array_to(colinfo, ncolumns);
  Assert(colinfo->num_cols == ncolumns);

     
                                                                      
     
                                                                             
                                                                             
                                     
     
  colinfo->new_colnames = (char **)palloc(ncolumns * sizeof(char *));
  colinfo->is_new_col = (bool *)palloc(ncolumns * sizeof(bool));

     
                                                                           
                                                                             
                                                                    
                                                                            
                                                                       
     
  noldcolumns = list_length(rte->eref->colnames);
  changed_any = false;
  j = 0;
  for (i = 0; i < ncolumns; i++)
  {
    char *real_colname = real_colnames[i];
    char *colname = colinfo->colnames[i];

                              
    if (real_colname == NULL)
    {
      Assert(colname == NULL);                                  
      continue;
    }

                                                       
    if (colname == NULL)
    {
                                                                     
      if (rte->alias && i < list_length(rte->alias->colnames))
      {
        colname = strVal(list_nth(rte->alias->colnames, i));
      }
      else
      {
        colname = real_colname;
      }

                                              
      colname = make_colname_unique(colname, dpns, colinfo);

      colinfo->colnames[i] = colname;
    }

                                                                
    colinfo->new_colnames[j] = colname;
                                     
    colinfo->is_new_col[j] = (i >= noldcolumns);
    j++;

                                                                  
    if (!changed_any && strcmp(colname, real_colname) != 0)
    {
      changed_any = true;
    }
  }

     
                                                                          
                                                                            
                                                                           
                                                           
     
  colinfo->num_new_cols = j;

     
                                                                          
                                                                          
                                                                          
                                                                         
                                                                            
                                                                           
                                                                       
                                                                           
     
  if (rte->rtekind == RTE_RELATION)
  {
    colinfo->printaliases = changed_any;
  }
  else if (rte->rtekind == RTE_FUNCTION)
  {
    colinfo->printaliases = true;
  }
  else if (rte->rtekind == RTE_TABLEFUNC)
  {
    colinfo->printaliases = false;
  }
  else if (rte->alias && rte->alias->colnames != NIL)
  {
    colinfo->printaliases = true;
  }
  else
  {
    colinfo->printaliases = changed_any;
  }
}

   
                                                               
   
                                                                              
                                                                       
                                                                  
                                                                              
                                  
   
static void
set_join_column_names(deparse_namespace *dpns, RangeTblEntry *rte, deparse_columns *colinfo)
{
  deparse_columns *leftcolinfo;
  deparse_columns *rightcolinfo;
  bool changed_any;
  int noldcolumns;
  int nnewcolumns;
  Bitmapset *leftmerged = NULL;
  Bitmapset *rightmerged = NULL;
  int i;
  int j;
  int ic;
  int jc;

                                                                      
  leftcolinfo = deparse_columns_fetch(colinfo->leftrti, dpns);
  rightcolinfo = deparse_columns_fetch(colinfo->rightrti, dpns);

     
                                                                             
                                                                           
                                                                            
                                                                         
                                                             
     
  noldcolumns = list_length(rte->eref->colnames);
  expand_colnames_array_to(colinfo, noldcolumns);
  Assert(colinfo->num_cols == noldcolumns);

     
                                                                           
                                                                             
                                                                         
                        
     
  changed_any = false;
  for (i = list_length(colinfo->usingNames); i < noldcolumns; i++)
  {
    char *colname = colinfo->colnames[i];
    char *real_colname;

                                                                     
    if (colinfo->leftattnos[i] == 0 && colinfo->rightattnos[i] == 0)
    {
      Assert(colname == NULL);
      continue;
    }

                                   
    if (colinfo->leftattnos[i] > 0)
    {
      real_colname = leftcolinfo->colnames[colinfo->leftattnos[i] - 1];
    }
    else if (colinfo->rightattnos[i] > 0)
    {
      real_colname = rightcolinfo->colnames[colinfo->rightattnos[i] - 1];
    }
    else
    {
                                                          
      real_colname = strVal(list_nth(rte->eref->colnames, i));
    }
    Assert(real_colname != NULL);

                                                                  
    if (rte->alias == NULL)
    {
      colinfo->colnames[i] = real_colname;
      continue;
    }

                                                       
    if (colname == NULL)
    {
                                                                     
      if (rte->alias && i < list_length(rte->alias->colnames))
      {
        colname = strVal(list_nth(rte->alias->colnames, i));
      }
      else
      {
        colname = real_colname;
      }

                                              
      colname = make_colname_unique(colname, dpns, colinfo);

      colinfo->colnames[i] = colname;
    }

                                                                  
    if (!changed_any && strcmp(colname, real_colname) != 0)
    {
      changed_any = true;
    }
  }

     
                                                                          
                                                                         
     
                                                                          
                                                                
     
  nnewcolumns = leftcolinfo->num_new_cols + rightcolinfo->num_new_cols - list_length(colinfo->usingNames);
  colinfo->num_new_cols = nnewcolumns;
  colinfo->new_colnames = (char **)palloc0(nnewcolumns * sizeof(char *));
  colinfo->is_new_col = (bool *)palloc0(nnewcolumns * sizeof(bool));

     
                                                                             
                                                                             
                                                                        
                                                                             
                                                                          
                                                                             
                                                                            
                                                                           
                                                                             
     
                                                                             
                                                                          
                                         
     

                                                              
  i = j = 0;
  while (i < noldcolumns && colinfo->leftattnos[i] != 0 && colinfo->rightattnos[i] != 0)
  {
                                                            
    colinfo->new_colnames[j] = colinfo->colnames[i];
    colinfo->is_new_col[j] = false;

                                                             
    if (colinfo->leftattnos[i] > 0)
    {
      leftmerged = bms_add_member(leftmerged, colinfo->leftattnos[i]);
    }
    if (colinfo->rightattnos[i] > 0)
    {
      rightmerged = bms_add_member(rightmerged, colinfo->rightattnos[i]);
    }

    i++, j++;
  }

                                            
  ic = 0;
  for (jc = 0; jc < leftcolinfo->num_new_cols; jc++)
  {
    char *child_colname = leftcolinfo->new_colnames[jc];

    if (!leftcolinfo->is_new_col[jc])
    {
                                                                   
      while (ic < leftcolinfo->num_cols && leftcolinfo->colnames[ic] == NULL)
      {
        ic++;
      }
      Assert(ic < leftcolinfo->num_cols);
      ic++;
                                                             
      if (bms_is_member(ic, leftmerged))
      {
        continue;
      }
                                                                     
      while (i < colinfo->num_cols && colinfo->colnames[i] == NULL)
      {
        i++;
      }
      Assert(i < colinfo->num_cols);
      Assert(ic == colinfo->leftattnos[i]);
                                                        
      colinfo->new_colnames[j] = colinfo->colnames[i];
      i++;
    }
    else
    {
         
                                                                       
                                                     
         
      if (rte->alias != NULL)
      {
        colinfo->new_colnames[j] = make_colname_unique(child_colname, dpns, colinfo);
        if (!changed_any && strcmp(colinfo->new_colnames[j], child_colname) != 0)
        {
          changed_any = true;
        }
      }
      else
      {
        colinfo->new_colnames[j] = child_colname;
      }
    }

    colinfo->is_new_col[j] = leftcolinfo->is_new_col[jc];
    j++;
  }

                                                                     
  ic = 0;
  for (jc = 0; jc < rightcolinfo->num_new_cols; jc++)
  {
    char *child_colname = rightcolinfo->new_colnames[jc];

    if (!rightcolinfo->is_new_col[jc])
    {
                                                                    
      while (ic < rightcolinfo->num_cols && rightcolinfo->colnames[ic] == NULL)
      {
        ic++;
      }
      Assert(ic < rightcolinfo->num_cols);
      ic++;
                                                             
      if (bms_is_member(ic, rightmerged))
      {
        continue;
      }
                                                                     
      while (i < colinfo->num_cols && colinfo->colnames[i] == NULL)
      {
        i++;
      }
      Assert(i < colinfo->num_cols);
      Assert(ic == colinfo->rightattnos[i]);
                                                        
      colinfo->new_colnames[j] = colinfo->colnames[i];
      i++;
    }
    else
    {
         
                                                                       
                                                     
         
      if (rte->alias != NULL)
      {
        colinfo->new_colnames[j] = make_colname_unique(child_colname, dpns, colinfo);
        if (!changed_any && strcmp(colinfo->new_colnames[j], child_colname) != 0)
        {
          changed_any = true;
        }
      }
      else
      {
        colinfo->new_colnames[j] = child_colname;
      }
    }

    colinfo->is_new_col[j] = rightcolinfo->is_new_col[jc];
    j++;
  }

                                                       
#ifdef USE_ASSERT_CHECKING
  while (i < colinfo->num_cols && colinfo->colnames[i] == NULL)
  {
    i++;
  }
  Assert(i == colinfo->num_cols);
  Assert(j == nnewcolumns);
#endif

     
                                                                             
                                                 
     
  if (rte->alias != NULL)
  {
    colinfo->printaliases = changed_any;
  }
  else
  {
    colinfo->printaliases = false;
  }
}

   
                                                                            
   
                                                            
   
static bool
colname_is_unique(const char *colname, deparse_namespace *dpns, deparse_columns *colinfo)
{
  int i;
  ListCell *lc;

                                                                
  for (i = 0; i < colinfo->num_cols; i++)
  {
    char *oldname = colinfo->colnames[i];

    if (oldname && strcmp(oldname, colname) == 0)
    {
      return false;
    }
  }

     
                                                                          
                                                                      
     
  for (i = 0; i < colinfo->num_new_cols; i++)
  {
    char *oldname = colinfo->new_colnames[i];

    if (oldname && strcmp(oldname, colname) == 0)
    {
      return false;
    }
  }

                                                                          
  foreach (lc, dpns->using_names)
  {
    char *oldname = (char *)lfirst(lc);

    if (strcmp(oldname, colname) == 0)
    {
      return false;
    }
  }

                                                                            
  foreach (lc, colinfo->parentUsing)
  {
    char *oldname = (char *)lfirst(lc);

    if (strcmp(oldname, colname) == 0)
    {
      return false;
    }
  }

  return true;
}

   
                                                                      
   
                                                            
   
static char *
make_colname_unique(char *colname, deparse_namespace *dpns, deparse_columns *colinfo)
{
     
                                                                            
                                                                    
                  
     
  if (!colname_is_unique(colname, dpns, colinfo))
  {
    int colnamelen = strlen(colname);
    char *modname = (char *)palloc(colnamelen + 16);
    int i = 0;

    do
    {
      i++;
      for (;;)
      {
           
                                                                    
                                                                   
                     
           
        memcpy(modname, colname, colnamelen);
        sprintf(modname + colnamelen, "_%d", i);
        if (strlen(modname) < NAMEDATALEN)
        {
          break;
        }
                                                            
        colnamelen = pg_mbcliplen(colname, colnamelen, colnamelen - 1);
      }
    } while (!colname_is_unique(modname, dpns, colinfo));
    colname = modname;
  }
  return colname;
}

   
                                                                          
   
                                                    
   
static void
expand_colnames_array_to(deparse_columns *colinfo, int n)
{
  if (n > colinfo->num_cols)
  {
    if (colinfo->colnames == NULL)
    {
      colinfo->colnames = (char **)palloc0(n * sizeof(char *));
    }
    else
    {
      colinfo->colnames = (char **)repalloc(colinfo->colnames, n * sizeof(char *));
      memset(colinfo->colnames + colinfo->num_cols, 0, (n - colinfo->num_cols) * sizeof(char *));
    }
    colinfo->num_cols = n;
  }
}

   
                                                                       
   
                                                                    
                                     
   
static void
identify_join_columns(JoinExpr *j, RangeTblEntry *jrte, deparse_columns *colinfo)
{
  int numjoincols;
  int i;
  ListCell *lc;

                                           
  if (IsA(j->larg, RangeTblRef))
  {
    colinfo->leftrti = ((RangeTblRef *)j->larg)->rtindex;
  }
  else if (IsA(j->larg, JoinExpr))
  {
    colinfo->leftrti = ((JoinExpr *)j->larg)->rtindex;
  }
  else
  {
    elog(ERROR, "unrecognized node type in jointree: %d", (int)nodeTag(j->larg));
  }
  if (IsA(j->rarg, RangeTblRef))
  {
    colinfo->rightrti = ((RangeTblRef *)j->rarg)->rtindex;
  }
  else if (IsA(j->rarg, JoinExpr))
  {
    colinfo->rightrti = ((JoinExpr *)j->rarg)->rtindex;
  }
  else
  {
    elog(ERROR, "unrecognized node type in jointree: %d", (int)nodeTag(j->rarg));
  }

                                                                          
  Assert(colinfo->leftrti < j->rtindex);
  Assert(colinfo->rightrti < j->rtindex);

                                            
  numjoincols = list_length(jrte->joinaliasvars);
  Assert(numjoincols == list_length(jrte->eref->colnames));
  colinfo->leftattnos = (int *)palloc0(numjoincols * sizeof(int));
  colinfo->rightattnos = (int *)palloc0(numjoincols * sizeof(int));

                                                                        
  i = 0;
  foreach (lc, jrte->joinaliasvars)
  {
    Var *aliasvar = (Var *)lfirst(lc);

                                                        
    aliasvar = (Var *)strip_implicit_coercions((Node *)aliasvar);

    if (aliasvar == NULL)
    {
                                                     
    }
    else if (IsA(aliasvar, Var))
    {
      Assert(aliasvar->varlevelsup == 0);
      Assert(aliasvar->varattno != 0);
      if (aliasvar->varno == colinfo->leftrti)
      {
        colinfo->leftattnos[i] = aliasvar->varattno;
      }
      else if (aliasvar->varno == colinfo->rightrti)
      {
        colinfo->rightattnos[i] = aliasvar->varattno;
      }
      else
      {
        elog(ERROR, "unexpected varno %d in JOIN RTE", aliasvar->varno);
      }
    }
    else if (IsA(aliasvar, CoalesceExpr))
    {
         
                                                                         
                                                         
         
    }
    else
    {
      elog(ERROR, "unrecognized node type in join alias vars: %d", (int)nodeTag(aliasvar));
    }

    i++;
  }

     
                                                                           
                                                                         
                                                                          
                                                                             
                                                                             
                                                                
     
  if (j->usingClause)
  {
    List *leftvars = NIL;
    List *rightvars = NIL;
    ListCell *lc2;

                                                                    
    flatten_join_using_qual(j->quals, &leftvars, &rightvars);
    Assert(list_length(leftvars) == list_length(j->usingClause));
    Assert(list_length(rightvars) == list_length(j->usingClause));

                                             
    i = 0;
    forboth(lc, leftvars, lc2, rightvars)
    {
      Var *leftvar = (Var *)lfirst(lc);
      Var *rightvar = (Var *)lfirst(lc2);

      Assert(leftvar->varlevelsup == 0);
      Assert(leftvar->varattno != 0);
      if (leftvar->varno != colinfo->leftrti)
      {
        elog(ERROR, "unexpected varno %d in JOIN USING qual", leftvar->varno);
      }
      colinfo->leftattnos[i] = leftvar->varattno;

      Assert(rightvar->varlevelsup == 0);
      Assert(rightvar->varattno != 0);
      if (rightvar->varno != colinfo->rightrti)
      {
        elog(ERROR, "unexpected varno %d in JOIN USING qual", rightvar->varno);
      }
      colinfo->rightattnos[i] = rightvar->varattno;

      i++;
    }
  }
}

   
                                                                             
   
                                                                               
                                                                            
                                                                              
   
                                                   
   
static void
flatten_join_using_qual(Node *qual, List **leftvars, List **rightvars)
{
  if (IsA(qual, BoolExpr))
  {
                                       
    BoolExpr *b = (BoolExpr *)qual;
    ListCell *lc;

    Assert(b->boolop == AND_EXPR);
    foreach (lc, b->args)
    {
      flatten_join_using_qual((Node *)lfirst(lc), leftvars, rightvars);
    }
  }
  else if (IsA(qual, OpExpr))
  {
                                                       
    OpExpr *op = (OpExpr *)qual;
    Var *var;

    if (list_length(op->args) != 2)
    {
      elog(ERROR, "unexpected unary operator in JOIN/USING qual");
    }
                                                                  
    var = (Var *)strip_implicit_coercions((Node *)linitial(op->args));
    if (!IsA(var, Var))
    {
      elog(ERROR, "unexpected node type in JOIN/USING qual: %d", (int)nodeTag(var));
    }
    *leftvars = lappend(*leftvars, var);
    var = (Var *)strip_implicit_coercions((Node *)lsecond(op->args));
    if (!IsA(var, Var))
    {
      elog(ERROR, "unexpected node type in JOIN/USING qual: %d", (int)nodeTag(var));
    }
    *rightvars = lappend(*rightvars, var);
  }
  else
  {
                                                          
    Node *q = strip_implicit_coercions(qual);

    if (q != qual)
    {
      flatten_join_using_qual(q, leftvars, rightvars);
    }
    else
    {
      elog(ERROR, "unexpected node type in JOIN/USING qual: %d", (int)nodeTag(qual));
    }
  }
}

   
                                                                                
   
                                                                    
   
static char *
get_rtable_name(int rtindex, deparse_context *context)
{
  deparse_namespace *dpns = (deparse_namespace *)linitial(context->namespaces);

  Assert(rtindex > 0 && rtindex <= list_length(dpns->rtable_names));
  return (char *)list_nth(dpns->rtable_names, rtindex - 1);
}

   
                                                                           
                             
   
                                                                           
                                                                             
                                                                             
                                                                           
                     
   
static void
set_deparse_planstate(deparse_namespace *dpns, PlanState *ps)
{
  dpns->planstate = ps;

     
                                                                            
                                                                          
                                                                            
                                                                            
                                                                          
                                                          
     
  if (IsA(ps, AppendState))
  {
    dpns->outer_planstate = ((AppendState *)ps)->appendplans[0];
  }
  else if (IsA(ps, MergeAppendState))
  {
    dpns->outer_planstate = ((MergeAppendState *)ps)->mergeplans[0];
  }
  else if (IsA(ps, ModifyTableState))
  {
    dpns->outer_planstate = ((ModifyTableState *)ps)->mt_plans[0];
  }
  else
  {
    dpns->outer_planstate = outerPlanState(ps);
  }

  if (dpns->outer_planstate)
  {
    dpns->outer_tlist = dpns->outer_planstate->plan->targetlist;
  }
  else
  {
    dpns->outer_tlist = NIL;
  }

     
                                                                           
                                                                             
                                                                             
                                                                            
                                                                             
                                                                         
                                       
     
  if (IsA(ps, SubqueryScanState))
  {
    dpns->inner_planstate = ((SubqueryScanState *)ps)->subplan;
  }
  else if (IsA(ps, CteScanState))
  {
    dpns->inner_planstate = ((CteScanState *)ps)->cteplanstate;
  }
  else if (IsA(ps, ModifyTableState))
  {
    dpns->inner_planstate = ps;
  }
  else
  {
    dpns->inner_planstate = innerPlanState(ps);
  }

  if (IsA(ps, ModifyTableState))
  {
    dpns->inner_tlist = ((ModifyTableState *)ps)->mt_excludedtlist;
  }
  else if (dpns->inner_planstate)
  {
    dpns->inner_tlist = dpns->inner_planstate->plan->targetlist;
  }
  else
  {
    dpns->inner_tlist = NIL;
  }

                                                     
  if (IsA(ps->plan, IndexOnlyScan))
  {
    dpns->index_tlist = ((IndexOnlyScan *)ps->plan)->indextlist;
  }
  else if (IsA(ps->plan, ForeignScan))
  {
    dpns->index_tlist = ((ForeignScan *)ps->plan)->fdw_scan_tlist;
  }
  else if (IsA(ps->plan, CustomScan))
  {
    dpns->index_tlist = ((CustomScan *)ps->plan)->custom_scan_tlist;
  }
  else
  {
    dpns->index_tlist = NIL;
  }
}

   
                                                                             
   
                                                                          
                                                                 
                                                                         
                                                                             
            
   
                                                                      
                                      
   
static void
push_child_plan(deparse_namespace *dpns, PlanState *ps, deparse_namespace *save_dpns)
{
                                        
  *save_dpns = *dpns;

                                                  
  dpns->ancestors = lcons(dpns->planstate, dpns->ancestors);

                                       
  set_deparse_planstate(dpns, ps);
}

   
                                                       
   
static void
pop_child_plan(deparse_namespace *dpns, deparse_namespace *save_dpns)
{
  List *ancestors;

                                                               
  ancestors = list_delete_first(dpns->ancestors);

                                                 
  *dpns = *save_dpns;

                                                               
  dpns->ancestors = ancestors;
}

   
                                                                      
                 
   
                                                                        
                                                                      
                                                                     
                                           
   
                                                                             
                       
   
                                                                      
                                         
   
static void
push_ancestor_plan(deparse_namespace *dpns, ListCell *ancestor_cell, deparse_namespace *save_dpns)
{
  PlanState *ps = (PlanState *)lfirst(ancestor_cell);
  List *ancestors;

                                        
  *save_dpns = *dpns;

                                                                 
  ancestors = NIL;
  while ((ancestor_cell = lnext(ancestor_cell)) != NULL)
  {
    ancestors = lappend(ancestors, lfirst(ancestor_cell));
  }
  dpns->ancestors = ancestors;

                                          
  set_deparse_planstate(dpns, ps);
}

   
                                                             
   
static void
pop_ancestor_plan(deparse_namespace *dpns, deparse_namespace *save_dpns)
{
                                                         
  list_free(dpns->ancestors);

                                                    
  *dpns = *save_dpns;
}

              
                                                        
                                     
              
   
static void
make_ruledef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc, int prettyFlags)
{
  char *rulename;
  char ev_type;
  Oid ev_class;
  bool is_instead;
  char *ev_qual;
  char *ev_action;
  List *actions = NIL;
  Relation ev_relation;
  TupleDesc viewResultDesc = NULL;
  int fno;
  Datum dat;
  bool isnull;

     
                                                   
     
  fno = SPI_fnumber(rulettc, "rulename");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  rulename = NameStr(*(DatumGetName(dat)));

  fno = SPI_fnumber(rulettc, "ev_type");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  ev_type = DatumGetChar(dat);

  fno = SPI_fnumber(rulettc, "ev_class");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  ev_class = DatumGetObjectId(dat);

  fno = SPI_fnumber(rulettc, "is_instead");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  is_instead = DatumGetBool(dat);

                            
  fno = SPI_fnumber(rulettc, "ev_qual");
  ev_qual = SPI_getvalue(ruletup, rulettc, fno);

  fno = SPI_fnumber(rulettc, "ev_action");
  ev_action = SPI_getvalue(ruletup, rulettc, fno);
  if (ev_action != NULL)
  {
    actions = (List *)stringToNode(ev_action);
  }

  ev_relation = table_open(ev_class, AccessShareLock);

     
                                     
     
  appendStringInfo(buf, "CREATE RULE %s AS", quote_identifier(rulename));

  if (prettyFlags & PRETTYFLAG_INDENT)
  {
    appendStringInfoString(buf, "\n    ON ");
  }
  else
  {
    appendStringInfoString(buf, " ON ");
  }

                                       
  switch (ev_type)
  {
  case '1':
    appendStringInfoString(buf, "SELECT");
    viewResultDesc = RelationGetDescr(ev_relation);
    break;

  case '2':
    appendStringInfoString(buf, "UPDATE");
    break;

  case '3':
    appendStringInfoString(buf, "INSERT");
    break;

  case '4':
    appendStringInfoString(buf, "DELETE");
    break;

  default:
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("rule \"%s\" has unsupported event type %d", rulename, ev_type)));
    break;
  }

                                         
  appendStringInfo(buf, " TO %s", (prettyFlags & PRETTYFLAG_SCHEMA) ? generate_relation_name(ev_class, NIL) : generate_qualified_relation_name(ev_class));

                                                      
  if (ev_qual == NULL)
  {
    ev_qual = "";
  }
  if (strlen(ev_qual) > 0 && strcmp(ev_qual, "<>") != 0)
  {
    Node *qual;
    Query *query;
    deparse_context context;
    deparse_namespace dpns;

    if (prettyFlags & PRETTYFLAG_INDENT)
    {
      appendStringInfoString(buf, "\n  ");
    }
    appendStringInfoString(buf, " WHERE ");

    qual = stringToNode(ev_qual);

       
                                                                      
                                                                         
                                                            
       
    query = (Query *)linitial(actions);

       
                                                                       
                                                                        
                                                         
       
    query = getInsertSelectQuery(query, NULL);

                                                                     
    AcquireRewriteLocks(query, false, false);

    context.buf = buf;
    context.namespaces = list_make1(&dpns);
    context.windowClause = NIL;
    context.windowTList = NIL;
    context.varprefix = (list_length(query->rtable) != 1);
    context.prettyFlags = prettyFlags;
    context.wrapColumn = WRAP_COLUMN_DEFAULT;
    context.indentLevel = PRETTYINDENT_STD;
    context.special_exprkind = EXPR_KIND_NONE;

    set_deparse_for_query(&dpns, query, NIL);

    get_rule_expr(qual, &context, false);
  }

  appendStringInfoString(buf, " DO ");

                                   
  if (is_instead)
  {
    appendStringInfoString(buf, "INSTEAD ");
  }

                                 
  if (list_length(actions) > 1)
  {
    ListCell *action;
    Query *query;

    appendStringInfoChar(buf, '(');
    foreach (action, actions)
    {
      query = (Query *)lfirst(action);
      get_query_def(query, buf, NIL, viewResultDesc, true, prettyFlags, WRAP_COLUMN_DEFAULT, 0);
      if (prettyFlags)
      {
        appendStringInfoString(buf, ";\n");
      }
      else
      {
        appendStringInfoString(buf, "; ");
      }
    }
    appendStringInfoString(buf, ");");
  }
  else if (list_length(actions) == 0)
  {
    appendStringInfoString(buf, "NOTHING;");
  }
  else
  {
    Query *query;

    query = (Query *)linitial(actions);
    get_query_def(query, buf, NIL, viewResultDesc, true, prettyFlags, WRAP_COLUMN_DEFAULT, 0);
    appendStringInfoChar(buf, ';');
  }

  table_close(ev_relation, AccessShareLock);
}

              
                                                     
                          
              
   
static void
make_viewdef(StringInfo buf, HeapTuple ruletup, TupleDesc rulettc, int prettyFlags, int wrapColumn)
{
  Query *query;
  char ev_type;
  Oid ev_class;
  bool is_instead;
  char *ev_qual;
  char *ev_action;
  List *actions = NIL;
  Relation ev_relation;
  int fno;
  Datum dat;
  bool isnull;

     
                                                   
     
  fno = SPI_fnumber(rulettc, "ev_type");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  ev_type = DatumGetChar(dat);

  fno = SPI_fnumber(rulettc, "ev_class");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  ev_class = DatumGetObjectId(dat);

  fno = SPI_fnumber(rulettc, "is_instead");
  dat = SPI_getbinval(ruletup, rulettc, fno, &isnull);
  Assert(!isnull);
  is_instead = DatumGetBool(dat);

                            
  fno = SPI_fnumber(rulettc, "ev_qual");
  ev_qual = SPI_getvalue(ruletup, rulettc, fno);

  fno = SPI_fnumber(rulettc, "ev_action");
  ev_action = SPI_getvalue(ruletup, rulettc, fno);
  if (ev_action != NULL)
  {
    actions = (List *)stringToNode(ev_action);
  }

  if (list_length(actions) != 1)
  {
                                            
    return;
  }

  query = (Query *)linitial(actions);

  if (ev_type != '1' || !is_instead || strcmp(ev_qual, "<>") != 0 || query->commandType != CMD_SELECT)
  {
                                            
    return;
  }

  ev_relation = table_open(ev_class, AccessShareLock);

  get_query_def(query, buf, NIL, RelationGetDescr(ev_relation), true, prettyFlags, wrapColumn, 0);
  appendStringInfoChar(buf, ';');

  table_close(ev_relation, AccessShareLock);
}

              
                                                    
   
                                    
                                       
                                                                              
                                                                     
                                                                    
                                                                        
                                                                           
                                                                         
                                                                       
                                                  
                                                              
                                           
              
   
static void
get_query_def(Query *query, StringInfo buf, List *parentnamespace, TupleDesc resultDesc, bool colNamesVisible, int prettyFlags, int wrapColumn, int startIndent)
{
  deparse_context context;
  deparse_namespace dpns;

                                                               
  CHECK_FOR_INTERRUPTS();
  check_stack_depth();

     
                                                                       
                                                                       
                                                                           
                
     
                                                                             
                                                             
     
  AcquireRewriteLocks(query, false, false);

  context.buf = buf;
  context.namespaces = lcons(&dpns, list_copy(parentnamespace));
  context.windowClause = NIL;
  context.windowTList = NIL;
  context.varprefix = (parentnamespace != NIL || list_length(query->rtable) != 1);
  context.prettyFlags = prettyFlags;
  context.wrapColumn = wrapColumn;
  context.indentLevel = startIndent;
  context.special_exprkind = EXPR_KIND_NONE;

  set_deparse_for_query(&dpns, query, parentnamespace);

  switch (query->commandType)
  {
  case CMD_SELECT:
    get_select_query_def(query, &context, resultDesc, colNamesVisible);
    break;

  case CMD_UPDATE:
    get_update_query_def(query, &context, colNamesVisible);
    break;

  case CMD_INSERT:
    get_insert_query_def(query, &context, colNamesVisible);
    break;

  case CMD_DELETE:
    get_delete_query_def(query, &context, colNamesVisible);
    break;

  case CMD_NOTHING:
    appendStringInfoString(buf, "NOTHING");
    break;

  case CMD_UTILITY:
    get_utility_query_def(query, &context);
    break;

  default:
    elog(ERROR, "unrecognized query command type: %d", query->commandType);
    break;
  }
}

              
                                               
              
   
static void
get_values_def(List *values_lists, deparse_context *context)
{
  StringInfo buf = context->buf;
  bool first_list = true;
  ListCell *vtl;

  appendStringInfoString(buf, "VALUES ");

  foreach (vtl, values_lists)
  {
    List *sublist = (List *)lfirst(vtl);
    bool first_col = true;
    ListCell *lc;

    if (first_list)
    {
      first_list = false;
    }
    else
    {
      appendStringInfoString(buf, ", ");
    }

    appendStringInfoChar(buf, '(');
    foreach (lc, sublist)
    {
      Node *col = (Node *)lfirst(lc);

      if (first_col)
      {
        first_col = false;
      }
      else
      {
        appendStringInfoChar(buf, ',');
      }

         
                                                                  
         
      get_rule_expr_toplevel(col, context, false);
    }
    appendStringInfoChar(buf, ')');
  }
}

              
                                                
              
   
static void
get_with_clause(Query *query, deparse_context *context)
{
  StringInfo buf = context->buf;
  const char *sep;
  ListCell *l;

  if (query->cteList == NIL)
  {
    return;
  }

  if (PRETTY_INDENT(context))
  {
    context->indentLevel += PRETTYINDENT_STD;
    appendStringInfoChar(buf, ' ');
  }

  if (query->hasRecursive)
  {
    sep = "WITH RECURSIVE ";
  }
  else
  {
    sep = "WITH ";
  }
  foreach (l, query->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(l);

    appendStringInfoString(buf, sep);
    appendStringInfoString(buf, quote_identifier(cte->ctename));
    if (cte->aliascolnames)
    {
      bool first = true;
      ListCell *col;

      appendStringInfoChar(buf, '(');
      foreach (col, cte->aliascolnames)
      {
        if (first)
        {
          first = false;
        }
        else
        {
          appendStringInfoString(buf, ", ");
        }
        appendStringInfoString(buf, quote_identifier(strVal(lfirst(col))));
      }
      appendStringInfoChar(buf, ')');
    }
    appendStringInfoString(buf, " AS ");
    switch (cte->ctematerialized)
    {
    case CTEMaterializeDefault:
      break;
    case CTEMaterializeAlways:
      appendStringInfoString(buf, "MATERIALIZED ");
      break;
    case CTEMaterializeNever:
      appendStringInfoString(buf, "NOT MATERIALIZED ");
      break;
    }
    appendStringInfoChar(buf, '(');
    if (PRETTY_INDENT(context))
    {
      appendContextKeyword(context, "", 0, 0, 0);
    }
    get_query_def((Query *)cte->ctequery, buf, context->namespaces, NULL, true, context->prettyFlags, context->wrapColumn, context->indentLevel);
    if (PRETTY_INDENT(context))
    {
      appendContextKeyword(context, "", 0, 0, 0);
    }
    appendStringInfoChar(buf, ')');
    sep = ", ";
  }

  if (PRETTY_INDENT(context))
  {
    context->indentLevel -= PRETTYINDENT_STD;
    appendContextKeyword(context, "", 0, 0, 0);
  }
  else
  {
    appendStringInfoChar(buf, ' ');
  }
}

              
                                                          
              
   
static void
get_select_query_def(Query *query, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  List *save_windowclause;
  List *save_windowtlist;
  bool force_colno;
  ListCell *l;

                                       
  get_with_clause(query, context);

                                                    
  save_windowclause = context->windowClause;
  context->windowClause = query->windowClause;
  save_windowtlist = context->windowTList;
  context->windowTList = query->targetList;

     
                                                                            
                                                                       
                                                     
     
  if (query->setOperations)
  {
    get_setop_query(query->setOperations, query, context, resultDesc, colNamesVisible);
                                                      
    force_colno = true;
  }
  else
  {
    get_basic_select_query(query, context, resultDesc, colNamesVisible);
    force_colno = false;
  }

                                        
  if (query->sortClause != NIL)
  {
    appendContextKeyword(context, " ORDER BY ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_rule_orderby(query->sortClause, query->targetList, force_colno, context);
  }

                                     
  if (query->limitOffset != NULL)
  {
    appendContextKeyword(context, " OFFSET ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
    get_rule_expr(query->limitOffset, context, false);
  }
  if (query->limitCount != NULL)
  {
    appendContextKeyword(context, " LIMIT ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
    if (IsA(query->limitCount, Const) && ((Const *)query->limitCount)->constisnull)
    {
      appendStringInfoString(buf, "ALL");
    }
    else
    {
      get_rule_expr(query->limitCount, context, false);
    }
  }

                                                     
  if (query->hasForUpdate)
  {
    foreach (l, query->rowMarks)
    {
      RowMarkClause *rc = (RowMarkClause *)lfirst(l);

                                        
      if (rc->pushedDown)
      {
        continue;
      }

      switch (rc->strength)
      {
      case LCS_NONE:
                                                          
        elog(ERROR, "unrecognized LockClauseStrength %d", (int)rc->strength);
        break;
      case LCS_FORKEYSHARE:
        appendContextKeyword(context, " FOR KEY SHARE", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
        break;
      case LCS_FORSHARE:
        appendContextKeyword(context, " FOR SHARE", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
        break;
      case LCS_FORNOKEYUPDATE:
        appendContextKeyword(context, " FOR NO KEY UPDATE", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
        break;
      case LCS_FORUPDATE:
        appendContextKeyword(context, " FOR UPDATE", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
        break;
      }

      appendStringInfo(buf, " OF %s", quote_identifier(get_rtable_name(rc->rti, context)));
      if (rc->waitPolicy == LockWaitError)
      {
        appendStringInfoString(buf, " NOWAIT");
      }
      else if (rc->waitPolicy == LockWaitSkip)
      {
        appendStringInfoString(buf, " SKIP LOCKED");
      }
    }
  }

  context->windowClause = save_windowclause;
  context->windowTList = save_windowtlist;
}

   
                                                             
                                                                
                                                         
   
static RangeTblEntry *
get_simple_values_rte(Query *query, TupleDesc resultDesc)
{
  RangeTblEntry *result = NULL;
  ListCell *lc;

     
                                                                          
                                                                            
                                            
     
  foreach (lc, query->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

    if (rte->rtekind == RTE_VALUES && rte->inFromCl)
    {
      if (result)
      {
        return NULL;                                              
      }
      result = rte;
    }
    else if (rte->rtekind == RTE_RELATION && !rte->inFromCl)
    {
      continue;                          
    }
    else
    {
      return NULL;                                          
    }
  }

     
                                                                        
                                                                            
                                                                   
                                                                      
                                                                      
                                                                        
                                                                       
     
  if (result)
  {
    ListCell *lcn;
    int colno;

    if (list_length(query->targetList) != list_length(result->eref->colnames))
    {
      return NULL;                                  
    }
    colno = 0;
    forboth(lc, query->targetList, lcn, result->eref->colnames)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(lc);
      char *cname = strVal(lfirst(lcn));
      char *colname;

      if (tle->resjunk)
      {
        return NULL;                                  
      }

                                                                  
      colno++;
      if (resultDesc && colno <= resultDesc->natts)
      {
        colname = NameStr(TupleDescAttr(resultDesc, colno - 1)->attname);
      }
      else
      {
        colname = tle->resname;
      }

                                         
      if (colname == NULL || strcmp(colname, cname) != 0)
      {
        return NULL;                                   
      }
    }
  }

  return result;
}

static void
get_basic_select_query(Query *query, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  RangeTblEntry *values_rte;
  char *sep;
  ListCell *l;

  if (PRETTY_INDENT(context))
  {
    context->indentLevel += PRETTYINDENT_STD;
    appendStringInfoChar(buf, ' ');
  }

     
                                                                             
                                                                           
           
     
  values_rte = get_simple_values_rte(query, resultDesc);
  if (values_rte)
  {
    get_values_def(values_rte->values_lists, context);
    return;
  }

     
                                                     
     
  appendStringInfoString(buf, "SELECT");

                                        
  if (query->distinctClause != NIL)
  {
    if (query->hasDistinctOn)
    {
      appendStringInfoString(buf, " DISTINCT ON (");
      sep = "";
      foreach (l, query->distinctClause)
      {
        SortGroupClause *srt = (SortGroupClause *)lfirst(l);

        appendStringInfoString(buf, sep);
        get_rule_sortgroupclause(srt->tleSortGroupRef, query->targetList, false, context);
        sep = ", ";
      }
      appendStringInfoChar(buf, ')');
    }
    else
    {
      appendStringInfoString(buf, " DISTINCT");
    }
  }

                                                    
  get_target_list(query->targetList, context, resultDesc, colNamesVisible);

                                     
  get_from_clause(query, " FROM ", context);

                                     
  if (query->jointree->quals != NULL)
  {
    appendContextKeyword(context, " WHERE ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_rule_expr(query->jointree->quals, context, false);
  }

                                        
  if (query->groupClause != NULL || query->groupingSets != NULL)
  {
    ParseExprKind save_exprkind;

    appendContextKeyword(context, " GROUP BY ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);

    save_exprkind = context->special_exprkind;
    context->special_exprkind = EXPR_KIND_GROUP_BY;

    if (query->groupingSets == NIL)
    {
      sep = "";
      foreach (l, query->groupClause)
      {
        SortGroupClause *grp = (SortGroupClause *)lfirst(l);

        appendStringInfoString(buf, sep);
        get_rule_sortgroupclause(grp->tleSortGroupRef, query->targetList, false, context);
        sep = ", ";
      }
    }
    else
    {
      sep = "";
      foreach (l, query->groupingSets)
      {
        GroupingSet *grp = lfirst(l);

        appendStringInfoString(buf, sep);
        get_rule_groupingset(grp, query->targetList, true, context);
        sep = ", ";
      }
    }

    context->special_exprkind = save_exprkind;
  }

                                      
  if (query->havingQual != NULL)
  {
    appendContextKeyword(context, " HAVING ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 0);
    get_rule_expr(query->havingQual, context, false);
  }

                                       
  if (query->windowClause != NIL)
  {
    get_rule_windowclause(query, context);
  }
}

              
                                                       
   
                                                                  
   
                                                             
              
   
static void
get_target_list(List *targetList, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  StringInfoData targetbuf;
  bool last_was_multiline = false;
  char *sep;
  int colno;
  ListCell *l;

                                                            
  initStringInfo(&targetbuf);

  sep = " ";
  colno = 0;
  foreach (l, targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);
    char *colname;
    char *attname;

    if (tle->resjunk)
    {
      continue;                          
    }

    appendStringInfoString(buf, sep);
    sep = ", ";
    colno++;

       
                                                                          
                                                           
       
    resetStringInfo(&targetbuf);
    context->buf = &targetbuf;

       
                                                                          
                                                                    
                                                                         
                                                                      
                                                                     
                                                                      
                                                                          
                                                                    
       
    if (tle->expr && (IsA(tle->expr, Var)))
    {
      attname = get_variable((Var *)tle->expr, 0, true, context);
    }
    else
    {
      get_rule_expr((Node *)tle->expr, context, true);

         
                                                                 
                                                                      
                                            
         
      attname = colNamesVisible ? NULL : "?column?";
    }

       
                                                                           
                                                                        
                                                                   
                                                        
       
    if (resultDesc && colno <= resultDesc->natts)
    {
      colname = NameStr(TupleDescAttr(resultDesc, colno - 1)->attname);
    }
    else
    {
      colname = tle->resname;
    }

                                                           
    if (colname)                            
    {
      if (attname == NULL || strcmp(attname, colname) != 0)
      {
        appendStringInfo(&targetbuf, " AS %s", quote_identifier(colname));
      }
    }

                                         
    context->buf = buf;

                                           
    if (PRETTY_INDENT(context) && context->wrapColumn >= 0)
    {
      int leading_nl_pos;

                                                     
      if (targetbuf.len > 0 && targetbuf.data[0] == '\n')
      {
        leading_nl_pos = 0;
      }
      else
      {
        leading_nl_pos = -1;
      }

                                            
      if (leading_nl_pos >= 0)
      {
                                                                  
        removeStringInfoSpaces(buf);
      }
      else
      {
        char *trailing_nl;

                                                                       
        trailing_nl = strrchr(buf->data, '\n');
        if (trailing_nl == NULL)
        {
          trailing_nl = buf->data;
        }
        else
        {
          trailing_nl++;
        }

           
                                                                     
                                                                 
                                                               
           
        if (colno > 1 && ((strlen(trailing_nl) + targetbuf.len > context->wrapColumn) || last_was_multiline))
        {
          appendContextKeyword(context, "", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_VAR);
        }
      }

                                                                     
      last_was_multiline = (strchr(targetbuf.data + leading_nl_pos + 1, '\n') != NULL);
    }

                           
    appendStringInfoString(buf, targetbuf.data);
  }

                
  pfree(targetbuf.data);
}

static void
get_setop_query(Node *setOp, Query *query, deparse_context *context, TupleDesc resultDesc, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  bool need_paren;

                                                               
  CHECK_FOR_INTERRUPTS();
  check_stack_depth();

  if (IsA(setOp, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)setOp;
    RangeTblEntry *rte = rt_fetch(rtr->rtindex, query->rtable);
    Query *subquery = rte->subquery;

    Assert(subquery != NULL);
    Assert(subquery->setOperations == NULL);
                                                                         
    need_paren = (subquery->cteList || subquery->sortClause || subquery->rowMarks || subquery->limitOffset || subquery->limitCount);
    if (need_paren)
    {
      appendStringInfoChar(buf, '(');
    }
    get_query_def(subquery, buf, context->namespaces, resultDesc, colNamesVisible, context->prettyFlags, context->wrapColumn, context->indentLevel);
    if (need_paren)
    {
      appendStringInfoChar(buf, ')');
    }
  }
  else if (IsA(setOp, SetOperationStmt))
  {
    SetOperationStmt *op = (SetOperationStmt *)setOp;
    int subindent;

       
                                                                           
                                                                         
                                                                           
                                                                         
                                                                           
       
                                                                           
                                                                          
                    
       
    if (IsA(op->larg, SetOperationStmt))
    {
      SetOperationStmt *lop = (SetOperationStmt *)op->larg;

      if (op->op == lop->op && op->all == lop->all)
      {
        need_paren = false;
      }
      else
      {
        need_paren = true;
      }
    }
    else
    {
      need_paren = false;
    }

    if (need_paren)
    {
      appendStringInfoChar(buf, '(');
      subindent = PRETTYINDENT_STD;
      appendContextKeyword(context, "", subindent, 0, 0);
    }
    else
    {
      subindent = 0;
    }

    get_setop_query(op->larg, query, context, resultDesc, colNamesVisible);

    if (need_paren)
    {
      appendContextKeyword(context, ") ", -subindent, 0, 0);
    }
    else if (PRETTY_INDENT(context))
    {
      appendContextKeyword(context, "", -subindent, 0, 0);
    }
    else
    {
      appendStringInfoChar(buf, ' ');
    }

    switch (op->op)
    {
    case SETOP_UNION:
      appendStringInfoString(buf, "UNION ");
      break;
    case SETOP_INTERSECT:
      appendStringInfoString(buf, "INTERSECT ");
      break;
    case SETOP_EXCEPT:
      appendStringInfoString(buf, "EXCEPT ");
      break;
    default:
      elog(ERROR, "unrecognized set op: %d", (int)op->op);
    }
    if (op->all)
    {
      appendStringInfoString(buf, "ALL ");
    }

                                                     
    need_paren = IsA(op->rarg, SetOperationStmt);

       
                                                                           
                                                                  
                         
       
    if (need_paren)
    {
      appendStringInfoChar(buf, '(');
      subindent = PRETTYINDENT_STD;
    }
    else
    {
      subindent = 0;
    }
    appendContextKeyword(context, "", subindent, 0, 0);

    get_setop_query(op->rarg, query, context, resultDesc, false);

    if (PRETTY_INDENT(context))
    {
      context->indentLevel -= subindent;
    }
    if (need_paren)
    {
      appendContextKeyword(context, ")", 0, 0, 0);
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(setOp));
  }
}

   
                                
   
                                                                       
   
static Node *
get_rule_sortgroupclause(Index ref, List *tlist, bool force_colno, deparse_context *context)
{
  StringInfo buf = context->buf;
  TargetEntry *tle;
  Node *expr;

  tle = get_sortgroupref_tle(ref, tlist);
  expr = (Node *)tle->expr;

     
                                                                   
                                                                           
                                                                    
                                                                           
                                                                           
                                                                          
                                                       
     
  if (force_colno)
  {
    Assert(!tle->resjunk);
    appendStringInfo(buf, "%d", tle->resno);
  }
  else if (expr && IsA(expr, Const))
  {
    get_const_expr((Const *)expr, context, 1);
  }
  else if (!expr || IsA(expr, Var))
  {
    get_rule_expr(expr, context, true);
  }
  else
  {
       
                                                                  
                                                                  
                                                                       
                                                                           
                                           
       
    bool need_paren = (PRETTY_PAREN(context) || IsA(expr, FuncExpr) || IsA(expr, Aggref) || IsA(expr, WindowFunc));

    if (need_paren)
    {
      appendStringInfoChar(context->buf, '(');
    }
    get_rule_expr(expr, context, true);
    if (need_paren)
    {
      appendStringInfoChar(context->buf, ')');
    }
  }

  return expr;
}

   
                         
   
static void
get_rule_groupingset(GroupingSet *gset, List *targetlist, bool omit_parens, deparse_context *context)
{
  ListCell *l;
  StringInfo buf = context->buf;
  bool omit_child_parens = true;
  char *sep = "";

  switch (gset->kind)
  {
  case GROUPING_SET_EMPTY:
    appendStringInfoString(buf, "()");
    return;

  case GROUPING_SET_SIMPLE:
  {
    if (!omit_parens || list_length(gset->content) != 1)
    {
      appendStringInfoChar(buf, '(');
    }

    foreach (l, gset->content)
    {
      Index ref = lfirst_int(l);

      appendStringInfoString(buf, sep);
      get_rule_sortgroupclause(ref, targetlist, false, context);
      sep = ", ";
    }

    if (!omit_parens || list_length(gset->content) != 1)
    {
      appendStringInfoChar(buf, ')');
    }
  }
    return;

  case GROUPING_SET_ROLLUP:
    appendStringInfoString(buf, "ROLLUP(");
    break;
  case GROUPING_SET_CUBE:
    appendStringInfoString(buf, "CUBE(");
    break;
  case GROUPING_SET_SETS:
    appendStringInfoString(buf, "GROUPING SETS (");
    omit_child_parens = false;
    break;
  }

  foreach (l, gset->content)
  {
    appendStringInfoString(buf, sep);
    get_rule_groupingset(lfirst(l), targetlist, omit_child_parens, context);
    sep = ", ";
  }

  appendStringInfoChar(buf, ')');
}

   
                             
   
static void
get_rule_orderby(List *orderList, List *targetList, bool force_colno, deparse_context *context)
{
  StringInfo buf = context->buf;
  const char *sep;
  ListCell *l;

  sep = "";
  foreach (l, orderList)
  {
    SortGroupClause *srt = (SortGroupClause *)lfirst(l);
    Node *sortexpr;
    Oid sortcoltype;
    TypeCacheEntry *typentry;

    appendStringInfoString(buf, sep);
    sortexpr = get_rule_sortgroupclause(srt->tleSortGroupRef, targetList, force_colno, context);
    sortcoltype = exprType(sortexpr);
                                                             
    typentry = lookup_type_cache(sortcoltype, TYPECACHE_LT_OPR | TYPECACHE_GT_OPR);
    if (srt->sortop == typentry->lt_opr)
    {
                                                  
      if (srt->nulls_first)
      {
        appendStringInfoString(buf, " NULLS FIRST");
      }
    }
    else if (srt->sortop == typentry->gt_opr)
    {
      appendStringInfoString(buf, " DESC");
                                        
      if (!srt->nulls_first)
      {
        appendStringInfoString(buf, " NULLS LAST");
      }
    }
    else
    {
      appendStringInfo(buf, " USING %s", generate_operator_name(srt->sortop, sortcoltype, sortcoltype));
                                              
      if (srt->nulls_first)
      {
        appendStringInfoString(buf, " NULLS FIRST");
      }
      else
      {
        appendStringInfoString(buf, " NULLS LAST");
      }
    }
    sep = ", ";
  }
}

   
                            
   
                                                                       
                                                               
   
static void
get_rule_windowclause(Query *query, deparse_context *context)
{
  StringInfo buf = context->buf;
  const char *sep;
  ListCell *l;

  sep = NULL;
  foreach (l, query->windowClause)
  {
    WindowClause *wc = (WindowClause *)lfirst(l);

    if (wc->name == NULL)
    {
      continue;                               
    }

    if (sep == NULL)
    {
      appendContextKeyword(context, " WINDOW ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    }
    else
    {
      appendStringInfoString(buf, sep);
    }

    appendStringInfo(buf, "%s AS ", quote_identifier(wc->name));

    get_rule_windowspec(wc, query->targetList, context);

    sep = ", ";
  }
}

   
                               
   
static void
get_rule_windowspec(WindowClause *wc, List *targetList, deparse_context *context)
{
  StringInfo buf = context->buf;
  bool needspace = false;
  const char *sep;
  ListCell *l;

  appendStringInfoChar(buf, '(');
  if (wc->refname)
  {
    appendStringInfoString(buf, quote_identifier(wc->refname));
    needspace = true;
  }
                                                                           
  if (wc->partitionClause && !wc->refname)
  {
    if (needspace)
    {
      appendStringInfoChar(buf, ' ');
    }
    appendStringInfoString(buf, "PARTITION BY ");
    sep = "";
    foreach (l, wc->partitionClause)
    {
      SortGroupClause *grp = (SortGroupClause *)lfirst(l);

      appendStringInfoString(buf, sep);
      get_rule_sortgroupclause(grp->tleSortGroupRef, targetList, false, context);
      sep = ", ";
    }
    needspace = true;
  }
                                                   
  if (wc->orderClause && !wc->copiedOrder)
  {
    if (needspace)
    {
      appendStringInfoChar(buf, ' ');
    }
    appendStringInfoString(buf, "ORDER BY ");
    get_rule_orderby(wc->orderClause, targetList, false, context);
    needspace = true;
  }
                                                                       
  if (wc->frameOptions & FRAMEOPTION_NONDEFAULT)
  {
    if (needspace)
    {
      appendStringInfoChar(buf, ' ');
    }
    if (wc->frameOptions & FRAMEOPTION_RANGE)
    {
      appendStringInfoString(buf, "RANGE ");
    }
    else if (wc->frameOptions & FRAMEOPTION_ROWS)
    {
      appendStringInfoString(buf, "ROWS ");
    }
    else if (wc->frameOptions & FRAMEOPTION_GROUPS)
    {
      appendStringInfoString(buf, "GROUPS ");
    }
    else
    {
      Assert(false);
    }
    if (wc->frameOptions & FRAMEOPTION_BETWEEN)
    {
      appendStringInfoString(buf, "BETWEEN ");
    }
    if (wc->frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING)
    {
      appendStringInfoString(buf, "UNBOUNDED PRECEDING ");
    }
    else if (wc->frameOptions & FRAMEOPTION_START_CURRENT_ROW)
    {
      appendStringInfoString(buf, "CURRENT ROW ");
    }
    else if (wc->frameOptions & FRAMEOPTION_START_OFFSET)
    {
      get_rule_expr(wc->startOffset, context, false);
      if (wc->frameOptions & FRAMEOPTION_START_OFFSET_PRECEDING)
      {
        appendStringInfoString(buf, " PRECEDING ");
      }
      else if (wc->frameOptions & FRAMEOPTION_START_OFFSET_FOLLOWING)
      {
        appendStringInfoString(buf, " FOLLOWING ");
      }
      else
      {
        Assert(false);
      }
    }
    else
    {
      Assert(false);
    }
    if (wc->frameOptions & FRAMEOPTION_BETWEEN)
    {
      appendStringInfoString(buf, "AND ");
      if (wc->frameOptions & FRAMEOPTION_END_UNBOUNDED_FOLLOWING)
      {
        appendStringInfoString(buf, "UNBOUNDED FOLLOWING ");
      }
      else if (wc->frameOptions & FRAMEOPTION_END_CURRENT_ROW)
      {
        appendStringInfoString(buf, "CURRENT ROW ");
      }
      else if (wc->frameOptions & FRAMEOPTION_END_OFFSET)
      {
        get_rule_expr(wc->endOffset, context, false);
        if (wc->frameOptions & FRAMEOPTION_END_OFFSET_PRECEDING)
        {
          appendStringInfoString(buf, " PRECEDING ");
        }
        else if (wc->frameOptions & FRAMEOPTION_END_OFFSET_FOLLOWING)
        {
          appendStringInfoString(buf, " FOLLOWING ");
        }
        else
        {
          Assert(false);
        }
      }
      else
      {
        Assert(false);
      }
    }
    if (wc->frameOptions & FRAMEOPTION_EXCLUDE_CURRENT_ROW)
    {
      appendStringInfoString(buf, "EXCLUDE CURRENT ROW ");
    }
    else if (wc->frameOptions & FRAMEOPTION_EXCLUDE_GROUP)
    {
      appendStringInfoString(buf, "EXCLUDE GROUP ");
    }
    else if (wc->frameOptions & FRAMEOPTION_EXCLUDE_TIES)
    {
      appendStringInfoString(buf, "EXCLUDE TIES ");
    }
                                                      
    buf->len--;
  }
  appendStringInfoChar(buf, ')');
}

              
                                                           
              
   
static void
get_insert_query_def(Query *query, deparse_context *context, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  RangeTblEntry *select_rte = NULL;
  RangeTblEntry *values_rte = NULL;
  RangeTblEntry *rte;
  char *sep;
  ListCell *l;
  List *strippedexprs;

                                       
  get_with_clause(query, context);

     
                                                                       
                                                                     
     
  foreach (l, query->rtable)
  {
    rte = (RangeTblEntry *)lfirst(l);

    if (rte->rtekind == RTE_SUBQUERY)
    {
      if (select_rte)
      {
        elog(ERROR, "too many subquery RTEs in INSERT");
      }
      select_rte = rte;
    }

    if (rte->rtekind == RTE_VALUES)
    {
      if (values_rte)
      {
        elog(ERROR, "too many values RTEs in INSERT");
      }
      values_rte = rte;
    }
  }
  if (select_rte && values_rte)
  {
    elog(ERROR, "both subquery and values RTEs in INSERT");
  }

     
                                              
     
  rte = rt_fetch(query->resultRelation, query->rtable);
  Assert(rte->rtekind == RTE_RELATION);

  if (PRETTY_INDENT(context))
  {
    context->indentLevel += PRETTYINDENT_STD;
    appendStringInfoChar(buf, ' ');
  }
  appendStringInfo(buf, "INSERT INTO %s ", generate_relation_name(rte->relid, NIL));
                                                   
  if (rte->alias != NULL)
  {
    appendStringInfo(buf, "AS %s ", quote_identifier(rte->alias->aliasname));
  }

     
                                                                             
                                                               
     
  strippedexprs = NIL;
  sep = "";
  if (query->targetList)
  {
    appendStringInfoChar(buf, '(');
  }
  foreach (l, query->targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resjunk)
    {
      continue;                          
    }

    appendStringInfoString(buf, sep);
    sep = ", ";

       
                                                                   
                                                              
       
    appendStringInfoString(buf, quote_identifier(get_attname(rte->relid, tle->resno, false)));

       
                                                                         
                                                                         
                                                                  
                                                                           
                                                                
                     
       
    strippedexprs = lappend(strippedexprs, processIndirection((Node *)tle->expr, context));
  }
  if (query->targetList)
  {
    appendStringInfoString(buf, ") ");
  }

  if (query->override)
  {
    if (query->override == OVERRIDING_SYSTEM_VALUE)
    {
      appendStringInfoString(buf, "OVERRIDING SYSTEM VALUE ");
    }
    else if (query->override == OVERRIDING_USER_VALUE)
    {
      appendStringInfoString(buf, "OVERRIDING USER VALUE ");
    }
  }

  if (select_rte)
  {
                        
    get_query_def(select_rte->subquery, buf, NIL, NULL, false, context->prettyFlags, context->wrapColumn, context->indentLevel);
  }
  else if (values_rte)
  {
                                               
    get_values_def(values_rte->values_lists, context);
  }
  else if (strippedexprs)
  {
                                               
    appendContextKeyword(context, "VALUES (", -PRETTYINDENT_STD, PRETTYINDENT_STD, 2);
    get_rule_list_toplevel(strippedexprs, context, false);
    appendStringInfoChar(buf, ')');
  }
  else
  {
                                                      
    appendStringInfoString(buf, "DEFAULT VALUES");
  }

                                  
  if (query->onConflict)
  {
    OnConflictExpr *confl = query->onConflict;

    appendStringInfoString(buf, " ON CONFLICT");

    if (confl->arbiterElems)
    {
                                                 
      appendStringInfoChar(buf, '(');
      get_rule_expr((Node *)confl->arbiterElems, context, false);
      appendStringInfoChar(buf, ')');

                                                             
      if (confl->arbiterWhere != NULL)
      {
        bool save_varprefix;

           
                                                                       
                                                                 
                                                          
           
        save_varprefix = context->varprefix;
        context->varprefix = false;

        appendContextKeyword(context, " WHERE ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
        get_rule_expr(confl->arbiterWhere, context, false);

        context->varprefix = save_varprefix;
      }
    }
    else if (OidIsValid(confl->constraint))
    {
      char *constraint = get_constraint_name(confl->constraint);

      if (!constraint)
      {
        elog(ERROR, "cache lookup failed for constraint %u", confl->constraint);
      }
      appendStringInfo(buf, " ON CONSTRAINT %s", quote_identifier(constraint));
    }

    if (confl->action == ONCONFLICT_NOTHING)
    {
      appendStringInfoString(buf, " DO NOTHING");
    }
    else
    {
      appendStringInfoString(buf, " DO UPDATE SET ");
                              
      get_update_query_targetlist_def(query, confl->onConflictSet, context, rte);

                                       
      if (confl->onConflictWhere != NULL)
      {
        appendContextKeyword(context, " WHERE ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
        get_rule_expr(confl->onConflictWhere, context, false);
      }
    }
  }

                                
  if (query->returningList)
  {
    appendContextKeyword(context, " RETURNING", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_target_list(query->returningList, context, NULL, colNamesVisible);
  }
}

              
                                                           
              
   
static void
get_update_query_def(Query *query, deparse_context *context, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  RangeTblEntry *rte;

                                       
  get_with_clause(query, context);

     
                                             
     
  rte = rt_fetch(query->resultRelation, query->rtable);
  Assert(rte->rtekind == RTE_RELATION);
  if (PRETTY_INDENT(context))
  {
    appendStringInfoChar(buf, ' ');
    context->indentLevel += PRETTYINDENT_STD;
  }
  appendStringInfo(buf, "UPDATE %s%s", only_marker(rte), generate_relation_name(rte->relid, NIL));
  if (rte->alias != NULL)
  {
    appendStringInfo(buf, " %s", quote_identifier(rte->alias->aliasname));
  }
  appendStringInfoString(buf, " SET ");

                          
  get_update_query_targetlist_def(query, query->targetList, context, rte);

                                     
  get_from_clause(query, " FROM ", context);

                                   
  if (query->jointree->quals != NULL)
  {
    appendContextKeyword(context, " WHERE ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_rule_expr(query->jointree->quals, context, false);
  }

                                
  if (query->returningList)
  {
    appendContextKeyword(context, " RETURNING", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_target_list(query->returningList, context, NULL, colNamesVisible);
  }
}

              
                                                                       
              
   
static void
get_update_query_targetlist_def(Query *query, List *targetList, deparse_context *context, RangeTblEntry *rte)
{
  StringInfo buf = context->buf;
  ListCell *l;
  ListCell *next_ma_cell;
  int remaining_ma_columns;
  const char *sep;
  SubLink *cur_ma_sublink;
  List *ma_sublinks;

     
                                                                             
                                                                           
              
     
  ma_sublinks = NIL;
  if (query->hasSubLinks)                              
  {
    foreach (l, targetList)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(l);

      if (tle->resjunk && IsA(tle->expr, SubLink))
      {
        SubLink *sl = (SubLink *)tle->expr;

        if (sl->subLinkType == MULTIEXPR_SUBLINK)
        {
          ma_sublinks = lappend(ma_sublinks, sl);
          Assert(sl->subLinkId == list_length(ma_sublinks));
        }
      }
    }
  }
  next_ma_cell = list_head(ma_sublinks);
  cur_ma_sublink = NULL;
  remaining_ma_columns = 0;

                                                         
  sep = "";
  foreach (l, targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);
    Node *expr;

    if (tle->resjunk)
    {
      continue;                          
    }

                                                                     
    appendStringInfoString(buf, sep);
    sep = ", ";

       
                                                                      
                            
       
    if (next_ma_cell != NULL && cur_ma_sublink == NULL)
    {
         
                                                                         
                                                            
                                                                         
                                                                        
                                                                    
                                                                      
                                                   
         
      expr = (Node *)tle->expr;
      while (expr)
      {
        if (IsA(expr, FieldStore))
        {
          FieldStore *fstore = (FieldStore *)expr;

          expr = (Node *)linitial(fstore->newvals);
        }
        else if (IsA(expr, SubscriptingRef))
        {
          SubscriptingRef *sbsref = (SubscriptingRef *)expr;

          if (sbsref->refassgnexpr == NULL)
          {
            break;
          }

          expr = (Node *)sbsref->refassgnexpr;
        }
        else if (IsA(expr, CoerceToDomain))
        {
          CoerceToDomain *cdomain = (CoerceToDomain *)expr;

          if (cdomain->coercionformat != COERCE_IMPLICIT_CAST)
          {
            break;
          }
          expr = (Node *)cdomain->arg;
        }
        else
        {
          break;
        }
      }
      expr = strip_implicit_coercions(expr);

      if (expr && IsA(expr, Param) && ((Param *)expr)->paramkind == PARAM_MULTIEXPR)
      {
        cur_ma_sublink = (SubLink *)lfirst(next_ma_cell);
        next_ma_cell = lnext(next_ma_cell);
        remaining_ma_columns = count_nonjunk_tlist_entries(((Query *)cur_ma_sublink->subselect)->targetList);
        Assert(((Param *)expr)->paramid == ((cur_ma_sublink->subLinkId << 16) | 1));
        appendStringInfoChar(buf, '(');
      }
    }

       
                                                                   
                                                              
       
    appendStringInfoString(buf, quote_identifier(get_attname(rte->relid, tle->resno, false)));

       
                                                                         
                                                                         
       
    expr = processIndirection((Node *)tle->expr, context);

       
                                                                          
                                                                           
                               
       
    if (cur_ma_sublink != NULL)
    {
      if (--remaining_ma_columns > 0)
      {
        continue;                                             
      }
      appendStringInfoChar(buf, ')');
      expr = (Node *)cur_ma_sublink;
      cur_ma_sublink = NULL;
    }

    appendStringInfoString(buf, " = ");

    get_rule_expr(expr, context, false);
  }
}

              
                                                          
              
   
static void
get_delete_query_def(Query *query, deparse_context *context, bool colNamesVisible)
{
  StringInfo buf = context->buf;
  RangeTblEntry *rte;

                                       
  get_with_clause(query, context);

     
                                              
     
  rte = rt_fetch(query->resultRelation, query->rtable);
  Assert(rte->rtekind == RTE_RELATION);
  if (PRETTY_INDENT(context))
  {
    appendStringInfoChar(buf, ' ');
    context->indentLevel += PRETTYINDENT_STD;
  }
  appendStringInfo(buf, "DELETE FROM %s%s", only_marker(rte), generate_relation_name(rte->relid, NIL));
  if (rte->alias != NULL)
  {
    appendStringInfo(buf, " %s", quote_identifier(rte->alias->aliasname));
  }

                                     
  get_from_clause(query, " USING ", context);

                                   
  if (query->jointree->quals != NULL)
  {
    appendContextKeyword(context, " WHERE ", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_rule_expr(query->jointree->quals, context, false);
  }

                                
  if (query->returningList)
  {
    appendContextKeyword(context, " RETURNING", -PRETTYINDENT_STD, PRETTYINDENT_STD, 1);
    get_target_list(query->returningList, context, NULL, colNamesVisible);
  }
}

              
                                                            
              
   
static void
get_utility_query_def(Query *query, deparse_context *context)
{
  StringInfo buf = context->buf;

  if (query->utilityStmt && IsA(query->utilityStmt, NotifyStmt))
  {
    NotifyStmt *stmt = (NotifyStmt *)query->utilityStmt;

    appendContextKeyword(context, "", 0, PRETTYINDENT_STD, 1);
    appendStringInfo(buf, "NOTIFY %s", quote_identifier(stmt->conditionname));
    if (stmt->payload)
    {
      appendStringInfoString(buf, ", ");
      simple_quote_literal(buf, stmt->payload);
    }
  }
  else
  {
                                                                    
    elog(ERROR, "unexpected utility statement type");
  }
}

   
                                
   
                                                                      
                                                                         
                                                         
   
                                                                    
                                                                        
                                                                            
                                                                            
                                                                               
                                                                             
                                                                             
                      
   
                                                                              
                                                         
   
static char *
get_variable(Var *var, int levelsup, bool istoplevel, deparse_context *context)
{
  StringInfo buf = context->buf;
  RangeTblEntry *rte;
  AttrNumber attnum;
  int netlevelsup;
  deparse_namespace *dpns;
  deparse_columns *colinfo;
  char *refname;
  char *attname;

                                      
  netlevelsup = var->varlevelsup + levelsup;
  if (netlevelsup >= list_length(context->namespaces))
  {
    elog(ERROR, "bogus varlevelsup: %d offset %d", var->varlevelsup, levelsup);
  }
  dpns = (deparse_namespace *)list_nth(context->namespaces, netlevelsup);

     
                                                                        
                                                                            
                                                                             
                                                        
     
  if (var->varno >= 1 && var->varno <= list_length(dpns->rtable))
  {
    rte = rt_fetch(var->varno, dpns->rtable);
    refname = (char *)list_nth(dpns->rtable_names, var->varno - 1);
    colinfo = deparse_columns_fetch(var->varno, dpns);
    attnum = var->varattno;
  }
  else
  {
    resolve_special_varno((Node *)var, context, NULL, get_special_variable);
    return NULL;
  }

     
                                                                            
                                                                           
                                                                         
                                                                     
                                                                             
                                                                         
                                                                          
                                                                           
                                                                            
     
  if ((rte->rtekind == RTE_SUBQUERY || rte->rtekind == RTE_CTE) && attnum > list_length(rte->eref->colnames) && dpns->inner_planstate)
  {
    TargetEntry *tle;
    deparse_namespace save_dpns;

    tle = get_tle_by_resno(dpns->inner_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "invalid attnum %d for relation \"%s\"", var->varattno, rte->eref->aliasname);
    }

    Assert(netlevelsup == 0);
    push_child_plan(dpns, dpns->inner_planstate, &save_dpns);

       
                                                                        
                          
       
    if (!IsA(tle->expr, Var))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr((Node *)tle->expr, context, true);
    if (!IsA(tle->expr, Var))
    {
      appendStringInfoChar(buf, ')');
    }

    pop_child_plan(dpns, &save_dpns);
    return NULL;
  }

     
                                                                           
                                                                           
                                                                            
                                                                             
                                                                           
                                                                           
     
                                                                          
                                                                      
                                    
     
  if (rte->rtekind == RTE_JOIN && rte->alias == NULL)
  {
    if (rte->joinaliasvars == NIL)
    {
      elog(ERROR, "cannot decompile join alias var in plan tree");
    }
    if (attnum > 0)
    {
      Var *aliasvar;

      aliasvar = (Var *)list_nth(rte->joinaliasvars, attnum - 1);
                                                                
      if (aliasvar && IsA(aliasvar, Var))
      {
        return get_variable(aliasvar, var->varlevelsup + levelsup, istoplevel, context);
      }
    }

       
                                                                         
                                                                          
                                                            
       
    Assert(refname == NULL);
  }

  if (attnum == InvalidAttrNumber)
  {
    attname = NULL;
  }
  else if (attnum > 0)
  {
                                                        
    if (attnum > colinfo->num_cols)
    {
      elog(ERROR, "invalid attnum %d for relation \"%s\"", attnum, rte->eref->aliasname);
    }
    attname = colinfo->colnames[attnum - 1];

       
                                                                         
                                                                        
                                                                    
                                                                           
                                                                    
       
    if (attname == NULL)
    {
      attname = "?dropped?column?";
    }
  }
  else
  {
                                                                
    attname = get_rte_attribute_name(rte, attnum);
  }

  if (refname && (context->varprefix || attname == NULL))
  {
    appendStringInfoString(buf, quote_identifier(refname));
    appendStringInfoChar(buf, '.');
  }
  if (attname)
  {
    appendStringInfoString(buf, quote_identifier(attname));
  }
  else
  {
    appendStringInfoChar(buf, '*');
    if (istoplevel)
    {
      appendStringInfo(buf, "::%s", format_type_with_typemod(var->vartype, var->vartypmod));
    }
  }

  return attname;
}

   
                                                                            
                                                                               
                                                                     
                                                                       
                  
   
static void
get_special_variable(Node *node, deparse_context *context, void *private)
{
  StringInfo buf = context->buf;

     
                                                                             
                 
     
  if (!IsA(node, Var))
  {
    appendStringInfoChar(buf, '(');
  }
  get_rule_expr(node, context, true);
  if (!IsA(node, Var))
  {
    appendStringInfoChar(buf, ')');
  }
}

   
                                                                          
                                                                           
                                 
   
static void
resolve_special_varno(Node *node, deparse_context *context, void *private, void (*callback)(Node *, deparse_context *, void *))
{
  Var *var;
  deparse_namespace *dpns;

                                               
  if (!IsA(node, Var))
  {
    callback(node, context, private);
    return;
  }

                                      
  var = (Var *)node;
  dpns = (deparse_namespace *)list_nth(context->namespaces, var->varlevelsup);

     
                                     
     
  if (var->varno == OUTER_VAR && dpns->outer_tlist)
  {
    TargetEntry *tle;
    deparse_namespace save_dpns;

    tle = get_tle_by_resno(dpns->outer_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "bogus varattno for OUTER_VAR var: %d", var->varattno);
    }

    push_child_plan(dpns, dpns->outer_planstate, &save_dpns);
    resolve_special_varno((Node *)tle->expr, context, private, callback);
    pop_child_plan(dpns, &save_dpns);
    return;
  }
  else if (var->varno == INNER_VAR && dpns->inner_tlist)
  {
    TargetEntry *tle;
    deparse_namespace save_dpns;

    tle = get_tle_by_resno(dpns->inner_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "bogus varattno for INNER_VAR var: %d", var->varattno);
    }

    push_child_plan(dpns, dpns->inner_planstate, &save_dpns);
    resolve_special_varno((Node *)tle->expr, context, private, callback);
    pop_child_plan(dpns, &save_dpns);
    return;
  }
  else if (var->varno == INDEX_VAR && dpns->index_tlist)
  {
    TargetEntry *tle;

    tle = get_tle_by_resno(dpns->index_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "bogus varattno for INDEX_VAR var: %d", var->varattno);
    }

    resolve_special_varno((Node *)tle->expr, context, private, callback);
    return;
  }
  else if (var->varno < 1 || var->varno > list_length(dpns->rtable))
  {
    elog(ERROR, "bogus varno: %d", var->varno);
  }

                                               
  callback(node, context, private);
}

   
                                                                    
                                                               
   
                                                                             
   
                                                                            
                                                                           
                                                                             
                                                                               
                                                                         
                                                                             
                             
   
                                                                        
                                  
   
static const char *
get_name_for_var_field(Var *var, int fieldno, int levelsup, deparse_context *context)
{
  RangeTblEntry *rte;
  AttrNumber attnum;
  int netlevelsup;
  deparse_namespace *dpns;
  TupleDesc tupleDesc;
  Node *expr;

     
                                                                       
                                  
     
  if (IsA(var, RowExpr))
  {
    RowExpr *r = (RowExpr *)var;

    if (fieldno > 0 && fieldno <= list_length(r->colnames))
    {
      return strVal(list_nth(r->colnames, fieldno - 1));
    }
  }

     
                                                                           
     
  if (IsA(var, Param))
  {
    Param *param = (Param *)var;
    ListCell *ancestor_cell;

    expr = find_param_referent(param, context, &dpns, &ancestor_cell);
    if (expr)
    {
                                                                
      deparse_namespace save_dpns;
      const char *result;

      push_ancestor_plan(dpns, ancestor_cell, &save_dpns);
      result = get_name_for_var_field((Var *)expr, fieldno, 0, context);
      pop_ancestor_plan(dpns, &save_dpns);
      return result;
    }
  }

     
                                                                           
                                                   
     
  if (!IsA(var, Var) || var->vartype != RECORDOID)
  {
    tupleDesc = get_expr_result_tupdesc((Node *)var, false);
                                                           
    Assert(fieldno >= 1 && fieldno <= tupleDesc->natts);
    return NameStr(TupleDescAttr(tupleDesc, fieldno - 1)->attname);
  }

                                      
  netlevelsup = var->varlevelsup + levelsup;
  if (netlevelsup >= list_length(context->namespaces))
  {
    elog(ERROR, "bogus varlevelsup: %d offset %d", var->varlevelsup, levelsup);
  }
  dpns = (deparse_namespace *)list_nth(context->namespaces, netlevelsup);

     
                                                                        
                                                                            
                                                                        
     
  if (var->varno >= 1 && var->varno <= list_length(dpns->rtable))
  {
    rte = rt_fetch(var->varno, dpns->rtable);
    attnum = var->varattno;
  }
  else if (var->varno == OUTER_VAR && dpns->outer_tlist)
  {
    TargetEntry *tle;
    deparse_namespace save_dpns;
    const char *result;

    tle = get_tle_by_resno(dpns->outer_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "bogus varattno for OUTER_VAR var: %d", var->varattno);
    }

    Assert(netlevelsup == 0);
    push_child_plan(dpns, dpns->outer_planstate, &save_dpns);

    result = get_name_for_var_field((Var *)tle->expr, fieldno, levelsup, context);

    pop_child_plan(dpns, &save_dpns);
    return result;
  }
  else if (var->varno == INNER_VAR && dpns->inner_tlist)
  {
    TargetEntry *tle;
    deparse_namespace save_dpns;
    const char *result;

    tle = get_tle_by_resno(dpns->inner_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "bogus varattno for INNER_VAR var: %d", var->varattno);
    }

    Assert(netlevelsup == 0);
    push_child_plan(dpns, dpns->inner_planstate, &save_dpns);

    result = get_name_for_var_field((Var *)tle->expr, fieldno, levelsup, context);

    pop_child_plan(dpns, &save_dpns);
    return result;
  }
  else if (var->varno == INDEX_VAR && dpns->index_tlist)
  {
    TargetEntry *tle;
    const char *result;

    tle = get_tle_by_resno(dpns->index_tlist, var->varattno);
    if (!tle)
    {
      elog(ERROR, "bogus varattno for INDEX_VAR var: %d", var->varattno);
    }

    Assert(netlevelsup == 0);

    result = get_name_for_var_field((Var *)tle->expr, fieldno, levelsup, context);

    return result;
  }
  else
  {
    elog(ERROR, "bogus varno: %d", var->varno);
    return NULL;                          
  }

  if (attnum == InvalidAttrNumber)
  {
                                                                      
    return get_rte_attribute_name(rte, fieldno);
  }

     
                                                              
                                                                          
                                                                          
                                                                            
                                             
     
  expr = (Node *)var;                                     

  switch (rte->rtekind)
  {
  case RTE_RELATION:
  case RTE_VALUES:
  case RTE_NAMEDTUPLESTORE:
  case RTE_RESULT:

       
                                                                     
                                                                       
                              
       
    break;
  case RTE_SUBQUERY:
                                                             
    {
      if (rte->subquery)
      {
        TargetEntry *ste = get_tle_by_resno(rte->subquery->targetList, attnum);

        if (ste == NULL || ste->resjunk)
        {
          elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
        }
        expr = (Node *)ste->expr;
        if (IsA(expr, Var))
        {
             
                                                             
                                                                
                                                               
                        
             
          deparse_namespace mydpns;
          const char *result;

          set_deparse_for_query(&mydpns, rte->subquery, context->namespaces);

          context->namespaces = lcons(&mydpns, context->namespaces);

          result = get_name_for_var_field((Var *)expr, fieldno, 0, context);

          context->namespaces = list_delete_first(context->namespaces);

          return result;
        }
                                                         
      }
      else
      {
           
                                                                 
                                                                   
                                                                
                                                                   
                                                     
           
        TargetEntry *tle;
        deparse_namespace save_dpns;
        const char *result;

        if (!dpns->inner_planstate)
        {
          elog(ERROR, "failed to find plan for subquery %s", rte->eref->aliasname);
        }
        tle = get_tle_by_resno(dpns->inner_tlist, attnum);
        if (!tle)
        {
          elog(ERROR, "bogus varattno for subquery var: %d", attnum);
        }
        Assert(netlevelsup == 0);
        push_child_plan(dpns, dpns->inner_planstate, &save_dpns);

        result = get_name_for_var_field((Var *)tle->expr, fieldno, levelsup, context);

        pop_child_plan(dpns, &save_dpns);
        return result;
      }
    }
    break;
  case RTE_JOIN:
                                                             
    if (rte->joinaliasvars == NIL)
    {
      elog(ERROR, "cannot decompile join alias var in plan tree");
    }
    Assert(attnum > 0 && attnum <= list_length(rte->joinaliasvars));
    expr = (Node *)list_nth(rte->joinaliasvars, attnum - 1);
    Assert(expr != NULL);
                                                              
    if (IsA(expr, Var))
    {
      return get_name_for_var_field((Var *)expr, fieldno, var->varlevelsup + levelsup, context);
    }
                                                     
    break;
  case RTE_FUNCTION:
  case RTE_TABLEFUNC:

       
                                                                      
                                                           
       
    break;
  case RTE_CTE:
                                                       
    {
      CommonTableExpr *cte = NULL;
      Index ctelevelsup;
      ListCell *lc;

         
                                                                   
         
      ctelevelsup = rte->ctelevelsup + netlevelsup;
      if (ctelevelsup >= list_length(context->namespaces))
      {
        lc = NULL;
      }
      else
      {
        deparse_namespace *ctedpns;

        ctedpns = (deparse_namespace *)list_nth(context->namespaces, ctelevelsup);
        foreach (lc, ctedpns->ctes)
        {
          cte = (CommonTableExpr *)lfirst(lc);
          if (strcmp(cte->ctename, rte->ctename) == 0)
          {
            break;
          }
        }
      }
      if (lc != NULL)
      {
        Query *ctequery = (Query *)cte->ctequery;
        TargetEntry *ste = get_tle_by_resno(GetCTETargetList(cte), attnum);

        if (ste == NULL || ste->resjunk)
        {
          elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
        }
        expr = (Node *)ste->expr;
        if (IsA(expr, Var))
        {
             
                                                                 
                                                               
                                                          
                                                             
                                                      
             
          List *save_nslist = context->namespaces;
          List *new_nslist;
          deparse_namespace mydpns;
          const char *result;

          set_deparse_for_query(&mydpns, ctequery, context->namespaces);

          new_nslist = list_copy_tail(context->namespaces, ctelevelsup);
          context->namespaces = lcons(&mydpns, new_nslist);

          result = get_name_for_var_field((Var *)expr, fieldno, 0, context);

          context->namespaces = save_nslist;

          return result;
        }
                                                         
      }
      else
      {
           
                                                              
                                                             
                                                                   
                                                      
           
        TargetEntry *tle;
        deparse_namespace save_dpns;
        const char *result;

        if (!dpns->inner_planstate)
        {
          elog(ERROR, "failed to find plan for CTE %s", rte->eref->aliasname);
        }
        tle = get_tle_by_resno(dpns->inner_tlist, attnum);
        if (!tle)
        {
          elog(ERROR, "bogus varattno for subquery var: %d", attnum);
        }
        Assert(netlevelsup == 0);
        push_child_plan(dpns, dpns->inner_planstate, &save_dpns);

        result = get_name_for_var_field((Var *)tle->expr, fieldno, levelsup, context);

        pop_child_plan(dpns, &save_dpns);
        return result;
      }
    }
    break;
  }

     
                                                                   
                                                        
     
  tupleDesc = get_expr_result_tupdesc(expr, false);
                                                         
  Assert(fieldno >= 1 && fieldno <= tupleDesc->natts);
  return NameStr(TupleDescAttr(tupleDesc, fieldno - 1)->attname);
}

   
                                                                           
                                                                             
   
                                                                             
                                                                          
                       
   
static Node *
find_param_referent(Param *param, deparse_context *context, deparse_namespace **dpns_p, ListCell **ancestor_cell_p)
{
                                                                 
  *dpns_p = NULL;
  *ancestor_cell_p = NULL;

     
                                                                          
                                                                         
                                     
     
  if (param->paramkind == PARAM_EXEC)
  {
    deparse_namespace *dpns;
    PlanState *child_ps;
    bool in_same_plan_level;
    ListCell *lc;

    dpns = (deparse_namespace *)linitial(context->namespaces);
    child_ps = dpns->planstate;
    in_same_plan_level = true;

    foreach (lc, dpns->ancestors)
    {
      PlanState *ps = (PlanState *)lfirst(lc);
      ListCell *lc2;

         
                                                                         
                                                                      
                          
         
      if (IsA(ps, NestLoopState) && child_ps == innerPlanState(ps) && in_same_plan_level)
      {
        NestLoop *nl = (NestLoop *)ps->plan;

        foreach (lc2, nl->nestParams)
        {
          NestLoopParam *nlp = (NestLoopParam *)lfirst(lc2);

          if (nlp->paramno == param->paramid)
          {
                                             
            *dpns_p = dpns;
            *ancestor_cell_p = lc;
            return (Node *)nlp->paramval;
          }
        }
      }

         
                                                           
         
      foreach (lc2, ps->subPlan)
      {
        SubPlanState *sstate = (SubPlanState *)lfirst(lc2);
        SubPlan *subplan = sstate->subplan;
        ListCell *lc3;
        ListCell *lc4;

        if (child_ps != sstate->planstate)
        {
          continue;
        }

                                                     
        forboth(lc3, subplan->parParam, lc4, subplan->args)
        {
          int paramid = lfirst_int(lc3);
          Node *arg = (Node *)lfirst(lc4);

          if (paramid == param->paramid)
          {
                                             
            *dpns_p = dpns;
            *ancestor_cell_p = lc;
            return arg;
          }
        }

                                                               
        in_same_plan_level = false;
        break;
      }

         
                                                                   
                                                                       
                                                      
                             
         
      foreach (lc2, ps->initPlan)
      {
        SubPlanState *sstate = (SubPlanState *)lfirst(lc2);

        if (child_ps != sstate->planstate)
        {
          continue;
        }

                                           
        Assert(sstate->subplan->parParam == NIL);

                                                                 
        in_same_plan_level = false;
        break;
      }

                                              
      child_ps = ps;
    }
  }

                         
  return NULL;
}

   
                                  
   
static void
get_parameter(Param *param, deparse_context *context)
{
  Node *expr;
  deparse_namespace *dpns;
  ListCell *ancestor_cell;

     
                                                                             
                                                                             
                                                                             
            
     
  expr = find_param_referent(param, context, &dpns, &ancestor_cell);
  if (expr)
  {
                                    
    deparse_namespace save_dpns;
    bool save_varprefix;
    bool need_paren;

                                                    
    push_ancestor_plan(dpns, ancestor_cell, &save_dpns);

       
                                                                        
                                                
       
    save_varprefix = context->varprefix;
    context->varprefix = true;

       
                                                                        
                                                                 
                                                                       
       
    need_paren = !(IsA(expr, Var) || IsA(expr, Aggref) || IsA(expr, GroupingFunc) || IsA(expr, Param));
    if (need_paren)
    {
      appendStringInfoChar(context->buf, '(');
    }

    get_rule_expr(expr, context, false);

    if (need_paren)
    {
      appendStringInfoChar(context->buf, ')');
    }

    context->varprefix = save_varprefix;

    pop_ancestor_plan(dpns, &save_dpns);

    return;
  }

     
                                                               
     
  appendStringInfo(context->buf, "$%d", param->paramid);
}

   
                             
   
                                    
                                                                     
   
static const char *
get_simple_binary_op_name(OpExpr *expr)
{
  List *args = expr->args;

  if (list_length(args) == 2)
  {
                         
    Node *arg1 = (Node *)linitial(args);
    Node *arg2 = (Node *)lsecond(args);
    const char *op;

    op = generate_operator_name(expr->opno, exprType(arg1), exprType(arg2));
    if (strlen(op) == 1)
    {
      return op;
    }
  }
  return NULL;
}

   
                                                                              
   
                                                        
                       
   
static bool
isSimpleNode(Node *node, Node *parentNode, int prettyFlags)
{
  if (!node)
  {
    return false;
  }

  switch (nodeTag(node))
  {
  case T_Var:
  case T_Const:
  case T_Param:
  case T_CoerceToDomainValue:
  case T_SetToDefault:
  case T_CurrentOfExpr:
                                     
    return true;

  case T_SubscriptingRef:
  case T_ArrayExpr:
  case T_RowExpr:
  case T_CoalesceExpr:
  case T_MinMaxExpr:
  case T_SQLValueFunction:
  case T_XmlExpr:
  case T_NextValueExpr:
  case T_NullIfExpr:
  case T_Aggref:
  case T_GroupingFunc:
  case T_WindowFunc:
  case T_FuncExpr:
                                             
    return true;

                                          
  case T_CaseExpr:
    return true;

  case T_FieldSelect:

       
                                                                   
                             
       
    return (IsA(parentNode, FieldSelect) ? false : true);

  case T_FieldStore:

       
                                                        
       
    return (IsA(parentNode, FieldStore) ? false : true);

  case T_CoerceToDomain:
                                  
    return isSimpleNode((Node *)((CoerceToDomain *)node)->arg, node, prettyFlags);
  case T_RelabelType:
    return isSimpleNode((Node *)((RelabelType *)node)->arg, node, prettyFlags);
  case T_CoerceViaIO:
    return isSimpleNode((Node *)((CoerceViaIO *)node)->arg, node, prettyFlags);
  case T_ArrayCoerceExpr:
    return isSimpleNode((Node *)((ArrayCoerceExpr *)node)->arg, node, prettyFlags);
  case T_ConvertRowtypeExpr:
    return isSimpleNode((Node *)((ConvertRowtypeExpr *)node)->arg, node, prettyFlags);

  case T_OpExpr:
  {
                                                             
    if (prettyFlags & PRETTYFLAG_PAREN && IsA(parentNode, OpExpr))
    {
      const char *op;
      const char *parentOp;
      bool is_lopriop;
      bool is_hipriop;
      bool is_lopriparent;
      bool is_hipriparent;

      op = get_simple_binary_op_name((OpExpr *)node);
      if (!op)
      {
        return false;
      }

                                                          
      is_lopriop = (strchr("+-", *op) != NULL);
      is_hipriop = (strchr("*/%", *op) != NULL);
      if (!(is_lopriop || is_hipriop))
      {
        return false;
      }

      parentOp = get_simple_binary_op_name((OpExpr *)parentNode);
      if (!parentOp)
      {
        return false;
      }

      is_lopriparent = (strchr("+-", *parentOp) != NULL);
      is_hipriparent = (strchr("*/%", *parentOp) != NULL);
      if (!(is_lopriparent || is_hipriparent))
      {
        return false;
      }

      if (is_hipriop && is_lopriparent)
      {
        return true;                                   
      }

      if (is_lopriop && is_hipriparent)
      {
        return false;
      }

         
                                                                 
                                               
         
      if (node == (Node *)linitial(((OpExpr *)parentNode)->args))
      {
        return true;
      }

      return false;
    }
                                                        
  }
                     

  case T_SubLink:
  case T_NullTest:
  case T_BooleanTest:
  case T_DistinctExpr:
    switch (nodeTag(parentNode))
    {
    case T_FuncExpr:
    {
                                      
      CoercionForm type = ((FuncExpr *)parentNode)->funcformat;

      if (type == COERCE_EXPLICIT_CAST || type == COERCE_IMPLICIT_CAST)
      {
        return false;
      }
      return true;                      
    }
    case T_BoolExpr:                              
    case T_SubscriptingRef:                       
    case T_ArrayExpr:                             
    case T_RowExpr:                               
    case T_CoalesceExpr:                         
    case T_MinMaxExpr:                           
    case T_XmlExpr:                              
    case T_NullIfExpr:                            
    case T_Aggref:                               
    case T_GroupingFunc:                         
    case T_WindowFunc:                           
    case T_CaseExpr:                              
      return true;
    default:
      return false;
    }

  case T_BoolExpr:
    switch (nodeTag(parentNode))
    {
    case T_BoolExpr:
      if (prettyFlags & PRETTYFLAG_PAREN)
      {
        BoolExprType type;
        BoolExprType parentType;

        type = ((BoolExpr *)node)->boolop;
        parentType = ((BoolExpr *)parentNode)->boolop;
        switch (type)
        {
        case NOT_EXPR:
        case AND_EXPR:
          if (parentType == AND_EXPR || parentType == OR_EXPR)
          {
            return true;
          }
          break;
        case OR_EXPR:
          if (parentType == OR_EXPR)
          {
            return true;
          }
          break;
        }
      }
      return false;
    case T_FuncExpr:
    {
                                      
      CoercionForm type = ((FuncExpr *)parentNode)->funcformat;

      if (type == COERCE_EXPLICIT_CAST || type == COERCE_IMPLICIT_CAST)
      {
        return false;
      }
      return true;                      
    }
    case T_SubscriptingRef:                       
    case T_ArrayExpr:                             
    case T_RowExpr:                               
    case T_CoalesceExpr:                         
    case T_MinMaxExpr:                           
    case T_XmlExpr:                              
    case T_NullIfExpr:                            
    case T_Aggref:                               
    case T_GroupingFunc:                         
    case T_WindowFunc:                           
    case T_CaseExpr:                              
      return true;
    default:
      return false;
    }

  default:
    break;
  }
                                              
  return false;
}

   
                                                     
   
                                                                            
                                       
   
static void
appendContextKeyword(deparse_context *context, const char *str, int indentBefore, int indentAfter, int indentPlus)
{
  StringInfo buf = context->buf;

  if (PRETTY_INDENT(context))
  {
    int indentAmount;

    context->indentLevel += indentBefore;

                                                                
    removeStringInfoSpaces(buf);
                                                
    appendStringInfoChar(buf, '\n');

    if (context->indentLevel < PRETTYINDENT_LIMIT)
    {
      indentAmount = Max(context->indentLevel, 0) + indentPlus;
    }
    else
    {
         
                                                                        
                                                                
                                                                     
                                                                     
                                                                       
                                                                   
                                                                
         
      indentAmount = PRETTYINDENT_LIMIT + (context->indentLevel - PRETTYINDENT_LIMIT) / (PRETTYINDENT_STD / 2);
      indentAmount %= PRETTYINDENT_LIMIT;
                                                                    
      indentAmount += indentPlus;
    }
    appendStringInfoSpaces(buf, indentAmount);

    appendStringInfoString(buf, str);

    context->indentLevel += indentAfter;
    if (context->indentLevel < 0)
    {
      context->indentLevel = 0;
    }
  }
  else
  {
    appendStringInfoString(buf, str);
  }
}

   
                                                                  
   
                                                            
   
static void
removeStringInfoSpaces(StringInfo str)
{
  while (str->len > 0 && str->data[str->len - 1] == ' ')
  {
    str->data[--(str->len)] = '\0';
  }
}

   
                                                           
                                                                       
   
                                                                          
   
                                                                          
                                                                              
                                                                          
          
   
static void
get_rule_expr_paren(Node *node, deparse_context *context, bool showimplicit, Node *parentNode)
{
  bool need_paren;

  need_paren = PRETTY_PAREN(context) && !isSimpleNode(node, parentNode, context->prettyFlags);

  if (need_paren)
  {
    appendStringInfoChar(context->buf, '(');
  }

  get_rule_expr(node, context, showimplicit);

  if (need_paren)
  {
    appendStringInfoChar(context->buf, ')');
  }
}

              
                                              
   
                                                                           
                                                                           
                                                                        
                                                                            
                                                                         
                                                                              
                                                                            
                                                                
              
   
static void
get_rule_expr(Node *node, deparse_context *context, bool showimplicit)
{
  StringInfo buf = context->buf;

  if (node == NULL)
  {
    return;
  }

                                                               
  CHECK_FOR_INTERRUPTS();
  check_stack_depth();

     
                                                               
                                                                             
                                                                            
                                                                     
                                                      
     
  switch (nodeTag(node))
  {
  case T_Var:
    (void)get_variable((Var *)node, 0, false, context);
    break;

  case T_Const:
    get_const_expr((Const *)node, context, 0);
    break;

  case T_Param:
    get_parameter((Param *)node, context);
    break;

  case T_Aggref:
    get_agg_expr((Aggref *)node, context, (Aggref *)node);
    break;

  case T_GroupingFunc:
  {
    GroupingFunc *gexpr = (GroupingFunc *)node;

    appendStringInfoString(buf, "GROUPING(");
    get_rule_expr((Node *)gexpr->args, context, true);
    appendStringInfoChar(buf, ')');
  }
  break;

  case T_WindowFunc:
    get_windowfunc_expr((WindowFunc *)node, context);
    break;

  case T_SubscriptingRef:
  {
    SubscriptingRef *sbsref = (SubscriptingRef *)node;
    bool need_parens;

       
                                                              
                                                                  
                                                              
                                                                 
                                                        
                   
       
    if (IsA(sbsref->refexpr, CaseTestExpr))
    {
      Assert(sbsref->refassgnexpr);
      get_rule_expr((Node *)sbsref->refassgnexpr, context, showimplicit);
      break;
    }

       
                                                               
                                                     
                                                        
                   
       
    need_parens = !IsA(sbsref->refexpr, Var) && !IsA(sbsref->refexpr, FieldSelect);
    if (need_parens)
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr((Node *)sbsref->refexpr, context, showimplicit);
    if (need_parens)
    {
      appendStringInfoChar(buf, ')');
    }

       
                                                                   
                                                                
                                                           
                                                                  
                                                                 
                                                                 
                              
       
    if (sbsref->refassgnexpr)
    {
      Node *refassgnexpr;

         
                                                                
                                                       
                                                                
                                                     
         
      refassgnexpr = processIndirection(node, context);
      appendStringInfoString(buf, " := ");
      get_rule_expr(refassgnexpr, context, showimplicit);
    }
    else
    {
                                                                 
      printSubscripts(sbsref, context);
    }
  }
  break;

  case T_FuncExpr:
    get_func_expr((FuncExpr *)node, context, showimplicit);
    break;

  case T_NamedArgExpr:
  {
    NamedArgExpr *na = (NamedArgExpr *)node;

    appendStringInfo(buf, "%s => ", quote_identifier(na->name));
    get_rule_expr((Node *)na->arg, context, showimplicit);
  }
  break;

  case T_OpExpr:
    get_oper_expr((OpExpr *)node, context);
    break;

  case T_DistinctExpr:
  {
    DistinctExpr *expr = (DistinctExpr *)node;
    List *args = expr->args;
    Node *arg1 = (Node *)linitial(args);
    Node *arg2 = (Node *)lsecond(args);

    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr_paren(arg1, context, true, node);
    appendStringInfoString(buf, " IS DISTINCT FROM ");
    get_rule_expr_paren(arg2, context, true, node);
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_NullIfExpr:
  {
    NullIfExpr *nullifexpr = (NullIfExpr *)node;

    appendStringInfoString(buf, "NULLIF(");
    get_rule_expr((Node *)nullifexpr->args, context, true);
    appendStringInfoChar(buf, ')');
  }
  break;

  case T_ScalarArrayOpExpr:
  {
    ScalarArrayOpExpr *expr = (ScalarArrayOpExpr *)node;
    List *args = expr->args;
    Node *arg1 = (Node *)linitial(args);
    Node *arg2 = (Node *)lsecond(args);

    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr_paren(arg1, context, true, node);
    appendStringInfo(buf, " %s %s (", generate_operator_name(expr->opno, exprType(arg1), get_base_element_type(exprType(arg2))), expr->useOr ? "ANY" : "ALL");
    get_rule_expr_paren(arg2, context, true, node);

       
                                                                  
                                                                 
                                                                  
                                                          
                                                               
                                                                   
                                                                   
                                                                 
                                                                   
                                    
       
    if (IsA(arg2, SubLink) && ((SubLink *)arg2)->subLinkType == EXPR_SUBLINK)
    {
      appendStringInfo(buf, "::%s", format_type_with_typemod(exprType(arg2), exprTypmod(arg2)));
    }
    appendStringInfoChar(buf, ')');
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_BoolExpr:
  {
    BoolExpr *expr = (BoolExpr *)node;
    Node *first_arg = linitial(expr->args);
    ListCell *arg = lnext(list_head(expr->args));

    switch (expr->boolop)
    {
    case AND_EXPR:
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, '(');
      }
      get_rule_expr_paren(first_arg, context, false, node);
      while (arg)
      {
        appendStringInfoString(buf, " AND ");
        get_rule_expr_paren((Node *)lfirst(arg), context, false, node);
        arg = lnext(arg);
      }
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, ')');
      }
      break;

    case OR_EXPR:
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, '(');
      }
      get_rule_expr_paren(first_arg, context, false, node);
      while (arg)
      {
        appendStringInfoString(buf, " OR ");
        get_rule_expr_paren((Node *)lfirst(arg), context, false, node);
        arg = lnext(arg);
      }
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, ')');
      }
      break;

    case NOT_EXPR:
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, '(');
      }
      appendStringInfoString(buf, "NOT ");
      get_rule_expr_paren(first_arg, context, false, node);
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, ')');
      }
      break;

    default:
      elog(ERROR, "unrecognized boolop: %d", (int)expr->boolop);
    }
  }
  break;

  case T_SubLink:
    get_sublink_expr((SubLink *)node, context);
    break;

  case T_SubPlan:
  {
    SubPlan *subplan = (SubPlan *)node;

       
                                                                   
                                                            
                                                                
                                                   
       
    if (subplan->useHashTable)
    {
      appendStringInfo(buf, "(hashed %s)", subplan->plan_name);
    }
    else
    {
      appendStringInfo(buf, "(%s)", subplan->plan_name);
    }
  }
  break;

  case T_AlternativeSubPlan:
  {
    AlternativeSubPlan *asplan = (AlternativeSubPlan *)node;
    ListCell *lc;

                                                       
    appendStringInfoString(buf, "(alternatives: ");
    foreach (lc, asplan->subplans)
    {
      SubPlan *splan = lfirst_node(SubPlan, lc);

      if (splan->useHashTable)
      {
        appendStringInfo(buf, "hashed %s", splan->plan_name);
      }
      else
      {
        appendStringInfoString(buf, splan->plan_name);
      }
      if (lnext(lc))
      {
        appendStringInfoString(buf, " or ");
      }
    }
    appendStringInfoChar(buf, ')');
  }
  break;

  case T_FieldSelect:
  {
    FieldSelect *fselect = (FieldSelect *)node;
    Node *arg = (Node *)fselect->arg;
    int fno = fselect->fieldnum;
    const char *fieldname;
    bool need_parens;

       
                                                                   
                                                                 
                                                                   
                                                            
       
    need_parens = !IsA(arg, SubscriptingRef) && !IsA(arg, FieldSelect);
    if (need_parens)
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr(arg, context, true);
    if (need_parens)
    {
      appendStringInfoChar(buf, ')');
    }

       
                                     
       
    fieldname = get_name_for_var_field((Var *)arg, fno, 0, context);
    appendStringInfo(buf, ".%s", quote_identifier(fieldname));
  }
  break;

  case T_FieldStore:
  {
    FieldStore *fstore = (FieldStore *)node;
    bool need_parens;

       
                                                                   
                                                              
                                                    
                                                             
                                                                 
                                                                 
                                                             
                                                                   
                                                                  
                                                            
                                                               
                                                               
                                                               
                                                            
                                                                   
                                                     
                                                          
       
    need_parens = (list_length(fstore->newvals) != 1);
    if (need_parens)
    {
      appendStringInfoString(buf, "ROW(");
    }
    get_rule_expr((Node *)fstore->newvals, context, showimplicit);
    if (need_parens)
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_RelabelType:
  {
    RelabelType *relabel = (RelabelType *)node;
    Node *arg = (Node *)relabel->arg;

    if (relabel->relabelformat == COERCE_IMPLICIT_CAST && !showimplicit)
    {
                                        
      get_rule_expr_paren(arg, context, false, node);
    }
    else
    {
      get_coercion_expr(arg, context, relabel->resulttype, relabel->resulttypmod, node);
    }
  }
  break;

  case T_CoerceViaIO:
  {
    CoerceViaIO *iocoerce = (CoerceViaIO *)node;
    Node *arg = (Node *)iocoerce->arg;

    if (iocoerce->coerceformat == COERCE_IMPLICIT_CAST && !showimplicit)
    {
                                        
      get_rule_expr_paren(arg, context, false, node);
    }
    else
    {
      get_coercion_expr(arg, context, iocoerce->resulttype, -1, node);
    }
  }
  break;

  case T_ArrayCoerceExpr:
  {
    ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;
    Node *arg = (Node *)acoerce->arg;

    if (acoerce->coerceformat == COERCE_IMPLICIT_CAST && !showimplicit)
    {
                                        
      get_rule_expr_paren(arg, context, false, node);
    }
    else
    {
      get_coercion_expr(arg, context, acoerce->resulttype, acoerce->resulttypmod, node);
    }
  }
  break;

  case T_ConvertRowtypeExpr:
  {
    ConvertRowtypeExpr *convert = (ConvertRowtypeExpr *)node;
    Node *arg = (Node *)convert->arg;

    if (convert->convertformat == COERCE_IMPLICIT_CAST && !showimplicit)
    {
                                        
      get_rule_expr_paren(arg, context, false, node);
    }
    else
    {
      get_coercion_expr(arg, context, convert->resulttype, -1, node);
    }
  }
  break;

  case T_CollateExpr:
  {
    CollateExpr *collate = (CollateExpr *)node;
    Node *arg = (Node *)collate->arg;

    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr_paren(arg, context, showimplicit, node);
    appendStringInfo(buf, " COLLATE %s", generate_collation_name(collate->collOid));
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_CaseExpr:
  {
    CaseExpr *caseexpr = (CaseExpr *)node;
    ListCell *temp;

    appendContextKeyword(context, "CASE", 0, PRETTYINDENT_VAR, 0);
    if (caseexpr->arg)
    {
      appendStringInfoChar(buf, ' ');
      get_rule_expr((Node *)caseexpr->arg, context, true);
    }
    foreach (temp, caseexpr->args)
    {
      CaseWhen *when = (CaseWhen *)lfirst(temp);
      Node *w = (Node *)when->expr;

      if (caseexpr->arg)
      {
           
                                                               
                                                       
                                                              
                                                              
                                                     
                                                             
                                                           
                                                              
                                                             
                                                               
           
        if (IsA(w, OpExpr))
        {
          List *args = ((OpExpr *)w)->args;

          if (list_length(args) == 2 && IsA(strip_implicit_coercions(linitial(args)), CaseTestExpr))
          {
            w = (Node *)lsecond(args);
          }
        }
      }

      if (!PRETTY_INDENT(context))
      {
        appendStringInfoChar(buf, ' ');
      }
      appendContextKeyword(context, "WHEN ", 0, 0, 0);
      get_rule_expr(w, context, false);
      appendStringInfoString(buf, " THEN ");
      get_rule_expr((Node *)when->result, context, true);
    }
    if (!PRETTY_INDENT(context))
    {
      appendStringInfoChar(buf, ' ');
    }
    appendContextKeyword(context, "ELSE ", 0, 0, 0);
    get_rule_expr((Node *)caseexpr->defresult, context, true);
    if (!PRETTY_INDENT(context))
    {
      appendStringInfoChar(buf, ' ');
    }
    appendContextKeyword(context, "END", -PRETTYINDENT_VAR, 0, 0);
  }
  break;

  case T_CaseTestExpr:
  {
       
                                                                
                                                           
                                                                 
                                                                   
                                               
       
    appendStringInfoString(buf, "CASE_TEST_EXPR");
  }
  break;

  case T_ArrayExpr:
  {
    ArrayExpr *arrayexpr = (ArrayExpr *)node;

    appendStringInfoString(buf, "ARRAY[");
    get_rule_expr((Node *)arrayexpr->elements, context, true);
    appendStringInfoChar(buf, ']');

       
                                                            
                                                               
                                                    
       
    if (arrayexpr->elements == NIL)
    {
      appendStringInfo(buf, "::%s", format_type_with_typemod(arrayexpr->array_typeid, -1));
    }
  }
  break;

  case T_RowExpr:
  {
    RowExpr *rowexpr = (RowExpr *)node;
    TupleDesc tupdesc = NULL;
    ListCell *arg;
    int i;
    char *sep;

       
                                                                
                                                              
                
       
    if (rowexpr->row_typeid != RECORDOID)
    {
      tupdesc = lookup_rowtype_tupdesc(rowexpr->row_typeid, -1);
      Assert(list_length(rowexpr->args) <= tupdesc->natts);
    }

       
                                                                
                                                          
       
    appendStringInfoString(buf, "ROW(");
    sep = "";
    i = 0;
    foreach (arg, rowexpr->args)
    {
      Node *e = (Node *)lfirst(arg);

      if (tupdesc == NULL || !TupleDescAttr(tupdesc, i)->attisdropped)
      {
        appendStringInfoString(buf, sep);
                                                        
        get_rule_expr_toplevel(e, context, true);
        sep = ", ";
      }
      i++;
    }
    if (tupdesc != NULL)
    {
      while (i < tupdesc->natts)
      {
        if (!TupleDescAttr(tupdesc, i)->attisdropped)
        {
          appendStringInfoString(buf, sep);
          appendStringInfoString(buf, "NULL");
          sep = ", ";
        }
        i++;
      }

      ReleaseTupleDesc(tupdesc);
    }
    appendStringInfoChar(buf, ')');
    if (rowexpr->row_format == COERCE_EXPLICIT_CAST)
    {
      appendStringInfo(buf, "::%s", format_type_with_typemod(rowexpr->row_typeid, -1));
    }
  }
  break;

  case T_RowCompareExpr:
  {
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;

       
                                                                
                                                                  
                                                                   
                                   
       
    appendStringInfoString(buf, "(ROW(");
    get_rule_list_toplevel(rcexpr->largs, context, true);

       
                                                                 
                                                            
                                                              
                                                                 
                   
       
    appendStringInfo(buf, ") %s ROW(", generate_operator_name(linitial_oid(rcexpr->opnos), exprType(linitial(rcexpr->largs)), exprType(linitial(rcexpr->rargs))));
    get_rule_list_toplevel(rcexpr->rargs, context, true);
    appendStringInfoString(buf, "))");
  }
  break;

  case T_CoalesceExpr:
  {
    CoalesceExpr *coalesceexpr = (CoalesceExpr *)node;

    appendStringInfoString(buf, "COALESCE(");
    get_rule_expr((Node *)coalesceexpr->args, context, true);
    appendStringInfoChar(buf, ')');
  }
  break;

  case T_MinMaxExpr:
  {
    MinMaxExpr *minmaxexpr = (MinMaxExpr *)node;

    switch (minmaxexpr->op)
    {
    case IS_GREATEST:
      appendStringInfoString(buf, "GREATEST(");
      break;
    case IS_LEAST:
      appendStringInfoString(buf, "LEAST(");
      break;
    }
    get_rule_expr((Node *)minmaxexpr->args, context, true);
    appendStringInfoChar(buf, ')');
  }
  break;

  case T_SQLValueFunction:
  {
    SQLValueFunction *svf = (SQLValueFunction *)node;

       
                                                                  
                                           
       
    switch (svf->op)
    {
    case SVFOP_CURRENT_DATE:
      appendStringInfoString(buf, "CURRENT_DATE");
      break;
    case SVFOP_CURRENT_TIME:
      appendStringInfoString(buf, "CURRENT_TIME");
      break;
    case SVFOP_CURRENT_TIME_N:
      appendStringInfo(buf, "CURRENT_TIME(%d)", svf->typmod);
      break;
    case SVFOP_CURRENT_TIMESTAMP:
      appendStringInfoString(buf, "CURRENT_TIMESTAMP");
      break;
    case SVFOP_CURRENT_TIMESTAMP_N:
      appendStringInfo(buf, "CURRENT_TIMESTAMP(%d)", svf->typmod);
      break;
    case SVFOP_LOCALTIME:
      appendStringInfoString(buf, "LOCALTIME");
      break;
    case SVFOP_LOCALTIME_N:
      appendStringInfo(buf, "LOCALTIME(%d)", svf->typmod);
      break;
    case SVFOP_LOCALTIMESTAMP:
      appendStringInfoString(buf, "LOCALTIMESTAMP");
      break;
    case SVFOP_LOCALTIMESTAMP_N:
      appendStringInfo(buf, "LOCALTIMESTAMP(%d)", svf->typmod);
      break;
    case SVFOP_CURRENT_ROLE:
      appendStringInfoString(buf, "CURRENT_ROLE");
      break;
    case SVFOP_CURRENT_USER:
      appendStringInfoString(buf, "CURRENT_USER");
      break;
    case SVFOP_USER:
      appendStringInfoString(buf, "USER");
      break;
    case SVFOP_SESSION_USER:
      appendStringInfoString(buf, "SESSION_USER");
      break;
    case SVFOP_CURRENT_CATALOG:
      appendStringInfoString(buf, "CURRENT_CATALOG");
      break;
    case SVFOP_CURRENT_SCHEMA:
      appendStringInfoString(buf, "CURRENT_SCHEMA");
      break;
    }
  }
  break;

  case T_XmlExpr:
  {
    XmlExpr *xexpr = (XmlExpr *)node;
    bool needcomma = false;
    ListCell *arg;
    ListCell *narg;
    Const *con;

    switch (xexpr->op)
    {
    case IS_XMLCONCAT:
      appendStringInfoString(buf, "XMLCONCAT(");
      break;
    case IS_XMLELEMENT:
      appendStringInfoString(buf, "XMLELEMENT(");
      break;
    case IS_XMLFOREST:
      appendStringInfoString(buf, "XMLFOREST(");
      break;
    case IS_XMLPARSE:
      appendStringInfoString(buf, "XMLPARSE(");
      break;
    case IS_XMLPI:
      appendStringInfoString(buf, "XMLPI(");
      break;
    case IS_XMLROOT:
      appendStringInfoString(buf, "XMLROOT(");
      break;
    case IS_XMLSERIALIZE:
      appendStringInfoString(buf, "XMLSERIALIZE(");
      break;
    case IS_DOCUMENT:
      break;
    }
    if (xexpr->op == IS_XMLPARSE || xexpr->op == IS_XMLSERIALIZE)
    {
      if (xexpr->xmloption == XMLOPTION_DOCUMENT)
      {
        appendStringInfoString(buf, "DOCUMENT ");
      }
      else
      {
        appendStringInfoString(buf, "CONTENT ");
      }
    }
    if (xexpr->name)
    {
      appendStringInfo(buf, "NAME %s", quote_identifier(map_xml_name_to_sql_identifier(xexpr->name)));
      needcomma = true;
    }
    if (xexpr->named_args)
    {
      if (xexpr->op != IS_XMLFOREST)
      {
        if (needcomma)
        {
          appendStringInfoString(buf, ", ");
        }
        appendStringInfoString(buf, "XMLATTRIBUTES(");
        needcomma = false;
      }
      forboth(arg, xexpr->named_args, narg, xexpr->arg_names)
      {
        Node *e = (Node *)lfirst(arg);
        char *argname = strVal(lfirst(narg));

        if (needcomma)
        {
          appendStringInfoString(buf, ", ");
        }
        get_rule_expr((Node *)e, context, true);
        appendStringInfo(buf, " AS %s", quote_identifier(map_xml_name_to_sql_identifier(argname)));
        needcomma = true;
      }
      if (xexpr->op != IS_XMLFOREST)
      {
        appendStringInfoChar(buf, ')');
      }
    }
    if (xexpr->args)
    {
      if (needcomma)
      {
        appendStringInfoString(buf, ", ");
      }
      switch (xexpr->op)
      {
      case IS_XMLCONCAT:
      case IS_XMLELEMENT:
      case IS_XMLFOREST:
      case IS_XMLPI:
      case IS_XMLSERIALIZE:
                                        
        get_rule_expr((Node *)xexpr->args, context, true);
        break;
      case IS_XMLPARSE:
        Assert(list_length(xexpr->args) == 2);

        get_rule_expr((Node *)linitial(xexpr->args), context, true);

        con = lsecond_node(Const, xexpr->args);
        Assert(!con->constisnull);
        if (DatumGetBool(con->constvalue))
        {
          appendStringInfoString(buf, " PRESERVE WHITESPACE");
        }
        else
        {
          appendStringInfoString(buf, " STRIP WHITESPACE");
        }
        break;
      case IS_XMLROOT:
        Assert(list_length(xexpr->args) == 3);

        get_rule_expr((Node *)linitial(xexpr->args), context, true);

        appendStringInfoString(buf, ", VERSION ");
        con = (Const *)lsecond(xexpr->args);
        if (IsA(con, Const) && con->constisnull)
        {
          appendStringInfoString(buf, "NO VALUE");
        }
        else
        {
          get_rule_expr((Node *)con, context, false);
        }

        con = lthird_node(Const, xexpr->args);
        if (con->constisnull)
                                            ;
        else
        {
          switch (DatumGetInt32(con->constvalue))
          {
          case XML_STANDALONE_YES:
            appendStringInfoString(buf, ", STANDALONE YES");
            break;
          case XML_STANDALONE_NO:
            appendStringInfoString(buf, ", STANDALONE NO");
            break;
          case XML_STANDALONE_NO_VALUE:
            appendStringInfoString(buf, ", STANDALONE NO VALUE");
            break;
          default:
            break;
          }
        }
        break;
      case IS_DOCUMENT:
        get_rule_expr_paren((Node *)xexpr->args, context, false, node);
        break;
      }
    }
    if (xexpr->op == IS_XMLSERIALIZE)
    {
      appendStringInfo(buf, " AS %s", format_type_with_typemod(xexpr->type, xexpr->typmod));
    }
    if (xexpr->op == IS_DOCUMENT)
    {
      appendStringInfoString(buf, " IS DOCUMENT");
    }
    else
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_NullTest:
  {
    NullTest *ntest = (NullTest *)node;

    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr_paren((Node *)ntest->arg, context, true, node);

       
                                                               
                                                                  
                                                             
                                                      
       
    if (ntest->argisrow || !type_is_rowtype(exprType((Node *)ntest->arg)))
    {
      switch (ntest->nulltesttype)
      {
      case IS_NULL:
        appendStringInfoString(buf, " IS NULL");
        break;
      case IS_NOT_NULL:
        appendStringInfoString(buf, " IS NOT NULL");
        break;
      default:
        elog(ERROR, "unrecognized nulltesttype: %d", (int)ntest->nulltesttype);
      }
    }
    else
    {
      switch (ntest->nulltesttype)
      {
      case IS_NULL:
        appendStringInfoString(buf, " IS NOT DISTINCT FROM NULL");
        break;
      case IS_NOT_NULL:
        appendStringInfoString(buf, " IS DISTINCT FROM NULL");
        break;
      default:
        elog(ERROR, "unrecognized nulltesttype: %d", (int)ntest->nulltesttype);
      }
    }
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_BooleanTest:
  {
    BooleanTest *btest = (BooleanTest *)node;

    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr_paren((Node *)btest->arg, context, false, node);
    switch (btest->booltesttype)
    {
    case IS_TRUE:
      appendStringInfoString(buf, " IS TRUE");
      break;
    case IS_NOT_TRUE:
      appendStringInfoString(buf, " IS NOT TRUE");
      break;
    case IS_FALSE:
      appendStringInfoString(buf, " IS FALSE");
      break;
    case IS_NOT_FALSE:
      appendStringInfoString(buf, " IS NOT FALSE");
      break;
    case IS_UNKNOWN:
      appendStringInfoString(buf, " IS UNKNOWN");
      break;
    case IS_NOT_UNKNOWN:
      appendStringInfoString(buf, " IS NOT UNKNOWN");
      break;
    default:
      elog(ERROR, "unrecognized booltesttype: %d", (int)btest->booltesttype);
    }
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, ')');
    }
  }
  break;

  case T_CoerceToDomain:
  {
    CoerceToDomain *ctest = (CoerceToDomain *)node;
    Node *arg = (Node *)ctest->arg;

    if (ctest->coercionformat == COERCE_IMPLICIT_CAST && !showimplicit)
    {
                                        
      get_rule_expr(arg, context, false);
    }
    else
    {
      get_coercion_expr(arg, context, ctest->resulttype, ctest->resulttypmod, node);
    }
  }
  break;

  case T_CoerceToDomainValue:
    appendStringInfoString(buf, "VALUE");
    break;

  case T_SetToDefault:
    appendStringInfoString(buf, "DEFAULT");
    break;

  case T_CurrentOfExpr:
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)node;

    if (cexpr->cursor_name)
    {
      appendStringInfo(buf, "CURRENT OF %s", quote_identifier(cexpr->cursor_name));
    }
    else
    {
      appendStringInfo(buf, "CURRENT OF $%d", cexpr->cursor_param);
    }
  }
  break;

  case T_NextValueExpr:
  {
    NextValueExpr *nvexpr = (NextValueExpr *)node;

       
                                                                 
                               
       
    appendStringInfoString(buf, "nextval(");
    simple_quote_literal(buf, generate_relation_name(nvexpr->seqid, NIL));
    appendStringInfoChar(buf, ')');
  }
  break;

  case T_InferenceElem:
  {
    InferenceElem *iexpr = (InferenceElem *)node;
    bool save_varprefix;
    bool need_parens;

       
                                                             
                                                                  
       
    save_varprefix = context->varprefix;
    context->varprefix = false;

       
                                                                   
                                                         
       
    need_parens = !IsA(iexpr->expr, Var);
    if (IsA(iexpr->expr, FuncExpr) && ((FuncExpr *)iexpr->expr)->funcformat == COERCE_EXPLICIT_CALL)
    {
      need_parens = false;
    }

    if (need_parens)
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr((Node *)iexpr->expr, context, false);
    if (need_parens)
    {
      appendStringInfoChar(buf, ')');
    }

    context->varprefix = save_varprefix;

    if (iexpr->infercollid)
    {
      appendStringInfo(buf, " COLLATE %s", generate_collation_name(iexpr->infercollid));
    }

                                                     
    if (iexpr->inferopclass)
    {
      Oid inferopclass = iexpr->inferopclass;
      Oid inferopcinputtype = get_opclass_input_type(iexpr->inferopclass);

      get_opclass_name(inferopclass, inferopcinputtype, buf);
    }
  }
  break;

  case T_PartitionBoundSpec:
  {
    PartitionBoundSpec *spec = (PartitionBoundSpec *)node;
    ListCell *cell;
    char *sep;

    if (spec->is_default)
    {
      appendStringInfoString(buf, "DEFAULT");
      break;
    }

    switch (spec->strategy)
    {
    case PARTITION_STRATEGY_HASH:
      Assert(spec->modulus > 0 && spec->remainder >= 0);
      Assert(spec->modulus > spec->remainder);

      appendStringInfoString(buf, "FOR VALUES");
      appendStringInfo(buf, " WITH (modulus %d, remainder %d)", spec->modulus, spec->remainder);
      break;

    case PARTITION_STRATEGY_LIST:
      Assert(spec->listdatums != NIL);

      appendStringInfoString(buf, "FOR VALUES IN (");
      sep = "";
      foreach (cell, spec->listdatums)
      {
        Const *val = castNode(Const, lfirst(cell));

        appendStringInfoString(buf, sep);
        get_const_expr(val, context, -1);
        sep = ", ";
      }

      appendStringInfoChar(buf, ')');
      break;

    case PARTITION_STRATEGY_RANGE:
      Assert(spec->lowerdatums != NIL && spec->upperdatums != NIL && list_length(spec->lowerdatums) == list_length(spec->upperdatums));

      appendStringInfo(buf, "FOR VALUES FROM %s TO %s", get_range_partbound_string(spec->lowerdatums), get_range_partbound_string(spec->upperdatums));
      break;

    default:
      elog(ERROR, "unrecognized partition strategy: %d", (int)spec->strategy);
      break;
    }
  }
  break;

  case T_List:
  {
    char *sep;
    ListCell *l;

    sep = "";
    foreach (l, (List *)node)
    {
      appendStringInfoString(buf, sep);
      get_rule_expr((Node *)lfirst(l), context, showimplicit);
      sep = ", ";
    }
  }
  break;

  case T_TableFunc:
    get_tablefunc((TableFunc *)node, context, showimplicit);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
}

   
                                                              
   
                                                                           
                                                                              
                                                                      
                                                                         
                                                                           
                                                                            
                                                                               
   
static void
get_rule_expr_toplevel(Node *node, deparse_context *context, bool showimplicit)
{
  if (node && IsA(node, Var))
  {
    (void)get_variable((Var *)node, 0, true, context);
  }
  else
  {
    get_rule_expr(node, context, showimplicit);
  }
}

   
                                                                       
   
                                                             
   
                                                                       
                                        
   
static void
get_rule_list_toplevel(List *lst, deparse_context *context, bool showimplicit)
{
  const char *sep;
  ListCell *lc;

  sep = "";
  foreach (lc, lst)
  {
    Node *e = (Node *)lfirst(lc);

    appendStringInfoString(context->buf, sep);
    get_rule_expr_toplevel(e, context, showimplicit);
    sep = ", ";
  }
}

   
                                                                   
   
                                                                          
                                                                              
                                                                            
                                                                            
                                                                             
                                                                             
                                                                              
                         
   
static void
get_rule_expr_funccall(Node *node, deparse_context *context, bool showimplicit)
{
  if (looks_like_function(node))
  {
    get_rule_expr(node, context, showimplicit);
  }
  else
  {
    StringInfo buf = context->buf;

    appendStringInfoString(buf, "CAST(");
                                                         
    get_rule_expr(node, context, false);
    appendStringInfo(buf, " AS %s)", format_type_with_typemod(exprType(node), exprTypmod(node)));
  }
}

   
                                                                             
                                                 
   
static bool
looks_like_function(Node *node)
{
  if (node == NULL)
  {
    return false;                                
  }
  switch (nodeTag(node))
  {
  case T_FuncExpr:
                                                    
    return (((FuncExpr *)node)->funcformat == COERCE_EXPLICIT_CALL);
  case T_NullIfExpr:
  case T_CoalesceExpr:
  case T_MinMaxExpr:
  case T_SQLValueFunction:
  case T_XmlExpr:
                                                            
    return true;
  default:
    break;
  }
  return false;
}

   
                                               
   
static void
get_oper_expr(OpExpr *expr, deparse_context *context)
{
  StringInfo buf = context->buf;
  Oid opno = expr->opno;
  List *args = expr->args;

  if (!PRETTY_PAREN(context))
  {
    appendStringInfoChar(buf, '(');
  }
  if (list_length(args) == 2)
  {
                         
    Node *arg1 = (Node *)linitial(args);
    Node *arg2 = (Node *)lsecond(args);

    get_rule_expr_paren(arg1, context, true, (Node *)expr);
    appendStringInfo(buf, " %s ", generate_operator_name(opno, exprType(arg1), exprType(arg2)));
    get_rule_expr_paren(arg2, context, true, (Node *)expr);
  }
  else
  {
                                            
    Node *arg = (Node *)linitial(args);
    HeapTuple tp;
    Form_pg_operator optup;

    tp = SearchSysCache1(OPEROID, ObjectIdGetDatum(opno));
    if (!HeapTupleIsValid(tp))
    {
      elog(ERROR, "cache lookup failed for operator %u", opno);
    }
    optup = (Form_pg_operator)GETSTRUCT(tp);
    switch (optup->oprkind)
    {
    case 'l':
      appendStringInfo(buf, "%s ", generate_operator_name(opno, InvalidOid, exprType(arg)));
      get_rule_expr_paren(arg, context, true, (Node *)expr);
      break;
    case 'r':
      get_rule_expr_paren(arg, context, true, (Node *)expr);
      appendStringInfo(buf, " %s", generate_operator_name(opno, exprType(arg), InvalidOid));
      break;
    default:
      elog(ERROR, "bogus oprkind: %d", optup->oprkind);
    }
    ReleaseSysCache(tp);
  }
  if (!PRETTY_PAREN(context))
  {
    appendStringInfoChar(buf, ')');
  }
}

   
                                                
   
static void
get_func_expr(FuncExpr *expr, deparse_context *context, bool showimplicit)
{
  StringInfo buf = context->buf;
  Oid funcoid = expr->funcid;
  Oid argtypes[FUNC_MAX_ARGS];
  int nargs;
  List *argnames;
  bool use_variadic;
  ListCell *l;

     
                                                                             
                                                                       
     
  if (expr->funcformat == COERCE_IMPLICIT_CAST && !showimplicit)
  {
    get_rule_expr_paren((Node *)linitial(expr->args), context, false, (Node *)expr);
    return;
  }

     
                                                                         
                                      
     
  if (expr->funcformat == COERCE_EXPLICIT_CAST || expr->funcformat == COERCE_IMPLICIT_CAST)
  {
    Node *arg = linitial(expr->args);
    Oid rettype = expr->funcresulttype;
    int32 coercedTypmod;

                                                              
    (void)exprIsLengthCoercion((Node *)expr, &coercedTypmod);

    get_coercion_expr(arg, context, rettype, coercedTypmod, (Node *)expr);

    return;
  }

     
                                                                          
                             
     
  if (list_length(expr->args) > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg("too many arguments")));
  }
  nargs = 0;
  argnames = NIL;
  foreach (l, expr->args)
  {
    Node *arg = (Node *)lfirst(l);

    if (IsA(arg, NamedArgExpr))
    {
      argnames = lappend(argnames, ((NamedArgExpr *)arg)->name);
    }
    argtypes[nargs] = exprType(arg);
    nargs++;
  }

  appendStringInfo(buf, "%s(", generate_function_name(funcoid, nargs, argnames, argtypes, expr->funcvariadic, &use_variadic, context->special_exprkind));
  nargs = 0;
  foreach (l, expr->args)
  {
    if (nargs++ > 0)
    {
      appendStringInfoString(buf, ", ");
    }
    if (use_variadic && lnext(l) == NULL)
    {
      appendStringInfoString(buf, "VARIADIC ");
    }
    get_rule_expr((Node *)lfirst(l), context, true);
  }
  appendStringInfoChar(buf, ')');
}

   
                                              
   
static void
get_agg_expr(Aggref *aggref, deparse_context *context, Aggref *original_aggref)
{
  StringInfo buf = context->buf;
  Oid argtypes[FUNC_MAX_ARGS];
  int nargs;
  bool use_variadic;

     
                                                                         
                                                                     
                                                                            
                                                                             
                                   
     
  if (DO_AGGSPLIT_COMBINE(aggref->aggsplit))
  {
    TargetEntry *tle = linitial_node(TargetEntry, aggref->args);

    Assert(list_length(aggref->args) == 1);
    resolve_special_varno((Node *)tle->expr, context, original_aggref, get_agg_combine_expr);
    return;
  }

     
                                                                            
                                                                     
     
  if (DO_AGGSPLIT_SKIPFINAL(original_aggref->aggsplit))
  {
    appendStringInfoString(buf, "PARTIAL ");
  }

                                                        
  nargs = get_aggregate_argtypes(aggref, argtypes);

                                                            
  appendStringInfo(buf, "%s(%s", generate_function_name(aggref->aggfnoid, nargs, NIL, argtypes, aggref->aggvariadic, &use_variadic, context->special_exprkind), (aggref->aggdistinct != NIL) ? "DISTINCT " : "");

  if (AGGKIND_IS_ORDERED_SET(aggref->aggkind))
  {
       
                                                                       
                                                                       
                   
       
    Assert(!aggref->aggvariadic);
    get_rule_expr((Node *)aggref->aggdirectargs, context, true);
    Assert(aggref->aggorder != NIL);
    appendStringInfoString(buf, ") WITHIN GROUP (ORDER BY ");
    get_rule_orderby(aggref->aggorder, aggref->args, false, context);
  }
  else
  {
                                                             
    if (aggref->aggstar)
    {
      appendStringInfoChar(buf, '*');
    }
    else
    {
      ListCell *l;
      int i;

      i = 0;
      foreach (l, aggref->args)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(l);
        Node *arg = (Node *)tle->expr;

        Assert(!IsA(arg, NamedArgExpr));
        if (tle->resjunk)
        {
          continue;
        }
        if (i++ > 0)
        {
          appendStringInfoString(buf, ", ");
        }
        if (use_variadic && i == nargs)
        {
          appendStringInfoString(buf, "VARIADIC ");
        }
        get_rule_expr(arg, context, true);
      }
    }

    if (aggref->aggorder != NIL)
    {
      appendStringInfoString(buf, " ORDER BY ");
      get_rule_orderby(aggref->aggorder, aggref->args, false, context);
    }
  }

  if (aggref->aggfilter != NULL)
  {
    appendStringInfoString(buf, ") FILTER (WHERE ");
    get_rule_expr((Node *)aggref->aggfilter, context, false);
  }

  appendStringInfoChar(buf, ')');
}

   
                                                                            
                                                                               
                               
   
static void
get_agg_combine_expr(Node *node, deparse_context *context, void *private)
{
  Aggref *aggref;
  Aggref *original_aggref = private;

  if (!IsA(node, Aggref))
  {
    elog(ERROR, "combining Aggref does not point to an Aggref");
  }

  aggref = (Aggref *)node;
  get_agg_expr(aggref, context, original_aggref);
}

   
                                                      
   
static void
get_windowfunc_expr(WindowFunc *wfunc, deparse_context *context)
{
  StringInfo buf = context->buf;
  Oid argtypes[FUNC_MAX_ARGS];
  int nargs;
  List *argnames;
  ListCell *l;

  if (list_length(wfunc->args) > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg("too many arguments")));
  }
  nargs = 0;
  argnames = NIL;
  foreach (l, wfunc->args)
  {
    Node *arg = (Node *)lfirst(l);

    if (IsA(arg, NamedArgExpr))
    {
      argnames = lappend(argnames, ((NamedArgExpr *)arg)->name);
    }
    argtypes[nargs] = exprType(arg);
    nargs++;
  }

  appendStringInfo(buf, "%s(", generate_function_name(wfunc->winfnoid, nargs, argnames, argtypes, false, NULL, context->special_exprkind));
                                                           
  if (wfunc->winstar)
  {
    appendStringInfoChar(buf, '*');
  }
  else
  {
    get_rule_expr((Node *)wfunc->args, context, true);
  }

  if (wfunc->aggfilter != NULL)
  {
    appendStringInfoString(buf, ") FILTER (WHERE ");
    get_rule_expr((Node *)wfunc->aggfilter, context, false);
  }

  appendStringInfoString(buf, ") OVER ");

  foreach (l, context->windowClause)
  {
    WindowClause *wc = (WindowClause *)lfirst(l);

    if (wc->winref == wfunc->winref)
    {
      if (wc->name)
      {
        appendStringInfoString(buf, quote_identifier(wc->name));
      }
      else
      {
        get_rule_windowspec(wc, context->windowTList, context);
      }
      break;
    }
  }
  if (l == NULL)
  {
    if (context->windowClause)
    {
      elog(ERROR, "could not find window clause for winref %u", wfunc->winref);
    }

       
                                                                          
                                   
       
    appendStringInfoString(buf, "(?)");
  }
}

              
                     
   
                                                                      
              
   
static void
get_coercion_expr(Node *arg, deparse_context *context, Oid resulttype, int32 resulttypmod, Node *parentNode)
{
  StringInfo buf = context->buf;

     
                                                                      
                                                                         
                                                                         
                                                                            
                                                                   
                           
     
                                                                           
                                                                           
                                                                             
                                                                       
                                                                       
                                                            
     
  if (arg && IsA(arg, Const) && ((Const *)arg)->consttype == resulttype && ((Const *)arg)->consttypmod == -1)
  {
                                                                
    get_const_expr((Const *)arg, context, -1);
  }
  else
  {
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, '(');
    }
    get_rule_expr_paren(arg, context, false, parentNode);
    if (!PRETTY_PAREN(context))
    {
      appendStringInfoChar(buf, ')');
    }
  }

     
                                                                           
                                                                       
                                                                             
                                                                           
                      
     
  appendStringInfo(buf, "::%s", format_type_with_typemod(resulttype, resulttypmod));
}

              
                  
   
                                           
   
                                                                             
                                                                           
                              
   
                                                                       
                                                                            
                                                                            
                                                                      
              
   
static void
get_const_expr(Const *constval, deparse_context *context, int showtype)
{
  StringInfo buf = context->buf;
  Oid typoutput;
  bool typIsVarlena;
  char *extval;
  bool needlabel = false;

  if (constval->constisnull)
  {
       
                                                                        
                                  
       
    appendStringInfoString(buf, "NULL");
    if (showtype >= 0)
    {
      appendStringInfo(buf, "::%s", format_type_with_typemod(constval->consttype, constval->consttypmod));
      get_const_collation(constval, context);
    }
    return;
  }

  getTypeOutputInfo(constval->consttype, &typoutput, &typIsVarlena);

  extval = OidOutputFunctionCall(typoutput, constval->constvalue);

  switch (constval->consttype)
  {
  case INT4OID:

       
                                                                
                                                                    
                                                                      
                                                                     
                                                                       
                                                                   
                                       
       
    if (extval[0] != '-')
    {
      appendStringInfoString(buf, extval);
    }
    else
    {
      appendStringInfo(buf, "'%s'", extval);
      needlabel = true;                            
    }
    break;

  case NUMERICOID:

       
                                                                      
                                                                      
                                                              
       
    if (isdigit((unsigned char)extval[0]) && strcspn(extval, "eE.") != strlen(extval))
    {
      appendStringInfoString(buf, extval);
    }
    else
    {
      appendStringInfo(buf, "'%s'", extval);
      needlabel = true;                            
    }
    break;

  case BOOLOID:
    if (strcmp(extval, "t") == 0)
    {
      appendStringInfoString(buf, "true");
    }
    else
    {
      appendStringInfoString(buf, "false");
    }
    break;

  default:
    simple_quote_literal(buf, extval);
    break;
  }

  pfree(extval);

  if (showtype < 0)
  {
    return;
  }

     
                                                                      
                                                            
     
                                                                           
                            
     
  switch (constval->consttype)
  {
  case BOOLOID:
  case UNKNOWNOID:
                                           
    needlabel = false;
    break;
  case INT4OID:
                                                       
    break;
  case NUMERICOID:

       
                                                                  
                                                                    
                
       
    needlabel |= (constval->consttypmod >= 0);
    break;
  default:
    needlabel = true;
    break;
  }
  if (needlabel || showtype > 0)
  {
    appendStringInfo(buf, "::%s", format_type_with_typemod(constval->consttype, constval->consttypmod));
  }

  get_const_collation(constval, context);
}

   
                                                       
   
static void
get_const_collation(Const *constval, deparse_context *context)
{
  StringInfo buf = context->buf;

  if (OidIsValid(constval->constcollid))
  {
    Oid typcollation = get_typcollation(constval->consttype);

    if (constval->constcollid != typcollation)
    {
      appendStringInfo(buf, " COLLATE %s", generate_collation_name(constval->constcollid));
    }
  }
}

   
                                                                          
   
static void
simple_quote_literal(StringInfo buf, const char *val)
{
  const char *valptr;

     
                                                                       
                                                                            
                                           
     
  appendStringInfoChar(buf, '\'');
  for (valptr = val; *valptr; valptr++)
  {
    char ch = *valptr;

    if (SQL_STR_DOUBLE(ch, !standard_conforming_strings))
    {
      appendStringInfoChar(buf, ch);
    }
    appendStringInfoChar(buf, ch);
  }
  appendStringInfoChar(buf, '\'');
}

              
                                             
              
   
static void
get_sublink_expr(SubLink *sublink, deparse_context *context)
{
  StringInfo buf = context->buf;
  Query *query = (Query *)(sublink->subselect);
  char *opname = NULL;
  bool need_paren;

  if (sublink->subLinkType == ARRAY_SUBLINK)
  {
    appendStringInfoString(buf, "ARRAY(");
  }
  else
  {
    appendStringInfoChar(buf, '(');
  }

     
                                                                            
                                                                           
                                                                         
                                                                            
                                                      
     
  if (sublink->testexpr)
  {
    if (IsA(sublink->testexpr, OpExpr))
    {
                                     
      OpExpr *opexpr = (OpExpr *)sublink->testexpr;

      get_rule_expr(linitial(opexpr->args), context, true);
      opname = generate_operator_name(opexpr->opno, exprType(linitial(opexpr->args)), exprType(lsecond(opexpr->args)));
    }
    else if (IsA(sublink->testexpr, BoolExpr))
    {
                                                       
      char *sep;
      ListCell *l;

      appendStringInfoChar(buf, '(');
      sep = "";
      foreach (l, ((BoolExpr *)sublink->testexpr)->args)
      {
        OpExpr *opexpr = lfirst_node(OpExpr, l);

        appendStringInfoString(buf, sep);
        get_rule_expr(linitial(opexpr->args), context, true);
        if (!opname)
        {
          opname = generate_operator_name(opexpr->opno, exprType(linitial(opexpr->args)), exprType(lsecond(opexpr->args)));
        }
        sep = ", ";
      }
      appendStringInfoChar(buf, ')');
    }
    else if (IsA(sublink->testexpr, RowCompareExpr))
    {
                                                         
      RowCompareExpr *rcexpr = (RowCompareExpr *)sublink->testexpr;

      appendStringInfoChar(buf, '(');
      get_rule_expr((Node *)rcexpr->largs, context, true);
      opname = generate_operator_name(linitial_oid(rcexpr->opnos), exprType(linitial(rcexpr->largs)), exprType(linitial(rcexpr->rargs)));
      appendStringInfoChar(buf, ')');
    }
    else
    {
      elog(ERROR, "unrecognized testexpr type: %d", (int)nodeTag(sublink->testexpr));
    }
  }

  need_paren = true;

  switch (sublink->subLinkType)
  {
  case EXISTS_SUBLINK:
    appendStringInfoString(buf, "EXISTS ");
    break;

  case ANY_SUBLINK:
    if (strcmp(opname, "=") == 0)                            
    {
      appendStringInfoString(buf, " IN ");
    }
    else
    {
      appendStringInfo(buf, " %s ANY ", opname);
    }
    break;

  case ALL_SUBLINK:
    appendStringInfo(buf, " %s ALL ", opname);
    break;

  case ROWCOMPARE_SUBLINK:
    appendStringInfo(buf, " %s ", opname);
    break;

  case EXPR_SUBLINK:
  case MULTIEXPR_SUBLINK:
  case ARRAY_SUBLINK:
    need_paren = false;
    break;

  case CTE_SUBLINK:                                   
  default:
    elog(ERROR, "unrecognized sublink type: %d", (int)sublink->subLinkType);
    break;
  }

  if (need_paren)
  {
    appendStringInfoChar(buf, '(');
  }

  get_query_def(query, buf, context->namespaces, NULL, false, context->prettyFlags, context->wrapColumn, context->indentLevel);

  if (need_paren)
  {
    appendStringInfoString(buf, "))");
  }
  else
  {
    appendStringInfoChar(buf, ')');
  }
}

              
                                                 
              
   
static void
get_tablefunc(TableFunc *tf, deparse_context *context, bool showimplicit)
{
  StringInfo buf = context->buf;

                                                      

  appendStringInfoString(buf, "XMLTABLE(");

  if (tf->ns_uris != NIL)
  {
    ListCell *lc1, *lc2;
    bool first = true;

    appendStringInfoString(buf, "XMLNAMESPACES (");
    forboth(lc1, tf->ns_uris, lc2, tf->ns_names)
    {
      Node *expr = (Node *)lfirst(lc1);
      Value *ns_node = (Value *)lfirst(lc2);

      if (!first)
      {
        appendStringInfoString(buf, ", ");
      }
      else
      {
        first = false;
      }

      if (ns_node != NULL)
      {
        get_rule_expr(expr, context, showimplicit);
        appendStringInfo(buf, " AS %s", strVal(ns_node));
      }
      else
      {
        appendStringInfoString(buf, "DEFAULT ");
        get_rule_expr(expr, context, showimplicit);
      }
    }
    appendStringInfoString(buf, "), ");
  }

  appendStringInfoChar(buf, '(');
  get_rule_expr((Node *)tf->rowexpr, context, showimplicit);
  appendStringInfoString(buf, ") PASSING (");
  get_rule_expr((Node *)tf->docexpr, context, showimplicit);
  appendStringInfoChar(buf, ')');

  if (tf->colexprs != NIL)
  {
    ListCell *l1;
    ListCell *l2;
    ListCell *l3;
    ListCell *l4;
    ListCell *l5;
    int colnum = 0;

    appendStringInfoString(buf, " COLUMNS ");
    forfive(l1, tf->colnames, l2, tf->coltypes, l3, tf->coltypmods, l4, tf->colexprs, l5, tf->coldefexprs)
    {
      char *colname = strVal(lfirst(l1));
      Oid typid = lfirst_oid(l2);
      int32 typmod = lfirst_int(l3);
      Node *colexpr = (Node *)lfirst(l4);
      Node *coldefexpr = (Node *)lfirst(l5);
      bool ordinality = (tf->ordinalitycol == colnum);
      bool notnull = bms_is_member(colnum, tf->notnulls);

      if (colnum > 0)
      {
        appendStringInfoString(buf, ", ");
      }
      colnum++;

      appendStringInfo(buf, "%s %s", quote_identifier(colname), ordinality ? "FOR ORDINALITY" : format_type_with_typemod(typid, typmod));
      if (ordinality)
      {
        continue;
      }

      if (coldefexpr != NULL)
      {
        appendStringInfoString(buf, " DEFAULT (");
        get_rule_expr((Node *)coldefexpr, context, showimplicit);
        appendStringInfoChar(buf, ')');
      }
      if (colexpr != NULL)
      {
        appendStringInfoString(buf, " PATH (");
        get_rule_expr((Node *)colexpr, context, showimplicit);
        appendStringInfoChar(buf, ')');
      }
      if (notnull)
      {
        appendStringInfoString(buf, " NOT NULL");
      }
    }
  }

  appendStringInfoChar(buf, ')');
}

              
                                                
   
                                                                      
                                                                       
                                      
              
   
static void
get_from_clause(Query *query, const char *prefix, deparse_context *context)
{
  StringInfo buf = context->buf;
  bool first = true;
  ListCell *l;

     
                                                                           
                                                                          
                                                                         
                                                                          
                      
     
  foreach (l, query->jointree->fromlist)
  {
    Node *jtnode = (Node *)lfirst(l);

    if (IsA(jtnode, RangeTblRef))
    {
      int varno = ((RangeTblRef *)jtnode)->rtindex;
      RangeTblEntry *rte = rt_fetch(varno, query->rtable);

      if (!rte->inFromCl)
      {
        continue;
      }
    }

    if (first)
    {
      appendContextKeyword(context, prefix, -PRETTYINDENT_STD, PRETTYINDENT_STD, 2);
      first = false;

      get_from_clause_item(jtnode, query, context);
    }
    else
    {
      StringInfoData itembuf;

      appendStringInfoString(buf, ", ");

         
                                                                    
                                                                         
         
      initStringInfo(&itembuf);
      context->buf = &itembuf;

      get_from_clause_item(jtnode, query, context);

                                           
      context->buf = buf;

                                             
      if (PRETTY_INDENT(context) && context->wrapColumn >= 0)
      {
                                                      
        if (itembuf.len > 0 && itembuf.data[0] == '\n')
        {
                                                
                                                                    
          removeStringInfoSpaces(buf);
        }
        else
        {
          char *trailing_nl;

                                                                  
          trailing_nl = strrchr(buf->data, '\n');
          if (trailing_nl == NULL)
          {
            trailing_nl = buf->data;
          }
          else
          {
            trailing_nl++;
          }

             
                                                                   
                                      
             
          if (strlen(trailing_nl) + itembuf.len > context->wrapColumn)
          {
            appendContextKeyword(context, "", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_VAR);
          }
        }
      }

                            
      appendStringInfoString(buf, itembuf.data);

                    
      pfree(itembuf.data);
    }
  }
}

static void
get_from_clause_item(Node *jtnode, Query *query, deparse_context *context)
{
  StringInfo buf = context->buf;
  deparse_namespace *dpns = (deparse_namespace *)linitial(context->namespaces);

  if (IsA(jtnode, RangeTblRef))
  {
    int varno = ((RangeTblRef *)jtnode)->rtindex;
    RangeTblEntry *rte = rt_fetch(varno, query->rtable);
    char *refname = get_rtable_name(varno, context);
    deparse_columns *colinfo = deparse_columns_fetch(varno, dpns);
    RangeTblFunction *rtfunc1 = NULL;
    bool printalias;

    if (rte->lateral)
    {
      appendStringInfoString(buf, "LATERAL ");
    }

                                    
    switch (rte->rtekind)
    {
    case RTE_RELATION:
                               
      appendStringInfo(buf, "%s%s", only_marker(rte), generate_relation_name(rte->relid, context->namespaces));
      break;
    case RTE_SUBQUERY:
                        
      appendStringInfoChar(buf, '(');
      get_query_def(rte->subquery, buf, context->namespaces, NULL, true, context->prettyFlags, context->wrapColumn, context->indentLevel);
      appendStringInfoChar(buf, ')');
      break;
    case RTE_FUNCTION:
                        
      rtfunc1 = (RangeTblFunction *)linitial(rte->functions);

         
                                                                  
                                                                    
                                                                 
                                                                
         
      if (list_length(rte->functions) == 1 && (rtfunc1->funccolnames == NIL || !rte->funcordinality))
      {
        get_rule_expr_funccall(rtfunc1->funcexpr, context, true);
                                                             
      }
      else
      {
        bool all_unnest;
        ListCell *lc;

           
                                                                
                                                                   
                                                           
                                                         
                       
           
                                                                 
                                                                   
                                                               
                                                                
                                                          
                     
           
        all_unnest = true;
        foreach (lc, rte->functions)
        {
          RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);

          if (!IsA(rtfunc->funcexpr, FuncExpr) || ((FuncExpr *)rtfunc->funcexpr)->funcid != F_ARRAY_UNNEST || rtfunc->funccolnames != NIL)
          {
            all_unnest = false;
            break;
          }
        }

        if (all_unnest)
        {
          List *allargs = NIL;

          foreach (lc, rte->functions)
          {
            RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);
            List *args = ((FuncExpr *)rtfunc->funcexpr)->args;

            allargs = list_concat(allargs, list_copy(args));
          }

          appendStringInfoString(buf, "UNNEST(");
          get_rule_expr((Node *)allargs, context, true);
          appendStringInfoChar(buf, ')');
        }
        else
        {
          int funcno = 0;

          appendStringInfoString(buf, "ROWS FROM(");
          foreach (lc, rte->functions)
          {
            RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);

            if (funcno > 0)
            {
              appendStringInfoString(buf, ", ");
            }
            get_rule_expr_funccall(rtfunc->funcexpr, context, true);
            if (rtfunc->funccolnames != NIL)
            {
                                                          
              appendStringInfoString(buf, " AS ");
              get_from_clause_coldeflist(rtfunc, NULL, context);
            }
            funcno++;
          }
          appendStringInfoChar(buf, ')');
        }
                                                         
        rtfunc1 = NULL;
      }
      if (rte->funcordinality)
      {
        appendStringInfoString(buf, " WITH ORDINALITY");
      }
      break;
    case RTE_TABLEFUNC:
      get_tablefunc(rte->tablefunc, context, true);
      break;
    case RTE_VALUES:
                           
      appendStringInfoChar(buf, '(');
      get_values_def(rte->values_lists, context);
      appendStringInfoChar(buf, ')');
      break;
    case RTE_CTE:
      appendStringInfoString(buf, quote_identifier(rte->ctename));
      break;
    default:
      elog(ERROR, "unrecognized RTE kind: %d", (int)rte->rtekind);
      break;
    }

                                             
    printalias = false;
    if (rte->alias != NULL)
    {
                                                   
      printalias = true;
    }
    else if (colinfo->printaliases)
    {
                                                                 
      printalias = true;
    }
    else if (rte->rtekind == RTE_RELATION)
    {
         
                                                                    
                                                                        
                              
         
      if (strcmp(refname, get_relation_name(rte->relid)) != 0)
      {
        printalias = true;
      }
    }
    else if (rte->rtekind == RTE_FUNCTION)
    {
         
                                                                       
                                                            
                                                                      
                                                                        
         
      printalias = true;
    }
    else if (rte->rtekind == RTE_VALUES)
    {
                                                      
      printalias = true;
    }
    else if (rte->rtekind == RTE_CTE)
    {
         
                                                                     
                                                                  
                              
         
      if (strcmp(refname, rte->ctename) != 0)
      {
        printalias = true;
      }
    }
    if (printalias)
    {
      appendStringInfo(buf, " %s", quote_identifier(refname));
    }

                                                            
    if (rtfunc1 && rtfunc1->funccolnames != NIL)
    {
                                                                     
      get_from_clause_coldeflist(rtfunc1, colinfo, context);
    }
    else
    {
                                               
      get_column_alias_list(colinfo, context);
    }

                                                    
    if (rte->rtekind == RTE_RELATION && rte->tablesample)
    {
      get_tablesample_def(rte->tablesample, context);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;
    deparse_columns *colinfo = deparse_columns_fetch(j->rtindex, dpns);
    bool need_paren_on_right;

    need_paren_on_right = PRETTY_PAREN(context) && !IsA(j->rarg, RangeTblRef) && !(IsA(j->rarg, JoinExpr) && ((JoinExpr *)j->rarg)->alias != NULL);

    if (!PRETTY_PAREN(context) || j->alias != NULL)
    {
      appendStringInfoChar(buf, '(');
    }

    get_from_clause_item(j->larg, query, context);

    switch (j->jointype)
    {
    case JOIN_INNER:
      if (j->quals)
      {
        appendContextKeyword(context, " JOIN ", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_JOIN);
      }
      else
      {
        appendContextKeyword(context, " CROSS JOIN ", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_JOIN);
      }
      break;
    case JOIN_LEFT:
      appendContextKeyword(context, " LEFT JOIN ", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_JOIN);
      break;
    case JOIN_FULL:
      appendContextKeyword(context, " FULL JOIN ", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_JOIN);
      break;
    case JOIN_RIGHT:
      appendContextKeyword(context, " RIGHT JOIN ", -PRETTYINDENT_STD, PRETTYINDENT_STD, PRETTYINDENT_JOIN);
      break;
    default:
      elog(ERROR, "unrecognized join type: %d", (int)j->jointype);
    }

    if (need_paren_on_right)
    {
      appendStringInfoChar(buf, '(');
    }
    get_from_clause_item(j->rarg, query, context);
    if (need_paren_on_right)
    {
      appendStringInfoChar(buf, ')');
    }

    if (j->usingClause)
    {
      ListCell *lc;
      bool first = true;

      appendStringInfoString(buf, " USING (");
                                                             
      foreach (lc, colinfo->usingNames)
      {
        char *colname = (char *)lfirst(lc);

        if (first)
        {
          first = false;
        }
        else
        {
          appendStringInfoString(buf, ", ");
        }
        appendStringInfoString(buf, quote_identifier(colname));
      }
      appendStringInfoChar(buf, ')');
    }
    else if (j->quals)
    {
      appendStringInfoString(buf, " ON ");
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, '(');
      }
      get_rule_expr(j->quals, context, false);
      if (!PRETTY_PAREN(context))
      {
        appendStringInfoChar(buf, ')');
      }
    }
    else if (j->jointype != JOIN_INNER)
    {
                                                                    
      appendStringInfoString(buf, " ON TRUE");
    }

    if (!PRETTY_PAREN(context) || j->alias != NULL)
    {
      appendStringInfoChar(buf, ')');
    }

                                                                  
    if (j->alias != NULL)
    {
         
                                                                       
                                                                         
                                                               
                                                                        
                                               
         
      appendStringInfo(buf, " %s", quote_identifier(get_rtable_name(j->rtindex, context)));
      get_column_alias_list(colinfo, context);
    }
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                                                              
   
                                                               
   
static void
get_column_alias_list(deparse_columns *colinfo, deparse_context *context)
{
  StringInfo buf = context->buf;
  int i;
  bool first = true;

                                         
  if (!colinfo->printaliases)
  {
    return;
  }

  for (i = 0; i < colinfo->num_new_cols; i++)
  {
    char *colname = colinfo->new_colnames[i];

    if (first)
    {
      appendStringInfoChar(buf, '(');
      first = false;
    }
    else
    {
      appendStringInfoString(buf, ", ");
    }
    appendStringInfoString(buf, quote_identifier(colname));
  }
  if (!first)
  {
    appendStringInfoChar(buf, ')');
  }
}

   
                                                                 
   
                                                                         
                                                                           
                                                                           
                                                                             
                                                        
   
                                                                        
                                                                      
   
static void
get_from_clause_coldeflist(RangeTblFunction *rtfunc, deparse_columns *colinfo, deparse_context *context)
{
  StringInfo buf = context->buf;
  ListCell *l1;
  ListCell *l2;
  ListCell *l3;
  ListCell *l4;
  int i;

  appendStringInfoChar(buf, '(');

  i = 0;
  forfour(l1, rtfunc->funccoltypes, l2, rtfunc->funccoltypmods, l3, rtfunc->funccolcollations, l4, rtfunc->funccolnames)
  {
    Oid atttypid = lfirst_oid(l1);
    int32 atttypmod = lfirst_int(l2);
    Oid attcollation = lfirst_oid(l3);
    char *attname;

    if (colinfo)
    {
      attname = colinfo->colnames[i];
    }
    else
    {
      attname = strVal(lfirst(l4));
    }

    Assert(attname);                                            

    if (i > 0)
    {
      appendStringInfoString(buf, ", ");
    }
    appendStringInfo(buf, "%s %s", quote_identifier(attname), format_type_with_typemod(atttypid, atttypmod));
    if (OidIsValid(attcollation) && attcollation != get_typcollation(atttypid))
    {
      appendStringInfo(buf, " COLLATE %s", generate_collation_name(attcollation));
    }

    i++;
  }

  appendStringInfoChar(buf, ')');
}

   
                                                     
   
static void
get_tablesample_def(TableSampleClause *tablesample, deparse_context *context)
{
  StringInfo buf = context->buf;
  Oid argtypes[1];
  int nargs;
  ListCell *l;

     
                                                                     
                                                    
     
  argtypes[0] = INTERNALOID;
  appendStringInfo(buf, " TABLESAMPLE %s (", generate_function_name(tablesample->tsmhandler, 1, NIL, argtypes, false, NULL, EXPR_KIND_NONE));

  nargs = 0;
  foreach (l, tablesample->args)
  {
    if (nargs++ > 0)
    {
      appendStringInfoString(buf, ", ");
    }
    get_rule_expr((Node *)lfirst(l), context, false);
  }
  appendStringInfoChar(buf, ')');

  if (tablesample->repeatable != NULL)
  {
    appendStringInfoString(buf, " REPEATABLE (");
    get_rule_expr((Node *)tablesample->repeatable, context, false);
    appendStringInfoChar(buf, ')');
  }
}

   
                                                              
   
                                                        
   
                                                                    
                                                                 
                                    
   
static void
get_opclass_name(Oid opclass, Oid actual_datatype, StringInfo buf)
{
  HeapTuple ht_opc;
  Form_pg_opclass opcrec;
  char *opcname;
  char *nspname;

  ht_opc = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
  if (!HeapTupleIsValid(ht_opc))
  {
    elog(ERROR, "cache lookup failed for opclass %u", opclass);
  }
  opcrec = (Form_pg_opclass)GETSTRUCT(ht_opc);

  if (!OidIsValid(actual_datatype) || GetDefaultOpClass(actual_datatype, opcrec->opcmethod) != opclass)
  {
                                                                    
    opcname = NameStr(opcrec->opcname);
    if (OpclassIsVisible(opclass))
    {
      appendStringInfo(buf, " %s", quote_identifier(opcname));
    }
    else
    {
      nspname = get_namespace_name(opcrec->opcnamespace);
      appendStringInfo(buf, " %s.%s", quote_identifier(nspname), quote_identifier(opcname));
    }
  }
  ReleaseSysCache(ht_opc);
}

   
                                                                   
   
                                                                              
                                                                        
                                                                          
                                                                          
          
   
                                                    
   
static Node *
processIndirection(Node *node, deparse_context *context)
{
  StringInfo buf = context->buf;
  CoerceToDomain *cdomain = NULL;

  for (;;)
  {
    if (node == NULL)
    {
      break;
    }
    if (IsA(node, FieldStore))
    {
      FieldStore *fstore = (FieldStore *)node;
      Oid typrelid;
      char *fieldname;

                             
      typrelid = get_typ_typrelid(fstore->resulttype);
      if (!OidIsValid(typrelid))
      {
        elog(ERROR, "argument type %s of FieldStore is not a tuple type", format_type_be(fstore->resulttype));
      }

         
                                                                         
                                                                    
                                                                       
         
      Assert(list_length(fstore->fieldnums) == 1);
      fieldname = get_attname(typrelid, linitial_int(fstore->fieldnums), false);
      appendStringInfo(buf, ".%s", quote_identifier(fieldname));

         
                                                                        
                                         
         
      node = (Node *)linitial(fstore->newvals);
    }
    else if (IsA(node, SubscriptingRef))
    {
      SubscriptingRef *sbsref = (SubscriptingRef *)node;

      if (sbsref->refassgnexpr == NULL)
      {
        break;
      }

      printSubscripts(sbsref, context);

         
                                                                         
                                            
         
      node = (Node *)sbsref->refassgnexpr;
    }
    else if (IsA(node, CoerceToDomain))
    {
      cdomain = (CoerceToDomain *)node;
                                                           
      if (cdomain->coercionformat != COERCE_IMPLICIT_CAST)
      {
        break;
      }
                                                       
      node = (Node *)cdomain->arg;
    }
    else
    {
      break;
    }
  }

     
                                                                            
                                                                         
                                                                          
                                                            
     
  if (cdomain && node == (Node *)cdomain->arg)
  {
    node = (Node *)cdomain;
  }

  return node;
}

static void
printSubscripts(SubscriptingRef *sbsref, deparse_context *context)
{
  StringInfo buf = context->buf;
  ListCell *lowlist_item;
  ListCell *uplist_item;

  lowlist_item = list_head(sbsref->reflowerindexpr);                    
  foreach (uplist_item, sbsref->refupperindexpr)
  {
    appendStringInfoChar(buf, '[');
    if (lowlist_item)
    {
                                                                  
      get_rule_expr((Node *)lfirst(lowlist_item), context, false);
      appendStringInfoChar(buf, ':');
      lowlist_item = lnext(lowlist_item);
    }
                                                                
    get_rule_expr((Node *)lfirst(uplist_item), context, false);
    appendStringInfoChar(buf, ']');
  }
}

   
                                                           
   
                                                                  
                                                               
   
const char *
quote_identifier(const char *ident)
{
     
                                                                             
                                                                            
                                                     
     
  int nquotes = 0;
  bool safe;
  const char *ptr;
  char *result;
  char *optr;

     
                                                                            
                                
     
  safe = ((ident[0] >= 'a' && ident[0] <= 'z') || ident[0] == '_');

  for (ptr = ident; *ptr; ptr++)
  {
    char ch = *ptr;

    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || (ch == '_'))
    {
                
    }
    else
    {
      safe = false;
      if (ch == '"')
      {
        nquotes++;
      }
    }
  }

  if (quote_all_identifiers)
  {
    safe = false;
  }

  if (safe)
  {
       
                                                                         
                                                                          
                                                                        
       
                                                                       
                                                                  
       
    int kwnum = ScanKeywordLookup(ident, &ScanKeywords);

    if (kwnum >= 0 && ScanKeywordCategories[kwnum] != UNRESERVED_KEYWORD)
    {
      safe = false;
    }
  }

  if (safe)
  {
    return ident;                       
  }

  result = (char *)palloc(strlen(ident) + nquotes + 2 + 1);

  optr = result;
  *optr++ = '"';
  for (ptr = ident; *ptr; ptr++)
  {
    char ch = *ptr;

    if (ch == '"')
    {
      *optr++ = '"';
    }
    *optr++ = ch;
  }
  *optr++ = '"';
  *optr = '\0';

  return result;
}

   
                                                                      
   
                                                                         
                                                                          
   
char *
quote_qualified_identifier(const char *qualifier, const char *ident)
{
  StringInfoData buf;

  initStringInfo(&buf);
  if (qualifier)
  {
    appendStringInfo(&buf, "%s.", quote_identifier(qualifier));
  }
  appendStringInfoString(&buf, quote_identifier(ident));
  return buf.data;
}

   
                     
                                                            
   
                                                                            
                                                                     
   
static char *
get_relation_name(Oid relid)
{
  char *relname = get_rel_name(relid);

  if (!relname)
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  return relname;
}

   
                          
                                                                
   
                                                                   
   
                                                                          
                                                                        
                                  
   
static char *
generate_relation_name(Oid relid, List *namespaces)
{
  HeapTuple tp;
  Form_pg_class reltup;
  bool need_qual;
  ListCell *nslist;
  char *relname;
  char *nspname;
  char *result;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  reltup = (Form_pg_class)GETSTRUCT(tp);
  relname = NameStr(reltup->relname);

                                      
  need_qual = false;
  foreach (nslist, namespaces)
  {
    deparse_namespace *dpns = (deparse_namespace *)lfirst(nslist);
    ListCell *ctlist;

    foreach (ctlist, dpns->ctes)
    {
      CommonTableExpr *cte = (CommonTableExpr *)lfirst(ctlist);

      if (strcmp(cte->ctename, relname) == 0)
      {
        need_qual = true;
        break;
      }
    }
    if (need_qual)
    {
      break;
    }
  }

                                                                 
  if (!need_qual)
  {
    need_qual = !RelationIsVisible(relid);
  }

  if (need_qual)
  {
    nspname = get_namespace_name(reltup->relnamespace);
  }
  else
  {
    nspname = NULL;
  }

  result = quote_qualified_identifier(nspname, relname);

  ReleaseSysCache(tp);

  return result;
}

   
                                    
                                                                
   
                                                          
   
static char *
generate_qualified_relation_name(Oid relid)
{
  HeapTuple tp;
  Form_pg_class reltup;
  char *relname;
  char *nspname;
  char *result;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  reltup = (Form_pg_class)GETSTRUCT(tp);
  relname = NameStr(reltup->relname);

  nspname = get_namespace_name(reltup->relnamespace);
  if (!nspname)
  {
    elog(ERROR, "cache lookup failed for namespace %u", reltup->relnamespace);
  }

  result = quote_qualified_identifier(nspname, relname);

  ReleaseSysCache(tp);

  return result;
}

   
                          
                                                                 
                                                                          
                                                                           
   
                                                                            
                                                                               
                                                                          
                                                                            
                                                                         
                               
   
                                                                   
   
static char *
generate_function_name(Oid funcid, int nargs, List *argnames, Oid *argtypes, bool has_variadic, bool *use_variadic_p, ParseExprKind special_exprkind)
{
  char *result;
  HeapTuple proctup;
  Form_pg_proc procform;
  char *proname;
  bool use_variadic;
  char *nspname;
  FuncDetailCode p_result;
  Oid p_funcid;
  Oid p_rettype;
  bool p_retset;
  int p_nvargs;
  Oid p_vatype;
  Oid *p_true_typeids;
  bool force_qualify = false;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(proctup);
  proname = NameStr(procform->proname);

     
                                                                            
                                          
     
  if (special_exprkind == EXPR_KIND_GROUP_BY)
  {
    if (strcmp(proname, "cube") == 0 || strcmp(proname, "rollup") == 0)
    {
      force_qualify = true;
    }
  }

     
                                                                          
                                                             
     
                                                                          
                                                                         
                                                                             
                                                                          
                                          
     
  if (use_variadic_p)
  {
                                                                       
    Assert(!has_variadic || OidIsValid(procform->provariadic));
    use_variadic = has_variadic;
    *use_variadic_p = use_variadic;
  }
  else
  {
    Assert(!has_variadic);
    use_variadic = false;
  }

     
                                                                         
                                                                           
                                                                         
                                                                            
              
     
  if (!force_qualify)
  {
    p_result = func_get_detail(list_make1(makeString(proname)), NIL, argnames, nargs, argtypes, !use_variadic, true, &p_funcid, &p_rettype, &p_retset, &p_nvargs, &p_vatype, &p_true_typeids, NULL);
  }
  else
  {
    p_result = FUNCDETAIL_NOTFOUND;
    p_funcid = InvalidOid;
  }

  if ((p_result == FUNCDETAIL_NORMAL || p_result == FUNCDETAIL_AGGREGATE || p_result == FUNCDETAIL_WINDOWFUNC) && p_funcid == funcid)
  {
    nspname = NULL;
  }
  else
  {
    nspname = get_namespace_name(procform->pronamespace);
  }

  result = quote_qualified_identifier(nspname, proname);

  ReleaseSysCache(proctup);

  return result;
}

   
                          
                                                                  
                                                                       
                                                                      
                                                         
   
                                                                   
                                                                          
                     
   
static char *
generate_operator_name(Oid operid, Oid arg1, Oid arg2)
{
  StringInfoData buf;
  HeapTuple opertup;
  Form_pg_operator operform;
  char *oprname;
  char *nspname;
  Operator p_result;

  initStringInfo(&buf);

  opertup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operid));
  if (!HeapTupleIsValid(opertup))
  {
    elog(ERROR, "cache lookup failed for operator %u", operid);
  }
  operform = (Form_pg_operator)GETSTRUCT(opertup);
  oprname = NameStr(operform->oprname);

     
                                                                         
                                                                         
                         
     
  switch (operform->oprkind)
  {
  case 'b':
    p_result = oper(NULL, list_make1(makeString(oprname)), arg1, arg2, true, -1);
    break;
  case 'l':
    p_result = left_oper(NULL, list_make1(makeString(oprname)), arg2, true, -1);
    break;
  case 'r':
    p_result = right_oper(NULL, list_make1(makeString(oprname)), arg1, true, -1);
    break;
  default:
    elog(ERROR, "unrecognized oprkind: %d", operform->oprkind);
    p_result = NULL;                          
    break;
  }

  if (p_result != NULL && oprid(p_result) == operid)
  {
    nspname = NULL;
  }
  else
  {
    nspname = get_namespace_name(operform->oprnamespace);
    appendStringInfo(&buf, "OPERATOR(%s.", quote_identifier(nspname));
  }

  appendStringInfoString(&buf, oprname);

  if (nspname)
  {
    appendStringInfoChar(&buf, ')');
  }

  if (p_result != NULL)
  {
    ReleaseSysCache(p_result);
  }

  ReleaseSysCache(opertup);

  return buf.data;
}

   
                                                                        
   
                                                                         
                                                                   
                                                                         
                                                                          
                                                                       
                                                                         
                                                                         
                                                                         
                                                                           
                                                                            
                                                                          
                                                                            
                                                            
   
void
generate_operator_clause(StringInfo buf, const char *leftop, Oid leftoptype, Oid opoid, const char *rightop, Oid rightoptype)
{
  HeapTuple opertup;
  Form_pg_operator operform;
  char *oprname;
  char *nspname;

  opertup = SearchSysCache1(OPEROID, ObjectIdGetDatum(opoid));
  if (!HeapTupleIsValid(opertup))
  {
    elog(ERROR, "cache lookup failed for operator %u", opoid);
  }
  operform = (Form_pg_operator)GETSTRUCT(opertup);
  Assert(operform->oprkind == 'b');
  oprname = NameStr(operform->oprname);

  nspname = get_namespace_name(operform->oprnamespace);

  appendStringInfoString(buf, leftop);
  if (leftoptype != operform->oprleft)
  {
    add_cast_to(buf, operform->oprleft);
  }
  appendStringInfo(buf, " OPERATOR(%s.", quote_identifier(nspname));
  appendStringInfoString(buf, oprname);
  appendStringInfo(buf, ") %s", rightop);
  if (rightoptype != operform->oprright)
  {
    add_cast_to(buf, operform->oprright);
  }

  ReleaseSysCache(opertup);
}

   
                                                                              
                                                                            
                                                                          
                                                                         
                                                                            
                  
   
static void
add_cast_to(StringInfo buf, Oid typid)
{
  HeapTuple typetup;
  Form_pg_type typform;
  char *typname;
  char *nspname;

  typetup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(typetup))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typform = (Form_pg_type)GETSTRUCT(typetup);

  typname = NameStr(typform->typname);
  nspname = get_namespace_name(typform->typnamespace);

  appendStringInfo(buf, "::%s.%s", quote_identifier(nspname), quote_identifier(typname));

  ReleaseSysCache(typetup);
}

   
                                
                                                            
   
                                                                      
                                                                   
                                                                      
                                                                   
   
static char *
generate_qualified_type_name(Oid typid)
{
  HeapTuple tp;
  Form_pg_type typtup;
  char *typname;
  char *nspname;
  char *result;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typtup = (Form_pg_type)GETSTRUCT(tp);
  typname = NameStr(typtup->typname);

  nspname = get_namespace_name(typtup->typnamespace);
  if (!nspname)
  {
    elog(ERROR, "cache lookup failed for namespace %u", typtup->typnamespace);
  }

  result = quote_qualified_identifier(nspname, typname);

  ReleaseSysCache(tp);

  return result;
}

   
                           
                                                                 
   
                                                                   
   
char *
generate_collation_name(Oid collid)
{
  HeapTuple tp;
  Form_pg_collation colltup;
  char *collname;
  char *nspname;
  char *result;

  tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(collid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for collation %u", collid);
  }
  colltup = (Form_pg_collation)GETSTRUCT(tp);
  collname = NameStr(colltup->collname);

  if (!CollationIsVisible(collid))
  {
    nspname = get_namespace_name(colltup->collnamespace);
  }
  else
  {
    nspname = NULL;
  }

  result = quote_qualified_identifier(nspname, collname);

  ReleaseSysCache(tp);

  return result;
}

   
                                           
   
                                                           
   
static text *
string_to_text(char *str)
{
  text *result;

  result = cstring_to_text(str);
  pfree(str);
  return result;
}

   
                                                                              
   
static char *
flatten_reloptions(Oid relid)
{
  char *result = NULL;
  HeapTuple tuple;
  Datum reloptions;
  bool isnull;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }

  reloptions = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions, &isnull);
  if (!isnull)
  {
    StringInfoData buf;
    Datum *options;
    int noptions;
    int i;

    initStringInfo(&buf);

    deconstruct_array(DatumGetArrayTypeP(reloptions), TEXTOID, -1, false, 'i', &options, NULL, &noptions);

    for (i = 0; i < noptions; i++)
    {
      char *option = TextDatumGetCString(options[i]);
      char *name;
      char *separator;
      char *value;

         
                                                                         
                                                                   
         
      name = option;
      separator = strchr(option, '=');
      if (separator)
      {
        *separator = '\0';
        value = separator + 1;
      }
      else
      {
        value = "";
      }

      if (i > 0)
      {
        appendStringInfoString(&buf, ", ");
      }
      appendStringInfo(&buf, "%s=", quote_identifier(name));

         
                                                                         
                                                                     
                                                                        
                                                                    
                                                                         
                                        
         
      if (quote_identifier(value) == value)
      {
        appendStringInfoString(&buf, value);
      }
      else
      {
        simple_quote_literal(&buf, value);
      }

      pfree(option);
    }

    result = buf.data;
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                        
                                                           
   
char *
get_range_partbound_string(List *bound_datums)
{
  deparse_context context;
  StringInfo buf = makeStringInfo();
  ListCell *cell;
  char *sep;

  memset(&context, 0, sizeof(deparse_context));
  context.buf = buf;

  appendStringInfoString(buf, "(");
  sep = "";
  foreach (cell, bound_datums)
  {
    PartitionRangeDatum *datum = castNode(PartitionRangeDatum, lfirst(cell));

    appendStringInfoString(buf, sep);
    if (datum->kind == PARTITION_RANGE_DATUM_MINVALUE)
    {
      appendStringInfoString(buf, "MINVALUE");
    }
    else if (datum->kind == PARTITION_RANGE_DATUM_MAXVALUE)
    {
      appendStringInfoString(buf, "MAXVALUE");
    }
    else
    {
      Const *val = castNode(Const, datum->value);

      get_const_expr(val, &context, -1);
    }
    sep = ", ";
  }
  appendStringInfoChar(buf, ')');

  return buf->data;
}
