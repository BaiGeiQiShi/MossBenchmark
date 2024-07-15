                                                                            
   
               
                                                   
   
   
                                                                         
                                                                        
   
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "commands/trigger.h"
#include "utils/builtins.h"
#include "utils/rel.h"

   
                                      
   
                                                                
                                             
   
Datum
suppress_redundant_updates_trigger(PG_FUNCTION_ARGS)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  HeapTuple newtuple, oldtuple, rettuple;
  HeapTupleHeader newheader, oldheader;

                                          
  if (!CALLED_AS_TRIGGER(fcinfo))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("suppress_redundant_updates_trigger: must be called as trigger")));
  }

                                      
  if (!TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("suppress_redundant_updates_trigger: must be called on update")));
  }

                                          
  if (!TRIGGER_FIRED_BEFORE(trigdata->tg_event))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("suppress_redundant_updates_trigger: must be called before update")));
  }

                                         
  if (!TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("suppress_redundant_updates_trigger: must be called for each row")));
  }

                                          
  rettuple = newtuple = trigdata->tg_newtuple;
  oldtuple = trigdata->tg_trigtuple;

  newheader = newtuple->t_data;
  oldheader = oldtuple->t_data;

                                            
  if (newtuple->t_len == oldtuple->t_len && newheader->t_hoff == oldheader->t_hoff && (HeapTupleHeaderGetNatts(newheader) == HeapTupleHeaderGetNatts(oldheader)) && ((newheader->t_infomask & ~HEAP_XACT_MASK) == (oldheader->t_infomask & ~HEAP_XACT_MASK)) && memcmp(((char *)newheader) + SizeofHeapTupleHeader, ((char *)oldheader) + SizeofHeapTupleHeader, newtuple->t_len - SizeofHeapTupleHeader) == 0)
  {
                                      
    rettuple = NULL;
  }

  return PointerGetDatum(rettuple);
}
