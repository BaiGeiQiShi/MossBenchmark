                                                                            
   
                
                                                                    
                                          
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "access/itup.h"
#include "access/tuptoaster.h"

                                                                    
                                        
                                                                    
   

                    
                     
   
                                                               
                                                          
   
                                                                 
                                                   
                    
   
IndexTuple
index_form_tuple(TupleDesc tupleDescriptor, Datum *values, bool *isnull)
{
  char *tp;                            
  IndexTuple tuple;                   
  Size size, data_size, hoff;
  int i;
  unsigned short infomask = 0;
  bool hasnull = false;
  uint16 tupmask = 0;
  int numberOfAttributes = tupleDescriptor->natts;

#ifdef TOAST_INDEX_HACK
  Datum untoasted_values[INDEX_MAX_KEYS];
  bool untoasted_free[INDEX_MAX_KEYS];
#endif

  if (numberOfAttributes > INDEX_MAX_KEYS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("number of index columns (%d) exceeds limit (%d)", numberOfAttributes, INDEX_MAX_KEYS)));
  }

#ifdef TOAST_INDEX_HACK
  for (i = 0; i < numberOfAttributes; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupleDescriptor, i);

    untoasted_values[i] = values[i];
    untoasted_free[i] = false;

                                                            
    if (isnull[i] || att->attlen != -1)
    {
      continue;
    }

       
                                                                          
                                                             
       
    if (VARATT_IS_EXTERNAL(DatumGetPointer(values[i])))
    {
      untoasted_values[i] = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(values[i])));
      untoasted_free[i] = true;
    }

       
                                                                         
                                   
       
    if (!VARATT_IS_EXTENDED(DatumGetPointer(untoasted_values[i])) && VARSIZE(DatumGetPointer(untoasted_values[i])) > TOAST_INDEX_TARGET && (att->attstorage == 'x' || att->attstorage == 'm'))
    {
      Datum cvalue = toast_compress_datum(untoasted_values[i]);

      if (DatumGetPointer(cvalue) != NULL)
      {
                                    
        if (untoasted_free[i])
        {
          pfree(DatumGetPointer(untoasted_values[i]));
        }
        untoasted_values[i] = cvalue;
        untoasted_free[i] = true;
      }
    }
  }
#endif

  for (i = 0; i < numberOfAttributes; i++)
  {
    if (isnull[i])
    {
      hasnull = true;
      break;
    }
  }

  if (hasnull)
  {
    infomask |= INDEX_NULL_MASK;
  }

  hoff = IndexInfoFindDataOffset(infomask);
#ifdef TOAST_INDEX_HACK
  data_size = heap_compute_data_size(tupleDescriptor, untoasted_values, isnull);
#else
  data_size = heap_compute_data_size(tupleDescriptor, values, isnull);
#endif
  size = hoff + data_size;
  size = MAXALIGN(size);                      

  tp = (char *)palloc0(size);
  tuple = (IndexTuple)tp;

  heap_fill_tuple(tupleDescriptor,
#ifdef TOAST_INDEX_HACK
      untoasted_values,
#else
      values,
#endif
      isnull, (char *)tp + hoff, data_size, &tupmask, (hasnull ? (bits8 *)tp + sizeof(IndexTupleData) : NULL));

#ifdef TOAST_INDEX_HACK
  for (i = 0; i < numberOfAttributes; i++)
  {
    if (untoasted_free[i])
    {
      pfree(DatumGetPointer(untoasted_values[i]));
    }
  }
#endif

     
                                                                        
                                                                           
                                                                        
                                        
     
  if (tupmask & HEAP_HASVARWIDTH)
  {
    infomask |= INDEX_VAR_MASK;
  }

                                                     
#ifdef TOAST_INDEX_HACK
  Assert((tupmask & HEAP_HASEXTERNAL) == 0);
