                                                                            
   
              
                                                            
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "miscadmin.h"
#include "foreign/fdwapi.h"
#include "nodes/extensible.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/selfuncs.h"

typedef enum
{
  COSTS_EQUAL,                                      
  COSTS_BETTER1,                                         
  COSTS_BETTER2,                                         
  COSTS_DIFFERENT                                               
} PathCostComparison;

   
                                                                             
                                                                          
                                                                      
   
#define STD_FUZZ_FACTOR 1.01

static List *
translate_sub_tlist(List *tlist, int relid);
static int
append_total_cost_compare(const void *a, const void *b);
static int
append_startup_cost_compare(const void *a, const void *b);
static List *
reparameterize_pathlist_by_child(PlannerInfo *root, List *pathlist, RelOptInfo *child_rel);

                                                                               
                         
                                                                               

   
                      
                                                                       
                                                               
   
int
compare_path_costs(Path *path1, Path *path2, CostSelector criterion)
{
  if (criterion == STARTUP_COST)
  {
    if (path1->startup_cost < path2->startup_cost)
    {
      return -1;
    }
    if (path1->startup_cost > path2->startup_cost)
    {
      return +1;
    }

       
                                                                        
                           
       
    if (path1->total_cost < path2->total_cost)
    {
      return -1;
    }
    if (path1->total_cost > path2->total_cost)
    {
      return +1;
    }
  }
  else
  {
    if (path1->total_cost < path2->total_cost)
    {
      return -1;
    }
    if (path1->total_cost > path2->total_cost)
    {
      return +1;
    }

       
                                                                      
       
    if (path1->startup_cost < path2->startup_cost)
    {
      return -1;
    }
    if (path1->startup_cost > path2->startup_cost)
    {
      return +1;
    }
  }
  return 0;
}

   
                                 
                                                                       
                                                                      
                          
   
                                                                       
                                     
   
int
compare_fractional_path_costs(Path *path1, Path *path2, double fraction)
{
  Cost cost1, cost2;

  if (fraction <= 0.0 || fraction >= 1.0)
  {
    return compare_path_costs(path1, path2, TOTAL_COST);
  }
  cost1 = path1->startup_cost + fraction * (path1->total_cost - path1->startup_cost);
  cost2 = path2->startup_cost + fraction * (path2->total_cost - path2->startup_cost);
  if (cost1 < cost2)
  {
    return -1;
  }
  if (cost1 > cost2)
  {
    return +1;
  }
  return 0;
}

   
                              
                                                                    
                         
   
                                                                         
                                                                    
   
                                                                       
                                                                       
                                                                          
                              
   
                                                                          
                                                                         
                                                                       
                                                                             
                                                                          
                                                                         
                                                                           
                                                              
   
                                                                               
                                                                               
                                                                             
                                                                               
                                                                           
                                                   
   
static PathCostComparison
compare_path_costs_fuzzily(Path *path1, Path *path2, double fuzz_factor)
{
#define CONSIDER_PATH_STARTUP_COST(p) ((p)->param_info == NULL ? (p)->parent->consider_startup : (p)->parent->consider_param_startup)

     
                                                                         
                                   
     
  if (path1->total_cost > path2->total_cost * fuzz_factor)
  {
                                           
    if (CONSIDER_PATH_STARTUP_COST(path1) && path2->startup_cost > path1->startup_cost * fuzz_factor)
    {
                                                                
      return COSTS_DIFFERENT;
    }
                              
    return COSTS_BETTER2;
  }
  if (path2->total_cost > path1->total_cost * fuzz_factor)
  {
                                           
    if (CONSIDER_PATH_STARTUP_COST(path2) && path1->startup_cost > path2->startup_cost * fuzz_factor)
    {
                                                                
      return COSTS_DIFFERENT;
    }
                              
    return COSTS_BETTER1;
  }
                                          
  if (path1->startup_cost > path2->startup_cost * fuzz_factor)
  {
                                                               
    return COSTS_BETTER2;
  }
  if (path2->startup_cost > path1->startup_cost * fuzz_factor)
  {
                                                               
    return COSTS_BETTER1;
  }
                                      
  return COSTS_EQUAL;

#undef CONSIDER_PATH_STARTUP_COST
}

   
                
                                                                
                                                      
   
                                                                           
                                                                           
                                                                          
                                                                              
                                                               
   
                                                                          
                                                                           
                                                                           
                                                                         
                                                                           
                                                                         
                                                                          
                                                
   
                                                                   
                                                                           
                                          
   
                                                                           
                          
   
void
set_cheapest(RelOptInfo *parent_rel)
{
  Path *cheapest_startup_path;
  Path *cheapest_total_path;
  Path *best_param_path;
  List *parameterized_paths;
  ListCell *p;

  Assert(IsA(parent_rel, RelOptInfo));

  if (parent_rel->pathlist == NIL)
  {
    elog(ERROR, "could not devise a query plan for the given query");
  }

  cheapest_startup_path = cheapest_total_path = best_param_path = NULL;
  parameterized_paths = NIL;

  foreach (p, parent_rel->pathlist)
  {
    Path *path = (Path *)lfirst(p);
    int cmp;

    if (path->param_info)
    {
                                                                
      parameterized_paths = lappend(parameterized_paths, path);

         
                                                                         
                                                                
         
      if (cheapest_total_path)
      {
        continue;
      }

         
                                                                        
                                                          
                           
         
      if (best_param_path == NULL)
      {
        best_param_path = path;
      }
      else
      {
        switch (bms_subset_compare(PATH_REQ_OUTER(path), PATH_REQ_OUTER(best_param_path)))
        {
        case BMS_EQUAL:
                                    
          if (compare_path_costs(path, best_param_path, TOTAL_COST) < 0)
          {
            best_param_path = path;
          }
          break;
        case BMS_SUBSET1:
                                              
          best_param_path = path;
          break;
        case BMS_SUBSET2:
                                                       
          break;
        case BMS_DIFFERENT:

             
                                                                 
                                                                 
                                                      
             
          break;
        }
      }
    }
    else
    {
                                                                   
      if (cheapest_total_path == NULL)
      {
        cheapest_startup_path = cheapest_total_path = path;
        continue;
      }

         
                                                                  
                                                                 
                                                                   
                                                                   
                               
         
      cmp = compare_path_costs(cheapest_startup_path, path, STARTUP_COST);
      if (cmp > 0 || (cmp == 0 && compare_pathkeys(cheapest_startup_path->pathkeys, path->pathkeys) == PATHKEYS_BETTER2))
      {
        cheapest_startup_path = path;
      }

      cmp = compare_path_costs(cheapest_total_path, path, TOTAL_COST);
      if (cmp > 0 || (cmp == 0 && compare_pathkeys(cheapest_total_path->pathkeys, path->pathkeys) == PATHKEYS_BETTER2))
      {
        cheapest_total_path = path;
      }
    }
  }

                                                                         
  if (cheapest_total_path)
  {
    parameterized_paths = lcons(cheapest_total_path, parameterized_paths);
  }

     
                                                                             
                                                             
     
  if (cheapest_total_path == NULL)
  {
    cheapest_total_path = best_param_path;
  }
  Assert(cheapest_total_path != NULL);

  parent_rel->cheapest_startup_path = cheapest_startup_path;
  parent_rel->cheapest_total_path = cheapest_total_path;
  parent_rel->cheapest_unique_path = NULL;                              
  parent_rel->cheapest_parameterized_paths = parameterized_paths;
}

   
            
                                                                            
                                                                        
                                                                         
                                                                           
                                                                        
                                                                   
   
                                                                             
                                                                             
                                                                            
                                         
   
                                                                          
                                                                             
                                                                             
                                                                           
                                                               
   
                                                                          
                                                                             
                                                                        
                                                                         
                                                                          
   
                                                                      
                                                                          
                                                                         
                                                         
   
                                                                   
                                                                     
                                                                            
                                                                 
                                                                      
                    
   
                                                                            
                                                                              
                                                                               
                                                                         
                                                                         
   
                                                                          
                                                                          
                                                                              
                                                                          
                                                                             
                                                                           
                                                                         
                                                                             
   
                                                                     
                                                  
   
                                                       
   
