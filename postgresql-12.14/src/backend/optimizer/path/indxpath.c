                                                                            
   
              
                                                                   
                                                   
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/stratnum.h"
#include "access/sysattr.h"
#include "catalog/pg_am.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)
#undef make_simple_restrictinfo
#define make_simple_restrictinfo(root, clause) make_restrictinfo_new(root, clause, true, false, false, 0, NULL, NULL, NULL)

                                     
#define IndexCollMatchesExprColl(idxcollation, exprcollation) ((idxcollation) == InvalidOid || (idxcollation) == (exprcollation))

                                                                        
typedef enum
{
  ST_INDEXSCAN,                               
  ST_BITMAPSCAN,                               
  ST_ANYSCAN                         
} ScanTypeControl;

                                                                    
typedef struct
{
  bool nonempty;                                      
                                                             
  List *indexclauses[INDEX_MAX_KEYS];
} IndexClauseSet;

                                                   
typedef struct
{
  Path *path;                                                          
  List *quals;                                         
  List *preds;                                                   
  Bitmapset *clauseids;                                             
  bool unclassifiable;                                            
} PathClauseUsage;

                                                      
typedef struct
{
  IndexOptInfo *index;                              
  int indexcol;                                              
} ec_member_matches_arg;

static void
consider_index_join_clauses(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths);
static void
consider_index_join_outer_rels(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, List *indexjoinclauses, int considered_clauses, List **considered_relids);
static void
get_join_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, Relids relids, List **considered_relids);
static bool
eclass_already_used(EquivalenceClass *parent_ec, Relids oldrelids, List *indexjoinclauses);
static bool
bms_equal_any(Relids relids, List *relids_list);
static void
get_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, List **bitindexpaths);
static List *
build_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, bool useful_predicate, ScanTypeControl scantype, bool *skip_nonnative_saop, bool *skip_lower_saop);
static List *
build_paths_for_OR(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses);
static List *
generate_bitmap_or_paths(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses);
static Path *
choose_bitmap_and(PlannerInfo *root, RelOptInfo *rel, List *paths);
static int
path_usage_comparator(const void *a, const void *b);
static Cost
bitmap_scan_cost_est(PlannerInfo *root, RelOptInfo *rel, Path *ipath);
static Cost
bitmap_and_cost_est(PlannerInfo *root, RelOptInfo *rel, List *paths);
static PathClauseUsage *
classify_index_clause_usage(Path *path, List **clauselist);
static void
find_indexpath_quals(Path *bitmapqual, List **quals, List **preds);
static int
find_list_position(Node *node, List **nodelist);
static bool
check_index_only(RelOptInfo *rel, IndexOptInfo *index);
static double
get_loop_count(PlannerInfo *root, Index cur_relid, Relids outer_relids);
static double
adjust_rowcount_for_semijoins(PlannerInfo *root, Index cur_relid, Index outer_relid, double rowcount);
static double
approximate_joinrel_size(PlannerInfo *root, Relids relids);
static void
match_restriction_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset);
static void
match_join_clauses_to_index(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauseset, List **joinorclauses);
static void
match_eclass_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset);
static void
match_clauses_to_index(PlannerInfo *root, List *clauses, IndexOptInfo *index, IndexClauseSet *clauseset);
static void
match_clause_to_index(PlannerInfo *root, RestrictInfo *rinfo, IndexOptInfo *index, IndexClauseSet *clauseset);
static IndexClause *
match_clause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_boolean_index_clause(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_opclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_funcclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
get_index_clause_from_support(PlannerInfo *root, RestrictInfo *rinfo, Oid funcid, int indexarg, int indexcol, IndexOptInfo *index);
static IndexClause *
match_saopclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
match_rowcompare_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index);
static IndexClause *
expand_indexqual_rowcompare(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index, Oid expr_op, bool var_on_left);
static void
match_pathkeys_to_index(IndexOptInfo *index, List *pathkeys, List **orderby_clauses_p, List **clause_columns_p);
static Expr *
match_clause_to_ordering_op(IndexOptInfo *index, int indexcol, Expr *clause, Oid pk_opfamily);
static bool
ec_member_matches_indexcol(PlannerInfo *root, RelOptInfo *rel, EquivalenceClass *ec, EquivalenceMember *em, void *arg);

   
                        
                                                                  
                                                                       
   
                                                                       
                                                                        
                                                                     
                                       
   
                                                                        
                                                                     
                                                                           
                                                                           
                                                                        
                                                                          
                                                                              
                                                                             
                                                                            
   
                                                                           
                                                                           
                      
   
                                                                   
   
                                                                              
   
                                                                             
                                                                         
                                                                              
                                                                          
                                                                          
                                                                      
                                                                              
                                                                        
   
void
create_index_paths(PlannerInfo *root, RelOptInfo *rel)
{
  List *indexpaths;
  List *bitindexpaths;
  List *bitjoinpaths;
  List *joinorclauses;
  IndexClauseSet rclauseset;
  IndexClauseSet jclauseset;
  IndexClauseSet eclauseset;
  ListCell *lc;

                                         
  if (rel->indexlist == NIL)
  {
    return;
  }

                                                                 
  bitindexpaths = bitjoinpaths = joinorclauses = NIL;

                                  
  foreach (lc, rel->indexlist)
  {
    IndexOptInfo *index = (IndexOptInfo *)lfirst(lc);

                                                       
    Assert(index->nkeycolumns <= INDEX_MAX_KEYS);

       
                                                           
                                                                      
                                             
       
    if (index->indpred != NIL && !index->predOK)
    {
      continue;
    }

       
                                                                  
       
    MemSet(&rclauseset, 0, sizeof(rclauseset));
    match_restriction_clauses_to_index(root, index, &rclauseset);

       
                                                                      
                                                                        
                                                                    
       
    get_index_paths(root, rel, index, &rclauseset, &bitindexpaths);

       
                                                                           
                                                                           
                                                                           
                                                                     
       
    MemSet(&jclauseset, 0, sizeof(jclauseset));
    match_join_clauses_to_index(root, rel, index, &jclauseset, &joinorclauses);

       
                                                                          
                  
       
    MemSet(&eclauseset, 0, sizeof(eclauseset));
    match_eclass_clauses_to_index(root, index, &eclauseset);

       
                                                                         
                               
       
    if (jclauseset.nonempty || eclauseset.nonempty)
    {
      consider_index_join_clauses(root, rel, index, &rclauseset, &jclauseset, &eclauseset, &bitjoinpaths);
    }
  }

     
                                                                       
                                                    
     
  indexpaths = generate_bitmap_or_paths(root, rel, rel->baserestrictinfo, NIL);
  bitindexpaths = list_concat(bitindexpaths, indexpaths);

     
                                                                             
                                                      
     
  indexpaths = generate_bitmap_or_paths(root, rel, joinorclauses, rel->baserestrictinfo);
  bitjoinpaths = list_concat(bitjoinpaths, indexpaths);

     
                                                                         
                                                                          
                                                                        
                                                                           
                                   
     
  if (bitindexpaths != NIL)
  {
    Path *bitmapqual;
    BitmapHeapPath *bpath;

    bitmapqual = choose_bitmap_and(root, rel, bitindexpaths);
    bpath = create_bitmap_heap_path(root, rel, bitmapqual, rel->lateral_relids, 1.0, 0);
    add_path(rel, (Path *)bpath);

                                           
    if (rel->consider_parallel && rel->lateral_relids == NULL)
    {
      create_partial_bitmap_paths(root, rel, bitmapqual);
    }
  }

     
                                                                             
                                                                           
                                                                          
                                                                   
                                                              
                                                                 
                                                                         
                          
     
  if (bitjoinpaths != NIL)
  {
    List *all_path_outers;
    ListCell *lc;

                                                                      
    all_path_outers = NIL;
    foreach (lc, bitjoinpaths)
    {
      Path *path = (Path *)lfirst(lc);
      Relids required_outer = PATH_REQ_OUTER(path);

      if (!bms_equal_any(required_outer, all_path_outers))
      {
        all_path_outers = lappend(all_path_outers, required_outer);
      }
    }

                                                         
    foreach (lc, all_path_outers)
    {
      Relids max_outers = (Relids)lfirst(lc);
      List *this_path_set;
      Path *bitmapqual;
      Relids required_outer;
      double loop_count;
      BitmapHeapPath *bpath;
      ListCell *lcp;

                                                                        
      this_path_set = NIL;
      foreach (lcp, bitjoinpaths)
      {
        Path *path = (Path *)lfirst(lcp);

        if (bms_is_subset(PATH_REQ_OUTER(path), max_outers))
        {
          this_path_set = lappend(this_path_set, path);
        }
      }

         
                                                                 
                                       
         
      this_path_set = list_concat(this_path_set, bitindexpaths);

                                                                 
      bitmapqual = choose_bitmap_and(root, rel, this_path_set);

                                           
      required_outer = PATH_REQ_OUTER(bitmapqual);
      loop_count = get_loop_count(root, rel->relid, required_outer);
      bpath = create_bitmap_heap_path(root, rel, bitmapqual, required_outer, loop_count, 0);
      add_path(rel, (Path *)bpath);
    }
  }
}

   
                               
                                                                         
                           
   
                                                                   
                                                                       
   
                                      
                                                            
                                                                   
                                                                   
                                                                               
                                                       
   
static void
consider_index_join_clauses(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths)
{
  int considered_clauses = 0;
  List *considered_relids = NIL;
  int indexcol;

     
                                                                            
                                                                       
                                                                             
                                                                         
                                                                           
                                                                         
                                                                            
                                                
     
                                                                           
                                                                           
                                                                             
                                                                           
                                                                 
     
                                                                            
                                                                           
                                                                            
                                                
     
  for (indexcol = 0; indexcol < index->nkeycolumns; indexcol++)
  {
                                                     
    considered_clauses += list_length(jclauseset->indexclauses[indexcol]);
    consider_index_join_outer_rels(root, rel, index, rclauseset, jclauseset, eclauseset, bitindexpaths, jclauseset->indexclauses[indexcol], considered_clauses, &considered_relids);
                                                     
    considered_clauses += list_length(eclauseset->indexclauses[indexcol]);
    consider_index_join_outer_rels(root, rel, index, rclauseset, jclauseset, eclauseset, bitindexpaths, eclauseset->indexclauses[indexcol], considered_clauses, &considered_relids);
  }
}

   
                                  
                                                                             
   
                                                                               
   
                                                                 
                             
                                                                 
                                                                           
                                                                        
   
