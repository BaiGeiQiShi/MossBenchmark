                                                                            
   
                
                                              
   
                                                                          
   
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "utils/lsyscache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

static EquivalenceMember *
add_eq_member(EquivalenceClass *ec, Expr *expr, Relids relids, Relids nullable_relids, bool is_child, Oid datatype);
static void
generate_base_implied_equalities_const(PlannerInfo *root, EquivalenceClass *ec);
static void
generate_base_implied_equalities_no_const(PlannerInfo *root, EquivalenceClass *ec);
static void
generate_base_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec);
static List *
generate_join_implied_equalities_normal(PlannerInfo *root, EquivalenceClass *ec, Relids join_relids, Relids outer_relids, Relids inner_relids);
static List *
generate_join_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec, Relids nominal_join_relids, Relids outer_relids, Relids nominal_inner_relids, RelOptInfo *inner_rel);
static Oid
select_equality_operator(EquivalenceClass *ec, Oid lefttype, Oid righttype);
static RestrictInfo *
create_join_clause(PlannerInfo *root, EquivalenceClass *ec, Oid opno, EquivalenceMember *leftem, EquivalenceMember *rightem, EquivalenceClass *parent_ec);
static bool
reconsider_outer_join_clause(PlannerInfo *root, RestrictInfo *rinfo, bool outer_on_left);
static bool
reconsider_full_join_clause(PlannerInfo *root, RestrictInfo *rinfo);

   
                       
                                                                              
                                                                          
                                                                      
                                                                           
                                                                              
                                                                       
                   
   
                                                                            
                                                                              
                                                                               
                                                 
   
                                                                             
                                                                              
                                                                              
                                                                             
                                                                          
                                                                          
                                                                            
                                                                 
   
                                                                               
                                                                              
                                                                        
                                                                                
                                                                          
                                                  
   
                                                                             
                                                                               
                 
   
                                                                         
                                                                            
                                                                            
                                     
   
                                                                     
                                                                      
                   
   
bool
process_equivalence(PlannerInfo *root, RestrictInfo **p_restrictinfo, bool below_outer_join)
{
  RestrictInfo *restrictinfo = *p_restrictinfo;
  Expr *clause = restrictinfo->clause;
  Oid opno, collation, item1_type, item2_type;
  Expr *item1;
  Expr *item2;
  Relids item1_relids, item2_relids, item1_nullable_relids, item2_nullable_relids;
  List *opfamilies;
  EquivalenceClass *ec1, *ec2;
  EquivalenceMember *em1, *em2;
  ListCell *lc1;

                                                                  
  Assert(restrictinfo->left_ec == NULL);
  Assert(restrictinfo->right_ec == NULL);

                                                                          
  if (restrictinfo->security_level > 0 && !restrictinfo->leakproof)
  {
    return false;
  }

                                      
  Assert(is_opclause(clause));
  opno = ((OpExpr *)clause)->opno;
  collation = ((OpExpr *)clause)->inputcollid;
  item1 = (Expr *)get_leftop(clause);
  item2 = (Expr *)get_rightop(clause);
  item1_relids = restrictinfo->left_relids;
  item2_relids = restrictinfo->right_relids;

     
                                                                             
                                                                         
     
  item1 = canonicalize_ec_expression(item1, exprType((Node *)item1), collation);
  item2 = canonicalize_ec_expression(item2, exprType((Node *)item2), collation);

     
                                                                           
                                                                          
                                                                      
                                    
     
  if (equal(item1, item2))
  {
       
                                                                         
                                                                       
                                                                           
                                                                           
                                                          
       
                                                                       
                                                    
       
    set_opfuncid((OpExpr *)clause);
    if (func_strict(((OpExpr *)clause)->opfuncid))
    {
      NullTest *ntest = makeNode(NullTest);

      ntest->arg = item1;
      ntest->nulltesttype = IS_NOT_NULL;
      ntest->argisrow = false;                                    
      ntest->location = -1;

      *p_restrictinfo = make_restrictinfo(root, (Expr *)ntest, restrictinfo->is_pushed_down, restrictinfo->outerjoin_delayed, restrictinfo->pseudoconstant, restrictinfo->security_level, NULL, restrictinfo->outer_relids, restrictinfo->nullable_relids);
    }
    return false;
  }

     
                                                             
     
  if (below_outer_join)
  {
    if (!bms_is_empty(item1_relids) && contain_nonstrict_functions((Node *)item1))
    {
      return false;                                         
    }
    if (!bms_is_empty(item2_relids) && contain_nonstrict_functions((Node *)item2))
    {
      return false;                                         
    }
  }

                                                                 
  item1_nullable_relids = bms_intersect(item1_relids, restrictinfo->nullable_relids);
  item2_nullable_relids = bms_intersect(item2_relids, restrictinfo->nullable_relids);

     
                                                                            
                                                                          
                                                                      
                                                                           
                                                                            
                                                                      
     
  op_input_types(opno, &item1_type, &item2_type);

  opfamilies = restrictinfo->mergeopfamilies;

     
                                                                          
                                                        
     
                                                                           
                            
     
                                                                    
     
                                                    
     
                                                    
     
                                                                       
                                                                           
                                                                             
                         
     
  ec1 = ec2 = NULL;
  em1 = em2 = NULL;
  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    ListCell *lc2;

                                      
    if (cur_ec->ec_has_volatile)
    {
      continue;
    }

       
                                                                       
                                     
       
    if (collation != cur_ec->ec_collation)
    {
      continue;
    }

       
                                                                     
                                                                        
                                       
       
    if (!equal(opfamilies, cur_ec->ec_opfamilies))
    {
      continue;
    }

    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);

      Assert(!cur_em->em_is_child);                      

         
                                                                       
                                
         
      if ((below_outer_join || cur_ec->ec_below_outer_join) && cur_em->em_is_const)
      {
        continue;
      }

      if (!ec1 && item1_type == cur_em->em_datatype && equal(item1, cur_em->em_expr))
      {
        ec1 = cur_ec;
        em1 = cur_em;
        if (ec2)
        {
          break;
        }
      }

      if (!ec2 && item2_type == cur_em->em_datatype && equal(item2, cur_em->em_expr))
      {
        ec2 = cur_ec;
        em2 = cur_em;
        if (ec1)
        {
          break;
        }
      }
    }

    if (ec1 && ec2)
    {
      break;
    }
  }

                                         

  if (ec1 && ec2)
  {
                                                         
    if (ec1 == ec2)
    {
      ec1->ec_sources = lappend(ec1->ec_sources, restrictinfo);
      ec1->ec_below_outer_join |= below_outer_join;
      ec1->ec_min_security = Min(ec1->ec_min_security, restrictinfo->security_level);
      ec1->ec_max_security = Max(ec1->ec_max_security, restrictinfo->security_level);
                                                      
      restrictinfo->left_ec = ec1;
      restrictinfo->right_ec = ec1;
                                                       
      restrictinfo->left_em = em1;
      restrictinfo->right_em = em2;
      return true;
    }

       
                                                                          
                                                                           
                                               
       
    if (root->canon_pathkeys != NIL)
    {
      elog(ERROR, "too late to merge equivalence classes");
    }

       
                                                                         
                                                                         
                                                                         
                                                                           
                 
       
    ec1->ec_members = list_concat(ec1->ec_members, ec2->ec_members);
    ec1->ec_sources = list_concat(ec1->ec_sources, ec2->ec_sources);
    ec1->ec_derives = list_concat(ec1->ec_derives, ec2->ec_derives);
    ec1->ec_relids = bms_join(ec1->ec_relids, ec2->ec_relids);
    ec1->ec_has_const |= ec2->ec_has_const;
                                        
    ec1->ec_below_outer_join |= ec2->ec_below_outer_join;
    ec1->ec_min_security = Min(ec1->ec_min_security, ec2->ec_min_security);
    ec1->ec_max_security = Max(ec1->ec_max_security, ec2->ec_max_security);
    ec2->ec_merged = ec1;
    root->eq_classes = list_delete_ptr(root->eq_classes, ec2);
                                                                 
    ec2->ec_members = NIL;
    ec2->ec_sources = NIL;
    ec2->ec_derives = NIL;
    ec2->ec_relids = NULL;
    ec1->ec_sources = lappend(ec1->ec_sources, restrictinfo);
    ec1->ec_below_outer_join |= below_outer_join;
    ec1->ec_min_security = Min(ec1->ec_min_security, restrictinfo->security_level);
    ec1->ec_max_security = Max(ec1->ec_max_security, restrictinfo->security_level);
                                                    
    restrictinfo->left_ec = ec1;
    restrictinfo->right_ec = ec1;
                                                     
    restrictinfo->left_em = em1;
    restrictinfo->right_em = em2;
  }
  else if (ec1)
  {
                                  
    em2 = add_eq_member(ec1, item2, item2_relids, item2_nullable_relids, false, item2_type);
    ec1->ec_sources = lappend(ec1->ec_sources, restrictinfo);
    ec1->ec_below_outer_join |= below_outer_join;
    ec1->ec_min_security = Min(ec1->ec_min_security, restrictinfo->security_level);
    ec1->ec_max_security = Max(ec1->ec_max_security, restrictinfo->security_level);
                                                    
    restrictinfo->left_ec = ec1;
    restrictinfo->right_ec = ec1;
                                                     
    restrictinfo->left_em = em1;
    restrictinfo->right_em = em2;
  }
  else if (ec2)
  {
                                  
    em1 = add_eq_member(ec2, item1, item1_relids, item1_nullable_relids, false, item1_type);
    ec2->ec_sources = lappend(ec2->ec_sources, restrictinfo);
    ec2->ec_below_outer_join |= below_outer_join;
    ec2->ec_min_security = Min(ec2->ec_min_security, restrictinfo->security_level);
    ec2->ec_max_security = Max(ec2->ec_max_security, restrictinfo->security_level);
                                                    
    restrictinfo->left_ec = ec2;
    restrictinfo->right_ec = ec2;
                                                     
    restrictinfo->left_em = em1;
    restrictinfo->right_em = em2;
  }
  else
  {
                                          
    EquivalenceClass *ec = makeNode(EquivalenceClass);

    ec->ec_opfamilies = opfamilies;
    ec->ec_collation = collation;
    ec->ec_members = NIL;
    ec->ec_sources = list_make1(restrictinfo);
    ec->ec_derives = NIL;
    ec->ec_relids = NULL;
    ec->ec_has_const = false;
    ec->ec_has_volatile = false;
    ec->ec_below_outer_join = below_outer_join;
    ec->ec_broken = false;
    ec->ec_sortref = 0;
    ec->ec_min_security = restrictinfo->security_level;
    ec->ec_max_security = restrictinfo->security_level;
    ec->ec_merged = NULL;
    em1 = add_eq_member(ec, item1, item1_relids, item1_nullable_relids, false, item1_type);
    em2 = add_eq_member(ec, item2, item2_relids, item2_nullable_relids, false, item2_type);

    root->eq_classes = lappend(root->eq_classes, ec);

                                                    
    restrictinfo->left_ec = ec;
    restrictinfo->right_ec = ec;
                                                     
    restrictinfo->left_em = em1;
    restrictinfo->right_em = em2;
  }

  return true;
}

   
                              
   
                                                                           
                                                                                
                                   
   
                                                                              
                                                                           
                                                                           
                                                                               
                                                                       
                                                                            
                                                                               
                                                                              
                                                                             
                                                          
   
                                                                           
                                                                          
                                                                               
                                                                         
                                                                         
                                                                           
                                                                           
                                                                        
                                 
   
