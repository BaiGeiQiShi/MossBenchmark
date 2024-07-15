                                                                            
   
              
                                                                         
                                                                          
                                                                          
                                                                 
                    
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "executor/executor.h"
#include "miscadmin.h"
#include "utils/memutils.h"

   
                                                                  
   
                                                                      
                                                               
                                           
   
static inline TupleTableSlot *
ExecScanFetch(ScanState *node, ExecScanAccessMtd accessMtd, ExecScanRecheckMtd recheckMtd)
{
  EState *estate = node->ps.state;

  CHECK_FOR_INTERRUPTS();

  if (estate->es_epq_active != NULL)
  {
    EPQState *epqstate = estate->es_epq_active;

       
                                                                        
                                                                     
                   
       
    Index scanrelid = ((Scan *)node->ps.plan)->scanrelid;

    if (scanrelid == 0)
    {
         
                                                                     
                                                                         
                                                                      
                                        
         

      TupleTableSlot *slot = node->ss_ScanTupleSlot;

      if (!(*recheckMtd)(node, slot))
      {
        ExecClearTuple(slot);                                    
      }
      return slot;
    }
    else if (epqstate->relsubs_done[scanrelid - 1])
    {
         
                                                                        
                            
         

      TupleTableSlot *slot = node->ss_ScanTupleSlot;

                                                             
      return ExecClearTuple(slot);
    }
    else if (epqstate->relsubs_slot[scanrelid - 1] != NULL)
    {
         
                                                              
         

      TupleTableSlot *slot = epqstate->relsubs_slot[scanrelid - 1];

      Assert(epqstate->relsubs_rowmark[scanrelid - 1] == NULL);

                                                          
      epqstate->relsubs_done[scanrelid - 1] = true;

                                                            
      if (TupIsNull(slot))
      {
        return NULL;
      }

                                                          
      if (!(*recheckMtd)(node, slot))
      {
        return ExecClearTuple(slot);                             
                                               
      }
      return slot;
    }
    else if (epqstate->relsubs_rowmark[scanrelid - 1] != NULL)
    {
         
                                                                         
         

      TupleTableSlot *slot = node->ss_ScanTupleSlot;

                                                          
      epqstate->relsubs_done[scanrelid - 1] = true;

      if (!EvalPlanQualFetchRowMark(epqstate, scanrelid, slot))
      {
        return NULL;
      }

                                                            
      if (TupIsNull(slot))
      {
        return NULL;
      }

                                                          
      if (!(*recheckMtd)(node, slot))
      {
        return ExecClearTuple(slot);                             
                                               
      }
      return slot;
    }
  }

     
                                                                             
     
  return (*accessMtd)(node);
}

                                                                    
             
   
                                                               
                                                                 
                                          
                                                               
                                                                         
   
                                                               
                                                                
                                                        
   
                
                                                                       
                           
   
                    
                                                                   
                                                                
                                                                    
   
TupleTableSlot *
ExecScan(ScanState *node, ExecScanAccessMtd accessMtd,                                 
    ExecScanRecheckMtd recheckMtd)
{
  ExprContext *econtext;
  ExprState *qual;
  ProjectionInfo *projInfo;

     
                          
     
  qual = node->ps.qual;
  projInfo = node->ps.ps_ProjInfo;
  econtext = node->ps.ps_ExprContext;

                                             

     
                                                                          
                                                     
     
  if (!qual && !projInfo)
  {
    ResetExprContext(econtext);
    return ExecScanFetch(node, accessMtd, recheckMtd);
  }

     
                                                                      
                                                    
     
  ResetExprContext(econtext);

     
                                                                            
                               
     
  for (;;)
  {
    TupleTableSlot *slot;

    slot = ExecScanFetch(node, accessMtd, recheckMtd);

       
                                                                          
                                                                      
                                                                         
                  
       
    if (TupIsNull(slot))
    {
      if (projInfo)
      {
        return ExecClearTuple(projInfo->pi_state.resultslot);
      }
      else
      {
        return slot;
      }
    }

       
                                                     
       
    econtext->ecxt_scantuple = slot;

       
                                                              
       
                                                                           
                                                                          
           
       
    if (qual == NULL || ExecQual(qual, econtext))
    {
         
                                          
         
      if (projInfo)
      {
           
                                                                      
                          
           
        return ExecProject(projInfo);
      }
      else
      {
           
                                                                  
           
        return slot;
      }
    }
    else
    {
      InstrCountFiltered1(node, 1);
    }

       
                                                                 
       
    ResetExprContext(econtext);
  }
}

   
                                
                                                          
   
                                                                         
                                                                       
                                                                           
                                                                            
                                                                              
          
   
                                                          
   
void
ExecAssignScanProjectionInfo(ScanState *node)
{
  Scan *scan = (Scan *)node->ps.plan;
  TupleDesc tupdesc = node->ss_ScanTupleSlot->tts_tupleDescriptor;

  ExecConditionalAssignProjectionInfo(&node->ps, tupdesc, scan->scanrelid);
}

   
                                         
                                                                          
   
void
ExecAssignScanProjectionInfoWithVarno(ScanState *node, Index varno)
{
  TupleDesc tupdesc = node->ss_ScanTupleSlot->tts_tupleDescriptor;

  ExecConditionalAssignProjectionInfo(&node->ps, tupdesc, varno);
}

   
                  
   
                                                                        
                         
   
void
ExecScanReScan(ScanState *node)
{
  EState *estate = node->ps.state;

     
                                                                          
                                                                
     
  ExecClearTuple(node->ss_ScanTupleSlot);

                                                                         
  if (estate->es_epq_active != NULL)
  {
    EPQState *epqstate = estate->es_epq_active;
    Index scanrelid = ((Scan *)node->ps.plan)->scanrelid;

    if (scanrelid > 0)
    {
      epqstate->relsubs_done[scanrelid - 1] = false;
    }
    else
    {
      Bitmapset *relids;
      int rtindex = -1;

         
                                                                        
                                                                       
                      
         
      if (IsA(node->ps.plan, ForeignScan))
      {
        relids = ((ForeignScan *)node->ps.plan)->fs_relids;
      }
      else if (IsA(node->ps.plan, CustomScan))
      {
        relids = ((CustomScan *)node->ps.plan)->custom_relids;
      }
      else
      {
        elog(ERROR, "unexpected scan node: %d", (int)nodeTag(node->ps.plan));
      }

      while ((rtindex = bms_next_member(relids, rtindex)) >= 0)
      {
        Assert(rtindex > 0);
        epqstate->relsubs_done[rtindex - 1] = false;
      }
    }
  }
}
