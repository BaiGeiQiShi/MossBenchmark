                                                                            
   
                     
                                                                     
   
                                                                         
                                                                        
   
   
                  
                                
   
                                                                            
   
#include "c.h"

#include "port/pg_crc32c.h"

#include <arm_acle.h>

pg_crc32c
pg_comp_crc32c_armv8(pg_crc32c crc, const void *data, size_t len)
{
  const unsigned char *p = data;
  const unsigned char *pend = p + len;

     
                                                                   
                                                                        
                                                   
     
  if (!PointerIsAligned(p, uint16) && p + 1 <= pend)
  {
    crc = __crc32cb(crc, *p);
    p += 1;
  }
  if (!PointerIsAligned(p, uint32) && p + 2 <= pend)
  {
    crc = __crc32ch(crc, *(uint16 *)p);
    p += 2;
  }
  if (!PointerIsAligned(p, uint64) && p + 4 <= pend)
  {
    crc = __crc32cw(crc, *(uint32 *)p);
    p += 4;
  }

                                                        
  while (p + 8 <= pend)
  {
    crc = __crc32cd(crc, *(uint64 *)p);
    p += 8;
  }

                                    
  if (p + 4 <= pend)
  {
    crc = __crc32cw(crc, *(uint32 *)p);
    p += 4;
  }
  if (p + 2 <= pend)
  {
    crc = __crc32ch(crc, *(uint16 *)p);
    p += 2;
  }
  if (p < pend)
  {
    crc = __crc32cb(crc, *p);
  }

  return crc;
}
