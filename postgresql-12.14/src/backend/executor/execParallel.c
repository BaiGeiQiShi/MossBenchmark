                                                                            
   
                  
                                              
   
                                                                         
                                                                        
   
                                                                        
                                                                        
                                                                     
                                                                       
                                                                       
                                                                      
                                                                       
                                                    
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "executor/execParallel.h"
#include "executor/executor.h"
#include "executor/nodeAppend.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeCustom.h"
#include "executor/nodeForeignscan.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeSeqscan.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/tqueue.h"
#include "jit/jit.h"
#include "nodes/nodeFuncs.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/datum.h"
#include "utils/dsa.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "pgstat.h"

   
                                                                        
                                                                          
                                                          
   
#define PARALLEL_KEY_EXECUTOR_FIXED UINT64CONST(0xE000000000000001)
#define PARALLEL_KEY_PLANNEDSTMT UINT64CONST(0xE000000000000002)
#define PARALLEL_KEY_PARAMLISTINFO UINT64CONST(0xE000000000000003)
#define PARALLEL_KEY_BUFFER_USAGE UINT64CONST(0xE000000000000004)
#define PARALLEL_KEY_TUPLE_QUEUE UINT64CONST(0xE000000000000005)
#define PARALLEL_KEY_INSTRUMENTATION UINT64CONST(0xE000000000000006)
#define PARALLEL_KEY_DSA UINT64CONST(0xE000000000000007)
#define PARALLEL_KEY_QUERY_TEXT UINT64CONST(0xE000000000000008)
#define PARALLEL_KEY_JIT_INSTRUMENTATION UINT64CONST(0xE000000000000009)

#define PARALLEL_TUPLE_QUEUE_SIZE 65536

   
                                                                     
   
typedef struct FixedParallelExecutorState
{
  int64 tuples_needed;                                         
  dsa_pointer param_exec;
  int eflags;
  int jit_flags;
} FixedParallelExecutorState;

   
                                                                 
   
                                                             
   
                                                                       
                                                                           
                           
   
                                   
   
                                         
   
                                                                                
                                                                                
   
