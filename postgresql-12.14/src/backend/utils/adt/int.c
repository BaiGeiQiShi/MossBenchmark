                                                                            
   
         
                                                             
   
                                                                         
                                                                        
   
   
                  
                                 
   
                                                                            
   
   
                
                  
                                         
                                         
                                                                 
                       
                                              
                          
                                   
   
                          
            
   
#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "catalog/pg_type.h"
#include "common/int.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/optimizer.h"
#include "utils/array.h"
#include "utils/builtins.h"

#define Int2VectorSize(n) (offsetof(int2vector, values) + (n) * sizeof(int16))

typedef struct
{
  int32 current;
  int32 finish;
  int32 step;
} generate_series_fctx;

                                                                               
                                      
                                                                               

   
                                       
   
Datum
int2in(PG_FUNCTION_ARGS)
{
  char *num = PG_GETARG_CSTRING(0);

  PG_RETURN_INT16(pg_strtoint16(num));
}

   
                                        
   
Datum
int2out(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  char *result = (char *)palloc(7);                           

  pg_itoa(arg1, result);
  PG_RETURN_CSTRING(result);
}

   
                                                         
   
Datum
int2recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_INT16((int16)pq_getmsgint(buf, sizeof(int16)));
}

   
                                                
   
Datum
int2send(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint16(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                   
   
                                                             
   
int2vector *
buildint2vector(const int16 *int2s, int n)
{
  int2vector *result;

  result = (int2vector *)palloc0(Int2VectorSize(n));

  if (n > 0 && int2s)
  {
    memcpy(result->values, int2s, n * sizeof(int16));
  }

     
                                                                             
                             
     
  SET_VARSIZE(result, Int2VectorSize(n));
  result->ndim = 1;
  result->dataoffset = 0;                      
  result->elemtype = INT2OID;
  result->dim1 = n;
  result->lbound1 = 0;

  return result;
}

   
                                                             
   
Datum
int2vectorin(PG_FUNCTION_ARGS)
{
  char *intString = PG_GETARG_CSTRING(0);
  int2vector *result;
  int n;

  result = (int2vector *)palloc0(Int2VectorSize(FUNC_MAX_ARGS));

  for (n = 0; *intString && n < FUNC_MAX_ARGS; n++)
  {
    while (*intString && isspace((unsigned char)*intString))
    {
      intString++;
    }
    if (*intString == '\0')
    {
      break;
    }
    result->values[n] = pg_atoi(intString, sizeof(int16), ' ');
    while (*intString && !isspace((unsigned char)*intString))
    {
      intString++;
    }
  }
  while (*intString && isspace((unsigned char)*intString))
  {
    intString++;
  }
  if (*intString)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("int2vector has too many elements")));
  }

  SET_VARSIZE(result, Int2VectorSize(n));
  result->ndim = 1;
  result->dataoffset = 0;                      
  result->elemtype = INT2OID;
  result->dim1 = n;
  result->lbound1 = 0;

  PG_RETURN_POINTER(result);
}

   
                                                             
   
Datum
int2vectorout(PG_FUNCTION_ARGS)
{
  int2vector *int2Array = (int2vector *)PG_GETARG_POINTER(0);
  int num, nnums = int2Array->dim1;
  char *rp;
  char *result;

                                   
  rp = result = (char *)palloc(nnums * 7 + 1);
  for (num = 0; num < nnums; num++)
  {
    if (num != 0)
    {
      *rp++ = ' ';
    }
    pg_itoa(int2Array->values[num], rp);
    while (*++rp != '\0')
      ;
  }
  *rp = '\0';
  PG_RETURN_CSTRING(result);
}

   
                                                                     
   
Datum
int2vectorrecv(PG_FUNCTION_ARGS)
{
  LOCAL_FCINFO(locfcinfo, 3);
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  int2vector *result;

     
                                                                         
                                                                        
                                                                     
                
     
  InitFunctionCallInfoData(*locfcinfo, fcinfo->flinfo, 3, InvalidOid, NULL, NULL);

  locfcinfo->args[0].value = PointerGetDatum(buf);
  locfcinfo->args[0].isnull = false;
  locfcinfo->args[1].value = ObjectIdGetDatum(INT2OID);
  locfcinfo->args[1].isnull = false;
  locfcinfo->args[2].value = Int32GetDatum(-1);
  locfcinfo->args[2].isnull = false;

  result = (int2vector *)DatumGetPointer(array_recv(locfcinfo));

  Assert(!locfcinfo->isnull);

                                                                
  if (ARR_NDIM(result) != 1 || ARR_HASNULL(result) || ARR_ELEMTYPE(result) != INT2OID || ARR_LBOUND(result)[0] != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid int2vector data")));
  }

                                                        
  if (ARR_DIMS(result)[0] > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("oidvector has too many elements")));
  }

  PG_RETURN_POINTER(result);
}

   
                                                            
   
Datum
int2vectorsend(PG_FUNCTION_ARGS)
{
  return array_send(fcinfo);
}

                                                                               
                                    
                                                                               

   
                                      
   
Datum
int4in(PG_FUNCTION_ARGS)
{
  char *num = PG_GETARG_CSTRING(0);

  PG_RETURN_INT32(pg_strtoint32(num));
}

   
                                       
   
Datum
int4out(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  char *result = (char *)palloc(12);                            

  pg_ltoa(arg1, result);
  PG_RETURN_CSTRING(result);
}

   
                                                         
   
Datum
int4recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_INT32((int32)pq_getmsgint(buf, sizeof(int32)));
}

   
                                                
   
Datum
int4send(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                        
                        
                        
   

Datum
i2toi4(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);

  PG_RETURN_INT32((int32)arg1);
}

