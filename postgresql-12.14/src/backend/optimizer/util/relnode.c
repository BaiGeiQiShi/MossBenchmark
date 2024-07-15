                                                                            
   
             
                                                
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "miscadmin.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/inherit.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/plancat.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "partitioning/partbounds.h"
#include "utils/hsearch.h"

typedef struct JoinHashEntry
{
  Relids join_relids;                                 
  RelOptInfo *join_rel;
} JoinHashEntry;

static void
build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *input_rel);
static List *
build_joinrel_restrictlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static void
build_joinrel_joinlist(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static List *
subbuild_joinrel_restrictlist(RelOptInfo *joinrel, List *joininfo_list, List *new_restrictlist);
static List *
subbuild_joinrel_joinlist(RelOptInfo *joinrel, List *joininfo_list, List *new_joininfo);
static void
set_foreign_rel_properties(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static void
add_join_rel(PlannerInfo *root, RelOptInfo *joinrel);
static void
build_joinrel_partition_info(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, List *restrictlist, JoinType jointype);
static void
build_child_join_reltarget(PlannerInfo *root, RelOptInfo *parentrel, RelOptInfo *childrel, int nappinfos, AppendRelInfo **appinfos);

   
                           
                                                                     
   
void
setup_simple_rel_arrays(PlannerInfo *root)
{
  Index rti;
  ListCell *lc;

                                                   
  root->simple_rel_array_size = list_length(root->parse->rtable) + 1;

                                                    
  root->simple_rel_array = (RelOptInfo **)palloc0(root->simple_rel_array_size * sizeof(RelOptInfo *));

                                                                  
  root->simple_rte_array = (RangeTblEntry **)palloc0(root->simple_rel_array_size * sizeof(RangeTblEntry *));
  rti = 1;
  foreach (lc, root->parse->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

    root->simple_rte_array[rti++] = rte;
  }
}

   
                          
                                                             
                                   
   
                                                                 
   
void
setup_append_rel_array(PlannerInfo *root)
{
  ListCell *lc;
  int size = list_length(root->parse->rtable) + 1;

  if (root->append_rel_list == NIL)
  {
    root->append_rel_array = NULL;
    return;
  }

  root->append_rel_array = (AppendRelInfo **)palloc0(size * sizeof(AppendRelInfo *));

  foreach (lc, root->append_rel_list)
  {
    AppendRelInfo *appinfo = lfirst_node(AppendRelInfo, lc);
    int child_relid = appinfo->child_relid;

                      
    Assert(child_relid < size);

    if (root->append_rel_array[child_relid])
    {
      elog(ERROR, "child relation already exists");
    }

    root->append_rel_array[child_relid] = appinfo;
  }
}

   
                         
                                                                
                                                    
   
void
expand_planner_arrays(PlannerInfo *root, int add_size)
{
  int new_size;

  Assert(add_size > 0);

  new_size = root->simple_rel_array_size + add_size;

  root->simple_rte_array = (RangeTblEntry **)repalloc(root->simple_rte_array, sizeof(RangeTblEntry *) * new_size);
  MemSet(root->simple_rte_array + root->simple_rel_array_size, 0, sizeof(RangeTblEntry *) * add_size);

  root->simple_rel_array = (RelOptInfo **)repalloc(root->simple_rel_array, sizeof(RelOptInfo *) * new_size);
  MemSet(root->simple_rel_array + root->simple_rel_array_size, 0, sizeof(RelOptInfo *) * add_size);

  if (root->append_rel_array)
  {
    root->append_rel_array = (AppendRelInfo **)repalloc(root->append_rel_array, sizeof(AppendRelInfo *) * new_size);
    MemSet(root->append_rel_array + root->simple_rel_array_size, 0, sizeof(AppendRelInfo *) * add_size);
  }
  else
  {
    root->append_rel_array = (AppendRelInfo **)palloc0(sizeof(AppendRelInfo *) * new_size);
  }

  root->simple_rel_array_size = new_size;
}

   
                    
                                                                         
   
RelOptInfo *
build_simple_rel(PlannerInfo *root, int relid, RelOptInfo *parent)
{
  RelOptInfo *rel;
  RangeTblEntry *rte;

                                    
  Assert(relid > 0 && relid < root->simple_rel_array_size);
  if (root->simple_rel_array[relid] != NULL)
  {
    elog(ERROR, "rel %d already exists", relid);
  }

                              
  rte = root->simple_rte_array[relid];
  Assert(rte != NULL);

  rel = makeNode(RelOptInfo);
  rel->reloptkind = parent ? RELOPT_OTHER_MEMBER_REL : RELOPT_BASEREL;
  rel->relids = bms_make_singleton(relid);
  rel->rows = 0;
                                                                            
  rel->consider_startup = (root->tuple_fraction > 0);
  rel->consider_param_startup = false;                              
  rel->consider_parallel = false;                                   
  rel->reltarget = create_empty_pathtarget();
  rel->pathlist = NIL;
  rel->ppilist = NIL;
  rel->partial_pathlist = NIL;
  rel->cheapest_startup_path = NULL;
  rel->cheapest_total_path = NULL;
  rel->cheapest_unique_path = NULL;
  rel->cheapest_parameterized_paths = NIL;
  rel->relid = relid;
  rel->rtekind = rte->rtekind;
                                                                  
  rel->lateral_vars = NIL;
  rel->indexlist = NIL;
  rel->statlist = NIL;
  rel->pages = 0;
  rel->tuples = 0;
  rel->allvisfrac = 0;
  rel->subroot = NULL;
  rel->subplan_params = NIL;
  rel->rel_parallel_workers = -1;                                  
  rel->serverid = InvalidOid;
  rel->userid = rte->checkAsUser;
  rel->useridiscurrent = false;
  rel->fdwroutine = NULL;
  rel->fdw_private = NULL;
  rel->unique_for_rels = NIL;
  rel->non_unique_for_rels = NIL;
  rel->baserestrictinfo = NIL;
  rel->baserestrictcost.startup = 0;
  rel->baserestrictcost.per_tuple = 0;
  rel->baserestrict_min_security = UINT_MAX;
  rel->joininfo = NIL;
  rel->has_eclass_joins = false;
  rel->consider_partitionwise_join = false;                              
  rel->part_scheme = NULL;
  rel->nparts = 0;
  rel->boundinfo = NULL;
  rel->partition_qual = NIL;
  rel->part_rels = NULL;
  rel->partexprs = NULL;
  rel->nullable_partexprs = NULL;
  rel->partitioned_child_rels = NIL;

     
                                                               
     
  if (parent)
  {
       
                                                                     
                       
       
    if (parent->top_parent_relids)
    {
      rel->top_parent_relids = parent->top_parent_relids;
    }
    else
    {
      rel->top_parent_relids = bms_copy(parent->relids);
    }

       
                                                                          
                                                                           
                                                                           
                                                                       
                                                             
                                                                        
                                                                        
                                                                           
                       
       
                                                                         
                                                                           
       
    rel->direct_lateral_relids = parent->direct_lateral_relids;
    rel->lateral_relids = parent->lateral_relids;
    rel->lateral_referencers = parent->lateral_referencers;
  }
  else
  {
    rel->top_parent_relids = NULL;
    rel->direct_lateral_relids = NULL;
    rel->lateral_relids = NULL;
    rel->lateral_referencers = NULL;
  }

                                  
  switch (rte->rtekind)
  {
  case RTE_RELATION:
                                                                
    get_relation_info(root, rte->relid, rte->inh, rel);
    break;
  case RTE_SUBQUERY:
  case RTE_FUNCTION:
  case RTE_TABLEFUNC:
  case RTE_VALUES:
  case RTE_CTE:
  case RTE_NAMEDTUPLESTORE:

       
                                                                       
                                
       
                                                              
       
    rel->min_attr = 0;
    rel->max_attr = list_length(rte->eref->colnames);
    rel->attr_needed = (Relids *)palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(Relids));
    rel->attr_widths = (int32 *)palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(int32));
    break;
  case RTE_RESULT:
                                                                    
    rel->min_attr = 0;
    rel->max_attr = -1;
    rel->attr_needed = NULL;
    rel->attr_widths = NULL;
    break;
  default:
    elog(ERROR, "unrecognized RTE kind: %d", (int)rte->rtekind);
    break;
  }

     
                                                                            
                                                                            
                                                                          
                                                                            
     
  if (parent)
  {
    AppendRelInfo *appinfo = root->append_rel_array[relid];

    Assert(appinfo != NULL);
    if (!apply_child_basequals(root, parent, rel, rte, appinfo))
    {
         
                                                                         
                                                          
         
      mark_dummy_rel(rel);
    }
  }

                                                                
  root->simple_rel_array[relid] = rel;

  return rel;
}

   
                 
                                                                    
   
