                                                                            
   
                
                                                     
   
                                                                         
                                                                        
   
                                                                            
   
#include "postgres.h"

#include "access/parallel.h"
#include "executor/executor.h"
#include "executor/nodeCustom.h"
#include "nodes/execnodes.h"
#include "nodes/extensible.h"
#include "nodes/plannodes.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
ExecCustomScan(PlanState *pstate);

CustomScanState *
ExecInitCustomScan(CustomScan *cscan, EState *estate, int eflags)
{
  CustomScanState *css;
  Relation scan_rel = NULL;
  Index scanrelid = cscan->scan.scanrelid;
  Index tlistvarno;

     
                                                                           
                                                                         
                                                                           
                                                                            
                  
     
  css = castNode(CustomScanState, cscan->methods->CreateCustomScanState(cscan));

                                        
  css->flags = cscan->flags;

                                   
  css->ss.ps.plan = &cscan->scan.plan;
  css->ss.ps.state = estate;
  css->ss.ps.ExecProcNode = ExecCustomScan;

                                          
  ExecAssignExprContext(estate, &css->ss.ps);

     
                                    
     
  if (scanrelid > 0)
  {
    scan_rel = ExecOpenScanRelation(estate, scanrelid, eflags);
    css->ss.ss_currentRelation = scan_rel;
  }

     
                                                                            
                                                                    
                         
     
  if (cscan->custom_scan_tlist != NIL || scan_rel == NULL)
  {
    TupleDesc scan_tupdesc;

    scan_tupdesc = ExecTypeFromTL(cscan->custom_scan_tlist);
    ExecInitScanTupleSlot(estate, &css->ss, scan_tupdesc, &TTSOpsVirtual);
                                                                    
    tlistvarno = INDEX_VAR;
  }
  else
  {
    ExecInitScanTupleSlot(estate, &css->ss, RelationGetDescr(scan_rel), &TTSOpsVirtual);
                                                                    
    tlistvarno = scanrelid;
  }

     
                                                  
     
  ExecInitResultTupleSlotTL(&css->ss.ps, &TTSOpsVirtual);
  ExecAssignScanProjectionInfoWithVarno(&css->ss, tlistvarno);

                                    
  css->ss.ps.qual = ExecInitQual(cscan->scan.plan.qual, (PlanState *)css);

     
                                                                           
                                                           
     
  css->methods->BeginCustomScan(css, estate, eflags);

  return css;
}

static TupleTableSlot *
ExecCustomScan(PlanState *pstate)
{
  CustomScanState *node = castNode(CustomScanState, pstate);

  CHECK_FOR_INTERRUPTS();

  Assert(node->methods->ExecCustomScan != NULL);
  return node->methods->ExecCustomScan(node);
}

void
ExecEndCustomScan(CustomScanState *node)
{
  Assert(node->methods->EndCustomScan != NULL);
  node->methods->EndCustomScan(node);

                            
  ExecFreeExprContext(&node->ss.ps);

                                 
  ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

void
ExecReScanCustomScan(CustomScanState *node)
{
  Assert(node->methods->ReScanCustomScan != NULL);
  node->methods->ReScanCustomScan(node);
}

void
ExecCustomMarkPos(CustomScanState *node)
{
  if (!node->methods->MarkPosCustomScan)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("custom scan \"%s\" does not support MarkPos", node->methods->CustomName)));
  }
  node->methods->MarkPosCustomScan(node);
}

void
ExecCustomRestrPos(CustomScanState *node)
{
  if (!node->methods->RestrPosCustomScan)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("custom scan \"%s\" does not support MarkPos", node->methods->CustomName)));
  }
  node->methods->RestrPosCustomScan(node);
}

void
ExecCustomScanEstimate(CustomScanState *node, ParallelContext *pcxt)
{
  const CustomExecMethods *methods = node->methods;

  if (methods->EstimateDSMCustomScan)
  {
    node->pscan_len = methods->EstimateDSMCustomScan(node, pcxt);
    shm_toc_estimate_chunk(&pcxt->estimator, node->pscan_len);
    shm_toc_estimate_keys(&pcxt->estimator, 1);
  }
}

void
ExecCustomScanInitializeDSM(CustomScanState *node, ParallelContext *pcxt)
{
  const CustomExecMethods *methods = node->methods;

  if (methods->InitializeDSMCustomScan)
  {
    int plan_node_id = node->ss.ps.plan->plan_node_id;
    void *coordinate;

    coordinate = shm_toc_allocate(pcxt->toc, node->pscan_len);
    methods->InitializeDSMCustomScan(node, pcxt, coordinate);
    shm_toc_insert(pcxt->toc, plan_node_id, coordinate);
  }
}

void
ExecCustomScanReInitializeDSM(CustomScanState *node, ParallelContext *pcxt)
{
  const CustomExecMethods *methods = node->methods;

  if (methods->ReInitializeDSMCustomScan)
  {
    int plan_node_id = node->ss.ps.plan->plan_node_id;
    void *coordinate;

    coordinate = shm_toc_lookup(pcxt->toc, plan_node_id, false);
    methods->ReInitializeDSMCustomScan(node, pcxt, coordinate);
  }
}

void
ExecCustomScanInitializeWorker(CustomScanState *node, ParallelWorkerContext *pwcxt)
{
  const CustomExecMethods *methods = node->methods;

  if (methods->InitializeWorkerCustomScan)
  {
    int plan_node_id = node->ss.ps.plan->plan_node_id;
    void *coordinate;

    coordinate = shm_toc_lookup(pwcxt->toc, plan_node_id, false);
    methods->InitializeWorkerCustomScan(node, pwcxt->toc, coordinate);
  }
}

void
ExecShutdownCustomScan(CustomScanState *node)
{
  const CustomExecMethods *methods = node->methods;

  if (methods->ShutdownCustomScan)
  {
    methods->ShutdownCustomScan(node);
  }
}
