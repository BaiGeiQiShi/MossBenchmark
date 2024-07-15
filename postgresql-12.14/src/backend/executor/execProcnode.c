                                                                            
   
                  
                                                                         
                                                                   
                                                                         
                                                                        
                
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
   
          
                                                              
                                                                
                                      
   
            
                                                                      
                                                                       
   
                                   
                     
                                    
                            
   
                                                     
   
                                        
           \
              \
                            
                   
                      
   
                                     
                                                      
                                                
   
                                                                    
                                                                 
                                                                        
                                                                   
                                                                         
                                 
   
                                                                             
                                                                      
                                                               
                                                                
                                                                    
                                                             
                                                               
                                
   
                                                                    
                                                                     
                                                                 
                                                   
   
                                                      
                                                              
                                                                  
                                                              
   
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "executor/nodeAppend.h"
#include "executor/nodeBitmapAnd.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeBitmapOr.h"
#include "executor/nodeCtescan.h"
#include "executor/nodeCustom.h"
#include "executor/nodeForeignscan.h"
#include "executor/nodeFunctionscan.h"
#include "executor/nodeGather.h"
#include "executor/nodeGatherMerge.h"
#include "executor/nodeGroup.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeLimit.h"
#include "executor/nodeLockRows.h"
#include "executor/nodeMaterial.h"
#include "executor/nodeMergeAppend.h"
#include "executor/nodeMergejoin.h"
#include "executor/nodeModifyTable.h"
#include "executor/nodeNamedtuplestorescan.h"
#include "executor/nodeNestloop.h"
#include "executor/nodeProjectSet.h"
#include "executor/nodeRecursiveunion.h"
#include "executor/nodeResult.h"
#include "executor/nodeSamplescan.h"
#include "executor/nodeSeqscan.h"
#include "executor/nodeSetOp.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/nodeSubqueryscan.h"
#include "executor/nodeTableFuncscan.h"
#include "executor/nodeTidscan.h"
#include "executor/nodeUnique.h"
#include "executor/nodeValuesscan.h"
#include "executor/nodeWindowAgg.h"
#include "executor/nodeWorktablescan.h"
#include "nodes/nodeFuncs.h"
#include "miscadmin.h"

static TupleTableSlot *
ExecProcNodeFirst(PlanState *node);
static TupleTableSlot *
ExecProcNodeInstr(PlanState *node);
static bool
ExecShutdownNode_walker(PlanState *node, void *context);

                                                                            
                 
   
                                                                  
               
   
            
                                                                           
                                                               
                                                                    
   
                                                                   
                                                                            
   
