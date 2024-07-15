                                                                            
   
                 
                                                       
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
   
                      
   
                                              
                                                         
                                                
                                          
   
#include "postgres.h"

#include "access/sysattr.h"
#include "access/tableam.h"
#include "catalog/pg_type.h"
#include "executor/execdebug.h"
#include "executor/nodeTidscan.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "storage/bufmgr.h"
#include "utils/array.h"
#include "utils/rel.h"

#define IsCTIDVar(node) ((node) != NULL && IsA((node), Var) && ((Var *)(node))->varattno == SelfItemPointerAttributeNumber && ((Var *)(node))->varlevelsup == 0)

                                 
typedef struct TidExpr
{
  ExprState *exprstate;                                           
  bool isarray;                                                    
  CurrentOfExpr *cexpr;                                            
} TidExpr;

static void
TidExprListCreate(TidScanState *tidstate);
static void
TidListEval(TidScanState *tidstate);
static int
itemptr_comparator(const void *a, const void *b);
static TupleTableSlot *
TidNext(TidScanState *node);

   
                                                                  
                                                                     
   
                                                                
                                             
   
static void
TidExprListCreate(TidScanState *tidstate)
{
  TidScan *node = (TidScan *)tidstate->ss.ps.plan;
  ListCell *l;

  tidstate->tss_tidexprs = NIL;
  tidstate->tss_isCurrentOf = false;

  foreach (l, node->tidquals)
  {
    Expr *expr = (Expr *)lfirst(l);
    TidExpr *tidexpr = (TidExpr *)palloc0(sizeof(TidExpr));

    if (is_opclause(expr))
    {
      Node *arg1;
      Node *arg2;

      arg1 = get_leftop(expr);
      arg2 = get_rightop(expr);
      if (IsCTIDVar(arg1))
      {
        tidexpr->exprstate = ExecInitExpr((Expr *)arg2, &tidstate->ss.ps);
      }
      else if (IsCTIDVar(arg2))
      {
        tidexpr->exprstate = ExecInitExpr((Expr *)arg1, &tidstate->ss.ps);
      }
      else
      {
        elog(ERROR, "could not identify CTID variable");
      }
      tidexpr->isarray = false;
    }
    else if (expr && IsA(expr, ScalarArrayOpExpr))
    {
      ScalarArrayOpExpr *saex = (ScalarArrayOpExpr *)expr;

      Assert(IsCTIDVar(linitial(saex->args)));
      tidexpr->exprstate = ExecInitExpr(lsecond(saex->args), &tidstate->ss.ps);
      tidexpr->isarray = true;
    }
    else if (expr && IsA(expr, CurrentOfExpr))
    {
      CurrentOfExpr *cexpr = (CurrentOfExpr *)expr;

      tidexpr->cexpr = cexpr;
      tidstate->tss_isCurrentOf = true;
    }
    else
    {
      elog(ERROR, "could not identify CTID expression");
    }

    tidstate->tss_tidexprs = lappend(tidstate->tss_tidexprs, tidexpr);
  }

                                                                 
  Assert(list_length(tidstate->tss_tidexprs) == 1 || !tidstate->tss_isCurrentOf);
}

   
                                                                         
             
   
                                                  
   
