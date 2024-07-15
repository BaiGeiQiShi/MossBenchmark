                                                                            
   
                
                                                             
                               
   
                                                                
   
   
                  
                                          
   
   
                      
                             
                                                                
                              
   
                   
                                                   
   
                              
                                                            
   
                                                                            
   

#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "common/int.h"
#include "common/pg_lzcompress.h"
#include "miscadmin.h"
#include "utils/expandeddatum.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/typcache.h"

#undef TOAST_DEBUG

   
                                                              
   
typedef struct toast_compress_header
{
  int32 vl_len_;                                              
  int32 rawsize;
} toast_compress_header;

   
                                                                   
                  
   
#define TOAST_COMPRESS_HDRSZ ((int32)sizeof(toast_compress_header))
#define TOAST_COMPRESS_RAWSIZE(ptr) (((toast_compress_header *)(ptr))->rawsize)
#define TOAST_COMPRESS_RAWDATA(ptr) (((char *)(ptr)) + TOAST_COMPRESS_HDRSZ)
#define TOAST_COMPRESS_SET_RAWSIZE(ptr, len) (((toast_compress_header *)(ptr))->rawsize = (len))

static void
toast_delete_datum(Relation rel, Datum value, bool is_speculative);
static Datum
toast_save_datum(Relation rel, Datum value, struct varlena *oldexternal, int options);
static bool
toastrel_valueid_exists(Relation toastrel, Oid valueid);
static bool
toastid_valueid_exists(Oid toastrelid, Oid valueid);
static struct varlena *
toast_fetch_datum(struct varlena *attr);
static struct varlena *
toast_fetch_datum_slice(struct varlena *attr, int32 sliceoffset, int32 length);
static struct varlena *
toast_decompress_datum(struct varlena *attr);
static struct varlena *
toast_decompress_datum_slice(struct varlena *attr, int32 slicelength);
static int
toast_open_indexes(Relation toastrel, LOCKMODE lock, Relation **toastidxs, int *num_indexes);
static void
toast_close_indexes(Relation *toastidxs, int num_indexes, LOCKMODE lock);
static void
init_toast_snapshot(Snapshot toast_snapshot);

              
                           
   
                                                       
                                                          
   
                                                                           
                                                                            
                                                                          
                                                          
              
   
struct varlena *
heap_tuple_fetch_attr(struct varlena *attr)
{
  struct varlena *result;

  if (VARATT_IS_EXTERNAL_ONDISK(attr))
  {
       
                                              
       
    result = toast_fetch_datum(attr);
  }
  else if (VARATT_IS_EXTERNAL_INDIRECT(attr))
  {
       
                                                      
       
    struct varatt_indirect redirect;

    VARATT_EXTERNAL_GET_POINTER(redirect, attr);
    attr = (struct varlena *)redirect.pointer;

                                               
    Assert(!VARATT_IS_EXTERNAL_INDIRECT(attr));

                                                              
    if (VARATT_IS_EXTERNAL(attr))
    {
      return heap_tuple_fetch_attr(attr);
    }

       
                                                                      
                         
       
    result = (struct varlena *)palloc(VARSIZE_ANY(attr));
    memcpy(result, attr, VARSIZE_ANY(attr));
  }
  else if (VARATT_IS_EXTERNAL_EXPANDED(attr))
  {
       
                                                              
       
    ExpandedObjectHeader *eoh;
    Size resultsize;

    eoh = DatumGetEOHP(PointerGetDatum(attr));
    resultsize = EOH_get_flat_size(eoh);
    result = (struct varlena *)palloc(resultsize);
    EOH_flatten_into(eoh, (void *)result, resultsize);
  }
  else
  {
       
                                                                         
       
    result = attr;
  }

  return result;
}

              
                             
   
                                                                   
                                                                         
   
                                                                           
                                                 
              
   
struct varlena *
heap_tuple_untoast_attr(struct varlena *attr)
{
  if (VARATT_IS_EXTERNAL_ONDISK(attr))
  {
       
                                                                       
       
    attr = toast_fetch_datum(attr);
                                           
    if (VARATT_IS_COMPRESSED(attr))
    {
      struct varlena *tmp = attr;

      attr = toast_decompress_datum(tmp);
      pfree(tmp);
    }
  }
  else if (VARATT_IS_EXTERNAL_INDIRECT(attr))
  {
       
                                                      
       
    struct varatt_indirect redirect;

    VARATT_EXTERNAL_GET_POINTER(redirect, attr);
    attr = (struct varlena *)redirect.pointer;

                                               
    Assert(!VARATT_IS_EXTERNAL_INDIRECT(attr));

                                                                   
    attr = heap_tuple_untoast_attr(attr);

                                          
    if (attr == (struct varlena *)redirect.pointer)
    {
      struct varlena *result;

      result = (struct varlena *)palloc(VARSIZE_ANY(attr));
      memcpy(result, attr, VARSIZE_ANY(attr));
      attr = result;
    }
  }
  else if (VARATT_IS_EXTERNAL_EXPANDED(attr))
  {
       
                                                              
       
    attr = heap_tuple_fetch_attr(attr);
                                                                       
    Assert(!VARATT_IS_EXTENDED(attr));
  }
  else if (VARATT_IS_COMPRESSED(attr))
  {
       
                                                           
       
    attr = toast_decompress_datum(attr);
  }
  else if (VARATT_IS_SHORT(attr))
  {
       
                                                                          
       
    Size data_size = VARSIZE_SHORT(attr) - VARHDRSZ_SHORT;
    Size new_size = data_size + VARHDRSZ;
    struct varlena *new_attr;

    new_attr = (struct varlena *)palloc(new_size);
    SET_VARSIZE(new_attr, new_size);
    memcpy(VARDATA(new_attr), VARDATA_SHORT(attr), data_size);
    attr = new_attr;
  }

  return attr;
}

              
                                   
   
                                                           
                                          
   
                                                
                                                            
              
   
