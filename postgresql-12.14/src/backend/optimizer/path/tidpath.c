                                                                            
   
             
                                                                        
                                                        
   
                                                                
                                                                      
                                                                            
                                                                            
                                                                           
               
                                    
   
                                                                             
                                                                          
                                                                           
                                                                  
   
                                                                         
                                                                     
                                                                        
                                                                        
                                                    
   
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/restrictinfo.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)

   
                                                                     
   
static inline bool
IsCTIDVar(Var *var, RelOptInfo *rel)
{
                                              
  if (var->varattno == SelfItemPointerAttributeNumber && var->vartype == TIDOID && var->varno == rel->relid && var->varlevelsup == 0)
  {
    return true;
  }
  return false;
}

   
                                                 
                          
      
                          
                                                                    
                                  
   
static bool
IsTidEqualClause(RestrictInfo *rinfo, RelOptInfo *rel)
{
  OpExpr *node;
  Node *arg1, *arg2, *other;
  Relids other_relids;

                         
  if (!is_opclause(rinfo->clause))
  {
    return false;
  }
  node = (OpExpr *)rinfo->clause;

                              
  if (node->opno != TIDEqualOperator)
  {
    return false;
  }
  Assert(list_length(node->args) == 2);
  arg1 = linitial(node->args);
  arg2 = lsecond(node->args);

                                        
  other = NULL;
  other_relids = NULL;
  if (arg1 && IsA(arg1, Var) && IsCTIDVar((Var *)arg1, rel))
  {
    other = arg2;
    other_relids = rinfo->right_relids;
  }
  if (!other && arg2 && IsA(arg2, Var) && IsCTIDVar((Var *)arg2, rel))
  {
    other = arg1;
    other_relids = rinfo->left_relids;
  }
  if (!other)
  {
    return false;
  }

                                                   
  if (bms_is_member(rel->relid, other_relids) || contain_volatile_functions(other))
  {
    return false;
  }

  return true;              
}

   
                                                 
                                      
                                                                    
                                  
   
static bool
IsTidEqualAnyClause(PlannerInfo *root, RestrictInfo *rinfo, RelOptInfo *rel)
{
  ScalarArrayOpExpr *node;
  Node *arg1, *arg2;

                                   
  if (!(rinfo->clause && IsA(rinfo->clause, ScalarArrayOpExpr)))
  {
    return false;
  }
  node = (ScalarArrayOpExpr *)rinfo->clause;

                              
  if (node->opno != TIDEqualOperator)
  {
    return false;
  }
  if (!node->useOr)
  {
    return false;
  }
  Assert(list_length(node->args) == 2);
  arg1 = linitial(node->args);
  arg2 = lsecond(node->args);

                                   
  if (arg1 && IsA(arg1, Var) && IsCTIDVar((Var *)arg1, rel))
  {
                                                     
    if (bms_is_member(rel->relid, pull_varnos(root, arg2)) || contain_volatile_functions(arg2))
    {
      return false;
    }

    return true;              
  }

  return false;
}

   
                                                                        
   
static bool
IsCurrentOfClause(RestrictInfo *rinfo, RelOptInfo *rel)
{
  CurrentOfExpr *node;

                               
  if (!(rinfo->clause && IsA(rinfo->clause, CurrentOfExpr)))
  {
    return false;
  }
  node = (CurrentOfExpr *)rinfo->clause;

                                             
  if (node->cvarno == rel->relid)
  {
    return true;
  }

  return false;
}

   
                                                                
   
                                                                         
                                                                         
               
   
                                                                          
                                                                        
                                                                      
   
static List *
TidQualFromRestrictInfo(PlannerInfo *root, RestrictInfo *rinfo, RelOptInfo *rel)
{
     
                                                                             
                        
     
  if (rinfo->pseudoconstant)
  {
    return NIL;
  }

     
                                                                          
                        
     
  if (!restriction_is_securely_promotable(rinfo, rel))
  {
    return NIL;
  }

     
                                                                  
     
  if (IsTidEqualClause(rinfo, rel) || IsTidEqualAnyClause(root, rinfo, rel) || IsCurrentOfClause(rinfo, rel))
  {
    return list_make1(rinfo);
  }

  return NIL;
}

   
                                                                            
   
                                                                         
                                                                         
               
   
                                                                   
   
static List *
TidQualFromRestrictInfoList(PlannerInfo *root, List *rlist, RelOptInfo *rel)
{
  List *rlst = NIL;
  ListCell *l;

  foreach (l, rlist)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

    if (restriction_is_or_clause(rinfo))
    {
      ListCell *j;

         
                                                                
                                                  
         
      foreach (j, ((BoolExpr *)rinfo->orclause)->args)
      {
        Node *orarg = (Node *)lfirst(j);
        List *sublist;

                                                              
        if (is_andclause(orarg))
        {
          List *andargs = ((BoolExpr *)orarg)->args;

                                                 
          sublist = TidQualFromRestrictInfoList(root, andargs, rel);
        }
        else
        {
          RestrictInfo *rinfo = castNode(RestrictInfo, orarg);

          Assert(!restriction_is_or_clause(rinfo));
          sublist = TidQualFromRestrictInfo(root, rinfo, rel);
        }

           
                                                                   
                           
           
        if (sublist == NIL)
        {
          rlst = NIL;                             
          break;                                    
        }

           
                                                                   
           
        rlst = list_concat(rlst, sublist);
      }
    }
    else
    {
                                                  
      rlst = TidQualFromRestrictInfo(root, rinfo, rel);
    }

       
                                                                        
                                                                         
                                                                      
                                                                        
                                                         
       
    if (rlst)
    {
      break;
    }
  }

  return rlst;
}

   
                                                                          
                                                            
   
                                                                             
                                                                            
                   
   