static void
consider_index_join_outer_rels(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, List *indexjoinclauses, int considered_clauses, List **considered_relids)
{
  ListCell *lc;

                                                           
  foreach (lc, indexjoinclauses)
  {
    IndexClause *iclause = (IndexClause *)lfirst(lc);
    Relids clause_relids = iclause->rinfo->clause_relids;
    EquivalenceClass *parent_ec = iclause->rinfo->parent_ec;
    ListCell *lc2;

                                                                    
    if (bms_equal_any(clause_relids, *considered_relids))
    {
      continue;
    }

       
                                                                
                                                                         
                                                                        
                                                                        
                                                                           
       
                                                                          
                                                                        
                                                                        
                                                                         
                    
       
    foreach (lc2, *considered_relids)
    {
      Relids oldrelids = (Relids)lfirst(lc2);

         
                                                                     
                                                                      
                                                                      
                                                
         
      if (bms_subset_compare(clause_relids, oldrelids) != BMS_DIFFERENT)
      {
        continue;
      }

         
                                                                   
                                                                     
                                                                         
                                                             
                                                                       
                                                                       
         
      if (parent_ec && eclass_already_used(parent_ec, oldrelids, indexjoinclauses))
      {
        continue;
      }

         
                                                                      
                                                                       
                                                                      
         
      if (list_length(*considered_relids) >= 10 * considered_clauses)
      {
        break;
      }

                                 
      get_join_index_paths(root, rel, index, rclauseset, jclauseset, eclauseset, bitindexpaths, bms_union(clause_relids, oldrelids), considered_relids);
    }

                                               
    get_join_index_paths(root, rel, index, rclauseset, jclauseset, eclauseset, bitindexpaths, clause_relids, considered_relids);
  }
}

   
                        
                                                                            
                                                                            
                             
   
                                                                               
   
                                                             
                                                  
                                                                          
                            
   
static void
get_join_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *rclauseset, IndexClauseSet *jclauseset, IndexClauseSet *eclauseset, List **bitindexpaths, Relids relids, List **considered_relids)
{
  IndexClauseSet clauseset;
  int indexcol;

                                                                       
  if (bms_equal_any(relids, *considered_relids))
  {
    return;
  }

                                                         
  MemSet(&clauseset, 0, sizeof(clauseset));

  for (indexcol = 0; indexcol < index->nkeycolumns; indexcol++)
  {
    ListCell *lc;

                                                   
    foreach (lc, jclauseset->indexclauses[indexcol])
    {
      IndexClause *iclause = (IndexClause *)lfirst(lc);

      if (bms_is_subset(iclause->rinfo->clause_relids, relids))
      {
        clauseset.indexclauses[indexcol] = lappend(clauseset.indexclauses[indexcol], iclause);
      }
    }

       
                                                                           
                                                                         
                                                                          
                                                  
       
    foreach (lc, eclauseset->indexclauses[indexcol])
    {
      IndexClause *iclause = (IndexClause *)lfirst(lc);

      if (bms_is_subset(iclause->rinfo->clause_relids, relids))
      {
        clauseset.indexclauses[indexcol] = lappend(clauseset.indexclauses[indexcol], iclause);
        break;
      }
    }

                                                                        
    clauseset.indexclauses[indexcol] = list_concat(clauseset.indexclauses[indexcol], rclauseset->indexclauses[indexcol]);

    if (clauseset.indexclauses[indexcol] != NIL)
    {
      clauseset.nonempty = true;
    }
  }

                                                                       
  Assert(clauseset.nonempty);

                                                              
  get_index_paths(root, rel, index, &clauseset, bitindexpaths);

     
                                                                            
                                                                            
     
  *considered_relids = lcons(relids, *considered_relids);
}

   
                       
                                                                     
                                     
   
static bool
eclass_already_used(EquivalenceClass *parent_ec, Relids oldrelids, List *indexjoinclauses)
{
  ListCell *lc;

  foreach (lc, indexjoinclauses)
  {
    IndexClause *iclause = (IndexClause *)lfirst(lc);
    RestrictInfo *rinfo = iclause->rinfo;

    if (rinfo->parent_ec == parent_ec && bms_is_subset(rinfo->clause_relids, oldrelids))
    {
      return true;
    }
  }
  return false;
}

   
                 
                                                             
   
                                                  
   
static bool
bms_equal_any(Relids relids, List *relids_list)
{
  ListCell *lc;

  foreach (lc, relids_list)
  {
    if (bms_equal(relids, (Relids)lfirst(lc)))
    {
      return true;
    }
  }
  return false;
}

   
                   
                                                                             
   
                                                                   
                                                                       
   
                                                                            
                                                                           
                                                                          
                                                                            
                                                                            
                                                                           
                                        
   
static void
get_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, List **bitindexpaths)
{
  List *indexpaths;
  bool skip_nonnative_saop = false;
  bool skip_lower_saop = false;
  ListCell *lc;

     
                                                                          
                                                                            
                                                                           
                         
     
  indexpaths = build_index_paths(root, rel, index, clauses, index->predOK, ST_ANYSCAN, &skip_nonnative_saop, &skip_lower_saop);

     
                                                                             
                                                                            
                                                          
     
  if (skip_lower_saop)
  {
    indexpaths = list_concat(indexpaths, build_index_paths(root, rel, index, clauses, index->predOK, ST_ANYSCAN, &skip_nonnative_saop, NULL));
  }

     
                                                                             
                                                                  
                                                                        
                                                                            
                                                    
     
                                                                            
                                                                           
                                                                            
                                                               
     
  foreach (lc, indexpaths)
  {
    IndexPath *ipath = (IndexPath *)lfirst(lc);

    if (index->amhasgettuple)
    {
      add_path(rel, (Path *)ipath);
    }

    if (index->amhasgetbitmap && (ipath->path.pathkeys == NIL || ipath->indexselectivity < 1.0))
    {
      *bitindexpaths = lappend(*bitindexpaths, ipath);
    }
  }

     
                                                                         
                                                                      
                        
     
  if (skip_nonnative_saop)
  {
    indexpaths = build_index_paths(root, rel, index, clauses, false, ST_BITMAPSCAN, NULL, NULL);
    *bitindexpaths = list_concat(*bitindexpaths, indexpaths);
  }
}

   
                     
                                                                      
                                                                             
   
                                                                        
                                                                       
                                                                       
                                                                      
                                                        
   
                                                                            
                                                                        
                                                                       
                                                                              
                                                                            
                         
   
                                                                         
                                                                       
                                                                
   
                                                                           
                                                                               
                                                                             
                                                                         
   
                                                                           
                                                                            
                                                                             
                                                                             
                                                            
   
                                      
                                                            
                                                                        
                                                                         
                                                                     
                                                                              
                                                                       
   
static List *
build_index_paths(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauses, bool useful_predicate, ScanTypeControl scantype, bool *skip_nonnative_saop, bool *skip_lower_saop)
{
  List *result = NIL;
  IndexPath *ipath;
  List *index_clauses;
  Relids outer_relids;
  double loop_count;
  List *orderbyclauses;
  List *orderbyclausecols;
  List *index_pathkeys;
  List *useful_pathkeys;
  bool found_lower_saop_clause;
  bool pathkeys_possibly_useful;
  bool index_is_ordered;
  bool index_only_scan;
  int indexcol;

     
                                                        
     
  switch (scantype)
  {
  case ST_INDEXSCAN:
    if (!index->amhasgettuple)
    {
      return NIL;
    }
    break;
  case ST_BITMAPSCAN:
    if (!index->amhasgetbitmap)
    {
      return NIL;
    }
    break;
  case ST_ANYSCAN:
                               
    break;
  }

     
                                                                       
     
                                                                          
                                                                            
                                                                            
                           
     
                                                                          
                                                                       
                                                                         
                                                                     
                                                                             
                                
     
                                                                             
                                                                         
                              
     
  index_clauses = NIL;
  found_lower_saop_clause = false;
  outer_relids = bms_copy(rel->lateral_relids);
  for (indexcol = 0; indexcol < index->nkeycolumns; indexcol++)
  {
    ListCell *lc;

    foreach (lc, clauses->indexclauses[indexcol])
    {
      IndexClause *iclause = (IndexClause *)lfirst(lc);
      RestrictInfo *rinfo = iclause->rinfo;

                                                           
      if (IsA(rinfo->clause, ScalarArrayOpExpr))
      {
        if (!index->amsearcharray)
        {
          if (skip_nonnative_saop)
          {
                                                       
            *skip_nonnative_saop = true;
            continue;
          }
                                                                  
          Assert(scantype == ST_BITMAPSCAN);
        }
        if (indexcol > 0)
        {
          if (skip_lower_saop)
          {
                                                            
            *skip_lower_saop = true;
            continue;
          }
          found_lower_saop_clause = true;
        }
      }

                                     
      index_clauses = lappend(index_clauses, iclause);
      outer_relids = bms_add_members(outer_relids, rinfo->clause_relids);
    }

       
                                                                           
                                                                 
                                                                       
                                                                           
                                                                  
                 
       
    if (index_clauses == NIL && !index->amoptionalkey)
    {
      return NIL;
    }
  }

                                                                    
  outer_relids = bms_del_member(outer_relids, rel->relid);
                                                                     
  if (bms_is_empty(outer_relids))
  {
    outer_relids = NULL;
  }

                                                       
  loop_count = get_loop_count(root, rel->relid, outer_relids);

     
                                                                           
                                                                            
                                                                         
                                   
     
  pathkeys_possibly_useful = (scantype != ST_BITMAPSCAN && !found_lower_saop_clause && has_useful_pathkeys(root, rel));
  index_is_ordered = (index->sortopfamily != NULL);
  if (index_is_ordered && pathkeys_possibly_useful)
  {
    index_pathkeys = build_index_pathkeys(root, index, ForwardScanDirection);
    useful_pathkeys = truncate_useless_pathkeys(root, rel, index_pathkeys);
    orderbyclauses = NIL;
    orderbyclausecols = NIL;
  }
  else if (index->amcanorderbyop && pathkeys_possibly_useful)
  {
                                                                      
    match_pathkeys_to_index(index, root->query_pathkeys, &orderbyclauses, &orderbyclausecols);
    if (orderbyclauses)
    {
      useful_pathkeys = root->query_pathkeys;
    }
    else
    {
      useful_pathkeys = NIL;
    }
  }
  else
  {
    useful_pathkeys = NIL;
    orderbyclauses = NIL;
    orderbyclausecols = NIL;
  }

     
                                                                        
                                                                            
                                  
     
  index_only_scan = (scantype != ST_BITMAPSCAN && check_index_only(rel, index));

     
                                                                             
                                                                             
                                                                       
                                                   
     
  if (index_clauses != NIL || useful_pathkeys != NIL || useful_predicate || index_only_scan)
  {
    ipath = create_index_path(root, index, index_clauses, orderbyclauses, orderbyclausecols, useful_pathkeys, index_is_ordered ? ForwardScanDirection : NoMovementScanDirection, index_only_scan, outer_relids, loop_count, false);
    result = lappend(result, ipath);

       
                                                                     
                                                   
       
    if (index->amcanparallel && rel->consider_parallel && outer_relids == NULL && scantype != ST_BITMAPSCAN)
    {
      ipath = create_index_path(root, index, index_clauses, orderbyclauses, orderbyclausecols, useful_pathkeys, index_is_ordered ? ForwardScanDirection : NoMovementScanDirection, index_only_scan, outer_relids, loop_count, true);

         
                                                                       
                                         
         
      if (ipath->path.parallel_workers > 0)
      {
        add_partial_path(rel, (Path *)ipath);
      }
      else
      {
        pfree(ipath);
      }
    }
  }

     
                                                                        
     
  if (index_is_ordered && pathkeys_possibly_useful)
  {
    index_pathkeys = build_index_pathkeys(root, index, BackwardScanDirection);
    useful_pathkeys = truncate_useless_pathkeys(root, rel, index_pathkeys);
    if (useful_pathkeys != NIL)
    {
      ipath = create_index_path(root, index, index_clauses, NIL, NIL, useful_pathkeys, BackwardScanDirection, index_only_scan, outer_relids, loop_count, false);
      result = lappend(result, ipath);

                                                        
      if (index->amcanparallel && rel->consider_parallel && outer_relids == NULL && scantype != ST_BITMAPSCAN)
      {
        ipath = create_index_path(root, index, index_clauses, NIL, NIL, useful_pathkeys, BackwardScanDirection, index_only_scan, outer_relids, loop_count, true);

           
                                                                   
                                                 
           
        if (ipath->path.parallel_workers > 0)
        {
          add_partial_path(rel, (Path *)ipath);
        }
        else
        {
          pfree(ipath);
        }
      }
    }
  }

  return result;
}

   
                      
                                                                       
                                                         
   
                                                                         
                             
   
                                                                       
                                                                        
                                                                         
                                                                          
                 
                                                            
                                                                             
                                                                        
                                                                             
                                                                         
                                                                        
                                                                        
                                                             
   
                                                                   
                                                                 
                                                                 
   
