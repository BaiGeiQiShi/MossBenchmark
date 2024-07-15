                                                                            
   
               
                            
   
                                                                            
                                                                          
                                                                              
                                          
   
                                                                            
                           
   
                                                                         
                                                                          
                                                                           
                                                                           
                                                                            
                                                                             
                                                                               
   
                                                                            
                                                                            
                                                                             
                                                                             
                                                                            
                                                                      
                                                
   
                                                                       
                                                                            
                                                                             
                                                                           
                                                                            
                                                                        
                                                                          
                                                                         
                                                                             
               
   
                                                                      
                                                                         
                                                                          
                                    
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   
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

   
                                                                            
                                                                     
   
#define IsTransactionStmtPlan(plansource) ((plansource)->raw_parse_tree && IsA((plansource)->raw_parse_tree->stmt, TransactionStmt))

   
                                                                              
                                                                             
                                                                          
                                             
   
static dlist_head saved_plan_list = DLIST_STATIC_INIT(saved_plan_list);

   
                                                                
   
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

                   
int plan_cache_mode;

   
                                                         
   
                                                            
   
void
InitPlanCache(void)
{
  CacheRegisterRelcacheCallback(PlanCacheRelCallback, (Datum)0);
  CacheRegisterSyscacheCallback(PROCOID, PlanCacheObjectCallback, (Datum)0);
  CacheRegisterSyscacheCallback(TYPEOID, PlanCacheObjectCallback, (Datum)0);
  CacheRegisterSyscacheCallback(NAMESPACEOID, PlanCacheSysCallback, (Datum)0);
  CacheRegisterSyscacheCallback(OPEROID, PlanCacheSysCallback, (Datum)0);
  CacheRegisterSyscacheCallback(AMOPOPID, PlanCacheSysCallback, (Datum)0);
  CacheRegisterSyscacheCallback(FOREIGNSERVEROID, PlanCacheSysCallback, (Datum)0);
  CacheRegisterSyscacheCallback(FOREIGNDATAWRAPPEROID, PlanCacheSysCallback, (Datum)0);
}

   
                                                          
   
                                                                             
                                                                            
                                                                          
                                                                             
                                                                         
                                                                           
                                                                         
                                                                  
   
                                                                        
                                                                           
                                                                           
                                                                             
                                                                              
                                                                          
                              
   
                                                                  
                                     
                                                                           
   
CachedPlanSource *
CreateCachedPlan(RawStmt *raw_parse_tree, const char *query_string, const char *commandTag)
{
  CachedPlanSource *plansource;
  MemoryContext source_context;
  MemoryContext oldcxt;

  Assert(query_string != NULL);                         

     
                                                                      
                                                                          
                                                                          
                                                                            
                          
     
  source_context = AllocSetContextCreate(CurrentMemoryContext, "CachedPlanSource", ALLOCSET_START_SMALL_SIZES);

     
                                                                         
                                                     
     
  oldcxt = MemoryContextSwitchTo(source_context);

  plansource = (CachedPlanSource *)palloc0(sizeof(CachedPlanSource));
  plansource->magic = CACHEDPLANSOURCE_MAGIC;
  plansource->raw_parse_tree = copyObject(raw_parse_tree);
  plansource->query_string = pstrdup(query_string);
  MemoryContextSetIdentifier(source_context, plansource->query_string);
  plansource->commandTag = commandTag;
  plansource->param_types = NULL;
  plansource->num_params = 0;
  plansource->parserSetup = NULL;
  plansource->parserSetupArg = NULL;
  plansource->cursor_options = 0;
  plansource->fixed_result = false;
  plansource->resultDesc = NULL;
  plansource->context = source_context;
  plansource->query_list = NIL;
  plansource->relationOids = NIL;
  plansource->invalItems = NIL;
  plansource->search_path = NULL;
  plansource->query_context = NULL;
  plansource->rewriteRoleId = InvalidOid;
  plansource->rewriteRowSecurity = false;
  plansource->dependsOnRLS = false;
  plansource->gplan = NULL;
  plansource->is_oneshot = false;
  plansource->is_complete = false;
  plansource->is_saved = false;
  plansource->is_valid = false;
  plansource->generation = 0;
  plansource->generic_cost = -1;
  plansource->total_custom_cost = 0;
  plansource->num_custom_plans = 0;

  MemoryContextSwitchTo(oldcxt);

  return plansource;
}

   
                                                                          
   
                                                                             
                                                                             
                                                                            
                                                                              
                    
   
                                                                         
                                                                         
                                                                           
                                                                             
   
                                                                  
                                     
                                                                           
   
