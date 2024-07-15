                                                                            
   
                     
                                                                
   
                                                                         
                                                                        
   
   
                  
                                
   
                                                                            
   
#include "c.h"

#include "port/pg_crc32c.h"

#include <nmmintrin.h>

pg_attribute_no_sanitize_alignment() pg_crc32c pg_comp_crc32c_sse42(pg_crc32c crc, const void *data, size_t len)
{
  const unsigned char *p = data;
  const unsigned char *pend = p + len;

     
                                            
     
                                                                            
                                                                            
                        
     
#ifdef __x86_64__
  while (p + 8 <= pend)
  {
    crc = (uint32)_mm_crc32_u64(crc, *((const uint64 *)p));
    p += 8;
  }

                                                
  if (p + 4 <= pend)
  {
    crc = _mm_crc32_u32(crc, *((const unsigned int *)p));
    p += 4;
  }
#else

     
                                                                      
                                                
     
  while (p + 4 <= pend)
  {
    crc = _mm_crc32_u32(crc, *((const unsigned int *)p));
    p += 4;
  }
#endif                 

                                                  
  while (p < pend)
  {
    crc = _mm_crc32_u8(crc, *p);
    p++;
  }

  return crc;
}