static List *
build_paths_for_OR(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses)
{
  List *result = NIL;
  List *all_clauses = NIL;                               
  ListCell *lc;

  foreach (lc, rel->indexlist)
  {
    IndexOptInfo *index = (IndexOptInfo *)lfirst(lc);
    IndexClauseSet clauseset;
    List *indexpaths;
    bool useful_predicate;

                                                         
    if (!index->amhasgetbitmap)
    {
      continue;
    }

       
                                                                         
                                                                           
                                                                  
                                                                      
       
                                                                          
                                                                         
                                                                   
                                                                          
                                                     
       
    useful_predicate = false;
    if (index->indpred != NIL)
    {
      if (index->predOK)
      {
                                                    
      }
      else
      {
                                                  
        if (all_clauses == NIL)
        {
          all_clauses = list_concat(list_copy(clauses), other_clauses);
        }

        if (!predicate_implied_by(index->indpred, all_clauses, false))
        {
          continue;                          
        }

        if (!predicate_implied_by(index->indpred, other_clauses, false))
        {
          useful_predicate = true;
        }
      }
    }

       
                                                                  
       
    MemSet(&clauseset, 0, sizeof(clauseset));
    match_clauses_to_index(root, clauses, index, &clauseset);

       
                                                                      
                      
       
    if (!clauseset.nonempty && !useful_predicate)
    {
      continue;
    }

       
                                                         
       
    match_clauses_to_index(root, other_clauses, index, &clauseset);

       
                                    
       
    indexpaths = build_index_paths(root, rel, index, &clauseset, useful_predicate, ST_BITMAPSCAN, NULL, NULL);
    result = list_concat(result, indexpaths);
  }

  return result;
}

   
                            
                                                                      
                                                                       
                                    
   
                                                                          
                                                                            
                                                    
   
static List *
generate_bitmap_or_paths(PlannerInfo *root, RelOptInfo *rel, List *clauses, List *other_clauses)
{
  List *result = NIL;
  List *all_clauses;
  ListCell *lc;

     
                                                                  
                                                               
     
  all_clauses = list_concat(list_copy(clauses), other_clauses);

  foreach (lc, clauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc);
    List *pathlist;
    Path *bitmapqual;
    ListCell *j;

                                              
    if (!restriction_is_or_clause(rinfo))
    {
      continue;
    }

       
                                                                          
                                     
       
    pathlist = NIL;
    foreach (j, ((BoolExpr *)rinfo->orclause)->args)
    {
      Node *orarg = (Node *)lfirst(j);
      List *indlist;

                                                            
      if (is_andclause(orarg))
      {
        List *andargs = ((BoolExpr *)orarg)->args;

        indlist = build_paths_for_OR(root, rel, andargs, all_clauses);

                                               
        indlist = list_concat(indlist, generate_bitmap_or_paths(root, rel, andargs, all_clauses));
      }
      else
      {
        RestrictInfo *rinfo = castNode(RestrictInfo, orarg);
        List *orargs;

        Assert(!restriction_is_or_clause(rinfo));
        orargs = list_make1(rinfo);

        indlist = build_paths_for_OR(root, rel, orargs, all_clauses);
      }

         
                                                                        
                 
         
      if (indlist == NIL)
      {
        pathlist = NIL;
        break;
      }

         
                                                                    
                   
         
      bitmapqual = choose_bitmap_and(root, rel, indlist);
      pathlist = lappend(pathlist, bitmapqual);
    }

       
                                                               
                                             
       
    if (pathlist != NIL)
    {
      bitmapqual = (Path *)create_bitmap_or_path(root, rel, pathlist);
      result = lappend(result, bitmapqual);
    }
  }

  return result;
}

   
                     
                                                                   
   
                                                                            
                                                                          
                                     
   
                                                                       
                              
   
static Path *
choose_bitmap_and(PlannerInfo *root, RelOptInfo *rel, List *paths)
{
  int npaths = list_length(paths);
  PathClauseUsage **pathinfoarray;
  PathClauseUsage *pathinfo;
  List *clauselist;
  List *bestpaths = NIL;
  Cost bestcost = 0;
  int i, j;
  ListCell *l;

  Assert(npaths > 0);                        
  if (npaths == 1)
  {
    return (Path *)linitial(paths);                
  }

     
                                                                            
                                                                         
                                                                            
                                                                             
                                                           
     
                                                                             
                                                                        
                                                                             
                                                                            
                                                                     
                                                                        
                                
     
                                                                           
                                                                             
                                                                         
                                                                           
                                                                            
                                                                          
                                                                          
                                                                           
     
                                                                            
                                                                       
                                                                           
                                                                       
                                                                        
                                                                            
                                                                      
                                                                      
                                                                         
                                                                          
          
     
                                                                       
                                                                            
                                                                     
                                                                             
                                                                          
                                                                         
                                                                            
                                                                           
                                                                      
                                                                         
                                                                           
                        
     

     
                                                                         
                                                                             
                                                              
     
  pathinfoarray = (PathClauseUsage **)palloc(npaths * sizeof(PathClauseUsage *));
  clauselist = NIL;
  npaths = 0;
  foreach (l, paths)
  {
    Path *ipath = (Path *)lfirst(l);

    pathinfo = classify_index_clause_usage(ipath, &clauselist);

                                                                      
    if (pathinfo->unclassifiable)
    {
      pathinfoarray[npaths++] = pathinfo;
      continue;
    }

    for (i = 0; i < npaths; i++)
    {
      if (!pathinfoarray[i]->unclassifiable && bms_equal(pathinfo->clauseids, pathinfoarray[i]->clauseids))
      {
        break;
      }
    }
    if (i < npaths)
    {
                                                     
      Cost ncost;
      Cost ocost;
      Selectivity nselec;
      Selectivity oselec;

      cost_bitmap_tree_node(pathinfo->path, &ncost, &nselec);
      cost_bitmap_tree_node(pathinfoarray[i]->path, &ocost, &oselec);
      if (ncost < ocost)
      {
        pathinfoarray[i] = pathinfo;
      }
    }
    else
    {
                                                 
      pathinfoarray[npaths++] = pathinfo;
    }
  }

                                              
  if (npaths == 1)
  {
    return pathinfoarray[0]->path;
  }

                                                     
  qsort(pathinfoarray, npaths, sizeof(PathClauseUsage *), path_usage_comparator);

     
                                                                             
                                                                            
                                                                        
     
                                                                        
                                                                         
                                                                        
     
  for (i = 0; i < npaths; i++)
  {
    Cost costsofar;
    List *qualsofar;
    Bitmapset *clauseidsofar;
    ListCell *lastcell;

    pathinfo = pathinfoarray[i];
    paths = list_make1(pathinfo->path);
    costsofar = bitmap_scan_cost_est(root, rel, pathinfo->path);
    qualsofar = list_concat(list_copy(pathinfo->quals), list_copy(pathinfo->preds));
    clauseidsofar = bms_copy(pathinfo->clauseids);
    lastcell = list_head(paths);                          

    for (j = i + 1; j < npaths; j++)
    {
      Cost newcost;

      pathinfo = pathinfoarray[j];
                                
      if (bms_overlap(pathinfo->clauseids, clauseidsofar))
      {
        continue;                            
      }
      if (pathinfo->preds)
      {
        bool redundant = false;

                                                       
        foreach (l, pathinfo->preds)
        {
          Node *np = (Node *)lfirst(l);

          if (predicate_implied_by(list_make1(np), qualsofar, false))
          {
            redundant = true;
            break;                                
          }
        }
        if (redundant)
        {
          continue;
        }
      }
                                                                      
      paths = lappend(paths, pathinfo->path);
      newcost = bitmap_and_cost_est(root, rel, paths);
      if (newcost < costsofar)
      {
                                                                 
        costsofar = newcost;
        qualsofar = list_concat(qualsofar, list_copy(pathinfo->quals));
        qualsofar = list_concat(qualsofar, list_copy(pathinfo->preds));
        clauseidsofar = bms_add_members(clauseidsofar, pathinfo->clauseids);
        lastcell = lnext(lastcell);
      }
      else
      {
                                                        
        paths = list_delete_cell(paths, lnext(lastcell), lastcell);
      }
      Assert(lnext(lastcell) == NULL);
    }

                                                    
    if (i == 0 || costsofar < bestcost)
    {
      bestpaths = paths;
      bestcost = costsofar;
    }

                                                           
    list_free(qualsofar);
  }

  if (list_length(bestpaths) == 1)
  {
    return (Path *)linitial(bestpaths);                      
  }
  return (Path *)create_bitmap_and_path(root, rel, bestpaths);
}

                                                                    
static int
path_usage_comparator(const void *a, const void *b)
{
  PathClauseUsage *pa = *(PathClauseUsage *const *)a;
  PathClauseUsage *pb = *(PathClauseUsage *const *)b;
  Cost acost;
  Cost bcost;
  Selectivity aselec;
  Selectivity bselec;

  cost_bitmap_tree_node(pa->path, &acost, &aselec);
  cost_bitmap_tree_node(pb->path, &bcost, &bselec);

     
                                                 
     
  if (acost < bcost)
  {
    return -1;
  }
  if (acost > bcost)
  {
    return 1;
  }

  if (aselec < bselec)
  {
    return -1;
  }
  if (aselec > bselec)
  {
    return 1;
  }

  return 0;
}

   
                                                                       
                                                             
   
