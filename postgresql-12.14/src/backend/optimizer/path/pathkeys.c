                                                                            
   
              
                                                   
   
                                                                          
                                    
   
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/stratnum.h"
#include "catalog/pg_opfamily.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/plannodes.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "partitioning/partbounds.h"
#include "utils/lsyscache.h"

static bool
pathkey_is_redundant(PathKey *new_pathkey, List *pathkeys);
static bool
matches_boolean_partition_clause(RestrictInfo *rinfo, RelOptInfo *partrel, int partkeycol);
static Var *
find_var_for_subquery_tle(RelOptInfo *rel, TargetEntry *tle);
static bool
right_merge_direction(PlannerInfo *root, PathKey *pathkey);

                                                                              
                                                
                                                                              

   
                          
                                                                        
                                                                      
                                       
   
                                                                          
                                                                             
                                                                           
                         
   
PathKey *
make_canonical_pathkey(PlannerInfo *root, EquivalenceClass *eclass, Oid opfamily, int strategy, bool nulls_first)
{
  PathKey *pk;
  ListCell *lc;
  MemoryContext oldcontext;

                                                                        
  while (eclass->ec_merged)
  {
    eclass = eclass->ec_merged;
  }

  foreach (lc, root->canon_pathkeys)
  {
    pk = (PathKey *)lfirst(lc);
    if (eclass == pk->pk_eclass && opfamily == pk->pk_opfamily && strategy == pk->pk_strategy && nulls_first == pk->pk_nulls_first)
    {
      return pk;
    }
  }

     
                                                                            
                                                          
     
  oldcontext = MemoryContextSwitchTo(root->planner_cxt);

  pk = makeNode(PathKey);
  pk->pk_eclass = eclass;
  pk->pk_opfamily = opfamily;
  pk->pk_strategy = strategy;
  pk->pk_nulls_first = nulls_first;

  root->canon_pathkeys = lappend(root->canon_pathkeys, pk);

  MemoryContextSwitchTo(oldcontext);

  return pk;
}

   
                        
                                                                 
   
                        
   
                                                                            
                                                                             
                                            
                                                                           
                                                                             
                                                                          
                                                                             
                                                                             
                                                                   
   
                                                                        
                                                                              
                               
                                    
                                           
                                                                             
                                                                       
                                                                            
                                                                   
   
                                                                          
                                                                           
                                                                        
                                                                          
                                                    
   
                                                                               
                                                                              
   
static bool
pathkey_is_redundant(PathKey *new_pathkey, List *pathkeys)
{
  EquivalenceClass *new_ec = new_pathkey->pk_eclass;
  ListCell *lc;

                                                                        
  if (EC_MUST_BE_REDUNDANT(new_ec))
  {
    return true;
  }

                                                       
  foreach (lc, pathkeys)
  {
    PathKey *old_pathkey = (PathKey *)lfirst(lc);

    if (new_ec == old_pathkey->pk_eclass)
    {
      return true;
    }
  }

  return false;
}

   
                              
                                                                       
                                                                            
   
                                                                         
                                           
   
                                                                               
                                                       
   
                                                                           
                                                                            
                                                                            
                              
   
                                                                      
                                                                           
                                                           
   
static PathKey *
make_pathkey_from_sortinfo(PlannerInfo *root, Expr *expr, Relids nullable_relids, Oid opfamily, Oid opcintype, Oid collation, bool reverse_sort, bool nulls_first, Index sortref, Relids rel, bool create_it)
{
  int16 strategy;
  Oid equality_op;
  List *opfamilies;
  EquivalenceClass *eclass;

  strategy = reverse_sort ? BTGreaterStrategyNumber : BTLessStrategyNumber;

     
                                                                           
                                                                           
                                                                            
                                      
     
  equality_op = get_opfamily_member(opfamily, opcintype, opcintype, BTEqualStrategyNumber);
  if (!OidIsValid(equality_op))                       
  {
    elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", BTEqualStrategyNumber, opcintype, opcintype, opfamily);
  }
  opfamilies = get_mergejoin_opfamilies(equality_op);
  if (!opfamilies)                                 
  {
    elog(ERROR, "could not find opfamilies for equality operator %u", equality_op);
  }

                                                                   
  eclass = get_eclass_for_sort_expr(root, expr, nullable_relids, opfamilies, opcintype, collation, sortref, rel, create_it);

                                    
  if (!eclass)
  {
    return NULL;
  }

                                                        
  return make_canonical_pathkey(root, eclass, opfamily, strategy, nulls_first);
}

   
                            
                                                                     
   
                                                                              
          
   
static PathKey *
make_pathkey_from_sortop(PlannerInfo *root, Expr *expr, Relids nullable_relids, Oid ordering_op, bool nulls_first, Index sortref, bool create_it)
{
  Oid opfamily, opcintype, collation;
  int16 strategy;

                                                                 
  if (!get_ordering_op_properties(ordering_op, &opfamily, &opcintype, &strategy))
  {
    elog(ERROR, "operator %u is not a valid ordering operator", ordering_op);
  }

                                                                         
  collation = exprCollation((Node *)expr);

  return make_pathkey_from_sortinfo(root, expr, nullable_relids, opfamily, opcintype, collation, (strategy == BTGreaterStrategyNumber), nulls_first, sortref, NULL, create_it);
}

                                                                              
                        
                                                                              

   
                    
                                                                            
                                     
   
                                                                          
                                            
   