void
add_path(RelOptInfo *parent_rel, Path *new_path)
{
  bool accept_new = true;                                                
  ListCell *insert_after = NULL;                               
  List *new_path_pathkeys;
  ListCell *p1;
  ListCell *p1_prev;
  ListCell *p1_next;

     
                                                                             
                                                        
     
  CHECK_FOR_INTERRUPTS();

                                                                       
  new_path_pathkeys = new_path->param_info ? NIL : new_path->pathkeys;

     
                                                                             
                                                                            
         
     
                                                                            
                
     
  p1_prev = NULL;
  for (p1 = list_head(parent_rel->pathlist); p1 != NULL; p1 = p1_next)
  {
    Path *old_path = (Path *)lfirst(p1);
    bool remove_old = false;                                 
    PathCostComparison costcmp;
    PathKeysComparison keyscmp;
    BMS_Comparison outercmp;

    p1_next = lnext(p1);

       
                                                                 
       
    costcmp = compare_path_costs_fuzzily(new_path, old_path, STD_FUZZ_FACTOR);

       
                                                                        
                                                                         
                                                                        
                                                                          
                                                                        
                                                                          
                                                                          
                                                                          
                 
       
    if (costcmp != COSTS_DIFFERENT)
    {
                                                                  
      List *old_path_pathkeys;

      old_path_pathkeys = old_path->param_info ? NIL : old_path->pathkeys;
      keyscmp = compare_pathkeys(new_path_pathkeys, old_path_pathkeys);
      if (keyscmp != PATHKEYS_DIFFERENT)
      {
        switch (costcmp)
        {
        case COSTS_EQUAL:
          outercmp = bms_subset_compare(PATH_REQ_OUTER(new_path), PATH_REQ_OUTER(old_path));
          if (keyscmp == PATHKEYS_BETTER1)
          {
            if ((outercmp == BMS_EQUAL || outercmp == BMS_SUBSET1) && new_path->rows <= old_path->rows && new_path->parallel_safe >= old_path->parallel_safe)
            {
              remove_old = true;                        
            }
          }
          else if (keyscmp == PATHKEYS_BETTER2)
          {
            if ((outercmp == BMS_EQUAL || outercmp == BMS_SUBSET2) && new_path->rows >= old_path->rows && new_path->parallel_safe <= old_path->parallel_safe)
            {
              accept_new = false;                        
            }
          }
          else                                
          {
            if (outercmp == BMS_EQUAL)
            {
                 
                                                           
                                                            
                                                          
                                                            
                                                           
                                                            
                                                            
                                                            
                                                         
                                                         
                                                    
                                                           
                                                      
                 
              if (new_path->parallel_safe > old_path->parallel_safe)
              {
                remove_old = true;                        
              }
              else if (new_path->parallel_safe < old_path->parallel_safe)
              {
                accept_new = false;                        
              }
              else if (new_path->rows < old_path->rows)
              {
                remove_old = true;                        
              }
              else if (new_path->rows > old_path->rows)
              {
                accept_new = false;                        
              }
              else if (compare_path_costs_fuzzily(new_path, old_path, 1.0000000001) == COSTS_BETTER1)
              {
                remove_old = true;                        
              }
              else
              {
                accept_new = false;                  
                                                       
              }
            }
            else if (outercmp == BMS_SUBSET1 && new_path->rows <= old_path->rows && new_path->parallel_safe >= old_path->parallel_safe)
            {
              remove_old = true;                        
            }
            else if (outercmp == BMS_SUBSET2 && new_path->rows >= old_path->rows && new_path->parallel_safe <= old_path->parallel_safe)
            {
              accept_new = false;                        
            }
                                                             
          }
          break;
        case COSTS_BETTER1:
          if (keyscmp != PATHKEYS_BETTER2)
          {
            outercmp = bms_subset_compare(PATH_REQ_OUTER(new_path), PATH_REQ_OUTER(old_path));
            if ((outercmp == BMS_EQUAL || outercmp == BMS_SUBSET1) && new_path->rows <= old_path->rows && new_path->parallel_safe >= old_path->parallel_safe)
            {
              remove_old = true;                        
            }
          }
          break;
        case COSTS_BETTER2:
          if (keyscmp != PATHKEYS_BETTER1)
          {
            outercmp = bms_subset_compare(PATH_REQ_OUTER(new_path), PATH_REQ_OUTER(old_path));
            if ((outercmp == BMS_EQUAL || outercmp == BMS_SUBSET2) && new_path->rows >= old_path->rows && new_path->parallel_safe <= old_path->parallel_safe)
            {
              accept_new = false;                        
            }
          }
          break;
        case COSTS_DIFFERENT:

             
                                                                 
                   
             
          break;
        }
      }
    }

       
                                                                 
       
    if (remove_old)
    {
      parent_rel->pathlist = list_delete_cell(parent_rel->pathlist, p1, p1_prev);

         
                                                                     
         
      if (!IsA(old_path, IndexPath))
      {
        pfree(old_path);
      }
                                    
    }
    else
    {
                                                                   
      if (new_path->total_cost >= old_path->total_cost)
      {
        insert_after = p1;
      }
                            
      p1_prev = p1;
    }

       
                                                                    
                                                                      
                                                                    
       
    if (!accept_new)
    {
      break;
    }
  }

  if (accept_new)
  {
                                                                    
    if (insert_after)
    {
      lappend_cell(parent_rel->pathlist, insert_after, new_path);
    }
    else
    {
      parent_rel->pathlist = lcons(new_path, parent_rel->pathlist);
    }
  }
  else
  {
                                         
    if (!IsA(new_path, IndexPath))
    {
      pfree(new_path);
    }
  }
}

   
                     
                                                                    
                                                                            
                                          
   
                                                                               
                                                                              
                                                                           
                                                                          
                                                                           
                                                                          
                                  
   
                                                                           
                                                           
   
bool
add_path_precheck(RelOptInfo *parent_rel, Cost startup_cost, Cost total_cost, List *pathkeys, Relids required_outer)
{
  List *new_path_pathkeys;
  bool consider_startup;
  ListCell *p1;

                                                                         
  new_path_pathkeys = required_outer ? NIL : pathkeys;

                                                             
  consider_startup = required_outer ? parent_rel->consider_param_startup : parent_rel->consider_startup;

  foreach (p1, parent_rel->pathlist)
  {
    Path *old_path = (Path *)lfirst(p1);
    PathKeysComparison keyscmp;

       
                                                                          
                                                                       
                                                                      
                            
       
                                                                      
       
    if (total_cost > old_path->total_cost * STD_FUZZ_FACTOR)
    {
                                                                     
      if (startup_cost > old_path->startup_cost * STD_FUZZ_FACTOR || !consider_startup)
      {
                                                          
        List *old_path_pathkeys;

        old_path_pathkeys = old_path->param_info ? NIL : old_path->pathkeys;
        keyscmp = compare_pathkeys(new_path_pathkeys, old_path_pathkeys);
        if (keyscmp == PATHKEYS_EQUAL || keyscmp == PATHKEYS_BETTER2)
        {
                                                    
          if (bms_equal(required_outer, PATH_REQ_OUTER(old_path)))
          {
                                                              
            return false;
          }
        }
      }
    }
    else
    {
         
                                                                         
                                                                    
                 
         
      break;
    }
  }

  return true;
}

   
                    
                                                                          
                                                                            
                                                                             
                                                                         
                     
   
                                                                           
                                                                         
                                                                       
   
                                                                              
                                                                          
                                                                            
                                                                           
                                                                            
                                                                          
                                                                            
                                                                             
                                                                            
                                                                               
                                                                        
                                                                         
   
                                                                       
                                                                              
                                                                              
                                                                               
                                                                        
                                            
   
                                                                        
                                                                              
                                                                             
                                                                              
                                                                      
                                            
   
void
add_partial_path(RelOptInfo *parent_rel, Path *new_path)
{
  bool accept_new = true;                                                
  ListCell *insert_after = NULL;                               
  ListCell *p1;
  ListCell *p1_prev;
  ListCell *p1_next;

                               
  CHECK_FOR_INTERRUPTS();

                                               
  Assert(new_path->parallel_safe);

                                                   
  Assert(parent_rel->consider_parallel);

     
                                                                        
                                                                          
     
  p1_prev = NULL;
  for (p1 = list_head(parent_rel->partial_pathlist); p1 != NULL; p1 = p1_next)
  {
    Path *old_path = (Path *)lfirst(p1);
    bool remove_old = false;                                 
    PathKeysComparison keyscmp;

    p1_next = lnext(p1);

                           
    keyscmp = compare_pathkeys(new_path->pathkeys, old_path->pathkeys);

                                                                         
    if (keyscmp != PATHKEYS_DIFFERENT)
    {
      if (new_path->total_cost > old_path->total_cost * STD_FUZZ_FACTOR)
      {
                                                                       
        if (keyscmp != PATHKEYS_BETTER1)
        {
          accept_new = false;
        }
      }
      else if (old_path->total_cost > new_path->total_cost * STD_FUZZ_FACTOR)
      {
                                                                       
        if (keyscmp != PATHKEYS_BETTER2)
        {
          remove_old = true;
        }
      }
      else if (keyscmp == PATHKEYS_BETTER1)
      {
                                                                     
        remove_old = true;
      }
      else if (keyscmp == PATHKEYS_BETTER2)
      {
                                                                     
        accept_new = false;
      }
      else if (old_path->total_cost > new_path->total_cost * 1.0000000001)
      {
                                                                 
        remove_old = true;
      }
      else
      {
           
                                                                
                    
           
        accept_new = false;
      }
    }

       
                                                                         
       
    if (remove_old)
    {
      parent_rel->partial_pathlist = list_delete_cell(parent_rel->partial_pathlist, p1, p1_prev);
      pfree(old_path);
                                    
    }
    else
    {
                                                                   
      if (new_path->total_cost >= old_path->total_cost)
      {
        insert_after = p1;
      }
                            
      p1_prev = p1;
    }

       
                                                                    
                                                                       
                                                       
       
    if (!accept_new)
    {
      break;
    }
  }

  if (accept_new)
  {
                                                        
    if (insert_after)
    {
      lappend_cell(parent_rel->partial_pathlist, insert_after, new_path);
    }
    else
    {
      parent_rel->partial_pathlist = lcons(new_path, parent_rel->partial_pathlist);
    }
  }
  else
  {
                                         
    pfree(new_path);
  }
}

   
                             
                                                                            
   
                                                                              
                                                                          
                                                                          
                                                                           
                      
   
bool
add_partial_path_precheck(RelOptInfo *parent_rel, Cost total_cost, List *pathkeys)
{
  ListCell *p1;

     
                                                                             
                                                                           
                                                                           
                                                                           
                                                                         
     
                                                                             
                                                                        
                                                                     
     
  foreach (p1, parent_rel->partial_pathlist)
  {
    Path *old_path = (Path *)lfirst(p1);
    PathKeysComparison keyscmp;

    keyscmp = compare_pathkeys(pathkeys, old_path->pathkeys);
    if (keyscmp != PATHKEYS_DIFFERENT)
    {
      if (total_cost > old_path->total_cost * STD_FUZZ_FACTOR && keyscmp != PATHKEYS_BETTER1)
      {
        return false;
      }
      if (old_path->total_cost > total_cost * STD_FUZZ_FACTOR && keyscmp != PATHKEYS_BETTER2)
      {
        return true;
      }
    }
  }

     
                                                                           
                                                                   
                                                                             
                                                      
     
                                                                           
                                                                       
                                                                           
                 
     
  if (!add_path_precheck(parent_rel, total_cost, total_cost, pathkeys, NULL))
  {
    return false;
  }

  return true;
}

                                                                               
                                
                                                                               

   
                       
                                                                      
               
   
Path *
create_seqscan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer, int parallel_workers)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_SeqScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = parallel_workers > 0 ? true : false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = parallel_workers;
  pathnode->pathkeys = NIL;                                   

  cost_seqscan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                          
                                                   
   
Path *
create_samplescan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_SampleScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                      

  cost_samplescan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                     
                                            
   
                              
                                                                      
                                                    
                                                                    
                                                         
                                                                              
                                              
                                                  
                                                                   
                                                          
                         
                                                        
                                                                         
                                                                             
                                   
                                                                      
   
                              
   
IndexPath *
create_index_path(PlannerInfo *root, IndexOptInfo *index, List *indexclauses, List *indexorderbys, List *indexorderbycols, List *pathkeys, ScanDirection indexscandir, bool indexonly, Relids required_outer, double loop_count, bool partial_path)
{
  IndexPath *pathnode = makeNode(IndexPath);
  RelOptInfo *rel = index->rel;

  pathnode->path.pathtype = indexonly ? T_IndexOnlyScan : T_IndexScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.pathkeys = pathkeys;

  pathnode->indexinfo = index;
  pathnode->indexclauses = indexclauses;
  pathnode->indexorderbys = indexorderbys;
  pathnode->indexorderbycols = indexorderbycols;
  pathnode->indexscandir = indexscandir;

  cost_index(pathnode, root, loop_count, partial_path);

  return pathnode;
}

   
                           
                                            
   
                                                                               
                                                                         
                                                                             
                                   
   
                                                                      
               
   
