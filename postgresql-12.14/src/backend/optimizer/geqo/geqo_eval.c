                                                                           
   
               
                                      
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

#include "postgres.h"

#include <float.h>
#include <limits.h>
#include <math.h>

#include "optimizer/geqo.h"
#include "optimizer/joininfo.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "utils/memutils.h"

                                                             
typedef struct
{
  RelOptInfo *joinrel;                                       
  int size;                                                    
} Clump;

static List *
merge_clump(PlannerInfo *root, List *clumps, Clump *new_clump, int num_gene, bool force);
static bool
desirable_join(PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel);

   
             
   
                                                                    
   
                                                                   
                    
   
Cost
geqo_eval(PlannerInfo *root, Gene *tour, int num_gene)
{
  MemoryContext mycontext;
  MemoryContext oldcxt;
  RelOptInfo *joinrel;
  Cost fitness;
  int savelength;
  struct HTAB *savehash;

     
                                                                     
                                    
     
                                                                             
                                                                          
                                                                           
                                                   
     
  mycontext = AllocSetContextCreate(CurrentMemoryContext, "GEQO", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(mycontext);

     
                                                                          
                                                                        
                                                                           
                                                                             
                                                                             
                                            
     
                                                                             
                                                                           
                                                                           
                                                      
     
                                                                    
     
  savelength = list_length(root->join_rel_list);
  savehash = root->join_rel_hash;
  Assert(root->join_rel_level == NULL);

  root->join_rel_hash = NULL;

                                                                      
  joinrel = gimme_tree(root, tour, num_gene);

     
                                               
     
                                                                         
                                                                 
                                         
     
  if (joinrel)
  {
    Path *best_path = joinrel->cheapest_total_path;

    fitness = best_path->total_cost;
  }
  else
  {
    fitness = DBL_MAX;
  }

     
                                                                      
                       
     
  root->join_rel_list = list_truncate(root->join_rel_list, savelength);
  root->join_rel_hash = savehash;

                                                         
  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(mycontext);

  return fitness;
}

   
              
                                                                         
            
   
                                                            
   
                                                                        
                                                                       
                                          
   
                                                                              
                                                                        
                                                                            
                                                                              
                                                                         
                                                      
   
                                                                       
                                                                         
                                                                              
                                                                               
                                                                           
                                                                      
                                                                              
                                                                           
   
RelOptInfo *
gimme_tree(PlannerInfo *root, Gene *tour, int num_gene)
{
  GeqoPrivateData *private = (GeqoPrivateData *)root->join_search_private;
  List *clumps;
  int rel_count;

     
                                                                           
                                                                         
                                                                          
                                                                             
                                                                          
                                                                            
                                                                           
                                                                          
                                                    
     
  clumps = NIL;

  for (rel_count = 0; rel_count < num_gene; rel_count++)
  {
    int cur_rel_index;
    RelOptInfo *cur_rel;
    Clump *cur_clump;

                                     
    cur_rel_index = (int)tour[rel_count];
    cur_rel = (RelOptInfo *)list_nth(private->initial_rels, cur_rel_index - 1);

                                         
    cur_clump = (Clump *)palloc(sizeof(Clump));
    cur_clump->joinrel = cur_rel;
    cur_clump->size = 1;

                                                                   
    clumps = merge_clump(root, clumps, cur_clump, num_gene, false);
  }

  if (list_length(clumps) > 1)
  {
                                                             
    List *fclumps;
    ListCell *lc;

    fclumps = NIL;
    foreach (lc, clumps)
    {
      Clump *clump = (Clump *)lfirst(lc);

      fclumps = merge_clump(root, fclumps, clump, num_gene, true);
    }
    clumps = fclumps;
  }

                                                         
  if (list_length(clumps) != 1)
  {
    return NULL;
  }

  return ((Clump *)linitial(clumps))->joinrel;
}

   
                                                                    
   
                                                                     
                                                                   
                                                                  
                                          
   
                                                                       
                                                                   
                      
   
static List *
merge_clump(PlannerInfo *root, List *clumps, Clump *new_clump, int num_gene, bool force)
{
  ListCell *prev;
  ListCell *lc;

                                                   
  prev = NULL;
  foreach (lc, clumps)
  {
    Clump *old_clump = (Clump *)lfirst(lc);

    if (force || desirable_join(root, old_clump->joinrel, new_clump->joinrel))
    {
      RelOptInfo *joinrel;

         
                                                                         
                                                                     
                                                                      
                                             
         
      joinrel = make_join_rel(root, old_clump->joinrel, new_clump->joinrel);

                                                     
      if (joinrel)
      {
                                                   
        generate_partitionwise_join_paths(root, joinrel);

           
                                                                    
                                                                       
                                                      
                              
           
        if (old_clump->size + new_clump->size < num_gene)
        {
          generate_gather_paths(root, joinrel, false);
        }

                                                               
        set_cheapest(joinrel);

                                       
        old_clump->joinrel = joinrel;
        old_clump->size += new_clump->size;
        pfree(new_clump);

                                        
        clumps = list_delete_cell(clumps, lc, prev);

           
                                                                
                                                                      
                             
           
        return merge_clump(root, clumps, old_clump, num_gene, force);
      }
    }
    prev = lc;
  }

     
                                                                          
                                                                         
                                                             
     
  if (clumps == NIL || new_clump->size == 1)
  {
    return lappend(clumps, new_clump);
  }

                                        
  lc = list_head(clumps);
  if (new_clump->size > ((Clump *)lfirst(lc))->size)
  {
    return lcons(new_clump, clumps);
  }

                                              
  for (;;)
  {
    ListCell *nxt = lnext(lc);

    if (nxt == NULL || new_clump->size > ((Clump *)lfirst(nxt))->size)
    {
      break;                                          
    }
    lc = nxt;
  }
  lappend_cell(clumps, lc, new_clump);

  return clumps;
}

   
                                                                      
   
static bool
desirable_join(PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
     
                                                                             
                                                  
     
  if (have_relevant_joinclause(root, outer_rel, inner_rel) || have_join_order_restriction(root, outer_rel, inner_rel))
  {
    return true;
  }

                                               
  return false;
}
