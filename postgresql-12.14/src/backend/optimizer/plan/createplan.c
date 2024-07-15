                                                                            
   
                
                                                                 
                                                                
                       
   
                                                                         
                                                                        
   
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/sysattr.h"
#include "catalog/pg_class.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/extensible.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/paramassign.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "partitioning/partprune.h"
#include "utils/lsyscache.h"

   
                                                                             
                                
   
                                                                             
                                                                     
                                                                         
                                                                           
                               
   
                                                                         
                                                                         
                              
   
                                                                            
                                                                   
                                                                             
                                                                       
   
                                                                              
                                                                          
   
#define CP_EXACT_TLIST 0x0001                                        
#define CP_SMALL_TLIST 0x0002                              
#define CP_LABEL_TLIST 0x0004                                        
#define CP_IGNORE_TLIST 0x0008                                

static Plan *
create_plan_recurse(PlannerInfo *root, Path *best_path, int flags);
static Plan *
create_scan_plan(PlannerInfo *root, Path *best_path, int flags);
static List *
build_path_tlist(PlannerInfo *root, Path *path);
static bool
use_physical_tlist(PlannerInfo *root, Path *path, int flags);
static List *
get_gating_quals(PlannerInfo *root, List *quals);
static Plan *
create_gating_plan(PlannerInfo *root, Path *path, Plan *plan, List *gating_quals);
static Plan *
create_join_plan(PlannerInfo *root, JoinPath *best_path);
static Plan *
create_append_plan(PlannerInfo *root, AppendPath *best_path, int flags);
static Plan *
create_merge_append_plan(PlannerInfo *root, MergeAppendPath *best_path, int flags);
static Result *
create_group_result_plan(PlannerInfo *root, GroupResultPath *best_path);
static ProjectSet *
create_project_set_plan(PlannerInfo *root, ProjectSetPath *best_path);
static Material *
create_material_plan(PlannerInfo *root, MaterialPath *best_path, int flags);
static Plan *
create_unique_plan(PlannerInfo *root, UniquePath *best_path, int flags);
static Gather *
create_gather_plan(PlannerInfo *root, GatherPath *best_path);
static Plan *
create_projection_plan(PlannerInfo *root, ProjectionPath *best_path, int flags);
static Plan *
inject_projection_plan(Plan *subplan, List *tlist, bool parallel_safe);
static Sort *
create_sort_plan(PlannerInfo *root, SortPath *best_path, int flags);
static Group *
create_group_plan(PlannerInfo *root, GroupPath *best_path);
static Unique *
create_upper_unique_plan(PlannerInfo *root, UpperUniquePath *best_path, int flags);
static Agg *
create_agg_plan(PlannerInfo *root, AggPath *best_path);
static Plan *
create_groupingsets_plan(PlannerInfo *root, GroupingSetsPath *best_path);
static Result *
create_minmaxagg_plan(PlannerInfo *root, MinMaxAggPath *best_path);
static WindowAgg *
create_windowagg_plan(PlannerInfo *root, WindowAggPath *best_path);
static SetOp *
create_setop_plan(PlannerInfo *root, SetOpPath *best_path, int flags);
static RecursiveUnion *
create_recursiveunion_plan(PlannerInfo *root, RecursiveUnionPath *best_path);
static LockRows *
create_lockrows_plan(PlannerInfo *root, LockRowsPath *best_path, int flags);
static ModifyTable *
create_modifytable_plan(PlannerInfo *root, ModifyTablePath *best_path);
static Limit *
create_limit_plan(PlannerInfo *root, LimitPath *best_path, int flags);
static SeqScan *
create_seqscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static SampleScan *
create_samplescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static Scan *
create_indexscan_plan(PlannerInfo *root, IndexPath *best_path, List *tlist, List *scan_clauses, bool indexonly);
static BitmapHeapScan *
create_bitmap_scan_plan(PlannerInfo *root, BitmapHeapPath *best_path, List *tlist, List *scan_clauses);
static Plan *
create_bitmap_subplan(PlannerInfo *root, Path *bitmapqual, List **qual, List **indexqual, List **indexECs);
static void
bitmap_subplan_mark_shared(Plan *plan);
static TidScan *
create_tidscan_plan(PlannerInfo *root, TidPath *best_path, List *tlist, List *scan_clauses);
static SubqueryScan *
create_subqueryscan_plan(PlannerInfo *root, SubqueryScanPath *best_path, List *tlist, List *scan_clauses);
static FunctionScan *
create_functionscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static ValuesScan *
create_valuesscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static TableFuncScan *
create_tablefuncscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static CteScan *
create_ctescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static NamedTuplestoreScan *
create_namedtuplestorescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static Result *
create_resultscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static WorkTableScan *
create_worktablescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static ForeignScan *
create_foreignscan_plan(PlannerInfo *root, ForeignPath *best_path, List *tlist, List *scan_clauses);
static CustomScan *
create_customscan_plan(PlannerInfo *root, CustomPath *best_path, List *tlist, List *scan_clauses);
static NestLoop *
create_nestloop_plan(PlannerInfo *root, NestPath *best_path);
static MergeJoin *
create_mergejoin_plan(PlannerInfo *root, MergePath *best_path);
static HashJoin *
create_hashjoin_plan(PlannerInfo *root, HashPath *best_path);
static Node *
replace_nestloop_params(PlannerInfo *root, Node *expr);
static Node *
replace_nestloop_params_mutator(Node *node, PlannerInfo *root);
static void
fix_indexqual_references(PlannerInfo *root, IndexPath *index_path, List **stripped_indexquals_p, List **fixed_indexquals_p);
static List *
fix_indexorderby_references(PlannerInfo *root, IndexPath *index_path);
static Node *
fix_indexqual_clause(PlannerInfo *root, IndexOptInfo *index, int indexcol, Node *clause, List *indexcolnos);
static Node *
fix_indexqual_operand(Node *node, IndexOptInfo *index, int indexcol);
static List *
get_switched_clauses(List *clauses, Relids outerrelids);
static List *
order_qual_clauses(PlannerInfo *root, List *clauses);
static void
copy_generic_path_info(Plan *dest, Path *src);
static void
copy_plan_costsize(Plan *dest, Plan *src);
static void
label_sort_with_costsize(PlannerInfo *root, Sort *plan, double limit_tuples);
static SeqScan *
make_seqscan(List *qptlist, List *qpqual, Index scanrelid);
static SampleScan *
make_samplescan(List *qptlist, List *qpqual, Index scanrelid, TableSampleClause *tsc);
static IndexScan *
make_indexscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig, List *indexorderby, List *indexorderbyorig, List *indexorderbyops, ScanDirection indexscandir);
static IndexOnlyScan *
make_indexonlyscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *recheckqual, List *indexorderby, List *indextlist, ScanDirection indexscandir);
static BitmapIndexScan *
make_bitmap_indexscan(Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig);
static BitmapHeapScan *
make_bitmap_heapscan(List *qptlist, List *qpqual, Plan *lefttree, List *bitmapqualorig, Index scanrelid);
static TidScan *
make_tidscan(List *qptlist, List *qpqual, Index scanrelid, List *tidquals);
static SubqueryScan *
make_subqueryscan(List *qptlist, List *qpqual, Index scanrelid, Plan *subplan);
static FunctionScan *
make_functionscan(List *qptlist, List *qpqual, Index scanrelid, List *functions, bool funcordinality);
static ValuesScan *
make_valuesscan(List *qptlist, List *qpqual, Index scanrelid, List *values_lists);
static TableFuncScan *
make_tablefuncscan(List *qptlist, List *qpqual, Index scanrelid, TableFunc *tablefunc);
static CteScan *
make_ctescan(List *qptlist, List *qpqual, Index scanrelid, int ctePlanId, int cteParam);
static NamedTuplestoreScan *
make_namedtuplestorescan(List *qptlist, List *qpqual, Index scanrelid, char *enrname);
static WorkTableScan *
make_worktablescan(List *qptlist, List *qpqual, Index scanrelid, int wtParam);
static RecursiveUnion *
make_recursive_union(List *tlist, Plan *lefttree, Plan *righttree, int wtParam, List *distinctList, long numGroups);
static BitmapAnd *
make_bitmap_and(List *bitmapplans);
static BitmapOr *
make_bitmap_or(List *bitmapplans);
static NestLoop *
make_nestloop(List *tlist, List *joinclauses, List *otherclauses, List *nestParams, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique);
static HashJoin *
make_hashjoin(List *tlist, List *joinclauses, List *otherclauses, List *hashclauses, List *hashoperators, List *hashcollations, List *hashkeys, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique);
static Hash *
make_hash(Plan *lefttree, List *hashkeys, Oid skewTable, AttrNumber skewColumn, bool skewInherit);
static MergeJoin *
make_mergejoin(List *tlist, List *joinclauses, List *otherclauses, List *mergeclauses, Oid *mergefamilies, Oid *mergecollations, int *mergestrategies, bool *mergenullsfirst, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique, bool skip_mark_restore);
static Sort *
make_sort(Plan *lefttree, int numCols, AttrNumber *sortColIdx, Oid *sortOperators, Oid *collations, bool *nullsFirst);
static Plan *
prepare_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids, const AttrNumber *reqColIdx, bool adjust_tlist_in_place, int *p_numsortkeys, AttrNumber **p_sortColIdx, Oid **p_sortOperators, Oid **p_collations, bool **p_nullsFirst);
static EquivalenceMember *
find_ec_member_for_tle(EquivalenceClass *ec, TargetEntry *tle, Relids relids);
static Sort *
make_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids);
static Sort *
make_sort_from_groupcols(List *groupcls, AttrNumber *grpColIdx, Plan *lefttree);
static Material *
make_material(Plan *lefttree);
static WindowAgg *
make_windowagg(List *tlist, Index winref, int partNumCols, AttrNumber *partColIdx, Oid *partOperators, Oid *partCollations, int ordNumCols, AttrNumber *ordColIdx, Oid *ordOperators, Oid *ordCollations, int frameOptions, Node *startOffset, Node *endOffset, Oid startInRangeFunc, Oid endInRangeFunc, Oid inRangeColl, bool inRangeAsc, bool inRangeNullsFirst, Plan *lefttree);
static Group *
make_group(List *tlist, List *qual, int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations, Plan *lefttree);
static Unique *
make_unique_from_sortclauses(Plan *lefttree, List *distinctList);
static Unique *
make_unique_from_pathkeys(Plan *lefttree, List *pathkeys, int numCols);
static Gather *
make_gather(List *qptlist, List *qpqual, int nworkers, int rescan_param, bool single_copy, Plan *subplan);
static SetOp *
make_setop(SetOpCmd cmd, SetOpStrategy strategy, Plan *lefttree, List *distinctList, AttrNumber flagColIdx, int firstFlag, long numGroups);
static LockRows *
make_lockrows(Plan *lefttree, List *rowMarks, int epqParam);
static Result *
make_result(List *tlist, Node *resconstantqual, Plan *subplan);
static ProjectSet *
make_project_set(List *tlist, Plan *subplan);
static ModifyTable *
make_modifytable(PlannerInfo *root, CmdType operation, bool canSetTag, Index nominalRelation, Index rootRelation, bool partColsUpdated, List *resultRelations, List *subplans, List *subroots, List *withCheckOptionLists, List *returningLists, List *rowMarks, OnConflictExpr *onconflict, int epqParam);
static GatherMerge *
create_gather_merge_plan(PlannerInfo *root, GatherMergePath *best_path);

   
               
                                                                       
                                                                       
                                                                          
                                                                 
   
                                                                        
                                                                        
                               
   
                                       
   
                          
   
Plan *
create_plan(PlannerInfo *root, Path *best_path)
{
  Plan *plan;

                                                               
  Assert(root->plan_params == NIL);

                                                         
  root->curOuterRels = NULL;
  root->curOuterParams = NIL;

                                                                             
  plan = create_plan_recurse(root, best_path, CP_EXACT_TLIST);

     
                                                                       
                                                                           
                                                                          
                                                                        
                                                                 
     
  if (!IsA(plan, ModifyTable))
  {
    apply_tlist_labeling(plan->targetlist, root->processed_tlist);
  }

     
                                                                          
                                                                        
                                                                           
                                                                     
                                                                   
     
  SS_attach_initplans(root, plan);

                                                                       
  if (root->curOuterParams != NIL)
  {
    elog(ERROR, "failed to assign all NestLoopParams to plan nodes");
  }

     
                                                                            
                   
     
  root->plan_params = NIL;

  return plan;
}

   
                       
                                      
   
static Plan *
create_plan_recurse(PlannerInfo *root, Path *best_path, int flags)
{
  Plan *plan;

                                                                
  check_stack_depth();

  switch (best_path->pathtype)
  {
  case T_SeqScan:
  case T_SampleScan:
  case T_IndexScan:
  case T_IndexOnlyScan:
  case T_BitmapHeapScan:
  case T_TidScan:
  case T_SubqueryScan:
  case T_FunctionScan:
  case T_TableFuncScan:
  case T_ValuesScan:
  case T_CteScan:
  case T_WorkTableScan:
  case T_NamedTuplestoreScan:
  case T_ForeignScan:
  case T_CustomScan:
    plan = create_scan_plan(root, best_path, flags);
    break;
  case T_HashJoin:
  case T_MergeJoin:
  case T_NestLoop:
    plan = create_join_plan(root, (JoinPath *)best_path);
    break;
  case T_Append:
    plan = create_append_plan(root, (AppendPath *)best_path, flags);
    break;
  case T_MergeAppend:
    plan = create_merge_append_plan(root, (MergeAppendPath *)best_path, flags);
    break;
  case T_Result:
    if (IsA(best_path, ProjectionPath))
    {
      plan = create_projection_plan(root, (ProjectionPath *)best_path, flags);
    }
    else if (IsA(best_path, MinMaxAggPath))
    {
      plan = (Plan *)create_minmaxagg_plan(root, (MinMaxAggPath *)best_path);
    }
    else if (IsA(best_path, GroupResultPath))
    {
      plan = (Plan *)create_group_result_plan(root, (GroupResultPath *)best_path);
    }
    else
    {
                                           
      Assert(IsA(best_path, Path));
      plan = create_scan_plan(root, best_path, flags);
    }
    break;
  case T_ProjectSet:
    plan = (Plan *)create_project_set_plan(root, (ProjectSetPath *)best_path);
    break;
  case T_Material:
    plan = (Plan *)create_material_plan(root, (MaterialPath *)best_path, flags);
    break;
  case T_Unique:
    if (IsA(best_path, UpperUniquePath))
    {
      plan = (Plan *)create_upper_unique_plan(root, (UpperUniquePath *)best_path, flags);
    }
    else
    {
      Assert(IsA(best_path, UniquePath));
      plan = create_unique_plan(root, (UniquePath *)best_path, flags);
    }
    break;
  case T_Gather:
    plan = (Plan *)create_gather_plan(root, (GatherPath *)best_path);
    break;
  case T_Sort:
    plan = (Plan *)create_sort_plan(root, (SortPath *)best_path, flags);
    break;
  case T_Group:
    plan = (Plan *)create_group_plan(root, (GroupPath *)best_path);
    break;
  case T_Agg:
    if (IsA(best_path, GroupingSetsPath))
    {
      plan = create_groupingsets_plan(root, (GroupingSetsPath *)best_path);
    }
    else
    {
      Assert(IsA(best_path, AggPath));
      plan = (Plan *)create_agg_plan(root, (AggPath *)best_path);
    }
    break;
  case T_WindowAgg:
    plan = (Plan *)create_windowagg_plan(root, (WindowAggPath *)best_path);
    break;
  case T_SetOp:
    plan = (Plan *)create_setop_plan(root, (SetOpPath *)best_path, flags);
    break;
  case T_RecursiveUnion:
    plan = (Plan *)create_recursiveunion_plan(root, (RecursiveUnionPath *)best_path);
    break;
  case T_LockRows:
    plan = (Plan *)create_lockrows_plan(root, (LockRowsPath *)best_path, flags);
    break;
  case T_ModifyTable:
    plan = (Plan *)create_modifytable_plan(root, (ModifyTablePath *)best_path);
    break;
  case T_Limit:
    plan = (Plan *)create_limit_plan(root, (LimitPath *)best_path, flags);
    break;
  case T_GatherMerge:
    plan = (Plan *)create_gather_merge_plan(root, (GatherMergePath *)best_path);
    break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)best_path->pathtype);
    plan = NULL;                          
    break;
  }

  return plan;
}

   
                    
                                                               
   
static Plan *
create_scan_plan(PlannerInfo *root, Path *best_path, int flags)
{
  RelOptInfo *rel = best_path->parent;
  List *scan_clauses;
  List *gating_clauses;
  List *tlist;
  Plan *plan;

     
                                                                            
                                                                            
                                                     
     
                                                                           
                                                                           
                                                                           
                                                                     
                                                                             
                                                                 
     
  switch (best_path->pathtype)
  {
  case T_IndexScan:
  case T_IndexOnlyScan:
    scan_clauses = castNode(IndexPath, best_path)->indexinfo->indrestrictinfo;
    break;
  default:
    scan_clauses = rel->baserestrictinfo;
    break;
  }

     
                                                                           
                                                   
     
                                                                         
     
  if (best_path->param_info)
  {
    scan_clauses = list_concat(list_copy(scan_clauses), best_path->param_info->ppi_clauses);
  }

     
                                                                             
                                                                           
                                               
     
  gating_clauses = get_gating_quals(root, scan_clauses);
  if (gating_clauses)
  {
    flags = 0;
  }

     
                                                                          
                                                                            
                                                                          
                                                                
     
                                                                       
                                                                           
                                                                       
     
  if (flags == CP_IGNORE_TLIST)
  {
    tlist = NULL;
  }
  else if (use_physical_tlist(root, best_path, flags))
  {
    if (best_path->pathtype == T_IndexOnlyScan)
    {
                                                                   
      tlist = copyObject(((IndexPath *)best_path)->indexinfo->indextlist);

         
                                                                 
                                                                     
         
      if (flags & CP_LABEL_TLIST)
      {
        apply_pathtarget_labeling_to_tlist(tlist, best_path->pathtarget);
      }
    }
    else
    {
      tlist = build_physical_tlist(root, rel);
      if (tlist == NIL)
      {
                                                                   
        tlist = build_path_tlist(root, best_path);
      }
      else
      {
                                                                       
        if (flags & CP_LABEL_TLIST)
        {
          apply_pathtarget_labeling_to_tlist(tlist, best_path->pathtarget);
        }
      }
    }
  }
  else
  {
    tlist = build_path_tlist(root, best_path);
  }

  switch (best_path->pathtype)
  {
  case T_SeqScan:
    plan = (Plan *)create_seqscan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_SampleScan:
    plan = (Plan *)create_samplescan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_IndexScan:
    plan = (Plan *)create_indexscan_plan(root, (IndexPath *)best_path, tlist, scan_clauses, false);
    break;

  case T_IndexOnlyScan:
    plan = (Plan *)create_indexscan_plan(root, (IndexPath *)best_path, tlist, scan_clauses, true);
    break;

  case T_BitmapHeapScan:
    plan = (Plan *)create_bitmap_scan_plan(root, (BitmapHeapPath *)best_path, tlist, scan_clauses);
    break;

  case T_TidScan:
    plan = (Plan *)create_tidscan_plan(root, (TidPath *)best_path, tlist, scan_clauses);
    break;

  case T_SubqueryScan:
    plan = (Plan *)create_subqueryscan_plan(root, (SubqueryScanPath *)best_path, tlist, scan_clauses);
    break;

  case T_FunctionScan:
    plan = (Plan *)create_functionscan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_TableFuncScan:
    plan = (Plan *)create_tablefuncscan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_ValuesScan:
    plan = (Plan *)create_valuesscan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_CteScan:
    plan = (Plan *)create_ctescan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_NamedTuplestoreScan:
    plan = (Plan *)create_namedtuplestorescan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_Result:
    plan = (Plan *)create_resultscan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_WorkTableScan:
    plan = (Plan *)create_worktablescan_plan(root, best_path, tlist, scan_clauses);
    break;

  case T_ForeignScan:
    plan = (Plan *)create_foreignscan_plan(root, (ForeignPath *)best_path, tlist, scan_clauses);
    break;

  case T_CustomScan:
    plan = (Plan *)create_customscan_plan(root, (CustomPath *)best_path, tlist, scan_clauses);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)best_path->pathtype);
    plan = NULL;                          
    break;
  }

     
                                                                             
                                                                       
            
     
  if (gating_clauses)
  {
    plan = create_gating_plan(root, best_path, plan, gating_clauses);
  }

  return plan;
}

   
                                                                          
   
                                                                         
                                        
   