BitmapHeapPath *
create_bitmap_heap_path(PlannerInfo *root, RelOptInfo *rel, Path *bitmapqual, Relids required_outer, double loop_count, int parallel_degree)
{
  BitmapHeapPath *pathnode = makeNode(BitmapHeapPath);

  pathnode->path.pathtype = T_BitmapHeapScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = parallel_degree > 0 ? true : false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = parallel_degree;
  pathnode->path.pathkeys = NIL;                       

  pathnode->bitmapqual = bitmapqual;

  cost_bitmap_heap_scan(&pathnode->path, root, rel, pathnode->path.param_info, bitmapqual, loop_count);

  return pathnode;
}

   
                          
                                                   
   
BitmapAndPath *
create_bitmap_and_path(PlannerInfo *root, RelOptInfo *rel, List *bitmapquals)
{
  BitmapAndPath *pathnode = makeNode(BitmapAndPath);
  Relids required_outer = NULL;
  ListCell *lc;

  pathnode->path.pathtype = T_BitmapAnd;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;

     
                                                                           
                                                                           
                                                                    
     
  foreach (lc, bitmapquals)
  {
    Path *bitmapqual = (Path *)lfirst(lc);

    required_outer = bms_add_members(required_outer, PATH_REQ_OUTER(bitmapqual));
  }
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);

     
                                                                         
                                                                             
                                                                       
                                                           
     
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;

  pathnode->path.pathkeys = NIL;                       

  pathnode->bitmapquals = bitmapquals;

                                                                       
  cost_bitmap_and_node(pathnode, root);

  return pathnode;
}

   
                         
                                                  
   
BitmapOrPath *
create_bitmap_or_path(PlannerInfo *root, RelOptInfo *rel, List *bitmapquals)
{
  BitmapOrPath *pathnode = makeNode(BitmapOrPath);
  Relids required_outer = NULL;
  ListCell *lc;

  pathnode->path.pathtype = T_BitmapOr;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;

     
                                                                           
                                                                           
                                                                    
     
  foreach (lc, bitmapquals)
  {
    Path *bitmapqual = (Path *)lfirst(lc);

    required_outer = bms_add_members(required_outer, PATH_REQ_OUTER(bitmapqual));
  }
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);

     
                                                                         
                                                                             
                                                                       
                                                           
     
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;

  pathnode->path.pathkeys = NIL;                       

  pathnode->bitmapquals = bitmapquals;

                                                                       
  cost_bitmap_or_node(pathnode, root);

  return pathnode;
}

   
                       
                                                                            
   
TidPath *
create_tidscan_path(PlannerInfo *root, RelOptInfo *rel, List *tidquals, Relids required_outer)
{
  TidPath *pathnode = makeNode(TidPath);

  pathnode->path.pathtype = T_TidScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.pathkeys = NIL;                       

  pathnode->tidquals = tidquals;

  cost_tidscan(&pathnode->path, root, rel, tidquals, pathnode->path.param_info);

  return pathnode;
}

   
                      
                                                                   
               
   
                                                                              
                                                  
   
AppendPath *
create_append_path(PlannerInfo *root, RelOptInfo *rel, List *subpaths, List *partial_subpaths, List *pathkeys, Relids required_outer, int parallel_workers, bool parallel_aware, List *partitioned_rels, double rows)
{
  AppendPath *pathnode = makeNode(AppendPath);
  ListCell *l;

  Assert(!parallel_aware || parallel_workers > 0);

  pathnode->path.pathtype = T_Append;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;

     
                                                                          
                                                                       
                                                                         
                                                                          
                                                             
                                                                           
                                                                           
                          
     
  if (partitioned_rels != NIL && root && rel->reloptkind == RELOPT_BASEREL)
  {
    pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  }
  else
  {
    pathnode->path.param_info = get_appendrel_parampathinfo(rel, required_outer);
  }

  pathnode->path.parallel_aware = parallel_aware;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = parallel_workers;
  pathnode->path.pathkeys = pathkeys;
  pathnode->partitioned_rels = list_copy(partitioned_rels);

     
                                                                           
                                                                        
                                                                          
                                                                          
                                                                         
                                                                         
                   
     
  if (pathnode->path.parallel_aware)
  {
       
                                                                        
                                                                         
                       
       
    Assert(pathkeys == NIL);

    subpaths = list_qsort(subpaths, append_total_cost_compare);
    partial_subpaths = list_qsort(partial_subpaths, append_startup_cost_compare);
  }
  pathnode->first_partial_path = list_length(subpaths);
  pathnode->subpaths = list_concat(subpaths, partial_subpaths);

     
                                                                         
                                                       
     
  if (root != NULL && bms_equal(rel->relids, root->all_baserels))
  {
    pathnode->limit_tuples = root->limit_tuples;
  }
  else
  {
    pathnode->limit_tuples = -1.0;
  }

  foreach (l, pathnode->subpaths)
  {
    Path *subpath = (Path *)lfirst(l);

    pathnode->path.parallel_safe = pathnode->path.parallel_safe && subpath->parallel_safe;

                                                         
    Assert(bms_equal(PATH_REQ_OUTER(subpath), required_outer));
  }

  Assert(!parallel_aware || pathnode->path.parallel_safe);

     
                                                                          
                                                                           
                                                                            
                                                                       
                  
     
  if (list_length(pathnode->subpaths) == 1)
  {
    Path *child = (Path *)linitial(pathnode->subpaths);

    pathnode->path.rows = child->rows;
    pathnode->path.startup_cost = child->startup_cost;
    pathnode->path.total_cost = child->total_cost;
    pathnode->path.pathkeys = child->pathkeys;
  }
  else
  {
    cost_append(pathnode);
  }

                                                                           
  if (rows >= 0)
  {
    pathnode->path.rows = rows;
  }

  return pathnode;
}

   
                             
                                                                              
   
                                                                            
                                                                     
                                                                
   
static int
append_total_cost_compare(const void *a, const void *b)
{
  Path *path1 = (Path *)lfirst(*(ListCell **)a);
  Path *path2 = (Path *)lfirst(*(ListCell **)b);
  int cmp;

  cmp = compare_path_costs(path1, path2, TOTAL_COST);
  if (cmp != 0)
  {
    return -cmp;
  }
  return bms_compare(path1->parent->relids, path2->parent->relids);
}

   
                               
                                                                                
   
                                                                            
                                                                     
                                                                
   
