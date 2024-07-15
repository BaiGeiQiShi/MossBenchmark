                                                                            
   
                             
                                                   
   
                                                                         
                                                                        
   
   
                  
                                                    
   
                                                                            
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeNamedtuplestorescan.h"
#include "miscadmin.h"
#include "utils/queryenvironment.h"

static TupleTableSlot *
NamedTuplestoreScanNext(NamedTuplestoreScanState *node);

                                                                    
                            
   
                                                    
                                                                    
   
static TupleTableSlot *
NamedTuplestoreScanNext(NamedTuplestoreScanState *node)
{
  TupleTableSlot *slot;

                                                      
  Assert(ScanDirectionIsForward(node->ss.ps.state->es_direction));

     
                                                                        
     
  slot = node->ss.ss_ScanTupleSlot;
  tuplestore_select_read_pointer(node->relation, node->readptr);
  (void)tuplestore_gettupleslot(node->relation, true, false, slot);
  return slot;
}

   
                                                                             
                
   
static bool
NamedTuplestoreScanRecheck(NamedTuplestoreScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                                  
   
                                                                      
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecNamedTuplestoreScan(PlanState *pstate)
{
  NamedTuplestoreScanState *node = castNode(NamedTuplestoreScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)NamedTuplestoreScanNext, (ExecScanRecheckMtd)NamedTuplestoreScanRecheck);
}

                                                                    
                                
                                                                    
   
NamedTuplestoreScanState *
ExecInitNamedTuplestoreScan(NamedTuplestoreScan *node, EState *estate, int eflags)
{
  NamedTuplestoreScanState *scanstate;
  EphemeralNamedRelation enr;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                                                       
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                                                  
     
  scanstate = makeNode(NamedTuplestoreScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecNamedTuplestoreScan;

  enr = get_ENR(estate->es_queryEnv, node->enrname);
  if (!enr)
  {
    elog(ERROR, "executor could not find named tuplestore \"%s\"", node->enrname);
  }

  Assert(enr->reldata);
  scanstate->relation = (Tuplestorestate *)enr->reldata;
  scanstate->tupdesc = ENRMetadataGetTupDesc(&(enr->md));
  scanstate->readptr = tuplestore_alloc_read_pointer(scanstate->relation, EXEC_FLAG_REWIND);

     
                                                                         
                                                 
     
  tuplestore_select_read_pointer(scanstate->relation, scanstate->readptr);
  tuplestore_rescan(scanstate->relation);

     
                                                                        
     
                                                                            
                          
     

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                                                          
     
  ExecInitScanTupleSlot(estate, &scanstate->ss, scanstate->tupdesc, &TTSOpsMinimalTuple);

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, (PlanState *)scanstate);

  return scanstate;
}

                                                                    
                               
   
                                                    
                                                                    
   
void
ExecEndNamedTuplestoreScan(NamedTuplestoreScanState *node)
{
     
                      
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

                                                                    
                                  
   
                          
                                                                    
   
void
ExecReScanNamedTuplestoreScan(NamedTuplestoreScanState *node)
{
  Tuplestorestate *tuplestorestate = node->relation;

  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }

  ExecScanReScan(&node->ss);

     
                            
     
  tuplestore_select_read_pointer(tuplestorestate, node->readptr);
  tuplestore_rescan(tuplestorestate);
}
