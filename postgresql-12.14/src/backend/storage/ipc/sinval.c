                                                                            
   
            
                                                            
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/xact.h"
#include "commands/async.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/sinvaladt.h"
#include "utils/inval.h"

uint64 SharedInvalidMessageCounter;

   
                                                                       
                                                                        
                                                                     
                                                            
                                                                      
   
                                                                          
                                                                       
                                                                      
                          
   
volatile sig_atomic_t catchupInterruptPending = false;

   
                             
                                                                            
   
void
SendSharedInvalidMessages(const SharedInvalidationMessage *msgs, int n)
{
  SIInsertDataEntries(msgs, n);
}

   
                                
                                                                        
   
                                                                        
                                                                           
                                                      
   
                                                                            
                                                                             
                                                                          
                                                                           
                                                                             
                                                                            
                              
   
void
ReceiveSharedInvalidMessages(void (*invalFunction)(SharedInvalidationMessage *msg), void (*resetFunction)(void))
{
#define MAXINVALMSGS 32
  static SharedInvalidationMessage messages[MAXINVALMSGS];

     
                                                                             
                                    
     
  static volatile int nextmsg = 0;
  static volatile int nummsgs = 0;

                                                                    
  while (nextmsg < nummsgs)
  {
    SharedInvalidationMessage msg = messages[nextmsg++];

    SharedInvalidMessageCounter++;
    invalFunction(&msg);
  }

  do
  {
    int getResult;

    nextmsg = nummsgs = 0;

                                       
    getResult = SIGetDataEntries(messages, MAXINVALMSGS);

    if (getResult < 0)
    {
                               
      elog(DEBUG4, "cache state reset");
      SharedInvalidMessageCounter++;
      resetFunction();
      break;                         
    }

                                                                       
    nextmsg = 0;
    nummsgs = getResult;

    while (nextmsg < nummsgs)
    {
      SharedInvalidationMessage msg = messages[nextmsg++];

      SharedInvalidMessageCounter++;
      invalFunction(&msg);
    }

       
                                                                           
                                                                  
       
  } while (nummsgs == MAXINVALMSGS);

     
                                                                        
                                                                           
                                                                      
                                                                       
                                                                            
                                                       
     
  if (catchupInterruptPending)
  {
    catchupInterruptPending = false;
    elog(DEBUG4, "sinval catchup complete, cleaning queue");
    SICleanupQueue(false, 0);
  }
}

   
                          
   
                                                              
   
                                                                               
                                                                            
                                
   
void
HandleCatchupInterrupt(void)
{
     
                                                                          
                  
     

  catchupInterruptPending = true;

                                                      
  SetLatch(MyLatch);
}

   
                           
   
                                                                             
                                                                       
   
void
ProcessCatchupInterrupt(void)
{
  while (catchupInterruptPending)
  {
       
                                                                          
                                                                
                                                                        
                                                                     
                                                                       
                                                                           
       
                                                                        
                                                                          
                                                                      
                                                                        
       
    if (IsTransactionOrTransactionBlock())
    {
      elog(DEBUG4, "ProcessCatchupEvent inside transaction");
      AcceptInvalidationMessages();
    }
    else
    {
      elog(DEBUG4, "ProcessCatchupEvent outside transaction");
      StartTransactionCommand();
      CommitTransactionCommand();
    }
  }
}
