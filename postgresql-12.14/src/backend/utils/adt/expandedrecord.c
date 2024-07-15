                                                                            
   
                    
                                                            
   
                                                                          
                                                                              
                                               
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/tuptoaster.h"
#include "catalog/heap.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/expandedrecord.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

                                               
static Size
ER_get_flat_size(ExpandedObjectHeader *eohptr);
static void
ER_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size);

static const ExpandedObjectMethods ER_methods = {ER_get_flat_size, ER_flatten_into};

                           
static void
ER_mc_callback(void *arg);
static MemoryContext
get_short_term_cxt(ExpandedRecordHeader *erh);
static void
build_dummy_expanded_header(ExpandedRecordHeader *main_erh);
static pg_noinline void
check_domain_for_new_field(ExpandedRecordHeader *erh, int fnumber, Datum newValue, bool isnull);
static pg_noinline void
check_domain_for_new_tuple(ExpandedRecordHeader *erh, HeapTuple tuple);

   
                                                            
   
                                                                     
   
                                                                      
                                                                    
                                                                
                                           
                                                       
   
                                                         
   
ExpandedRecordHeader *
make_expanded_record_from_typeid(Oid type_id, int32 typmod, MemoryContext parentcontext)
{
  ExpandedRecordHeader *erh;
  int flags = 0;
  TupleDesc tupdesc;
  uint64 tupdesc_id;
  MemoryContext objcxt;
  char *chunk;

  if (type_id != RECORDOID)
  {
       
                                                                           
                                                           
       
    TypeCacheEntry *typentry;

    typentry = lookup_type_cache(type_id, TYPECACHE_TUPDESC | TYPECACHE_DOMAIN_BASE_INFO);
    if (typentry->typtype == TYPTYPE_DOMAIN)
    {
      flags |= ER_FLAG_IS_DOMAIN;
      typentry = lookup_type_cache(typentry->domainBaseType, TYPECACHE_TUPDESC);
    }
    if (typentry->tupDesc == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(type_id))));
    }
    tupdesc = typentry->tupDesc;
    tupdesc_id = typentry->tupDesc_identifier;
  }
  else
  {
       
                                                                       
       
    tupdesc = lookup_rowtype_tupdesc(type_id, typmod);
    tupdesc_id = assign_record_type_identifier(type_id, typmod);
  }

     
                                                                          
                                                                             
                                                                             
                                                                        
                                                                   
     
  objcxt = AllocSetContextCreate(parentcontext, "expanded record", ALLOCSET_DEFAULT_SIZES);

     
                                                                       
                                                                            
                                                                            
                                                         
     
  erh = (ExpandedRecordHeader *)MemoryContextAlloc(objcxt, MAXALIGN(sizeof(ExpandedRecordHeader)) + tupdesc->natts * (sizeof(Datum) + sizeof(bool)));

                                                          
  memset(erh, 0, sizeof(ExpandedRecordHeader));

  EOH_init_header(&erh->hdr, &ER_methods, objcxt);
  erh->er_magic = ER_MAGIC;

                                                            
  chunk = (char *)erh + MAXALIGN(sizeof(ExpandedRecordHeader));
  erh->dvalues = (Datum *)chunk;
  erh->dnulls = (bool *)(chunk + tupdesc->natts * sizeof(Datum));
  erh->nfields = tupdesc->natts;

                                                  
  erh->er_decltypeid = type_id;
  erh->er_typeid = tupdesc->tdtypeid;
  erh->er_typmod = tupdesc->tdtypmod;
  erh->er_tupdesc_id = tupdesc_id;

  erh->flags = flags;

     
                                                                          
                                                                           
                                                                            
                                             
     
  if (tupdesc->tdrefcount >= 0)
  {
                                                   
    erh->er_mcb.func = ER_mc_callback;
    erh->er_mcb.arg = (void *)erh;
    MemoryContextRegisterResetCallback(erh->hdr.eoh_context, &erh->er_mcb);

                              
    erh->er_tupdesc = tupdesc;
    tupdesc->tdrefcount++;

                                                                      
    if (type_id == RECORDOID)
    {
      DecrTupleDescRefCount(tupdesc);
    }
  }
  else
  {
       
                                                                        
                                                                         
       
    erh->er_tupdesc = tupdesc;
  }

     
                                                                        
                                     
     

  return erh;
}

   
                                                                  
   
                                                                       
                             
   
                                                                      
                                                                    
   
                                                         
   
ExpandedRecordHeader *
make_expanded_record_from_tupdesc(TupleDesc tupdesc, MemoryContext parentcontext)
{
  ExpandedRecordHeader *erh;
  uint64 tupdesc_id;
  MemoryContext objcxt;
  MemoryContext oldcxt;
  char *chunk;

  if (tupdesc->tdtypeid != RECORDOID)
  {
       
                                                                           
                                                                     
                                                                          
                                                                      
       
                                                                    
                                
       
    TypeCacheEntry *typentry;

    typentry = lookup_type_cache(tupdesc->tdtypeid, TYPECACHE_TUPDESC);
    if (typentry->tupDesc == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(tupdesc->tdtypeid))));
    }
    tupdesc = typentry->tupDesc;
    tupdesc_id = typentry->tupDesc_identifier;
  }
  else
  {
       
                                                                         
                          
       
    tupdesc_id = assign_record_type_identifier(tupdesc->tdtypeid, tupdesc->tdtypmod);
  }

     
                                                                          
                                                                             
                                                    
     
  objcxt = AllocSetContextCreate(parentcontext, "expanded record", ALLOCSET_DEFAULT_SIZES);

     
                                                                       
                                                                            
                                                                            
                                                         
     
  erh = (ExpandedRecordHeader *)MemoryContextAlloc(objcxt, MAXALIGN(sizeof(ExpandedRecordHeader)) + tupdesc->natts * (sizeof(Datum) + sizeof(bool)));

                                                          
  memset(erh, 0, sizeof(ExpandedRecordHeader));

  EOH_init_header(&erh->hdr, &ER_methods, objcxt);
  erh->er_magic = ER_MAGIC;

                                                            
  chunk = (char *)erh + MAXALIGN(sizeof(ExpandedRecordHeader));
  erh->dvalues = (Datum *)chunk;
  erh->dnulls = (bool *)(chunk + tupdesc->natts * sizeof(Datum));
  erh->nfields = tupdesc->natts;

                                                  
  erh->er_decltypeid = erh->er_typeid = tupdesc->tdtypeid;
  erh->er_typmod = tupdesc->tdtypmod;
  erh->er_tupdesc_id = tupdesc_id;

     
                                                                             
                                                                       
                                                                      
                      
     
  if (tupdesc->tdrefcount >= 0)
  {
                                                   
    erh->er_mcb.func = ER_mc_callback;
    erh->er_mcb.arg = (void *)erh;
    MemoryContextRegisterResetCallback(erh->hdr.eoh_context, &erh->er_mcb);

                              
    erh->er_tupdesc = tupdesc;
    tupdesc->tdrefcount++;
  }
  else
  {
                      
    oldcxt = MemoryContextSwitchTo(objcxt);
    erh->er_tupdesc = CreateTupleDescCopy(tupdesc);
    erh->flags |= ER_FLAG_TUPDESC_ALLOCED;
    MemoryContextSwitchTo(oldcxt);
  }

     
                                                                        
                                     
     

  return erh;
}

   
                                                                             
   
                                                                          
                       
   
                                                                        
                                                 
   
                                                         
   
ExpandedRecordHeader *
make_expanded_record_from_exprecord(ExpandedRecordHeader *olderh, MemoryContext parentcontext)
{
  ExpandedRecordHeader *erh;
  TupleDesc tupdesc = expanded_record_get_tupdesc(olderh);
  MemoryContext objcxt;
  MemoryContext oldcxt;
  char *chunk;

     
                                                                          
                                                                             
                                                    
     
  objcxt = AllocSetContextCreate(parentcontext, "expanded record", ALLOCSET_DEFAULT_SIZES);

     
                                                                       
                                                                            
                                                                            
                                                         
     
  erh = (ExpandedRecordHeader *)MemoryContextAlloc(objcxt, MAXALIGN(sizeof(ExpandedRecordHeader)) + tupdesc->natts * (sizeof(Datum) + sizeof(bool)));

                                                          
  memset(erh, 0, sizeof(ExpandedRecordHeader));

  EOH_init_header(&erh->hdr, &ER_methods, objcxt);
  erh->er_magic = ER_MAGIC;

                                                            
  chunk = (char *)erh + MAXALIGN(sizeof(ExpandedRecordHeader));
  erh->dvalues = (Datum *)chunk;
  erh->dnulls = (bool *)(chunk + tupdesc->natts * sizeof(Datum));
  erh->nfields = tupdesc->natts;

                                                  
  erh->er_decltypeid = olderh->er_decltypeid;
  erh->er_typeid = olderh->er_typeid;
  erh->er_typmod = olderh->er_typmod;
  erh->er_tupdesc_id = olderh->er_tupdesc_id;

                                                          
  erh->flags = olderh->flags & ER_FLAG_IS_DOMAIN;

     
                                                                             
                                                                       
                                                                      
                      
     
  if (tupdesc->tdrefcount >= 0)
  {
                                                   
    erh->er_mcb.func = ER_mc_callback;
    erh->er_mcb.arg = (void *)erh;
    MemoryContextRegisterResetCallback(erh->hdr.eoh_context, &erh->er_mcb);

                              
    erh->er_tupdesc = tupdesc;
    tupdesc->tdrefcount++;
  }
  else if (olderh->flags & ER_FLAG_TUPDESC_ALLOCED)
  {
                                                     
    oldcxt = MemoryContextSwitchTo(objcxt);
    erh->er_tupdesc = CreateTupleDescCopy(tupdesc);
    erh->flags |= ER_FLAG_TUPDESC_ALLOCED;
    MemoryContextSwitchTo(oldcxt);
  }
  else
  {
       
                                                                       
                                                         
       
    erh->er_tupdesc = tupdesc;
  }

     
                                                                        
                                     
     

  return erh;
}

   
                                                          
   
                                                                     
                                                                      
                          
   
                                                                           
                                                                            
                                                  
   
                                                                      
                                                                             
                                                              
   
                                                                            
                       
   