RelOptInfo *
find_base_rel(PlannerInfo *root, int relid)
{
  RelOptInfo *rel;

  Assert(relid > 0);

  if (relid < root->simple_rel_array_size)
  {
    rel = root->simple_rel_array[relid];
    if (rel)
    {
      return rel;
    }
  }

  elog(ERROR, "no relation entry for relid %d", relid);

  return NULL;                          
}

   
                       
                                                            
   
static void
build_join_rel_hash(PlannerInfo *root)
{
  HTAB *hashtab;
  HASHCTL hash_ctl;
  ListCell *l;

                             
  MemSet(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Relids);
  hash_ctl.entrysize = sizeof(JoinHashEntry);
  hash_ctl.hash = bitmap_hash;
  hash_ctl.match = bitmap_match;
  hash_ctl.hcxt = CurrentMemoryContext;
  hashtab = hash_create("JoinRelHashTable", 256L, &hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

                                                
  foreach (l, root->join_rel_list)
  {
    RelOptInfo *rel = (RelOptInfo *)lfirst(l);
    JoinHashEntry *hentry;
    bool found;

    hentry = (JoinHashEntry *)hash_search(hashtab, &(rel->relids), HASH_ENTER, &found);
    Assert(!found);
    hentry->join_rel = rel;
  }

  root->join_rel_hash = hashtab;
}

   
                 
                                                                             
                                                          
   
RelOptInfo *
find_join_rel(PlannerInfo *root, Relids relids)
{
     
                                                                            
                                          
     
  if (!root->join_rel_hash && list_length(root->join_rel_list) > 32)
  {
    build_join_rel_hash(root);
  }

     
                                                                   
     
                                                                            
                                                                            
                                                                             
                       
     
  if (root->join_rel_hash)
  {
    Relids hashkey = relids;
    JoinHashEntry *hentry;

    hentry = (JoinHashEntry *)hash_search(root->join_rel_hash, &hashkey, HASH_FIND, NULL);
    if (hentry)
    {
      return hentry->join_rel;
    }
  }
  else
  {
    ListCell *l;

    foreach (l, root->join_rel_list)
    {
      RelOptInfo *rel = (RelOptInfo *)lfirst(l);

      if (bms_equal(rel->relids, relids))
      {
        return rel;
      }
    }
  }

  return NULL;
}

   
                              
                                                                       
                                                                            
                                         
   
                                                                             
                                                                           
                                                                                
                                                                           
                                                                          
             
   
                                                                               
                                 
   
   
static void
set_foreign_rel_properties(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
  if (OidIsValid(outer_rel->serverid) && inner_rel->serverid == outer_rel->serverid)
  {
    if (inner_rel->userid == outer_rel->userid)
    {
      joinrel->serverid = outer_rel->serverid;
      joinrel->userid = outer_rel->userid;
      joinrel->useridiscurrent = outer_rel->useridiscurrent || inner_rel->useridiscurrent;
      joinrel->fdwroutine = outer_rel->fdwroutine;
    }
    else if (!OidIsValid(inner_rel->userid) && outer_rel->userid == GetUserId())
    {
      joinrel->serverid = outer_rel->serverid;
      joinrel->userid = outer_rel->userid;
      joinrel->useridiscurrent = true;
      joinrel->fdwroutine = outer_rel->fdwroutine;
    }
    else if (!OidIsValid(outer_rel->userid) && inner_rel->userid == GetUserId())
    {
      joinrel->serverid = outer_rel->serverid;
      joinrel->userid = inner_rel->userid;
      joinrel->useridiscurrent = true;
      joinrel->fdwroutine = outer_rel->fdwroutine;
    }
  }
}

   
                
                                                                       
                                                                         
   
static void
add_join_rel(PlannerInfo *root, RelOptInfo *joinrel)
{
                                                                          
  root->join_rel_list = lappend(root->join_rel_list, joinrel);

                                                              
  if (root->join_rel_hash)
  {
    JoinHashEntry *hentry;
    bool found;

    hentry = (JoinHashEntry *)hash_search(root->join_rel_hash, &(joinrel->relids), HASH_ENTER, &found);
    Assert(!found);
    hentry->join_rel = joinrel;
  }
}

   
                  
                                                                          
                                                           
   
                                                                    
                                                                          
           
                               
                                                                        
                                                               
                                           
   
                                                                          
                                                 
   
RelOptInfo *
build_join_rel(PlannerInfo *root, Relids joinrelids, RelOptInfo *outer_rel, RelOptInfo *inner_rel, SpecialJoinInfo *sjinfo, List **restrictlist_ptr)
{
  RelOptInfo *joinrel;
  List *restrictlist;

                                                                   
  Assert(!IS_OTHER_REL(outer_rel) && !IS_OTHER_REL(inner_rel));

     
                                                                 
     
  joinrel = find_join_rel(root, joinrelids);

  if (joinrel)
  {
       
                                                                           
                                    
       
    if (restrictlist_ptr)
    {
      *restrictlist_ptr = build_joinrel_restrictlist(root, joinrel, outer_rel, inner_rel);
    }
    return joinrel;
  }

     
                        
     
  joinrel = makeNode(RelOptInfo);
  joinrel->reloptkind = RELOPT_JOINREL;
  joinrel->relids = bms_copy(joinrelids);
  joinrel->rows = 0;
                                                                            
  joinrel->consider_startup = (root->tuple_fraction > 0);
  joinrel->consider_param_startup = false;
  joinrel->consider_parallel = false;
  joinrel->reltarget = create_empty_pathtarget();
  joinrel->pathlist = NIL;
  joinrel->ppilist = NIL;
  joinrel->partial_pathlist = NIL;
  joinrel->cheapest_startup_path = NULL;
  joinrel->cheapest_total_path = NULL;
  joinrel->cheapest_unique_path = NULL;
  joinrel->cheapest_parameterized_paths = NIL;
                                                                          
  joinrel->direct_lateral_relids = bms_union(outer_rel->direct_lateral_relids, inner_rel->direct_lateral_relids);
  joinrel->lateral_relids = min_join_parameterization(root, joinrel->relids, outer_rel, inner_rel);
  joinrel->relid = 0;                              
  joinrel->rtekind = RTE_JOIN;
  joinrel->min_attr = 0;
  joinrel->max_attr = 0;
  joinrel->attr_needed = NULL;
  joinrel->attr_widths = NULL;
  joinrel->lateral_vars = NIL;
  joinrel->lateral_referencers = NULL;
  joinrel->indexlist = NIL;
  joinrel->statlist = NIL;
  joinrel->pages = 0;
  joinrel->tuples = 0;
  joinrel->allvisfrac = 0;
  joinrel->subroot = NULL;
  joinrel->subplan_params = NIL;
  joinrel->rel_parallel_workers = -1;
  joinrel->serverid = InvalidOid;
  joinrel->userid = InvalidOid;
  joinrel->useridiscurrent = false;
  joinrel->fdwroutine = NULL;
  joinrel->fdw_private = NULL;
  joinrel->unique_for_rels = NIL;
  joinrel->non_unique_for_rels = NIL;
  joinrel->baserestrictinfo = NIL;
  joinrel->baserestrictcost.startup = 0;
  joinrel->baserestrictcost.per_tuple = 0;
  joinrel->baserestrict_min_security = UINT_MAX;
  joinrel->joininfo = NIL;
  joinrel->has_eclass_joins = false;
  joinrel->consider_partitionwise_join = false;                              
  joinrel->top_parent_relids = NULL;
  joinrel->part_scheme = NULL;
  joinrel->nparts = 0;
  joinrel->boundinfo = NULL;
  joinrel->partition_qual = NIL;
  joinrel->part_rels = NULL;
  joinrel->partexprs = NULL;
  joinrel->nullable_partexprs = NULL;
  joinrel->partitioned_child_rels = NIL;

                                                              
  set_foreign_rel_properties(joinrel, outer_rel, inner_rel);

     
                                                                             
                                                                        
     
                                                                             
                                                                            
                             
     
  build_joinrel_tlist(root, joinrel, outer_rel);
  build_joinrel_tlist(root, joinrel, inner_rel);
  add_placeholders_to_joinrel(root, joinrel, outer_rel, inner_rel);

     
                                                                         
                                                                            
                                                                             
                                                                          
                                                               
     
  joinrel->direct_lateral_relids = bms_del_members(joinrel->direct_lateral_relids, joinrel->relids);
  if (bms_is_empty(joinrel->direct_lateral_relids))
  {
    joinrel->direct_lateral_relids = NULL;
  }

     
                                                                        
                                                                           
                                        
     
  restrictlist = build_joinrel_restrictlist(root, joinrel, outer_rel, inner_rel);
  if (restrictlist_ptr)
  {
    *restrictlist_ptr = restrictlist;
  }
  build_joinrel_joinlist(joinrel, outer_rel, inner_rel);

     
                                                                       
                                     
     
  joinrel->has_eclass_joins = has_relevant_eclass_joinclause(root, joinrel);

                                        
  build_joinrel_partition_info(joinrel, outer_rel, inner_rel, restrictlist, sjinfo->jointype);

     
                                          
     
  set_joinrel_size_estimates(root, joinrel, outer_rel, inner_rel, sjinfo, restrictlist);

     
                                                                         
                                                                         
                                                                         
                                                                           
                                 
     
                                                                            
                                                                          
                                                                             
                                                                          
                                                                           
           
     
  if (inner_rel->consider_parallel && outer_rel->consider_parallel && is_parallel_safe(root, (Node *)restrictlist) && is_parallel_safe(root, (Node *)joinrel->reltarget->exprs))
  {
    joinrel->consider_parallel = true;
  }

                                           
  add_join_rel(root, joinrel);

     
                                                                             
                                                                             
                                                                           
                                                           
     
  if (root->join_rel_level)
  {
    Assert(root->join_cur_level > 0);
    Assert(root->join_cur_level <= bms_num_members(joinrel->relids));
    root->join_rel_level[root->join_cur_level] = lappend(root->join_rel_level[root->join_cur_level], joinrel);
  }

  return joinrel;
}

   
                        
                                                                            
   
                                                                            
           
                                                                           
                                                                     
                                                         
                                     
                                                                            
                               
                                                        
   
RelOptInfo *
build_child_join_rel(PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel, RelOptInfo *parent_joinrel, List *restrictlist, SpecialJoinInfo *sjinfo, JoinType jointype)
{
  RelOptInfo *joinrel = makeNode(RelOptInfo);
  AppendRelInfo **appinfos;
  int nappinfos;

                                                       
  Assert(IS_OTHER_REL(outer_rel) && IS_OTHER_REL(inner_rel));

                                                                       
  Assert(parent_joinrel->consider_partitionwise_join);

  joinrel->reloptkind = RELOPT_OTHER_JOINREL;
  joinrel->relids = bms_union(outer_rel->relids, inner_rel->relids);
  joinrel->rows = 0;
                                                                            
  joinrel->consider_startup = (root->tuple_fraction > 0);
  joinrel->consider_param_startup = false;
  joinrel->consider_parallel = false;
  joinrel->reltarget = create_empty_pathtarget();
  joinrel->pathlist = NIL;
  joinrel->ppilist = NIL;
  joinrel->partial_pathlist = NIL;
  joinrel->cheapest_startup_path = NULL;
  joinrel->cheapest_total_path = NULL;
  joinrel->cheapest_unique_path = NULL;
  joinrel->cheapest_parameterized_paths = NIL;
  joinrel->direct_lateral_relids = NULL;
  joinrel->lateral_relids = NULL;
  joinrel->relid = 0;                              
  joinrel->rtekind = RTE_JOIN;
  joinrel->min_attr = 0;
  joinrel->max_attr = 0;
  joinrel->attr_needed = NULL;
  joinrel->attr_widths = NULL;
  joinrel->lateral_vars = NIL;
  joinrel->lateral_referencers = NULL;
  joinrel->indexlist = NIL;
  joinrel->pages = 0;
  joinrel->tuples = 0;
  joinrel->allvisfrac = 0;
  joinrel->subroot = NULL;
  joinrel->subplan_params = NIL;
  joinrel->serverid = InvalidOid;
  joinrel->userid = InvalidOid;
  joinrel->useridiscurrent = false;
  joinrel->fdwroutine = NULL;
  joinrel->fdw_private = NULL;
  joinrel->baserestrictinfo = NIL;
  joinrel->baserestrictcost.startup = 0;
  joinrel->baserestrictcost.per_tuple = 0;
  joinrel->joininfo = NIL;
  joinrel->has_eclass_joins = false;
  joinrel->consider_partitionwise_join = false;                              
  joinrel->top_parent_relids = NULL;
  joinrel->part_scheme = NULL;
  joinrel->nparts = 0;
  joinrel->boundinfo = NULL;
  joinrel->partition_qual = NIL;
  joinrel->part_rels = NULL;
  joinrel->partexprs = NULL;
  joinrel->nullable_partexprs = NULL;
  joinrel->partitioned_child_rels = NIL;

  joinrel->top_parent_relids = bms_union(outer_rel->top_parent_relids, inner_rel->top_parent_relids);

                                                          
  set_foreign_rel_properties(joinrel, outer_rel, inner_rel);

                                                                    
  appinfos = find_appinfos_by_relids(root, joinrel->relids, &nappinfos);

                               
  build_child_join_reltarget(root, parent_joinrel, joinrel, nappinfos, appinfos);

                                
  joinrel->joininfo = (List *)adjust_appendrel_attrs(root, (Node *)parent_joinrel->joininfo, nappinfos, appinfos);

     
                                                                            
                                                                           
                     
     
  bms_free(joinrel->direct_lateral_relids);
  bms_free(joinrel->lateral_relids);
  joinrel->direct_lateral_relids = (Relids)bms_copy(parent_joinrel->direct_lateral_relids);
  joinrel->lateral_relids = (Relids)bms_copy(parent_joinrel->lateral_relids);

     
                                                                        
            
     
  joinrel->has_eclass_joins = parent_joinrel->has_eclass_joins;

                                                          
  build_joinrel_partition_info(joinrel, outer_rel, inner_rel, restrictlist, jointype);

                                                                  
  joinrel->consider_parallel = parent_joinrel->consider_parallel;

                                                  
  set_joinrel_size_estimates(root, joinrel, outer_rel, inner_rel, sjinfo, restrictlist);

                                    
  Assert(!find_join_rel(root, joinrel->relids));

                                            
  add_join_rel(root, joinrel);

     
                                                                             
                                                                         
                                                                             
                                                                            
     
  if (joinrel->has_eclass_joins || has_useful_pathkeys(root, parent_joinrel))
  {
    add_child_join_rel_equivalences(root, nappinfos, appinfos, parent_joinrel, joinrel);
  }

  pfree(appinfos);

  return joinrel;
}

   
                             
   
                                                                              
                                                                               
                                                                          
                                                                        
   
Relids
min_join_parameterization(PlannerInfo *root, Relids joinrelids, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
  Relids result;

     
                                                                          
                                      
     
                                                                          
                                                                             
                                                                           
                                                                        
                                                                           
                                                                           
                                                                            
                                                                       
     
  result = bms_union(outer_rel->lateral_relids, inner_rel->lateral_relids);
  result = bms_del_members(result, joinrelids);

                                                               
  if (bms_is_empty(result))
  {
    result = NULL;
  }

  return result;
}

   
                       
                                                                  
                                                                
   
                                                                        
                                                                       
                                                                      
   
                                                                       
                                                                    
   
static void
build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *input_rel)
{
  Relids relids = joinrel->relids;
  ListCell *vars;

  foreach (vars, input_rel->reltarget->exprs)
  {
    Var *var = (Var *)lfirst(vars);
    RelOptInfo *baserel;
    int ndx;

       
                                                                      
                                             
       
    if (IsA(var, PlaceHolderVar))
    {
      continue;
    }

       
                                                                          
                                                                      
                                             
       
    if (!IsA(var, Var))
    {
      elog(ERROR, "unexpected node type in rel targetlist: %d", (int)nodeTag(var));
    }

                                         
    baserel = find_base_rel(root, var->varno);

                                                
    ndx = var->varattno - baserel->min_attr;
    if (bms_nonempty_difference(baserel->attr_needed[ndx], relids))
    {
                                     
      joinrel->reltarget->exprs = lappend(joinrel->reltarget->exprs, var);
                                                                     
      joinrel->reltarget->width += baserel->attr_widths[ndx];
    }
  }
}

   
                              
                          
                                                                      
                                                                      
   
                                                                      
                                                                            
                                                                       
                                                                        
                                                                          
                                                                            
                                                                 
                                   
   
                                                                           
                                                                             
                                                                   
                                                                         
                                                                             
                                                                            
                                              
   
                                                                     
                                                                  
                                                                      
                                                                    
                                                            
   
                                     
                                                                          
                     
   
                                                                          
                                                                        
                                                                   
   
                                                                           
                                                                            
                                                                              
                                                               
   