struct varlena *
heap_tuple_untoast_attr_slice(struct varlena *attr, int32 sliceoffset, int32 slicelength)
{
  struct varlena *preslice;
  struct varlena *result;
  char *attrdata;
  int32 slicelimit;
  int32 attrsize;

  if (sliceoffset < 0)
  {
    elog(ERROR, "invalid sliceoffset: %d", sliceoffset);
  }

     
                                                                             
                                                             
     
  if (slicelength < 0)
  {
    slicelimit = -1;
  }
  else if (pg_add_s32_overflow(sliceoffset, slicelength, &slicelimit))
  {
    slicelength = slicelimit = -1;
  }

  if (VARATT_IS_EXTERNAL_ONDISK(attr))
  {
    struct varatt_external toast_pointer;

    VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

                                                      
    if (!VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer))
    {
      return toast_fetch_datum_slice(attr, sliceoffset, slicelength);
    }

                                                                      
    preslice = toast_fetch_datum(attr);
  }
  else if (VARATT_IS_EXTERNAL_INDIRECT(attr))
  {
    struct varatt_indirect redirect;

    VARATT_EXTERNAL_GET_POINTER(redirect, attr);

                                               
    Assert(!VARATT_IS_EXTERNAL_INDIRECT(redirect.pointer));

    return heap_tuple_untoast_attr_slice(redirect.pointer, sliceoffset, slicelength);
  }
  else if (VARATT_IS_EXTERNAL_EXPANDED(attr))
  {
                                                         
    preslice = heap_tuple_fetch_attr(attr);
  }
  else
  {
    preslice = attr;
  }

  Assert(!VARATT_IS_EXTERNAL(preslice));

  if (VARATT_IS_COMPRESSED(preslice))
  {
    struct varlena *tmp = preslice;

                                                                 
    if (slicelimit >= 0)
    {
      preslice = toast_decompress_datum_slice(tmp, slicelimit);
    }
    else
    {
      preslice = toast_decompress_datum(tmp);
    }

    if (tmp != attr)
    {
      pfree(tmp);
    }
  }

  if (VARATT_IS_SHORT(preslice))
  {
    attrdata = VARDATA_SHORT(preslice);
    attrsize = VARSIZE_SHORT(preslice) - VARHDRSZ_SHORT;
  }
  else
  {
    attrdata = VARDATA(preslice);
    attrsize = VARSIZE(preslice) - VARHDRSZ;
  }

                                                             

  if (sliceoffset >= attrsize)
  {
    sliceoffset = 0;
    slicelength = 0;
  }
  else if (slicelength < 0 || slicelimit > attrsize)
  {
    slicelength = attrsize - sliceoffset;
  }

  result = (struct varlena *)palloc(slicelength + VARHDRSZ);
  SET_VARSIZE(result, slicelength + VARHDRSZ);

  memcpy(VARDATA(result), attrdata + sliceoffset, slicelength);

  if (preslice != attr)
  {
    pfree(preslice);
  }

  return result;
}

              
                          
   
                                                      
                                   
              
   
Size
toast_raw_datum_size(Datum value)
{
  struct varlena *attr = (struct varlena *)DatumGetPointer(value);
  Size result;

  if (VARATT_IS_EXTERNAL_ONDISK(attr))
  {
                                                                          
    struct varatt_external toast_pointer;

    VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);
    result = toast_pointer.va_rawsize;
  }
  else if (VARATT_IS_EXTERNAL_INDIRECT(attr))
  {
    struct varatt_indirect toast_pointer;

    VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

                                               
    Assert(!VARATT_IS_EXTERNAL_INDIRECT(toast_pointer.pointer));

    return toast_raw_datum_size(PointerGetDatum(toast_pointer.pointer));
  }
  else if (VARATT_IS_EXTERNAL_EXPANDED(attr))
  {
    result = EOH_get_flat_size(DatumGetEOHP(value));
  }
  else if (VARATT_IS_COMPRESSED(attr))
  {
                                                   
    result = VARRAWSIZE_4B_C(attr) + VARHDRSZ;
  }
  else if (VARATT_IS_SHORT(attr))
  {
       
                                                                      
                                                  
       
    result = VARSIZE_SHORT(attr) - VARHDRSZ_SHORT + VARHDRSZ;
  }
  else
  {
                               
    result = VARSIZE(attr);
  }
  return result;
}

              
                    
   
                                                                             
              
   
Size
toast_datum_size(Datum value)
{
  struct varlena *attr = (struct varlena *)DatumGetPointer(value);
  Size result;

  if (VARATT_IS_EXTERNAL_ONDISK(attr))
  {
       
                                                                   
                                                                         
                      
       
    struct varatt_external toast_pointer;

    VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);
    result = toast_pointer.va_extsize;
  }
  else if (VARATT_IS_EXTERNAL_INDIRECT(attr))
  {
    struct varatt_indirect toast_pointer;

    VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

                                               
    Assert(!VARATT_IS_EXTERNAL_INDIRECT(attr));

    return toast_datum_size(PointerGetDatum(toast_pointer.pointer));
  }
  else if (VARATT_IS_EXTERNAL_EXPANDED(attr))
  {
    result = EOH_get_flat_size(DatumGetEOHP(value));
  }
  else if (VARATT_IS_SHORT(attr))
  {
    result = VARSIZE_SHORT(attr);
  }
  else
  {
       
                                                                           
                                             
       
    result = VARSIZE(attr);
  }
  return result;
}

              
                  
   
                                           
              
   
void
toast_delete(Relation rel, HeapTuple oldtup, bool is_speculative)
{
  TupleDesc tupleDesc;
  int numAttrs;
  int i;
  Datum toast_values[MaxHeapAttributeNumber];
  bool toast_isnull[MaxHeapAttributeNumber];

     
                                                                    
                                                                  
     
  Assert(rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_MATVIEW);

     
                                                                    
     
                                                                          
                                                                             
                                                                 
                                                                             
                                                                            
                                                                        
                                            
     
  tupleDesc = rel->rd_att;
  numAttrs = tupleDesc->natts;

  Assert(numAttrs <= MaxHeapAttributeNumber);
  heap_deform_tuple(oldtup, tupleDesc, toast_values, toast_isnull);

     
                                                                             
               
     
  for (i = 0; i < numAttrs; i++)
  {
    if (TupleDescAttr(tupleDesc, i)->attlen == -1)
    {
      Datum value = toast_values[i];

      if (toast_isnull[i])
      {
        continue;
      }
      else if (VARATT_IS_EXTERNAL_ONDISK(PointerGetDatum(value)))
      {
        toast_delete_datum(rel, value, is_speculative);
      }
    }
  }
}

              
                            
   
                                                              
                                              
   
           
                                                  
                                                              
                                                                 
           
                                                                        
                                           
   
                                                                       
                                         
              
   
