                                                                            
   
                 
                                  
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "utils/syscache.h"
#include "mb/pg_wchar.h"

static char *
printTypmod(const char *typname, int32 typmod, Oid typmodout);

   
                                                
   
                                                     
                                                                    
                                                                      
                                                                 
                                                                       
   
                                                                            
                                                                         
                                                                           
                                                                          
                                                                        
                                                                        
                                                                     
                                                                      
                                                                           
                                                                          
                                                                      
                                            
   
                                                                    
                                                                        
                                       
   
Datum
format_type(PG_FUNCTION_ARGS)
{
  Oid type_oid;
  int32 typemod;
  char *result;
  bits16 flags = FORMAT_TYPE_ALLOW_INVALID;

                                                                     
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  type_oid = PG_GETARG_OID(0);

  if (PG_ARGISNULL(1))
  {
    typemod = -1;
  }
  else
  {
    typemod = PG_GETARG_INT32(1);
    flags |= FORMAT_TYPE_TYPEMOD_GIVEN;
  }

  result = format_type_extended(type_oid, typemod, flags);

  PG_RETURN_TEXT_P(cstring_to_text(result));
}

   
                        
                                             
   
                                                                            
                                                                             
                      
   
                                                      
                               
                                                                        
                               
                                                                       
                
                               
                                                                 
   
                                                                        
                                             
   
                              
   