CachedPlanSource *
CreateOneShotCachedPlan(RawStmt *raw_parse_tree, const char *query_string, const char *commandTag)
{
  CachedPlanSource *plansource;

  Assert(query_string != NULL);                         

     
                                                                            
                                                               
     
  plansource = (CachedPlanSource *)palloc0(sizeof(CachedPlanSource));
  plansource->magic = CACHEDPLANSOURCE_MAGIC;
  plansource->raw_parse_tree = raw_parse_tree;
  plansource->query_string = query_string;
  plansource->commandTag = commandTag;
  plansource->param_types = NULL;
  plansource->num_params = 0;
  plansource->parserSetup = NULL;
  plansource->parserSetupArg = NULL;
  plansource->cursor_options = 0;
  plansource->fixed_result = false;
  plansource->resultDesc = NULL;
  plansource->context = CurrentMemoryContext;
  plansource->query_list = NIL;
  plansource->relationOids = NIL;
  plansource->invalItems = NIL;
  plansource->search_path = NULL;
  plansource->query_context = NULL;
  plansource->rewriteRoleId = InvalidOid;
  plansource->rewriteRowSecurity = false;
  plansource->dependsOnRLS = false;
  plansource->gplan = NULL;
  plansource->is_oneshot = true;
  plansource->is_complete = false;
  plansource->is_saved = false;
  plansource->is_valid = false;
  plansource->generation = 0;
  plansource->generic_cost = -1;
  plansource->total_custom_cost = 0;
  plansource->num_custom_plans = 0;

  return plansource;
}

   
                                                                   
   
                                                                        
                                                                               
                                                                            
                                                                           
                                                                      
   
                                                                               
                                                                              
                                                                            
                                                                         
                                                                            
                                                                               
                                                                             
                                                                            
                                                                               
                                                                            
                                                                       
                                                     
   
                                                                              
                                                                         
                                                                          
                                                     
   
                                                                          
                                                                   
                                                                             
   
                                                      
                                                                              
                                                                
                                                             
                                                                    
                                          
                                                               
                                               
                                                      
                                                                           
   
void
CompleteCachedPlan(CachedPlanSource *plansource, List *querytree_list, MemoryContext querytree_context, Oid *param_types, int num_params, ParserSetupHook parserSetup, void *parserSetupArg, int cursor_options, bool fixed_result)
{
  MemoryContext source_context = plansource->context;
  MemoryContext oldcxt = CurrentMemoryContext;

                                                     
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  Assert(!plansource->is_complete);

     
                                                                        
                                                                          
                                                                          
                                                                        
                              
     
  if (plansource->is_oneshot)
  {
    querytree_context = CurrentMemoryContext;
  }
  else if (querytree_context != NULL)
  {
    MemoryContextSetParent(querytree_context, source_context);
    MemoryContextSwitchTo(querytree_context);
  }
  else
  {
                                                                   
    querytree_context = AllocSetContextCreate(source_context, "CachedPlanQuery", ALLOCSET_START_SMALL_SIZES);
    MemoryContextSwitchTo(querytree_context);
    querytree_list = copyObject(querytree_list);
  }

  plansource->query_context = querytree_context;
  plansource->query_list = querytree_list;

  if (!plansource->is_oneshot && !IsTransactionStmtPlan(plansource))
  {
       
                                                                         
                                                                      
                                                                        
                                                                      
       
    extract_query_dependencies((Node *)querytree_list, &plansource->relationOids, &plansource->invalItems, &plansource->dependsOnRLS);

                                  
    plansource->rewriteRoleId = GetUserId();
    plansource->rewriteRowSecurity = row_security;

       
                                                                      
                                                                           
                                                                        
                                                                       
                                                                
       
    plansource->search_path = GetOverrideSearchPath(querytree_context);
  }

     
                                                                            
                                                                          
                                  
     
  MemoryContextSwitchTo(source_context);

  if (num_params > 0)
  {
    plansource->param_types = (Oid *)palloc(num_params * sizeof(Oid));
    memcpy(plansource->param_types, param_types, num_params * sizeof(Oid));
  }
  else
  {
    plansource->param_types = NULL;
  }
  plansource->num_params = num_params;
  plansource->parserSetup = parserSetup;
  plansource->parserSetupArg = parserSetupArg;
  plansource->cursor_options = cursor_options;
  plansource->fixed_result = fixed_result;
  plansource->resultDesc = PlanCacheComputeResultDesc(querytree_list);

  MemoryContextSwitchTo(oldcxt);

  plansource->is_complete = true;
  plansource->is_valid = true;
}

   
                                                  
   
                                                                             
                                                                             
                                                                            
                        
   
                                                                           
                                                                        
                                                                          
                                                                             
                                                                             
                                       
   
void
SaveCachedPlan(CachedPlanSource *plansource)
{
                                                     
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  Assert(plansource->is_complete);
  Assert(!plansource->is_saved);

                                            
  if (plansource->is_oneshot)
  {
    elog(ERROR, "cannot save one-shot cached plan");
  }

     
                                                                         
                                                                             
                                                                      
                                                                       
                                                                    
     
  ReleaseGenericPlan(plansource);

     
                                                                            
                                                                         
                                       
     
  MemoryContextSetParent(plansource->context, CacheMemoryContext);

     
                                                       
     
  dlist_push_tail(&saved_plan_list, &plansource->node);

  plansource->is_saved = true;
}

   
                                          
   
                                                                               
                                                                         
                                                                          
                 
   
void
DropCachedPlan(CachedPlanSource *plansource)
{
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

                                                   
  if (plansource->is_saved)
  {
    dlist_delete(&plansource->node);
    plansource->is_saved = false;
  }

                                                                            
  ReleaseGenericPlan(plansource);

                               
  plansource->magic = 0;

     
                                                                        
                                                                            
     
  if (!plansource->is_oneshot)
  {
    MemoryContextDelete(plansource->context);
  }
}

   
                                                                          
   
static void
ReleaseGenericPlan(CachedPlanSource *plansource)
{
                                                                      
  if (plansource->gplan)
  {
    CachedPlan *plan = plansource->gplan;

    Assert(plan->magic == CACHEDPLAN_MAGIC);
    plansource->gplan = NULL;
    ReleaseCachedPlan(plan, false);
  }
}

   
                                                                                
   
                                                                             
                                                                            
             
   
                                                                              
                       
   
                                                                             
                                                                             
                                                              
   
