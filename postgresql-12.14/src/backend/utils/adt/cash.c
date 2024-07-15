   
          
                               
                   
                               
   
                                                                   
                                
   
                                                                    
                                                                  
                                                                    
                                                                    
                               
   
                                
   

#include "postgres.h"

#include <limits.h>
#include <ctype.h>
#include <math.h>

#include "common/int.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/cash.h"
#include "utils/int8.h"
#include "utils/numeric.h"
#include "utils/pg_locale.h"

                                                                           
                    
                                                                          

static const char *
num_word(Cash value)
{
  static char buf[128];
  static const char *const small[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
  const char *const *big = small + 18;
  int tu = value % 100;

                                        
  if (value <= 20)
  {
    return small[value];
  }

                                      
  if (!tu)
  {
    sprintf(buf, "%s hundred", small[value / 100]);
    return buf;
  }

                     
  if (value > 99)
  {
                                                     
    if (value % 10 == 0 && tu > 10)
    {
      sprintf(buf, "%s hundred %s", small[value / 100], big[tu / 10]);
    }
    else if (tu < 20)
    {
      sprintf(buf, "%s hundred and %s", small[value / 100], small[tu]);
    }
    else
    {
      sprintf(buf, "%s hundred %s %s", small[value / 100], big[tu / 10], small[tu % 10]);
    }
  }
  else
  {
                                                     
    if (value % 10 == 0 && tu > 10)
    {
      sprintf(buf, "%s", big[tu / 10]);
    }
    else if (tu < 20)
    {
      sprintf(buf, "%s", small[tu]);
    }
    else
    {
      sprintf(buf, "%s %s", big[tu / 10], small[tu % 10]);
    }
  }

  return buf;
}                 

             
                                         
                               
                                        
   
   
Datum
cash_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  Cash result;
  Cash value = 0;
  Cash dec = 0;
  Cash sgn = 1;
  bool seen_dot = false;
  const char *s = str;
  int fpoint;
  char dsymbol;
  const char *ssymbol, *psymbol, *nsymbol, *csymbol;
  struct lconv *lconvert = PGLC_localeconv();

     
                                                                             
                                                                          
                                                                           
                                                                         
                                                                          
                                                                           
                                                                            
                         
     
  fpoint = lconvert->frac_digits;
  if (fpoint < 0 || fpoint > 10)
  {
    fpoint = 2;                                       
  }

                                                                          
  if (*lconvert->mon_decimal_point != '\0' && lconvert->mon_decimal_point[1] == '\0')
  {
    dsymbol = *lconvert->mon_decimal_point;
  }
  else
  {
    dsymbol = '.';
  }
  if (*lconvert->mon_thousands_sep != '\0')
  {
    ssymbol = lconvert->mon_thousands_sep;
  }
  else                                       
  {
    ssymbol = (dsymbol != ',') ? "," : ".";
  }
  csymbol = (*lconvert->currency_symbol != '\0') ? lconvert->currency_symbol : "$";
  psymbol = (*lconvert->positive_sign != '\0') ? lconvert->positive_sign : "+";
  nsymbol = (*lconvert->negative_sign != '\0') ? lconvert->negative_sign : "-";

#ifdef CASHDEBUG
  printf("cashin- precision '%d'; decimal '%c'; thousands '%s'; currency '%s'; positive '%s'; negative '%s'\n", fpoint, dsymbol, ssymbol, csymbol, psymbol, nsymbol);
#endif

                                                                
                                                                    
  while (isspace((unsigned char)*s))
  {
    s++;
  }
  if (strncmp(s, csymbol, strlen(csymbol)) == 0)
  {
    s += strlen(csymbol);
  }
  while (isspace((unsigned char)*s))
  {
    s++;
  }

#ifdef CASHDEBUG
  printf("cashin- string is '%s'\n", s);
#endif

                                                            
                                       
                                                               
  if (strncmp(s, nsymbol, strlen(nsymbol)) == 0)
  {
    sgn = -1;
    s += strlen(nsymbol);
  }
  else if (*s == '(')
  {
    sgn = -1;
    s++;
  }
  else if (strncmp(s, psymbol, strlen(psymbol)) == 0)
  {
    s += strlen(psymbol);
  }

#ifdef CASHDEBUG
  printf("cashin- string is '%s'\n", s);
#endif

                                                                
  while (isspace((unsigned char)*s))
  {
    s++;
  }
  if (strncmp(s, csymbol, strlen(csymbol)) == 0)
  {
    s += strlen(csymbol);
  }
  while (isspace((unsigned char)*s))
  {
    s++;
  }

#ifdef CASHDEBUG
  printf("cashin- string is '%s'\n", s);
#endif

     
                                                                             
                                                                            
                                                                           
                                                                         
                                                                         
                
     

  for (; *s; s++)
  {
       
                                                                          
                                 
       
    if (isdigit((unsigned char)*s) && (!seen_dot || dec < fpoint))
    {
      int8 digit = *s - '0';

      if (pg_mul_s64_overflow(value, 10, &value) || pg_sub_s64_overflow(value, digit, &value))
      {
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", str, "money")));
      }

      if (seen_dot)
      {
        dec++;
      }
    }
                                                         
    else if (*s == dsymbol && !seen_dot)
    {
      seen_dot = true;
    }
                                                          
    else if (strncmp(s, ssymbol, strlen(ssymbol)) == 0)
    {
      s += strlen(ssymbol) - 1;
    }
    else
    {
      break;
    }
  }

                                          
  if (isdigit((unsigned char)*s) && *s >= '5')
  {
                                                     
    if (pg_sub_s64_overflow(value, 1, &value))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", str, "money")));
    }
  }

                                                    
  for (; dec < fpoint; dec++)
  {
    if (pg_mul_s64_overflow(value, 10, &value))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", str, "money")));
    }
  }

     
                                                                         
                                                    
     
  while (isdigit((unsigned char)*s))
  {
    s++;
  }

  while (*s)
  {
    if (isspace((unsigned char)*s) || *s == ')')
    {
      s++;
    }
    else if (strncmp(s, nsymbol, strlen(nsymbol)) == 0)
    {
      sgn = -1;
      s += strlen(nsymbol);
    }
    else if (strncmp(s, psymbol, strlen(psymbol)) == 0)
    {
      s += strlen(psymbol);
    }
    else if (strncmp(s, csymbol, strlen(csymbol)) == 0)
    {
      s += strlen(csymbol);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "money", str)));
    }
  }

     
                                                                           
                               
     
  if (sgn > 0)
  {
    if (value == PG_INT64_MIN)
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", str, "money")));
    }
    result = -value;
  }
  else
  {
    result = value;
  }