static Cost
bitmap_scan_cost_est(PlannerInfo *root, RelOptInfo *rel, Path *ipath)
{
  BitmapHeapPath bpath;

                                     
  bpath.path.type = T_BitmapHeapPath;
  bpath.path.pathtype = T_BitmapHeapScan;
  bpath.path.parent = rel;
  bpath.path.pathtarget = rel->reltarget;
  bpath.path.param_info = ipath->param_info;
  bpath.path.pathkeys = NIL;
  bpath.bitmapqual = ipath;

     
                                                                       
                                                                  
     
  bpath.path.parallel_workers = 0;

                                           
  cost_bitmap_heap_scan(&bpath.path, root, rel, bpath.path.param_info, ipath, get_loop_count(root, rel->relid, PATH_REQ_OUTER(ipath)));

  return bpath.path.total_cost;
}

   
                                                                           
           
   
static Cost
bitmap_and_cost_est(PlannerInfo *root, RelOptInfo *rel, List *paths)
{
  BitmapAndPath *apath;

     
                                                                            
                                                                    
     
  apath = create_bitmap_and_path(root, rel, paths);

  return bitmap_scan_cost_est(root, rel, (Path *)apath);
}

   
                               
                                                                        
                                                              
                                                          
   
                                                                       
                                                                
                        
   
                                                                           
                                                                           
                               
   
static PathClauseUsage *
classify_index_clause_usage(Path *path, List **clauselist)
{
  PathClauseUsage *result;
  Bitmapset *clauseids;
  ListCell *lc;

  result = (PathClauseUsage *)palloc(sizeof(PathClauseUsage));
  result->path = path;

                                                             
  result->quals = NIL;
  result->preds = NIL;
  find_indexpath_quals(path, &result->quals, &result->preds);

     
                                                                             
                                                                    
                                                                        
                                                                           
                                                                           
                                    
     
  if (list_length(result->quals) + list_length(result->preds) > 100)
  {
    result->clauseids = NULL;
    result->unclassifiable = true;
    return result;
  }

                                                             
  clauseids = NULL;
  foreach (lc, result->quals)
  {
    Node *node = (Node *)lfirst(lc);

    clauseids = bms_add_member(clauseids, find_list_position(node, clauselist));
  }
  foreach (lc, result->preds)
  {
    Node *node = (Node *)lfirst(lc);

    clauseids = bms_add_member(clauseids, find_list_position(node, clauselist));
  }
  result->clauseids = clauseids;
  result->unclassifiable = false;

  return result;
}

   
                        
   
                                                                           
                                                                             
                                                                          
                                           
   
                                                                              
                                                                          
   
                                                                          
                                                                           
                                                          
   
static void
find_indexpath_quals(Path *bitmapqual, List **quals, List **preds)
{
  if (IsA(bitmapqual, BitmapAndPath))
  {
    BitmapAndPath *apath = (BitmapAndPath *)bitmapqual;
    ListCell *l;

    foreach (l, apath->bitmapquals)
    {
      find_indexpath_quals((Path *)lfirst(l), quals, preds);
    }
  }
  else if (IsA(bitmapqual, BitmapOrPath))
  {
    BitmapOrPath *opath = (BitmapOrPath *)bitmapqual;
    ListCell *l;

    foreach (l, opath->bitmapquals)
    {
      find_indexpath_quals((Path *)lfirst(l), quals, preds);
    }
  }
  else if (IsA(bitmapqual, IndexPath))
  {
    IndexPath *ipath = (IndexPath *)bitmapqual;
    ListCell *l;

    foreach (l, ipath->indexclauses)
    {
      IndexClause *iclause = (IndexClause *)lfirst(l);

      *quals = lappend(*quals, iclause->rinfo->clause);
    }
    *preds = list_concat(*preds, list_copy(ipath->indexinfo->indpred));
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", nodeTag(bitmapqual));
  }
}

   
                      
                                                                    
                                                                     
                                                 
   
static int
find_list_position(Node *node, List **nodelist)
{
  int i;
  ListCell *lc;

  i = 0;
  foreach (lc, *nodelist)
  {
    Node *oldnode = (Node *)lfirst(lc);

    if (equal(node, oldnode))
    {
      return i;
    }
    i++;
  }

  *nodelist = lappend(*nodelist, node);

  return i;
}

   
                    
                                                                     
   
static bool
check_index_only(RelOptInfo *rel, IndexOptInfo *index)
{
  bool result;
  Bitmapset *attrs_used = NULL;
  Bitmapset *index_canreturn_attrs = NULL;
  Bitmapset *index_cannotreturn_attrs = NULL;
  ListCell *lc;
  int i;

                                        
  if (!enable_indexonlyscan)
  {
    return false;
  }

     
                                                                             
            
     

     
                                                                          
                                                                       
                                                                    
     
  pull_varattnos((Node *)rel->reltarget->exprs, rel->relid, &attrs_used);

     
                                                                           
                                                                           
                                                                 
     
                                                                      
                                                                             
                                                                        
                                                                        
                                      
     
  foreach (lc, index->indrestrictinfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

    pull_varattnos((Node *)rinfo->clause, rel->relid, &attrs_used);
  }

     
                                                                           
                                                                          
                                                                         
                                                                             
                                                                            
                                             
     
  for (i = 0; i < index->ncolumns; i++)
  {
    int attno = index->indexkeys[i];

       
                                                                           
                                         
       
    if (attno == 0)
    {
      continue;
    }

    if (index->canreturn[i])
    {
      index_canreturn_attrs = bms_add_member(index_canreturn_attrs, attno - FirstLowInvalidHeapAttributeNumber);
    }
    else
    {
      index_cannotreturn_attrs = bms_add_member(index_cannotreturn_attrs, attno - FirstLowInvalidHeapAttributeNumber);
    }
  }

  index_canreturn_attrs = bms_del_members(index_canreturn_attrs, index_cannotreturn_attrs);

                                                
  result = bms_is_subset(attrs_used, index_canreturn_attrs);

  bms_free(attrs_used);
  bms_free(index_canreturn_attrs);
  bms_free(index_cannotreturn_attrs);

  return result;
}

   
                  
                                                                           
                                        
   
                                                                            
                                                                                
                                                                              
                                                                          
                                                                            
                                                                             
                                                                            
                                                                             
                                                                          
                                                     
   
                                                                            
                                                                               
                                                                         
                                                                             
                                                           
   
                                                                      
                                                                      
                               
   
