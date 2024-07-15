                                                                            
   
              
                                               
   
                                                                         
                                                                        
   
   
                  
                                        
   
         
                                                                       
                                                                            
   
                                                                       
                            
   
                                                                         
                                                                        
                                                                           
                                                                  
                                                                            
   

#include "postgres.h"

#include "access/hash.h"
#include "catalog/pg_collation.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/hashutils.h"
#include "utils/pg_locale.h"

   
                                     
   
                                                   
   
                                                                     
                                                                     
                                            
   

                                                              
Datum
hashchar(PG_FUNCTION_ARGS)
{
  return hash_uint32((int32)PG_GETARG_CHAR(0));
}

Datum
hashcharextended(PG_FUNCTION_ARGS)
{
  return hash_uint32_extended((int32)PG_GETARG_CHAR(0), PG_GETARG_INT64(1));
}

Datum
hashint2(PG_FUNCTION_ARGS)
{
  return hash_uint32((int32)PG_GETARG_INT16(0));
}

Datum
hashint2extended(PG_FUNCTION_ARGS)
{
  return hash_uint32_extended((int32)PG_GETARG_INT16(0), PG_GETARG_INT64(1));
}

Datum
hashint4(PG_FUNCTION_ARGS)
{
  return hash_uint32(PG_GETARG_INT32(0));
}

Datum
hashint4extended(PG_FUNCTION_ARGS)
{
  return hash_uint32_extended(PG_GETARG_INT32(0), PG_GETARG_INT64(1));
}

Datum
hashint8(PG_FUNCTION_ARGS)
{
     
                                                                         
                                                                           
                                                                          
                                                                            
                                                                            
                           
     
  int64 val = PG_GETARG_INT64(0);
  uint32 lohalf = (uint32)val;
  uint32 hihalf = (uint32)(val >> 32);

  lohalf ^= (val >= 0) ? hihalf : ~hihalf;

  return hash_uint32(lohalf);
}

Datum
hashint8extended(PG_FUNCTION_ARGS)
{
                                 
  int64 val = PG_GETARG_INT64(0);
  uint32 lohalf = (uint32)val;
  uint32 hihalf = (uint32)(val >> 32);

  lohalf ^= (val >= 0) ? hihalf : ~hihalf;

  return hash_uint32_extended(lohalf, PG_GETARG_INT64(1));
}

Datum
hashoid(PG_FUNCTION_ARGS)
{
  return hash_uint32((uint32)PG_GETARG_OID(0));
}

Datum
hashoidextended(PG_FUNCTION_ARGS)
{
  return hash_uint32_extended((uint32)PG_GETARG_OID(0), PG_GETARG_INT64(1));
}

Datum
hashenum(PG_FUNCTION_ARGS)
{
  return hash_uint32((uint32)PG_GETARG_OID(0));
}

Datum
hashenumextended(PG_FUNCTION_ARGS)
{
  return hash_uint32_extended((uint32)PG_GETARG_OID(0), PG_GETARG_INT64(1));
}

Datum
hashfloat4(PG_FUNCTION_ARGS)
{
  float4 key = PG_GETARG_FLOAT4(0);
  float8 key8;

     
                                                                             
                                                                          
                                                       
     
  if (key == (float4)0)
  {
    PG_RETURN_UINT32(0);
  }

     
                                                                           
                                                                             
                                                                           
                                                                             
                   
     
  key8 = key;

     
                                                                         
                                                                             
                                                                          
                                                                      
                                                            
     
  if (isnan(key8))
  {
    key8 = get_float8_nan();
  }

  return hash_any((unsigned char *)&key8, sizeof(key8));
}

Datum
hashfloat4extended(PG_FUNCTION_ARGS)
{
  float4 key = PG_GETARG_FLOAT4(0);
  uint64 seed = PG_GETARG_INT64(1);
  float8 key8;

                                   
  if (key == (float4)0)
  {
    PG_RETURN_UINT64(seed);
  }
  key8 = key;
  if (isnan(key8))
  {
    key8 = get_float8_nan();
  }

  return hash_any_extended((unsigned char *)&key8, sizeof(key8), seed);
}

Datum
hashfloat8(PG_FUNCTION_ARGS)
{
  float8 key = PG_GETARG_FLOAT8(0);

     
                                                                             
                                                                          
                                                       
     
  if (key == (float8)0)
  {
    PG_RETURN_UINT32(0);
  }

     
                                                                         
                                                                             
                                            
     
  if (isnan(key))
  {
    key = get_float8_nan();
  }

  return hash_any((unsigned char *)&key, sizeof(key));
}

