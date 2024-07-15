                                                                            
   
                   
                                                    
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
   
                      
                                                    
                                                 
                                                                
                                                           
                                                       
                                            
                                           
                                               
                                                                             
                                                                     
                                                                 
                                                                        
   
#include "postgres.h"

#include "access/nbtree.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "catalog/pg_am.h"
#include "executor/execdebug.h"
#include "executor/nodeIndexscan.h"
#include "lib/pairingheap.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/array.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                         
                                                                        
   
typedef struct
{
  pairingheap_node ph_node;
  HeapTuple htup;
  Datum *orderbyvals;
  bool *orderbynulls;
} ReorderTuple;

static TupleTableSlot *
IndexNext(IndexScanState *node);
static TupleTableSlot *
IndexNextWithReorder(IndexScanState *node);
static void
EvalOrderByExpressions(IndexScanState *node, ExprContext *econtext);
static bool
IndexRecheck(IndexScanState *node, TupleTableSlot *slot);
static int
cmp_orderbyvals(const Datum *adist, const bool *anulls, const Datum *bdist, const bool *bnulls, IndexScanState *node);
static int
reorderqueue_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg);
static void
reorderqueue_push(IndexScanState *node, TupleTableSlot *slot, Datum *orderbyvals, bool *orderbynulls);
static HeapTuple
reorderqueue_pop(IndexScanState *node);

                                                                    
              
   
                                                               
                                                                 
                                                                    
   
static TupleTableSlot *
IndexNext(IndexScanState *node)
{
  EState *estate;
  ExprContext *econtext;
  ScanDirection direction;
  IndexScanDesc scandesc;
  TupleTableSlot *slot;

     
                                                        
     
  estate = node->ss.ps.state;
  direction = estate->es_direction;
                                                          
  if (ScanDirectionIsBackward(((IndexScan *)node->ss.ps.plan)->indexorderdir))
  {
    if (ScanDirectionIsForward(direction))
    {
      direction = BackwardScanDirection;
    }
    else if (ScanDirectionIsBackward(direction))
    {
      direction = ForwardScanDirection;
    }
  }
  scandesc = node->iss_ScanDesc;
  econtext = node->ss.ps.ps_ExprContext;
  slot = node->ss.ss_ScanTupleSlot;

  if (scandesc == NULL)
  {
       
                                                                    
                                                                         
       
    scandesc = index_beginscan(node->ss.ss_currentRelation, node->iss_RelationDesc, estate->es_snapshot, node->iss_NumScanKeys, node->iss_NumOrderByKeys);

    node->iss_ScanDesc = scandesc;

       
                                                                        
                                          
       
    if (node->iss_NumRuntimeKeys == 0 || node->iss_RuntimeKeysReady)
    {
      index_rescan(scandesc, node->iss_ScanKeys, node->iss_NumScanKeys, node->iss_OrderByKeys, node->iss_NumOrderByKeys);
    }
  }

     
                                                              
     
  while (index_getnext_slot(scandesc, direction, slot))
  {
    CHECK_FOR_INTERRUPTS();

       
                                                                        
                          
       
    if (scandesc->xs_recheck)
    {
      econtext->ecxt_scantuple = slot;
      if (!ExecQualAndReset(node->indexqualorig, econtext))
      {
                                                                 
        InstrCountFiltered2(node, 1);
        continue;
      }
    }

    return slot;
  }

     
                                                                           
                
     
  node->iss_ReachedEnd = true;
  return ExecClearTuple(slot);
}

                                                                    
                         
   
                                                                
                                                      
                                                                    
   
static TupleTableSlot *
IndexNextWithReorder(IndexScanState *node)
{
  EState *estate;
  ExprContext *econtext;
  IndexScanDesc scandesc;
  TupleTableSlot *slot;
  ReorderTuple *topmost = NULL;
  bool was_exact;
  Datum *lastfetched_vals;
  bool *lastfetched_nulls;
  int cmp;

  estate = node->ss.ps.state;

     
                                                                            
                                                                         
                                                                      
                                                                     
                                                       
                                                                    
                 
     
  Assert(!ScanDirectionIsBackward(((IndexScan *)node->ss.ps.plan)->indexorderdir));
  Assert(ScanDirectionIsForward(estate->es_direction));

  scandesc = node->iss_ScanDesc;
  econtext = node->ss.ps.ps_ExprContext;
  slot = node->ss.ss_ScanTupleSlot;

  if (scandesc == NULL)
  {
       
                                                                    
                                                                         
       
    scandesc = index_beginscan(node->ss.ss_currentRelation, node->iss_RelationDesc, estate->es_snapshot, node->iss_NumScanKeys, node->iss_NumOrderByKeys);

    node->iss_ScanDesc = scandesc;

       
                                                                        
                                          
       
    if (node->iss_NumRuntimeKeys == 0 || node->iss_RuntimeKeysReady)
    {
      index_rescan(scandesc, node->iss_ScanKeys, node->iss_NumScanKeys, node->iss_OrderByKeys, node->iss_NumOrderByKeys);
    }
  }

  for (;;)
  {
    CHECK_FOR_INTERRUPTS();

       
                                                                         
                                                                       
                                                    
       
    if (!pairingheap_is_empty(node->iss_ReorderQueue))
    {
      topmost = (ReorderTuple *)pairingheap_first(node->iss_ReorderQueue);

      if (node->iss_ReachedEnd || cmp_orderbyvals(topmost->orderbyvals, topmost->orderbynulls, scandesc->xs_orderbyvals, scandesc->xs_orderbynulls, node) <= 0)
      {
        HeapTuple tuple;

        tuple = reorderqueue_pop(node);

                                                                       
        ExecForceStoreHeapTuple(tuple, slot, true);
        return slot;
      }
    }
    else if (node->iss_ReachedEnd)
    {
                                                                       
      return ExecClearTuple(slot);
    }

       
                                        
       
  next_indextuple:
    if (!index_getnext_slot(scandesc, ForwardScanDirection, slot))
    {
         
                                                                        
                                                            
         
      node->iss_ReachedEnd = true;
      continue;
    }

       
                                                                      
                                                     
       
    if (scandesc->xs_recheck)
    {
      econtext->ecxt_scantuple = slot;
      if (!ExecQualAndReset(node->indexqualorig, econtext))
      {
                                                                 
        InstrCountFiltered2(node, 1);
                                               
        CHECK_FOR_INTERRUPTS();
        goto next_indextuple;
      }
    }

    if (scandesc->xs_recheckorderby)
    {
      econtext->ecxt_scantuple = slot;
      ResetExprContext(econtext);
      EvalOrderByExpressions(node, econtext);

         
                                                                     
                                                                         
                                                                     
                                                                       
                                                                        
                                                                     
                                                                         
         
      cmp = cmp_orderbyvals(node->iss_OrderByValues, node->iss_OrderByNulls, scandesc->xs_orderbyvals, scandesc->xs_orderbynulls, node);
      if (cmp < 0)
      {
        elog(ERROR, "index returned tuples in wrong order");
      }
      else if (cmp == 0)
      {
        was_exact = true;
      }
      else
      {
        was_exact = false;
      }
      lastfetched_vals = node->iss_OrderByValues;
      lastfetched_nulls = node->iss_OrderByNulls;
    }
    else
    {
      was_exact = true;
      lastfetched_vals = scandesc->xs_orderbyvals;
      lastfetched_nulls = scandesc->xs_orderbynulls;
    }

       
                                                                          
                                                                         
                                                                         
                                                                           
                                                                           
                
       
    if (!was_exact || (topmost && cmp_orderbyvals(lastfetched_vals, lastfetched_nulls, topmost->orderbyvals, topmost->orderbynulls, node) > 0))
    {
                                       
      reorderqueue_push(node, slot, lastfetched_vals, lastfetched_nulls);
      continue;
    }
    else
    {
                                              
      return slot;
    }
  }

     
                                                                           
                
     
  return ExecClearTuple(slot);
}

   
                                                                              
   
static void
EvalOrderByExpressions(IndexScanState *node, ExprContext *econtext)
{
  int i;
  ListCell *l;
  MemoryContext oldContext;

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  i = 0;
  foreach (l, node->indexorderbyorig)
  {
    ExprState *orderby = (ExprState *)lfirst(l);

    node->iss_OrderByValues[i] = ExecEvalExpr(orderby, econtext, &node->iss_OrderByNulls[i]);
    i++;
  }

  MemoryContextSwitchTo(oldContext);
}

   
                                                                            
   
static bool
IndexRecheck(IndexScanState *node, TupleTableSlot *slot)
{
  ExprContext *econtext;

     
                                                        
     
  econtext = node->ss.ps.ps_ExprContext;

                                                    
  econtext->ecxt_scantuple = slot;
  return ExecQualAndReset(node->indexqualorig, econtext);
}

   
                                       
   
static int
cmp_orderbyvals(const Datum *adist, const bool *anulls, const Datum *bdist, const bool *bnulls, IndexScanState *node)
{
  int i;
  int result;

  for (i = 0; i < node->iss_NumOrderByKeys; i++)
  {
    SortSupport ssup = &node->iss_SortSupport[i];

       
                                                                           
                                                               
                                 
       
    if (anulls[i] && !bnulls[i])
    {
      return 1;
    }
    else if (!anulls[i] && bnulls[i])
    {
      return -1;
    }
    else if (anulls[i] && bnulls[i])
    {
      return 0;
    }

    result = ssup->comparator(adist[i], bdist[i], ssup);
    if (result != 0)
    {
      return result;
    }
  }

  return 0;
}

   
                                                                               
                                                         
   