Expr *
canonicalize_ec_expression(Expr *expr, Oid req_type, Oid req_collation)
{
  Oid expr_type = exprType((Node *)expr);

     
                                                                            
                                                                        
     
  if (IsPolymorphicType(req_type) || req_type == RECORDOID)
  {
    req_type = expr_type;
  }

     
                                                                         
     
  if (expr_type != req_type || exprCollation((Node *)expr) != req_collation)
  {
       
                                                                          
                                                                       
                                                                           
       
    int32 req_typmod;

    if (expr_type != req_type)
    {
      req_typmod = -1;
    }
    else
    {
      req_typmod = exprTypmod((Node *)expr);
    }

       
                                                                         
                                                                        
       
    expr = (Expr *)applyRelabelType((Node *)expr, req_type, req_typmod, req_collation, COERCE_IMPLICIT_CAST, -1, false);
  }

  return expr;
}

   
                                                                     
   
static EquivalenceMember *
add_eq_member(EquivalenceClass *ec, Expr *expr, Relids relids, Relids nullable_relids, bool is_child, Oid datatype)
{
  EquivalenceMember *em = makeNode(EquivalenceMember);

  em->em_expr = expr;
  em->em_relids = relids;
  em->em_nullable_relids = nullable_relids;
  em->em_is_const = false;
  em->em_is_child = is_child;
  em->em_datatype = datatype;

  if (bms_is_empty(relids))
  {
       
                                                                           
                                                                          
                                                                         
                                                   
                                                                        
                                                              
       
    Assert(!is_child);
    em->em_is_const = true;
    ec->ec_has_const = true;
                                   
  }
  else if (!is_child)                                           
  {
    ec->ec_relids = bms_add_members(ec->ec_relids, relids);
  }
  ec->ec_members = lappend(ec->ec_members, em);

  return em;
}

   
                            
                                                                       
                                                                          
                                            
   
                                                                         
                                                                        
                                                                          
                                                                         
                                                                         
                             
   
                                                                           
                                                                             
   
                                                                           
                                                                            
                                                                            
                                                                             
                                                                               
                                                                             
                                                                     
                                                                        
   
                                                                             
                                                                     
   
                                                                           
                                                                         
                                                                        
                                                                          
   
                                                             
                                                                           
                                                                        
                                              
   