static int
append_startup_cost_compare(const void *a, const void *b)
{
  Path *path1 = (Path *)lfirst(*(ListCell **)a);
  Path *path2 = (Path *)lfirst(*(ListCell **)b);
  int cmp;

  cmp = compare_path_costs(path1, path2, STARTUP_COST);
  if (cmp != 0)
  {
    return -cmp;
  }
  return bms_compare(path1->parent->relids, path2->parent->relids);
}

   
                            
                                                                       
               
   
MergeAppendPath *
create_merge_append_path(PlannerInfo *root, RelOptInfo *rel, List *subpaths, List *pathkeys, Relids required_outer, List *partitioned_rels)
{
  MergeAppendPath *pathnode = makeNode(MergeAppendPath);
  Cost input_startup_cost;
  Cost input_total_cost;
  ListCell *l;

  pathnode->path.pathtype = T_MergeAppend;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = get_appendrel_parampathinfo(rel, required_outer);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.pathkeys = pathkeys;
  pathnode->partitioned_rels = list_copy(partitioned_rels);
  pathnode->subpaths = subpaths;

     
                                                                         
                                                       
     
  if (bms_equal(rel->relids, root->all_baserels))
  {
    pathnode->limit_tuples = root->limit_tuples;
  }
  else
  {
    pathnode->limit_tuples = -1.0;
  }

     
                                                    
     
  pathnode->path.rows = 0;
  input_startup_cost = 0;
  input_total_cost = 0;
  foreach (l, subpaths)
  {
    Path *subpath = (Path *)lfirst(l);

    pathnode->path.rows += subpath->rows;
    pathnode->path.parallel_safe = pathnode->path.parallel_safe && subpath->parallel_safe;

    if (pathkeys_contained_in(pathkeys, subpath->pathkeys))
    {
                                                                   
      input_startup_cost += subpath->startup_cost;
      input_total_cost += subpath->total_cost;
    }
    else
    {
                                                                      
      Path sort_path;                                    

      cost_sort(&sort_path, root, pathkeys, subpath->total_cost, subpath->parent->tuples, subpath->pathtarget->width, 0.0, work_mem, pathnode->limit_tuples);
      input_startup_cost += sort_path.startup_cost;
      input_total_cost += sort_path.total_cost;
    }

                                                         
    Assert(bms_equal(PATH_REQ_OUTER(subpath), required_outer));
  }

     
                                                                            
                                                                            
                                                                  
     
  if (list_length(subpaths) == 1)
  {
    pathnode->path.startup_cost = input_startup_cost;
    pathnode->path.total_cost = input_total_cost;
  }
  else
  {
    cost_merge_append(&pathnode->path, root, pathkeys, list_length(subpaths), input_startup_cost, input_total_cost, pathnode->path.rows);
  }

  return pathnode;
}

   
                            
                                                                 
   
                                                                        
                                                                       
   
GroupResultPath *
create_group_result_path(PlannerInfo *root, RelOptInfo *rel, PathTarget *target, List *havingqual)
{
  GroupResultPath *pathnode = makeNode(GroupResultPath);

  pathnode->path.pathtype = T_Result;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
  pathnode->path.param_info = NULL;                                 
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.pathkeys = NIL;
  pathnode->quals = havingqual;

     
                                                                       
                                                                            
                   
     
  pathnode->path.rows = 1;
  pathnode->path.startup_cost = target->cost.startup;
  pathnode->path.total_cost = target->cost.startup + cpu_tuple_cost + target->cost.per_tuple;

     
                                                                           
                                                               
     
  if (havingqual)
  {
    QualCost qual_cost;

    cost_qual_eval(&qual_cost, havingqual, root);
                                                 
    pathnode->path.startup_cost += qual_cost.startup + qual_cost.per_tuple;
    pathnode->path.total_cost += qual_cost.startup + qual_cost.per_tuple;
  }

  return pathnode;
}

   
                        
                                                                    
               
   
MaterialPath *
create_material_path(RelOptInfo *rel, Path *subpath)
{
  MaterialPath *pathnode = makeNode(MaterialPath);

  Assert(subpath->parent == rel);

  pathnode->path.pathtype = T_Material;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = subpath->param_info;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
  pathnode->path.pathkeys = subpath->pathkeys;

  pathnode->subpath = subpath;

  cost_material(&pathnode->path, subpath->startup_cost, subpath->total_cost, subpath->rows, subpath->pathtarget->width);

  return pathnode;
}

   
                      
                                                                       
                                                                         
                                                                        
                                                    
   
                                                                           
                                                                            
                                          
   
UniquePath *
create_unique_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, SpecialJoinInfo *sjinfo)
{
  UniquePath *pathnode;
  Path sort_path;                                    
  Path agg_path;                                    
  MemoryContext oldcontext;
  int numCols;

                                                                 
  Assert(subpath == rel->cheapest_total_path);
  Assert(subpath->parent == rel);
                                                  
  Assert(sjinfo->jointype == JOIN_SEMI);
  Assert(bms_equal(rel->relids, sjinfo->syn_righthand));

                                           
  if (rel->cheapest_unique_path)
  {
    return (UniquePath *)rel->cheapest_unique_path;
  }

                                                       
  if (!(sjinfo->semi_can_btree || sjinfo->semi_can_hash))
  {
    return NULL;
  }

     
                                                                           
                                                                       
                                                                       
                                                                            
                                                                         
                                                                             
                                                                            
            
     
  oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));

  pathnode = makeNode(UniquePath);

  pathnode->path.pathtype = T_Unique;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = subpath->param_info;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;

     
                                                                             
                                                          
     
  pathnode->path.pathkeys = NIL;

  pathnode->subpath = subpath;
  pathnode->in_operators = sjinfo->semi_operators;
  pathnode->uniq_exprs = sjinfo->semi_rhs_exprs;

     
                                                                          
                                                                         
                                                                            
                                   
     
  if (rel->rtekind == RTE_RELATION && sjinfo->semi_can_btree && relation_has_unique_index_for(root, rel, NIL, sjinfo->semi_rhs_exprs, sjinfo->semi_operators))
  {
    pathnode->umethod = UNIQUE_PATH_NOOP;
    pathnode->path.rows = rel->rows;
    pathnode->path.startup_cost = subpath->startup_cost;
    pathnode->path.total_cost = subpath->total_cost;
    pathnode->path.pathkeys = subpath->pathkeys;

    rel->cheapest_unique_path = (Path *)pathnode;

    MemoryContextSwitchTo(oldcontext);

    return pathnode;
  }

     
                                                                             
                                                                         
                                                                           
                                                                             
                                                                          
                                                                         
                                                                            
     
  if (rel->rtekind == RTE_SUBQUERY)
  {
    RangeTblEntry *rte = planner_rt_fetch(rel->relid, root);

    if (query_supports_distinctness(rte->subquery))
    {
      List *sub_tlist_colnos;

      sub_tlist_colnos = translate_sub_tlist(sjinfo->semi_rhs_exprs, rel->relid);

      if (sub_tlist_colnos && query_is_distinct_for(rte->subquery, sub_tlist_colnos, sjinfo->semi_operators))
      {
        pathnode->umethod = UNIQUE_PATH_NOOP;
        pathnode->path.rows = rel->rows;
        pathnode->path.startup_cost = subpath->startup_cost;
        pathnode->path.total_cost = subpath->total_cost;
        pathnode->path.pathkeys = subpath->pathkeys;

        rel->cheapest_unique_path = (Path *)pathnode;

        MemoryContextSwitchTo(oldcontext);

        return pathnode;
      }
    }
  }

                                      
  pathnode->path.rows = estimate_num_groups(root, sjinfo->semi_rhs_exprs, rel->rows, NULL);
  numCols = list_length(sjinfo->semi_rhs_exprs);

  if (sjinfo->semi_can_btree)
  {
       
                                                    
       
    cost_sort(&sort_path, root, NIL, subpath->total_cost, rel->rows, subpath->pathtarget->width, 0.0, work_mem, -1.0);

       
                                                                       
                                                                   
                                                                  
                                 
       
    sort_path.total_cost += cpu_operator_cost * rel->rows * numCols;
  }

  if (sjinfo->semi_can_hash)
  {
       
                                                                         
                   
       
    int hashentrysize = subpath->pathtarget->width + 64;

    if (hashentrysize * pathnode->path.rows > work_mem * 1024L)
    {
         
                                                                 
                                                            
         
      sjinfo->semi_can_hash = false;
    }
    else
    {
      cost_agg(&agg_path, root, AGG_HASHED, NULL, numCols, pathnode->path.rows, NIL, subpath->startup_cost, subpath->total_cost, rel->rows);
    }
  }

  if (sjinfo->semi_can_btree && sjinfo->semi_can_hash)
  {
    if (agg_path.total_cost < sort_path.total_cost)
    {
      pathnode->umethod = UNIQUE_PATH_HASH;
    }
    else
    {
      pathnode->umethod = UNIQUE_PATH_SORT;
    }
  }
  else if (sjinfo->semi_can_btree)
  {
    pathnode->umethod = UNIQUE_PATH_SORT;
  }
  else if (sjinfo->semi_can_hash)
  {
    pathnode->umethod = UNIQUE_PATH_HASH;
  }
  else
  {
                                                            
    MemoryContextSwitchTo(oldcontext);
    return NULL;
  }

  if (pathnode->umethod == UNIQUE_PATH_HASH)
  {
    pathnode->path.startup_cost = agg_path.startup_cost;
    pathnode->path.total_cost = agg_path.total_cost;
  }
  else
  {
    pathnode->path.startup_cost = sort_path.startup_cost;
    pathnode->path.total_cost = sort_path.total_cost;
  }

  rel->cheapest_unique_path = (Path *)pathnode;

  MemoryContextSwitchTo(oldcontext);

  return pathnode;
}

   
                            
   
                                                                    
                   
   