PlanState *
ExecInitNode(Plan *node, EState *estate, int eflags)
{
  PlanState *result;
  List *subps;
  ListCell *l;

     
                                                          
     
  if (node == NULL)
  {
    return NULL;
  }

     
                                                                      
                                                                         
                                                           
     
  check_stack_depth();

  switch (nodeTag(node))
  {
       
                     
       
  case T_Result:
    result = (PlanState *)ExecInitResult((Result *)node, estate, eflags);
    break;

  case T_ProjectSet:
    result = (PlanState *)ExecInitProjectSet((ProjectSet *)node, estate, eflags);
    break;

  case T_ModifyTable:
    result = (PlanState *)ExecInitModifyTable((ModifyTable *)node, estate, eflags);
    break;

  case T_Append:
    result = (PlanState *)ExecInitAppend((Append *)node, estate, eflags);
    break;

  case T_MergeAppend:
    result = (PlanState *)ExecInitMergeAppend((MergeAppend *)node, estate, eflags);
    break;

  case T_RecursiveUnion:
    result = (PlanState *)ExecInitRecursiveUnion((RecursiveUnion *)node, estate, eflags);
    break;

  case T_BitmapAnd:
    result = (PlanState *)ExecInitBitmapAnd((BitmapAnd *)node, estate, eflags);
    break;

  case T_BitmapOr:
    result = (PlanState *)ExecInitBitmapOr((BitmapOr *)node, estate, eflags);
    break;

       
                  
       
  case T_SeqScan:
    result = (PlanState *)ExecInitSeqScan((SeqScan *)node, estate, eflags);
    break;

  case T_SampleScan:
    result = (PlanState *)ExecInitSampleScan((SampleScan *)node, estate, eflags);
    break;

  case T_IndexScan:
    result = (PlanState *)ExecInitIndexScan((IndexScan *)node, estate, eflags);
    break;

  case T_IndexOnlyScan:
    result = (PlanState *)ExecInitIndexOnlyScan((IndexOnlyScan *)node, estate, eflags);
    break;

  case T_BitmapIndexScan:
    result = (PlanState *)ExecInitBitmapIndexScan((BitmapIndexScan *)node, estate, eflags);
    break;

  case T_BitmapHeapScan:
    result = (PlanState *)ExecInitBitmapHeapScan((BitmapHeapScan *)node, estate, eflags);
    break;

  case T_TidScan:
    result = (PlanState *)ExecInitTidScan((TidScan *)node, estate, eflags);
    break;

  case T_SubqueryScan:
    result = (PlanState *)ExecInitSubqueryScan((SubqueryScan *)node, estate, eflags);
    break;

  case T_FunctionScan:
    result = (PlanState *)ExecInitFunctionScan((FunctionScan *)node, estate, eflags);
    break;

  case T_TableFuncScan:
    result = (PlanState *)ExecInitTableFuncScan((TableFuncScan *)node, estate, eflags);
    break;

  case T_ValuesScan:
    result = (PlanState *)ExecInitValuesScan((ValuesScan *)node, estate, eflags);
    break;

  case T_CteScan:
    result = (PlanState *)ExecInitCteScan((CteScan *)node, estate, eflags);
    break;

  case T_NamedTuplestoreScan:
    result = (PlanState *)ExecInitNamedTuplestoreScan((NamedTuplestoreScan *)node, estate, eflags);
    break;

  case T_WorkTableScan:
    result = (PlanState *)ExecInitWorkTableScan((WorkTableScan *)node, estate, eflags);
    break;

  case T_ForeignScan:
    result = (PlanState *)ExecInitForeignScan((ForeignScan *)node, estate, eflags);
    break;

  case T_CustomScan:
    result = (PlanState *)ExecInitCustomScan((CustomScan *)node, estate, eflags);
    break;

       
                  
       
  case T_NestLoop:
    result = (PlanState *)ExecInitNestLoop((NestLoop *)node, estate, eflags);
    break;

  case T_MergeJoin:
    result = (PlanState *)ExecInitMergeJoin((MergeJoin *)node, estate, eflags);
    break;

  case T_HashJoin:
    result = (PlanState *)ExecInitHashJoin((HashJoin *)node, estate, eflags);
    break;

       
                             
       
  case T_Material:
    result = (PlanState *)ExecInitMaterial((Material *)node, estate, eflags);
    break;

  case T_Sort:
    result = (PlanState *)ExecInitSort((Sort *)node, estate, eflags);
    break;

  case T_Group:
    result = (PlanState *)ExecInitGroup((Group *)node, estate, eflags);
    break;

  case T_Agg:
    result = (PlanState *)ExecInitAgg((Agg *)node, estate, eflags);
    break;

  case T_WindowAgg:
    result = (PlanState *)ExecInitWindowAgg((WindowAgg *)node, estate, eflags);
    break;

  case T_Unique:
    result = (PlanState *)ExecInitUnique((Unique *)node, estate, eflags);
    break;

  case T_Gather:
    result = (PlanState *)ExecInitGather((Gather *)node, estate, eflags);
    break;

  case T_GatherMerge:
    result = (PlanState *)ExecInitGatherMerge((GatherMerge *)node, estate, eflags);
    break;

  case T_Hash:
    result = (PlanState *)ExecInitHash((Hash *)node, estate, eflags);
    break;

  case T_SetOp:
    result = (PlanState *)ExecInitSetOp((SetOp *)node, estate, eflags);
    break;

  case T_LockRows:
    result = (PlanState *)ExecInitLockRows((LockRows *)node, estate, eflags);
    break;

  case T_Limit:
    result = (PlanState *)ExecInitLimit((Limit *)node, estate, eflags);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    result = NULL;                          
    break;
  }

  ExecSetExecProcNode(result, result->ExecProcNode);

     
                                                                             
                             
     
  subps = NIL;
  foreach (l, node->initPlan)
  {
    SubPlan *subplan = (SubPlan *)lfirst(l);
    SubPlanState *sstate;

    Assert(IsA(subplan, SubPlan));
    sstate = ExecInitSubPlan(subplan, result);
    subps = lappend(subps, sstate);
  }
  result->initPlan = subps;

                                                         
  if (estate->es_instrument)
  {
    result->instrument = InstrAlloc(1, estate->es_instrument);
  }

  return result;
}

   
                                                                            
                                                                           
                                                                          
          
   
