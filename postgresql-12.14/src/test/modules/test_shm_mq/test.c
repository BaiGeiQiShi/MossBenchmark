                                                                             
   
          
                                                        
   
                                                                
   
                  
                                        
   
                                                                             
   

#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"
#include "pgstat.h"

#include "test_shm_mq.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(test_shm_mq);
PG_FUNCTION_INFO_V1(test_shm_mq_pipelined);

void
_PG_init(void);

static void
verify_message(Size origlen, char *origdata, Size newlen, char *newdata);

   
                                                                  
   
                                                                           
                                                                               
                                                                                
                                                                       
   
Datum
test_shm_mq(PG_FUNCTION_ARGS)
{
  int64 queue_size = PG_GETARG_INT64(0);
  text *message = PG_GETARG_TEXT_PP(1);
  char *message_contents = VARDATA_ANY(message);
  int message_size = VARSIZE_ANY_EXHDR(message);
  int32 loop_count = PG_GETARG_INT32(2);
  int32 nworkers = PG_GETARG_INT32(3);
  dsm_segment *seg;
  shm_mq_handle *outqh;
  shm_mq_handle *inqh;
  shm_mq_result res;
  Size len;
  void *data;

                                            
  if (loop_count < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("repeat count size must be an integer value greater than or equal to zero")));
  }

     
                                                                         
                                                                            
                                                     
     
  if (nworkers <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("number of workers must be an integer value greater than zero")));
  }

                                                                    
  test_shm_mq_setup(queue_size, nworkers, &seg, &outqh, &inqh);

                                 
  res = shm_mq_send(outqh, message_size, message_contents, false);
  if (res != SHM_MQ_SUCCESS)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not send message")));
  }

     
                                                                        
                                    
     
  for (;;)
  {
                            
    res = shm_mq_receive(inqh, &len, &data, false);
    if (res != SHM_MQ_SUCCESS)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not receive message")));
    }

                                                                  
    if (--loop_count <= 0)
    {
      break;
    }

                           
    res = shm_mq_send(outqh, len, data, false);
    if (res != SHM_MQ_SUCCESS)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not send message")));
    }
  }

     
                                                                    
                                        
     
  verify_message(message_size, message_contents, len, data);

                 
  dsm_detach(seg);

  PG_RETURN_VOID();
}

   
                                                                     
   
                                                                            
                                                                            
                                                                             
                                                                              
                                                                            
                                            
   
Datum
test_shm_mq_pipelined(PG_FUNCTION_ARGS)
{
  int64 queue_size = PG_GETARG_INT64(0);
  text *message = PG_GETARG_TEXT_PP(1);
  char *message_contents = VARDATA_ANY(message);
  int message_size = VARSIZE_ANY_EXHDR(message);
  int32 loop_count = PG_GETARG_INT32(2);
  int32 nworkers = PG_GETARG_INT32(3);
  bool verify = PG_GETARG_BOOL(4);
  int32 send_count = 0;
  int32 receive_count = 0;
  dsm_segment *seg;
  shm_mq_handle *outqh;
  shm_mq_handle *inqh;
  shm_mq_result res;
  Size len;
  void *data;

                                            
  if (loop_count < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("repeat count size must be an integer value greater than or equal to zero")));
  }

     
                                                                           
                                                             
     
  if (nworkers < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("number of workers must be an integer value greater than or equal to zero")));
  }

                                                                    
  test_shm_mq_setup(queue_size, nworkers, &seg, &outqh, &inqh);

                  
  for (;;)
  {
    bool wait = true;

       
                                                                         
                                                                       
                                                                        
                                                                        
                                                  
       
    if (send_count < loop_count)
    {
      res = shm_mq_send(outqh, message_size, message_contents, true);
      if (res == SHM_MQ_SUCCESS)
      {
        ++send_count;
        wait = false;
      }
      else if (res == SHM_MQ_DETACHED)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not send message")));
      }
    }

       
                                                                      
                                           
       
    if (receive_count < loop_count)
    {
      res = shm_mq_receive(inqh, &len, &data, true);
      if (res == SHM_MQ_SUCCESS)
      {
        ++receive_count;
                                                             
        if (verify)
        {
          verify_message(message_size, message_contents, len, data);
        }
        wait = false;
      }
      else if (res == SHM_MQ_DETACHED)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not receive message")));
      }
    }
    else
    {
         
                                                                   
                                                                  
         
      if (send_count != receive_count)
      {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("message sent %d times, but received %d times", send_count, receive_count)));
      }
      break;
    }

    if (wait)
    {
         
                                                                        
                                                                       
                                                                       
                       
         
      (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, 0, PG_WAIT_EXTENSION);
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }
  }

                 
  dsm_detach(seg);

  PG_RETURN_VOID();
}

   
                                          
   
static void
verify_message(Size origlen, char *origdata, Size newlen, char *newdata)
{
  Size i;

  if (origlen != newlen)
  {
    ereport(ERROR, (errmsg("message corrupted"), errdetail("The original message was %zu bytes but the final message is %zu bytes.", origlen, newlen)));
  }

  for (i = 0; i < origlen; ++i)
  {
    if (origdata[i] != newdata[i])
    {
      ereport(ERROR, (errmsg("message corrupted"), errdetail("The new and original messages differ at byte %zu of %zu.", i, origlen)));
    }
  }
}