void
expanded_record_set_tuple(ExpandedRecordHeader *erh, HeapTuple tuple, bool copy, bool expand_external)
{
  int oldflags;
  HeapTuple oldtuple;
  char *oldfstartptr;
  char *oldfendptr;
  int newflags;
  HeapTuple newtuple;
  MemoryContext oldcxt;

                                                                     
  Assert(!(erh->flags & ER_FLAG_IS_DUMMY));

     
                                                                          
     
  if (erh->flags & ER_FLAG_IS_DOMAIN)
  {
    check_domain_for_new_tuple(erh, tuple);
  }

     
                                                                         
                                                                        
                     
     
  if (expand_external && tuple)
  {
                                                       
    Assert(copy);
    if (HeapTupleHasExternal(tuple))
    {
      oldcxt = MemoryContextSwitchTo(get_short_term_cxt(erh));
      tuple = toast_flatten_tuple(tuple, erh->er_tupdesc);
      MemoryContextSwitchTo(oldcxt);
    }
    else
    {
      expand_external = false;                              
    }
  }

     
                                                              
     
  oldflags = erh->flags;
  newflags = oldflags & ER_FLAGS_NON_DATA;

     
                                                                             
                                                            
     
  if (copy && tuple)
  {
    oldcxt = MemoryContextSwitchTo(erh->hdr.eoh_context);
    newtuple = heap_copytuple(tuple);
    newflags |= ER_FLAG_FVALUE_ALLOCED;
    MemoryContextSwitchTo(oldcxt);

                                                                      
    if (expand_external)
    {
      MemoryContextReset(erh->er_short_term_cxt);
    }
  }
  else
  {
    newtuple = tuple;
  }

                                                      
  oldtuple = erh->fvalue;
  oldfstartptr = erh->fstartptr;
  oldfendptr = erh->fendptr;

     
                                                          
     
  if (newtuple)
  {
                                  
    erh->fvalue = newtuple;
    erh->fstartptr = (char *)newtuple->t_data;
    erh->fendptr = ((char *)newtuple->t_data) + newtuple->t_len;
    newflags |= ER_FLAG_FVALUE_VALID;

                                                          
    if (HeapTupleHasExternal(newtuple))
    {
      newflags |= ER_FLAG_HAVE_EXTERNAL;
    }
  }
  else
  {
    erh->fvalue = NULL;
    erh->fstartptr = erh->fendptr = NULL;
  }

  erh->flags = newflags;

                                                                  
  erh->flat_size = 0;

     
                                                                           
                                                                           
                                                                        
                                         
     
  if (oldflags & ER_FLAG_DVALUES_ALLOCED)
  {
    TupleDesc tupdesc = erh->er_tupdesc;
    int i;

    for (i = 0; i < erh->nfields; i++)
    {
      if (!erh->dnulls[i] && !(TupleDescAttr(tupdesc, i)->attbyval))
      {
        char *oldValue = (char *)DatumGetPointer(erh->dvalues[i]);

        if (oldValue < oldfstartptr || oldValue >= oldfendptr)
        {
          pfree(oldValue);
        }
      }
    }
  }

                                                                
  if (oldflags & ER_FLAG_FVALUE_ALLOCED)
  {
    heap_freetuple(oldtuple);
  }

                                                                            
}

   
                                                                               
   
                                                                       
                                                                        
                                                                 
   
                                                         
   
                                                                           
                                              
   