struct SharedExecutorInstrumentation
{
  int instrument_options;
  int instrument_offset;
  int num_workers;
  int num_plan_nodes;
  int plan_node_id[FLEXIBLE_ARRAY_MEMBER];
                                                                             
};
#define GetInstrumentationArray(sei) (AssertVariableIsOfTypeMacro(sei, SharedExecutorInstrumentation *), (Instrumentation *)(((char *)sei) + sei->instrument_offset))

                                              
typedef struct ExecParallelEstimateContext
{
  ParallelContext *pcxt;
  int nnodes;
} ExecParallelEstimateContext;

                                                   
typedef struct ExecParallelInitializeDSMContext
{
  ParallelContext *pcxt;
  SharedExecutorInstrumentation *instrumentation;
  int nnodes;
} ExecParallelInitializeDSMContext;

                                                       
static char *
ExecSerializePlan(Plan *plan, EState *estate);
static bool
ExecParallelEstimate(PlanState *node, ExecParallelEstimateContext *e);
static bool
ExecParallelInitializeDSM(PlanState *node, ExecParallelInitializeDSMContext *d);
static shm_mq_handle **
ExecParallelSetupTupleQueues(ParallelContext *pcxt, bool reinitialize);
static bool
ExecParallelReInitializeDSM(PlanState *planstate, ParallelContext *pcxt);
static bool
ExecParallelRetrieveInstrumentation(PlanState *planstate, SharedExecutorInstrumentation *instrumentation);

                                                       
static DestReceiver *
ExecParallelGetReceiver(dsm_segment *seg, shm_toc *toc);

   
                                                                             
   
static char *
ExecSerializePlan(Plan *plan, EState *estate)
{
  PlannedStmt *pstmt;
  ListCell *lc;

                                                               
  plan = copyObject(plan);

     
                                                                            
                                                                           
                                                                           
                                                                       
                                                                       
                                                                            
             
     
  foreach (lc, plan->targetlist)
  {
    TargetEntry *tle = lfirst_node(TargetEntry, lc);

    tle->resjunk = false;
  }

     
                                                                            
                                                                   
                                        
     
  pstmt = makeNode(PlannedStmt);
  pstmt->commandType = CMD_SELECT;
  pstmt->queryId = UINT64CONST(0);
  pstmt->hasReturning = false;
  pstmt->hasModifyingCTE = false;
  pstmt->canSetTag = true;
  pstmt->transientPlan = false;
  pstmt->dependsOnRole = false;
  pstmt->parallelModeNeeded = false;
  pstmt->planTree = plan;
  pstmt->rtable = estate->es_range_table;
  pstmt->resultRelations = NIL;

     
                                                                             
                                                                    
                                                                            
                                                                         
                                                                     
     
  pstmt->subplans = NIL;
  foreach (lc, estate->es_plannedstmt->subplans)
  {
    Plan *subplan = (Plan *)lfirst(lc);

    if (subplan && !subplan->parallel_safe)
    {
      subplan = NULL;
    }
    pstmt->subplans = lappend(pstmt->subplans, subplan);
  }

  pstmt->rewindPlanIDs = NULL;
  pstmt->rowMarks = NIL;
  pstmt->relationOids = NIL;
  pstmt->invalItems = NIL;                                     
  pstmt->paramExecTypes = estate->es_plannedstmt->paramExecTypes;
  pstmt->utilityStmt = NULL;
  pstmt->stmt_location = -1;
  pstmt->stmt_len = -1;

                                                        
  return nodeToString(pstmt);
}

   
                                                                           
                                                                              
                                                                            
                     
   
                                                                          
                                                        
   
static bool
ExecParallelEstimate(PlanState *planstate, ExecParallelEstimateContext *e)
{
  if (planstate == NULL)
  {
    return false;
  }

                        
  e->nnodes++;

  switch (nodeTag(planstate))
  {
  case T_SeqScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecSeqScanEstimate((SeqScanState *)planstate, e->pcxt);
    }
    break;
  case T_IndexScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexScanEstimate((IndexScanState *)planstate, e->pcxt);
    }
    break;
  case T_IndexOnlyScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexOnlyScanEstimate((IndexOnlyScanState *)planstate, e->pcxt);
    }
    break;
  case T_ForeignScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecForeignScanEstimate((ForeignScanState *)planstate, e->pcxt);
    }
    break;
  case T_AppendState:
    if (planstate->plan->parallel_aware)
    {
      ExecAppendEstimate((AppendState *)planstate, e->pcxt);
    }
    break;
  case T_CustomScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecCustomScanEstimate((CustomScanState *)planstate, e->pcxt);
    }
    break;
  case T_BitmapHeapScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecBitmapHeapEstimate((BitmapHeapScanState *)planstate, e->pcxt);
    }
    break;
  case T_HashJoinState:
    if (planstate->plan->parallel_aware)
    {
      ExecHashJoinEstimate((HashJoinState *)planstate, e->pcxt);
    }
    break;
  case T_HashState:
                                                           
    ExecHashEstimate((HashState *)planstate, e->pcxt);
    break;
  case T_SortState:
                                                           
    ExecSortEstimate((SortState *)planstate, e->pcxt);
    break;

  default:
    break;
  }

  return planstate_tree_walker(planstate, ExecParallelEstimate, e);
}

   
                                                                                
   
static Size
EstimateParamExecSpace(EState *estate, Bitmapset *params)
{
  int paramid;
  Size sz = sizeof(int);

  paramid = -1;
  while ((paramid = bms_next_member(params, paramid)) >= 0)
  {
    Oid typeOid;
    int16 typLen;
    bool typByVal;
    ParamExecData *prm;

    prm = &(estate->es_param_exec_vals[paramid]);
    typeOid = list_nth_oid(estate->es_plannedstmt->paramExecTypes, paramid);

    sz = add_size(sz, sizeof(int));                        

                                
    if (OidIsValid(typeOid))
    {
      get_typlenbyval(typeOid, &typLen, &typByVal);
    }
    else
    {
                                                                     
      typLen = sizeof(Datum);
      typByVal = true;
    }
    sz = add_size(sz, datumEstimateSpace(prm->value, prm->isnull, typByVal, typLen));
  }
  return sz;
}

   
                                              
   
                                                                          
                                                                             
                                                                             
                                                                          
   
static dsa_pointer
SerializeParamExecParams(EState *estate, Bitmapset *params, dsa_area *area)
{
  Size size;
  int nparams;
  int paramid;
  ParamExecData *prm;
  dsa_pointer handle;
  char *start_address;

                                                               
  size = EstimateParamExecSpace(estate, params);
  handle = dsa_allocate(area, size);
  start_address = dsa_get_address(area, handle);

                                                                 
  nparams = bms_num_members(params);
  memcpy(start_address, &nparams, sizeof(int));
  start_address += sizeof(int);

                                                 
  paramid = -1;
  while ((paramid = bms_next_member(params, paramid)) >= 0)
  {
    Oid typeOid;
    int16 typLen;
    bool typByVal;

    prm = &(estate->es_param_exec_vals[paramid]);
    typeOid = list_nth_oid(estate->es_plannedstmt->paramExecTypes, paramid);

                        
    memcpy(start_address, &paramid, sizeof(int));
    start_address += sizeof(int);

                            
    if (OidIsValid(typeOid))
    {
      get_typlenbyval(typeOid, &typLen, &typByVal);
    }
    else
    {
                                                                     
      typLen = sizeof(Datum);
      typByVal = true;
    }
    datumSerialize(prm->value, prm->isnull, typByVal, typLen, &start_address);
  }

  return handle;
}

   
                                            
   
static void
RestoreParamExecParams(char *start_address, EState *estate)
{
  int nparams;
  int i;
  int paramid;

  memcpy(&nparams, start_address, sizeof(int));
  start_address += sizeof(int);

  for (i = 0; i < nparams; i++)
  {
    ParamExecData *prm;

                      
    memcpy(&paramid, start_address, sizeof(int));
    start_address += sizeof(int);
    prm = &(estate->es_param_exec_vals[paramid]);

                            
    prm->value = datumRestore(&start_address, &prm->isnull);
    prm->execPlan = NULL;
  }
}

   
                                                                             
                       
   
