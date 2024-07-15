                                                                            
   
              
                                      
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "executor/executor.h"

                                                                            
                                                         
                                                   
                 
   
                                                              
                                                                      
                                                                   
                                                                
                                                                         
                                                                         
              
   
                                                                          
                                                                          
                                                                 
                                                  
   
                                                                            
   
                                                                              
   
                                                                   
                                                                            
                                                                              
                                                                          
                         
   
                                                                            
   

   
                      
   
                               
   
                                                                       
                                          
                                                                           
   
JunkFilter *
ExecInitJunkFilter(List *targetList, TupleTableSlot *slot)
{
  TupleDesc cleanTupType;

     
                                                         
     
  cleanTupType = ExecCleanTypeFromTL(targetList);

     
                                                                             
                                                             
     
  return ExecInitJunkFilterInsertion(targetList, cleanTupType, slot);
}

   
                               
   
                                                        
   
                                                                      
                                                                         
                                                                              
                                                                          
                                                                               
                                                
   
JunkFilter *
ExecInitJunkFilterInsertion(List *targetList, TupleDesc cleanTupType, TupleTableSlot *slot)
{
  JunkFilter *junkfilter;
  int cleanLength;
  AttrNumber *cleanMap;
  ListCell *t;
  AttrNumber cleanResno;

     
                                                                     
     
  if (slot)
  {
    ExecSetSlotDescriptor(slot, cleanTupType);
  }
  else
  {
    slot = MakeSingleTupleTableSlot(cleanTupType, &TTSOpsVirtual);
  }

     
                                                                           
                                     
     
                                                                        
                                                                             
                                                                   
                                                                           
                                            
     
  cleanLength = cleanTupType->natts;
  if (cleanLength > 0)
  {
    cleanMap = (AttrNumber *)palloc(cleanLength * sizeof(AttrNumber));
    cleanResno = 0;
    foreach (t, targetList)
    {
      TargetEntry *tle = lfirst(t);

      if (!tle->resjunk)
      {
        cleanMap[cleanResno] = tle->resno;
        cleanResno++;
      }
    }
    Assert(cleanResno == cleanLength);
  }
  else
  {
    cleanMap = NULL;
  }

     
                                                          
     
  junkfilter = makeNode(JunkFilter);

  junkfilter->jf_targetList = targetList;
  junkfilter->jf_cleanTupType = cleanTupType;
  junkfilter->jf_cleanMap = cleanMap;
  junkfilter->jf_resultSlot = slot;

  return junkfilter;
}

   
                                
   
                                                    
   
                                                                      
                                                                        
                                                                        
                                                                             
   
JunkFilter *
ExecInitJunkFilterConversion(List *targetList, TupleDesc cleanTupType, TupleTableSlot *slot)
{
  JunkFilter *junkfilter;
  int cleanLength;
  AttrNumber *cleanMap;
  ListCell *t;
  int i;

     
                                                                     
     
  if (slot)
  {
    ExecSetSlotDescriptor(slot, cleanTupType);
  }
  else
  {
    slot = MakeSingleTupleTableSlot(cleanTupType, &TTSOpsVirtual);
  }

     
                                                                           
                                 
     
                                                                        
                                                                             
                                                                   
                                                                          
                                                
     
  cleanLength = cleanTupType->natts;
  if (cleanLength > 0)
  {
    cleanMap = (AttrNumber *)palloc0(cleanLength * sizeof(AttrNumber));
    t = list_head(targetList);
    for (i = 0; i < cleanLength; i++)
    {
      if (TupleDescAttr(cleanTupType, i)->attisdropped)
      {
        continue;                                
      }
      for (;;)
      {
        TargetEntry *tle = lfirst(t);

        t = lnext(t);
        if (!tle->resjunk)
        {
          cleanMap[i] = tle->resno;
          break;
        }
      }
    }
  }
  else
  {
    cleanMap = NULL;
  }

     
                                                          
     
  junkfilter = makeNode(JunkFilter);

  junkfilter->jf_targetList = targetList;
  junkfilter->jf_cleanTupType = cleanTupType;
  junkfilter->jf_cleanMap = cleanMap;
  junkfilter->jf_resultSlot = slot;

  return junkfilter;
}

   
                         
   
                                                                        
                                                                  
   
AttrNumber
ExecFindJunkAttribute(JunkFilter *junkfilter, const char *attrName)
{
  return ExecFindJunkAttributeInTlist(junkfilter->jf_targetList, attrName);
}

   
                                
   
                                                                       
                          
   
AttrNumber
ExecFindJunkAttributeInTlist(List *targetlist, const char *attrName)
{
  ListCell *t;

  foreach (t, targetlist)
  {
    TargetEntry *tle = lfirst(t);

    if (tle->resjunk && tle->resname && (strcmp(tle->resname, attrName) == 0))
    {
                         
      return tle->resno;
    }
  }

  return InvalidAttrNumber;
}

   
                        
   
                                                                          
                                                                             
                                 
   
Datum
ExecGetJunkAttribute(TupleTableSlot *slot, AttrNumber attno, bool *isNull)
{
  Assert(attno > 0);

  return slot_getattr(slot, attno, isNull);
}

   
                  
   
                                                                     
   
TupleTableSlot *
ExecFilterJunk(JunkFilter *junkfilter, TupleTableSlot *slot)
{
  TupleTableSlot *resultSlot;
  AttrNumber *cleanMap;
  TupleDesc cleanTupType;
  int cleanLength;
  int i;
  Datum *values;
  bool *isnull;
  Datum *old_values;
  bool *old_isnull;

     
                                              
     
  slot_getallattrs(slot);
  old_values = slot->tts_values;
  old_isnull = slot->tts_isnull;

     
                                   
     
  cleanTupType = junkfilter->jf_cleanTupType;
  cleanLength = cleanTupType->natts;
  cleanMap = junkfilter->jf_cleanMap;
  resultSlot = junkfilter->jf_resultSlot;

     
                                              
     
  ExecClearTuple(resultSlot);
  values = resultSlot->tts_values;
  isnull = resultSlot->tts_isnull;

     
                                                         
     
  for (i = 0; i < cleanLength; i++)
  {
    int j = cleanMap[i];

    if (j == 0)
    {
      values[i] = (Datum)0;
      isnull[i] = true;
    }
    else
    {
      values[i] = old_values[j - 1];
      isnull[i] = old_isnull[j - 1];
    }
  }

     
                                   
     
  return ExecStoreVirtualTuple(resultSlot);
}
