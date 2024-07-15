                                                                            
   
             
                                                                         
                                                     
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/transam.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/tlist.h"
#include "tcop/utility.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

typedef struct
{
  Index varno;                              
  AttrNumber varattno;                         
  AttrNumber resno;                             
} tlist_vinfo;

typedef struct
{
  List *tlist;                                                         
  int num_vars;                                                                   
  bool has_ph_vars;                                                               
  bool has_non_vars;                                                     
  tlist_vinfo vars[FLEXIBLE_ARRAY_MEMBER];                           
} indexed_tlist;

typedef struct
{
  PlannerInfo *root;
  int rtoffset;
} fix_scan_expr_context;

typedef struct
{
  PlannerInfo *root;
  indexed_tlist *outer_itlist;
  indexed_tlist *inner_itlist;
  Index acceptable_rel;
  int rtoffset;
} fix_join_expr_context;

typedef struct
{
  PlannerInfo *root;
  indexed_tlist *subplan_itlist;
  Index newvarno;
  int rtoffset;
} fix_upper_expr_context;

   
                                                                        
                                                                           
                                                                         
                                                                          
                                         
   
#define ISREGCLASSCONST(con) (((con)->consttype == REGCLASSOID || (con)->consttype == OIDOID) && !(con)->constisnull)

#define fix_scan_list(root, lst, rtoffset) ((List *)fix_scan_expr(root, (Node *)(lst), rtoffset))

static void
add_rtes_to_flat_rtable(PlannerInfo *root, bool recursing);
static void
flatten_unplanned_rtes(PlannerGlobal *glob, RangeTblEntry *rte);
static bool
flatten_rtes_walker(Node *node, PlannerGlobal *glob);
static void
add_rte_to_flat_rtable(PlannerGlobal *glob, RangeTblEntry *rte);
static Plan *
set_plan_refs(PlannerInfo *root, Plan *plan, int rtoffset);
static Plan *
set_indexonlyscan_references(PlannerInfo *root, IndexOnlyScan *plan, int rtoffset);
static Plan *
set_subqueryscan_references(PlannerInfo *root, SubqueryScan *plan, int rtoffset);
static bool
trivial_subqueryscan(SubqueryScan *plan);
static Plan *
clean_up_removed_plan_level(Plan *parent, Plan *child);
static void
set_foreignscan_references(PlannerInfo *root, ForeignScan *fscan, int rtoffset);
static void
set_customscan_references(PlannerInfo *root, CustomScan *cscan, int rtoffset);
static Plan *
set_append_references(PlannerInfo *root, Append *aplan, int rtoffset);
static Plan *
set_mergeappend_references(PlannerInfo *root, MergeAppend *mplan, int rtoffset);
static void
set_hash_references(PlannerInfo *root, Plan *plan, int rtoffset);
static Node *
fix_scan_expr(PlannerInfo *root, Node *node, int rtoffset);
static Node *
fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context);
static bool
fix_scan_expr_walker(Node *node, fix_scan_expr_context *context);
static void
set_join_references(PlannerInfo *root, Join *join, int rtoffset);
static void
set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset);
static void
set_param_references(PlannerInfo *root, Plan *plan);
static Node *
convert_combining_aggrefs(Node *node, void *context);
static void
set_dummy_tlist_references(Plan *plan, int rtoffset);
static indexed_tlist *
build_tlist_index(List *tlist);
static Var *
search_indexed_tlist_for_var(Var *var, indexed_tlist *itlist, Index newvarno, int rtoffset);
static Var *
search_indexed_tlist_for_non_var(Expr *node, indexed_tlist *itlist, Index newvarno);
static Var *
search_indexed_tlist_for_sortgroupref(Expr *node, Index sortgroupref, indexed_tlist *itlist, Index newvarno);
static List *
fix_join_expr(PlannerInfo *root, List *clauses, indexed_tlist *outer_itlist, indexed_tlist *inner_itlist, Index acceptable_rel, int rtoffset);
static Node *
fix_join_expr_mutator(Node *node, fix_join_expr_context *context);
static Node *
fix_upper_expr(PlannerInfo *root, Node *node, indexed_tlist *subplan_itlist, Index newvarno, int rtoffset);
static Node *
fix_upper_expr_mutator(Node *node, fix_upper_expr_context *context);
static List *
set_returning_clause_references(PlannerInfo *root, List *rlist, Plan *topplan, Index resultRelation, int rtoffset);

                                                                               
   
                       
   
                                                                               

   
                       
   
                                                                         
                                                                          
                                        
   
                                                                          
                                                                      
   
                                                                              
   
                                                                          
             
   
                                                                            
                                                         
   
                                                                        
                                                              
   
                                                                         
                             
   
                                                                    
                                                                           
                                                                         
                                                                            
                                                                          
                                                          
   
                                                         
   
                                                                   
                                                                      
                                                            
                                                                         
                                                                      
                                                                           
                                                                            
                                                                           
                                                                       
                                                                             
                                                                          
                        
   
                                                                  
   
                                                                         
                                                                       
   
                                                                             
                                                                             
                                                                              
                                                                              
                                                     
   
                                                                              
                                                                            
                                                                             
                                                              
   
Plan *
set_plan_references(PlannerInfo *root, Plan *plan)
{
  PlannerGlobal *glob = root->glob;
  int rtoffset = list_length(glob->finalrtable);
  ListCell *lc;

     
                                                                          
                                                                            
                                                                          
     
  add_rtes_to_flat_rtable(root, false);

     
                                                                      
     
  foreach (lc, root->rowMarks)
  {
    PlanRowMark *rc = lfirst_node(PlanRowMark, lc);
    PlanRowMark *newrc;

                                                          
    newrc = (PlanRowMark *)palloc(sizeof(PlanRowMark));
    memcpy(newrc, rc, sizeof(PlanRowMark));

                                                    
    newrc->rti += rtoffset;
    newrc->prti += rtoffset;

    glob->finalrowmarks = lappend(glob->finalrowmarks, newrc);
  }

                             
  return set_plan_refs(root, plan, rtoffset);
}

   
                                                                              
   
                                                                    
   
static void
add_rtes_to_flat_rtable(PlannerInfo *root, bool recursing)
{
  PlannerGlobal *glob = root->glob;
  Index rti;
  ListCell *lc;

     
                                                           
     
                                                                     
                                                                      
                                                             
     
  foreach (lc, root->parse->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

    if (!recursing || rte->rtekind == RTE_RELATION)
    {
      add_rte_to_flat_rtable(glob, rte);
    }
  }

     
                                                                           
                                                                         
                                                                           
                                                                          
     
                                                                             
                                                                           
                           
     
  rti = 1;
  foreach (lc, root->parse->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

       
                                                                          
                                                                        
                                                                     
                  
       
    if (rte->rtekind == RTE_SUBQUERY && !rte->inh && rti < root->simple_rel_array_size)
    {
      RelOptInfo *rel = root->simple_rel_array[rti];

      if (rel != NULL)
      {
        Assert(rel->relid == rti);                            

           
                                                                    
                                                                       
                                                   
                                   
           
                                                                    
                                                            
                                                                    
           
                                                                      
                                                                       
                                      
           
                                                                    
                                                                     
                                                                      
                                                               
           
        if (rel->subroot == NULL)
        {
          flatten_unplanned_rtes(glob, rte);
        }
        else if (recursing || IS_DUMMY_REL(fetch_upper_rel(rel->subroot, UPPERREL_FINAL, NULL)))
        {
          add_rtes_to_flat_rtable(rel->subroot, true);
        }
      }
    }
    rti++;
  }
}

   
                                                                         
   
static void
flatten_unplanned_rtes(PlannerGlobal *glob, RangeTblEntry *rte)
{
                                                                
  (void)query_tree_walker(rte->subquery, flatten_rtes_walker, (void *)glob, QTW_EXAMINE_RTES_BEFORE);
}

