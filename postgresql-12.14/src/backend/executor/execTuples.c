                                                                            
   
                
                                                                         
                                                                      
                                                                         
                                                                            
                                                       
   
                                                                       
                                                                            
                                                                
                                      
   
   
                                       
                                                                         
                                             
   
                       
                     
   
                                                                     
                                                                        
                                                         
                                                                      
                                                                      
                               
   
                         
                     
                                                                    
                                                              
   
                                                                       
                                                                        
                                                                         
   
                                               
   
                                                                      
                                                                    
                                                                           
                                                                           
                                                                       
                                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupdesc_details.h"
#include "access/tuptoaster.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

static TupleDesc
ExecTypeFromTLInternal(List *targetList, bool skipjunk);
static pg_attribute_always_inline void
slot_deform_heap_tuple(TupleTableSlot *slot, HeapTuple tuple, uint32 *offp, int natts);
static inline void
tts_buffer_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, Buffer buffer, bool transfer_pin);
static void
tts_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, bool shouldFree);

const TupleTableSlotOps TTSOpsVirtual;
const TupleTableSlotOps TTSOpsHeapTuple;
const TupleTableSlotOps TTSOpsMinimalTuple;
const TupleTableSlotOps TTSOpsBufferHeapTuple;

   
                                      
   

   
                                                               
   
static void
tts_virtual_init(TupleTableSlot *slot)
{
}

static void
tts_virtual_release(TupleTableSlot *slot)
{
}

static void
tts_virtual_clear(TupleTableSlot *slot)
{
  if (unlikely(TTS_SHOULDFREE(slot)))
  {
    VirtualTupleTableSlot *vslot = (VirtualTupleTableSlot *)slot;

    pfree(vslot->data);
    vslot->data = NULL;

    slot->tts_flags &= ~TTS_FLAG_SHOULDFREE;
  }

  slot->tts_nvalid = 0;
  slot->tts_flags |= TTS_FLAG_EMPTY;
  ItemPointerSetInvalid(&slot->tts_tid);
}

   
                                                                     
                                                                
   
static void
tts_virtual_getsomeattrs(TupleTableSlot *slot, int natts)
{
  elog(ERROR, "getsomeattrs is not required to be called on a virtual tuple table slot");
}

   
                                                                        
                                                                       
                                                       
   
static Datum
tts_virtual_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{
  Assert(!TTS_EMPTY(slot));

  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot retrieve a system column in this context")));

  return 0;                                
}

   
                                                                            
                                                                            
                                                                              
                                                                       
                            
   
static void
tts_virtual_materialize(TupleTableSlot *slot)
{
  VirtualTupleTableSlot *vslot = (VirtualTupleTableSlot *)slot;
  TupleDesc desc = slot->tts_tupleDescriptor;
  Size sz = 0;
  char *data;

                            
  if (TTS_SHOULDFREE(slot))
  {
    return;
  }

                                       
  for (int natt = 0; natt < desc->natts; natt++)
  {
    Form_pg_attribute att = TupleDescAttr(desc, natt);
    Datum val;

    if (att->attbyval || slot->tts_isnull[natt])
    {
      continue;
    }

    val = slot->tts_values[natt];

    if (att->attlen == -1 && VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(val)))
    {
         
                                                                        
                                    
         
      sz = att_align_nominal(sz, att->attalign);
      sz += EOH_get_flat_size(DatumGetEOHP(val));
    }
    else
    {
      sz = att_align_nominal(sz, att->attalign);
      sz = att_addlength_datum(sz, att->attlen, val);
    }
  }

                         
  if (sz == 0)
  {
    return;
  }

                       
  vslot->data = data = MemoryContextAlloc(slot->tts_mcxt, sz);
  slot->tts_flags |= TTS_FLAG_SHOULDFREE;

                                                            
  for (int natt = 0; natt < desc->natts; natt++)
  {
    Form_pg_attribute att = TupleDescAttr(desc, natt);
    Datum val;

    if (att->attbyval || slot->tts_isnull[natt])
    {
      continue;
    }

    val = slot->tts_values[natt];

    if (att->attlen == -1 && VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(val)))
    {
      Size data_length;

         
                                                                        
                                    
         
      ExpandedObjectHeader *eoh = DatumGetEOHP(val);

      data = (char *)att_align_nominal(data, att->attalign);
      data_length = EOH_get_flat_size(eoh);
      EOH_flatten_into(eoh, data, data_length);

      slot->tts_values[natt] = PointerGetDatum(data);
      data += data_length;
    }
    else
    {
      Size data_length = 0;

      data = (char *)att_align_nominal(data, att->attalign);
      data_length = att_addlength_datum(data_length, att->attlen, val);

      memcpy(data, DatumGetPointer(val), data_length);

      slot->tts_values[natt] = PointerGetDatum(data);
      data += data_length;
    }
  }
}

static void
tts_virtual_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{
  TupleDesc srcdesc = srcslot->tts_tupleDescriptor;

  Assert(srcdesc->natts <= dstslot->tts_tupleDescriptor->natts);

  tts_virtual_clear(dstslot);

  slot_getallattrs(srcslot);

  for (int natt = 0; natt < srcdesc->natts; natt++)
  {
    dstslot->tts_values[natt] = srcslot->tts_values[natt];
    dstslot->tts_isnull[natt] = srcslot->tts_isnull[natt];
  }

  dstslot->tts_nvalid = srcdesc->natts;
  dstslot->tts_flags &= ~TTS_FLAG_EMPTY;

                                                           
  tts_virtual_materialize(dstslot);
}

static HeapTuple
tts_virtual_copy_heap_tuple(TupleTableSlot *slot)
{
  Assert(!TTS_EMPTY(slot));

  return heap_form_tuple(slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
}

static MinimalTuple
tts_virtual_copy_minimal_tuple(TupleTableSlot *slot)
{
  Assert(!TTS_EMPTY(slot));

  return heap_form_minimal_tuple(slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
}

   
                                                            
   

static void
tts_heap_init(TupleTableSlot *slot)
{
}

static void
tts_heap_release(TupleTableSlot *slot)
{
}

static void
tts_heap_clear(TupleTableSlot *slot)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

                                                           
  if (TTS_SHOULDFREE(slot))
  {
    heap_freetuple(hslot->tuple);
    slot->tts_flags &= ~TTS_FLAG_SHOULDFREE;
  }

  slot->tts_nvalid = 0;
  slot->tts_flags |= TTS_FLAG_EMPTY;
  ItemPointerSetInvalid(&slot->tts_tid);
  hslot->off = 0;
  hslot->tuple = NULL;
}

static void
tts_heap_getsomeattrs(TupleTableSlot *slot, int natts)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

  slot_deform_heap_tuple(slot, hslot->tuple, &hslot->off, natts);
}