Datum
make_expanded_record_from_datum(Datum recorddatum, MemoryContext parentcontext)
{
  ExpandedRecordHeader *erh;
  HeapTupleHeader tuphdr;
  HeapTupleData tmptup;
  HeapTuple newtuple;
  MemoryContext objcxt;
  MemoryContext oldcxt;

     
                                                                          
                                                                             
                                                    
     
  objcxt = AllocSetContextCreate(parentcontext, "expanded record", ALLOCSET_DEFAULT_SIZES);

                                                                    
  erh = (ExpandedRecordHeader *)MemoryContextAllocZero(objcxt, sizeof(ExpandedRecordHeader));

  EOH_init_header(&erh->hdr, &ER_methods, objcxt);
  erh->er_magic = ER_MAGIC;

     
                                                                          
                                                                           
                                                                      
     
  tuphdr = DatumGetHeapTupleHeader(recorddatum);

  tmptup.t_len = HeapTupleHeaderGetDatumLength(tuphdr);
  ItemPointerSetInvalid(&(tmptup.t_self));
  tmptup.t_tableOid = InvalidOid;
  tmptup.t_data = tuphdr;

  oldcxt = MemoryContextSwitchTo(objcxt);
  newtuple = heap_copytuple(&tmptup);
  erh->flags |= ER_FLAG_FVALUE_ALLOCED;
  MemoryContextSwitchTo(oldcxt);

                                                  
  erh->er_decltypeid = erh->er_typeid = HeapTupleHeaderGetTypeId(tuphdr);
  erh->er_typmod = HeapTupleHeaderGetTypMod(tuphdr);

                                              
  erh->fvalue = newtuple;
  erh->fstartptr = (char *)newtuple->t_data;
  erh->fendptr = ((char *)newtuple->t_data) + newtuple->t_len;
  erh->flags |= ER_FLAG_FVALUE_VALID;

                                                   
  Assert(!HeapTupleHeaderHasExternal(tuphdr));

     
                                                                            
                                                                      
                      
     

                                                   
  return EOHPGetRWDatum(&erh->hdr);
}

   
                                             
   
                                                                          
                                                    
   
