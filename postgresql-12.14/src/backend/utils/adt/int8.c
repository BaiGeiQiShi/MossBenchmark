                                                                            
   
          
                                        
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "common/int.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/optimizer.h"
#include "utils/int8.h"
#include "utils/builtins.h"

#define MAXINT8LEN 25

typedef struct
{
  int64 current;
  int64 finish;
  int64 step;
} generate_series_fctx;

                                                                         
    
                                   
    
                                                                         

                                                             
                                       
                                                             

   
                                                    
   
                                                                             
                                                          
   
bool
scanint8(const char *str, bool errorOK, int64 *result)
{
  const char *ptr = str;
  int64 tmp = 0;
  bool neg = false;

     
                                                                          
                    
     
                                                                           
                                 
     

                           
  while (*ptr && isspace((unsigned char)*ptr))
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

    if (unlikely(pg_mul_s64_overflow(tmp, 10, &tmp)) || unlikely(pg_sub_s64_overflow(tmp, digit, &tmp)))
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
                                                     
    if (unlikely(tmp == PG_INT64_MIN))
    {
      goto out_of_range;
    }
    tmp = -tmp;
  }

  *result = tmp;
  return true;

out_of_range:
  if (!errorOK)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", str, "bigint")));
  }
  return false;

invalid_syntax:
  if (!errorOK)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "bigint", str)));
  }
  return false;
}

            
   
Datum
int8in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  int64 result;

  (void)scanint8(str, false, &result);
  PG_RETURN_INT64(result);
}

             
   
Datum
int8out(PG_FUNCTION_ARGS)
{
  int64 val = PG_GETARG_INT64(0);
  char buf[MAXINT8LEN + 1];
  char *result;

  pg_lltoa(val, buf);
  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                         
   
Datum
int8recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_INT64(pq_getmsgint64(buf));
}

   
                                                
   
Datum
int8send(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                             
                                                                          
                                                             

               
                       
   
Datum
int8eq(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 == val2);
}

Datum
int8ne(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 != val2);
}

Datum
int8lt(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 < val2);
}

Datum
int8gt(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 > val2);
}

Datum
int8le(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 <= val2);
}

Datum
int8ge(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 >= val2);
}

                
                                     
   
Datum
int84eq(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int32 val2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(val1 == val2);
}

Datum
int84ne(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int32 val2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(val1 != val2);
}

Datum
int84lt(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int32 val2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(val1 < val2);
}

Datum
int84gt(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int32 val2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(val1 > val2);
}

Datum
int84le(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int32 val2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(val1 <= val2);
}

Datum
int84ge(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int32 val2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(val1 >= val2);
}

                
                                     
   
Datum
int48eq(PG_FUNCTION_ARGS)
{
  int32 val1 = PG_GETARG_INT32(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 == val2);
}

Datum
int48ne(PG_FUNCTION_ARGS)
{
  int32 val1 = PG_GETARG_INT32(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 != val2);
}

Datum
int48lt(PG_FUNCTION_ARGS)
{
  int32 val1 = PG_GETARG_INT32(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 < val2);
}

Datum
int48gt(PG_FUNCTION_ARGS)
{
  int32 val1 = PG_GETARG_INT32(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 > val2);
}

Datum
int48le(PG_FUNCTION_ARGS)
{
  int32 val1 = PG_GETARG_INT32(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 <= val2);
}

Datum
int48ge(PG_FUNCTION_ARGS)
{
  int32 val1 = PG_GETARG_INT32(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 >= val2);
}

                
                                     
   
Datum
int82eq(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int16 val2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(val1 == val2);
}

Datum
int82ne(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int16 val2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(val1 != val2);
}

Datum
int82lt(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int16 val2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(val1 < val2);
}

Datum
int82gt(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int16 val2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(val1 > val2);
}

Datum
int82le(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int16 val2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(val1 <= val2);
}

Datum
int82ge(PG_FUNCTION_ARGS)
{
  int64 val1 = PG_GETARG_INT64(0);
  int16 val2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(val1 >= val2);
}

                
                                     
   
Datum
int28eq(PG_FUNCTION_ARGS)
{
  int16 val1 = PG_GETARG_INT16(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 == val2);
}

Datum
int28ne(PG_FUNCTION_ARGS)
{
  int16 val1 = PG_GETARG_INT16(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 != val2);
}

