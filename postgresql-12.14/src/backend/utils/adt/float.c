                                                                            
   
           
                                                      
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include "catalog/pg_type.h"
#include "common/int.h"
#include "common/shortest_dec.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/sortsupport.h"
#include "utils/timestamp.h"

   
                              
   
                                                                               
                                                                             
                                                                          
                            
   
int extra_float_digits = 1;

                                                      
static bool degree_consts_set = false;
static float8 sin_30 = 0;
static float8 one_minus_cos_60 = 0;
static float8 asin_0_5 = 0;
static float8 acos_0_5 = 0;
static float8 atan_1_0 = 0;
static float8 tan_45 = 0;
static float8 cot_45 = 0;

   
                                                                          
                                                                          
                                                                      
                                                              
   
float8 degree_c_thirty = 30.0;
float8 degree_c_forty_five = 45.0;
float8 degree_c_sixty = 60.0;
float8 degree_c_one_half = 0.5;
float8 degree_c_one = 1.0;

                                       
static bool drandom_seed_set = false;
static unsigned short drandom_seed[3] = {0, 0, 0};

                               
static double
sind_q1(double x);
static double
cosd_q1(double x);
static void
init_degree_constants(void);

#ifndef HAVE_CBRT
   
                                                                      
                                                                     
                                                                        
                                                                      
                                                                     
                                                                          
   
#define cbrt my_cbrt
static double
cbrt(double x);
#endif                

   
                                                                      
                                                                       
                                                                       
   
                                                                        
   
pg_noinline void
float_overflow_error(void)
{
  ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value out of range: overflow")));
}

pg_noinline void
float_underflow_error(void)
{
  ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value out of range: underflow")));
}

pg_noinline void
float_zero_divide_error(void)
{
  ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
}

   
                                                                
                                                                       
                                                                    
                                                                       
                          
   
int
is_infinite(double val)
{
  int inf = isinf(val);

  if (inf == 0)
  {
    return 0;
  }
  else if (val > 0)
  {
    return 1;
  }
  else
  {
    return -1;
  }
}

                                             

   
                                         
   
                                                                         
   
                                                                            
                                                                             
                                                                            
                                                                              
   
                                             
   
                                                 
                                                 
                                                 
   
                                                                               
                         
   
                                                          
                                                          
                                                          
                                                          
   
                                                                             
                                                                          
                            
   
   
Datum
float4in(PG_FUNCTION_ARGS)
{
  char *num = PG_GETARG_CSTRING(0);
  char *orig_num;
  float val;
  char *endptr;

     
                                                                             
                                                                             
             
     
  orig_num = num;

                               
  while (*num != '\0' && isspace((unsigned char)*num))
  {
    num++;
  }

     
                                                                             
                                      
     
  if (*num == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "real", orig_num)));
  }

  errno = 0;
  val = strtof(num, &endptr);

                                                         
  if (endptr == num || errno != 0)
  {
    int save_errno = errno;

       
                                                                         
                                                                        
                                                                       
                                    
       
                                                                          
                                                                          
                                                                     
       
    if (pg_strncasecmp(num, "NaN", 3) == 0)
    {
      val = get_float4_nan();
      endptr = num + 3;
    }
    else if (pg_strncasecmp(num, "Infinity", 8) == 0)
    {
      val = get_float4_infinity();
      endptr = num + 8;
    }
    else if (pg_strncasecmp(num, "+Infinity", 9) == 0)
    {
      val = get_float4_infinity();
      endptr = num + 9;
    }
    else if (pg_strncasecmp(num, "-Infinity", 9) == 0)
    {
      val = -get_float4_infinity();
      endptr = num + 9;
    }
    else if (pg_strncasecmp(num, "inf", 3) == 0)
    {
      val = get_float4_infinity();
      endptr = num + 3;
    }
    else if (pg_strncasecmp(num, "+inf", 4) == 0)
    {
      val = get_float4_infinity();
      endptr = num + 4;
    }
    else if (pg_strncasecmp(num, "-inf", 4) == 0)
    {
      val = -get_float4_infinity();
      endptr = num + 4;
    }
    else if (save_errno == ERANGE)
    {
         
                                                                      
                                                                   
                                                                         
                                                                         
                                               
         
                                                                
                                                                         
                                          
         
      if (val == 0.0 ||
#if !defined(HUGE_VALF) || (defined(_MSC_VER) && (_MSC_VER < 1900))
          isinf(val)
#else
          (val >= HUGE_VALF || val <= -HUGE_VALF)
#endif
      )
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("\"%s\" is out of range for type real", orig_num)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "real", orig_num)));
    }
  }
#ifdef HAVE_BUGGY_SOLARIS_STRTOD
  else
  {
       
                                                                         
                                                                       
                   
       
    if (endptr != num && endptr[-1] == '\0')
    {
      endptr--;
    }
  }
#endif                                

                                
  while (*endptr != '\0' && isspace((unsigned char)*endptr))
  {
    endptr++;
  }

                                                                    
  if (*endptr != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "real", orig_num)));
  }

  PG_RETURN_FLOAT4(val);
}

   
                                                      
                                         
   
Datum
float4out(PG_FUNCTION_ARGS)
{
  float4 num = PG_GETARG_FLOAT4(0);
  char *ascii = (char *)palloc(32);
  int ndig = FLT_DIG + extra_float_digits;

  if (extra_float_digits > 0)
  {
    float_to_shortest_decimal_buf(num, ascii);
    PG_RETURN_CSTRING(ascii);
  }

  (void)pg_strfromd(ascii, 32, ndig, num);
  PG_RETURN_CSTRING(ascii);
}

   
                                                             
   
Datum
float4recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_FLOAT4(pq_getmsgfloat4(buf));
}

   
                                                    
   
Datum
float4send(PG_FUNCTION_ARGS)
{
  float4 num = PG_GETARG_FLOAT4(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat4(&buf, num);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                         
   
Datum
float8in(PG_FUNCTION_ARGS)
{
  char *num = PG_GETARG_CSTRING(0);

  PG_RETURN_FLOAT8(float8in_internal(num, NULL, "double precision", num));
}

                                                                          
#define RETURN_ERROR(throw_error)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (have_error)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *have_error = true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      return 0.0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      throw_error;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

   
                                                    
   
                                                               
                                                                   
                                                                      
                
                                                        
                                                                    
                                                                     
                                                                          
                                                                        
                                                                         
                                                    
   
                                                                          
                                                                               
   
                                                                        
                                                                        
   
double
float8in_internal_opt_error(char *num, char **endptr_p, const char *type_name, const char *orig_string, bool *have_error)
{
  double val;
  char *endptr;

                               
  while (*num != '\0' && isspace((unsigned char)*num))
  {
    num++;
  }

     
                                                                             
                                      
     
  if (*num == '\0')
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string))));
  }

  errno = 0;
  val = strtod(num, &endptr);

                                                         
  if (endptr == num || errno != 0)
  {
    int save_errno = errno;

       
                                                                         
                                                                        
                                                                       
                                    
       
                                                                          
                                                                          
                                                                     
       
    if (pg_strncasecmp(num, "NaN", 3) == 0)
    {
      val = get_float8_nan();
      endptr = num + 3;
    }
    else if (pg_strncasecmp(num, "Infinity", 8) == 0)
    {
      val = get_float8_infinity();
      endptr = num + 8;
    }
    else if (pg_strncasecmp(num, "+Infinity", 9) == 0)
    {
      val = get_float8_infinity();
      endptr = num + 9;
    }
    else if (pg_strncasecmp(num, "-Infinity", 9) == 0)
    {
      val = -get_float8_infinity();
      endptr = num + 9;
    }
    else if (pg_strncasecmp(num, "inf", 3) == 0)
    {
      val = get_float8_infinity();
      endptr = num + 3;
    }
    else if (pg_strncasecmp(num, "+inf", 4) == 0)
    {
      val = get_float8_infinity();
      endptr = num + 4;
    }
    else if (pg_strncasecmp(num, "-inf", 4) == 0)
    {
      val = -get_float8_infinity();
      endptr = num + 4;
    }
    else if (save_errno == ERANGE)
    {
         
                                                                      
                                                                   
                                                                         
                                                                         
                                               
         
                                                                        
                                                                       
                                     
         
      if (val == 0.0 || val >= HUGE_VAL || val <= -HUGE_VAL)
      {
        char *errnumber = pstrdup(num);

        errnumber[endptr - num] = '\0';
        RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("\"%s\" is out of range for "
                                                                                         "type double precision",
                                                                                      errnumber))));
      }
    }
    else
    {
      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type "
                                                                                        "%s: \"%s\"",
                                                                                     type_name, orig_string))));
    }
  }