static Size
ER_get_flat_size(ExpandedObjectHeader *eohptr)
{
  ExpandedRecordHeader *erh = (ExpandedRecordHeader *)eohptr;
  TupleDesc tupdesc;
  Size len;
  Size data_len;
  int hoff;
  bool hasnull;
  int i;

  Assert(erh->er_magic == ER_MAGIC);

     
                                                                           
                                                            
     
  if (erh->er_typeid == RECORDOID && erh->er_typmod < 0)
  {
    tupdesc = expanded_record_get_tupdesc(erh);
    assign_record_type_typmod(tupdesc);
    erh->er_typmod = tupdesc->tdtypmod;
  }

     
                                                                           
                        
     
  if (erh->flags & ER_FLAG_FVALUE_VALID && !(erh->flags & ER_FLAG_HAVE_EXTERNAL))
  {
    return erh->fvalue->t_len;
  }

                                                    
  if (erh->flat_size)
  {
    return erh->flat_size;
  }

                                                          
  if (!(erh->flags & ER_FLAG_DVALUES_VALID))
  {
    deconstruct_expanded_record(erh);
  }

                                             
  tupdesc = erh->er_tupdesc;

     
                                                              
     
  if (erh->flags & ER_FLAG_HAVE_EXTERNAL)
  {
    for (i = 0; i < erh->nfields; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

      if (!erh->dnulls[i] && !attr->attbyval && attr->attlen == -1 && VARATT_IS_EXTERNAL(DatumGetPointer(erh->dvalues[i])))
      {
           
                                                                     
                                                                  
           
        expanded_record_set_field_internal(erh, i + 1, erh->dvalues[i], false, true, false);
      }
    }

       
                                                                          
                                                                          
                                                                       
                                     
       
    erh->flags &= ~ER_FLAG_HAVE_EXTERNAL;
  }

                                                 
  hasnull = false;
  for (i = 0; i < erh->nfields; i++)
  {
    if (erh->dnulls[i])
    {
      hasnull = true;
      break;
    }
  }

                                    
  len = offsetof(HeapTupleHeaderData, t_bits);

  if (hasnull)
  {
    len += BITMAPLEN(tupdesc->natts);
  }

  hoff = len = MAXALIGN(len);                             

  data_len = heap_compute_data_size(tupdesc, erh->dvalues, erh->dnulls);

  len += data_len;

                           
  erh->flat_size = len;
  erh->data_len = data_len;
  erh->hoff = hoff;
  erh->hasnull = hasnull;

  return len;
}

   
                                            
   
static void
ER_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size)
{
  ExpandedRecordHeader *erh = (ExpandedRecordHeader *)eohptr;
  HeapTupleHeader tuphdr = (HeapTupleHeader)result;
  TupleDesc tupdesc;

  Assert(erh->er_magic == ER_MAGIC);

                                                                          
  if (erh->flags & ER_FLAG_FVALUE_VALID && !(erh->flags & ER_FLAG_HAVE_EXTERNAL))
  {
    Assert(allocated_size == erh->fvalue->t_len);
    memcpy(tuphdr, erh->fvalue->t_data, allocated_size);
                                                                         
    HeapTupleHeaderSetDatumLength(tuphdr, allocated_size);
    HeapTupleHeaderSetTypeId(tuphdr, erh->er_typeid);
    HeapTupleHeaderSetTypMod(tuphdr, erh->er_typmod);
    return;
  }

                                                                  
  Assert(allocated_size == erh->flat_size);

                                       
  tupdesc = expanded_record_get_tupdesc(erh);

                                                        
  memset(tuphdr, 0, allocated_size);

                                               
  HeapTupleHeaderSetDatumLength(tuphdr, allocated_size);
  HeapTupleHeaderSetTypeId(tuphdr, erh->er_typeid);
  HeapTupleHeaderSetTypMod(tuphdr, erh->er_typmod);
                                                                      
  ItemPointerSetInvalid(&(tuphdr->t_ctid));

  HeapTupleHeaderSetNatts(tuphdr, tupdesc->natts);
  tuphdr->t_hoff = erh->hoff;

                                                  
  heap_fill_tuple(tupdesc, erh->dvalues, erh->dnulls, (char *)tuphdr + erh->hoff, erh->data_len, &tuphdr->t_infomask, (erh->hasnull ? tuphdr->t_bits : NULL));
}

   
                                                             
   
                                                               
                                                                             
                                                                          
                                   
   