static List *
build_path_tlist(PlannerInfo *root, Path *path)
{
  List *tlist = NIL;
  Index *sortgrouprefs = path->pathtarget->sortgrouprefs;
  int resno = 1;
  ListCell *v;

  foreach (v, path->pathtarget->exprs)
  {
    Node *node = (Node *)lfirst(v);
    TargetEntry *tle;

       
                                                                          
                                                                          
                                                                        
                   
       
    if (path->param_info)
    {
      node = replace_nestloop_params(root, node);
    }

    tle = makeTargetEntry((Expr *)node, resno, NULL, false);
    if (sortgrouprefs)
    {
      tle->ressortgroupref = sortgrouprefs[resno - 1];
    }

    tlist = lappend(tlist, tle);
    resno++;
  }
  return tlist;
}

   
                      
                                                               
                                                     
   
static bool
use_physical_tlist(PlannerInfo *root, Path *path, int flags)
{
  RelOptInfo *rel = path->parent;
  int i;
  ListCell *lc;

     
                                                                 
     
  if (flags & (CP_EXACT_TLIST | CP_SMALL_TLIST))
  {
    return false;
  }

     
                                                                             
                                                                            
     
  if (rel->rtekind != RTE_RELATION && rel->rtekind != RTE_SUBQUERY && rel->rtekind != RTE_FUNCTION && rel->rtekind != RTE_TABLEFUNC && rel->rtekind != RTE_VALUES && rel->rtekind != RTE_CTE)
  {
    return false;
  }

     
                                                                      
                                                            
                                                                          
     
  if (rel->reloptkind != RELOPT_BASEREL)
  {
    return false;
  }

     
                                                                          
                                                                         
                                                                             
                           
     
  if (IsA(path, CustomPath))
  {
    return false;
  }

     
                                                                           
                                                                         
                                                      
     
  if (IsA(path, BitmapHeapPath) && path->pathtarget->exprs == NIL)
  {
    return false;
  }

     
                                                                        
                                                                           
                             
     
  for (i = rel->min_attr; i <= 0; i++)
  {
    if (!bms_is_empty(rel->attr_needed[i - rel->min_attr]))
    {
      return false;
    }
  }

     
                                                                             
             
     
  foreach (lc, root->placeholder_list)
  {
    PlaceHolderInfo *phinfo = (PlaceHolderInfo *)lfirst(lc);

    if (bms_nonempty_difference(phinfo->ph_needed, rel->relids) && bms_is_subset(phinfo->ph_eval_at, rel->relids))
    {
      return false;
    }
  }

     
                                                                             
                                                                             
                     
     
  if (path->pathtype == T_IndexOnlyScan)
  {
    IndexOptInfo *indexinfo = ((IndexPath *)path)->indexinfo;

    for (i = 0; i < indexinfo->ncolumns; i++)
    {
      if (!indexinfo->canreturn[i])
      {
        return false;
      }
    }
  }

     
                                                                            
                                                                            
                                                                
                                                                       
                                                                          
                                                                          
                                         
     
  if ((flags & CP_LABEL_TLIST) && path->pathtarget->sortgrouprefs)
  {
    Bitmapset *sortgroupatts = NULL;

    i = 0;
    foreach (lc, path->pathtarget->exprs)
    {
      Expr *expr = (Expr *)lfirst(lc);

      if (path->pathtarget->sortgrouprefs[i])
      {
        if (expr && IsA(expr, Var))
        {
          int attno = ((Var *)expr)->varattno;

          attno -= FirstLowInvalidHeapAttributeNumber;
          if (bms_is_member(attno, sortgroupatts))
          {
            return false;
          }
          sortgroupatts = bms_add_member(sortgroupatts, attno);
        }
        else
        {
          return false;
        }
      }
      i++;
    }
  }

  return true;
}

   
                    
                                                                  
   
                                                               
                            
   
static List *
get_gating_quals(PlannerInfo *root, List *quals)
{
                                                               
  if (!root->hasPseudoConstantQuals)
  {
    return NIL;
  }

                                                                            
  quals = order_qual_clauses(root, quals);

                                                                    
  return extract_actual_clauses(quals, true);
}

   
                      
                                           
   
                                                         
   
static Plan *
create_gating_plan(PlannerInfo *root, Path *path, Plan *plan, List *gating_quals)
{
  Plan *gplan;
  Plan *splan;

  Assert(gating_quals);

     
                                                                            
                                                                        
                                                                          
                                                                           
     
  splan = plan;
  if (IsA(plan, Result))
  {
    Result *rplan = (Result *)plan;

    if (rplan->plan.lefttree == NULL && rplan->resconstantqual == NULL)
    {
      splan = NULL;
    }
  }

     
                                                                            
                                                                            
                         
     
  gplan = (Plan *)make_result(build_path_tlist(root, path), (Node *)gating_quals, splan);

     
                                                                           
                                                                         
                                                                          
                                                                             
                                                                           
                                                                            
                                                                             
                                                                           
                                                                          
                             
     
  copy_plan_costsize(gplan, plan);

                                                                          
  gplan->parallel_safe = path->parallel_safe;

  return gplan;
}

   
                    
                                                                        
                            
   
static Plan *
create_join_plan(PlannerInfo *root, JoinPath *best_path)
{
  Plan *plan;
  List *gating_clauses;

  switch (best_path->path.pathtype)
  {
  case T_MergeJoin:
    plan = (Plan *)create_mergejoin_plan(root, (MergePath *)best_path);
    break;
  case T_HashJoin:
    plan = (Plan *)create_hashjoin_plan(root, (HashPath *)best_path);
    break;
  case T_NestLoop:
    plan = (Plan *)create_nestloop_plan(root, (NestPath *)best_path);
    break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)best_path->path.pathtype);
    plan = NULL;                          
    break;
  }

     
                                                                             
                                                                       
            
     
  gating_clauses = get_gating_quals(root, best_path->joinrestrictinfo);
  if (gating_clauses)
  {
    plan = create_gating_plan(root, (Path *)best_path, plan, gating_clauses);
  }

#ifdef NOT_USED

     
                                                                          
                                                                      
             
     
  if (get_loc_restrictinfo(best_path) != NIL)
  {
    set_qpqual((Plan)plan, list_concat(get_qpqual((Plan)plan), get_actual_clauses(get_loc_restrictinfo(best_path))));
  }