static bool
ExecParallelInitializeDSM(PlanState *planstate, ExecParallelInitializeDSMContext *d)
{
  if (planstate == NULL)
  {
    return false;
  }

                                                                     
  if (d->instrumentation != NULL)
  {
    d->instrumentation->plan_node_id[d->nnodes] = planstate->plan->plan_node_id;
  }

                        
  d->nnodes++;

     
                                                 
     
                                                                           
                                                                        
                                                                        
                                                                        
                                                                       
     
  switch (nodeTag(planstate))
  {
  case T_SeqScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecSeqScanInitializeDSM((SeqScanState *)planstate, d->pcxt);
    }
    break;
  case T_IndexScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexScanInitializeDSM((IndexScanState *)planstate, d->pcxt);
    }
    break;
  case T_IndexOnlyScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexOnlyScanInitializeDSM((IndexOnlyScanState *)planstate, d->pcxt);
    }
    break;
  case T_ForeignScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecForeignScanInitializeDSM((ForeignScanState *)planstate, d->pcxt);
    }
    break;
  case T_AppendState:
    if (planstate->plan->parallel_aware)
    {
      ExecAppendInitializeDSM((AppendState *)planstate, d->pcxt);
    }
    break;
  case T_CustomScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecCustomScanInitializeDSM((CustomScanState *)planstate, d->pcxt);
    }
    break;
  case T_BitmapHeapScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecBitmapHeapInitializeDSM((BitmapHeapScanState *)planstate, d->pcxt);
    }
    break;
  case T_HashJoinState:
    if (planstate->plan->parallel_aware)
    {
      ExecHashJoinInitializeDSM((HashJoinState *)planstate, d->pcxt);
    }
    break;
  case T_HashState:
                                                           
    ExecHashInitializeDSM((HashState *)planstate, d->pcxt);
    break;
  case T_SortState:
                                                           
    ExecSortInitializeDSM((SortState *)planstate, d->pcxt);
    break;

  default:
    break;
  }

  return planstate_tree_walker(planstate, ExecParallelInitializeDSM, d);
}

   
                                                                       
                                              
   
static shm_mq_handle **
ExecParallelSetupTupleQueues(ParallelContext *pcxt, bool reinitialize)
{
  shm_mq_handle **responseq;
  char *tqueuespace;
  int i;

                                
  if (pcxt->nworkers == 0)
  {
    return NULL;
  }

                                                        
  responseq = (shm_mq_handle **)palloc(pcxt->nworkers * sizeof(shm_mq_handle *));

     
                                                                        
                                                  
     
  if (!reinitialize)
  {
    tqueuespace = shm_toc_allocate(pcxt->toc, mul_size(PARALLEL_TUPLE_QUEUE_SIZE, pcxt->nworkers));
  }
  else
  {
    tqueuespace = shm_toc_lookup(pcxt->toc, PARALLEL_KEY_TUPLE_QUEUE, false);
  }

                                                            
  for (i = 0; i < pcxt->nworkers; ++i)
  {
    shm_mq *mq;

    mq = shm_mq_create(tqueuespace + ((Size)i) * PARALLEL_TUPLE_QUEUE_SIZE, (Size)PARALLEL_TUPLE_QUEUE_SIZE);

    shm_mq_set_receiver(mq, MyProc);
    responseq[i] = shm_mq_attach(mq, pcxt->seg, NULL);
  }

                                                              
  if (!reinitialize)
  {
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_TUPLE_QUEUE, tqueuespace);
  }

                                
  return responseq;
}

   
                                                                      
                                                     
   
