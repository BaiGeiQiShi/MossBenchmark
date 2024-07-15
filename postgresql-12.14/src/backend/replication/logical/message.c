                                                                            
   
             
                               
   
                                                                
   
                  
                                               
   
         
   
                                                                              
                                                                             
                     
   
                                                                    
                                                                              
                                                          
                                                                          
                                                                             
                                                                          
                                                   
   
                                                                              
                                                                          
                                                                          
   
                                                                               
   

#include "postgres.h"

#include "miscadmin.h"

#include "access/xact.h"

#include "catalog/indexing.h"

#include "nodes/execnodes.h"

#include "replication/message.h"
#include "replication/logical.h"

#include "utils/memutils.h"

   
                                             
   
XLogRecPtr
LogLogicalMessage(const char *prefix, const char *message, size_t size, bool transactional)
{
  xl_logical_message xlrec;

     
                                                                          
     
  if (transactional)
  {
    Assert(IsTransactionState());
    GetCurrentTransactionId();
  }

  xlrec.dbId = MyDatabaseId;
  xlrec.transactional = transactional;
  xlrec.prefix_size = strlen(prefix) + 1;
  xlrec.message_size = size;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfLogicalMessage);
  XLogRegisterData(unconstify(char *, prefix), xlrec.prefix_size);
  XLogRegisterData(unconstify(char *, message), size);

                              
  XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

  return XLogInsert(RM_LOGICALMSG_ID, XLOG_LOGICAL_MESSAGE);
}

   
                                                              
   
void
logicalmsg_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  if (info != XLOG_LOGICAL_MESSAGE)
  {
    elog(PANIC, "logicalmsg_redo: unknown op code %u", info);
  }

                                                                    
}
