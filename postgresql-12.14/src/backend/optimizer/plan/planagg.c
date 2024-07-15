                                                                            
   
             
                                             
   
                                                                          
               
                         
                                              
                           
              
                                                                       
                                                                       
                                                                   
                                                                   
                                                                    
                             
   
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "parser/parse_clause.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static bool
find_minmax_aggs_walker(Node *node, List **context);
static bool
build_minmax_path(PlannerInfo *root, MinMaxAggInfo *mminfo, Oid eqop, Oid sortop, bool nulls_first);
static void
minmax_qp_callback(PlannerInfo *root, void *extra);
static Oid
fetch_agg_sort_op(Oid aggfnoid);

   
                                                                
   
                                                                            
                                                                            
                                                                          
                                            
   
                                                                              
                                                                       
                                                                         
                                                                               
                         
   
void
preprocess_minmax_aggregates(PlannerInfo *root)
{
  Query *parse = root->parse;
  FromExpr *jtnode;
  RangeTblRef *rtr;
  RangeTblEntry *rte;
  List *aggs_list;
  RelOptInfo *grouped_rel;
  ListCell *lc;

                                                      
  Assert(root->minmax_aggs == NIL);

                                                
  if (!parse->hasAggs)
  {
    return;
  }

  Assert(!parse->setOperations);                                     
  Assert(parse->rowMarks == NIL);                        

     
                                 
     
                                                                
                                                                             
                                                      
     
  if (parse->groupClause || list_length(parse->groupingSets) > 1 || parse->hasWindowFuncs)
  {
    return;
  }

     
                                                                             
                                                                         
                                                                            
     
  if (parse->cteList)
  {
    return;
  }

     
                                                                           
                                                                         
                                                                             
                                                                            
                                                                         
                                                                         
                                            
     
  jtnode = parse->jointree;
  while (IsA(jtnode, FromExpr))
  {
    if (list_length(jtnode->fromlist) != 1)
    {
      return;
    }
    jtnode = linitial(jtnode->fromlist);
  }
  if (!IsA(jtnode, RangeTblRef))
  {
    return;
  }
  rtr = (RangeTblRef *)jtnode;
  rte = planner_rt_fetch(rtr->rtindex, root);
  if (rte->rtekind == RTE_RELATION)
                               ;
  else if (rte->rtekind == RTE_SUBQUERY && rte->inh)
                                          ;
  else
  {
    return;
  }

     
                                                                          
                                                                          
     
  aggs_list = NIL;
  if (find_minmax_aggs_walker((Node *)root->processed_tlist, &aggs_list))
  {
    return;
  }
  if (find_minmax_aggs_walker(parse->havingQual, &aggs_list))
  {
    return;
  }

     
                                                                           
                                                                        
                                                                         
                        
     
  foreach (lc, aggs_list)
  {
    MinMaxAggInfo *mminfo = (MinMaxAggInfo *)lfirst(lc);
    Oid eqop;
    bool reverse;

       
                                                                       
                          
       
    eqop = get_equality_op_for_ordering_op(mminfo->aggsortop, &reverse);
    if (!OidIsValid(eqop))                       
    {
      elog(ERROR, "could not find equality operator for ordering operator %u", mminfo->aggsortop);
    }

       
                                                                        
                                                                 
                                                                     
                                                                      
                                                                 
                                                            
       
    if (build_minmax_path(root, mminfo, eqop, mminfo->aggsortop, reverse))
    {
      continue;
    }
    if (build_minmax_path(root, mminfo, eqop, mminfo->aggsortop, !reverse))
    {
      continue;
    }

                                                       
    return;
  }

     
                                                                          
           
     
                                                                         
                                                                          
                                                                             
                                                          
     
  foreach (lc, aggs_list)
  {
    MinMaxAggInfo *mminfo = (MinMaxAggInfo *)lfirst(lc);

    mminfo->param = SS_make_initplan_output_param(root, exprType((Node *)mminfo->target), -1, exprCollation((Node *)mminfo->target));
  }

     
                                                                          
                                                                             
                                                                         
                                                                
     
                                                                           
                                                                            
                                                                          
                                                                       
                                                                          
                                                                           
                                                                           
     
  grouped_rel = fetch_upper_rel(root, UPPERREL_GROUP_AGG, NULL);
  add_path(grouped_rel, (Path *)create_minmaxagg_path(root, grouped_rel, create_pathtarget(root, root->processed_tlist), aggs_list, (List *)parse->havingQual));
}

   
                           
                                                                       
                                                                      
                                          
   
                                                                      
                                                                              
                                                           
   
                                                                             
                                  
   
                                                                           
                                                                        
                      
   
static bool
find_minmax_aggs_walker(Node *node, List **context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Aggref))
  {
    Aggref *aggref = (Aggref *)node;
    Oid aggsortop;
    TargetEntry *curTarget;
    MinMaxAggInfo *mminfo;
    ListCell *l;

    Assert(aggref->agglevelsup == 0);
    if (list_length(aggref->args) != 1)
    {
      return true;                             
    }

       
                                                                         
                                                                          
                                                                          
                                                                          
                                                                        
                                                                          
                                                                   
                                                                      
                                                                          
                                                                    
                
       
    if (aggref->aggorder != NIL)
    {
      return true;
    }
                                                           

       
                                                                           
                                                                         
                       
       
    if (aggref->aggfilter != NULL)
    {
      return true;
    }

    aggsortop = fetch_agg_sort_op(aggref->aggfnoid);
    if (!OidIsValid(aggsortop))
    {
      return true;                              
    }

    curTarget = (TargetEntry *)linitial(aggref->args);

    if (contain_mutable_functions((Node *)curTarget->expr))
    {
      return true;                                
    }

    if (type_is_rowtype(exprType((Node *)curTarget->expr)))
    {
      return true;                                             
    }

       
                                                                  
       
    foreach (l, *context)
    {
      mminfo = (MinMaxAggInfo *)lfirst(l);
      if (mminfo->aggfnoid == aggref->aggfnoid && equal(mminfo->target, curTarget->expr))
      {
        return false;
      }
    }

    mminfo = makeNode(MinMaxAggInfo);
    mminfo->aggfnoid = aggref->aggfnoid;
    mminfo->aggsortop = aggsortop;
    mminfo->target = curTarget->expr;
    mminfo->subroot = NULL;                             
    mminfo->path = NULL;
    mminfo->pathcost = 0;
    mminfo->param = NULL;

    *context = lappend(*context, mminfo);

       
                                                                         
                   
       
    return false;
  }
  Assert(!IsA(node, SubLink));
  return expression_tree_walker(node, find_minmax_aggs_walker, (void *)context);
}

   
                     
                                                                        
                    
   
                                                                  
                            
   