PathKeysComparison
compare_pathkeys(List *keys1, List *keys2)
{
  ListCell *key1, *key2;

     
                                                                         
                                                                      
                       
     
  if (keys1 == keys2)
  {
    return PATHKEYS_EQUAL;
  }

  forboth(key1, keys1, key2, keys2)
  {
    PathKey *pathkey1 = (PathKey *)lfirst(key1);
    PathKey *pathkey2 = (PathKey *)lfirst(key2);

    if (pathkey1 != pathkey2)
    {
      return PATHKEYS_DIFFERENT;                              
    }
  }

     
                                                                     
                             
     
  if (key1 != NULL)
  {
    return PATHKEYS_BETTER1;                     
  }
  if (key2 != NULL)
  {
    return PATHKEYS_BETTER2;                     
  }
  return PATHKEYS_EQUAL;
}

   
                         
                                                                   
                                                    
   
bool
pathkeys_contained_in(List *keys1, List *keys2)
{
  switch (compare_pathkeys(keys1, keys2))
  {
  case PATHKEYS_EQUAL:
  case PATHKEYS_BETTER2:
    return true;
  default:
    break;
  }
  return false;
}

   
                                  
                                                                        
                                                        
                                  
   
                                                                           
                                                                  
                                                                              
                                                  
                                                                          
   
Path *
get_cheapest_path_for_pathkeys(List *paths, List *pathkeys, Relids required_outer, CostSelector cost_criterion, bool require_parallel_safe)
{
  Path *matched_path = NULL;
  ListCell *l;

  foreach (l, paths)
  {
    Path *path = (Path *)lfirst(l);

       
                                                                          
                                              
       
    if (matched_path != NULL && compare_path_costs(matched_path, path, cost_criterion) <= 0)
    {
      continue;
    }

    if (require_parallel_safe && !path->parallel_safe)
    {
      continue;
    }

    if (pathkeys_contained_in(pathkeys, path->pathkeys) && bms_is_subset(PATH_REQ_OUTER(path), required_outer))
    {
      matched_path = path;
    }
  }
  return matched_path;
}

   
                                             
                                                                        
                                                                         
                                  
   
                                                                              
              
   
                                                                           
                                                                  
                                                                              
                                                                           
   
Path *
get_cheapest_fractional_path_for_pathkeys(List *paths, List *pathkeys, Relids required_outer, double fraction)
{
  Path *matched_path = NULL;
  ListCell *l;

  foreach (l, paths)
  {
    Path *path = (Path *)lfirst(l);

       
                                                                          
                                              
       
    if (matched_path != NULL && compare_fractional_path_costs(matched_path, path, fraction) <= 0)
    {
      continue;
    }

    if (pathkeys_contained_in(pathkeys, path->pathkeys) && bms_is_subset(PATH_REQ_OUTER(path), required_outer))
    {
      matched_path = path;
    }
  }
  return matched_path;
}

   
                                          
                                                                            
   
Path *
get_cheapest_parallel_safe_total_inner(List *paths)
{
  ListCell *l;

  foreach (l, paths)
  {
    Path *innerpath = (Path *)lfirst(l);

    if (innerpath->parallel_safe && bms_is_empty(PATH_REQ_OUTER(innerpath)))
    {
      return innerpath;
    }
  }

  return NULL;
}

                                                                              
                          
                                                                              

   
                        
                                                                           
                                                                        
                                             
   
                                                                        
                                
   
                                                                          
                                                                          
                                                                            
                                       
   
                                                                         
                                                                           
                                                                           
                                                                           
                                                                 
   
