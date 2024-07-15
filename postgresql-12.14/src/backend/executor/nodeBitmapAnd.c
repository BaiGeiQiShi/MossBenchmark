                                                                            
   
                   
                                         
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
                      
                                                      
                                                                  
                                                    
                                                    
   
          
                                                           
                                                       
                                                            
                                                                 
               
   

#include "postgres.h"

#include "executor/execdebug.h"
#include "executor/nodeBitmapAnd.h"

                                                                    
                  
   
                                  
                                                                    
   
static TupleTableSlot *
ExecBitmapAnd(PlanState *pstate)
{
  elog(ERROR, "BitmapAnd node does not support ExecProcNode call convention");
  return NULL;
}

                                                                    
                      
   
                                                     
                                                                    
   
BitmapAndState *
ExecInitBitmapAnd(BitmapAnd *node, EState *estate, int eflags)
{
  BitmapAndState *bitmapandstate = makeNode(BitmapAndState);
  PlanState **bitmapplanstates;
  int nplans;
  int i;
  ListCell *l;
  Plan *initNode;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                                           
     
  nplans = list_length(node->bitmapplans);

  bitmapplanstates = (PlanState **)palloc0(nplans * sizeof(PlanState *));

     
                                                      
     
  bitmapandstate->ps.plan = (Plan *)node;
  bitmapandstate->ps.state = estate;
  bitmapandstate->ps.ExecProcNode = ExecBitmapAnd;
  bitmapandstate->bitmapplans = bitmapplanstates;
  bitmapandstate->nplans = nplans;

     
                                                                        
                                                
     
  i = 0;
  foreach (l, node->bitmapplans)
  {
    initNode = (Plan *)lfirst(l);
    bitmapplanstates[i] = ExecInitNode(initNode, estate, eflags);
    i++;
  }

     
                                  
     
                                                                            
                                                                       
     

  return bitmapandstate;
}

                                                                    
                         
                                                                    
   
Node *
MultiExecBitmapAnd(BitmapAndState *node)
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
      tbm_intersect(result, subresult);
      tbm_free(subresult);
    }

       
                                                                          
                                                                           
                                                                         
                                                                        
               
       
    if (tbm_is_empty(result))
    {
      break;
    }
  }

  if (result == NULL)
  {
    elog(ERROR, "BitmapAnd doesn't support zero inputs");
  }

                                                    
  if (node->ps.instrument)
  {
    InstrStopNode(node->ps.instrument, 0          );
  }

  return (Node *)result;
}

                                                                    
                     
   
                                                   
   
                                 
                                                                    
   
void
ExecEndBitmapAnd(BitmapAndState *node)
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
ExecReScanBitmapAnd(BitmapAndState *node)
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
