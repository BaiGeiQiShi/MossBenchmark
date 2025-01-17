                                                                            
   
                    
                                                               
   
                                                                         
   
   
                  
                                                  
   
                                                                            
   
#include "postgres.h"

#include "replication/origin.h"

void
replorigin_desc(StringInfo buf, XLogReaderState *record)
{
  char *rec = XLogRecGetData(record);
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  switch (info)
  {
  case XLOG_REPLORIGIN_SET:
  {
    xl_replorigin_set *xlrec;

    xlrec = (xl_replorigin_set *)rec;

    appendStringInfo(buf, "set %u; lsn %X/%X; force: %d", xlrec->node_id, (uint32)(xlrec->remote_lsn >> 32), (uint32)xlrec->remote_lsn, xlrec->force);
    break;
  }
  case XLOG_REPLORIGIN_DROP:
  {
    xl_replorigin_drop *xlrec;

    xlrec = (xl_replorigin_drop *)rec;

    appendStringInfo(buf, "drop %u", xlrec->node_id);
    break;
  }
  }
}

const char *
replorigin_identify(uint8 info)
{
  switch (info)
  {
  case XLOG_REPLORIGIN_SET:
    return "SET";
  case XLOG_REPLORIGIN_DROP:
    return "DROP";
  default:
    return NULL;
  }
}