#ifdef HAVE_BUGGY_SOLARIS_STRTOD
  else
  {
       
                                                                         
                                                                       
                   
       
    if (endptr != num && endptr[-1] == '\0')
    {
      endptr--;
    }
  }
#endif                                

                                
  while (*endptr != '\0' && isspace((unsigned char)*endptr))
  {
    endptr++;
  }

                                                                           
  if (endptr_p)
  {
    *endptr_p = endptr;
  }
  else if (*endptr != '\0')
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type "
                                                                                      "%s: \"%s\"",
                                                                                   type_name, orig_string))));
  }

  return val;
}

   
                                                                             
   
double
float8in_internal(char *num, char **endptr_p, const char *type_name, const char *orig_string)
{
  return float8in_internal_opt_error(num, endptr_p, type_name, orig_string, NULL);
}

   
                                                    
                                         
   
Datum
float8out(PG_FUNCTION_ARGS)
{
  float8 num = PG_GETARG_FLOAT8(0);

  PG_RETURN_CSTRING(float8out_internal(num));
}

   
                                            
   
                                                               
                                                   
                                  
   
char *
float8out_internal(double num)
{
  char *ascii = (char *)palloc(32);
  int ndig = DBL_DIG + extra_float_digits;

  if (extra_float_digits > 0)
  {
    double_to_shortest_decimal_buf(num, ascii);
    return ascii;
  }

  (void)pg_strfromd(ascii, 32, ndig, num);
  return ascii;
}

   
                                                             
   
Datum
float8recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_FLOAT8(pq_getmsgfloat8(buf));
}

   
                                                    
   
Datum
float8send(PG_FUNCTION_ARGS)
{
  float8 num = PG_GETARG_FLOAT8(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, num);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                           

   
                           
                           
                           
   

   
                                                 
   
Datum
float4abs(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);

  PG_RETURN_FLOAT4((float4)fabs(arg1));
}

   
                                            
   
Datum
float4um(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 result;

  result = -arg1;
  PG_RETURN_FLOAT4(result);
}

Datum
float4up(PG_FUNCTION_ARGS)
{
  float4 arg = PG_GETARG_FLOAT4(0);

  PG_RETURN_FLOAT4(arg);
}

Datum
float4larger(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);
  float4 result;

  if (float4_gt(arg1, arg2))
  {
    result = arg1;
  }
  else
  {
    result = arg2;
  }
  PG_RETURN_FLOAT4(result);
}

Datum
float4smaller(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);
  float4 result;

  if (float4_lt(arg1, arg2))
  {
    result = arg1;
  }
  else
  {
    result = arg2;
  }
  PG_RETURN_FLOAT4(result);
}

   
                           
                           
                           
   

   
                                                 
   
Datum
float8abs(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(fabs(arg1));
}

   
                                            
   
Datum
float8um(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  result = -arg1;
  PG_RETURN_FLOAT8(result);
}

Datum
float8up(PG_FUNCTION_ARGS)
{
  float8 arg = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(arg);
}

Datum
float8larger(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);
  float8 result;

  if (float8_gt(arg1, arg2))
  {
    result = arg1;
  }
  else
  {
    result = arg2;
  }
  PG_RETURN_FLOAT8(result);
}

Datum
float8smaller(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);
  float8 result;

  if (float8_lt(arg1, arg2))
  {
    result = arg1;
  }
  else
  {
    result = arg2;
  }
  PG_RETURN_FLOAT8(result);
}

   
                         
                         
                         
   

   
                                    
                                    
                                     
                                     
   
Datum
float4pl(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT4(float4_pl(arg1, arg2));
}

Datum
float4mi(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT4(float4_mi(arg1, arg2));
}

Datum
float4mul(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT4(float4_mul(arg1, arg2));
}

Datum
float4div(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT4(float4_div(arg1, arg2));
}

   
                                    
                                    
                                     
                                     
   
Datum
float8pl(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_pl(arg1, arg2));
}

Datum
float8mi(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_mi(arg1, arg2));
}

Datum
float8mul(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_mul(arg1, arg2));
}

Datum
float8div(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_div(arg1, arg2));
}

   
                         
                         
                         
   

   
                                                                     
   
int
float4_cmp_internal(float4 a, float4 b)
{
  if (float4_gt(a, b))
  {
    return 1;
  }
  if (float4_lt(a, b))
  {
    return -1;
  }
  return 0;
}

Datum
float4eq(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float4_eq(arg1, arg2));
}

Datum
float4ne(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float4_ne(arg1, arg2));
}

Datum
float4lt(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float4_lt(arg1, arg2));
}

Datum
float4le(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float4_le(arg1, arg2));
}

Datum
float4gt(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float4_gt(arg1, arg2));
}

Datum
float4ge(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float4_ge(arg1, arg2));
}

Datum
btfloat4cmp(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_INT32(float4_cmp_internal(arg1, arg2));
}

static int
btfloat4fastcmp(Datum x, Datum y, SortSupport ssup)
{
  float4 arg1 = DatumGetFloat4(x);
  float4 arg2 = DatumGetFloat4(y);

  return float4_cmp_internal(arg1, arg2);
}

Datum
btfloat4sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = btfloat4fastcmp;
  PG_RETURN_VOID();
}

   
                                                                     
   
int
float8_cmp_internal(float8 a, float8 b)
{
  if (float8_gt(a, b))
  {
    return 1;
  }
  if (float8_lt(a, b))
  {
    return -1;
  }
  return 0;
}

Datum
float8eq(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_eq(arg1, arg2));
}

Datum
float8ne(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_ne(arg1, arg2));
}

Datum
float8lt(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_lt(arg1, arg2));
}

Datum
float8le(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_le(arg1, arg2));
}

Datum
float8gt(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_gt(arg1, arg2));
}

Datum
float8ge(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_ge(arg1, arg2));
}

Datum
btfloat8cmp(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_INT32(float8_cmp_internal(arg1, arg2));
}

static int
btfloat8fastcmp(Datum x, Datum y, SortSupport ssup)
{
  float8 arg1 = DatumGetFloat8(x);
  float8 arg2 = DatumGetFloat8(y);

  return float8_cmp_internal(arg1, arg2);
}

Datum
btfloat8sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = btfloat8fastcmp;
  PG_RETURN_VOID();
}

Datum
btfloat48cmp(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

                                               
  PG_RETURN_INT32(float8_cmp_internal(arg1, arg2));
}

Datum
btfloat84cmp(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

                                               
  PG_RETURN_INT32(float8_cmp_internal(arg1, arg2));
}

   
                                         
   
                                                                         
                                                                 
   