List *
build_index_pathkeys(PlannerInfo *root, IndexOptInfo *index, ScanDirection scandir)
{
  List *retval = NIL;
  ListCell *lc;
  int i;

  if (index->sortopfamily == NULL)
  {
    return NIL;                          
  }

  i = 0;
  foreach (lc, index->indextlist)
  {
    TargetEntry *indextle = (TargetEntry *)lfirst(lc);
    Expr *indexkey;
    bool reverse_sort;
    bool nulls_first;
    PathKey *cpathkey;

       
                                                                    
                                   
       
    if (i >= index->nkeycolumns)
    {
      break;
    }

                                                                  
    indexkey = indextle->expr;

    if (ScanDirectionIsBackward(scandir))
    {
      reverse_sort = !index->reverse_sort[i];
      nulls_first = !index->nulls_first[i];
    }
    else
    {
      reverse_sort = index->reverse_sort[i];
      nulls_first = index->nulls_first[i];
    }

       
                                                                          
                                                                      
       
    cpathkey = make_pathkey_from_sortinfo(root, indexkey, NULL, index->sortopfamily[i], index->opcintype[i], index->indexcollations[i], reverse_sort, nulls_first, 0, index->rel->relids, false);

    if (cpathkey)
    {
         
                                                                        
                                                                 
         
      if (!pathkey_is_redundant(cpathkey, retval))
      {
        retval = lappend(retval, cpathkey);
      }
    }
    else
    {
         
                                                                   
                                                                         
                                                                
                                                                         
                                                                        
                                                                         
                                                                     
                                      
         
      if (!indexcol_is_bool_constant_for_query(root, index, i))
      {
        break;
      }
    }

    i++;
  }

  return retval;
}

   
                                      
   
                                                                            
                                                                 
                                                                    
                                                                           
                                                              
                                                                     
                                                                         
                                                                          
                                                                         
                                                                       
                                                                               
                                                                           
                                                                        
   
static bool
partkey_is_bool_constant_for_query(RelOptInfo *partrel, int partkeycol)
{
  PartitionScheme partscheme = partrel->part_scheme;
  ListCell *lc;

                                                                   
  if (!IsBooleanOpfamily(partscheme->partopfamily[partkeycol]))
  {
    return false;
  }

                                                             
  foreach (lc, partrel->baserestrictinfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

                                                       
    if (rinfo->pseudoconstant)
    {
      continue;
    }

                                                                           
    if (matches_boolean_partition_clause(rinfo, partrel, partkeycol))
    {
      return true;
    }
  }

  return false;
}

   
                                    
                                                               
                                                  
   
                                                                          
                                                                  
   
static bool
matches_boolean_partition_clause(RestrictInfo *rinfo, RelOptInfo *partrel, int partkeycol)
{
  Node *clause = (Node *)rinfo->clause;
  Node *partexpr = (Node *)linitial(partrel->partexprs[partkeycol]);

                     
  if (equal(partexpr, clause))
  {
    return true;
  }
                   
  else if (is_notclause(clause))
  {
    Node *arg = (Node *)get_notclausearg((Expr *)clause);

    if (equal(partexpr, arg))
    {
      return true;
    }
  }

  return false;
}

   
                            
                                                                      
                                                                  
                     
   
                                                                      
                                            
   
                                                                             
                                                                      
                  
   
List *
build_partition_pathkeys(PlannerInfo *root, RelOptInfo *partrel, ScanDirection scandir, bool *partialkeys)
{
  List *retval = NIL;
  PartitionScheme partscheme = partrel->part_scheme;
  int i;

  Assert(partscheme != NULL);
  Assert(partitions_are_ordered(partrel->boundinfo, partrel->nparts));
                                               
  Assert(IS_SIMPLE_REL(partrel));

  for (i = 0; i < partscheme->partnatts; i++)
  {
    PathKey *cpathkey;
    Expr *keyCol = (Expr *)linitial(partrel->partexprs[i]);

       
                                                         
       
                                                                      
                                                                         
                                                                   
                                            
       
    cpathkey = make_pathkey_from_sortinfo(root, keyCol, NULL, partscheme->partopfamily[i], partscheme->partopcintype[i], partscheme->partcollation[i], ScanDirectionIsBackward(scandir), ScanDirectionIsBackward(scandir), 0, partrel->relids, false);

    if (cpathkey)
    {
         
                                                                        
                                                                 
         
      if (!pathkey_is_redundant(cpathkey, retval))
      {
        retval = lappend(retval, cpathkey);
      }
    }
    else
    {
         
                                                                       
                                                                         
                                                                
                                                                        
                                                                         
                                                                         
                                                                         
                                      
         
      if (!partkey_is_bool_constant_for_query(partrel, i))
      {
        *partialkeys = true;
        return retval;
      }
    }
  }

  *partialkeys = false;
  return retval;
}

   
                            
                                                                             
                                    
   
                                                                         
                                                                               
   
                                                                           
                                                                       
   
List *
build_expression_pathkey(PlannerInfo *root, Expr *expr, Relids nullable_relids, Oid opno, Relids rel, bool create_it)
{
  List *pathkeys;
  Oid opfamily, opcintype;
  int16 strategy;
  PathKey *cpathkey;

                                                                 
  if (!get_ordering_op_properties(opno, &opfamily, &opcintype, &strategy))
  {
    elog(ERROR, "operator %u is not a valid ordering operator", opno);
  }

  cpathkey = make_pathkey_from_sortinfo(root, expr, nullable_relids, opfamily, opcintype, exprCollation((Node *)expr), (strategy == BTGreaterStrategyNumber), (strategy == BTGreaterStrategyNumber), 0, rel, create_it);

  if (cpathkey)
  {
    pathkeys = list_make1(cpathkey);
  }
  else
  {
    pathkeys = NIL;
  }

  return pathkeys;
}

   
                             
                                                                       
                                                                     
                         
   
                                                              
                                                                      
                                                                     
   
                                                                             
                                                                            
                                                                     
                                                                         
                                                                           
   