#endif

  return plan;
}

   
                      
                                                                   
                       
   
                          
   
static Plan *
create_append_plan(PlannerInfo *root, AppendPath *best_path, int flags)
{
  Append *plan;
  List *tlist = build_path_tlist(root, &best_path->path);
  int orig_tlist_length = list_length(tlist);
  bool tlist_was_changed = false;
  List *pathkeys = best_path->path.pathkeys;
  List *subplans = NIL;
  ListCell *subpaths;
  RelOptInfo *rel = best_path->path.parent;
  PartitionPruneInfo *partpruneinfo = NULL;
  int nodenumsortkeys = 0;
  AttrNumber *nodeSortColIdx = NULL;
  Oid *nodeSortOperators = NULL;
  Oid *nodeCollations = NULL;
  bool *nodeNullsFirst = NULL;

     
                                                                          
                                                                            
              
     
                                                                          
                                                                          
                                                                        
     
  if (best_path->subpaths == NIL)
  {
                                                                
    Plan *plan;

    plan = (Plan *)make_result(tlist, (Node *)list_make1(makeBoolConst(false, false)), NULL);

    copy_generic_path_info(plan, (Path *)best_path);

    return plan;
  }

     
                                                                           
                                                                            
                                                                         
                                                                  
     
                                                                           
                                                                 
                                                                        
                                                               
     
  plan = makeNode(Append);
  plan->plan.targetlist = tlist;
  plan->plan.qual = NIL;
  plan->plan.lefttree = NULL;
  plan->plan.righttree = NULL;

  if (pathkeys != NIL)
  {
       
                                                                          
                                                                       
                                                                         
                                                            
       
    (void)prepare_sort_from_pathkeys((Plan *)plan, pathkeys, best_path->path.parent->relids, NULL, true, &nodenumsortkeys, &nodeSortColIdx, &nodeSortOperators, &nodeCollations, &nodeNullsFirst);
    tlist_was_changed = (orig_tlist_length != list_length(plan->plan.targetlist));
  }

                                     
  foreach (subpaths, best_path->subpaths)
  {
    Path *subpath = (Path *)lfirst(subpaths);
    Plan *subplan;

                                                             
    subplan = create_plan_recurse(root, subpath, CP_EXACT_TLIST);

       
                                                                        
                             
       
    if (pathkeys != NIL)
    {
      int numsortkeys;
      AttrNumber *sortColIdx;
      Oid *sortOperators;
      Oid *collations;
      bool *nullsFirst;

         
                                                                         
                                                                        
                                                                      
                                                       
         
      subplan = prepare_sort_from_pathkeys(subplan, pathkeys, subpath->parent->relids, nodeSortColIdx, false, &numsortkeys, &sortColIdx, &sortOperators, &collations, &nullsFirst);

         
                                                                   
                                                                       
                                                                   
                                                                   
         
      Assert(numsortkeys == nodenumsortkeys);
      if (memcmp(sortColIdx, nodeSortColIdx, numsortkeys * sizeof(AttrNumber)) != 0)
      {
        elog(ERROR, "Append child's targetlist doesn't match Append");
      }
      Assert(memcmp(sortOperators, nodeSortOperators, numsortkeys * sizeof(Oid)) == 0);
      Assert(memcmp(collations, nodeCollations, numsortkeys * sizeof(Oid)) == 0);
      Assert(memcmp(nullsFirst, nodeNullsFirst, numsortkeys * sizeof(bool)) == 0);

                                                                         
      if (!pathkeys_contained_in(pathkeys, subpath->pathkeys))
      {
        Sort *sort = make_sort(subplan, numsortkeys, sortColIdx, sortOperators, collations, nullsFirst);

        label_sort_with_costsize(root, sort, best_path->limit_tuples);
        subplan = (Plan *)sort;
      }
    }

    subplans = lappend(subplans, subplan);
  }

     
                                                                         
                                                                             
                           
     
  if (enable_partition_pruning && rel->reloptkind == RELOPT_BASEREL && best_path->partitioned_rels != NIL)
  {
    List *prunequal;

    prunequal = extract_actual_clauses(rel->baserestrictinfo, false);

    if (best_path->path.param_info)
    {
      List *prmquals = best_path->path.param_info->ppi_clauses;

      prmquals = extract_actual_clauses(prmquals, false);
      prmquals = (List *)replace_nestloop_params(root, (Node *)prmquals);

      prunequal = list_concat(prunequal, prmquals);
    }

    if (prunequal != NIL)
    {
      partpruneinfo = make_partition_pruneinfo(root, rel, best_path->subpaths, best_path->partitioned_rels, prunequal);
    }
  }

  plan->appendplans = subplans;
  plan->first_partial_plan = best_path->first_partial_path;
  plan->part_prune_info = partpruneinfo;

  copy_generic_path_info(&plan->plan, (Path *)best_path);

     
                                                                           
                                                                            
                                                                         
     
  if (tlist_was_changed && (flags & (CP_EXACT_TLIST | CP_SMALL_TLIST)))
  {
    tlist = list_truncate(list_copy(plan->plan.targetlist), orig_tlist_length);
    return inject_projection_plan((Plan *)plan, tlist, plan->plan.parallel_safe);
  }
  else
  {
    return (Plan *)plan;
  }
}

   
                            
                                                                       
                       
   
                          
   
static Plan *
create_merge_append_plan(PlannerInfo *root, MergeAppendPath *best_path, int flags)
{
  MergeAppend *node = makeNode(MergeAppend);
  Plan *plan = &node->plan;
  List *tlist = build_path_tlist(root, &best_path->path);
  int orig_tlist_length = list_length(tlist);
  bool tlist_was_changed;
  List *pathkeys = best_path->path.pathkeys;
  List *subplans = NIL;
  ListCell *subpaths;
  RelOptInfo *rel = best_path->path.parent;
  PartitionPruneInfo *partpruneinfo = NULL;

     
                                                                         
                                                                        
                                                                        
                                                               
     
  copy_generic_path_info(plan, (Path *)best_path);
  plan->targetlist = tlist;
  plan->qual = NIL;
  plan->lefttree = NULL;
  plan->righttree = NULL;

     
                                                                         
                                                                     
                                                                            
                                                     
     
  (void)prepare_sort_from_pathkeys(plan, pathkeys, best_path->path.parent->relids, NULL, true, &node->numCols, &node->sortColIdx, &node->sortOperators, &node->collations, &node->nullsFirst);
  tlist_was_changed = (orig_tlist_length != list_length(plan->targetlist));

     
                                                                            
                                                                          
                                                                      
     
  foreach (subpaths, best_path->subpaths)
  {
    Path *subpath = (Path *)lfirst(subpaths);
    Plan *subplan;
    int numsortkeys;
    AttrNumber *sortColIdx;
    Oid *sortOperators;
    Oid *collations;
    bool *nullsFirst;

                              
                                                             
    subplan = create_plan_recurse(root, subpath, CP_EXACT_TLIST);

                                                                        
    subplan = prepare_sort_from_pathkeys(subplan, pathkeys, subpath->parent->relids, node->sortColIdx, false, &numsortkeys, &sortColIdx, &sortOperators, &collations, &nullsFirst);

       
                                                                        
                                                                        
                                                                      
                                                            
       
    Assert(numsortkeys == node->numCols);
    if (memcmp(sortColIdx, node->sortColIdx, numsortkeys * sizeof(AttrNumber)) != 0)
    {
      elog(ERROR, "MergeAppend child's targetlist doesn't match MergeAppend");
    }
    Assert(memcmp(sortOperators, node->sortOperators, numsortkeys * sizeof(Oid)) == 0);
    Assert(memcmp(collations, node->collations, numsortkeys * sizeof(Oid)) == 0);
    Assert(memcmp(nullsFirst, node->nullsFirst, numsortkeys * sizeof(bool)) == 0);

                                                                       
    if (!pathkeys_contained_in(pathkeys, subpath->pathkeys))
    {
      Sort *sort = make_sort(subplan, numsortkeys, sortColIdx, sortOperators, collations, nullsFirst);

      label_sort_with_costsize(root, sort, best_path->limit_tuples);
      subplan = (Plan *)sort;
    }

    subplans = lappend(subplans, subplan);
  }

     
                                                                         
                                                                             
                           
     
  if (enable_partition_pruning && rel->reloptkind == RELOPT_BASEREL && best_path->partitioned_rels != NIL)
  {
    List *prunequal;

    prunequal = extract_actual_clauses(rel->baserestrictinfo, false);

    if (best_path->path.param_info)
    {
      List *prmquals = best_path->path.param_info->ppi_clauses;

      prmquals = extract_actual_clauses(prmquals, false);
      prmquals = (List *)replace_nestloop_params(root, (Node *)prmquals);

      prunequal = list_concat(prunequal, prmquals);
    }

    if (prunequal != NIL)
    {
      partpruneinfo = make_partition_pruneinfo(root, rel, best_path->subpaths, best_path->partitioned_rels, prunequal);
    }
  }

  node->mergeplans = subplans;
  node->part_prune_info = partpruneinfo;

     
                                                                           
                                                                            
                                                                         
     
  if (tlist_was_changed && (flags & (CP_EXACT_TLIST | CP_SMALL_TLIST)))
  {
    tlist = list_truncate(list_copy(plan->targetlist), orig_tlist_length);
    return inject_projection_plan(plan, tlist, plan->parallel_safe);
  }
  else
  {
    return plan;
  }
}

   
                            
                                           
                                                      
   
                          
   
static Result *
create_group_result_plan(PlannerInfo *root, GroupResultPath *best_path)
{
  Result *plan;
  List *tlist;
  List *quals;

  tlist = build_path_tlist(root, &best_path->path);

                                             
  quals = order_qual_clauses(root, best_path->quals);

  plan = make_result(tlist, (Node *)quals, NULL);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                           
                                               
   
                          
   
static ProjectSet *
create_project_set_plan(PlannerInfo *root, ProjectSetPath *best_path)
{
  ProjectSet *plan;
  Plan *subplan;
  List *tlist;

                                                                          
  subplan = create_plan_recurse(root, best_path->subpath, 0);

  tlist = build_path_tlist(root, &best_path->path);

  plan = make_project_set(tlist, subplan);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                        
                                                                    
                       
   
                          
   
static Material *
create_material_plan(PlannerInfo *root, MaterialPath *best_path, int flags)
{
  Material *plan;
  Plan *subplan;

     
                                                                             
                                                                        
                                
     
  subplan = create_plan_recurse(root, best_path->subpath, flags | CP_SMALL_TLIST);

  plan = make_material(subplan);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                      
                                                                  
                       
   
                          
   
static Plan *
create_unique_plan(PlannerInfo *root, UniquePath *best_path, int flags)
{
  Plan *plan;
  Plan *subplan;
  List *in_operators;
  List *uniq_exprs;
  List *newtlist;
  int nextresno;
  bool newitems;
  int numGroupCols;
  AttrNumber *groupColIdx;
  Oid *groupCollations;
  int groupColPos;
  ListCell *l;

                                                                  
  subplan = create_plan_recurse(root, best_path->subpath, flags);

                                                            
  if (best_path->umethod == UNIQUE_PATH_NOOP)
  {
    return subplan;
  }

     
                                                                             
                                                                     
                                                                           
                                              
     
                                                                             
                                                                          
                                                                           
                                                                             
                                                                          
                                                                        
                                                                    
                                                                             
                                       
     
  in_operators = best_path->in_operators;
  uniq_exprs = best_path->uniq_exprs;

                                                                     
  newtlist = build_path_tlist(root, &best_path->path);
  nextresno = list_length(newtlist) + 1;
  newitems = false;

  foreach (l, uniq_exprs)
  {
    Expr *uniqexpr = lfirst(l);
    TargetEntry *tle;

    tle = tlist_member(uniqexpr, newtlist);
    if (!tle)
    {
      tle = makeTargetEntry((Expr *)uniqexpr, nextresno, NULL, false);
      newtlist = lappend(newtlist, tle);
      nextresno++;
      newitems = true;
    }
  }

                                                                          
  if (newitems || best_path->umethod == UNIQUE_PATH_SORT)
  {
    subplan = change_plan_targetlist(subplan, newtlist, best_path->path.parallel_safe);
  }

     
                                                                           
                                                                          
                                                                            
                                      
     
  newtlist = subplan->targetlist;
  numGroupCols = list_length(uniq_exprs);
  groupColIdx = (AttrNumber *)palloc(numGroupCols * sizeof(AttrNumber));
  groupCollations = (Oid *)palloc(numGroupCols * sizeof(Oid));

  groupColPos = 0;
  foreach (l, uniq_exprs)
  {
    Expr *uniqexpr = lfirst(l);
    TargetEntry *tle;

    tle = tlist_member(uniqexpr, newtlist);
    if (!tle)                       
    {
      elog(ERROR, "failed to find unique expression in subplan tlist");
    }
    groupColIdx[groupColPos] = tle->resno;
    groupCollations[groupColPos] = exprCollation((Node *)tle->expr);
    groupColPos++;
  }

  if (best_path->umethod == UNIQUE_PATH_HASH)
  {
    Oid *groupOperators;

       
                                                                    
                                                                      
                                                                          
                                                       
       
    groupOperators = (Oid *)palloc(numGroupCols * sizeof(Oid));
    groupColPos = 0;
    foreach (l, in_operators)
    {
      Oid in_oper = lfirst_oid(l);
      Oid eq_oper;

      if (!get_compatible_hash_operators(in_oper, NULL, &eq_oper))
      {
        elog(ERROR, "could not find compatible hash operator for operator %u", in_oper);
      }
      groupOperators[groupColPos++] = eq_oper;
    }

       
                                                                         
                                                                          
                      
       
    plan = (Plan *)make_agg(build_path_tlist(root, &best_path->path), NIL, AGG_HASHED, AGGSPLIT_SIMPLE, numGroupCols, groupColIdx, groupOperators, groupCollations, NIL, NIL, best_path->path.rows, subplan);
  }
  else
  {
    List *sortList = NIL;
    Sort *sort;

                                                              
    groupColPos = 0;
    foreach (l, in_operators)
    {
      Oid in_oper = lfirst_oid(l);
      Oid sortop;
      Oid eqop;
      TargetEntry *tle;
      SortGroupClause *sortcl;

      sortop = get_ordering_op_for_equality_op(in_oper, false);
      if (!OidIsValid(sortop))                       
      {
        elog(ERROR, "could not find ordering operator for equality operator %u", in_oper);
      }

         
                                                                       
                                                                   
                                                                       
                                                    
         
      eqop = get_equality_op_for_ordering_op(sortop, NULL);
      if (!OidIsValid(eqop))                       
      {
        elog(ERROR, "could not find equality operator for ordering operator %u", sortop);
      }

      tle = get_tle_by_resno(subplan->targetlist, groupColIdx[groupColPos]);
      Assert(tle != NULL);

      sortcl = makeNode(SortGroupClause);
      sortcl->tleSortGroupRef = assignSortGroupRef(tle, subplan->targetlist);
      sortcl->eqop = eqop;
      sortcl->sortop = sortop;
      sortcl->nulls_first = false;
      sortcl->hashable = false;                                    
      sortList = lappend(sortList, sortcl);
      groupColPos++;
    }
    sort = make_sort_from_sortclauses(sortList, subplan);
    label_sort_with_costsize(root, sort, -1.0);
    plan = (Plan *)make_unique_from_sortclauses((Plan *)sort, sortList);
  }

                                        
  copy_generic_path_info(plan, &best_path->path);

  return plan;
}

   
                      
   
                                                                  
                       
   
static Gather *
create_gather_plan(PlannerInfo *root, GatherPath *best_path)
{
  Gather *gather_plan;
  Plan *subplan;
  List *tlist;

     
                                                                            
                                                                 
     
  subplan = create_plan_recurse(root, best_path->subpath, CP_EXACT_TLIST);

  tlist = build_path_tlist(root, &best_path->path);

  gather_plan = make_gather(tlist, NIL, best_path->num_workers, assign_special_exec_param(root), best_path->single_copy, subplan);

  copy_generic_path_info(&gather_plan->plan, &best_path->path);

                                             
  root->glob->parallelModeNeeded = true;

  return gather_plan;
}

   
                            
   
                                                                  
                             
   
static GatherMerge *
create_gather_merge_plan(PlannerInfo *root, GatherMergePath *best_path)
{
  GatherMerge *gm_plan;
  Plan *subplan;
  List *pathkeys = best_path->path.pathkeys;
  List *tlist = build_path_tlist(root, &best_path->path);

                                                                         
  subplan = create_plan_recurse(root, best_path->subpath, CP_EXACT_TLIST);

                                              
  gm_plan = makeNode(GatherMerge);
  gm_plan->plan.targetlist = tlist;
  gm_plan->num_workers = best_path->num_workers;
  copy_generic_path_info(&gm_plan->plan, &best_path->path);

                                
  gm_plan->rescan_param = assign_special_exec_param(root);

                                                                       
  Assert(pathkeys != NIL);

                                                                      
  subplan = prepare_sort_from_pathkeys(subplan, pathkeys, best_path->subpath->parent->relids, gm_plan->sortColIdx, false, &gm_plan->numCols, &gm_plan->sortColIdx, &gm_plan->sortOperators, &gm_plan->collations, &gm_plan->nullsFirst);

                                                                     
  if (!pathkeys_contained_in(pathkeys, best_path->subpath->pathkeys))
  {
    subplan = (Plan *)make_sort(subplan, gm_plan->numCols, gm_plan->sortColIdx, gm_plan->sortOperators, gm_plan->collations, gm_plan->nullsFirst);
  }

                                                 
  gm_plan->plan.lefttree = subplan;

                                             
  root->glob->parallelModeNeeded = true;

  return gm_plan;
}

   
                          
   
                                                                        
                                                                      
                                                            
   
static Plan *
create_projection_plan(PlannerInfo *root, ProjectionPath *best_path, int flags)
{
  Plan *plan;
  Plan *subplan;
  List *tlist;
  bool needs_result_node = false;

     
                                                                          
           
     
                                                                            
                                                                      
                                                                     
                                                                  
                                                                         
                                                                     
                                                                         
                                                     
     
  if (use_physical_tlist(root, &best_path->path, flags))
  {
       
                                                                        
                                                                       
                                                                    
       
    subplan = create_plan_recurse(root, best_path->subpath, 0);
    tlist = subplan->targetlist;
    if (flags & CP_LABEL_TLIST)
    {
      apply_pathtarget_labeling_to_tlist(tlist, best_path->path.pathtarget);
    }
  }
  else if (is_projection_capable_path(best_path->subpath))
  {
       
                                                                           
                                                                        
                                                                        
                 
       
    subplan = create_plan_recurse(root, best_path->subpath, CP_IGNORE_TLIST);
    Assert(is_projection_capable_plan(subplan));
    tlist = build_path_tlist(root, &best_path->path);
  }
  else
  {
       
                                                                       
                                                                      
       
    subplan = create_plan_recurse(root, best_path->subpath, 0);
    tlist = build_path_tlist(root, &best_path->path);
    needs_result_node = !tlist_same_exprs(tlist, subplan->targetlist);
  }

     
                                                                            
                                                                          
                                                                             
                                                                           
                                                                          
                                                           
     
  if (!needs_result_node)
  {
                                                                    
    plan = subplan;
    plan->targetlist = tlist;

                                                              
    plan->startup_cost = best_path->path.startup_cost;
    plan->total_cost = best_path->path.total_cost;
    plan->plan_rows = best_path->path.rows;
    plan->plan_width = best_path->path.pathtarget->width;
    plan->parallel_safe = best_path->path.parallel_safe;
                                                            
  }
  else
  {
                               
    plan = (Plan *)make_result(tlist, NULL, subplan);

    copy_generic_path_info(plan, (Path *)best_path);
  }

  return plan;
}

   
                          
                                                   
   
                                                                          
                                                                     
                                                                        
   
                                                                              
                                                                              
   
static Plan *
inject_projection_plan(Plan *subplan, List *tlist, bool parallel_safe)
{
  Plan *plan;

  plan = (Plan *)make_result(tlist, NULL, subplan);

     
                                                                           
                                                                            
                                                                            
                                                                            
                                                                   
     
  copy_plan_costsize(plan, subplan);
  plan->parallel_safe = parallel_safe;

  return plan;
}

   
                          
                                                              
   
                                                                       
                                                                        
                                                                         
               
   
                                                                              
                                    
   
Plan *
change_plan_targetlist(Plan *subplan, List *tlist, bool tlist_parallel_safe)
{
     
                                                                            
                                                                         
            
     
  if (!is_projection_capable_plan(subplan) && !tlist_same_exprs(tlist, subplan->targetlist))
  {
    subplan = inject_projection_plan(subplan, tlist, subplan->parallel_safe && tlist_parallel_safe);
  }
  else
  {
                                                        
    subplan->targetlist = tlist;
    subplan->parallel_safe &= tlist_parallel_safe;
  }
  return subplan;
}

   
                    
   
                                                                
                       
   
static Sort *
create_sort_plan(PlannerInfo *root, SortPath *best_path, int flags)
{
  Sort *plan;
  Plan *subplan;

     
                                                                         
                                                                  
                                
     
  subplan = create_plan_recurse(root, best_path->subpath, flags | CP_SMALL_TLIST);

     
                                                                          
                                                                           
                                                                           
                      
     
  plan = make_sort_from_pathkeys(subplan, best_path->path.pathkeys, IS_OTHER_REL(best_path->subpath->parent) ? best_path->path.parent->relids : NULL);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                     
   
                                                                 
                       
   
static Group *
create_group_plan(PlannerInfo *root, GroupPath *best_path)
{
  Group *plan;
  Plan *subplan;
  List *tlist;
  List *quals;

     
                                                                           
                                                     
     
  subplan = create_plan_recurse(root, best_path->subpath, CP_LABEL_TLIST);

  tlist = build_path_tlist(root, &best_path->path);

  quals = order_qual_clauses(root, best_path->qual);

  plan = make_group(tlist, quals, list_length(best_path->groupClause), extract_grouping_cols(best_path->groupClause, subplan->targetlist), extract_grouping_ops(best_path->groupClause), extract_grouping_collations(best_path->groupClause, subplan->targetlist), subplan);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                            
   
                                                                  
                       
   
static Unique *
create_upper_unique_plan(PlannerInfo *root, UpperUniquePath *best_path, int flags)
{
  Unique *plan;
  Plan *subplan;

     
                                                                             
                                          
     
  subplan = create_plan_recurse(root, best_path->subpath, flags | CP_LABEL_TLIST);

  plan = make_unique_from_pathkeys(subplan, best_path->path.pathkeys, best_path->numkeys);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                   
   
                                                                
                       
   
static Agg *
create_agg_plan(PlannerInfo *root, AggPath *best_path)
{
  Agg *plan;
  Plan *subplan;
  List *tlist;
  List *quals;

     
                                                                             
                                                 
     
  subplan = create_plan_recurse(root, best_path->subpath, CP_LABEL_TLIST);

  tlist = build_path_tlist(root, &best_path->path);

  quals = order_qual_clauses(root, best_path->qual);

  plan = make_agg(tlist, quals, best_path->aggstrategy, best_path->aggsplit, list_length(best_path->groupClause), extract_grouping_cols(best_path->groupClause, subplan->targetlist), extract_grouping_ops(best_path->groupClause), extract_grouping_collations(best_path->groupClause, subplan->targetlist), NIL, NIL, best_path->numGroups, subplan);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                                                                      
                              
   
                                                                                
                                                                              
                       
   
static AttrNumber *
remap_groupColIdx(PlannerInfo *root, List *groupClause)
{
  AttrNumber *grouping_map = root->grouping_map;
  AttrNumber *new_grpColIdx;
  ListCell *lc;
  int i;

  Assert(grouping_map);

  new_grpColIdx = palloc0(sizeof(AttrNumber) * list_length(groupClause));

  i = 0;
  foreach (lc, groupClause)
  {
    SortGroupClause *clause = lfirst(lc);

    new_grpColIdx[i++] = grouping_map[clause->tleSortGroupRef];
  }

  return new_grpColIdx;
}

   
                            
                                                           
                       
   
                                                                        
                                                                         
                                                                         
                                                                       
                                                                        
                                                                      
                      
   
                          
   
static Plan *
create_groupingsets_plan(PlannerInfo *root, GroupingSetsPath *best_path)
{
  Agg *plan;
  Plan *subplan;
  List *rollups = best_path->rollups;
  AttrNumber *grouping_map;
  int maxref;
  List *chain;
  ListCell *lc;

                                                
  Assert(root->parse->groupingSets);
  Assert(rollups != NIL);

     
                                                                             
                                                 
     
  subplan = create_plan_recurse(root, best_path->subpath, CP_LABEL_TLIST);

     
                                                                             
                                                                        
             
     
  maxref = 0;
  foreach (lc, root->parse->groupClause)
  {
    SortGroupClause *gc = (SortGroupClause *)lfirst(lc);

    if (gc->tleSortGroupRef > maxref)
    {
      maxref = gc->tleSortGroupRef;
    }
  }

  grouping_map = (AttrNumber *)palloc0((maxref + 1) * sizeof(AttrNumber));

                                                           
  foreach (lc, root->parse->groupClause)
  {
    SortGroupClause *gc = (SortGroupClause *)lfirst(lc);
    TargetEntry *tle = get_sortgroupclause_tle(gc, subplan->targetlist);

    grouping_map[gc->tleSortGroupRef] = tle->resno;
  }

     
                                                                            
                                                           
     
                                                                        
                                                                            
                                                                   
     
  Assert(root->inhTargetKind == INHKIND_NONE);
  Assert(root->grouping_map == NULL);
  root->grouping_map = grouping_map;

     
                                                                    
                                                                             
                                                                            
                                     
     
  chain = NIL;
  if (list_length(rollups) > 1)
  {
    ListCell *lc2 = lnext(list_head(rollups));
    bool is_first_sort = ((RollupData *)linitial(rollups))->is_hashed;

    for_each_cell(lc, lc2)
    {
      RollupData *rollup = lfirst(lc);
      AttrNumber *new_grpColIdx;
      Plan *sort_plan = NULL;
      Plan *agg_plan;
      AggStrategy strat;

      new_grpColIdx = remap_groupColIdx(root, rollup->groupClause);

      if (!rollup->is_hashed && !is_first_sort)
      {
        sort_plan = (Plan *)make_sort_from_groupcols(rollup->groupClause, new_grpColIdx, subplan);
      }

      if (!rollup->is_hashed)
      {
        is_first_sort = false;
      }

      if (rollup->is_hashed)
      {
        strat = AGG_HASHED;
      }
      else if (list_length(linitial(rollup->gsets)) == 0)
      {
        strat = AGG_PLAIN;
      }
      else
      {
        strat = AGG_SORTED;
      }

      agg_plan = (Plan *)make_agg(NIL, NIL, strat, AGGSPLIT_SIMPLE, list_length((List *)linitial(rollup->gsets)), new_grpColIdx, extract_grouping_ops(rollup->groupClause), extract_grouping_collations(rollup->groupClause, subplan->targetlist), rollup->gsets, NIL, rollup->numGroups, sort_plan);

         
                                                                    
         
      if (sort_plan)
      {
        sort_plan->targetlist = NIL;
        sort_plan->lefttree = NULL;
      }

      chain = lappend(chain, agg_plan);
    }
  }

     
                                
     
  {
    RollupData *rollup = linitial(rollups);
    AttrNumber *top_grpColIdx;
    int numGroupCols;

    top_grpColIdx = remap_groupColIdx(root, rollup->groupClause);

    numGroupCols = list_length((List *)linitial(rollup->gsets));

    plan = make_agg(build_path_tlist(root, &best_path->path), best_path->qual, best_path->aggstrategy, AGGSPLIT_SIMPLE, numGroupCols, top_grpColIdx, extract_grouping_ops(rollup->groupClause), extract_grouping_collations(rollup->groupClause, subplan->targetlist), rollup->gsets, chain, rollup->numGroups, subplan);

                                          
    copy_generic_path_info(&plan->plan, &best_path->path);
  }

  return (Plan *)plan;
}

   
                         
   
                                                                  
                       
   
static Result *
create_minmaxagg_plan(PlannerInfo *root, MinMaxAggPath *best_path)
{
  Result *plan;
  List *tlist;
  ListCell *lc;

                                                          
  foreach (lc, best_path->mmaggregates)
  {
    MinMaxAggInfo *mminfo = (MinMaxAggInfo *)lfirst(lc);
    PlannerInfo *subroot = mminfo->subroot;
    Query *subparse = subroot->parse;
    Plan *plan;

       
                                                                          
                                                                      
                                                                    
                                                       
       
    plan = create_plan(subroot, mminfo->path);

    plan = (Plan *)make_limit(plan, subparse->limitOffset, subparse->limitCount);

                                                          
    plan->startup_cost = mminfo->path->startup_cost;
    plan->total_cost = mminfo->pathcost;
    plan->plan_rows = 1;
    plan->plan_width = mminfo->path->pathtarget->width;
    plan->parallel_aware = false;
    plan->parallel_safe = mminfo->path->parallel_safe;

                                                               
    SS_make_initplan_from_plan(root, subroot, plan, mminfo->param);
  }

                                                            
  tlist = build_path_tlist(root, &best_path->path);

  plan = make_result(tlist, (Node *)best_path->quals, NULL);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

     
                                                                         
                                                                         
                                                                           
                                                                         
     
                                                                        
                                                                            
                                                                     
     
  Assert(root->inhTargetKind == INHKIND_NONE);
  Assert(root->minmax_aggs == NIL);
  root->minmax_aggs = best_path->mmaggregates;

  return plan;
}

   
                         
   
                                                                     
                       
   
static WindowAgg *
create_windowagg_plan(PlannerInfo *root, WindowAggPath *best_path)
{
  WindowAgg *plan;
  WindowClause *wc = best_path->winclause;
  int numPart = list_length(wc->partitionClause);
  int numOrder = list_length(wc->orderClause);
  Plan *subplan;
  List *tlist;
  int partNumCols;
  AttrNumber *partColIdx;
  Oid *partOperators;
  Oid *partCollations;
  int ordNumCols;
  AttrNumber *ordColIdx;
  Oid *ordOperators;
  Oid *ordCollations;
  ListCell *lc;

     
                                                                          
                                                                           
                                                                           
                                                   
     
  subplan = create_plan_recurse(root, best_path->subpath, CP_LABEL_TLIST | CP_SMALL_TLIST);

  tlist = build_path_tlist(root, &best_path->path);

     
                                                                            
                                                                           
                                                                        
                                                                            
                                                                          
                                                                            
                                                                        
     
  partColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numPart);
  partOperators = (Oid *)palloc(sizeof(Oid) * numPart);
  partCollations = (Oid *)palloc(sizeof(Oid) * numPart);

  partNumCols = 0;
  foreach (lc, wc->partitionClause)
  {
    SortGroupClause *sgc = (SortGroupClause *)lfirst(lc);
    TargetEntry *tle = get_sortgroupclause_tle(sgc, subplan->targetlist);

    Assert(OidIsValid(sgc->eqop));
    partColIdx[partNumCols] = tle->resno;
    partOperators[partNumCols] = sgc->eqop;
    partCollations[partNumCols] = exprCollation((Node *)tle->expr);
    partNumCols++;
  }

  ordColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numOrder);
  ordOperators = (Oid *)palloc(sizeof(Oid) * numOrder);
  ordCollations = (Oid *)palloc(sizeof(Oid) * numOrder);

  ordNumCols = 0;
  foreach (lc, wc->orderClause)
  {
    SortGroupClause *sgc = (SortGroupClause *)lfirst(lc);
    TargetEntry *tle = get_sortgroupclause_tle(sgc, subplan->targetlist);

    Assert(OidIsValid(sgc->eqop));
    ordColIdx[ordNumCols] = tle->resno;
    ordOperators[ordNumCols] = sgc->eqop;
    ordCollations[ordNumCols] = exprCollation((Node *)tle->expr);
    ordNumCols++;
  }

                                                  
  plan = make_windowagg(tlist, wc->winref, partNumCols, partColIdx, partOperators, partCollations, ordNumCols, ordColIdx, ordOperators, ordCollations, wc->frameOptions, wc->startOffset, wc->endOffset, wc->startInRangeFunc, wc->endInRangeFunc, wc->inRangeColl, wc->inRangeAsc, wc->inRangeNullsFirst, subplan);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                     
   
                                                                 
                       
   
static SetOp *
create_setop_plan(PlannerInfo *root, SetOpPath *best_path, int flags)
{
  SetOp *plan;
  Plan *subplan;
  long numGroups;

     
                                                                            
                                          
     
  subplan = create_plan_recurse(root, best_path->subpath, flags | CP_LABEL_TLIST);

                                                             
  numGroups = (long)Min(best_path->numGroups, (double)LONG_MAX);

  plan = make_setop(best_path->cmd, best_path->strategy, subplan, best_path->distinctList, best_path->flagColIdx, best_path->firstFlag, numGroups);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                              
   
                                                                          
                       
   
static RecursiveUnion *
create_recursiveunion_plan(PlannerInfo *root, RecursiveUnionPath *best_path)
{
  RecursiveUnion *plan;
  Plan *leftplan;
  Plan *rightplan;
  List *tlist;
  long numGroups;

                                                             
  leftplan = create_plan_recurse(root, best_path->leftpath, CP_EXACT_TLIST);
  rightplan = create_plan_recurse(root, best_path->rightpath, CP_EXACT_TLIST);

  tlist = build_path_tlist(root, &best_path->path);

                                                             
  numGroups = (long)Min(best_path->numGroups, (double)LONG_MAX);

  plan = make_recursive_union(tlist, leftplan, rightplan, best_path->wtParam, best_path->distinctList, numGroups);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                        
   
                                                                    
                       
   
static LockRows *
create_lockrows_plan(PlannerInfo *root, LockRowsPath *best_path, int flags)
{
  LockRows *plan;
  Plan *subplan;

                                                                    
  subplan = create_plan_recurse(root, best_path->subpath, flags);

  plan = make_lockrows(subplan, best_path->rowMarks, best_path->epqParam);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

   
                           
                                                
   
                          
   
static ModifyTable *
create_modifytable_plan(PlannerInfo *root, ModifyTablePath *best_path)
{
  ModifyTable *plan;
  List *subplans = NIL;
  ListCell *subpaths, *subroots;

                                          
  forboth(subpaths, best_path->subpaths, subroots, best_path->subroots)
  {
    Path *subpath = (Path *)lfirst(subpaths);
    PlannerInfo *subroot = (PlannerInfo *)lfirst(subroots);
    Plan *subplan;

       
                                                                       
                                                                           
                                                                         
                                                                        
                                                                      
                                                                       
                                                                          
                                                                        
                                                                   
       
    subplan = create_plan_recurse(subroot, subpath, CP_EXACT_TLIST);

                                                                        
    apply_tlist_labeling(subplan->targetlist, subroot->processed_tlist);

    subplans = lappend(subplans, subplan);
  }

  plan = make_modifytable(root, best_path->operation, best_path->canSetTag, best_path->nominalRelation, best_path->rootRelation, best_path->partColsUpdated, best_path->resultRelations, subplans, best_path->subroots, best_path->withCheckOptionLists, best_path->returningLists, best_path->rowMarks, best_path->onconflict, best_path->epqParam);

  copy_generic_path_info(&plan->plan, &best_path->path);

  return plan;
}

   
                     
   
                                                                 
                       
   
static Limit *
create_limit_plan(PlannerInfo *root, LimitPath *best_path, int flags)
{
  Limit *plan;
  Plan *subplan;

                                                                 
  subplan = create_plan_recurse(root, best_path->subpath, flags);

  plan = make_limit(subplan, best_path->limitOffset, best_path->limitCount);

  copy_generic_path_info(&plan->plan, (Path *)best_path);

  return plan;
}

                                                                               
   
                              
   
                                                                               

   
                       
                                                                        
                                                                    
   
static SeqScan *
create_seqscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  SeqScan *scan_plan;
  Index scan_relid = best_path->parent->relid;

                                  
  Assert(scan_relid > 0);
  Assert(best_path->parent->rtekind == RTE_RELATION);

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
  }

  scan_plan = make_seqscan(tlist, scan_clauses, scan_relid);

  copy_generic_path_info(&scan_plan->plan, best_path);

  return scan_plan;
}

   
                          
                                                                           
                                                                    
   
static SampleScan *
create_samplescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  SampleScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;
  TableSampleClause *tsc;

                                                            
  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_RELATION);
  tsc = rte->tablesample;
  Assert(tsc != NULL);

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
    tsc = (TableSampleClause *)replace_nestloop_params(root, (Node *)tsc);
  }

  scan_plan = make_samplescan(tlist, scan_clauses, scan_relid, tsc);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                         
                                                                            
                                                                     
   
                                                                         
                                                                             
                                                                            
                                                                          
   
static Scan *
create_indexscan_plan(PlannerInfo *root, IndexPath *best_path, List *tlist, List *scan_clauses, bool indexonly)
{
  Scan *scan_plan;
  List *indexclauses = best_path->indexclauses;
  List *indexorderbys = best_path->indexorderbys;
  Index baserelid = best_path->path.parent->relid;
  IndexOptInfo *indexinfo = best_path->indexinfo;
  Oid indexoid = indexinfo->indexoid;
  List *qpqual;
  List *stripped_indexquals;
  List *fixed_indexquals;
  List *fixed_indexorderbys;
  List *indexorderbyops = NIL;
  ListCell *l;

                                  
  Assert(baserelid > 0);
  Assert(best_path->path.parent->rtekind == RTE_RELATION);

     
                                                                             
                                                                           
                                                                      
                        
     
  fix_indexqual_references(root, best_path, &stripped_indexquals, &fixed_indexquals);

     
                                                                        
     
  fixed_indexorderbys = fix_indexorderby_references(root, best_path);

     
                                                                             
                                                                           
                                                                           
                                                                          
                                                                         
                                                                 
                                                        
     
                                                                           
                                                                    
                                                                           
                                                                          
                                                                           
                                                                            
                                                                        
                                                         
     
                                                                         
                                                                            
                                                                           
                                                                          
                                                                        
            
     
                                                                  
                                                  
     
  qpqual = NIL;
  foreach (l, scan_clauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

    if (rinfo->pseudoconstant)
    {
      continue;                                       
    }
    if (is_redundant_with_indexclauses(rinfo, indexclauses))
    {
      continue;                                                
    }
    if (!contain_mutable_functions((Node *)rinfo->clause) && predicate_implied_by(list_make1(rinfo->clause), stripped_indexquals, false))
    {
      continue;                                     
    }
    qpqual = lappend(qpqual, rinfo);
  }

                                              
  qpqual = order_qual_clauses(root, qpqual);

                                                                            
  qpqual = extract_actual_clauses(qpqual, false);

     
                                                                             
                                                                         
                                                                   
                                                                           
                                                                          
                                                                             
                               
     
  if (best_path->path.param_info)
  {
    stripped_indexquals = (List *)replace_nestloop_params(root, (Node *)stripped_indexquals);
    qpqual = (List *)replace_nestloop_params(root, (Node *)qpqual);
    indexorderbys = (List *)replace_nestloop_params(root, (Node *)indexorderbys);
  }

     
                                                                             
                       
     
  if (indexorderbys)
  {
    ListCell *pathkeyCell, *exprCell;

       
                                                                        
                                                                         
                                                            
       
    Assert(list_length(best_path->path.pathkeys) == list_length(indexorderbys));
    forboth(pathkeyCell, best_path->path.pathkeys, exprCell, indexorderbys)
    {
      PathKey *pathkey = (PathKey *)lfirst(pathkeyCell);
      Node *expr = (Node *)lfirst(exprCell);
      Oid exprtype = exprType(expr);
      Oid sortop;

                                           
      sortop = get_opfamily_member(pathkey->pk_opfamily, exprtype, exprtype, pathkey->pk_strategy);
      if (!OidIsValid(sortop))
      {
        elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", pathkey->pk_strategy, exprtype, exprtype, pathkey->pk_opfamily);
      }
      indexorderbyops = lappend_oid(indexorderbyops, sortop);
    }
  }

     
                                                                           
                                                                             
                                               
     
  if (indexonly)
  {
    int i = 0;

    foreach (l, indexinfo->indextlist)
    {
      TargetEntry *indextle = (TargetEntry *)lfirst(l);

      indextle->resjunk = !indexinfo->canreturn[i];
      i++;
    }
  }

                                            
  if (indexonly)
  {
    scan_plan = (Scan *)make_indexonlyscan(tlist, qpqual, baserelid, indexoid, fixed_indexquals, stripped_indexquals, fixed_indexorderbys, indexinfo->indextlist, best_path->indexscandir);
  }
  else
  {
    scan_plan = (Scan *)make_indexscan(tlist, qpqual, baserelid, indexoid, fixed_indexquals, stripped_indexquals, fixed_indexorderbys, indexorderbys, indexorderbyops, best_path->indexscandir);
  }

  copy_generic_path_info(&scan_plan->plan, &best_path->path);

  return scan_plan;
}

   
                           
                                                                             
                                                                     
   
static BitmapHeapScan *
create_bitmap_scan_plan(PlannerInfo *root, BitmapHeapPath *best_path, List *tlist, List *scan_clauses)
{
  Index baserelid = best_path->path.parent->relid;
  Plan *bitmapqualplan;
  List *bitmapqualorig;
  List *indexquals;
  List *indexECs;
  List *qpqual;
  ListCell *l;
  BitmapHeapScan *scan_plan;

                                  
  Assert(baserelid > 0);
  Assert(best_path->path.parent->rtekind == RTE_RELATION);

                                                                   
  bitmapqualplan = create_bitmap_subplan(root, best_path->bitmapqual, &bitmapqualorig, &indexquals, &indexECs);

  if (best_path->path.parallel_aware)
  {
    bitmap_subplan_mark_shared(bitmapqualplan);
  }

     
                                                                             
                                                                           
                                                                           
                                                        
                                                                     
                                                                            
                                                                     
     
                                                                             
                                                                             
                                                                          
           
     
                                                                            
                                                                       
                                                                         
                                                                
     
                                                                             
                                                                             
                                                                       
                                                                           
                                                                       
                                                                   
     
  qpqual = NIL;
  foreach (l, scan_clauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);
    Node *clause = (Node *)rinfo->clause;

    if (rinfo->pseudoconstant)
    {
      continue;                                       
    }
    if (list_member(indexquals, clause))
    {
      continue;                       
    }
    if (rinfo->parent_ec && list_member_ptr(indexECs, rinfo->parent_ec))
    {
      continue;                                         
    }
    if (!contain_mutable_functions(clause) && predicate_implied_by(list_make1(clause), indexquals, false))
    {
      continue;                                     
    }
    qpqual = lappend(qpqual, rinfo);
  }

                                              
  qpqual = order_qual_clauses(root, qpqual);

                                                                            
  qpqual = extract_actual_clauses(qpqual, false);

     
                                                                     
                                                                          
                                                                         
            
     
  bitmapqualorig = list_difference_ptr(bitmapqualorig, qpqual);

     
                                                                             
                                                                            
                                                                     
     
  if (best_path->path.param_info)
  {
    qpqual = (List *)replace_nestloop_params(root, (Node *)qpqual);
    bitmapqualorig = (List *)replace_nestloop_params(root, (Node *)bitmapqualorig);
  }

                                            
  scan_plan = make_bitmap_heapscan(tlist, qpqual, bitmapqualplan, bitmapqualorig, baserelid);

  copy_generic_path_info(&scan_plan->scan.plan, &best_path->path);

  return scan_plan;
}

   
                                                                      
   
                                                                        
                                                                               
                                                                              
                                                                           
                                                                             
                                                                              
                                                                      
                                                
   
                                                                          
                                                                         
                                                                           
                                                                            
                                                                           
                                                                              
                                                                     
   
static Plan *
create_bitmap_subplan(PlannerInfo *root, Path *bitmapqual, List **qual, List **indexqual, List **indexECs)
{
  Plan *plan;

  if (IsA(bitmapqual, BitmapAndPath))
  {
    BitmapAndPath *apath = (BitmapAndPath *)bitmapqual;
    List *subplans = NIL;
    List *subquals = NIL;
    List *subindexquals = NIL;
    List *subindexECs = NIL;
    ListCell *l;

       
                                                                     
                                                                   
                                                                          
                                                                     
                           
       
    foreach (l, apath->bitmapquals)
    {
      Plan *subplan;
      List *subqual;
      List *subindexqual;
      List *subindexEC;

      subplan = create_bitmap_subplan(root, (Path *)lfirst(l), &subqual, &subindexqual, &subindexEC);
      subplans = lappend(subplans, subplan);
      subquals = list_concat_unique(subquals, subqual);
      subindexquals = list_concat_unique(subindexquals, subindexqual);
                                                              
      subindexECs = list_concat(subindexECs, subindexEC);
    }
    plan = (Plan *)make_bitmap_and(subplans);
    plan->startup_cost = apath->path.startup_cost;
    plan->total_cost = apath->path.total_cost;
    plan->plan_rows = clamp_row_est(apath->bitmapselectivity * apath->path.parent->tuples);
    plan->plan_width = 0;                  
    plan->parallel_aware = false;
    plan->parallel_safe = apath->path.parallel_safe;
    *qual = subquals;
    *indexqual = subindexquals;
    *indexECs = subindexECs;
  }
  else if (IsA(bitmapqual, BitmapOrPath))
  {
    BitmapOrPath *opath = (BitmapOrPath *)bitmapqual;
    List *subplans = NIL;
    List *subquals = NIL;
    List *subindexquals = NIL;
    bool const_true_subqual = false;
    bool const_true_subindexqual = false;
    ListCell *l;

       
                                                                           
                                                                           
                                                                        
                                                                           
                                                                         
                                                                           
                              
       
    foreach (l, opath->bitmapquals)
    {
      Plan *subplan;
      List *subqual;
      List *subindexqual;
      List *subindexEC;

      subplan = create_bitmap_subplan(root, (Path *)lfirst(l), &subqual, &subindexqual, &subindexEC);
      subplans = lappend(subplans, subplan);
      if (subqual == NIL)
      {
        const_true_subqual = true;
      }
      else if (!const_true_subqual)
      {
        subquals = lappend(subquals, make_ands_explicit(subqual));
      }
      if (subindexqual == NIL)
      {
        const_true_subindexqual = true;
      }
      else if (!const_true_subindexqual)
      {
        subindexquals = lappend(subindexquals, make_ands_explicit(subindexqual));
      }
    }

       
                                                                       
                                                                  
       
    if (list_length(subplans) == 1)
    {
      plan = (Plan *)linitial(subplans);
    }
    else
    {
      plan = (Plan *)make_bitmap_or(subplans);
      plan->startup_cost = opath->path.startup_cost;
      plan->total_cost = opath->path.total_cost;
      plan->plan_rows = clamp_row_est(opath->bitmapselectivity * opath->path.parent->tuples);
      plan->plan_width = 0;                  
      plan->parallel_aware = false;
      plan->parallel_safe = opath->path.parallel_safe;
    }

       
                                                                        
                                                                         
                                                                 
       
    if (const_true_subqual)
    {
      *qual = NIL;
    }
    else if (list_length(subquals) <= 1)
    {
      *qual = subquals;
    }
    else
    {
      *qual = list_make1(make_orclause(subquals));
    }
    if (const_true_subindexqual)
    {
      *indexqual = NIL;
    }
    else if (list_length(subindexquals) <= 1)
    {
      *indexqual = subindexquals;
    }
    else
    {
      *indexqual = list_make1(make_orclause(subindexquals));
    }
    *indexECs = NIL;
  }
  else if (IsA(bitmapqual, IndexPath))
  {
    IndexPath *ipath = (IndexPath *)bitmapqual;
    IndexScan *iscan;
    List *subquals;
    List *subindexquals;
    List *subindexECs;
    ListCell *l;

                                                           
    iscan = castNode(IndexScan, create_indexscan_plan(root, ipath, NIL, NIL, false));
                                            
    plan = (Plan *)make_bitmap_indexscan(iscan->scan.scanrelid, iscan->indexid, iscan->indexqual, iscan->indexqualorig);
                                                     
    plan->startup_cost = 0.0;
    plan->total_cost = ipath->indextotalcost;
    plan->plan_rows = clamp_row_est(ipath->indexselectivity * ipath->path.parent->tuples);
    plan->plan_width = 0;                  
    plan->parallel_aware = false;
    plan->parallel_safe = ipath->path.parallel_safe;
                                                                          
    subquals = NIL;
    subindexquals = NIL;
    subindexECs = NIL;
    foreach (l, ipath->indexclauses)
    {
      IndexClause *iclause = (IndexClause *)lfirst(l);
      RestrictInfo *rinfo = iclause->rinfo;

      Assert(!rinfo->pseudoconstant);
      subquals = lappend(subquals, rinfo->clause);
      subindexquals = list_concat(subindexquals, get_actual_clauses(iclause->indexquals));
      if (rinfo->parent_ec)
      {
        subindexECs = lappend(subindexECs, rinfo->parent_ec);
      }
    }
                                                        
    foreach (l, ipath->indexinfo->indpred)
    {
      Expr *pred = (Expr *)lfirst(l);

         
                                                                        
                                                                         
                                                                    
                                          
         
      if (!predicate_implied_by(list_make1(pred), subquals, false))
      {
        subquals = lappend(subquals, pred);
        subindexquals = lappend(subindexquals, pred);
      }
    }
    *qual = subquals;
    *indexqual = subindexquals;
    *indexECs = subindexECs;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", nodeTag(bitmapqual));
    plan = NULL;                          
  }

  return plan;
}

   
                       
                                                                        
                                                                    
   
static TidScan *
create_tidscan_plan(PlannerInfo *root, TidPath *best_path, List *tlist, List *scan_clauses)
{
  TidScan *scan_plan;
  Index scan_relid = best_path->path.parent->relid;
  List *tidquals = best_path->tidquals;

                                  
  Assert(scan_relid > 0);
  Assert(best_path->path.parent->rtekind == RTE_RELATION);

     
                                                                       
                                                                            
                                                                          
                                                                            
                                                                          
                                                                          
              
     
                                                                           
                                                    
     
                                                                            
                                                                            
                           
     
                                                                         
                                                          
     
  if (list_length(tidquals) == 1)
  {
    List *qpqual = NIL;
    ListCell *l;

    foreach (l, scan_clauses)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

      if (rinfo->pseudoconstant)
      {
        continue;                                       
      }
      if (list_member_ptr(tidquals, rinfo))
      {
        continue;                       
      }
      if (is_redundant_derived_clause(rinfo, tidquals))
      {
        continue;                                         
      }
      qpqual = lappend(qpqual, rinfo);
    }
    scan_clauses = qpqual;
  }

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                             
  tidquals = extract_actual_clauses(tidquals, false);
  scan_clauses = extract_actual_clauses(scan_clauses, false);

     
                                                                            
                                                                         
                                                                             
                                                                         
                                                                            
                                                                             
                                                                         
                                                                          
                                              
     
  if (list_length(tidquals) > 1)
  {
    scan_clauses = list_difference(scan_clauses, list_make1(make_orclause(tidquals)));
  }

                                                                 
  if (best_path->path.param_info)
  {
    tidquals = (List *)replace_nestloop_params(root, (Node *)tidquals);
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
  }

  scan_plan = make_tidscan(tlist, scan_clauses, scan_relid, tidquals);

  copy_generic_path_info(&scan_plan->scan.plan, &best_path->path);

  return scan_plan;
}

   
                            
                                                                             
                                                                    
   
static SubqueryScan *
create_subqueryscan_plan(PlannerInfo *root, SubqueryScanPath *best_path, List *tlist, List *scan_clauses)
{
  SubqueryScan *scan_plan;
  RelOptInfo *rel = best_path->path.parent;
  Index scan_relid = rel->relid;
  Plan *subplan;

                                           
  Assert(scan_relid > 0);
  Assert(rel->rtekind == RTE_SUBQUERY);

     
                                                                            
                                                                       
                          
     
  subplan = create_plan(rel->subroot, best_path->subpath);

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->path.param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
    process_subquery_nestloop_params(root, rel->subplan_params);
  }

  scan_plan = make_subqueryscan(tlist, scan_clauses, scan_relid, subplan);

  copy_generic_path_info(&scan_plan->scan.plan, &best_path->path);

  return scan_plan;
}

   
                            
                                                                             
                                                                    
   
static FunctionScan *
create_functionscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  FunctionScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;
  List *functions;

                                           
  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_FUNCTION);
  functions = rte->functions;

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
                                                                     
    functions = (List *)replace_nestloop_params(root, (Node *)functions);
  }

  scan_plan = make_functionscan(tlist, scan_clauses, scan_relid, functions, rte->funcordinality);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                             
                                                                              
                                                                    
   
