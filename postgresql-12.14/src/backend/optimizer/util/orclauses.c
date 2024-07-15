                                                                            
   
               
                                                                     
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/orclauses.h"
#include "optimizer/restrictinfo.h"

                                                                  
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

static bool
is_safe_restriction_clause_for(RestrictInfo *rinfo, RelOptInfo *rel);
static Expr *
extract_or_clause(RestrictInfo *or_rinfo, RelOptInfo *rel);
static void
consider_new_or_clause(PlannerInfo *root, RelOptInfo *rel, Expr *orclause, RestrictInfo *join_or_rinfo);

   
                                  
                                                                        
                                                              
   
                                                                     
                                                                          
                                                                        
                        
                                                                
                              
                                                               
                                
                                 
                                                                              
                                                                           
                                                                              
                                                                            
                                                                              
                              
   
                                                                               
                                                                               
                                                                             
                                                                              
                                                                             
                                                                            
                                                                          
                                                                           
                                                                             
                                                                        
                                                                
                                                                       
                                                                 
   
                                                                           
                                                                            
                                                                            
                                                                         
                                                                          
                                                      
   
void
extract_restriction_or_clauses(PlannerInfo *root)
{
  Index rti;

                                                          
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *rel = root->simple_rel_array[rti];
    ListCell *lc;

                                                                    
    if (rel == NULL)
    {
      continue;
    }

    Assert(rel->relid == rti);                            

                                           
    if (rel->reloptkind != RELOPT_BASEREL)
    {
      continue;
    }

       
                                                                    
                                                                     
                                                                         
                                                    
       
                                                                      
                                                                        
                                          
       
    foreach (lc, rel->joininfo)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

      if (restriction_is_or_clause(rinfo) && join_clause_is_movable_to(rinfo, rel) && rinfo->norm_selec <= 1)
      {
                                                     
        Expr *orclause = extract_or_clause(rinfo, rel);

           
                                                                    
                                                                 
           
        if (orclause)
        {
          consider_new_or_clause(root, rel, orclause, rinfo);
        }
      }
    }
  }
}

   
                                                                         
   
static bool
is_safe_restriction_clause_for(RestrictInfo *rinfo, RelOptInfo *rel)
{
     
                                                                    
                                                                            
                                  
     
  if (rinfo->pseudoconstant)
  {
    return false;
  }
  if (!bms_equal(rinfo->clause_relids, rel->relids))
  {
    return false;
  }

                                                                 
  if (contain_volatile_functions((Node *)rinfo->clause))
  {
    return false;
  }

  return true;
}

   
                                                                            
                   
   
                                                                          
                                             
   
                                                                         
                                       
   
static Expr *
extract_or_clause(RestrictInfo *or_rinfo, RelOptInfo *rel)
{
  List *clauselist = NIL;
  ListCell *lc;

     
                                                                   
                                                                         
                                                                            
                                                                      
                                                                       
                                                                          
                                                                          
                                                                  
                                                                            
                                                                         
     
  Assert(is_orclause(or_rinfo->orclause));
  foreach (lc, ((BoolExpr *)or_rinfo->orclause)->args)
  {
    Node *orarg = (Node *)lfirst(lc);
    List *subclauses = NIL;
    Node *subclause;

                                                          
    if (is_andclause(orarg))
    {
      List *andargs = ((BoolExpr *)orarg)->args;
      ListCell *lc2;

      foreach (lc2, andargs)
      {
        RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc2);

        if (restriction_is_or_clause(rinfo))
        {
             
                                                                     
                                                                 
                                                              
                                              
             
          Expr *suborclause;

          suborclause = extract_or_clause(rinfo, rel);
          if (suborclause)
          {
            subclauses = lappend(subclauses, suborclause);
          }
        }
        else if (is_safe_restriction_clause_for(rinfo, rel))
        {
          subclauses = lappend(subclauses, rinfo->clause);
        }
      }
    }
    else
    {
      RestrictInfo *rinfo = castNode(RestrictInfo, orarg);

      Assert(!restriction_is_or_clause(rinfo));
      if (is_safe_restriction_clause_for(rinfo, rel))
      {
        subclauses = lappend(subclauses, rinfo->clause);
      }
    }

       
                                                                         
                            
       
    if (subclauses == NIL)
    {
      return NULL;
    }

       
                                                                          
                                                                           
                                                                         
                                                                       
       
    subclause = (Node *)make_ands_explicit(subclauses);
    if (is_orclause(subclause))
    {
      clauselist = list_concat(clauselist, list_copy(((BoolExpr *)subclause)->args));
    }
    else
    {
      clauselist = lappend(clauselist, subclause);
    }
  }

     
                                                                          
                                                                           
                                                                 
     
  if (clauselist != NIL)
  {
    return make_orclause(clauselist);
  }
  return NULL;
}

   
                                                                      
                                                                          
                                                                      
   
static void
consider_new_or_clause(PlannerInfo *root, RelOptInfo *rel, Expr *orclause, RestrictInfo *join_or_rinfo)
{
  RestrictInfo *or_rinfo;
  Selectivity or_selec, orig_selec;

     
                                                                            
                                   
     
  or_rinfo = make_restrictinfo(root, orclause, true, false, false, join_or_rinfo->security_level, NULL, NULL, NULL);

     
                                                                            
                                                                            
                         
     
  or_selec = clause_selectivity(root, (Node *)or_rinfo, 0, JOIN_INNER, NULL);

     
                                                                         
                                                                         
                                                                        
                                                                            
                                           
     
  if (or_selec > 0.9)
  {
    return;                
  }

     
                                                      
     
  rel->baserestrictinfo = lappend(rel->baserestrictinfo, or_rinfo);
  rel->baserestrict_min_security = Min(rel->baserestrict_min_security, or_rinfo->security_level);

     
                                                                           
                                                                             
                                                                       
                                                                     
     
                                                                   
                                   
     
                                                                   
                                                                       
                                                                          
                                                                           
                                                                            
                                                                          
                                                                           
                                                                          
                                                                     
                                
     
  if (or_selec > 0)
  {
    SpecialJoinInfo sjinfo;

       
                                                                     
                                            
       
    sjinfo.type = T_SpecialJoinInfo;
    sjinfo.min_lefthand = bms_difference(join_or_rinfo->clause_relids, rel->relids);
    sjinfo.min_righthand = rel->relids;
    sjinfo.syn_lefthand = sjinfo.min_lefthand;
    sjinfo.syn_righthand = sjinfo.min_righthand;
    sjinfo.jointype = JOIN_INNER;
                                                                   
    sjinfo.lhs_strict = false;
    sjinfo.delay_upper_joins = false;
    sjinfo.semi_can_btree = false;
    sjinfo.semi_can_hash = false;
    sjinfo.semi_operators = NIL;
    sjinfo.semi_rhs_exprs = NIL;

                                 
    orig_selec = clause_selectivity(root, (Node *)join_or_rinfo, 0, JOIN_INNER, &sjinfo);

                                                                   
    join_or_rinfo->norm_selec = orig_selec / or_selec;
                                                                          
    if (join_or_rinfo->norm_selec > 1)
    {
      join_or_rinfo->norm_selec = 1;
    }
                                                        
  }
}
