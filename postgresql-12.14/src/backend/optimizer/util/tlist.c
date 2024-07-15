                                                                            
   
           
                                       
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/tlist.h"

                                                                              
#define IS_SRF_CALL(node) ((IsA(node, FuncExpr) && ((FuncExpr *)(node))->funcretset) || (IsA(node, OpExpr) && ((OpExpr *)(node))->opretset))

   
                                                                             
                                                                              
                                                                              
   
typedef struct
{
  Node *expr;                                                 
  Index sortgroupref;                                     
} split_pathtarget_item;

typedef struct
{
                                           
  List *input_target_exprs;                                 
                                                           
  List *level_srfs;                                                
  List *level_input_vars;                                      
  List *level_input_srfs;                                      
                                                  
  List *current_input_vars;                                     
  List *current_input_srfs;                                     
                                                                     
  int current_depth;                                         
  Index current_sgref;                                           
} split_pathtarget_context;

static bool
split_pathtarget_walker(Node *node, split_pathtarget_context *context);
static void
add_sp_item_to_pathtarget(PathTarget *target, split_pathtarget_item *item);
static void
add_sp_items_to_pathtarget(PathTarget *target, List *items);

                                                                               
                                                 
                                                                               

   
                
                                                                     
                                                                         
   
TargetEntry *
tlist_member(Expr *node, List *targetlist)
{
  ListCell *temp;

  foreach (temp, targetlist)
  {
    TargetEntry *tlentry = (TargetEntry *)lfirst(temp);

    if (equal(node, tlentry->expr))
    {
      return tlentry;
    }
  }
  return NULL;
}

   
                               
                                                                      
                                                                    
                                                  
   
TargetEntry *
tlist_member_ignore_relabel(Expr *node, List *targetlist)
{
  ListCell *temp;

  while (node && IsA(node, RelabelType))
  {
    node = ((RelabelType *)node)->arg;
  }

  foreach (temp, targetlist)
  {
    TargetEntry *tlentry = (TargetEntry *)lfirst(temp);
    Expr *tlexpr = tlentry->expr;

    while (tlexpr && IsA(tlexpr, RelabelType))
    {
      tlexpr = ((RelabelType *)tlexpr)->arg;
    }

    if (equal(node, tlexpr))
    {
      return tlentry;
    }
  }
  return NULL;
}

   
                          
                                                                       
                                                                           
   
                                                                          
                                                           
   
static TargetEntry *
tlist_member_match_var(Var *var, List *targetlist)
{
  ListCell *temp;

  foreach (temp, targetlist)
  {
    TargetEntry *tlentry = (TargetEntry *)lfirst(temp);
    Var *tlvar = (Var *)tlentry->expr;

    if (!tlvar || !IsA(tlvar, Var))
    {
      continue;
    }
    if (var->varno == tlvar->varno && var->varattno == tlvar->varattno && var->varlevelsup == tlvar->varlevelsup && var->vartype == tlvar->vartype)
    {
      return tlentry;
    }
  }
  return NULL;
}

   
                     
                                                                       
   
                                  
                                                                         
   
                               
   
List *
add_to_flat_tlist(List *tlist, List *exprs)
{
  int next_resno = list_length(tlist) + 1;
  ListCell *lc;

  foreach (lc, exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);

    if (!tlist_member(expr, tlist))
    {
      TargetEntry *tle;

      tle = makeTargetEntry(copyObject(expr),                    
          next_resno++, NULL, false);
      tlist = lappend(tlist, tle);
    }
  }
  return tlist;
}

   
                   
                                                
   
                                                          
   
List *
get_tlist_exprs(List *tlist, bool includeJunk)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resjunk && !includeJunk)
    {
      continue;
    }

    result = lappend(result, tle->expr);
  }
  return result;
}

   
                               
                     
   