List *
convert_subquery_pathkeys(PlannerInfo *root, RelOptInfo *rel, List *subquery_pathkeys, List *subquery_tlist)
{
  List *retval = NIL;
  int retvallen = 0;
  int outer_query_keys = list_length(root->query_pathkeys);
  ListCell *i;

  foreach (i, subquery_pathkeys)
  {
    PathKey *sub_pathkey = (PathKey *)lfirst(i);
    EquivalenceClass *sub_eclass = sub_pathkey->pk_eclass;
    PathKey *best_pathkey = NULL;

    if (sub_eclass->ec_has_volatile)
    {
         
                                                                         
                                                                       
                                     
         
      TargetEntry *tle;
      Var *outer_var;

      if (sub_eclass->ec_sortref == 0)                   
      {
        elog(ERROR, "volatile EquivalenceClass has no sortref");
      }
      tle = get_sortgroupref_tle(sub_eclass->ec_sortref, subquery_tlist);
      Assert(tle);
                                                         
      outer_var = find_var_for_subquery_tle(rel, tle);
      if (outer_var)
      {
                                               
        EquivalenceMember *sub_member;
        EquivalenceClass *outer_ec;

        Assert(list_length(sub_eclass->ec_members) == 1);
        sub_member = (EquivalenceMember *)linitial(sub_eclass->ec_members);

           
                                                                     
                                                             
                                                                      
                                                                      
                                                              
                                                                
                                                                 
                                                                    
           
        outer_ec = get_eclass_for_sort_expr(root, (Expr *)outer_var, NULL, sub_eclass->ec_opfamilies, sub_member->em_datatype, sub_eclass->ec_collation, 0, rel->relids, false);

           
                                                             
                                          
           
        if (outer_ec)
        {
          best_pathkey = make_canonical_pathkey(root, outer_ec, sub_pathkey->pk_opfamily, sub_pathkey->pk_strategy, sub_pathkey->pk_nulls_first);
        }
      }
    }
    else
    {
         
                                                                     
                                                                       
                                                                         
                                                                         
                                                                         
                                                                    
                                                                    
                                                                       
                                                                    
                                                                    
                                                                        
                                                               
                                                                      
                      
         
      int best_score = -1;
      ListCell *j;

      foreach (j, sub_eclass->ec_members)
      {
        EquivalenceMember *sub_member = (EquivalenceMember *)lfirst(j);
        Expr *sub_expr = sub_member->em_expr;
        Oid sub_expr_type = sub_member->em_datatype;
        Oid sub_expr_coll = sub_eclass->ec_collation;
        ListCell *k;

        if (sub_member->em_is_child)
        {
          continue;                           
        }

        foreach (k, subquery_tlist)
        {
          TargetEntry *tle = (TargetEntry *)lfirst(k);
          Var *outer_var;
          Expr *tle_expr;
          EquivalenceClass *outer_ec;
          PathKey *outer_pk;
          int score;

                                                             
          outer_var = find_var_for_subquery_tle(rel, tle);
          if (!outer_var)
          {
            continue;
          }

             
                                                               
                                                               
                                                                 
                      
             
          tle_expr = canonicalize_ec_expression(tle->expr, sub_expr_type, sub_expr_coll);
          if (!equal(tle_expr, sub_expr))
          {
            continue;
          }

                                                        
          outer_ec = get_eclass_for_sort_expr(root, (Expr *)outer_var, NULL, sub_eclass->ec_opfamilies, sub_expr_type, sub_expr_coll, 0, rel->relids, false);

             
                                                                    
                                            
             
          if (!outer_ec)
          {
            continue;
          }

          outer_pk = make_canonical_pathkey(root, outer_ec, sub_pathkey->pk_opfamily, sub_pathkey->pk_strategy, sub_pathkey->pk_nulls_first);
                                              
          score = list_length(outer_ec->ec_members) - 1;
                                                               
          if (retvallen < outer_query_keys && list_nth(root->query_pathkeys, retvallen) == outer_pk)
          {
            score++;
          }
          if (score > best_score)
          {
            best_pathkey = outer_pk;
            best_score = score;
          }
        }
      }
    }

       
                                                                       
                                                          
       
    if (!best_pathkey)
    {
      break;
    }

       
                                                                      
                                     
       
    if (!pathkey_is_redundant(best_pathkey, retval))
    {
      retval = lappend(retval, best_pathkey);
      retvallen++;
    }
  }

  return retval;
}

   
                             
   
                                                                            
                                                     
   
                                                                          
                                                              
   
static Var *
find_var_for_subquery_tle(RelOptInfo *rel, TargetEntry *tle)
{
  ListCell *lc;

                                                                            
  if (tle->resjunk)
  {
    return NULL;
  }

                                                              
  foreach (lc, rel->reltarget->exprs)
  {
    Var *var = (Var *)lfirst(lc);

                             
    if (!IsA(var, Var))
    {
      continue;
    }
    Assert(var->varno == rel->relid);

                                                           
    if (var->varattno == tle->resno)
    {
      return copyObject(var);                             
    }
  }
  return NULL;
}

   
                       
                                                                         
                                                                         
   
                                                                       
                                                                          
                                                                 
   
                                                                            
   
                                                                  
                                                        
                                                                      
   
                                      
   
List *
build_join_pathkeys(PlannerInfo *root, RelOptInfo *joinrel, JoinType jointype, List *outer_pathkeys)
{
  if (jointype == JOIN_FULL || jointype == JOIN_RIGHT)
  {
    return NIL;
  }

     
                                                                           
                                                                             
           
     
                                                                      
                                                                        
                                     
     
  return truncate_useless_pathkeys(root, joinrel, outer_pathkeys);
}

                                                                              
                              
                                                                              

   
                                 
                                                                      
                                  
   
                                                                          
                                                                        
   
                                                                               
                                                                             
                                                                             
                                                                            
                                                                
                                                          
   
                                                    
                                                                     
   