static bool
flatten_rtes_walker(Node *node, PlannerGlobal *glob)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, RangeTblEntry))
  {
    RangeTblEntry *rte = (RangeTblEntry *)node;

                                                   
    if (rte->rtekind == RTE_RELATION)
    {
      add_rte_to_flat_rtable(glob, rte);
    }
    return false;
  }
  if (IsA(node, Query))
  {
                                 
    return query_tree_walker((Query *)node, flatten_rtes_walker, (void *)glob, QTW_EXAMINE_RTES_BEFORE);
  }
  return expression_tree_walker(node, flatten_rtes_walker, (void *)glob);
}

   
                                                         
   
                                                                          
                                                                           
                                                                             
                                                                    
                                                                   
                                                                         
   
static void
add_rte_to_flat_rtable(PlannerGlobal *glob, RangeTblEntry *rte)
{
  RangeTblEntry *newrte;

                                                    
  newrte = (RangeTblEntry *)palloc(sizeof(RangeTblEntry));
  memcpy(newrte, rte, sizeof(RangeTblEntry));

                                  
  newrte->tablesample = NULL;
  newrte->subquery = NULL;
  newrte->joinaliasvars = NIL;
  newrte->functions = NIL;
  newrte->tablefunc = NULL;
  newrte->values_lists = NIL;
  newrte->coltypes = NIL;
  newrte->coltypmods = NIL;
  newrte->colcollations = NIL;
  newrte->securityQuals = NIL;

  glob->finalrtable = lappend(glob->finalrtable, newrte);

     
                                                                            
                                                                            
             
     
  if (IS_SPECIAL_VARNO(list_length(glob->finalrtable)))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many range table entries")));
  }

     
                                                                  
     
                                                                            
                                                                            
                                                                      
                                                                         
     
                                                                             
                                                                
     
  if (newrte->rtekind == RTE_RELATION)
  {
    glob->relationOids = lappend_oid(glob->relationOids, newrte->relid);
  }
}

   
                                                                            
   
static Plan *
set_plan_refs(PlannerInfo *root, Plan *plan, int rtoffset)
{
  ListCell *l;

  if (plan == NULL)
  {
    return NULL;
  }

                                     
  plan->plan_node_id = root->glob->lastPlanNodeId++;

     
                              
     
  switch (nodeTag(plan))
  {
  case T_SeqScan:
  {
    SeqScan *splan = (SeqScan *)plan;

    splan->scanrelid += rtoffset;
    splan->plan.targetlist = fix_scan_list(root, splan->plan.targetlist, rtoffset);
    splan->plan.qual = fix_scan_list(root, splan->plan.qual, rtoffset);
  }
  break;
  case T_SampleScan:
  {
    SampleScan *splan = (SampleScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->tablesample = (TableSampleClause *)fix_scan_expr(root, (Node *)splan->tablesample, rtoffset);
  }
  break;
  case T_IndexScan:
  {
    IndexScan *splan = (IndexScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->indexqual = fix_scan_list(root, splan->indexqual, rtoffset);
    splan->indexqualorig = fix_scan_list(root, splan->indexqualorig, rtoffset);
    splan->indexorderby = fix_scan_list(root, splan->indexorderby, rtoffset);
    splan->indexorderbyorig = fix_scan_list(root, splan->indexorderbyorig, rtoffset);
  }
  break;
  case T_IndexOnlyScan:
  {
    IndexOnlyScan *splan = (IndexOnlyScan *)plan;

    return set_indexonlyscan_references(root, splan, rtoffset);
  }
  break;
  case T_BitmapIndexScan:
  {
    BitmapIndexScan *splan = (BitmapIndexScan *)plan;

    splan->scan.scanrelid += rtoffset;
                                            
    Assert(splan->scan.plan.targetlist == NIL);
    Assert(splan->scan.plan.qual == NIL);
    splan->indexqual = fix_scan_list(root, splan->indexqual, rtoffset);
    splan->indexqualorig = fix_scan_list(root, splan->indexqualorig, rtoffset);
  }
  break;
  case T_BitmapHeapScan:
  {
    BitmapHeapScan *splan = (BitmapHeapScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->bitmapqualorig = fix_scan_list(root, splan->bitmapqualorig, rtoffset);
  }
  break;
  case T_TidScan:
  {
    TidScan *splan = (TidScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->tidquals = fix_scan_list(root, splan->tidquals, rtoffset);
  }
  break;
  case T_SubqueryScan:
                                                     
    return set_subqueryscan_references(root, (SubqueryScan *)plan, rtoffset);
  case T_FunctionScan:
  {
    FunctionScan *splan = (FunctionScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->functions = fix_scan_list(root, splan->functions, rtoffset);
  }
  break;
  case T_TableFuncScan:
  {
    TableFuncScan *splan = (TableFuncScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->tablefunc = (TableFunc *)fix_scan_expr(root, (Node *)splan->tablefunc, rtoffset);
  }
  break;
  case T_ValuesScan:
  {
    ValuesScan *splan = (ValuesScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
    splan->values_lists = fix_scan_list(root, splan->values_lists, rtoffset);
  }
  break;
  case T_CteScan:
  {
    CteScan *splan = (CteScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
  }
  break;
  case T_NamedTuplestoreScan:
  {
    NamedTuplestoreScan *splan = (NamedTuplestoreScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
  }
  break;
  case T_WorkTableScan:
  {
    WorkTableScan *splan = (WorkTableScan *)plan;

    splan->scan.scanrelid += rtoffset;
    splan->scan.plan.targetlist = fix_scan_list(root, splan->scan.plan.targetlist, rtoffset);
    splan->scan.plan.qual = fix_scan_list(root, splan->scan.plan.qual, rtoffset);
  }
  break;
  case T_ForeignScan:
    set_foreignscan_references(root, (ForeignScan *)plan, rtoffset);
    break;
  case T_CustomScan:
    set_customscan_references(root, (CustomScan *)plan, rtoffset);
    break;

  case T_NestLoop:
  case T_MergeJoin:
  case T_HashJoin:
    set_join_references(root, (Join *)plan, rtoffset);
    break;

  case T_Gather:
  case T_GatherMerge:
  {
    set_upper_references(root, plan, rtoffset);
    set_param_references(root, plan);
  }
  break;

  case T_Hash:
    set_hash_references(root, plan, rtoffset);
    break;

  case T_Material:
  case T_Sort:
  case T_Unique:
  case T_SetOp:

       
                                                                
                                                                    
                                                                
                                                                  
                                                                       
       
    set_dummy_tlist_references(plan, rtoffset);

       
                                                                      
                                                  
       
    Assert(plan->qual == NIL);
    break;
  case T_LockRows:
  {
    LockRows *splan = (LockRows *)plan;

       
                                                                
                                                                
                     
       
    set_dummy_tlist_references(plan, rtoffset);
    Assert(splan->plan.qual == NIL);

    foreach (l, splan->rowMarks)
    {
      PlanRowMark *rc = (PlanRowMark *)lfirst(l);

      rc->rti += rtoffset;
      rc->prti += rtoffset;
    }
  }
  break;
  case T_Limit:
  {
    Limit *splan = (Limit *)plan;

       
                                                                   
                                                                  
                                                                   
                                     
       
    set_dummy_tlist_references(plan, rtoffset);
    Assert(splan->plan.qual == NIL);

    splan->limitOffset = fix_scan_expr(root, splan->limitOffset, rtoffset);
    splan->limitCount = fix_scan_expr(root, splan->limitCount, rtoffset);
  }
  break;
  case T_Agg:
  {
    Agg *agg = (Agg *)plan;

       
                                                                 
                                                             
                                                               
                                 
       
    if (DO_AGGSPLIT_COMBINE(agg->aggsplit))
    {
      plan->targetlist = (List *)convert_combining_aggrefs((Node *)plan->targetlist, NULL);
      plan->qual = (List *)convert_combining_aggrefs((Node *)plan->qual, NULL);
    }

    set_upper_references(root, plan, rtoffset);
  }
  break;
  case T_Group:
    set_upper_references(root, plan, rtoffset);
    break;
  case T_WindowAgg:
  {
    WindowAgg *wplan = (WindowAgg *)plan;

    set_upper_references(root, plan, rtoffset);

       
                                                               
                                                              
                                                       
       
    wplan->startOffset = fix_scan_expr(root, wplan->startOffset, rtoffset);
    wplan->endOffset = fix_scan_expr(root, wplan->endOffset, rtoffset);
  }
  break;
  case T_Result:
  {
    Result *splan = (Result *)plan;

       
                                                               
                                            
       
    if (splan->plan.lefttree != NULL)
    {
      set_upper_references(root, plan, rtoffset);
    }
    else
    {
      splan->plan.targetlist = fix_scan_list(root, splan->plan.targetlist, rtoffset);
      splan->plan.qual = fix_scan_list(root, splan->plan.qual, rtoffset);
    }
                                                                 
    splan->resconstantqual = fix_scan_expr(root, splan->resconstantqual, rtoffset);
  }
  break;
  case T_ProjectSet:
    set_upper_references(root, plan, rtoffset);
    break;
  case T_ModifyTable:
  {
    ModifyTable *splan = (ModifyTable *)plan;

    Assert(splan->plan.targetlist == NIL);
    Assert(splan->plan.qual == NIL);

    splan->withCheckOptionLists = fix_scan_list(root, splan->withCheckOptionLists, rtoffset);

    if (splan->returningLists)
    {
      List *newRL = NIL;
      ListCell *lcrl, *lcrr, *lcp;

         
                                                     
                                            
         
      Assert(list_length(splan->returningLists) == list_length(splan->resultRelations));
      Assert(list_length(splan->returningLists) == list_length(splan->plans));
      forthree(lcrl, splan->returningLists, lcrr, splan->resultRelations, lcp, splan->plans)
      {
        List *rlist = (List *)lfirst(lcrl);
        Index resultrel = lfirst_int(lcrr);
        Plan *subplan = (Plan *)lfirst(lcp);

        rlist = set_returning_clause_references(root, rlist, subplan, resultrel, rtoffset);
        newRL = lappend(newRL, rlist);
      }
      splan->returningLists = newRL;

         
                                                                 
                                                          
                                                              
                                                               
                                                               
                                         
         
      splan->plan.targetlist = copyObject(linitial(newRL));
    }

       
                                                                  
                                                          
                                                                   
                                                                  
                                                                  
                           
       
    if (splan->onConflictSet)
    {
      indexed_tlist *itlist;

      itlist = build_tlist_index(splan->exclRelTlist);

      splan->onConflictSet = fix_join_expr(root, splan->onConflictSet, NULL, itlist, linitial_int(splan->resultRelations), rtoffset);

      splan->onConflictWhere = (Node *)fix_join_expr(root, (List *)splan->onConflictWhere, NULL, itlist, linitial_int(splan->resultRelations), rtoffset);

      pfree(itlist);

      splan->exclRelTlist = fix_scan_list(root, splan->exclRelTlist, rtoffset);
    }

    splan->nominalRelation += rtoffset;
    if (splan->rootRelation)
    {
      splan->rootRelation += rtoffset;
    }
    splan->exclRelRTI += rtoffset;

    foreach (l, splan->resultRelations)
    {
      lfirst_int(l) += rtoffset;
    }
    foreach (l, splan->rowMarks)
    {
      PlanRowMark *rc = (PlanRowMark *)lfirst(l);

      rc->rti += rtoffset;
      rc->prti += rtoffset;
    }
    foreach (l, splan->plans)
    {
      lfirst(l) = set_plan_refs(root, (Plan *)lfirst(l), rtoffset);
    }

       
                                                               
                                                              
                                                                
                    
       
    splan->resultRelIndex = list_length(root->glob->resultRelations);
    root->glob->resultRelations = list_concat(root->glob->resultRelations, list_copy(splan->resultRelations));

       
                                                                
                                                                 
                                                                  
       
    if (splan->rootRelation)
    {
      splan->rootResultRelIndex = list_length(root->glob->rootResultRelations);
      root->glob->rootResultRelations = lappend_int(root->glob->rootResultRelations, splan->rootRelation);
    }
  }
  break;
  case T_Append:
                                                     
    return set_append_references(root, (Append *)plan, rtoffset);
  case T_MergeAppend:
                                                     
    return set_mergeappend_references(root, (MergeAppend *)plan, rtoffset);
  case T_RecursiveUnion:
                                                                
    set_dummy_tlist_references(plan, rtoffset);
    Assert(plan->qual == NIL);
    break;
  case T_BitmapAnd:
  {
    BitmapAnd *splan = (BitmapAnd *)plan;

                                                       
    Assert(splan->plan.targetlist == NIL);
    Assert(splan->plan.qual == NIL);
    foreach (l, splan->bitmapplans)
    {
      lfirst(l) = set_plan_refs(root, (Plan *)lfirst(l), rtoffset);
    }
  }
  break;
  case T_BitmapOr:
  {
    BitmapOr *splan = (BitmapOr *)plan;

                                                      
    Assert(splan->plan.targetlist == NIL);
    Assert(splan->plan.qual == NIL);
    foreach (l, splan->bitmapplans)
    {
      lfirst(l) = set_plan_refs(root, (Plan *)lfirst(l), rtoffset);
    }
  }
  break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(plan));
    break;
  }

     
                                          
     
                                                                         
                                                                       
                                                                       
                                                                          
     
  plan->lefttree = set_plan_refs(root, plan->lefttree, rtoffset);
  plan->righttree = set_plan_refs(root, plan->righttree, rtoffset);

  return plan;
}

   
                                
                                                          
   
                                                                       
                                                                      
                                                                       
                                            
   
static Plan *
set_indexonlyscan_references(PlannerInfo *root, IndexOnlyScan *plan, int rtoffset)
{
  indexed_tlist *index_itlist;
  List *stripped_indextlist;
  ListCell *lc;

     
                                                                         
                                                                         
                                                                            
                                                                         
                                                                         
     
  stripped_indextlist = NIL;
  foreach (lc, plan->indextlist)
  {
    TargetEntry *indextle = (TargetEntry *)lfirst(lc);

    if (!indextle->resjunk)
    {
      stripped_indextlist = lappend(stripped_indextlist, indextle);
    }
  }

  index_itlist = build_tlist_index(stripped_indextlist);

  plan->scan.scanrelid += rtoffset;
  plan->scan.plan.targetlist = (List *)fix_upper_expr(root, (Node *)plan->scan.plan.targetlist, index_itlist, INDEX_VAR, rtoffset);
  plan->scan.plan.qual = (List *)fix_upper_expr(root, (Node *)plan->scan.plan.qual, index_itlist, INDEX_VAR, rtoffset);
  plan->recheckqual = (List *)fix_upper_expr(root, (Node *)plan->recheckqual, index_itlist, INDEX_VAR, rtoffset);
                                                                   
  plan->indexqual = fix_scan_list(root, plan->indexqual, rtoffset);
                                                                      
  plan->indexorderby = fix_scan_list(root, plan->indexorderby, rtoffset);
                                                                     
  plan->indextlist = fix_scan_list(root, plan->indextlist, rtoffset);

  pfree(index_itlist);

  return (Plan *)plan;
}

   
                               
                                                        
   
                                                                       
                                      
   
static Plan *
set_subqueryscan_references(PlannerInfo *root, SubqueryScan *plan, int rtoffset)
{
  RelOptInfo *rel;
  Plan *result;

                                                                            
  rel = find_base_rel(root, plan->scan.scanrelid);

                                       
  plan->subplan = set_plan_references(rel->subroot, plan->subplan);

  if (trivial_subqueryscan(plan))
  {
       
                                                                       
       
    result = clean_up_removed_plan_level((Plan *)plan, plan->subplan);
  }
  else
  {
       
                                                                      
                                                                          
                                                                       
                                                                         
                              
       
    plan->scan.scanrelid += rtoffset;
    plan->scan.plan.targetlist = fix_scan_list(root, plan->scan.plan.targetlist, rtoffset);
    plan->scan.plan.qual = fix_scan_list(root, plan->scan.plan.qual, rtoffset);

    result = (Plan *)plan;
  }

  return result;
}

   
                        
                                                                     
   
                                                                       
                                              
   
static bool
trivial_subqueryscan(SubqueryScan *plan)
{
  int attrno;
  ListCell *lp, *lc;

  if (plan->scan.plan.qual != NIL)
  {
    return false;
  }

  if (list_length(plan->scan.plan.targetlist) != list_length(plan->subplan->targetlist))
  {
    return false;                             
  }

  attrno = 1;
  forboth(lp, plan->scan.plan.targetlist, lc, plan->subplan->targetlist)
  {
    TargetEntry *ptle = (TargetEntry *)lfirst(lp);
    TargetEntry *ctle = (TargetEntry *)lfirst(lc);

    if (ptle->resjunk != ctle->resjunk)
    {
      return false;                                      
    }

       
                                                                           
                                                                   
                                              
       
    if (ptle->expr && IsA(ptle->expr, Var))
    {
      Var *var = (Var *)ptle->expr;

      Assert(var->varno == plan->scan.scanrelid);
      Assert(var->varlevelsup == 0);
      if (var->varattno != attrno)
      {
        return false;                   
      }
    }
    else if (ptle->expr && IsA(ptle->expr, Const))
    {
      if (!equal(ptle->expr, ctle->expr))
      {
        return false;
      }
    }
    else
    {
      return false;
    }

    attrno++;
  }

  return true;
}

   
                               
                                                                       
   
                                                                             
                                  
   
static Plan *
clean_up_removed_plan_level(Plan *parent, Plan *child)
{
                                                      
  child->initPlan = list_concat(parent->initPlan, child->initPlan);

     
                                                                         
                                                                           
                                                                         
     
  apply_tlist_labeling(child->targetlist, parent->targetlist);

  return child;
}

   
                              
                                                         
   
static void
set_foreignscan_references(PlannerInfo *root, ForeignScan *fscan, int rtoffset)
{
                                      
  if (fscan->scan.scanrelid > 0)
  {
    fscan->scan.scanrelid += rtoffset;
  }

  if (fscan->fdw_scan_tlist != NIL || fscan->scan.scanrelid == 0)
  {
       
                                                                     
                          
       
    indexed_tlist *itlist = build_tlist_index(fscan->fdw_scan_tlist);

    fscan->scan.plan.targetlist = (List *)fix_upper_expr(root, (Node *)fscan->scan.plan.targetlist, itlist, INDEX_VAR, rtoffset);
    fscan->scan.plan.qual = (List *)fix_upper_expr(root, (Node *)fscan->scan.plan.qual, itlist, INDEX_VAR, rtoffset);
    fscan->fdw_exprs = (List *)fix_upper_expr(root, (Node *)fscan->fdw_exprs, itlist, INDEX_VAR, rtoffset);
    fscan->fdw_recheck_quals = (List *)fix_upper_expr(root, (Node *)fscan->fdw_recheck_quals, itlist, INDEX_VAR, rtoffset);
    pfree(itlist);
                                                                      
    fscan->fdw_scan_tlist = fix_scan_list(root, fscan->fdw_scan_tlist, rtoffset);
  }
  else
  {
       
                                                                        
           
       
    fscan->scan.plan.targetlist = fix_scan_list(root, fscan->scan.plan.targetlist, rtoffset);
    fscan->scan.plan.qual = fix_scan_list(root, fscan->scan.plan.qual, rtoffset);
    fscan->fdw_exprs = fix_scan_list(root, fscan->fdw_exprs, rtoffset);
    fscan->fdw_recheck_quals = fix_scan_list(root, fscan->fdw_recheck_quals, rtoffset);
  }

                                  
  if (rtoffset > 0)
  {
    Bitmapset *tempset = NULL;
    int x = -1;

    while ((x = bms_next_member(fscan->fs_relids, x)) >= 0)
    {
      tempset = bms_add_member(tempset, x + rtoffset);
    }
    fscan->fs_relids = tempset;
  }
}

   
                             
                                                        
   
static void
set_customscan_references(PlannerInfo *root, CustomScan *cscan, int rtoffset)
{
  ListCell *lc;

                                      
  if (cscan->scan.scanrelid > 0)
  {
    cscan->scan.scanrelid += rtoffset;
  }

  if (cscan->custom_scan_tlist != NIL || cscan->scan.scanrelid == 0)
  {
                                                                         
    indexed_tlist *itlist = build_tlist_index(cscan->custom_scan_tlist);

    cscan->scan.plan.targetlist = (List *)fix_upper_expr(root, (Node *)cscan->scan.plan.targetlist, itlist, INDEX_VAR, rtoffset);
    cscan->scan.plan.qual = (List *)fix_upper_expr(root, (Node *)cscan->scan.plan.qual, itlist, INDEX_VAR, rtoffset);
    cscan->custom_exprs = (List *)fix_upper_expr(root, (Node *)cscan->custom_exprs, itlist, INDEX_VAR, rtoffset);
    pfree(itlist);
                                                                         
    cscan->custom_scan_tlist = fix_scan_list(root, cscan->custom_scan_tlist, rtoffset);
  }
  else
  {
                                                              
    cscan->scan.plan.targetlist = fix_scan_list(root, cscan->scan.plan.targetlist, rtoffset);
    cscan->scan.plan.qual = fix_scan_list(root, cscan->scan.plan.qual, rtoffset);
    cscan->custom_exprs = fix_scan_list(root, cscan->custom_exprs, rtoffset);
  }

                                                      
  foreach (lc, cscan->custom_plans)
  {
    lfirst(lc) = set_plan_refs(root, (Plan *)lfirst(lc), rtoffset);
  }

                                      
  if (rtoffset > 0)
  {
    Bitmapset *tempset = NULL;
    int x = -1;

    while ((x = bms_next_member(cscan->custom_relids, x)) >= 0)
    {
      tempset = bms_add_member(tempset, x + rtoffset);
    }
    cscan->custom_relids = tempset;
  }
}

   
                         
                                                   
   
                                                                 
                                      
   
static Plan *
set_append_references(PlannerInfo *root, Append *aplan, int rtoffset)
{
  ListCell *l;

     
                                                                          
                                                                           
                                                      
     
  Assert(aplan->plan.qual == NIL);

                                               
  foreach (l, aplan->appendplans)
  {
    lfirst(l) = set_plan_refs(root, (Plan *)lfirst(l), rtoffset);
  }

     
                                                                         
                                                                            
                                                                           
                                                                           
                                                                        
            
     
  if (list_length(aplan->appendplans) == 1 && ((Plan *)linitial(aplan->appendplans))->parallel_aware == aplan->plan.parallel_aware)
  {
    return clean_up_removed_plan_level((Plan *)aplan, (Plan *)linitial(aplan->appendplans));
  }

     
                                                                           
                                                                           
                    
     
  set_dummy_tlist_references((Plan *)aplan, rtoffset);

  if (aplan->part_prune_info)
  {
    foreach (l, aplan->part_prune_info->prune_infos)
    {
      List *prune_infos = lfirst(l);
      ListCell *l2;

      foreach (l2, prune_infos)
      {
        PartitionedRelPruneInfo *pinfo = lfirst(l2);

        pinfo->rtindex += rtoffset;
      }
    }
  }

                                                             
  Assert(aplan->plan.lefttree == NULL);
  Assert(aplan->plan.righttree == NULL);

  return (Plan *)aplan;
}

   
                              
                                                       
   
                                                                      
                                      
   
static Plan *
set_mergeappend_references(PlannerInfo *root, MergeAppend *mplan, int rtoffset)
{
  ListCell *l;

     
                                                                            
                                                                        
                                                            
     
  Assert(mplan->plan.qual == NIL);

                                               
  foreach (l, mplan->mergeplans)
  {
    lfirst(l) = set_plan_refs(root, (Plan *)lfirst(l), rtoffset);
  }

     
                                                                           
                                                                      
                                                                          
                                                                             
                                                                           
                           
     
  if (list_length(mplan->mergeplans) == 1 && ((Plan *)linitial(mplan->mergeplans))->parallel_aware == mplan->plan.parallel_aware)
  {
    return clean_up_removed_plan_level((Plan *)mplan, (Plan *)linitial(mplan->mergeplans));
  }

     
                                                                          
                                                                         
                            
     
  set_dummy_tlist_references((Plan *)mplan, rtoffset);

  if (mplan->part_prune_info)
  {
    foreach (l, mplan->part_prune_info->prune_infos)
    {
      List *prune_infos = lfirst(l);
      ListCell *l2;

      foreach (l2, prune_infos)
      {
        PartitionedRelPruneInfo *pinfo = lfirst(l2);

        pinfo->rtindex += rtoffset;
      }
    }
  }

                                                             
  Assert(mplan->plan.lefttree == NULL);
  Assert(mplan->plan.righttree == NULL);

  return (Plan *)mplan;
}

   
                       
                                                       
   
static void
set_hash_references(PlannerInfo *root, Plan *plan, int rtoffset)
{
  Hash *hplan = (Hash *)plan;
  Plan *outer_plan = plan->lefttree;
  indexed_tlist *outer_itlist;

     
                                                                      
                                                                          
                                  
     
  outer_itlist = build_tlist_index(outer_plan->targetlist);
  hplan->hashkeys = (List *)fix_upper_expr(root, (Node *)hplan->hashkeys, outer_itlist, OUTER_VAR, rtoffset);

                            
  set_dummy_tlist_references(plan, rtoffset);

                                             
  Assert(plan->qual == NIL);
}

   
           
                     
   
                                                                         
                                                                         
   
static inline Var *
copyVar(Var *var)
{
  Var *newvar = (Var *)palloc(sizeof(Var));

  *newvar = *var;
  return newvar;
}

   
                   
                                                                    
   
                                                                     
                                                                      
                                                                         
                                                                              
                                                                  
   
                                                                               
                                                                 
   
static void
fix_expr_common(PlannerInfo *root, Node *node)
{
                                                         
  if (IsA(node, Aggref))
  {
    record_plan_function_dependency(root, ((Aggref *)node)->aggfnoid);
  }
  else if (IsA(node, WindowFunc))
  {
    record_plan_function_dependency(root, ((WindowFunc *)node)->winfnoid);
  }
  else if (IsA(node, FuncExpr))
  {
    record_plan_function_dependency(root, ((FuncExpr *)node)->funcid);
  }
  else if (IsA(node, OpExpr))
  {
    set_opfuncid((OpExpr *)node);
    record_plan_function_dependency(root, ((OpExpr *)node)->opfuncid);
  }
  else if (IsA(node, DistinctExpr))
  {
    set_opfuncid((OpExpr *)node);                                 
    record_plan_function_dependency(root, ((DistinctExpr *)node)->opfuncid);
  }
  else if (IsA(node, NullIfExpr))
  {
    set_opfuncid((OpExpr *)node);                                 
    record_plan_function_dependency(root, ((NullIfExpr *)node)->opfuncid);
  }
  else if (IsA(node, ScalarArrayOpExpr))
  {
    set_sa_opfuncid((ScalarArrayOpExpr *)node);
    record_plan_function_dependency(root, ((ScalarArrayOpExpr *)node)->opfuncid);
  }
  else if (IsA(node, Const))
  {
    Const *con = (Const *)node;

                                      
    if (ISREGCLASSCONST(con))
    {
      root->glob->relationOids = lappend_oid(root->glob->relationOids, DatumGetObjectId(con->constvalue));
    }
  }
  else if (IsA(node, GroupingFunc))
  {
    GroupingFunc *g = (GroupingFunc *)node;
    AttrNumber *grouping_map = root->grouping_map;

                                                            

    Assert(grouping_map || g->cols == NIL);

    if (grouping_map)
    {
      ListCell *lc;
      List *cols = NIL;

      foreach (lc, g->refs)
      {
        cols = lappend_int(cols, grouping_map[lfirst_int(lc)]);
      }

      Assert(!g->cols || equal(cols, g->cols));

      if (!g->cols)
      {
        g->cols = cols;
      }
    }
  }
}

   
                  
                                                 
   
                                                                         
                                                          
                                                                        
   
static Node *
fix_param_node(PlannerInfo *root, Param *p)
{
  if (p->paramkind == PARAM_MULTIEXPR)
  {
    int subqueryid = p->paramid >> 16;
    int colno = p->paramid & 0xFFFF;
    List *params;

    if (subqueryid <= 0 || subqueryid > list_length(root->multiexpr_params))
    {
      elog(ERROR, "unexpected PARAM_MULTIEXPR ID: %d", p->paramid);
    }
    params = (List *)list_nth(root->multiexpr_params, subqueryid - 1);
    if (colno <= 0 || colno > list_length(params))
    {
      elog(ERROR, "unexpected PARAM_MULTIEXPR ID: %d", p->paramid);
    }
    return copyObject(list_nth(params, colno - 1));
  }
  return (Node *)copyObject(p);
}

   
                 
                                                                 
   
                                                               
                                                                
                                                                             
                                                                 
                                                                            
   
static Node *
fix_scan_expr(PlannerInfo *root, Node *node, int rtoffset)
{
  fix_scan_expr_context context;

  context.root = root;
  context.rtoffset = rtoffset;

  if (rtoffset != 0 || root->multiexpr_params != NIL || root->glob->lastPHId != 0 || root->minmax_aggs != NIL)
  {
    return fix_scan_expr_mutator(node, &context);
  }
  else
  {
       
                                                                        
                                                                 
                                                                         
                                                                           
                                                                         
                                                                          
                                                                           
                                                   
       
    (void)fix_scan_expr_walker(node, &context);
    return node;
  }
}

static Node *
fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = copyVar((Var *)node);

    Assert(var->varlevelsup == 0);

       
                                                                         
                                                          
       
    Assert(var->varno != INNER_VAR);
    Assert(var->varno != OUTER_VAR);
    if (!IS_SPECIAL_VARNO(var->varno))
    {
      var->varno += context->rtoffset;
    }
    if (var->varnoold > 0)
    {
      var->varnoold += context->rtoffset;
    }
    return (Node *)var;
  }
  if (IsA(node, Param))
  {
    return fix_param_node(context->root, (Param *)node);
  }
  if (IsA(node, Aggref))
  {
    Aggref *aggref = (Aggref *)node;

                                                         
    if (context->root->minmax_aggs != NIL && list_length(aggref->args) == 1)
    {
      TargetEntry *curTarget = (TargetEntry *)linitial(aggref->args);
      ListCell *lc;

      foreach (lc, context->root->minmax_aggs)
      {
        MinMaxAggInfo *mminfo = (MinMaxAggInfo *)lfirst(lc);

        if (mminfo->aggfnoid == aggref->aggfnoid && equal(mminfo->target, curTarget->expr))
        {
          return (Node *)copyObject(mminfo->param);
        }
      }
    }
                                                               
  }
  if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)copyObject(node);

    Assert(cexpr->cvarno != INNER_VAR);
    Assert(cexpr->cvarno != OUTER_VAR);
    if (!IS_SPECIAL_VARNO(cexpr->cvarno))
    {
      cexpr->cvarno += context->rtoffset;
    }
    return (Node *)cexpr;
  }
  if (IsA(node, PlaceHolderVar))
  {
                                                                          
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

    return fix_scan_expr_mutator((Node *)phv->phexpr, context);
  }
  fix_expr_common(context->root, node);
  return expression_tree_mutator(node, fix_scan_expr_mutator, (void *)context);
}

static bool
fix_scan_expr_walker(Node *node, fix_scan_expr_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  Assert(!IsA(node, PlaceHolderVar));
  fix_expr_common(context->root, node);
  return expression_tree_walker(node, fix_scan_expr_walker, (void *)context);
}

   
                       
                                                                      
                                                                           
                                                                          
                                                                           
                                                                     
   
static void
set_join_references(PlannerInfo *root, Join *join, int rtoffset)
{
  Plan *outer_plan = join->plan.lefttree;
  Plan *inner_plan = join->plan.righttree;
  indexed_tlist *outer_itlist;
  indexed_tlist *inner_itlist;

  outer_itlist = build_tlist_index(outer_plan->targetlist);
  inner_itlist = build_tlist_index(inner_plan->targetlist);

     
                                                                           
                                                                    
                                                                
                                                                  
                     
     
  join->joinqual = fix_join_expr(root, join->joinqual, outer_itlist, inner_itlist, (Index)0, rtoffset);

                                       
  if (IsA(join, NestLoop))
  {
    NestLoop *nl = (NestLoop *)join;
    ListCell *lc;

    foreach (lc, nl->nestParams)
    {
      NestLoopParam *nlp = (NestLoopParam *)lfirst(lc);

      nlp->paramval = (Var *)fix_upper_expr(root, (Node *)nlp->paramval, outer_itlist, OUTER_VAR, rtoffset);
                                                                
      if (!(IsA(nlp->paramval, Var) && nlp->paramval->varno == OUTER_VAR))
      {
        elog(ERROR, "NestLoopParam was not reduced to a simple Var");
      }
    }
  }
  else if (IsA(join, MergeJoin))
  {
    MergeJoin *mj = (MergeJoin *)join;

    mj->mergeclauses = fix_join_expr(root, mj->mergeclauses, outer_itlist, inner_itlist, (Index)0, rtoffset);
  }
  else if (IsA(join, HashJoin))
  {
    HashJoin *hj = (HashJoin *)join;

    hj->hashclauses = fix_join_expr(root, hj->hashclauses, outer_itlist, inner_itlist, (Index)0, rtoffset);

       
                                                                         
                                                         
       
    hj->hashkeys = (List *)fix_upper_expr(root, (Node *)hj->hashkeys, outer_itlist, OUTER_VAR, rtoffset);
  }

     
                                                                          
                                                                             
                                                                        
                                                                            
                                                         
     
                                                                        
                                                                            
                                                                            
                     
     
  switch (join->jointype)
  {
  case JOIN_LEFT:
  case JOIN_SEMI:
  case JOIN_ANTI:
    inner_itlist->has_non_vars = false;
    break;
  case JOIN_RIGHT:
    outer_itlist->has_non_vars = false;
    break;
  case JOIN_FULL:
    outer_itlist->has_non_vars = false;
    inner_itlist->has_non_vars = false;
    break;
  default:
    break;
  }

  join->plan.targetlist = fix_join_expr(root, join->plan.targetlist, outer_itlist, inner_itlist, (Index)0, rtoffset);
  join->plan.qual = fix_join_expr(root, join->plan.qual, outer_itlist, inner_itlist, (Index)0, rtoffset);

  pfree(outer_itlist);
  pfree(inner_itlist);
}

   
                        
                                                                 
                                                              
                                                           
                                                    
   
                                                                     
   
                                                                       
                                                                    
                                                                             
                                                                       
                                                                           
                                                                             
                                                                            
                                           
   
static void
set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset)
{
  Plan *subplan = plan->lefttree;
  indexed_tlist *subplan_itlist;
  List *output_targetlist;
  ListCell *l;

  subplan_itlist = build_tlist_index(subplan->targetlist);

  output_targetlist = NIL;
  foreach (l, plan->targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);
    Node *newexpr;

                                                                  
    if (tle->ressortgroupref != 0)
    {
      newexpr = (Node *)search_indexed_tlist_for_sortgroupref(tle->expr, tle->ressortgroupref, subplan_itlist, OUTER_VAR);
      if (!newexpr)
      {
        newexpr = fix_upper_expr(root, (Node *)tle->expr, subplan_itlist, OUTER_VAR, rtoffset);
      }
    }
    else
    {
      newexpr = fix_upper_expr(root, (Node *)tle->expr, subplan_itlist, OUTER_VAR, rtoffset);
    }
    tle = flatCopyTargetEntry(tle);
    tle->expr = (Expr *)newexpr;
    output_targetlist = lappend(output_targetlist, tle);
  }
  plan->targetlist = output_targetlist;

  plan->qual = (List *)fix_upper_expr(root, (Node *)plan->qual, subplan_itlist, OUTER_VAR, rtoffset);

  pfree(subplan_itlist);
}

   
                        
                                                                            
                                                                        
                                                                             
                                              
   
static void
set_param_references(PlannerInfo *root, Plan *plan)
{
  Assert(IsA(plan, Gather) || IsA(plan, GatherMerge));

  if (plan->lefttree->extParam)
  {
    PlannerInfo *proot;
    Bitmapset *initSetParam = NULL;
    ListCell *l;

    for (proot = root; proot != NULL; proot = proot->parent_root)
    {
      foreach (l, proot->init_plans)
      {
        SubPlan *initsubplan = (SubPlan *)lfirst(l);
        ListCell *l2;

        foreach (l2, initsubplan->setParam)
        {
          initSetParam = bms_add_member(initSetParam, lfirst_int(l2));
        }
      }
    }

       
                                                                          
                                                    
       
    if (IsA(plan, Gather))
    {
      ((Gather *)plan)->initParam = bms_intersect(plan->lefttree->extParam, initSetParam);
    }
    else
    {
      ((GatherMerge *)plan)->initParam = bms_intersect(plan->lefttree->extParam, initSetParam);
    }
  }
}

   
                                                                         
                                                                              
                                                                          
                                                                             
                               
   
                                                                          
                                                                              
                                                                        
   
                                                                            
                                                                           
                                                                          
                                                                            
                                                                    
   
static Node *
convert_combining_aggrefs(Node *node, void *context)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Aggref))
  {
    Aggref *orig_agg = (Aggref *)node;
    Aggref *child_agg;
    Aggref *parent_agg;

                                                                      
    Assert(orig_agg->aggorder == NIL);
    Assert(orig_agg->aggdistinct == NIL);

       
                                                                          
                                                                           
                                   
       
    child_agg = makeNode(Aggref);
    memcpy(child_agg, orig_agg, sizeof(Aggref));

       
                                                                    
                                                                      
                                                                         
                                                                          
                                                                         
                                                 
       
    child_agg->args = NIL;
    child_agg->aggfilter = NULL;
    parent_agg = copyObject(child_agg);
    child_agg->args = orig_agg->args;
    child_agg->aggfilter = orig_agg->aggfilter;

       
                                                                     
                                                                
       
    mark_partial_aggref(child_agg, AGGSPLIT_INITIAL_SERIAL);

       
                                                            
       
    parent_agg->args = list_make1(makeTargetEntry((Expr *)child_agg, 1, NULL, false));
    mark_partial_aggref(parent_agg, AGGSPLIT_FINAL_DESERIAL);

    return (Node *)parent_agg;
  }
  return expression_tree_mutator(node, convert_combining_aggrefs, (void *)context);
}

   
                              
                                                                      
                                                
   
                                                                        
                                                                           
                                                
   
                                                                           
                                                                         
           
   
static void
set_dummy_tlist_references(Plan *plan, int rtoffset)
{
  List *output_targetlist;
  ListCell *l;

  output_targetlist = NIL;
  foreach (l, plan->targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);
    Var *oldvar = (Var *)tle->expr;
    Var *newvar;

       
                                                                          
                                                                       
                                                                          
                                               
       
    if (IsA(oldvar, Const))
    {
                                            
      output_targetlist = lappend(output_targetlist, tle);
      continue;
    }

    newvar = makeVar(OUTER_VAR, tle->resno, exprType((Node *)oldvar), exprTypmod((Node *)oldvar), exprCollation((Node *)oldvar), 0);
    if (IsA(oldvar, Var))
    {
      newvar->varnoold = oldvar->varno + rtoffset;
      newvar->varoattno = oldvar->varattno;
    }
    else
    {
      newvar->varnoold = 0;                              
      newvar->varoattno = 0;
    }

    tle = flatCopyTargetEntry(tle);
    tle->expr = (Expr *)newvar;
    output_targetlist = lappend(output_targetlist, tle);
  }
  plan->targetlist = output_targetlist;

                                      
}

   
                                                                         
   
                                                                       
                                                                        
                                                                      
                                                                          
                            
   
                                                                     
                                                                         
                                                                    
   
static indexed_tlist *
build_tlist_index(List *tlist)
{
  indexed_tlist *itlist;
  tlist_vinfo *vinfo;
  ListCell *l;

                                                                     
  itlist = (indexed_tlist *)palloc(offsetof(indexed_tlist, vars) + list_length(tlist) * sizeof(tlist_vinfo));

  itlist->tlist = tlist;
  itlist->has_ph_vars = false;
  itlist->has_non_vars = false;

                                                 
  vinfo = itlist->vars;
  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->expr && IsA(tle->expr, Var))
    {
      Var *var = (Var *)tle->expr;

      vinfo->varno = var->varno;
      vinfo->varattno = var->varattno;
      vinfo->resno = tle->resno;
      vinfo++;
    }
    else if (tle->expr && IsA(tle->expr, PlaceHolderVar))
    {
      itlist->has_ph_vars = true;
    }
    else
    {
      itlist->has_non_vars = true;
    }
  }

  itlist->num_vars = (vinfo - itlist->vars);

  return itlist;
}

   
                                                                   
   
                                                                        
                                                                             
                                                                              
                                                                    
   