static double
get_loop_count(PlannerInfo *root, Index cur_relid, Relids outer_relids)
{
  double result;
  int outer_relid;

                                                             
  if (outer_relids == NULL)
  {
    return 1.0;
  }

  result = 0.0;
  outer_relid = -1;
  while ((outer_relid = bms_next_member(outer_relids, outer_relid)) >= 0)
  {
    RelOptInfo *outer_rel;
    double rowcount;

                                              
    if (outer_relid >= root->simple_rel_array_size)
    {
      continue;
    }
    outer_rel = root->simple_rel_array[outer_relid];
    if (outer_rel == NULL)
    {
      continue;
    }
    Assert(outer_rel->relid == outer_relid);                            

                                                            
    if (IS_DUMMY_REL(outer_rel))
    {
      continue;
    }

                                                               
    Assert(outer_rel->rows > 0);

                                                               
    rowcount = adjust_rowcount_for_semijoins(root, cur_relid, outer_relid, outer_rel->rows);

                                                                   
    if (result == 0.0 || result > rowcount)
    {
      result = rowcount;
    }
  }
                                                                    
  return (result > 0.0) ? result : 1.0;
}

   
                                                                               
                                                                               
                                                                              
                                                                             
                       
   
static double
adjust_rowcount_for_semijoins(PlannerInfo *root, Index cur_relid, Index outer_relid, double rowcount)
{
  ListCell *lc;

  foreach (lc, root->join_info_list)
  {
    SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)lfirst(lc);

    if (sjinfo->jointype == JOIN_SEMI && bms_is_member(cur_relid, sjinfo->syn_lefthand) && bms_is_member(outer_relid, sjinfo->syn_righthand))
    {
                                                
      double nraw;
      double nunique;

      nraw = approximate_joinrel_size(root, sjinfo->syn_righthand);
      nunique = estimate_num_groups(root, sjinfo->semi_rhs_exprs, nraw, NULL);
      if (rowcount > nunique)
      {
        rowcount = nunique;
      }
    }
  }
  return rowcount;
}

   
                                                          
   
                                                                         
                                                                         
                                                                           
                                                                          
                                                                            
                                                                            
                                       
   
static double
approximate_joinrel_size(PlannerInfo *root, Relids relids)
{
  double rowcount = 1.0;
  int relid;

  relid = -1;
  while ((relid = bms_next_member(relids, relid)) >= 0)
  {
    RelOptInfo *rel;

                                              
    if (relid >= root->simple_rel_array_size)
    {
      continue;
    }
    rel = root->simple_rel_array[relid];
    if (rel == NULL)
    {
      continue;
    }
    Assert(rel->relid == relid);                            

                                                      
    if (IS_DUMMY_REL(rel))
    {
      continue;
    }

                                                               
    Assert(rel->rows > 0);

                            
    rowcount *= rel->rows;
  }
  return rowcount;
}

                                                                              
                                                  
                                                                              

   
                                      
                                                                    
                                               
   
static void
match_restriction_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset)
{
                                                                     
  match_clauses_to_index(root, index->indrestrictinfo, index, clauseset);
}

   
                               
                                                             
                                               
                                                                         
   
static void
match_join_clauses_to_index(PlannerInfo *root, RelOptInfo *rel, IndexOptInfo *index, IndexClauseSet *clauseset, List **joinorclauses)
{
  ListCell *lc;

                                   
  foreach (lc, rel->joininfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

                                                  
    if (!join_clause_is_movable_to(rinfo, rel))
    {
      continue;
    }

                                                                        
    if (restriction_is_or_clause(rinfo))
    {
      *joinorclauses = lappend(*joinorclauses, rinfo);
    }
    else
    {
      match_clause_to_index(root, rinfo, index, clauseset);
    }
  }
}

   
                                 
                                                                              
                                               
   
static void
match_eclass_clauses_to_index(PlannerInfo *root, IndexOptInfo *index, IndexClauseSet *clauseset)
{
  int indexcol;

                                             
  if (!index->rel->has_eclass_joins)
  {
    return;
  }

  for (indexcol = 0; indexcol < index->nkeycolumns; indexcol++)
  {
    ec_member_matches_arg arg;
    List *clauses;

                                                                         
    arg.index = index;
    arg.indexcol = indexcol;
    clauses = generate_implied_equalities_for_column(root, index->rel, ec_member_matches_indexcol, (void *)&arg, index->rel->lateral_referencers);

       
                                                                         
                                                                         
                                                                
       
    match_clauses_to_index(root, clauses, index, clauseset);
  }
}

   
                          
                                                                
                                               
   
static void
match_clauses_to_index(PlannerInfo *root, List *clauses, IndexOptInfo *index, IndexClauseSet *clauseset)
{
  ListCell *lc;

  foreach (lc, clauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc);

    match_clause_to_index(root, rinfo, index, clauseset);
  }
}

   
                         
                                                           
   
                                                                               
                                                                               
          
   
                                                                              
                                                                           
                                                                           
   
                                                                               
                                                                           
                                                                           
                                                            
   
static void
match_clause_to_index(PlannerInfo *root, RestrictInfo *rinfo, IndexOptInfo *index, IndexClauseSet *clauseset)
{
  int indexcol;

     
                                                                          
                                                                          
                                                                            
                                                                  
     
  if (rinfo->pseudoconstant)
  {
    return;
  }

     
                                                                             
                                                              
     
  if (!restriction_is_securely_promotable(rinfo, index->rel))
  {
    return;
  }

                                                   
  for (indexcol = 0; indexcol < index->nkeycolumns; indexcol++)
  {
    IndexClause *iclause;
    ListCell *lc;

                           
    foreach (lc, clauseset->indexclauses[indexcol])
    {
      IndexClause *iclause = (IndexClause *)lfirst(lc);

      if (iclause->rinfo == rinfo)
      {
        return;
      }
    }

                                                         
    iclause = match_clause_to_indexcol(root, rinfo, indexcol, index);
    if (iclause)
    {
                                 
      clauseset->indexclauses[indexcol] = lappend(clauseset->indexclauses[indexcol], iclause);
      clauseset->nonempty = true;
      return;
    }
  }
}

   
                              
                                                                          
                                                                  
   
                                                     
   
                                                                          
           
                                                                           
                            
                                                                           
   
                                                                              
                                                                           
                                                                       
                                                                 
                                                                             
                                                           
   
                                                                       
                                                                             
                                                                       
                                                 
   
                                                                         
                                                                             
                                                                           
                                                                             
                                      
   
                                                                            
                                                                      
                                                                          
                                                                      
                                                                    
                                                      
   
                                                                         
                                                 
   
                                                                             
                                                               
   
                                                                           
                                                               
   
                                                                            
                                                                        
                                                                            
                                                                            
                                                                         
                             
   
                                                                
                                                               
                                     
   
                                                                         
                   
   
                                                                   
                                                               
   
static IndexClause *
match_clause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
  IndexClause *iclause;
  Expr *clause = rinfo->clause;
  Oid opfamily;

  Assert(indexcol < index->nkeycolumns);

     
                                                                          
                                                                  
     
  if (clause == NULL)
  {
    return NULL;
  }

                                            
  opfamily = index->opfamily[indexcol];
  if (IsBooleanOpfamily(opfamily))
  {
    iclause = match_boolean_index_clause(root, rinfo, indexcol, index);
    if (iclause)
    {
      return iclause;
    }
  }

     
                                                                   
                                                                     
                            
     
  if (IsA(clause, OpExpr))
  {
    return match_opclause_to_indexcol(root, rinfo, indexcol, index);
  }
  else if (IsA(clause, FuncExpr))
  {
    return match_funcclause_to_indexcol(root, rinfo, indexcol, index);
  }
  else if (IsA(clause, ScalarArrayOpExpr))
  {
    return match_saopclause_to_indexcol(root, rinfo, indexcol, index);
  }
  else if (IsA(clause, RowCompareExpr))
  {
    return match_rowcompare_to_indexcol(root, rinfo, indexcol, index);
  }
  else if (index->amsearchnulls && IsA(clause, NullTest))
  {
    NullTest *nt = (NullTest *)clause;

    if (!nt->argisrow && match_index_to_operand((Node *)nt->arg, indexcol, index))
    {
      iclause = makeNode(IndexClause);
      iclause->rinfo = rinfo;
      iclause->indexquals = list_make1(rinfo);
      iclause->lossy = false;
      iclause->indexcol = indexcol;
      iclause->indexcols = NIL;
      return iclause;
    }
  }

  return NULL;
}

   
                              
                                                                           
   
                                                                             
                                                                            
                                                                           
                                                                          
                                                                            
                                                                          
                         
   
                                                                      
                                                                       
                                                         
   