EquivalenceClass *
get_eclass_for_sort_expr(PlannerInfo *root, Expr *expr, Relids nullable_relids, List *opfamilies, Oid opcintype, Oid collation, Index sortref, Relids rel, bool create_it)
{
  Relids expr_relids;
  EquivalenceClass *newec;
  EquivalenceMember *newem;
  ListCell *lc1;
  MemoryContext oldcontext;

     
                                                                   
     
  expr = canonicalize_ec_expression(expr, opcintype, collation);

     
                                                              
     
  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    ListCell *lc2;

       
                                                                           
                                                       
       
    if (cur_ec->ec_has_volatile && (sortref == 0 || sortref != cur_ec->ec_sortref))
    {
      continue;
    }

    if (collation != cur_ec->ec_collation)
    {
      continue;
    }
    if (!equal(opfamilies, cur_ec->ec_opfamilies))
    {
      continue;
    }

    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);

         
                                                             
         
      if (cur_em->em_is_child && !bms_equal(cur_em->em_relids, rel))
      {
        continue;
      }

         
                                                                       
                                
         
      if (cur_ec->ec_below_outer_join && cur_em->em_is_const)
      {
        continue;
      }

      if (opcintype == cur_em->em_datatype && equal(expr, cur_em->em_expr))
      {
        return cur_ec;             
      }
    }
  }

                                                 
  if (!create_it)
  {
    return NULL;
  }

     
                                      
     
                                                                          
     
  oldcontext = MemoryContextSwitchTo(root->planner_cxt);

  newec = makeNode(EquivalenceClass);
  newec->ec_opfamilies = list_copy(opfamilies);
  newec->ec_collation = collation;
  newec->ec_members = NIL;
  newec->ec_sources = NIL;
  newec->ec_derives = NIL;
  newec->ec_relids = NULL;
  newec->ec_has_const = false;
  newec->ec_has_volatile = contain_volatile_functions((Node *)expr);
  newec->ec_below_outer_join = false;
  newec->ec_broken = false;
  newec->ec_sortref = sortref;
  newec->ec_min_security = UINT_MAX;
  newec->ec_max_security = 0;
  newec->ec_merged = NULL;

  if (newec->ec_has_volatile && sortref == 0)                        
  {
    elog(ERROR, "volatile EquivalenceClass has no sortref");
  }

     
                                                                         
     
  expr_relids = pull_varnos(root, (Node *)expr);
  nullable_relids = bms_intersect(nullable_relids, expr_relids);

  newem = add_eq_member(newec, copyObject(expr), expr_relids, nullable_relids, false, opcintype);

     
                                                                       
                                                                          
                                                                         
              
     
  if (newec->ec_has_const)
  {
    if (newec->ec_has_volatile || expression_returns_set((Node *)expr) || contain_agg_clause((Node *)expr) || contain_window_function((Node *)expr))
    {
      newec->ec_has_const = false;
      newem->em_is_const = false;
    }
  }

  root->eq_classes = lappend(root->eq_classes, newec);

  MemoryContextSwitchTo(oldcontext);

  return newec;
}

   
                                    
                                                                          
              
   
                                                                    
                                                                            
                                                                              
                                                                             
                                                                        
                                                                              
                                                                        
                                                                              
                                                                            
                                                                           
                                
   
                                                                        
                                                                           
                                                                              
                                                                           
                                                                             
                                                                         
                                                                        
                                                                      
   
                                                                               
                                                                             
                                                                         
                                                                        
                                                                              
                                                                         
                                                                             
                                                                             
                                                                              
                                                             
   
                                                               
                                                                          
                                                                             
                                                              
   
                                                                           
                                                                          
                                                                          
                                                                         
                                                                              
   
void
generate_base_implied_equalities(PlannerInfo *root)
{
  ListCell *lc;
  Index rti;

  foreach (lc, root->eq_classes)
  {
    EquivalenceClass *ec = (EquivalenceClass *)lfirst(lc);

    Assert(ec->ec_merged == NULL);                                
    Assert(!ec->ec_broken);                               

                                                         
    if (list_length(ec->ec_members) <= 1)
    {
      continue;
    }

    if (ec->ec_has_const)
    {
      generate_base_implied_equalities_const(root, ec);
    }
    else
    {
      generate_base_implied_equalities_no_const(root, ec);
    }

                                                                   
    if (ec->ec_broken)
    {
      generate_base_implied_equalities_broken(root, ec);
    }
  }

     
                                                                             
                                                                     
     
  for (rti = 1; rti < root->simple_rel_array_size; rti++)
  {
    RelOptInfo *brel = root->simple_rel_array[rti];

    if (brel == NULL)
    {
      continue;
    }

    brel->has_eclass_joins = has_relevant_eclass_joinclause(root, brel);
  }
}

   
                                                                       
   
static void
generate_base_implied_equalities_const(PlannerInfo *root, EquivalenceClass *ec)
{
  EquivalenceMember *const_em = NULL;
  ListCell *lc;

     
                                                                          
                                                                         
                                                                             
                                                                     
                                
     
  if (list_length(ec->ec_members) == 2 && list_length(ec->ec_sources) == 1)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)linitial(ec->ec_sources);

    if (bms_membership(restrictinfo->required_relids) != BMS_MULTIPLE)
    {
      distribute_restrictinfo_to_rels(root, restrictinfo);
      return;
    }
  }

     
                                                                       
                                                                         
                                                                            
                                                                      
     
  foreach (lc, ec->ec_members)
  {
    EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc);

    if (cur_em->em_is_const)
    {
      const_em = cur_em;
      if (IsA(cur_em->em_expr, Const))
      {
        break;
      }
    }
  }
  Assert(const_em != NULL);

                                                             
  foreach (lc, ec->ec_members)
  {
    EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc);
    Oid eq_op;

    Assert(!cur_em->em_is_child);                      
    if (cur_em == const_em)
    {
      continue;
    }
    eq_op = select_equality_operator(ec, cur_em->em_datatype, const_em->em_datatype);
    if (!OidIsValid(eq_op))
    {
                     
      ec->ec_broken = true;
      break;
    }
    process_implied_equality(root, eq_op, ec->ec_collation, cur_em->em_expr, const_em->em_expr, bms_copy(ec->ec_relids), bms_union(cur_em->em_nullable_relids, const_em->em_nullable_relids), ec->ec_min_security, ec->ec_below_outer_join, cur_em->em_is_const);
  }
}

   
                                                                        
   
static void
generate_base_implied_equalities_no_const(PlannerInfo *root, EquivalenceClass *ec)
{
  EquivalenceMember **prev_ems;
  ListCell *lc;

     
                                                                         
                                                                           
                                                                            
                                                                           
                                                                            
                                                                          
     
  prev_ems = (EquivalenceMember **)palloc0(root->simple_rel_array_size * sizeof(EquivalenceMember *));

  foreach (lc, ec->ec_members)
  {
    EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc);
    int relid;

    Assert(!cur_em->em_is_child);                      
    if (!bms_get_singleton_member(cur_em->em_relids, &relid))
    {
      continue;
    }
    Assert(relid < root->simple_rel_array_size);

    if (prev_ems[relid] != NULL)
    {
      EquivalenceMember *prev_em = prev_ems[relid];
      Oid eq_op;

      eq_op = select_equality_operator(ec, prev_em->em_datatype, cur_em->em_datatype);
      if (!OidIsValid(eq_op))
      {
                       
        ec->ec_broken = true;
        break;
      }
      process_implied_equality(root, eq_op, ec->ec_collation, prev_em->em_expr, cur_em->em_expr, bms_copy(ec->ec_relids), bms_union(prev_em->em_nullable_relids, cur_em->em_nullable_relids), ec->ec_min_security, ec->ec_below_outer_join, false);
    }
    prev_ems[relid] = cur_em;
  }

  pfree(prev_ems);

     
                                                                            
                                                                           
                                                                            
                                                                    
                                                                             
                                        
     
  foreach (lc, ec->ec_members)
  {
    EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc);
    List *vars = pull_var_clause((Node *)cur_em->em_expr, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);

    add_vars_to_targetlist(root, vars, ec->ec_relids, false);
    list_free(vars);
  }
}

   
                                                          
   
                                                                               
                                                                             
                                                                             
                                                                          
                                                                            
                                                                           
                                                                           
                                                                            
                                                                            
                     
   