static indexed_tlist *
build_tlist_index_other_vars(List *tlist, Index ignore_rel)
{
  indexed_tlist *itlist;
  tlist_vinfo *vinfo;
  ListCell *l;

                                                                     
  itlist = (indexed_tlist *)palloc(offsetof(indexed_tlist, vars) + list_length(tlist) * sizeof(tlist_vinfo));

  itlist->tlist = tlist;
  itlist->has_ph_vars = false;
  itlist->has_non_vars = false;

                                                         
  vinfo = itlist->vars;
  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->expr && IsA(tle->expr, Var))
    {
      Var *var = (Var *)tle->expr;

      if (var->varno != ignore_rel)
      {
        vinfo->varno = var->varno;
        vinfo->varattno = var->varattno;
        vinfo->resno = tle->resno;
        vinfo++;
      }
    }
    else if (tle->expr && IsA(tle->expr, PlaceHolderVar))
    {
      itlist->has_ph_vars = true;
    }
  }

  itlist->num_vars = (vinfo - itlist->vars);

  return itlist;
}

   
                                                                   
   
                                                                     
                                                                              
                                                         
                             
   
static Var *
search_indexed_tlist_for_var(Var *var, indexed_tlist *itlist, Index newvarno, int rtoffset)
{
  Index varno = var->varno;
  AttrNumber varattno = var->varattno;
  tlist_vinfo *vinfo;
  int i;

  vinfo = itlist->vars;
  i = itlist->num_vars;
  while (i-- > 0)
  {
    if (vinfo->varno == varno && vinfo->varattno == varattno)
    {
                         
      Var *newvar = copyVar(var);

      newvar->varno = newvarno;
      newvar->varattno = vinfo->resno;
      if (newvar->varnoold > 0)
      {
        newvar->varnoold += rtoffset;
      }
      return newvar;
    }
    vinfo++;
  }
  return NULL;               
}

   
                                                                           
   
                                                                              
                             
   
                                                                          
                                                                             
                                                                          
                                                                     
   
static Var *
search_indexed_tlist_for_non_var(Expr *node, indexed_tlist *itlist, Index newvarno)
{
  TargetEntry *tle;

     
                                                                             
                                                                        
                                                                         
                                                                      
                      
     
  if (IsA(node, Const))
  {
    return NULL;
  }

  tle = tlist_member(node, itlist->tlist);
  if (tle)
  {
                                                    
    Var *newvar;

    newvar = makeVarFromTargetEntry(newvarno, tle);
    newvar->varnoold = 0;                              
    newvar->varoattno = 0;
    return newvar;
  }
  return NULL;               
}

   
                                                                          
   
                                                                              
                             
   
                                                                          
                                                                             
                                                               
   