static bool
build_minmax_path(PlannerInfo *root, MinMaxAggInfo *mminfo, Oid eqop, Oid sortop, bool nulls_first)
{
  PlannerInfo *subroot;
  Query *parse;
  TargetEntry *tle;
  List *tlist;
  NullTest *ntest;
  SortGroupClause *sortcl;
  RelOptInfo *final_rel;
  Path *sorted_path;
  Cost path_cost;
  double path_fraction;

     
                                                                          
                                                                         
                                                                         
                                                                            
                                                                    
     
  subroot = (PlannerInfo *)palloc(sizeof(PlannerInfo));
  memcpy(subroot, root, sizeof(PlannerInfo));
  subroot->query_level++;
  subroot->parent_root = root;
                                   
  subroot->plan_params = NIL;
  subroot->outer_params = NULL;
  subroot->init_plans = NIL;

  subroot->parse = parse = copyObject(root->parse);
  IncrementVarSublevelsUp((Node *)parse, 1, 1);

                                                 
  subroot->append_rel_list = copyObject(root->append_rel_list);
  IncrementVarSublevelsUp((Node *)subroot->append_rel_list, 1, 1);
                                                           
  Assert(subroot->join_info_list == NIL);
                                                       
  Assert(subroot->eq_classes == NIL);
                                                       
  Assert(subroot->placeholder_list == NIL);

               
                                         
                           
                                                
                             
                
               
     
                                                       
  tle = makeTargetEntry(copyObject(mminfo->target), (AttrNumber)1, pstrdup("agg_target"), false);
  tlist = list_make1(tle);
  subroot->processed_tlist = parse->targetList = tlist;

                                                     
  parse->havingQual = NULL;
  subroot->hasHavingQual = false;
  parse->distinctClause = NIL;
  parse->hasDistinctOn = false;
  parse->hasAggs = false;

                                             
  ntest = makeNode(NullTest);
  ntest->nulltesttype = IS_NOT_NULL;
  ntest->arg = copyObject(mminfo->target);
                                                                 
  ntest->argisrow = false;
  ntest->location = -1;

                                                 
  if (!list_member((List *)parse->jointree->quals, ntest))
  {
    parse->jointree->quals = (Node *)lcons(ntest, (List *)parse->jointree->quals);
  }

                                      
  sortcl = makeNode(SortGroupClause);
  sortcl->tleSortGroupRef = assignSortGroupRef(tle, subroot->processed_tlist);
  sortcl->eqop = eqop;
  sortcl->sortop = sortop;
  sortcl->nulls_first = nulls_first;
  sortcl->hashable = false;                                    
  parse->sortClause = list_make1(sortcl);

                                      
  parse->limitOffset = NULL;
  parse->limitCount = (Node *)makeConst(INT8OID, -1, InvalidOid, sizeof(int64), Int64GetDatum(1), false, FLOAT8PASSBYVAL);

     
                                                                           
                   
     
  subroot->tuple_fraction = 1.0;
  subroot->limit_tuples = 1.0;

  final_rel = query_planner(subroot, minmax_qp_callback, NULL);

     
                                                                           
                                                                            
                                                                       
                                                 
     
  SS_identify_outer_params(subroot);
  SS_charge_for_initplans(subroot, final_rel);

     
                                                                         
                                                            
     
  if (final_rel->rows > 1.0)
  {
    path_fraction = 1.0 / final_rel->rows;
  }
  else
  {
    path_fraction = 1.0;
  }

  sorted_path = get_cheapest_fractional_path_for_pathkeys(final_rel->pathlist, subroot->query_pathkeys, NULL, path_fraction);
  if (!sorted_path)
  {
    return false;
  }

     
                                                                       
                                                                       
                     
     
  sorted_path = apply_projection_to_path(subroot, final_rel, sorted_path, create_pathtarget(subroot, subroot->processed_tlist));

     
                                                                     
     
                                              
                                      
     
  path_cost = sorted_path->startup_cost + path_fraction * (sorted_path->total_cost - sorted_path->startup_cost);

                                         
  mminfo->subroot = subroot;
  mminfo->path = sorted_path;
  mminfo->pathcost = path_cost;

  return true;
}

   
                                                                    
   
static void
minmax_qp_callback(PlannerInfo *root, void *extra)
{
  root->group_pathkeys = NIL;
  root->window_pathkeys = NIL;
  root->distinct_pathkeys = NIL;

  root->sort_pathkeys = make_pathkeys_for_sortclauses(root, root->parse->sortClause, root->parse->targetList);

  root->query_pathkeys = root->sort_pathkeys;
}

   
                                                                           
                                                    
   
static Oid
fetch_agg_sort_op(Oid aggfnoid)
{
  HeapTuple aggTuple;
  Form_pg_aggregate aggform;
  Oid aggsortop;

                                               
  aggTuple = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(aggfnoid));
  if (!HeapTupleIsValid(aggTuple))
  {
    return InvalidOid;
  }
  aggform = (Form_pg_aggregate)GETSTRUCT(aggTuple);
  aggsortop = aggform->aggsortop;
  ReleaseSysCache(aggTuple);

  return aggsortop;
}