static TableFuncScan *
create_tablefuncscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  TableFuncScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;
  TableFunc *tablefunc;

                                           
  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_TABLEFUNC);
  tablefunc = rte->tablefunc;

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
                                                                     
    tablefunc = (TableFunc *)replace_nestloop_params(root, (Node *)tablefunc);
  }

  scan_plan = make_tablefuncscan(tlist, scan_clauses, scan_relid, tablefunc);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                          
                                                                           
                                                                    
   
static ValuesScan *
create_valuesscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  ValuesScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;
  List *values_lists;

                                         
  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_VALUES);
  values_lists = rte->values_lists;

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
                                                             
    values_lists = (List *)replace_nestloop_params(root, (Node *)values_lists);
  }

  scan_plan = make_valuesscan(tlist, scan_clauses, scan_relid, values_lists);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                       
                                                                        
                                                                    
   
static CteScan *
create_ctescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  CteScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;
  SubPlan *ctesplan = NULL;
  int plan_id;
  int cte_param_id;
  PlannerInfo *cteroot;
  Index levelsup;
  int ndx;
  ListCell *lc;

  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_CTE);
  Assert(!rte->self_reference);

     
                                                                             
     
  levelsup = rte->ctelevelsup;
  cteroot = root;
  while (levelsup-- > 0)
  {
    cteroot = cteroot->parent_root;
    if (!cteroot)                       
    {
      elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
    }
  }

     
                                                                             
                                                                           
                                     
     
  ndx = 0;
  foreach (lc, cteroot->parse->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

    if (strcmp(cte->ctename, rte->ctename) == 0)
    {
      break;
    }
    ndx++;
  }
  if (lc == NULL)                       
  {
    elog(ERROR, "could not find CTE \"%s\"", rte->ctename);
  }
  if (ndx >= list_length(cteroot->cte_plan_ids))
  {
    elog(ERROR, "could not find plan for CTE \"%s\"", rte->ctename);
  }
  plan_id = list_nth_int(cteroot->cte_plan_ids, ndx);
  if (plan_id <= 0)
  {
    elog(ERROR, "no plan was made for CTE \"%s\"", rte->ctename);
  }
  foreach (lc, cteroot->init_plans)
  {
    ctesplan = (SubPlan *)lfirst(lc);
    if (ctesplan->plan_id == plan_id)
    {
      break;
    }
  }
  if (lc == NULL)                       
  {
    elog(ERROR, "could not find plan for CTE \"%s\"", rte->ctename);
  }

     
                                                                         
                    
     
  cte_param_id = linitial_int(ctesplan->setParam);

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
  }

  scan_plan = make_ctescan(tlist, scan_clauses, scan_relid, plan_id, cte_param_id);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                                   
                                                                   
                                                                      
            
   
