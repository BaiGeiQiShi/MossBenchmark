                                                                            
   
             
                                                      
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "commands/sequence.h"

void
seq_desc(StringInfo buf, XLogReaderState *record)
{
  char *rec = XLogRecGetData(record);
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
  xl_seq_rec *xlrec = (xl_seq_rec *)rec;

  if (info == XLOG_SEQ_LOG)
  {
    appendStringInfo(buf, "rel %u/%u/%u", xlrec->node.spcNode, xlrec->node.dbNode, xlrec->node.relNode);
  }
}

const char *
seq_identify(uint8 info)
{
  const char *id = NULL;

  switch (info & ~XLR_INFO_MASK)
  {
  case XLOG_SEQ_LOG:
    id = "LOG";
    break;
  }

  return id;
}
