                                                                            
   
                      
                                                                             
   
                                                                         
                                                                        
   
   
                  
                                             
   
                                                                            
   
   
                      
                                        
                                                               
                                                                      
                                                         
                                                
   
#include "postgres.h"

#include "catalog/pg_type.h"
#include "executor/nodeFunctionscan.h"
#include "funcapi.h"
#include "nodes/nodeFuncs.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

   
                                                 
   
typedef struct FunctionScanPerFuncState
{
  SetExprState *setexpr;                                                  
  TupleDesc tupdesc;                                               
  int colcount;                                                     
  Tuplestorestate *tstore;                                      
  int64 rowcount;                                                          
  TupleTableSlot *func_slot;                                     
} FunctionScanPerFuncState;

static TupleTableSlot *
FunctionNext(FunctionScanState *node);

                                                                    
                     
                                                                    
   
                                                                    
                 
   
                                             
                                                                    
   
static TupleTableSlot *
FunctionNext(FunctionScanState *node)
{
  EState *estate;
  ScanDirection direction;
  TupleTableSlot *scanslot;
  bool alldone;
  int64 oldpos;
  int funcno;
  int att;

     
                                                    
     
  estate = node->ss.ps.state;
  direction = estate->es_direction;
  scanslot = node->ss.ss_ScanTupleSlot;

  if (node->simple)
  {
       
                                                                         
                                                                          
                                                                  
                         
       
    Tuplestorestate *tstore = node->funcstates[0].tstore;

       
                                                                         
                                                                
                   
       
    if (tstore == NULL)
    {
      node->funcstates[0].tstore = tstore = ExecMakeTableFunctionResult(node->funcstates[0].setexpr, node->ss.ps.ps_ExprContext, node->argcontext, node->funcstates[0].tupdesc, node->eflags & EXEC_FLAG_BACKWARD);

         
                                                                         
                                                                        
                                                              
         
      tuplestore_rescan(tstore);
    }

       
                                           
       
    (void)tuplestore_gettupleslot(tstore, ScanDirectionIsForward(direction), false, scanslot);
    return scanslot;
  }

     
                                                                             
                                                                             
                                                                          
                                                                         
     
  oldpos = node->ordinal;
  if (ScanDirectionIsForward(direction))
  {
    node->ordinal++;
  }
  else
  {
    node->ordinal--;
  }

     
                               
     
                                                                             
                                                                            
                                                                     
     
  ExecClearTuple(scanslot);
  att = 0;
  alldone = true;
  for (funcno = 0; funcno < node->nfuncs; funcno++)
  {
    FunctionScanPerFuncState *fs = &node->funcstates[funcno];
    int i;

       
                                                                         
                                                                
                   
       
    if (fs->tstore == NULL)
    {
      fs->tstore = ExecMakeTableFunctionResult(fs->setexpr, node->ss.ps.ps_ExprContext, node->argcontext, fs->tupdesc, node->eflags & EXEC_FLAG_BACKWARD);

         
                                                                         
                                                                        
                                                              
         
      tuplestore_rescan(fs->tstore);
    }

       
                                           
       
                                                                        
                                                                        
                                                                      
       
    if (fs->rowcount != -1 && fs->rowcount < oldpos)
    {
      ExecClearTuple(fs->func_slot);
    }
    else
    {
      (void)tuplestore_gettupleslot(fs->tstore, ScanDirectionIsForward(direction), false, fs->func_slot);
    }

    if (TupIsNull(fs->func_slot))
    {
         
                                                                
                                                                       
                                                                        
                                                                       
                                                          
         
      if (ScanDirectionIsForward(direction) && fs->rowcount == -1)
      {
        fs->rowcount = node->ordinal;
      }

         
                                             
         
      for (i = 0; i < fs->colcount; i++)
      {
        scanslot->tts_values[att] = (Datum)0;
        scanslot->tts_isnull[att] = true;
        att++;
      }
    }
    else
    {
         
                                                               
         
      slot_getallattrs(fs->func_slot);

      for (i = 0; i < fs->colcount; i++)
      {
        scanslot->tts_values[att] = fs->func_slot->tts_values[i];
        scanslot->tts_isnull[att] = fs->func_slot->tts_isnull[i];
        att++;
      }

         
                                                                         
                                                    
         
      alldone = false;
    }
  }

     
                                           
     
  if (node->ordinality)
  {
    scanslot->tts_values[att] = Int64GetDatumFast(node->ordinal);
    scanslot->tts_isnull[att] = false;
  }

     
                                                                             
                                        
     
  if (!alldone)
  {
    ExecStoreVirtualTuple(scanslot);
  }

  return scanslot;
}

   
                                                                               
   
static bool
FunctionRecheck(FunctionScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                           
   
                                                                    
           
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecFunctionScan(PlanState *pstate)
{
  FunctionScanState *node = castNode(FunctionScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)FunctionNext, (ExecScanRecheckMtd)FunctionRecheck);
}

                                                                    
                         
                                                                    
   
