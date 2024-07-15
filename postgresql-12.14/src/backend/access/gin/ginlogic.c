                                                                            
   
              
                                                                          
   
                                                                    
                                                                   
                                                                       
                               
   
                                                                      
                                                                        
                                                                          
                                                                          
                                                                           
                                                                           
                                                                      
                                       
   
                                                                        
                                                                        
                                                                         
                                                                         
                 
   
   
                                                                         
                                                                        
   
                  
                                       
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/reloptions.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"

   
                                                                       
                                        
   
#define MAX_MAYBE_ENTRIES 4

   
                                                                             
   
static bool
trueConsistentFn(GinScanKey key)
{
  key->recheckCurItem = false;
  return true;
}
static GinTernaryValue
trueTriConsistentFn(GinScanKey key)
{
  return GIN_TRUE;
}

   
                                                                               
   
static bool
directBoolConsistentFn(GinScanKey key)
{
     
                                                                        
                                                                           
     
  key->recheckCurItem = true;

  return DatumGetBool(FunctionCall8Coll(key->consistentFmgrInfo, key->collation, PointerGetDatum(key->entryRes), UInt16GetDatum(key->strategy), key->query, UInt32GetDatum(key->nuserentries), PointerGetDatum(key->extra_data), PointerGetDatum(&key->recheckCurItem), PointerGetDatum(key->queryValues), PointerGetDatum(key->queryCategories)));
}

   
                                                                             
   
static GinTernaryValue
directTriConsistentFn(GinScanKey key)
{
  return DatumGetGinTernaryValue(FunctionCall7Coll(key->triConsistentFmgrInfo, key->collation, PointerGetDatum(key->entryRes), UInt16GetDatum(key->strategy), key->query, UInt32GetDatum(key->nuserentries), PointerGetDatum(key->extra_data), PointerGetDatum(key->queryValues), PointerGetDatum(key->queryCategories)));
}

   
                                                                              
                                                                             
                                             
   
static bool
shimBoolConsistentFn(GinScanKey key)
{
  GinTernaryValue result;

  result = DatumGetGinTernaryValue(FunctionCall7Coll(key->triConsistentFmgrInfo, key->collation, PointerGetDatum(key->entryRes), UInt16GetDatum(key->strategy), key->query, UInt32GetDatum(key->nuserentries), PointerGetDatum(key->extra_data), PointerGetDatum(key->queryValues), PointerGetDatum(key->queryCategories)));
  if (result == GIN_MAYBE)
  {
    key->recheckCurItem = true;
    return true;
  }
  else
  {
    key->recheckCurItem = false;
    return result;
  }
}

   
                                                                           
                                                
   
                                                                              
                                                                               
                                                                            
                                                                               
                 
   
                                                       
   
static GinTernaryValue
shimTriConsistentFn(GinScanKey key)
{
  int nmaybe;
  int maybeEntries[MAX_MAYBE_ENTRIES];
  int i;
  bool boolResult;
  bool recheck = false;
  GinTernaryValue curResult;

     
                                                                       
                                                                            
                                                         
     
  nmaybe = 0;
  for (i = 0; i < key->nentries; i++)
  {
    if (key->entryRes[i] == GIN_MAYBE)
    {
      if (nmaybe >= MAX_MAYBE_ENTRIES)
      {
        return GIN_MAYBE;
      }
      maybeEntries[nmaybe++] = i;
    }
  }

     
                                                                      
                     
     
  if (nmaybe == 0)
  {
    return directBoolConsistentFn(key);
  }

                                                                          
  for (i = 0; i < nmaybe; i++)
  {
    key->entryRes[maybeEntries[i]] = GIN_FALSE;
  }
  curResult = directBoolConsistentFn(key);

  for (;;)
  {
                                                   
    for (i = 0; i < nmaybe; i++)
    {
      if (key->entryRes[maybeEntries[i]] == GIN_FALSE)
      {
        key->entryRes[maybeEntries[i]] = GIN_TRUE;
        break;
      }
      else
      {
        key->entryRes[maybeEntries[i]] = GIN_FALSE;
      }
    }
    if (i == nmaybe)
    {
      break;
    }

    boolResult = directBoolConsistentFn(key);
    recheck |= key->recheckCurItem;

    if (curResult != boolResult)
    {
      return GIN_MAYBE;
    }
  }

                                                
  if (curResult == GIN_TRUE && recheck)
  {
    curResult = GIN_MAYBE;
  }

  return curResult;
}

   
                                                                         
   
void
ginInitConsistentFunction(GinState *ginstate, GinScanKey key)
{
  if (key->searchMode == GIN_SEARCH_MODE_EVERYTHING)
  {
    key->boolConsistentFn = trueConsistentFn;
    key->triConsistentFn = trueTriConsistentFn;
  }
  else
  {
    key->consistentFmgrInfo = &ginstate->consistentFn[key->attnum - 1];
    key->triConsistentFmgrInfo = &ginstate->triConsistentFn[key->attnum - 1];
    key->collation = ginstate->supportCollation[key->attnum - 1];

    if (OidIsValid(ginstate->consistentFn[key->attnum - 1].fn_oid))
    {
      key->boolConsistentFn = directBoolConsistentFn;
    }
    else
    {
      key->boolConsistentFn = shimBoolConsistentFn;
    }

    if (OidIsValid(ginstate->triConsistentFn[key->attnum - 1].fn_oid))
    {
      key->triConsistentFn = directTriConsistentFn;
    }
    else
    {
      key->triConsistentFn = shimTriConsistentFn;
    }
  }
}
