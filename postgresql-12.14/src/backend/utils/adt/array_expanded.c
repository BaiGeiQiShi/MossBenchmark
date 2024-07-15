                                                                            
   
                    
                                                       
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "access/tupmacs.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

                                               
static Size
EA_get_flat_size(ExpandedObjectHeader *eohptr);
static void
EA_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size);

static const ExpandedObjectMethods EA_methods = {EA_get_flat_size, EA_flatten_into};

                           
static void
copy_byval_expanded_array(ExpandedArrayHeader *eah, ExpandedArrayHeader *oldeah);

   
                                                               
   
                                                         
   
                                                                             
                                                                             
                                                                           
                                                                  
   
Datum
expand_array(Datum arraydatum, MemoryContext parentcontext, ArrayMetaState *metacache)
{
  ArrayType *array;
  ExpandedArrayHeader *eah;
  MemoryContext objcxt;
  MemoryContext oldcxt;
  ArrayMetaState fakecache;

     
                                                                         
                                                                          
                                                
     
  objcxt = AllocSetContextCreate(parentcontext, "expanded array", ALLOCSET_START_SMALL_SIZES);

                                    
  eah = (ExpandedArrayHeader *)MemoryContextAlloc(objcxt, sizeof(ExpandedArrayHeader));

  EOH_init_header(&eah->hdr, &EA_methods, objcxt);
  eah->ea_magic = EA_MAGIC;

                                                                      
  if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(arraydatum)))
  {
    ExpandedArrayHeader *oldeah = (ExpandedArrayHeader *)DatumGetEOHP(arraydatum);

    Assert(oldeah->ea_magic == EA_MAGIC);

       
                                                                          
                                                                         
                                                                         
                                                                      
                                                         
       
    if (metacache == NULL)
    {
      metacache = &fakecache;
    }
    metacache->element_type = oldeah->element_type;
    metacache->typlen = oldeah->typlen;
    metacache->typbyval = oldeah->typbyval;
    metacache->typalign = oldeah->typalign;

       
                                                                  
                                                                        
                                                                    
                                                      
       
    if (oldeah->typbyval && oldeah->dvalues != NULL)
    {
      copy_byval_expanded_array(eah, oldeah);
                                                      
      return EOHPGetRWDatum(&eah->hdr);
    }

       
                                                                   
                                                                       
                                                                        
                                                                          
                                                                          
                                                                         
                                                                          
                                                    
       
  }

     
                                                                          
     
                                                                            
                                                                           
                                                                          
                                                                             
                       
     
  oldcxt = MemoryContextSwitchTo(objcxt);
  array = DatumGetArrayTypePCopy(arraydatum);
  MemoryContextSwitchTo(oldcxt);

  eah->ndims = ARR_NDIM(array);
                                                         
  eah->dims = ARR_DIMS(array);
  eah->lbound = ARR_LBOUND(array);

                                                             
  eah->element_type = ARR_ELEMTYPE(array);
  if (metacache && metacache->element_type == eah->element_type)
  {
                                                        
    eah->typlen = metacache->typlen;
    eah->typbyval = metacache->typbyval;
    eah->typalign = metacache->typalign;
  }
  else
  {
                           
    get_typlenbyvalalign(eah->element_type, &eah->typlen, &eah->typbyval, &eah->typalign);
                                  
    if (metacache)
    {
      metacache->element_type = eah->element_type;
      metacache->typlen = eah->typlen;
      metacache->typbyval = eah->typbyval;
      metacache->typalign = eah->typalign;
    }
  }

                                                        
  eah->dvalues = NULL;
  eah->dnulls = NULL;
  eah->dvalueslen = 0;
  eah->nelems = 0;
  eah->flat_size = 0;

                                              
  eah->fvalue = array;
  eah->fstartptr = ARR_DATA_PTR(array);
  eah->fendptr = ((char *)array) + ARR_SIZE(array);

                                                  
  return EOHPGetRWDatum(&eah->hdr);
}

   
                                                                            
   
static void
copy_byval_expanded_array(ExpandedArrayHeader *eah, ExpandedArrayHeader *oldeah)
{
  MemoryContext objcxt = eah->hdr.eoh_context;
  int ndims = oldeah->ndims;
  int dvalueslen = oldeah->dvalueslen;

                                             
  eah->ndims = ndims;
                                                               
  eah->dims = (int *)MemoryContextAlloc(objcxt, ndims * 2 * sizeof(int));
  eah->lbound = eah->dims + ndims;
                                                              
  memcpy(eah->dims, oldeah->dims, ndims * sizeof(int));
  memcpy(eah->lbound, oldeah->lbound, ndims * sizeof(int));

                              
  eah->element_type = oldeah->element_type;
  eah->typlen = oldeah->typlen;
  eah->typbyval = oldeah->typbyval;
  eah->typalign = oldeah->typalign;

                                             
  eah->dvalues = (Datum *)MemoryContextAlloc(objcxt, dvalueslen * sizeof(Datum));
  memcpy(eah->dvalues, oldeah->dvalues, dvalueslen * sizeof(Datum));
  if (oldeah->dnulls)
  {
    eah->dnulls = (bool *)MemoryContextAlloc(objcxt, dvalueslen * sizeof(bool));
    memcpy(eah->dnulls, oldeah->dnulls, dvalueslen * sizeof(bool));
  }
  else
  {
    eah->dnulls = NULL;
  }
  eah->dvalueslen = dvalueslen;
  eah->nelems = oldeah->nelems;
  eah->flat_size = oldeah->flat_size;

                                           
  eah->fvalue = NULL;
  eah->fstartptr = NULL;
  eah->fendptr = NULL;
}

   
                                            
   
static Size
EA_get_flat_size(ExpandedObjectHeader *eohptr)
{
  ExpandedArrayHeader *eah = (ExpandedArrayHeader *)eohptr;
  int nelems;
  int ndims;
  Datum *dvalues;
  bool *dnulls;
  Size nbytes;
  int i;

  Assert(eah->ea_magic == EA_MAGIC);

                                               
  if (eah->fvalue)
  {
    return ARR_SIZE(eah->fvalue);
  }

                                                    
  if (eah->flat_size)
  {
    return eah->flat_size;
  }

     
                                                                             
                                                                            
                                             
     
  nelems = eah->nelems;
  ndims = eah->ndims;
  Assert(nelems == ArrayGetNItems(ndims, eah->dims));
  dvalues = eah->dvalues;
  dnulls = eah->dnulls;
  nbytes = 0;
  for (i = 0; i < nelems; i++)
  {
    if (dnulls && dnulls[i])
    {
      continue;
    }
    nbytes = att_addlength_datum(nbytes, eah->typlen, dvalues[i]);
    nbytes = att_align_nominal(nbytes, eah->typalign);
                                             
    if (!AllocSizeIsValid(nbytes))
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("array size exceeds the maximum allowed (%d)", (int)MaxAllocSize)));
    }
  }

  if (dnulls)
  {
    nbytes += ARR_OVERHEAD_WITHNULLS(ndims, nelems);
  }
  else
  {
    nbytes += ARR_OVERHEAD_NONULLS(ndims);
  }

                           
  eah->flat_size = nbytes;

  return nbytes;
}

   
                                           
   
static void
EA_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size)
{
  ExpandedArrayHeader *eah = (ExpandedArrayHeader *)eohptr;
  ArrayType *aresult = (ArrayType *)result;
  int nelems;
  int ndims;
  int32 dataoffset;

  Assert(eah->ea_magic == EA_MAGIC);

                                               
  if (eah->fvalue)
  {
    Assert(allocated_size == ARR_SIZE(eah->fvalue));
    memcpy(result, eah->fvalue, allocated_size);
    return;
  }

                                                                  
  Assert(allocated_size == eah->flat_size);

                                             
  nelems = eah->nelems;
  ndims = eah->ndims;

  if (eah->dnulls)
  {
    dataoffset = ARR_OVERHEAD_WITHNULLS(ndims, nelems);
  }
  else
  {
    dataoffset = 0;                                
  }

                                                        
  memset(aresult, 0, allocated_size);

  SET_VARSIZE(aresult, allocated_size);
  aresult->ndim = ndims;
  aresult->dataoffset = dataoffset;
  aresult->elemtype = eah->element_type;
  memcpy(ARR_DIMS(aresult), eah->dims, ndims * sizeof(int));
  memcpy(ARR_LBOUND(aresult), eah->lbound, ndims * sizeof(int));

  CopyArrayEls(aresult, eah->dvalues, eah->dnulls, nelems, eah->typlen, eah->typbyval, eah->typalign, false);
}

   
                                  
   

   
                                                                               
   
                                                                         
                                                                            
                                                   
   
ExpandedArrayHeader *
DatumGetExpandedArray(Datum d)
{
                                                                 
  if (VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(d)))
  {
    ExpandedArrayHeader *eah = (ExpandedArrayHeader *)DatumGetEOHP(d);

    Assert(eah->ea_magic == EA_MAGIC);
    return eah;
  }

                                
  d = expand_array(d, CurrentMemoryContext, NULL);
  return (ExpandedArrayHeader *)DatumGetEOHP(d);
}

   
                                                                    
   
ExpandedArrayHeader *
DatumGetExpandedArrayX(Datum d, ArrayMetaState *metacache)
{
                                                                 
  if (VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(d)))
  {
    ExpandedArrayHeader *eah = (ExpandedArrayHeader *)DatumGetEOHP(d);

    Assert(eah->ea_magic == EA_MAGIC);
                                  
    if (metacache)
    {
      metacache->element_type = eah->element_type;
      metacache->typlen = eah->typlen;
      metacache->typbyval = eah->typbyval;
      metacache->typalign = eah->typalign;
    }
    return eah;
  }

                                               
  d = expand_array(d, CurrentMemoryContext, metacache);
  return (ExpandedArrayHeader *)DatumGetEOHP(d);
}

   
                                                                             
                                                     
   