ParallelExecutorInfo *
ExecInitParallelPlan(PlanState *planstate, EState *estate, Bitmapset *sendParams, int nworkers, int64 tuples_needed)
{
  ParallelExecutorInfo *pei;
  ParallelContext *pcxt;
  ExecParallelEstimateContext e;
  ExecParallelInitializeDSMContext d;
  FixedParallelExecutorState *fpes;
  char *pstmt_data;
  char *pstmt_space;
  char *paramlistinfo_space;
  BufferUsage *bufusage_space;
  SharedExecutorInstrumentation *instrumentation = NULL;
  SharedJitInstrumentation *jit_instrumentation = NULL;
  int pstmt_len;
  int paramlistinfo_len;
  int instrumentation_len = 0;
  int jit_instrumentation_len = 0;
  int instrument_offset = 0;
  Size dsa_minsize = dsa_minimum_size();
  char *query_string;
  int query_len;

     
                                                                          
                                         
     
                                                                            
                                                                             
                                                                         
                                                                            
                                                                         
                                                                    
     
  ExecSetParamPlanMulti(sendParams, GetPerTupleExprContext(estate));

                                         
  pei = palloc0(sizeof(ParallelExecutorInfo));
  pei->finished = false;
  pei->planstate = planstate;

                                                        
  pstmt_data = ExecSerializePlan(planstate->plan, estate);

                                  
  pcxt = CreateParallelContext("postgres", "ParallelQueryMain", nworkers);
  pei->pcxt = pcxt;

     
                                                                           
                                                                          
                                              
     

                                            
  shm_toc_estimate_chunk(&pcxt->estimator, sizeof(FixedParallelExecutorState));
  shm_toc_estimate_keys(&pcxt->estimator, 1);

                                      
  query_len = strlen(estate->es_sourceText);
  shm_toc_estimate_chunk(&pcxt->estimator, query_len + 1);
  shm_toc_estimate_keys(&pcxt->estimator, 1);

                                                  
  pstmt_len = strlen(pstmt_data) + 1;
  shm_toc_estimate_chunk(&pcxt->estimator, pstmt_len);
  shm_toc_estimate_keys(&pcxt->estimator, 1);

                                                    
  paramlistinfo_len = EstimateParamListSpace(estate->es_param_list_info);
  shm_toc_estimate_chunk(&pcxt->estimator, paramlistinfo_len);
  shm_toc_estimate_keys(&pcxt->estimator, 1);

     
                                     
     
                                                                            
                                                                         
                                                         
     
  shm_toc_estimate_chunk(&pcxt->estimator, mul_size(sizeof(BufferUsage), pcxt->nworkers));
  shm_toc_estimate_keys(&pcxt->estimator, 1);

                                        
  shm_toc_estimate_chunk(&pcxt->estimator, mul_size(PARALLEL_TUPLE_QUEUE_SIZE, pcxt->nworkers));
  shm_toc_estimate_keys(&pcxt->estimator, 1);

     
                                                                           
                                                  
     
  e.pcxt = pcxt;
  e.nnodes = 0;
  ExecParallelEstimate(planstate, &e);

                                                        
  if (estate->es_instrument)
  {
    instrumentation_len = offsetof(SharedExecutorInstrumentation, plan_node_id) + sizeof(int) * e.nnodes;
    instrumentation_len = MAXALIGN(instrumentation_len);
    instrument_offset = instrumentation_len;
    instrumentation_len += mul_size(sizeof(Instrumentation), mul_size(e.nnodes, nworkers));
    shm_toc_estimate_chunk(&pcxt->estimator, instrumentation_len);
    shm_toc_estimate_keys(&pcxt->estimator, 1);

                                                              
    if (estate->es_jit_flags != PGJIT_NONE)
    {
      jit_instrumentation_len = offsetof(SharedJitInstrumentation, jit_instr) + sizeof(JitInstrumentation) * nworkers;
      shm_toc_estimate_chunk(&pcxt->estimator, jit_instrumentation_len);
      shm_toc_estimate_keys(&pcxt->estimator, 1);
    }
  }

                                    
  shm_toc_estimate_chunk(&pcxt->estimator, dsa_minsize);
  shm_toc_estimate_keys(&pcxt->estimator, 1);

                                                                        
  InitializeParallelDSM(pcxt);

     
                                                                           
                                                                            
                                                                             
                                                                        
                                                                          
     

                               
  fpes = shm_toc_allocate(pcxt->toc, sizeof(FixedParallelExecutorState));
  fpes->tuples_needed = tuples_needed;
  fpes->param_exec = InvalidDsaPointer;
  fpes->eflags = estate->es_top_eflags;
  fpes->jit_flags = estate->es_jit_flags;
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_EXECUTOR_FIXED, fpes);

                          
  query_string = shm_toc_allocate(pcxt->toc, query_len + 1);
  memcpy(query_string, estate->es_sourceText, query_len + 1);
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_QUERY_TEXT, query_string);

                                     
  pstmt_space = shm_toc_allocate(pcxt->toc, pstmt_len);
  memcpy(pstmt_space, pstmt_data, pstmt_len);
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_PLANNEDSTMT, pstmt_space);

                                       
  paramlistinfo_space = shm_toc_allocate(pcxt->toc, paramlistinfo_len);
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_PARAMLISTINFO, paramlistinfo_space);
  SerializeParamList(estate->es_param_list_info, &paramlistinfo_space);

                                                                            
  bufusage_space = shm_toc_allocate(pcxt->toc, mul_size(sizeof(BufferUsage), pcxt->nworkers));
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_BUFFER_USAGE, bufusage_space);
  pei->buffer_usage = bufusage_space;

                                                                 
  pei->tqueue = ExecParallelSetupTupleQueues(pcxt, false);

                                                        
  pei->reader = NULL;

     
                                                                            
                                                                      
                                
     
  if (estate->es_instrument)
  {
    Instrumentation *instrument;
    int i;

    instrumentation = shm_toc_allocate(pcxt->toc, instrumentation_len);
    instrumentation->instrument_options = estate->es_instrument;
    instrumentation->instrument_offset = instrument_offset;
    instrumentation->num_workers = nworkers;
    instrumentation->num_plan_nodes = e.nnodes;
    instrument = GetInstrumentationArray(instrumentation);
    for (i = 0; i < nworkers * e.nnodes; ++i)
    {
      InstrInit(&instrument[i], estate->es_instrument);
    }
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_INSTRUMENTATION, instrumentation);
    pei->instrumentation = instrumentation;

    if (estate->es_jit_flags != PGJIT_NONE)
    {
      jit_instrumentation = shm_toc_allocate(pcxt->toc, jit_instrumentation_len);
      jit_instrumentation->num_workers = nworkers;
      memset(jit_instrumentation->jit_instr, 0, sizeof(JitInstrumentation) * nworkers);
      shm_toc_insert(pcxt->toc, PARALLEL_KEY_JIT_INSTRUMENTATION, jit_instrumentation);
      pei->jit_instrumentation = jit_instrumentation;
    }
  }

     
                                                                       
                                                                         
                               
     
  if (pcxt->seg != NULL)
  {
    char *area_space;

    area_space = shm_toc_allocate(pcxt->toc, dsa_minsize);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_DSA, area_space);
    pei->area = dsa_create_in_place(area_space, dsa_minsize, LWTRANCHE_PARALLEL_QUERY_DSA, pcxt->seg);

       
                                                                           
                                                                      
                                                                     
                                      
       
    if (!bms_is_empty(sendParams))
    {
      pei->param_exec = SerializeParamExecParams(estate, sendParams, pei->area);
      fpes->param_exec = pei->param_exec;
    }
  }

     
                                                                         
                                                                           
                   
     
  d.pcxt = pcxt;
  d.instrumentation = instrumentation;
  d.nnodes = 0;

                                                         
  estate->es_query_dsa = pei->area;
  ExecParallelInitializeDSM(planstate, &d);
  estate->es_query_dsa = NULL;

     
                                                                         
                                                                      
     
  if (e.nnodes != d.nnodes)
  {
    elog(ERROR, "inconsistent count of PlanState nodes");
  }

                                         
  return pei;
}

   
                                                                         
   
                                                                          
                                                                          
   