void
ExecSetExecProcNode(PlanState *node, ExecProcNodeMtd function)
{
     
                                                                            
                                                                           
                                                                         
                                                                             
     
  node->ExecProcNodeReal = function;
  node->ExecProcNode = ExecProcNodeFirst;
}

   
                                                                           
                                                                       
   
static TupleTableSlot *
ExecProcNodeFirst(PlanState *node)
{
     
                                                                           
                                                                             
                                                                         
                                                                          
                                   
     
  check_stack_depth();

     
                                                                         
                                                                            
                                                                          
     
  if (node->instrument)
  {
    node->ExecProcNode = ExecProcNodeInstr;
  }
  else
  {
    node->ExecProcNode = node->ExecProcNodeReal;
  }

  return node->ExecProcNode(node);
}

   
                                                                         
                                                                        
                                 
   
static TupleTableSlot *
ExecProcNodeInstr(PlanState *node)
{
  TupleTableSlot *result;

  InstrStartNode(node->instrument);

  result = node->ExecProcNodeReal(node);

  InstrStopNode(node->instrument, TupIsNull(result) ? 0.0 : 1.0);

  return result;
}

                                                                    
                      
   
                                                         
                                                               
                                                 
   
                                                                   
                                                                   
                                                                    
                                                          
                                                                    
   
Node *
MultiExecProcNode(PlanState *node)
{
  Node *result;

  check_stack_depth();

  CHECK_FOR_INTERRUPTS();

  if (node->chgParam != NULL)                        
  {
    ExecReScan(node);                             
  }

  switch (nodeTag(node))
  {
       
                                                                      
       

  case T_HashState:
    result = MultiExecHash((HashState *)node);
    break;

  case T_BitmapIndexScanState:
    result = MultiExecBitmapIndexScan((BitmapIndexScanState *)node);
    break;

  case T_BitmapAndState:
    result = MultiExecBitmapAnd((BitmapAndState *)node);
    break;

  case T_BitmapOrState:
    result = MultiExecBitmapOr((BitmapOrState *)node);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    result = NULL;
    break;
  }

  return result;
}

                                                                    
                
   
                                                           
               
   
                                                                
                                                             
                                            
                                                                    
   
void
ExecEndNode(PlanState *node)
{
     
                                                          
     
  if (node == NULL)
  {
    return;
  }

     
                                                                      
                                                                            
                                                              
     
  check_stack_depth();

  if (node->chgParam != NULL)
  {
    bms_free(node->chgParam);
    node->chgParam = NULL;
  }

  switch (nodeTag(node))
  {
       
                     
       
  case T_ResultState:
    ExecEndResult((ResultState *)node);
    break;

  case T_ProjectSetState:
    ExecEndProjectSet((ProjectSetState *)node);
    break;

  case T_ModifyTableState:
    ExecEndModifyTable((ModifyTableState *)node);
    break;

  case T_AppendState:
    ExecEndAppend((AppendState *)node);
    break;

  case T_MergeAppendState:
    ExecEndMergeAppend((MergeAppendState *)node);
    break;

  case T_RecursiveUnionState:
    ExecEndRecursiveUnion((RecursiveUnionState *)node);
    break;

  case T_BitmapAndState:
    ExecEndBitmapAnd((BitmapAndState *)node);
    break;

  case T_BitmapOrState:
    ExecEndBitmapOr((BitmapOrState *)node);
    break;

       
                  
       
  case T_SeqScanState:
    ExecEndSeqScan((SeqScanState *)node);
    break;

  case T_SampleScanState:
    ExecEndSampleScan((SampleScanState *)node);
    break;

  case T_GatherState:
    ExecEndGather((GatherState *)node);
    break;

  case T_GatherMergeState:
    ExecEndGatherMerge((GatherMergeState *)node);
    break;

  case T_IndexScanState:
    ExecEndIndexScan((IndexScanState *)node);
    break;

  case T_IndexOnlyScanState:
    ExecEndIndexOnlyScan((IndexOnlyScanState *)node);
    break;

  case T_BitmapIndexScanState:
    ExecEndBitmapIndexScan((BitmapIndexScanState *)node);
    break;

  case T_BitmapHeapScanState:
    ExecEndBitmapHeapScan((BitmapHeapScanState *)node);
    break;

  case T_TidScanState:
    ExecEndTidScan((TidScanState *)node);
    break;

  case T_SubqueryScanState:
    ExecEndSubqueryScan((SubqueryScanState *)node);
    break;

  case T_FunctionScanState:
    ExecEndFunctionScan((FunctionScanState *)node);
    break;

  case T_TableFuncScanState:
    ExecEndTableFuncScan((TableFuncScanState *)node);
    break;

  case T_ValuesScanState:
    ExecEndValuesScan((ValuesScanState *)node);
    break;

  case T_CteScanState:
    ExecEndCteScan((CteScanState *)node);
    break;

  case T_NamedTuplestoreScanState:
    ExecEndNamedTuplestoreScan((NamedTuplestoreScanState *)node);
    break;

  case T_WorkTableScanState:
    ExecEndWorkTableScan((WorkTableScanState *)node);
    break;

  case T_ForeignScanState:
    ExecEndForeignScan((ForeignScanState *)node);
    break;

  case T_CustomScanState:
    ExecEndCustomScan((CustomScanState *)node);
    break;

       
                  
       
  case T_NestLoopState:
    ExecEndNestLoop((NestLoopState *)node);
    break;

  case T_MergeJoinState:
    ExecEndMergeJoin((MergeJoinState *)node);
    break;

  case T_HashJoinState:
    ExecEndHashJoin((HashJoinState *)node);
    break;

       
                             
       
  case T_MaterialState:
    ExecEndMaterial((MaterialState *)node);
    break;

  case T_SortState:
    ExecEndSort((SortState *)node);
    break;

  case T_GroupState:
    ExecEndGroup((GroupState *)node);
    break;

  case T_AggState:
    ExecEndAgg((AggState *)node);
    break;

  case T_WindowAggState:
    ExecEndWindowAgg((WindowAggState *)node);
    break;

  case T_UniqueState:
    ExecEndUnique((UniqueState *)node);
    break;

  case T_HashState:
    ExecEndHash((HashState *)node);
    break;

  case T_SetOpState:
    ExecEndSetOp((SetOpState *)node);
    break;

  case T_LockRowsState:
    ExecEndLockRows((LockRowsState *)node);
    break;

  case T_LimitState:
    ExecEndLimit((LimitState *)node);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
}

   
                    
   
                                                                           
                                         
   
bool
ExecShutdownNode(PlanState *node)
{
  return ExecShutdownNode_walker(node, NULL);
}

static bool
ExecShutdownNode_walker(PlanState *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }

  check_stack_depth();

     
                                                                           
                                                                         
                                                                           
                                                                     
                                                                           
                                                                             
                                                                          
                  
     
  if (node->instrument && node->instrument->running)
  {
    InstrStartNode(node->instrument);
  }

  planstate_tree_walker(node, ExecShutdownNode_walker, context);

  switch (nodeTag(node))
  {
  case T_GatherState:
    ExecShutdownGather((GatherState *)node);
    break;
  case T_ForeignScanState:
    ExecShutdownForeignScan((ForeignScanState *)node);
    break;
  case T_CustomScanState:
    ExecShutdownCustomScan((CustomScanState *)node);
    break;
  case T_GatherMergeState:
    ExecShutdownGatherMerge((GatherMergeState *)node);
    break;
  case T_HashState:
    ExecShutdownHash((HashState *)node);
    break;
  case T_HashJoinState:
    ExecShutdownHashJoin((HashJoinState *)node);
    break;
  default:
    break;
  }

                                                                 
  if (node->instrument && node->instrument->running)
  {
    InstrStopNode(node->instrument, 0);
  }

  return false;
}

   
                     
   
                                                                       
                                                                          
                                                                        
                                                                          
                               
   
                                                                          
                                                                            
   
                                                                         
                                                                          
                                               
   
