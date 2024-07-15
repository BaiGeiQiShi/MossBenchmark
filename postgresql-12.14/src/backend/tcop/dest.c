                                                                            
   
          
                                            
   
   
                                                                         
                                                                        
   
                  
                             
   
                                                                            
   
   
                       
                                                                  
                                                                      
                                                            
                                                                      
                                                                
   
          
                                                            
                                                               
                                        
   

#include "postgres.h"

#include "access/printsimple.h"
#include "access/printtup.h"
#include "access/xact.h"
#include "commands/copy.h"
#include "commands/createas.h"
#include "commands/matview.h"
#include "executor/functions.h"
#include "executor/tqueue.h"
#include "executor/tstoreReceiver.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "utils/portal.h"

                    
                                 
                    
   
static bool
donothingReceive(TupleTableSlot *slot, DestReceiver *self)
{
  return true;
}

static void
donothingStartup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
}

static void
donothingCleanup(DestReceiver *self)
{
                                                          
}

                    
                                                                      
                    
   
static const DestReceiver donothingDR = {donothingReceive, donothingStartup, donothingCleanup, donothingCleanup, DestNone};

static const DestReceiver debugtupDR = {debugtup, debugStartup, donothingCleanup, donothingCleanup, DestDebug};

static const DestReceiver printsimpleDR = {printsimple, printsimple_startup, donothingCleanup, donothingCleanup, DestRemoteSimple};

static const DestReceiver spi_printtupDR = {spi_printtup, spi_dest_startup, donothingCleanup, donothingCleanup, DestSPI};

   
                                             
   
                                                                               
                                                         
   
DestReceiver *None_Receiver = (DestReceiver *)&donothingDR;

                    
                                                                  
                    
   
void
BeginCommand(const char *commandTag, CommandDest dest)
{
                                
}

                    
                                                                           
                    
   
DestReceiver *
CreateDestReceiver(CommandDest dest)
{
     
                                                                        
                                                                    
     

  switch (dest)
  {
  case DestRemote:
  case DestRemoteExecute:
    return printtup_create_DR(dest);

  case DestRemoteSimple:
    return unconstify(DestReceiver *, &printsimpleDR);

  case DestNone:
    return unconstify(DestReceiver *, &donothingDR);

  case DestDebug:
    return unconstify(DestReceiver *, &debugtupDR);

  case DestSPI:
    return unconstify(DestReceiver *, &spi_printtupDR);

  case DestTuplestore:
    return CreateTuplestoreDestReceiver();

  case DestIntoRel:
    return CreateIntoRelDestReceiver(NULL);

  case DestCopyOut:
    return CreateCopyDestReceiver();

  case DestSQLFunction:
    return CreateSQLFunctionDestReceiver();

  case DestTransientRel:
    return CreateTransientRelDestReceiver(InvalidOid);

  case DestTupleQueue:
    return CreateTupleQueueDestReceiver(NULL);
  }

                             
  pg_unreachable();
}

                    
                                                            
                    
   
void
EndCommand(const char *commandTag, CommandDest dest)
{
  switch (dest)
  {
  case DestRemote:
  case DestRemoteExecute:
  case DestRemoteSimple:

       
                                                                      
                               
       
    pq_putmessage('C', commandTag, strlen(commandTag) + 1);
    break;

  case DestNone:
  case DestDebug:
  case DestSPI:
  case DestTuplestore:
  case DestIntoRel:
  case DestCopyOut:
  case DestSQLFunction:
  case DestTransientRel:
  case DestTupleQueue:
    break;
  }
}

                    
                                                                      
   
                                                                     
                                                                 
                                                                    
                                                                       
                                                                       
                                                                      
                                           
                    
   
void
NullCommand(CommandDest dest)
{
  switch (dest)
  {
  case DestRemote:
  case DestRemoteExecute:
  case DestRemoteSimple:

       
                                                                    
                                                                
       
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
    {
      pq_putemptymessage('I');
    }
    else
    {
      pq_putmessage('I', "", 1);
    }
    break;

  case DestNone:
  case DestDebug:
  case DestSPI:
  case DestTuplestore:
  case DestIntoRel:
  case DestCopyOut:
  case DestSQLFunction:
  case DestTransientRel:
  case DestTupleQueue:
    break;
  }
}

                    
                                                                
   
                                                                   
                                           
                                                                           
   
                                                                       
                                                                           
                    
   
void
ReadyForQuery(CommandDest dest)
{
  switch (dest)
  {
  case DestRemote:
  case DestRemoteExecute:
  case DestRemoteSimple:
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
    {
      StringInfoData buf;

      pq_beginmessage(&buf, 'Z');
      pq_sendbyte(&buf, TransactionBlockStatusCode());
      pq_endmessage(&buf);
    }
    else
    {
      pq_putemptymessage('Z');
    }
                                                   
    pq_flush();
    break;

  case DestNone:
  case DestDebug:
  case DestSPI:
  case DestTuplestore:
  case DestIntoRel:
  case DestCopyOut:
  case DestSQLFunction:
  case DestTransientRel:
  case DestTupleQueue:
    break;
  }
}