void
ExecParallelCreateReaders(ParallelExecutorInfo *pei)
{
  int nworkers = pei->pcxt->nworkers_launched;
  int i;

  Assert(pei->reader == NULL);

  if (nworkers > 0)
  {
    pei->reader = (TupleQueueReader **)palloc(nworkers * sizeof(TupleQueueReader *));

    for (i = 0; i < nworkers; i++)
    {
      shm_mq_set_handle(pei->tqueue[i], pei->pcxt->worker[i].bgwhandle);
      pei->reader[i] = CreateTupleQueueReader(pei->tqueue[i]);
    }
  }
}

   
                                                                            
                             
   
void
ExecParallelReinitialize(PlanState *planstate, ParallelExecutorInfo *pei, Bitmapset *sendParams)
{
  EState *estate = planstate->state;
  FixedParallelExecutorState *fpes;

                                             
  Assert(pei->finished);

     
                                                                          
                                                         
                            
     
  ExecSetParamPlanMulti(sendParams, GetPerTupleExprContext(estate));

  ReinitializeParallelDSM(pei->pcxt);
  pei->tqueue = ExecParallelSetupTupleQueues(pei->pcxt, true);
  pei->reader = NULL;
  pei->finished = false;

  fpes = shm_toc_lookup(pei->pcxt->toc, PARALLEL_KEY_EXECUTOR_FIXED, false);

                                                           
  if (DsaPointerIsValid(fpes->param_exec))
  {
    dsa_free(pei->area, fpes->param_exec);
    fpes->param_exec = InvalidDsaPointer;
  }

                                                       
  if (!bms_is_empty(sendParams))
  {
    pei->param_exec = SerializeParamExecParams(estate, sendParams, pei->area);
    fpes->param_exec = pei->param_exec;
  }

                                                                          
  estate->es_query_dsa = pei->area;
  ExecParallelReInitializeDSM(planstate, pei->pcxt);
  estate->es_query_dsa = NULL;
}

   
                                                                           
   
static bool
ExecParallelReInitializeDSM(PlanState *planstate, ParallelContext *pcxt)
{
  if (planstate == NULL)
  {
    return false;
  }

     
                                                   
     
  switch (nodeTag(planstate))
  {
  case T_SeqScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecSeqScanReInitializeDSM((SeqScanState *)planstate, pcxt);
    }
    break;
  case T_IndexScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexScanReInitializeDSM((IndexScanState *)planstate, pcxt);
    }
    break;
  case T_IndexOnlyScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexOnlyScanReInitializeDSM((IndexOnlyScanState *)planstate, pcxt);
    }
    break;
  case T_ForeignScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecForeignScanReInitializeDSM((ForeignScanState *)planstate, pcxt);
    }
    break;
  case T_AppendState:
    if (planstate->plan->parallel_aware)
    {
      ExecAppendReInitializeDSM((AppendState *)planstate, pcxt);
    }
    break;
  case T_CustomScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecCustomScanReInitializeDSM((CustomScanState *)planstate, pcxt);
    }
    break;
  case T_BitmapHeapScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecBitmapHeapReInitializeDSM((BitmapHeapScanState *)planstate, pcxt);
    }
    break;
  case T_HashJoinState:
    if (planstate->plan->parallel_aware)
    {
      ExecHashJoinReInitializeDSM((HashJoinState *)planstate, pcxt);
    }
    break;
  case T_HashState:
  case T_SortState:
                                                                         
    break;

  default:
    break;
  }

  return planstate_tree_walker(planstate, ExecParallelReInitializeDSM, pcxt);
}

   
                                                                             
                          
   
