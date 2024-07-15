                                                                            
   
               
                                                                          
                                 
   
                                            
   
                                                                       
                                                                           
                                                                             
                                             
   
                                                                             
                                                                              
                                                                             
                                                                            
                                                                          
                                                                              
                                                                         
                 
   
                                                                          
                                                                            
                        
   
                                                                             
                                                                               
                                                                            
                                                                               
                                                                            
                                                                             
                                                                               
                                                                             
                                     
   
                                                                           
                                                                        
                                                                        
                                                          
   
                                                                       
                                                                          
                                                                      
                                                              
   
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/sysattr.h"
#include "access/tupdesc_details.h"
#include "access/tuptoaster.h"
#include "executor/tuptable.h"
#include "utils/expandeddatum.h"

                                                                              
#define ATT_IS_PACKABLE(att) ((att)->attlen == -1 && (att)->attstorage != 'p')
                                            
#define VARLENA_ATT_IS_PACKABLE(att) ((att)->attstorage != 'p')

                                                                    
                              
                                                                    
   

   
                                                                         
   
Datum
getmissingattr(TupleDesc tupleDesc, int attnum, bool *isnull)
{
  Form_pg_attribute att;

  Assert(attnum <= tupleDesc->natts);
  Assert(attnum > 0);

  att = TupleDescAttr(tupleDesc, attnum - 1);

  if (att->atthasmissing)
  {
    AttrMissing *attrmiss;

    Assert(tupleDesc->constr);
    Assert(tupleDesc->constr->missing);

    attrmiss = tupleDesc->constr->missing + (attnum - 1);

    if (attrmiss->am_present)
    {
      *isnull = false;
      return attrmiss->am_value;
    }
  }

  *isnull = true;
  return PointerGetDatum(NULL);
}

   
                          
                                                                 
   