int
count_nonjunk_tlist_entries(List *tlist)
{
  int len = 0;
  ListCell *l;

  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (!tle->resjunk)
    {
      len++;
    }
  }
  return len;
}

   
                    
                                                                
   
                                                                              
                                                                               
                                                                        
                                                                               
                                                                              
                                                                              
                                                                             
                                                                              
                                                                             
                                                                             
   
bool
tlist_same_exprs(List *tlist1, List *tlist2)
{
  ListCell *lc1, *lc2;

  if (list_length(tlist1) != list_length(tlist2))
  {
    return false;                                      
  }

  forboth(lc1, tlist1, lc2, tlist2)
  {
    TargetEntry *tle1 = (TargetEntry *)lfirst(lc1);
    TargetEntry *tle2 = (TargetEntry *)lfirst(lc2);

    if (!equal(tle1->expr, tle2->expr))
    {
      return false;
    }
  }

  return true;
}

   
                                                                
   
                                                                        
                                                        
   
                                                            
   
bool
tlist_same_datatypes(List *tlist, List *colTypes, bool junkOK)
{
  ListCell *l;
  ListCell *curColType = list_head(colTypes);

  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resjunk)
    {
      if (!junkOK)
      {
        return false;
      }
    }
    else
    {
      if (curColType == NULL)
      {
        return false;                                 
      }
      if (exprType((Node *)tle->expr) != lfirst_oid(curColType))
      {
        return false;
      }
      curColType = lnext(curColType);
    }
  }
  if (curColType != NULL)
  {
    return false;                                  
  }
  return true;
}

   
                                                                       
   
                                                     
   
bool
tlist_same_collations(List *tlist, List *colCollations, bool junkOK)
{
  ListCell *l;
  ListCell *curColColl = list_head(colCollations);

  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resjunk)
    {
      if (!junkOK)
      {
        return false;
      }
    }
    else
    {
      if (curColColl == NULL)
      {
        return false;                                      
      }
      if (exprCollation((Node *)tle->expr) != lfirst_oid(curColColl))
      {
        return false;
      }
      curColColl = lnext(curColColl);
    }
  }
  if (curColColl != NULL)
  {
    return false;                                       
  }
  return true;
}

   
                        
                                                                         
   
                                                                            
               
   
void
apply_tlist_labeling(List *dest_tlist, List *src_tlist)
{
  ListCell *ld, *ls;

  Assert(list_length(dest_tlist) == list_length(src_tlist));
  forboth(ld, dest_tlist, ls, src_tlist)
  {
    TargetEntry *dest_tle = (TargetEntry *)lfirst(ld);
    TargetEntry *src_tle = (TargetEntry *)lfirst(ls);

    Assert(dest_tle->resno == src_tle->resno);
    dest_tle->resname = src_tle->resname;
    dest_tle->ressortgroupref = src_tle->ressortgroupref;
    dest_tle->resorigtbl = src_tle->resorigtbl;
    dest_tle->resorigcol = src_tle->resorigcol;
    dest_tle->resjunk = src_tle->resjunk;
  }
}

   
                        
                                                                     
                   
   
TargetEntry *
get_sortgroupref_tle(Index sortref, List *targetList)
{
  ListCell *l;

  foreach (l, targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->ressortgroupref == sortref)
    {
      return tle;
    }
  }

  elog(ERROR, "ORDER/GROUP BY expression not found in targetlist");
  return NULL;                          
}

   
                           
                                                                 
                                       
   
TargetEntry *
get_sortgroupclause_tle(SortGroupClause *sgClause, List *targetList)
{
  return get_sortgroupref_tle(sgClause->tleSortGroupRef, targetList);
}

   
                            
                                                                 
                                                   
   
Node *
get_sortgroupclause_expr(SortGroupClause *sgClause, List *targetList)
{
  TargetEntry *tle = get_sortgroupclause_tle(sgClause, targetList);

  return (Node *)tle->expr;
}

   
                           
                                                   
                                              
   
List *
get_sortgrouplist_exprs(List *sgClauses, List *targetList)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, sgClauses)
  {
    SortGroupClause *sortcl = (SortGroupClause *)lfirst(l);
    Node *sortexpr;

    sortexpr = get_sortgroupclause_expr(sortcl, targetList);
    result = lappend(result, sortexpr);
  }
  return result;
}

                                                                               
                                                              
   
                                                                             
                                                                        
                                                                               

   
                           
                                                                    
                   
   
SortGroupClause *
get_sortgroupref_clause(Index sortref, List *clauses)
{
  ListCell *l;

  foreach (l, clauses)
  {
    SortGroupClause *cl = (SortGroupClause *)lfirst(l);

    if (cl->tleSortGroupRef == sortref)
    {
      return cl;
    }
  }

  elog(ERROR, "ORDER/GROUP BY expression not found in list");
  return NULL;                          
}

   
                                 
                                                                          
   
