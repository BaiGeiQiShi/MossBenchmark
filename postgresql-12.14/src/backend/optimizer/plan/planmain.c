                                                                            
   
              
                                     
   
                                                                        
                                                                        
                                                                              
                                                                         
                                                            
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/orclauses.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"

   
                 
                                                                     
                                                           
   
                                                                          
                                                                        
                                                                       
                                                                        
   
                                    
                                                                               
                                                          
   
                                                                          
                                                                          
                                                                         
                                                                               
                                                               
   
RelOptInfo *
query_planner(PlannerInfo *root, query_pathkeys_callback qp_callback, void *qp_extra)
{
  Query *parse = root->parse;
  List *joinlist;
  RelOptInfo *final_rel;

     
                                  
     
                                                                           
           
     
  root->join_rel_list = NIL;
  root->join_rel_hash = NULL;
  root->join_rel_level = NULL;
  root->join_cur_level = 0;
  root->canon_pathkeys = NIL;
  root->left_join_clauses = NIL;
  root->right_join_clauses = NIL;
  root->full_join_clauses = NIL;
  root->join_info_list = NIL;
  root->placeholder_list = NIL;
  root->fkey_list = NIL;
  root->initial_rels = NIL;

     
                                                                           
                                                                           
                                        
     
  setup_simple_rel_arrays(root);

     
                                                                             
                                                                             
                                                                       
                                                                      
     
  Assert(parse->jointree->fromlist != NIL);
  if (list_length(parse->jointree->fromlist) == 1)
  {
    Node *jtnode = (Node *)linitial(parse->jointree->fromlist);

    if (IsA(jtnode, RangeTblRef))
    {
      int varno = ((RangeTblRef *)jtnode)->rtindex;
      RangeTblEntry *rte = root->simple_rte_array[varno];

      Assert(rte != NULL);
      if (rte->rtekind == RTE_RESULT)
      {
                                                 
        final_rel = build_simple_rel(root, varno, NULL);

           
                                                                     
                                                              
                                                                  
                                                                   
                                                                      
                                                            
                                                                     
                                                                 
                                         
           
        if (root->glob->parallelModeOK && force_parallel_mode != FORCE_PARALLEL_OFF)
        {
          final_rel->consider_parallel = is_parallel_safe(root, parse->jointree->quals);
        }

           
                                                                      
                                                                    
                                                                      
                                                                       
                                                                     
                                  
           
        add_path(final_rel, (Path *)create_group_result_path(root, final_rel, final_rel->reltarget, (List *)parse->jointree->quals));

                                                                
        set_cheapest(final_rel);

           
                                                                   
                                                   
           
        (*qp_callback)(root, qp_extra);

        return final_rel;
      }
    }
  }

     
                                                                       
                             
     
  setup_append_rel_array(root);

     
                                                                          
                                                                    
     
                                                                             
                                                                           
                                                                           
                                        
     
  add_base_rels_to_query(root, (Node *)parse->jointree);

     
                                                                     
                                                                         
                                                                            
                                                                             
                                                                            
                                                                             
                                                                             
                
     
  build_base_rel_tlists(root, root->processed_tlist);

  find_placeholders_in_jointree(root);

  find_lateral_references(root);

  joinlist = deconstruct_jointree(root);

     
                                                                         
                                                                      
                           
     
  reconsider_outer_join_clauses(root);

     
                                                                           
                                                                          
             
     
  generate_base_implied_equalities(root);

     
                                                                         
                                                                        
                                           
     
  (*qp_callback)(root, qp_extra);

     
                                                                             
                                                                            
                                                                         
                                                                            
                       
     
  fix_placeholder_input_needed_levels(root);

     
                                                                        
                                                                           
                                                                            
     
  joinlist = remove_useless_joins(root, joinlist);

     
                                                                             
                                                                     
     
  reduce_unique_semijoins(root);

     
                                                                           
                                                                    
                                             
     
  add_placeholders_to_base_rels(root);

     
                                                                     
                                 
     
  create_lateral_join_info(root);

     
                                                                             
                                                                             
                                                                    
                                  
     
  match_foreign_keys_to_quals(root);

     
                                                                  
                                  
     
  extract_restriction_or_clauses(root);

     
                                                                         
                                                                           
                                                                          
                                                                           
                                                                          
                                                                           
     
  add_other_rels_to_query(root);

     
                                       
     
  final_rel = make_one_rel(root, joinlist);

                                                  
  if (!final_rel || !final_rel->cheapest_total_path || final_rel->cheapest_total_path->param_info != NULL)
  {
    elog(ERROR, "failed to construct the join relation");
  }

  return final_rel;
}
