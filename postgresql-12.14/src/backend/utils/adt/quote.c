                                                                            
   
           
                                                    
   
                                                                         
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "utils/builtins.h"

   
                 
                                          
   
Datum
quote_ident(PG_FUNCTION_ARGS)
{
  text *t = PG_GETARG_TEXT_PP(0);
  const char *qstr;
  char *str;

  str = text_to_cstring(t);
  qstr = quote_identifier(str);
  PG_RETURN_TEXT_P(cstring_to_text(qstr));
}

   
                            
                                                              
   
                                                                
                                                                
                                                               
                                                              
                                                  
   
static size_t
quote_literal_internal(char *dst, const char *src, size_t len)
{
  const char *s;
  char *savedst = dst;

  for (s = src; s < src + len; s++)
  {
    if (*s == '\\')
    {
      *dst++ = ESCAPE_STRING_SYNTAX;
      break;
    }
  }

  *dst++ = '\'';
  while (len-- > 0)
  {
    if (SQL_STR_DOUBLE(*src, true))
    {
      *dst++ = *src;
    }
    *dst++ = *src++;
  }
  *dst++ = '\'';

  return dst - savedst;
}

   
                   
                                       
   
Datum
quote_literal(PG_FUNCTION_ARGS)
{
  text *t = PG_GETARG_TEXT_PP(0);
  text *result;
  char *cp1;
  char *cp2;
  int len;

  len = VARSIZE_ANY_EXHDR(t);
                                                                      
  result = (text *)palloc(len * 2 + 3 + VARHDRSZ);

  cp1 = VARDATA_ANY(t);
  cp2 = VARDATA(result);

  SET_VARSIZE(result, VARHDRSZ + quote_literal_internal(cp2, cp1, len));

  PG_RETURN_TEXT_P(result);
}

   
                        
                                       
   
char *
quote_literal_cstr(const char *rawstr)
{
  char *result;
  int len;
  int newlen;

  len = strlen(rawstr);
                                                                      
  result = palloc(len * 2 + 3 + 1);

  newlen = quote_literal_internal(result, rawstr, len);
  result[newlen] = '\0';

  return result;
}

   
                    
                                                                  
                                
   
Datum
quote_nullable(PG_FUNCTION_ARGS)
{
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_TEXT_P(cstring_to_text("NULL"));
  }
  else
  {
    PG_RETURN_DATUM(DirectFunctionCall1(quote_literal, PG_GETARG_DATUM(0)));
  }
}
