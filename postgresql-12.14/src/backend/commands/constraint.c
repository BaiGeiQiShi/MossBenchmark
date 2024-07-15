                                                                            
   
                
                                         
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/tableam.h"
#include "catalog/index.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

   
                                                                            
   
                                                                           
                        
   
                                                                       
                                                                      
                            
   
                                                                    
                                                 
   
Datum
unique_key_recheck(PG_FUNCTION_ARGS)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  const char *funcname = "unique_key_recheck";
  ItemPointerData checktid;
  ItemPointerData tmptid;
  Relation indexRel;
  IndexInfo *indexInfo;
  EState *estate;
  ExprContext *econtext;
  TupleTableSlot *slot;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];

     
                                                                    
                                                                             
                                                     
     
  if (!CALLED_AS_TRIGGER(fcinfo))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" was not called by trigger manager", funcname)));
  }

  if (!TRIGGER_FIRED_AFTER(trigdata->tg_event) || !TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" must be fired AFTER ROW", funcname)));
  }

     
                                                 
     
  if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
  {
    checktid = trigdata->tg_trigslot->tts_tid;
  }
  else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
  {
    checktid = trigdata->tg_newslot->tts_tid;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" must be fired for INSERT or UPDATE", funcname)));
    ItemPointerSetInvalid(&checktid);                          
  }

  slot = table_slot_create(trigdata->tg_relation, NULL);

     
                                                                          
                                                                          
                                                                           
                                                                          
                                                                            
                                                                  
                                                                       
                                                                          
                                             
     
                                                                          
                                                                           
                                                                           
                                                                             
                                                                       
              
     
  tmptid = checktid;
  {
    IndexFetchTableData *scan = table_index_fetch_begin(trigdata->tg_relation);
    bool call_again = false;

    if (!table_index_fetch_tuple(scan, &tmptid, SnapshotSelf, slot, &call_again, NULL))
    {
         
                                                                      
                
         
      ExecDropSingleTupleTableSlot(slot);
      table_index_fetch_end(scan);
      return PointerGetDatum(NULL);
    }
    table_index_fetch_end(scan);
  }

     
                                                                            
                                                                         
                                              
     
  indexRel = index_open(trigdata->tg_trigger->tgconstrindid, RowExclusiveLock);
  indexInfo = BuildIndexInfo(indexRel);

     
                                                                           
                                                                         
                                              
     
  if (indexInfo->ii_Expressions != NIL || indexInfo->ii_ExclusionOps != NULL)
  {
    estate = CreateExecutorState();
    econtext = GetPerTupleExprContext(estate);
    econtext->ecxt_scantuple = slot;
  }
  else
  {
    estate = NULL;
  }

     
                                                                             
               
     
                                                                             
                                                                          
                                                                        
                                                                       
                                                                
     
  FormIndexDatum(indexInfo, slot, estate, values, isnull);

     
                                   
     
  if (indexInfo->ii_ExclusionOps == NULL)
  {
       
                                                                           
                                                                          
                                                                         
                                                                        
              
       
    index_insert(indexRel, values, isnull, &checktid, trigdata->tg_relation, UNIQUE_CHECK_EXISTING, indexInfo);
  }
  else
  {
       
                                                                           
                                                                          
                                                                        
                                
       
    check_exclusion_constraint(trigdata->tg_relation, indexRel, indexInfo, &tmptid, values, isnull, estate, false);
  }

     
                                                                             
               
     
  if (estate != NULL)
  {
    FreeExecutorState(estate);
  }

  ExecDropSingleTupleTableSlot(slot);

  index_close(indexRel, RowExclusiveLock);

  return PointerGetDatum(NULL);
}
