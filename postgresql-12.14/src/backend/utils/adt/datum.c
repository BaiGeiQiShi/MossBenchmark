                                                                            
   
           
                                                                
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

   
                                                                    
   
                                                                     
                                                               
                                                                    
                   
   
                                                                       
                                                                    
                                                                   
   
                                                     
                                                         
                                                                          
                                                        
   
                                                     
                                                                 
   
                                                                        
                                                                         
                                                                        
                                                                           
                                                       
   

#include "postgres.h"

#include "access/tuptoaster.h"
#include "fmgr.h"
#include "utils/datum.h"
#include "utils/expandeddatum.h"

                                                                            
                
   
                                                           
                                                             
                                                                      
   
                                                                           
                                                                       
                                                                            
   
Size
datumGetSize(Datum value, bool typByVal, int typLen)
{
  Size size;

  if (typByVal)
  {
                                                     
    Assert(typLen > 0 && typLen <= sizeof(Datum));
    size = (Size)typLen;
  }
  else
  {
    if (typLen > 0)
    {
                                         
      size = (Size)typLen;
    }
    else if (typLen == -1)
    {
                                    
      struct varlena *s = (struct varlena *)DatumGetPointer(value);

      if (!PointerIsValid(s))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("invalid Datum pointer")));
      }

      size = (Size)VARSIZE_ANY(s);
    }
    else if (typLen == -2)
    {
                                    
      char *s = (char *)DatumGetPointer(value);

      if (!PointerIsValid(s))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("invalid Datum pointer")));
      }

      size = (Size)(strlen(s) + 1);
    }
    else
    {
      elog(ERROR, "invalid typLen: %d", typLen);
      size = 0;                          
    }
  }

  return size;
}

                                                                            
             
   
                                    
   
                                                                           
   
                                                                             
                                                                            
                                                                             
                                                                         
                                                                           
                                        
                                                                            
   
Datum
datumCopy(Datum value, bool typByVal, int typLen)
{
  Datum res;

  if (typByVal)
  {
    res = value;
  }
  else if (typLen == -1)
  {
                                  
    struct varlena *vl = (struct varlena *)DatumGetPointer(value);

    if (VARATT_IS_EXTERNAL_EXPANDED(vl))
    {
                                                    
      ExpandedObjectHeader *eoh = DatumGetEOHP(value);
      Size resultsize;
      char *resultptr;

      resultsize = EOH_get_flat_size(eoh);
      resultptr = (char *)palloc(resultsize);
      EOH_flatten_into(eoh, (void *)resultptr, resultsize);
      res = PointerGetDatum(resultptr);
    }
    else
    {
                                                           
      Size realSize;
      char *resultptr;

      realSize = (Size)VARSIZE_ANY(vl);
      resultptr = (char *)palloc(realSize);
      memcpy(resultptr, vl, realSize);
      res = PointerGetDatum(resultptr);
    }
  }
  else
  {
                                                            
    Size realSize;
    char *resultptr;

    realSize = datumGetSize(value, typByVal, typLen);

    resultptr = (char *)palloc(realSize);
    memcpy(resultptr, DatumGetPointer(value), realSize);
    res = PointerGetDatum(resultptr);
  }
  return res;
}

                                                                            
                 
   
                                                              
   
                                                                           
                                                                              
                                                                              
                                                          
                                                                            
   
Datum
datumTransfer(Datum value, bool typByVal, int typLen)
{
  if (!typByVal && typLen == -1 && VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(value)))
  {
    value = TransferExpandedObject(value, CurrentMemoryContext);
  }
  else
  {
    value = datumCopy(value, typByVal, typLen);
  }
  return value;
}

                                                                            
                
   
                                                        
   
              
                                                            
                                                           
                                                                
                                                                      
                                                                 
                             
   
                                                                         
                                                                            
                                      
                                                                            
   
