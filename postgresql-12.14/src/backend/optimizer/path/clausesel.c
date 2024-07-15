                                                                            
   
               
                                              
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/plancat.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "statistics/statistics.h"

                                                                  
#define NumRelids(a, b) NumRelids_new(a, b)

   
                                                                   
                                           
   
typedef struct RangeQueryClause
{
  struct RangeQueryClause *next;                          
  Node *var;                                                             
  bool have_lobound;                                                
  bool have_hibound;                                                 
  Selectivity lobound;                                                        
  Selectivity hibound;                                                        
} RangeQueryClause;

static void
addRangeClause(RangeQueryClause **rqlist, Node *clause, bool varonleft, bool isLTsel, Selectivity s2);
static RelOptInfo *
find_single_rel_for_clauses(PlannerInfo *root, List *clauses);

                                                                              
                                      
                                                                              

   
                            
                                                                    
                                                                   
                                                                  
                                                                  
                                   
   
                                                                          
   
                                                                        
                                                                           
                                                                             
                                                                          
                                  
   
Selectivity
clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  Selectivity s1 = 1.0;
  RelOptInfo *rel;
  Bitmapset *estimatedclauses = NULL;

     
                                                                            
                                                     
     
  rel = find_single_rel_for_clauses(root, clauses);
  if (rel && rel->rtekind == RTE_RELATION && rel->statlist != NIL)
  {
       
                                                                       
       
                                                                    
                                                                        
                          
       
    s1 *= statext_clauselist_selectivity(root, clauses, varRelid, jointype, sjinfo, rel, &estimatedclauses);
  }

     
                                                                           
                                                                 
     
  return s1 * clauselist_selectivity_simple(root, clauses, varRelid, jointype, sjinfo, estimatedclauses);
}

   
                                   
                                                                    
                                                                   
                                                                  
                                                                  
                                                                       
                                                              
   
                                                                          
   
                                                                         
                                                                              
                                                                      
                                       
   
                                                                            
                                                                             
                                                                             
                                                                            
                                                                            
                                                                           
                                                                      
                                                                              
                                                                             
                                                                               
                                                                             
                                                                            
                                                                           
                                                                         
                                                            
   
                                                                              
                                                                             
                                           
   
                                                                           
                                                                      
   
                                                                          
                                                                           
   