static IndexClause *
match_boolean_index_clause(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
  Node *clause = (Node *)rinfo->clause;
  Expr *op = NULL;

                     
  if (match_index_to_operand(clause, indexcol, index))
  {
                                    
    op = make_opclause(BooleanEqualOperator, BOOLOID, false, (Expr *)clause, (Expr *)makeBoolConst(true, false), InvalidOid, InvalidOid);
  }
                   
  else if (is_notclause(clause))
  {
    Node *arg = (Node *)get_notclausearg((Expr *)clause);

    if (match_index_to_operand(arg, indexcol, index))
    {
                                       
      op = make_opclause(BooleanEqualOperator, BOOLOID, false, (Expr *)arg, (Expr *)makeBoolConst(false, false), InvalidOid, InvalidOid);
    }
  }

     
                                                                          
                                                                            
                                                 
     
  else if (clause && IsA(clause, BooleanTest))
  {
    BooleanTest *btest = (BooleanTest *)clause;
    Node *arg = (Node *)btest->arg;

    if (btest->booltesttype == IS_TRUE && match_index_to_operand(arg, indexcol, index))
    {
                                      
      op = make_opclause(BooleanEqualOperator, BOOLOID, false, (Expr *)arg, (Expr *)makeBoolConst(true, false), InvalidOid, InvalidOid);
    }
    else if (btest->booltesttype == IS_FALSE && match_index_to_operand(arg, indexcol, index))
    {
                                       
      op = make_opclause(BooleanEqualOperator, BOOLOID, false, (Expr *)arg, (Expr *)makeBoolConst(false, false), InvalidOid, InvalidOid);
    }
  }

     
                                                                             
                                                 
     
  if (op)
  {
    IndexClause *iclause = makeNode(IndexClause);

    iclause->rinfo = rinfo;
    iclause->indexquals = list_make1(make_simple_restrictinfo(root, op));
    iclause->lossy = false;
    iclause->indexcol = indexcol;
    iclause->indexcols = NIL;
    return iclause;
  }

  return NULL;
}

   
                                
                                                             
                             
   
static IndexClause *
match_opclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
  IndexClause *iclause;
  OpExpr *clause = (OpExpr *)rinfo->clause;
  Node *leftop, *rightop;
  Oid expr_op;
  Oid expr_coll;
  Index index_relid;
  Oid opfamily;
  Oid idxcollation;

     
                                                                      
                                                                     
                                                
     
  if (list_length(clause->args) != 2)
  {
    return NULL;
  }

  leftop = (Node *)linitial(clause->args);
  rightop = (Node *)lsecond(clause->args);
  expr_op = clause->opno;
  expr_coll = clause->inputcollid;

  index_relid = index->rel->relid;
  opfamily = index->opfamily[indexcol];
  idxcollation = index->indexcollations[indexcol];

     
                                                                    
                                                                         
                       
     
                                                                          
                                                                           
                                                                          
     
  if (match_index_to_operand(leftop, indexcol, index) && !bms_is_member(index_relid, rinfo->right_relids) && !contain_volatile_functions(rightop))
  {
    if (IndexCollMatchesExprColl(idxcollation, expr_coll) && op_in_opfamily(expr_op, opfamily))
    {
      iclause = makeNode(IndexClause);
      iclause->rinfo = rinfo;
      iclause->indexquals = list_make1(rinfo);
      iclause->lossy = false;
      iclause->indexcol = indexcol;
      iclause->indexcols = NIL;
      return iclause;
    }

       
                                                                           
                                                        
       
    set_opfuncid(clause);                                                                                  
    return get_index_clause_from_support(root, rinfo, clause->opfuncid, 0,                       
        indexcol, index);
  }

  if (match_index_to_operand(rightop, indexcol, index) && !bms_is_member(index_relid, rinfo->left_relids) && !contain_volatile_functions(leftop))
  {
    if (IndexCollMatchesExprColl(idxcollation, expr_coll))
    {
      Oid comm_op = get_commutator(expr_op);

      if (OidIsValid(comm_op) && op_in_opfamily(comm_op, opfamily))
      {
        RestrictInfo *commrinfo;

                                                      
        commrinfo = commute_restrictinfo(rinfo, comm_op);

                                                                
        iclause = makeNode(IndexClause);
        iclause->rinfo = rinfo;
        iclause->indexquals = list_make1(commrinfo);
        iclause->lossy = false;
        iclause->indexcol = indexcol;
        iclause->indexcols = NIL;
        return iclause;
      }
    }

       
                                                                           
                                                        
       
    set_opfuncid(clause);                                                                                  
    return get_index_clause_from_support(root, rinfo, clause->opfuncid, 1,                        
        indexcol, index);
  }

  return NULL;
}

   
                                  
                                                               
                             
   
static IndexClause *
match_funcclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
  FuncExpr *clause = (FuncExpr *)rinfo->clause;
  int indexarg;
  ListCell *lc;

     
                                                                             
                                                                            
                                                                           
                                                            
     
                                                                             
                                                                           
                                                                            
     
  indexarg = 0;
  foreach (lc, clause->args)
  {
    Node *op = (Node *)lfirst(lc);

    if (match_index_to_operand(op, indexcol, index))
    {
      return get_index_clause_from_support(root, rinfo, clause->funcid, indexarg, indexcol, index);
    }

    indexarg++;
  }

  return NULL;
}

   
                                   
                                                                     
                                                                     
   
static IndexClause *
get_index_clause_from_support(PlannerInfo *root, RestrictInfo *rinfo, Oid funcid, int indexarg, int indexcol, IndexOptInfo *index)
{
  Oid prosupport = get_func_support(funcid);
  SupportRequestIndexCondition req;
  List *sresult;

  if (!OidIsValid(prosupport))
  {
    return NULL;
  }

  req.type = T_SupportRequestIndexCondition;
  req.root = root;
  req.funcid = funcid;
  req.node = (Node *)rinfo->clause;
  req.indexarg = indexarg;
  req.index = index;
  req.indexcol = indexcol;
  req.opfamily = index->opfamily[indexcol];
  req.indexcollation = index->indexcollations[indexcol];

  req.lossy = true;                         

  sresult = (List *)DatumGetPointer(OidFunctionCall1(prosupport, PointerGetDatum(&req)));

  if (sresult != NIL)
  {
    IndexClause *iclause = makeNode(IndexClause);
    List *indexquals = NIL;
    ListCell *lc;

       
                                                                   
                                                                 
       
    foreach (lc, sresult)
    {
      Expr *clause = (Expr *)lfirst(lc);

      indexquals = lappend(indexquals, make_simple_restrictinfo(root, clause));
    }

    iclause->rinfo = rinfo;
    iclause->indexquals = indexquals;
    iclause->lossy = req.lossy;
    iclause->indexcol = indexcol;
    iclause->indexcols = NIL;

    return iclause;
  }

  return NULL;
}

   
                                  
                                                                        
                             
   
static IndexClause *
match_saopclause_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
  ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)rinfo->clause;
  Node *leftop, *rightop;
  Relids right_relids;
  Oid expr_op;
  Oid expr_coll;
  Index index_relid;
  Oid opfamily;
  Oid idxcollation;

                                           
  if (!saop->useOr)
  {
    return NULL;
  }
  leftop = (Node *)linitial(saop->args);
  rightop = (Node *)lsecond(saop->args);
  right_relids = pull_varnos(root, rightop);
  expr_op = saop->opno;
  expr_coll = saop->inputcollid;

  index_relid = index->rel->relid;
  opfamily = index->opfamily[indexcol];
  idxcollation = index->indexcollations[indexcol];

     
                                                                             
     
  if (match_index_to_operand(leftop, indexcol, index) && !bms_is_member(index_relid, right_relids) && !contain_volatile_functions(rightop))
  {
    if (IndexCollMatchesExprColl(idxcollation, expr_coll) && op_in_opfamily(expr_op, opfamily))
    {
      IndexClause *iclause = makeNode(IndexClause);

      iclause->rinfo = rinfo;
      iclause->indexquals = list_make1(rinfo);
      iclause->lossy = false;
      iclause->indexcol = indexcol;
      iclause->indexcols = NIL;
      return iclause;
    }

       
                                                                           
                                     
       
  }

  return NULL;
}

   
                                  
                                                                     
                             
   
                                                                           
                                                                               
                                                                           
                                                
   