static Var *
search_indexed_tlist_for_sortgroupref(Expr *node, Index sortgroupref, indexed_tlist *itlist, Index newvarno)
{
  ListCell *lc;

  foreach (lc, itlist->tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);

                                                                      
    if (tle->ressortgroupref == sortgroupref && equal(node, tle->expr))
    {
                                                      
      Var *newvar;

      newvar = makeVarFromTargetEntry(newvarno, tle);
      newvar->varnoold = 0;                              
      newvar->varoattno = 0;
      return newvar;
    }
  }
  return NULL;               
}

   
                 
                                                                     
                                                                     
                                                                    
                                                                 
                                                 
   
                                              
                                                                       
                                                                  
                                                                              
                           
                                                                            
                                                                      
                                                                            
                                                                        
                                       
                                                                             
                                                                              
                                                                          
                                                                          
               
   
                                                       
                                                                         
            
                                                                         
            
                                                                         
                                                                   
                                                 
   
                                                                      
                 
   
static List *
fix_join_expr(PlannerInfo *root, List *clauses, indexed_tlist *outer_itlist, indexed_tlist *inner_itlist, Index acceptable_rel, int rtoffset)
{
  fix_join_expr_context context;

  context.root = root;
  context.outer_itlist = outer_itlist;
  context.inner_itlist = inner_itlist;
  context.acceptable_rel = acceptable_rel;
  context.rtoffset = rtoffset;
  return (List *)fix_join_expr_mutator((Node *)clauses, &context);
}