static Datum
tts_heap_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

     
                                                                          
                                                           
     
  if (!hslot->tuple)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot retrieve a system column in this context")));
  }

  return heap_getsysattr(hslot->tuple, attnum, slot->tts_tupleDescriptor, isnull);
}

static void
tts_heap_materialize(TupleTableSlot *slot)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;
  MemoryContext oldContext;

  Assert(!TTS_EMPTY(slot));

                                                                  
  if (TTS_SHOULDFREE(slot))
  {
    return;
  }

  oldContext = MemoryContextSwitchTo(slot->tts_mcxt);

     
                                                                             
                                                                          
     
  slot->tts_nvalid = 0;
  hslot->off = 0;

  if (!hslot->tuple)
  {
    hslot->tuple = heap_form_tuple(slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
  }
  else
  {
       
                                                                       
                                                                          
                                                            
       
    hslot->tuple = heap_copytuple(hslot->tuple);
  }

  slot->tts_flags |= TTS_FLAG_SHOULDFREE;

  MemoryContextSwitchTo(oldContext);
}

static void
tts_heap_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{
  HeapTuple tuple;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(dstslot->tts_mcxt);
  tuple = ExecCopySlotHeapTuple(srcslot);
  MemoryContextSwitchTo(oldcontext);

  ExecStoreHeapTuple(tuple, dstslot, true);
}

static HeapTuple
tts_heap_get_heap_tuple(TupleTableSlot *slot)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));
  if (!hslot->tuple)
  {
    tts_heap_materialize(slot);
  }

  return hslot->tuple;
}

static HeapTuple
tts_heap_copy_heap_tuple(TupleTableSlot *slot)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));
  if (!hslot->tuple)
  {
    tts_heap_materialize(slot);
  }

  return heap_copytuple(hslot->tuple);
}

static MinimalTuple
tts_heap_copy_minimal_tuple(TupleTableSlot *slot)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

  if (!hslot->tuple)
  {
    tts_heap_materialize(slot);
  }

  return minimal_tuple_from_heap_tuple(hslot->tuple);
}

static void
tts_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, bool shouldFree)
{
  HeapTupleTableSlot *hslot = (HeapTupleTableSlot *)slot;

  tts_heap_clear(slot);

  slot->tts_nvalid = 0;
  hslot->tuple = tuple;
  hslot->off = 0;
  slot->tts_flags &= ~(TTS_FLAG_EMPTY | TTS_FLAG_SHOULDFREE);
  slot->tts_tid = tuple->t_self;

  if (shouldFree)
  {
    slot->tts_flags |= TTS_FLAG_SHOULDFREE;
  }
}

   
                                                               
   

static void
tts_minimal_init(TupleTableSlot *slot)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

     
                                                                           
                                                         
     
  mslot->tuple = &mslot->minhdr;
}

static void
tts_minimal_release(TupleTableSlot *slot)
{
}

static void
tts_minimal_clear(TupleTableSlot *slot)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

  if (TTS_SHOULDFREE(slot))
  {
    heap_free_minimal_tuple(mslot->mintuple);
    slot->tts_flags &= ~TTS_FLAG_SHOULDFREE;
  }

  slot->tts_nvalid = 0;
  slot->tts_flags |= TTS_FLAG_EMPTY;
  ItemPointerSetInvalid(&slot->tts_tid);
  mslot->off = 0;
  mslot->mintuple = NULL;
}

static void
tts_minimal_getsomeattrs(TupleTableSlot *slot, int natts)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

  slot_deform_heap_tuple(slot, mslot->tuple, &mslot->off, natts);
}

static Datum
tts_minimal_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{
  Assert(!TTS_EMPTY(slot));

  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot retrieve a system column in this context")));

  return 0;                                
}

static void
tts_minimal_materialize(TupleTableSlot *slot)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;
  MemoryContext oldContext;

  Assert(!TTS_EMPTY(slot));

                                                                  
  if (TTS_SHOULDFREE(slot))
  {
    return;
  }

  oldContext = MemoryContextSwitchTo(slot->tts_mcxt);

     
                                                                             
                                                                          
     
  slot->tts_nvalid = 0;
  mslot->off = 0;

  if (!mslot->mintuple)
  {
    mslot->mintuple = heap_form_minimal_tuple(slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
  }
  else
  {
       
                                                                        
                                                                           
                                                                           
       
    mslot->mintuple = heap_copy_minimal_tuple(mslot->mintuple);
  }

  slot->tts_flags |= TTS_FLAG_SHOULDFREE;

  Assert(mslot->tuple == &mslot->minhdr);

  mslot->minhdr.t_len = mslot->mintuple->t_len + MINIMAL_TUPLE_OFFSET;
  mslot->minhdr.t_data = (HeapTupleHeader)((char *)mslot->mintuple - MINIMAL_TUPLE_OFFSET);

  MemoryContextSwitchTo(oldContext);
}

static void
tts_minimal_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{
  MemoryContext oldcontext;
  MinimalTuple mintuple;

  oldcontext = MemoryContextSwitchTo(dstslot->tts_mcxt);
  mintuple = ExecCopySlotMinimalTuple(srcslot);
  MemoryContextSwitchTo(oldcontext);

  ExecStoreMinimalTuple(mintuple, dstslot, true);
}

static MinimalTuple
tts_minimal_get_minimal_tuple(TupleTableSlot *slot)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

  if (!mslot->mintuple)
  {
    tts_minimal_materialize(slot);
  }

  return mslot->mintuple;
}

static HeapTuple
tts_minimal_copy_heap_tuple(TupleTableSlot *slot)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

  if (!mslot->mintuple)
  {
    tts_minimal_materialize(slot);
  }

  return heap_tuple_from_minimal_tuple(mslot->mintuple);
}

static MinimalTuple
tts_minimal_copy_minimal_tuple(TupleTableSlot *slot)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

  if (!mslot->mintuple)
  {
    tts_minimal_materialize(slot);
  }

  return heap_copy_minimal_tuple(mslot->mintuple);
}