TupleDesc
expanded_record_fetch_tupdesc(ExpandedRecordHeader *erh)
{
  TupleDesc tupdesc;

                                                                           
  if (erh->er_tupdesc)
  {
    return erh->er_tupdesc;
  }

                                                              
  tupdesc = lookup_rowtype_tupdesc(erh->er_typeid, erh->er_typmod);

     
                                                                             
                                                                            
                                                                      
                      
     
  if (tupdesc->tdrefcount >= 0)
  {
                                                
    if (erh->er_mcb.arg == NULL)
    {
      erh->er_mcb.func = ER_mc_callback;
      erh->er_mcb.arg = (void *)erh;
      MemoryContextRegisterResetCallback(erh->hdr.eoh_context, &erh->er_mcb);
    }

                                  
    erh->er_tupdesc = tupdesc;
    tupdesc->tdrefcount++;

                                                         
    DecrTupleDescRefCount(tupdesc);
  }
  else
  {
                                   
    erh->er_tupdesc = tupdesc;
  }

                                                                    
  erh->er_tupdesc_id = assign_record_type_identifier(tupdesc->tdtypeid, tupdesc->tdtypmod);

  return tupdesc;
}

   
                                                                         
   
                                                                         
                                                                            
                                                                       
                                                                        
          
   
                                             
   
HeapTuple
expanded_record_get_tuple(ExpandedRecordHeader *erh)
{
                                                 
  if (erh->flags & ER_FLAG_FVALUE_VALID)
  {
    return erh->fvalue;
  }

                                           
  if (erh->flags & ER_FLAG_DVALUES_VALID)
  {
    return heap_form_tuple(erh->er_tupdesc, erh->dvalues, erh->dnulls);
  }

                                
  return NULL;
}

   
                                                                    
   
static void
ER_mc_callback(void *arg)
{
  ExpandedRecordHeader *erh = (ExpandedRecordHeader *)arg;
  TupleDesc tupdesc = erh->er_tupdesc;

                                                              
  if (tupdesc)
  {
    erh->er_tupdesc = NULL;                    
    if (tupdesc->tdrefcount > 0)
    {
      if (--tupdesc->tdrefcount == 0)
      {
        FreeTupleDesc(tupdesc);
      }
    }
  }
}

   
                                                                                 
   
                                                                         
                                                                            
                                                    
   
ExpandedRecordHeader *
DatumGetExpandedRecord(Datum d)
{
                                                                  
  if (VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(d)))
  {
    ExpandedRecordHeader *erh = (ExpandedRecordHeader *)DatumGetEOHP(d);

    Assert(erh->er_magic == ER_MAGIC);
    return erh;
  }

                                
  d = make_expanded_record_from_datum(d, CurrentMemoryContext);
  return (ExpandedRecordHeader *)DatumGetEOHP(d);
}

   
                                                                       
                                                                        
                                                                        
   
                                                                         
                                   
   
void
deconstruct_expanded_record(ExpandedRecordHeader *erh)
{
  TupleDesc tupdesc;
  Datum *dvalues;
  bool *dnulls;
  int nfields;

  if (erh->flags & ER_FLAG_DVALUES_VALID)
  {
    return;                                   
  }

                                       
  tupdesc = expanded_record_get_tupdesc(erh);

     
                                                                            
                                                                          
                                                                             
                                          
     
  nfields = tupdesc->natts;
  if (erh->dvalues == NULL || erh->nfields != nfields)
  {
    char *chunk;

       
                                                                     
                                   
       
    chunk = MemoryContextAlloc(erh->hdr.eoh_context, nfields * (sizeof(Datum) + sizeof(bool)));
    dvalues = (Datum *)chunk;
    dnulls = (bool *)(chunk + nfields * sizeof(Datum));
    erh->dvalues = dvalues;
    erh->dnulls = dnulls;
    erh->nfields = nfields;
  }
  else
  {
    dvalues = erh->dvalues;
    dnulls = erh->dnulls;
  }

  if (erh->flags & ER_FLAG_FVALUE_VALID)
  {
                           
    heap_deform_tuple(erh->fvalue, tupdesc, dvalues, dnulls);
  }
  else
  {
                                                               
    memset(dvalues, 0, nfields * sizeof(Datum));
    memset(dnulls, true, nfields * sizeof(bool));
  }

                                 
  erh->flags |= ER_FLAG_DVALUES_VALID;
}

   
                                  
   
                                                                        
                                                                    
   