List *
make_pathkeys_for_sortclauses(PlannerInfo *root, List *sortclauses, List *tlist)
{
  List *pathkeys = NIL;
  ListCell *l;

  foreach (l, sortclauses)
  {
    SortGroupClause *sortcl = (SortGroupClause *)lfirst(l);
    Expr *sortkey;
    PathKey *pathkey;

    sortkey = (Expr *)get_sortgroupclause_expr(sortcl, tlist);
    Assert(OidIsValid(sortcl->sortop));
    pathkey = make_pathkey_from_sortop(root, sortkey, root->nullable_baserels, sortcl->sortop, sortcl->nulls_first, sortcl->tleSortGroupRef, true);

                                                           
    if (!pathkey_is_redundant(pathkey, pathkeys))
    {
      pathkeys = lappend(pathkeys, pathkey);
    }
  }
  return pathkeys;
}

                                                                              
                              
                                                                              

   
                                   
                                                                  
   
                                                                  
                                                                        
                                                                      
                                                                        
                                                                              
                                                                            
                                                                        
                                                                           
                
   
                                                                         
                                                                          
                                                                       
                                                   
   
void
initialize_mergeclause_eclasses(PlannerInfo *root, RestrictInfo *restrictinfo)
{
  Expr *clause = restrictinfo->clause;
  Oid lefttype, righttype;

                                   
  Assert(restrictinfo->mergeopfamilies != NIL);
                                  
  Assert(restrictinfo->left_ec == NULL);
  Assert(restrictinfo->right_ec == NULL);

                                                     
  op_input_types(((OpExpr *)clause)->opno, &lefttype, &righttype);

                                                                
  restrictinfo->left_ec = get_eclass_for_sort_expr(root, (Expr *)get_leftop(clause), restrictinfo->nullable_relids, restrictinfo->mergeopfamilies, lefttype, ((OpExpr *)clause)->inputcollid, 0, NULL, true);
  restrictinfo->right_ec = get_eclass_for_sort_expr(root, (Expr *)get_rightop(clause), restrictinfo->nullable_relids, restrictinfo->mergeopfamilies, righttype, ((OpExpr *)clause)->inputcollid, 0, NULL, true);
}

   
                               
                                                                  
                  
   
                                                                 
                                                                    
                                                                       
                        
   
