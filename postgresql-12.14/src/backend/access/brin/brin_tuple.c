   
                
                                                       
   
                                                                 
                                                                             
                           
   
         
   
                                                                             
                                                                               
                                                                             
                                                                       
                                                                            
                                        
   
                                                                            
                                                                             
                                                                              
                                                                             
                                                                          
                                                                        
                
   
                                                                         
                                                                        
   
                  
                                          
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/brin_tuple.h"
#include "access/tupdesc.h"
#include "access/tupmacs.h"
#include "access/tuptoaster.h"
#include "utils/datum.h"
#include "utils/memutils.h"

static inline void
brin_deconstruct_tuple(BrinDesc *brdesc, char *tp, bits8 *nullbits, bool nulls, Datum *values, bool *allnulls, bool *hasnulls);

   
                                                                      
   
static TupleDesc
brtuple_disk_tupdesc(BrinDesc *brdesc)
{
                                      
  if (brdesc->bd_disktdesc == NULL)
  {
    int i;
    int j;
    AttrNumber attno = 1;
    TupleDesc tupdesc;
    MemoryContext oldcxt;

                                               
    oldcxt = MemoryContextSwitchTo(brdesc->bd_context);

    tupdesc = CreateTemplateTupleDesc(brdesc->bd_totalstored);

    for (i = 0; i < brdesc->bd_tupdesc->natts; i++)
    {
      for (j = 0; j < brdesc->bd_info[i]->oi_nstored; j++)
      {
        TupleDescInitEntry(tupdesc, attno++, NULL, brdesc->bd_info[i]->oi_typcache[j]->type_id, -1, 0);
      }
    }

    MemoryContextSwitchTo(oldcxt);

    brdesc->bd_disktdesc = tupdesc;
  }

  return brdesc->bd_disktdesc;
}

   
                                                                
   
                                                      
   
BrinTuple *
brin_form_tuple(BrinDesc *brdesc, BlockNumber blkno, BrinMemTuple *tuple, Size *size)
{
  Datum *values;
  bool *nulls;
  bool anynulls = false;
  BrinTuple *rettuple;
  int keyno;
  int idxattno;
  uint16 phony_infomask = 0;
  bits8 *phony_nullbitmap;
  Size len, hoff, data_len;
  int i;

#ifdef TOAST_INDEX_HACK
  Datum *untoasted_values;
  int nuntoasted = 0;
#endif

  Assert(brdesc->bd_totalstored > 0);

  values = (Datum *)palloc(sizeof(Datum) * brdesc->bd_totalstored);
  nulls = (bool *)palloc0(sizeof(bool) * brdesc->bd_totalstored);
  phony_nullbitmap = (bits8 *)palloc(sizeof(bits8) * BITMAPLEN(brdesc->bd_totalstored));

#ifdef TOAST_INDEX_HACK
  untoasted_values = (Datum *)palloc(sizeof(Datum) * brdesc->bd_totalstored);
#endif

     
                                                        
     
  idxattno = 0;
  for (keyno = 0; keyno < brdesc->bd_tupdesc->natts; keyno++)
  {
    int datumno;

       
                                                                         
                                                                           
                                                                          
       
    if (tuple->bt_columns[keyno].bv_allnulls)
    {
      for (datumno = 0; datumno < brdesc->bd_info[keyno]->oi_nstored; datumno++)
      {
        nulls[idxattno++] = true;
      }
      anynulls = true;
      continue;
    }

       
                                                                        
                                                                       
                                         
       
    if (tuple->bt_columns[keyno].bv_hasnulls)
    {
      anynulls = true;
    }

       
                                                                          
                                                                        
                                                                      
                      
       
    for (datumno = 0; datumno < brdesc->bd_info[keyno]->oi_nstored; datumno++)
    {
      Datum value = tuple->bt_columns[keyno].bv_values[datumno];

#ifdef TOAST_INDEX_HACK

                                                                         
      TypeCacheEntry *atttype = brdesc->bd_info[keyno]->oi_typcache[datumno];

                                                    
      bool free_value = false;

                                                                      
      if (atttype->typlen != -1)
      {
        values[idxattno++] = value;
        continue;
      }

         
                                                                      
                                                                   
         
                                                                  
                                       
         
                                                                    
                                                                       
                                         
         
      if (VARATT_IS_EXTERNAL(DatumGetPointer(value)))
      {
        value = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(value)));
        free_value = true;
      }

         
                                                                           
                                     
         
      if (!VARATT_IS_EXTENDED(DatumGetPointer(value)) && VARSIZE(DatumGetPointer(value)) > TOAST_INDEX_TARGET && (atttype->typstorage == 'x' || atttype->typstorage == 'm'))
      {
        Datum cvalue = toast_compress_datum(value);

        if (DatumGetPointer(cvalue) != NULL)
        {
                                      
          if (free_value)
          {
            pfree(DatumGetPointer(value));
          }

          value = cvalue;
          free_value = true;
        }
      }

         
                                                                    
                                        
         
      if (free_value)
      {
        untoasted_values[nuntoasted++] = value;
      }

#endif

      values[idxattno++] = value;
    }
  }

                                             
  Assert(idxattno <= brdesc->bd_totalstored);

                                  
  len = SizeOfBrinTuple;
  if (anynulls)
  {
       
                                                                          
                                                                
                   
       
    len += BITMAPLEN(brdesc->bd_tupdesc->natts * 2);
  }

  len = hoff = MAXALIGN(len);

  data_len = heap_compute_data_size(brtuple_disk_tupdesc(brdesc), values, nulls);
  len += data_len;

  len = MAXALIGN(len);

  rettuple = palloc0(len);
  rettuple->bt_blkno = blkno;
  rettuple->bt_info = hoff;

                                                    
  Assert((rettuple->bt_info & BRIN_OFFSET_MASK) == hoff);

     
                                                                             
                                                                            
                                                                     
                                                  
     
  heap_fill_tuple(brtuple_disk_tupdesc(brdesc), values, nulls, (char *)rettuple + hoff, data_len, &phony_infomask, phony_nullbitmap);

                       
  pfree(values);
  pfree(nulls);
  pfree(phony_nullbitmap);

