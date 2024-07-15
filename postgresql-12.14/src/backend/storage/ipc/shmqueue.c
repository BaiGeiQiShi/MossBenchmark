                                                                            
   
              
                                
   
                                                                         
                                                                        
   
   
                  
                                        
   
         
   
                                                              
                                                                   
                                                             
                                                                   
                                                                   
                                            
   
                                                                      
   
                                                                            
   
#include "postgres.h"

#include "storage/shmem.h"

   
                                                        
              
   
void
SHMQueueInit(SHM_QUEUE *queue)
{
  Assert(ShmemAddrIsValid(queue));
  queue->prev = queue->next = queue;
}

   
                                                          
                
   
bool
SHMQueueIsDetached(const SHM_QUEUE *queue)
{
  Assert(ShmemAddrIsValid(queue));
  return (queue->prev == NULL);
}

   
                                                
   
void
SHMQueueElemInit(SHM_QUEUE *queue)
{
  Assert(ShmemAddrIsValid(queue));
  queue->prev = queue->next = NULL;
}

   
                                                          
                    
   
void
SHMQueueDelete(SHM_QUEUE *queue)
{
  SHM_QUEUE *nextElem = queue->next;
  SHM_QUEUE *prevElem = queue->prev;

  Assert(ShmemAddrIsValid(queue));
  Assert(ShmemAddrIsValid(nextElem));
  Assert(ShmemAddrIsValid(prevElem));

  prevElem->next = queue->next;
  nextElem->prev = queue->prev;

  queue->prev = queue->next = NULL;
}

   
                                                                    
                                                              
                              
   
void
SHMQueueInsertBefore(SHM_QUEUE *queue, SHM_QUEUE *elem)
{
  SHM_QUEUE *prevPtr = queue->prev;

  Assert(ShmemAddrIsValid(queue));
  Assert(ShmemAddrIsValid(elem));

  elem->next = prevPtr->next;
  elem->prev = queue->prev;
  queue->prev = elem;
  prevPtr->next = elem;
}

   
                                                                  
                                                             
                              
   
void
SHMQueueInsertAfter(SHM_QUEUE *queue, SHM_QUEUE *elem)
{
  SHM_QUEUE *nextPtr = queue->next;

  Assert(ShmemAddrIsValid(queue));
  Assert(ShmemAddrIsValid(elem));

  elem->prev = nextPtr->prev;
  elem->next = queue->next;
  queue->next = elem;
  nextPtr->prev = elem;
}

                       
                                                     
   
                                                                          
                                     
   
                                                             
                                                          
                                                                
                
            
                  
                    
               
                                                                          
                                                                             
   
                                                       
                                                         
      
                                                             
                       
   
Pointer
SHMQueueNext(const SHM_QUEUE *queue, const SHM_QUEUE *curElem, Size linkOffset)
{
  SHM_QUEUE *elemPtr = curElem->next;

  Assert(ShmemAddrIsValid(curElem));

  if (elemPtr == queue)                              
  {
    return NULL;
  }

  return (Pointer)(((char *)elemPtr) - linkOffset);
}

                       
                                                         
   
                                                                        
                                         
   
Pointer
SHMQueuePrev(const SHM_QUEUE *queue, const SHM_QUEUE *curElem, Size linkOffset)
{
  SHM_QUEUE *elemPtr = curElem->prev;

  Assert(ShmemAddrIsValid(curElem));

  if (elemPtr == queue)                              
  {
    return NULL;
  }

  return (Pointer)(((char *)elemPtr) - linkOffset);
}

   
                                                                        
   
bool
SHMQueueEmpty(const SHM_QUEUE *queue)
{
  Assert(ShmemAddrIsValid(queue));

  if (queue->prev == queue)
  {
    Assert(queue->next == queue);
    return true;
  }
  return false;
}