void
update_mergeclause_eclasses(PlannerInfo *root, RestrictInfo *restrictinfo)
{
                                    
  Assert(restrictinfo->mergeopfamilies != NIL);
                                     
  Assert(restrictinfo->left_ec != NULL);
  Assert(restrictinfo->right_ec != NULL);

                                     
  while (restrictinfo->left_ec->ec_merged)
  {
    restrictinfo->left_ec = restrictinfo->left_ec->ec_merged;
  }
  while (restrictinfo->right_ec->ec_merged)
  {
    restrictinfo->right_ec = restrictinfo->right_ec->ec_merged;
  }
}

   
                                        
                                                                      
                                                                   
                                                       
   
                                                                            
                                                                          
                                                         
   
                                                                           
                                                                   
                               
   
                                                                     
                                                                            
                                                                         
   
List *
find_mergeclauses_for_outer_pathkeys(PlannerInfo *root, List *pathkeys, List *restrictinfos)
{
  List *mergeclauses = NIL;
  ListCell *i;

                                                        
  foreach (i, restrictinfos)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(i);

    update_mergeclause_eclasses(root, rinfo);
  }

  foreach (i, pathkeys)
  {
    PathKey *pathkey = (PathKey *)lfirst(i);
    EquivalenceClass *pathkey_ec = pathkey->pk_eclass;
    List *matched_restrictinfos = NIL;
    ListCell *j;

                 
                                                                   
                                                                        
                                                              
                                                                    
                                                                      
                                        
       
                                   
                                                        
       
                                                                          
                                                                         
                                                                  
       
                                                                        
                                                                      
                                                                      
       
                                                                         
                                                                           
                                                                        
                                                                        
                                                          
       
                                                                      
                                                                      
                                                                    
                                                                      
                                                                      
                                                                         
                                                                          
                                                                      
                                                                   
                                
                 
       
    foreach (j, restrictinfos)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(j);
      EquivalenceClass *clause_ec;

      clause_ec = rinfo->outer_is_left ? rinfo->left_ec : rinfo->right_ec;
      if (clause_ec == pathkey_ec)
      {
        matched_restrictinfos = lappend(matched_restrictinfos, rinfo);
      }
    }

       
                                                                      
                                                                          
                                                        
       
    if (matched_restrictinfos == NIL)
    {
      break;
    }

       
                                                                        
                                
       
    mergeclauses = list_concat(mergeclauses, matched_restrictinfos);
  }

  return mergeclauses;
}

   
                                   
                                                                 
                                                   
   
                                                                   
                                        
                                                              
   
                                                                           
                                                                   
                               
   
                                                                      
   
                                                                            
                                                                           
                                                                               
                                                                           
                                                                             
                                                                        
                                                                         
                                        
   