Selectivity
clauselist_selectivity_simple(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, Bitmapset *estimatedclauses)
{
  Selectivity s1 = 1.0;
  RangeQueryClause *rqlist = NULL;
  ListCell *l;
  int listidx;

     
                                                                           
                                                                         
               
     
  if ((list_length(clauses) == 1) && bms_num_members(estimatedclauses) == 0)
  {
    return clause_selectivity(root, (Node *)linitial(clauses), varRelid, jointype, sjinfo);
  }

     
                                                                        
                                                                             
                      
     
  listidx = -1;
  foreach (l, clauses)
  {
    Node *clause = (Node *)lfirst(l);
    RestrictInfo *rinfo;
    Selectivity s2;

    listidx++;

       
                                                                     
                         
       
    if (bms_is_member(listidx, estimatedclauses))
    {
      continue;
    }

                                                                 
    s2 = clause_selectivity(root, clause, varRelid, jointype, sjinfo);

       
                                              
       
                                                                       
                                                               
       
    if (IsA(clause, RestrictInfo))
    {
      rinfo = (RestrictInfo *)clause;
      if (rinfo->pseudoconstant)
      {
        s1 = s1 * s2;
        continue;
      }
      clause = (Node *)rinfo->clause;
    }
    else
    {
      rinfo = NULL;
    }

       
                                                                          
                                                                           
                                                                        
                                                      
       
    if (is_opclause(clause) && list_length(((OpExpr *)clause)->args) == 2)
    {
      OpExpr *expr = (OpExpr *)clause;
      bool varonleft = true;
      bool ok;

      if (rinfo)
      {
        ok = (bms_membership(rinfo->clause_relids) == BMS_SINGLETON) && (is_pseudo_constant_clause_relids(lsecond(expr->args), rinfo->right_relids) || (varonleft = false, is_pseudo_constant_clause_relids(linitial(expr->args), rinfo->left_relids)));
      }
      else
      {
        ok = (NumRelids(root, clause) == 1) && (is_pseudo_constant_clause(lsecond(expr->args)) || (varonleft = false, is_pseudo_constant_clause(linitial(expr->args))));
      }

      if (ok)
      {
           
                                                                    
                                                                       
                                                          
           
        switch (get_oprrest(expr->opno))
        {
        case F_SCALARLTSEL:
        case F_SCALARLESEL:
          addRangeClause(&rqlist, clause, varonleft, true, s2);
          break;
        case F_SCALARGTSEL:
        case F_SCALARGESEL:
          addRangeClause(&rqlist, clause, varonleft, false, s2);
          break;
        default:
                                                         
          s1 = s1 * s2;
          break;
        }
        continue;                          
      }
    }

                                                      
    s1 = s1 * s2;
  }

     
                                        
     
  while (rqlist != NULL)
  {
    RangeQueryClause *rqnext;

    if (rqlist->have_lobound && rqlist->have_hibound)
    {
                                                        
      Selectivity s2;

         
                                                                
                                                                       
                         
         
      if (rqlist->hibound == DEFAULT_INEQ_SEL || rqlist->lobound == DEFAULT_INEQ_SEL)
      {
        s2 = DEFAULT_RANGE_INEQ_SEL;
      }
      else
      {
        s2 = rqlist->hibound + rqlist->lobound - 1.0;

                                                  
        s2 += nulltestsel(root, IS_NULL, rqlist->var, varRelid, jointype, sjinfo);

           
                                                                     
                                                                     
                                                                      
                                                                  
                                                                     
                                                                    
           
        if (s2 <= 0.0)
        {
          if (s2 < -0.01)
          {
               
                                                                 
                                             
               
            s2 = DEFAULT_RANGE_INEQ_SEL;
          }
          else
          {
               
                                                              
                     
               
            s2 = 1.0e-10;
          }
        }
      }
                                                           
      s1 *= s2;
    }
    else
    {
                                                             
      if (rqlist->have_lobound)
      {
        s1 *= rqlist->lobound;
      }
      else
      {
        s1 *= rqlist->hibound;
      }
    }
                                     
    rqnext = rqlist->next;
    pfree(rqlist);
    rqlist = rqnext;
  }

  return s1;
}

   
                                                                        
   
                                                                 
   
static void
addRangeClause(RangeQueryClause **rqlist, Node *clause, bool varonleft, bool isLTsel, Selectivity s2)
{
  RangeQueryClause *rqelem;
  Node *var;
  bool is_lobound;

  if (varonleft)
  {
    var = get_leftop((Expr *)clause);
    is_lobound = !isLTsel;                                  
  }
  else
  {
    var = get_rightop((Expr *)clause);
    is_lobound = isLTsel;                                 
  }

  for (rqelem = *rqlist; rqelem; rqelem = rqelem->next)
  {
       
                                                                         
                                                      
       
    if (!equal(var, rqelem->var))
    {
      continue;
    }
                                                     
    if (is_lobound)
    {
      if (!rqelem->have_lobound)
      {
        rqelem->have_lobound = true;
        rqelem->lobound = s2;
      }
      else
      {

                 
                                                      
                             
                                               
                 
           
        if (rqelem->lobound > s2)
        {
          rqelem->lobound = s2;
        }
      }
    }
    else
    {
      if (!rqelem->have_hibound)
      {
        rqelem->have_hibound = true;
        rqelem->hibound = s2;
      }
      else
      {

                 
                                                      
                             
                                               
                 
           
        if (rqelem->hibound > s2)
        {
          rqelem->hibound = s2;
        }
      }
    }
    return;
  }

                                                                       
  rqelem = (RangeQueryClause *)palloc(sizeof(RangeQueryClause));
  rqelem->var = var;
  if (is_lobound)
  {
    rqelem->have_lobound = true;
    rqelem->have_hibound = false;
    rqelem->lobound = s2;
  }
  else
  {
    rqelem->have_lobound = false;
    rqelem->have_hibound = true;
    rqelem->hibound = s2;
  }
  rqelem->next = *rqlist;
  *rqlist = rqelem;
}

   
                               
                                                                  
                                                                   
                           
   