static List *
RevalidateCachedQuery(CachedPlanSource *plansource, QueryEnvironment *queryEnv)
{
  bool snapshot_set;
  RawStmt *rawtree;
  List *tlist;                                
  List *qlist;                                
  TupleDesc resultDesc;
  MemoryContext querytree_context;
  MemoryContext oldcxt;

     
                                                                       
                                                                            
                                                                            
                                                                       
                                                             
     
  if (plansource->is_oneshot || IsTransactionStmtPlan(plansource))
  {
    Assert(plansource->is_valid);
    return NIL;
  }

     
                                                                             
                                                                            
                      
     
  if (plansource->is_valid)
  {
    Assert(plansource->search_path != NULL);
    if (!OverrideSearchPathMatchesCurrent(plansource->search_path))
    {
                                                     
      plansource->is_valid = false;
      if (plansource->gplan)
      {
        plansource->gplan->is_valid = false;
      }
    }
  }

     
                                                                            
                                                                    
     
  if (plansource->is_valid && plansource->dependsOnRLS && (plansource->rewriteRoleId != GetUserId() || plansource->rewriteRowSecurity != row_security))
  {
    plansource->is_valid = false;
  }

     
                                                                      
                                                                             
                                                                             
     
  if (plansource->is_valid)
  {
    AcquirePlannerLocks(plansource->query_list, true);

       
                                                                    
                                                     
       
    if (plansource->is_valid)
    {
                                                          
      return NIL;
    }

                                                               
    AcquirePlannerLocks(plansource->query_list, false);
  }

     
                                                                          
                                                                     
                                            
     
  plansource->is_valid = false;
  plansource->query_list = NIL;
  plansource->relationOids = NIL;
  plansource->invalItems = NIL;
  plansource->search_path = NULL;

     
                                                                            
                                                                         
                                                                           
                         
     
  if (plansource->query_context)
  {
    MemoryContext qcxt = plansource->query_context;

    plansource->query_context = NULL;
    MemoryContextDelete(qcxt);
  }

                                              
  ReleaseGenericPlan(plansource);

     
                                                                           
                                              
     
  Assert(plansource->is_complete);

     
                                                                          
                                                                             
                                                                           
                                                                     
              
     
  snapshot_set = false;
  if (!ActiveSnapshotSet())
  {
    PushActiveSnapshot(GetTransactionSnapshot());
    snapshot_set = true;
  }

     
                                                                             
                                                                            
                
     
  rawtree = copyObject(plansource->raw_parse_tree);
  if (rawtree == NULL)
  {
    tlist = NIL;
  }
  else if (plansource->parserSetup != NULL)
  {
    tlist = pg_analyze_and_rewrite_params(rawtree, plansource->query_string, plansource->parserSetup, plansource->parserSetupArg, queryEnv);
  }
  else
  {
    tlist = pg_analyze_and_rewrite(rawtree, plansource->query_string, plansource->param_types, plansource->num_params, queryEnv);
  }

                                      
  if (snapshot_set)
  {
    PopActiveSnapshot();
  }

     
                                                                     
                                            
     
                                                                            
                          
     
  resultDesc = PlanCacheComputeResultDesc(tlist);
  if (resultDesc == NULL && plansource->resultDesc == NULL)
  {
                                   
  }
  else if (resultDesc == NULL || plansource->resultDesc == NULL || !equalTupleDescs(resultDesc, plansource->resultDesc))
  {
                                             
    if (plansource->fixed_result)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cached plan must not change result type")));
    }
    oldcxt = MemoryContextSwitchTo(plansource->context);
    if (resultDesc)
    {
      resultDesc = CreateTupleDescCopy(resultDesc);
    }
    if (plansource->resultDesc)
    {
      FreeTupleDesc(plansource->resultDesc);
    }
    plansource->resultDesc = resultDesc;
    MemoryContextSwitchTo(oldcxt);
  }

     
                                                                          
                                                                             
     
  querytree_context = AllocSetContextCreate(CurrentMemoryContext, "CachedPlanQuery", ALLOCSET_START_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(querytree_context);

  qlist = copyObject(tlist);

     
                                                                          
                                                                            
                 
     
  extract_query_dependencies((Node *)qlist, &plansource->relationOids, &plansource->invalItems, &plansource->dependsOnRLS);

                                
  plansource->rewriteRoleId = GetUserId();
  plansource->rewriteRowSecurity = row_security;

     
                                                                           
                                                                           
                        
     
  plansource->search_path = GetOverrideSearchPath(querytree_context);

  MemoryContextSwitchTo(oldcxt);

                                                                  
  MemoryContextSetParent(querytree_context, plansource->context);

  plansource->query_context = querytree_context;
  plansource->query_list = qlist;

     
                                                                          
                                                                           
                                                                           
                                                                            
                                                                          
                     
     

  plansource->is_valid = true;

                                                                        
  return tlist;
}

   
                                                                         
   
                                                                            
                            
   
                                                                          
                                                                      
   
static bool
CheckCachedPlan(CachedPlanSource *plansource)
{
  CachedPlan *plan = plansource->gplan;

                                                
  Assert(plansource->is_valid);

                                                    
  if (!plan)
  {
    return false;
  }

  Assert(plan->magic == CACHEDPLAN_MAGIC);
                                        
  Assert(!plan->is_oneshot);

     
                                                            
     
  if (plan->is_valid && plan->dependsOnRole && plan->planRoleId != GetUserId())
  {
    plan->is_valid = false;
  }

     
                                                                           
                                                        
     
  if (plan->is_valid)
  {
       
                                                                    
                                                                   
       
    Assert(plan->refcount > 0);

    AcquireExecutorLocks(plan->stmt_list, true);

       
                                                                  
                                          
       
    if (plan->is_valid && TransactionIdIsValid(plan->saved_xmin) && !TransactionIdEquals(plan->saved_xmin, TransactionXmin))
    {
      plan->is_valid = false;
    }

       
                                                                    
                                                    
       
    if (plan->is_valid)
    {
                                                          
      return true;
    }

                                                               
    AcquireExecutorLocks(plan->stmt_list, false);
  }

     
                                                                             
     
  ReleaseGenericPlan(plansource);

  return false;
}

   
                                                                        
   
                                                                           
                                                                              
   
                                                                       
                                                                              
                                                                             
                                                                         
                                     
   
                                                                            
                                                                       
                                                                           
   