List *
select_outer_pathkeys_for_merge(PlannerInfo *root, List *mergeclauses, RelOptInfo *joinrel)
{
  List *pathkeys = NIL;
  int nClauses = list_length(mergeclauses);
  EquivalenceClass **ecs;
  int *scores;
  int necs;
  ListCell *lc;
  int j;

                                  
  if (nClauses == 0)
  {
    return NIL;
  }

     
                                                                   
                                                
     
  ecs = (EquivalenceClass **)palloc(nClauses * sizeof(EquivalenceClass *));
  scores = (int *)palloc(nClauses * sizeof(int));
  necs = 0;

  foreach (lc, mergeclauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);
    EquivalenceClass *oeclass;
    int score;
    ListCell *lc2;

                              
    update_mergeclause_eclasses(root, rinfo);

    if (rinfo->outer_is_left)
    {
      oeclass = rinfo->left_ec;
    }
    else
    {
      oeclass = rinfo->right_ec;
    }

                           
    for (j = 0; j < necs; j++)
    {
      if (ecs[j] == oeclass)
      {
        break;
      }
    }
    if (j < necs)
    {
      continue;
    }

                       
    score = 0;
    foreach (lc2, oeclass->ec_members)
    {
      EquivalenceMember *em = (EquivalenceMember *)lfirst(lc2);

                                          
      if (!em->em_is_const && !em->em_is_child && !bms_overlap(em->em_relids, joinrel->relids))
      {
        score++;
      }
    }

    ecs[necs] = oeclass;
    scores[necs] = score;
    necs++;
  }

     
                                                                           
                                                                             
                                                                           
     
  if (root->query_pathkeys)
  {
    foreach (lc, root->query_pathkeys)
    {
      PathKey *query_pathkey = (PathKey *)lfirst(lc);
      EquivalenceClass *query_ec = query_pathkey->pk_eclass;

      for (j = 0; j < necs; j++)
      {
        if (ecs[j] == query_ec)
        {
          break;                  
        }
      }
      if (j >= necs)
      {
        break;                        
      }
    }
                                                            
    if (lc == NULL)
    {
                                                                
      pathkeys = list_copy(root->query_pathkeys);
                                             
      foreach (lc, root->query_pathkeys)
      {
        PathKey *query_pathkey = (PathKey *)lfirst(lc);
        EquivalenceClass *query_ec = query_pathkey->pk_eclass;

        for (j = 0; j < necs; j++)
        {
          if (ecs[j] == query_ec)
          {
            scores[j] = -1;
            break;
          }
        }
      }
    }
  }

     
                                                                             
                                                                           
                                  
     
  for (;;)
  {
    int best_j;
    int best_score;
    EquivalenceClass *ec;
    PathKey *pathkey;

    best_j = 0;
    best_score = scores[0];
    for (j = 1; j < necs; j++)
    {
      if (scores[j] > best_score)
      {
        best_j = j;
        best_score = scores[j];
      }
    }
    if (best_score < 0)
    {
      break;               
    }
    ec = ecs[best_j];
    scores[best_j] = -1;
    pathkey = make_canonical_pathkey(root, ec, linitial_oid(ec->ec_opfamilies), BTLessStrategyNumber, false);
                                                     
    Assert(!pathkey_is_redundant(pathkey, pathkeys));
    pathkeys = lappend(pathkeys, pathkey);
  }

  pfree(ecs);
  pfree(scores);

  return pathkeys;
}

   
                                 
                                                                     
                                                                 
                         
   
                                                                       
                                                  
                                                                           
                       
   
                                                                           
                                                                   
                               
   
                                                                      
   
                                                                       
                                                                             
                           
   
List *
make_inner_pathkeys_for_merge(PlannerInfo *root, List *mergeclauses, List *outer_pathkeys)
{
  List *pathkeys = NIL;
  EquivalenceClass *lastoeclass;
  PathKey *opathkey;
  ListCell *lc;
  ListCell *lop;

  lastoeclass = NULL;
  opathkey = NULL;
  lop = list_head(outer_pathkeys);

  foreach (lc, mergeclauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);
    EquivalenceClass *oeclass;
    EquivalenceClass *ieclass;
    PathKey *pathkey;

    update_mergeclause_eclasses(root, rinfo);

    if (rinfo->outer_is_left)
    {
      oeclass = rinfo->left_ec;
      ieclass = rinfo->right_ec;
    }
    else
    {
      oeclass = rinfo->right_ec;
      ieclass = rinfo->left_ec;
    }

                                                            
                                                       
    if (oeclass != lastoeclass)
    {
      if (!lop)
      {
        elog(ERROR, "too few pathkeys for mergeclauses");
      }
      opathkey = (PathKey *)lfirst(lop);
      lop = lnext(lop);
      lastoeclass = opathkey->pk_eclass;
      if (oeclass != lastoeclass)
      {
        elog(ERROR, "outer pathkeys do not match mergeclause");
      }
    }

       
                                                                        
                                                                       
                       
       
    if (ieclass == oeclass)
    {
      pathkey = opathkey;
    }
    else
    {
      pathkey = make_canonical_pathkey(root, ieclass, opathkey->pk_opfamily, opathkey->pk_strategy, opathkey->pk_nulls_first);
    }

       
                                                                       
                                                                           
                                                                           
                                                                        
                                                                           
                                                                        
                           
       
    if (!pathkey_is_redundant(pathkey, pathkeys))
    {
      pathkeys = lappend(pathkeys, pathkey);
    }
  }

  return pathkeys;
}

   
                                        
                                                                          
                                                                   
   
                                                                           
                                                                   
                                                                 
                                                                            
                                                              
                                                           
   
                                                                   
   
                                                                           
                                                                             
                                                                           
                                                                               
                                                                           
                                    
   
                                                                          
                                                                   
                               
   
List *
trim_mergeclauses_for_inner_pathkeys(PlannerInfo *root, List *mergeclauses, List *pathkeys)
{
  List *new_mergeclauses = NIL;
  PathKey *pathkey;
  EquivalenceClass *pathkey_ec;
  bool matched_pathkey;
  ListCell *lip;
  ListCell *i;

                                                                         
  if (pathkeys == NIL)
  {
    return NIL;
  }
                                            
  lip = list_head(pathkeys);
  pathkey = (PathKey *)lfirst(lip);
  pathkey_ec = pathkey->pk_eclass;
  lip = lnext(lip);
  matched_pathkey = false;

                                                    
  foreach (i, mergeclauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(i);
    EquivalenceClass *clause_ec;

                                                                     

                                                             
    clause_ec = rinfo->outer_is_left ? rinfo->right_ec : rinfo->left_ec;

                                                                      
    if (clause_ec != pathkey_ec)
    {
                                                                       
      if (!matched_pathkey)
      {
        break;
      }

                                                 
      if (lip == NULL)
      {
        break;
      }
      pathkey = (PathKey *)lfirst(lip);
      pathkey_ec = pathkey->pk_eclass;
      lip = lnext(lip);
      matched_pathkey = false;
    }

                                                                     
    if (clause_ec == pathkey_ec)
    {
      new_mergeclauses = lappend(new_mergeclauses, rinfo);
      matched_pathkey = true;
    }
    else
    {
                                                         
      break;
    }
  }

  return new_mergeclauses;
}

                                                                              
                              
   
                                                                           
                                                                              
                                                                           
                                                                            
                                                          
                                                                              

   
                               
                                                                   
                              
   
                                                                           
                                                                         
                                                                            
                                                                            
                                       
   
                                                                             
                                                                           
                                                                            
                                                                         
                                                                               
                                                      
   