char *
format_type_extended(Oid type_oid, int32 typemod, bits16 flags)
{
  HeapTuple tuple;
  Form_pg_type typeform;
  Oid array_base_type;
  bool is_array;
  char *buf;
  bool with_typemod;

  if (type_oid == InvalidOid && (flags & FORMAT_TYPE_ALLOW_INVALID) != 0)
  {
    return pstrdup("-");
  }

  tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type_oid));
  if (!HeapTupleIsValid(tuple))
  {
    if ((flags & FORMAT_TYPE_ALLOW_INVALID) != 0)
    {
      return pstrdup("???");
    }
    else
    {
      elog(ERROR, "cache lookup failed for type %u", type_oid);
    }
  }
  typeform = (Form_pg_type)GETSTRUCT(tuple);

     
                                                                         
                                                                             
                                                                             
                                                                          
                                      
     
  array_base_type = typeform->typelem;

  if (array_base_type != InvalidOid && typeform->typstorage != 'p')
  {
                                                        
    ReleaseSysCache(tuple);
    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(array_base_type));
    if (!HeapTupleIsValid(tuple))
    {
      if ((flags & FORMAT_TYPE_ALLOW_INVALID) != 0)
      {
        return pstrdup("???[]");
      }
      else
      {
        elog(ERROR, "cache lookup failed for type %u", type_oid);
      }
    }
    typeform = (Form_pg_type)GETSTRUCT(tuple);
    type_oid = array_base_type;
    is_array = true;
  }
  else
  {
    is_array = false;
  }

  with_typemod = (flags & FORMAT_TYPE_TYPEMOD_GIVEN) != 0 && (typemod >= 0);

     
                                                                           
                                                                    
                                                                            
                                                    
     
                                                                            
                                                                            
                                                                      
                                                               
     
  buf = NULL;                               

  switch (type_oid)
  {
  case BITOID:
    if (with_typemod)
    {
      buf = printTypmod("bit", typemod, typeform->typmodout);
    }
    else if ((flags & FORMAT_TYPE_TYPEMOD_GIVEN) != 0)
    {
         
                                                                
                                                                   
                                                     
         
    }
    else
    {
      buf = pstrdup("bit");
    }
    break;

  case BOOLOID:
    buf = pstrdup("boolean");
    break;

  case BPCHAROID:
    if (with_typemod)
    {
      buf = printTypmod("character", typemod, typeform->typmodout);
    }
    else if ((flags & FORMAT_TYPE_TYPEMOD_GIVEN) != 0)
    {
         
                                                                   
                                                                  
                                                     
         
    }
    else
    {
      buf = pstrdup("character");
    }
    break;

  case FLOAT4OID:
    buf = pstrdup("real");
    break;

  case FLOAT8OID:
    buf = pstrdup("double precision");
    break;

  case INT2OID:
    buf = pstrdup("smallint");
    break;

  case INT4OID:
    buf = pstrdup("integer");
    break;

  case INT8OID:
    buf = pstrdup("bigint");
    break;

  case NUMERICOID:
    if (with_typemod)
    {
      buf = printTypmod("numeric", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("numeric");
    }
    break;

  case INTERVALOID:
    if (with_typemod)
    {
      buf = printTypmod("interval", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("interval");
    }
    break;

  case TIMEOID:
    if (with_typemod)
    {
      buf = printTypmod("time", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("time without time zone");
    }
    break;

  case TIMETZOID:
    if (with_typemod)
    {
      buf = printTypmod("time", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("time with time zone");
    }
    break;

  case TIMESTAMPOID:
    if (with_typemod)
    {
      buf = printTypmod("timestamp", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("timestamp without time zone");
    }
    break;

  case TIMESTAMPTZOID:
    if (with_typemod)
    {
      buf = printTypmod("timestamp", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("timestamp with time zone");
    }
    break;

  case VARBITOID:
    if (with_typemod)
    {
      buf = printTypmod("bit varying", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("bit varying");
    }
    break;

  case VARCHAROID:
    if (with_typemod)
    {
      buf = printTypmod("character varying", typemod, typeform->typmodout);
    }
    else
    {
      buf = pstrdup("character varying");
    }
    break;
  }

  if (buf == NULL)
  {
       
                                                                       
                                                                         
                                                                          
                                                               
       
    char *nspname;
    char *typname;

    if ((flags & FORMAT_TYPE_FORCE_QUALIFY) == 0 && TypeIsVisible(type_oid))
    {
      nspname = NULL;
    }
    else
    {
      nspname = get_namespace_name_or_temp(typeform->typnamespace);
    }

    typname = NameStr(typeform->typname);

    buf = quote_qualified_identifier(nspname, typname);

    if (with_typemod)
    {
      buf = printTypmod(buf, typemod, typeform->typmodout);
    }
  }

  if (is_array)
  {
    buf = psprintf("%s[]", buf);
  }

  ReleaseSysCache(tuple);

  return buf;
}

   
                                                                      
                                                            
   
                                           
   
char *
format_type_be(Oid type_oid)
{
  return format_type_extended(type_oid, -1, 0);
}

   
                                                                         
                                                                     
   
char *
format_type_be_qualified(Oid type_oid)
{
  return format_type_extended(type_oid, -1, FORMAT_TYPE_FORCE_QUALIFY);
}

   
                                                             
   
char *
format_type_with_typemod(Oid type_oid, int32 typemod)
{
  return format_type_extended(type_oid, typemod, FORMAT_TYPE_TYPEMOD_GIVEN);
}

   
                                                
   
static char *
printTypmod(const char *typname, int32 typmod, Oid typmodout)
{
  char *res;

                                           
  Assert(typmod >= 0);

  if (typmodout == InvalidOid)
  {
                                                                     
    res = psprintf("%s(%d)", typname, (int)typmod);
  }
  else
  {
                                                   
    char *tmstr;

    tmstr = DatumGetCString(OidFunctionCall1(typmodout, Int32GetDatum(typmod)));
    res = psprintf("%s%s", typname, tmstr);
  }

  return res;
}

   
                                                                            
   
                                                                           
                                                                        
                                                                          
                                                       
   
                                                                            
                                                                          
                                                                           
                                                                         
                                                                          
   
int32
type_maximum_size(Oid type_oid, int32 typemod)
{
  if (typemod < 0)
  {
    return -1;
  }

  switch (type_oid)
  {
  case BPCHAROID:
  case VARCHAROID:
                                         

                                            
    return (typemod - VARHDRSZ) * pg_encoding_max_length(GetDatabaseEncoding()) + VARHDRSZ;

  case NUMERICOID:
    return numeric_maximum_size(typemod);

  case VARBITOID:
  case BITOID:
                                             
    return (typemod + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE + 2 * sizeof(int32);
  }

                                                            
  return -1;
}

   
                                                                       
   
Datum
oidvectortypes(PG_FUNCTION_ARGS)
{
  oidvector *oidArray = (oidvector *)PG_GETARG_POINTER(0);
  char *result;
  int numargs = oidArray->dim1;
  int num;
  size_t total;
  size_t left;

  total = 20 * numargs + 1;
  result = palloc(total);
  result[0] = '\0';
  left = total - 1;

  for (num = 0; num < numargs; num++)
  {
    char *typename = format_type_extended(oidArray->values[num], -1, FORMAT_TYPE_ALLOW_INVALID);
    size_t slen = strlen(typename);

    if (left < (slen + 2))
    {
      total += slen + 2;
      result = repalloc(result, total);
      left += slen + 2;
    }

    if (num > 0)
    {
      strcat(result, ", ");
      left -= 2;
    }
    strcat(result, typename);
    left -= slen;
  }

  PG_RETURN_TEXT_P(cstring_to_text(result));
}