static void
tts_minimal_store_tuple(TupleTableSlot *slot, MinimalTuple mtup, bool shouldFree)
{
  MinimalTupleTableSlot *mslot = (MinimalTupleTableSlot *)slot;

  tts_minimal_clear(slot);

  Assert(!TTS_SHOULDFREE(slot));
  Assert(TTS_EMPTY(slot));

  slot->tts_flags &= ~TTS_FLAG_EMPTY;
  slot->tts_nvalid = 0;
  mslot->off = 0;

  mslot->mintuple = mtup;
  Assert(mslot->tuple == &mslot->minhdr);
  mslot->minhdr.t_len = mtup->t_len + MINIMAL_TUPLE_OFFSET;
  mslot->minhdr.t_data = (HeapTupleHeader)((char *)mtup - MINIMAL_TUPLE_OFFSET);
                                                                       

  if (shouldFree)
  {
    slot->tts_flags |= TTS_FLAG_SHOULDFREE;
  }
}

   
                                                                  
   

static void
tts_buffer_heap_init(TupleTableSlot *slot)
{
}

static void
tts_buffer_heap_release(TupleTableSlot *slot)
{
}

static void
tts_buffer_heap_clear(TupleTableSlot *slot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

     
                                                                           
                                                                           
                                
     
  if (TTS_SHOULDFREE(slot))
  {
                                                                           
    Assert(!BufferIsValid(bslot->buffer));

    heap_freetuple(bslot->base.tuple);
    slot->tts_flags &= ~TTS_FLAG_SHOULDFREE;
  }

  if (BufferIsValid(bslot->buffer))
  {
    ReleaseBuffer(bslot->buffer);
  }

  slot->tts_nvalid = 0;
  slot->tts_flags |= TTS_FLAG_EMPTY;
  ItemPointerSetInvalid(&slot->tts_tid);
  bslot->base.tuple = NULL;
  bslot->base.off = 0;
  bslot->buffer = InvalidBuffer;
}

static void
tts_buffer_heap_getsomeattrs(TupleTableSlot *slot, int natts)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

  slot_deform_heap_tuple(slot, bslot->base.tuple, &bslot->base.off, natts);
}

static Datum
tts_buffer_heap_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

     
                                                                          
                                                           
     
  if (!bslot->base.tuple)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot retrieve a system column in this context")));
  }

  return heap_getsysattr(bslot->base.tuple, attnum, slot->tts_tupleDescriptor, isnull);
}

static void
tts_buffer_heap_materialize(TupleTableSlot *slot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;
  MemoryContext oldContext;

  Assert(!TTS_EMPTY(slot));

                                                                  
  if (TTS_SHOULDFREE(slot))
  {
    return;
  }

  oldContext = MemoryContextSwitchTo(slot->tts_mcxt);

     
                                                                             
                                                                          
     
  bslot->base.off = 0;
  slot->tts_nvalid = 0;

  if (!bslot->base.tuple)
  {
       
                                                                      
                                                                    
                                                                      
                                                            
                       
       
    bslot->base.tuple = heap_form_tuple(slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
  }
  else
  {
    bslot->base.tuple = heap_copytuple(bslot->base.tuple);

       
                                                                       
                                                                       
       
    if (likely(BufferIsValid(bslot->buffer)))
    {
      ReleaseBuffer(bslot->buffer);
    }
    bslot->buffer = InvalidBuffer;
  }

     
                                                                           
                                                                            
                                                                           
                                                                       
                                                                        
     
  slot->tts_flags |= TTS_FLAG_SHOULDFREE;

  MemoryContextSwitchTo(oldContext);
}

static void
tts_buffer_heap_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{
  BufferHeapTupleTableSlot *bsrcslot = (BufferHeapTupleTableSlot *)srcslot;
  BufferHeapTupleTableSlot *bdstslot = (BufferHeapTupleTableSlot *)dstslot;

     
                                                                             
                                                                             
                                                  
     
  if (dstslot->tts_ops != srcslot->tts_ops || TTS_SHOULDFREE(srcslot) || !bsrcslot->base.tuple)
  {
    MemoryContext oldContext;

    ExecClearTuple(dstslot);
    dstslot->tts_flags &= ~TTS_FLAG_EMPTY;
    oldContext = MemoryContextSwitchTo(dstslot->tts_mcxt);
    bdstslot->base.tuple = ExecCopySlotHeapTuple(srcslot);
    dstslot->tts_flags |= TTS_FLAG_SHOULDFREE;
    MemoryContextSwitchTo(oldContext);
  }
  else
  {
    Assert(BufferIsValid(bsrcslot->buffer));

    tts_buffer_heap_store_tuple(dstslot, bsrcslot->base.tuple, bsrcslot->buffer, false);

       
                                                                      
                                                                          
                                                                        
                                          
       
    memcpy(&bdstslot->base.tupdata, bdstslot->base.tuple, sizeof(HeapTupleData));
    bdstslot->base.tuple = &bdstslot->base.tupdata;
  }
}

static HeapTuple
tts_buffer_heap_get_heap_tuple(TupleTableSlot *slot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

  if (!bslot->base.tuple)
  {
    tts_buffer_heap_materialize(slot);
  }

  return bslot->base.tuple;
}

static HeapTuple
tts_buffer_heap_copy_heap_tuple(TupleTableSlot *slot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

  if (!bslot->base.tuple)
  {
    tts_buffer_heap_materialize(slot);
  }

  return heap_copytuple(bslot->base.tuple);
}

static MinimalTuple
tts_buffer_heap_copy_minimal_tuple(TupleTableSlot *slot)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

  Assert(!TTS_EMPTY(slot));

  if (!bslot->base.tuple)
  {
    tts_buffer_heap_materialize(slot);
  }

  return minimal_tuple_from_heap_tuple(bslot->base.tuple);
}

static inline void
tts_buffer_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, Buffer buffer, bool transfer_pin)
{
  BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

  if (TTS_SHOULDFREE(slot))
  {
                                                              
    Assert(!BufferIsValid(bslot->buffer));

    heap_freetuple(bslot->base.tuple);
    slot->tts_flags &= ~TTS_FLAG_SHOULDFREE;
  }

  slot->tts_flags &= ~TTS_FLAG_EMPTY;
  slot->tts_nvalid = 0;
  bslot->base.tuple = tuple;
  bslot->base.off = 0;
  slot->tts_tid = tuple->t_self;

     
                                                                           
                                                                        
                                                                        
                                   
     
                                                                         
                                                                          
                                                                      
                                             
     
  if (bslot->buffer != buffer)
  {
    if (BufferIsValid(bslot->buffer))
    {
      ReleaseBuffer(bslot->buffer);
    }

    bslot->buffer = buffer;

    if (!transfer_pin && BufferIsValid(buffer))
    {
      IncrBufferRefCount(buffer);
    }
  }
  else if (transfer_pin && BufferIsValid(buffer))
  {
       
                                                                      
                                                  
       
    ReleaseBuffer(buffer);
  }
}

   
                          
                                                                        
                                                                    
                                                                        
   
                                                                     
                                                                     
                                                                    
                                                                    
   
                                                                              
                                 
   
