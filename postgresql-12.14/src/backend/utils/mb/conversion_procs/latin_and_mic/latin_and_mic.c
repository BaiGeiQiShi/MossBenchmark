                                                                            
   
                              
   
                                                                         
                                                                        
   
                  
                                                                         
   
                                                                            
   

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(latin1_to_mic);
PG_FUNCTION_INFO_V1(mic_to_latin1);
PG_FUNCTION_INFO_V1(latin3_to_mic);
PG_FUNCTION_INFO_V1(mic_to_latin3);
PG_FUNCTION_INFO_V1(latin4_to_mic);
PG_FUNCTION_INFO_V1(mic_to_latin4);

              
              
                                   
                                        
                                                         
                                                              
                                     
                   
              
   

Datum
latin1_to_mic(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_LATIN1, PG_MULE_INTERNAL);

  latin2mic(src, dest, len, LC_ISO8859_1, PG_LATIN1);

  PG_RETURN_VOID();
}

Datum
mic_to_latin1(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_MULE_INTERNAL, PG_LATIN1);

  mic2latin(src, dest, len, LC_ISO8859_1, PG_LATIN1);

  PG_RETURN_VOID();
}

Datum
latin3_to_mic(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_LATIN3, PG_MULE_INTERNAL);

  latin2mic(src, dest, len, LC_ISO8859_3, PG_LATIN3);

  PG_RETURN_VOID();
}

Datum
mic_to_latin3(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_MULE_INTERNAL, PG_LATIN3);

  mic2latin(src, dest, len, LC_ISO8859_3, PG_LATIN3);

  PG_RETURN_VOID();
}

Datum
latin4_to_mic(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_LATIN4, PG_MULE_INTERNAL);

  latin2mic(src, dest, len, LC_ISO8859_4, PG_LATIN4);

  PG_RETURN_VOID();
}

Datum
mic_to_latin4(PG_FUNCTION_ARGS)
{
  unsigned char *src = (unsigned char *)PG_GETARG_CSTRING(2);
  unsigned char *dest = (unsigned char *)PG_GETARG_CSTRING(3);
  int len = PG_GETARG_INT32(4);

  CHECK_ENCODING_CONVERSION_ARGS(PG_MULE_INTERNAL, PG_LATIN4);

  mic2latin(src, dest, len, LC_ISO8859_4, PG_LATIN4);

  PG_RETURN_VOID();
}