static List *
build_joinrel_restrictlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
  List *result;

     
                                                                      
                                                                         
                                                       
     
  result = subbuild_joinrel_restrictlist(joinrel, outer_rel->joininfo, NIL);
  result = subbuild_joinrel_restrictlist(joinrel, inner_rel->joininfo, result);

     
                                                                          
                                                                       
               
     
  result = list_concat(result, generate_join_implied_equalities(root, joinrel->relids, outer_rel->relids, inner_rel));

  return result;
}

static void
build_joinrel_joinlist(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
  List *result;

     
                                                                         
                                                                         
                                                       
     
  result = subbuild_joinrel_joinlist(joinrel, outer_rel->joininfo, NIL);
  result = subbuild_joinrel_joinlist(joinrel, inner_rel->joininfo, result);

  joinrel->joininfo = result;
}

static List *
subbuild_joinrel_restrictlist(RelOptInfo *joinrel, List *joininfo_list, List *new_restrictlist)
{
  ListCell *l;

  foreach (l, joininfo_list)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);

    if (bms_is_subset(rinfo->required_relids, joinrel->relids))
    {
         
                                                                         
                                                                  
                                                                       
                                                                        
                                                                
         
      new_restrictlist = list_append_unique_ptr(new_restrictlist, rinfo);
    }
    else
    {
         
                                                                        
                             
         
    }
  }

  return new_restrictlist;
}