HeapTuple
toast_insert_or_update(Relation rel, HeapTuple newtup, HeapTuple oldtup, int options)
{
  HeapTuple result_tuple;
  TupleDesc tupleDesc;
  int numAttrs;
  int i;

  bool need_change = false;
  bool need_free = false;
  bool need_delold = false;
  bool has_nulls = false;

  Size maxDataLen;
  Size hoff;

  char toast_action[MaxHeapAttributeNumber];
  bool toast_isnull[MaxHeapAttributeNumber];
  bool toast_oldisnull[MaxHeapAttributeNumber];
  Datum toast_values[MaxHeapAttributeNumber];
  Datum toast_oldvalues[MaxHeapAttributeNumber];
  struct varlena *toast_oldexternal[MaxHeapAttributeNumber];
  int32 toast_sizes[MaxHeapAttributeNumber];
  bool toast_free[MaxHeapAttributeNumber];
  bool toast_delold[MaxHeapAttributeNumber];

     
                                                                        
                                                                      
                                                                       
              
     
  options &= ~HEAP_INSERT_SPECULATIVE;

     
                                                                    
                                                                  
     
  Assert(rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_MATVIEW);

     
                                                                       
     
  tupleDesc = rel->rd_att;
  numAttrs = tupleDesc->natts;

  Assert(numAttrs <= MaxHeapAttributeNumber);
  heap_deform_tuple(newtup, tupleDesc, toast_values, toast_isnull);
  if (oldtup != NULL)
  {
    heap_deform_tuple(oldtup, tupleDesc, toast_oldvalues, toast_oldisnull);
  }

                
                                                     
     
                                                  
                            
                                                
                                              
     
                                                                         
                                          
                
     
  memset(toast_action, ' ', numAttrs * sizeof(char));
  memset(toast_oldexternal, 0, numAttrs * sizeof(struct varlena *));
  memset(toast_free, 0, numAttrs * sizeof(bool));
  memset(toast_delold, 0, numAttrs * sizeof(bool));

  for (i = 0; i < numAttrs; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupleDesc, i);
    struct varlena *old_value;
    struct varlena *new_value;

    if (oldtup != NULL)
    {
         
                                                                 
         
      old_value = (struct varlena *)DatumGetPointer(toast_oldvalues[i]);
      new_value = (struct varlena *)DatumGetPointer(toast_values[i]);

         
                                                                        
                                     
         
      if (att->attlen == -1 && !toast_oldisnull[i] && VARATT_IS_EXTERNAL_ONDISK(old_value))
      {
        if (toast_isnull[i] || !VARATT_IS_EXTERNAL_ONDISK(new_value) || memcmp((char *)old_value, (char *)new_value, VARSIZE_EXTERNAL(old_value)) != 0)
        {
             
                                                                 
                              
             
          toast_delold[i] = true;
          need_delold = true;
        }
        else
        {
             
                                                                     
                                                                
                    
             
          toast_action[i] = 'p';
          continue;
        }
      }
    }
    else
    {
         
                                             
         
      new_value = (struct varlena *)DatumGetPointer(toast_values[i]);
    }

       
                              
       
    if (toast_isnull[i])
    {
      toast_action[i] = 'p';
      has_nulls = true;
      continue;
    }

       
                                      
       
    if (att->attlen == -1)
    {
         
                                                                  
         
      if (att->attstorage == 'p')
      {
        toast_action[i] = 'p';
      }

         
                                                                     
                                                                        
                                                                     
                                                                     
                                                                   
                               
         
      if (VARATT_IS_EXTERNAL(new_value))
      {
        toast_oldexternal[i] = new_value;
        if (att->attstorage == 'p')
        {
          new_value = heap_tuple_untoast_attr(new_value);
        }
        else
        {
          new_value = heap_tuple_fetch_attr(new_value);
        }
        toast_values[i] = PointerGetDatum(new_value);
        toast_free[i] = true;
        need_change = true;
        need_free = true;
      }

         
                                             
         
      toast_sizes[i] = VARSIZE_ANY(new_value);
    }
    else
    {
         
                                                       
         
      toast_action[i] = 'p';
    }
  }

                
                                                                      
     
                                                                       
                                                                         
                                                             
                                                       
                                                      
                
     

                                                                       
  hoff = SizeofHeapTupleHeader;
  if (has_nulls)
  {
    hoff += BITMAPLEN(numAttrs);
  }
  hoff = MAXALIGN(hoff);
                                                     
  maxDataLen = RelationGetToastTupleTarget(rel, TOAST_TUPLE_TARGET) - hoff;

     
                                                                           
                                                                     
     
  while (heap_compute_data_size(tupleDesc, toast_values, toast_isnull) > maxDataLen)
  {
    int biggest_attno = -1;
    int32 biggest_size = MAXALIGN(TOAST_POINTER_SIZE);
    Datum old_value;
    Datum new_value;

       
                                                                 
       
    for (i = 0; i < numAttrs; i++)
    {
      Form_pg_attribute att = TupleDescAttr(tupleDesc, i);

      if (toast_action[i] != ' ')
      {
        continue;
      }
      if (VARATT_IS_EXTERNAL(DatumGetPointer(toast_values[i])))
      {
        continue;                                              
      }
      if (VARATT_IS_COMPRESSED(DatumGetPointer(toast_values[i])))
      {
        continue;
      }
      if (att->attstorage != 'x' && att->attstorage != 'e')
      {
        continue;
      }
      if (toast_sizes[i] > biggest_size)
      {
        biggest_attno = i;
        biggest_size = toast_sizes[i];
      }
    }

    if (biggest_attno < 0)
    {
      break;
    }

       
                                                               
       
    i = biggest_attno;
    if (TupleDescAttr(tupleDesc, i)->attstorage == 'x')
    {
      old_value = toast_values[i];
      new_value = toast_compress_datum(old_value);

      if (DatumGetPointer(new_value) != NULL)
      {
                                    
        if (toast_free[i])
        {
          pfree(DatumGetPointer(old_value));
        }
        toast_values[i] = new_value;
        toast_free[i] = true;
        toast_sizes[i] = VARSIZE(DatumGetPointer(toast_values[i]));
        need_change = true;
        need_free = true;
      }
      else
      {
                                                                     
        toast_action[i] = 'x';
      }
    }
    else
    {
                                                                       
      toast_action[i] = 'x';
    }

       
                                                                          
                                                                         
                                                                         
                                                            
       
                                                               
       
    if (toast_sizes[i] > maxDataLen && rel->rd_rel->reltoastrelid != InvalidOid)
    {
      old_value = toast_values[i];
      toast_action[i] = 'p';
      toast_values[i] = toast_save_datum(rel, toast_values[i], toast_oldexternal[i], options);
      if (toast_free[i])
      {
        pfree(DatumGetPointer(old_value));
      }
      toast_free[i] = true;
      need_change = true;
      need_free = true;
    }
  }

     
                                                                           
                                                                       
     
  while (heap_compute_data_size(tupleDesc, toast_values, toast_isnull) > maxDataLen && rel->rd_rel->reltoastrelid != InvalidOid)
  {
    int biggest_attno = -1;
    int32 biggest_size = MAXALIGN(TOAST_POINTER_SIZE);
    Datum old_value;

             
                                                         
                                    
             
       
    for (i = 0; i < numAttrs; i++)
    {
      Form_pg_attribute att = TupleDescAttr(tupleDesc, i);

      if (toast_action[i] == 'p')
      {
        continue;
      }
      if (VARATT_IS_EXTERNAL(DatumGetPointer(toast_values[i])))
      {
        continue;                                              
      }
      if (att->attstorage != 'x' && att->attstorage != 'e')
      {
        continue;
      }
      if (toast_sizes[i] > biggest_size)
      {
        biggest_attno = i;
        biggest_size = toast_sizes[i];
      }
    }

    if (biggest_attno < 0)
    {
      break;
    }

       
                           
       
    i = biggest_attno;
    old_value = toast_values[i];
    toast_action[i] = 'p';
    toast_values[i] = toast_save_datum(rel, toast_values[i], toast_oldexternal[i], options);
    if (toast_free[i])
    {
      pfree(DatumGetPointer(old_value));
    }
    toast_free[i] = true;

    need_change = true;
    need_free = true;
  }

     
                                                                  
                 
     
  while (heap_compute_data_size(tupleDesc, toast_values, toast_isnull) > maxDataLen)
  {
    int biggest_attno = -1;
    int32 biggest_size = MAXALIGN(TOAST_POINTER_SIZE);
    Datum old_value;
    Datum new_value;

       
                                                                  
       
    for (i = 0; i < numAttrs; i++)
    {
      if (toast_action[i] != ' ')
      {
        continue;
      }
      if (VARATT_IS_EXTERNAL(DatumGetPointer(toast_values[i])))
      {
        continue;                                              
      }
      if (VARATT_IS_COMPRESSED(DatumGetPointer(toast_values[i])))
      {
        continue;
      }
      if (TupleDescAttr(tupleDesc, i)->attstorage != 'm')
      {
        continue;
      }
      if (toast_sizes[i] > biggest_size)
      {
        biggest_attno = i;
        biggest_size = toast_sizes[i];
      }
    }

    if (biggest_attno < 0)
    {
      break;
    }

       
                                     
       
    i = biggest_attno;
    old_value = toast_values[i];
    new_value = toast_compress_datum(old_value);

    if (DatumGetPointer(new_value) != NULL)
    {
                                  
      if (toast_free[i])
      {
        pfree(DatumGetPointer(old_value));
      }
      toast_values[i] = new_value;
      toast_free[i] = true;
      toast_sizes[i] = VARSIZE(DatumGetPointer(toast_values[i]));
      need_change = true;
      need_free = true;
    }
    else
    {
                                                                   
      toast_action[i] = 'x';
    }
  }

     
                                                                           
                                                                          
                                         
     
  maxDataLen = TOAST_TUPLE_TARGET_MAIN - hoff;

  while (heap_compute_data_size(tupleDesc, toast_values, toast_isnull) > maxDataLen && rel->rd_rel->reltoastrelid != InvalidOid)
  {
    int biggest_attno = -1;
    int32 biggest_size = MAXALIGN(TOAST_POINTER_SIZE);
    Datum old_value;

               
                                                         
                        
               
       
    for (i = 0; i < numAttrs; i++)
    {
      if (toast_action[i] == 'p')
      {
        continue;
      }
      if (VARATT_IS_EXTERNAL(DatumGetPointer(toast_values[i])))
      {
        continue;                                              
      }
      if (TupleDescAttr(tupleDesc, i)->attstorage != 'm')
      {
        continue;
      }
      if (toast_sizes[i] > biggest_size)
      {
        biggest_attno = i;
        biggest_size = toast_sizes[i];
      }
    }

    if (biggest_attno < 0)
    {
      break;
    }

       
                           
       
    i = biggest_attno;
    old_value = toast_values[i];
    toast_action[i] = 'p';
    toast_values[i] = toast_save_datum(rel, toast_values[i], toast_oldexternal[i], options);
    if (toast_free[i])
    {
      pfree(DatumGetPointer(old_value));
    }
    toast_free[i] = true;

    need_change = true;
    need_free = true;
  }

     
                                                                          
                              
     
  if (need_change)
  {
    HeapTupleHeader olddata = newtup->t_data;
    HeapTupleHeader new_data;
    int32 new_header_len;
    int32 new_data_len;
    int32 new_tuple_len;

       
                                            
       
                                                                           
                                                                        
                                                                         
                                                                      
                                                                       
                                             
       
    new_header_len = SizeofHeapTupleHeader;
    if (has_nulls)
    {
      new_header_len += BITMAPLEN(numAttrs);
    }
    new_header_len = MAXALIGN(new_header_len);
    new_data_len = heap_compute_data_size(tupleDesc, toast_values, toast_isnull);
    new_tuple_len = new_header_len + new_data_len;

       
                                                                          
       
    result_tuple = (HeapTuple)palloc0(HEAPTUPLESIZE + new_tuple_len);
    result_tuple->t_len = new_tuple_len;
    result_tuple->t_self = newtup->t_self;
    result_tuple->t_tableOid = newtup->t_tableOid;
    new_data = (HeapTupleHeader)((char *)result_tuple + HEAPTUPLESIZE);
    result_tuple->t_data = new_data;

       
                                                                    
       
    memcpy(new_data, olddata, SizeofHeapTupleHeader);
    HeapTupleHeaderSetNatts(new_data, numAttrs);
    new_data->t_hoff = new_header_len;

                                                                
    heap_fill_tuple(tupleDesc, toast_values, toast_isnull, (char *)new_data + new_header_len, new_data_len, &(new_data->t_infomask), has_nulls ? new_data->t_bits : NULL);
  }
  else
  {
    result_tuple = newtup;
  }

     
                                
     
  if (need_free)
  {
    for (i = 0; i < numAttrs; i++)
    {
      if (toast_free[i])
      {
        pfree(DatumGetPointer(toast_values[i]));
      }
    }
  }

     
                                               
     
  if (need_delold)
  {
    for (i = 0; i < numAttrs; i++)
    {
      if (toast_delold[i])
      {
        toast_delete_datum(rel, toast_oldvalues[i], false);
      }
    }
  }

  return result_tuple;
}

              
                         
   
                                                               
                                                                
   
                                                                         
                                                 
              
   