Datum
in_range_float8_float8(PG_FUNCTION_ARGS)
{
  float8 val = PG_GETARG_FLOAT8(0);
  float8 base = PG_GETARG_FLOAT8(1);
  float8 offset = PG_GETARG_FLOAT8(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  float8 sum;

     
                                                                      
                                                              
     
  if (isnan(offset) || offset < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

     
                                                                           
                                                                          
                            
     
  if (isnan(val))
  {
    if (isnan(base))
    {
      PG_RETURN_BOOL(true);                
    }
    else
    {
      PG_RETURN_BOOL(!less);                    
    }
  }
  else if (isnan(base))
  {
    PG_RETURN_BOOL(less);                    
  }

     
                                                                           
                                                                           
                                                                 
     
  if (isinf(offset))
  {
    PG_RETURN_BOOL(sub ? !less : less);
  }

     
                                                                           
                                                                       
                                                                             
                                         
     
  if (sub)
  {
    sum = base - offset;
  }
  else
  {
    sum = base + offset;
  }

  if (less)
  {
    PG_RETURN_BOOL(val <= sum);
  }
  else
  {
    PG_RETURN_BOOL(val >= sum);
  }
}

   
                                         
   
                                                                            
                                                              
   
Datum
in_range_float4_float8(PG_FUNCTION_ARGS)
{
  float4 val = PG_GETARG_FLOAT4(0);
  float4 base = PG_GETARG_FLOAT4(1);
  float8 offset = PG_GETARG_FLOAT8(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  float8 sum;

     
                                                                      
                                                              
     
  if (isnan(offset) || offset < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

     
                                                                           
                                                                          
                            
     
  if (isnan(val))
  {
    if (isnan(base))
    {
      PG_RETURN_BOOL(true);                
    }
    else
    {
      PG_RETURN_BOOL(!less);                    
    }
  }
  else if (isnan(base))
  {
    PG_RETURN_BOOL(less);                    
  }

     
                                                                           
                                                                           
                                                                 
     
  if (isinf(offset))
  {
    PG_RETURN_BOOL(sub ? !less : less);
  }

     
                                                                           
                                                                       
                                                                             
                                         
     
  if (sub)
  {
    sum = base - offset;
  }
  else
  {
    sum = base + offset;
  }

  if (less)
  {
    PG_RETURN_BOOL(val <= sum);
  }
  else
  {
    PG_RETURN_BOOL(val >= sum);
  }
}

   
                        
                        
                        
   

   
                                                         
   
Datum
ftod(PG_FUNCTION_ARGS)
{
  float4 num = PG_GETARG_FLOAT4(0);

  PG_RETURN_FLOAT8((float8)num);
}

   
                                                         
   
Datum
dtof(PG_FUNCTION_ARGS)
{
  float8 num = PG_GETARG_FLOAT8(0);
  float4 result;

  result = (float4)num;
  if (unlikely(isinf(result)) && !isinf(num))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0f) && num != 0.0)
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT4(result);
}

   
                                                         
   
Datum
dtoi4(PG_FUNCTION_ARGS)
{
  float8 num = PG_GETARG_FLOAT8(0);

     
                                                                            
                                                                    
                                                                      
     
  num = rint(num);

                   
  if (unlikely(isnan(num) || !FLOAT8_FITS_IN_INT32(num)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  PG_RETURN_INT32((int32)num);
}

   
                                                         
   
Datum
dtoi2(PG_FUNCTION_ARGS)
{
  float8 num = PG_GETARG_FLOAT8(0);

     
                                                                            
                                                                    
                                                                      
     
  num = rint(num);

                   
  if (unlikely(isnan(num) || !FLOAT8_FITS_IN_INT16(num)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

  PG_RETURN_INT16((int16)num);
}

   
                                                         
   
Datum
i4tod(PG_FUNCTION_ARGS)
{
  int32 num = PG_GETARG_INT32(0);

  PG_RETURN_FLOAT8((float8)num);
}

   
                                                         
   
Datum
i2tod(PG_FUNCTION_ARGS)
{
  int16 num = PG_GETARG_INT16(0);

  PG_RETURN_FLOAT8((float8)num);
}

   
                                                         
   
Datum
ftoi4(PG_FUNCTION_ARGS)
{
  float4 num = PG_GETARG_FLOAT4(0);

     
                                                                            
                                                                    
                                                                      
     
  num = rint(num);

                   
  if (unlikely(isnan(num) || !FLOAT4_FITS_IN_INT32(num)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  PG_RETURN_INT32((int32)num);
}

   
                                                         
   
Datum
ftoi2(PG_FUNCTION_ARGS)
{
  float4 num = PG_GETARG_FLOAT4(0);

     
                                                                            
                                                                    
                                                                      
     
  num = rint(num);

                   
  if (unlikely(isnan(num) || !FLOAT4_FITS_IN_INT16(num)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

  PG_RETURN_INT16((int16)num);
}

   
                                                         
   
Datum
i4tof(PG_FUNCTION_ARGS)
{
  int32 num = PG_GETARG_INT32(0);

  PG_RETURN_FLOAT4((float4)num);
}

   
                                                         
   
Datum
i2tof(PG_FUNCTION_ARGS)
{
  int16 num = PG_GETARG_INT16(0);

  PG_RETURN_FLOAT4((float4)num);
}

   
                            
                            
                            
   

   
                                   
   
Datum
dround(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(rint(arg1));
}

   
                                                           
                                       
   
Datum
dceil(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(ceil(arg1));
}

   
                                                          
                                       
   
Datum
dfloor(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(floor(arg1));
}

   
                                                           
                                                      
                                         
   
Datum
dsign(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  if (arg1 > 0)
  {
    result = 1.0;
  }
  else if (arg1 < 0)
  {
    result = -1.0;
  }
  else
  {
    result = 0.0;
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                        
                                                  
                                  
                                                 
                                  
   
Datum
dtrunc(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  if (arg1 >= 0)
  {
    result = floor(arg1);
  }
  else
  {
    result = -floor(-arg1);
  }

  PG_RETURN_FLOAT8(result);
}

   
                                          
   
Datum
dsqrt(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  if (arg1 < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_POWER_FUNCTION), errmsg("cannot take square root of a negative number")));
  }

  result = sqrt(arg1);
  if (unlikely(isinf(result)) && !isinf(arg1))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0) && arg1 != 0.0)
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                        
   
Datum
dcbrt(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  result = cbrt(arg1);
  if (unlikely(isinf(result)) && !isinf(arg1))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0) && arg1 != 0.0)
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                    
   
Datum
dpow(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);
  float8 result;

     
                                                                            
                                                                            
                                                                          
                                        
     
  if (isnan(arg1))
  {
    if (isnan(arg2) || arg2 != 0.0)
    {
      PG_RETURN_FLOAT8(get_float8_nan());
    }
    PG_RETURN_FLOAT8(1.0);
  }
  if (isnan(arg2))
  {
    if (arg1 != 1.0)
    {
      PG_RETURN_FLOAT8(get_float8_nan());
    }
    PG_RETURN_FLOAT8(1.0);
  }

     
                                                                             
                                                                
                                           
     
  if (arg1 == 0 && arg2 < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_POWER_FUNCTION), errmsg("zero raised to a negative power is undefined")));
  }
  if (arg1 < 0 && floor(arg2) != arg2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_POWER_FUNCTION), errmsg("a negative number raised to a non-integer power yields a complex result")));
  }

     
                                                                      
                                                                           
                                                                          
                                                                           
                                                                    
                                                                          
                                                      
     
  errno = 0;
  result = pow(arg1, arg2);
  if (errno == EDOM && isnan(result))
  {
    if ((fabs(arg1) > 1 && arg2 >= 0) || (fabs(arg1) < 1 && arg2 < 0))
    {
                                                            
      result = get_float8_infinity();
    }
    else if (fabs(arg1) != 1)
    {
      result = 0;
    }
    else
    {
      result = 1;
    }
  }
  else if (errno == ERANGE && result != 0 && !isinf(result))
  {
    result = get_float8_infinity();
  }

  if (unlikely(isinf(result)) && !isinf(arg1) && !isinf(arg2))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0) && arg1 != 0.0)
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                      
   