GatherMergePath *
create_gather_merge_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, PathTarget *target, List *pathkeys, Relids required_outer, double *rows)
{
  GatherMergePath *pathnode = makeNode(GatherMergePath);
  Cost input_startup_cost = 0;
  Cost input_total_cost = 0;

  Assert(subpath->parallel_safe);
  Assert(pathkeys);

  pathnode->path.pathtype = T_GatherMerge;
  pathnode->path.parent = rel;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = false;

  pathnode->subpath = subpath;
  pathnode->num_workers = subpath->parallel_workers;
  pathnode->path.pathkeys = pathkeys;
  pathnode->path.pathtarget = target ? target : rel->reltarget;
  pathnode->path.rows += subpath->rows;

  if (pathkeys_contained_in(pathkeys, subpath->pathkeys))
  {
                                                                 
    input_startup_cost += subpath->startup_cost;
    input_total_cost += subpath->total_cost;
  }
  else
  {
                                                                    
    Path sort_path;                                    

    cost_sort(&sort_path, root, pathkeys, subpath->total_cost, subpath->rows, subpath->pathtarget->width, 0.0, work_mem, -1);
    input_startup_cost += sort_path.startup_cost;
    input_total_cost += sort_path.total_cost;
  }

  cost_gather_merge(pathnode, root, rel, pathnode->path.param_info, input_startup_cost, input_total_cost, rows);

  return pathnode;
}

   
                                                                          
   
                                                                                
                                                                               
                       
   
                                                                              
                                                                          
               
   
static List *
translate_sub_tlist(List *tlist, int relid)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, tlist)
  {
    Var *var = (Var *)lfirst(l);

    if (!var || !IsA(var, Var) || var->varno != relid)
    {
      return NIL;           
    }

    result = lappend_int(result, var->varattno);
  }
  return result;
}

   
                      
                                                                  
               
   
                                                                              
   
GatherPath *
create_gather_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, PathTarget *target, Relids required_outer, double *rows)
{
  GatherPath *pathnode = makeNode(GatherPath);

  Assert(subpath->parallel_safe);

  pathnode->path.pathtype = T_Gather;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = false;
  pathnode->path.parallel_workers = 0;
  pathnode->path.pathkeys = NIL;                                  

  pathnode->subpath = subpath;
  pathnode->num_workers = subpath->parallel_workers;
  pathnode->single_copy = false;

  if (pathnode->num_workers == 0)
  {
    pathnode->path.pathkeys = subpath->pathkeys;
    pathnode->num_workers = 1;
    pathnode->single_copy = true;
  }

  cost_gather(pathnode, root, rel, pathnode->path.param_info, rows);

  return pathnode;
}

   
                            
                                                           
                             
   
SubqueryScanPath *
create_subqueryscan_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, List *pathkeys, Relids required_outer)
{
  SubqueryScanPath *pathnode = makeNode(SubqueryScanPath);

  pathnode->path.pathtype = T_SubqueryScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = rel->reltarget;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
  pathnode->path.pathkeys = pathkeys;
  pathnode->subpath = subpath;

  cost_subqueryscan(pathnode, root, rel, pathnode->path.param_info);

  return pathnode;
}

   
                            
                                                                      
                             
   
Path *
create_functionscan_path(PlannerInfo *root, RelOptInfo *rel, List *pathkeys, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_FunctionScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = pathkeys;

  cost_functionscan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                             
                                                                            
                             
   
Path *
create_tablefuncscan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_TableFuncScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                 

  cost_tablefuncscan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                          
                                                              
                             
   
Path *
create_valuesscan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_ValuesScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                 

  cost_valuesscan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                       
                                                                         
                             
   
Path *
create_ctescan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_CteScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                              

  cost_ctescan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                                   
                                                                             
                   
   
Path *
create_namedtuplestorescan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_NamedTuplestoreScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                 

  cost_namedtuplestorescan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                          
                                                                       
                             
   
Path *
create_resultscan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_Result;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                 

  cost_resultscan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                             
                                                                     
                             
   
Path *
create_worktablescan_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{
  Path *pathnode = makeNode(Path);

  pathnode->pathtype = T_WorkTableScan;
  pathnode->parent = rel;
  pathnode->pathtarget = rel->reltarget;
  pathnode->param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->parallel_aware = false;
  pathnode->parallel_safe = rel->consider_parallel;
  pathnode->parallel_workers = 0;
  pathnode->pathkeys = NIL;                                 

                                                  
  cost_ctescan(pathnode, root, rel, pathnode->param_info);

  return pathnode;
}

   
                           
                                                                     
                             
   
                                                                           
                                                                           
                                                                               
                                                                            
                                                                               
   
ForeignPath *
create_foreignscan_path(PlannerInfo *root, RelOptInfo *rel, PathTarget *target, double rows, Cost startup_cost, Cost total_cost, List *pathkeys, Relids required_outer, Path *fdw_outerpath, List *fdw_private)
{
  ForeignPath *pathnode = makeNode(ForeignPath);

                                                                   
  Assert(IS_SIMPLE_REL(rel));

  pathnode->path.pathtype = T_ForeignScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target ? target : rel->reltarget;
  pathnode->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.rows = rows;
  pathnode->path.startup_cost = startup_cost;
  pathnode->path.total_cost = total_cost;
  pathnode->path.pathkeys = pathkeys;

  pathnode->fdw_outerpath = fdw_outerpath;
  pathnode->fdw_private = fdw_private;

  return pathnode;
}

   
                            
                                                               
                             
   
                                                                           
                                                                               
                                                                               
                                                                            
                                                                               
   
ForeignPath *
create_foreign_join_path(PlannerInfo *root, RelOptInfo *rel, PathTarget *target, double rows, Cost startup_cost, Cost total_cost, List *pathkeys, Relids required_outer, Path *fdw_outerpath, List *fdw_private)
{
  ForeignPath *pathnode = makeNode(ForeignPath);

     
                                                                            
                                                                   
                                                                       
                                                                          
                   
     
  if (!bms_is_empty(required_outer) || !bms_is_empty(rel->lateral_relids))
  {
    elog(ERROR, "parameterized foreign joins are not supported yet");
  }

  pathnode->path.pathtype = T_ForeignScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target ? target : rel->reltarget;
  pathnode->path.param_info = NULL;                    
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.rows = rows;
  pathnode->path.startup_cost = startup_cost;
  pathnode->path.total_cost = total_cost;
  pathnode->path.pathkeys = pathkeys;

  pathnode->fdw_outerpath = fdw_outerpath;
  pathnode->fdw_private = fdw_private;

  return pathnode;
}

   
                             
                                                                       
                                                 
   
                                                                              
                                                                             
                                                                               
                                                                            
                                                                               
   
ForeignPath *
create_foreign_upper_path(PlannerInfo *root, RelOptInfo *rel, PathTarget *target, double rows, Cost startup_cost, Cost total_cost, List *pathkeys, Path *fdw_outerpath, List *fdw_private)
{
  ForeignPath *pathnode = makeNode(ForeignPath);

     
                                                                             
                  
     
  Assert(bms_is_empty(rel->lateral_relids));

  pathnode->path.pathtype = T_ForeignScan;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target ? target : rel->reltarget;
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel;
  pathnode->path.parallel_workers = 0;
  pathnode->path.rows = rows;
  pathnode->path.startup_cost = startup_cost;
  pathnode->path.total_cost = total_cost;
  pathnode->path.pathkeys = pathkeys;

  pathnode->fdw_outerpath = fdw_outerpath;
  pathnode->fdw_private = fdw_private;

  return pathnode;
}

   
                                
                                                             
   
                                                         
   
Relids
calc_nestloop_required_outer(Relids outerrelids, Relids outer_paramrels, Relids innerrelids, Relids inner_paramrels)
{
  Relids required_outer;

                                                                       
  Assert(!bms_overlap(outer_paramrels, innerrelids));
                                                    
  if (!inner_paramrels)
  {
    return bms_copy(outer_paramrels);
  }
                                
  required_outer = bms_union(outer_paramrels, inner_paramrels);
                                                              
  required_outer = bms_del_members(required_outer, outerrelids);
                                                                       
  if (bms_is_empty(required_outer))
  {
    bms_free(required_outer);
    required_outer = NULL;
  }
  return required_outer;
}

   
                                    
                                                                  
   
                                                         
   
Relids
calc_non_nestloop_required_outer(Path *outer_path, Path *inner_path)
{
  Relids outer_paramrels = PATH_REQ_OUTER(outer_path);
  Relids inner_paramrels = PATH_REQ_OUTER(inner_path);
  Relids required_outer;

                                                    
  Assert(!bms_overlap(outer_paramrels, inner_path->parent->relids));
  Assert(!bms_overlap(inner_paramrels, outer_path->parent->relids));
                          
  required_outer = bms_union(outer_paramrels, inner_paramrels);
                                                                          
  return required_outer;
}

   
                        
                                                                     
                
   
                                   
                                           
                                                        
                                                       
                                  
                                  
                                                                      
                                                     
                                                      
   
                                    
   
NestPath *
create_nestloop_path(PlannerInfo *root, RelOptInfo *joinrel, JoinType jointype, JoinCostWorkspace *workspace, JoinPathExtraData *extra, Path *outer_path, Path *inner_path, List *restrict_clauses, List *pathkeys, Relids required_outer)
{
  NestPath *pathnode = makeNode(NestPath);
  Relids inner_req_outer = PATH_REQ_OUTER(inner_path);

     
                                                                       
                                                                             
                                                                         
                                                                    
                              
     
  if (bms_overlap(inner_req_outer, outer_path->parent->relids))
  {
    Relids inner_and_outer = bms_union(inner_path->parent->relids, inner_req_outer);
    List *jclauses = NIL;
    ListCell *lc;

    foreach (lc, restrict_clauses)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

      if (!join_clause_is_movable_into(rinfo, inner_path->parent->relids, inner_and_outer))
      {
        jclauses = lappend(jclauses, rinfo);
      }
    }
    restrict_clauses = jclauses;
  }

  pathnode->path.pathtype = T_NestLoop;
  pathnode->path.parent = joinrel;
  pathnode->path.pathtarget = joinrel->reltarget;
  pathnode->path.param_info = get_joinrel_parampathinfo(root, joinrel, outer_path, inner_path, extra->sjinfo, required_outer, &restrict_clauses);
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = joinrel->consider_parallel && outer_path->parallel_safe && inner_path->parallel_safe;
                                                                          
  pathnode->path.parallel_workers = outer_path->parallel_workers;
  pathnode->path.pathkeys = pathkeys;
  pathnode->jointype = jointype;
  pathnode->inner_unique = extra->inner_unique;
  pathnode->outerjoinpath = outer_path;
  pathnode->innerjoinpath = inner_path;
  pathnode->joinrestrictinfo = restrict_clauses;

  final_cost_nestloop(root, pathnode, workspace, extra);

  return pathnode;
}

   
                         
                                                                  
                   
   
                                  
                                           
                                                         
                                                       
                                  
                                  
                                                                      
                                                     
                                                      
                                                                     
                                                           
                                                               
                                                               
   
MergePath *
create_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, JoinType jointype, JoinCostWorkspace *workspace, JoinPathExtraData *extra, Path *outer_path, Path *inner_path, List *restrict_clauses, List *pathkeys, Relids required_outer, List *mergeclauses, List *outersortkeys, List *innersortkeys)
{
  MergePath *pathnode = makeNode(MergePath);

  pathnode->jpath.path.pathtype = T_MergeJoin;
  pathnode->jpath.path.parent = joinrel;
  pathnode->jpath.path.pathtarget = joinrel->reltarget;
  pathnode->jpath.path.param_info = get_joinrel_parampathinfo(root, joinrel, outer_path, inner_path, extra->sjinfo, required_outer, &restrict_clauses);
  pathnode->jpath.path.parallel_aware = false;
  pathnode->jpath.path.parallel_safe = joinrel->consider_parallel && outer_path->parallel_safe && inner_path->parallel_safe;
                                                                          
  pathnode->jpath.path.parallel_workers = outer_path->parallel_workers;
  pathnode->jpath.path.pathkeys = pathkeys;
  pathnode->jpath.jointype = jointype;
  pathnode->jpath.inner_unique = extra->inner_unique;
  pathnode->jpath.outerjoinpath = outer_path;
  pathnode->jpath.innerjoinpath = inner_path;
  pathnode->jpath.joinrestrictinfo = restrict_clauses;
  pathnode->path_mergeclauses = mergeclauses;
  pathnode->outersortkeys = outersortkeys;
  pathnode->innersortkeys = innersortkeys;
                                                                       
                                                                       

  final_cost_mergejoin(root, pathnode, workspace, extra);

  return pathnode;
}

   
                        
                                                                            
   
                                  
                                           
                                                        
                                                       
                                           
                                           
                                                                             
                                                                      
                                                      
                                                                   
                                                           
   
HashPath *
create_hashjoin_path(PlannerInfo *root, RelOptInfo *joinrel, JoinType jointype, JoinCostWorkspace *workspace, JoinPathExtraData *extra, Path *outer_path, Path *inner_path, bool parallel_hash, List *restrict_clauses, Relids required_outer, List *hashclauses)
{
  HashPath *pathnode = makeNode(HashPath);

  pathnode->jpath.path.pathtype = T_HashJoin;
  pathnode->jpath.path.parent = joinrel;
  pathnode->jpath.path.pathtarget = joinrel->reltarget;
  pathnode->jpath.path.param_info = get_joinrel_parampathinfo(root, joinrel, outer_path, inner_path, extra->sjinfo, required_outer, &restrict_clauses);
  pathnode->jpath.path.parallel_aware = joinrel->consider_parallel && parallel_hash;
  pathnode->jpath.path.parallel_safe = joinrel->consider_parallel && outer_path->parallel_safe && inner_path->parallel_safe;
                                                                          
  pathnode->jpath.path.parallel_workers = outer_path->parallel_workers;

     
                                                                 
                                                                           
                                                                          
                                                                            
                                                                            
                                                                           
                                                                             
                                                                          
                                  
     
  pathnode->jpath.path.pathkeys = NIL;
  pathnode->jpath.jointype = jointype;
  pathnode->jpath.inner_unique = extra->inner_unique;
  pathnode->jpath.outerjoinpath = outer_path;
  pathnode->jpath.innerjoinpath = inner_path;
  pathnode->jpath.joinrestrictinfo = restrict_clauses;
  pathnode->path_hashclauses = hashclauses;
                                                              

  final_cost_hashjoin(root, pathnode, workspace, extra);

  return pathnode;
}

   
                          
                                                                 
   
                                                           
                                                         
                                             
   
ProjectionPath *
create_projection_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, PathTarget *target)
{
  ProjectionPath *pathnode = makeNode(ProjectionPath);
  PathTarget *oldtarget;

     
                                                                          
                                                                           
                                                                        
                                                                          
                    
     
  if (IsA(subpath, ProjectionPath))
  {
    ProjectionPath *subpp = (ProjectionPath *)subpath;

    Assert(subpp->path.parent == rel);
    subpath = subpp->subpath;
    Assert(!IsA(subpath, ProjectionPath));
  }

  pathnode->path.pathtype = T_Result;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe && is_parallel_safe(root, (Node *)target->exprs);
  pathnode->path.parallel_workers = subpath->parallel_workers;
                                                 
  pathnode->path.pathkeys = subpath->pathkeys;

  pathnode->subpath = subpath;

     
                                                                            
                                                                            
                                                                          
                                                                          
                                                                             
                                                                         
                                       
     
  oldtarget = subpath->pathtarget;
  if (is_projection_capable_path(subpath) || equal(oldtarget->exprs, target->exprs))
  {
                                        
    pathnode->dummypp = true;

       
                                                                           
       
    pathnode->path.rows = subpath->rows;
    pathnode->path.startup_cost = subpath->startup_cost + (target->cost.startup - oldtarget->cost.startup);
    pathnode->path.total_cost = subpath->total_cost + (target->cost.startup - oldtarget->cost.startup) + (target->cost.per_tuple - oldtarget->cost.per_tuple) * subpath->rows;
  }
  else
  {
                                           
    pathnode->dummypp = false;

       
                                                                          
                                                               
       
    pathnode->path.rows = subpath->rows;
    pathnode->path.startup_cost = subpath->startup_cost + target->cost.startup;
    pathnode->path.total_cost = subpath->total_cost + target->cost.startup + (cpu_tuple_cost + target->cost.per_tuple) * subpath->rows;
  }

  return pathnode;
}

   
                            
                                                                             
   
                                                                            
                                                                              
                                                                            
                                                                               
             
   
                                                                            
                                                                     
                                                                            
   
                                                                            
                                                                           
                           
   
                                                           
                                                      
                                             
   
