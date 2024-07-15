                                                                            
   
                 
                                                                
   
   
                                                                         
                                                                        
   
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/generic_xlog.h"
#include "lib/stringinfo.h"
#include "storage/relfilenode.h"

   
                                                                           
              
   
void
generic_desc(StringInfo buf, XLogReaderState *record)
{
  Pointer ptr = XLogRecGetData(record), end = ptr + XLogRecGetDataLen(record);

  while (ptr < end)
  {
    OffsetNumber offset, length;

    memcpy(&offset, ptr, sizeof(offset));
    ptr += sizeof(offset);
    memcpy(&length, ptr, sizeof(length));
    ptr += sizeof(length);
    ptr += length;

    if (ptr < end)
    {
      appendStringInfo(buf, "offset %u, length %u; ", offset, length);
    }
    else
    {
      appendStringInfo(buf, "offset %u, length %u", offset, length);
    }
  }

  return;
}

   
                                                                            
                                
   
const char *
generic_identify(uint8 info)
{
  return "Generic";
}