static NamedTuplestoreScan *
create_namedtuplestorescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  NamedTuplestoreScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;

  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_NAMEDTUPLESTORE);

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
  }

  scan_plan = make_namedtuplestorescan(tlist, scan_clauses, scan_relid, rte->enrname);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                          
                                                                      
                                                                      
            
   
static Result *
create_resultscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  Result *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte PG_USED_FOR_ASSERTS_ONLY;

  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_RESULT);

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
  }

  scan_plan = make_result(tlist, (Node *)scan_clauses, NULL);

  copy_generic_path_info(&scan_plan->plan, best_path);

  return scan_plan;
}

   
                             
                                                                              
                                                                    
   
static WorkTableScan *
create_worktablescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
  WorkTableScan *scan_plan;
  Index scan_relid = best_path->parent->relid;
  RangeTblEntry *rte;
  Index levelsup;
  PlannerInfo *cteroot;

  Assert(scan_relid > 0);
  rte = planner_rt_fetch(scan_relid, root);
  Assert(rte->rtekind == RTE_CTE);
  Assert(rte->self_reference);

     
                                                                        
                                                                             
                         
     
  levelsup = rte->ctelevelsup;
  if (levelsup == 0)                       
  {
    elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
  }
  levelsup--;
  cteroot = root;
  while (levelsup-- > 0)
  {
    cteroot = cteroot->parent_root;
    if (!cteroot)                       
    {
      elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
    }
  }
  if (cteroot->wt_param_id < 0)                       
  {
    elog(ERROR, "could not find param ID for CTE \"%s\"", rte->ctename);
  }

                                              
  scan_clauses = order_qual_clauses(root, scan_clauses);

                                                                            
  scan_clauses = extract_actual_clauses(scan_clauses, false);

                                                                 
  if (best_path->param_info)
  {
    scan_clauses = (List *)replace_nestloop_params(root, (Node *)scan_clauses);
  }

  scan_plan = make_worktablescan(tlist, scan_clauses, scan_relid, cteroot->wt_param_id);

  copy_generic_path_info(&scan_plan->scan.plan, best_path);

  return scan_plan;
}

   
                           
                                                                       
                                                                    
   
static ForeignScan *
create_foreignscan_plan(PlannerInfo *root, ForeignPath *best_path, List *tlist, List *scan_clauses)
{
  ForeignScan *scan_plan;
  RelOptInfo *rel = best_path->path.parent;
  Index scan_relid = rel->relid;
  Oid rel_oid = InvalidOid;
  Plan *outer_plan = NULL;

  Assert(rel->fdwroutine != NULL);

                                       
  if (best_path->fdw_outerpath)
  {
    outer_plan = create_plan_recurse(root, best_path->fdw_outerpath, CP_EXACT_TLIST);
  }

     
                                                                       
                                
     
  if (scan_relid > 0)
  {
    RangeTblEntry *rte;

    Assert(rel->rtekind == RTE_RELATION);
    rte = planner_rt_fetch(scan_relid, root);
    Assert(rte->rtekind == RTE_RELATION);
    rel_oid = rte->relid;
  }

     
                                                                             
                                                                      
     
  scan_clauses = order_qual_clauses(root, scan_clauses);

     
                                                                       
                                                                         
                                                                          
                                                                       
                         
     
  scan_plan = rel->fdwroutine->GetForeignPlan(root, rel, rel_oid, best_path, tlist, scan_clauses, outer_plan);

                                                                     
  copy_generic_path_info(&scan_plan->scan.plan, &best_path->path);

                                                                      
  scan_plan->fs_server = rel->serverid;

     
                                                                             
                                                                             
                                                                       
     
  if (rel->reloptkind == RELOPT_UPPER_REL)
  {
    scan_plan->fs_relids = root->all_baserels;
  }
  else
  {
    scan_plan->fs_relids = best_path->path.parent->relids;
  }

     
                                                                            
                                                                            
                                                                            
     
  if (rel->useridiscurrent)
  {
    root->glob->dependsOnRole = true;
  }

     
                                                                            
                                                                           
                                                                            
                                                                        
                                                               
                                                
     
  if (best_path->path.param_info)
  {
    scan_plan->scan.plan.qual = (List *)replace_nestloop_params(root, (Node *)scan_plan->scan.plan.qual);
    scan_plan->fdw_exprs = (List *)replace_nestloop_params(root, (Node *)scan_plan->fdw_exprs);
    scan_plan->fdw_recheck_quals = (List *)replace_nestloop_params(root, (Node *)scan_plan->fdw_recheck_quals);
  }

     
                                                                      
                                                                             
                                                                            
                                                                            
                                                                            
                                                                    
                                                                             
     
  scan_plan->fsSystemCol = false;
  if (scan_relid > 0)
  {
    Bitmapset *attrs_used = NULL;
    ListCell *lc;
    int i;

       
                                                                           
                                                                         
                                                                      
       
    pull_varattnos((Node *)rel->reltarget->exprs, scan_relid, &attrs_used);

                                                             
    foreach (lc, rel->baserestrictinfo)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

      pull_varattnos((Node *)rinfo->clause, scan_relid, &attrs_used);
    }

                                                         
    for (i = FirstLowInvalidHeapAttributeNumber + 1; i < 0; i++)
    {
      if (bms_is_member(i - FirstLowInvalidHeapAttributeNumber, attrs_used))
      {
        scan_plan->fsSystemCol = true;
        break;
      }
    }

    bms_free(attrs_used);
  }

  return scan_plan;
}

   
                          
   
                                       
   
static CustomScan *
create_customscan_plan(PlannerInfo *root, CustomPath *best_path, List *tlist, List *scan_clauses)
{
  CustomScan *cplan;
  RelOptInfo *rel = best_path->path.parent;
  List *custom_plans = NIL;
  ListCell *lc;

                                          
  foreach (lc, best_path->custom_paths)
  {
    Plan *plan = create_plan_recurse(root, (Path *)lfirst(lc), CP_EXACT_TLIST);

    custom_plans = lappend(custom_plans, plan);
  }

     
                                                                      
                                      
     
  scan_clauses = order_qual_clauses(root, scan_clauses);

     
                                                                            
                 
     
  cplan = castNode(CustomScan, best_path->methods->PlanCustomPath(root, rel, best_path, tlist, scan_clauses, custom_plans));

     
                                                                             
             
     
  copy_generic_path_info(&cplan->scan.plan, &best_path->path);

                                                                          
  cplan->custom_relids = best_path->path.parent->relids;

     
                                                                           
                                                                            
                                                                             
                                                                        
                                                                           
                     
     
  if (best_path->path.param_info)
  {
    cplan->scan.plan.qual = (List *)replace_nestloop_params(root, (Node *)cplan->scan.plan.qual);
    cplan->custom_exprs = (List *)replace_nestloop_params(root, (Node *)cplan->custom_exprs);
  }

  return cplan;
}

                                                                               
   
                
   
                                                                               

static NestLoop *
create_nestloop_plan(PlannerInfo *root, NestPath *best_path)
{
  NestLoop *join_plan;
  Plan *outer_plan;
  Plan *inner_plan;
  List *tlist = build_path_tlist(root, &best_path->path);
  List *joinrestrictclauses = best_path->joinrestrictinfo;
  List *joinclauses;
  List *otherclauses;
  Relids outerrelids;
  List *nestParams;
  Relids saveOuterRels = root->curOuterRels;

                                                                       
  outer_plan = create_plan_recurse(root, best_path->outerjoinpath, 0);

                                                                           
  root->curOuterRels = bms_union(root->curOuterRels, best_path->outerjoinpath->parent->relids);

  inner_plan = create_plan_recurse(root, best_path->innerjoinpath, 0);

                            
  bms_free(root->curOuterRels);
  root->curOuterRels = saveOuterRels;

                                                        
  joinrestrictclauses = order_qual_clauses(root, joinrestrictclauses);

                                                            
                                                   
  if (IS_OUTER_JOIN(best_path->jointype))
  {
    extract_actual_join_clauses(joinrestrictclauses, best_path->path.parent->relids, &joinclauses, &otherclauses);
  }
  else
  {
                                                          
    joinclauses = extract_actual_clauses(joinrestrictclauses, false);
    otherclauses = NIL;
  }

                                                                 
  if (best_path->path.param_info)
  {
    joinclauses = (List *)replace_nestloop_params(root, (Node *)joinclauses);
    otherclauses = (List *)replace_nestloop_params(root, (Node *)otherclauses);
  }

     
                                                                           
                                                      
     
  outerrelids = best_path->outerjoinpath->parent->relids;
  nestParams = identify_current_nestloop_params(root, outerrelids);

  join_plan = make_nestloop(tlist, joinclauses, otherclauses, nestParams, outer_plan, inner_plan, best_path->jointype, best_path->inner_unique);

  copy_generic_path_info(&join_plan->join.plan, &best_path->path);

  return join_plan;
}

static MergeJoin *
create_mergejoin_plan(PlannerInfo *root, MergePath *best_path)
{
  MergeJoin *join_plan;
  Plan *outer_plan;
  Plan *inner_plan;
  List *tlist = build_path_tlist(root, &best_path->jpath.path);
  List *joinclauses;
  List *otherclauses;
  List *mergeclauses;
  List *outerpathkeys;
  List *innerpathkeys;
  int nClauses;
  Oid *mergefamilies;
  Oid *mergecollations;
  int *mergestrategies;
  bool *mergenullsfirst;
  PathKey *opathkey;
  EquivalenceClass *opeclass;
  int i;
  ListCell *lc;
  ListCell *lop;
  ListCell *lip;
  Path *outer_path = best_path->jpath.outerjoinpath;
  Path *inner_path = best_path->jpath.innerjoinpath;

     
                                                                             
                                                                          
                                                                       
                
     
  outer_plan = create_plan_recurse(root, best_path->jpath.outerjoinpath, (best_path->outersortkeys != NIL) ? CP_SMALL_TLIST : 0);

  inner_plan = create_plan_recurse(root, best_path->jpath.innerjoinpath, (best_path->innersortkeys != NIL) ? CP_SMALL_TLIST : 0);

                                                        
                                           
  joinclauses = order_qual_clauses(root, best_path->jpath.joinrestrictinfo);

                                                            
                                                   
  if (IS_OUTER_JOIN(best_path->jpath.jointype))
  {
    extract_actual_join_clauses(joinclauses, best_path->jpath.path.parent->relids, &joinclauses, &otherclauses);
  }
  else
  {
                                                          
    joinclauses = extract_actual_clauses(joinclauses, false);
    otherclauses = NIL;
  }

     
                                                                             
                                                    
     
  mergeclauses = get_actual_clauses(best_path->path_mergeclauses);
  joinclauses = list_difference(joinclauses, mergeclauses);

     
                                                                       
                                            
     
  if (best_path->jpath.path.param_info)
  {
    joinclauses = (List *)replace_nestloop_params(root, (Node *)joinclauses);
    otherclauses = (List *)replace_nestloop_params(root, (Node *)otherclauses);
  }

     
                                                                             
                                                                  
                           
     
  mergeclauses = get_switched_clauses(best_path->path_mergeclauses, best_path->jpath.outerjoinpath->parent->relids);

     
                                                                            
     
  if (best_path->outersortkeys)
  {
    Relids outer_relids = outer_path->parent->relids;
    Sort *sort = make_sort_from_pathkeys(outer_plan, best_path->outersortkeys, outer_relids);

    label_sort_with_costsize(root, sort, -1.0);
    outer_plan = (Plan *)sort;
    outerpathkeys = best_path->outersortkeys;
  }
  else
  {
    outerpathkeys = best_path->jpath.outerjoinpath->pathkeys;
  }

  if (best_path->innersortkeys)
  {
    Relids inner_relids = inner_path->parent->relids;
    Sort *sort = make_sort_from_pathkeys(inner_plan, best_path->innersortkeys, inner_relids);

    label_sort_with_costsize(root, sort, -1.0);
    inner_plan = (Plan *)sort;
    innerpathkeys = best_path->innersortkeys;
  }
  else
  {
    innerpathkeys = best_path->jpath.innerjoinpath->pathkeys;
  }

     
                                                                            
                                  
     
  if (best_path->materialize_inner)
  {
    Plan *matplan = (Plan *)make_material(inner_plan);

       
                                                                       
                                                                        
                                        
       
    copy_plan_costsize(matplan, inner_plan);
    matplan->total_cost += cpu_operator_cost * matplan->plan_rows;

    inner_plan = matplan;
  }

     
                                                                             
                                                                           
                                                                           
                                                                            
                                         
     
  nClauses = list_length(mergeclauses);
  Assert(nClauses == list_length(best_path->path_mergeclauses));
  mergefamilies = (Oid *)palloc(nClauses * sizeof(Oid));
  mergecollations = (Oid *)palloc(nClauses * sizeof(Oid));
  mergestrategies = (int *)palloc(nClauses * sizeof(int));
  mergenullsfirst = (bool *)palloc(nClauses * sizeof(bool));

  opathkey = NULL;
  opeclass = NULL;
  lop = list_head(outerpathkeys);
  lip = list_head(innerpathkeys);
  i = 0;
  foreach (lc, best_path->path_mergeclauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc);
    EquivalenceClass *oeclass;
    EquivalenceClass *ieclass;
    PathKey *ipathkey = NULL;
    EquivalenceClass *ipeclass = NULL;
    bool first_inner_match = false;

                                                   
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
    Assert(oeclass != NULL);
    Assert(ieclass != NULL);

       
                                                                         
                                                                         
                                                                           
                                                                       
                                                                          
       
                                                                          
                                                                    
       
                                                                         
                                                                           
                                                                          
                                                
       
    if (oeclass != opeclass)                                           
    {
                                                                      
      if (lop == NULL)
      {
        elog(ERROR, "outer pathkeys do not match mergeclauses");
      }
      opathkey = (PathKey *)lfirst(lop);
      opeclass = opathkey->pk_eclass;
      lop = lnext(lop);
      if (oeclass != opeclass)
      {
        elog(ERROR, "outer pathkeys do not match mergeclauses");
      }
    }

       
                                                                          
                                                                       
                                                                        
                                                                       
                                                                          
                                                                        
                  
       
                                                                        
                                                                           
                                                                       
                                                                       
                                                                          
       
    if (lip)
    {
      ipathkey = (PathKey *)lfirst(lip);
      ipeclass = ipathkey->pk_eclass;
      if (ieclass == ipeclass)
      {
                                                          
        lip = lnext(lip);
        first_inner_match = true;
      }
    }
    if (!first_inner_match)
    {
                                                                
      ListCell *l2;

      foreach (l2, innerpathkeys)
      {
        if (l2 == lip)
        {
          break;
        }
        ipathkey = (PathKey *)lfirst(l2);
        ipeclass = ipathkey->pk_eclass;
        if (ieclass == ipeclass)
        {
          break;
        }
      }
      if (ieclass != ipeclass)
      {
        elog(ERROR, "inner pathkeys do not match mergeclauses");
      }
    }

       
                                                                      
                                                                     
                                                                       
                                                                          
                                                                       
                                                                         
                                                                      
                                                                           
                                                                           
                                                                       
                                                                      
                                                                          
       
    if (opathkey->pk_opfamily != ipathkey->pk_opfamily || opathkey->pk_eclass->ec_collation != ipathkey->pk_eclass->ec_collation)
    {
      elog(ERROR, "left and right pathkeys do not match in mergejoin");
    }
    if (first_inner_match && (opathkey->pk_strategy != ipathkey->pk_strategy || opathkey->pk_nulls_first != ipathkey->pk_nulls_first))
    {
      elog(ERROR, "left and right pathkeys do not match in mergejoin");
    }

                                    
    mergefamilies[i] = opathkey->pk_opfamily;
    mergecollations[i] = opathkey->pk_eclass->ec_collation;
    mergestrategies[i] = opathkey->pk_strategy;
    mergenullsfirst[i] = opathkey->pk_nulls_first;
    i++;
  }

     
                                                                            
                                                                          
                                             
     

     
                                          
     
  join_plan = make_mergejoin(tlist, joinclauses, otherclauses, mergeclauses, mergefamilies, mergecollations, mergestrategies, mergenullsfirst, outer_plan, inner_plan, best_path->jpath.jointype, best_path->jpath.inner_unique, best_path->skip_mark_restore);

                                                                          
  copy_generic_path_info(&join_plan->join.plan, &best_path->jpath.path);

  return join_plan;
}