static Node *
fix_join_expr_mutator(Node *node, fix_join_expr_context *context)
{
  Var *newvar;

  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

                                                                  
    if (context->outer_itlist)
    {
      newvar = search_indexed_tlist_for_var(var, context->outer_itlist, OUTER_VAR, context->rtoffset);
      if (newvar)
      {
        return (Node *)newvar;
      }
    }

                            
    if (context->inner_itlist)
    {
      newvar = search_indexed_tlist_for_var(var, context->inner_itlist, INNER_VAR, context->rtoffset);
      if (newvar)
      {
        return (Node *)newvar;
      }
    }

                                                          
    if (var->varno == context->acceptable_rel)
    {
      var = copyVar(var);
      var->varno += context->rtoffset;
      if (var->varnoold > 0)
      {
        var->varnoold += context->rtoffset;
      }
      return (Node *)var;
    }

                                   
    elog(ERROR, "variable not found in subplan target lists");
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

                                                                         
    if (context->outer_itlist && context->outer_itlist->has_ph_vars)
    {
      newvar = search_indexed_tlist_for_non_var((Expr *)phv, context->outer_itlist, OUTER_VAR);
      if (newvar)
      {
        return (Node *)newvar;
      }
    }
    if (context->inner_itlist && context->inner_itlist->has_ph_vars)
    {
      newvar = search_indexed_tlist_for_non_var((Expr *)phv, context->inner_itlist, INNER_VAR);
      if (newvar)
      {
        return (Node *)newvar;
      }
    }

                                                                     
    return fix_join_expr_mutator((Node *)phv->phexpr, context);
  }
                                                                     
  if (context->outer_itlist && context->outer_itlist->has_non_vars)
  {
    newvar = search_indexed_tlist_for_non_var((Expr *)node, context->outer_itlist, OUTER_VAR);
    if (newvar)
    {
      return (Node *)newvar;
    }
  }
  if (context->inner_itlist && context->inner_itlist->has_non_vars)
  {
    newvar = search_indexed_tlist_for_non_var((Expr *)node, context->inner_itlist, INNER_VAR);
    if (newvar)
    {
      return (Node *)newvar;
    }
  }
                                                                        
  if (IsA(node, Param))
  {
    return fix_param_node(context->root, (Param *)node);
  }
  fix_expr_common(context->root, node);
  return expression_tree_mutator(node, fix_join_expr_mutator, (void *)context);
}

   
                  
                                                                        
                                                                       
                                                                      
                                               
   
                                                                              
                                                 
   
                                                                           
                                                                       
                                                                        
                
   
                                                                              
                                                                           
                                                                               
                                                                      
                                                    
   
                                                        
                                                                
                                                                
                                                 
   
                                                                            
                                                                           
                                      
   