SortGroupClause *
get_sortgroupref_clause_noerr(Index sortref, List *clauses)
{
  ListCell *l;

  foreach (l, clauses)
  {
    SortGroupClause *cl = (SortGroupClause *)lfirst(l);

    if (cl->tleSortGroupRef == sortref)
    {
      return cl;
    }
  }

  return NULL;
}

   
                                                                      
                               
   
Oid *
extract_grouping_ops(List *groupClause)
{
  int numCols = list_length(groupClause);
  int colno = 0;
  Oid *groupOperators;
  ListCell *glitem;

  groupOperators = (Oid *)palloc(sizeof(Oid) * numCols);

  foreach (glitem, groupClause)
  {
    SortGroupClause *groupcl = (SortGroupClause *)lfirst(glitem);

    groupOperators[colno] = groupcl->eqop;
    Assert(OidIsValid(groupOperators[colno]));
    colno++;
  }

  return groupOperators;
}

   
                                                                                 
                               
   
Oid *
extract_grouping_collations(List *groupClause, List *tlist)
{
  int numCols = list_length(groupClause);
  int colno = 0;
  Oid *grpCollations;
  ListCell *glitem;

  grpCollations = (Oid *)palloc(sizeof(Oid) * numCols);

  foreach (glitem, groupClause)
  {
    SortGroupClause *groupcl = (SortGroupClause *)lfirst(glitem);
    TargetEntry *tle = get_sortgroupclause_tle(groupcl, tlist);

    grpCollations[colno++] = exprCollation((Node *)tle->expr);
  }

  return grpCollations;
}

   
                                                                       
                               
   
AttrNumber *
extract_grouping_cols(List *groupClause, List *tlist)
{
  AttrNumber *grpColIdx;
  int numCols = list_length(groupClause);
  int colno = 0;
  ListCell *glitem;

  grpColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numCols);

  foreach (glitem, groupClause)
  {
    SortGroupClause *groupcl = (SortGroupClause *)lfirst(glitem);
    TargetEntry *tle = get_sortgroupclause_tle(groupcl, tlist);

    grpColIdx[colno++] = tle->resno;
  }

  return grpColIdx;
}

   
                                                                                
   
                                                                            
   
bool
grouping_is_sortable(List *groupClause)
{
  ListCell *glitem;

  foreach (glitem, groupClause)
  {
    SortGroupClause *groupcl = (SortGroupClause *)lfirst(glitem);

    if (!OidIsValid(groupcl->sortop))
    {
      return false;
    }
  }
  return true;
}

   
                                                                                
   
                                                                  
   
bool
grouping_is_hashable(List *groupClause)
{
  ListCell *glitem;

  foreach (glitem, groupClause)
  {
    SortGroupClause *groupcl = (SortGroupClause *)lfirst(glitem);

    if (!groupcl->hashable)
    {
      return false;
    }
  }
  return true;
}

                                                                               
                                      
   
                                                                           
                                                                               
                                                           
                                                                               

   
                              
                                                                
   
                                                                            
                                                       
   
PathTarget *
make_pathtarget_from_tlist(List *tlist)
{
  PathTarget *target = makeNode(PathTarget);
  int i;
  ListCell *lc;

  target->sortgrouprefs = (Index *)palloc(list_length(tlist) * sizeof(Index));

  i = 0;
  foreach (lc, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);

    target->exprs = lappend(target->exprs, tle->expr);
    target->sortgrouprefs[i] = tle->ressortgroupref;
    i++;
  }

  return target;
}

   
                              
                                               
   
List *
make_tlist_from_pathtarget(PathTarget *target)
{
  List *tlist = NIL;
  int i;
  ListCell *lc;

  i = 0;
  foreach (lc, target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);
    TargetEntry *tle;

    tle = makeTargetEntry(expr, i + 1, NULL, false);
    if (target->sortgrouprefs)
    {
      tle->ressortgroupref = target->sortgrouprefs[i];
    }
    tlist = lappend(tlist, tle);
    i++;
  }

  return tlist;
}

   
                   
                        
   
                                                                        
                                                                          
                                                                        
   