static bool
ExecParallelRetrieveInstrumentation(PlanState *planstate, SharedExecutorInstrumentation *instrumentation)
{
  Instrumentation *instrument;
  int i;
  int n;
  int ibytes;
  int plan_node_id = planstate->plan->plan_node_id;
  MemoryContext oldcontext;

                                               
  for (i = 0; i < instrumentation->num_plan_nodes; ++i)
  {
    if (instrumentation->plan_node_id[i] == plan_node_id)
    {
      break;
    }
  }
  if (i >= instrumentation->num_plan_nodes)
  {
    elog(ERROR, "plan node %d not found", plan_node_id);
  }

                                                   
  instrument = GetInstrumentationArray(instrumentation);
  instrument += i * instrumentation->num_workers;
  for (n = 0; n < instrumentation->num_workers; ++n)
  {
    InstrAggNode(planstate->instrument, &instrument[n]);
  }

     
                                       
     
                                                                           
                                                                          
                                           
     
  oldcontext = MemoryContextSwitchTo(planstate->state->es_query_cxt);
  ibytes = mul_size(instrumentation->num_workers, sizeof(Instrumentation));
  planstate->worker_instrument = palloc(ibytes + offsetof(WorkerInstrumentation, instrument));
  MemoryContextSwitchTo(oldcontext);

  planstate->worker_instrument->num_workers = instrumentation->num_workers;
  memcpy(&planstate->worker_instrument->instrument, instrument, ibytes);

                                                                  
  switch (nodeTag(planstate))
  {
  case T_SortState:
    ExecSortRetrieveInstrumentation((SortState *)planstate);
    break;
  case T_HashState:
    ExecHashRetrieveInstrumentation((HashState *)planstate);
    break;
  default:
    break;
  }

  return planstate_tree_walker(planstate, ExecParallelRetrieveInstrumentation, instrumentation);
}

   
                                                                       
   
static void
ExecParallelRetrieveJitInstrumentation(PlanState *planstate, SharedJitInstrumentation *shared_jit)
{
  JitInstrumentation *combined;
  int ibytes;

  int n;

     
                                                                 
                                                 
     
  if (!planstate->state->es_jit_worker_instr)
  {
    planstate->state->es_jit_worker_instr = MemoryContextAllocZero(planstate->state->es_query_cxt, sizeof(JitInstrumentation));
  }
  combined = planstate->state->es_jit_worker_instr;

                                                     
  for (n = 0; n < shared_jit->num_workers; ++n)
  {
    InstrJitAgg(combined, &shared_jit->jit_instr[n]);
  }

     
                                  
     
                                                                    
                                           
     
  ibytes = offsetof(SharedJitInstrumentation, jit_instr) + mul_size(shared_jit->num_workers, sizeof(JitInstrumentation));
  planstate->worker_jit_instrument = MemoryContextAlloc(planstate->state->es_query_cxt, ibytes);

  memcpy(planstate->worker_jit_instrument, shared_jit, ibytes);
}

   
                                                                           
                                  
   
