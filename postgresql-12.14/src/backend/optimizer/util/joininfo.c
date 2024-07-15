                                                                            
   
              
                                         
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "optimizer/joininfo.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"

   
                            
                                                       
                             
   
                                                                          
                                                          
                                                  
                                                                      
                                                                         
                                                                            
                                          
   
bool
have_relevant_joinclause(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{
  bool result = false;
  List *joininfo;
  Relids other_relids;
  ListCell *l;

     
                                                                        
                  
     
  if (list_length(rel1->joininfo) <= list_length(rel2->joininfo))
  {
    joininfo = rel1->joininfo;
    other_relids = rel2->relids;
  }
  else
  {
    joininfo = rel2->joininfo;
    other_relids = rel1->relids;
  }

  foreach (l, joininfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);

    if (bms_overlap(other_relids, rinfo->required_relids))
    {
      result = true;
      break;
    }
  }

     
                                                                            
                                                                
     
  if (!result && rel1->has_eclass_joins && rel2->has_eclass_joins)
  {
    result = have_relevant_eclass_joinclause(root, rel1, rel2);
  }

  return result;
}

   
                           
                                                                           
   
                                                                            
                                                                           
                                                                            
                           
   
                                            
                                                                           
                                     
   
void
add_join_clause_to_rels(PlannerInfo *root, RestrictInfo *restrictinfo, Relids join_relids)
{
  int cur_relid;

  cur_relid = -1;
  while ((cur_relid = bms_next_member(join_relids, cur_relid)) >= 0)
  {
    RelOptInfo *rel = find_base_rel(root, cur_relid);

    rel->joininfo = lappend(rel->joininfo, restrictinfo);
  }
}

   
                                
                                                                
   
                                                                           
                                                       
   
                                            
                                                                           
                                     
   
void
remove_join_clause_from_rels(PlannerInfo *root, RestrictInfo *restrictinfo, Relids join_relids)
{
  int cur_relid;

  cur_relid = -1;
  while ((cur_relid = bms_next_member(join_relids, cur_relid)) >= 0)
  {
    RelOptInfo *rel = find_base_rel(root, cur_relid);

       
                                                                     
                   
       
    Assert(list_member_ptr(rel->joininfo, restrictinfo));
    rel->joininfo = list_delete_ptr(rel->joininfo, restrictinfo);
  }
}
