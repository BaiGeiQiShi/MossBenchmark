                                                                            
   
             
                           
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/skey.h"
#include "catalog/pg_collation.h"

   
                          
                                                             
                                                                 
                                                  
   
                                                                             
                                                                             
                                     
   
void
ScanKeyEntryInitialize(ScanKey entry, int flags, AttrNumber attributeNumber, StrategyNumber strategy, Oid subtype, Oid collation, RegProcedure procedure, Datum argument)
{
  entry->sk_flags = flags;
  entry->sk_attno = attributeNumber;
  entry->sk_strategy = strategy;
  entry->sk_subtype = subtype;
  entry->sk_collation = collation;
  entry->sk_argument = argument;
  if (RegProcedureIsValid(procedure))
  {
    fmgr_info(procedure, &entry->sk_func);
  }
  else
  {
    Assert(flags & (SK_SEARCHNULL | SK_SEARCHNOTNULL));
    MemSet(&entry->sk_func, 0, sizeof(entry->sk_func));
  }
}

   
               
                                                                   
                                                                          
   
                                                                             
                                                                              
                                                                  
   
                                                                          
                                                                              
                                                                        
   
                                                                             
                                                                             
                                     
   
void
ScanKeyInit(ScanKey entry, AttrNumber attributeNumber, StrategyNumber strategy, RegProcedure procedure, Datum argument)
{
  entry->sk_flags = 0;
  entry->sk_attno = attributeNumber;
  entry->sk_strategy = strategy;
  entry->sk_subtype = InvalidOid;
  entry->sk_collation = C_COLLATION_OID;
  entry->sk_argument = argument;
  fmgr_info(procedure, &entry->sk_func);
}

   
                                  
                                                                     
                            
   
                                                                             
                                                                             
                                     
   
void
ScanKeyEntryInitializeWithInfo(ScanKey entry, int flags, AttrNumber attributeNumber, StrategyNumber strategy, Oid subtype, Oid collation, FmgrInfo *finfo, Datum argument)
{
  entry->sk_flags = flags;
  entry->sk_attno = attributeNumber;
  entry->sk_strategy = strategy;
  entry->sk_subtype = subtype;
  entry->sk_collation = collation;
  entry->sk_argument = argument;
  fmgr_info_copy(&entry->sk_func, finfo, CurrentMemoryContext);
}
