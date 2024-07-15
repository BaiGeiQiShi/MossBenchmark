                                                                          
           
                                                           
   
                                                                          
   
                  
                                   
   
                                                                          
   
#include "postgres.h"

#include "mb/pg_wchar.h"
#include "utils/ascii.h"
#include "utils/builtins.h"

static void
pg_to_ascii(unsigned char *src, unsigned char *src_end, unsigned char *dest, int enc);
static text *
encode_to_ascii(text *data, int enc);

              
            
              
   
static void
pg_to_ascii(unsigned char *src, unsigned char *src_end, unsigned char *dest, int enc)
{
  unsigned char *x;
  const unsigned char *ascii;
  int range;

     
                                    
     
#define RANGE_128 128
#define RANGE_160 160

  if (enc == PG_LATIN1)
  {
       
                                      
       
    ascii = (const unsigned char *)"  cL Y  \"Ca  -R     'u .,      ?AAAAAAACEEEEIIII NOOOOOxOUUUUYTBaaaaaaaceeeeiiii nooooo/ouuuuyty";
    range = RANGE_160;
  }
  else if (enc == PG_LATIN2)
  {
       
                                      
       
    ascii = (const unsigned char *)" A L LS \"SSTZ-ZZ a,l'ls ,sstz\"zzRAAAALCCCEEEEIIDDNNOOOOxRUUUUYTBraaaalccceeeeiiddnnoooo/ruuuuyt.";
    range = RANGE_160;
  }
  else if (enc == PG_LATIN9)
  {
       
                                       
       
    ascii = (const unsigned char *)"  cL YS sCa  -R     Zu .z   EeY?AAAAAAACEEEEIIII NOOOOOxOUUUUYTBaaaaaaaceeeeiiii nooooo/ouuuuyty";
    range = RANGE_160;
  }
  else if (enc == PG_WIN1250)
  {
       
                                         
       
    ascii = (const unsigned char *)"  ' \"    %S<STZZ `'\"\".--  s>stzz   L A  \"CS  -RZ  ,l'u .,as L\"lzRAAAALCCCEEEEIIDDNNOOOOxRUUUUYTBraaaalccceeeeiiddnnoooo/ruuuuyt ";
    range = RANGE_128;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("encoding conversion from %s to ASCII not supported", pg_encoding_to_char(enc))));
    return;                          
  }

     
            
     
  for (x = src; x < src_end; x++)
  {
    if (*x < 128)
    {
      *dest++ = *x;
    }
    else if (*x < range)
    {
      *dest++ = ' ';                           
    }
    else
    {
      *dest++ = ascii[*x - range];
    }
  }
}

              
               
   
                                                                        
                                                             
              
   
static text *
encode_to_ascii(text *data, int enc)
{
  pg_to_ascii((unsigned char *)VARDATA(data),           
      (unsigned char *)(data) + VARSIZE(data),              
      (unsigned char *)VARDATA(data),                    
      enc);                                                  

  return data;
}

              
                                                
              
   
Datum
to_ascii_encname(PG_FUNCTION_ARGS)
{
  text *data = PG_GETARG_TEXT_P_COPY(0);
  char *encname = NameStr(*PG_GETARG_NAME(1));
  int enc = pg_char_to_encoding(encname);

  if (enc < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("%s is not a valid encoding name", encname)));
  }

  PG_RETURN_TEXT_P(encode_to_ascii(data, enc));
}

              
                                         
              
   
Datum
to_ascii_enc(PG_FUNCTION_ARGS)
{
  text *data = PG_GETARG_TEXT_P_COPY(0);
  int enc = PG_GETARG_INT32(1);

  if (!PG_VALID_ENCODING(enc))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("%d is not a valid encoding code", enc)));
  }

  PG_RETURN_TEXT_P(encode_to_ascii(data, enc));
}

              
                                                      
              
   
Datum
to_ascii_default(PG_FUNCTION_ARGS)
{
  text *data = PG_GETARG_TEXT_P_COPY(0);
  int enc = GetDatabaseEncoding();

  PG_RETURN_TEXT_P(encode_to_ascii(data, enc));
}

              
                                                                           
                                                                            
                                                                          
                 
   
                                                                        
              
   
void
ascii_safe_strlcpy(char *dest, const char *src, size_t destsiz)
{
  if (destsiz == 0)                                            
  {
    return;
  }

  while (--destsiz > 0)
  {
                                                          
    unsigned char ch = *src++;

    if (ch == '\0')
    {
      break;
    }
                                         
    if (32 <= ch && ch <= 127)
    {
      *dest = ch;
    }
                                
    else if (ch == '\n' || ch == '\r' || ch == '\t')
    {
      *dest = ch;
    }
                                              
    else
    {
      *dest = '?';
    }
    dest++;
  }

  *dest = '\0';
}