Datum
int28lt(PG_FUNCTION_ARGS)
{
  int16 val1 = PG_GETARG_INT16(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 < val2);
}

Datum
int28gt(PG_FUNCTION_ARGS)
{
  int16 val1 = PG_GETARG_INT16(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 > val2);
}

Datum
int28le(PG_FUNCTION_ARGS)
{
  int16 val1 = PG_GETARG_INT16(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 <= val2);
}

Datum
int28ge(PG_FUNCTION_ARGS)
{
  int16 val1 = PG_GETARG_INT16(0);
  int64 val2 = PG_GETARG_INT64(1);

  PG_RETURN_BOOL(val1 >= val2);
}

   
                                       
   
                                                                        
                                                                            
   
Datum
in_range_int8_int8(PG_FUNCTION_ARGS)
{
  int64 val = PG_GETARG_INT64(0);
  int64 base = PG_GETARG_INT64(1);
  int64 offset = PG_GETARG_INT64(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  int64 sum;

  if (offset < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

  if (sub)
  {
    offset = -offset;                      
  }

  if (unlikely(pg_add_s64_overflow(base, offset, &sum)))
  {
       
                                                                         
                                                                      
                                                       
       
    PG_RETURN_BOOL(sub ? !less : less);
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
int8um(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);
  int64 result;

  if (unlikely(arg == PG_INT64_MIN))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  result = -arg;
  PG_RETURN_INT64(result);
}

Datum
int8up(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);

  PG_RETURN_INT64(arg);
}

Datum
int8pl(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_add_s64_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int8mi(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_sub_s64_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int8mul(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_mul_s64_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int8div(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (arg2 == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                             
                                                                        
                                                                         
                                                              
     
  if (arg2 == -1)
  {
    if (unlikely(arg1 == PG_INT64_MIN))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }
    result = -arg1;
    PG_RETURN_INT64(result);
  }

                               

  result = arg1 / arg2;

  PG_RETURN_INT64(result);
}

             
                  
   
Datum
int8abs(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 result;

  if (unlikely(arg1 == PG_INT64_MIN))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  result = (arg1 < 0) ? -arg1 : arg1;
  PG_RETURN_INT64(result);
}

             
                     
   
Datum
int8mod(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                        
                                                                
                                
     
  if (arg2 == -1)
  {
    PG_RETURN_INT64(0);
  }

                               

  PG_RETURN_INT64(arg1 % arg2);
}

Datum
int8inc(PG_FUNCTION_ARGS)
{
     
                                                                           
                                                                            
                                                                           
                                                                          
                                       
     
#ifndef USE_FLOAT8_BYVAL                        
  if (AggCheckCallContext(fcinfo, NULL))
  {
    int64 *arg = (int64 *)PG_GETARG_POINTER(0);

    if (unlikely(pg_add_s64_overflow(*arg, 1, arg)))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }

    PG_RETURN_POINTER(arg);
  }
  else
#endif
  {
                                                                
    int64 arg = PG_GETARG_INT64(0);
    int64 result;

    if (unlikely(pg_add_s64_overflow(arg, 1, &result)))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }

    PG_RETURN_INT64(result);
  }
}

Datum
int8dec(PG_FUNCTION_ARGS)
{
     
                                                                           
                                                                            
                                                                           
                                                                          
                                       
     
#ifndef USE_FLOAT8_BYVAL                        
  if (AggCheckCallContext(fcinfo, NULL))
  {
    int64 *arg = (int64 *)PG_GETARG_POINTER(0);

    if (unlikely(pg_sub_s64_overflow(*arg, 1, arg)))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }
    PG_RETURN_POINTER(arg);
  }
  else
#endif
  {
                                                                
    int64 arg = PG_GETARG_INT64(0);
    int64 result;

    if (unlikely(pg_sub_s64_overflow(arg, 1, &result)))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }

    PG_RETURN_INT64(result);
  }
}

   
                                                                     
                                                                        
                                                                               
                                                                               
                                                                              
                                                                         
   

Datum
int8inc_any(PG_FUNCTION_ARGS)
{
  return int8inc(fcinfo);
}

Datum
int8inc_float8_float8(PG_FUNCTION_ARGS)
{
  return int8inc(fcinfo);
}

Datum
int8dec_any(PG_FUNCTION_ARGS)
{
  return int8dec(fcinfo);
}

Datum
int8larger(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  result = ((arg1 > arg2) ? arg1 : arg2);

  PG_RETURN_INT64(result);
}

Datum
int8smaller(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  result = ((arg1 < arg2) ? arg1 : arg2);

  PG_RETURN_INT64(result);
}

Datum
int84pl(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int64 result;

  if (unlikely(pg_add_s64_overflow(arg1, (int64)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int84mi(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int64 result;

  if (unlikely(pg_sub_s64_overflow(arg1, (int64)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int84mul(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int64 result;

  if (unlikely(pg_mul_s64_overflow(arg1, (int64)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int84div(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int64 result;

  if (arg2 == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                             
                                                                        
                                                                         
                                                              
     
  if (arg2 == -1)
  {
    if (unlikely(arg1 == PG_INT64_MIN))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }
    result = -arg1;
    PG_RETURN_INT64(result);
  }

                               

  result = arg1 / arg2;

  PG_RETURN_INT64(result);
}

Datum
int48pl(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_add_s64_overflow((int64)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int48mi(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_sub_s64_overflow((int64)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int48mul(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_mul_s64_overflow((int64)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int48div(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int64 arg2 = PG_GETARG_INT64(1);

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

                               
  PG_RETURN_INT64((int64)arg1 / arg2);
}

Datum
int82pl(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int64 result;

  if (unlikely(pg_add_s64_overflow(arg1, (int64)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int82mi(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int64 result;

  if (unlikely(pg_sub_s64_overflow(arg1, (int64)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int82mul(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int64 result;

  if (unlikely(pg_mul_s64_overflow(arg1, (int64)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int82div(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int64 result;

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                             
                                                                        
                                                                         
                                                              
     
  if (arg2 == -1)
  {
    if (unlikely(arg1 == PG_INT64_MIN))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
    }
    result = -arg1;
    PG_RETURN_INT64(result);
  }

                               

  result = arg1 / arg2;

  PG_RETURN_INT64(result);
}

Datum
int28pl(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_add_s64_overflow((int64)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int28mi(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_sub_s64_overflow((int64)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int28mul(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int64 arg2 = PG_GETARG_INT64(1);
  int64 result;

  if (unlikely(pg_mul_s64_overflow((int64)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }
  PG_RETURN_INT64(result);
}

Datum
int28div(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int64 arg2 = PG_GETARG_INT64(1);

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

                               
  PG_RETURN_INT64((int64)arg1 / arg2);
}

                      
   
                                   
                                  
                                   
                             
                                    
                                    
   

Datum
int8and(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);

  PG_RETURN_INT64(arg1 & arg2);
}

Datum
int8or(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);

  PG_RETURN_INT64(arg1 | arg2);
}

Datum
int8xor(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int64 arg2 = PG_GETARG_INT64(1);

  PG_RETURN_INT64(arg1 ^ arg2);
}

Datum
int8not(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);

  PG_RETURN_INT64(~arg1);
}

Datum
int8shl(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT64(arg1 << arg2);
}

Datum
int8shr(PG_FUNCTION_ARGS)
{
  int64 arg1 = PG_GETARG_INT64(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT64(arg1 >> arg2);
}

                                                             
                         
                                                             

Datum
int48(PG_FUNCTION_ARGS)
{
  int32 arg = PG_GETARG_INT32(0);

  PG_RETURN_INT64((int64)arg);
}

Datum
int84(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);

  if (unlikely(arg < PG_INT32_MIN) || unlikely(arg > PG_INT32_MAX))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  PG_RETURN_INT32((int32)arg);
}

Datum
int28(PG_FUNCTION_ARGS)
{
  int16 arg = PG_GETARG_INT16(0);

  PG_RETURN_INT64((int64)arg);
}

Datum
int82(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);

  if (unlikely(arg < PG_INT16_MIN) || unlikely(arg > PG_INT16_MAX))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

  PG_RETURN_INT16((int16)arg);
}

Datum
i8tod(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);
  float8 result;

  result = arg;

  PG_RETURN_FLOAT8(result);
}

           
                                     
   
Datum
dtoi8(PG_FUNCTION_ARGS)
{
  float8 num = PG_GETARG_FLOAT8(0);

     
                                                                            
                                                                    
                                                                      
     
  num = rint(num);

                   
  if (unlikely(isnan(num) || !FLOAT8_FITS_IN_INT64(num)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }

  PG_RETURN_INT64((int64)num);
}

Datum
i8tof(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);
  float4 result;

  result = arg;

  PG_RETURN_FLOAT4(result);
}

           
                                     
   
Datum
ftoi8(PG_FUNCTION_ARGS)
{
  float4 num = PG_GETARG_FLOAT4(0);

     
                                                                            
                                                                    
                                                                      
     
  num = rint(num);

                   
  if (unlikely(isnan(num) || !FLOAT4_FITS_IN_INT64(num)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }

  PG_RETURN_INT64((int64)num);
}

Datum
i8tooid(PG_FUNCTION_ARGS)
{
  int64 arg = PG_GETARG_INT64(0);

  if (unlikely(arg < 0) || unlikely(arg > PG_UINT32_MAX))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("OID out of range")));
  }

  PG_RETURN_OID((Oid)arg);
}

Datum
oidtoi8(PG_FUNCTION_ARGS)
{
  Oid arg = PG_GETARG_OID(0);

  PG_RETURN_INT64((int64)arg);
}

   
                                           
   
Datum
generate_series_int8(PG_FUNCTION_ARGS)
{
  return generate_series_step_int8(fcinfo);
}

Datum
generate_series_step_int8(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  generate_series_fctx *fctx;
  int64 result;
  MemoryContext oldcontext;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    int64 start = PG_GETARG_INT64(0);
    int64 finish = PG_GETARG_INT64(1);
    int64 step = 1;

                                                    
    if (PG_NARGS() == 3)
    {
      step = PG_GETARG_INT64(2);
    }
    if (step == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("step size cannot equal zero")));
    }

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                          
    fctx = (generate_series_fctx *)palloc(sizeof(generate_series_fctx));

       
                                                                       
                            
       
    fctx->current = start;
    fctx->finish = finish;
    fctx->step = step;

    funcctx->user_fctx = fctx;
    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();

     
                                                                          
     
  fctx = funcctx->user_fctx;
  result = fctx->current;

  if ((fctx->step > 0 && fctx->current <= fctx->finish) || (fctx->step < 0 && fctx->current >= fctx->finish))
  {
       
                                                                          
                                                        
       
    if (pg_add_s64_overflow(fctx->current, fctx->step, &fctx->current))
    {
      fctx->step = 0;
    }

                                            
    SRF_RETURN_NEXT(funcctx, Int64GetDatum(result));
  }
  else
  {
                                       
    SRF_RETURN_DONE(funcctx);
  }
}

   
                                                                     
   
Datum
generate_series_int8_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestRows))
  {
                                                     
    SupportRequestRows *req = (SupportRequestRows *)rawreq;

    if (is_funcclause(req->node))                  
    {
      List *args = ((FuncExpr *)req->node)->args;
      Node *arg1, *arg2, *arg3;

                                                     
      arg1 = estimate_expression_value(req->root, linitial(args));
      arg2 = estimate_expression_value(req->root, lsecond(args));
      if (list_length(args) >= 3)
      {
        arg3 = estimate_expression_value(req->root, lthird(args));
      }
      else
      {
        arg3 = NULL;
      }

         
                                                                     
                                                                     
                                                                     
                                                                     
         
      if ((IsA(arg1, Const) && ((Const *)arg1)->constisnull) || (IsA(arg2, Const) && ((Const *)arg2)->constisnull) || (arg3 != NULL && IsA(arg3, Const) && ((Const *)arg3)->constisnull))
      {
        req->rows = 0;
        ret = (Node *)req;
      }
      else if (IsA(arg1, Const) && IsA(arg2, Const) && (arg3 == NULL || IsA(arg3, Const)))
      {
        double start, finish, step;

        start = DatumGetInt64(((Const *)arg1)->constvalue);
        finish = DatumGetInt64(((Const *)arg2)->constvalue);
        step = arg3 ? DatumGetInt64(((Const *)arg3)->constvalue) : 1;

                                                         
        if (step != 0)
        {
          req->rows = floor((finish - start + step) / step);
          ret = (Node *)req;
        }
      }
    }
  }

  PG_RETURN_POINTER(ret);
}