Path *
apply_projection_to_path(PlannerInfo *root, RelOptInfo *rel, Path *path, PathTarget *target)
{
  QualCost oldcost;

     
                                                                         
                              
     
  if (!is_projection_capable_path(path))
  {
    return (Path *)create_projection_path(root, rel, path, target);
  }

     
                                                                             
                                              
     
  oldcost = path->pathtarget->cost;
  path->pathtarget = target;

  path->startup_cost += target->cost.startup - oldcost.startup;
  path->total_cost += target->cost.startup - oldcost.startup + (target->cost.per_tuple - oldcost.per_tuple) * path->rows;

     
                                                                          
                                                                        
                                                                      
                                                             
     
  if ((IsA(path, GatherPath) || IsA(path, GatherMergePath)) && is_parallel_safe(root, (Node *)target->exprs))
  {
       
                                                                         
                                                                          
                                                                  
                                                              
       
                                                                        
                                                                           
                                                     
       
    if (IsA(path, GatherPath))
    {
      GatherPath *gpath = (GatherPath *)path;

      gpath->subpath = (Path *)create_projection_path(root, gpath->subpath->parent, gpath->subpath, target);
    }
    else
    {
      GatherMergePath *gmpath = (GatherMergePath *)path;

      gmpath->subpath = (Path *)create_projection_path(root, gmpath->subpath->parent, gmpath->subpath, target);
    }
  }
  else if (path->parallel_safe && !is_parallel_safe(root, (Node *)target->exprs))
  {
       
                                                                     
                                                                          
             
       
    path->parallel_safe = false;
  }

  return path;
}

   
                              
                                                                     
                                       
   
                                                           
                                                         
                                             
   
ProjectSetPath *
create_set_projection_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, PathTarget *target)
{
  ProjectSetPath *pathnode = makeNode(ProjectSetPath);
  double tlist_rows;
  ListCell *lc;

  pathnode->path.pathtype = T_ProjectSet;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe && is_parallel_safe(root, (Node *)target->exprs);
  pathnode->path.parallel_workers = subpath->parallel_workers;
                                                      
  pathnode->path.pathkeys = subpath->pathkeys;

  pathnode->subpath = subpath;

     
                                                                        
                                                          
     
  tlist_rows = 1;
  foreach (lc, target->exprs)
  {
    Node *node = (Node *)lfirst(lc);
    double itemrows;

    itemrows = expression_returns_set_rows(root, node);
    if (tlist_rows < itemrows)
    {
      tlist_rows = itemrows;
    }
  }

     
                                                                            
                                                                          
                                                                           
                          
     
  pathnode->path.rows = subpath->rows * tlist_rows;
  pathnode->path.startup_cost = subpath->startup_cost + target->cost.startup;
  pathnode->path.total_cost = subpath->total_cost + target->cost.startup + (cpu_tuple_cost + target->cost.per_tuple) * subpath->rows + (pathnode->path.rows - subpath->rows) * cpu_tuple_cost / 2;

  return pathnode;
}

   
                    
                                                                     
   
                                                           
                                                         
                                                
                                                                         
                                           
   
SortPath *
create_sort_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, List *pathkeys, double limit_tuples)
{
  SortPath *pathnode = makeNode(SortPath);

  pathnode->path.pathtype = T_Sort;
  pathnode->path.parent = rel;
                                                             
  pathnode->path.pathtarget = subpath->pathtarget;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
  pathnode->path.pathkeys = pathkeys;

  pathnode->subpath = subpath;

  cost_sort(&pathnode->path, root, pathkeys, subpath->total_cost, subpath->rows, subpath->pathtarget->width, 0.0,                                          
      work_mem, limit_tuples);

  return pathnode;
}

   
                     
                                                                               
   
                                                           
                                                         
                                             
                                                                          
                                     
                                                 
   
GroupPath *
create_group_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, List *groupClause, List *qual, double numGroups)
{
  GroupPath *pathnode = makeNode(GroupPath);
  PathTarget *target = rel->reltarget;

  pathnode->path.pathtype = T_Group;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
                                          
  pathnode->path.pathkeys = subpath->pathkeys;

  pathnode->subpath = subpath;

  pathnode->groupClause = groupClause;
  pathnode->qual = qual;

  cost_group(&pathnode->path, root, list_length(groupClause), numGroups, qual, subpath->startup_cost, subpath->total_cost, subpath->rows);

                                               
  pathnode->path.startup_cost += target->cost.startup;
  pathnode->path.total_cost += target->cost.startup + target->cost.per_tuple * pathnode->path.rows;

  return pathnode;
}

   
                            
                                                                           
                         
   
                                                                           
                                                                          
   
                                                           
                                                         
                                               
                                                 
   
                                                                        
                                                                              
   
UpperUniquePath *
create_upper_unique_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, int numCols, double numGroups)
{
  UpperUniquePath *pathnode = makeNode(UpperUniquePath);

  pathnode->path.pathtype = T_Unique;
  pathnode->path.parent = rel;
                                                               
  pathnode->path.pathtarget = subpath->pathtarget;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
                                                
  pathnode->path.pathkeys = subpath->pathkeys;

  pathnode->subpath = subpath;
  pathnode->numkeys = numCols;

     
                                                                            
                                                                            
                       
     
  pathnode->path.startup_cost = subpath->startup_cost;
  pathnode->path.total_cost = subpath->total_cost + cpu_operator_cost * subpath->rows * numCols;
  pathnode->path.rows = numGroups;

  return pathnode;
}

   
                   
                                                                        
   
                                                           
                                                         
                                             
                                                                 
                                                         
                                                                          
                                     
                                                                              
                                                                     
   
AggPath *
create_agg_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, PathTarget *target, AggStrategy aggstrategy, AggSplit aggsplit, List *groupClause, List *qual, const AggClauseCosts *aggcosts, double numGroups)
{
  AggPath *pathnode = makeNode(AggPath);

  pathnode->path.pathtype = T_Agg;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
  if (aggstrategy == AGG_SORTED)
  {
    pathnode->path.pathkeys = subpath->pathkeys;                      
  }
  else
  {
    pathnode->path.pathkeys = NIL;                          
  }
  pathnode->subpath = subpath;

  pathnode->aggstrategy = aggstrategy;
  pathnode->aggsplit = aggsplit;
  pathnode->numGroups = numGroups;
  pathnode->groupClause = groupClause;
  pathnode->qual = qual;

  cost_agg(&pathnode->path, root, aggstrategy, aggcosts, list_length(groupClause), numGroups, qual, subpath->startup_cost, subpath->total_cost, subpath->rows);

                                               
  pathnode->path.startup_cost += target->cost.startup;
  pathnode->path.total_cost += target->cost.startup + target->cost.per_tuple * pathnode->path.rows;

  return pathnode;
}

   
                            
                                                                             
   
                                                                               
                                                                     
                        
   
                                                           
                                                         
                                             
                                            
                                           
                                                                               
                                                       
   
GroupingSetsPath *
create_groupingsets_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, List *having_qual, AggStrategy aggstrategy, List *rollups, const AggClauseCosts *agg_costs, double numGroups)
{
  GroupingSetsPath *pathnode = makeNode(GroupingSetsPath);
  PathTarget *target = rel->reltarget;
  ListCell *lc;
  bool is_first = true;
  bool is_first_sort = true;

                                                      
  pathnode->path.pathtype = T_Agg;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
  pathnode->path.param_info = subpath->param_info;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
  pathnode->subpath = subpath;

     
                                                                            
                                      
     
  if (aggstrategy == AGG_SORTED && list_length(rollups) == 1 && ((RollupData *)linitial(rollups))->groupClause == NIL)
  {
    aggstrategy = AGG_PLAIN;
  }

  if (aggstrategy == AGG_MIXED && list_length(rollups) == 1)
  {
    aggstrategy = AGG_HASHED;
  }

     
                                                                             
                                                                  
                  
     
  if (aggstrategy == AGG_SORTED && list_length(rollups) == 1)
  {
    pathnode->path.pathkeys = root->group_pathkeys;
  }
  else
  {
    pathnode->path.pathkeys = NIL;
  }

  pathnode->aggstrategy = aggstrategy;
  pathnode->rollups = rollups;
  pathnode->qual = having_qual;

  Assert(rollups != NIL);
  Assert(aggstrategy != AGG_PLAIN || list_length(rollups) == 1);
  Assert(aggstrategy != AGG_MIXED || list_length(rollups) > 1);

  foreach (lc, rollups)
  {
    RollupData *rollup = lfirst(lc);
    List *gsets = rollup->gsets;
    int numGroupCols = list_length(linitial(gsets));

       
                                                                   
                                                                     
       
                                                                      
       
                                                                  
                                                                           
                          
       
    if (is_first)
    {
      cost_agg(&pathnode->path, root, aggstrategy, agg_costs, numGroupCols, rollup->numGroups, having_qual, subpath->startup_cost, subpath->total_cost, subpath->rows);
      is_first = false;
      if (!rollup->is_hashed)
      {
        is_first_sort = false;
      }
    }
    else
    {
      Path sort_path;                                    
      Path agg_path;                                    

      if (rollup->is_hashed || is_first_sort)
      {
           
                                                                   
                      
           
        cost_agg(&agg_path, root, rollup->is_hashed ? AGG_HASHED : AGG_SORTED, agg_costs, numGroupCols, rollup->numGroups, having_qual, 0.0, 0.0, subpath->rows);
        if (!rollup->is_hashed)
        {
          is_first_sort = false;
        }
      }
      else
      {
                                                                         
        cost_sort(&sort_path, root, NIL, 0.0, subpath->rows, subpath->pathtarget->width, 0.0, work_mem, -1.0);

                                             

        cost_agg(&agg_path, root, AGG_SORTED, agg_costs, numGroupCols, rollup->numGroups, having_qual, sort_path.startup_cost, sort_path.total_cost, sort_path.rows);
      }

      pathnode->path.total_cost += agg_path.total_cost;
      pathnode->path.rows += agg_path.rows;
    }
  }

                                               
  pathnode->path.startup_cost += target->cost.startup;
  pathnode->path.total_cost += target->cost.startup + target->cost.per_tuple * pathnode->path.rows;

  return pathnode;
}

   
                         
                                                                          
   
                                                           
                                             
                                                     
                                      
   
MinMaxAggPath *
create_minmaxagg_path(PlannerInfo *root, RelOptInfo *rel, PathTarget *target, List *mmaggregates, List *quals)
{
  MinMaxAggPath *pathnode = makeNode(MinMaxAggPath);
  Cost initplan_cost;
  ListCell *lc;

                                                        
  pathnode->path.pathtype = T_Result;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
                                                                           
  pathnode->path.parallel_safe = false;
  pathnode->path.parallel_workers = 0;
                                   
  pathnode->path.rows = 1;
  pathnode->path.pathkeys = NIL;

  pathnode->mmaggregates = mmaggregates;
  pathnode->quals = quals;

                                               
  initplan_cost = 0;
  foreach (lc, mmaggregates)
  {
    MinMaxAggInfo *mminfo = (MinMaxAggInfo *)lfirst(lc);

    initplan_cost += mminfo->pathcost;
  }

                                                                    
  pathnode->path.startup_cost = initplan_cost + target->cost.startup;
  pathnode->path.total_cost = initplan_cost + target->cost.startup + target->cost.per_tuple + cpu_tuple_cost;

     
                                                                           
                                                               
     
  if (quals)
  {
    QualCost qual_cost;

    cost_qual_eval(&qual_cost, quals, root);
    pathnode->path.startup_cost += qual_cost.startup;
    pathnode->path.total_cost += qual_cost.startup + qual_cost.per_tuple;
  }

  return pathnode;
}

   
                         
                                                                        
   
                                                           
                                                         
                                             
                                                 
                                                                       
   
                                                                           
                       
   
WindowAggPath *
create_windowagg_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, PathTarget *target, List *windowFuncs, WindowClause *winclause)
{
  WindowAggPath *pathnode = makeNode(WindowAggPath);

  pathnode->path.pathtype = T_WindowAgg;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
                                                
  pathnode->path.pathkeys = subpath->pathkeys;

  pathnode->subpath = subpath;
  pathnode->winclause = winclause;

     
                                                                           
                                                                       
                                                                       
                     
     
  cost_windowagg(&pathnode->path, root, windowFuncs, list_length(winclause->partitionClause), list_length(winclause->orderClause), subpath->startup_cost, subpath->total_cost, subpath->rows);

                                               
  pathnode->path.startup_cost += target->cost.startup;
  pathnode->path.total_cost += target->cost.startup + target->cost.per_tuple * pathnode->path.rows;

  return pathnode;
}

   
                     
                                                                           
   
                                                           
                                                         
                                                                           
                                                                
                                                                           
                                                                           
                                                                            
                       
                                                          
                                                       
   
SetOpPath *
create_setop_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, SetOpCmd cmd, SetOpStrategy strategy, List *distinctList, AttrNumber flagColIdx, int firstFlag, double numGroups, double outputRows)
{
  SetOpPath *pathnode = makeNode(SetOpPath);

  pathnode->path.pathtype = T_SetOp;
  pathnode->path.parent = rel;
                                                              
  pathnode->path.pathtarget = subpath->pathtarget;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
                                                            
  pathnode->path.pathkeys = (strategy == SETOP_SORTED) ? subpath->pathkeys : NIL;

  pathnode->subpath = subpath;
  pathnode->cmd = cmd;
  pathnode->strategy = strategy;
  pathnode->distinctList = distinctList;
  pathnode->flagColIdx = flagColIdx;
  pathnode->firstFlag = firstFlag;
  pathnode->numGroups = numGroups;

     
                                                                            
                                                     
     
  pathnode->path.startup_cost = subpath->startup_cost;
  pathnode->path.total_cost = subpath->total_cost + cpu_operator_cost * subpath->rows * list_length(distinctList);
  pathnode->path.rows = outputRows;

  return pathnode;
}

   
                              
                                                               
   
                                                           
                                                               
                                                            
                                             
                                                                           
                                                        
                                                 
   
                                                                        
   