FunctionScanState *
ExecInitFunctionScan(FunctionScan *node, EState *estate, int eflags)
{
  FunctionScanState *scanstate;
  int nfuncs = list_length(node->functions);
  TupleDesc scan_tupdesc;
  int i, natts;
  ListCell *lc;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

     
                                                
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                                   
     
  scanstate = makeNode(FunctionScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecFunctionScan;
  scanstate->eflags = eflags;

     
                                         
     
  scanstate->ordinality = node->funcordinality;

  scanstate->nfuncs = nfuncs;
  if (nfuncs == 1 && !node->funcordinality)
  {
    scanstate->simple = true;
  }
  else
  {
    scanstate->simple = false;
  }

     
                                                               
     
                                                                          
                                                                          
                                                                             
                                                                           
                                                                    
                                                
     
  scanstate->ordinal = 0;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

  scanstate->funcstates = palloc(nfuncs * sizeof(FunctionScanPerFuncState));

  natts = 0;
  i = 0;
  foreach (lc, node->functions)
  {
    RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);
    Node *funcexpr = rtfunc->funcexpr;
    int colcount = rtfunc->funccolcount;
    FunctionScanPerFuncState *fs = &scanstate->funcstates[i];
    TypeFuncClass functypclass;
    Oid funcrettype;
    TupleDesc tupdesc;

    fs->setexpr = ExecInitTableFunctionResult((Expr *)funcexpr, scanstate->ss.ps.ps_ExprContext, &scanstate->ss.ps);

       
                                                                         
                                                                         
                                              
       
    fs->tstore = NULL;
    fs->rowcount = -1;

       
                                                                         
                                                                           
                                                                          
                                                                  
       
    functypclass = get_expr_result_type(funcexpr, &funcrettype, &tupdesc);

    if (functypclass == TYPEFUNC_COMPOSITE || functypclass == TYPEFUNC_COMPOSITE_DOMAIN)
    {
                                                        
      Assert(tupdesc);
      Assert(tupdesc->natts >= colcount);
                                                   
      tupdesc = CreateTupleDescCopy(tupdesc);
    }
    else if (functypclass == TYPEFUNC_SCALAR)
    {
                                       
      tupdesc = CreateTemplateTupleDesc(1);
      TupleDescInitEntry(tupdesc, (AttrNumber)1, NULL,                                     
          funcrettype, -1, 0);
      TupleDescInitEntryCollation(tupdesc, (AttrNumber)1, exprCollation(funcexpr));
    }
    else if (functypclass == TYPEFUNC_RECORD)
    {
      tupdesc = BuildDescFromLists(rtfunc->funccolnames, rtfunc->funccoltypes, rtfunc->funccoltypmods, rtfunc->funccolcollations);

         
                                                                         
                                                                       
                           
         
      BlessTupleDesc(tupdesc);
    }
    else
    {
                                                                    
      elog(ERROR, "function in FROM has unsupported return type");
    }

    fs->tupdesc = tupdesc;
    fs->colcount = colcount;

       
                                                                      
                                                                      
                                                     
       
    if (!scanstate->simple)
    {
      fs->func_slot = ExecInitExtraTupleSlot(estate, fs->tupdesc, &TTSOpsMinimalTuple);
    }
    else
    {
      fs->func_slot = NULL;
    }

    natts += colcount;
    i++;
  }

     
                                   
     
                                                                       
                                                                           
                                                                   
     
  if (scanstate->simple)
  {
    scan_tupdesc = CreateTupleDescCopy(scanstate->funcstates[0].tupdesc);
    scan_tupdesc->tdtypeid = RECORDOID;
    scan_tupdesc->tdtypmod = -1;
  }
  else
  {
    AttrNumber attno = 0;

    if (node->funcordinality)
    {
      natts++;
    }

    scan_tupdesc = CreateTemplateTupleDesc(natts);

    for (i = 0; i < nfuncs; i++)
    {
      TupleDesc tupdesc = scanstate->funcstates[i].tupdesc;
      int colcount = scanstate->funcstates[i].colcount;
      int j;

      for (j = 1; j <= colcount; j++)
      {
        TupleDescCopyEntry(scan_tupdesc, ++attno, tupdesc, j);
      }
    }

                                                                       
    if (node->funcordinality)
    {
      TupleDescInitEntry(scan_tupdesc, ++attno, NULL,                                     
          INT8OID, -1, 0);
    }

    Assert(attno == natts);
  }

     
                                    
     
  ExecInitScanTupleSlot(estate, &scanstate->ss, scan_tupdesc, &TTSOpsMinimalTuple);

     
                                                  
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);

     
                                                                         
                                                                             
                                                                     
                                                                         
                                                                     
     
  scanstate->argcontext = AllocSetContextCreate(CurrentMemoryContext, "Table function arguments", ALLOCSET_DEFAULT_SIZES);

  return scanstate;
}

                                                                    
                        
   
                                                    
                                                                    
   