Datum
i4toi2(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);

  if (unlikely(arg1 < SHRT_MIN) || unlikely(arg1 > SHRT_MAX))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

  PG_RETURN_INT16((int16)arg1);
}

                       
Datum
int4_bool(PG_FUNCTION_ARGS)
{
  if (PG_GETARG_INT32(0) == 0)
  {
    PG_RETURN_BOOL(false);
  }
  else
  {
    PG_RETURN_BOOL(true);
  }
}

                       
Datum
bool_int4(PG_FUNCTION_ARGS)
{
  if (PG_GETARG_BOOL(0) == false)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(1);
  }
}

   
                                 
                                 
                                 
   

   
                                         
                                         
                                        
                                         
                                        
                                         
   

Datum
int4eq(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
int4ne(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
int4lt(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 < arg2);
}

Datum
int4le(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 <= arg2);
}

Datum
int4gt(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 > arg2);
}

Datum
int4ge(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 >= arg2);
}

Datum
int2eq(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
int2ne(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
int2lt(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 < arg2);
}

Datum
int2le(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 <= arg2);
}

Datum
int2gt(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 > arg2);
}

Datum
int2ge(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 >= arg2);
}

Datum
int24eq(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
int24ne(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
int24lt(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 < arg2);
}

Datum
int24le(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 <= arg2);
}

Datum
int24gt(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 > arg2);
}

Datum
int24ge(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_BOOL(arg1 >= arg2);
}

Datum
int42eq(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
int42ne(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
int42lt(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 < arg2);
}

Datum
int42le(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 <= arg2);
}

Datum
int42gt(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 > arg2);
}

Datum
int42ge(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_BOOL(arg1 >= arg2);
}

                                                             
                                         
                                          
   
                                                                 
                                                                     
                                                                      
                                                                     
                                                             