static void
TidListEval(TidScanState *tidstate)
{
  ExprContext *econtext = tidstate->ss.ps.ps_ExprContext;
  TableScanDesc scan;
  ItemPointerData *tidList;
  int numAllocTids;
  int numTids;
  ListCell *l;

     
                                                                            
                                                                            
                                        
     
  if (tidstate->ss.ss_currentScanDesc == NULL)
  {
    tidstate->ss.ss_currentScanDesc = table_beginscan_tid(tidstate->ss.ss_currentRelation, tidstate->ss.ps.state->es_snapshot);
  }
  scan = tidstate->ss.ss_currentScanDesc;

     
                                                                           
                                                             
                                                           
     
  numAllocTids = list_length(tidstate->tss_tidexprs);
  tidList = (ItemPointerData *)palloc(numAllocTids * sizeof(ItemPointerData));
  numTids = 0;

  foreach (l, tidstate->tss_tidexprs)
  {
    TidExpr *tidexpr = (TidExpr *)lfirst(l);
    ItemPointer itemptr;
    bool isNull;

    if (tidexpr->exprstate && !tidexpr->isarray)
    {
      itemptr = (ItemPointer)DatumGetPointer(ExecEvalExprSwitchContext(tidexpr->exprstate, econtext, &isNull));
      if (isNull)
      {
        continue;
      }

         
                                                                    
                                                                        
                                                                         
                                                                      
                            
         
      if (!table_tuple_tid_valid(scan, itemptr))
      {
        continue;
      }

      if (numTids >= numAllocTids)
      {
        numAllocTids *= 2;
        tidList = (ItemPointerData *)repalloc(tidList, numAllocTids * sizeof(ItemPointerData));
      }
      tidList[numTids++] = *itemptr;
    }
    else if (tidexpr->exprstate && tidexpr->isarray)
    {
      Datum arraydatum;
      ArrayType *itemarray;
      Datum *ipdatums;
      bool *ipnulls;
      int ndatums;
      int i;

      arraydatum = ExecEvalExprSwitchContext(tidexpr->exprstate, econtext, &isNull);
      if (isNull)
      {
        continue;
      }
      itemarray = DatumGetArrayTypeP(arraydatum);
      deconstruct_array(itemarray, TIDOID, sizeof(ItemPointerData), false, 's', &ipdatums, &ipnulls, &ndatums);
      if (numTids + ndatums > numAllocTids)
      {
        numAllocTids = numTids + ndatums;
        tidList = (ItemPointerData *)repalloc(tidList, numAllocTids * sizeof(ItemPointerData));
      }
      for (i = 0; i < ndatums; i++)
      {
        if (ipnulls[i])
        {
          continue;
        }

        itemptr = (ItemPointer)DatumGetPointer(ipdatums[i]);

        if (!table_tuple_tid_valid(scan, itemptr))
        {
          continue;
        }

        tidList[numTids++] = *itemptr;
      }
      pfree(ipdatums);
      pfree(ipnulls);
    }
    else
    {
      ItemPointerData cursor_tid;

      Assert(tidexpr->cexpr);
      if (execCurrentOf(tidexpr->cexpr, econtext, RelationGetRelid(tidstate->ss.ss_currentRelation), &cursor_tid))
      {
        if (numTids >= numAllocTids)
        {
          numAllocTids *= 2;
          tidList = (ItemPointerData *)repalloc(tidList, numAllocTids * sizeof(ItemPointerData));
        }
        tidList[numTids++] = cursor_tid;
      }
    }
  }

     
                                                                  
                                                                           
                                                                             
                                                                    
     
  if (numTids > 1)
  {
    int lastTid;
    int i;

                                                                   
    Assert(!tidstate->tss_isCurrentOf);

    qsort((void *)tidList, numTids, sizeof(ItemPointerData), itemptr_comparator);
    lastTid = 0;
    for (i = 1; i < numTids; i++)
    {
      if (!ItemPointerEquals(&tidList[lastTid], &tidList[i]))
      {
        tidList[++lastTid] = tidList[i];
      }
    }
    numTids = lastTid + 1;
  }

  tidstate->tss_TidList = tidList;
  tidstate->tss_NumTids = numTids;
  tidstate->tss_TidPtr = -1;
}

   
                                              
   
static int
itemptr_comparator(const void *a, const void *b)
{
  const ItemPointerData *ipa = (const ItemPointerData *)a;
  const ItemPointerData *ipb = (const ItemPointerData *)b;
  BlockNumber ba = ItemPointerGetBlockNumber(ipa);
  BlockNumber bb = ItemPointerGetBlockNumber(ipb);
  OffsetNumber oa = ItemPointerGetOffsetNumber(ipa);
  OffsetNumber ob = ItemPointerGetOffsetNumber(ipb);

  if (ba < bb)
  {
    return -1;
  }
  if (ba > bb)
  {
    return 1;
  }
  if (oa < ob)
  {
    return -1;
  }
  if (oa > ob)
  {
    return 1;
  }
  return 0;
}

                                                                    
            
   
                                                             
                                                    
   
                                                                    
   