PathTarget *
copy_pathtarget(PathTarget *src)
{
  PathTarget *dst = makeNode(PathTarget);

                          
  memcpy(dst, src, sizeof(PathTarget));
                                        
  dst->exprs = list_copy(src->exprs);
                                                                        
  if (src->sortgrouprefs)
  {
    Size nbytes = list_length(src->exprs) * sizeof(Index);

    dst->sortgrouprefs = (Index *)palloc(nbytes);
    memcpy(dst->sortgrouprefs, src->sortgrouprefs, nbytes);
  }
  return dst;
}

   
                           
                                                           
   
PathTarget *
create_empty_pathtarget(void)
{
                                                                     
  return makeNode(PathTarget);
}

   
                            
                                              
   
                                                                           
                              
   
void
add_column_to_pathtarget(PathTarget *target, Expr *expr, Index sortgroupref)
{
                                           
  target->exprs = lappend(target->exprs, expr);
                                                
  if (target->sortgrouprefs)
  {
    int nexprs = list_length(target->exprs);

                                                                      
    target->sortgrouprefs = (Index *)repalloc(target->sortgrouprefs, nexprs * sizeof(Index));
    target->sortgrouprefs[nexprs - 1] = sortgroupref;
  }
  else if (sortgroupref)
  {
                                                                       
    int nexprs = list_length(target->exprs);

    target->sortgrouprefs = (Index *)palloc0(nexprs * sizeof(Index));
    target->sortgrouprefs[nexprs - 1] = sortgroupref;
  }
}

   
                                
                                                                   
                                                   
   
                                                                           
                                             
   
                                                                           
                              
   
void
add_new_column_to_pathtarget(PathTarget *target, Expr *expr)
{
  if (!list_member(target->exprs, expr))
  {
    add_column_to_pathtarget(target, expr, 0);
  }
}

   
                                 
                                                                       
   