static CachedPlan *
BuildCachedPlan(CachedPlanSource *plansource, List *qlist, ParamListInfo boundParams, QueryEnvironment *queryEnv)
{
  CachedPlan *plan;
  List *plist;
  bool snapshot_set;
  bool is_transient;
  MemoryContext plan_context;
  MemoryContext oldcxt = CurrentMemoryContext;
  ListCell *lc;

     
                                                                      
                 
     
                                                                            
                                                                          
                                                                     
                                                                        
                                                                            
                                                                            
                                                                         
                                                                             
     
  if (!plansource->is_valid)
  {
    qlist = RevalidateCachedQuery(plansource, queryEnv);
  }

     
                                                                       
                                                                            
                                                       
     
  if (qlist == NIL)
  {
    if (!plansource->is_oneshot)
    {
      qlist = copyObject(plansource->query_list);
    }
    else
    {
      qlist = plansource->query_list;
    }
  }

     
                                                                          
                                                                   
     
  snapshot_set = false;
  if (!ActiveSnapshotSet() && plansource->raw_parse_tree && analyze_requires_snapshot(plansource->raw_parse_tree))
  {
    PushActiveSnapshot(GetTransactionSnapshot());
    snapshot_set = true;
  }

     
                        
     
  plist = pg_plan_queries(qlist, plansource->cursor_options, boundParams);

                                      
  if (snapshot_set)
  {
    PopActiveSnapshot();
  }

     
                                                                            
                                                                         
                                                                             
                                                                       
     
  if (!plansource->is_oneshot)
  {
    plan_context = AllocSetContextCreate(CurrentMemoryContext, "CachedPlan", ALLOCSET_START_SMALL_SIZES);
    MemoryContextCopyAndSetIdentifier(plan_context, plansource->query_string);

       
                                       
       
    MemoryContextSwitchTo(plan_context);

    plist = copyObject(plist);
  }
  else
  {
    plan_context = CurrentMemoryContext;
  }

     
                                                                   
     
  plan = (CachedPlan *)palloc(sizeof(CachedPlan));
  plan->magic = CACHEDPLAN_MAGIC;
  plan->stmt_list = plist;

     
                                                                        
                                                                           
                                         
     
  plan->planRoleId = GetUserId();
  plan->dependsOnRole = plansource->dependsOnRLS;
  is_transient = false;
  foreach (lc, plist)
  {
    PlannedStmt *plannedstmt = lfirst_node(PlannedStmt, lc);

    if (plannedstmt->commandType == CMD_UTILITY)
    {
      continue;                                
    }

    if (plannedstmt->transientPlan)
    {
      is_transient = true;
    }
    if (plannedstmt->dependsOnRole)
    {
      plan->dependsOnRole = true;
    }
  }
  if (is_transient)
  {
    Assert(TransactionIdIsNormal(TransactionXmin));
    plan->saved_xmin = TransactionXmin;
  }
  else
  {
    plan->saved_xmin = InvalidTransactionId;
  }
  plan->refcount = 0;
  plan->context = plan_context;
  plan->is_oneshot = plansource->is_oneshot;
  plan->is_saved = false;
  plan->is_valid = true;

                                            
  plan->generation = ++(plansource->generation);

  MemoryContextSwitchTo(oldcxt);

  return plan;
}

   
                                                                    
   
                                                      
   
static bool
choose_custom_plan(CachedPlanSource *plansource, ParamListInfo boundParams)
{
  double avg_custom_cost;

                                                       
  if (plansource->is_oneshot)
  {
    return true;
  }

                                                                            
  if (boundParams == NULL)
  {
    return false;
  }
                                                  
  if (IsTransactionStmtPlan(plansource))
  {
    return false;
  }

                                       
  if (plan_cache_mode == PLAN_CACHE_MODE_FORCE_GENERIC_PLAN)
  {
    return false;
  }
  if (plan_cache_mode == PLAN_CACHE_MODE_FORCE_CUSTOM_PLAN)
  {
    return true;
  }

                                                 
  if (plansource->cursor_options & CURSOR_OPT_GENERIC_PLAN)
  {
    return false;
  }
  if (plansource->cursor_options & CURSOR_OPT_CUSTOM_PLAN)
  {
    return true;
  }

                                                                       
  if (plansource->num_custom_plans < 5)
  {
    return true;
  }

  avg_custom_cost = plansource->total_custom_cost / plansource->num_custom_plans;

     
                                                                        
                                                                     
                                                                        
                                                                      
             
     
                                                                          
                                                                        
     
  if (plansource->generic_cost < avg_custom_cost)
  {
    return false;
  }

  return true;
}

   
                                                        
   
                                                                               
                                                                             
                                          
   
static double
cached_plan_cost(CachedPlan *plan, bool include_planner)
{
  double result = 0;
  ListCell *lc;

  foreach (lc, plan->stmt_list)
  {
    PlannedStmt *plannedstmt = lfirst_node(PlannedStmt, lc);

    if (plannedstmt->commandType == CMD_UTILITY)
    {
      continue;                                
    }

    result += plannedstmt->planTree->total_cost;

    if (include_planner)
    {
         
                                                                         
                                                                       
                                                                       
                                                                         
                                                                         
                                                                    
                                                                       
                                                                   
                                                                       
                                                
         
                                                                      
                                                                      
                                                                        
                                                                        
                                     
         
                                                                     
                                                           
         
      int nrelations = list_length(plannedstmt->rtable);

      result += 1000.0 * cpu_operator_cost * (nrelations + 1);
    }
  }

  return result;
}

   
                                                             
   
                                                                       
                                                                            
                      
   
                                                                      
              
   
                                                                     
                                                                         
                                                                           
                                                     
   
                                                                             
                          
   