static HashJoin *
create_hashjoin_plan(PlannerInfo *root, HashPath *best_path)
{
  HashJoin *join_plan;
  Hash *hash_plan;
  Plan *outer_plan;
  Plan *inner_plan;
  List *tlist = build_path_tlist(root, &best_path->jpath.path);
  List *joinclauses;
  List *otherclauses;
  List *hashclauses;
  List *hashoperators = NIL;
  List *hashcollations = NIL;
  List *inner_hashkeys = NIL;
  List *outer_hashkeys = NIL;
  Oid skewTable = InvalidOid;
  AttrNumber skewColumn = InvalidAttrNumber;
  bool skewInherit = false;
  ListCell *lc;

     
                                                                            
                                                                         
                                                                             
                                                                          
                                                            
     
  outer_plan = create_plan_recurse(root, best_path->jpath.outerjoinpath, (best_path->num_batches > 1) ? CP_SMALL_TLIST : 0);

  inner_plan = create_plan_recurse(root, best_path->jpath.innerjoinpath, CP_SMALL_TLIST);

                                                        
  joinclauses = order_qual_clauses(root, best_path->jpath.joinrestrictinfo);
                                                        

                                                            
                                                   
  if (IS_OUTER_JOIN(best_path->jpath.jointype))
  {
    extract_actual_join_clauses(joinclauses, best_path->jpath.path.parent->relids, &joinclauses, &otherclauses);
  }
  else
  {
                                                          
    joinclauses = extract_actual_clauses(joinclauses, false);
    otherclauses = NIL;
  }

     
                                                                            
                                                    
     
  hashclauses = get_actual_clauses(best_path->path_hashclauses);
  joinclauses = list_difference(joinclauses, hashclauses);

     
                                                                       
                                           
     
  if (best_path->jpath.path.param_info)
  {
    joinclauses = (List *)replace_nestloop_params(root, (Node *)joinclauses);
    otherclauses = (List *)replace_nestloop_params(root, (Node *)otherclauses);
  }

     
                                                                            
                  
     
  hashclauses = get_switched_clauses(best_path->path_hashclauses, best_path->jpath.outerjoinpath->parent->relids);

     
                                                                             
                                                                           
                                                                           
                                                                           
                                                                             
                        
     
  if (list_length(hashclauses) == 1)
  {
    OpExpr *clause = (OpExpr *)linitial(hashclauses);
    Node *node;

    Assert(is_opclause(clause));
    node = (Node *)linitial(clause->args);
    if (IsA(node, RelabelType))
    {
      node = (Node *)((RelabelType *)node)->arg;
    }
    if (IsA(node, Var))
    {
      Var *var = (Var *)node;
      RangeTblEntry *rte;

      rte = root->simple_rte_array[var->varno];
      if (rte->rtekind == RTE_RELATION)
      {
        skewTable = rte->relid;
        skewColumn = var->varattno;
        skewInherit = rte->inh;
      }
    }
  }

     
                                                                  
                                                                         
                                                                             
                                                                          
                                                                            
                          
     
  foreach (lc, hashclauses)
  {
    OpExpr *hclause = lfirst_node(OpExpr, lc);

    hashoperators = lappend_oid(hashoperators, hclause->opno);
    hashcollations = lappend_oid(hashcollations, hclause->inputcollid);
    outer_hashkeys = lappend(outer_hashkeys, linitial(hclause->args));
    inner_hashkeys = lappend(inner_hashkeys, lsecond(hclause->args));
  }

     
                                             
     
  hash_plan = make_hash(inner_plan, inner_hashkeys, skewTable, skewColumn, skewInherit);

     
                                                                        
                                                            
     
  copy_plan_costsize(&hash_plan->plan, inner_plan);
  hash_plan->plan.startup_cost = hash_plan->plan.total_cost;

     
                                                                             
                                                                           
                        
     
  if (best_path->jpath.path.parallel_aware)
  {
    hash_plan->plan.parallel_aware = true;
    hash_plan->rows_total = best_path->inner_rows_total;
  }

  join_plan = make_hashjoin(tlist, joinclauses, otherclauses, hashclauses, hashoperators, hashcollations, outer_hashkeys, outer_plan, (Plan *)hash_plan, best_path->jpath.jointype, best_path->jpath.inner_unique);

  copy_generic_path_info(&join_plan->join.plan, &best_path->jpath.path);

  return join_plan;
}

                                                                               
   
                       
   
                                                                               

   
                           
                                                                             
                          
   
                                                                           
                                                                       
                                                
   
static Node *
replace_nestloop_params(PlannerInfo *root, Node *expr)
{
                                                    
  return replace_nestloop_params_mutator(expr, root);
}

static Node *
replace_nestloop_params_mutator(Node *node, PlannerInfo *root)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

                                                            
    Assert(var->varlevelsup == 0);
                                                                      
    if (!bms_is_member(var->varno, root->curOuterRels))
    {
      return node;
    }
                                               
    return (Node *)replace_nestloop_param_var(root, var);
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

                                                                       
    Assert(phv->phlevelsup == 0);

       
                                                                          
                                                                          
                                                                 
       
    if (!bms_overlap(phv->phrels, root->curOuterRels) || !bms_is_subset(find_placeholder_info(root, phv, false)->ph_eval_at, root->curOuterRels))
    {
         
                                                                    
                                                                        
                                                                      
                                                                         
                                                                        
                                                                     
                                                  
         
                                                             
                                                                      
                                                                       
                                                                        
                                                                      
         
      PlaceHolderVar *newphv = makeNode(PlaceHolderVar);

      memcpy(newphv, phv, sizeof(PlaceHolderVar));
      newphv->phexpr = (Expr *)replace_nestloop_params_mutator((Node *)phv->phexpr, root);
      return (Node *)newphv;
    }
                                                          
    return (Node *)replace_nestloop_param_placeholdervar(root, phv);
  }
  return expression_tree_mutator(node, replace_nestloop_params_mutator, (void *)root);
}

   
                            
                                                                   
                      
   
                             
                                                                       
                                                          
                                                                       
                                                                
                                                                          
                                                                             
   
                                                                      
   
                                                                              
                                                                               
                                                                           
                         
   
static void
fix_indexqual_references(PlannerInfo *root, IndexPath *index_path, List **stripped_indexquals_p, List **fixed_indexquals_p)
{
  IndexOptInfo *index = index_path->indexinfo;
  List *stripped_indexquals;
  List *fixed_indexquals;
  ListCell *lc;

  stripped_indexquals = fixed_indexquals = NIL;

  foreach (lc, index_path->indexclauses)
  {
    IndexClause *iclause = lfirst_node(IndexClause, lc);
    int indexcol = iclause->indexcol;
    ListCell *lc2;

    foreach (lc2, iclause->indexquals)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc2);
      Node *clause = (Node *)rinfo->clause;

      stripped_indexquals = lappend(stripped_indexquals, clause);
      clause = fix_indexqual_clause(root, index, indexcol, clause, iclause->indexcols);
      fixed_indexquals = lappend(fixed_indexquals, clause);
    }
  }

  *stripped_indexquals_p = stripped_indexquals;
  *fixed_indexquals_p = fixed_indexquals;
}

   
                               
                                                                  
                      
   
                                                                           
                                                                       
   