static IndexClause *
match_rowcompare_to_indexcol(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index)
{
  RowCompareExpr *clause = (RowCompareExpr *)rinfo->clause;
  Index index_relid;
  Oid opfamily;
  Oid idxcollation;
  Node *leftop, *rightop;
  bool var_on_left;
  Oid expr_op;
  Oid expr_coll;

                                                         
  if (index->relam != BTREE_AM_OID)
  {
    return NULL;
  }

  index_relid = index->rel->relid;
  opfamily = index->opfamily[indexcol];
  idxcollation = index->indexcollations[indexcol];

     
                                                                          
                                                                             
                                                                             
                                                                         
                                                                            
                                                                             
                                                                    
                                           
     
  leftop = (Node *)linitial(clause->largs);
  rightop = (Node *)linitial(clause->rargs);
  expr_op = linitial_oid(clause->opnos);
  expr_coll = linitial_oid(clause->inputcollids);

                                          
  if (!IndexCollMatchesExprColl(idxcollation, expr_coll))
  {
    return NULL;
  }

     
                                                                           
     
  if (match_index_to_operand(leftop, indexcol, index) && !bms_is_member(index_relid, pull_varnos(root, rightop)) && !contain_volatile_functions(rightop))
  {
                                 
    var_on_left = true;
  }
  else if (match_index_to_operand(rightop, indexcol, index) && !bms_is_member(index_relid, pull_varnos(root, leftop)) && !contain_volatile_functions(leftop))
  {
                                                       
    expr_op = get_commutator(expr_op);
    if (expr_op == InvalidOid)
    {
      return NULL;
    }
    var_on_left = false;
  }
  else
  {
    return NULL;
  }

                                                                       
  switch (get_op_opfamily_strategy(expr_op, opfamily))
  {
  case BTLessStrategyNumber:
  case BTLessEqualStrategyNumber:
  case BTGreaterEqualStrategyNumber:
  case BTGreaterStrategyNumber:
    return expand_indexqual_rowcompare(root, rinfo, indexcol, index, expr_op, var_on_left);
  }

  return NULL;
}

   
                                                                       
                             
   
                                                                          
                                                                            
                                                                           
                                                                              
                                                                             
   
                                                                              
                                                                             
   
                                                                    
                                                                        
                                                                         
                                                                            
                                                                             
                                                                   
   
                                                                            
                                              
   
static IndexClause *
expand_indexqual_rowcompare(PlannerInfo *root, RestrictInfo *rinfo, int indexcol, IndexOptInfo *index, Oid expr_op, bool var_on_left)
{
  IndexClause *iclause = makeNode(IndexClause);
  RowCompareExpr *clause = (RowCompareExpr *)rinfo->clause;
  int op_strategy;
  Oid op_lefttype;
  Oid op_righttype;
  int matching_cols;
  List *expr_ops;
  List *opfamilies;
  List *lefttypes;
  List *righttypes;
  List *new_ops;
  List *var_args;
  List *non_var_args;
  ListCell *vargs_cell;
  ListCell *nargs_cell;
  ListCell *opnos_cell;
  ListCell *collids_cell;

  iclause->rinfo = rinfo;
  iclause->indexcol = indexcol;

  if (var_on_left)
  {
    var_args = clause->largs;
    non_var_args = clause->rargs;
  }
  else
  {
    var_args = clause->rargs;
    non_var_args = clause->largs;
  }

  get_op_opfamily_properties(expr_op, index->opfamily[indexcol], false, &op_strategy, &op_lefttype, &op_righttype);

                                                                
  iclause->indexcols = list_make1_int(indexcol);

                                                                            
  expr_ops = list_make1_oid(expr_op);
  opfamilies = list_make1_oid(index->opfamily[indexcol]);
  lefttypes = list_make1_oid(op_lefttype);
  righttypes = list_make1_oid(op_righttype);

     
                                                                          
                                                                          
                                                                             
                       
     
  matching_cols = 1;
  vargs_cell = lnext(list_head(var_args));
  nargs_cell = lnext(list_head(non_var_args));
  opnos_cell = lnext(list_head(clause->opnos));
  collids_cell = lnext(list_head(clause->inputcollids));

  while (vargs_cell != NULL)
  {
    Node *varop = (Node *)lfirst(vargs_cell);
    Node *constop = (Node *)lfirst(nargs_cell);
    int i;

    expr_op = lfirst_oid(opnos_cell);
    if (!var_on_left)
    {
                                                         
      expr_op = get_commutator(expr_op);
      if (expr_op == InvalidOid)
      {
        break;                             
      }
    }
    if (bms_is_member(index->rel->relid, pull_varnos(root, constop)))
    {
      break;                                 
    }
    if (contain_volatile_functions(constop))
    {
      break;                                         
    }

       
                                                           
       
    for (i = 0; i < index->nkeycolumns; i++)
    {
      if (match_index_to_operand(varop, i, index) && get_op_opfamily_strategy(expr_op, index->opfamily[i]) == op_strategy && IndexCollMatchesExprColl(index->indexcollations[i], lfirst_oid(collids_cell)))
      {
        break;
      }
    }
    if (i >= index->nkeycolumns)
    {
      break;                     
    }

                                            
    iclause->indexcols = lappend_int(iclause->indexcols, i);

                                    
    get_op_opfamily_properties(expr_op, index->opfamily[i], false, &op_strategy, &op_lefttype, &op_righttype);
    expr_ops = lappend_oid(expr_ops, expr_op);
    opfamilies = lappend_oid(opfamilies, index->opfamily[i]);
    lefttypes = lappend_oid(lefttypes, op_lefttype);
    righttypes = lappend_oid(righttypes, op_righttype);

                                            
    matching_cols++;
    vargs_cell = lnext(vargs_cell);
    nargs_cell = lnext(nargs_cell);
    opnos_cell = lnext(opnos_cell);
    collids_cell = lnext(collids_cell);
  }

                                                                    
  iclause->lossy = (matching_cols != list_length(clause->opnos));

     
                                                                        
                            
     
  if (var_on_left && !iclause->lossy)
  {
    iclause->indexquals = list_make1(rinfo);
  }
  else
  {
       
                                                                    
                                                                          
                                
       
    if (!iclause->lossy)
    {
                                                      
      new_ops = expr_ops;
    }
    else if (op_strategy == BTLessEqualStrategyNumber || op_strategy == BTGreaterEqualStrategyNumber)
    {
                                                                 
      new_ops = list_truncate(expr_ops, matching_cols);
    }
    else
    {
      ListCell *opfamilies_cell;
      ListCell *lefttypes_cell;
      ListCell *righttypes_cell;

      if (op_strategy == BTLessStrategyNumber)
      {
        op_strategy = BTLessEqualStrategyNumber;
      }
      else if (op_strategy == BTGreaterStrategyNumber)
      {
        op_strategy = BTGreaterEqualStrategyNumber;
      }
      else
      {
        elog(ERROR, "unexpected strategy number %d", op_strategy);
      }
      new_ops = NIL;
      forthree(opfamilies_cell, opfamilies, lefttypes_cell, lefttypes, righttypes_cell, righttypes)
      {
        Oid opfam = lfirst_oid(opfamilies_cell);
        Oid lefttype = lfirst_oid(lefttypes_cell);
        Oid righttype = lfirst_oid(righttypes_cell);

        expr_op = get_opfamily_member(opfam, lefttype, righttype, op_strategy);
        if (!OidIsValid(expr_op))                        
        {
          elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", op_strategy, lefttype, righttype, opfam);
        }
        new_ops = lappend_oid(new_ops, expr_op);
      }
    }

                                                                           
    if (matching_cols > 1)
    {
      RowCompareExpr *rc = makeNode(RowCompareExpr);

      rc->rctype = (RowCompareType)op_strategy;
      rc->opnos = new_ops;
      rc->opfamilies = list_truncate(list_copy(clause->opfamilies), matching_cols);
      rc->inputcollids = list_truncate(list_copy(clause->inputcollids), matching_cols);
      rc->largs = list_truncate(copyObject(var_args), matching_cols);
      rc->rargs = list_truncate(copyObject(non_var_args), matching_cols);
      iclause->indexquals = list_make1(make_simple_restrictinfo(root, (Expr *)rc));
    }
    else
    {
      Expr *op;

                                                             
      iclause->indexcols = NIL;

      op = make_opclause(linitial_oid(new_ops), BOOLOID, false, copyObject(linitial(var_args)), copyObject(linitial(non_var_args)), InvalidOid, linitial_oid(clause->inputcollids));
      iclause->indexquals = list_make1(make_simple_restrictinfo(root, op));
    }
  }

  return iclause;
}

                                                                              
                                                      
                                                                              

   
                           
                                                                      
                                               
   
                                                                               
                                                                           
                                                                          
                                                                      
   
                                                                      
                                           
   
static void
match_pathkeys_to_index(IndexOptInfo *index, List *pathkeys, List **orderby_clauses_p, List **clause_columns_p)
{
  List *orderby_clauses = NIL;
  List *clause_columns = NIL;
  ListCell *lc1;

  *orderby_clauses_p = NIL;                          
  *clause_columns_p = NIL;

                                                                          
  if (!index->amcanorderbyop)
  {
    return;
  }

  foreach (lc1, pathkeys)
  {
    PathKey *pathkey = (PathKey *)lfirst(lc1);
    bool found = false;
    ListCell *lc2;

       
                                                                       
                                                                
       

                                                                         
    if (pathkey->pk_strategy != BTLessStrategyNumber || pathkey->pk_nulls_first)
    {
      return;
    }

                                                              
    if (pathkey->pk_eclass->ec_has_volatile)
    {
      return;
    }

       
                                                                           
                                                                          
                                                                          
                                                                          
                                                                      
                                                  
       
    foreach (lc2, pathkey->pk_eclass->ec_members)
    {
      EquivalenceMember *member = (EquivalenceMember *)lfirst(lc2);
      int indexcol;

                                                                    
      if (!bms_equal(member->em_relids, index->rel->relids))
      {
        continue;
      }

         
                                                                      
                                                                         
                                                                     
                                                                   
                                                                       
                                                    
         
      for (indexcol = 0; indexcol < index->nkeycolumns; indexcol++)
      {
        Expr *expr;

        expr = match_clause_to_ordering_op(index, indexcol, member->em_expr, pathkey->pk_opfamily);
        if (expr)
        {
          orderby_clauses = lappend(orderby_clauses, expr);
          clause_columns = lappend_int(clause_columns, indexcol);
          found = true;
          break;
        }
      }

      if (found)                                              
      {
        break;
      }
    }

    if (!found)                                        
    {
      return;
    }
  }

  *orderby_clauses_p = orderby_clauses;               
  *clause_columns_p = clause_columns;
}

   
                               
                                                                   
                   
   
                                                                     
                                                                  
                                                                    
                                                                        
                                         
   
                                     
                                                               
                                                     
                                                                           
   
                                                                        
                                                                          
                                                                        
                                                                       
                                                        
   
                                                                        
                                                                     
   
static Expr *
match_clause_to_ordering_op(IndexOptInfo *index, int indexcol, Expr *clause, Oid pk_opfamily)
{
  Oid opfamily;
  Oid idxcollation;
  Node *leftop, *rightop;
  Oid expr_op;
  Oid expr_coll;
  Oid sortfamily;
  bool commuted;

  Assert(indexcol < index->nkeycolumns);

  opfamily = index->opfamily[indexcol];
  idxcollation = index->indexcollations[indexcol];

     
                                       
     
  if (!is_opclause(clause))
  {
    return NULL;
  }
  leftop = get_leftop(clause);
  rightop = get_rightop(clause);
  if (!leftop || !rightop)
  {
    return NULL;
  }
  expr_op = ((OpExpr *)clause)->opno;
  expr_coll = ((OpExpr *)clause)->inputcollid;

     
                                                                  
     
  if (!IndexCollMatchesExprColl(idxcollation, expr_coll))
  {
    return NULL;
  }

     
                                                                    
                                   
     
  if (match_index_to_operand(leftop, indexcol, index) && !contain_var_clause(rightop) && !contain_volatile_functions(rightop))
  {
    commuted = false;
  }
  else if (match_index_to_operand(rightop, indexcol, index) && !contain_var_clause(leftop) && !contain_volatile_functions(leftop))
  {
                                                      
    expr_op = get_commutator(expr_op);
    if (expr_op == InvalidOid)
    {
      return NULL;
    }
    commuted = true;
  }
  else
  {
    return NULL;
  }

     
                                                                           
                                                       
     
  sortfamily = get_op_opfamily_sortfamily(expr_op, opfamily);
  if (sortfamily != pk_opfamily)
  {
    return NULL;
  }

                                                                      
  if (commuted)
  {
    OpExpr *newclause = makeNode(OpExpr);

                                            
    memcpy(newclause, clause, sizeof(OpExpr));

                    
    newclause->opno = expr_op;
    newclause->opfuncid = InvalidOid;
    newclause->args = list_make2(rightop, leftop);

    clause = (Expr *)newclause;
  }

  return clause;
}

                                                                              
                                                              
                                                                              

   
                          
                                                                 
                               
   
                                                                             
                                                                     
   
                                                                           
                                                                            
                                                                              
                                                                        
   
                                                                        
                                                                            
                                                                         
                                                                            
                                                                              
                                                                              
                                                                    
   
void
check_index_predicates(PlannerInfo *root, RelOptInfo *rel)
{
  List *clauselist;
  bool have_partial;
  bool is_target_rel;
  Relids otherrels;
  ListCell *lc;

                                                                       
  Assert(IS_SIMPLE_REL(rel));

     
                                                             
                                                                            
                                     
     
  have_partial = false;
  foreach (lc, rel->indexlist)
  {
    IndexOptInfo *index = (IndexOptInfo *)lfirst(lc);

    index->indrestrictinfo = rel->baserestrictinfo;
    if (index->indpred)
    {
      have_partial = true;
    }
  }
  if (!have_partial)
  {
    return;
  }

     
                                                                            
                                                                        
                                                                           
                                                                           
                                               
     
  clauselist = list_copy(rel->baserestrictinfo);

                                   
  foreach (lc, rel->joininfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

                                                  
    if (!join_clause_is_movable_to(rinfo, rel))
    {
      continue;
    }

    clauselist = lappend(clauselist, rinfo);
  }

     
                                                                           
                                                                        
                                                                             
                                                                       
     
  if (rel->reloptkind == RELOPT_OTHER_MEMBER_REL)
  {
    otherrels = bms_difference(root->all_baserels, find_childrel_parents(root, rel));
  }
  else
  {
    otherrels = bms_difference(root->all_baserels, rel->relids);
  }

  if (!bms_is_empty(otherrels))
  {
    clauselist = list_concat(clauselist, generate_join_implied_equalities(root, bms_union(rel->relids, otherrels), otherrels, rel));
  }

     
                                                                    
                                                                      
                                                                            
                                                                         
                                                                            
                                                                           
                                                                           
                                                                            
                                                                     
     
  is_target_rel = (rel->relid == root->parse->resultRelation || get_plan_rowmark(root->rowMarks, rel->relid) != NULL);

     
                                                                 
                                                                          
                                                                       
                                                                        
                                 
     
  foreach (lc, rel->indexlist)
  {
    IndexOptInfo *index = (IndexOptInfo *)lfirst(lc);
    ListCell *lcr;

    if (index->indpred == NIL)
    {
      continue;                                      
    }

    if (!index->predOK)                                             
    {
      index->predOK = predicate_implied_by(index->indpred, clauselist, false);
    }

                                                                        
    if (is_target_rel)
    {
      continue;
    }

                                                               
    index->indrestrictinfo = NIL;
    foreach (lcr, rel->baserestrictinfo)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(lcr);

                                                                 
      if (contain_mutable_functions((Node *)rinfo->clause) || !predicate_implied_by(list_make1(rinfo->clause), index->indpred, false))
      {
        index->indrestrictinfo = lappend(index->indrestrictinfo, rinfo);
      }
    }
  }
}

                                                                              
                                                                  
                                                                              

   
                              
                                                                      
   
                                                                         
   