HeapTuple
toast_flatten_tuple(HeapTuple tup, TupleDesc tupleDesc)
{
  HeapTuple new_tuple;
  int numAttrs = tupleDesc->natts;
  int i;
  Datum toast_values[MaxTupleAttributeNumber];
  bool toast_isnull[MaxTupleAttributeNumber];
  bool toast_free[MaxTupleAttributeNumber];

     
                                       
     
  Assert(numAttrs <= MaxTupleAttributeNumber);
  heap_deform_tuple(tup, tupleDesc, toast_values, toast_isnull);

  memset(toast_free, 0, numAttrs * sizeof(bool));

  for (i = 0; i < numAttrs; i++)
  {
       
                                           
       
    if (!toast_isnull[i] && TupleDescAttr(tupleDesc, i)->attlen == -1)
    {
      struct varlena *new_value;

      new_value = (struct varlena *)DatumGetPointer(toast_values[i]);
      if (VARATT_IS_EXTERNAL(new_value))
      {
        new_value = heap_tuple_fetch_attr(new_value);
        toast_values[i] = PointerGetDatum(new_value);
        toast_free[i] = true;
      }
    }
  }

     
                                  
     
  new_tuple = heap_form_tuple(tupleDesc, toast_values, toast_isnull);

     
                                                                           
                                                                            
                       
     
  new_tuple->t_self = tup->t_self;
  new_tuple->t_tableOid = tup->t_tableOid;

  new_tuple->t_data->t_choice = tup->t_data->t_choice;
  new_tuple->t_data->t_ctid = tup->t_data->t_ctid;
  new_tuple->t_data->t_infomask &= ~HEAP_XACT_MASK;
  new_tuple->t_data->t_infomask |= tup->t_data->t_infomask & HEAP_XACT_MASK;
  new_tuple->t_data->t_infomask2 &= ~HEAP2_XACT_MASK;
  new_tuple->t_data->t_infomask2 |= tup->t_data->t_infomask2 & HEAP2_XACT_MASK;

     
                                
     
  for (i = 0; i < numAttrs; i++)
  {
    if (toast_free[i])
    {
      pfree(DatumGetPointer(toast_values[i]));
    }
  }

  return new_tuple;
}

              
                                  
   
                                                                         
                                                                
   
                                                                        
                                                                       
                                                                         
                                                                          
                                    
   
                                                                       
                                                                          
                                                          
   
                                                                            
                                                                           
                                                                       
                                                                           
                                                                         
                                                                           
                                
   
                                                                          
                                                                             
                                         
              
   
Datum
toast_flatten_tuple_to_datum(HeapTupleHeader tup, uint32 tup_len, TupleDesc tupleDesc)
{
  HeapTupleHeader new_data;
  int32 new_header_len;
  int32 new_data_len;
  int32 new_tuple_len;
  HeapTupleData tmptup;
  int numAttrs = tupleDesc->natts;
  int i;
  bool has_nulls = false;
  Datum toast_values[MaxTupleAttributeNumber];
  bool toast_isnull[MaxTupleAttributeNumber];
  bool toast_free[MaxTupleAttributeNumber];

                                                     
  tmptup.t_len = tup_len;
  ItemPointerSetInvalid(&(tmptup.t_self));
  tmptup.t_tableOid = InvalidOid;
  tmptup.t_data = tup;

     
                                       
     
  Assert(numAttrs <= MaxTupleAttributeNumber);
  heap_deform_tuple(&tmptup, tupleDesc, toast_values, toast_isnull);

  memset(toast_free, 0, numAttrs * sizeof(bool));

  for (i = 0; i < numAttrs; i++)
  {
       
                                           
       
    if (toast_isnull[i])
    {
      has_nulls = true;
    }
    else if (TupleDescAttr(tupleDesc, i)->attlen == -1)
    {
      struct varlena *new_value;

      new_value = (struct varlena *)DatumGetPointer(toast_values[i]);
      if (VARATT_IS_EXTERNAL(new_value) || VARATT_IS_COMPRESSED(new_value))
      {
        new_value = heap_tuple_untoast_attr(new_value);
        toast_values[i] = PointerGetDatum(new_value);
        toast_free[i] = true;
      }
    }
  }

     
                                          
     
                                                                          
     
  new_header_len = SizeofHeapTupleHeader;
  if (has_nulls)
  {
    new_header_len += BITMAPLEN(numAttrs);
  }
  new_header_len = MAXALIGN(new_header_len);
  new_data_len = heap_compute_data_size(tupleDesc, toast_values, toast_isnull);
  new_tuple_len = new_header_len + new_data_len;

  new_data = (HeapTupleHeader)palloc0(new_tuple_len);

     
                                                                  
     
  memcpy(new_data, tup, SizeofHeapTupleHeader);
  HeapTupleHeaderSetNatts(new_data, numAttrs);
  new_data->t_hoff = new_header_len;

                                                       
  HeapTupleHeaderSetDatumLength(new_data, new_tuple_len);
  HeapTupleHeaderSetTypeId(new_data, tupleDesc->tdtypeid);
  HeapTupleHeaderSetTypMod(new_data, tupleDesc->tdtypmod);

                                                              
  heap_fill_tuple(tupleDesc, toast_values, toast_isnull, (char *)new_data + new_header_len, new_data_len, &(new_data->t_infomask), has_nulls ? new_data->t_bits : NULL);

     
                                
     
  for (i = 0; i < numAttrs; i++)
  {
    if (toast_free[i])
    {
      pfree(DatumGetPointer(toast_values[i]));
    }
  }

  return PointerGetDatum(new_data);
}

              
                                 
   
                                                           
                                                                
   
                                                                      
                                                 
   
                                                                    
                                                              
              
   
HeapTuple
toast_build_flattened_tuple(TupleDesc tupleDesc, Datum *values, bool *isnull)
{
  HeapTuple new_tuple;
  int numAttrs = tupleDesc->natts;
  int num_to_free;
  int i;
  Datum new_values[MaxTupleAttributeNumber];
  Pointer freeable_values[MaxTupleAttributeNumber];

     
                                                                            
                                                     
     
  Assert(numAttrs <= MaxTupleAttributeNumber);
  memcpy(new_values, values, numAttrs * sizeof(Datum));

  num_to_free = 0;
  for (i = 0; i < numAttrs; i++)
  {
       
                                           
       
    if (!isnull[i] && TupleDescAttr(tupleDesc, i)->attlen == -1)
    {
      struct varlena *new_value;

      new_value = (struct varlena *)DatumGetPointer(new_values[i]);
      if (VARATT_IS_EXTERNAL(new_value))
      {
        new_value = heap_tuple_fetch_attr(new_value);
        new_values[i] = PointerGetDatum(new_value);
        freeable_values[num_to_free++] = (Pointer)new_value;
      }
    }
  }

     
                                  
     
  new_tuple = heap_form_tuple(tupleDesc, new_values, isnull);

     
                                
     
  for (i = 0; i < num_to_free; i++)
  {
    pfree(freeable_values[i]);
  }

  return new_tuple;
}

              
                          
   
                                                  
   
                                                                       
                                                                     
              
   
                                                                          
                                                                     
              
   
Datum
toast_compress_datum(Datum value)
{
  struct varlena *tmp;
  int32 valsize = VARSIZE_ANY_EXHDR(DatumGetPointer(value));
  int32 len;

  Assert(!VARATT_IS_EXTERNAL(DatumGetPointer(value)));
  Assert(!VARATT_IS_COMPRESSED(DatumGetPointer(value)));

     
                                                                            
                           
     
  if (valsize < PGLZ_strategy_default->min_input_size || valsize > PGLZ_strategy_default->max_input_size)
  {
    return PointerGetDatum(NULL);
  }

  tmp = (struct varlena *)palloc(PGLZ_MAX_OUTPUT(valsize) + TOAST_COMPRESS_HDRSZ);

     
                                                                         
                                                                           
                                                                          
                                                                        
                                                                     
                                                                           
                                                                           
                                                                           
     
  len = pglz_compress(VARDATA_ANY(DatumGetPointer(value)), valsize, TOAST_COMPRESS_RAWDATA(tmp), PGLZ_strategy_default);
  if (len >= 0 && len + TOAST_COMPRESS_HDRSZ < valsize - 2)
  {
    TOAST_COMPRESS_SET_RAWSIZE(tmp, valsize);
    SET_VARSIZE_COMPRESSED(tmp, len + TOAST_COMPRESS_HDRSZ);
                                
    return PointerGetDatum(tmp);
  }
  else
  {
                             
    pfree(tmp);
    return PointerGetDatum(NULL);
  }
}

              
                         
   
                                                                      
                                                            
   
