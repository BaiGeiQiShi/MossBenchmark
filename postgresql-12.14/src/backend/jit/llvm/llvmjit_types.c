                                                                            
   
                   
                                                
   
                                                                             
                                                                             
                                                                            
                                                          
   
                                                                             
                                                                           
                           
   
                                                                            
            
   
   
                                                                
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_attribute.h"
#include "executor/execExpr.h"
#include "executor/nodeAgg.h"
#include "executor/tuptable.h"
#include "fmgr.h"
#include "nodes/execnodes.h"
#include "nodes/memnodes.h"
#include "utils/expandeddatum.h"
#include "utils/palloc.h"

   
                                                                           
                                                                      
                              
   
PGFunction TypePGFunction;
size_t TypeSizeT;
bool TypeStorageBool;

NullableDatum StructNullableDatum;
AggState StructAggState;
AggStatePerGroupData StructAggStatePerGroupData;
AggStatePerTransData StructAggStatePerTransData;
ExprContext StructExprContext;
ExprEvalStep StructExprEvalStep;
ExprState StructExprState;
FunctionCallInfoBaseData StructFunctionCallInfoData;
HeapTupleData StructHeapTupleData;
MemoryContextData StructMemoryContextData;
TupleTableSlot StructTupleTableSlot;
HeapTupleTableSlot StructHeapTupleTableSlot;
MinimalTupleTableSlot StructMinimalTupleTableSlot;
TupleDescData StructTupleDescData;

   
                                                                         
                                                                           
                                         
   
extern Datum AttributeTemplate(PG_FUNCTION_ARGS);
Datum
AttributeTemplate(PG_FUNCTION_ARGS)
{
  PG_RETURN_NULL();
}

   
                                                                            
                                                                               
                                                                         
                                                                     
                  
   
extern bool
FunctionReturningBool(void);
bool
FunctionReturningBool(void)
{
  return false;
}

   
                                                                      
                                                                               
                                 
   
void *referenced_functions[] = {strlen, varsize_any, slot_getsomeattrs_int, slot_getmissingattrs, MakeExpandedObjectReadOnlyInternal, ExecEvalSubscriptingRef, ExecEvalSysVar, ExecAggTransReparent, ExecAggInitGroup};