static Node *
fix_upper_expr(PlannerInfo *root, Node *node, indexed_tlist *subplan_itlist, Index newvarno, int rtoffset)
{
  fix_upper_expr_context context;

  context.root = root;
  context.subplan_itlist = subplan_itlist;
  context.newvarno = newvarno;
  context.rtoffset = rtoffset;
  return fix_upper_expr_mutator(node, &context);
}

static Node *
fix_upper_expr_mutator(Node *node, fix_upper_expr_context *context)
{
  Var *newvar;

  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)node;

    newvar = search_indexed_tlist_for_var(var, context->subplan_itlist, context->newvarno, context->rtoffset);
    if (!newvar)
    {
      elog(ERROR, "variable not found in subplan target list");
    }
    return (Node *)newvar;
  }
  if (IsA(node, PlaceHolderVar))
  {
    PlaceHolderVar *phv = (PlaceHolderVar *)node;

                                                                         
    if (context->subplan_itlist->has_ph_vars)
    {
      newvar = search_indexed_tlist_for_non_var((Expr *)phv, context->subplan_itlist, context->newvarno);
      if (newvar)
      {
        return (Node *)newvar;
      }
    }
                                                                    
    return fix_upper_expr_mutator((Node *)phv->phexpr, context);
  }
                                                                   
  if (context->subplan_itlist->has_non_vars)
  {
    newvar = search_indexed_tlist_for_non_var((Expr *)node, context->subplan_itlist, context->newvarno);
    if (newvar)
    {
      return (Node *)newvar;
    }
  }
                                                                        
  if (IsA(node, Param))
  {
    return fix_param_node(context->root, (Param *)node);
  }
  if (IsA(node, Aggref))
  {
    Aggref *aggref = (Aggref *)node;

                                                         
    if (context->root->minmax_aggs != NIL && list_length(aggref->args) == 1)
    {
      TargetEntry *curTarget = (TargetEntry *)linitial(aggref->args);
      ListCell *lc;

      foreach (lc, context->root->minmax_aggs)
      {
        MinMaxAggInfo *mminfo = (MinMaxAggInfo *)lfirst(lc);

        if (mminfo->aggfnoid == aggref->aggfnoid && equal(mminfo->target, curTarget->expr))
        {
          return (Node *)copyObject(mminfo->param);
        }
      }
    }
                                                               
  }
  fix_expr_common(context->root, node);
  return expression_tree_mutator(node, fix_upper_expr_mutator, (void *)context);
}

   
                                   
                                                       
   
                                                                     
                                                                      
                                                                         
                                                                        
                                                                       
                                                                       
                                                                             
   
                                                               
                             
   
                                                 
                                                                           
                                                          
                                                                
                                               
   
                                                                            
                                                                           
                                                                           
                                      
   
                                                         
   
