                                                                            
   
                  
                                              
   
                                                                         
                                                                        
   
   
                  
                                               
   
                                                                            
   
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/restrictinfo.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)

static RestrictInfo *
make_restrictinfo_internal(PlannerInfo *root, Expr *clause, Expr *orclause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids);
static Expr *
make_sub_restrictinfos(PlannerInfo *root, Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids);

   
                     
   
                                                                 
   
                                                                           
                                                                              
                                                          
                                                                               
                                   
   
                                                                             
                                                                              
          
   
RestrictInfo *
make_restrictinfo(Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{
  return make_restrictinfo_new(NULL, clause, is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, required_relids, outer_relids, nullable_relids);
}

RestrictInfo *
make_restrictinfo_new(PlannerInfo *root, Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{
     
                                                                             
                                                             
     
  if (is_orclause(clause))
  {
    return (RestrictInfo *)make_sub_restrictinfos(root, clause, is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, required_relids, outer_relids, nullable_relids);
  }

                                                                    
  Assert(!is_andclause(clause));

  return make_restrictinfo_internal(root, clause, NULL, is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, required_relids, outer_relids, nullable_relids);
}

   
                              
   
                                                                  
   
static RestrictInfo *
make_restrictinfo_internal(PlannerInfo *root, Expr *clause, Expr *orclause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{
  RestrictInfo *restrictinfo = makeNode(RestrictInfo);

  restrictinfo->clause = clause;
  restrictinfo->orclause = orclause;
  restrictinfo->is_pushed_down = is_pushed_down;
  restrictinfo->outerjoin_delayed = outerjoin_delayed;
  restrictinfo->pseudoconstant = pseudoconstant;
  restrictinfo->can_join = false;                        
  restrictinfo->security_level = security_level;
  restrictinfo->outer_relids = outer_relids;
  restrictinfo->nullable_relids = nullable_relids;

     
                                                                             
                                                                             
                                                                    
     
  if (security_level > 0)
  {
    restrictinfo->leakproof = !contain_leaked_vars((Node *)clause);
  }
  else
  {
    restrictinfo->leakproof = false;                           
  }

     
                                                                           
                                          
     
  if (is_opclause(clause) && list_length(((OpExpr *)clause)->args) == 2)
  {
    restrictinfo->left_relids = pull_varnos(root, get_leftop(clause));
    restrictinfo->right_relids = pull_varnos(root, get_rightop(clause));

    restrictinfo->clause_relids = bms_union(restrictinfo->left_relids, restrictinfo->right_relids);

       
                                                                       
                                                                        
                                                                         
                                                                 
       
    if (!bms_is_empty(restrictinfo->left_relids) && !bms_is_empty(restrictinfo->right_relids) && !bms_overlap(restrictinfo->left_relids, restrictinfo->right_relids))
    {
      restrictinfo->can_join = true;
                                                       
      Assert(!restrictinfo->pseudoconstant);
    }
  }
  else
  {
                                                                       
    restrictinfo->left_relids = NULL;
    restrictinfo->right_relids = NULL;
                                                  
    restrictinfo->clause_relids = pull_varnos(root, (Node *)clause);
  }

                                                 
  if (required_relids != NULL)
  {
    restrictinfo->required_relids = required_relids;
  }
  else
  {
    restrictinfo->required_relids = restrictinfo->clause_relids;
  }

     
                                                                          
                                                                             
                                                                         
                                                                          
                       
     
  restrictinfo->parent_ec = NULL;

  restrictinfo->eval_cost.startup = -1;
  restrictinfo->norm_selec = -1;
  restrictinfo->outer_selec = -1;

  restrictinfo->mergeopfamilies = NIL;

  restrictinfo->left_ec = NULL;
  restrictinfo->right_ec = NULL;
  restrictinfo->left_em = NULL;
  restrictinfo->right_em = NULL;
  restrictinfo->scansel_cache = NIL;

  restrictinfo->outer_is_left = false;

  restrictinfo->hashjoinoperator = InvalidOid;

  restrictinfo->left_bucketsize = -1;
  restrictinfo->right_bucketsize = -1;
  restrictinfo->left_mcvfreq = -1;
  restrictinfo->right_mcvfreq = -1;

  return restrictinfo;
}

   
                                                                        
   
                                                                    
                                                                           
                                                                       
                                                                        
                                           
   
                                                                       
                                                                            
                                                          
   
                                                                   
                                                                     
                   
   
static Expr *
make_sub_restrictinfos(PlannerInfo *root, Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{
  if (is_orclause(clause))
  {
    List *orlist = NIL;
    ListCell *temp;

    foreach (temp, ((BoolExpr *)clause)->args)
    {
      orlist = lappend(orlist, make_sub_restrictinfos(root, lfirst(temp), is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, NULL, outer_relids, nullable_relids));
    }
    return (Expr *)make_restrictinfo_internal(root, clause, make_orclause(orlist), is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, required_relids, outer_relids, nullable_relids);
  }
  else if (is_andclause(clause))
  {
    List *andlist = NIL;
    ListCell *temp;

    foreach (temp, ((BoolExpr *)clause)->args)
    {
      andlist = lappend(andlist, make_sub_restrictinfos(root, lfirst(temp), is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, required_relids, outer_relids, nullable_relids));
    }
    return make_andclause(andlist);
  }
  else
  {
    return (Expr *)make_restrictinfo_internal(root, clause, NULL, is_pushed_down, outerjoin_delayed, pseudoconstant, security_level, required_relids, outer_relids, nullable_relids);
  }
}

   
                        
   
                                                                             
                                                                          
                                                                         
                                     
   
                                                                            
                                                                          
                                                                         
                                                                          
                                                
   
RestrictInfo *
commute_restrictinfo(RestrictInfo *rinfo, Oid comm_op)
{
  RestrictInfo *result;
  OpExpr *newclause;
  OpExpr *clause = castNode(OpExpr, rinfo->clause);

  Assert(list_length(clause->args) == 2);

                                              
  newclause = makeNode(OpExpr);
  memcpy(newclause, clause, sizeof(OpExpr));

                                                            
  newclause->opno = comm_op;
  newclause->opfuncid = InvalidOid;
  newclause->args = list_make2(lsecond(clause->args), linitial(clause->args));

                                                       
  result = makeNode(RestrictInfo);
  memcpy(result, rinfo, sizeof(RestrictInfo));

     
                                                                             
                                                                             
                                                                        
                
     
  result->clause = (Expr *)newclause;
  result->left_relids = rinfo->right_relids;
  result->right_relids = rinfo->left_relids;
  Assert(result->orclause == NULL);
  result->left_ec = rinfo->right_ec;
  result->right_ec = rinfo->left_ec;
  result->left_em = rinfo->right_em;
  result->right_em = rinfo->left_em;
  result->scansel_cache = NIL;                              
  if (rinfo->hashjoinoperator == clause->opno)
  {
    result->hashjoinoperator = comm_op;
  }
  else
  {
    result->hashjoinoperator = InvalidOid;
  }
  result->left_bucketsize = rinfo->right_bucketsize;
  result->right_bucketsize = rinfo->left_bucketsize;
  result->left_mcvfreq = rinfo->right_mcvfreq;
  result->right_mcvfreq = rinfo->left_mcvfreq;

  return result;
}

   
                            
   
                                                                
   
bool
restriction_is_or_clause(RestrictInfo *restrictinfo)
{
  if (restrictinfo->orclause != NULL)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                      
   
                                                                             
                                                                 
   
bool
restriction_is_securely_promotable(RestrictInfo *restrictinfo, RelOptInfo *rel)
{
     
                                                                         
                                                                      
     
  if (restrictinfo->security_level <= rel->baserestrict_min_security || restrictinfo->leakproof)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                      
   
                                                                        
   
                                                                        
                                                                         
   
List *
get_actual_clauses(List *restrictinfo_list)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, restrictinfo_list)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

    Assert(!rinfo->pseudoconstant);

    result = lappend(result, rinfo->clause);
  }
  return result;
}

   
                          
   
                                                                       
                                                                 
   
List *
extract_actual_clauses(List *restrictinfo_list, bool pseudoconstant)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, restrictinfo_list)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

    if (rinfo->pseudoconstant == pseudoconstant)
    {
      result = lappend(result, rinfo->clause);
    }
  }
  return result;
}

   
                               
   
                                                                        
                                                                       
                                                         
   
                                                                         
                           
   