static void
generate_base_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec)
{
  ListCell *lc;

  foreach (lc, ec->ec_sources)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(lc);

    if (ec->ec_has_const || bms_membership(restrictinfo->required_relids) != BMS_MULTIPLE)
    {
      distribute_restrictinfo_to_rels(root, restrictinfo);
    }
  }
}

   
                                    
                                                                            
   
                                                                            
                                                                         
                                                                          
                                                                          
                                                                         
                             
   
                                                                         
                                                                            
                                                                            
                                                                        
                               
   
                                                                              
                                                                          
                                                                           
                                                  
   
                                                                              
                                                                           
                                                                              
                                                                             
                                                                         
                                                                
   
                                                                           
                                                                          
                                                                       
                                                                           
                                                                             
                                                  
   
                                                                               
                                                                            
                                                               
   
List *
generate_join_implied_equalities(PlannerInfo *root, Relids join_relids, Relids outer_relids, RelOptInfo *inner_rel)
{
  return generate_join_implied_equalities_for_ecs(root, root->eq_classes, join_relids, outer_relids, inner_rel);
}

   
                                            
                                                 
   
List *
generate_join_implied_equalities_for_ecs(PlannerInfo *root, List *eclasses, Relids join_relids, Relids outer_relids, RelOptInfo *inner_rel)
{
  List *result = NIL;
  Relids inner_relids = inner_rel->relids;
  Relids nominal_inner_relids;
  Relids nominal_join_relids;
  ListCell *lc;

                                                           
  if (IS_OTHER_REL(inner_rel))
  {
    Assert(!bms_is_empty(inner_rel->top_parent_relids));

                                                    
    nominal_inner_relids = inner_rel->top_parent_relids;
                                                                     
    nominal_join_relids = bms_union(outer_relids, nominal_inner_relids);
  }
  else
  {
    nominal_inner_relids = inner_relids;
    nominal_join_relids = join_relids;
  }

  foreach (lc, eclasses)
  {
    EquivalenceClass *ec = (EquivalenceClass *)lfirst(lc);
    List *sublist = NIL;

                                                                   
    if (ec->ec_has_const)
    {
      continue;
    }

                                                         
    if (list_length(ec->ec_members) <= 1)
    {
      continue;
    }

                                                                    
    if (!bms_overlap(ec->ec_relids, nominal_join_relids))
    {
      continue;
    }

    if (!ec->ec_broken)
    {
      sublist = generate_join_implied_equalities_normal(root, ec, join_relids, outer_relids, inner_relids);
    }

                                                                   
    if (ec->ec_broken)
    {
      sublist = generate_join_implied_equalities_broken(root, ec, nominal_join_relids, outer_relids, nominal_inner_relids, inner_rel);
    }

    result = list_concat(result, sublist);
  }

  return result;
}

   
                                                         
   
static List *
generate_join_implied_equalities_normal(PlannerInfo *root, EquivalenceClass *ec, Relids join_relids, Relids outer_relids, Relids inner_relids)
{
  List *result = NIL;
  List *new_members = NIL;
  List *outer_members = NIL;
  List *inner_members = NIL;
  ListCell *lc1;

     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                           
                                                                        
                                                              
     
  foreach (lc1, ec->ec_members)
  {
    EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc1);

       
                                                                          
                                                                     
                                                             
       
    if (!bms_is_subset(cur_em->em_relids, join_relids))
    {
      continue;                                         
    }

    if (bms_is_subset(cur_em->em_relids, outer_relids))
    {
      outer_members = lappend(outer_members, cur_em);
    }
    else if (bms_is_subset(cur_em->em_relids, inner_relids))
    {
      inner_members = lappend(inner_members, cur_em);
    }
    else
    {
      new_members = lappend(new_members, cur_em);
    }
  }

     
                                                                          
                                                                    
                                                                           
                                                                             
                                                                         
                                                                  
                   
     
  if (outer_members && inner_members)
  {
    EquivalenceMember *best_outer_em = NULL;
    EquivalenceMember *best_inner_em = NULL;
    Oid best_eq_op = InvalidOid;
    int best_score = -1;
    RestrictInfo *rinfo;

    foreach (lc1, outer_members)
    {
      EquivalenceMember *outer_em = (EquivalenceMember *)lfirst(lc1);
      ListCell *lc2;

      foreach (lc2, inner_members)
      {
        EquivalenceMember *inner_em = (EquivalenceMember *)lfirst(lc2);
        Oid eq_op;
        int score;

        eq_op = select_equality_operator(ec, outer_em->em_datatype, inner_em->em_datatype);
        if (!OidIsValid(eq_op))
        {
          continue;
        }
        score = 0;
        if (IsA(outer_em->em_expr, Var) || (IsA(outer_em->em_expr, RelabelType) && IsA(((RelabelType *)outer_em->em_expr)->arg, Var)))
        {
          score++;
        }
        if (IsA(inner_em->em_expr, Var) || (IsA(inner_em->em_expr, RelabelType) && IsA(((RelabelType *)inner_em->em_expr)->arg, Var)))
        {
          score++;
        }
        if (op_hashjoinable(eq_op, exprType((Node *)outer_em->em_expr)))
        {
          score++;
        }
        if (score > best_score)
        {
          best_outer_em = outer_em;
          best_inner_em = inner_em;
          best_eq_op = eq_op;
          best_score = score;
          if (best_score == 3)
          {
            break;                              
          }
        }
      }
      if (best_score == 3)
      {
        break;                              
      }
    }
    if (best_score < 0)
    {
                     
      ec->ec_broken = true;
      return NIL;
    }

       
                                                                           
                   
       
    rinfo = create_join_clause(root, ec, best_eq_op, best_outer_em, best_inner_em, ec);

    result = lappend(result, rinfo);
  }

     
                                                                          
                                                                          
                                                                
     
                                                                            
                                                                          
                                                            
     
  if (new_members)
  {
    List *old_members = list_concat(outer_members, inner_members);
    EquivalenceMember *prev_em = NULL;
    RestrictInfo *rinfo;

                                                                          
    if (old_members)
    {
      new_members = lappend(new_members, linitial(old_members));
    }

    foreach (lc1, new_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc1);

      if (prev_em != NULL)
      {
        Oid eq_op;

        eq_op = select_equality_operator(ec, prev_em->em_datatype, cur_em->em_datatype);
        if (!OidIsValid(eq_op))
        {
                         
          ec->ec_broken = true;
          return NIL;
        }
                                                               
        rinfo = create_join_clause(root, ec, eq_op, prev_em, cur_em, NULL);

        result = lappend(result, rinfo);
      }
      prev_em = cur_em;
    }
  }

  return result;
}

   
                                                          
   
                                                                        
   
                                                                   
                                                     
   