#ifdef TOAST_INDEX_HACK
  for (i = 0; i < nuntoasted; i++)
  {
    pfree(DatumGetPointer(untoasted_values[i]));
  }
#endif

     
                                                          
     
  if (anynulls)
  {
    bits8 *bitP;
    int bitmask;

    rettuple->bt_info |= BRIN_NULLS_MASK;

       
                                                                      
                                                                           
                                                                           
       
    bitP = ((bits8 *)((char *)rettuple + SizeOfBrinTuple)) - 1;
    bitmask = HIGHBIT;
    for (keyno = 0; keyno < brdesc->bd_tupdesc->natts; keyno++)
    {
      if (bitmask != HIGHBIT)
      {
        bitmask <<= 1;
      }
      else
      {
        bitP += 1;
        *bitP = 0x0;
        bitmask = 1;
      }

      if (!tuple->bt_columns[keyno].bv_allnulls)
      {
        continue;
      }

      *bitP |= bitmask;
    }
                              
    for (keyno = 0; keyno < brdesc->bd_tupdesc->natts; keyno++)
    {
      if (bitmask != HIGHBIT)
      {
        bitmask <<= 1;
      }
      else
      {
        bitP += 1;
        *bitP = 0x0;
        bitmask = 1;
      }

      if (!tuple->bt_columns[keyno].bv_hasnulls)
      {
        continue;
      }

      *bitP |= bitmask;
    }
    bitP = ((bits8 *)(rettuple + SizeOfBrinTuple)) - 1;
  }

  if (tuple->bt_placeholder)
  {
    rettuple->bt_info |= BRIN_PLACEHOLDER_MASK;
  }

  *size = len;
  return rettuple;
}

   
                                                                            
   
                                                  
   
BrinTuple *
brin_form_placeholder_tuple(BrinDesc *brdesc, BlockNumber blkno, Size *size)
{
  Size len;
  Size hoff;
  BrinTuple *rettuple;
  int keyno;
  bits8 *bitP;
  int bitmask;

                                                    
  len = SizeOfBrinTuple;
  len += BITMAPLEN(brdesc->bd_tupdesc->natts * 2);
  len = hoff = MAXALIGN(len);

  rettuple = palloc0(len);
  rettuple->bt_blkno = blkno;
  rettuple->bt_info = hoff;
  rettuple->bt_info |= BRIN_NULLS_MASK | BRIN_PLACEHOLDER_MASK;

  bitP = ((bits8 *)((char *)rettuple + SizeOfBrinTuple)) - 1;
  bitmask = HIGHBIT;
                                            
  for (keyno = 0; keyno < brdesc->bd_tupdesc->natts; keyno++)
  {
    if (bitmask != HIGHBIT)
    {
      bitmask <<= 1;
    }
    else
    {
      bitP += 1;
      *bitP = 0x0;
      bitmask = 1;
    }

    *bitP |= bitmask;
  }
                               

  *size = len;
  return rettuple;
}

   
                                           
   
void
brin_free_tuple(BrinTuple *tuple)
{
  pfree(tuple);
}

   
                                                                          
                                                                            
                                                                          
                                                                           
                                 
   
BrinTuple *
brin_copy_tuple(BrinTuple *tuple, Size len, BrinTuple *dest, Size *destsz)
{
  if (!destsz || *destsz == 0)
  {
    dest = palloc(len);
  }
  else if (len > *destsz)
  {
    dest = repalloc(dest, len);
    *destsz = len;
  }

  memcpy(dest, tuple, len);

  return dest;
}

   
                                                        
   
bool
brin_tuples_equal(const BrinTuple *a, Size alen, const BrinTuple *b, Size blen)
{
  if (alen != blen)
  {
    return false;
  }
  if (memcmp(a, b, alen) != 0)
  {
    return false;
  }
  return true;
}

   
                                                                         
          
   
                                                                              
                                   
   
BrinMemTuple *
brin_new_memtuple(BrinDesc *brdesc)
{
  BrinMemTuple *dtup;
  long basesize;

  basesize = MAXALIGN(sizeof(BrinMemTuple) + sizeof(BrinValues) * brdesc->bd_tupdesc->natts);
  dtup = palloc0(basesize + sizeof(Datum) * brdesc->bd_totalstored);

  dtup->bt_values = palloc(sizeof(Datum) * brdesc->bd_totalstored);
  dtup->bt_allnulls = palloc(sizeof(bool) * brdesc->bd_tupdesc->natts);
  dtup->bt_hasnulls = palloc(sizeof(bool) * brdesc->bd_tupdesc->natts);

  dtup->bt_context = AllocSetContextCreate(CurrentMemoryContext, "brin dtuple", ALLOCSET_DEFAULT_SIZES);

  brin_memtuple_initialize(dtup, brdesc);

  return dtup;
}

   
                                                                         
                           
   