static int
reorderqueue_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{
  ReorderTuple *rta = (ReorderTuple *)a;
  ReorderTuple *rtb = (ReorderTuple *)b;
  IndexScanState *node = (IndexScanState *)arg;

                                                        
  return cmp_orderbyvals(rtb->orderbyvals, rtb->orderbynulls, rta->orderbyvals, rta->orderbynulls, node);
}

   
                                                         
   
static void
reorderqueue_push(IndexScanState *node, TupleTableSlot *slot, Datum *orderbyvals, bool *orderbynulls)
{
  IndexScanDesc scandesc = node->iss_ScanDesc;
  EState *estate = node->ss.ps.state;
  MemoryContext oldContext = MemoryContextSwitchTo(estate->es_query_cxt);
  ReorderTuple *rt;
  int i;

  rt = (ReorderTuple *)palloc(sizeof(ReorderTuple));
  rt->htup = ExecCopySlotHeapTuple(slot);
  rt->orderbyvals = (Datum *)palloc(sizeof(Datum) * scandesc->numberOfOrderBys);
  rt->orderbynulls = (bool *)palloc(sizeof(bool) * scandesc->numberOfOrderBys);
  for (i = 0; i < node->iss_NumOrderByKeys; i++)
  {
    if (!orderbynulls[i])
    {
      rt->orderbyvals[i] = datumCopy(orderbyvals[i], node->iss_OrderByTypByVals[i], node->iss_OrderByTypLens[i]);
    }
    else
    {
      rt->orderbyvals[i] = (Datum)0;
    }
    rt->orderbynulls[i] = orderbynulls[i];
  }
  pairingheap_add(node->iss_ReorderQueue, &rt->ph_node);

  MemoryContextSwitchTo(oldContext);
}

   
                                                                 
   
static HeapTuple
reorderqueue_pop(IndexScanState *node)
{
  HeapTuple result;
  ReorderTuple *topmost;
  int i;

  topmost = (ReorderTuple *)pairingheap_remove_first(node->iss_ReorderQueue);

  result = topmost->htup;
  for (i = 0; i < node->iss_NumOrderByKeys; i++)
  {
    if (!node->iss_OrderByTypByVals[i] && !topmost->orderbynulls[i])
    {
      pfree(DatumGetPointer(topmost->orderbyvals[i]));
    }
  }
  pfree(topmost->orderbyvals);
  pfree(topmost->orderbynulls);
  pfree(topmost);

  return result;
}

                                                                    
                        
                                                                    
   