bool
datumIsEqual(Datum value1, Datum value2, bool typByVal, int typLen)
{
  bool res;

  if (typByVal)
  {
       
                                                                          
                                                                           
                                                                         
                                                                   
       
    res = (value1 == value2);
  }
  else
  {
    Size size1, size2;
    char *s1, *s2;

       
                                                                       
       
    size1 = datumGetSize(value1, typByVal, typLen);
    size2 = datumGetSize(value2, typByVal, typLen);
    if (size1 != size2)
    {
      return false;
    }
    s1 = (char *)DatumGetPointer(value1);
    s2 = (char *)DatumGetPointer(value2);
    res = (memcmp(s1, s2, size1) == 0);
  }
  return res;
}

                                                                            
                  
   
                                                                             
                                                      
                                                                            
   
bool
datum_image_eq(Datum value1, Datum value2, bool typByVal, int typLen)
{
  bool result = true;

  if (typByVal)
  {
    result = (value1 == value2);
  }
  else if (typLen > 0)
  {
    result = (memcmp(DatumGetPointer(value1), DatumGetPointer(value2), typLen) == 0);
  }
  else if (typLen == -1)
  {
    Size len1, len2;

    len1 = toast_raw_datum_size(value1);
    len2 = toast_raw_datum_size(value2);
                                                     
    if (len1 != len2)
    {
      result = false;
    }
    else
    {
      struct varlena *arg1val;
      struct varlena *arg2val;

      arg1val = PG_DETOAST_DATUM_PACKED(value1);
      arg2val = PG_DETOAST_DATUM_PACKED(value2);

      result = (memcmp(VARDATA_ANY(arg1val), VARDATA_ANY(arg2val), len1 - VARHDRSZ) == 0);

                                                      
      if ((Pointer)arg1val != (Pointer)value1)
      {
        pfree(arg1val);
      }
      if ((Pointer)arg2val != (Pointer)value2)
      {
        pfree(arg2val);
      }
    }
  }
  else
  {
    elog(ERROR, "unexpected typLen: %d", typLen);
  }

  return result;
}

                                                                            
                      
   
                                                                      
                     
                                                                            
   
Size
datumEstimateSpace(Datum value, bool isnull, bool typByVal, int typLen)
{
  Size sz = sizeof(int);

  if (!isnull)
  {
                                                 
    if (typByVal)
    {
      sz += sizeof(Datum);
    }
    else if (typLen == -1 && VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(value)))
    {
                                                                    
      sz += EOH_get_flat_size(DatumGetEOHP(value));
    }
    else
    {
      sz += datumGetSize(value, typByVal, typLen);
    }
  }

  return sz;
}

                                                                            
                  
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                                                                          
                                                                           
                                                                      
   
                                                                         
                                                               
                                                                         
                                                                    
                                                                        
                                                                        
                                                                  
                                                             
                                                                        
                  
                                                                            
   
void
datumSerialize(Datum value, bool isnull, bool typByVal, int typLen, char **start_address)
{
  ExpandedObjectHeader *eoh = NULL;
  int header;

                          
  if (isnull)
  {
    header = -2;
  }
  else if (typByVal)
  {
    header = -1;
  }
  else if (typLen == -1 && VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(value)))
  {
    eoh = DatumGetEOHP(value);
    header = EOH_get_flat_size(eoh);
  }
  else
  {
    header = datumGetSize(value, typByVal, typLen);
  }
  memcpy(*start_address, &header, sizeof(int));
  *start_address += sizeof(int);

                                         
  if (!isnull)
  {
    if (typByVal)
    {
      memcpy(*start_address, &value, sizeof(Datum));
      *start_address += sizeof(Datum);
    }
    else if (eoh)
    {
      char *tmp;

         
                                                                       
                                                       
         
      tmp = (char *)palloc(header);
      EOH_flatten_into(eoh, (void *)tmp, header);
      memcpy(*start_address, tmp, header);
      *start_address += header;

                    
      pfree(tmp);
    }
    else
    {
      memcpy(*start_address, DatumGetPointer(value), header);
      *start_address += header;
    }
  }
}

                                                                            
                
   
                                                                          
                                                                        
                                                                            
   
Datum
datumRestore(char **start_address, bool *isnull)
{
  int header;
  void *d;

                         
  memcpy(&header, *start_address, sizeof(int));
  *start_address += sizeof(int);

                                                
  if (header == -2)
  {
    *isnull = true;
    return (Datum)0;
  }

                              
  *isnull = false;

                                                                   
  if (header == -1)
  {
    Datum val;

    memcpy(&val, *start_address, sizeof(Datum));
    *start_address += sizeof(Datum);
    return val;
  }

                                                               
  Assert(header > 0);
  d = palloc(header);
  memcpy(d, *start_address, header);
  *start_address += header;
  return PointerGetDatum(d);
}
