                                                                            
   
              
                                                          
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <math.h>
#include <limits.h>
#include <ctype.h>

#include "common/int.h"
#include "utils/builtins.h"

   
                                      
   
                                                                   
   
                                                                          
   
                                                                    
                                                                            
   
                                                                           
             
   
int32
pg_atoi(const char *s, int size, int c)
{
  long l;
  char *badp;

     
                                                                          
                                                                 
     
  if (s == NULL)
  {
    elog(ERROR, "NULL pointer");
  }
  if (*s == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "integer", s)));
  }

  errno = 0;
  l = strtol(s, &badp, 10);

                                                           
  if (s == badp)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "integer", s)));
  }

  switch (size)
  {
  case sizeof(int32):
    if (errno == ERANGE
#if defined(HAVE_LONG_INT_64)
                                                            
        || l < INT_MIN || l > INT_MAX
#endif
    )
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", s, "integer")));
    break;
  case sizeof(int16):
    if (errno == ERANGE || l < SHRT_MIN || l > SHRT_MAX)
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", s, "smallint")));
    }
    break;
  case sizeof(int8):
    if (errno == ERANGE || l < SCHAR_MIN || l > SCHAR_MAX)
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for 8-bit integer", s)));
    }
    break;
  default:
    elog(ERROR, "unsupported result size: %d", size);
  }

     
                                                                             
                                         
     
  while (*badp && *badp != c && isspace((unsigned char)*badp))
  {
    badp++;
  }

  if (*badp && *badp != c)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "integer", s)));
  }

  return (int32)l;
}

   
                                                    
   
                                                                              
                                                
   
                                                                            
                                                                               
                    
   
int16
pg_strtoint16(const char *s)
{
  const char *ptr = s;
  int16 tmp = 0;
  bool neg = false;

                           
  while (likely(*ptr) && isspace((unsigned char)*ptr))
  {
    ptr++;
  }

                   
  if (*ptr == '-')
  {
    ptr++;
    neg = true;
  }
  else if (*ptr == '+')
  {
    ptr++;
  }

                                  
  if (unlikely(!isdigit((unsigned char)*ptr)))
  {
    goto invalid_syntax;
  }

                      
  while (*ptr && isdigit((unsigned char)*ptr))
  {
    int8 digit = (*ptr++ - '0');

    if (unlikely(pg_mul_s16_overflow(tmp, 10, &tmp)) || unlikely(pg_sub_s16_overflow(tmp, digit, &tmp)))
    {
      goto out_of_range;
    }
  }

                                                               
  while (*ptr != '\0' && isspace((unsigned char)*ptr))
  {
    ptr++;
  }

  if (unlikely(*ptr != '\0'))
  {
    goto invalid_syntax;
  }

  if (!neg)
  {
                                                     
    if (unlikely(tmp == PG_INT16_MIN))
    {
      goto out_of_range;
    }
    tmp = -tmp;
  }

  return tmp;

out_of_range:
  ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", s, "smallint")));

invalid_syntax:
  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "smallint", s)));

  return 0;                          
}

   
                                                    
   
                                                                              
                                                
   
                                                                            
                                                                               
                    
   
int32
pg_strtoint32(const char *s)
{
  const char *ptr = s;
  int32 tmp = 0;
  bool neg = false;

                           
  while (likely(*ptr) && isspace((unsigned char)*ptr))
  {
    ptr++;
  }

                   
  if (*ptr == '-')
  {
    ptr++;
    neg = true;
  }
  else if (*ptr == '+')
  {
    ptr++;
  }

                                  
  if (unlikely(!isdigit((unsigned char)*ptr)))
  {
    goto invalid_syntax;
  }

                      
  while (*ptr && isdigit((unsigned char)*ptr))
  {
    int8 digit = (*ptr++ - '0');

    if (unlikely(pg_mul_s32_overflow(tmp, 10, &tmp)) || unlikely(pg_sub_s32_overflow(tmp, digit, &tmp)))
    {
      goto out_of_range;
    }
  }

                                                               
  while (*ptr != '\0' && isspace((unsigned char)*ptr))
  {
    ptr++;
  }

  if (unlikely(*ptr != '\0'))
  {
    goto invalid_syntax;
  }

  if (!neg)
  {
                                                     
    if (unlikely(tmp == PG_INT32_MIN))
    {
      goto out_of_range;
    }
    tmp = -tmp;
  }

  return tmp;

out_of_range:
  ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", s, "integer")));

invalid_syntax:
  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "integer", s)));

  return 0;                          
}

   
                                                                          
   
                                                                          
                                                                 
   
                                                       
   