AnyArrayType *
DatumGetAnyArrayP(Datum d)
{
  ExpandedArrayHeader *eah;

     
                                                                      
     
  if (VARATT_IS_EXTERNAL_EXPANDED(DatumGetPointer(d)))
  {
    eah = (ExpandedArrayHeader *)DatumGetEOHP(d);
    Assert(eah->ea_magic == EA_MAGIC);
    return (AnyArrayType *)eah;
  }

                                            
  return (AnyArrayType *)PG_DETOAST_DATUM(d);
}

   
                                                                      
                                 
   
void
deconstruct_expanded_array(ExpandedArrayHeader *eah)
{
  if (eah->dvalues == NULL)
  {
    MemoryContext oldcxt = MemoryContextSwitchTo(eah->hdr.eoh_context);
    Datum *dvalues;
    bool *dnulls;
    int nelems;

    dnulls = NULL;
    deconstruct_array(eah->fvalue, eah->element_type, eah->typlen, eah->typbyval, eah->typalign, &dvalues, ARR_HASNULL(eah->fvalue) ? &dnulls : NULL, &nelems);

       
                                                                        
                                                                          
                                                                        
                                                                           
                     
       
    eah->dvalues = dvalues;
    eah->dnulls = dnulls;
    eah->dvalueslen = eah->nelems = nelems;
    MemoryContextSwitchTo(oldcxt);
  }
}
