                                                                            
   
                  
                                        
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
                      
                                                    
                                                                 
                                                   
                                                  
   
          
                                                          
                                                       
                                                            
                                                                 
               
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeBitmapOr.h"
#include "miscadmin.h"

                                                                    
                 
   
                                  
                                                                    
   
static TupleTableSlot *
ExecBitmapOr(PlanState *pstate)
{
  elog(ERROR, "BitmapOr node does not support ExecProcNode call convention");
  return NULL;
}

                                                                    
                     
   
                                                    
                                                                    
   
BitmapOrState *
ExecInitBitmapOr(BitmapOr *node, EState *estate, int eflags)
{
  BitmapOrState *bitmaporstate = makeNode(BitmapOrState);
  PlanState **bitmapplanstates;
  int nplans;
  int i;
  ListCell *l;
  Plan *initNode;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                                           
     
  nplans = list_length(node->bitmapplans);

  bitmapplanstates = (PlanState **)palloc0(nplans * sizeof(PlanState *));

     
                                                    
     
  bitmaporstate->ps.plan = (Plan *)node;
  bitmaporstate->ps.state = estate;
  bitmaporstate->ps.ExecProcNode = ExecBitmapOr;
  bitmaporstate->bitmapplans = bitmapplanstates;
  bitmaporstate->nplans = nplans;

     
                                                                        
                                                
     
  i = 0;
  foreach (l, node->bitmapplans)
  {
    initNode = (Plan *)lfirst(l);
    bitmapplanstates[i] = ExecInitNode(initNode, estate, eflags);
    i++;
  }

     
                                  
     
                                                                           
                                                                       
     

  return bitmaporstate;
}

                                                                    
                        
                                                                    
   
Node *
MultiExecBitmapOr(BitmapOrState *node)
{
  PlanState **bitmapplans;
  int nplans;
  int i;
  TIDBitmap *result = NULL;

                                                    
  if (node->ps.instrument)
  {
    InstrStartNode(node->ps.instrument);
  }

     
                                   
     
  bitmapplans = node->bitmapplans;
  nplans = node->nplans;

     
                                                       
     
  for (i = 0; i < nplans; i++)
  {
    PlanState *subnode = bitmapplans[i];
    TIDBitmap *subresult;

       
                                                                         
                                                                        
                                                     
       
    if (IsA(subnode, BitmapIndexScanState))
    {
      if (result == NULL)                    
      {
                                                            
        result = tbm_create(work_mem * 1024L, ((BitmapOr *)node->ps.plan)->isshared ? node->ps.state->es_query_dsa : NULL);
      }

      ((BitmapIndexScanState *)subnode)->biss_result = result;

      subresult = (TIDBitmap *)MultiExecProcNode(subnode);

      if (subresult != result)
      {
        elog(ERROR, "unrecognized result from subplan");
      }
    }
    else
    {
                                   
      subresult = (TIDBitmap *)MultiExecProcNode(subnode);

      if (!subresult || !IsA(subresult, TIDBitmap))
      {
        elog(ERROR, "unrecognized result from subplan");
      }

      if (result == NULL)
      {
        result = subresult;                    
      }
      else
      {
        tbm_union(result, subresult);
        tbm_free(subresult);
      }
    }
  }

                                                 
  if (result == NULL)
  {
    elog(ERROR, "BitmapOr doesn't support zero inputs");
  }

                                                    
  if (node->ps.instrument)
  {
    InstrStopNode(node->ps.instrument, 0          );
  }

  return (Node *)result;
}

                                                                    
                    
   
                                                  
   
                                 
                                                                    
   
void
ExecEndBitmapOr(BitmapOrState *node)
{
  PlanState **bitmapplans;
  int nplans;
  int i;

     
                                   
     
  bitmapplans = node->bitmapplans;
  nplans = node->nplans;

     
                                                             
     
  for (i = 0; i < nplans; i++)
  {
    if (bitmapplans[i])
    {
      ExecEndNode(bitmapplans[i]);
    }
  }
}

void
ExecReScanBitmapOr(BitmapOrState *node)
{
  int i;

  for (i = 0; i < node->nplans; i++)
  {
    PlanState *subnode = node->bitmapplans[i];

       
                                                                  
                                           
       
    if (node->ps.chgParam != NULL)
    {
      UpdateChangedParamSet(subnode, node->ps.chgParam);
    }

       
                                                                          
                           
       
    if (subnode->chgParam == NULL)
    {
      ExecReScan(subnode);
    }
  }
}