static List *
generate_join_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec, Relids nominal_join_relids, Relids outer_relids, Relids nominal_inner_relids, RelOptInfo *inner_rel)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, ec->ec_sources)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(lc);
    Relids clause_relids = restrictinfo->required_relids;

    if (bms_is_subset(clause_relids, nominal_join_relids) && !bms_is_subset(clause_relids, outer_relids) && !bms_is_subset(clause_relids, nominal_inner_relids))
    {
      result = lappend(result, restrictinfo);
    }
  }

     
                                                                            
                                                                      
                                                                             
                                                                         
                                              
     
                                                                    
                                                                          
                                         
     
  if (IS_OTHER_REL(inner_rel) && result != NIL)
  {
    result = (List *)adjust_appendrel_attrs_multilevel(root, (Node *)result, inner_rel->relids, inner_rel->top_parent_relids);
  }

  return result;
}

   
                            
                                                                      
   
                                                                                
   
static Oid
select_equality_operator(EquivalenceClass *ec, Oid lefttype, Oid righttype)
{
  ListCell *lc;

  foreach (lc, ec->ec_opfamilies)
  {
    Oid opfamily = lfirst_oid(lc);
    Oid opno;

    opno = get_opfamily_member(opfamily, lefttype, righttype, BTEqualStrategyNumber);
    if (!OidIsValid(opno))
    {
      continue;
    }
                                                                         
    if (ec->ec_max_security == 0)
    {
      return opno;
    }
                                                                
    if (get_func_leakproof(get_opcode(opno)))
    {
      return opno;
    }
  }
  return InvalidOid;
}

   
                      
                                                                    
                              
   
                                                                             
                                                                        
                                                                             
                                                                              
   
static RestrictInfo *
create_join_clause(PlannerInfo *root, EquivalenceClass *ec, Oid opno, EquivalenceMember *leftem, EquivalenceMember *rightem, EquivalenceClass *parent_ec)
{
  RestrictInfo *rinfo;
  ListCell *lc;
  MemoryContext oldcontext;

     
                                                                       
                                                                       
                                                                           
                     
     
  foreach (lc, ec->ec_sources)
  {
    rinfo = (RestrictInfo *)lfirst(lc);
    if (rinfo->left_em == leftem && rinfo->right_em == rightem && rinfo->parent_ec == parent_ec && opno == ((OpExpr *)rinfo->clause)->opno)
    {
      return rinfo;
    }
  }

  foreach (lc, ec->ec_derives)
  {
    rinfo = (RestrictInfo *)lfirst(lc);
    if (rinfo->left_em == leftem && rinfo->right_em == rightem && rinfo->parent_ec == parent_ec && opno == ((OpExpr *)rinfo->clause)->opno)
    {
      return rinfo;
    }
  }

     
                                                                          
                                                               
     
  oldcontext = MemoryContextSwitchTo(root->planner_cxt);

  rinfo = build_implied_join_equality(root, opno, ec->ec_collation, leftem->em_expr, rightem->em_expr, bms_union(leftem->em_relids, rightem->em_relids), bms_union(leftem->em_nullable_relids, rightem->em_nullable_relids), ec->ec_min_security);

                                            
  rinfo->parent_ec = parent_ec;

     
                                                                             
                                                                         
     
  rinfo->left_ec = ec;
  rinfo->right_ec = ec;

                                        
  rinfo->left_em = leftem;
  rinfo->right_em = rightem;
                                       
  ec->ec_derives = lappend(ec->ec_derives, rinfo);

  MemoryContextSwitchTo(oldcontext);

  return rinfo;
}

   
                                 
                                                              
                                                             
                                                                
                                                                  
   
                                                                         
                                                                           
                                                                       
                                                                         
                                                                     
   
                                                                              
                                                                      
                                                                       
                                                                           
                                                                               
                                                                            
                        
   
                                                                          
                                                                             
                                                                               
                                                                       
                                                               
   
                                                                            
                                                                      
                                                              
                                
                                                                            
                                                                                
                                                                        
                                                                  
   
                                                                       
                                                                          
                                                                            
                                                                       
                                                  
   
                                                                        
                                                                             
                                                                            
                                                                            
                                                                           
                                                                           
                                                               
                                                    
   
                                                                      
                                                                               
                                                                              
                                                                             
                                                                
   
                                                                         
                                                                         
                                                                           
                                                                         
                                                                            
                                                                         
                                                                          
                                                                         
                                                                          
                                                                         
                                      
   
                                                                          
                                                                         
                                                                          
                                                                          
                                                                      
                                                                           
                                                                         
                                                                 
   
