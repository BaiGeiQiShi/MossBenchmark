                                                                            
   
                   
                                                             
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "utils/expandeddatum.h"
#include "utils/memutils.h"

   
                
   
                                                                            
   
                                                                        
                                          
   
ExpandedObjectHeader *
DatumGetEOHP(Datum d)
{
  varattrib_1b_e *datum = (varattrib_1b_e *)DatumGetPointer(d);
  varatt_expanded ptr;

  Assert(VARATT_IS_EXTERNAL_EXPANDED(datum));
  memcpy(&ptr, VARDATA_EXTERNAL(datum), sizeof(ptr));
  Assert(VARATT_IS_EXPANDED_HEADER(ptr.eohptr));
  return ptr.eohptr;
}

   
                   
   
                                                       
   
                                                                        
   
void
EOH_init_header(ExpandedObjectHeader *eohptr, const ExpandedObjectMethods *methods, MemoryContext obj_context)
{
  varatt_expanded ptr;

  eohptr->vl_len_ = EOH_HEADER_MAGIC;
  eohptr->eoh_methods = methods;
  eohptr->eoh_context = obj_context;

  ptr.eohptr = eohptr;

  SET_VARTAG_EXTERNAL(eohptr->eoh_rw_ptr, VARTAG_EXPANDED_RW);
  memcpy(VARDATA_EXTERNAL(eohptr->eoh_rw_ptr), &ptr, sizeof(ptr));

  SET_VARTAG_EXTERNAL(eohptr->eoh_ro_ptr, VARTAG_EXPANDED_RO);
  memcpy(VARDATA_EXTERNAL(eohptr->eoh_ro_ptr), &ptr, sizeof(ptr));
}

   
                     
                    
   
                                                                           
   

Size
EOH_get_flat_size(ExpandedObjectHeader *eohptr)
{
  return eohptr->eoh_methods->get_flat_size(eohptr);
}

void
EOH_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size)
{
  eohptr->eoh_methods->flatten_into(eohptr, result, allocated_size);
}

   
                                                                    
                                        
   
                                                                             
                                                                        
   
Datum
MakeExpandedObjectReadOnlyInternal(Datum d)
{
  ExpandedObjectHeader *eohptr;

                                                                 
  if (!VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(d)))
  {
    return d;
  }

                                              
  eohptr = DatumGetEOHP(d);

                                                                      
  return EOHPGetRODatum(eohptr);
}

   
                                                                            
                                                                         
                                                                        
                                                                         
                                                                           
   
Datum
TransferExpandedObject(Datum d, MemoryContext new_parent)
{
  ExpandedObjectHeader *eohptr = DatumGetEOHP(d);

                                        
  Assert(VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(d)));

                          
  MemoryContextSetParent(eohptr->eoh_context, new_parent);

                                                       
  return EOHPGetRWDatum(eohptr);
}

   
                                                                    
   
void
DeleteExpandedObject(Datum d)
{
  ExpandedObjectHeader *eohptr = DatumGetEOHP(d);

                                        
  Assert(VARATT_IS_EXTERNAL_EXPANDED_RW(DatumGetPointer(d)));

               
  MemoryContextDelete(eohptr->eoh_context);
}
