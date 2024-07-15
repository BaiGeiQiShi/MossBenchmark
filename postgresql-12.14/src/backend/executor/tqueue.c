                                                                            
   
            
                                                                   
   
                                                                        
                                                                
   
                                                                         
   
                                                                         
                                                                        
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "executor/tqueue.h"

   
                                          
   
                                                                 
   
typedef struct TQueueDestReceiver
{
  DestReceiver pub;                        
  shm_mq_handle *queue;                        
} TQueueDestReceiver;

   
                                              
   
                                                           
   
                                                                     
   
struct TupleQueueReader
{
  shm_mq_handle *queue;                             
};

   
                                                                       
   
                                                                  
   
static bool
tqueueReceiveSlot(TupleTableSlot *slot, DestReceiver *self)
{
  TQueueDestReceiver *tqueue = (TQueueDestReceiver *)self;
  HeapTuple tuple;
  shm_mq_result result;
  bool should_free;

                              
  tuple = ExecFetchSlotHeapTuple(slot, true, &should_free);
  result = shm_mq_send(tqueue->queue, tuple->t_len, tuple->t_data, false);

  if (should_free)
  {
    heap_freetuple(tuple);
  }

                          
  if (result == SHM_MQ_DETACHED)
  {
    return false;
  }
  else if (result != SHM_MQ_SUCCESS)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not send tuple to shared-memory queue")));
  }

  return true;
}

   
                                            
   
static void
tqueueStartupReceiver(DestReceiver *self, int operation, TupleDesc typeinfo)
{
                  
}

   
                                      
   
static void
tqueueShutdownReceiver(DestReceiver *self)
{
  TQueueDestReceiver *tqueue = (TQueueDestReceiver *)self;

  if (tqueue->queue != NULL)
  {
    shm_mq_detach(tqueue->queue);
  }
  tqueue->queue = NULL;
}

   
                                      
   
static void
tqueueDestroyReceiver(DestReceiver *self)
{
  TQueueDestReceiver *tqueue = (TQueueDestReceiver *)self;

                                                                  
  if (tqueue->queue != NULL)
  {
    shm_mq_detach(tqueue->queue);
  }
  pfree(self);
}

   
                                                              
   
DestReceiver *
CreateTupleQueueDestReceiver(shm_mq_handle *handle)
{
  TQueueDestReceiver *self;

  self = (TQueueDestReceiver *)palloc0(sizeof(TQueueDestReceiver));

  self->pub.receiveSlot = tqueueReceiveSlot;
  self->pub.rStartup = tqueueStartupReceiver;
  self->pub.rShutdown = tqueueShutdownReceiver;
  self->pub.rDestroy = tqueueDestroyReceiver;
  self->pub.mydest = DestTupleQueue;
  self->queue = handle;

  return (DestReceiver *)self;
}

   
                                
   
TupleQueueReader *
CreateTupleQueueReader(shm_mq_handle *handle)
{
  TupleQueueReader *reader = palloc0(sizeof(TupleQueueReader));

  reader->queue = handle;

  return reader;
}

   
                                 
   
                                                                           
                                                           
   
void
DestroyTupleQueueReader(TupleQueueReader *reader)
{
  pfree(reader);
}

   
                                            
   
                                                                   
                                                                       
                                                                             
   
                                                                     
                                                                         
                      
   
                                                                         
                                                                          
                                                        
   
HeapTuple
TupleQueueReaderNext(TupleQueueReader *reader, bool nowait, bool *done)
{
  HeapTupleData htup;
  shm_mq_result result;
  Size nbytes;
  void *data;

  if (done != NULL)
  {
    *done = false;
  }

                                  
  result = shm_mq_receive(reader->queue, &nbytes, &data, nowait);

                                                        
  if (result == SHM_MQ_DETACHED)
  {
    if (done != NULL)
    {
      *done = true;
    }
    return NULL;
  }

                                                               
  if (result == SHM_MQ_WOULD_BLOCK)
  {
    return NULL;
  }
  Assert(result == SHM_MQ_SUCCESS);

     
                                                                       
                                                 
     
  ItemPointerSetInvalid(&htup.t_self);
  htup.t_tableOid = InvalidOid;
  htup.t_len = nbytes;
  htup.t_data = data;

  return heap_copytuple(&htup);
}