Size
heap_compute_data_size(TupleDesc tupleDesc, Datum *values, bool *isnull)
{
  Size data_length = 0;
  int i;
  int numberOfAttributes = tupleDesc->natts;

  for (i = 0; i < numberOfAttributes; i++)
  {
    Datum val;
    Form_pg_attribute atti;

    if (isnull[i])
    {
      continue;
    }

    val = values[i];
    atti = TupleDescAttr(tupleDesc, i);

    if (ATT_IS_PACKABLE(atti) && VARATT_CAN_MAKE_SHORT(DatumGetPointer(val)))
    {
         
                                                                     
                                                     
         
      data_length += VARATT_CONVERTED_SHORT_SIZE(DatumGetPointer(val));
    }
    else if (atti->attlen == -1 && VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(val)))
    {
         
                                                                       
                                    
         
      data_length = att_align_nominal(data_length, atti->attalign);
      data_length += EOH_get_flat_size(DatumGetEOHP(val));
    }
    else
    {
      data_length = att_align_datum(data_length, atti->attalign, atti->attlen, val);
      data_length = att_addlength_datum(data_length, atti->attlen, val);
    }
  }

  return data_length;
}

   
                                                                                
   
                                                            
   
static inline void
fill_val(Form_pg_attribute att, bits8 **bit, int *bitmask, char **dataP, uint16 *infomask, Datum datum, bool isnull)
{
  Size data_length;
  char *data = *dataP;

     
                                                                      
                                
     
  if (bit != NULL)
  {
    if (*bitmask != HIGHBIT)
    {
      *bitmask <<= 1;
    }
    else
    {
      *bit += 1;
      **bit = 0x0;
      *bitmask = 1;
    }

    if (isnull)
    {
      *infomask |= HEAP_HASNULL;
      return;
    }

    **bit |= *bitmask;
  }

     
                                                                            
                                       
     
  if (att->attbyval)
  {
                       
    data = (char *)att_align_nominal(data, att->attalign);
    store_att_byval(data, datum, att->attlen);
    data_length = att->attlen;
  }
  else if (att->attlen == -1)
  {
                 
    Pointer val = DatumGetPointer(datum);

    *infomask |= HEAP_HASVARWIDTH;
    if (VARATT_IS_EXTERNAL(val))
    {
      if (VARATT_IS_EXTERNAL_EXPANDED(val))
      {
           
                                                             
                                                  
           
        ExpandedObjectHeader *eoh = DatumGetEOHP(datum);

        data = (char *)att_align_nominal(data, att->attalign);
        data_length = EOH_get_flat_size(eoh);
        EOH_flatten_into(eoh, data, data_length);
      }
      else
      {
        *infomask |= HEAP_HASEXTERNAL;
                                                          
        data_length = VARSIZE_EXTERNAL(val);
        memcpy(data, val, data_length);
      }
    }
    else if (VARATT_IS_SHORT(val))
    {
                                           
      data_length = VARSIZE_SHORT(val);
      memcpy(data, val, data_length);
    }
    else if (VARLENA_ATT_IS_PACKABLE(att) && VARATT_CAN_MAKE_SHORT(val))
    {
                                                    
      data_length = VARATT_CONVERTED_SHORT_SIZE(val);
      SET_VARSIZE_SHORT(data, data_length);
      memcpy(data + 1, VARDATA(val), data_length - 1);
    }
    else
    {
                                      
      data = (char *)att_align_nominal(data, att->attalign);
      data_length = VARSIZE(val);
      memcpy(data, val, data_length);
    }
  }
  else if (att->attlen == -2)
  {
                                           
    *infomask |= HEAP_HASVARWIDTH;
    Assert(att->attalign == 'c');
    data_length = strlen(DatumGetCString(datum)) + 1;
    memcpy(data, DatumGetPointer(datum), data_length);
  }
  else
  {
                                        
    data = (char *)att_align_nominal(data, att->attalign);
    Assert(att->attlen > 0);
    data_length = att->attlen;
    memcpy(data, DatumGetPointer(datum), data_length);
  }

  data += data_length;
  *dataP = data;
}

   
                   
                                                           
   
                                                                   
                                           
   
                                                                           
   
void
heap_fill_tuple(TupleDesc tupleDesc, Datum *values, bool *isnull, char *data, Size data_size, uint16 *infomask, bits8 *bit)
{
  bits8 *bitP;
  int bitmask;
  int i;
  int numberOfAttributes = tupleDesc->natts;

#ifdef USE_ASSERT_CHECKING
  char *start = data;
#endif

  if (bit != NULL)
  {
    bitP = &bit[-1];
    bitmask = HIGHBIT;
  }
  else
  {
                                     
    bitP = NULL;
    bitmask = 0;
  }

  *infomask &= ~(HEAP_HASNULL | HEAP_HASVARWIDTH | HEAP_HASEXTERNAL);

  for (i = 0; i < numberOfAttributes; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupleDesc, i);

    fill_val(attr, bitP ? &bitP : NULL, &bitmask, &data, infomask, values ? values[i] : PointerGetDatum(NULL), isnull ? isnull[i] : true);
  }

  Assert((data - start) == data_size);
}

                                                                    
                             
                                                                    
   

                    
                                                                     
                    
   
