                                                                            
   
                     
   
                                                                         
                                                                        
   
                  
                                                                           
   
                                                                            
   

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(ascii_to_utf8);
PG_FUNCTION_INFO_V1(utf8_to_ascii);

              
              
                                   
                                        
                                                         
                                                              
                                     
                   
              
   

Datum
ascii_to_utf8(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_SQL_ASCII, PG_UTF8);

                                                                         
  pg_ascii2mic(src, dest, len);

  PG_RETURN_VOID();
}

Datum
utf8_to_ascii(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_UTF8, PG_SQL_ASCII);

                                                                         
  pg_mic2ascii(src, dest, len);

  PG_RETURN_VOID();
}