static bool
ec_member_matches_indexcol(PlannerInfo *root, RelOptInfo *rel, EquivalenceClass *ec, EquivalenceMember *em, void *arg)
{
  IndexOptInfo *index = ((ec_member_matches_arg *)arg)->index;
  int indexcol = ((ec_member_matches_arg *)arg)->indexcol;
  Oid curFamily;
  Oid curCollation;

  Assert(indexcol < index->nkeycolumns);

  curFamily = index->opfamily[indexcol];
  curCollation = index->indexcollations[indexcol];

     
                                                                   
                                                                            
                                                                       
                                                                            
                                                                       
                                                      
                                                 
                                    
     
  if (index->relam == BTREE_AM_OID && !list_member_oid(ec->ec_opfamilies, curFamily))
  {
    return false;
  }

                                                                
  if (!IndexCollMatchesExprColl(curCollation, ec->ec_collation))
  {
    return false;
  }

  return match_index_to_operand((Node *)em->em_expr, indexcol, index);
}

   
                                 
                                                                            
                                                                        
                                   
   
                                                                    
                                                                            
                                                                         
                                                                            
                                                                             
                                                                           
                  
                                                                          
                                                                               
                                                                      
                                                       
   
                                                                       
                                                                           
                                                                          
   
bool
relation_has_unique_index_for(PlannerInfo *root, RelOptInfo *rel, List *restrictlist, List *exprlist, List *oprlist)
{
  ListCell *ic;

  Assert(list_length(exprlist) == list_length(oprlist));

                                      
  if (rel->indexlist == NIL)
  {
    return false;
  }

     
                                                                          
                                          
     
  foreach (ic, rel->baserestrictinfo)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(ic);

       
                                                                 
                                                                      
                                           
       
    if (restrictinfo->mergeopfamilies == NIL)
    {
      continue;                        
    }

       
                                                                         
                                                            
       
    if (bms_is_empty(restrictinfo->left_relids))
    {
                                   
      restrictinfo->outer_is_left = true;
    }
    else if (bms_is_empty(restrictinfo->right_relids))
    {
                                  
      restrictinfo->outer_is_left = false;
    }
    else
    {
      continue;
    }

                         
    restrictlist = lappend(restrictlist, restrictinfo);
  }

                                   
  if (restrictlist == NIL && exprlist == NIL)
  {
    return false;
  }

                                              
  foreach (ic, rel->indexlist)
  {
    IndexOptInfo *ind = (IndexOptInfo *)lfirst(ic);
    int c;

       
                                                                           
                                                                        
       
    if (!ind->unique || !ind->immediate || (ind->indpred != NIL && !ind->predOK))
    {
      continue;
    }

       
                                                                          
                                                                 
       
    for (c = 0; c < ind->nkeycolumns; c++)
    {
      bool matched = false;
      ListCell *lc;
      ListCell *lc2;

      foreach (lc, restrictlist)
      {
        RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);
        Node *rexpr;

           
                                                                     
                                                                      
                                                                  
                                                                      
           
        if (!list_member_oid(rinfo->mergeopfamilies, ind->opfamily[c]))
        {
          continue;
        }

           
                                                                       
                                                                      
                               
           

                                                                    
        if (rinfo->outer_is_left)
        {
          rexpr = get_rightop(rinfo->clause);
        }
        else
        {
          rexpr = get_leftop(rinfo->clause);
        }

        if (match_index_to_operand(rexpr, c, ind))
        {
          matched = true;                       
          break;
        }
      }

      if (matched)
      {
        continue;
      }

      forboth(lc, exprlist, lc2, oprlist)
      {
        Node *expr = (Node *)lfirst(lc);
        Oid opr = lfirst_oid(lc2);

                                                         
        if (!match_index_to_operand(expr, c, ind))
        {
          continue;
        }

           
                                                               
                                                                
                                                                   
                                                                      
                                             
           
        if (!op_in_opfamily(opr, ind->opfamily[c]))
        {
          continue;
        }

           
                                                                       
                                                                      
                               
           

        matched = true;                       
        break;
      }

      if (!matched)
      {
        break;                                           
      }
    }

                                                
    if (c == ind->nkeycolumns)
    {
      return true;
    }
  }

  return false;
}

   
                                       
   
                                                                             
                                                                         
                                                                              
                                                                           
                                                                           
                                                                          
                                                                             
                                                                               
                                                                             
                                                                             
                                                                           
                                                                
   
bool
indexcol_is_bool_constant_for_query(PlannerInfo *root, IndexOptInfo *index, int indexcol)
{
  ListCell *lc;

                                                                 
  if (!IsBooleanOpfamily(index->opfamily[indexcol]))
  {
    return false;
  }

                                                         
  foreach (lc, index->rel->baserestrictinfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

       
                                                                      
                                                                       
                                                                           
       
    if (rinfo->pseudoconstant)
    {
      continue;
    }

                                                                         
    if (match_boolean_index_clause(root, rinfo, indexcol, index))
    {
      return true;
    }
  }

  return false;
}

                                                                              
                                             
                                                                              

   
                            
                                                         
                                                                  
   
                                                     
                                                              
                                
   
                                                                            
                                                                               
   
                                           
   
bool
match_index_to_operand(Node *operand, int indexcol, IndexOptInfo *index)
{
  int indkey;

     
                                                                           
                                                                            
                                                          
                                                                     
     
  if (operand && IsA(operand, RelabelType))
  {
    operand = (Node *)((RelabelType *)operand)->arg;
  }

  indkey = index->indexkeys[indexcol];
  if (indkey != 0)
  {
       
                                                            
       
    if (operand && IsA(operand, Var) && index->rel->relid == ((Var *)operand)->varno && indkey == ((Var *)operand)->varattno)
    {
      return true;
    }
  }
  else
  {
       
                                                                          
                                                                       
                                        
       
    ListCell *indexpr_item;
    int i;
    Node *indexkey;

    indexpr_item = list_head(index->indexprs);
    for (i = 0; i < indexcol; i++)
    {
      if (index->indexkeys[i] == 0)
      {
        if (indexpr_item == NULL)
        {
          elog(ERROR, "wrong number of index expressions");
        }
        indexpr_item = lnext(indexpr_item);
      }
    }
    if (indexpr_item == NULL)
    {
      elog(ERROR, "wrong number of index expressions");
    }
    indexkey = (Node *)lfirst(indexpr_item);

       
                                                                
       
    if (indexkey && IsA(indexkey, RelabelType))
    {
      indexkey = (Node *)((RelabelType *)indexkey)->arg;
    }

    if (equal(indexkey, operand))
    {
      return true;
    }
  }

  return false;
}

   
                                  
                                                                   
                       
   
                                                                          
                                                                    
                                                                    
                                                                       
                                                        
   
                                                                   
                                                                        
                                                                       
                                                                       
                                                          
   
                                    
                                
   
bool
is_pseudo_constant_for_index(Node *expr, IndexOptInfo *index)
{
  return is_pseudo_constant_for_index_new(NULL, expr, index);
}

bool
is_pseudo_constant_for_index_new(PlannerInfo *root, Node *expr, IndexOptInfo *index)
{
                                                                      
  if (bms_is_member(index->rel->relid, pull_varnos(root, expr)))
  {
    return false;                                     
  }
  if (contain_volatile_functions(expr))
  {
    return false;                                         
  }
  return true;
}