static void
BuildParameterizedTidPaths(PlannerInfo *root, RelOptInfo *rel, List *clauses)
{
  ListCell *l;

  foreach (l, clauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);
    List *tidquals;
    Relids required_outer;

       
                                                                           
                                                                       
                                                                          
                                                                         
                                                                    
       
                                                                          
                                                                           
                                                                        
                                                                           
                                                                      
                  
       
    if (rinfo->pseudoconstant || !restriction_is_securely_promotable(rinfo, rel) || !IsTidEqualClause(rinfo, rel))
    {
      continue;
    }

       
                                                                  
                                                                           
                                 
       
    if (!join_clause_is_movable_to(rinfo, rel))
    {
      continue;
    }

                                                
    tidquals = list_make1(rinfo);

                                                   
    required_outer = bms_union(rinfo->required_relids, rel->lateral_relids);
    required_outer = bms_del_member(required_outer, rel->relid);

    add_path(rel, (Path *)create_tidscan_path(root, rel, tidquals, required_outer));
  }
}

   
                                                                       
   
                                                                         
   
static bool
ec_member_matches_ctid(PlannerInfo *root, RelOptInfo *rel, EquivalenceClass *ec, EquivalenceMember *em, void *arg)
{
  if (em->em_expr && IsA(em->em_expr, Var) && IsCTIDVar((Var *)em->em_expr, rel))
  {
    return true;
  }
  return false;
}

   
                        
                                                                      
   
                                                                       
   
void
create_tidscan_paths(PlannerInfo *root, RelOptInfo *rel)
{
  List *tidquals;

     
                                                                            
                                                
     
  tidquals = TidQualFromRestrictInfoList(root, rel->baserestrictinfo, rel);

  if (tidquals)
  {
       
                                                                        
                                                          
       
    Relids required_outer = rel->lateral_relids;

    add_path(rel, (Path *)create_tidscan_path(root, rel, tidquals, required_outer));
  }

     
                                                                             
                                                                          
                                           
     
  if (rel->has_eclass_joins)
  {
    List *clauses;

                                                                         
    clauses = generate_implied_equalities_for_column(root, rel, ec_member_matches_ctid, NULL, rel->lateral_referencers);

                                                     
    BuildParameterizedTidPaths(root, rel, clauses);
  }

     
                                                                           
                                                                             
                              
     
  BuildParameterizedTidPaths(root, rel, rel->joininfo);
}
