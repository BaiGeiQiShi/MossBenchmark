                                                                            
   
               
                                                        
   
                                                                           
                                                                             
                                                                            
                                                                       
                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include "storage/bufmgr.h"
#include "storage/buf_internals.h"

                                       
typedef struct
{
  BufferTag key;                         
  int id;                                  
} BufferLookupEnt;

static HTAB *SharedBufHash;

   
                                               
                                                                      
   
Size
BufTableShmemSize(int size)
{
  return hash_estimate_size(size, sizeof(BufferLookupEnt));
}

   
                                                   
                                                                      
   
void
InitBufTable(int size)
{
  HASHCTL info;

                                       

                                
  info.keysize = sizeof(BufferTag);
  info.entrysize = sizeof(BufferLookupEnt);
  info.num_partitions = NUM_BUFFER_PARTITIONS;

  SharedBufHash = ShmemInitHash("Shared Buffer Lookup Table", size, size, &info, HASH_ELEM | HASH_BLOBS | HASH_PARTITION);
}

   
                    
                                                      
   
                                                                           
                                                                           
                                                                           
                                                              
   
uint32
BufTableHashCode(BufferTag *tagPtr)
{
  return get_hash_value(SharedBufHash, (void *)tagPtr);
}

   
                  
                                                                     
   
                                                                              
   
int
BufTableLookup(BufferTag *tagPtr, uint32 hashcode)
{
  BufferLookupEnt *result;

  result = (BufferLookupEnt *)hash_search_with_hash_value(SharedBufHash, (void *)tagPtr, hashcode, HASH_FIND, NULL);

  if (!result)
  {
    return -1;
  }

  return result->id;
}

   
                  
                                                          
                                                
   
                                                                      
                                                 
   
                                                                         
   
int
BufTableInsert(BufferTag *tagPtr, uint32 hashcode, int buf_id)
{
  BufferLookupEnt *result;
  bool found;

  Assert(buf_id >= 0);                                                    
  Assert(tagPtr->blockNum != P_NEW);                  

  result = (BufferLookupEnt *)hash_search_with_hash_value(SharedBufHash, (void *)tagPtr, hashcode, HASH_ENTER, &found);

  if (found)                                           
  {
    return result->id;
  }

  result->id = buf_id;

  return -1;
}

   
                  
                                                                
   
                                                                         
   
void
BufTableDelete(BufferTag *tagPtr, uint32 hashcode)
{
  BufferLookupEnt *result;

  result = (BufferLookupEnt *)hash_search_with_hash_value(SharedBufHash, (void *)tagPtr, hashcode, HASH_REMOVE, NULL);

  if (!result)                       
  {
    elog(ERROR, "shared buffer hash table corrupted");
  }
}
