                                                                            
   
             
                                   
   
                                                                             
                                                                          
                                                                            
                                                                              
                                                                      
   
                                                   
                                                                        
   
                                                                         
   
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/session.h"
#include "storage/lwlock.h"
#include "storage/shm_toc.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

                                           
#define SESSION_MAGIC 0xabb0fbc9

   
                                                                        
                                                                            
                                                                            
                                                                           
                               
   
#define SESSION_DSA_SIZE 0x30000

   
                                                                
   
#define SESSION_KEY_DSA UINT64CONST(0xFFFFFFFFFFFF0001)
#define SESSION_KEY_RECORD_TYPMOD_REGISTRY UINT64CONST(0xFFFFFFFFFFFF0002)

                                     
Session *CurrentSession = NULL;

   
                                                              
   
void
InitializeSession(void)
{
  CurrentSession = MemoryContextAllocZero(TopMemoryContext, sizeof(Session));
}

   
                                                                               
                                                                
   
                                                                         
                                       
   
                                                                            
              
   
dsm_handle
GetSessionDsmHandle(void)
{
  shm_toc_estimator estimator;
  shm_toc *toc;
  dsm_segment *seg;
  size_t typmod_registry_size;
  size_t size;
  void *dsa_space;
  void *typmod_registry_space;
  dsa_area *dsa;
  MemoryContext old_context;

     
                                                                             
                                                                            
                         
     
  if (CurrentSession->segment != NULL)
  {
    return dsm_segment_handle(CurrentSession->segment);
  }

                                         
  old_context = MemoryContextSwitchTo(TopMemoryContext);
  shm_toc_initialize_estimator(&estimator);

                                                    
  shm_toc_estimate_keys(&estimator, 1);
  shm_toc_estimate_chunk(&estimator, SESSION_DSA_SIZE);

                                                                  
  typmod_registry_size = SharedRecordTypmodRegistryEstimate();
  shm_toc_estimate_keys(&estimator, 1);
  shm_toc_estimate_chunk(&estimator, typmod_registry_size);

                               
  size = shm_toc_estimate(&estimator);
  seg = dsm_create(size, DSM_CREATE_NULL_IF_MAXSEGMENTS);
  if (seg == NULL)
  {
    MemoryContextSwitchTo(old_context);

    return DSM_HANDLE_INVALID;
  }
  toc = shm_toc_create(SESSION_MAGIC, dsm_segment_address(seg), size);

                                    
  dsa_space = shm_toc_allocate(toc, SESSION_DSA_SIZE);
  dsa = dsa_create_in_place(dsa_space, SESSION_DSA_SIZE, LWTRANCHE_SESSION_DSA, seg);
  shm_toc_insert(toc, SESSION_KEY_DSA, dsa_space);

                                                            
  typmod_registry_space = shm_toc_allocate(toc, typmod_registry_size);
  SharedRecordTypmodRegistryInit((SharedRecordTypmodRegistry *)typmod_registry_space, seg, dsa);
  shm_toc_insert(toc, SESSION_KEY_RECORD_TYPMOD_REGISTRY, typmod_registry_space);

     
                                                                             
                                                                             
                                                             
                                                                           
                                                                             
     
  dsm_pin_mapping(seg);
  dsa_pin_mapping(dsa);

                                                           
  CurrentSession->segment = seg;
  CurrentSession->area = dsa;

  MemoryContextSwitchTo(old_context);

  return dsm_segment_handle(seg);
}

   
                                                                      
   
void
AttachSession(dsm_handle handle)
{
  dsm_segment *seg;
  shm_toc *toc;
  void *dsa_space;
  void *typmod_registry_space;
  dsa_area *dsa;
  MemoryContext old_context;

  old_context = MemoryContextSwitchTo(TopMemoryContext);

                                  
  seg = dsm_attach(handle);
  if (seg == NULL)
  {
    elog(ERROR, "could not attach to per-session DSM segment");
  }
  toc = shm_toc_attach(SESSION_MAGIC, dsm_segment_address(seg));

                               
  dsa_space = shm_toc_lookup(toc, SESSION_KEY_DSA, false);
  dsa = dsa_attach_in_place(dsa_space, seg);

                                                    
  CurrentSession->segment = seg;
  CurrentSession->area = dsa;

                                                    
  typmod_registry_space = shm_toc_lookup(toc, SESSION_KEY_RECORD_TYPMOD_REGISTRY, false);
  SharedRecordTypmodRegistryAttach((SharedRecordTypmodRegistry *)typmod_registry_space);

                                                                
  dsm_pin_mapping(seg);
  dsa_pin_mapping(dsa);

  MemoryContextSwitchTo(old_context);
}

   
                                                                             
                                                                               
                                                                             
                                                                             
                 
   
void
DetachSession(void)
{
                          
  dsm_detach(CurrentSession->segment);
  CurrentSession->segment = NULL;
  dsa_detach(CurrentSession->area);
  CurrentSession->area = NULL;
}