CachedPlan *
GetCachedPlan(CachedPlanSource *plansource, ParamListInfo boundParams, bool useResOwner, QueryEnvironment *queryEnv)
{
  CachedPlan *plan = NULL;
  List *qlist;
  bool customplan;

                                                     
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  Assert(plansource->is_complete);
                                            
  if (useResOwner && !plansource->is_saved)
  {
    elog(ERROR, "cannot apply ResourceOwner to non-saved cached plan");
  }

                                                                          
  qlist = RevalidateCachedQuery(plansource, queryEnv);

                                           
  customplan = choose_custom_plan(plansource, boundParams);

  if (!customplan)
  {
    if (CheckCachedPlan(plansource))
    {
                                                                   
      plan = plansource->gplan;
      Assert(plan->magic == CACHEDPLAN_MAGIC);
    }
    else
    {
                                    
      plan = BuildCachedPlan(plansource, qlist, NULL, queryEnv);
                                                          
      ReleaseGenericPlan(plansource);
                                                         
      plansource->gplan = plan;
      plan->refcount++;
                                                         
      if (plansource->is_saved)
      {
                                                           
        MemoryContextSetParent(plan->context, CacheMemoryContext);
        plan->is_saved = true;
      }
      else
      {
                                                                 
        MemoryContextSetParent(plan->context, MemoryContextGetParent(plansource->context));
      }
                                                                   
      plansource->generic_cost = cached_plan_cost(plan, false);

         
                                                                         
                                                                        
                                                                    
                                                                       
                                                                         
                                                                       
               
         
      customplan = choose_custom_plan(plansource, boundParams);

         
                                                                        
                                                                   
                                                    
         
      qlist = NIL;
    }
  }

  if (customplan)
  {
                             
    plan = BuildCachedPlan(plansource, qlist, boundParams, queryEnv);
                                                                    
    if (plansource->num_custom_plans < INT_MAX)
    {
      plansource->total_custom_cost += cached_plan_cost(plan, true);
      plansource->num_custom_plans++;
    }
  }

  Assert(plan != NULL);

                                         
  if (useResOwner)
  {
    ResourceOwnerEnlargePlanCacheRefs(CurrentResourceOwner);
  }
  plan->refcount++;
  if (useResOwner)
  {
    ResourceOwnerRememberPlanCacheRef(CurrentResourceOwner, plan);
  }

     
                                                                             
                                                                             
                                                                           
                                            
     
  if (customplan && plansource->is_saved)
  {
    MemoryContextSetParent(plan->context, CacheMemoryContext);
    plan->is_saved = true;
  }

  return plan;
}

   
                                                           
   
                                                                        
                                                                         
                                                               
   
                                                                          
                                                                        
                                                                          
   
void
ReleaseCachedPlan(CachedPlan *plan, bool useResOwner)
{
  Assert(plan->magic == CACHEDPLAN_MAGIC);
  if (useResOwner)
  {
    Assert(plan->is_saved);
    ResourceOwnerForgetPlanCacheRef(CurrentResourceOwner, plan);
  }
  Assert(plan->refcount > 0);
  plan->refcount--;
  if (plan->refcount == 0)
  {
                                 
    plan->magic = 0;

                                                                        
    if (!plan->is_oneshot)
    {
      MemoryContextDelete(plan->context);
    }
  }
}

   
                                                                               
   
                                                                        
                                        
   
