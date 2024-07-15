                                                                            
   
               
                                                     
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"
#include "catalog/pg_type.h"
#include "common/config_info.h"
#include "utils/builtins.h"
#include "port.h"

Datum
pg_config(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  Tuplestorestate *tupstore;
  HeapTuple tuple;
  TupleDesc tupdesc;
  AttInMetadata *attinmeta;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  ConfigData *configdata;
  size_t configdata_len;
  char *values[2];
  int i = 0;

                                                                 
  if (!rsinfo || !(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("materialize mode required, but it is not "
                                                          "allowed in this context")));
  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

                                                  
  tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);

     
                                                              
     
  if (tupdesc->natts != 2 || TupleDescAttr(tupdesc, 0)->atttypid != TEXTOID || TupleDescAttr(tupdesc, 1)->atttypid != TEXTOID)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("query-specified return tuple and "
                                                          "function return type are not compatible")));
  }

                    
  attinmeta = TupleDescGetAttInMetadata(tupdesc);

                                                           
  rsinfo->returnMode = SFRM_Materialize;

                                 
  tupstore = tuplestore_begin_heap(true, false, work_mem);

  configdata = get_configdata(my_exec_path, &configdata_len);
  for (i = 0; i < configdata_len; i++)
  {
    values[0] = configdata[i].name;
    values[1] = configdata[i].setting;

    tuple = BuildTupleFromCStrings(attinmeta, values);
    tuplestore_puttuple(tupstore, tuple);
  }

     
                                                              
                                 
     
  ReleaseTupleDesc(tupdesc);

  tuplestore_donestoring(tupstore);
  rsinfo->setResult = tupstore;

     
                                                                         
                                                                             
                                                                           
                                                                           
                
     
  rsinfo->setDesc = tupdesc;
  MemoryContextSwitchTo(oldcontext);

  return (Datum)0;
}