#ifdef CASHDEBUG
  printf("cashin- result is " INT64_FORMAT "\n", result);
#endif

  PG_RETURN_CASH(result);
}

              
                                                                         
                                        
   
Datum
cash_out(PG_FUNCTION_ARGS)
{
  Cash value = PG_GETARG_CASH(0);
  char *result;
  char buf[128];
  char *bufptr;
  int digit_pos;
  int points, mon_group;
  char dsymbol;
  const char *ssymbol, *csymbol, *signsymbol;
  char sign_posn, cs_precedes, sep_by_space;
  struct lconv *lconvert = PGLC_localeconv();

                                                   
  points = lconvert->frac_digits;
  if (points < 0 || points > 10)
  {
    points = 2;                                       
  }

     
                                                                            
                                              
     
  mon_group = *lconvert->mon_grouping;
  if (mon_group <= 0 || mon_group > 6)
  {
    mon_group = 3;
  }

                                                                          
  if (*lconvert->mon_decimal_point != '\0' && lconvert->mon_decimal_point[1] == '\0')
  {
    dsymbol = *lconvert->mon_decimal_point;
  }
  else
  {
    dsymbol = '.';
  }
  if (*lconvert->mon_thousands_sep != '\0')
  {
    ssymbol = lconvert->mon_thousands_sep;
  }
  else                                       
  {
    ssymbol = (dsymbol != ',') ? "," : ".";
  }
  csymbol = (*lconvert->currency_symbol != '\0') ? lconvert->currency_symbol : "$";

  if (value < 0)
  {
                                                                
    value = -value;
                                
    signsymbol = (*lconvert->negative_sign != '\0') ? lconvert->negative_sign : "-";
    sign_posn = lconvert->n_sign_posn;
    cs_precedes = lconvert->n_cs_precedes;
    sep_by_space = lconvert->n_sep_by_space;
  }
  else
  {
    signsymbol = lconvert->positive_sign;
    sign_posn = lconvert->p_sign_posn;
    cs_precedes = lconvert->p_cs_precedes;
    sep_by_space = lconvert->p_sep_by_space;
  }

                                                                           
  bufptr = buf + sizeof(buf) - 1;
  *bufptr = '\0';

     
                                                                           
                                                                      
                                                                             
                                     
     
  digit_pos = points;
  do
  {
    if (points && digit_pos == 0)
    {
                                                                       
      *(--bufptr) = dsymbol;
    }
    else if (digit_pos < 0 && (digit_pos % mon_group) == 0)
    {
                                                                 
      bufptr -= strlen(ssymbol);
      memcpy(bufptr, ssymbol, strlen(ssymbol));
    }

    *(--bufptr) = ((uint64)value % 10) + '0';
    value = ((uint64)value) / 10;
    digit_pos--;
  } while (value || digit_pos >= 0);

               
                                                                       
     
                                                                
     
                    
                                                                 
                                                                      
                                                                      
                                                     
                                                     
     
                                                                           
     
                       
                                                           
                                                                      
                                                                    
                                          
                                                                      
                                                                     
                      
               
     
  switch (sign_posn)
  {
  case 0:
    if (cs_precedes)
    {
      result = psprintf("(%s%s%s)", csymbol, (sep_by_space == 1) ? " " : "", bufptr);
    }
    else
    {
      result = psprintf("(%s%s%s)", bufptr, (sep_by_space == 1) ? " " : "", csymbol);
    }
    break;
  case 1:
  default:
    if (cs_precedes)
    {
      result = psprintf("%s%s%s%s%s", signsymbol, (sep_by_space == 2) ? " " : "", csymbol, (sep_by_space == 1) ? " " : "", bufptr);
    }
    else
    {
      result = psprintf("%s%s%s%s%s", signsymbol, (sep_by_space == 2) ? " " : "", bufptr, (sep_by_space == 1) ? " " : "", csymbol);
    }
    break;
  case 2:
    if (cs_precedes)
    {
      result = psprintf("%s%s%s%s%s", csymbol, (sep_by_space == 1) ? " " : "", bufptr, (sep_by_space == 2) ? " " : "", signsymbol);
    }
    else
    {
      result = psprintf("%s%s%s%s%s", bufptr, (sep_by_space == 1) ? " " : "", csymbol, (sep_by_space == 2) ? " " : "", signsymbol);
    }
    break;
  case 3:
    if (cs_precedes)
    {
      result = psprintf("%s%s%s%s%s", signsymbol, (sep_by_space == 2) ? " " : "", csymbol, (sep_by_space == 1) ? " " : "", bufptr);
    }
    else
    {
      result = psprintf("%s%s%s%s%s", bufptr, (sep_by_space == 1) ? " " : "", signsymbol, (sep_by_space == 2) ? " " : "", csymbol);
    }
    break;
  case 4:
    if (cs_precedes)
    {
      result = psprintf("%s%s%s%s%s", csymbol, (sep_by_space == 2) ? " " : "", signsymbol, (sep_by_space == 1) ? " " : "", bufptr);
    }
    else
    {
      result = psprintf("%s%s%s%s%s", bufptr, (sep_by_space == 1) ? " " : "", csymbol, (sep_by_space == 2) ? " " : "", signsymbol);
    }
    break;
  }

  PG_RETURN_CSTRING(result);
}

   
                                                          
   
Datum
cash_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_CASH((Cash)pq_getmsgint64(buf));
}

   
                                                 
   
Datum
cash_send(PG_FUNCTION_ARGS)
{
  Cash arg1 = PG_GETARG_CASH(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                        
   

Datum
cash_eq(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  PG_RETURN_BOOL(c1 == c2);
}

Datum
cash_ne(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  PG_RETURN_BOOL(c1 != c2);
}

Datum
cash_lt(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  PG_RETURN_BOOL(c1 < c2);
}

Datum
cash_le(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  PG_RETURN_BOOL(c1 <= c2);
}

Datum
cash_gt(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  PG_RETURN_BOOL(c1 > c2);
}

Datum
cash_ge(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  PG_RETURN_BOOL(c1 >= c2);
}

Datum
cash_cmp(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);

  if (c1 > c2)
  {
    PG_RETURN_INT32(1);
  }
  else if (c1 == c2)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(-1);
  }
}

             
                        
   
Datum
cash_pl(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);
  Cash result;

  result = c1 + c2;

  PG_RETURN_CASH(result);
}

             
                             
   
Datum
cash_mi(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);
  Cash result;

  result = c1 - c2;

  PG_RETURN_CASH(result);
}

                   
                                          
   
Datum
cash_div_cash(PG_FUNCTION_ARGS)
{
  Cash dividend = PG_GETARG_CASH(0);
  Cash divisor = PG_GETARG_CASH(1);
  float8 quotient;

  if (divisor == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  quotient = (float8)dividend / (float8)divisor;
  PG_RETURN_FLOAT8(quotient);
}

                   
                            
   
Datum
cash_mul_flt8(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  float8 f = PG_GETARG_FLOAT8(1);
  Cash result;

  result = rint(c * f);
  PG_RETURN_CASH(result);
}

                   
                            
   
Datum
flt8_mul_cash(PG_FUNCTION_ARGS)
{
  float8 f = PG_GETARG_FLOAT8(0);
  Cash c = PG_GETARG_CASH(1);
  Cash result;

  result = rint(f * c);
  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
cash_div_flt8(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  float8 f = PG_GETARG_FLOAT8(1);
  Cash result;

  if (f == 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  result = rint(c / f);
  PG_RETURN_CASH(result);
}

                   
                            
   
Datum
cash_mul_flt4(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  float4 f = PG_GETARG_FLOAT4(1);
  Cash result;

  result = rint(c * (float8)f);
  PG_RETURN_CASH(result);
}

                   
                            
   
Datum
flt4_mul_cash(PG_FUNCTION_ARGS)
{
  float4 f = PG_GETARG_FLOAT4(0);
  Cash c = PG_GETARG_CASH(1);
  Cash result;

  result = rint((float8)f * c);
  PG_RETURN_CASH(result);
}

                   
                          
   
   
Datum
cash_div_flt4(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  float4 f = PG_GETARG_FLOAT4(1);
  Cash result;

  if (f == 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  result = rint(c / (float8)f);
  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
cash_mul_int8(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  int64 i = PG_GETARG_INT64(1);
  Cash result;

  result = c * i;
  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
int8_mul_cash(PG_FUNCTION_ARGS)
{
  int64 i = PG_GETARG_INT64(0);
  Cash c = PG_GETARG_CASH(1);
  Cash result;

  result = i * c;
  PG_RETURN_CASH(result);
}

                   
                                  
   
Datum
cash_div_int8(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  int64 i = PG_GETARG_INT64(1);
  Cash result;

  if (i == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  result = c / i;

  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
cash_mul_int4(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  int32 i = PG_GETARG_INT32(1);
  Cash result;

  result = c * i;
  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
int4_mul_cash(PG_FUNCTION_ARGS)
{
  int32 i = PG_GETARG_INT32(0);
  Cash c = PG_GETARG_CASH(1);
  Cash result;

  result = i * c;
  PG_RETURN_CASH(result);
}

                   
                                  
   
   
Datum
cash_div_int4(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  int32 i = PG_GETARG_INT32(1);
  Cash result;

  if (i == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  result = c / i;

  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
cash_mul_int2(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  int16 s = PG_GETARG_INT16(1);
  Cash result;

  result = c * s;
  PG_RETURN_CASH(result);
}

                   
                          
   
Datum
int2_mul_cash(PG_FUNCTION_ARGS)
{
  int16 s = PG_GETARG_INT16(0);
  Cash c = PG_GETARG_CASH(1);
  Cash result;

  result = s * c;
  PG_RETURN_CASH(result);
}

                   
                        
   
   
Datum
cash_div_int2(PG_FUNCTION_ARGS)
{
  Cash c = PG_GETARG_CASH(0);
  int16 s = PG_GETARG_INT16(1);
  Cash result;

  if (s == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  result = c / s;
  PG_RETURN_CASH(result);
}

                
                                     
   
Datum
cashlarger(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);
  Cash result;

  result = (c1 > c2) ? c1 : c2;

  PG_RETURN_CASH(result);
}

                 
                                      
   
Datum
cashsmaller(PG_FUNCTION_ARGS)
{
  Cash c1 = PG_GETARG_CASH(0);
  Cash c2 = PG_GETARG_CASH(1);
  Cash result;

  result = (c1 < c2) ? c1 : c2;

  PG_RETURN_CASH(result);
}

                
                                                                     
                                                
   
Datum
cash_words(PG_FUNCTION_ARGS)
{
  Cash value = PG_GETARG_CASH(0);
  uint64 val;
  char buf[256];
  char *p = buf;
  Cash m0;
  Cash m1;
  Cash m2;
  Cash m3;
  Cash m4;
  Cash m5;
  Cash m6;

                                  
  if (value < 0)
  {
    value = -value;
    strcpy(buf, "minus ");
    p += 6;
  }
  else
  {
    buf[0] = '\0';
  }

                                                          
  val = (uint64)value;

  m0 = val % INT64CONST(100);                                    
  m1 = (val / INT64CONST(100)) % 1000;                              
  m2 = (val / INT64CONST(100000)) % 1000;                            
  m3 = (val / INT64CONST(100000000)) % 1000;                        
  m4 = (val / INT64CONST(100000000000)) % 1000;                     
  m5 = (val / INT64CONST(100000000000000)) % 1000;                   
  m6 = (val / INT64CONST(100000000000000000)) % 1000;                   

  if (m6)
  {
    strcat(buf, num_word(m6));
    strcat(buf, " quadrillion ");
  }

  if (m5)
  {
    strcat(buf, num_word(m5));
    strcat(buf, " trillion ");
  }

  if (m4)
  {
    strcat(buf, num_word(m4));
    strcat(buf, " billion ");
  }

  if (m3)
  {
    strcat(buf, num_word(m3));
    strcat(buf, " million ");
  }

  if (m2)
  {
    strcat(buf, num_word(m2));
    strcat(buf, " thousand ");
  }

  if (m1)
  {
    strcat(buf, num_word(m1));
  }

  if (!*p)
  {
    strcat(buf, "zero");
  }

  strcat(buf, (val / 100) == 1 ? " dollar and " : " dollars and ");
  strcat(buf, num_word(m0));
  strcat(buf, m0 == 1 ? " cent" : " cents");

                         
  buf[0] = pg_toupper((unsigned char)buf[0]);

                            
  PG_RETURN_TEXT_P(cstring_to_text(buf));
}

                  
                            
   
Datum
cash_numeric(PG_FUNCTION_ARGS)
{
  Cash money = PG_GETARG_CASH(0);
  Datum result;
  int fpoint;
  struct lconv *lconvert = PGLC_localeconv();

                                                   
  fpoint = lconvert->frac_digits;
  if (fpoint < 0 || fpoint > 10)
  {
    fpoint = 2;
  }

                                                   
  result = DirectFunctionCall1(int8_numeric, Int64GetDatum(money));

                                      
  if (fpoint > 0)
  {
    int64 scale;
    int i;
    Datum numeric_scale;
    Datum quotient;

                                       
    scale = 1;
    for (i = 0; i < fpoint; i++)
    {
      scale *= 10;
    }
    numeric_scale = DirectFunctionCall1(int8_numeric, Int64GetDatum(scale));

       
                                                                       
                                                                       
                                                                         
                                                                         
                                                           
       
    numeric_scale = DirectFunctionCall2(numeric_round, numeric_scale, Int32GetDatum(fpoint));

                                      
    quotient = DirectFunctionCall2(numeric_div, result, numeric_scale);

                                                                         
    result = DirectFunctionCall2(numeric_round, quotient, Int32GetDatum(fpoint));
  }

  PG_RETURN_DATUM(result);
}

                  
                            
   
Datum
numeric_cash(PG_FUNCTION_ARGS)
{
  Datum amount = PG_GETARG_DATUM(0);
  Cash result;
  int fpoint;
  int64 scale;
  int i;
  Datum numeric_scale;
  struct lconv *lconvert = PGLC_localeconv();

                                                   
  fpoint = lconvert->frac_digits;
  if (fpoint < 0 || fpoint > 10)
  {
    fpoint = 2;
  }

                                     
  scale = 1;
  for (i = 0; i < fpoint; i++)
  {
    scale *= 10;
  }

                                                 
  numeric_scale = DirectFunctionCall1(int8_numeric, Int64GetDatum(scale));
  amount = DirectFunctionCall2(numeric_mul, amount, numeric_scale);

                                                                   
  result = DatumGetInt64(DirectFunctionCall1(numeric_int8, amount));

  PG_RETURN_CASH(result);
}

               
                              
   
Datum
int4_cash(PG_FUNCTION_ARGS)
{
  int32 amount = PG_GETARG_INT32(0);
  Cash result;
  int fpoint;
  int64 scale;
  int i;
  struct lconv *lconvert = PGLC_localeconv();

                                                   
  fpoint = lconvert->frac_digits;
  if (fpoint < 0 || fpoint > 10)
  {
    fpoint = 2;
  }

                                     
  scale = 1;
  for (i = 0; i < fpoint; i++)
  {
    scale *= 10;
  }

                                                     
  result = DatumGetInt64(DirectFunctionCall2(int8mul, Int64GetDatum(amount), Int64GetDatum(scale)));

  PG_RETURN_CASH(result);
}

               
                                 
   
Datum
int8_cash(PG_FUNCTION_ARGS)
{
  int64 amount = PG_GETARG_INT64(0);
  Cash result;
  int fpoint;
  int64 scale;
  int i;
  struct lconv *lconvert = PGLC_localeconv();

                                                   
  fpoint = lconvert->frac_digits;
  if (fpoint < 0 || fpoint > 10)
  {
    fpoint = 2;
  }

                                     
  scale = 1;
  for (i = 0; i < fpoint; i++)
  {
    scale *= 10;
  }

                                                     
  result = DatumGetInt64(DirectFunctionCall2(int8mul, Int64GetDatum(amount), Int64GetDatum(scale)));

  PG_RETURN_CASH(result);
}