void
CachedPlanSetParentContext(CachedPlanSource *plansource, MemoryContext newcontext)
{
                                                     
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  Assert(plansource->is_complete);

                                           
  if (plansource->is_saved)
  {
    elog(ERROR, "cannot move a saved cached plan to another context");
  }
  if (plansource->is_oneshot)
  {
    elog(ERROR, "cannot move a one-shot cached plan to another context");
  }

                                                        
  MemoryContextSetParent(plansource->context, newcontext);

     
                                                                        
                                                                       
                                                     
     
  if (plansource->gplan)
  {
    Assert(plansource->gplan->magic == CACHEDPLAN_MAGIC);
    MemoryContextSetParent(plansource->gplan->context, newcontext);
  }
}

   
                                                     
   
                                                             
                                                                       
                                                                          
                                                                           
                                                                       
   
CachedPlanSource *
CopyCachedPlan(CachedPlanSource *plansource)
{
  CachedPlanSource *newsource;
  MemoryContext source_context;
  MemoryContext querytree_context;
  MemoryContext oldcxt;

  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  Assert(plansource->is_complete);

     
                                                                        
                                                                           
     
  if (plansource->is_oneshot)
  {
    elog(ERROR, "cannot copy a one-shot cached plan");
  }

  source_context = AllocSetContextCreate(CurrentMemoryContext, "CachedPlanSource", ALLOCSET_START_SMALL_SIZES);

  oldcxt = MemoryContextSwitchTo(source_context);

  newsource = (CachedPlanSource *)palloc0(sizeof(CachedPlanSource));
  newsource->magic = CACHEDPLANSOURCE_MAGIC;
  newsource->raw_parse_tree = copyObject(plansource->raw_parse_tree);
  newsource->query_string = pstrdup(plansource->query_string);
  MemoryContextSetIdentifier(source_context, newsource->query_string);
  newsource->commandTag = plansource->commandTag;
  if (plansource->num_params > 0)
  {
    newsource->param_types = (Oid *)palloc(plansource->num_params * sizeof(Oid));
    memcpy(newsource->param_types, plansource->param_types, plansource->num_params * sizeof(Oid));
  }
  else
  {
    newsource->param_types = NULL;
  }
  newsource->num_params = plansource->num_params;
  newsource->parserSetup = plansource->parserSetup;
  newsource->parserSetupArg = plansource->parserSetupArg;
  newsource->cursor_options = plansource->cursor_options;
  newsource->fixed_result = plansource->fixed_result;
  if (plansource->resultDesc)
  {
    newsource->resultDesc = CreateTupleDescCopy(plansource->resultDesc);
  }
  else
  {
    newsource->resultDesc = NULL;
  }
  newsource->context = source_context;

  querytree_context = AllocSetContextCreate(source_context, "CachedPlanQuery", ALLOCSET_START_SMALL_SIZES);
  MemoryContextSwitchTo(querytree_context);
  newsource->query_list = copyObject(plansource->query_list);
  newsource->relationOids = copyObject(plansource->relationOids);
  newsource->invalItems = copyObject(plansource->invalItems);
  if (plansource->search_path)
  {
    newsource->search_path = CopyOverrideSearchPath(plansource->search_path);
  }
  newsource->query_context = querytree_context;
  newsource->rewriteRoleId = plansource->rewriteRoleId;
  newsource->rewriteRowSecurity = plansource->rewriteRowSecurity;
  newsource->dependsOnRLS = plansource->dependsOnRLS;

  newsource->gplan = NULL;

  newsource->is_oneshot = false;
  newsource->is_complete = true;
  newsource->is_saved = false;
  newsource->is_valid = plansource->is_valid;
  newsource->generation = plansource->generation;

                                                       
  newsource->generic_cost = plansource->generic_cost;
  newsource->total_custom_cost = plansource->total_custom_cost;
  newsource->num_custom_plans = plansource->num_custom_plans;

  MemoryContextSwitchTo(oldcxt);

  return newsource;
}

   
                                                                    
                                                                             
                     
   
                                                                      
                                                                        
   
bool
CachedPlanIsValid(CachedPlanSource *plansource)
{
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  return plansource->is_valid;
}

   
                                                                           
   
                                                                      
                                                                            
   
List *
CachedPlanGetTargetList(CachedPlanSource *plansource, QueryEnvironment *queryEnv)
{
  Query *pstmt;

                                                     
  Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
  Assert(plansource->is_complete);

     
                                                                       
                                                   
     
  if (plansource->resultDesc == NULL)
  {
    return NIL;
  }

                                                                          
  RevalidateCachedQuery(plansource, queryEnv);

                                                              
  pstmt = QueryListGetPrimaryStmt(plansource->query_list);

  return FetchStatementTargetList((Node *)pstmt);
}

   
                                                                        
   
                                                               
                                                                       
                                                 
   
                                                                  
                                                                    
                                                                   
   
CachedExpression *
GetCachedExpression(Node *expr)
{
  CachedExpression *cexpr;
  List *relationOids;
  List *invalItems;
  MemoryContext cexpr_context;
  MemoryContext oldcxt;

     
                                                                        
                                                                     
                                                                       
     
  expr = (Node *)expression_planner_with_deps((Expr *)expr, &relationOids, &invalItems);

     
                                                                         
                                                                          
                                                            
     
  cexpr_context = AllocSetContextCreate(CurrentMemoryContext, "CachedExpression", ALLOCSET_SMALL_SIZES);

  oldcxt = MemoryContextSwitchTo(cexpr_context);

  cexpr = (CachedExpression *)palloc(sizeof(CachedExpression));
  cexpr->magic = CACHEDEXPR_MAGIC;
  cexpr->expr = copyObject(expr);
  cexpr->is_valid = true;
  cexpr->relationOids = copyObject(relationOids);
  cexpr->invalItems = copyObject(invalItems);
  cexpr->context = cexpr_context;

  MemoryContextSwitchTo(oldcxt);

     
                                                                            
                             
     
  MemoryContextSetParent(cexpr_context, CacheMemoryContext);

     
                                                             
     
  dlist_push_tail(&cached_expression_list, &cexpr->node);

  return cexpr;
}

   
                        
                               
   