static TupleTableSlot *
ExecIndexScan(PlanState *pstate)
{
  IndexScanState *node = castNode(IndexScanState, pstate);

     
                                                                             
     
  if (node->iss_NumRuntimeKeys != 0 && !node->iss_RuntimeKeysReady)
  {
    ExecReScan((PlanState *)node);
  }

  if (node->iss_NumOrderByKeys > 0)
  {
    return ExecScan(&node->ss, (ExecScanAccessMtd)IndexNextWithReorder, (ExecScanRecheckMtd)IndexRecheck);
  }
  else
  {
    return ExecScan(&node->ss, (ExecScanAccessMtd)IndexNext, (ExecScanRecheckMtd)IndexRecheck);
  }
}

                                                                    
                              
   
                                                                    
                                                                     
   
                                                          
                                                              
                                                                   
                                                                    
   
void
ExecReScanIndexScan(IndexScanState *node)
{
     
                                                                        
                                                                            
                                                                      
                                                                             
                   
     
  if (node->iss_NumRuntimeKeys != 0)
  {
    ExprContext *econtext = node->iss_RuntimeContext;

    ResetExprContext(econtext);
    ExecIndexEvalRuntimeKeys(econtext, node->iss_RuntimeKeys, node->iss_NumRuntimeKeys);
  }
  node->iss_RuntimeKeysReady = true;

                               
  if (node->iss_ReorderQueue)
  {
    HeapTuple tuple;
    while (!pairingheap_is_empty(node->iss_ReorderQueue))
    {
      tuple = reorderqueue_pop(node);
      heap_freetuple(tuple);
    }
  }

                        
  if (node->iss_ScanDesc)
  {
    index_rescan(node->iss_ScanDesc, node->iss_ScanKeys, node->iss_NumScanKeys, node->iss_OrderByKeys, node->iss_NumOrderByKeys);
  }
  node->iss_ReachedEnd = false;

  ExecScanReScan(&node->ss);
}

   
                            
                                                              
   
void
ExecIndexEvalRuntimeKeys(ExprContext *econtext, IndexRuntimeKeyInfo *runtimeKeys, int numRuntimeKeys)
{
  int j;
  MemoryContext oldContext;

                                                          
  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  for (j = 0; j < numRuntimeKeys; j++)
  {
    ScanKey scan_key = runtimeKeys[j].scan_key;
    ExprState *key_expr = runtimeKeys[j].key_expr;
    Datum scanvalue;
    bool isNull;

       
                                                                           
                                                                         
                                 
       
                                                                        
                                                 
                                                                        
                                                                           
                                                                      
                  
       
                                                                    
                                                                      
                                                                      
                               
       
    scanvalue = ExecEvalExpr(key_expr, econtext, &isNull);
    if (isNull)
    {
      scan_key->sk_argument = scanvalue;
      scan_key->sk_flags |= SK_ISNULL;
    }
    else
    {
      if (runtimeKeys[j].key_toastable)
      {
        scanvalue = PointerGetDatum(PG_DETOAST_DATUM(scanvalue));
      }
      scan_key->sk_argument = scanvalue;
      scan_key->sk_flags &= ~SK_ISNULL;
    }
  }

  MemoryContextSwitchTo(oldContext);
}

   
                          
                                                                         
   
                                                                           
                                                                          
                                                                               
   