static List *
fix_indexorderby_references(PlannerInfo *root, IndexPath *index_path)
{
  IndexOptInfo *index = index_path->indexinfo;
  List *fixed_indexorderbys;
  ListCell *lcc, *lci;

  fixed_indexorderbys = NIL;

  forboth(lcc, index_path->indexorderbys, lci, index_path->indexorderbycols)
  {
    Node *clause = (Node *)lfirst(lcc);
    int indexcol = lfirst_int(lci);

    clause = fix_indexqual_clause(root, index, indexcol, clause, NIL);
    fixed_indexorderbys = lappend(fixed_indexorderbys, clause);
  }

  return fixed_indexorderbys;
}

   
                        
                                                                           
   
                                                                        
                                      
   
static Node *
fix_indexqual_clause(PlannerInfo *root, IndexOptInfo *index, int indexcol, Node *clause, List *indexcolnos)
{
     
                                                                
     
                                                                     
                     
     
  clause = replace_nestloop_params(root, clause);

  if (IsA(clause, OpExpr))
  {
    OpExpr *op = (OpExpr *)clause;

                                                            
    linitial(op->args) = fix_indexqual_operand(linitial(op->args), index, indexcol);
  }
  else if (IsA(clause, RowCompareExpr))
  {
    RowCompareExpr *rc = (RowCompareExpr *)clause;
    ListCell *lca, *lcai;

                                                           
    Assert(list_length(rc->largs) == list_length(indexcolnos));
    forboth(lca, rc->largs, lcai, indexcolnos) { lfirst(lca) = fix_indexqual_operand(lfirst(lca), index, lfirst_int(lcai)); }
  }
  else if (IsA(clause, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;

                                                            
    linitial(saop->args) = fix_indexqual_operand(linitial(saop->args), index, indexcol);
  }
  else if (IsA(clause, NullTest))
  {
    NullTest *nt = (NullTest *)clause;

                                                            
    nt->arg = (Expr *)fix_indexqual_operand((Node *)nt->arg, index, indexcol);
  }
  else
  {
    elog(ERROR, "unsupported indexqual type: %d", (int)nodeTag(clause));
  }

  return clause;
}

   
                         
                                                                            
   
                                                                               
                                                                  
   
                                                                          
                                                                 
   
static Node *
fix_indexqual_operand(Node *node, IndexOptInfo *index, int indexcol)
{
  Var *result;
  int pos;
  ListCell *indexpr_item;

     
                                                             
     
  if (IsA(node, RelabelType))
  {
    node = (Node *)((RelabelType *)node)->arg;
  }

  Assert(indexcol >= 0 && indexcol < index->ncolumns);

  if (index->indexkeys[indexcol] != 0)
  {
                                    
    if (IsA(node, Var) && ((Var *)node)->varno == index->rel->relid && ((Var *)node)->varattno == index->indexkeys[indexcol])
    {
      result = (Var *)copyObject(node);
      result->varno = INDEX_VAR;
      result->varattno = indexcol + 1;
      return (Node *)result;
    }
    else
    {
      elog(ERROR, "index key does not match expected index column");
    }
  }

                                                                        
  indexpr_item = list_head(index->indexprs);
  for (pos = 0; pos < index->ncolumns; pos++)
  {
    if (index->indexkeys[pos] == 0)
    {
      if (indexpr_item == NULL)
      {
        elog(ERROR, "too few entries in indexprs list");
      }
      if (pos == indexcol)
      {
        Node *indexkey;

        indexkey = (Node *)lfirst(indexpr_item);
        if (indexkey && IsA(indexkey, RelabelType))
        {
          indexkey = (Node *)((RelabelType *)indexkey)->arg;
        }
        if (equal(node, indexkey))
        {
          result = makeVar(INDEX_VAR, indexcol + 1, exprType(lfirst(indexpr_item)), -1, exprCollation(lfirst(indexpr_item)), 0);
          return (Node *)result;
        }
        else
        {
          elog(ERROR, "index key does not match expected index column");
        }
      }
      indexpr_item = lnext(indexpr_item);
    }
  }

               
  elog(ERROR, "index key does not match expected index column");
  return NULL;                          
}

   
                        
                                                                        
                                                                     
                                                                       
                                                                           
                                                                              
                                                                            
   
static List *
get_switched_clauses(List *clauses, Relids outerrelids)
{
  List *t_list = NIL;
  ListCell *l;

  foreach (l, clauses)
  {
    RestrictInfo *restrictinfo = (RestrictInfo *)lfirst(l);
    OpExpr *clause = (OpExpr *)restrictinfo->clause;

    Assert(is_opclause(clause));
    if (bms_is_subset(restrictinfo->right_relids, outerrelids))
    {
         
                                                                       
                                                               
                                                           
         
      OpExpr *temp = makeNode(OpExpr);

      temp->opno = clause->opno;
      temp->opfuncid = InvalidOid;
      temp->opresulttype = clause->opresulttype;
      temp->opretset = clause->opretset;
      temp->opcollid = clause->opcollid;
      temp->inputcollid = clause->inputcollid;
      temp->args = list_copy(clause->args);
      temp->location = clause->location;
                                                                     
      CommuteOpExpr(temp);
      t_list = lappend(t_list, temp);
      restrictinfo->outer_is_left = false;
    }
    else
    {
      Assert(bms_is_subset(restrictinfo->left_relids, outerrelids));
      t_list = lappend(t_list, clause);
      restrictinfo->outer_is_left = true;
    }
  }
  return t_list;
}

   
                      
                                                                        
                                                                       
                   
   
                                                                             
                                                                         
                                                                           
                                                                        
                                                                       
                                   
   
                                                                             
                                                                        
                                                                               
                                                                      
                                                                        
                                                       
   
                                                                         
                                                                      
                                                                        
                                                                         
                                                                        
                                                                           
                  
   
                                                                         
                                                                          
                                                                           
                                                                
   
static List *
order_qual_clauses(PlannerInfo *root, List *clauses)
{
  typedef struct
  {
    Node *clause;
    Cost cost;
    Index security_level;
  } QualItem;
  int nitems = list_length(clauses);
  QualItem *items;
  ListCell *lc;
  int i;
  List *result;

                                              
  if (nitems <= 1)
  {
    return clauses;
  }

     
                                                                           
                                                             
     
  items = (QualItem *)palloc(nitems * sizeof(QualItem));
  i = 0;
  foreach (lc, clauses)
  {
    Node *clause = (Node *)lfirst(lc);
    QualCost qcost;

    cost_qual_eval_node(&qcost, clause, root);
    items[i].clause = clause;
    items[i].cost = qcost.per_tuple;
    if (IsA(clause, RestrictInfo))
    {
      RestrictInfo *rinfo = (RestrictInfo *)clause;

         
                                                                        
                                                                    
                                                                 
                                                                 
                                                                         
                                                                       
                                                                     
                                                      
         
      if (rinfo->leakproof && items[i].cost < 10 * cpu_operator_cost)
      {
        items[i].security_level = 0;
      }
      else
      {
        items[i].security_level = rinfo->security_level;
      }
    }
    else
    {
      items[i].security_level = 0;
    }
    i++;
  }

     
                                                                        
                                                                        
                                                  
     
  for (i = 1; i < nitems; i++)
  {
    QualItem newitem = items[i];
    int j;

                                                         
    for (j = i; j > 0; j--)
    {
      QualItem *olditem = &items[j - 1];

      if (newitem.security_level > olditem->security_level || (newitem.security_level == olditem->security_level && newitem.cost >= olditem->cost))
      {
        break;
      }
      items[j] = *olditem;
    }
    items[j] = newitem;
  }

                              
  result = NIL;
  for (i = 0; i < nitems; i++)
  {
    result = lappend(result, items[i].clause);
  }

  return result;
}

   
                                                                              
                                                                         
                                                                        
   
static void
copy_generic_path_info(Plan *dest, Path *src)
{
  dest->startup_cost = src->startup_cost;
  dest->total_cost = src->total_cost;
  dest->plan_rows = src->rows;
  dest->plan_width = src->pathtarget->width;
  dest->parallel_aware = src->parallel_aware;
  dest->parallel_safe = src->parallel_safe;
}

   
                                                                       
                                                   
   
static void
copy_plan_costsize(Plan *dest, Plan *src)
{
  dest->startup_cost = src->startup_cost;
  dest->total_cost = src->total_cost;
  dest->plan_rows = src->plan_rows;
  dest->plan_width = src->plan_width;
                                                       
  dest->parallel_aware = false;
                                                                    
  dest->parallel_safe = src->parallel_safe;
}

   
                                                                        
                                                                           
                                                                            
                                                                           
                                              
   
                                                                         
   
static void
label_sort_with_costsize(PlannerInfo *root, Sort *plan, double limit_tuples)
{
  Plan *lefttree = plan->plan.lefttree;
  Path sort_path;                                    

  cost_sort(&sort_path, root, NIL, lefttree->total_cost, lefttree->plan_rows, lefttree->plan_width, 0.0, work_mem, limit_tuples);
  plan->plan.startup_cost = sort_path.startup_cost;
  plan->plan.total_cost = sort_path.total_cost;
  plan->plan.plan_rows = lefttree->plan_rows;
  plan->plan.plan_width = lefttree->plan_width;
  plan->plan.parallel_aware = false;
  plan->plan.parallel_safe = lefttree->parallel_safe;
}

   
                              
                                                                      
                   
   
static void
bitmap_subplan_mark_shared(Plan *plan)
{
  if (IsA(plan, BitmapAnd))
  {
    bitmap_subplan_mark_shared(linitial(((BitmapAnd *)plan)->bitmapplans));
  }
  else if (IsA(plan, BitmapOr))
  {
    ((BitmapOr *)plan)->isshared = true;
    bitmap_subplan_mark_shared(linitial(((BitmapOr *)plan)->bitmapplans));
  }
  else if (IsA(plan, BitmapIndexScan))
  {
    ((BitmapIndexScan *)plan)->isshared = true;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", nodeTag(plan));
  }
}

                                                                               
   
                               
   
                                                                              
                                                                          
                                                                      
                                                                              
                                                                            
                                                                             
                                                                           
   
                                                                               

static SeqScan *
make_seqscan(List *qptlist, List *qpqual, Index scanrelid)
{
  SeqScan *node = makeNode(SeqScan);
  Plan *plan = &node->plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scanrelid = scanrelid;

  return node;
}

static SampleScan *
make_samplescan(List *qptlist, List *qpqual, Index scanrelid, TableSampleClause *tsc)
{
  SampleScan *node = makeNode(SampleScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->tablesample = tsc;

  return node;
}

static IndexScan *
make_indexscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig, List *indexorderby, List *indexorderbyorig, List *indexorderbyops, ScanDirection indexscandir)
{
  IndexScan *node = makeNode(IndexScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->indexid = indexid;
  node->indexqual = indexqual;
  node->indexqualorig = indexqualorig;
  node->indexorderby = indexorderby;
  node->indexorderbyorig = indexorderbyorig;
  node->indexorderbyops = indexorderbyops;
  node->indexorderdir = indexscandir;

  return node;
}

static IndexOnlyScan *
make_indexonlyscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *recheckqual, List *indexorderby, List *indextlist, ScanDirection indexscandir)
{
  IndexOnlyScan *node = makeNode(IndexOnlyScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->indexid = indexid;
  node->indexqual = indexqual;
  node->recheckqual = recheckqual;
  node->indexorderby = indexorderby;
  node->indextlist = indextlist;
  node->indexorderdir = indexscandir;

  return node;
}

static BitmapIndexScan *
make_bitmap_indexscan(Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig)
{
  BitmapIndexScan *node = makeNode(BitmapIndexScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = NIL;               
  plan->qual = NIL;                     
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->indexid = indexid;
  node->indexqual = indexqual;
  node->indexqualorig = indexqualorig;

  return node;
}

static BitmapHeapScan *
make_bitmap_heapscan(List *qptlist, List *qpqual, Plan *lefttree, List *bitmapqualorig, Index scanrelid)
{
  BitmapHeapScan *node = makeNode(BitmapHeapScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = lefttree;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->bitmapqualorig = bitmapqualorig;

  return node;
}

static TidScan *
make_tidscan(List *qptlist, List *qpqual, Index scanrelid, List *tidquals)
{
  TidScan *node = makeNode(TidScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->tidquals = tidquals;

  return node;
}

static SubqueryScan *
make_subqueryscan(List *qptlist, List *qpqual, Index scanrelid, Plan *subplan)
{
  SubqueryScan *node = makeNode(SubqueryScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->subplan = subplan;

  return node;
}

static FunctionScan *
make_functionscan(List *qptlist, List *qpqual, Index scanrelid, List *functions, bool funcordinality)
{
  FunctionScan *node = makeNode(FunctionScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->functions = functions;
  node->funcordinality = funcordinality;

  return node;
}

static TableFuncScan *
make_tablefuncscan(List *qptlist, List *qpqual, Index scanrelid, TableFunc *tablefunc)
{
  TableFuncScan *node = makeNode(TableFuncScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->tablefunc = tablefunc;

  return node;
}

static ValuesScan *
make_valuesscan(List *qptlist, List *qpqual, Index scanrelid, List *values_lists)
{
  ValuesScan *node = makeNode(ValuesScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->values_lists = values_lists;

  return node;
}

static CteScan *
make_ctescan(List *qptlist, List *qpqual, Index scanrelid, int ctePlanId, int cteParam)
{
  CteScan *node = makeNode(CteScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->ctePlanId = ctePlanId;
  node->cteParam = cteParam;

  return node;
}

static NamedTuplestoreScan *
make_namedtuplestorescan(List *qptlist, List *qpqual, Index scanrelid, char *enrname)
{
  NamedTuplestoreScan *node = makeNode(NamedTuplestoreScan);
  Plan *plan = &node->scan.plan;

                                         
  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->enrname = enrname;

  return node;
}

static WorkTableScan *
make_worktablescan(List *qptlist, List *qpqual, Index scanrelid, int wtParam)
{
  WorkTableScan *node = makeNode(WorkTableScan);
  Plan *plan = &node->scan.plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->wtParam = wtParam;

  return node;
}

ForeignScan *
make_foreignscan(List *qptlist, List *qpqual, Index scanrelid, List *fdw_exprs, List *fdw_private, List *fdw_scan_tlist, List *fdw_recheck_quals, Plan *outer_plan)
{
  ForeignScan *node = makeNode(ForeignScan);
  Plan *plan = &node->scan.plan;

                                                         
  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = outer_plan;
  plan->righttree = NULL;
  node->scan.scanrelid = scanrelid;
  node->operation = CMD_SELECT;
                                                              
  node->fs_server = InvalidOid;
  node->fdw_exprs = fdw_exprs;
  node->fdw_private = fdw_private;
  node->fdw_scan_tlist = fdw_scan_tlist;
  node->fdw_recheck_quals = fdw_recheck_quals;
                                                              
  node->fs_relids = NULL;
                                                                
  node->fsSystemCol = false;

  return node;
}

static RecursiveUnion *
make_recursive_union(List *tlist, Plan *lefttree, Plan *righttree, int wtParam, List *distinctList, long numGroups)
{
  RecursiveUnion *node = makeNode(RecursiveUnion);
  Plan *plan = &node->plan;
  int numCols = list_length(distinctList);

  plan->targetlist = tlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = righttree;
  node->wtParam = wtParam;

     
                                                                           
                                      
     
  node->numCols = numCols;
  if (numCols > 0)
  {
    int keyno = 0;
    AttrNumber *dupColIdx;
    Oid *dupOperators;
    Oid *dupCollations;
    ListCell *slitem;

    dupColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numCols);
    dupOperators = (Oid *)palloc(sizeof(Oid) * numCols);
    dupCollations = (Oid *)palloc(sizeof(Oid) * numCols);

    foreach (slitem, distinctList)
    {
      SortGroupClause *sortcl = (SortGroupClause *)lfirst(slitem);
      TargetEntry *tle = get_sortgroupclause_tle(sortcl, plan->targetlist);

      dupColIdx[keyno] = tle->resno;
      dupOperators[keyno] = sortcl->eqop;
      dupCollations[keyno] = exprCollation((Node *)tle->expr);
      Assert(OidIsValid(dupOperators[keyno]));
      keyno++;
    }
    node->dupColIdx = dupColIdx;
    node->dupOperators = dupOperators;
    node->dupCollations = dupCollations;
  }
  node->numGroups = numGroups;

  return node;
}

static BitmapAnd *
make_bitmap_and(List *bitmapplans)
{
  BitmapAnd *node = makeNode(BitmapAnd);
  Plan *plan = &node->plan;

  plan->targetlist = NIL;
  plan->qual = NIL;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->bitmapplans = bitmapplans;

  return node;
}

static BitmapOr *
make_bitmap_or(List *bitmapplans)
{
  BitmapOr *node = makeNode(BitmapOr);
  Plan *plan = &node->plan;

  plan->targetlist = NIL;
  plan->qual = NIL;
  plan->lefttree = NULL;
  plan->righttree = NULL;
  node->bitmapplans = bitmapplans;

  return node;
}

static NestLoop *
make_nestloop(List *tlist, List *joinclauses, List *otherclauses, List *nestParams, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique)
{
  NestLoop *node = makeNode(NestLoop);
  Plan *plan = &node->join.plan;

  plan->targetlist = tlist;
  plan->qual = otherclauses;
  plan->lefttree = lefttree;
  plan->righttree = righttree;
  node->join.jointype = jointype;
  node->join.inner_unique = inner_unique;
  node->join.joinqual = joinclauses;
  node->nestParams = nestParams;

  return node;
}

static HashJoin *
make_hashjoin(List *tlist, List *joinclauses, List *otherclauses, List *hashclauses, List *hashoperators, List *hashcollations, List *hashkeys, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique)
{
  HashJoin *node = makeNode(HashJoin);
  Plan *plan = &node->join.plan;

  plan->targetlist = tlist;
  plan->qual = otherclauses;
  plan->lefttree = lefttree;
  plan->righttree = righttree;
  node->hashclauses = hashclauses;
  node->hashoperators = hashoperators;
  node->hashcollations = hashcollations;
  node->hashkeys = hashkeys;
  node->join.jointype = jointype;
  node->join.inner_unique = inner_unique;
  node->join.joinqual = joinclauses;

  return node;
}

static Hash *
make_hash(Plan *lefttree, List *hashkeys, Oid skewTable, AttrNumber skewColumn, bool skewInherit)
{
  Hash *node = makeNode(Hash);
  Plan *plan = &node->plan;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

  node->hashkeys = hashkeys;
  node->skewTable = skewTable;
  node->skewColumn = skewColumn;
  node->skewInherit = skewInherit;

  return node;
}

static MergeJoin *
make_mergejoin(List *tlist, List *joinclauses, List *otherclauses, List *mergeclauses, Oid *mergefamilies, Oid *mergecollations, int *mergestrategies, bool *mergenullsfirst, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique, bool skip_mark_restore)
{
  MergeJoin *node = makeNode(MergeJoin);
  Plan *plan = &node->join.plan;

  plan->targetlist = tlist;
  plan->qual = otherclauses;
  plan->lefttree = lefttree;
  plan->righttree = righttree;
  node->skip_mark_restore = skip_mark_restore;
  node->mergeclauses = mergeclauses;
  node->mergeFamilies = mergefamilies;
  node->mergeCollations = mergecollations;
  node->mergeStrategies = mergestrategies;
  node->mergeNullsFirst = mergenullsfirst;
  node->join.jointype = jointype;
  node->join.inner_unique = inner_unique;
  node->join.joinqual = joinclauses;

  return node;
}

   
                                                         
   
                                                                         
                              
   
static Sort *
make_sort(Plan *lefttree, int numCols, AttrNumber *sortColIdx, Oid *sortOperators, Oid *collations, bool *nullsFirst)
{
  Sort *node = makeNode(Sort);
  Plan *plan = &node->plan;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;
  node->numCols = numCols;
  node->sortColIdx = sortColIdx;
  node->sortOperators = sortOperators;
  node->collations = collations;
  node->nullsFirst = nullsFirst;

  return node;
}

   
                              
                                                 
   
                                                                             
                                                                             
                                                                      
   
                     
                                                           
                                                                            
                                                                 
                                                                         
                                                                           
   
                                                                          
                                                                       
                                                                            
                                             
   
                                                                           
                                                                              
                                                                           
         
   
                                                                         
                                                                             
                                                
   
                                                                        
                                                                       
                                                                            
                                                                           
                                                                           
                                                                          
                                                                              
                                                                           
   
                                                                           
                                       
   
static Plan *
prepare_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids, const AttrNumber *reqColIdx, bool adjust_tlist_in_place, int *p_numsortkeys, AttrNumber **p_sortColIdx, Oid **p_sortOperators, Oid **p_collations, bool **p_nullsFirst)
{
  List *tlist = lefttree->targetlist;
  ListCell *i;
  int numsortkeys;
  AttrNumber *sortColIdx;
  Oid *sortOperators;
  Oid *collations;
  bool *nullsFirst;

     
                                                                            
     
  numsortkeys = list_length(pathkeys);
  sortColIdx = (AttrNumber *)palloc(numsortkeys * sizeof(AttrNumber));
  sortOperators = (Oid *)palloc(numsortkeys * sizeof(Oid));
  collations = (Oid *)palloc(numsortkeys * sizeof(Oid));
  nullsFirst = (bool *)palloc(numsortkeys * sizeof(bool));

  numsortkeys = 0;

  foreach (i, pathkeys)
  {
    PathKey *pathkey = (PathKey *)lfirst(i);
    EquivalenceClass *ec = pathkey->pk_eclass;
    EquivalenceMember *em;
    TargetEntry *tle = NULL;
    Oid pk_datatype = InvalidOid;
    Oid sortop;
    ListCell *j;

    if (ec->ec_has_volatile)
    {
         
                                                                     
                                                                       
                                     
         
      if (ec->ec_sortref == 0)                   
      {
        elog(ERROR, "volatile EquivalenceClass has no sortref");
      }
      tle = get_sortgroupref_tle(ec->ec_sortref, tlist);
      Assert(tle);
      Assert(list_length(ec->ec_members) == 1);
      pk_datatype = ((EquivalenceMember *)linitial(ec->ec_members))->em_datatype;
    }
    else if (reqColIdx != NULL)
    {
         
                                                                      
                                                                       
                                                                        
                                                                      
                                                                    
                                                                         
                                                                      
         
      tle = get_tle_by_resno(tlist, reqColIdx[numsortkeys]);
      if (tle)
      {
        em = find_ec_member_for_tle(ec, tle, relids);
        if (em)
        {
                                                  
          pk_datatype = em->em_datatype;
        }
        else
        {
          tle = NULL;
        }
      }
    }
    else
    {
         
                                                                         
                                                                     
                                                                         
                                                                         
                                                                       
                                                                  
                                                                        
                     
         
                                                                         
                                                                       
                                                                       
                                                                        
                                                                       
         
      foreach (j, tlist)
      {
        tle = (TargetEntry *)lfirst(j);
        em = find_ec_member_for_tle(ec, tle, relids);
        if (em)
        {
                                           
          pk_datatype = em->em_datatype;
          break;
        }
        tle = NULL;
      }
    }

    if (!tle)
    {
         
                                                                        
                                                                  
                                                                       
                                                                   
                                                                         
                                                                  
         
      Expr *sortexpr = NULL;

      foreach (j, ec->ec_members)
      {
        EquivalenceMember *em = (EquivalenceMember *)lfirst(j);
        List *exprvars;
        ListCell *k;

           
                                                                       
                                                                      
                    
           
        if (em->em_is_const)
        {
          continue;
        }

           
                                                                    
                   
           
        if (em->em_is_child && !bms_is_subset(em->em_relids, relids))
        {
          continue;
        }

        sortexpr = em->em_expr;
        exprvars = pull_var_clause((Node *)sortexpr, PVC_INCLUDE_AGGREGATES | PVC_INCLUDE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);
        foreach (k, exprvars)
        {
          if (!tlist_member_ignore_relabel(lfirst(k), tlist))
          {
            break;
          }
        }
        list_free(exprvars);
        if (!k)
        {
          pk_datatype = em->em_datatype;
          break;                              
        }
      }
      if (!j)
      {
        elog(ERROR, "could not find pathkey item to sort");
      }

         
                                             
         
      if (!adjust_tlist_in_place && !is_projection_capable_plan(lefttree))
      {
                                                                
        tlist = copyObject(tlist);
        lefttree = inject_projection_plan(lefttree, tlist, lefttree->parallel_safe);
      }

                                                                 
      adjust_tlist_in_place = true;

         
                                            
         
      tle = makeTargetEntry(sortexpr, list_length(tlist) + 1, NULL, true);
      tlist = lappend(tlist, tle);
      lefttree->targetlist = tlist;                              
    }

       
                                                                     
                                  
       
    sortop = get_opfamily_member(pathkey->pk_opfamily, pk_datatype, pk_datatype, pathkey->pk_strategy);
    if (!OidIsValid(sortop))                        
    {
      elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", pathkey->pk_strategy, pk_datatype, pk_datatype, pathkey->pk_opfamily);
    }

                                           
    sortColIdx[numsortkeys] = tle->resno;
    sortOperators[numsortkeys] = sortop;
    collations[numsortkeys] = ec->ec_collation;
    nullsFirst[numsortkeys] = pathkey->pk_nulls_first;
    numsortkeys++;
  }

                      
  *p_numsortkeys = numsortkeys;
  *p_sortColIdx = sortColIdx;
  *p_sortOperators = sortOperators;
  *p_collations = collations;
  *p_nullsFirst = nullsFirst;

  return lefttree;
}

   
                          
                                                                     
   
                                                                      
   
static EquivalenceMember *
find_ec_member_for_tle(EquivalenceClass *ec, TargetEntry *tle, Relids relids)
{
  Expr *tlexpr;
  ListCell *lc;

                                                           
  tlexpr = tle->expr;
  while (tlexpr && IsA(tlexpr, RelabelType))
  {
    tlexpr = ((RelabelType *)tlexpr)->arg;
  }

  foreach (lc, ec->ec_members)
  {
    EquivalenceMember *em = (EquivalenceMember *)lfirst(lc);
    Expr *emexpr;

       
                                                                   
                                                                           
       
    if (em->em_is_const)
    {
      continue;
    }

       
                                                                        
       
    if (em->em_is_child && !bms_is_subset(em->em_relids, relids))
    {
      continue;
    }

                                                            
    emexpr = em->em_expr;
    while (emexpr && IsA(emexpr, RelabelType))
    {
      emexpr = ((RelabelType *)emexpr)->arg;
    }

    if (equal(emexpr, tlexpr))
    {
      return em;
    }
  }

  return NULL;
}

   
                           
                                                          
   
                                                      
                                                                            
                                                                               
   
static Sort *
make_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids)
{
  int numsortkeys;
  AttrNumber *sortColIdx;
  Oid *sortOperators;
  Oid *collations;
  bool *nullsFirst;

                                                               
  lefttree = prepare_sort_from_pathkeys(lefttree, pathkeys, relids, NULL, false, &numsortkeys, &sortColIdx, &sortOperators, &collations, &nullsFirst);

                               
  return make_sort(lefttree, numsortkeys, sortColIdx, sortOperators, collations, nullsFirst);
}

   
                              
                                                             
   
                                             
                                                      
   
Sort *
make_sort_from_sortclauses(List *sortcls, Plan *lefttree)
{
  List *sub_tlist = lefttree->targetlist;
  ListCell *l;
  int numsortkeys;
  AttrNumber *sortColIdx;
  Oid *sortOperators;
  Oid *collations;
  bool *nullsFirst;

                                                                    
  numsortkeys = list_length(sortcls);
  sortColIdx = (AttrNumber *)palloc(numsortkeys * sizeof(AttrNumber));
  sortOperators = (Oid *)palloc(numsortkeys * sizeof(Oid));
  collations = (Oid *)palloc(numsortkeys * sizeof(Oid));
  nullsFirst = (bool *)palloc(numsortkeys * sizeof(bool));

  numsortkeys = 0;
  foreach (l, sortcls)
  {
    SortGroupClause *sortcl = (SortGroupClause *)lfirst(l);
    TargetEntry *tle = get_sortgroupclause_tle(sortcl, sub_tlist);

    sortColIdx[numsortkeys] = tle->resno;
    sortOperators[numsortkeys] = sortcl->sortop;
    collations[numsortkeys] = exprCollation((Node *)tle->expr);
    nullsFirst[numsortkeys] = sortcl->nulls_first;
    numsortkeys++;
  }

  return make_sort(lefttree, numsortkeys, sortColIdx, sortOperators, collations, nullsFirst);
}

   
                            
                                                        
   
                                              
                                               
   
                                                                            
                                                                             
                                                                          
                                                                      
                                             
   
static Sort *
make_sort_from_groupcols(List *groupcls, AttrNumber *grpColIdx, Plan *lefttree)
{
  List *sub_tlist = lefttree->targetlist;
  ListCell *l;
  int numsortkeys;
  AttrNumber *sortColIdx;
  Oid *sortOperators;
  Oid *collations;
  bool *nullsFirst;

                                                                    
  numsortkeys = list_length(groupcls);
  sortColIdx = (AttrNumber *)palloc(numsortkeys * sizeof(AttrNumber));
  sortOperators = (Oid *)palloc(numsortkeys * sizeof(Oid));
  collations = (Oid *)palloc(numsortkeys * sizeof(Oid));
  nullsFirst = (bool *)palloc(numsortkeys * sizeof(bool));

  numsortkeys = 0;
  foreach (l, groupcls)
  {
    SortGroupClause *grpcl = (SortGroupClause *)lfirst(l);
    TargetEntry *tle = get_tle_by_resno(sub_tlist, grpColIdx[numsortkeys]);

    if (!tle)
    {
      elog(ERROR, "could not retrieve tle for sort-from-groupcols");
    }

    sortColIdx[numsortkeys] = tle->resno;
    sortOperators[numsortkeys] = grpcl->sortop;
    collations[numsortkeys] = exprCollation((Node *)tle->expr);
    nullsFirst[numsortkeys] = grpcl->nulls_first;
    numsortkeys++;
  }

  return make_sort(lefttree, numsortkeys, sortColIdx, sortOperators, collations, nullsFirst);
}

static Material *
make_material(Plan *lefttree)
{
  Material *node = makeNode(Material);
  Plan *plan = &node->plan;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

  return node;
}

   
                                                                          
   
                                                                        
                                                                     
                                                                        
                                                            
   
Plan *
materialize_finished_plan(Plan *subplan)
{
  Plan *matplan;
  Path matpath;                                        

  matplan = (Plan *)make_material(subplan);

     
                                                                           
                                                                         
                                                             
                                                                            
                                           
     
  matplan->initPlan = subplan->initPlan;
  subplan->initPlan = NIL;

                     
  cost_material(&matpath, subplan->startup_cost, subplan->total_cost, subplan->plan_rows, subplan->plan_width);
  matplan->startup_cost = matpath.startup_cost;
  matplan->total_cost = matpath.total_cost;
  matplan->plan_rows = subplan->plan_rows;
  matplan->plan_width = subplan->plan_width;
  matplan->parallel_aware = false;
  matplan->parallel_safe = subplan->parallel_safe;

  return matplan;
}

Agg *
make_agg(List *tlist, List *qual, AggStrategy aggstrategy, AggSplit aggsplit, int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations, List *groupingSets, List *chain, double dNumGroups, Plan *lefttree)
{
  Agg *node = makeNode(Agg);
  Plan *plan = &node->plan;
  long numGroups;

                                           
  numGroups = (long)Min(dNumGroups, (double)LONG_MAX);

  node->aggstrategy = aggstrategy;
  node->aggsplit = aggsplit;
  node->numCols = numGroupCols;
  node->grpColIdx = grpColIdx;
  node->grpOperators = grpOperators;
  node->grpCollations = grpCollations;
  node->numGroups = numGroups;
  node->aggParams = NULL;                                        
  node->groupingSets = groupingSets;
  node->chain = chain;

  plan->qual = qual;
  plan->targetlist = tlist;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

  return node;
}

static WindowAgg *
make_windowagg(List *tlist, Index winref, int partNumCols, AttrNumber *partColIdx, Oid *partOperators, Oid *partCollations, int ordNumCols, AttrNumber *ordColIdx, Oid *ordOperators, Oid *ordCollations, int frameOptions, Node *startOffset, Node *endOffset, Oid startInRangeFunc, Oid endInRangeFunc, Oid inRangeColl, bool inRangeAsc, bool inRangeNullsFirst, Plan *lefttree)
{
  WindowAgg *node = makeNode(WindowAgg);
  Plan *plan = &node->plan;

  node->winref = winref;
  node->partNumCols = partNumCols;
  node->partColIdx = partColIdx;
  node->partOperators = partOperators;
  node->partCollations = partCollations;
  node->ordNumCols = ordNumCols;
  node->ordColIdx = ordColIdx;
  node->ordOperators = ordOperators;
  node->ordCollations = ordCollations;
  node->frameOptions = frameOptions;
  node->startOffset = startOffset;
  node->endOffset = endOffset;
  node->startInRangeFunc = startInRangeFunc;
  node->endInRangeFunc = endInRangeFunc;
  node->inRangeColl = inRangeColl;
  node->inRangeAsc = inRangeAsc;
  node->inRangeNullsFirst = inRangeNullsFirst;

  plan->targetlist = tlist;
  plan->lefttree = lefttree;
  plan->righttree = NULL;
                                                
  plan->qual = NIL;

  return node;
}

static Group *
make_group(List *tlist, List *qual, int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations, Plan *lefttree)
{
  Group *node = makeNode(Group);
  Plan *plan = &node->plan;

  node->numCols = numGroupCols;
  node->grpColIdx = grpColIdx;
  node->grpOperators = grpOperators;
  node->grpCollations = grpCollations;

  plan->qual = qual;
  plan->targetlist = tlist;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

  return node;
}

   
                                                                                
                                                                        
                                  
   
static Unique *
make_unique_from_sortclauses(Plan *lefttree, List *distinctList)
{
  Unique *node = makeNode(Unique);
  Plan *plan = &node->plan;
  int numCols = list_length(distinctList);
  int keyno = 0;
  AttrNumber *uniqColIdx;
  Oid *uniqOperators;
  Oid *uniqCollations;
  ListCell *slitem;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

     
                                                                           
                                      
     
  Assert(numCols > 0);
  uniqColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numCols);
  uniqOperators = (Oid *)palloc(sizeof(Oid) * numCols);
  uniqCollations = (Oid *)palloc(sizeof(Oid) * numCols);

  foreach (slitem, distinctList)
  {
    SortGroupClause *sortcl = (SortGroupClause *)lfirst(slitem);
    TargetEntry *tle = get_sortgroupclause_tle(sortcl, plan->targetlist);

    uniqColIdx[keyno] = tle->resno;
    uniqOperators[keyno] = sortcl->eqop;
    uniqCollations[keyno] = exprCollation((Node *)tle->expr);
    Assert(OidIsValid(uniqOperators[keyno]));
    keyno++;
  }

  node->numCols = numCols;
  node->uniqColIdx = uniqColIdx;
  node->uniqOperators = uniqOperators;
  node->uniqCollations = uniqCollations;

  return node;
}

   
                                                                         
   
static Unique *
make_unique_from_pathkeys(Plan *lefttree, List *pathkeys, int numCols)
{
  Unique *node = makeNode(Unique);
  Plan *plan = &node->plan;
  int keyno = 0;
  AttrNumber *uniqColIdx;
  Oid *uniqOperators;
  Oid *uniqCollations;
  ListCell *lc;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

     
                                                                    
                                                                      
                                                          
     
  Assert(numCols >= 0 && numCols <= list_length(pathkeys));
  uniqColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numCols);
  uniqOperators = (Oid *)palloc(sizeof(Oid) * numCols);
  uniqCollations = (Oid *)palloc(sizeof(Oid) * numCols);

  foreach (lc, pathkeys)
  {
    PathKey *pathkey = (PathKey *)lfirst(lc);
    EquivalenceClass *ec = pathkey->pk_eclass;
    EquivalenceMember *em;
    TargetEntry *tle = NULL;
    Oid pk_datatype = InvalidOid;
    Oid eqop;
    ListCell *j;

                                                                
    if (keyno >= numCols)
    {
      break;
    }

    if (ec->ec_has_volatile)
    {
         
                                                                     
                                                                       
                                     
         
      if (ec->ec_sortref == 0)                   
      {
        elog(ERROR, "volatile EquivalenceClass has no sortref");
      }
      tle = get_sortgroupref_tle(ec->ec_sortref, plan->targetlist);
      Assert(tle);
      Assert(list_length(ec->ec_members) == 1);
      pk_datatype = ((EquivalenceMember *)linitial(ec->ec_members))->em_datatype;
    }
    else
    {
         
                                                                         
                                                                       
                               
         
      foreach (j, plan->targetlist)
      {
        tle = (TargetEntry *)lfirst(j);
        em = find_ec_member_for_tle(ec, tle, NULL);
        if (em)
        {
                                           
          pk_datatype = em->em_datatype;
          break;
        }
        tle = NULL;
      }
    }

    if (!tle)
    {
      elog(ERROR, "could not find pathkey item to sort");
    }

       
                                                                         
                                  
       
    eqop = get_opfamily_member(pathkey->pk_opfamily, pk_datatype, pk_datatype, BTEqualStrategyNumber);
    if (!OidIsValid(eqop))                        
    {
      elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", BTEqualStrategyNumber, pk_datatype, pk_datatype, pathkey->pk_opfamily);
    }

    uniqColIdx[keyno] = tle->resno;
    uniqOperators[keyno] = eqop;
    uniqCollations[keyno] = ec->ec_collation;

    keyno++;
  }

  node->numCols = numCols;
  node->uniqColIdx = uniqColIdx;
  node->uniqOperators = uniqOperators;
  node->uniqCollations = uniqCollations;

  return node;
}

static Gather *
make_gather(List *qptlist, List *qpqual, int nworkers, int rescan_param, bool single_copy, Plan *subplan)
{
  Gather *node = makeNode(Gather);
  Plan *plan = &node->plan;

  plan->targetlist = qptlist;
  plan->qual = qpqual;
  plan->lefttree = subplan;
  plan->righttree = NULL;
  node->num_workers = nworkers;
  node->rescan_param = rescan_param;
  node->single_copy = single_copy;
  node->invisible = false;
  node->initParam = NULL;

  return node;
}

   
                                                                          
                                                                             
                                  
   
static SetOp *
make_setop(SetOpCmd cmd, SetOpStrategy strategy, Plan *lefttree, List *distinctList, AttrNumber flagColIdx, int firstFlag, long numGroups)
{
  SetOp *node = makeNode(SetOp);
  Plan *plan = &node->plan;
  int numCols = list_length(distinctList);
  int keyno = 0;
  AttrNumber *dupColIdx;
  Oid *dupOperators;
  Oid *dupCollations;
  ListCell *slitem;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

     
                                                                           
                                      
     
  dupColIdx = (AttrNumber *)palloc(sizeof(AttrNumber) * numCols);
  dupOperators = (Oid *)palloc(sizeof(Oid) * numCols);
  dupCollations = (Oid *)palloc(sizeof(Oid) * numCols);

  foreach (slitem, distinctList)
  {
    SortGroupClause *sortcl = (SortGroupClause *)lfirst(slitem);
    TargetEntry *tle = get_sortgroupclause_tle(sortcl, plan->targetlist);

    dupColIdx[keyno] = tle->resno;
    dupOperators[keyno] = sortcl->eqop;
    dupCollations[keyno] = exprCollation((Node *)tle->expr);
    Assert(OidIsValid(dupOperators[keyno]));
    keyno++;
  }

  node->cmd = cmd;
  node->strategy = strategy;
  node->numCols = numCols;
  node->dupColIdx = dupColIdx;
  node->dupOperators = dupOperators;
  node->dupCollations = dupCollations;
  node->flagColIdx = flagColIdx;
  node->firstFlag = firstFlag;
  node->numGroups = numGroups;

  return node;
}

   
                 
                                
   
static LockRows *
make_lockrows(Plan *lefttree, List *rowMarks, int epqParam)
{
  LockRows *node = makeNode(LockRows);
  Plan *plan = &node->plan;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

  node->rowMarks = rowMarks;
  node->epqParam = epqParam;

  return node;
}

   
              
                             
   
Limit *
make_limit(Plan *lefttree, Node *limitOffset, Node *limitCount)
{
  Limit *node = makeNode(Limit);
  Plan *plan = &node->plan;

  plan->targetlist = lefttree->targetlist;
  plan->qual = NIL;
  plan->lefttree = lefttree;
  plan->righttree = NULL;

  node->limitOffset = limitOffset;
  node->limitCount = limitCount;

  return node;
}

   
               
                              
   
static Result *
make_result(List *tlist, Node *resconstantqual, Plan *subplan)
{
  Result *node = makeNode(Result);
  Plan *plan = &node->plan;

  plan->targetlist = tlist;
  plan->qual = NIL;
  plan->lefttree = subplan;
  plan->righttree = NULL;
  node->resconstantqual = resconstantqual;

  return node;
}

   
                    
                                  
   
static ProjectSet *
make_project_set(List *tlist, Plan *subplan)
{
  ProjectSet *node = makeNode(ProjectSet);
  Plan *plan = &node->plan;

  plan->targetlist = tlist;
  plan->qual = NIL;
  plan->lefttree = subplan;
  plan->righttree = NULL;

  return node;
}

   
                    
                                   
   
static ModifyTable *
make_modifytable(PlannerInfo *root, CmdType operation, bool canSetTag, Index nominalRelation, Index rootRelation, bool partColsUpdated, List *resultRelations, List *subplans, List *subroots, List *withCheckOptionLists, List *returningLists, List *rowMarks, OnConflictExpr *onconflict, int epqParam)
{
  ModifyTable *node = makeNode(ModifyTable);
  List *fdw_private_list;
  Bitmapset *direct_modify_plans;
  ListCell *lc;
  ListCell *lc2;
  int i;

  Assert(list_length(resultRelations) == list_length(subplans));
  Assert(list_length(resultRelations) == list_length(subroots));
  Assert(withCheckOptionLists == NIL || list_length(resultRelations) == list_length(withCheckOptionLists));
  Assert(returningLists == NIL || list_length(resultRelations) == list_length(returningLists));

  node->plan.lefttree = NULL;
  node->plan.righttree = NULL;
  node->plan.qual = NIL;
                                                        
  node->plan.targetlist = NIL;

  node->operation = operation;
  node->canSetTag = canSetTag;
  node->nominalRelation = nominalRelation;
  node->rootRelation = rootRelation;
  node->partColsUpdated = partColsUpdated;
  node->resultRelations = resultRelations;
  node->resultRelIndex = -1;                                             
  node->rootResultRelIndex = -1;                                         
  node->plans = subplans;
  if (!onconflict)
  {
    node->onConflictAction = ONCONFLICT_NONE;
    node->onConflictSet = NIL;
    node->onConflictWhere = NULL;
    node->arbiterIndexes = NIL;
    node->exclRelRTI = 0;
    node->exclRelTlist = NIL;
  }
  else
  {
    node->onConflictAction = onconflict->action;
    node->onConflictSet = onconflict->onConflictSet;
    node->onConflictWhere = onconflict->onConflictWhere;

       
                                                                    
                                                                   
                                                                 
                   
       
    node->arbiterIndexes = infer_arbiter_indexes(root);

    node->exclRelRTI = onconflict->exclRelIndex;
    node->exclRelTlist = onconflict->exclRelTlist;
  }
  node->withCheckOptionLists = withCheckOptionLists;
  node->returningLists = returningLists;
  node->rowMarks = rowMarks;
  node->epqParam = epqParam;

     
                                                                        
                                                                     
     
  fdw_private_list = NIL;
  direct_modify_plans = NULL;
  i = 0;
  forboth(lc, resultRelations, lc2, subroots)
  {
    Index rti = lfirst_int(lc);
    PlannerInfo *subroot = lfirst_node(PlannerInfo, lc2);
    FdwRoutine *fdwroutine;
    List *fdw_private;
    bool direct_modify;

       
                                                                          
                                                                         
                                                                         
                                                                  
                                                              
       
    if (rti < subroot->simple_rel_array_size && subroot->simple_rel_array[rti] != NULL)
    {
      RelOptInfo *resultRel = subroot->simple_rel_array[rti];

      fdwroutine = resultRel->fdwroutine;
    }
    else
    {
      RangeTblEntry *rte = planner_rt_fetch(rti, subroot);

      Assert(rte->rtekind == RTE_RELATION);
      if (rte->relkind == RELKIND_FOREIGN_TABLE)
      {
        fdwroutine = GetFdwRoutineByRelId(rte->relid);
      }
      else
      {
        fdwroutine = NULL;
      }
    }

       
                                                                        
                                                                     
                                                                       
                                                                           
                                  
       
    direct_modify = false;
    if (fdwroutine != NULL && fdwroutine->PlanDirectModify != NULL && fdwroutine->BeginDirectModify != NULL && fdwroutine->IterateDirectModify != NULL && fdwroutine->EndDirectModify != NULL && withCheckOptionLists == NIL && !has_row_triggers(subroot, rti, operation) && !has_stored_generated_columns(subroot, rti))
    {
      direct_modify = fdwroutine->PlanDirectModify(subroot, node, rti, i);
    }
    if (direct_modify)
    {
      direct_modify_plans = bms_add_member(direct_modify_plans, i);
    }

    if (!direct_modify && fdwroutine != NULL && fdwroutine->PlanForeignModify != NULL)
    {
      fdw_private = fdwroutine->PlanForeignModify(subroot, node, rti, i);
    }
    else
    {
      fdw_private = NIL;
    }
    fdw_private_list = lappend(fdw_private_list, fdw_private);
    i++;
  }
  node->fdwPrivLists = fdw_private_list;
  node->fdwDirectModifyPlans = direct_modify_plans;

  return node;
}

   
                              
                                                              
   
bool
is_projection_capable_path(Path *path)
{
                                                                     
  switch (path->pathtype)
  {
  case T_Hash:
  case T_Material:
  case T_Sort:
  case T_Unique:
  case T_SetOp:
  case T_LockRows:
  case T_Limit:
  case T_ModifyTable:
  case T_MergeAppend:
  case T_RecursiveUnion:
    return false;
  case T_Append:

       
                                                                   
                                                                    
                                 
       
    return IS_DUMMY_APPEND(path);
  case T_ProjectSet:

       
                                                                   
                                                                 
                                                                       
                          
       
    return false;
  default:
    break;
  }
  return true;
}

   
                              
                                                              
   
bool
is_projection_capable_plan(Plan *plan)
{
                                                                     
  switch (nodeTag(plan))
  {
  case T_Hash:
  case T_Material:
  case T_Sort:
  case T_Unique:
  case T_SetOp:
  case T_LockRows:
  case T_Limit:
  case T_ModifyTable:
  case T_Append:
  case T_MergeAppend:
  case T_RecursiveUnion:
    return false;
  case T_ProjectSet:

       
                                                                   
                                                                 
                                                                       
                          
       
    return false;
  default:
    break;
  }
  return true;
}