void
extract_actual_join_clauses(List *restrictinfo_list, Relids joinrelids, List **joinquals, List **otherquals)
{
  ListCell *l;

  *joinquals = NIL;
  *otherquals = NIL;

  foreach (l, restrictinfo_list)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

    if (RINFO_IS_PUSHED_DOWN(rinfo, joinrelids))
    {
      if (!rinfo->pseudoconstant)
      {
        *otherquals = lappend(*otherquals, rinfo->clause);
      }
    }
    else
    {
                                                               
      Assert(!rinfo->pseudoconstant);
      *joinquals = lappend(*joinquals, rinfo->clause);
    }
  }
}

   
                             
                                                                        
                                              
   
                                                                            
                                                                         
                                                              
   
                                                                          
                                                                         
                                                                    
   
                                                                         
                                                                          
                                     
   
                                                                             
                                                                          
                                                                           
   
                                                                      
                                                                          
                                                          
   
bool
join_clause_is_movable_to(RestrictInfo *rinfo, RelOptInfo *baserel)
{
                                                   
  if (!bms_is_member(baserel->relid, rinfo->clause_relids))
  {
    return false;
  }

                                                                   
  if (bms_is_member(baserel->relid, rinfo->outer_relids))
  {
    return false;
  }

                                                        
  if (bms_is_member(baserel->relid, rinfo->nullable_relids))
  {
    return false;
  }

                                                                        
  if (bms_overlap(baserel->lateral_referencers, rinfo->clause_relids))
  {
    return false;
  }

  return true;
}

   
                               
                                                                      
                              
   
                                                                 
                                                                        
                                                
   
                                                                          
                                                                        
                                                                        
                             
   
                                                                            
                                                                          
                                                                          
                                                                         
                                                                        
                                    
   
                                                                              
                                                                            
                                                                           
                                                                              
                                                                             
                                                                            
                                                                        
                                                                           
                          
   
                                                                           
                                                                           
                                                                            
                                                                            
                                                                            
                                                                            
                                                                            
                                                                          
                                                                           
                                                           
   
                                                                          
                                                                          
                                                                             
                                                                         
   
                                                               
                                                                     
                                                              
   
bool
join_clause_is_movable_into(RestrictInfo *rinfo, Relids currentrelids, Relids current_and_outer)
{
                                                        
  if (!bms_is_subset(rinfo->clause_relids, current_and_outer))
  {
    return false;
  }

                                                                
  if (!bms_overlap(currentrelids, rinfo->clause_relids))
  {
    return false;
  }

                                                                   
  if (bms_overlap(currentrelids, rinfo->outer_relids))
  {
    return false;
  }

     
                                                                   
                                                                           
                                                                             
                                                                           
     
  if (bms_overlap(currentrelids, rinfo->nullable_relids))
  {
    return false;
  }

  return true;
}