#endif

     
                                                                           
                
     
  if ((size & INDEX_SIZE_MASK) != size)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row requires %zu bytes, maximum size is %zu", size, (Size)INDEX_SIZE_MASK)));
  }

  infomask |= size;

     
                         
     
  tuple->t_info = infomask;
  return tuple;
}

                    
                          
   
                                                                   
                                                              
   
                                                               
   
                                                                    
                                                                  
                                                               
                                   
   
                                                              
                                                                   
                                                                     
                                                                    
                    
   
Datum
nocache_index_getattr(IndexTuple tup, int attnum, TupleDesc tupleDesc)
{
  char *tp;                                         
  bits8 *bp = NULL;                                   
  bool slow = false;                                
  int data_off;                             
  int off;                                           

                      
                   
     
                                                    
                                              
                                             
                      
     

  data_off = IndexInfoFindDataOffset(tup->t_info);

  attnum--;

  if (IndexTupleHasNulls(tup))
  {
       
                                             
       
                                           
       

                                                               
    bp = (bits8 *)((char *)tup + sizeof(IndexTupleData));

       
                                                          
       
    {
      int byte = attnum >> 3;
      int finalbit = attnum & 0x07;

                                                           
      if ((~bp[byte]) & ((1 << finalbit) - 1))
      {
        slow = true;
      }
      else
      {
                                                    
        int i;

        for (i = 0; i < byte; i++)
        {
          if (bp[i] != 0xFF)
          {
            slow = true;
            break;
          }
        }
      }
    }
  }

  tp = (char *)tup + data_off;

  if (!slow)
  {
    Form_pg_attribute att;

       
                                                                         
                                                              
       
    att = TupleDescAttr(tupleDesc, attnum);
    if (att->attcacheoff >= 0)
    {
      return fetchatt(att, tp + att->attcacheoff);
    }

       
                                                                       
                                                                         
                                       
       
    if (IndexTupleHasVarwidths(tup))
    {
      int j;

      for (j = 0; j <= attnum; j++)
      {
        if (TupleDescAttr(tupleDesc, j)->attlen <= 0)
        {
          slow = true;
          break;
        }
      }
    }
  }

  if (!slow)
  {
    int natts = tupleDesc->natts;
    int j = 1;

       
                                                                         
                                                                           
                                                                        
                                                                          
                                                                          
                                                                      
                
       
    TupleDescAttr(tupleDesc, 0)->attcacheoff = 0;

                                                                    
    while (j < natts && TupleDescAttr(tupleDesc, j)->attcacheoff > 0)
    {
      j++;
    }

    off = TupleDescAttr(tupleDesc, j - 1)->attcacheoff + TupleDescAttr(tupleDesc, j - 1)->attlen;

    for (; j < natts; j++)
    {
      Form_pg_attribute att = TupleDescAttr(tupleDesc, j);

      if (att->attlen <= 0)
      {
        break;
      }

      off = att_align_nominal(off, att->attalign);

      att->attcacheoff = off;

      off += att->attlen;
    }

    Assert(j > attnum);

    off = TupleDescAttr(tupleDesc, attnum)->attcacheoff;
  }
  else
  {
    bool usecache = true;
    int i;

       
                                                                           
                                                          
       
                                                                          
                                                                       
                                                                      
                                                                
                                                                          
       
    off = 0;
    for (i = 0;; i++)                              
    {
      Form_pg_attribute att = TupleDescAttr(tupleDesc, i);

      if (IndexTupleHasNulls(tup) && att_isnull(i, bp))
      {
        usecache = false;
        continue;                                    
      }

                                                            
      if (usecache && att->attcacheoff >= 0)
      {
        off = att->attcacheoff;
      }
      else if (att->attlen == -1)
      {
           
                                                                       
                                                                      
                                                                       
                                                 
           
        if (usecache && off == att_align_nominal(off, att->attalign))
        {
          att->attcacheoff = off;
        }
        else
        {
          off = att_align_pointer(off, att->attalign, -1, tp + off);
          usecache = false;
        }
      }
      else
      {
                                                           
        off = att_align_nominal(off, att->attalign);

        if (usecache)
        {
          att->attcacheoff = off;
        }
      }

      if (i == attnum)
      {
        break;
      }

      off = att_addlength_pointer(off, att->attlen, tp + off);

      if (usecache && att->attlen <= 0)
      {
        usecache = false;
      }
    }
  }

  return fetchatt(TupleDescAttr(tupleDesc, attnum), tp + off);
}

   
                                                    
   
                                                                      
                                              
   
                                                                        
                                                                           
   
void
index_deform_tuple(IndexTuple tup, TupleDesc tupleDescriptor, Datum *values, bool *isnull)
{
  int hasnulls = IndexTupleHasNulls(tup);
  int natts = tupleDescriptor->natts;                                
  int attnum;
  char *tp;                                 
  int off;                                     
  bits8 *bp;                                          
  bool slow = false;                                  

                                                                
  Assert(natts <= INDEX_MAX_KEYS);

                                                             
  bp = (bits8 *)((char *)tup + sizeof(IndexTupleData));

  tp = (char *)tup + IndexInfoFindDataOffset(tup->t_info);
  off = 0;

  for (attnum = 0; attnum < natts; attnum++)
  {
    Form_pg_attribute thisatt = TupleDescAttr(tupleDescriptor, attnum);

    if (hasnulls && att_isnull(attnum, bp))
    {
      values[attnum] = (Datum)0;
      isnull[attnum] = true;
      slow = true;                                    
      continue;
    }

    isnull[attnum] = false;

    if (!slow && thisatt->attcacheoff >= 0)
    {
      off = thisatt->attcacheoff;
    }
    else if (thisatt->attlen == -1)
    {
         
                                                                     
                                                                       
                                                                         
                                        
         
      if (!slow && off == att_align_nominal(off, thisatt->attalign))
      {
        thisatt->attcacheoff = off;
      }
      else
      {
        off = att_align_pointer(off, thisatt->attalign, -1, tp + off);
        slow = true;
      }
    }
    else
    {
                                                         
      off = att_align_nominal(off, thisatt->attalign);

      if (!slow)
      {
        thisatt->attcacheoff = off;
      }
    }

    values[attnum] = fetchatt(thisatt, tp + off);

    off = att_addlength_pointer(off, thisatt->attlen, tp + off);

    if (thisatt->attlen <= 0)
    {
      slow = true;                                    
    }
  }
}

   
                                             
   
IndexTuple
CopyIndexTuple(IndexTuple source)
{
  IndexTuple result;
  Size size;

  size = IndexTupleSize(source);
  result = (IndexTuple)palloc(size);
  memcpy(result, source, size);
  return result;
}

   
                                                                    
                                    
   
                                                                   
                                                                    
                                                                 
                                                                    
                                                                     
                                                                     
                                  
   
                                                                     
                                                                     
                                                                      
                         
   
IndexTuple
index_truncate_tuple(TupleDesc sourceDescriptor, IndexTuple source, int leavenatts)
{
  TupleDesc truncdesc;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  IndexTuple truncated;

  Assert(leavenatts <= sourceDescriptor->natts);

                                                  
  if (leavenatts == sourceDescriptor->natts)
  {
    return CopyIndexTuple(source);
  }

                                                  
  truncdesc = palloc(TupleDescSize(sourceDescriptor));
  TupleDescCopy(truncdesc, sourceDescriptor);
  truncdesc->natts = leavenatts;

                                                        
  index_deform_tuple(source, truncdesc, values, isnull);
  truncated = index_form_tuple(truncdesc, values, isnull);
  truncated->t_tid = source->t_tid;
  Assert(IndexTupleSize(truncated) <= IndexTupleSize(source));

     
                                                                         
                                                                    
     
  pfree(truncdesc);

  return truncated;
}
