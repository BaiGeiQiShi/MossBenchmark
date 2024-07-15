                                                                            
   
             
                                                   
   
                                                                         
                                                                        
   
                                  
   
                                                                            
   
#include "postgres.h"

#include "access/amapi.h"
#include "access/htup_details.h"
#include "executor/execdebug.h"
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
#include "nodes/extensible.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static bool
IndexSupportsBackwardScan(Oid indexid);

   
              
                                                            
   
                                                                      
                                                 
   
void
ExecReScan(PlanState *node)
{
                                               
  if (node->instrument)
  {
    InstrEndLoop(node->instrument);
  }

     
                                                         
     
                                                                    
                                                                         
                                                                            
                                                                          
                                                                          
                                                                         
                                                                       
                                                                             
                 
     
  if (node->chgParam != NULL)
  {
    ListCell *l;

    foreach (l, node->initPlan)
    {
      SubPlanState *sstate = (SubPlanState *)lfirst(l);
      PlanState *splan = sstate->planstate;

      if (splan->plan->extParam != NULL)                           
                                                           
      {
        UpdateChangedParamSet(splan, node->chgParam);
      }
      if (splan->chgParam != NULL)
      {
        ExecReScanSetParamPlan(sstate, node);
      }
    }
    foreach (l, node->subPlan)
    {
      SubPlanState *sstate = (SubPlanState *)lfirst(l);
      PlanState *splan = sstate->planstate;

      if (splan->plan->extParam != NULL)
      {
        UpdateChangedParamSet(splan, node->chgParam);
      }
    }
                                                      
    if (node->lefttree != NULL)
    {
      UpdateChangedParamSet(node->lefttree, node->chgParam);
    }
    if (node->righttree != NULL)
    {
      UpdateChangedParamSet(node->righttree, node->chgParam);
    }
  }

                                 
  if (node->ps_ExprContext)
  {
    ReScanExprContext(node->ps_ExprContext);
  }

                                            
  switch (nodeTag(node))
  {
  case T_ResultState:
    ExecReScanResult((ResultState *)node);
    break;

  case T_ProjectSetState:
    ExecReScanProjectSet((ProjectSetState *)node);
    break;

  case T_ModifyTableState:
    ExecReScanModifyTable((ModifyTableState *)node);
    break;

  case T_AppendState:
    ExecReScanAppend((AppendState *)node);
    break;

  case T_MergeAppendState:
    ExecReScanMergeAppend((MergeAppendState *)node);
    break;

  case T_RecursiveUnionState:
    ExecReScanRecursiveUnion((RecursiveUnionState *)node);
    break;

  case T_BitmapAndState:
    ExecReScanBitmapAnd((BitmapAndState *)node);
    break;

  case T_BitmapOrState:
    ExecReScanBitmapOr((BitmapOrState *)node);
    break;

  case T_SeqScanState:
    ExecReScanSeqScan((SeqScanState *)node);
    break;

  case T_SampleScanState:
    ExecReScanSampleScan((SampleScanState *)node);
    break;

  case T_GatherState:
    ExecReScanGather((GatherState *)node);
    break;

  case T_GatherMergeState:
    ExecReScanGatherMerge((GatherMergeState *)node);
    break;

  case T_IndexScanState:
    ExecReScanIndexScan((IndexScanState *)node);
    break;

  case T_IndexOnlyScanState:
    ExecReScanIndexOnlyScan((IndexOnlyScanState *)node);
    break;

  case T_BitmapIndexScanState:
    ExecReScanBitmapIndexScan((BitmapIndexScanState *)node);
    break;

  case T_BitmapHeapScanState:
    ExecReScanBitmapHeapScan((BitmapHeapScanState *)node);
    break;

  case T_TidScanState:
    ExecReScanTidScan((TidScanState *)node);
    break;

  case T_SubqueryScanState:
    ExecReScanSubqueryScan((SubqueryScanState *)node);
    break;

  case T_FunctionScanState:
    ExecReScanFunctionScan((FunctionScanState *)node);
    break;

  case T_TableFuncScanState:
    ExecReScanTableFuncScan((TableFuncScanState *)node);
    break;

  case T_ValuesScanState:
    ExecReScanValuesScan((ValuesScanState *)node);
    break;

  case T_CteScanState:
    ExecReScanCteScan((CteScanState *)node);
    break;

  case T_NamedTuplestoreScanState:
    ExecReScanNamedTuplestoreScan((NamedTuplestoreScanState *)node);
    break;

  case T_WorkTableScanState:
    ExecReScanWorkTableScan((WorkTableScanState *)node);
    break;

  case T_ForeignScanState:
    ExecReScanForeignScan((ForeignScanState *)node);
    break;

  case T_CustomScanState:
    ExecReScanCustomScan((CustomScanState *)node);
    break;

  case T_NestLoopState:
    ExecReScanNestLoop((NestLoopState *)node);
    break;

  case T_MergeJoinState:
    ExecReScanMergeJoin((MergeJoinState *)node);
    break;

  case T_HashJoinState:
    ExecReScanHashJoin((HashJoinState *)node);
    break;

  case T_MaterialState:
    ExecReScanMaterial((MaterialState *)node);
    break;

  case T_SortState:
    ExecReScanSort((SortState *)node);
    break;

  case T_GroupState:
    ExecReScanGroup((GroupState *)node);
    break;

  case T_AggState:
    ExecReScanAgg((AggState *)node);
    break;

  case T_WindowAggState:
    ExecReScanWindowAgg((WindowAggState *)node);
    break;

  case T_UniqueState:
    ExecReScanUnique((UniqueState *)node);
    break;

  case T_HashState:
    ExecReScanHash((HashState *)node);
    break;

  case T_SetOpState:
    ExecReScanSetOp((SetOpState *)node);
    break;

  case T_LockRowsState:
    ExecReScanLockRows((LockRowsState *)node);
    break;

  case T_LimitState:
    ExecReScanLimit((LimitState *)node);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }

  if (node->chgParam != NULL)
  {
    bms_free(node->chgParam);
    node->chgParam = NULL;
  }
}

   
               
   
                                    
   
                                                                         
                                                                            
                                                                             
                                                                          
                                                                         
                                                                           
                                              
   
void
ExecMarkPos(PlanState *node)
{
  switch (nodeTag(node))
  {
  case T_IndexScanState:
    ExecIndexMarkPos((IndexScanState *)node);
    break;

  case T_IndexOnlyScanState:
    ExecIndexOnlyMarkPos((IndexOnlyScanState *)node);
    break;

  case T_CustomScanState:
    ExecCustomMarkPos((CustomScanState *)node);
    break;

  case T_MaterialState:
    ExecMaterialMarkPos((MaterialState *)node);
    break;

  case T_SortState:
    ExecSortMarkPos((SortState *)node);
    break;

  case T_ResultState:
    ExecResultMarkPos((ResultState *)node);
    break;

  default:
                                                                
    elog(DEBUG2, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
}

   
                
   
                                                                  
   
                                                                         
                                                                              
                                                                          
                                                                          
                                                                         
                                                                      
                                                  
   
void
ExecRestrPos(PlanState *node)
{
  switch (nodeTag(node))
  {
  case T_IndexScanState:
    ExecIndexRestrPos((IndexScanState *)node);
    break;

  case T_IndexOnlyScanState:
    ExecIndexOnlyRestrPos((IndexOnlyScanState *)node);
    break;

  case T_CustomScanState:
    ExecCustomRestrPos((CustomScanState *)node);
    break;

  case T_MaterialState:
    ExecMaterialRestrPos((MaterialState *)node);
    break;

  case T_SortState:
    ExecSortRestrPos((SortState *)node);
    break;

  case T_ResultState:
    ExecResultRestrPos((ResultState *)node);
    break;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
    break;
  }
}

   
                                                               
   
                                                                       
                                                                         
                                               
   
bool
ExecSupportsMarkRestore(Path *pathnode)
{
     
                                                                            
                                                                         
              
     
  switch (pathnode->pathtype)
  {
  case T_IndexScan:
  case T_IndexOnlyScan:
       
                                                 
       
    return castNode(IndexPath, pathnode)->indexinfo->amcanmarkpos;

  case T_Material:
  case T_Sort:
    return true;

  case T_CustomScan:
  {
    CustomPath *customPath = castNode(CustomPath, pathnode);

    if (customPath->flags & CUSTOMPATH_SUPPORT_MARK_RESTORE)
    {
      return true;
    }
    return false;
  }
  case T_Result:

       
                                                                       
       
                                                                      
                                                 
       
    if (IsA(pathnode, ProjectionPath))
    {
      return ExecSupportsMarkRestore(((ProjectionPath *)pathnode)->subpath);
    }
    else if (IsA(pathnode, MinMaxAggPath))
    {
      return false;                       
    }
    else if (IsA(pathnode, GroupResultPath))
    {
      return false;                       
    }
    else
    {
                                           
      Assert(IsA(pathnode, Path));
      return false;                       
    }

  case T_Append:
  {
    AppendPath *appendPath = castNode(AppendPath, pathnode);

       
                                                                  
                                                               
                            
       
    if (list_length(appendPath->subpaths) == 1)
    {
      return ExecSupportsMarkRestore((Path *)linitial(appendPath->subpaths));
    }
                                           
    return false;
  }

  case T_MergeAppend:
  {
    MergeAppendPath *mapath = castNode(MergeAppendPath, pathnode);

       
                                                               
                                                              
                             
       
    if (list_length(mapath->subpaths) == 1)
    {
      return ExecSupportsMarkRestore((Path *)linitial(mapath->subpaths));
    }
                                                
    return false;
  }

  default:
    break;
  }

  return false;
}

   
                                                                           
   
                                                                        
                                                                             
                                                                         
                                                                              
   
bool
ExecSupportsBackwardScan(Plan *node)
{
  if (node == NULL)
  {
    return false;
  }

     
                                                                            
                                                                         
                                                                            
     
  if (node->parallel_aware)
  {
    return false;
  }

  switch (nodeTag(node))
  {
  case T_Result:
    if (outerPlan(node) != NULL)
    {
      return ExecSupportsBackwardScan(outerPlan(node));
    }
    else
    {
      return false;
    }

  case T_Append:
  {
    ListCell *l;

    foreach (l, ((Append *)node)->appendplans)
    {
      if (!ExecSupportsBackwardScan((Plan *)lfirst(l)))
      {
        return false;
      }
    }
                                                                 
    return true;
  }

  case T_SampleScan:
                                                                   
    return false;

  case T_Gather:
    return false;

  case T_IndexScan:
    return IndexSupportsBackwardScan(((IndexScan *)node)->indexid);

  case T_IndexOnlyScan:
    return IndexSupportsBackwardScan(((IndexOnlyScan *)node)->indexid);

  case T_SubqueryScan:
    return ExecSupportsBackwardScan(((SubqueryScan *)node)->subplan);

  case T_CustomScan:
  {
    uint32 flags = ((CustomScan *)node)->flags;

    if (flags & CUSTOMPATH_SUPPORT_BACKWARD_SCAN)
    {
      return true;
    }
  }
    return false;

  case T_SeqScan:
  case T_TidScan:
  case T_FunctionScan:
  case T_ValuesScan:
  case T_CteScan:
  case T_Material:
  case T_Sort:
    return true;

  case T_LockRows:
  case T_Limit:
    return ExecSupportsBackwardScan(outerPlan(node));

  default:
    return false;
  }
}

   
                                                                         
                    
   
static bool
IndexSupportsBackwardScan(Oid indexid)
{
  bool result;
  HeapTuple ht_idxrel;
  Form_pg_class idxrelrec;
  IndexAmRoutine *amroutine;

                                                      
  ht_idxrel = SearchSysCache1(RELOID, ObjectIdGetDatum(indexid));
  if (!HeapTupleIsValid(ht_idxrel))
  {
    elog(ERROR, "cache lookup failed for relation %u", indexid);
  }
  idxrelrec = (Form_pg_class)GETSTRUCT(ht_idxrel);

                                       
  amroutine = GetIndexAmRoutineByAmId(idxrelrec->relam, false);

  result = amroutine->amcanbackward;

  pfree(amroutine);
  ReleaseSysCache(ht_idxrel);

  return result;
}

   
                                                                     
   
                                                                             
                                                                          
                                                                         
                            
   
bool
ExecMaterializesOutput(NodeTag plantype)
{
  switch (plantype)
  {
  case T_Material:
  case T_FunctionScan:
  case T_TableFuncScan:
  case T_CteScan:
  case T_NamedTuplestoreScan:
  case T_WorkTableScan:
  case T_Sort:
    return true;

  default:
    break;
  }

  return false;
}