static pg_attribute_always_inline void
slot_deform_heap_tuple(TupleTableSlot *slot, HeapTuple tuple, uint32 *offp, int natts)
{
  TupleDesc tupleDesc = slot->tts_tupleDescriptor;
  Datum *values = slot->tts_values;
  bool *isnull = slot->tts_isnull;
  HeapTupleHeader tup = tuple->t_data;
  bool hasnulls = HeapTupleHasNulls(tuple);
  int attnum;
  char *tp;                                       
  uint32 off;                                        
  bits8 *bp = tup->t_bits;                                  
  bool slow;                                                

                                                              
  natts = Min(HeapTupleHeaderGetNatts(tuple->t_data), natts);

     
                                                                            
                 
     
  attnum = slot->tts_nvalid;
  if (attnum == 0)
  {
                                        
    off = 0;
    slow = false;
  }
  else
  {
                                               
    off = *offp;
    slow = TTS_SLOW(slot);
  }

  tp = (char *)tup + tup->t_hoff;

  for (; attnum < natts; attnum++)
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

     
                                   
     
  slot->tts_nvalid = attnum;
  *offp = off;
  if (slow)
  {
    slot->tts_flags |= TTS_FLAG_SLOW;
  }
  else
  {
    slot->tts_flags &= ~TTS_FLAG_SLOW;
  }
}

const TupleTableSlotOps TTSOpsVirtual = {.base_slot_size = sizeof(VirtualTupleTableSlot),
    .init = tts_virtual_init,
    .release = tts_virtual_release,
    .clear = tts_virtual_clear,
    .getsomeattrs = tts_virtual_getsomeattrs,
    .getsysattr = tts_virtual_getsysattr,
    .materialize = tts_virtual_materialize,
    .copyslot = tts_virtual_copyslot,

       
                                                                          
              
       
    .get_heap_tuple = NULL,
    .get_minimal_tuple = NULL,
    .copy_heap_tuple = tts_virtual_copy_heap_tuple,
    .copy_minimal_tuple = tts_virtual_copy_minimal_tuple};

const TupleTableSlotOps TTSOpsHeapTuple = {.base_slot_size = sizeof(HeapTupleTableSlot),
    .init = tts_heap_init,
    .release = tts_heap_release,
    .clear = tts_heap_clear,
    .getsomeattrs = tts_heap_getsomeattrs,
    .getsysattr = tts_heap_getsysattr,
    .materialize = tts_heap_materialize,
    .copyslot = tts_heap_copyslot,
    .get_heap_tuple = tts_heap_get_heap_tuple,

                                                                
    .get_minimal_tuple = NULL,
    .copy_heap_tuple = tts_heap_copy_heap_tuple,
    .copy_minimal_tuple = tts_heap_copy_minimal_tuple};

const TupleTableSlotOps TTSOpsMinimalTuple = {.base_slot_size = sizeof(MinimalTupleTableSlot),
    .init = tts_minimal_init,
    .release = tts_minimal_release,
    .clear = tts_minimal_clear,
    .getsomeattrs = tts_minimal_getsomeattrs,
    .getsysattr = tts_minimal_getsysattr,
    .materialize = tts_minimal_materialize,
    .copyslot = tts_minimal_copyslot,

                                                                
    .get_heap_tuple = NULL,
    .get_minimal_tuple = tts_minimal_get_minimal_tuple,
    .copy_heap_tuple = tts_minimal_copy_heap_tuple,
    .copy_minimal_tuple = tts_minimal_copy_minimal_tuple};

const TupleTableSlotOps TTSOpsBufferHeapTuple = {.base_slot_size = sizeof(BufferHeapTupleTableSlot),
    .init = tts_buffer_heap_init,
    .release = tts_buffer_heap_release,
    .clear = tts_buffer_heap_clear,
    .getsomeattrs = tts_buffer_heap_getsomeattrs,
    .getsysattr = tts_buffer_heap_getsysattr,
    .materialize = tts_buffer_heap_materialize,
    .copyslot = tts_buffer_heap_copyslot,
    .get_heap_tuple = tts_buffer_heap_get_heap_tuple,

                                                                       
    .get_minimal_tuple = NULL,
    .copy_heap_tuple = tts_buffer_heap_copy_heap_tuple,
    .copy_minimal_tuple = tts_buffer_heap_copy_minimal_tuple};

                                                                    
                                            
                                                                    
   

                                    
                       
   
                                                           
                                                                           
                                                               
                            
                                    
   
TupleTableSlot *
MakeTupleTableSlot(TupleDesc tupleDesc, const TupleTableSlotOps *tts_ops)
{
  Size basesz, allocsz;
  TupleTableSlot *slot;

  basesz = tts_ops->base_slot_size;

     
                                                                     
                                           
     
  if (tupleDesc)
  {
    allocsz = MAXALIGN(basesz) + MAXALIGN(tupleDesc->natts * sizeof(Datum)) + MAXALIGN(tupleDesc->natts * sizeof(bool));
  }
  else
  {
    allocsz = basesz;
  }

  slot = palloc0(allocsz);
                                                                        
  *((const TupleTableSlotOps **)&slot->tts_ops) = tts_ops;
  slot->type = T_TupleTableSlot;
  slot->tts_flags |= TTS_FLAG_EMPTY;
  if (tupleDesc != NULL)
  {
    slot->tts_flags |= TTS_FLAG_FIXED;
  }
  slot->tts_tupleDescriptor = tupleDesc;
  slot->tts_mcxt = CurrentMemoryContext;
  slot->tts_nvalid = 0;

  if (tupleDesc != NULL)
  {
    slot->tts_values = (Datum *)(((char *)slot) + MAXALIGN(basesz));
    slot->tts_isnull = (bool *)(((char *)slot) + MAXALIGN(basesz) + MAXALIGN(tupleDesc->natts * sizeof(Datum)));

    PinTupleDesc(tupleDesc);
  }

     
                                                  
     
  slot->tts_ops->init(slot);

  return slot;
}

                                    
                       
   
                                                                           
                                    
   