static TupleTableSlot *
TidNext(TidScanState *node)
{
  EState *estate;
  ScanDirection direction;
  Snapshot snapshot;
  TableScanDesc scan;
  Relation heapRelation;
  TupleTableSlot *slot;
  ItemPointerData *tidList;
  int numTids;
  bool bBackward;

     
                                                      
     
  estate = node->ss.ps.state;
  direction = estate->es_direction;
  snapshot = estate->es_snapshot;
  heapRelation = node->ss.ss_currentRelation;
  slot = node->ss.ss_ScanTupleSlot;

     
                                                                
     
  if (node->tss_TidList == NULL)
  {
    TidListEval(node);
  }

  scan = node->ss.ss_currentScanDesc;
  tidList = node->tss_TidList;
  numTids = node->tss_NumTids;

     
                                                                  
     
  bBackward = ScanDirectionIsBackward(direction);
  if (bBackward)
  {
    if (node->tss_TidPtr < 0)
    {
                                        
      node->tss_TidPtr = numTids - 1;
    }
    else
    {
      node->tss_TidPtr--;
    }
  }
  else
  {
    if (node->tss_TidPtr < 0)
    {
                                       
      node->tss_TidPtr = 0;
    }
    else
    {
      node->tss_TidPtr++;
    }
  }

  while (node->tss_TidPtr >= 0 && node->tss_TidPtr < numTids)
  {
    ItemPointerData tid = tidList[node->tss_TidPtr];

       
                                                                       
                                                                           
                                          
       
    if (node->tss_isCurrentOf)
    {
      table_tuple_get_latest_tid(scan, &tid);
    }

    if (table_tuple_fetch_row_version(heapRelation, &tid, snapshot, slot))
    {
      return slot;
    }

                                                   
    if (bBackward)
    {
      node->tss_TidPtr--;
    }
    else
    {
      node->tss_TidPtr++;
    }

    CHECK_FOR_INTERRUPTS();
  }

     
                                                                             
            
     
  return ExecClearTuple(slot);
}

   
                                                                          
   
static bool
TidRecheck(TidScanState *node, TupleTableSlot *slot)
{
     
                                                                         
                                                                         
                                                   
     
  return true;
}

                                                                    
                      
   
                                              
                                                             
                                                               
                             
   
                
                                                                       
                           
   
                    
                                                                   
                                                                
                       
                                                                    
   
static TupleTableSlot *
ExecTidScan(PlanState *pstate)
{
  TidScanState *node = castNode(TidScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)TidNext, (ExecScanRecheckMtd)TidRecheck);
}

                                                                    
                            
                                                                    
   
void
ExecReScanTidScan(TidScanState *node)
{
  if (node->tss_TidList)
  {
    pfree(node->tss_TidList);
  }
  node->tss_TidList = NULL;
  node->tss_NumTids = 0;
  node->tss_TidPtr = -1;

                                                 
  if (node->ss.ss_currentScanDesc)
  {
    table_rescan(node->ss.ss_currentScanDesc, NULL);
  }

  ExecScanReScan(&node->ss);
}

                                                                    
                   
   
                                                       
                     
                                                                    
   
void
ExecEndTidScan(TidScanState *node)
{
  if (node->ss.ss_currentScanDesc)
  {
    table_endscan(node->ss.ss_currentScanDesc);
  }

     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                                 
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

                                                                    
                    
   
                                                          
                                                     
   
                
                                                  
                                                           
                                                                    
   
TidScanState *
ExecInitTidScan(TidScan *node, EState *estate, int eflags)
{
  TidScanState *tidstate;
  Relation currentRelation;

     
                            
     
  tidstate = makeNode(TidScanState);
  tidstate->ss.ps.plan = (Plan *)node;
  tidstate->ss.ps.state = estate;
  tidstate->ss.ps.ExecProcNode = ExecTidScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &tidstate->ss.ps);

     
                                       
     
  tidstate->tss_TidList = NULL;
  tidstate->tss_NumTids = 0;
  tidstate->tss_TidPtr = -1;

     
                            
     
  currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid, eflags);

  tidstate->ss.ss_currentRelation = currentRelation;
  tidstate->ss.ss_currentScanDesc = NULL;                        

     
                                                     
     
  ExecInitScanTupleSlot(estate, &tidstate->ss, RelationGetDescr(currentRelation), table_slot_callbacks(currentRelation));

     
                                            
     
  ExecInitResultTypeTL(&tidstate->ss.ps);
  ExecAssignScanProjectionInfo(&tidstate->ss);

     
                                  
     
  tidstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)tidstate);

  TidExprListCreate(tidstate);

     
               
     
  return tidstate;
}