Oid
toast_get_valid_index(Oid toastoid, LOCKMODE lock)
{
  int num_indexes;
  int validIndex;
  Oid validIndexOid;
  Relation *toastidxs;
  Relation toastrel;

                               
  toastrel = table_open(toastoid, lock);

                                                      
  validIndex = toast_open_indexes(toastrel, lock, &toastidxs, &num_indexes);
  validIndexOid = RelationGetRelid(toastidxs[validIndex]);

                                                    
  toast_close_indexes(toastidxs, num_indexes, NoLock);
  table_close(toastrel, NoLock);

  return validIndexOid;
}

              
                      
   
                                                                
                             
   
                                                                  
                                              
                                                                             
                                                                 
              
   
static Datum
toast_save_datum(Relation rel, Datum value, struct varlena *oldexternal, int options)
{
  Relation toastrel;
  Relation *toastidxs;
  HeapTuple toasttup;
  TupleDesc toasttupDesc;
  Datum t_values[3];
  bool t_isnull[3];
  CommandId mycid = GetCurrentCommandId(true);
  struct varlena *result;
  struct varatt_external toast_pointer;
  union
  {
    struct varlena hdr;
                                                           
    char data[TOAST_MAX_CHUNK_SIZE + VARHDRSZ];
                                              
    int32 align_it;
  } chunk_data;
  int32 chunk_size;
  int32 chunk_seq = 0;
  char *data_p;
  int32 data_todo;
  Pointer dval = DatumGetPointer(value);
  int num_indexes;
  int validIndex;

  Assert(!VARATT_IS_EXTERNAL(value));

     
                                                                             
                                                                             
                                     
     
  toastrel = table_open(rel->rd_rel->reltoastrelid, RowExclusiveLock);
  toasttupDesc = toastrel->rd_att;

                                                             
  validIndex = toast_open_indexes(toastrel, RowExclusiveLock, &toastidxs, &num_indexes);

     
                                                                             
     
                                                                           
                                          
     
                                                                             
     
  if (VARATT_IS_SHORT(dval))
  {
    data_p = VARDATA_SHORT(dval);
    data_todo = VARSIZE_SHORT(dval) - VARHDRSZ_SHORT;
    toast_pointer.va_rawsize = data_todo + VARHDRSZ;                      
    toast_pointer.va_extsize = data_todo;
  }
  else if (VARATT_IS_COMPRESSED(dval))
  {
    data_p = VARDATA(dval);
    data_todo = VARSIZE(dval) - VARHDRSZ;
                                                                       
    toast_pointer.va_rawsize = VARRAWSIZE_4B_C(dval) + VARHDRSZ;
    toast_pointer.va_extsize = data_todo;
                                                           
    Assert(VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer));
  }
  else
  {
    data_p = VARDATA(dval);
    data_todo = VARSIZE(dval) - VARHDRSZ;
    toast_pointer.va_rawsize = VARSIZE(dval);
    toast_pointer.va_extsize = data_todo;
  }

     
                                                                 
     
                                                                           
                                                                           
                                                                            
                                           
     
  if (OidIsValid(rel->rd_toastoid))
  {
    toast_pointer.va_toastrelid = rel->rd_toastoid;
  }
  else
  {
    toast_pointer.va_toastrelid = RelationGetRelid(toastrel);
  }

     
                                                                
     
                                                                        
                                                                           
                                                                        
                                                                         
                                                                           
                                                                           
                                                                         
                                                            
     
  if (!OidIsValid(rel->rd_toastoid))
  {
                                                
    toast_pointer.va_valueid = GetNewOidWithIndex(toastrel, RelationGetRelid(toastidxs[validIndex]), (AttrNumber)1);
  }
  else
  {
                                                                    
    toast_pointer.va_valueid = InvalidOid;
    if (oldexternal != NULL)
    {
      struct varatt_external old_toast_pointer;

      Assert(VARATT_IS_EXTERNAL_ONDISK(oldexternal));
                                              
      VARATT_EXTERNAL_GET_POINTER(old_toast_pointer, oldexternal);
      if (old_toast_pointer.va_toastrelid == rel->rd_toastoid)
      {
                                                                     
        toast_pointer.va_valueid = old_toast_pointer.va_valueid;

           
                                                                     
                                                                      
                                                                       
                                                                   
                                                                     
                                                                    
                                                        
           
                                                                 
                                                                    
                                                                  
                                                                      
                                                                 
                                                                     
                                   
           
        if (toastrel_valueid_exists(toastrel, toast_pointer.va_valueid))
        {
                                                                   
          data_todo = 0;
        }
      }
    }
    if (toast_pointer.va_valueid == InvalidOid)
    {
         
                                                                       
                                
         
      do
      {
        toast_pointer.va_valueid = GetNewOidWithIndex(toastrel, RelationGetRelid(toastidxs[validIndex]), (AttrNumber)1);
      } while (toastid_valueid_exists(rel->rd_toastoid, toast_pointer.va_valueid));
    }
  }

     
                                                 
     
  t_values[0] = ObjectIdGetDatum(toast_pointer.va_valueid);
  t_values[2] = PointerGetDatum(&chunk_data);
  t_isnull[0] = false;
  t_isnull[1] = false;
  t_isnull[2] = false;

     
                                   
     
  while (data_todo > 0)
  {
    int i;

    CHECK_FOR_INTERRUPTS();

       
                                        
       
    chunk_size = Min(TOAST_MAX_CHUNK_SIZE, data_todo);

       
                                  
       
    t_values[1] = Int32GetDatum(chunk_seq++);
    SET_VARSIZE(&chunk_data, chunk_size + VARHDRSZ);
    memcpy(VARDATA(&chunk_data), data_p, chunk_size);
    toasttup = heap_form_tuple(toasttupDesc, t_values, t_isnull);

    heap_insert(toastrel, toasttup, mycid, options, NULL);

       
                                                                    
                                                                           
                                                                    
                                                                           
                                                                       
                            
       
                                                                        
                                                                       
       
    for (i = 0; i < num_indexes; i++)
    {
                                                               
      if (toastidxs[i]->rd_index->indisready)
      {
        index_insert(toastidxs[i], t_values, t_isnull, &(toasttup->t_self), toastrel, toastidxs[i]->rd_index->indisunique ? UNIQUE_CHECK_YES : UNIQUE_CHECK_NO, NULL);
      }
    }

       
                   
       
    heap_freetuple(toasttup);

       
                             
       
    data_todo -= chunk_size;
    data_p += chunk_size;
  }

     
                                                                         
                                                                            
                                                 
     
  toast_close_indexes(toastidxs, num_indexes, NoLock);
  table_close(toastrel, NoLock);

     
                                                      
     
  result = (struct varlena *)palloc(TOAST_POINTER_SIZE);
  SET_VARTAG_EXTERNAL(result, VARTAG_ONDISK);
  memcpy(VARDATA_EXTERNAL(result), &toast_pointer, sizeof(toast_pointer));

  return PointerGetDatum(result);
}

              
                        
   
                                          
              
   