bool
expanded_record_lookup_field(ExpandedRecordHeader *erh, const char *fieldname, ExpandedRecordFieldInfo *finfo)
{
  TupleDesc tupdesc;
  int fno;
  Form_pg_attribute attr;
  const FormData_pg_attribute *sysattr;

  tupdesc = expanded_record_get_tupdesc(erh);

                                            
  for (fno = 0; fno < tupdesc->natts; fno++)
  {
    attr = TupleDescAttr(tupdesc, fno);
    if (namestrcmp(&attr->attname, fieldname) == 0 && !attr->attisdropped)
    {
      finfo->fnumber = attr->attnum;
      finfo->ftypeid = attr->atttypid;
      finfo->ftypmod = attr->atttypmod;
      finfo->fcollation = attr->attcollation;
      return true;
    }
  }

                                    
  sysattr = SystemAttributeByName(fieldname);
  if (sysattr != NULL)
  {
    finfo->fnumber = sysattr->attnum;
    finfo->ftypeid = sysattr->atttypid;
    finfo->ftypmod = sysattr->atttypmod;
    finfo->fcollation = sysattr->attcollation;
    return true;
  }

  return false;
}

   
                               
   
                                                                      
                           
   
Datum
expanded_record_fetch_field(ExpandedRecordHeader *erh, int fnumber, bool *isnull)
{
  if (fnumber > 0)
  {
                                      
    if (ExpandedRecordIsEmpty(erh))
    {
      *isnull = true;
      return (Datum)0;
    }
                                              
    deconstruct_expanded_record(erh);
                                                 
    if (unlikely(fnumber > erh->nfields))
    {
      *isnull = true;
      return (Datum)0;
    }
    *isnull = erh->dnulls[fnumber - 1];
    return erh->dvalues[fnumber - 1];
  }
  else
  {
                                                                  
    if (erh->fvalue == NULL)
    {
      *isnull = true;
      return (Datum)0;
    }
                                                                         
    return heap_getsysattr(erh->fvalue, fnumber, NULL, isnull);
  }
}

   
                             
   
                                                                             
                                                                           
                
   
                                                                          
                                                                               
   
                                                                           
                                                                  
   
void
expanded_record_set_field_internal(ExpandedRecordHeader *erh, int fnumber, Datum newValue, bool isnull, bool expand_external, bool check_constraints)
{
  TupleDesc tupdesc;
  Form_pg_attribute attr;
  Datum *dvalues;
  bool *dnulls;
  char *oldValue;

     
                                                                           
                                                         
     
  Assert(!(erh->flags & ER_FLAG_IS_DUMMY) || !check_constraints);

                                                                           
  if ((erh->flags & ER_FLAG_IS_DOMAIN) && check_constraints)
  {
    check_domain_for_new_field(erh, fnumber, newValue, isnull);
  }

                                                          
  if (!(erh->flags & ER_FLAG_DVALUES_VALID))
  {
    deconstruct_expanded_record(erh);
  }

                                             
  tupdesc = erh->er_tupdesc;
  Assert(erh->nfields == tupdesc->natts);

                                                                      
  if (unlikely(fnumber <= 0 || fnumber > erh->nfields))
  {
    elog(ERROR, "cannot assign to field %d of expanded record", fnumber);
  }

     
                                                                           
                
     
  attr = TupleDescAttr(tupdesc, fnumber - 1);
  if (!isnull && !attr->attbyval)
  {
    MemoryContext oldcxt;

                                                  
    if (expand_external)
    {
      if (attr->attlen == -1 && VARATT_IS_EXTERNAL(DatumGetPointer(newValue)))
      {
                                                               
        oldcxt = MemoryContextSwitchTo(get_short_term_cxt(erh));
        newValue = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(newValue)));
        MemoryContextSwitchTo(oldcxt);
      }
      else
      {
        expand_external = false;                              
      }
    }

                                          
    oldcxt = MemoryContextSwitchTo(erh->hdr.eoh_context);
    newValue = datumCopy(newValue, false, attr->attlen);
    MemoryContextSwitchTo(oldcxt);

                                                                     
    if (expand_external)
    {
      MemoryContextReset(erh->er_short_term_cxt);
    }

                                                                    
    erh->flags |= ER_FLAG_DVALUES_ALLOCED;

       
                                                                      
                                                                          
                                                                           
                                                    
       
    if (attr->attlen == -1 && VARATT_IS_EXTERNAL(DatumGetPointer(newValue)))
    {
      erh->flags |= ER_FLAG_HAVE_EXTERNAL;
    }
  }

     
                                               
     
  dvalues = erh->dvalues;
  dnulls = erh->dnulls;

                                                                  
  erh->flags &= ~ER_FLAG_FVALUE_VALID;
                                                   
  erh->flat_size = 0;

                                                      
  if (!attr->attbyval && !dnulls[fnumber - 1])
  {
    oldValue = (char *)DatumGetPointer(dvalues[fnumber - 1]);
  }
  else
  {
    oldValue = NULL;
  }

                                                
  dvalues[fnumber - 1] = newValue;
  dnulls[fnumber - 1] = isnull;

     
                                                                           
                                                                          
                         
     
                                                                       
                                                                            
                                                                             
                                                                        
              
     
  if (oldValue && !(erh->flags & ER_FLAG_IS_DUMMY))
  {
                                                               
    if (oldValue < erh->fstartptr || oldValue >= erh->fendptr)
    {
      pfree(oldValue);
    }
  }
}

   
                           
   
                                                                      
                                                      
   
                                                                               
                                                           
   
                                                                             
                                                                          
                                                                        
                                                                             
                                                                          
                               
   