TupleTableSlot *
ExecAllocTableSlot(List **tupleTable, TupleDesc desc, const TupleTableSlotOps *tts_ops)
{
  TupleTableSlot *slot = MakeTupleTableSlot(desc, tts_ops);

  *tupleTable = lappend(*tupleTable, slot);

  return slot;
}

                                    
                        
   
                                                                 
                                                                
                                                
                                                             
                                    
   
void
ExecResetTupleTable(List *tupleTable,                  
    bool shouldFree)                                                     
{
  ListCell *lc;

  foreach (lc, tupleTable)
  {
    TupleTableSlot *slot = lfirst_node(TupleTableSlot, lc);

                                                              
    ExecClearTuple(slot);
    slot->tts_ops->release(slot);
    if (slot->tts_tupleDescriptor)
    {
      ReleaseTupleDesc(slot->tts_tupleDescriptor);
      slot->tts_tupleDescriptor = NULL;
    }

                                                                   
    if (shouldFree)
    {
      if (!TTS_FIXED(slot))
      {
        if (slot->tts_values)
        {
          pfree(slot->tts_values);
        }
        if (slot->tts_isnull)
        {
          pfree(slot->tts_isnull);
        }
      }
      pfree(slot);
    }
  }

                                                 
  if (shouldFree)
  {
    list_free(tupleTable);
  }
}

                                    
                             
   
                                                                        
                                                                            
                                                                            
                            
                                    
   
TupleTableSlot *
MakeSingleTupleTableSlot(TupleDesc tupdesc, const TupleTableSlotOps *tts_ops)
{
  TupleTableSlot *slot = MakeTupleTableSlot(tupdesc, tts_ops);

  return slot;
}

                                    
                                 
   
                                                                 
                                                                
                                    
   
void
ExecDropSingleTupleTableSlot(TupleTableSlot *slot)
{
                                                                      
  Assert(IsA(slot, TupleTableSlot));
  ExecClearTuple(slot);
  slot->tts_ops->release(slot);
  if (slot->tts_tupleDescriptor)
  {
    ReleaseTupleDesc(slot->tts_tupleDescriptor);
  }
  if (!TTS_FIXED(slot))
  {
    if (slot->tts_values)
    {
      pfree(slot->tts_values);
    }
    if (slot->tts_isnull)
    {
      pfree(slot->tts_isnull);
    }
  }
  pfree(slot);
}

                                                                    
                                            
                                                                    
   

                                    
                          
   
                                                                 
                                                                     
                                                                           
                                                                          
                 
                                    
   
void
ExecSetSlotDescriptor(TupleTableSlot *slot,                     
    TupleDesc tupdesc)                                                
{
  Assert(!TTS_FIXED(slot));

                                                              
  ExecClearTuple(slot);

     
                                                                          
                                                                  
     
  if (slot->tts_tupleDescriptor)
  {
    ReleaseTupleDesc(slot->tts_tupleDescriptor);
  }

  if (slot->tts_values)
  {
    pfree(slot->tts_values);
  }
  if (slot->tts_isnull)
  {
    pfree(slot->tts_isnull);
  }

     
                                                                        
     
  slot->tts_tupleDescriptor = tupdesc;
  PinTupleDesc(tupdesc);

     
                                                                            
                                                                           
     
  slot->tts_values = (Datum *)MemoryContextAlloc(slot->tts_mcxt, tupdesc->natts * sizeof(Datum));
  slot->tts_isnull = (bool *)MemoryContextAlloc(slot->tts_mcxt, tupdesc->natts * sizeof(bool));
}

                                    
                       
   
                                                                                 
                             
   
                          
                                                   
                                                                
                         
   
                                                                                
                                                                          
                                                                                
                                                                          
                                                                             
                                                                             
                                                                                
                                                                    
   
                                                    
   
                                                                             
                                                   
                                    
   
TupleTableSlot *
ExecStoreHeapTuple(HeapTuple tuple, TupleTableSlot *slot, bool shouldFree)
{
     
                   
     
  Assert(tuple != NULL);
  Assert(slot != NULL);
  Assert(slot->tts_tupleDescriptor != NULL);

  if (unlikely(!TTS_IS_HEAPTUPLE(slot)))
  {
    elog(ERROR, "trying to store a heap tuple into wrong type of slot");
  }
  tts_heap_store_tuple(slot, tuple, shouldFree);

  slot->tts_tableOid = tuple->t_tableOid;

  return slot;
}

                                    
                             
   
                                                                           
                                              
   
                          
                                                         
                                                                       
   
                                                                             
                                                           
   
                                                    
   
                                                                               
                                                       
                                    
   
TupleTableSlot *
ExecStoreBufferHeapTuple(HeapTuple tuple, TupleTableSlot *slot, Buffer buffer)
{
     
                   
     
  Assert(tuple != NULL);
  Assert(slot != NULL);
  Assert(slot->tts_tupleDescriptor != NULL);
  Assert(BufferIsValid(buffer));

  if (unlikely(!TTS_IS_BUFFERTUPLE(slot)))
  {
    elog(ERROR, "trying to store an on-disk heap tuple into wrong type of slot");
  }
  tts_buffer_heap_store_tuple(slot, tuple, buffer, false);

  slot->tts_tableOid = tuple->t_tableOid;

  return slot;
}

   
                                                                               
                                                                               
   
TupleTableSlot *
ExecStorePinnedBufferHeapTuple(HeapTuple tuple, TupleTableSlot *slot, Buffer buffer)
{
     
                   
     
  Assert(tuple != NULL);
  Assert(slot != NULL);
  Assert(slot->tts_tupleDescriptor != NULL);
  Assert(BufferIsValid(buffer));

  if (unlikely(!TTS_IS_BUFFERTUPLE(slot)))
  {
    elog(ERROR, "trying to store an on-disk heap tuple into wrong type of slot");
  }
  tts_buffer_heap_store_tuple(slot, tuple, buffer, true);

  slot->tts_tableOid = tuple->t_tableOid;

  return slot;
}

   
                                                            
   
                                                                            
                                                          
   
TupleTableSlot *
ExecStoreMinimalTuple(MinimalTuple mtup, TupleTableSlot *slot, bool shouldFree)
{
     
                   
     
  Assert(mtup != NULL);
  Assert(slot != NULL);
  Assert(slot->tts_tupleDescriptor != NULL);

  if (unlikely(!TTS_IS_MINIMALTUPLE(slot)))
  {
    elog(ERROR, "trying to store a minimal tuple into wrong type of slot");
  }
  tts_minimal_store_tuple(slot, mtup, shouldFree);

  return slot;
}

   
                                                                     
              
   
