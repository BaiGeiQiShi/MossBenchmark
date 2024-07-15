                                                                            
   
                    
                                                                      
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/relscan.h"
#include "access/tableam.h"
#include "access/tsmapi.h"
#include "executor/executor.h"
#include "executor/nodeSamplescan.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "utils/builtins.h"
#include "utils/rel.h"

static TupleTableSlot *
SampleNext(SampleScanState *node);
static void
tablesample_init(SampleScanState *scanstate);
static TupleTableSlot *
tablesample_getnext(SampleScanState *scanstate);

                                                                    
                     
                                                                    
   

                                                                    
               
   
                                           
                                                                    
   
static TupleTableSlot *
SampleNext(SampleScanState *node)
{
     
                                                     
     
  if (!node->begun)
  {
    tablesample_init(node);
  }

     
                                                         
     
  return tablesample_getnext(node);
}

   
                                                                             
   
static bool
SampleRecheck(SampleScanState *node, TupleTableSlot *slot)
{
     
                                                                             
                                       
     
  return true;
}

                                                                    
                         
   
                                                             
                               
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecSampleScan(PlanState *pstate)
{
  SampleScanState *node = castNode(SampleScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)SampleNext, (ExecScanRecheckMtd)SampleRecheck);
}

                                                                    
                       
                                                                    
   
SampleScanState *
ExecInitSampleScan(SampleScan *node, EState *estate, int eflags)
{
  SampleScanState *scanstate;
  TableSampleClause *tsc = node->tablesample;
  TsmRoutine *tsm;

  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                            
     
  scanstate = makeNode(SampleScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecSampleScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                            
     
  scanstate->ss.ss_currentRelation = ExecOpenScanRelation(estate, node->scan.scanrelid, eflags);

                                                   
  scanstate->ss.ss_currentScanDesc = NULL;

                                                
  ExecInitScanTupleSlot(estate, &scanstate->ss, RelationGetDescr(scanstate->ss.ss_currentRelation), table_slot_callbacks(scanstate->ss.ss_currentRelation));

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);

  scanstate->args = ExecInitExprList(tsc->args, (PlanState *)scanstate);
  scanstate->repeatable = ExecInitExpr(tsc->repeatable, (PlanState *)scanstate);

     
                                                                             
                                                                      
     
  if (tsc->repeatable == NULL)
  {
    scanstate->seed = random();
  }

     
                                                         
     
  tsm = GetTsmRoutine(tsc->tsmhandler);
  scanstate->tsmroutine = tsm;
  scanstate->tsm_state = NULL;

  if (tsm->InitSampleScan)
  {
    tsm->InitSampleScan(scanstate, eflags);
  }

                                                                    
  scanstate->begun = false;

  return scanstate;
}

                                                                    
                      
   
                                                    
                                                                    
   
void
ExecEndSampleScan(SampleScanState *node)
{
     
                                                       
     
  if (node->tsmroutine->EndSampleScan)
  {
    node->tsmroutine->EndSampleScan(node);
  }

     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                     
     
  if (node->ss.ss_currentScanDesc)
  {
    table_endscan(node->ss.ss_currentScanDesc);
  }
}

                                                                    
                         
   
                          
   
                                                                    
   
void
ExecReScanSampleScan(SampleScanState *node)
{
                                                                          
  node->begun = false;
  node->done = false;
  node->haveblock = false;
  node->donetuples = 0;

  ExecScanReScan(&node->ss);
}

   
                                                                                
   
static void
tablesample_init(SampleScanState *scanstate)
{
  TsmRoutine *tsm = scanstate->tsmroutine;
  ExprContext *econtext = scanstate->ss.ps.ps_ExprContext;
  Datum *params;
  Datum datum;
  bool isnull;
  uint32 seed;
  bool allow_sync;
  int i;
  ListCell *arg;

  scanstate->donetuples = 0;
  params = (Datum *)palloc(list_length(scanstate->args) * sizeof(Datum));

  i = 0;
  foreach (arg, scanstate->args)
  {
    ExprState *argstate = (ExprState *)lfirst(arg);

    params[i] = ExecEvalExprSwitchContext(argstate, econtext, &isnull);
    if (isnull)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLESAMPLE_ARGUMENT), errmsg("TABLESAMPLE parameter cannot be null")));
    }
    i++;
  }

  if (scanstate->repeatable)
  {
    datum = ExecEvalExprSwitchContext(scanstate->repeatable, econtext, &isnull);
    if (isnull)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLESAMPLE_REPEAT), errmsg("TABLESAMPLE REPEATABLE parameter cannot be null")));
    }

       
                                                                          
                                                                    
                                                                          
                                                                       
                                                                         
                            
       
                                                                         
                                                                       
                                                                       
       
    seed = DatumGetUInt32(DirectFunctionCall1(hashfloat8, datum));
  }
  else
  {
                                                     
    seed = scanstate->seed;
  }

                                                                     
  scanstate->use_bulkread = true;
  scanstate->use_pagemode = true;

                                           
  tsm->BeginSampleScan(scanstate, params, list_length(scanstate->args), seed);

                                                                 
  allow_sync = (tsm->NextSampleBlock == NULL);

                                                   
  if (scanstate->ss.ss_currentScanDesc == NULL)
  {
    scanstate->ss.ss_currentScanDesc = table_beginscan_sampling(scanstate->ss.ss_currentRelation, scanstate->ss.ps.state->es_snapshot, 0, NULL, scanstate->use_bulkread, allow_sync, scanstate->use_pagemode);
  }
  else
  {
    table_rescan_set_params(scanstate->ss.ss_currentScanDesc, NULL, scanstate->use_bulkread, allow_sync, scanstate->use_pagemode);
  }

  pfree(params);

                              
  scanstate->begun = true;
}

   
                                           
   
static TupleTableSlot *
tablesample_getnext(SampleScanState *scanstate)
{
  TableScanDesc scan = scanstate->ss.ss_currentScanDesc;
  TupleTableSlot *slot = scanstate->ss.ss_ScanTupleSlot;

  ExecClearTuple(slot);

  if (scanstate->done)
  {
    return NULL;
  }

  for (;;)
  {
    if (!scanstate->haveblock)
    {
      if (!table_scan_sample_next_block(scan, scanstate))
      {
        scanstate->haveblock = false;
        scanstate->done = true;

                                
        return NULL;
      }

      scanstate->haveblock = true;
    }

    if (!table_scan_sample_next_tuple(scan, scanstate, slot))
    {
         
                                                                         
                                            
         
      scanstate->haveblock = false;
      continue;
    }

                                         
    break;
  }

  scanstate->donetuples++;

  return slot;
}