void
expanded_record_set_fields(ExpandedRecordHeader *erh, const Datum *newValues, const bool *isnulls, bool expand_external)
{
  TupleDesc tupdesc;
  Datum *dvalues;
  bool *dnulls;
  int fnumber;
  MemoryContext oldcxt;

                                                                     
  Assert(!(erh->flags & ER_FLAG_IS_DUMMY));

                                                          
  if (!(erh->flags & ER_FLAG_DVALUES_VALID))
  {
    deconstruct_expanded_record(erh);
  }

                                             
  tupdesc = erh->er_tupdesc;
  Assert(erh->nfields == tupdesc->natts);

                                                                  
  erh->flags &= ~ER_FLAG_FVALUE_VALID;
                                                   
  erh->flat_size = 0;

  oldcxt = MemoryContextSwitchTo(erh->hdr.eoh_context);

  dvalues = erh->dvalues;
  dnulls = erh->dnulls;

  for (fnumber = 0; fnumber < erh->nfields; fnumber++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, fnumber);
    Datum newValue;
    bool isnull;

                                
    if (attr->attisdropped)
    {
      continue;
    }

    newValue = newValues[fnumber];
    isnull = isnulls[fnumber];

    if (!attr->attbyval)
    {
         
                                                                   
                                
         
      if (!isnull)
      {
                                              
        if (attr->attlen == -1 && VARATT_IS_EXTERNAL(DatumGetPointer(newValue)))
        {
          if (expand_external)
          {
                                                              
            newValue = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(newValue)));
          }
          else
          {
                                     
            newValue = datumCopy(newValue, false, -1);
                                                       
            if (VARATT_IS_EXTERNAL(DatumGetPointer(newValue)))
            {
              erh->flags |= ER_FLAG_HAVE_EXTERNAL;
            }
          }
        }
        else
        {
                                                   
          newValue = datumCopy(newValue, false, attr->attlen);
        }

                                                                    
        erh->flags |= ER_FLAG_DVALUES_ALLOCED;
      }

         
                                                                         
                                                
         
      if (unlikely(!dnulls[fnumber]))
      {
        char *oldValue;

        oldValue = (char *)DatumGetPointer(dvalues[fnumber]);
                                                                   
        if (oldValue < erh->fstartptr || oldValue >= erh->fendptr)
        {
          pfree(oldValue);
        }
      }
    }

                                                  
    dvalues[fnumber] = newValue;
    dnulls[fnumber] = isnull;
  }

     
                                                                             
                                                                             
                        
     
  if (erh->flags & ER_FLAG_IS_DOMAIN)
  {
                                                                     
    MemoryContextSwitchTo(get_short_term_cxt(erh));

    domain_check(ExpandedRecordGetRODatum(erh), false, erh->er_decltypeid, &erh->er_domaininfo, erh->hdr.eoh_context);
  }

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                          
   
                                                                        
   
                                                                            
                                                                               
                                                                              
                                                                            
   
static MemoryContext
get_short_term_cxt(ExpandedRecordHeader *erh)
{
  if (erh->er_short_term_cxt == NULL)
  {
    erh->er_short_term_cxt = AllocSetContextCreate(erh->hdr.eoh_context, "expanded record short-term context", ALLOCSET_SMALL_SIZES);
  }
  else
  {
    MemoryContextReset(erh->er_short_term_cxt);
  }
  return erh->er_short_term_cxt;
}

   
                                                             
   
                                                                        
                                                                      
                                                                         
                                                                     
                                                                    
                             
   