bool
heap_attisnull(HeapTuple tup, int attnum, TupleDesc tupleDesc)
{
     
                                                                          
                                                    
     
  Assert(!tupleDesc || attnum <= tupleDesc->natts);
  if (attnum > (int)HeapTupleHeaderGetNatts(tup->t_data))
  {
    if (tupleDesc && TupleDescAttr(tupleDesc, attnum - 1)->atthasmissing)
    {
      return false;
    }
    else
    {
      return true;
    }
  }

  if (attnum > 0)
  {
    if (HeapTupleNoNulls(tup))
    {
      return false;
    }
    return att_isnull(attnum - 1, tup->t_data->t_bits);
  }

  switch (attnum)
  {
  case TableOidAttributeNumber:
  case SelfItemPointerAttributeNumber:
  case MinTransactionIdAttributeNumber:
  case MinCommandIdAttributeNumber:
  case MaxTransactionIdAttributeNumber:
  case MaxCommandIdAttributeNumber:
                              
    break;

  default:
    elog(ERROR, "invalid attnum: %d", attnum);
  }

  return false;
}

                    
                   
   
                                                                   
                                                          
   
                                                               
   
                                                                    
                                                                  
                                                               
                                   
   
                                                              
                                                                   
                                                                     
                                                                    
   
                                                                       
                                                                     
            
                    
   
Datum
nocachegetattr(HeapTuple tuple, int attnum, TupleDesc tupleDesc)
{
  HeapTupleHeader tup = tuple->t_data;
  char *tp;                                               
  bits8 *bp = tup->t_bits;                                  
  bool slow = false;                                      
  int off;                                                 

                      
                   
     
                                                    
                                              
                                             
                      
     

  attnum--;

  if (!HeapTupleNoNulls(tuple))
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

  tp = (char *)tup + tup->t_hoff;

  if (!slow)
  {
    Form_pg_attribute att;

       
                                                                         
                                                              
       
    att = TupleDescAttr(tupleDesc, attnum);
    if (att->attcacheoff >= 0)
    {
      return fetchatt(att, tp + att->attcacheoff);
    }

       
                                                                       
                                                                         
                                       
       
    if (HeapTupleHasVarWidth(tuple))
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

      if (HeapTupleHasNulls(tuple) && att_isnull(i, bp))
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

                    
                    
   
                                                       
   
                                                                    
                                                                        
                    
   
Datum
heap_getsysattr(HeapTuple tup, int attnum, TupleDesc tupleDesc, bool *isnull)
{
  Datum result;

  Assert(tup);

                                                       
  *isnull = false;

  switch (attnum)
  {
  case SelfItemPointerAttributeNumber:
                                    
    result = PointerGetDatum(&(tup->t_self));
    break;
  case MinTransactionIdAttributeNumber:
    result = TransactionIdGetDatum(HeapTupleHeaderGetRawXmin(tup->t_data));
    break;
  case MaxTransactionIdAttributeNumber:
    result = TransactionIdGetDatum(HeapTupleHeaderGetRawXmax(tup->t_data));
    break;
  case MinCommandIdAttributeNumber:
  case MaxCommandIdAttributeNumber:

       
                                                                    
                                                                      
                                                                     
                                           
       
    result = CommandIdGetDatum(HeapTupleHeaderGetRawCommandId(tup->t_data));
    break;
  case TableOidAttributeNumber:
    result = ObjectIdGetDatum(tup->t_tableOid);
    break;
  default:
    elog(ERROR, "invalid attnum: %d", attnum);
    result = 0;                          
    break;
  }
  return result;
}

                    
                   
   
                                      
   
                                                                        
                               
                    
   
HeapTuple
heap_copytuple(HeapTuple tuple)
{
  HeapTuple newTuple;

  if (!HeapTupleIsValid(tuple) || tuple->t_data == NULL)
  {
    return NULL;
  }

  newTuple = (HeapTuple)palloc(HEAPTUPLESIZE + tuple->t_len);
  newTuple->t_len = tuple->t_len;
  newTuple->t_self = tuple->t_self;
  newTuple->t_tableOid = tuple->t_tableOid;
  newTuple->t_data = (HeapTupleHeader)((char *)newTuple + HEAPTUPLESIZE);
  memcpy((char *)newTuple->t_data, (char *)tuple->t_data, tuple->t_len);
  return newTuple;
}

                    
                              
   
                                                                    
   
                                                                           
                                                                        
                    
   
void
heap_copytuple_with_tuple(HeapTuple src, HeapTuple dest)
{
  if (!HeapTupleIsValid(src) || src->t_data == NULL)
  {
    dest->t_data = NULL;
    return;
  }

  dest->t_len = src->t_len;
  dest->t_self = src->t_self;
  dest->t_tableOid = src->t_tableOid;
  dest->t_data = (HeapTupleHeader)palloc(src->t_len);
  memcpy((char *)dest->t_data, (char *)src->t_data, src->t_len);
}

   
                                                                              
                                                                            
                                                      
   
                                                                        
   
                                                                           
                                
   
static void
expand_tuple(HeapTuple *targetHeapTuple, MinimalTuple *targetMinimalTuple, HeapTuple sourceTuple, TupleDesc tupleDesc)
{
  AttrMissing *attrmiss = NULL;
  int attnum;
  int firstmissingnum = 0;
  bool hasNulls = HeapTupleHasNulls(sourceTuple);
  HeapTupleHeader targetTHeader;
  HeapTupleHeader sourceTHeader = sourceTuple->t_data;
  int sourceNatts = HeapTupleHeaderGetNatts(sourceTHeader);
  int natts = tupleDesc->natts;
  int sourceNullLen;
  int targetNullLen;
  Size sourceDataLen = sourceTuple->t_len - sourceTHeader->t_hoff;
  Size targetDataLen;
  Size len;
  int hoff;
  bits8 *nullBits = NULL;
  int bitMask = 0;
  char *targetData;
  uint16 *infoMask;

  Assert((targetHeapTuple && !targetMinimalTuple) || (!targetHeapTuple && targetMinimalTuple));

  Assert(sourceNatts < natts);

  sourceNullLen = (hasNulls ? BITMAPLEN(sourceNatts) : 0);

  targetDataLen = sourceDataLen;

  if (tupleDesc->constr && tupleDesc->constr->missing)
  {
       
                                                                       
                                                                      
                                           
       
    attrmiss = tupleDesc->constr->missing;

       
                                                                          
                                                                      
       
    for (firstmissingnum = sourceNatts; firstmissingnum < natts; firstmissingnum++)
    {
      if (attrmiss[firstmissingnum].am_present)
      {
        break;
      }
      else
      {
        hasNulls = true;
      }
    }

       
                                                                         
                                                       
       
    for (attnum = firstmissingnum; attnum < natts; attnum++)
    {
      if (attrmiss[attnum].am_present)
      {
        Form_pg_attribute att = TupleDescAttr(tupleDesc, attnum);

        targetDataLen = att_align_datum(targetDataLen, att->attalign, att->attlen, attrmiss[attnum].am_value);

        targetDataLen = att_addlength_pointer(targetDataLen, att->attlen, attrmiss[attnum].am_value);
      }
      else
      {
                                                  
        hasNulls = true;
      }
    }
  }                                 
  else
  {
       
                                                                         
                                                            
       
    hasNulls = true;
  }

  len = 0;

  if (hasNulls)
  {
    targetNullLen = BITMAPLEN(natts);
    len += targetNullLen;
  }
  else
  {
    targetNullLen = 0;
  }

     
                                                                       
                                                                    
     
  if (targetHeapTuple)
  {
    len += offsetof(HeapTupleHeaderData, t_bits);
    hoff = len = MAXALIGN(len);                             
    len += targetDataLen;

    *targetHeapTuple = (HeapTuple)palloc0(HEAPTUPLESIZE + len);
    (*targetHeapTuple)->t_data = targetTHeader = (HeapTupleHeader)((char *)*targetHeapTuple + HEAPTUPLESIZE);
    (*targetHeapTuple)->t_len = len;
    (*targetHeapTuple)->t_tableOid = sourceTuple->t_tableOid;
    (*targetHeapTuple)->t_self = sourceTuple->t_self;

    targetTHeader->t_infomask = sourceTHeader->t_infomask;
    targetTHeader->t_hoff = hoff;
    HeapTupleHeaderSetNatts(targetTHeader, natts);
    HeapTupleHeaderSetDatumLength(targetTHeader, len);
    HeapTupleHeaderSetTypeId(targetTHeader, tupleDesc->tdtypeid);
    HeapTupleHeaderSetTypMod(targetTHeader, tupleDesc->tdtypmod);
                                                                        
    ItemPointerSetInvalid(&(targetTHeader->t_ctid));
    if (targetNullLen > 0)
    {
      nullBits = (bits8 *)((char *)(*targetHeapTuple)->t_data + offsetof(HeapTupleHeaderData, t_bits));
    }
    targetData = (char *)(*targetHeapTuple)->t_data + hoff;
    infoMask = &(targetTHeader->t_infomask);
  }
  else
  {
    len += SizeofMinimalTupleHeader;
    hoff = len = MAXALIGN(len);                             
    len += targetDataLen;

    *targetMinimalTuple = (MinimalTuple)palloc0(len);
    (*targetMinimalTuple)->t_len = len;
    (*targetMinimalTuple)->t_hoff = hoff + MINIMAL_TUPLE_OFFSET;
    (*targetMinimalTuple)->t_infomask = sourceTHeader->t_infomask;
                                            
    HeapTupleHeaderSetNatts(*targetMinimalTuple, natts);
    if (targetNullLen > 0)
    {
      nullBits = (bits8 *)((char *)*targetMinimalTuple + offsetof(MinimalTupleData, t_bits));
    }
    targetData = (char *)*targetMinimalTuple + hoff;
    infoMask = &((*targetMinimalTuple)->t_infomask);
  }

  if (targetNullLen > 0)
  {
    if (sourceNullLen > 0)
    {
                                                      
      memcpy(nullBits, ((char *)sourceTHeader) + offsetof(HeapTupleHeaderData, t_bits), sourceNullLen);
      nullBits += sourceNullLen - 1;
    }
    else
    {
      sourceNullLen = BITMAPLEN(sourceNatts);
                                                    
      memset(nullBits, 0xff, sourceNullLen);

      nullBits += sourceNullLen - 1;

      if (sourceNatts & 0x07)
      {
                                        
        bitMask = 0xff << (sourceNatts & 0x07);
                   
        *nullBits = ~bitMask;
      }
    }

    bitMask = (1 << ((sourceNatts - 1) & 0x07));
  }                              

  memcpy(targetData, ((char *)sourceTuple->t_data) + sourceTHeader->t_hoff, sourceDataLen);

  targetData += sourceDataLen;

                                      
  for (attnum = sourceNatts; attnum < natts; attnum++)
  {

    Form_pg_attribute attr = TupleDescAttr(tupleDesc, attnum);

    if (attrmiss && attrmiss[attnum].am_present)
    {
      fill_val(attr, nullBits ? &nullBits : NULL, &bitMask, &targetData, infoMask, attrmiss[attnum].am_value, false);
    }
    else
    {
      fill_val(attr, &nullBits, &bitMask, &targetData, infoMask, (Datum)0, true);
    }
  }                                       
}

   
                                                      
   
MinimalTuple
minimal_expand_tuple(HeapTuple sourceTuple, TupleDesc tupleDesc)
{
  MinimalTuple minimalTuple;

  expand_tuple(NULL, &minimalTuple, sourceTuple, tupleDesc);
  return minimalTuple;
}

   
                                                        
   
HeapTuple
heap_expand_tuple(HeapTuple sourceTuple, TupleDesc tupleDesc)
{
  HeapTuple heapTuple;

  expand_tuple(&heapTuple, NULL, sourceTuple, tupleDesc);
  return heapTuple;
}

                    
                             
   
                                           
                    
   
Datum
heap_copy_tuple_as_datum(HeapTuple tuple, TupleDesc tupleDesc)
{
  HeapTupleHeader td;

     
                                                                          
                                                                     
     
  if (HeapTupleHasExternal(tuple))
  {
    return toast_flatten_tuple_to_datum(tuple->t_data, tuple->t_len, tupleDesc);
  }

     
                                                                       
                                                                          
                                                                        
     
  td = (HeapTupleHeader)palloc(tuple->t_len);
  memcpy((char *)td, (char *)tuple->t_data, tuple->t_len);

  HeapTupleHeaderSetDatumLength(td, tuple->t_len);
  HeapTupleHeaderSetTypeId(td, tupleDesc->tdtypeid);
  HeapTupleHeaderSetTypMod(td, tupleDesc->tdtypmod);

  return PointerGetDatum(td);
}

   
                   
                                                                   
                                                                
   
                                                          
   
HeapTuple
heap_form_tuple(TupleDesc tupleDescriptor, Datum *values, bool *isnull)
{
  HeapTuple tuple;                      
  HeapTupleHeader td;                 
  Size len, data_len;
  int hoff;
  bool hasnull = false;
  int numberOfAttributes = tupleDescriptor->natts;
  int i;

  if (numberOfAttributes > MaxTupleAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("number of columns (%d) exceeds limit (%d)", numberOfAttributes, MaxTupleAttributeNumber)));
  }

     
                     
     
  for (i = 0; i < numberOfAttributes; i++)
  {
    if (isnull[i])
    {
      hasnull = true;
      break;
    }
  }

     
                                  
     
  len = offsetof(HeapTupleHeaderData, t_bits);

  if (hasnull)
  {
    len += BITMAPLEN(numberOfAttributes);
  }

  hoff = len = MAXALIGN(len);                             

  data_len = heap_compute_data_size(tupleDescriptor, values, isnull);

  len += data_len;

     
                                                                       
                                                                    
     
  tuple = (HeapTuple)palloc0(HEAPTUPLESIZE + len);
  tuple->t_data = td = (HeapTupleHeader)((char *)tuple + HEAPTUPLESIZE);

     
                                                                             
                                                                             
                                        
     
  tuple->t_len = len;
  ItemPointerSetInvalid(&(tuple->t_self));
  tuple->t_tableOid = InvalidOid;

  HeapTupleHeaderSetDatumLength(td, len);
  HeapTupleHeaderSetTypeId(td, tupleDescriptor->tdtypeid);
  HeapTupleHeaderSetTypMod(td, tupleDescriptor->tdtypmod);
                                                                      
  ItemPointerSetInvalid(&(td->t_ctid));

  HeapTupleHeaderSetNatts(td, numberOfAttributes);
  td->t_hoff = hoff;

  heap_fill_tuple(tupleDescriptor, values, isnull, (char *)td + hoff, data_len, &td->t_infomask, (hasnull ? td->t_bits : NULL));

  return tuple;
}

   
                     
                                                                        
   
                                                                          
                                                                               
                                                                            
                                                                    
   
                                                          
   
HeapTuple
heap_modify_tuple(HeapTuple tuple, TupleDesc tupleDesc, Datum *replValues, bool *replIsnull, bool *doReplace)
{
  int numberOfAttributes = tupleDesc->natts;
  int attoff;
  Datum *values;
  bool *isnull;
  HeapTuple newTuple;

     
                                                                             
                                       
     
                                                                          
                                                                            
                                                                         
                                                                             
                                                                          
                                     
     
  values = (Datum *)palloc(numberOfAttributes * sizeof(Datum));
  isnull = (bool *)palloc(numberOfAttributes * sizeof(bool));

  heap_deform_tuple(tuple, tupleDesc, values, isnull);

  for (attoff = 0; attoff < numberOfAttributes; attoff++)
  {
    if (doReplace[attoff])
    {
      values[attoff] = replValues[attoff];
      isnull[attoff] = replIsnull[attoff];
    }
  }

     
                                                          
     
  newTuple = heap_form_tuple(tupleDesc, values, isnull);

  pfree(values);
  pfree(isnull);

     
                                                                   
     
  newTuple->t_data->t_ctid = tuple->t_data->t_ctid;
  newTuple->t_self = tuple->t_self;
  newTuple->t_tableOid = tuple->t_tableOid;

  return newTuple;
}

   
                             
                                                                        
   
                                                                           
                                                                            
                                                                          
                                                                             
                                                                  
   
                                                          
   