void
pg_itoa(int16 i, char *a)
{
  pg_ltoa((int32)i, a);
}

   
                                                                          
   
                                                                          
                                                                  
   
void
pg_ltoa(int32 value, char *a)
{
  char *start = a;
  bool neg = false;

     
                                                                           
                            
     
  if (value == PG_INT32_MIN)
  {
    memcpy(a, "-2147483648", 12);
    return;
  }
  else if (value < 0)
  {
    value = -value;
    neg = true;
  }

                                            
  do
  {
    int32 remainder;
    int32 oldval = value;

    value /= 10;
    remainder = oldval - value * 10;
    *a++ = '0' + remainder;
  } while (value != 0);

  if (neg)
  {
    *a++ = '-';
  }

                                                                     
  *a-- = '\0';

                       
  while (start < a)
  {
    char swap = *start;

    *start++ = *a;
    *a-- = swap;
  }
}

   
                                                                          
   
                                                                          
                                                                            
   
void
pg_lltoa(int64 value, char *a)
{
  char *start = a;
  bool neg = false;

     
                                                                           
                            
     
  if (value == PG_INT64_MIN)
  {
    memcpy(a, "-9223372036854775808", 21);
    return;
  }
  else if (value < 0)
  {
    value = -value;
    neg = true;
  }

                                            
  do
  {
    int64 remainder;
    int64 oldval = value;

    value /= 10;
    remainder = oldval - value * 10;
    *a++ = '0' + remainder;
  } while (value != 0);

  if (neg)
  {
    *a++ = '-';
  }

                                                                     
  *a-- = '\0';

                       
  while (start < a)
  {
    char swap = *start;

    *start++ = *a;
    *a-- = swap;
  }
}

   
                     
                                                                           
                                                                          
                                                     
   
                                                                               
                                                     
   
                                                                            
                                             
   
                                           
                 
                                          
                 
                                          
                
   
                                                                           
           
   
char *
pg_ltostr_zeropad(char *str, int32 value, int32 minwidth)
{
  char *start = str;
  char *end = &str[minwidth];
  int32 num = value;

  Assert(minwidth > 0);

     
                                                                          
                                                                       
     
  if (num < 0)
  {
    *start++ = '-';
    minwidth--;

       
                                                                         
                                                                           
                                                
       
    while (minwidth--)
    {
      int32 oldval = num;
      int32 remainder;

      num /= 10;
      remainder = oldval - num * 10;
      start[minwidth] = '0' - remainder;
    }
  }
  else
  {
                                                     
    while (minwidth--)
    {
      int32 oldval = num;
      int32 remainder;

      num /= 10;
      remainder = oldval - num * 10;
      start[minwidth] = '0' + remainder;
    }
  }

     
                                                                           
                                                                           
                                                                
     
  if (num != 0)
  {
    return pg_ltostr(str, value);
  }

                                                   
  return end;
}

   
             
                                                                           
   
                                                                               
                                                     
   
                                                                            
                                             
   
                            
                 
                            
                
   
                                                                           
           
   
char *
pg_ltostr(char *str, int32 value)
{
  char *start;
  char *end;

     
                                                                          
                                                                       
     
  if (value < 0)
  {
    *str++ = '-';

                                                            
    start = str;

                                              
    do
    {
      int32 oldval = value;
      int32 remainder;

      value /= 10;
      remainder = oldval - value * 10;
                                                         
      *str++ = '0' - remainder;
    } while (value != 0);
  }
  else
  {
                                                            
    start = str;

                                              
    do
    {
      int32 oldval = value;
      int32 remainder;

      value /= 10;
      remainder = oldval - value * 10;
      *str++ = '0' + remainder;
    } while (value != 0);
  }

                                                                   
  end = str--;

                       
  while (start < str)
  {
    char swap = *start;

    *start++ = *str;
    *str-- = swap;
  }

  return end;
}

   
                  
                                                    
   
                                                                        
                                                        
   
                                                                      
                                                      
   
uint64
pg_strtouint64(const char *str, char **endptr, int base)
{
#ifdef _MSC_VER                
  return _strtoui64(str, endptr, base);
#elif defined(HAVE_STRTOULL) && SIZEOF_LONG < 8
  return strtoull(str, endptr, base);
#else
  return strtoul(str, endptr, base);
#endif
}