static List *
set_returning_clause_references(PlannerInfo *root, List *rlist, Plan *topplan, Index resultRelation, int rtoffset)
{
  indexed_tlist *itlist;

     
                                                                       
                                                                           
                                                                     
                                                                            
                                                   
     
                                                                   
                                                                           
                                                                         
                                                                            
                                                                          
                         
     
  itlist = build_tlist_index_other_vars(topplan->targetlist, resultRelation);

  rlist = fix_join_expr(root, rlist, itlist, NULL, resultRelation, rtoffset);

  pfree(itlist);

  return rlist;
}

                                                                               
                                   
                                                                               

   
                                   
                                                                 
   
                                                                    
                                                                  
   
void
record_plan_function_dependency(PlannerInfo *root, Oid funcid)
{
     
                                                                           
                                                                         
                                                                       
                                                                            
                                                                          
                                            
     
  if (funcid >= (Oid)FirstBootstrapObjectId)
  {
    PlanInvalItem *inval_item = makeNode(PlanInvalItem);

       
                                                                        
                                                                       
                                              
       
    inval_item->cacheId = PROCOID;
    inval_item->hashValue = GetSysCacheHashValue1(PROCOID, ObjectIdGetDatum(funcid));

    root->glob->invalItems = lappend(root->glob->invalItems, inval_item);
  }
}

   
                               
                                                             
   
                                                                
                                                                       
   
                                                                      
                                                                       
                                                                     
                                          
   