HeapTuple
heap_modify_tuple_by_cols(HeapTuple tuple, TupleDesc tupleDesc, int nCols, int *replCols, Datum *replValues, bool *replIsnull)
{
  int numberOfAttributes = tupleDesc->natts;
  Datum *values;
  bool *isnull;
  HeapTuple newTuple;
  int i;

     
                                                                             
                                             
     
  values = (Datum *)palloc(numberOfAttributes * sizeof(Datum));
  isnull = (bool *)palloc(numberOfAttributes * sizeof(bool));

  heap_deform_tuple(tuple, tupleDesc, values, isnull);

  for (i = 0; i < nCols; i++)
  {
    int attnum = replCols[i];

    if (attnum <= 0 || attnum > numberOfAttributes)
    {
      elog(ERROR, "invalid column number %d", attnum);
    }
    values[attnum - 1] = replValues[i];
    isnull[attnum - 1] = replIsnull[i];
  }

     
                                                          
     
  newTuple = heap_form_tuple(tupleDesc, values, isnull);

  pfree(values);
  pfree(isnull);

     
                                                                   
     
  newTuple->t_data->t_ctid = tuple->t_data->t_ctid;
  newTuple->t_self = tuple->t_self;
  newTuple->t_tableOid = tuple->t_tableOid;

  return newTuple;
}

   
                     
                                                                   
                                    
   
                                                                    
                                                         
                                            
   
                                                                  
                                                  
   
                                                               
                                                                  
                                                             
                                                 
   
void
heap_deform_tuple(HeapTuple tuple, TupleDesc tupleDesc, Datum *values, bool *isnull)
{
  HeapTupleHeader tup = tuple->t_data;
  bool hasnulls = HeapTupleHasNulls(tuple);
  int tdesc_natts = tupleDesc->natts;
  int natts;                                
  int attnum;
  char *tp;                                       
  uint32 off;                                        
  bits8 *bp = tup->t_bits;                                  
  bool slow = false;                                        

  natts = HeapTupleHeaderGetNatts(tup);

     
                                                                             
                                                                             
                          
     
  natts = Min(natts, tdesc_natts);

  tp = (char *)tup + tup->t_hoff;

  off = 0;

  for (attnum = 0; attnum < natts; attnum++)
  {
    Form_pg_attribute thisatt = TupleDescAttr(tupleDesc, attnum);

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

     
                                                                         
                                                     
     
  for (; attnum < tdesc_natts; attnum++)
  {
    values[attnum] = getmissingattr(tupleDesc, attnum + 1, &isnull[attnum]);
  }
}

   
                  
   
void
heap_freetuple(HeapTuple htup)
{
  pfree(htup);
}

   
                           
                                                                          
                                                                
   
                                                                      
                                                                             
            
   
                                                          
   
MinimalTuple
heap_form_minimal_tuple(TupleDesc tupleDescriptor, Datum *values, bool *isnull)
{
  MinimalTuple tuple;                   
  Size len, data_len;
  int hoff;
  bool hasnull = false;
  int numberOfAttributes = tupleDescriptor->natts;
  int i;

  if (numberOfAttributes > MaxTupleAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("number of columns (%d) exceeds limit (%d)", numberOfAttributes, MaxTupleAttributeNumber)));
  }

     
                     
     
  for (i = 0; i < numberOfAttributes; i++)
  {
    if (isnull[i])
    {
      hasnull = true;
      break;
    }
  }

     
                                  
     
  len = SizeofMinimalTupleHeader;

  if (hasnull)
  {
    len += BITMAPLEN(numberOfAttributes);
  }

  hoff = len = MAXALIGN(len);                             

  data_len = heap_compute_data_size(tupleDescriptor, values, isnull);

  len += data_len;

     
                                         
     
  tuple = (MinimalTuple)palloc0(len);

     
                                  
     
  tuple->t_len = len;
  HeapTupleHeaderSetNatts(tuple, numberOfAttributes);
  tuple->t_hoff = hoff + MINIMAL_TUPLE_OFFSET;

  heap_fill_tuple(tupleDescriptor, values, isnull, (char *)tuple + hoff, data_len, &tuple->t_infomask, (hasnull ? tuple->t_bits : NULL));

  return tuple;
}

   
                           
   
void
heap_free_minimal_tuple(MinimalTuple mtup)
{
  pfree(mtup);
}

   
                           
                        
   
                                                          
   