static List *
subbuild_joinrel_joinlist(RelOptInfo *joinrel, List *joininfo_list, List *new_joininfo)
{
  ListCell *l;

                                                                     
  Assert(joinrel->reloptkind == RELOPT_JOINREL);

  foreach (l, joininfo_list)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);

    if (bms_is_subset(rinfo->required_relids, joinrel->relids))
    {
         
                                                                         
                                                                    
                  
         
    }
    else
    {
         
                                                                        
                                                                       
                                                                         
                                                                        
                             
         
      new_joininfo = list_append_unique_ptr(new_joininfo, rinfo);
    }
  }

  return new_joininfo;
}

   
                   
                                                                        
                                                               
   
                                                                               
                                                                             
                                      
   
                                                                            
                                                                               
                                                                            
   
RelOptInfo *
fetch_upper_rel(PlannerInfo *root, UpperRelationKind kind, Relids relids)
{
  RelOptInfo *upperrel;
  ListCell *lc;

     
                                                                         
                                                                        
                                                                            
                                                              
     

                                                                 
  foreach (lc, root->upper_rels[kind])
  {
    upperrel = (RelOptInfo *)lfirst(lc);

    if (bms_equal(upperrel->relids, relids))
    {
      return upperrel;
    }
  }

  upperrel = makeNode(RelOptInfo);
  upperrel->reloptkind = RELOPT_UPPER_REL;
  upperrel->relids = bms_copy(relids);

                                                                            
  upperrel->consider_startup = (root->tuple_fraction > 0);
  upperrel->consider_param_startup = false;
  upperrel->consider_parallel = false;                              
  upperrel->reltarget = create_empty_pathtarget();
  upperrel->pathlist = NIL;
  upperrel->cheapest_startup_path = NULL;
  upperrel->cheapest_total_path = NULL;
  upperrel->cheapest_unique_path = NULL;
  upperrel->cheapest_parameterized_paths = NIL;

  root->upper_rels[kind] = lappend(root->upper_rels[kind], upperrel);

  return upperrel;
}

   
                         
                                                                
   
                                                                         
                                                                        
                        
   