BrinMemTuple *
brin_memtuple_initialize(BrinMemTuple *dtuple, BrinDesc *brdesc)
{
  int i;
  char *currdatum;

  MemoryContextReset(dtuple->bt_context);

  currdatum = (char *)dtuple + MAXALIGN(sizeof(BrinMemTuple) + sizeof(BrinValues) * brdesc->bd_tupdesc->natts);
  for (i = 0; i < brdesc->bd_tupdesc->natts; i++)
  {
    dtuple->bt_columns[i].bv_attno = i + 1;
    dtuple->bt_columns[i].bv_allnulls = true;
    dtuple->bt_columns[i].bv_hasnulls = false;
    dtuple->bt_columns[i].bv_values = (Datum *)currdatum;
    currdatum += sizeof(Datum) * brdesc->bd_info[i]->oi_nstored;
  }

  return dtuple;
}

   
                                                                       
                    
   
                                                                               
                                                                         
                                                                           
                                                          
   
                                                                                
                                                  
   
BrinMemTuple *
brin_deform_tuple(BrinDesc *brdesc, BrinTuple *tuple, BrinMemTuple *dMemtuple)
{
  BrinMemTuple *dtup;
  Datum *values;
  bool *allnulls;
  bool *hasnulls;
  char *tp;
  bits8 *nullbits;
  int keyno;
  int valueno;
  MemoryContext oldcxt;

  dtup = dMemtuple ? brin_memtuple_initialize(dMemtuple, brdesc) : brin_new_memtuple(brdesc);

  if (BrinTupleIsPlaceholder(tuple))
  {
    dtup->bt_placeholder = true;
  }
  dtup->bt_blkno = tuple->bt_blkno;

  values = dtup->bt_values;
  allnulls = dtup->bt_allnulls;
  hasnulls = dtup->bt_hasnulls;

  tp = (char *)tuple + BrinTupleDataOffset(tuple);

  if (BrinTupleHasNulls(tuple))
  {
    nullbits = (bits8 *)((char *)tuple + SizeOfBrinTuple);
  }
  else
  {
    nullbits = NULL;
  }
  brin_deconstruct_tuple(brdesc, tp, nullbits, BrinTupleHasNulls(tuple), values, allnulls, hasnulls);

     
                                                                           
                                                                            
     
  oldcxt = MemoryContextSwitchTo(dtup->bt_context);
  for (valueno = 0, keyno = 0; keyno < brdesc->bd_tupdesc->natts; keyno++)
  {
    int i;

    if (allnulls[keyno])
    {
      valueno += brdesc->bd_info[keyno]->oi_nstored;
      continue;
    }

       
                                                                           
                             
       
    for (i = 0; i < brdesc->bd_info[keyno]->oi_nstored; i++)
    {
      dtup->bt_columns[keyno].bv_values[i] = datumCopy(values[valueno++], brdesc->bd_info[keyno]->oi_typcache[i]->typbyval, brdesc->bd_info[keyno]->oi_typcache[i]->typlen);
    }

    dtup->bt_columns[keyno].bv_hasnulls = hasnulls[keyno];
    dtup->bt_columns[keyno].bv_allnulls = false;
  }

  MemoryContextSwitchTo(oldcxt);

  return dtup;
}

   
                          
                                                             
   
                      
                                                
                                       
                                               
                                            
                                                               
                                                              
                                                              
   
                                                     
   
static inline void
brin_deconstruct_tuple(BrinDesc *brdesc, char *tp, bits8 *nullbits, bool nulls, Datum *values, bool *allnulls, bool *hasnulls)
{
  int attnum;
  int stored;
  TupleDesc diskdsc;
  long off;

     
                                                                          
                                                                             
                                                                        
                                                                  
     
  for (attnum = 0; attnum < brdesc->bd_tupdesc->natts; attnum++)
  {
       
                                                                       
                                                                          
                  
       
    allnulls[attnum] = nulls && !att_isnull(attnum, nullbits);

       
                                                                         
                                                                        
                        
       
                                                                       
       
    hasnulls[attnum] = nulls && !att_isnull(brdesc->bd_tupdesc->natts + attnum, nullbits);
  }

     
                                                                           
                                                                           
                   
     
  diskdsc = brtuple_disk_tupdesc(brdesc);
  stored = 0;
  off = 0;
  for (attnum = 0; attnum < brdesc->bd_tupdesc->natts; attnum++)
  {
    int datumno;

    if (allnulls[attnum])
    {
      stored += brdesc->bd_info[attnum]->oi_nstored;
      continue;
    }

    for (datumno = 0; datumno < brdesc->bd_info[attnum]->oi_nstored; datumno++)
    {
      Form_pg_attribute thisatt = TupleDescAttr(diskdsc, stored);

      if (thisatt->attlen == -1)
      {
        off = att_align_pointer(off, thisatt->attalign, -1, tp + off);
      }
      else
      {
                                                           
        off = att_align_nominal(off, thisatt->attalign);
      }

      values[stored++] = fetchatt(thisatt, tp + off);

      off = att_addlength_pointer(off, thisatt->attlen, tp + off);
    }
  }
}