MinimalTuple
heap_copy_minimal_tuple(MinimalTuple mtup)
{
  MinimalTuple result;

  result = (MinimalTuple)palloc(mtup->t_len);
  memcpy(result, mtup, mtup->t_len);
  return result;
}

   
                                 
                                                       
                                          
   
                                                          
                                                                        
                               
   
HeapTuple
heap_tuple_from_minimal_tuple(MinimalTuple mtup)
{
  HeapTuple result;
  uint32 len = mtup->t_len + MINIMAL_TUPLE_OFFSET;

  result = (HeapTuple)palloc(HEAPTUPLESIZE + len);
  result->t_len = len;
  ItemPointerSetInvalid(&(result->t_self));
  result->t_tableOid = InvalidOid;
  result->t_data = (HeapTupleHeader)((char *)result + HEAPTUPLESIZE);
  memcpy((char *)result->t_data + MINIMAL_TUPLE_OFFSET, mtup, mtup->t_len);
  memset(result->t_data, 0, offsetof(HeapTupleHeaderData, t_infomask2));
  return result;
}

   
                                 
                                                      
   
                                                          
   
MinimalTuple
minimal_tuple_from_heap_tuple(HeapTuple htup)
{
  MinimalTuple result;
  uint32 len;

  Assert(htup->t_len > MINIMAL_TUPLE_OFFSET);
  len = htup->t_len - MINIMAL_TUPLE_OFFSET;
  result = (MinimalTuple)palloc(len);
  memcpy(result, (char *)htup->t_data + MINIMAL_TUPLE_OFFSET, len);
  result->t_len = len;
  return result;
}

   
                                                                      
                                           
   
size_t
varsize_any(void *p)
{
  return VARSIZE_ANY(p);
}