static RelOptInfo *
find_single_rel_for_clauses(PlannerInfo *root, List *clauses)
{
  int lastrelid = 0;
  ListCell *l;

  foreach (l, clauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);
    int relid;

       
                                                                       
                                                                    
                                                                         
                                                                    
                                                                
       
    if (!IsA(rinfo, RestrictInfo))
    {
      return NULL;
    }

    if (bms_is_empty(rinfo->clause_relids))
    {
      continue;                                          
    }
    if (!bms_get_singleton_member(rinfo->clause_relids, &relid))
    {
      return NULL;                                        
    }
    if (lastrelid == 0)
    {
      lastrelid = relid;                                          
    }
    else if (relid != lastrelid)
    {
      return NULL;                                    
    }
  }

  if (lastrelid != 0)
  {
    return find_base_rel(root, lastrelid);
  }

  return NULL;                 
}

   
                           
   
                                                           
                                                
   
                                                                
   
static bool
bms_is_subset_singleton(const Bitmapset *s, int x)
{
  switch (bms_membership(s))
  {
  case BMS_EMPTY_SET:
    return true;
  case BMS_SINGLETON:
    return bms_is_member(x, s);
  case BMS_MULTIPLE:
    return false;
  }
                         
  return false;
}

   
                          
                                                               
                                                                          
   
static inline bool
treat_as_join_clause(PlannerInfo *root, Node *clause, RestrictInfo *rinfo, int varRelid, SpecialJoinInfo *sjinfo)
{
  if (varRelid != 0)
  {
       
                                                                           
                              
       
    return false;
  }
  else if (sjinfo == NULL)
  {
       
                                                                        
                  
       
    return false;
  }
  else
  {
       
                                                                         
                                                             
       
                                                                      
                                                                          
                                                                           
                                                                
                                              
       
    if (rinfo)
    {
      return (bms_membership(rinfo->clause_relids) == BMS_MULTIPLE);
    }
    else
    {
      return (NumRelids(root, clause) > 1);
    }
  }
}

   
                        
                                                                     
   
                                                                           
                                                                        
                                          
   
                                               
   
                                                                         
                                                                            
                                                                             
                                                                           
                                                                            
                   
   
                                                                     
                                                                     
   
                                                                               
                                      
   
                                                                          
                                                                       
                  
                                                                   
                            
                                                                         
                                                       
                                                                      
                                                                 
                                                             
   
                                                                         
                                                                          
                                                                
   