void
add_new_columns_to_pathtarget(PathTarget *target, List *exprs)
{
  ListCell *lc;

  foreach (lc, exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);

    add_new_column_to_pathtarget(target, expr);
  }
}

   
                                      
                                                                        
   
                                                                          
                                                                        
                                                                      
                                   
   
void
apply_pathtarget_labeling_to_tlist(List *tlist, PathTarget *target)
{
  int i;
  ListCell *lc;

                                                             
  if (target->sortgrouprefs == NULL)
  {
    return;
  }

  i = 0;
  foreach (lc, target->exprs)
  {
    Expr *expr = (Expr *)lfirst(lc);
    TargetEntry *tle;

    if (target->sortgrouprefs[i])
    {
         
                                                                        
                                                                      
                                                                       
                                                                     
                                                                       
                                                                        
                                                                         
         
      if (expr && IsA(expr, Var))
      {
        tle = tlist_member_match_var((Var *)expr, tlist);
      }
      else
      {
        tle = tlist_member(expr, tlist);
      }

         
                                                                     
                                                                       
                                                                     
                        
         
      if (!tle)
      {
        elog(ERROR, "ORDER/GROUP BY expression not found in targetlist");
      }
      if (tle->ressortgroupref != 0 && tle->ressortgroupref != target->sortgrouprefs[i])
      {
        elog(ERROR, "targetlist item has multiple sortgroupref labels");
      }

      tle->ressortgroupref = target->sortgrouprefs[i];
    }
    i++;
  }
}

   
                            
                                                                        
   
                                                                           
                                                                               
                                                                               
                                                                             
                                                     
   
                                                
                          
                                                                             
                                        
                         
                                                 
                   
                                                 
            
                                                                           
                                                                        
                                                          
                
                     
                            
            
                                       
   
                      
                           
                                                                        
                     
                                                                              
                                       
   
                                                                               
                                                                           
                                                                             
                                                                          
   
                                                                              
                                                                               
   
                                                                      
                                                                      
                                                                     
                                                                         
                                                                        
                                                                              
                                                                         
                                                                              
                                                                           
   
                                                                        
                                                                   
                                                                  
   
void
split_pathtarget_at_srfs(PlannerInfo *root, PathTarget *target, PathTarget *input_target, List **targets, List **targets_contain_srfs)
{
  split_pathtarget_context context;
  int max_depth;
  bool need_extra_projection;
  List *prev_level_tlist;
  int lci;
  ListCell *lc, *lc1, *lc2, *lc3;

     
                                                                        
                                                                         
                                                                       
                                                              
     
  if (target == input_target)
  {
    *targets = list_make1(target);
    *targets_contain_srfs = list_make1_int(false);
    return;
  }

                                                                     
  context.input_target_exprs = input_target ? input_target->exprs : NIL;

     
                                                                       
                                                                             
                                                                             
                                                                             
                                                                         
     
  context.level_srfs = list_make1(NIL);
  context.level_input_vars = list_make1(NIL);
  context.level_input_srfs = list_make1(NIL);

                                                                          
  context.current_input_vars = NIL;
  context.current_input_srfs = NIL;
  max_depth = 0;
  need_extra_projection = false;

                                                               
  lci = 0;
  foreach (lc, target->exprs)
  {
    Node *node = (Node *)lfirst(lc);

                                                                     
    context.current_sgref = get_pathtarget_sortgroupref(target, lci);
    lci++;

       
                                                                           
                                                                    
       
    context.current_depth = 0;
    split_pathtarget_walker(node, &context);

                                                                    
    if (context.current_depth == 0)
    {
      continue;
    }

       
                                                                        
                                                                      
                                                                         
                                                                      
       
    if (max_depth < context.current_depth)
    {
      max_depth = context.current_depth;
      need_extra_projection = false;
    }

       
                                                                           
                                                                       
                   
       
    if (max_depth == context.current_depth && !IS_SRF_CALL(node))
    {
      need_extra_projection = true;
    }
  }

     
                                                                            
                                                                            
                                             
     
  if (max_depth == 0)
  {
    *targets = list_make1(target);
    *targets_contain_srfs = list_make1_int(false);
    return;
  }

     
                                                                           
                                                                            
                                                  
     
  if (need_extra_projection)
  {
    context.level_srfs = lappend(context.level_srfs, NIL);
    context.level_input_vars = lappend(context.level_input_vars, context.current_input_vars);
    context.level_input_srfs = lappend(context.level_input_srfs, context.current_input_srfs);
  }
  else
  {
    lc = list_nth_cell(context.level_input_vars, max_depth);
    lfirst(lc) = list_concat(lfirst(lc), context.current_input_vars);
    lc = list_nth_cell(context.level_input_srfs, max_depth);
    lfirst(lc) = list_concat(lfirst(lc), context.current_input_srfs);
  }

     
                                                                            
                                                                            
                                                                         
                                                   
     
  *targets = *targets_contain_srfs = NIL;
  prev_level_tlist = NIL;

  forthree(lc1, context.level_srfs, lc2, context.level_input_vars, lc3, context.level_input_srfs)
  {
    List *level_srfs = (List *)lfirst(lc1);
    PathTarget *ntarget;

    if (lnext(lc1) == NULL)
    {
      ntarget = target;
    }
    else
    {
      ntarget = create_empty_pathtarget();

         
                                                                      
                                                                     
                                                                      
                       
         
      add_sp_items_to_pathtarget(ntarget, level_srfs);
      for_each_cell(lc, lnext(lc2))
      {
        List *input_vars = (List *)lfirst(lc);

        add_sp_items_to_pathtarget(ntarget, input_vars);
      }
      for_each_cell(lc, lnext(lc3))
      {
        List *input_srfs = (List *)lfirst(lc);
        ListCell *lcx;

        foreach (lcx, input_srfs)
        {
          split_pathtarget_item *item = lfirst(lcx);

          if (list_member(prev_level_tlist, item->expr))
          {
            add_sp_item_to_pathtarget(ntarget, item);
          }
        }
      }
      set_pathtarget_cost_width(root, ntarget);
    }

       
                                                                         
       
    *targets = lappend(*targets, ntarget);
    *targets_contain_srfs = lappend_int(*targets_contain_srfs, (level_srfs != NIL));

                                                    
    prev_level_tlist = ntarget->exprs;
  }
}

   
                                                                 
   
                                                                          
                                                   
   