void
ExecSetTupleBound(int64 tuples_needed, PlanState *child_node)
{
     
                                                                            
                                                                        
                                                                          
     

  if (IsA(child_node, SortState))
  {
       
                                                                     
       
                                                                         
                                                                         
                                                                       
                  
       
    SortState *sortState = (SortState *)child_node;

    if (tuples_needed < 0)
    {
                                                           
      sortState->bounded = false;
    }
    else
    {
      sortState->bounded = true;
      sortState->bound = tuples_needed;
    }
  }
  else if (IsA(child_node, AppendState))
  {
       
                                                                        
                                                                         
                                                 
       
    AppendState *aState = (AppendState *)child_node;
    int i;

    for (i = 0; i < aState->as_nplans; i++)
    {
      ExecSetTupleBound(tuples_needed, aState->appendplans[i]);
    }
  }
  else if (IsA(child_node, MergeAppendState))
  {
       
                                                                        
                                                                          
                                                              
       
    MergeAppendState *maState = (MergeAppendState *)child_node;
    int i;

    for (i = 0; i < maState->ms_nplans; i++)
    {
      ExecSetTupleBound(tuples_needed, maState->mergeplans[i]);
    }
  }
  else if (IsA(child_node, ResultState))
  {
       
                                                                         
                   
       
                                                                        
                                                                          
                                                                         
                                                           
       
    if (outerPlanState(child_node))
    {
      ExecSetTupleBound(tuples_needed, outerPlanState(child_node));
    }
  }
  else if (IsA(child_node, SubqueryScanState))
  {
       
                                                                       
                                               
       
    SubqueryScanState *subqueryState = (SubqueryScanState *)child_node;

    if (subqueryState->ss.ps.qual == NULL)
    {
      ExecSetTupleBound(tuples_needed, subqueryState->subplan);
    }
  }
  else if (IsA(child_node, GatherState))
  {
       
                                                                      
                                                                     
                                               
       
                                                                       
                                              
       
    GatherState *gstate = (GatherState *)child_node;

    gstate->tuples_needed = tuples_needed;

                                                                    
    ExecSetTupleBound(tuples_needed, outerPlanState(child_node));
  }
  else if (IsA(child_node, GatherMergeState))
  {
                                     
    GatherMergeState *gstate = (GatherMergeState *)child_node;

    gstate->tuples_needed = tuples_needed;

    ExecSetTupleBound(tuples_needed, outerPlanState(child_node));
  }

     
                                                                      
                                                                             
                                                                            
                                                                
     
}