static void
toast_delete_datum(Relation rel, Datum value, bool is_speculative)
{
  struct varlena *attr = (struct varlena *)DatumGetPointer(value);
  struct varatt_external toast_pointer;
  Relation toastrel;
  Relation *toastidxs;
  ScanKeyData toastkey;
  SysScanDesc toastscan;
  HeapTuple toasttup;
  int num_indexes;
  int validIndex;
  SnapshotData SnapshotToast;

  if (!VARATT_IS_EXTERNAL_ONDISK(attr))
  {
    return;
  }

                                          
  VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

     
                                             
     
  toastrel = table_open(toast_pointer.va_toastrelid, RowExclusiveLock);

                                             
  validIndex = toast_open_indexes(toastrel, RowExclusiveLock, &toastidxs, &num_indexes);

     
                                                              
     
  ScanKeyInit(&toastkey, (AttrNumber)1, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(toast_pointer.va_valueid));

     
                                                                          
                                                                           
                                           
     
  init_toast_snapshot(&SnapshotToast);
  toastscan = systable_beginscan_ordered(toastrel, toastidxs[validIndex], &SnapshotToast, 1, &toastkey);
  while ((toasttup = systable_getnext_ordered(toastscan, ForwardScanDirection)) != NULL)
  {
       
                               
       
    if (is_speculative)
    {
      heap_abort_speculative(toastrel, &toasttup->t_self);
    }
    else
    {
      simple_heap_delete(toastrel, &toasttup->t_self);
    }
  }

     
                                                                          
                                                                             
                                
     
  systable_endscan_ordered(toastscan);
  toast_close_indexes(toastidxs, num_indexes, NoLock);
  table_close(toastrel, NoLock);
}

              
                             
   
                                                                              
                                                                             
                                                                
              
   
static bool
toastrel_valueid_exists(Relation toastrel, Oid valueid)
{
  bool result = false;
  ScanKeyData toastkey;
  SysScanDesc toastscan;
  int num_indexes;
  int validIndex;
  Relation *toastidxs;

                                    
  validIndex = toast_open_indexes(toastrel, RowExclusiveLock, &toastidxs, &num_indexes);

     
                                                              
     
  ScanKeyInit(&toastkey, (AttrNumber)1, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(valueid));

     
                              
     
  toastscan = systable_beginscan(toastrel, RelationGetRelid(toastidxs[validIndex]), true, SnapshotAny, 1, &toastkey);

  if (systable_getnext(toastscan) != NULL)
  {
    result = true;
  }

  systable_endscan(toastscan);

                
  toast_close_indexes(toastidxs, num_indexes, RowExclusiveLock);

  return result;
}

              
                            
   
                                                                
              
   