void
ExecForceStoreHeapTuple(HeapTuple tuple, TupleTableSlot *slot, bool shouldFree)
{
  if (TTS_IS_HEAPTUPLE(slot))
  {
    ExecStoreHeapTuple(tuple, slot, shouldFree);
  }
  else if (TTS_IS_BUFFERTUPLE(slot))
  {
    MemoryContext oldContext;
    BufferHeapTupleTableSlot *bslot = (BufferHeapTupleTableSlot *)slot;

    ExecClearTuple(slot);
    slot->tts_flags &= ~TTS_FLAG_EMPTY;
    oldContext = MemoryContextSwitchTo(slot->tts_mcxt);
    bslot->base.tuple = heap_copytuple(tuple);
    slot->tts_flags |= TTS_FLAG_SHOULDFREE;
    MemoryContextSwitchTo(oldContext);

    if (shouldFree)
    {
      pfree(tuple);
    }
  }
  else
  {
    ExecClearTuple(slot);
    heap_deform_tuple(tuple, slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
    ExecStoreVirtualTuple(slot);

    if (shouldFree)
    {
      ExecMaterializeSlot(slot);
      pfree(tuple);
    }
  }
}

   
                                                                        
              
   
void
ExecForceStoreMinimalTuple(MinimalTuple mtup, TupleTableSlot *slot, bool shouldFree)
{
  if (TTS_IS_MINIMALTUPLE(slot))
  {
    tts_minimal_store_tuple(slot, mtup, shouldFree);
  }
  else
  {
    HeapTupleData htup;

    ExecClearTuple(slot);

    htup.t_len = mtup->t_len + MINIMAL_TUPLE_OFFSET;
    htup.t_data = (HeapTupleHeader)((char *)mtup - MINIMAL_TUPLE_OFFSET);
    heap_deform_tuple(&htup, slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
    ExecStoreVirtualTuple(slot);

    if (shouldFree)
    {
      ExecMaterializeSlot(slot);
      pfree(mtup);
    }
  }
}

                                    
                          
                                                
   
                                                               
                                                  
                                               
                                                         
                                                                  
                                    
   
TupleTableSlot *
ExecStoreVirtualTuple(TupleTableSlot *slot)
{
     
                   
     
  Assert(slot != NULL);
  Assert(slot->tts_tupleDescriptor != NULL);
  Assert(TTS_EMPTY(slot));

  slot->tts_flags &= ~TTS_FLAG_EMPTY;
  slot->tts_nvalid = slot->tts_tupleDescriptor->natts;

  return slot;
}

                                    
                          
                                                        
   
                                                                       
                                                         
                                    
   
TupleTableSlot *
ExecStoreAllNullTuple(TupleTableSlot *slot)
{
     
                   
     
  Assert(slot != NULL);
  Assert(slot->tts_tupleDescriptor != NULL);

                              
  ExecClearTuple(slot);

     
                                                          
     
  MemSet(slot->tts_values, 0, slot->tts_tupleDescriptor->natts * sizeof(Datum));
  memset(slot->tts_isnull, true, slot->tts_tupleDescriptor->natts * sizeof(bool));

  return ExecStoreVirtualTuple(slot);
}

   
                                                                      
                                                
   
                                                                          
          
   
void
ExecStoreHeapTupleDatum(Datum data, TupleTableSlot *slot)
{
  HeapTupleData tuple = {0};
  HeapTupleHeader td;

  td = DatumGetHeapTupleHeader(data);

  tuple.t_len = HeapTupleHeaderGetDatumLength(td);
  tuple.t_self = td->t_ctid;
  tuple.t_data = td;

  ExecClearTuple(slot);

  heap_deform_tuple(&tuple, slot->tts_tupleDescriptor, slot->tts_values, slot->tts_isnull);
  ExecStoreVirtualTuple(slot);
}

   
                                                                            
   
                                                                      
             
   
                                                                              
                                                                             
                                     
   
                                                                            
                                                                          
                                                            
   
                                                                       
                                                                              
                                                                      
                                                                          
   
HeapTuple
ExecFetchSlotHeapTuple(TupleTableSlot *slot, bool materialize, bool *shouldFree)
{
     
                   
     
  Assert(slot != NULL);
  Assert(!TTS_EMPTY(slot));

                                                                       
  if (materialize)
  {
    slot->tts_ops->materialize(slot);
  }

  if (slot->tts_ops->get_heap_tuple == NULL)
  {
    if (shouldFree)
    {
      *shouldFree = true;
    }
    return slot->tts_ops->copy_heap_tuple(slot);
  }
  else
  {
    if (shouldFree)
    {
      *shouldFree = false;
    }
    return slot->tts_ops->get_heap_tuple(slot);
  }
}

                                    
                              
                                              
   
                                                                           
                                                                          
                                                                       
                                                                     
                                                                            
                                                                         
                                                                            
                                             
   
                                                                            
                                                                         
                                                                   
                                                                         
                                                                          
        
                                    
   
MinimalTuple
ExecFetchSlotMinimalTuple(TupleTableSlot *slot, bool *shouldFree)
{
     
                   
     
  Assert(slot != NULL);
  Assert(!TTS_EMPTY(slot));

  if (slot->tts_ops->get_minimal_tuple)
  {
    if (shouldFree)
    {
      *shouldFree = false;
    }
    return slot->tts_ops->get_minimal_tuple(slot);
  }
  else
  {
    if (shouldFree)
    {
      *shouldFree = true;
    }
    return slot->tts_ops->copy_minimal_tuple(slot);
  }
}

                                    
                                
                                                       
   
                                                                          
                                    
   
Datum
ExecFetchSlotHeapTupleDatum(TupleTableSlot *slot)
{
  HeapTuple tup;
  TupleDesc tupdesc;
  bool shouldFree;
  Datum ret;

                                                            
  tup = ExecFetchSlotHeapTuple(slot, false, &shouldFree);
  tupdesc = slot->tts_tupleDescriptor;

                             
  ret = heap_copy_tuple_as_datum(tup, tupdesc);

  if (shouldFree)
  {
    pfree(tup);
  }

  return ret;
}

                                                                    
                                          
                                                                    
   

                    
                         
   
                                                              
                    
   
void
ExecInitResultTypeTL(PlanState *planstate)
{
  TupleDesc tupDesc = ExecTypeFromTL(planstate->plan->targetlist);

  planstate->ps_ResultTupleDesc = tupDesc;
}

                                    
                                             
   
                                                                    
                                                                       
                                                    
                                    
   

                    
                              
   
                                                                        
                                          
                    
   
void
ExecInitResultSlot(PlanState *planstate, const TupleTableSlotOps *tts_ops)
{
  TupleTableSlot *slot;

  slot = ExecAllocTableSlot(&planstate->state->es_tupleTable, planstate->ps_ResultTupleDesc, tts_ops);
  planstate->ps_ResultTupleSlot = slot;

  planstate->resultopsfixed = planstate->ps_ResultTupleDesc != NULL;
  planstate->resultops = tts_ops;
  planstate->resultopsset = true;
}

                    
                              
   
                                                                    
                    
   
void
ExecInitResultTupleSlotTL(PlanState *planstate, const TupleTableSlotOps *tts_ops)
{
  ExecInitResultTypeTL(planstate);
  ExecInitResultSlot(planstate, tts_ops);
}

                    
                          
                    
   
void
ExecInitScanTupleSlot(EState *estate, ScanState *scanstate, TupleDesc tupledesc, const TupleTableSlotOps *tts_ops)
{
  scanstate->ss_ScanTupleSlot = ExecAllocTableSlot(&estate->es_tupleTable, tupledesc, tts_ops);
  scanstate->ps.scandesc = tupledesc;
  scanstate->ps.scanopsfixed = tupledesc != NULL;
  scanstate->ps.scanops = tts_ops;
  scanstate->ps.scanopsset = true;
}

                    
                           
   
                                                                            
                                                                  
                                                             
                    
   
TupleTableSlot *
ExecInitExtraTupleSlot(EState *estate, TupleDesc tupledesc, const TupleTableSlotOps *tts_ops)
{
  return ExecAllocTableSlot(&estate->es_tupleTable, tupledesc, tts_ops);
}

                    
                          
   
                                                                 
                                                                      
               
                    
   
TupleTableSlot *
ExecInitNullTupleSlot(EState *estate, TupleDesc tupType, const TupleTableSlotOps *tts_ops)
{
  TupleTableSlot *slot = ExecInitExtraTupleSlot(estate, tupType, tts_ops);

  return ExecStoreAllNullTuple(slot);
}

                                                                   
                                                             
                                                                   
   

   
                                                
   
                                                                   
                                                                               
         
   
void
slot_getmissingattrs(TupleTableSlot *slot, int startAttNum, int lastAttNum)
{
  AttrMissing *attrmiss = NULL;

  if (slot->tts_tupleDescriptor->constr)
  {
    attrmiss = slot->tts_tupleDescriptor->constr->missing;
  }

  if (!attrmiss)
  {
                                                                            
    memset(slot->tts_values + startAttNum, 0, (lastAttNum - startAttNum) * sizeof(Datum));
    memset(slot->tts_isnull + startAttNum, 1, (lastAttNum - startAttNum) * sizeof(bool));
  }
  else
  {
    int missattnum;

                                                                            
    for (missattnum = startAttNum; missattnum < lastAttNum; missattnum++)
    {
      slot->tts_values[missattnum] = attrmiss[missattnum].am_value;
      slot->tts_isnull[missattnum] = !attrmiss[missattnum].am_present;
    }
  }
}

   
                                                             
   
void
slot_getsomeattrs_int(TupleTableSlot *slot, int attnum)
{
                               
  Assert(slot->tts_nvalid < attnum);                                   
  Assert(attnum > 0);

  if (unlikely(attnum > slot->tts_tupleDescriptor->natts))
  {
    elog(ERROR, "invalid attribute number %d", attnum);
  }

                                                                       
  slot->tts_ops->getsomeattrs(slot, attnum);

     
                                                                   
                                                  
     
  if (unlikely(slot->tts_nvalid < attnum))
  {
    slot_getmissingattrs(slot, slot->tts_nvalid, attnum);
    slot->tts_nvalid = attnum;
  }
}

                                                                    
                   
   
                                                                      
                                                                 
                                                                   
   
                                                                 
                                                             
                                      
                                                                    
   
TupleDesc
ExecTypeFromTL(List *targetList)
{
  return ExecTypeFromTLInternal(targetList, false);
}

                                                                    
                        
   
                                                                    
                                                                    
   
TupleDesc
ExecCleanTypeFromTL(List *targetList)
{
  return ExecTypeFromTLInternal(targetList, true);
}

static TupleDesc
ExecTypeFromTLInternal(List *targetList, bool skipjunk)
{
  TupleDesc typeInfo;
  ListCell *l;
  int len;
  int cur_resno = 1;

  if (skipjunk)
  {
    len = ExecCleanTargetListLength(targetList);
  }
  else
  {
    len = ExecTargetListLength(targetList);
  }
  typeInfo = CreateTemplateTupleDesc(len);

  foreach (l, targetList)
  {
    TargetEntry *tle = lfirst(l);

    if (skipjunk && tle->resjunk)
    {
      continue;
    }
    TupleDescInitEntry(typeInfo, cur_resno, tle->resname, exprType((Node *)tle->expr), exprTypmod((Node *)tle->expr), 0);
    TupleDescInitEntryCollation(typeInfo, cur_resno, exprCollation((Node *)tle->expr));
    cur_resno++;
  }

  return typeInfo;
}

   
                                                                        
   
                                                                          
                                                                        
   
TupleDesc
ExecTypeFromExprList(List *exprList)
{
  TupleDesc typeInfo;
  ListCell *lc;
  int cur_resno = 1;

  typeInfo = CreateTemplateTupleDesc(list_length(exprList));

  foreach (lc, exprList)
  {
    Node *e = lfirst(lc);

    TupleDescInitEntry(typeInfo, cur_resno, NULL, exprType(e), exprTypmod(e), 0);
    TupleDescInitEntryCollation(typeInfo, cur_resno, exprCollation(e));
    cur_resno++;
  }

  return typeInfo;
}

   
                                                                
   
                                                                          
   
void
ExecTypeSetColNames(TupleDesc typeInfo, List *namesList)
{
  int colno = 0;
  ListCell *lc;

                                                                         
  Assert(typeInfo->tdtypeid == RECORDOID);
  Assert(typeInfo->tdtypmod < 0);

  foreach (lc, namesList)
  {
    char *cname = strVal(lfirst(lc));
    Form_pg_attribute attr;

                                                                   
    if (colno >= typeInfo->natts)
    {
      break;
    }
    attr = TupleDescAttr(typeInfo, colno);
    colno++;

       
                                                                    
                                                     
       
    if (cname[0] == '\0' || attr->attisdropped)
    {
      continue;
    }

                                    
    namestrcpy(&(attr->attname), cname);
  }
}

   
                                                                      
   
                                                                              
                                                                          
                                                                          
                                                                           
   
TupleDesc
BlessTupleDesc(TupleDesc tupdesc)
{
  if (tupdesc->tdtypeid == RECORDOID && tupdesc->tdtypmod < 0)
  {
    assign_record_type_typmod(tupdesc);
  }

  return tupdesc;                                      
}

   
                                                                             
                                                                               
                                       
   
AttInMetadata *
TupleDescGetAttInMetadata(TupleDesc tupdesc)
{
  int natts = tupdesc->natts;
  int i;
  Oid atttypeid;
  Oid attinfuncid;
  FmgrInfo *attinfuncinfo;
  Oid *attioparams;
  int32 *atttypmods;
  AttInMetadata *attinmeta;

  attinmeta = (AttInMetadata *)palloc(sizeof(AttInMetadata));

                                                                        
  attinmeta->tupdesc = BlessTupleDesc(tupdesc);

     
                                                                           
     
  attinfuncinfo = (FmgrInfo *)palloc0(natts * sizeof(FmgrInfo));
  attioparams = (Oid *)palloc0(natts * sizeof(Oid));
  atttypmods = (int32 *)palloc0(natts * sizeof(int32));

  for (i = 0; i < natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

                                   
    if (!att->attisdropped)
    {
      atttypeid = att->atttypid;
      getTypeInputInfo(atttypeid, &attinfuncid, &attioparams[i]);
      fmgr_info(attinfuncid, &attinfuncinfo[i]);
      atttypmods[i] = att->atttypmod;
    }
  }
  attinmeta->attinfuncs = attinfuncinfo;
  attinmeta->attioparams = attioparams;
  attinmeta->atttypmods = atttypmods;

  return attinmeta;
}

   
                                                                                
                                                                                
                                                                   
   
HeapTuple
BuildTupleFromCStrings(AttInMetadata *attinmeta, char **values)
{
  TupleDesc tupdesc = attinmeta->tupdesc;
  int natts = tupdesc->natts;
  Datum *dvalues;
  bool *nulls;
  int i;
  HeapTuple tuple;

  dvalues = (Datum *)palloc(natts * sizeof(Datum));
  nulls = (bool *)palloc(natts * sizeof(bool));

     
                                                                            
                         
     
  for (i = 0; i < natts; i++)
  {
    if (!TupleDescAttr(tupdesc, i)->attisdropped)
    {
                                  
      dvalues[i] = InputFunctionCall(&attinmeta->attinfuncs[i], values[i], attinmeta->attioparams[i], attinmeta->atttypmods[i]);
      if (values[i] != NULL)
      {
        nulls[i] = false;
      }
      else
      {
        nulls[i] = true;
      }
    }
    else
    {
                                                        
      dvalues[i] = (Datum)0;
      nulls[i] = true;
    }
  }

     
                  
     
  tuple = heap_form_tuple(tupdesc, dvalues, nulls);

     
                                                                          
                                                  
     
  pfree(dvalues);
  pfree(nulls);

  return tuple;
}

   
                                                                           
   
                                                                        
                                                                           
                                                                           
                                               
   
                                                                           
                                                                          
                                                                             
                                                                               
                                                                              
                                                                           
                                                                           
                                                                        
                                                                             
                                                               
   
                                                                      
                                                                            
                                                                   
   
                                                                          
                                                                     
                                                                             
                                                                         
                                                                         
            
   
                                                                          
                                                                          
                                                                        
                      
   
Datum
HeapTupleHeaderGetDatum(HeapTupleHeader tuple)
{
  Datum result;
  TupleDesc tupDesc;

                                                                    
  if (!HeapTupleHeaderHasExternal(tuple))
  {
    return PointerGetDatum(tuple);
  }

                                                                         
  tupDesc = lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(tuple), HeapTupleHeaderGetTypMod(tuple));

                             
  result = toast_flatten_tuple_to_datum(tuple, HeapTupleHeaderGetDatumLength(tuple), tupDesc);

  ReleaseTupleDesc(tupDesc);

  return result;
}

   
                                                                                 
                                                                            
                                                                           
                                                                      
   
TupOutputState *
begin_tup_output_tupdesc(DestReceiver *dest, TupleDesc tupdesc, const TupleTableSlotOps *tts_ops)
{
  TupOutputState *tstate;

  tstate = (TupOutputState *)palloc(sizeof(TupOutputState));

  tstate->slot = MakeSingleTupleTableSlot(tupdesc, tts_ops);
  tstate->dest = dest;

  tstate->dest->rStartup(tstate->dest, (int)CMD_SELECT, tupdesc);

  return tstate;
}

   
                        
   
void
do_tup_output(TupOutputState *tstate, Datum *values, bool *isnull)
{
  TupleTableSlot *slot = tstate->slot;
  int natts = slot->tts_tupleDescriptor->natts;

                                   
  ExecClearTuple(slot);

                   
  memcpy(slot->tts_values, values, natts * sizeof(Datum));
  memcpy(slot->tts_isnull, isnull, natts * sizeof(bool));

                                               
  ExecStoreVirtualTuple(slot);

                                      
  (void)tstate->dest->receiveSlot(slot, tstate->dest);

                
  ExecClearTuple(slot);
}

   
                                                         
   
                                                             
   
void
do_text_output_multiline(TupOutputState *tstate, const char *txt)
{
  Datum values[1];
  bool isnull[1] = {false};

  while (*txt)
  {
    const char *eol;
    int len;

    eol = strchr(txt, '\n');
    if (eol)
    {
      len = eol - txt;
      eol++;
    }
    else
    {
      len = strlen(txt);
      eol = txt + len;
    }

    values[0] = PointerGetDatum(cstring_to_text_with_len(txt, len));
    do_tup_output(tstate, values, isnull);
    pfree(DatumGetPointer(values[0]));
    txt = eol;
  }
}

void
end_tup_output(TupOutputState *tstate)
{
  tstate->dest->rShutdown(tstate->dest);
                                                       
  ExecDropSingleTupleTableSlot(tstate->slot);
  pfree(tstate);
}