bool
ExecIndexEvalArrayKeys(ExprContext *econtext, IndexArrayKeyInfo *arrayKeys, int numArrayKeys)
{
  bool result = true;
  int j;
  MemoryContext oldContext;

                                                      
  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  for (j = 0; j < numArrayKeys; j++)
  {
    ScanKey scan_key = arrayKeys[j].scan_key;
    ExprState *array_expr = arrayKeys[j].array_expr;
    Datum arraydatum;
    bool isNull;
    ArrayType *arrayval;
    int16 elmlen;
    bool elmbyval;
    char elmalign;
    int num_elems;
    Datum *elem_values;
    bool *elem_nulls;

       
                                                               
                                                   
       
    arraydatum = ExecEvalExpr(array_expr, econtext, &isNull);
    if (isNull)
    {
      result = false;
      break;                                  
    }
    arrayval = DatumGetArrayTypeP(arraydatum);
                                                               
    get_typlenbyvalalign(ARR_ELEMTYPE(arrayval), &elmlen, &elmbyval, &elmalign);
    deconstruct_array(arrayval, ARR_ELEMTYPE(arrayval), elmlen, elmbyval, elmalign, &elem_values, &elem_nulls, &num_elems);
    if (num_elems <= 0)
    {
      result = false;
      break;                                  
    }

       
                                                              
                                                                        
                     
       
    arrayKeys[j].elem_values = elem_values;
    arrayKeys[j].elem_nulls = elem_nulls;
    arrayKeys[j].num_elems = num_elems;
    scan_key->sk_argument = elem_values[0];
    if (elem_nulls[0])
    {
      scan_key->sk_flags |= SK_ISNULL;
    }
    else
    {
      scan_key->sk_flags &= ~SK_ISNULL;
    }
    arrayKeys[j].next_elem = 1;
  }

  MemoryContextSwitchTo(oldContext);

  return result;
}

   
                             
                                                         
   
                                                                             
                                                                             
   
bool
ExecIndexAdvanceArrayKeys(IndexArrayKeyInfo *arrayKeys, int numArrayKeys)
{
  bool found = false;
  int j;

     
                                                                         
                                                                     
                                                                           
                          
     
  for (j = numArrayKeys - 1; j >= 0; j--)
  {
    ScanKey scan_key = arrayKeys[j].scan_key;
    int next_elem = arrayKeys[j].next_elem;
    int num_elems = arrayKeys[j].num_elems;
    Datum *elem_values = arrayKeys[j].elem_values;
    bool *elem_nulls = arrayKeys[j].elem_nulls;

    if (next_elem >= num_elems)
    {
      next_elem = 0;
      found = false;                                     
    }
    else
    {
      found = true;
    }
    scan_key->sk_argument = elem_values[next_elem];
    if (elem_nulls[next_elem])
    {
      scan_key->sk_flags |= SK_ISNULL;
    }
    else
    {
      scan_key->sk_flags &= ~SK_ISNULL;
    }
    arrayKeys[j].next_elem = next_elem + 1;
    if (found)
    {
      break;
    }
  }

  return found;
}

                                                                    
                     
                                                                    
   
void
ExecEndIndexScan(IndexScanState *node)
{
  Relation indexRelationDesc;
  IndexScanDesc indexScanDesc;

     
                                       
     
  indexRelationDesc = node->iss_RelationDesc;
  indexScanDesc = node->iss_ScanDesc;

     
                                                                        
     
#ifdef NOT_USED
  ExecFreeExprContext(&node->ss.ps);
  if (node->iss_RuntimeContext)
  {
    FreeExprContext(node->iss_RuntimeContext, true);
  }
#endif

     
                                 
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                                           
     
  if (indexScanDesc)
  {
    index_endscan(indexScanDesc);
  }
  if (indexRelationDesc)
  {
    index_close(indexRelationDesc, NoLock);
  }
}

                                                                    
                     
   
                                                                            
                                                                     
                                                                    
   
void
ExecIndexMarkPos(IndexScanState *node)
{
  EState *estate = node->ss.ps.state;
  EPQState *epqstate = estate->es_epq_active;

  if (epqstate != NULL)
  {
       
                                                                          
                                                                           
                                                                 
                                                                          
                                                                       
                                                                    
                                                   
       
    Index scanrelid = ((Scan *)node->ss.ps.plan)->scanrelid;

    Assert(scanrelid > 0);
    if (epqstate->relsubs_slot[scanrelid - 1] != NULL || epqstate->relsubs_rowmark[scanrelid - 1] != NULL)
    {
                                  
      if (!epqstate->relsubs_done[scanrelid - 1])
      {
        elog(ERROR, "unexpected ExecIndexMarkPos call in EPQ recheck");
      }
      return;
    }
  }

  index_markpos(node->iss_ScanDesc);
}

                                                                    
                      
                                                                    
   
