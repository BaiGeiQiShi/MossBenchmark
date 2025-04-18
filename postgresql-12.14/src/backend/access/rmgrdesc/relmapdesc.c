                                                                            
   
                
                                                          
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   
#include "postgres.h"

#include "utils/relmapper.h"

void
relmap_desc(StringInfo buf, XLogReaderState *record)
{
  char *rec = XLogRecGetData(record);
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  if (info == XLOG_RELMAP_UPDATE)
  {
    xl_relmap_update *xlrec = (xl_relmap_update *)rec;

    appendStringInfo(buf, "database %u tablespace %u size %u", xlrec->dbid, xlrec->tsid, xlrec->nbytes);
  }
}

const char *
relmap_identify(uint8 info)
{
  const char *id = NULL;

  switch (info & ~XLR_INFO_MASK)
  {
  case XLOG_RELMAP_UPDATE:
    id = "UPDATE";
    break;
  }

  return id;
}