Relids
find_childrel_parents(PlannerInfo *root, RelOptInfo *rel)
{
  Relids result = NULL;

  Assert(rel->reloptkind == RELOPT_OTHER_MEMBER_REL);
  Assert(rel->relid > 0 && rel->relid < root->simple_rel_array_size);

  do
  {
    AppendRelInfo *appinfo = root->append_rel_array[rel->relid];
    Index prelid = appinfo->parent_relid;

    result = bms_add_member(result, prelid);

                                                                      
    rel = find_base_rel(root, prelid);
  } while (rel->reloptkind == RELOPT_OTHER_MEMBER_REL);

  Assert(rel->reloptkind == RELOPT_BASEREL);

  return result;
}

   
                             
                                                                        
                                                   
   
                                                                      
                                                                            
                                                                            
                                                                             
                                  
   
ParamPathInfo *
get_baserel_parampathinfo(PlannerInfo *root, RelOptInfo *baserel, Relids required_outer)
{
  ParamPathInfo *ppi;
  Relids joinrelids;
  List *pclauses;
  double rows;
  ListCell *lc;

                                                                          
  Assert(bms_is_subset(baserel->lateral_relids, required_outer));

                                                   
  if (bms_is_empty(required_outer))
  {
    return NULL;
  }

  Assert(!bms_overlap(baserel->relids, required_outer));

                                                                          
  if ((ppi = find_param_path_info(baserel, required_outer)))
  {
    return ppi;
  }

     
                                                                           
                       
     
  joinrelids = bms_union(baserel->relids, required_outer);
  pclauses = NIL;
  foreach (lc, baserel->joininfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

    if (join_clause_is_movable_into(rinfo, baserel->relids, joinrelids))
    {
      pclauses = lappend(pclauses, rinfo);
    }
  }

     
                                                                      
                                                       
     
  pclauses = list_concat(pclauses, generate_join_implied_equalities(root, joinrelids, required_outer, baserel));

                                                                      
  rows = get_parameterized_baserel_size(root, baserel, pclauses);

                                              
  ppi = makeNode(ParamPathInfo);
  ppi->ppi_req_outer = required_outer;
  ppi->ppi_rows = rows;
  ppi->ppi_clauses = pclauses;
  baserel->ppilist = lappend(baserel->ppilist, ppi);

  return ppi;
}

   
                             
                                                                        
                                                   
   
                                                                      
                                                                            
                                                                            
                                                                             
                                  
   
                                                                           
                                                                        
                                                                            
                                                     
   
                                                                             
                                                                          
                                                                            
                                                                           
                                                                           
                                                                         
   
                                                                             
                                                                            
                                                                       
                                     
   