void
reconsider_outer_join_clauses(PlannerInfo *root)
{
  bool found;
  ListCell *cell;
  ListCell *prev;
  ListCell *next;

                                                           
  do
  {
    found = false;

                                       
    prev = NULL;
    for (cell = list_head(root->left_join_clauses); cell; cell = next)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);

      next = lnext(cell);
      if (reconsider_outer_join_clause(root, rinfo, true))
      {
        found = true;
                                     
        root->left_join_clauses = list_delete_cell(root->left_join_clauses, cell, prev);
                                                       
                                                                 
        rinfo->norm_selec = 2.0;
        rinfo->outer_selec = 1.0;
        distribute_restrictinfo_to_rels(root, rinfo);
      }
      else
      {
        prev = cell;
      }
    }

                                        
    prev = NULL;
    for (cell = list_head(root->right_join_clauses); cell; cell = next)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);

      next = lnext(cell);
      if (reconsider_outer_join_clause(root, rinfo, false))
      {
        found = true;
                                     
        root->right_join_clauses = list_delete_cell(root->right_join_clauses, cell, prev);
                                                       
                                                                 
        rinfo->norm_selec = 2.0;
        rinfo->outer_selec = 1.0;
        distribute_restrictinfo_to_rels(root, rinfo);
      }
      else
      {
        prev = cell;
      }
    }

                                       
    prev = NULL;
    for (cell = list_head(root->full_join_clauses); cell; cell = next)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);

      next = lnext(cell);
      if (reconsider_full_join_clause(root, rinfo))
      {
        found = true;
                                     
        root->full_join_clauses = list_delete_cell(root->full_join_clauses, cell, prev);
                                                       
                                                                 
        rinfo->norm_selec = 2.0;
        rinfo->outer_selec = 1.0;
        distribute_restrictinfo_to_rels(root, rinfo);
      }
      else
      {
        prev = cell;
      }
    }
  } while (found);

                                                         
  foreach (cell, root->left_join_clauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);

    distribute_restrictinfo_to_rels(root, rinfo);
  }
  foreach (cell, root->right_join_clauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);

    distribute_restrictinfo_to_rels(root, rinfo);
  }
  foreach (cell, root->full_join_clauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);

    distribute_restrictinfo_to_rels(root, rinfo);
  }
}

   
                                                                     
   
                                                                            
   
static bool
reconsider_outer_join_clause(PlannerInfo *root, RestrictInfo *rinfo, bool outer_on_left)
{
  Expr *outervar, *innervar;
  Oid opno, collation, left_type, right_type, inner_datatype;
  Relids inner_relids, inner_nullable_relids;
  ListCell *lc1;

  Assert(is_opclause(rinfo->clause));
  opno = ((OpExpr *)rinfo->clause)->opno;
  collation = ((OpExpr *)rinfo->clause)->inputcollid;

                                                               
  if (rinfo->outerjoin_delayed && !op_strict(opno))
  {
    return false;
  }

                                           
  op_input_types(opno, &left_type, &right_type);
  if (outer_on_left)
  {
    outervar = (Expr *)get_leftop(rinfo->clause);
    innervar = (Expr *)get_rightop(rinfo->clause);
    inner_datatype = right_type;
    inner_relids = rinfo->right_relids;
  }
  else
  {
    outervar = (Expr *)get_rightop(rinfo->clause);
    innervar = (Expr *)get_leftop(rinfo->clause);
    inner_datatype = left_type;
    inner_relids = rinfo->left_relids;
  }
  inner_nullable_relids = bms_intersect(inner_relids, rinfo->nullable_relids);

                                                       
  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    bool match;
    ListCell *lc2;

                                                      
    if (!cur_ec->ec_has_const)
    {
      continue;
    }
                                      
    if (cur_ec->ec_has_volatile)
    {
      continue;
    }
                                                                    
    if (collation != cur_ec->ec_collation)
    {
      continue;
    }
    if (!equal(rinfo->mergeopfamilies, cur_ec->ec_opfamilies))
    {
      continue;
    }
                                              
    match = false;
    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);

      Assert(!cur_em->em_is_child);                      
      if (equal(outervar, cur_em->em_expr))
      {
        match = true;
        break;
      }
    }
    if (!match)
    {
      continue;                                  
    }

       
                                                                           
                                                                        
                                                                          
       
    match = false;
    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);
      Oid eq_op;
      RestrictInfo *newrinfo;

      if (!cur_em->em_is_const)
      {
        continue;                               
      }
      eq_op = select_equality_operator(cur_ec, inner_datatype, cur_em->em_datatype);
      if (!OidIsValid(eq_op))
      {
        continue;                              
      }
      newrinfo = build_implied_join_equality(root, eq_op, cur_ec->ec_collation, innervar, cur_em->em_expr, bms_copy(inner_relids), bms_copy(inner_nullable_relids), cur_ec->ec_min_security);
      if (process_equivalence(root, &newrinfo, true))
      {
        match = true;
      }
    }

       
                                                                           
                                                                          
                                  
       
    if (match)
    {
      return true;
    }
    else
    {
      break;
    }
  }

  return false;                                   
}

   
                                                               
   
                                                                            
   
static bool
reconsider_full_join_clause(PlannerInfo *root, RestrictInfo *rinfo)
{
  Expr *leftvar;
  Expr *rightvar;
  Oid opno, collation, left_type, right_type;
  Relids left_relids, right_relids, left_nullable_relids, right_nullable_relids;
  ListCell *lc1;

                                                  
  if (rinfo->outerjoin_delayed)
  {
    return false;
  }

                                           
  Assert(is_opclause(rinfo->clause));
  opno = ((OpExpr *)rinfo->clause)->opno;
  collation = ((OpExpr *)rinfo->clause)->inputcollid;
  op_input_types(opno, &left_type, &right_type);
  leftvar = (Expr *)get_leftop(rinfo->clause);
  rightvar = (Expr *)get_rightop(rinfo->clause);
  left_relids = rinfo->left_relids;
  right_relids = rinfo->right_relids;
  left_nullable_relids = bms_intersect(left_relids, rinfo->nullable_relids);
  right_nullable_relids = bms_intersect(right_relids, rinfo->nullable_relids);

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    EquivalenceMember *coal_em = NULL;
    bool match;
    bool matchleft;
    bool matchright;
    ListCell *lc2;

                                                      
    if (!cur_ec->ec_has_const)
    {
      continue;
    }
                                      
    if (cur_ec->ec_has_volatile)
    {
      continue;
    }
                                                                    
    if (collation != cur_ec->ec_collation)
    {
      continue;
    }
    if (!equal(rinfo->mergeopfamilies, cur_ec->ec_opfamilies))
    {
      continue;
    }

       
                                                                
       
                                                                        
                                                                         
                      
       
                                                                        
                                                                           
                                                                          
                                                                        
                               
       
    match = false;
    foreach (lc2, cur_ec->ec_members)
    {
      coal_em = (EquivalenceMember *)lfirst(lc2);
      Assert(!coal_em->em_is_child);                      
      if (IsA(coal_em->em_expr, CoalesceExpr))
      {
        CoalesceExpr *cexpr = (CoalesceExpr *)coal_em->em_expr;
        Node *cfirst;
        Node *csecond;

        if (list_length(cexpr->args) != 2)
        {
          continue;
        }
        cfirst = (Node *)linitial(cexpr->args);
        csecond = (Node *)lsecond(cexpr->args);

        if (equal(leftvar, cfirst) && equal(rightvar, csecond))
        {
          match = true;
          break;
        }
      }
    }
    if (!match)
    {
      continue;                                  
    }

       
                                                                    
                                                                           
                                                                     
                                                   
       
    matchleft = matchright = false;
    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);
      Oid eq_op;
      RestrictInfo *newrinfo;

      if (!cur_em->em_is_const)
      {
        continue;                               
      }
      eq_op = select_equality_operator(cur_ec, left_type, cur_em->em_datatype);
      if (OidIsValid(eq_op))
      {
        newrinfo = build_implied_join_equality(root, eq_op, cur_ec->ec_collation, leftvar, cur_em->em_expr, bms_copy(left_relids), bms_copy(left_nullable_relids), cur_ec->ec_min_security);
        if (process_equivalence(root, &newrinfo, true))
        {
          matchleft = true;
        }
      }
      eq_op = select_equality_operator(cur_ec, right_type, cur_em->em_datatype);
      if (OidIsValid(eq_op))
      {
        newrinfo = build_implied_join_equality(root, eq_op, cur_ec->ec_collation, rightvar, cur_em->em_expr, bms_copy(right_relids), bms_copy(right_nullable_relids), cur_ec->ec_min_security);
        if (process_equivalence(root, &newrinfo, true))
        {
          matchright = true;
        }
      }
    }

       
                                                                         
                                                                          
                                                                  
                                                                       
                                                               
       
    if (matchleft && matchright)
    {
      cur_ec->ec_members = list_delete_ptr(cur_ec->ec_members, coal_em);
      return true;
    }

       
                                                                          
                                                                        
                                      
       
    break;
  }

  return false;                                   
}

   
                     
                                                                       
                    
   
                                                                      
                                                                    
                                                              
   
                                                                         
                                                             
   
bool
exprs_known_equal(PlannerInfo *root, Node *item1, Node *item2)
{
  ListCell *lc1;

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *ec = (EquivalenceClass *)lfirst(lc1);
    bool item1member = false;
    bool item2member = false;
    ListCell *lc2;

                                      
    if (ec->ec_has_volatile)
    {
      continue;
    }

    foreach (lc2, ec->ec_members)
    {
      EquivalenceMember *em = (EquivalenceMember *)lfirst(lc2);

      if (em->em_is_child)
      {
        continue;                           
      }
      if (equal(item1, em->em_expr))
      {
        item1member = true;
      }
      else if (equal(item2, em->em_expr))
      {
        item2member = true;
      }
                                              
      if (item1member && item2member)
      {
        return true;
      }
    }
  }
  return false;
}

   
                                     
                                                                     
   
                                                                            
                                                                             
                                                                          
                                                                             
                                                                          
                                                                            
                                                
   