void
ExecIndexRestrPos(IndexScanState *node)
{
  EState *estate = node->ss.ps.state;
  EPQState *epqstate = estate->es_epq_active;

  if (estate->es_epq_active != NULL)
  {
                                          
    Index scanrelid = ((Scan *)node->ss.ps.plan)->scanrelid;

    Assert(scanrelid > 0);
    if (epqstate->relsubs_slot[scanrelid - 1] != NULL || epqstate->relsubs_rowmark[scanrelid - 1] != NULL)
    {
                                  
      if (!epqstate->relsubs_done[scanrelid - 1])
      {
        elog(ERROR, "unexpected ExecIndexRestrPos call in EPQ recheck");
      }
      return;
    }
  }

  index_restrpos(node->iss_ScanDesc);
}

                                                                    
                      
   
                                                            
                                                       
   
                                                               
                                                          
                       
                                                                    
   
IndexScanState *
ExecInitIndexScan(IndexScan *node, EState *estate, int eflags)
{
  IndexScanState *indexstate;
  Relation currentRelation;
  LOCKMODE lockmode;

     
                            
     
  indexstate = makeNode(IndexScanState);
  indexstate->ss.ps.plan = (Plan *)node;
  indexstate->ss.ps.state = estate;
  indexstate->ss.ps.ExecProcNode = ExecIndexScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &indexstate->ss.ps);

     
                            
     
  currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid, eflags);

  indexstate->ss.ss_currentRelation = currentRelation;
  indexstate->ss.ss_currentScanDesc = NULL;                        

     
                                                     
     
  ExecInitScanTupleSlot(estate, &indexstate->ss, RelationGetDescr(currentRelation), table_slot_callbacks(currentRelation));

     
                                            
     
  ExecInitResultTypeTL(&indexstate->ss.ps);
  ExecAssignScanProjectionInfo(&indexstate->ss);

     
                                  
     
                                                                         
                                                                        
                                                                       
                                                                             
                                                                           
                                             
     
  indexstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)indexstate);
  indexstate->indexqualorig = ExecInitQual(node->indexqualorig, (PlanState *)indexstate);
  indexstate->indexorderbyorig = ExecInitExprList(node->indexorderbyorig, (PlanState *)indexstate);

     
                                                                           
                                                                             
                                        
     
  if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
  {
    return indexstate;
  }

                                
  lockmode = exec_rt_fetch(node->scan.scanrelid, estate)->rellockmode;
  indexstate->iss_RelationDesc = index_open(node->indexid, lockmode);

     
                                          
     
  indexstate->iss_RuntimeKeysReady = false;
  indexstate->iss_RuntimeKeys = NULL;
  indexstate->iss_NumRuntimeKeys = 0;

     
                                                            
     
  ExecIndexBuildScanKeys((PlanState *)indexstate, indexstate->iss_RelationDesc, node->indexqual, false, &indexstate->iss_ScanKeys, &indexstate->iss_NumScanKeys, &indexstate->iss_RuntimeKeys, &indexstate->iss_NumRuntimeKeys, NULL,                   
      NULL);

     
                                                                        
     
  ExecIndexBuildScanKeys((PlanState *)indexstate, indexstate->iss_RelationDesc, node->indexorderby, true, &indexstate->iss_OrderByKeys, &indexstate->iss_NumOrderByKeys, &indexstate->iss_RuntimeKeys, &indexstate->iss_NumRuntimeKeys, NULL,                   
      NULL);

                                                                      
  if (indexstate->iss_NumOrderByKeys > 0)
  {
    int numOrderByKeys = indexstate->iss_NumOrderByKeys;
    int i;
    ListCell *lco;
    ListCell *lcx;

       
                                                                         
                   
       
    Assert(numOrderByKeys == list_length(node->indexorderbyops));
    Assert(numOrderByKeys == list_length(node->indexorderbyorig));
    indexstate->iss_SortSupport = (SortSupportData *)palloc0(numOrderByKeys * sizeof(SortSupportData));
    indexstate->iss_OrderByTypByVals = (bool *)palloc(numOrderByKeys * sizeof(bool));
    indexstate->iss_OrderByTypLens = (int16 *)palloc(numOrderByKeys * sizeof(int16));
    i = 0;
    forboth(lco, node->indexorderbyops, lcx, node->indexorderbyorig)
    {
      Oid orderbyop = lfirst_oid(lco);
      Node *orderbyexpr = (Node *)lfirst(lcx);
      Oid orderbyType = exprType(orderbyexpr);
      Oid orderbyColl = exprCollation(orderbyexpr);
      SortSupport orderbysort = &indexstate->iss_SortSupport[i];

                                   
      orderbysort->ssup_cxt = CurrentMemoryContext;
      orderbysort->ssup_collation = orderbyColl;
                                                        
      orderbysort->ssup_nulls_first = false;
                                                   
      orderbysort->ssup_attno = 0;
                           
      orderbysort->abbreviate = false;
      PrepareSortSupportFromOrderingOp(orderbyop, orderbysort);

      get_typlenbyval(orderbyType, &indexstate->iss_OrderByTypLens[i], &indexstate->iss_OrderByTypByVals[i]);
      i++;
    }

                                                             
    indexstate->iss_OrderByValues = (Datum *)palloc(numOrderByKeys * sizeof(Datum));
    indexstate->iss_OrderByNulls = (bool *)palloc(numOrderByKeys * sizeof(bool));

                                          
    indexstate->iss_ReorderQueue = pairingheap_allocate(reorderqueue_cmp, indexstate);
  }

     
                                                                           
                                                                            
                                                                            
                  
     
  if (indexstate->iss_NumRuntimeKeys != 0)
  {
    ExprContext *stdecontext = indexstate->ss.ps.ps_ExprContext;

    ExecAssignExprContext(estate, &indexstate->ss.ps);
    indexstate->iss_RuntimeContext = indexstate->ss.ps.ps_ExprContext;
    indexstate->ss.ps.ps_ExprContext = stdecontext;
  }
  else
  {
    indexstate->iss_RuntimeContext = NULL;
  }

     
               
     
  return indexstate;
}

   
                          
                                                                       
   
                                                                              
                                                                          
                                                                            
                                                                         
   
                                                                               
                                                                       
   
                                                                          
                                                                       
                                                                       
                                                    
   
                                                                         
                                                                          
                                                                      
                                                               
   
                                                                              
                                                                         
                                                                              
                                                                              
                                                                           
                                                                             
                                                                             
   
                                                                      
                     
   
                                                                             
                                                                              
                                                                         
                          
   
                     
   
                                                     
                                                  
                                                    
                                                                           
                                                                           
                                                        
   
                      
   
                                                
                                             
                                                                                
                                                    
                                                                            
                                                
   
                                                                        
                                         
   
void
ExecIndexBuildScanKeys(PlanState *planstate, Relation index, List *quals, bool isorderby, ScanKey *scanKeys, int *numScanKeys, IndexRuntimeKeyInfo **runtimeKeys, int *numRuntimeKeys, IndexArrayKeyInfo **arrayKeys, int *numArrayKeys)
{
  ListCell *qual_cell;
  ScanKey scan_keys;
  IndexRuntimeKeyInfo *runtime_keys;
  IndexArrayKeyInfo *array_keys;
  int n_scan_keys;
  int n_runtime_keys;
  int max_runtime_keys;
  int n_array_keys;
  int j;

                                                        
  n_scan_keys = list_length(quals);
  scan_keys = (ScanKey)palloc(n_scan_keys * sizeof(ScanKeyData));

     
                                                                             
                                                                   
                                                                             
                                                                        
           
     
  runtime_keys = *runtimeKeys;
  n_runtime_keys = max_runtime_keys = *numRuntimeKeys;

                                                                    
  array_keys = (IndexArrayKeyInfo *)palloc0(n_scan_keys * sizeof(IndexArrayKeyInfo));
  n_array_keys = 0;

     
                                                                             
              
     
  j = 0;
  foreach (qual_cell, quals)
  {
    Expr *clause = (Expr *)lfirst(qual_cell);
    ScanKey this_scan_key = &scan_keys[j++];
    Oid opno;                                  
    RegProcedure opfuncid;                                    
    Oid opfamily;                                        
    int op_strategy;                                       
    Oid op_lefttype;                                            
    Oid op_righttype;
    Expr *leftop;                                     
    Expr *rightop;                            
    AttrNumber varattno;                              
    int indnkeyatts;

    indnkeyatts = IndexRelationGetNumberOfKeyAttributes(index);
    if (IsA(clause, OpExpr))
    {
                                                       
      int flags = 0;
      Datum scanvalue;

      opno = ((OpExpr *)clause)->opno;
      opfuncid = ((OpExpr *)clause)->opfuncid;

         
                                                                
         
      leftop = (Expr *)get_leftop(clause);

      if (leftop && IsA(leftop, RelabelType))
      {
        leftop = ((RelabelType *)leftop)->arg;
      }

      Assert(leftop != NULL);

      if (!(IsA(leftop, Var) && ((Var *)leftop)->varno == INDEX_VAR))
      {
        elog(ERROR, "indexqual doesn't have key on left side");
      }

      varattno = ((Var *)leftop)->varattno;
      if (varattno < 1 || varattno > indnkeyatts)
      {
        elog(ERROR, "bogus index qualification");
      }

         
                                                                  
                                                                        
         
      opfamily = index->rd_opfamily[varattno - 1];

      get_op_opfamily_properties(opno, opfamily, isorderby, &op_strategy, &op_lefttype, &op_righttype);

      if (isorderby)
      {
        flags |= SK_ORDER_BY;
      }

         
                                                              
         
      rightop = (Expr *)get_rightop(clause);

      if (rightop && IsA(rightop, RelabelType))
      {
        rightop = ((RelabelType *)rightop)->arg;
      }

      Assert(rightop != NULL);

      if (IsA(rightop, Const))
      {
                                                  
        scanvalue = ((Const *)rightop)->constvalue;
        if (((Const *)rightop)->constisnull)
        {
          flags |= SK_ISNULL;
        }
      }
      else
      {
                                                     
        if (n_runtime_keys >= max_runtime_keys)
        {
          if (max_runtime_keys == 0)
          {
            max_runtime_keys = 8;
            runtime_keys = (IndexRuntimeKeyInfo *)palloc(max_runtime_keys * sizeof(IndexRuntimeKeyInfo));
          }
          else
          {
            max_runtime_keys *= 2;
            runtime_keys = (IndexRuntimeKeyInfo *)repalloc(runtime_keys, max_runtime_keys * sizeof(IndexRuntimeKeyInfo));
          }
        }
        runtime_keys[n_runtime_keys].scan_key = this_scan_key;
        runtime_keys[n_runtime_keys].key_expr = ExecInitExpr(rightop, planstate);
        runtime_keys[n_runtime_keys].key_toastable = TypeIsToastable(op_righttype);
        n_runtime_keys++;
        scanvalue = (Datum)0;
      }

         
                                                        
         
      ScanKeyEntryInitialize(this_scan_key, flags, varattno,                               
          op_strategy,                                                          
          op_righttype,                                                            
          ((OpExpr *)clause)->inputcollid,                                  
          opfuncid,                                                               
          scanvalue);                                                      
    }
    else if (IsA(clause, RowCompareExpr))
    {
                                                                      
      RowCompareExpr *rc = (RowCompareExpr *)clause;
      ScanKey first_sub_key;
      int n_sub_key;
      ListCell *largs_cell;
      ListCell *rargs_cell;
      ListCell *opnos_cell;
      ListCell *collids_cell;

      Assert(!isorderby);

      first_sub_key = (ScanKey)palloc(list_length(rc->opnos) * sizeof(ScanKeyData));
      n_sub_key = 0;

                                                                         
      forfour(largs_cell, rc->largs, rargs_cell, rc->rargs, opnos_cell, rc->opnos, collids_cell, rc->inputcollids)
      {
        ScanKey this_sub_key = &first_sub_key[n_sub_key];
        int flags = SK_ROW_MEMBER;
        Datum scanvalue;
        Oid inputcollation;

        leftop = (Expr *)lfirst(largs_cell);
        rightop = (Expr *)lfirst(rargs_cell);
        opno = lfirst_oid(opnos_cell);
        inputcollation = lfirst_oid(collids_cell);

           
                                                                  
           
        if (leftop && IsA(leftop, RelabelType))
        {
          leftop = ((RelabelType *)leftop)->arg;
        }

        Assert(leftop != NULL);

        if (!(IsA(leftop, Var) && ((Var *)leftop)->varno == INDEX_VAR))
        {
          elog(ERROR, "indexqual doesn't have key on left side");
        }

        varattno = ((Var *)leftop)->varattno;

           
                                                                      
                    
           
        if (index->rd_rel->relam != BTREE_AM_OID || varattno < 1 || varattno > indnkeyatts)
        {
          elog(ERROR, "bogus RowCompare index qualification");
        }
        opfamily = index->rd_opfamily[varattno - 1];

        get_op_opfamily_properties(opno, opfamily, isorderby, &op_strategy, &op_lefttype, &op_righttype);

        if (op_strategy != rc->rctype)
        {
          elog(ERROR, "RowCompare index qualification contains wrong operator");
        }

        opfuncid = get_opfamily_proc(opfamily, op_lefttype, op_righttype, BTORDER_PROC);
        if (!RegProcedureIsValid(opfuncid))
        {
          elog(ERROR, "missing support function %d(%u,%u) in opfamily %u", BTORDER_PROC, op_lefttype, op_righttype, opfamily);
        }

           
                                                                
           
        if (rightop && IsA(rightop, RelabelType))
        {
          rightop = ((RelabelType *)rightop)->arg;
        }

        Assert(rightop != NULL);

        if (IsA(rightop, Const))
        {
                                                    
          scanvalue = ((Const *)rightop)->constvalue;
          if (((Const *)rightop)->constisnull)
          {
            flags |= SK_ISNULL;
          }
        }
        else
        {
                                                       
          if (n_runtime_keys >= max_runtime_keys)
          {
            if (max_runtime_keys == 0)
            {
              max_runtime_keys = 8;
              runtime_keys = (IndexRuntimeKeyInfo *)palloc(max_runtime_keys * sizeof(IndexRuntimeKeyInfo));
            }
            else
            {
              max_runtime_keys *= 2;
              runtime_keys = (IndexRuntimeKeyInfo *)repalloc(runtime_keys, max_runtime_keys * sizeof(IndexRuntimeKeyInfo));
            }
          }
          runtime_keys[n_runtime_keys].scan_key = this_sub_key;
          runtime_keys[n_runtime_keys].key_expr = ExecInitExpr(rightop, planstate);
          runtime_keys[n_runtime_keys].key_toastable = TypeIsToastable(op_righttype);
          n_runtime_keys++;
          scanvalue = (Datum)0;
        }

           
                                                                     
           
        ScanKeyEntryInitialize(this_sub_key, flags, varattno,                       
            op_strategy,                                                         
            op_righttype,                                                           
            inputcollation,                                                  
            opfuncid,                                                              
            scanvalue);                                                     
        n_sub_key++;
      }

                                                      
      first_sub_key[n_sub_key - 1].sk_flags |= SK_ROW_END;

         
                                                                       
                                                         
         
      MemSet(this_scan_key, 0, sizeof(ScanKeyData));
      this_scan_key->sk_flags = SK_ROW_HEADER;
      this_scan_key->sk_attno = first_sub_key->sk_attno;
      this_scan_key->sk_strategy = rc->rctype;
                                                                  
      this_scan_key->sk_argument = PointerGetDatum(first_sub_key);
    }
    else if (IsA(clause, ScalarArrayOpExpr))
    {
                                              
      ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
      int flags = 0;
      Datum scanvalue;

      Assert(!isorderby);

      Assert(saop->useOr);
      opno = saop->opno;
      opfuncid = saop->opfuncid;

         
                                                                
         
      leftop = (Expr *)linitial(saop->args);

      if (leftop && IsA(leftop, RelabelType))
      {
        leftop = ((RelabelType *)leftop)->arg;
      }

      Assert(leftop != NULL);

      if (!(IsA(leftop, Var) && ((Var *)leftop)->varno == INDEX_VAR))
      {
        elog(ERROR, "indexqual doesn't have key on left side");
      }

      varattno = ((Var *)leftop)->varattno;
      if (varattno < 1 || varattno > indnkeyatts)
      {
        elog(ERROR, "bogus index qualification");
      }

         
                                                                  
                                                                        
         
      opfamily = index->rd_opfamily[varattno - 1];

      get_op_opfamily_properties(opno, opfamily, isorderby, &op_strategy, &op_lefttype, &op_righttype);

         
                                                         
         
      rightop = (Expr *)lsecond(saop->args);

      if (rightop && IsA(rightop, RelabelType))
      {
        rightop = ((RelabelType *)rightop)->arg;
      }

      Assert(rightop != NULL);

      if (index->rd_indam->amsearcharray)
      {
                                                              
        flags |= SK_SEARCHARRAY;
        if (IsA(rightop, Const))
        {
                                                    
          scanvalue = ((Const *)rightop)->constvalue;
          if (((Const *)rightop)->constisnull)
          {
            flags |= SK_ISNULL;
          }
        }
        else
        {
                                                       
          if (n_runtime_keys >= max_runtime_keys)
          {
            if (max_runtime_keys == 0)
            {
              max_runtime_keys = 8;
              runtime_keys = (IndexRuntimeKeyInfo *)palloc(max_runtime_keys * sizeof(IndexRuntimeKeyInfo));
            }
            else
            {
              max_runtime_keys *= 2;
              runtime_keys = (IndexRuntimeKeyInfo *)repalloc(runtime_keys, max_runtime_keys * sizeof(IndexRuntimeKeyInfo));
            }
          }
          runtime_keys[n_runtime_keys].scan_key = this_scan_key;
          runtime_keys[n_runtime_keys].key_expr = ExecInitExpr(rightop, planstate);

             
                                                            
                                                              
                                                               
                                                        
             
          runtime_keys[n_runtime_keys].key_toastable = true;
          n_runtime_keys++;
          scanvalue = (Datum)0;
        }
      }
      else
      {
                                                    
        array_keys[n_array_keys].scan_key = this_scan_key;
        array_keys[n_array_keys].array_expr = ExecInitExpr(rightop, planstate);
                                                         
        n_array_keys++;
        scanvalue = (Datum)0;
      }

         
                                                        
         
      ScanKeyEntryInitialize(this_scan_key, flags, varattno,                               
          op_strategy,                                                          
          op_righttype,                                                            
          saop->inputcollid,                                                
          opfuncid,                                                               
          scanvalue);                                                      
    }
    else if (IsA(clause, NullTest))
    {
                                                    
      NullTest *ntest = (NullTest *)clause;
      int flags;

      Assert(!isorderby);

         
                                                                  
         
      leftop = ntest->arg;

      if (leftop && IsA(leftop, RelabelType))
      {
        leftop = ((RelabelType *)leftop)->arg;
      }

      Assert(leftop != NULL);

      if (!(IsA(leftop, Var) && ((Var *)leftop)->varno == INDEX_VAR))
      {
        elog(ERROR, "NullTest indexqual has wrong key");
      }

      varattno = ((Var *)leftop)->varattno;

         
                                                        
         
      switch (ntest->nulltesttype)
      {
      case IS_NULL:
        flags = SK_ISNULL | SK_SEARCHNULL;
        break;
      case IS_NOT_NULL:
        flags = SK_ISNULL | SK_SEARCHNOTNULL;
        break;
      default:
        elog(ERROR, "unrecognized nulltesttype: %d", (int)ntest->nulltesttype);
        flags = 0;                          
        break;
      }

      ScanKeyEntryInitialize(this_scan_key, flags, varattno,                               
          InvalidStrategy,                                                    
          InvalidOid,                                                                 
          InvalidOid,                                                          
          InvalidOid,                                                                  
          (Datum)0);                                                       
    }
    else
    {
      elog(ERROR, "unsupported indexqual type: %d", (int)nodeTag(clause));
    }
  }

  Assert(n_runtime_keys <= max_runtime_keys);

                                    
  if (n_array_keys == 0)
  {
    pfree(array_keys);
    array_keys = NULL;
  }

     
                                
     
  *scanKeys = scan_keys;
  *numScanKeys = n_scan_keys;
  *runtimeKeys = runtime_keys;
  *numRuntimeKeys = n_runtime_keys;
  if (arrayKeys)
  {
    *arrayKeys = array_keys;
    *numArrayKeys = n_array_keys;
  }
  else if (n_array_keys != 0)
  {
    elog(ERROR, "ScalarArrayOpExpr index qual found where not allowed");
  }
}

                                                                    
                              
                                                                    
   

                                                                    
                          
   
                                                           
                                                           
                                                                    
   
void
ExecIndexScanEstimate(IndexScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;

  node->iss_PscanLen = index_parallelscan_estimate(node->iss_RelationDesc, estate->es_snapshot);
  shm_toc_estimate_chunk(&pcxt->estimator, node->iss_PscanLen);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

                                                                    
                               
   
                                             
                                                                    
   
void
ExecIndexScanInitializeDSM(IndexScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;
  ParallelIndexScanDesc piscan;

  piscan = shm_toc_allocate(pcxt->toc, node->iss_PscanLen);
  index_parallelscan_initialize(node->ss.ss_currentRelation, node->iss_RelationDesc, estate->es_snapshot, piscan);
  shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, piscan);
  node->iss_ScanDesc = index_beginscan_parallel(node->ss.ss_currentRelation, node->iss_RelationDesc, node->iss_NumScanKeys, node->iss_NumOrderByKeys, piscan);

     
                                                                           
                                   
     
  if (node->iss_NumRuntimeKeys == 0 || node->iss_RuntimeKeysReady)
  {
    index_rescan(node->iss_ScanDesc, node->iss_ScanKeys, node->iss_NumScanKeys, node->iss_OrderByKeys, node->iss_NumOrderByKeys);
  }
}

                                                                    
                                 
   
                                                      
                                                                    
   
void
ExecIndexScanReInitializeDSM(IndexScanState *node, ParallelContext *pcxt)
{
  index_parallelrescan(node->iss_ScanDesc);
}

                                                                    
                                  
   
                                                       
                                                                    
   
void
ExecIndexScanInitializeWorker(IndexScanState *node, ParallelWorkerContext *pwcxt)
{
  ParallelIndexScanDesc piscan;

  piscan = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, false);
  node->iss_ScanDesc = index_beginscan_parallel(node->ss.ss_currentRelation, node->iss_RelationDesc, node->iss_NumScanKeys, node->iss_NumOrderByKeys, piscan);

     
                                                                           
                                   
     
  if (node->iss_NumRuntimeKeys == 0 || node->iss_RuntimeKeysReady)
  {
    index_rescan(node->iss_ScanDesc, node->iss_ScanKeys, node->iss_NumScanKeys, node->iss_OrderByKeys, node->iss_NumOrderByKeys);
  }
}