ParamPathInfo *
get_joinrel_parampathinfo(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, SpecialJoinInfo *sjinfo, Relids required_outer, List **restrict_clauses)
{
  ParamPathInfo *ppi;
  Relids join_and_req;
  Relids outer_and_req;
  Relids inner_and_req;
  List *pclauses;
  List *eclauses;
  List *dropped_ecs;
  double rows;
  ListCell *lc;

                                                                          
  Assert(bms_is_subset(joinrel->lateral_relids, required_outer));

                                                                         
  if (bms_is_empty(required_outer))
  {
    return NULL;
  }

  Assert(!bms_overlap(joinrel->relids, required_outer));

     
                                                                           
                                                                         
                                                                             
                                                                          
                                                                             
                                                                            
               
     
  join_and_req = bms_union(joinrel->relids, required_outer);
  if (outer_path->param_info)
  {
    outer_and_req = bms_union(outer_path->parent->relids, PATH_REQ_OUTER(outer_path));
  }
  else
  {
    outer_and_req = NULL;                                            
  }
  if (inner_path->param_info)
  {
    inner_and_req = bms_union(inner_path->parent->relids, PATH_REQ_OUTER(inner_path));
  }
  else
  {
    inner_and_req = NULL;                                            
  }

  pclauses = NIL;
  foreach (lc, joinrel->joininfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

    if (join_clause_is_movable_into(rinfo, joinrel->relids, join_and_req) && !join_clause_is_movable_into(rinfo, outer_path->parent->relids, outer_and_req) && !join_clause_is_movable_into(rinfo, inner_path->parent->relids, inner_and_req))
    {
      pclauses = lappend(pclauses, rinfo);
    }
  }

                                                                 
  eclauses = generate_join_implied_equalities(root, join_and_req, required_outer, joinrel);
                                                             
  dropped_ecs = NIL;
  foreach (lc, eclauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

       
                                                                          
                                                                       
                                                                  
                                                                          
                                           
       
#ifdef NOT_USED
    Assert(join_clause_is_movable_into(rinfo, joinrel->relids, join_and_req));
#endif
    if (join_clause_is_movable_into(rinfo, outer_path->parent->relids, outer_and_req))
    {
      continue;                               
    }
    if (join_clause_is_movable_into(rinfo, inner_path->parent->relids, inner_and_req))
    {
                                                                   
      Assert(rinfo->left_ec == rinfo->right_ec);
      dropped_ecs = lappend(dropped_ecs, rinfo->left_ec);
      continue;
    }
    pclauses = lappend(pclauses, rinfo);
  }

     
                                                                            
                                                                             
                                                                           
                                                                             
                                                                             
                                                                          
                                                                             
                                                                             
                                                                           
                                                                           
                                                                       
                                                                           
                                                                            
                                                                            
                                                                         
     
                                                                         
                                                                            
                                                                             
                                                                        
                                                                           
                                                                            
                                                                             
     
                                                                         
                                                                           
                                                                     
                                       
     
  if (dropped_ecs)
  {
    Relids real_outer_and_req;

    real_outer_and_req = bms_union(outer_path->parent->relids, required_outer);
    eclauses = generate_join_implied_equalities_for_ecs(root, dropped_ecs, real_outer_and_req, required_outer, outer_path->parent);
    foreach (lc, eclauses)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

                                                  
#ifdef NOT_USED
      Assert(join_clause_is_movable_into(rinfo, outer_path->parent->relids, real_outer_and_req));
#endif
      if (!join_clause_is_movable_into(rinfo, outer_path->parent->relids, outer_and_req))
      {
        pclauses = lappend(pclauses, rinfo);
      }
    }
  }

     
                                                                   
                                                                          
                                                                
     
  *restrict_clauses = list_concat(pclauses, *restrict_clauses);

                                                                          
  if ((ppi = find_param_path_info(joinrel, required_outer)))
  {
    return ppi;
  }

                                                                      
  rows = get_parameterized_joinrel_size(root, joinrel, outer_path, inner_path, sjinfo, *restrict_clauses);

     
                                                                     
                                               
     
                                                                            
                                                        
     
  ppi = makeNode(ParamPathInfo);
  ppi->ppi_req_outer = required_outer;
  ppi->ppi_rows = rows;
  ppi->ppi_clauses = NIL;
  joinrel->ppilist = lappend(joinrel->ppilist, ppi);

  return ppi;
}

   
                               
                                                                           
   
                                                                         
                                                                           
                                                                             
                                                                          
                                                          
   
