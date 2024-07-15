                                                                            
   
                 
                                                         
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
   
                      
                                                  
                                                            
                                                              
                                                     
                                            
   
                                                                      
                                                              
                                                                        
                                                                      
   
#include "postgres.h"

#include "access/relscan.h"
#include "access/tableam.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "utils/rel.h"

static TupleTableSlot *
SeqNext(SeqScanState *node);

                                                                    
                     
                                                                    
   

                                                                    
            
   
                                        
                                                                    
   
static TupleTableSlot *
SeqNext(SeqScanState *node)
{
  TableScanDesc scandesc;
  EState *estate;
  ScanDirection direction;
  TupleTableSlot *slot;

     
                                                    
     
  scandesc = node->ss.ss_currentScanDesc;
  estate = node->ss.ps.state;
  direction = estate->es_direction;
  slot = node->ss.ss_ScanTupleSlot;

  if (scandesc == NULL)
  {
       
                                                                       
                                                         
       
    scandesc = table_beginscan(node->ss.ss_currentRelation, estate->es_snapshot, 0, NULL);
    node->ss.ss_currentScanDesc = scandesc;
  }

     
                                       
     
  if (table_scan_getnextslot(scandesc, direction, slot))
  {
    return slot;
  }
  return NULL;
}

   
                                                                          
   
static bool
SeqRecheck(SeqScanState *node, TupleTableSlot *slot)
{
     
                                                                          
                                                                           
     
  return true;
}

                                                                    
                      
   
                                                                    
           
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecSeqScan(PlanState *pstate)
{
  SeqScanState *node = castNode(SeqScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)SeqNext, (ExecScanRecheckMtd)SeqRecheck);
}

                                                                    
                    
                                                                    
   
SeqScanState *
ExecInitSeqScan(SeqScan *node, EState *estate, int eflags)
{
  SeqScanState *scanstate;

     
                                                                             
                   
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                            
     
  scanstate = makeNode(SeqScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecSeqScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                            
     
  scanstate->ss.ss_currentRelation = ExecOpenScanRelation(estate, node->scanrelid, eflags);

                                                    
  ExecInitScanTupleSlot(estate, &scanstate->ss, RelationGetDescr(scanstate->ss.ss_currentRelation), table_slot_callbacks(scanstate->ss.ss_currentRelation));

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->plan.qual, (PlanState *)scanstate);

  return scanstate;
}

                                                                    
                   
   
                                                    
                                                                    
   
void
ExecEndSeqScan(SeqScanState *node)
{
  TableScanDesc scanDesc;

     
                               
     
  scanDesc = node->ss.ss_currentScanDesc;

     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                     
     
  if (scanDesc != NULL)
  {
    table_endscan(scanDesc);
  }
}

                                                                    
                     
                                                                    
   

                                                                    
                      
   
                          
                                                                    
   
void
ExecReScanSeqScan(SeqScanState *node)
{
  TableScanDesc scan;

  scan = node->ss.ss_currentScanDesc;

  if (scan != NULL)
  {
    table_rescan(scan,                
        NULL);                            
  }

  ExecScanReScan((ScanState *)node);
}

                                                                    
                              
                                                                    
   

                                                                    
                        
   
                                                           
                                                           
                                                                    
   
void
ExecSeqScanEstimate(SeqScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;

  node->pscan_len = table_parallelscan_estimate(node->ss.ss_currentRelation, estate->es_snapshot);
  shm_toc_estimate_chunk(&pcxt->estimator, node->pscan_len);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

                                                                    
                             
   
                                            
                                                                    
   
void
ExecSeqScanInitializeDSM(SeqScanState *node, ParallelContext *pcxt)
{
  EState *estate = node->ss.ps.state;
  ParallelTableScanDesc pscan;

  pscan = shm_toc_allocate(pcxt->toc, node->pscan_len);
  table_parallelscan_initialize(node->ss.ss_currentRelation, pscan, estate->es_snapshot);
  shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id, pscan);
  node->ss.ss_currentScanDesc = table_beginscan_parallel(node->ss.ss_currentRelation, pscan);
}

                                                                    
                               
   
                                                      
                                                                    
   
void
ExecSeqScanReInitializeDSM(SeqScanState *node, ParallelContext *pcxt)
{
  ParallelTableScanDesc pscan;

  pscan = node->ss.ss_currentScanDesc->rs_parallel;
  table_parallelscan_reinitialize(node->ss.ss_currentRelation, pscan);
}

                                                                    
                                
   
                                                       
                                                                    
   
void
ExecSeqScanInitializeWorker(SeqScanState *node, ParallelWorkerContext *pwcxt)
{
  ParallelTableScanDesc pscan;

  pscan = shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, false);
  node->ss.ss_currentScanDesc = table_beginscan_parallel(node->ss.ss_currentRelation, pscan);
}