Datum
dexp(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  errno = 0;
  result = exp(arg1);
  if (errno == ERANGE && result != 0 && !isinf(result))
  {
    result = get_float8_infinity();
  }

  if (unlikely(isinf(result)) && !isinf(arg1))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0))
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                    
   
Datum
dlog1(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

     
                                                                            
                   
     
  if (arg1 == 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_LOG), errmsg("cannot take logarithm of zero")));
  }
  if (arg1 < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_LOG), errmsg("cannot take logarithm of a negative number")));
  }

  result = log(arg1);
  if (unlikely(isinf(result)) && !isinf(arg1))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0) && arg1 != 1.0)
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                     
   
Datum
dlog10(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

     
                                                                          
                                                                          
                                                       
     
  if (arg1 == 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_LOG), errmsg("cannot take logarithm of zero")));
  }
  if (arg1 < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_LOG), errmsg("cannot take logarithm of a negative number")));
  }

  result = log10(arg1);
  if (unlikely(isinf(result)) && !isinf(arg1))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0) && arg1 != 1.0)
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                   
   
Datum
dacos(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

     
                                                                            
                                                                           
                                                                     
     
  if (arg1 < -1.0 || arg1 > 1.0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  result = acos(arg1);
  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                   
   
Datum
dasin(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

     
                                                                          
                                                                             
                                                                         
     
  if (arg1 < -1.0 || arg1 > 1.0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  result = asin(arg1);
  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                   
   
Datum
datan(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

     
                                                                             
                                                                       
                                            
     
  result = atan(arg1);
  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                        
   
Datum
datan2(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);
  float8 result;

                                                             
  if (isnan(arg1) || isnan(arg2))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

     
                                                                           
                                                               
     
  result = atan2(arg1, arg2);
  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                  
   
Datum
dcos(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

     
                                                                            
                                                                           
                                                                             
                                                                        
                                                                         
                                                                         
                                                                             
                              
     
                                                                           
                                                                       
                                                                      
             
     
  errno = 0;
  result = cos(arg1);
  if (errno != 0 || isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }
  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                     
   
Datum
dcot(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

                                                                         
  errno = 0;
  result = tan(arg1);
  if (errno != 0 || isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  result = 1.0 / result;
                                                       

  PG_RETURN_FLOAT8(result);
}

   
                                                
   
Datum
dsin(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

                                                                         
  errno = 0;
  result = sin(arg1);
  if (errno != 0 || isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }
  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                   
   
Datum
dtan(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

                                                                         
  errno = 0;
  result = tan(arg1);
  if (errno != 0 || isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }
                                                          

  PG_RETURN_FLOAT8(result);
}

                                                                

   
                                                                     
                                                                          
                                                                          
                                                                              
                                                                              
                                                                              
                                                                           
                                                                              
                                                   
   
                                                                        
                                                                         
                                                                        
   
                                                                           
                                                         
                             
                                                                              
                                                                         
                                                                             
                                                                             
                                                                              
                                                                               
                                                                            
                                                                             
                                                
   
static void
init_degree_constants(void)
{
  sin_30 = sin(degree_c_thirty * RADIANS_PER_DEGREE);
  one_minus_cos_60 = 1.0 - cos(degree_c_sixty * RADIANS_PER_DEGREE);
  asin_0_5 = asin(degree_c_one_half);
  acos_0_5 = acos(degree_c_one_half);
  atan_1_0 = atan(degree_c_one);
  tan_45 = sind_q1(degree_c_forty_five) / cosd_q1(degree_c_forty_five);
  cot_45 = cosd_q1(degree_c_forty_five) / sind_q1(degree_c_forty_five);
  degree_consts_set = true;
}

#define INIT_DEGREE_CONSTANTS()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!degree_consts_set)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      init_degree_constants();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

   
                                                                   
                                                           
                                              
   
                                                             
                                                          
                                 
   
static double
asind_q1(double x)
{
     
                                                                          
                                                                       
                                                                            
                          
     
  if (x <= 0.5)
  {
    volatile float8 asin_x = asin(x);

    return (asin_x / asin_0_5) * 30.0;
  }
  else
  {
    volatile float8 acos_x = acos(x);

    return 90.0 - (acos_x / acos_0_5) * 60.0;
  }
}

   
                                                                     
                                                           
                                              
   
                                                             
                                                          
                                 
   
static double
acosd_q1(double x)
{
     
                                                                          
                                                                       
                                                                            
                          
     
  if (x <= 0.5)
  {
    volatile float8 asin_x = asin(x);

    return 90.0 - (asin_x / asin_0_5) * 30.0;
  }
  else
  {
    volatile float8 acos_x = acos(x);

    return (acos_x / acos_0_5) * 60.0;
  }
}

   
                                                    
   
Datum
dacosd(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  INIT_DEGREE_CONSTANTS();

     
                                                                            
                                                                            
                                                                     
     
  if (arg1 < -1.0 || arg1 > 1.0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  if (arg1 >= 0.0)
  {
    result = acosd_q1(arg1);
  }
  else
  {
    result = 90.0 + asind_q1(-arg1);
  }

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                    
   
Datum
dasind(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  INIT_DEGREE_CONSTANTS();

     
                                                                          
                                                                             
                                                                     
     
  if (arg1 < -1.0 || arg1 > 1.0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  if (arg1 >= 0.0)
  {
    result = asind_q1(arg1);
  }
  else
  {
    result = -asind_q1(-arg1);
  }

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                    
   
Datum
datand(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;
  volatile float8 atan_arg1;

                                                          
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  INIT_DEGREE_CONSTANTS();

     
                                                                             
                                                                           
                                                                          
                                                    
     
  atan_arg1 = atan(arg1);
  result = (atan_arg1 / atan_1_0) * 45.0;

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                         
   
Datum
datan2d(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);
  float8 result;
  volatile float8 atan2_arg1_arg2;

                                                             
  if (isnan(arg1) || isnan(arg2))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  INIT_DEGREE_CONSTANTS();

     
                                                                       
                                                                      
     
                                                                             
                                                                         
                                                                           
                                             
     
  atan2_arg1_arg2 = atan2(arg1, arg2);
  result = (atan2_arg1_arg2 / atan_1_0) * 45.0;

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                                        
                                                               
                                                
   
static double
sind_0_to_30(double x)
{
  volatile float8 sin_x = sin(x * RADIANS_PER_DEGREE);

  return (sin_x / sin_30) / 2.0;
}

   
                                                                      
                                                             
                                                      
   
static double
cosd_0_to_60(double x)
{
  volatile float8 one_minus_cos_x = 1.0 - cos(x * RADIANS_PER_DEGREE);

  return 1.0 - (one_minus_cos_x / one_minus_cos_60) / 2.0;
}

   
                                                                   
                             
   
static double
sind_q1(double x)
{
     
                                                                          
                                                                     
                                                                         
                                                             
     
  if (x <= 30.0)
  {
    return sind_0_to_30(x);
  }
  else
  {
    return cosd_0_to_60(90.0 - x);
  }
}

   
                                                                     
                             
   
static double
cosd_q1(double x)
{
     
                                                                          
                                                                     
                                                                         
                                                             
     
  if (x <= 60.0)
  {
    return cosd_0_to_60(x);
  }
  else
  {
    return sind_0_to_30(90.0 - x);
  }
}

   
                                                   
   
Datum
dcosd(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;
  int sign = 1;

     
                                                                           
                               
     
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  if (isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  INIT_DEGREE_CONSTANTS();

                                                       
  arg1 = fmod(arg1, 360.0);

  if (arg1 < 0.0)
  {
                            
    arg1 = -arg1;
  }

  if (arg1 > 180.0)
  {
                               
    arg1 = 360.0 - arg1;
  }

  if (arg1 > 90.0)
  {
                                
    arg1 = 180.0 - arg1;
    sign = -sign;
  }

  result = sign * cosd_q1(arg1);

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                      
   
Datum
dcotd(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;
  volatile float8 cot_arg1;
  int sign = 1;

     
                                                                           
                               
     
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  if (isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  INIT_DEGREE_CONSTANTS();

                                                       
  arg1 = fmod(arg1, 360.0);

  if (arg1 < 0.0)
  {
                             
    arg1 = -arg1;
    sign = -sign;
  }

  if (arg1 > 180.0)
  {
                                
    arg1 = 360.0 - arg1;
    sign = -sign;
  }

  if (arg1 > 90.0)
  {
                                
    arg1 = 180.0 - arg1;
    sign = -sign;
  }

  cot_arg1 = cosd_q1(arg1) / sind_q1(arg1);
  result = sign * (cot_arg1 / cot_45);

     
                                                                           
                                                                        
                                                                        
     
  if (result == 0.0)
  {
    result = 0.0;
  }

                                                        

  PG_RETURN_FLOAT8(result);
}

   
                                                 
   
Datum
dsind(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;
  int sign = 1;

     
                                                                           
                               
     
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  if (isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  INIT_DEGREE_CONSTANTS();

                                                       
  arg1 = fmod(arg1, 360.0);

  if (arg1 < 0.0)
  {
                             
    arg1 = -arg1;
    sign = -sign;
  }

  if (arg1 > 180.0)
  {
                                
    arg1 = 360.0 - arg1;
    sign = -sign;
  }

  if (arg1 > 90.0)
  {
                               
    arg1 = 180.0 - arg1;
  }

  result = sign * sind_q1(arg1);

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                    
   
Datum
dtand(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;
  volatile float8 tan_arg1;
  int sign = 1;

     
                                                                           
                               
     
  if (isnan(arg1))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  if (isinf(arg1))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  INIT_DEGREE_CONSTANTS();

                                                       
  arg1 = fmod(arg1, 360.0);

  if (arg1 < 0.0)
  {
                             
    arg1 = -arg1;
    sign = -sign;
  }

  if (arg1 > 180.0)
  {
                                
    arg1 = 360.0 - arg1;
    sign = -sign;
  }

  if (arg1 > 90.0)
  {
                                
    arg1 = 180.0 - arg1;
    sign = -sign;
  }

  tan_arg1 = sind_q1(arg1) / cosd_q1(arg1);
  result = sign * (tan_arg1 / tan_45);

     
                                                                           
                                                                        
                                                                        
     
  if (result == 0.0)
  {
    result = 0.0;
  }

                                                         

  PG_RETURN_FLOAT8(result);
}

   
                                                      
   
Datum
degrees(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(float8_div(arg1, RADIANS_PER_DEGREE));
}

   
                                     
   
Datum
dpi(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(M_PI);
}

   
                                                      
   
Datum
radians(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);

  PG_RETURN_FLOAT8(float8_mul(arg1, RADIANS_PER_DEGREE));
}

                                                

   
                                                  
   
Datum
dsinh(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  errno = 0;
  result = sinh(arg1);

     
                                                                          
                                                                         
                   
     
  if (errno == ERANGE)
  {
    if (arg1 < 0)
    {
      result = -get_float8_infinity();
    }
    else
    {
      result = get_float8_infinity();
    }
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                    
   
Datum
dcosh(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

  errno = 0;
  result = cosh(arg1);

     
                                                                           
                                                                       
     
  if (errno == ERANGE)
  {
    result = get_float8_infinity();
  }

  if (unlikely(result == 0.0))
  {
    float_underflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                     
   
Datum
dtanh(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

     
                                                                        
     
  result = tanh(arg1);

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                           
   
Datum
dasinh(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

     
                                                                         
     
  result = asinh(arg1);

  PG_RETURN_FLOAT8(result);
}

   
                                                             
   
Datum
dacosh(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

     
                                                                           
                                                                         
                                                                             
                           
     
  if (arg1 < 1.0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

  result = acosh(arg1);

  PG_RETURN_FLOAT8(result);
}

   
                                                              
   
Datum
datanh(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float8 result;

     
                                                                          
                                                                             
                                                                         
     
  if (arg1 < -1.0 || arg1 > 1.0)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("input is out of range")));
  }

     
                                                                           
                                                                            
                              
     
  if (arg1 == -1.0)
  {
    result = -get_float8_infinity();
  }
  else if (arg1 == 1.0)
  {
    result = get_float8_infinity();
  }
  else
  {
    result = atanh(arg1);
  }

  PG_RETURN_FLOAT8(result);
}

   
                                       
   
Datum
drandom(PG_FUNCTION_ARGS)
{
  float8 result;

                                                               
  if (unlikely(!drandom_seed_set))
  {
       
                                                                        
                                                                         
                                           
       
    if (!pg_strong_random(drandom_seed, sizeof(drandom_seed)))
    {
      TimestampTz now = GetCurrentTimestamp();
      uint64 iseed;

                                                                       
      iseed = (uint64)now ^ ((uint64)MyProcPid << 32);
      drandom_seed[0] = (unsigned short)iseed;
      drandom_seed[1] = (unsigned short)(iseed >> 16);
      drandom_seed[2] = (unsigned short)(iseed >> 32);
    }
    drandom_seed_set = true;
  }

                                                            
  result = pg_erand48(drandom_seed);

  PG_RETURN_FLOAT8(result);
}

   
                                                        
   
Datum
setseed(PG_FUNCTION_ARGS)
{
  float8 seed = PG_GETARG_FLOAT8(0);
  uint64 iseed;

  if (seed < -1 || seed > 1 || isnan(seed))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("setseed parameter %g is out of allowed range [-1,1]", seed)));
  }

                                                                
  iseed = (int64)(seed * (float8)UINT64CONST(0x7FFFFFFFFFFF));
  drandom_seed[0] = (unsigned short)iseed;
  drandom_seed[1] = (unsigned short)(iseed >> 16);
  drandom_seed[2] = (unsigned short)(iseed >> 32);
  drandom_seed_set = true;

  PG_RETURN_VOID();
}

   
                              
                              
                              
   
                                                                    
                                                   
                                                        
                                                                 
                                                               
                                                                      
                                                                    
   
                                                                    
                                                                          
                                                                         
                                                                        
                                                                               
                
   
                                                                              
                                                                              
                                                                             
                                                                             
                                                                            
                                                                         
                                                                             
                                        
   
                                                                         
                                                           
   
                                                                          
                                                                             
                                                                        
                                                     
   
                                                                             
                                                                           
   
                                                                       
                                                                      
   
                                                                       
                                                                     
                                                                       
   

static float8 *
check_float8_array(ArrayType *transarray, const char *caller, int n)
{
     
                                                                         
                                                                        
                                                      
     
  if (ARR_NDIM(transarray) != 1 || ARR_DIMS(transarray)[0] != n || ARR_HASNULL(transarray) || ARR_ELEMTYPE(transarray) != FLOAT8OID)
  {
    elog(ERROR, "%s: expected %d-element float8 array", caller, n);
  }
  return (float8 *)ARR_DATA_PTR(transarray);
}

   
                  
   
                                                              
                                                            
                                                           
                                                  
   
Datum
float8_combine(PG_FUNCTION_ARGS)
{
  ArrayType *transarray1 = PG_GETARG_ARRAYTYPE_P(0);
  ArrayType *transarray2 = PG_GETARG_ARRAYTYPE_P(1);
  float8 *transvalues1;
  float8 *transvalues2;
  float8 N1, Sx1, Sxx1, N2, Sx2, Sxx2, tmp, N, Sx, Sxx;

  transvalues1 = check_float8_array(transarray1, "float8_combine", 3);
  transvalues2 = check_float8_array(transarray2, "float8_combine", 3);

  N1 = transvalues1[0];
  Sx1 = transvalues1[1];
  Sxx1 = transvalues1[2];

  N2 = transvalues2[0];
  Sx2 = transvalues2[1];
  Sxx2 = transvalues2[2];

                         
                                                                 
                                         
     
                 
                    
                                                            
     
                                                                        
                                                                          
                                                  
                         
     
  if (N1 == 0.0)
  {
    N = N2;
    Sx = Sx2;
    Sxx = Sxx2;
  }
  else if (N2 == 0.0)
  {
    N = N1;
    Sx = Sx1;
    Sxx = Sxx1;
  }
  else
  {
    N = N1 + N2;
    Sx = float8_pl(Sx1, Sx2);
    tmp = Sx1 / N1 - Sx2 / N2;
    Sxx = Sxx1 + Sxx2 + N1 * N2 * tmp * tmp / N;
    if (unlikely(isinf(Sxx)) && !isinf(Sxx1) && !isinf(Sxx2))
    {
      float_overflow_error();
    }
  }

     
                                                                         
                                                                            
                                                               
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transvalues1[0] = N;
    transvalues1[1] = Sx;
    transvalues1[2] = Sxx;

    PG_RETURN_ARRAYTYPE_P(transarray1);
  }
  else
  {
    Datum transdatums[3];
    ArrayType *result;

    transdatums[0] = Float8GetDatumFast(N);
    transdatums[1] = Float8GetDatumFast(Sx);
    transdatums[2] = Float8GetDatumFast(Sxx);

    result = construct_array(transdatums, 3, FLOAT8OID, sizeof(float8), FLOAT8PASSBYVAL, 'd');

    PG_RETURN_ARRAYTYPE_P(result);
  }
}

Datum
float8_accum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 newval = PG_GETARG_FLOAT8(1);
  float8 *transvalues;
  float8 N, Sx, Sxx, tmp;

  transvalues = check_float8_array(transarray, "float8_accum", 3);
  N = transvalues[0];
  Sx = transvalues[1];
  Sxx = transvalues[2];

     
                                                                           
                        
     
  N += 1.0;
  Sx += newval;
  if (transvalues[0] > 0.0)
  {
    tmp = newval * N - Sx;
    Sxx += tmp * tmp / (N * transvalues[0]);

       
                                                                     
                                                                          
                                                                          
                               
       
    if (isinf(Sx) || isinf(Sxx))
    {
      if (!isinf(transvalues[1]) && !isinf(newval))
      {
        float_overflow_error();
      }

      Sxx = get_float8_nan();
    }
  }
  else
  {
       
                                                                        
                                                                    
                                                                        
                    
       
    if (isnan(newval) || isinf(newval))
    {
      Sxx = get_float8_nan();
    }
  }

     
                                                                         
                                                                            
                                                               
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transvalues[0] = N;
    transvalues[1] = Sx;
    transvalues[2] = Sxx;

    PG_RETURN_ARRAYTYPE_P(transarray);
  }
  else
  {
    Datum transdatums[3];
    ArrayType *result;

    transdatums[0] = Float8GetDatumFast(N);
    transdatums[1] = Float8GetDatumFast(Sx);
    transdatums[2] = Float8GetDatumFast(Sxx);

    result = construct_array(transdatums, 3, FLOAT8OID, sizeof(float8), FLOAT8PASSBYVAL, 'd');

    PG_RETURN_ARRAYTYPE_P(result);
  }
}

Datum
float4_accum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);

                                 
  float8 newval = PG_GETARG_FLOAT4(1);
  float8 *transvalues;
  float8 N, Sx, Sxx, tmp;

  transvalues = check_float8_array(transarray, "float4_accum", 3);
  N = transvalues[0];
  Sx = transvalues[1];
  Sxx = transvalues[2];

     
                                                                           
                        
     
  N += 1.0;
  Sx += newval;
  if (transvalues[0] > 0.0)
  {
    tmp = newval * N - Sx;
    Sxx += tmp * tmp / (N * transvalues[0]);

       
                                                                     
                                                                          
                                                                          
                               
       
    if (isinf(Sx) || isinf(Sxx))
    {
      if (!isinf(transvalues[1]) && !isinf(newval))
      {
        float_overflow_error();
      }

      Sxx = get_float8_nan();
    }
  }
  else
  {
       
                                                                        
                                                                    
                                                                        
                    
       
    if (isnan(newval) || isinf(newval))
    {
      Sxx = get_float8_nan();
    }
  }

     
                                                                         
                                                                            
                                                               
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transvalues[0] = N;
    transvalues[1] = Sx;
    transvalues[2] = Sxx;

    PG_RETURN_ARRAYTYPE_P(transarray);
  }
  else
  {
    Datum transdatums[3];
    ArrayType *result;

    transdatums[0] = Float8GetDatumFast(N);
    transdatums[1] = Float8GetDatumFast(Sx);
    transdatums[2] = Float8GetDatumFast(Sxx);

    result = construct_array(transdatums, 3, FLOAT8OID, sizeof(float8), FLOAT8PASSBYVAL, 'd');

    PG_RETURN_ARRAYTYPE_P(result);
  }
}

Datum
float8_avg(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sx;

  transvalues = check_float8_array(transarray, "float8_avg", 3);
  N = transvalues[0];
  Sx = transvalues[1];
                  

                                               
  if (N == 0.0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sx / N);
}

Datum
float8_var_pop(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx;

  transvalues = check_float8_array(transarray, "float8_var_pop", 3);
  N = transvalues[0];
                 
  Sxx = transvalues[2];

                                                                    
  if (N == 0.0)
  {
    PG_RETURN_NULL();
  }

                                                      

  PG_RETURN_FLOAT8(Sxx / N);
}

Datum
float8_var_samp(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx;

  transvalues = check_float8_array(transarray, "float8_var_samp", 3);
  N = transvalues[0];
                 
  Sxx = transvalues[2];

                                                                     
  if (N <= 1.0)
  {
    PG_RETURN_NULL();
  }

                                                      

  PG_RETURN_FLOAT8(Sxx / (N - 1.0));
}

Datum
float8_stddev_pop(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx;

  transvalues = check_float8_array(transarray, "float8_stddev_pop", 3);
  N = transvalues[0];
                 
  Sxx = transvalues[2];

                                                                  
  if (N == 0.0)
  {
    PG_RETURN_NULL();
  }

                                                      

  PG_RETURN_FLOAT8(sqrt(Sxx / N));
}

Datum
float8_stddev_samp(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx;

  transvalues = check_float8_array(transarray, "float8_stddev_samp", 3);
  N = transvalues[0];
                 
  Sxx = transvalues[2];

                                                                   
  if (N <= 1.0)
  {
    PG_RETURN_NULL();
  }

                                                      

  PG_RETURN_FLOAT8(sqrt(Sxx / (N - 1.0)));
}

   
                              
                              
                              
   
                                                                           
                                                            
   
                                                                            
                                                                            
                                                                  
   
                                                              
   
                                                                            
                                                                        
                                                                        
                                                                          
                                   
   

Datum
float8_regr_accum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 newvalY = PG_GETARG_FLOAT8(1);
  float8 newvalX = PG_GETARG_FLOAT8(2);
  float8 *transvalues;
  float8 N, Sx, Sxx, Sy, Syy, Sxy, tmpX, tmpY, scale;

  transvalues = check_float8_array(transarray, "float8_regr_accum", 6);
  N = transvalues[0];
  Sx = transvalues[1];
  Sxx = transvalues[2];
  Sy = transvalues[3];
  Syy = transvalues[4];
  Sxy = transvalues[5];

     
                                                                            
                        
     
  N += 1.0;
  Sx += newvalX;
  Sy += newvalY;
  if (transvalues[0] > 0.0)
  {
    tmpX = newvalX * N - Sx;
    tmpY = newvalY * N - Sy;
    scale = 1.0 / (N * transvalues[0]);
    Sxx += tmpX * tmpX * scale;
    Syy += tmpY * tmpY * scale;
    Sxy += tmpX * tmpY * scale;

       
                                                                     
                                                                         
                                                                       
                                                          
       
    if (isinf(Sx) || isinf(Sxx) || isinf(Sy) || isinf(Syy) || isinf(Sxy))
    {
      if (((isinf(Sx) || isinf(Sxx)) && !isinf(transvalues[1]) && !isinf(newvalX)) || ((isinf(Sy) || isinf(Syy)) && !isinf(transvalues[3]) && !isinf(newvalY)) || (isinf(Sxy) && !isinf(transvalues[1]) && !isinf(newvalX) && !isinf(transvalues[3]) && !isinf(newvalY)))
      {
        float_overflow_error();
      }

      if (isinf(Sxx))
      {
        Sxx = get_float8_nan();
      }
      if (isinf(Syy))
      {
        Syy = get_float8_nan();
      }
      if (isinf(Sxy))
      {
        Sxy = get_float8_nan();
      }
    }
  }
  else
  {
       
                                                                           
                                                                         
                                                                        
                                 
       
    if (isnan(newvalX) || isinf(newvalX))
    {
      Sxx = Sxy = get_float8_nan();
    }
    if (isnan(newvalY) || isinf(newvalY))
    {
      Syy = Sxy = get_float8_nan();
    }
  }

     
                                                                         
                                                                            
                                                               
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transvalues[0] = N;
    transvalues[1] = Sx;
    transvalues[2] = Sxx;
    transvalues[3] = Sy;
    transvalues[4] = Syy;
    transvalues[5] = Sxy;

    PG_RETURN_ARRAYTYPE_P(transarray);
  }
  else
  {
    Datum transdatums[6];
    ArrayType *result;

    transdatums[0] = Float8GetDatumFast(N);
    transdatums[1] = Float8GetDatumFast(Sx);
    transdatums[2] = Float8GetDatumFast(Sxx);
    transdatums[3] = Float8GetDatumFast(Sy);
    transdatums[4] = Float8GetDatumFast(Syy);
    transdatums[5] = Float8GetDatumFast(Sxy);

    result = construct_array(transdatums, 6, FLOAT8OID, sizeof(float8), FLOAT8PASSBYVAL, 'd');

    PG_RETURN_ARRAYTYPE_P(result);
  }
}

   
                       
   
                                                              
                                                            
                                                           
                                                  
   
Datum
float8_regr_combine(PG_FUNCTION_ARGS)
{
  ArrayType *transarray1 = PG_GETARG_ARRAYTYPE_P(0);
  ArrayType *transarray2 = PG_GETARG_ARRAYTYPE_P(1);
  float8 *transvalues1;
  float8 *transvalues2;
  float8 N1, Sx1, Sxx1, Sy1, Syy1, Sxy1, N2, Sx2, Sxx2, Sy2, Syy2, Sxy2, tmp1, tmp2, N, Sx, Sxx, Sy, Syy, Sxy;

  transvalues1 = check_float8_array(transarray1, "float8_regr_combine", 6);
  transvalues2 = check_float8_array(transarray2, "float8_regr_combine", 6);

  N1 = transvalues1[0];
  Sx1 = transvalues1[1];
  Sxx1 = transvalues1[2];
  Sy1 = transvalues1[3];
  Syy1 = transvalues1[4];
  Sxy1 = transvalues1[5];

  N2 = transvalues2[0];
  Sx2 = transvalues2[1];
  Sxx2 = transvalues2[2];
  Sy2 = transvalues2[3];
  Syy2 = transvalues2[4];
  Sxy2 = transvalues2[5];

                         
                                                                 
                                         
     
                 
                    
                                                           
                    
                                                           
                                                                             
     
                                                                        
                                                                          
                                                  
                         
     
  if (N1 == 0.0)
  {
    N = N2;
    Sx = Sx2;
    Sxx = Sxx2;
    Sy = Sy2;
    Syy = Syy2;
    Sxy = Sxy2;
  }
  else if (N2 == 0.0)
  {
    N = N1;
    Sx = Sx1;
    Sxx = Sxx1;
    Sy = Sy1;
    Syy = Syy1;
    Sxy = Sxy1;
  }
  else
  {
    N = N1 + N2;
    Sx = float8_pl(Sx1, Sx2);
    tmp1 = Sx1 / N1 - Sx2 / N2;
    Sxx = Sxx1 + Sxx2 + N1 * N2 * tmp1 * tmp1 / N;
    if (unlikely(isinf(Sxx)) && !isinf(Sxx1) && !isinf(Sxx2))
    {
      float_overflow_error();
    }
    Sy = float8_pl(Sy1, Sy2);
    tmp2 = Sy1 / N1 - Sy2 / N2;
    Syy = Syy1 + Syy2 + N1 * N2 * tmp2 * tmp2 / N;
    if (unlikely(isinf(Syy)) && !isinf(Syy1) && !isinf(Syy2))
    {
      float_overflow_error();
    }
    Sxy = Sxy1 + Sxy2 + N1 * N2 * tmp1 * tmp2 / N;
    if (unlikely(isinf(Sxy)) && !isinf(Sxy1) && !isinf(Sxy2))
    {
      float_overflow_error();
    }
  }

     
                                                                         
                                                                            
                                                               
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transvalues1[0] = N;
    transvalues1[1] = Sx;
    transvalues1[2] = Sxx;
    transvalues1[3] = Sy;
    transvalues1[4] = Syy;
    transvalues1[5] = Sxy;

    PG_RETURN_ARRAYTYPE_P(transarray1);
  }
  else
  {
    Datum transdatums[6];
    ArrayType *result;

    transdatums[0] = Float8GetDatumFast(N);
    transdatums[1] = Float8GetDatumFast(Sx);
    transdatums[2] = Float8GetDatumFast(Sxx);
    transdatums[3] = Float8GetDatumFast(Sy);
    transdatums[4] = Float8GetDatumFast(Syy);
    transdatums[5] = Float8GetDatumFast(Sxy);

    result = construct_array(transdatums, 6, FLOAT8OID, sizeof(float8), FLOAT8PASSBYVAL, 'd');

    PG_RETURN_ARRAYTYPE_P(result);
  }
}

Datum
float8_regr_sxx(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx;

  transvalues = check_float8_array(transarray, "float8_regr_sxx", 6);
  N = transvalues[0];
  Sxx = transvalues[2];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                                      

  PG_RETURN_FLOAT8(Sxx);
}

Datum
float8_regr_syy(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Syy;

  transvalues = check_float8_array(transarray, "float8_regr_syy", 6);
  N = transvalues[0];
  Syy = transvalues[4];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                                      

  PG_RETURN_FLOAT8(Syy);
}

Datum
float8_regr_sxy(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxy;

  transvalues = check_float8_array(transarray, "float8_regr_sxy", 6);
  N = transvalues[0];
  Sxy = transvalues[5];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                       

  PG_RETURN_FLOAT8(Sxy);
}

Datum
float8_regr_avgx(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sx;

  transvalues = check_float8_array(transarray, "float8_regr_avgx", 6);
  N = transvalues[0];
  Sx = transvalues[1];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sx / N);
}

Datum
float8_regr_avgy(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sy;

  transvalues = check_float8_array(transarray, "float8_regr_avgy", 6);
  N = transvalues[0];
  Sy = transvalues[3];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sy / N);
}

Datum
float8_covar_pop(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxy;

  transvalues = check_float8_array(transarray, "float8_covar_pop", 6);
  N = transvalues[0];
  Sxy = transvalues[5];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sxy / N);
}

Datum
float8_covar_samp(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxy;

  transvalues = check_float8_array(transarray, "float8_covar_samp", 6);
  N = transvalues[0];
  Sxy = transvalues[5];

                                          
  if (N < 2.0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sxy / (N - 1.0));
}

Datum
float8_corr(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx, Syy, Sxy;

  transvalues = check_float8_array(transarray, "float8_corr", 6);
  N = transvalues[0];
  Sxx = transvalues[2];
  Syy = transvalues[4];
  Sxy = transvalues[5];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                                               

                                                               
  if (Sxx == 0 || Syy == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sxy / sqrt(Sxx * Syy));
}

Datum
float8_regr_r2(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx, Syy, Sxy;

  transvalues = check_float8_array(transarray, "float8_regr_r2", 6);
  N = transvalues[0];
  Sxx = transvalues[2];
  Syy = transvalues[4];
  Sxy = transvalues[5];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                                               

                                                 
  if (Sxx == 0)
  {
    PG_RETURN_NULL();
  }

                                                  
  if (Syy == 0)
  {
    PG_RETURN_FLOAT8(1.0);
  }

  PG_RETURN_FLOAT8((Sxy * Sxy) / (Sxx * Syy));
}

Datum
float8_regr_slope(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sxx, Sxy;

  transvalues = check_float8_array(transarray, "float8_regr_slope", 6);
  N = transvalues[0];
  Sxx = transvalues[2];
  Sxy = transvalues[5];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                                      

                                                 
  if (Sxx == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(Sxy / Sxx);
}

Datum
float8_regr_intercept(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  float8 *transvalues;
  float8 N, Sx, Sxx, Sy, Sxy;

  transvalues = check_float8_array(transarray, "float8_regr_intercept", 6);
  N = transvalues[0];
  Sx = transvalues[1];
  Sxx = transvalues[2];
  Sy = transvalues[3];
  Sxy = transvalues[5];

                                       
  if (N < 1.0)
  {
    PG_RETURN_NULL();
  }

                                                      

                                                 
  if (Sxx == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8((Sy - Sx * Sxy / Sxx) / N);
}

   
                                         
                                         
                                         
   

   
                                     
                                     
                                      
                                      
   
Datum
float48pl(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_pl((float8)arg1, arg2));
}

Datum
float48mi(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_mi((float8)arg1, arg2));
}

Datum
float48mul(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_mul((float8)arg1, arg2));
}

Datum
float48div(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_FLOAT8(float8_div((float8)arg1, arg2));
}

   
                                     
                                     
                                      
                                      
   
Datum
float84pl(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT8(float8_pl(arg1, (float8)arg2));
}

Datum
float84mi(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT8(float8_mi(arg1, (float8)arg2));
}

Datum
float84mul(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT8(float8_mul(arg1, (float8)arg2));
}

Datum
float84div(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_FLOAT8(float8_div(arg1, (float8)arg2));
}

   
                         
                         
                         
   

   
                                                                      
   
Datum
float48eq(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_eq((float8)arg1, arg2));
}

Datum
float48ne(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_ne((float8)arg1, arg2));
}

Datum
float48lt(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_lt((float8)arg1, arg2));
}

Datum
float48le(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_le((float8)arg1, arg2));
}