static bool
split_pathtarget_walker(Node *node, split_pathtarget_context *context)
{
  if (node == NULL)
  {
    return false;
  }

     
                                                                    
                                                                           
                                                                           
                                                                
                                                                            
                                                                    
     
  if (list_member(context->input_target_exprs, node))
  {
    split_pathtarget_item *item = palloc(sizeof(split_pathtarget_item));

    item->expr = node;
    item->sortgroupref = context->current_sgref;
    context->current_input_vars = lappend(context->current_input_vars, item);
    return false;
  }

     
                                                                            
                                                                           
                                                                    
     
  if (IsA(node, Var) || IsA(node, PlaceHolderVar) || IsA(node, Aggref) || IsA(node, GroupingFunc) || IsA(node, WindowFunc))
  {
    split_pathtarget_item *item = palloc(sizeof(split_pathtarget_item));

    item->expr = node;
    item->sortgroupref = context->current_sgref;
    context->current_input_vars = lappend(context->current_input_vars, item);
    return false;
  }

     
                                                                             
                                                   
     
  if (IS_SRF_CALL(node))
  {
    split_pathtarget_item *item = palloc(sizeof(split_pathtarget_item));
    List *save_input_vars = context->current_input_vars;
    List *save_input_srfs = context->current_input_srfs;
    int save_current_depth = context->current_depth;
    int srf_depth;
    ListCell *lc;

    item->expr = node;
    item->sortgroupref = context->current_sgref;

    context->current_input_vars = NIL;
    context->current_input_srfs = NIL;
    context->current_depth = 0;
    context->current_sgref = 0;                                             

    (void)expression_tree_walker(node, split_pathtarget_walker, (void *)context);

                                                 
    srf_depth = context->current_depth + 1;

                                                                       
    if (srf_depth >= list_length(context->level_srfs))
    {
      context->level_srfs = lappend(context->level_srfs, NIL);
      context->level_input_vars = lappend(context->level_input_vars, NIL);
      context->level_input_srfs = lappend(context->level_input_srfs, NIL);
    }

                                                                         
    lc = list_nth_cell(context->level_srfs, srf_depth);
    lfirst(lc) = lappend(lfirst(lc), item);

                                                             
    lc = list_nth_cell(context->level_input_vars, srf_depth);
    lfirst(lc) = list_concat(lfirst(lc), context->current_input_vars);
    lc = list_nth_cell(context->level_input_srfs, srf_depth);
    lfirst(lc) = list_concat(lfirst(lc), context->current_input_srfs);

       
                                                                          
                                                                         
                               
       
    context->current_input_vars = save_input_vars;
    context->current_input_srfs = lappend(save_input_srfs, item);
    context->current_depth = Max(save_current_depth, srf_depth);

                         
    return false;
  }

     
                                                                         
                         
     
  context->current_sgref = 0;                                             
  return expression_tree_walker(node, split_pathtarget_walker, (void *)context);
}

   
                                                                            
                                                                           
                                                                          
                                                                      
                 
   
                                                                          
                                                                             
                                                                         
                                                                            
                                        
   
static void
add_sp_item_to_pathtarget(PathTarget *target, split_pathtarget_item *item)
{
  int lci;
  ListCell *lc;

     
                                                                       
                                       
     
  lci = 0;
  foreach (lc, target->exprs)
  {
    Node *node = (Node *)lfirst(lc);
    Index sgref = get_pathtarget_sortgroupref(target, lci);

    if ((item->sortgroupref == sgref || item->sortgroupref == 0 || sgref == 0) && equal(item->expr, node))
    {
                                                                     
      if (item->sortgroupref)
      {
        if (target->sortgrouprefs == NULL)
        {
          target->sortgrouprefs = (Index *)palloc0(list_length(target->exprs) * sizeof(Index));
        }
        target->sortgrouprefs[lci] = item->sortgroupref;
      }
      return;
    }
    lci++;
  }

     
                                                                     
     
  add_column_to_pathtarget(target, (Expr *)copyObject(item->expr), item->sortgroupref);
}

   
                                                            
   
static void
add_sp_items_to_pathtarget(PathTarget *target, List *items)
{
  ListCell *lc;

  foreach (lc, items)
  {
    split_pathtarget_item *item = lfirst(lc);

    add_sp_item_to_pathtarget(target, item);
  }
}
