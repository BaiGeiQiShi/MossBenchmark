   
                          
   
                                                                                
                                                                          
                                                                           
                                                                  
                                                                               

#include "postgres.h"

#include "fmgr.h"
#include "libpq/pqformat.h"                                     

PG_MODULE_MAGIC;

typedef struct Complex
{
  double x;
  double y;
} Complex;

                                                                               
                          
                                                                               

PG_FUNCTION_INFO_V1(complex_in);

Datum
complex_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  double x, y;
  Complex *result;

  if (sscanf(str, " ( %lf , %lf )", &x, &y) != 2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "complex", str)));
  }

  result = (Complex *)palloc(sizeof(Complex));
  result->x = x;
  result->y = y;
  PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(complex_out);

Datum
complex_out(PG_FUNCTION_ARGS)
{
  Complex *complex = (Complex *)PG_GETARG_POINTER(0);
  char *result;

  result = psprintf("(%g,%g)", complex->x, complex->y);
  PG_RETURN_CSTRING(result);
}

                                                                               
                                 
   
                       
                                                                               

PG_FUNCTION_INFO_V1(complex_recv);

Datum
complex_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  Complex *result;

  result = (Complex *)palloc(sizeof(Complex));
  result->x = pq_getmsgfloat8(buf);
  result->y = pq_getmsgfloat8(buf);
  PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(complex_send);

Datum
complex_send(PG_FUNCTION_ARGS)
{
  Complex *complex = (Complex *)PG_GETARG_POINTER(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, complex->x);
  pq_sendfloat8(&buf, complex->y);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                                               
                 
   
                                                                              
                                                                               

PG_FUNCTION_INFO_V1(complex_add);

Datum
complex_add(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);
  Complex *result;

  result = (Complex *)palloc(sizeof(Complex));
  result->x = a->x + b->x;
  result->y = a->y + b->y;
  PG_RETURN_POINTER(result);
}

                                                                               
                                            
   
                                                                           
                                                                         
                                                                           
                                                                          
                                                                        
                                                             
                                                                               

#define Mag(c) ((c)->x * (c)->x + (c)->y * (c)->y)

static int
complex_abs_cmp_internal(Complex *a, Complex *b)
{
  double amag = Mag(a), bmag = Mag(b);

  if (amag < bmag)
  {
    return -1;
  }
  if (amag > bmag)
  {
    return 1;
  }
  return 0;
}

PG_FUNCTION_INFO_V1(complex_abs_lt);

Datum
complex_abs_lt(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);

  PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) < 0);
}

PG_FUNCTION_INFO_V1(complex_abs_le);

Datum
complex_abs_le(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);

  PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(complex_abs_eq);

Datum
complex_abs_eq(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);

  PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) == 0);
}

PG_FUNCTION_INFO_V1(complex_abs_ge);

Datum
complex_abs_ge(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);

  PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(complex_abs_gt);

Datum
complex_abs_gt(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);

  PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) > 0);
}

PG_FUNCTION_INFO_V1(complex_abs_cmp);

Datum
complex_abs_cmp(PG_FUNCTION_ARGS)
{
  Complex *a = (Complex *)PG_GETARG_POINTER(0);
  Complex *b = (Complex *)PG_GETARG_POINTER(1);

  PG_RETURN_INT32(complex_abs_cmp_internal(a, b));
}
