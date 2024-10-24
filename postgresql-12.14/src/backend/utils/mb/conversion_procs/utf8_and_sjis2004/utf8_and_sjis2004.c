                                                                            
   
                              
   
                                                                         
                                                                        
   
                  
                                                                                 
   
                                                                            
   

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"
#include "../../Unicode/shift_jis_2004_to_utf8.map"
#include "../../Unicode/utf8_to_shift_jis_2004.map"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(shift_jis_2004_to_utf8);
PG_FUNCTION_INFO_V1(utf8_to_shift_jis_2004);

              
              
                                   
                                        
                                                         
                                                              
                                     
                   
              
   
Datum
shift_jis_2004_to_utf8(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_SHIFT_JIS_2004, PG_UTF8);

  LocalToUtf(src, len, dest, &shift_jis_2004_to_unicode_tree, LUmapSHIFT_JIS_2004_combined, lengthof(LUmapSHIFT_JIS_2004_combined), NULL, PG_SHIFT_JIS_2004);

  PG_RETURN_VOID();
}

Datum
utf8_to_shift_jis_2004(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_UTF8, PG_SHIFT_JIS_2004);

  UtfToLocal(src, len, dest, &shift_jis_2004_from_unicode_tree, ULmapSHIFT_JIS_2004_combined, lengthof(ULmapSHIFT_JIS_2004_combined), NULL, PG_SHIFT_JIS_2004);

  PG_RETURN_VOID();
}