Selectivity
clause_selectivity(PlannerInfo *root, Node *clause, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  Selectivity s1 = 0.5;                                            
  RestrictInfo *rinfo = NULL;
  bool cacheable = false;

  if (clause == NULL)                             
  {
    return s1;
  }

  if (IsA(clause, RestrictInfo))
  {
    rinfo = (RestrictInfo *)clause;

       
                                                                         
                                                                      
                                                                       
                                                                          
                                                                     
                                  
       
    if (rinfo->pseudoconstant)
    {
      if (!IsA(rinfo->clause, Const))
      {
        return (Selectivity)1.0;
      }
    }

       
                                                             
       
    if (rinfo->norm_selec > 1)
    {
      return (Selectivity)1.0;
    }

       
                                                                        
                                                                   
                                                                           
                                                                         
                                                                          
                                                                        
                                                                      
                                                                           
                                     
       
    if (varRelid == 0 || bms_is_subset_singleton(rinfo->clause_relids, varRelid))
    {
                                                        
      if (jointype == JOIN_INNER)
      {
        if (rinfo->norm_selec >= 0)
        {
          return rinfo->norm_selec;
        }
      }
      else
      {
        if (rinfo->outer_selec >= 0)
        {
          return rinfo->outer_selec;
        }
      }
      cacheable = true;
    }

       
                                                                          
                                                                         
                                                          
       
    if (rinfo->orclause)
    {
      clause = (Node *)rinfo->orclause;
    }
    else
    {
      clause = (Node *)rinfo->clause;
    }
  }

  if (IsA(clause, Var))
  {
    Var *var = (Var *)clause;

       
                                                                         
                                         
       
    if (var->varlevelsup == 0 && (varRelid == 0 || varRelid == (int)var->varno))
    {
                                                                   
      s1 = boolvarsel(root, (Node *)var, varRelid);
    }
  }
  else if (IsA(clause, Const))
  {
                                         
    Const *con = (Const *)clause;

    s1 = con->constisnull ? 0.0 : DatumGetBool(con->constvalue) ? 1.0 : 0.0;
  }
  else if (IsA(clause, Param))
  {
                                         
    Node *subst = estimate_expression_value(root, clause);

    if (IsA(subst, Const))
    {
                                           
      Const *con = (Const *)subst;

      s1 = con->constisnull ? 0.0 : DatumGetBool(con->constvalue) ? 1.0 : 0.0;
    }
    else
    {
                                                  
    }
  }
  else if (is_notclause(clause))
  {
                                                             
    s1 = 1.0 - clause_selectivity(root, (Node *)get_notclausearg((Expr *)clause), varRelid, jointype, sjinfo);
  }
  else if (is_andclause(clause))
  {
                                                  
    s1 = clauselist_selectivity(root, ((BoolExpr *)clause)->args, varRelid, jointype, sjinfo);
  }
  else if (is_orclause(clause))
  {
       
                                                                       
                                                                
       
                                     
       
    ListCell *arg;

    s1 = 0.0;
    foreach (arg, ((BoolExpr *)clause)->args)
    {
      Selectivity s2 = clause_selectivity(root, (Node *)lfirst(arg), varRelid, jointype, sjinfo);

      s1 = s1 + s2 - s1 * s2;
    }
  }
  else if (is_opclause(clause) || IsA(clause, DistinctExpr))
  {
    OpExpr *opclause = (OpExpr *)clause;
    Oid opno = opclause->opno;

    if (treat_as_join_clause(root, clause, rinfo, varRelid, sjinfo))
    {
                                                   
      s1 = join_selectivity(root, opno, opclause->args, opclause->inputcollid, jointype, sjinfo);
    }
    else
    {
                                                          
      s1 = restriction_selectivity(root, opno, opclause->args, opclause->inputcollid, varRelid);
    }

       
                                                                   
                                                                         
                                                                         
                                           
       
    if (IsA(clause, DistinctExpr))
    {
      s1 = 1.0 - s1;
    }
  }
  else if (is_funcclause(clause))
  {
    FuncExpr *funcclause = (FuncExpr *)clause;

                                                                  
    s1 = function_selectivity(root, funcclause->funcid, funcclause->args, funcclause->inputcollid, treat_as_join_clause(root, clause, rinfo, varRelid, sjinfo), varRelid, jointype, sjinfo);
  }
  else if (IsA(clause, ScalarArrayOpExpr))
  {
                                                            
    s1 = scalararraysel(root, (ScalarArrayOpExpr *)clause, treat_as_join_clause(root, clause, rinfo, varRelid, sjinfo), varRelid, jointype, sjinfo);
  }
  else if (IsA(clause, RowCompareExpr))
  {
                                                            
    s1 = rowcomparesel(root, (RowCompareExpr *)clause, varRelid, jointype, sjinfo);
  }
  else if (IsA(clause, NullTest))
  {
                                                            
    s1 = nulltestsel(root, ((NullTest *)clause)->nulltesttype, (Node *)((NullTest *)clause)->arg, varRelid, jointype, sjinfo);
  }
  else if (IsA(clause, BooleanTest))
  {
                                                            
    s1 = booltestsel(root, ((BooleanTest *)clause)->booltesttype, (Node *)((BooleanTest *)clause)->arg, varRelid, jointype, sjinfo);
  }
  else if (IsA(clause, CurrentOfExpr))
  {
                                                         
    CurrentOfExpr *cexpr = (CurrentOfExpr *)clause;
    RelOptInfo *crel = find_base_rel(root, cexpr->cvarno);

    if (crel->tuples > 0)
    {
      s1 = 1.0 / crel->tuples;
    }
  }
  else if (IsA(clause, RelabelType))
  {
                                                         
    s1 = clause_selectivity(root, (Node *)((RelabelType *)clause)->arg, varRelid, jointype, sjinfo);
  }
  else if (IsA(clause, CoerceToDomain))
  {
                                                         
    s1 = clause_selectivity(root, (Node *)((CoerceToDomain *)clause)->arg, varRelid, jointype, sjinfo);
  }
  else
  {
       
                                                                           
                                                                           
                                                                       
                                                                         
                           
       
    s1 = boolvarsel(root, clause, varRelid);
  }

                                    
  if (cacheable)
  {
    if (jointype == JOIN_INNER)
    {
      rinfo->norm_selec = s1;
    }
    else
    {
      rinfo->outer_selec = s1;
    }
  }

#ifdef SELECTIVITY_DEBUG
  elog(DEBUG4, "clause_selectivity: s1 %f", s1);
#endif                        

  return s1;
}