Datum
float48gt(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_gt((float8)arg1, arg2));
}

Datum
float48ge(PG_FUNCTION_ARGS)
{
  float4 arg1 = PG_GETARG_FLOAT4(0);
  float8 arg2 = PG_GETARG_FLOAT8(1);

  PG_RETURN_BOOL(float8_ge((float8)arg1, arg2));
}

   
                                                                      
   
Datum
float84eq(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float8_eq(arg1, (float8)arg2));
}

Datum
float84ne(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float8_ne(arg1, (float8)arg2));
}

Datum
float84lt(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float8_lt(arg1, (float8)arg2));
}

Datum
float84le(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float8_le(arg1, (float8)arg2));
}

Datum
float84gt(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float8_gt(arg1, (float8)arg2));
}

Datum
float84ge(PG_FUNCTION_ARGS)
{
  float8 arg1 = PG_GETARG_FLOAT8(0);
  float4 arg2 = PG_GETARG_FLOAT4(1);

  PG_RETURN_BOOL(float8_ge(arg1, (float8)arg2));
}

   
                                                                
                                                        
   
                                                               
                                                                     
                                                                      
                                                                     
                                                                   
                                                                    
                                                                
                                                                       
                                                                  
   
Datum
width_bucket_float8(PG_FUNCTION_ARGS)
{
  float8 operand = PG_GETARG_FLOAT8(0);
  float8 bound1 = PG_GETARG_FLOAT8(1);
  float8 bound2 = PG_GETARG_FLOAT8(2);
  int32 count = PG_GETARG_INT32(3);
  int32 result;

  if (count <= 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("count must be greater than zero")));
  }

  if (isnan(operand) || isnan(bound1) || isnan(bound2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("operand, lower bound, and upper bound cannot be NaN")));
  }

                                                   
  if (isinf(bound1) || isinf(bound2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("lower and upper bounds must be finite")));
  }

  if (bound1 < bound2)
  {
    if (operand < bound1)
    {
      result = 0;
    }
    else if (operand >= bound2)
    {
      if (pg_add_s32_overflow(count, 1, &result))
      {
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
      }
    }
    else
    {
      result = ((float8)count * (operand - bound1) / (bound2 - bound1)) + 1;
    }
  }
  else if (bound1 > bound2)
  {
    if (operand > bound1)
    {
      result = 0;
    }
    else if (operand <= bound2)
    {
      if (pg_add_s32_overflow(count, 1, &result))
      {
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
      }
    }
    else
    {
      result = ((float8)count * (bound1 - operand) / (bound1 - bound2)) + 1;
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("lower bound cannot equal upper bound")));
    result = 0;                              
  }

  PG_RETURN_INT32(result);
}

                                            

#ifndef HAVE_CBRT

static double
cbrt(double x)
{
  int isneg = (x < 0.0);
  double absx = fabs(x);
  double tmpres = pow(absx, (double)1.0 / (double)3.0);

     
                                                                            
                                                                       
                                                                          
                    
     
  if (tmpres > 0.0)
  {
    tmpres -= (tmpres - absx / (tmpres * tmpres)) / (double)3.0;
  }

  return isneg ? -tmpres : tmpres;
}

#endif                 