void
ExecParallelFinish(ParallelExecutorInfo *pei)
{
  int nworkers = pei->pcxt->nworkers_launched;
  int i;

                                                      
  if (pei->finished)
  {
    return;
  }

     
                                                                          
                                                
     
  if (pei->tqueue != NULL)
  {
    for (i = 0; i < nworkers; i++)
    {
      shm_mq_detach(pei->tqueue[i]);
    }
    pfree(pei->tqueue);
    pei->tqueue = NULL;
  }

     
                                                                         
                                                                             
     
  if (pei->reader != NULL)
  {
    for (i = 0; i < nworkers; i++)
    {
      DestroyTupleQueueReader(pei->reader[i]);
    }
    pfree(pei->reader);
    pei->reader = NULL;
  }

                                           
  WaitForParallelWorkersToFinish(pei->pcxt);

     
                                                                        
                                               
     
  for (i = 0; i < nworkers; i++)
  {
    InstrAccumParallelQuery(&pei->buffer_usage[i]);
  }

  pei->finished = true;
}

   
                                                                               
                                                                      
                                                                          
                                                             
   
void
ExecParallelCleanup(ParallelExecutorInfo *pei)
{
                                           
  if (pei->instrumentation)
  {
    ExecParallelRetrieveInstrumentation(pei->planstate, pei->instrumentation);
  }

                                               
  if (pei->jit_instrumentation)
  {
    ExecParallelRetrieveJitInstrumentation(pei->planstate, pei->jit_instrumentation);
  }

                                       
  if (DsaPointerIsValid(pei->param_exec))
  {
    dsa_free(pei->area, pei->param_exec);
    pei->param_exec = InvalidDsaPointer;
  }
  if (pei->area != NULL)
  {
    dsa_detach(pei->area);
    pei->area = NULL;
  }
  if (pei->pcxt != NULL)
  {
    DestroyParallelContext(pei->pcxt);
    pei->pcxt = NULL;
  }
  pfree(pei);
}

   
                                                                             
                     
   
static DestReceiver *
ExecParallelGetReceiver(dsm_segment *seg, shm_toc *toc)
{
  char *mqspace;
  shm_mq *mq;

  mqspace = shm_toc_lookup(toc, PARALLEL_KEY_TUPLE_QUEUE, false);
  mqspace += ParallelWorkerNumber * PARALLEL_TUPLE_QUEUE_SIZE;
  mq = (shm_mq *)mqspace;
  shm_mq_set_sender(mq, MyProc);
  return CreateTupleQueueDestReceiver(shm_mq_attach(mq, seg, NULL));
}

   
                                                                            
   
static QueryDesc *
ExecParallelGetQueryDesc(shm_toc *toc, DestReceiver *receiver, int instrument_options)
{
  char *pstmtspace;
  char *paramspace;
  PlannedStmt *pstmt;
  ParamListInfo paramLI;
  char *queryString;

                                               
  queryString = shm_toc_lookup(toc, PARALLEL_KEY_QUERY_TEXT, false);

                                                
  pstmtspace = shm_toc_lookup(toc, PARALLEL_KEY_PLANNEDSTMT, false);
  pstmt = (PlannedStmt *)stringToNode(pstmtspace);

                                  
  paramspace = shm_toc_lookup(toc, PARALLEL_KEY_PARAMLISTINFO, false);
  paramLI = RestoreParamList(&paramspace);

                                         
  return CreateQueryDesc(pstmt, queryString, GetActiveSnapshot(), InvalidSnapshot, receiver, paramLI, NULL, instrument_options);
}

   
                                                                            
                                                                       
   
static bool
ExecParallelReportInstrumentation(PlanState *planstate, SharedExecutorInstrumentation *instrumentation)
{
  int i;
  int plan_node_id = planstate->plan->plan_node_id;
  Instrumentation *instrument;

  InstrEndLoop(planstate->instrument);

     
                                                                         
                                                                           
                                                                           
                     
     
  for (i = 0; i < instrumentation->num_plan_nodes; ++i)
  {
    if (instrumentation->plan_node_id[i] == plan_node_id)
    {
      break;
    }
  }
  if (i >= instrumentation->num_plan_nodes)
  {
    elog(ERROR, "plan node %d not found", plan_node_id);
  }

     
                                                                           
                                                                     
     
  instrument = GetInstrumentationArray(instrumentation);
  instrument += i * instrumentation->num_workers;
  Assert(IsParallelWorker());
  Assert(ParallelWorkerNumber < instrumentation->num_workers);
  InstrAggNode(&instrument[ParallelWorkerNumber], planstate->instrument);

  return planstate_tree_walker(planstate, ExecParallelReportInstrumentation, instrumentation);
}

   
                                                                     
                                                                         
                                                                             
   
static bool
ExecParallelInitializeWorker(PlanState *planstate, ParallelWorkerContext *pwcxt)
{
  if (planstate == NULL)
  {
    return false;
  }

  switch (nodeTag(planstate))
  {
  case T_SeqScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecSeqScanInitializeWorker((SeqScanState *)planstate, pwcxt);
    }
    break;
  case T_IndexScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexScanInitializeWorker((IndexScanState *)planstate, pwcxt);
    }
    break;
  case T_IndexOnlyScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecIndexOnlyScanInitializeWorker((IndexOnlyScanState *)planstate, pwcxt);
    }
    break;
  case T_ForeignScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecForeignScanInitializeWorker((ForeignScanState *)planstate, pwcxt);
    }
    break;
  case T_AppendState:
    if (planstate->plan->parallel_aware)
    {
      ExecAppendInitializeWorker((AppendState *)planstate, pwcxt);
    }
    break;
  case T_CustomScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecCustomScanInitializeWorker((CustomScanState *)planstate, pwcxt);
    }
    break;
  case T_BitmapHeapScanState:
    if (planstate->plan->parallel_aware)
    {
      ExecBitmapHeapInitializeWorker((BitmapHeapScanState *)planstate, pwcxt);
    }
    break;
  case T_HashJoinState:
    if (planstate->plan->parallel_aware)
    {
      ExecHashJoinInitializeWorker((HashJoinState *)planstate, pwcxt);
    }
    break;
  case T_HashState:
                                                           
    ExecHashInitializeWorker((HashState *)planstate, pwcxt);
    break;
  case T_SortState:
                                                           
    ExecSortInitializeWorker((SortState *)planstate, pwcxt);
    break;

  default:
    break;
  }

  return planstate_tree_walker(planstate, ExecParallelInitializeWorker, pwcxt);
}

   
                                                        
   
                                                                             
                                                                 
                                                                            
                                                                           
         
   
                                                                            
                                                                            
                                                                          
                                                                              
                                                                             
                
   