static void
build_dummy_expanded_header(ExpandedRecordHeader *main_erh)
{
  ExpandedRecordHeader *erh;
  TupleDesc tupdesc = expanded_record_get_tupdesc(main_erh);

                                            
  (void)get_short_term_cxt(main_erh);

     
                                                                           
                                                                           
                                                               
     
  erh = main_erh->er_dummy_header;
  if (erh == NULL || erh->nfields != tupdesc->natts)
  {
    char *chunk;

    erh = (ExpandedRecordHeader *)MemoryContextAlloc(main_erh->hdr.eoh_context, MAXALIGN(sizeof(ExpandedRecordHeader)) + tupdesc->natts * (sizeof(Datum) + sizeof(bool)));

                                                            
    memset(erh, 0, sizeof(ExpandedRecordHeader));

       
                                                                     
                                                                    
                                                                     
                                                                      
                                                                          
                                                                         
                                                                          
                                                                         
                                                      
       
    EOH_init_header(&erh->hdr, &ER_methods, main_erh->er_short_term_cxt);
    erh->er_magic = ER_MAGIC;

                                                              
    chunk = (char *)erh + MAXALIGN(sizeof(ExpandedRecordHeader));
    erh->dvalues = (Datum *)chunk;
    erh->dnulls = (bool *)(chunk + tupdesc->natts * sizeof(Datum));
    erh->nfields = tupdesc->natts;

       
                                                                     
                                                                           
                                                                      
                                                                           
       

    main_erh->er_dummy_header = erh;
  }

     
                                                                            
                                                                             
                                                                           
                                                                            
                                                                            
                                               
     
  erh->flags = ER_FLAG_IS_DUMMY;

                                               
  erh->er_decltypeid = erh->er_typeid = main_erh->er_typeid;
  erh->er_typmod = main_erh->er_typmod;

                                                           
  erh->er_tupdesc = tupdesc;
  erh->er_tupdesc_id = main_erh->er_tupdesc_id;

     
                                                                          
                                                                            
                                                                            
            
     
  erh->flat_size = 0;

                                                                            
  erh->fvalue = main_erh->fvalue;
  erh->fstartptr = main_erh->fstartptr;
  erh->fendptr = main_erh->fendptr;
}

   
                                                         
   
static pg_noinline void
check_domain_for_new_field(ExpandedRecordHeader *erh, int fnumber, Datum newValue, bool isnull)
{
  ExpandedRecordHeader *dummy_erh;
  MemoryContext oldcxt;

                                                                
  build_dummy_expanded_header(erh);
  dummy_erh = erh->er_dummy_header;

     
                                                                          
                                                                             
                                                                      
     
  if (!ExpandedRecordIsEmpty(erh))
  {
    deconstruct_expanded_record(erh);
    memcpy(dummy_erh->dvalues, erh->dvalues, dummy_erh->nfields * sizeof(Datum));
    memcpy(dummy_erh->dnulls, erh->dnulls, dummy_erh->nfields * sizeof(bool));
                                                         
    dummy_erh->flags |= erh->flags & ER_FLAG_HAVE_EXTERNAL;
  }
  else
  {
    memset(dummy_erh->dvalues, 0, dummy_erh->nfields * sizeof(Datum));
    memset(dummy_erh->dnulls, true, dummy_erh->nfields * sizeof(bool));
  }

                                             
  dummy_erh->flags |= ER_FLAG_DVALUES_VALID;

                                                                      
  if (unlikely(fnumber <= 0 || fnumber > dummy_erh->nfields))
  {
    elog(ERROR, "cannot assign to field %d of expanded record", fnumber);
  }

                                                        
  dummy_erh->dvalues[fnumber - 1] = newValue;
  dummy_erh->dnulls[fnumber - 1] = isnull;

     
                                                                             
                                                                             
                                                                           
              
     
  if (!isnull)
  {
    Form_pg_attribute attr = TupleDescAttr(erh->er_tupdesc, fnumber - 1);

    if (!attr->attbyval && attr->attlen == -1 && VARATT_IS_EXTERNAL(DatumGetPointer(newValue)))
    {
      dummy_erh->flags |= ER_FLAG_HAVE_EXTERNAL;
    }
  }

     
                                                                        
                                                       
     
  oldcxt = MemoryContextSwitchTo(erh->er_short_term_cxt);

     
                                                                             
                                                          
     
  domain_check(ExpandedRecordGetRODatum(dummy_erh), false, erh->er_decltypeid, &erh->er_domaininfo, erh->hdr.eoh_context);

  MemoryContextSwitchTo(oldcxt);

                                                    
  MemoryContextReset(erh->er_short_term_cxt);
}

   
                                                         
   
static pg_noinline void
check_domain_for_new_tuple(ExpandedRecordHeader *erh, HeapTuple tuple)
{
  ExpandedRecordHeader *dummy_erh;
  MemoryContext oldcxt;

                                                                          
  if (tuple == NULL)
  {
                                                                     
    oldcxt = MemoryContextSwitchTo(get_short_term_cxt(erh));

    domain_check((Datum)0, true, erh->er_decltypeid, &erh->er_domaininfo, erh->hdr.eoh_context);

    MemoryContextSwitchTo(oldcxt);

                                                      
    MemoryContextReset(erh->er_short_term_cxt);

    return;
  }

                                                           
  build_dummy_expanded_header(erh);
  dummy_erh = erh->er_dummy_header;

                                                                        
  dummy_erh->fvalue = tuple;
  dummy_erh->fstartptr = (char *)tuple->t_data;
  dummy_erh->fendptr = ((char *)tuple->t_data) + tuple->t_len;
  dummy_erh->flags |= ER_FLAG_FVALUE_VALID;

                                                        
  if (HeapTupleHasExternal(tuple))
  {
    dummy_erh->flags |= ER_FLAG_HAVE_EXTERNAL;
  }

     
                                                                        
                                                       
     
  oldcxt = MemoryContextSwitchTo(erh->er_short_term_cxt);

     
                                                                             
                                                          
     
  domain_check(ExpandedRecordGetRODatum(dummy_erh), false, erh->er_decltypeid, &erh->er_domaininfo, erh->hdr.eoh_context);

  MemoryContextSwitchTo(oldcxt);

                                                    
  MemoryContextReset(erh->er_short_term_cxt);
}