void
FreeCachedExpression(CachedExpression *cexpr)
{
                    
  Assert(cexpr->magic == CACHEDEXPR_MAGIC);
                               
  dlist_delete(&cexpr->node);
                                                         
  MemoryContextDelete(cexpr->context);
}

   
                           
                                                                        
   
                                                                          
                                                                           
                                             
   
static Query *
QueryListGetPrimaryStmt(List *stmts)
{
  ListCell *lc;

  foreach (lc, stmts)
  {
    Query *stmt = lfirst_node(Query, lc);

    if (stmt->canSetTag)
    {
      return stmt;
    }
  }
  return NULL;
}

   
                                                                              
                                        
   
static void
AcquireExecutorLocks(List *stmt_list, bool acquire)
{
  ListCell *lc1;

  foreach (lc1, stmt_list)
  {
    PlannedStmt *plannedstmt = lfirst_node(PlannedStmt, lc1);
    ListCell *lc2;

    if (plannedstmt->commandType == CMD_UTILITY)
    {
         
                                                                        
                                                                         
                                                                      
                                                                    
                         
         
      Query *query = UtilityContainsQuery(plannedstmt->utilityStmt);

      if (query)
      {
        ScanQueryForLocks(query, acquire);
      }
      continue;
    }

    foreach (lc2, plannedstmt->rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc2);

      if (rte->rtekind != RTE_RELATION)
      {
        continue;
      }

         
                                                                         
                                                                        
                                                                       
                                         
         
      if (acquire)
      {
        LockRelationOid(rte->relid, rte->rellockmode);
      }
      else
      {
        UnlockRelationOid(rte->relid, rte->rellockmode);
      }
    }
  }
}

   
                                                                               
                                        
   
                                                                             
                                                                            
                           
   