void
ParallelQueryMain(dsm_segment *seg, shm_toc *toc)
{
  FixedParallelExecutorState *fpes;
  BufferUsage *buffer_usage;
  DestReceiver *receiver;
  QueryDesc *queryDesc;
  SharedExecutorInstrumentation *instrumentation;
  SharedJitInstrumentation *jit_instrumentation;
  int instrument_options = 0;
  void *area_space;
  dsa_area *area;
  ParallelWorkerContext pwcxt;

                             
  fpes = shm_toc_lookup(toc, PARALLEL_KEY_EXECUTOR_FIXED, false);

                                                                          
  receiver = ExecParallelGetReceiver(seg, toc);
  instrumentation = shm_toc_lookup(toc, PARALLEL_KEY_INSTRUMENTATION, true);
  if (instrumentation != NULL)
  {
    instrument_options = instrumentation->instrument_options;
  }
  jit_instrumentation = shm_toc_lookup(toc, PARALLEL_KEY_JIT_INSTRUMENTATION, true);
  queryDesc = ExecParallelGetQueryDesc(toc, receiver, instrument_options);

                                                         
  debug_query_string = queryDesc->sourceText;

                                                     
  pgstat_report_activity(STATE_RUNNING, debug_query_string);

                                                 
  area_space = shm_toc_lookup(toc, PARALLEL_KEY_DSA, false);
  area = dsa_attach_in_place(area_space, seg);

                             
  queryDesc->plannedstmt->jitFlags = fpes->jit_flags;
  ExecutorStart(queryDesc, fpes->eflags);

                                                                  
  queryDesc->planstate->state->es_query_dsa = area;
  if (DsaPointerIsValid(fpes->param_exec))
  {
    char *paramexec_space;

    paramexec_space = dsa_get_address(area, fpes->param_exec);
    RestoreParamExecParams(paramexec_space, queryDesc->estate);
  }
  pwcxt.toc = toc;
  pwcxt.seg = seg;
  ExecParallelInitializeWorker(queryDesc->planstate, &pwcxt);

                                 
  ExecSetTupleBound(fpes->tuples_needed, queryDesc->planstate);

     
                                                           
     
                                                                            
                                                                        
                       
     
  InstrStartParallelQuery();

     
                                                                            
                            
     
  ExecutorRun(queryDesc, ForwardScanDirection, fpes->tuples_needed < 0 ? (int64)0 : fpes->tuples_needed, true);

                              
  ExecutorFinish(queryDesc);

                                                      
  buffer_usage = shm_toc_lookup(toc, PARALLEL_KEY_BUFFER_USAGE, false);
  InstrEndParallelQuery(&buffer_usage[ParallelWorkerNumber]);

                                                                           
  if (instrumentation != NULL)
  {
    ExecParallelReportInstrumentation(queryDesc->planstate, instrumentation);
  }

                                              
  if (queryDesc->estate->es_jit && jit_instrumentation != NULL)
  {
    Assert(ParallelWorkerNumber < jit_instrumentation->num_workers);
    jit_instrumentation->jit_instr[ParallelWorkerNumber] = queryDesc->estate->es_jit->instr;
  }

                                                     
  ExecutorEnd(queryDesc);

                
  dsa_detach(area);
  FreeQueryDesc(queryDesc);
  receiver->rDestroy(receiver);
}