Datum
hashfloat8extended(PG_FUNCTION_ARGS)
{
  float8 key = PG_GETARG_FLOAT8(0);
  uint64 seed = PG_GETARG_INT64(1);

                                   
  if (key == (float8)0)
  {
    PG_RETURN_UINT64(seed);
  }
  if (isnan(key))
  {
    key = get_float8_nan();
  }

  return hash_any_extended((unsigned char *)&key, sizeof(key), seed);
}

Datum
hashoidvector(PG_FUNCTION_ARGS)
{
  oidvector *key = (oidvector *)PG_GETARG_POINTER(0);

  return hash_any((unsigned char *)key->values, key->dim1 * sizeof(Oid));
}

Datum
hashoidvectorextended(PG_FUNCTION_ARGS)
{
  oidvector *key = (oidvector *)PG_GETARG_POINTER(0);

  return hash_any_extended((unsigned char *)key->values, key->dim1 * sizeof(Oid), PG_GETARG_INT64(1));
}

Datum
hashname(PG_FUNCTION_ARGS)
{
  char *key = NameStr(*PG_GETARG_NAME(0));

  return hash_any((unsigned char *)key, strlen(key));
}

Datum
hashnameextended(PG_FUNCTION_ARGS)
{
  char *key = NameStr(*PG_GETARG_NAME(0));

  return hash_any_extended((unsigned char *)key, strlen(key), PG_GETARG_INT64(1));
}

Datum
hashtext(PG_FUNCTION_ARGS)
{
  text *key = PG_GETARG_TEXT_PP(0);
  Oid collid = PG_GET_COLLATION();
  pg_locale_t mylocale = 0;
  Datum result;

  if (!collid)
  {
    ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for string hashing"), errhint("Use the COLLATE clause to set the collation explicitly.")));
  }

  if (!lc_collate_is_c(collid) && collid != DEFAULT_COLLATION_OID)
  {
    mylocale = pg_newlocale_from_collation(collid);
  }

  if (!mylocale || mylocale->deterministic)
  {
    result = hash_any((unsigned char *)VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));
  }
  else
  {
#ifdef USE_ICU
    if (mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t ulen = -1;
      UChar *uchar = NULL;
      Size bsize;
      uint8_t *buf;

      ulen = icu_to_uchar(&uchar, VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));

      bsize = ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, NULL, 0);
      buf = palloc(bsize);
      ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, buf, bsize);
      pfree(uchar);

      result = hash_any(buf, bsize);

      pfree(buf);
    }
    else
#endif
                            
      elog(ERROR, "unsupported collprovider: %c", mylocale->provider);
  }

                                               
  PG_FREE_IF_COPY(key, 0);

  return result;
}

Datum
hashtextextended(PG_FUNCTION_ARGS)
{
  text *key = PG_GETARG_TEXT_PP(0);
  Oid collid = PG_GET_COLLATION();
  pg_locale_t mylocale = 0;
  Datum result;

  if (!collid)
  {
    ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for string hashing"), errhint("Use the COLLATE clause to set the collation explicitly.")));
  }

  if (!lc_collate_is_c(collid) && collid != DEFAULT_COLLATION_OID)
  {
    mylocale = pg_newlocale_from_collation(collid);
  }

  if (!mylocale || mylocale->deterministic)
  {
    result = hash_any_extended((unsigned char *)VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key), PG_GETARG_INT64(1));
  }
  else
  {
#ifdef USE_ICU
    if (mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t ulen = -1;
      UChar *uchar = NULL;
      Size bsize;
      uint8_t *buf;

      ulen = icu_to_uchar(&uchar, VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));

      bsize = ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, NULL, 0);
      buf = palloc(bsize);
      ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, buf, bsize);
      pfree(uchar);

      result = hash_any_extended(buf, bsize, PG_GETARG_INT64(1));

      pfree(buf);
    }
    else
#endif
                            
      elog(ERROR, "unsupported collprovider: %c", mylocale->provider);
  }

  PG_FREE_IF_COPY(key, 0);

  return result;
}

   
                                                                         
                                                                             
   
Datum
hashvarlena(PG_FUNCTION_ARGS)
{
  struct varlena *key = PG_GETARG_VARLENA_PP(0);
  Datum result;

  result = hash_any((unsigned char *)VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));

                                               
  PG_FREE_IF_COPY(key, 0);

  return result;
}

Datum
hashvarlenaextended(PG_FUNCTION_ARGS)
{
  struct varlena *key = PG_GETARG_VARLENA_PP(0);
  Datum result;

  result = hash_any_extended((unsigned char *)VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key), PG_GETARG_INT64(1));

  PG_FREE_IF_COPY(key, 0);

  return result;
}
