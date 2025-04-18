                                                                            
   
                   
   
                                                                         
                                                                        
   
                  
                                                                       
   
                                                                            
   

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"
#include "../../Unicode/uhc_to_utf8.map"
#include "../../Unicode/utf8_to_uhc.map"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(uhc_to_utf8);
PG_FUNCTION_INFO_V1(utf8_to_uhc);

              
              
                                   
                                        
                                                         
                                                              
                                     
                   
              
   
Datum
uhc_to_utf8(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_UHC, PG_UTF8);

  LocalToUtf(src, len, dest, &uhc_to_unicode_tree, NULL, 0, NULL, PG_UHC);

  PG_RETURN_VOID();
}

Datum
utf8_to_uhc(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_UTF8, PG_UHC);

  UtfToLocal(src, len, dest, &uhc_from_unicode_tree, NULL, 0, NULL, PG_UHC);

  PG_RETURN_VOID();
}