void
record_plan_type_dependency(PlannerInfo *root, Oid typid)
{
     
                                                                        
                                             
     
  if (typid >= (Oid)FirstBootstrapObjectId)
  {
    PlanInvalItem *inval_item = makeNode(PlanInvalItem);

       
                                                                        
                                                                        
                                         
       
    inval_item->cacheId = TYPEOID;
    inval_item->hashValue = GetSysCacheHashValue1(TYPEOID, ObjectIdGetDatum(typid));

    root->glob->invalItems = lappend(root->glob->invalItems, inval_item);
  }
}

   
                              
                                                             
                                                                     
                                                                   
                                        
   
                                                                            
            
   
                                                                            
                                                                              
                                                                            
                                                                             
                                                                              
                                                         
   
void
extract_query_dependencies(Node *query, List **relationOids, List **invalItems, bool *hasRowSecurity)
{
  PlannerGlobal glob;
  PlannerInfo root;

                                                                         
  MemSet(&glob, 0, sizeof(glob));
  glob.type = T_PlannerGlobal;
  glob.relationOids = NIL;
  glob.invalItems = NIL;
                                                                       
  glob.dependsOnRole = false;

  MemSet(&root, 0, sizeof(root));
  root.type = T_PlannerInfo;
  root.glob = &glob;

  (void)extract_query_dependencies_walker(query, &root);

  *relationOids = glob.relationOids;
  *invalItems = glob.invalItems;
  *hasRowSecurity = glob.dependsOnRole;
}

   
                                               
   
                                                                        
                                                                          
                                                                            
                                                
   
bool
extract_query_dependencies_walker(Node *node, PlannerInfo *context)
{
  if (node == NULL)
  {
    return false;
  }
  Assert(!IsA(node, PlaceHolderVar));
  if (IsA(node, Query))
  {
    Query *query = (Query *)node;
    ListCell *lc;

    if (query->commandType == CMD_UTILITY)
    {
         
                                                                        
                                                 
         
      query = UtilityContainsQuery(query->utilityStmt);
      if (query == NULL)
      {
        return false;
      }
    }

                                                                 
    if (query->hasRowSecurity)
    {
      context->glob->dependsOnRole = true;
    }

                                                      
    foreach (lc, query->rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

      if (rte->rtekind == RTE_RELATION)
      {
        context->glob->relationOids = lappend_oid(context->glob->relationOids, rte->relid);
      }
      else if (rte->rtekind == RTE_NAMEDTUPLESTORE && OidIsValid(rte->relid))
      {
        context->glob->relationOids = lappend_oid(context->glob->relationOids, rte->relid);
      }
    }

                                                     
    return query_tree_walker(query, extract_query_dependencies_walker, (void *)context, 0);
  }
                                                                   
  fix_expr_common(context, node);
  return expression_tree_walker(node, extract_query_dependencies_walker, (void *)context);
}