static bool
toastid_valueid_exists(Oid toastrelid, Oid valueid)
{
  bool result;
  Relation toastrel;

  toastrel = table_open(toastrelid, AccessShareLock);

  result = toastrel_valueid_exists(toastrel, valueid);

  table_close(toastrel, AccessShareLock);

  return result;
}

              
                       
   
                                                        
                         
              
   
static struct varlena *
toast_fetch_datum(struct varlena *attr)
{
  Relation toastrel;
  Relation *toastidxs;
  ScanKeyData toastkey;
  SysScanDesc toastscan;
  HeapTuple ttup;
  TupleDesc toasttupDesc;
  struct varlena *result;
  struct varatt_external toast_pointer;
  int32 ressize;
  int32 residx, nextidx;
  int32 numchunks;
  Pointer chunk;
  bool isnull;
  char *chunkdata;
  int32 chunksize;
  int num_indexes;
  int validIndex;
  SnapshotData SnapshotToast;

  if (!VARATT_IS_EXTERNAL_ONDISK(attr))
  {
    elog(ERROR, "toast_fetch_datum shouldn't be called for non-ondisk datums");
  }

                                          
  VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

  ressize = toast_pointer.va_extsize;
  numchunks = ((ressize - 1) / TOAST_MAX_CHUNK_SIZE) + 1;

  result = (struct varlena *)palloc(ressize + VARHDRSZ);

  if (VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer))
  {
    SET_VARSIZE_COMPRESSED(result, ressize + VARHDRSZ);
  }
  else
  {
    SET_VARSIZE(result, ressize + VARHDRSZ);
  }

     
                                             
     
  toastrel = table_open(toast_pointer.va_toastrelid, AccessShareLock);
  toasttupDesc = toastrel->rd_att;

                                                      
  validIndex = toast_open_indexes(toastrel, AccessShareLock, &toastidxs, &num_indexes);

     
                                                            
     
  ScanKeyInit(&toastkey, (AttrNumber)1, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(toast_pointer.va_valueid));

     
                              
     
                                                                            
                                                                            
             
     
  nextidx = 0;

  init_toast_snapshot(&SnapshotToast);
  toastscan = systable_beginscan_ordered(toastrel, toastidxs[validIndex], &SnapshotToast, 1, &toastkey);
  while ((ttup = systable_getnext_ordered(toastscan, ForwardScanDirection)) != NULL)
  {
       
                                                              
       
    residx = DatumGetInt32(fastgetattr(ttup, 2, toasttupDesc, &isnull));
    Assert(!isnull);
    chunk = DatumGetPointer(fastgetattr(ttup, 3, toasttupDesc, &isnull));
    Assert(!isnull);
    if (!VARATT_IS_EXTENDED(chunk))
    {
      chunksize = VARSIZE(chunk) - VARHDRSZ;
      chunkdata = VARDATA(chunk);
    }
    else if (VARATT_IS_SHORT(chunk))
    {
                                                               
      chunksize = VARSIZE_SHORT(chunk) - VARHDRSZ_SHORT;
      chunkdata = VARDATA_SHORT(chunk);
    }
    else
    {
                               
      elog(ERROR, "found toasted toast chunk for toast value %u in %s", toast_pointer.va_valueid, RelationGetRelationName(toastrel));
      chunksize = 0;                          
      chunkdata = NULL;
    }

       
                                           
       
    if (residx != nextidx)
    {
      elog(ERROR, "unexpected chunk number %d (expected %d) for toast value %u in %s", residx, nextidx, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
    }
    if (residx < numchunks - 1)
    {
      if (chunksize != TOAST_MAX_CHUNK_SIZE)
      {
        elog(ERROR, "unexpected chunk size %d (expected %d) in chunk %d of %d for toast value %u in %s", chunksize, (int)TOAST_MAX_CHUNK_SIZE, residx, numchunks, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
      }
    }
    else if (residx == numchunks - 1)
    {
      if ((residx * TOAST_MAX_CHUNK_SIZE + chunksize) != ressize)
      {
        elog(ERROR, "unexpected chunk size %d (expected %d) in final chunk %d for toast value %u in %s", chunksize, (int)(ressize - residx * TOAST_MAX_CHUNK_SIZE), residx, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
      }
    }
    else
    {
      elog(ERROR, "unexpected chunk number %d (out of range %d..%d) for toast value %u in %s", residx, 0, numchunks - 1, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
    }

       
                                                     
       
    memcpy(VARDATA(result) + residx * TOAST_MAX_CHUNK_SIZE, chunkdata, chunksize);

    nextidx++;
  }

     
                                                         
     
  if (nextidx != numchunks)
  {
    elog(ERROR, "missing chunk number %d for toast value %u in %s", nextidx, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
  }

     
                                  
     
  systable_endscan_ordered(toastscan);
  toast_close_indexes(toastidxs, num_indexes, AccessShareLock);
  table_close(toastrel, AccessShareLock);

  return result;
}

              
                             
   
                                                          
                         
   
                                                                         
              
   
static struct varlena *
toast_fetch_datum_slice(struct varlena *attr, int32 sliceoffset, int32 length)
{
  Relation toastrel;
  Relation *toastidxs;
  ScanKeyData toastkey[3];
  int nscankeys;
  SysScanDesc toastscan;
  HeapTuple ttup;
  TupleDesc toasttupDesc;
  struct varlena *result;
  struct varatt_external toast_pointer;
  int32 attrsize;
  int32 residx;
  int32 nextidx;
  int numchunks;
  int startchunk;
  int endchunk;
  int32 startoffset;
  int32 endoffset;
  int totalchunks;
  Pointer chunk;
  bool isnull;
  char *chunkdata;
  int32 chunksize;
  int32 chcpystrt;
  int32 chcpyend;
  int num_indexes;
  int validIndex;
  SnapshotData SnapshotToast;

  if (!VARATT_IS_EXTERNAL_ONDISK(attr))
  {
    elog(ERROR, "toast_fetch_datum_slice shouldn't be called for non-ondisk datums");
  }

                                          
  VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

     
                                                                            
                                                                           
     
  Assert(!VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer));

  attrsize = toast_pointer.va_extsize;
  totalchunks = ((attrsize - 1) / TOAST_MAX_CHUNK_SIZE) + 1;

  if (sliceoffset >= attrsize)
  {
    sliceoffset = 0;
    length = 0;
  }

     
                                                               
                                                                             
                   
     
  else if (((sliceoffset + length) > attrsize) || length < 0)
  {
    length = attrsize - sliceoffset;
  }

  result = (struct varlena *)palloc(length + VARHDRSZ);

  SET_VARSIZE(result, length + VARHDRSZ);

  if (length == 0)
  {
    return result;                                            
  }

  startchunk = sliceoffset / TOAST_MAX_CHUNK_SIZE;
  endchunk = (sliceoffset + length - 1) / TOAST_MAX_CHUNK_SIZE;
  numchunks = (endchunk - startchunk) + 1;

  startoffset = sliceoffset % TOAST_MAX_CHUNK_SIZE;
  endoffset = (sliceoffset + length - 1) % TOAST_MAX_CHUNK_SIZE;

     
                                             
     
  toastrel = table_open(toast_pointer.va_toastrelid, AccessShareLock);
  toasttupDesc = toastrel->rd_att;

                                                  
  validIndex = toast_open_indexes(toastrel, AccessShareLock, &toastidxs, &num_indexes);

     
                                                                          
                                              
     
  ScanKeyInit(&toastkey[0], (AttrNumber)1, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(toast_pointer.va_valueid));

     
                                                                        
     
  if (numchunks == 1)
  {
    ScanKeyInit(&toastkey[1], (AttrNumber)2, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(startchunk));
    nscankeys = 2;
  }
  else
  {
    ScanKeyInit(&toastkey[1], (AttrNumber)2, BTGreaterEqualStrategyNumber, F_INT4GE, Int32GetDatum(startchunk));
    ScanKeyInit(&toastkey[2], (AttrNumber)2, BTLessEqualStrategyNumber, F_INT4LE, Int32GetDatum(endchunk));
    nscankeys = 3;
  }

     
                              
     
                                                                    
     
  init_toast_snapshot(&SnapshotToast);
  nextidx = startchunk;
  toastscan = systable_beginscan_ordered(toastrel, toastidxs[validIndex], &SnapshotToast, nscankeys, toastkey);
  while ((ttup = systable_getnext_ordered(toastscan, ForwardScanDirection)) != NULL)
  {
       
                                                              
       
    residx = DatumGetInt32(fastgetattr(ttup, 2, toasttupDesc, &isnull));
    Assert(!isnull);
    chunk = DatumGetPointer(fastgetattr(ttup, 3, toasttupDesc, &isnull));
    Assert(!isnull);
    if (!VARATT_IS_EXTENDED(chunk))
    {
      chunksize = VARSIZE(chunk) - VARHDRSZ;
      chunkdata = VARDATA(chunk);
    }
    else if (VARATT_IS_SHORT(chunk))
    {
                                                               
      chunksize = VARSIZE_SHORT(chunk) - VARHDRSZ_SHORT;
      chunkdata = VARDATA_SHORT(chunk);
    }
    else
    {
                               
      elog(ERROR, "found toasted toast chunk for toast value %u in %s", toast_pointer.va_valueid, RelationGetRelationName(toastrel));
      chunksize = 0;                          
      chunkdata = NULL;
    }

       
                                           
       
    if ((residx != nextidx) || (residx > endchunk) || (residx < startchunk))
    {
      elog(ERROR, "unexpected chunk number %d (expected %d) for toast value %u in %s", residx, nextidx, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
    }
    if (residx < totalchunks - 1)
    {
      if (chunksize != TOAST_MAX_CHUNK_SIZE)
      {
        elog(ERROR, "unexpected chunk size %d (expected %d) in chunk %d of %d for toast value %u in %s when fetching slice", chunksize, (int)TOAST_MAX_CHUNK_SIZE, residx, totalchunks, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
      }
    }
    else if (residx == totalchunks - 1)
    {
      if ((residx * TOAST_MAX_CHUNK_SIZE + chunksize) != attrsize)
      {
        elog(ERROR, "unexpected chunk size %d (expected %d) in final chunk %d for toast value %u in %s when fetching slice", chunksize, (int)(attrsize - residx * TOAST_MAX_CHUNK_SIZE), residx, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
      }
    }
    else
    {
      elog(ERROR, "unexpected chunk number %d (out of range %d..%d) for toast value %u in %s", residx, 0, totalchunks - 1, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
    }

       
                                                     
       
    chcpystrt = 0;
    chcpyend = chunksize - 1;
    if (residx == startchunk)
    {
      chcpystrt = startoffset;
    }
    if (residx == endchunk)
    {
      chcpyend = endoffset;
    }

    memcpy(VARDATA(result) + (residx * TOAST_MAX_CHUNK_SIZE - sliceoffset) + chcpystrt, chunkdata + chcpystrt, (chcpyend - chcpystrt) + 1);

    nextidx++;
  }

     
                                                         
     
  if (nextidx != (endchunk + 1))
  {
    elog(ERROR, "missing chunk number %d for toast value %u in %s", nextidx, toast_pointer.va_valueid, RelationGetRelationName(toastrel));
  }

     
                                  
     
  systable_endscan_ordered(toastscan);
  toast_close_indexes(toastidxs, num_indexes, AccessShareLock);
  table_close(toastrel, AccessShareLock);

  return result;
}

              
                            
   
                                                      
   
static struct varlena *
toast_decompress_datum(struct varlena *attr)
{
  struct varlena *result;

  Assert(VARATT_IS_COMPRESSED(attr));

  result = (struct varlena *)palloc(TOAST_COMPRESS_RAWSIZE(attr) + VARHDRSZ);
  SET_VARSIZE(result, TOAST_COMPRESS_RAWSIZE(attr) + VARHDRSZ);

  if (pglz_decompress(TOAST_COMPRESS_RAWDATA(attr), VARSIZE(attr) - TOAST_COMPRESS_HDRSZ, VARDATA(result), TOAST_COMPRESS_RAWSIZE(attr), true) < 0)
  {
    elog(ERROR, "compressed data is corrupted");
  }

  return result;
}

              
                                  
   
                                                                    
                                                             
                                                   
   
static struct varlena *
toast_decompress_datum_slice(struct varlena *attr, int32 slicelength)
{
  struct varlena *result;
  int32 rawsize;

  Assert(VARATT_IS_COMPRESSED(attr));

  result = (struct varlena *)palloc(slicelength + VARHDRSZ);

  rawsize = pglz_decompress(TOAST_COMPRESS_RAWDATA(attr), VARSIZE(attr) - TOAST_COMPRESS_HDRSZ, VARDATA(result), slicelength, false);
  if (rawsize < 0)
  {
    elog(ERROR, "compressed data is corrupted");
  }

  SET_VARSIZE(result, rawsize + VARHDRSZ);
  return result;
}

              
                      
   
                                                                      
                                                                        
                                                                          
                                                       
   
static int
toast_open_indexes(Relation toastrel, LOCKMODE lock, Relation **toastidxs, int *num_indexes)
{
  int i = 0;
  int res = 0;
  bool found = false;
  List *indexlist;
  ListCell *lc;

                                            
  indexlist = RelationGetIndexList(toastrel);
  Assert(indexlist != NIL);

  *num_indexes = list_length(indexlist);

                                    
  *toastidxs = (Relation *)palloc(*num_indexes * sizeof(Relation));
  foreach (lc, indexlist)
  {
    (*toastidxs)[i++] = index_open(lfirst_oid(lc), lock);
  }

                                           
  for (i = 0; i < *num_indexes; i++)
  {
    Relation toastidx = (*toastidxs)[i];

    if (toastidx->rd_index->indisvalid)
    {
      res = i;
      found = true;
      break;
    }
  }

     
                                                                          
                                 
     
  list_free(indexlist);

     
                                                                           
                                
     
  if (!found)
  {
    elog(ERROR, "no valid index found for toast relation with Oid %u", RelationGetRelid(toastrel));
  }

  return res;
}

              
                       
   
                                                                           
                                                                             
   
static void
toast_close_indexes(Relation *toastidxs, int num_indexes, LOCKMODE lock)
{
  int i;

                                           
  for (i = 0; i < num_indexes; i++)
  {
    index_close(toastidxs[i], lock);
  }
  pfree(toastidxs);
}

              
                       
   
                                                                           
                                                                           
                                                                             
                                                          
   
static void
init_toast_snapshot(Snapshot toast_snapshot)
{
  Snapshot snapshot = GetOldestSnapshot();

     
                                                                            
                                                                          
                                                                          
                                                                       
                                                                             
                                                                             
                                                                             
                                                                          
                                                                          
                                                                             
                                                            
     
  if (snapshot == NULL)
  {
    elog(ERROR, "cannot fetch toast data without an active snapshot");
  }

  InitToastSnapshot(*toast_snapshot, snapshot->lsn, snapshot->whenTaken);
}