ParamPathInfo *
get_appendrel_parampathinfo(RelOptInfo *appendrel, Relids required_outer)
{
  ParamPathInfo *ppi;

                                                                          
  Assert(bms_is_subset(appendrel->lateral_relids, required_outer));

                                                   
  if (bms_is_empty(required_outer))
  {
    return NULL;
  }

  Assert(!bms_overlap(appendrel->relids, required_outer));

                                                                          
  if ((ppi = find_param_path_info(appendrel, required_outer)))
  {
    return ppi;
  }

                                    
  ppi = makeNode(ParamPathInfo);
  ppi->ppi_req_outer = required_outer;
  ppi->ppi_rows = 0;
  ppi->ppi_clauses = NIL;
  appendrel->ppilist = lappend(appendrel->ppilist, ppi);

  return ppi;
}

   
                                                                                
                                                               
   
ParamPathInfo *
find_param_path_info(RelOptInfo *rel, Relids required_outer)
{
  ListCell *lc;

  foreach (lc, rel->ppilist)
  {
    ParamPathInfo *ppi = (ParamPathInfo *)lfirst(lc);

    if (bms_equal(ppi->ppi_req_outer, required_outer))
    {
      return ppi;
    }
  }

  return NULL;
}

   
                                
                                                                          
                                                                            
                                                                         
                       
   
static void
build_joinrel_partition_info(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, List *restrictlist, JoinType jointype)
{
  int partnatts;
  int cnt;
  PartitionScheme part_scheme;

                                                                  
  if (!enable_partitionwise_join)
  {
    Assert(!IS_PARTITIONED_REL(joinrel));
    return;
  }

     
                                                                         
                                                               
                                                                            
                                                                             
                                                                        
                                                                          
                                                                            
                                                                          
                                                          
     
  if (!IS_PARTITIONED_REL(outer_rel) || !IS_PARTITIONED_REL(inner_rel) || !outer_rel->consider_partitionwise_join || !inner_rel->consider_partitionwise_join || outer_rel->part_scheme != inner_rel->part_scheme || !have_partkey_equi_join(joinrel, outer_rel, inner_rel, jointype, restrictlist))
  {
    Assert(!IS_PARTITIONED_REL(joinrel));
    return;
  }

  part_scheme = outer_rel->part_scheme;

  Assert(REL_HAS_ALL_PART_PROPS(outer_rel) && REL_HAS_ALL_PART_PROPS(inner_rel));

     
                                                                         
                                                                          
                             
     
  if (outer_rel->nparts != inner_rel->nparts || !partition_bounds_equal(part_scheme->partnatts, part_scheme->parttyplen, part_scheme->parttypbyval, outer_rel->boundinfo, inner_rel->boundinfo))
  {
    Assert(!IS_PARTITIONED_REL(joinrel));
    return;
  }

     
                                                                       
                                                                       
                                                            
     
  Assert(!joinrel->part_scheme && !joinrel->partexprs && !joinrel->nullable_partexprs && !joinrel->part_rels && !joinrel->boundinfo);

     
                                                                            
                                            
     
  joinrel->part_scheme = part_scheme;
  joinrel->boundinfo = outer_rel->boundinfo;
  partnatts = joinrel->part_scheme->partnatts;
  joinrel->partexprs = (List **)palloc0(sizeof(List *) * partnatts);
  joinrel->nullable_partexprs = (List **)palloc0(sizeof(List *) * partnatts);
  joinrel->nparts = outer_rel->nparts;
  joinrel->part_rels = (RelOptInfo **)palloc0(sizeof(RelOptInfo *) * joinrel->nparts);

     
                                               
     
  Assert(outer_rel->consider_partitionwise_join);
  Assert(inner_rel->consider_partitionwise_join);
  joinrel->consider_partitionwise_join = true;

     
                                            
     
                                                                        
                                                                           
                                                                         
                 
     
                                                                       
                                                                       
                               
     
                                                                           
                                                                             
                                                                           
                                                                       
                                                                 
                                                                             
                                                                            
                                                                 
                                                                  
                                                                          
                                                                     
                                                                      
             
     
  for (cnt = 0; cnt < partnatts; cnt++)
  {
    List *outer_expr;
    List *outer_null_expr;
    List *inner_expr;
    List *inner_null_expr;
    List *partexpr = NIL;
    List *nullable_partexpr = NIL;

    outer_expr = list_copy(outer_rel->partexprs[cnt]);
    outer_null_expr = list_copy(outer_rel->nullable_partexprs[cnt]);
    inner_expr = list_copy(inner_rel->partexprs[cnt]);
    inner_null_expr = list_copy(inner_rel->nullable_partexprs[cnt]);

    switch (jointype)
    {
    case JOIN_INNER:
      partexpr = list_concat(outer_expr, inner_expr);
      nullable_partexpr = list_concat(outer_null_expr, inner_null_expr);
      break;

    case JOIN_SEMI:
    case JOIN_ANTI:
      partexpr = outer_expr;
      nullable_partexpr = outer_null_expr;
      break;

    case JOIN_LEFT:
      partexpr = outer_expr;
      nullable_partexpr = list_concat(inner_expr, outer_null_expr);
      nullable_partexpr = list_concat(nullable_partexpr, inner_null_expr);
      break;

    case JOIN_FULL:
      nullable_partexpr = list_concat(outer_expr, inner_expr);
      nullable_partexpr = list_concat(nullable_partexpr, outer_null_expr);
      nullable_partexpr = list_concat(nullable_partexpr, inner_null_expr);
      break;

    default:
      elog(ERROR, "unrecognized join type: %d", (int)jointype);
    }

    joinrel->partexprs[cnt] = partexpr;
    joinrel->nullable_partexprs[cnt] = nullable_partexpr;
  }
}

   
                              
                                                                           
   
static void
build_child_join_reltarget(PlannerInfo *root, RelOptInfo *parentrel, RelOptInfo *childrel, int nappinfos, AppendRelInfo **appinfos)
{
                            
  childrel->reltarget->exprs = (List *)adjust_appendrel_attrs(root, (Node *)parentrel->reltarget->exprs, nappinfos, appinfos);

                                     
  childrel->reltarget->cost.startup = parentrel->reltarget->cost.startup;
  childrel->reltarget->cost.per_tuple = parentrel->reltarget->cost.per_tuple;
  childrel->reltarget->width = parentrel->reltarget->width;
}