static int
pathkeys_useful_for_merging(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{
  int useful = 0;
  ListCell *i;

  foreach (i, pathkeys)
  {
    PathKey *pathkey = (PathKey *)lfirst(i);
    bool matched = false;
    ListCell *j;

                                                      
    if (!right_merge_direction(root, pathkey))
    {
      break;
    }

       
                                                                      
                                                                     
                                                                  
       
    if (rel->has_eclass_joins && eclass_useful_for_merging(root, pathkey->pk_eclass, rel))
    {
      matched = true;
    }
    else
    {
         
                                                                  
                                                                
                                       
         
      foreach (j, rel->joininfo)
      {
        RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(j);

        if (restrictinfo->mergeopfamilies == NIL)
        {
          continue;
        }
        update_mergeclause_eclasses(root, restrictinfo);

        if (pathkey->pk_eclass == restrictinfo->left_ec || pathkey->pk_eclass == restrictinfo->right_ec)
        {
          matched = true;
          break;
        }
      }
    }

       
                                                                      
                                                                          
                                                        
       
    if (matched)
    {
      useful++;
    }
    else
    {
      break;
    }
  }

  return useful;
}

   
                         
                                                                    
                                   
   
static bool
right_merge_direction(PlannerInfo *root, PathKey *pathkey)
{
  ListCell *l;

  foreach (l, root->query_pathkeys)
  {
    PathKey *query_pathkey = (PathKey *)lfirst(l);

    if (pathkey->pk_eclass == query_pathkey->pk_eclass && pathkey->pk_opfamily == query_pathkey->pk_opfamily)
    {
         
                                                                    
                                                                        
                                                                         
                                                                        
                                     
         
      return (pathkey->pk_strategy == query_pathkey->pk_strategy);
    }
  }

                                                                 
  return (pathkey->pk_strategy == BTLessStrategyNumber);
}

   
                                
                                                                 
                                       
   
                                                                       
                                                                        
                                                                          
   
static int
pathkeys_useful_for_ordering(PlannerInfo *root, List *pathkeys)
{
  if (root->query_pathkeys == NIL)
  {
    return 0;                                    
  }

  if (pathkeys == NIL)
  {
    return 0;                     
  }

  if (pathkeys_contained_in(root->query_pathkeys, pathkeys))
  {
                                                          
    return list_length(root->query_pathkeys);
  }

  return 0;                               
}

   
                             
                                                                
   
List *
truncate_useless_pathkeys(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{
  int nuseful;
  int nuseful2;

  nuseful = pathkeys_useful_for_merging(root, rel, pathkeys);
  nuseful2 = pathkeys_useful_for_ordering(root, pathkeys);
  if (nuseful2 > nuseful)
  {
    nuseful = nuseful2;
  }

     
                                                                         
                                                               
     
  if (nuseful == 0)
  {
    return NIL;
  }
  else if (nuseful == list_length(pathkeys))
  {
    return pathkeys;
  }
  else
  {
    return list_truncate(list_copy(pathkeys), nuseful);
  }
}

   
                       
                                                                      
                                                     
   
                                                                           
                                                                             
                                                                              
                                                            
   
                                                                              
                                                                          
                                                                            
                                                                             
   
bool
has_useful_pathkeys(PlannerInfo *root, RelOptInfo *rel)
{
  if (rel->joininfo != NIL || rel->has_eclass_joins)
  {
    return true;                                                
  }
  if (root->query_pathkeys != NIL)
  {
    return true;                                             
  }
  return false;                         
}