static void
AcquirePlannerLocks(List *stmt_list, bool acquire)
{
  ListCell *lc;

  foreach (lc, stmt_list)
  {
    Query *query = lfirst_node(Query, lc);

    if (query->commandType == CMD_UTILITY)
    {
                                                                  
      query = UtilityContainsQuery(query->utilityStmt);
      if (query)
      {
        ScanQueryForLocks(query, acquire);
      }
      continue;
    }

    ScanQueryForLocks(query, acquire);
  }
}

   
                                                                          
   
static void
ScanQueryForLocks(Query *parsetree, bool acquire)
{
  ListCell *lc;

                                                
  Assert(parsetree->commandType != CMD_UTILITY);

     
                                                     
     
  foreach (lc, parsetree->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

    switch (rte->rtekind)
    {
    case RTE_RELATION:
                                                           
      if (acquire)
      {
        LockRelationOid(rte->relid, rte->rellockmode);
      }
      else
      {
        UnlockRelationOid(rte->relid, rte->rellockmode);
      }
      break;

    case RTE_SUBQUERY:
                                         
      ScanQueryForLocks(rte->subquery, acquire);
      break;

    default:
                                      
      break;
    }
  }

                                     
  foreach (lc, parsetree->cteList)
  {
    CommonTableExpr *cte = lfirst_node(CommonTableExpr, lc);

    ScanQueryForLocks(castNode(Query, cte->ctequery), acquire);
  }

     
                                                                           
                             
     
  if (parsetree->hasSubLinks)
  {
    query_tree_walker(parsetree, ScanQueryWalker, (void *)&acquire, QTW_IGNORE_RC_SUBQUERIES);
  }
}

   
                                                           
   
static bool
ScanQueryWalker(Node *node, bool *acquire)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, SubLink))
  {
    SubLink *sub = (SubLink *)node;

                             
    ScanQueryForLocks(castNode(Query, sub->subselect), *acquire);
                                                          
  }

     
                                                                        
                                                
     
  return expression_tree_walker(node, ScanQueryWalker, (void *)acquire);
}

   
                                                                               
                                                                        
                                     
   
                                                                      
   
static TupleDesc
PlanCacheComputeResultDesc(List *stmt_list)
{
  Query *query;

  switch (ChoosePortalStrategy(stmt_list))
  {
  case PORTAL_ONE_SELECT:
  case PORTAL_ONE_MOD_WITH:
    query = linitial_node(Query, stmt_list);
    return ExecCleanTypeFromTL(query->targetList);

  case PORTAL_ONE_RETURNING:
    query = QueryListGetPrimaryStmt(stmt_list);
    Assert(query->returningList);
    return ExecCleanTypeFromTL(query->returningList);

  case PORTAL_UTIL_SELECT:
    query = linitial_node(Query, stmt_list);
    Assert(query->utilityStmt);
    return UtilityTupleDescriptor(query->utilityStmt);

  case PORTAL_MULTI_QUERY:
                                
    break;
  }
  return NULL;
}

   
                        
                                     
   
                                                                          
                                          
   
static void
PlanCacheRelCallback(Datum arg, Oid relid)
{
  dlist_iter iter;

  dlist_foreach(iter, &saved_plan_list)
  {
    CachedPlanSource *plansource = dlist_container(CachedPlanSource, node, iter.cur);

    Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

                                             
    if (!plansource->is_valid)
    {
      continue;
    }

                                                       
    if (IsTransactionStmtPlan(plansource))
    {
      continue;
    }

       
                                                              
       
    if ((relid == InvalidOid) ? plansource->relationOids != NIL : list_member_oid(plansource->relationOids, relid))
    {
                                                     
      plansource->is_valid = false;
      if (plansource->gplan)
      {
        plansource->gplan->is_valid = false;
      }
    }

       
                                                                       
                                                   
       
    if (plansource->gplan && plansource->gplan->is_valid)
    {
      ListCell *lc;

      foreach (lc, plansource->gplan->stmt_list)
      {
        PlannedStmt *plannedstmt = lfirst_node(PlannedStmt, lc);

        if (plannedstmt->commandType == CMD_UTILITY)
        {
          continue;                                
        }
        if ((relid == InvalidOid) ? plannedstmt->relationOids != NIL : list_member_oid(plannedstmt->relationOids, relid))
        {
                                                
          plansource->gplan->is_valid = false;
          break;                            
        }
      }
    }
  }

                                         
  dlist_foreach(iter, &cached_expression_list)
  {
    CachedExpression *cexpr = dlist_container(CachedExpression, node, iter.cur);

    Assert(cexpr->magic == CACHEDEXPR_MAGIC);

                                             
    if (!cexpr->is_valid)
    {
      continue;
    }

    if ((relid == InvalidOid) ? cexpr->relationOids != NIL : list_member_oid(cexpr->relationOids, relid))
    {
      cexpr->is_valid = false;
    }
  }
}

   
                           
                                                                    
   
                                                                             
                                                                       
   
static void
PlanCacheObjectCallback(Datum arg, int cacheid, uint32 hashvalue)
{
  dlist_iter iter;

  dlist_foreach(iter, &saved_plan_list)
  {
    CachedPlanSource *plansource = dlist_container(CachedPlanSource, node, iter.cur);
    ListCell *lc;

    Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

                                             
    if (!plansource->is_valid)
    {
      continue;
    }

                                                       
    if (IsTransactionStmtPlan(plansource))
    {
      continue;
    }

       
                                                              
       
    foreach (lc, plansource->invalItems)
    {
      PlanInvalItem *item = (PlanInvalItem *)lfirst(lc);

      if (item->cacheId != cacheid)
      {
        continue;
      }
      if (hashvalue == 0 || item->hashValue == hashvalue)
      {
                                                       
        plansource->is_valid = false;
        if (plansource->gplan)
        {
          plansource->gplan->is_valid = false;
        }
        break;
      }
    }

       
                                                                       
                                                   
       
    if (plansource->gplan && plansource->gplan->is_valid)
    {
      foreach (lc, plansource->gplan->stmt_list)
      {
        PlannedStmt *plannedstmt = lfirst_node(PlannedStmt, lc);
        ListCell *lc3;

        if (plannedstmt->commandType == CMD_UTILITY)
        {
          continue;                                
        }
        foreach (lc3, plannedstmt->invalItems)
        {
          PlanInvalItem *item = (PlanInvalItem *)lfirst(lc3);

          if (item->cacheId != cacheid)
          {
            continue;
          }
          if (hashvalue == 0 || item->hashValue == hashvalue)
          {
                                                  
            plansource->gplan->is_valid = false;
            break;                             
          }
        }
        if (!plansource->gplan->is_valid)
        {
          break;                            
        }
      }
    }
  }

                                         
  dlist_foreach(iter, &cached_expression_list)
  {
    CachedExpression *cexpr = dlist_container(CachedExpression, node, iter.cur);
    ListCell *lc;

    Assert(cexpr->magic == CACHEDEXPR_MAGIC);

                                             
    if (!cexpr->is_valid)
    {
      continue;
    }

    foreach (lc, cexpr->invalItems)
    {
      PlanInvalItem *item = (PlanInvalItem *)lfirst(lc);

      if (item->cacheId != cacheid)
      {
        continue;
      }
      if (hashvalue == 0 || item->hashValue == hashvalue)
      {
        cexpr->is_valid = false;
        break;
      }
    }
  }
}

   
                        
                                                      
   
                                 
   
static void
PlanCacheSysCallback(Datum arg, int cacheid, uint32 hashvalue)
{
  ResetPlanCache();
}

   
                                                
   
void
ResetPlanCache(void)
{
  dlist_iter iter;

  dlist_foreach(iter, &saved_plan_list)
  {
    CachedPlanSource *plansource = dlist_container(CachedPlanSource, node, iter.cur);
    ListCell *lc;

    Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

                                             
    if (!plansource->is_valid)
    {
      continue;
    }

       
                                                                     
                                                                          
                                                                          
       
    if (IsTransactionStmtPlan(plansource))
    {
      continue;
    }

       
                                                                       
                                                                     
                                                                          
                                                                      
                      
       
    foreach (lc, plansource->query_list)
    {
      Query *query = lfirst_node(Query, lc);

      if (query->commandType != CMD_UTILITY || UtilityContainsQuery(query->utilityStmt))
      {
                                                  
        plansource->is_valid = false;
        if (plansource->gplan)
        {
          plansource->gplan->is_valid = false;
        }
                                     
        break;
      }
    }
  }

                                              
  dlist_foreach(iter, &cached_expression_list)
  {
    CachedExpression *cexpr = dlist_container(CachedExpression, node, iter.cur);

    Assert(cexpr->magic == CACHEDEXPR_MAGIC);

    cexpr->is_valid = false;
  }
}