Datum
in_range_int4_int4(PG_FUNCTION_ARGS)
{
  int32 val = PG_GETARG_INT32(0);
  int32 base = PG_GETARG_INT32(1);
  int32 offset = PG_GETARG_INT32(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  int32 sum;

  if (offset < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

  if (sub)
  {
    offset = -offset;                      
  }

  if (unlikely(pg_add_s32_overflow(base, offset, &sum)))
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
in_range_int4_int2(PG_FUNCTION_ARGS)
{
                                                                         
  return DirectFunctionCall5(in_range_int4_int4, PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), Int32GetDatum((int32)PG_GETARG_INT16(2)), PG_GETARG_DATUM(3), PG_GETARG_DATUM(4));
}

Datum
in_range_int4_int8(PG_FUNCTION_ARGS)
{
                                        
  int64 val = (int64)PG_GETARG_INT32(0);
  int64 base = (int64)PG_GETARG_INT32(1);
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
in_range_int2_int4(PG_FUNCTION_ARGS)
{
                                        
  int32 val = (int32)PG_GETARG_INT16(0);
  int32 base = (int32)PG_GETARG_INT16(1);
  int32 offset = PG_GETARG_INT32(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  int32 sum;

  if (offset < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

  if (sub)
  {
    offset = -offset;                      
  }

  if (unlikely(pg_add_s32_overflow(base, offset, &sum)))
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
in_range_int2_int2(PG_FUNCTION_ARGS)
{
                                                                         
  return DirectFunctionCall5(in_range_int2_int4, PG_GETARG_DATUM(0), PG_GETARG_DATUM(1), Int32GetDatum((int32)PG_GETARG_INT16(2)), PG_GETARG_DATUM(3), PG_GETARG_DATUM(4));
}

Datum
in_range_int2_int8(PG_FUNCTION_ARGS)
{
                                                                         
  return DirectFunctionCall5(in_range_int4_int8, Int32GetDatum((int32)PG_GETARG_INT16(0)), Int32GetDatum((int32)PG_GETARG_INT16(1)), PG_GETARG_DATUM(2), PG_GETARG_DATUM(3), PG_GETARG_DATUM(4));
}

   
                                     
                                     
                                      
                                      
   

Datum
int4um(PG_FUNCTION_ARGS)
{
  int32 arg = PG_GETARG_INT32(0);

  if (unlikely(arg == PG_INT32_MIN))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(-arg);
}

Datum
int4up(PG_FUNCTION_ARGS)
{
  int32 arg = PG_GETARG_INT32(0);

  PG_RETURN_INT32(arg);
}

Datum
int4pl(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (unlikely(pg_add_s32_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int4mi(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (unlikely(pg_sub_s32_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int4mul(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (unlikely(pg_mul_s32_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int4div(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (arg2 == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                             
                                                                            
                                                                             
                                                  
     
  if (arg2 == -1)
  {
    if (unlikely(arg1 == PG_INT32_MIN))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
    }
    result = -arg1;
    PG_RETURN_INT32(result);
  }

                               

  result = arg1 / arg2;

  PG_RETURN_INT32(result);
}

Datum
int4inc(PG_FUNCTION_ARGS)
{
  int32 arg = PG_GETARG_INT32(0);
  int32 result;

  if (unlikely(pg_add_s32_overflow(arg, 1, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  PG_RETURN_INT32(result);
}

Datum
int2um(PG_FUNCTION_ARGS)
{
  int16 arg = PG_GETARG_INT16(0);

  if (unlikely(arg == PG_INT16_MIN))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }
  PG_RETURN_INT16(-arg);
}

Datum
int2up(PG_FUNCTION_ARGS)
{
  int16 arg = PG_GETARG_INT16(0);

  PG_RETURN_INT16(arg);
}

Datum
int2pl(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int16 result;

  if (unlikely(pg_add_s16_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }
  PG_RETURN_INT16(result);
}

Datum
int2mi(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int16 result;

  if (unlikely(pg_sub_s16_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }
  PG_RETURN_INT16(result);
}

Datum
int2mul(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int16 result;

  if (unlikely(pg_mul_s16_overflow(arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

  PG_RETURN_INT16(result);
}

Datum
int2div(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int16 result;

  if (arg2 == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                            
                                                                       
                                                                         
                                                              
     
  if (arg2 == -1)
  {
    if (unlikely(arg1 == PG_INT16_MIN))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
    }
    result = -arg1;
    PG_RETURN_INT16(result);
  }

                               

  result = arg1 / arg2;

  PG_RETURN_INT16(result);
}

Datum
int24pl(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (unlikely(pg_add_s32_overflow((int32)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int24mi(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (unlikely(pg_sub_s32_overflow((int32)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int24mul(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);
  int32 result;

  if (unlikely(pg_mul_s32_overflow((int32)arg1, arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int24div(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

                               
  PG_RETURN_INT32((int32)arg1 / arg2);
}

Datum
int42pl(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int32 result;

  if (unlikely(pg_add_s32_overflow(arg1, (int32)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int42mi(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int32 result;

  if (unlikely(pg_sub_s32_overflow(arg1, (int32)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int42mul(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int32 result;

  if (unlikely(pg_mul_s32_overflow(arg1, (int32)arg2, &result)))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  PG_RETURN_INT32(result);
}

Datum
int42div(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int16 arg2 = PG_GETARG_INT16(1);
  int32 result;

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                             
                                                                            
                                                                             
                                                  
     
  if (arg2 == -1)
  {
    if (unlikely(arg1 == PG_INT32_MIN))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
    }
    result = -arg1;
    PG_RETURN_INT32(result);
  }

                               

  result = arg1 / arg2;

  PG_RETURN_INT32(result);
}

Datum
int4mod(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                            
                                                                        
                  
     
  if (arg2 == -1)
  {
    PG_RETURN_INT32(0);
  }

                               

  PG_RETURN_INT32(arg1 % arg2);
}

Datum
int2mod(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  if (unlikely(arg2 == 0))
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
                                                                          
    PG_RETURN_NULL();
  }

     
                                                                            
                                                                        
                                                                       
                                                            
     
  if (arg2 == -1)
  {
    PG_RETURN_INT16(0);
  }

                               

  PG_RETURN_INT16(arg1 % arg2);
}

                
                  
   
Datum
int4abs(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 result;

  if (unlikely(arg1 == PG_INT32_MIN))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }
  result = (arg1 < 0) ? -arg1 : arg1;
  PG_RETURN_INT32(result);
}

Datum
int2abs(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 result;

  if (unlikely(arg1 == PG_INT16_MIN))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }
  result = (arg1 < 0) ? -arg1 : arg1;
  PG_RETURN_INT16(result);
}

Datum
int2larger(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_INT16((arg1 > arg2) ? arg1 : arg2);
}

Datum
int2smaller(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_INT16((arg1 < arg2) ? arg1 : arg2);
}

Datum
int4larger(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32((arg1 > arg2) ? arg1 : arg2);
}

Datum
int4smaller(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32((arg1 < arg2) ? arg1 : arg2);
}

   
                         
   
                                      
                                     
                                      
                                
                                       
                                       
   

Datum
int4and(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32(arg1 & arg2);
}

Datum
int4or(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32(arg1 | arg2);
}

Datum
int4xor(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32(arg1 ^ arg2);
}

Datum
int4shl(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32(arg1 << arg2);
}

Datum
int4shr(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT32(arg1 >> arg2);
}

Datum
int4not(PG_FUNCTION_ARGS)
{
  int32 arg1 = PG_GETARG_INT32(0);

  PG_RETURN_INT32(~arg1);
}

Datum
int2and(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_INT16(arg1 & arg2);
}

Datum
int2or(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_INT16(arg1 | arg2);
}

Datum
int2xor(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int16 arg2 = PG_GETARG_INT16(1);

  PG_RETURN_INT16(arg1 ^ arg2);
}

Datum
int2not(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);

  PG_RETURN_INT16(~arg1);
}

Datum
int2shl(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT16(arg1 << arg2);
}

Datum
int2shr(PG_FUNCTION_ARGS)
{
  int16 arg1 = PG_GETARG_INT16(0);
  int32 arg2 = PG_GETARG_INT32(1);

  PG_RETURN_INT16(arg1 >> arg2);
}

   
                                           
   
Datum
generate_series_int4(PG_FUNCTION_ARGS)
{
  return generate_series_step_int4(fcinfo);
}

Datum
generate_series_step_int4(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  generate_series_fctx *fctx;
  int32 result;
  MemoryContext oldcontext;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    int32 start = PG_GETARG_INT32(0);
    int32 finish = PG_GETARG_INT32(1);
    int32 step = 1;

                                                    
    if (PG_NARGS() == 3)
    {
      step = PG_GETARG_INT32(2);
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
       
                                                                          
                                                        
       
    if (pg_add_s32_overflow(fctx->current, fctx->step, &fctx->current))
    {
      fctx->step = 0;
    }

                                            
    SRF_RETURN_NEXT(funcctx, Int32GetDatum(result));
  }
  else
  {
                                       
    SRF_RETURN_DONE(funcctx);
  }
}

   
                                                                     
   
Datum
generate_series_int4_support(PG_FUNCTION_ARGS)
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

        start = DatumGetInt32(((Const *)arg1)->constvalue);
        finish = DatumGetInt32(((Const *)arg2)->constvalue);
        step = arg3 ? DatumGetInt32(((Const *)arg3)->constvalue) : 1;

                                                         
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