EquivalenceClass *
match_eclasses_to_foreign_key_col(PlannerInfo *root, ForeignKeyOptInfo *fkinfo, int colno)
{
  Index var1varno = fkinfo->con_relid;
  AttrNumber var1attno = fkinfo->conkey[colno];
  Index var2varno = fkinfo->ref_relid;
  AttrNumber var2attno = fkinfo->confkey[colno];
  Oid eqop = fkinfo->conpfeqop[colno];
  List *opfamilies = NIL;                             
  ListCell *lc1;

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *ec = (EquivalenceClass *)lfirst(lc1);
    bool item1member = false;
    bool item2member = false;
    ListCell *lc2;

                                      
    if (ec->ec_has_volatile)
    {
      continue;
    }
                                                                

       
                                                                        
                                           
       
    if (!bms_is_member(var1varno, ec->ec_relids) || !bms_is_member(var2varno, ec->ec_relids))
    {
      continue;
    }

    foreach (lc2, ec->ec_members)
    {
      EquivalenceMember *em = (EquivalenceMember *)lfirst(lc2);
      Var *var;

      if (em->em_is_child)
      {
        continue;                           
      }

                                                       
      var = (Var *)em->em_expr;
      while (var && IsA(var, RelabelType))
      {
        var = (Var *)((RelabelType *)var)->arg;
      }
      if (!(var && IsA(var, Var)))
      {
        continue;
      }

                  
      if (var->varno == var1varno && var->varattno == var1attno)
      {
        item1member = true;
      }
      else if (var->varno == var2varno && var->varattno == var2attno)
      {
        item2member = true;
      }

                                                           
      if (item1member && item2member)
      {
           
                                                                   
                                                                       
                                             
           
        if (opfamilies == NIL)                                   
        {
          opfamilies = get_mergejoin_opfamilies(eqop);
        }
        if (equal(opfamilies, ec->ec_opfamilies))
        {
          return ec;
        }
                                                               
        break;
      }
    }
  }
  return NULL;
}

   
                              
                                                                            
                                                        
   
                                                                               
                                                                      
   
                                                                         
                                                                         
   
                                                                        
                                                                         
                                                                           
                                                                              
                                
   
void
add_child_rel_equivalences(PlannerInfo *root, AppendRelInfo *appinfo, RelOptInfo *parent_rel, RelOptInfo *child_rel)
{
  Relids top_parent_relids = child_rel->top_parent_relids;
  Relids child_relids = child_rel->relids;
  ListCell *lc1;

  Assert(IS_SIMPLE_REL(parent_rel));

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    ListCell *lc2;

       
                                                                        
                                                                   
                                       
       
    if (cur_ec->ec_has_volatile)
    {
      continue;
    }

       
                                                                  
                            
       
    if (!bms_is_subset(top_parent_relids, cur_ec->ec_relids))
    {
      continue;
    }

    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);

      if (cur_em->em_is_const)
      {
        continue;                         
      }

         
                                                        
                                                                         
                                                                        
                                                                
                                                                         
                                                                         
         
      if (cur_em->em_is_child)
      {
        continue;                           
      }

                                                                  
      if (bms_overlap(cur_em->em_relids, top_parent_relids))
      {
                                                     
        Expr *child_expr;
        Relids new_relids;
        Relids new_nullable_relids;

        if (parent_rel->reloptkind == RELOPT_BASEREL)
        {
                                                  
          child_expr = (Expr *)adjust_appendrel_attrs(root, (Node *)cur_em->em_expr, 1, &appinfo);
        }
        else
        {
                                                  
          child_expr = (Expr *)adjust_appendrel_attrs_multilevel(root, (Node *)cur_em->em_expr, child_relids, top_parent_relids);
        }

           
                                                              
                                                            
                                                                    
                                                                 
           
        new_relids = bms_difference(cur_em->em_relids, top_parent_relids);
        new_relids = bms_add_members(new_relids, child_relids);

           
                                                                     
                                                   
           
        new_nullable_relids = cur_em->em_nullable_relids;
        if (bms_overlap(new_nullable_relids, top_parent_relids))
        {
          new_nullable_relids = bms_difference(new_nullable_relids, top_parent_relids);
          new_nullable_relids = bms_add_members(new_nullable_relids, child_relids);
        }

        (void)add_eq_member(cur_ec, child_expr, new_relids, new_nullable_relids, true, cur_em->em_datatype);
      }
    }
  }
}

   
                                   
                                                         
   
                                                                               
                                                        
   
                                                                               
                                                                      
   
void
add_child_join_rel_equivalences(PlannerInfo *root, int nappinfos, AppendRelInfo **appinfos, RelOptInfo *parent_joinrel, RelOptInfo *child_joinrel)
{
  Relids top_parent_relids = child_joinrel->top_parent_relids;
  Relids child_relids = child_joinrel->relids;
  MemoryContext oldcontext;
  ListCell *lc1;

  Assert(IS_JOIN_REL(child_joinrel) && IS_JOIN_REL(parent_joinrel));

     
                                                                       
                                                                            
                                                                           
                                                                           
                                                                          
                                                                          
     
  oldcontext = MemoryContextSwitchTo(root->planner_cxt);

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    ListCell *lc2;

       
                                                                        
                                                                   
                                       
       
    if (cur_ec->ec_has_volatile)
    {
      continue;
    }

       
                                                                  
                            
       
    if (!bms_overlap(top_parent_relids, cur_ec->ec_relids))
    {
      continue;
    }

    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc2);

      if (cur_em->em_is_const)
      {
        continue;                         
      }

         
                                                        
                                            
         
      if (cur_em->em_is_child)
      {
        continue;                           
      }

         
                                                                    
                                                                      
         
      if (bms_membership(cur_em->em_relids) != BMS_MULTIPLE)
      {
        continue;
      }

                                                                  
      if (bms_overlap(cur_em->em_relids, top_parent_relids))
      {
                                                     
        Expr *child_expr;
        Relids new_relids;
        Relids new_nullable_relids;

        if (parent_joinrel->reloptkind == RELOPT_JOINREL)
        {
                                                  
          child_expr = (Expr *)adjust_appendrel_attrs(root, (Node *)cur_em->em_expr, nappinfos, appinfos);
        }
        else
        {
                                                  
          Assert(parent_joinrel->reloptkind == RELOPT_OTHER_JOINREL);
          child_expr = (Expr *)adjust_appendrel_attrs_multilevel(root, (Node *)cur_em->em_expr, child_relids, top_parent_relids);
        }

           
                                                              
                                                            
                                                                    
                                                                 
           
        new_relids = bms_difference(cur_em->em_relids, top_parent_relids);
        new_relids = bms_add_members(new_relids, child_relids);

           
                                                                   
                                            
           
        new_nullable_relids = cur_em->em_nullable_relids;
        if (bms_overlap(new_nullable_relids, top_parent_relids))
        {
          new_nullable_relids = adjust_child_relids_multilevel(root, new_nullable_relids, child_relids, top_parent_relids);
        }

        (void)add_eq_member(cur_ec, child_expr, new_relids, new_nullable_relids, true, cur_em->em_datatype);
      }
    }
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                                          
                                                                  
   
                                                                           
                                                                            
                                                                              
                                                                         
                                                 
   
                                                                            
                                                                          
                                                                         
                                                                             
                                                                     
                                                                       
                                                                          
                                                                             
                 
   
                                                                            
                                                           
   