RecursiveUnionPath *
create_recursiveunion_path(PlannerInfo *root, RelOptInfo *rel, Path *leftpath, Path *rightpath, PathTarget *target, List *distinctList, int wtParam, double numGroups)
{
  RecursiveUnionPath *pathnode = makeNode(RecursiveUnionPath);

  pathnode->path.pathtype = T_RecursiveUnion;
  pathnode->path.parent = rel;
  pathnode->path.pathtarget = target;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && leftpath->parallel_safe && rightpath->parallel_safe;
                                                    
  pathnode->path.parallel_workers = leftpath->parallel_workers;
                                                
  pathnode->path.pathkeys = NIL;

  pathnode->leftpath = leftpath;
  pathnode->rightpath = rightpath;
  pathnode->distinctList = distinctList;
  pathnode->wtParam = wtParam;
  pathnode->numGroups = numGroups;

  cost_recursive_union(&pathnode->path, leftpath, rightpath);

  return pathnode;
}

   
                        
                                                            
   
                                                           
                                                         
                                         
                                                          
   
LockRowsPath *
create_lockrows_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, List *rowMarks, int epqParam)
{
  LockRowsPath *pathnode = makeNode(LockRowsPath);

  pathnode->path.pathtype = T_LockRows;
  pathnode->path.parent = rel;
                                                                 
  pathnode->path.pathtarget = subpath->pathtarget;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = false;
  pathnode->path.parallel_workers = 0;
  pathnode->path.rows = subpath->rows;

     
                                                                             
                                                 
     
  pathnode->path.pathkeys = NIL;

  pathnode->subpath = subpath;
  pathnode->rowMarks = rowMarks;
  pathnode->epqParam = epqParam;

     
                                                                       
                                                                      
                             
     
  pathnode->path.startup_cost = subpath->startup_cost;
  pathnode->path.total_cost = subpath->total_cost + cpu_tuple_cost * subpath->rows;

  return pathnode;
}

   
                           
                                                                             
   
                                                           
                                     
                                                              
                                                               
                                                                       
                                                                            
                                                                       
                                                                              
                                                                       
                                                             
                                                               
                                                                
                                                           
                                                   
                                                          
   
ModifyTablePath *
create_modifytable_path(PlannerInfo *root, RelOptInfo *rel, CmdType operation, bool canSetTag, Index nominalRelation, Index rootRelation, bool partColsUpdated, List *resultRelations, List *subpaths, List *subroots, List *withCheckOptionLists, List *returningLists, List *rowMarks, OnConflictExpr *onconflict, int epqParam)
{
  ModifyTablePath *pathnode = makeNode(ModifyTablePath);
  double total_size;
  ListCell *lc;

  Assert(list_length(resultRelations) == list_length(subpaths));
  Assert(list_length(resultRelations) == list_length(subroots));
  Assert(withCheckOptionLists == NIL || list_length(resultRelations) == list_length(withCheckOptionLists));
  Assert(returningLists == NIL || list_length(resultRelations) == list_length(returningLists));

  pathnode->path.pathtype = T_ModifyTable;
  pathnode->path.parent = rel;
                                                                   
  pathnode->path.pathtarget = rel->reltarget;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = false;
  pathnode->path.parallel_workers = 0;
  pathnode->path.pathkeys = NIL;

     
                                                                  
     
                                                                    
                                                                    
                                                                  
                                                                        
                                                                           
                                      
     
  pathnode->path.startup_cost = 0;
  pathnode->path.total_cost = 0;
  pathnode->path.rows = 0;
  total_size = 0;
  foreach (lc, subpaths)
  {
    Path *subpath = (Path *)lfirst(lc);

    if (lc == list_head(subpaths))                  
    {
      pathnode->path.startup_cost = subpath->startup_cost;
    }
    pathnode->path.total_cost += subpath->total_cost;
    pathnode->path.rows += subpath->rows;
    total_size += subpath->pathtarget->width * subpath->rows;
  }

     
                                                                         
                                                                           
                                                                          
                                                 
     
  if (pathnode->path.rows > 0)
  {
    total_size /= pathnode->path.rows;
  }
  pathnode->path.pathtarget->width = rint(total_size);

  pathnode->operation = operation;
  pathnode->canSetTag = canSetTag;
  pathnode->nominalRelation = nominalRelation;
  pathnode->rootRelation = rootRelation;
  pathnode->partColsUpdated = partColsUpdated;
  pathnode->resultRelations = resultRelations;
  pathnode->subpaths = subpaths;
  pathnode->subroots = subroots;
  pathnode->withCheckOptionLists = withCheckOptionLists;
  pathnode->returningLists = returningLists;
  pathnode->rowMarks = rowMarks;
  pathnode->onconflict = onconflict;
  pathnode->epqParam = epqParam;

  return pathnode;
}

   
                     
                                                                
   
                                                                     
                                                                           
                                                                         
                                                                        
                           
   
                                                           
                                                         
                                                          
                                                        
                                                                
                                                              
   
LimitPath *
create_limit_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath, Node *limitOffset, Node *limitCount, int64 offset_est, int64 count_est)
{
  LimitPath *pathnode = makeNode(LimitPath);

  pathnode->path.pathtype = T_Limit;
  pathnode->path.parent = rel;
                                                              
  pathnode->path.pathtarget = subpath->pathtarget;
                                                                      
  pathnode->path.param_info = NULL;
  pathnode->path.parallel_aware = false;
  pathnode->path.parallel_safe = rel->consider_parallel && subpath->parallel_safe;
  pathnode->path.parallel_workers = subpath->parallel_workers;
  pathnode->path.rows = subpath->rows;
  pathnode->path.startup_cost = subpath->startup_cost;
  pathnode->path.total_cost = subpath->total_cost;
  pathnode->path.pathkeys = subpath->pathkeys;
  pathnode->subpath = subpath;
  pathnode->limitOffset = limitOffset;
  pathnode->limitCount = limitCount;

     
                                                                           
     
  adjust_limit_rows_costs(&pathnode->path.rows, &pathnode->path.startup_cost, &pathnode->path.total_cost, offset_est, count_est);

  return pathnode;
}

   
                           
                                                                              
                   
   
                                                                       
                                                                               
            
   
                                                                            
                                            
   
                                                                         
                                                                               
                                                                
   