void
ExecEndFunctionScan(FunctionScanState *node)
{
  int i;

     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                            
     
  for (i = 0; i < node->nfuncs; i++)
  {
    FunctionScanPerFuncState *fs = &node->funcstates[i];

    if (fs->func_slot)
    {
      ExecClearTuple(fs->func_slot);
    }

    if (fs->tstore != NULL)
    {
      tuplestore_end(node->funcstates[i].tstore);
      fs->tstore = NULL;
    }
  }
}

                                                                    
                           
   
                          
                                                                    
   
void
ExecReScanFunctionScan(FunctionScanState *node)
{
  FunctionScan *scan = (FunctionScan *)node->ss.ps.plan;
  int i;
  Bitmapset *chgparam = node->ss.ps.chgParam;

  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  for (i = 0; i < node->nfuncs; i++)
  {
    FunctionScanPerFuncState *fs = &node->funcstates[i];

    if (fs->func_slot)
    {
      ExecClearTuple(fs->func_slot);
    }
  }

  ExecScanReScan(&node->ss);

     
                                                                          
                                                                         
                                                             
     
                                                                        
                                                                      
     
  if (chgparam)
  {
    ListCell *lc;

    i = 0;
    foreach (lc, scan->functions)
    {
      RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);

      if (bms_overlap(chgparam, rtfunc->funcparams))
      {
        if (node->funcstates[i].tstore != NULL)
        {
          tuplestore_end(node->funcstates[i].tstore);
          node->funcstates[i].tstore = NULL;
        }
        node->funcstates[i].rowcount = -1;
      }
      i++;
    }
  }

                                
  node->ordinal = 0;

                                                     
  for (i = 0; i < node->nfuncs; i++)
  {
    if (node->funcstates[i].tstore != NULL)
    {
      tuplestore_rescan(node->funcstates[i].tstore);
    }
  }
}
