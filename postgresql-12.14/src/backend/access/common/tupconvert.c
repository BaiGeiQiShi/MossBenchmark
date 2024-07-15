                                                                            
   
                
                               
   
                                                                          
                                                                               
                    
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupconvert.h"
#include "executor/tuptable.h"
#include "utils/builtins.h"

   
                                                                
   
                                                                           
                                                                      
                                                                               
                                                                                    
                              
   
                                                                         
                                                                          
                                                      
   
                                                                         
                                                                           
                                                                       
                                                        
   
   
                         
   
                                                                        
                                                                          
                                                                      
                                                                             
           
   

   
                                                                     
                                                                      
   
                                                                           
                                                                         
                                        
   
TupleConversionMap *
convert_tuples_by_position(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{
  TupleConversionMap *map;
  AttrNumber *attrMap;
  int nincols;
  int noutcols;
  int n;
  int i;
  int j;
  bool same;

                                                             
  n = outdesc->natts;
  attrMap = (AttrNumber *)palloc0(n * sizeof(AttrNumber));
  j = 0;                                                          
  nincols = noutcols = 0;                                         
  same = true;
  for (i = 0; i < n; i++)
  {
    Form_pg_attribute att = TupleDescAttr(outdesc, i);
    Oid atttypid;
    int32 atttypmod;

    if (att->attisdropped)
    {
      continue;                              
    }
    noutcols++;
    atttypid = att->atttypid;
    atttypmod = att->atttypmod;
    for (; j < indesc->natts; j++)
    {
      att = TupleDescAttr(indesc, j);
      if (att->attisdropped)
      {
        continue;
      }
      nincols++;
                                             
      if (atttypid != att->atttypid || (atttypmod != att->atttypmod && atttypmod >= 0))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg_internal("%s", _(msg)), errdetail("Returned type %s does not match expected type %s in column %d.", format_type_with_typemod(att->atttypid, att->atttypmod), format_type_with_typemod(atttypid, atttypmod), noutcols)));
      }
      attrMap[i] = (AttrNumber)(j + 1);
      j++;
      break;
    }
    if (attrMap[i] == 0)
    {
      same = false;                           
    }
  }

                                      
  for (; j < indesc->natts; j++)
  {
    if (TupleDescAttr(indesc, j)->attisdropped)
    {
      continue;
    }
    nincols++;
    same = false;                           
  }

                                                                        
  if (!same)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg_internal("%s", _(msg)),
                       errdetail("Number of returned columns (%d) does not match "
                                 "expected column count (%d).",
                           nincols, noutcols)));
  }

     
                                                                           
                       
     
  if (indesc->natts == outdesc->natts)
  {
    for (i = 0; i < n; i++)
    {
      Form_pg_attribute inatt = TupleDescAttr(indesc, i);
      Form_pg_attribute outatt = TupleDescAttr(outdesc, i);

         
                                                                
                     
         
      if (inatt->atthasmissing)
      {
        same = false;
        break;
      }

      if (attrMap[i] == (i + 1))
      {
        continue;
      }

         
                                                                        
                                                                         
                     
         
      if (attrMap[i] == 0 && inatt->attisdropped && inatt->attlen == outatt->attlen && inatt->attalign == outatt->attalign)
      {
        continue;
      }

      same = false;
      break;
    }
  }
  else
  {
    same = false;
  }

  if (same)
  {
                                          
    pfree(attrMap);
    return NULL;
  }

                                 
  map = (TupleConversionMap *)palloc(sizeof(TupleConversionMap));
  map->indesc = indesc;
  map->outdesc = outdesc;
  map->attrMap = attrMap;
                                              
  map->outvalues = (Datum *)palloc(n * sizeof(Datum));
  map->outisnull = (bool *)palloc(n * sizeof(bool));
  n = indesc->natts + 1;                  
  map->invalues = (Datum *)palloc(n * sizeof(Datum));
  map->inisnull = (bool *)palloc(n * sizeof(bool));
  map->invalues[0] = (Datum)0;                            
  map->inisnull[0] = true;

  return map;
}

   
                                                                           
                                                                            
                                                                               
                                                                              
                                                   
   
TupleConversionMap *
convert_tuples_by_name(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{
  TupleConversionMap *map;
  AttrNumber *attrMap;
  int n = outdesc->natts;

                                                             
  attrMap = convert_tuples_by_name_map_if_req(indesc, outdesc, msg);

  if (attrMap == NULL)
  {
                                          
    return NULL;
  }

                                 
  map = (TupleConversionMap *)palloc(sizeof(TupleConversionMap));
  map->indesc = indesc;
  map->outdesc = outdesc;
  map->attrMap = attrMap;
                                              
  map->outvalues = (Datum *)palloc(n * sizeof(Datum));
  map->outisnull = (bool *)palloc(n * sizeof(bool));
  n = indesc->natts + 1;                  
  map->invalues = (Datum *)palloc(n * sizeof(Datum));
  map->inisnull = (bool *)palloc(n * sizeof(bool));
  map->invalues[0] = (Datum)0;                            
  map->inisnull[0] = true;

  return map;
}

   
                                                                             
                                                                               
                                                                               
                       
   
AttrNumber *
convert_tuples_by_name_map(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{
  AttrNumber *attrMap;
  int outnatts;
  int innatts;
  int i;
  int nextindesc = -1;

  outnatts = outdesc->natts;
  innatts = indesc->natts;

  attrMap = (AttrNumber *)palloc0(outnatts * sizeof(AttrNumber));
  for (i = 0; i < outnatts; i++)
  {
    Form_pg_attribute outatt = TupleDescAttr(outdesc, i);
    char *attname;
    Oid atttypid;
    int32 atttypmod;
    int j;

    if (outatt->attisdropped)
    {
      continue;                              
    }
    attname = NameStr(outatt->attname);
    atttypid = outatt->atttypid;
    atttypmod = outatt->atttypmod;

       
                                                                        
                                                                         
                                                                         
                                                                         
                                                                    
                                                                        
                                                                      
                                                                          
                            
       
    for (j = 0; j < innatts; j++)
    {
      Form_pg_attribute inatt;

      nextindesc++;
      if (nextindesc >= innatts)
      {
        nextindesc = 0;
      }

      inatt = TupleDescAttr(indesc, nextindesc);
      if (inatt->attisdropped)
      {
        continue;
      }
      if (strcmp(attname, NameStr(inatt->attname)) == 0)
      {
                                  
        if (atttypid != inatt->atttypid || atttypmod != inatt->atttypmod)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg_internal("%s", _(msg)), errdetail("Attribute \"%s\" of type %s does not match corresponding attribute of type %s.", attname, format_type_be(outdesc->tdtypeid), format_type_be(indesc->tdtypeid))));
        }
        attrMap[i] = inatt->attnum;
        break;
      }
    }
    if (attrMap[i] == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg_internal("%s", _(msg)), errdetail("Attribute \"%s\" of type %s does not exist in type %s.", attname, format_type_be(outdesc->tdtypeid), format_type_be(indesc->tdtypeid))));
    }
  }
  return attrMap;
}

   
                                                                        
                                                              
                                                 
   
AttrNumber *
convert_tuples_by_name_map_if_req(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{
  AttrNumber *attrMap;
  int n = outdesc->natts;
  int i;
  bool same;

                                                             
  attrMap = convert_tuples_by_name_map(indesc, outdesc, msg);

     
                                                                           
                       
     
  if (indesc->natts == outdesc->natts)
  {
    same = true;
    for (i = 0; i < n; i++)
    {
      Form_pg_attribute inatt = TupleDescAttr(indesc, i);
      Form_pg_attribute outatt = TupleDescAttr(outdesc, i);

         
                                                                
                     
         
      if (inatt->atthasmissing)
      {
        same = false;
        break;
      }

      if (attrMap[i] == (i + 1))
      {
        continue;
      }

         
                                                                        
                                                                         
                     
         
      if (attrMap[i] == 0 && inatt->attisdropped && inatt->attlen == outatt->attlen && inatt->attalign == outatt->attalign)
      {
        continue;
      }

      same = false;
      break;
    }
  }
  else
  {
    same = false;
  }

  if (same)
  {
                                          
    pfree(attrMap);
    return NULL;
  }
  else
  {
    return attrMap;
  }
}

   
                                                       
   
HeapTuple
execute_attr_map_tuple(HeapTuple tuple, TupleConversionMap *map)
{
  AttrNumber *attrMap = map->attrMap;
  Datum *invalues = map->invalues;
  bool *inisnull = map->inisnull;
  Datum *outvalues = map->outvalues;
  bool *outisnull = map->outisnull;
  int outnatts = map->outdesc->natts;
  int i;

     
                                                                            
                                                                             
                                                               
     
  heap_deform_tuple(tuple, map->indesc, invalues + 1, inisnull + 1);

     
                                                    
     
  for (i = 0; i < outnatts; i++)
  {
    int j = attrMap[i];

    outvalues[i] = invalues[j];
    outisnull[i] = inisnull[j];
  }

     
                             
     
  return heap_form_tuple(map->outdesc, outvalues, outisnull);
}

   
                                                            
   
TupleTableSlot *
execute_attr_map_slot(AttrNumber *attrMap, TupleTableSlot *in_slot, TupleTableSlot *out_slot)
{
  Datum *invalues;
  bool *inisnull;
  Datum *outvalues;
  bool *outisnull;
  int outnatts;
  int i;

                     
  Assert(in_slot->tts_tupleDescriptor != NULL && out_slot->tts_tupleDescriptor != NULL);
  Assert(in_slot->tts_values != NULL && out_slot->tts_values != NULL);

  outnatts = out_slot->tts_tupleDescriptor->natts;

                                              
  slot_getallattrs(in_slot);

                                                                          
  ExecClearTuple(out_slot);

  invalues = in_slot->tts_values;
  inisnull = in_slot->tts_isnull;
  outvalues = out_slot->tts_values;
  outisnull = out_slot->tts_isnull;

                                                     
  for (i = 0; i < outnatts; i++)
  {
    int j = attrMap[i] - 1;

                                                  
    if (j == -1)
    {
      outvalues[i] = (Datum)0;
      outisnull[i] = true;
    }
    else
    {
      outvalues[i] = invalues[j];
      outisnull[i] = inisnull[j];
    }
  }

  ExecStoreVirtualTuple(out_slot);

  return out_slot;
}

   
                                                                 
   
                                              
                                                                           
                                    
   
Bitmapset *
execute_attr_map_cols(Bitmapset *in_cols, TupleConversionMap *map)
{
  AttrNumber *attrMap = map->attrMap;
  int maplen = map->outdesc->natts;
  Bitmapset *out_cols;
  int out_attnum;

                                             
  if (in_cols == NULL)
  {
    return NULL;
  }

     
                                                                         
     
  out_cols = NULL;

  for (out_attnum = FirstLowInvalidHeapAttributeNumber + 1; out_attnum <= maplen; out_attnum++)
  {
    int in_attnum;

    if (out_attnum < 0)
    {
                                      
      in_attnum = out_attnum;
    }
    else if (out_attnum == 0)
    {
      continue;
    }
    else
    {
                              
      in_attnum = attrMap[out_attnum - 1];

      if (in_attnum == 0)
      {
        continue;
      }
    }

    if (bms_is_member(in_attnum - FirstLowInvalidHeapAttributeNumber, in_cols))
    {
      out_cols = bms_add_member(out_cols, out_attnum - FirstLowInvalidHeapAttributeNumber);
    }
  }

  return out_cols;
}

   
                                        
   
void
free_conversion_map(TupleConversionMap *map)
{
                                               
  pfree(map->attrMap);
  pfree(map->invalues);
  pfree(map->inisnull);
  pfree(map->outvalues);
  pfree(map->outisnull);
  pfree(map);
}