List *
generate_implied_equalities_for_column(PlannerInfo *root, RelOptInfo *rel, ec_matches_callback_type callback, void *callback_arg, Relids prohibited_rels)
{
  List *result = NIL;
  bool is_child_rel = (rel->reloptkind == RELOPT_OTHER_MEMBER_REL);
  Relids parent_relids;
  ListCell *lc1;

                                                                       
  Assert(IS_SIMPLE_REL(rel));

                                                                      
  if (is_child_rel)
  {
    parent_relids = find_childrel_parents(root, rel);
  }
  else
  {
    parent_relids = NULL;                                        
  }

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *cur_ec = (EquivalenceClass *)lfirst(lc1);
    EquivalenceMember *cur_em;
    ListCell *lc2;

       
                                                                        
                                          
       
    if (cur_ec->ec_has_const || list_length(cur_ec->ec_members) <= 1)
    {
      continue;
    }

       
                                                                          
                                   
       
    if (!is_child_rel && !bms_is_subset(rel->relids, cur_ec->ec_relids))
    {
      continue;
    }

       
                                                                          
                                                                         
                                                                      
                                                                     
                                                                      
                                                                        
                                                                      
                                                   
       
    cur_em = NULL;
    foreach (lc2, cur_ec->ec_members)
    {
      cur_em = (EquivalenceMember *)lfirst(lc2);
      if (bms_equal(cur_em->em_relids, rel->relids) && callback(root, rel, cur_ec, cur_em, callback_arg))
      {
        break;
      }
      cur_em = NULL;
    }

    if (!cur_em)
    {
      continue;
    }

       
                                                                           
                    
       
    foreach (lc2, cur_ec->ec_members)
    {
      EquivalenceMember *other_em = (EquivalenceMember *)lfirst(lc2);
      Oid eq_op;
      RestrictInfo *rinfo;

      if (other_em->em_is_child)
      {
        continue;                           
      }

                                                        
      if (other_em == cur_em || bms_overlap(other_em->em_relids, rel->relids))
      {
        continue;
      }

                                                              
      if (bms_overlap(other_em->em_relids, prohibited_rels))
      {
        continue;
      }

         
                                                                       
                               
         
      if (is_child_rel && bms_overlap(parent_relids, other_em->em_relids))
      {
        continue;
      }

      eq_op = select_equality_operator(cur_ec, cur_em->em_datatype, other_em->em_datatype);
      if (!OidIsValid(eq_op))
      {
        continue;
      }

                                                                     
      rinfo = create_join_clause(root, cur_ec, eq_op, cur_em, other_em, cur_ec);

      result = lappend(result, rinfo);
    }

       
                                                                         
                                                                         
                                                                          
       
    if (result)
    {
      break;
    }
  }

  return result;
}

   
                                   
                                                                   
                                                    
   
                                                  
                                                                               
                                                                               
                                                                               
   
bool
have_relevant_eclass_joinclause(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{
  ListCell *lc1;

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *ec = (EquivalenceClass *)lfirst(lc1);

       
                                                                         
                          
       
    if (list_length(ec->ec_members) <= 1)
    {
      continue;
    }

       
                                                                           
                                                                         
                                                                           
                                                                       
                                                                         
                                                                        
                                                                  
                   
       
                                                                          
                                                                           
                                                  
       
                                                                           
                                                                           
                                                                          
                                                                         
                                         
       
    if (bms_overlap(rel1->relids, ec->ec_relids) && bms_overlap(rel2->relids, ec->ec_relids))
    {
      return true;
    }
  }

  return false;
}

   
                                  
                                                                   
                                                                 
   
                                                                          
                                                         
   
bool
has_relevant_eclass_joinclause(PlannerInfo *root, RelOptInfo *rel1)
{
  ListCell *lc1;

  foreach (lc1, root->eq_classes)
  {
    EquivalenceClass *ec = (EquivalenceClass *)lfirst(lc1);

       
                                                                         
                          
       
    if (list_length(ec->ec_members) <= 1)
    {
      continue;
    }

       
                                                                           
                                                                     
       
    if (bms_overlap(rel1->relids, ec->ec_relids) && !bms_is_subset(ec->ec_relids, rel1->relids))
    {
      return true;
    }
  }

  return false;
}

   
                             
                                                                        
                                     
   
                                                                           
                                                                           
                                                                           
                                  
   
bool
eclass_useful_for_merging(PlannerInfo *root, EquivalenceClass *eclass, RelOptInfo *rel)
{
  Relids relids;
  ListCell *lc;

  Assert(!eclass->ec_merged);

     
                                                                           
                                   
     
  if (eclass->ec_has_const || list_length(eclass->ec_members) <= 1)
  {
    return false;
  }

     
                                                                             
                                                                         
                                        
     

                                                                            
  if (IS_OTHER_REL(rel))
  {
    Assert(!bms_is_empty(rel->top_parent_relids));
    relids = rel->top_parent_relids;
  }
  else
  {
    relids = rel->relids;
  }

                                                                            
  if (bms_is_subset(eclass->ec_relids, relids))
  {
    return false;
  }

                                                      
  foreach (lc, eclass->ec_members)
  {
    EquivalenceMember *cur_em = (EquivalenceMember *)lfirst(lc);

    if (cur_em->em_is_child)
    {
      continue;                           
    }

    if (!bms_overlap(cur_em->em_relids, relids))
    {
      return true;
    }
  }

  return false;
}

   
                               
                                                                            
                                                                        
                                  
   
bool
is_redundant_derived_clause(RestrictInfo *rinfo, List *clauselist)
{
  EquivalenceClass *parent_ec = rinfo->parent_ec;
  ListCell *lc;

                                                                    
  if (parent_ec == NULL)
  {
    return false;
  }

  foreach (lc, clauselist)
  {
    RestrictInfo *otherrinfo = (RestrictInfo *)lfirst(lc);

    if (otherrinfo->parent_ec == parent_ec)
    {
      return true;
    }
  }

  return false;
}

   
                                  
                                                                       
                                                                   
                                                                      
   
bool
is_redundant_with_indexclauses(RestrictInfo *rinfo, List *indexclauses)
{
  EquivalenceClass *parent_ec = rinfo->parent_ec;
  ListCell *lc;

  foreach (lc, indexclauses)
  {
    IndexClause *iclause = lfirst_node(IndexClause, lc);
    RestrictInfo *otherrinfo = iclause->rinfo;

                                                                         
    if (iclause->lossy)
    {
      continue;
    }

                                                                       
    if (rinfo == otherrinfo)
    {
      return true;
    }
                                       
    if (parent_ec && otherrinfo->parent_ec == parent_ec)
    {
      return true;
    }

       
                                                                           
                                                   
       
  }

  return false;
}