void
adjust_limit_rows_costs(double *rows,                       
    Cost *startup_cost,                                     
    Cost *total_cost,                                       
    int64 offset_est, int64 count_est)
{
  double input_rows = *rows;
  Cost input_startup_cost = *startup_cost;
  Cost input_total_cost = *total_cost;

  if (offset_est != 0)
  {
    double offset_rows;

    if (offset_est > 0)
    {
      offset_rows = (double)offset_est;
    }
    else
    {
      offset_rows = clamp_row_est(input_rows * 0.10);
    }
    if (offset_rows > *rows)
    {
      offset_rows = *rows;
    }
    if (input_rows > 0)
    {
      *startup_cost += (input_total_cost - input_startup_cost) * offset_rows / input_rows;
    }
    *rows -= offset_rows;
    if (*rows < 1)
    {
      *rows = 1;
    }
  }

  if (count_est != 0)
  {
    double count_rows;

    if (count_est > 0)
    {
      count_rows = (double)count_est;
    }
    else
    {
      count_rows = clamp_row_est(input_rows * 0.10);
    }
    if (count_rows > *rows)
    {
      count_rows = *rows;
    }
    if (input_rows > 0)
    {
      *total_cost = *startup_cost + (input_total_cost - input_startup_cost) * count_rows / input_rows;
    }
    *rows = count_rows;
    if (*rows < 1)
    {
      *rows = 1;
    }
  }
}

   
                       
                                                              
   
                                                                          
                                                                            
                                                                           
                                                                            
                                                                           
                                              
   
                                                                            
                                                                            
                                                                           
                                                                         
                                 
   
Path *
reparameterize_path(PlannerInfo *root, Path *path, Relids required_outer, double loop_count)
{
  RelOptInfo *rel = path->parent;

                                                                
  if (!bms_is_subset(PATH_REQ_OUTER(path), required_outer))
  {
    return NULL;
  }
  switch (path->pathtype)
  {
  case T_SeqScan:
    return create_seqscan_path(root, rel, required_outer, 0);
  case T_SampleScan:
    return (Path *)create_samplescan_path(root, rel, required_outer);
  case T_IndexScan:
  case T_IndexOnlyScan:
  {
    IndexPath *ipath = (IndexPath *)path;
    IndexPath *newpath = makeNode(IndexPath);

       
                                                                   
                                                               
                                                              
                                                                
                          
       
    memcpy(newpath, ipath, sizeof(IndexPath));
    newpath->path.param_info = get_baserel_parampathinfo(root, rel, required_outer);
    cost_index(newpath, root, loop_count, false);
    return (Path *)newpath;
  }
  case T_BitmapHeapScan:
  {
    BitmapHeapPath *bpath = (BitmapHeapPath *)path;

    return (Path *)create_bitmap_heap_path(root, rel, bpath->bitmapqual, required_outer, loop_count, 0);
  }
  case T_SubqueryScan:
  {
    SubqueryScanPath *spath = (SubqueryScanPath *)path;

    return (Path *)create_subqueryscan_path(root, rel, spath->subpath, spath->path.pathkeys, required_outer);
  }
  case T_Result:
                                                  
    if (IsA(path, Path))
    {
      return create_resultscan_path(root, rel, required_outer);
    }
    break;
  case T_Append:
  {
    AppendPath *apath = (AppendPath *)path;
    List *childpaths = NIL;
    List *partialpaths = NIL;
    int i;
    ListCell *lc;

                                     
    i = 0;
    foreach (lc, apath->subpaths)
    {
      Path *spath = (Path *)lfirst(lc);

      spath = reparameterize_path(root, spath, required_outer, loop_count);
      if (spath == NULL)
      {
        return NULL;
      }
                                                             
      if (i < apath->first_partial_path)
      {
        childpaths = lappend(childpaths, spath);
      }
      else
      {
        partialpaths = lappend(partialpaths, spath);
      }
      i++;
    }
    return (Path *)create_append_path(root, rel, childpaths, partialpaths, apath->path.pathkeys, required_outer, apath->path.parallel_workers, apath->path.parallel_aware, apath->partitioned_rels, -1);
  }
  default:
    break;
  }
  return NULL;
}

   
                                
                                                                           
                                                                         
   
                                                                           
                                                                             
                                                                           
                                                                  
                                                                          
               
   
                                                                            
                                                                           
                                   
   
                                                                            
   
Path *
reparameterize_path_by_child(PlannerInfo *root, Path *path, RelOptInfo *child_rel)
{

#define FLAT_COPY_PATH(newnode, node, nodetype) ((newnode) = makeNode(nodetype), memcpy((newnode), (node), sizeof(nodetype)))

#define ADJUST_CHILD_ATTRS(node) ((node) = (List *)adjust_appendrel_attrs_multilevel(root, (Node *)(node), child_rel->relids, child_rel->top_parent_relids))

#define REPARAMETERIZE_CHILD_PATH(path)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (path) = reparameterize_path_by_child(root, (path), child_rel);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((path) == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  } while (0)

#define REPARAMETERIZE_CHILD_PATH_LIST(pathlist)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((pathlist) != NIL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      (pathlist) = reparameterize_pathlist_by_child(root, (pathlist), child_rel);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      if ((pathlist) == NIL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
        return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

  Path *new_path;
  ParamPathInfo *new_ppi;
  ParamPathInfo *old_ppi;
  Relids required_outer;

     
                                                                          
                                      
     
  if (!path->param_info || !bms_overlap(PATH_REQ_OUTER(path), child_rel->top_parent_relids))
  {
    return path;
  }

     
                                                                
     
                                                                             
                                                                            
                                                                           
                                                                           
                                                                      
                                                                           
                                                                            
                           
     
  switch (nodeTag(path))
  {
  case T_Path:
    FLAT_COPY_PATH(new_path, path, Path);
    break;

  case T_IndexPath:
  {
    IndexPath *ipath;

    FLAT_COPY_PATH(ipath, path, IndexPath);
    ADJUST_CHILD_ATTRS(ipath->indexclauses);
    new_path = (Path *)ipath;
  }
  break;

  case T_BitmapHeapPath:
  {
    BitmapHeapPath *bhpath;

    FLAT_COPY_PATH(bhpath, path, BitmapHeapPath);
    REPARAMETERIZE_CHILD_PATH(bhpath->bitmapqual);
    new_path = (Path *)bhpath;
  }
  break;

  case T_BitmapAndPath:
  {
    BitmapAndPath *bapath;

    FLAT_COPY_PATH(bapath, path, BitmapAndPath);
    REPARAMETERIZE_CHILD_PATH_LIST(bapath->bitmapquals);
    new_path = (Path *)bapath;
  }
  break;

  case T_BitmapOrPath:
  {
    BitmapOrPath *bopath;

    FLAT_COPY_PATH(bopath, path, BitmapOrPath);
    REPARAMETERIZE_CHILD_PATH_LIST(bopath->bitmapquals);
    new_path = (Path *)bopath;
  }
  break;

  case T_ForeignPath:
  {
    ForeignPath *fpath;
    ReparameterizeForeignPathByChild_function rfpc_func;

    FLAT_COPY_PATH(fpath, path, ForeignPath);
    if (fpath->fdw_outerpath)
    {
      REPARAMETERIZE_CHILD_PATH(fpath->fdw_outerpath);
    }

                                     
    rfpc_func = path->parent->fdwroutine->ReparameterizeForeignPathByChild;
    if (rfpc_func)
    {
      fpath->fdw_private = rfpc_func(root, fpath->fdw_private, child_rel);
    }
    new_path = (Path *)fpath;
  }
  break;

  case T_CustomPath:
  {
    CustomPath *cpath;

    FLAT_COPY_PATH(cpath, path, CustomPath);
    REPARAMETERIZE_CHILD_PATH_LIST(cpath->custom_paths);
    if (cpath->methods && cpath->methods->ReparameterizeCustomPathByChild)
    {
      cpath->custom_private = cpath->methods->ReparameterizeCustomPathByChild(root, cpath->custom_private, child_rel);
    }
    new_path = (Path *)cpath;
  }
  break;

  case T_NestPath:
  {
    JoinPath *jpath;

    FLAT_COPY_PATH(jpath, path, NestPath);

    REPARAMETERIZE_CHILD_PATH(jpath->outerjoinpath);
    REPARAMETERIZE_CHILD_PATH(jpath->innerjoinpath);
    ADJUST_CHILD_ATTRS(jpath->joinrestrictinfo);
    new_path = (Path *)jpath;
  }
  break;

  case T_MergePath:
  {
    JoinPath *jpath;
    MergePath *mpath;

    FLAT_COPY_PATH(mpath, path, MergePath);

    jpath = (JoinPath *)mpath;
    REPARAMETERIZE_CHILD_PATH(jpath->outerjoinpath);
    REPARAMETERIZE_CHILD_PATH(jpath->innerjoinpath);
    ADJUST_CHILD_ATTRS(jpath->joinrestrictinfo);
    ADJUST_CHILD_ATTRS(mpath->path_mergeclauses);
    new_path = (Path *)mpath;
  }
  break;

  case T_HashPath:
  {
    JoinPath *jpath;
    HashPath *hpath;

    FLAT_COPY_PATH(hpath, path, HashPath);

    jpath = (JoinPath *)hpath;
    REPARAMETERIZE_CHILD_PATH(jpath->outerjoinpath);
    REPARAMETERIZE_CHILD_PATH(jpath->innerjoinpath);
    ADJUST_CHILD_ATTRS(jpath->joinrestrictinfo);
    ADJUST_CHILD_ATTRS(hpath->path_hashclauses);
    new_path = (Path *)hpath;
  }
  break;

  case T_AppendPath:
  {
    AppendPath *apath;

    FLAT_COPY_PATH(apath, path, AppendPath);
    REPARAMETERIZE_CHILD_PATH_LIST(apath->subpaths);
    new_path = (Path *)apath;
  }
  break;

  case T_GatherPath:
  {
    GatherPath *gpath;

    FLAT_COPY_PATH(gpath, path, GatherPath);
    REPARAMETERIZE_CHILD_PATH(gpath->subpath);
    new_path = (Path *)gpath;
  }
  break;

  default:

                                                        
    return NULL;
  }

     
                                                                          
                                                                           
                                                                  
     
  old_ppi = new_path->param_info;
  required_outer = adjust_child_relids_multilevel(root, old_ppi->ppi_req_outer, child_rel->relids, child_rel->top_parent_relids);

                                                                          
  new_ppi = find_param_path_info(new_path->parent, required_outer);

     
                                                                           
                                                                           
                                         
     
  if (new_ppi == NULL)
  {
    MemoryContext oldcontext;
    RelOptInfo *rel = path->parent;

    oldcontext = MemoryContextSwitchTo(GetMemoryChunkContext(rel));

    new_ppi = makeNode(ParamPathInfo);
    new_ppi->ppi_req_outer = bms_copy(required_outer);
    new_ppi->ppi_rows = old_ppi->ppi_rows;
    new_ppi->ppi_clauses = old_ppi->ppi_clauses;
    ADJUST_CHILD_ATTRS(new_ppi->ppi_clauses);
    rel->ppilist = lappend(rel->ppilist, new_ppi);

    MemoryContextSwitchTo(oldcontext);
  }
  bms_free(required_outer);

  new_path->param_info = new_ppi;

     
                                                                   
                                                                           
                                                              
     
  if (bms_overlap(path->parent->lateral_relids, child_rel->top_parent_relids))
  {
    new_path->pathtarget = copy_pathtarget(new_path->pathtarget);
    ADJUST_CHILD_ATTRS(new_path->pathtarget->exprs);
  }

  return new_path;
}

   
                                    
                                                                           
   
static List *
reparameterize_pathlist_by_child(PlannerInfo *root, List *pathlist, RelOptInfo *child_rel)
{
  ListCell *lc;
  List *result = NIL;

  foreach (lc, pathlist)
  {
    Path *path = reparameterize_path_by_child(root, lfirst(lc), child_rel);

    if (path == NULL)
    {
      list_free(result);
      return NIL;
    }

    result = lappend(result, path);
  }

  return result;
}
