                                                                            
   
                
                                                   
   
                                                                         
                                                                        
   
   
                  
                                            
   
         
   
                                                                     
                                   
   
                     
                      
                       
                      
   
                                                                   
   
                                                                
                                                                    
                                                                     
                                                  
   
                                                                          
                                                                    
                                                                      
                                                                   
                                                                        
                                                                          
                                                                   
                                                                 
                                                                       
                                                                     
                                                                           
   
                                                                     
                                                                      
                                                                  
                                                                         
   
                                                                        
                                                                          
                                                                     
                                                                       
                                                                            
                                                                          
                                                                         
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "utils/builtins.h"
#include "utils/sortsupport.h"

#ifdef STRESS_SORT_INT_MIN
#define A_LESS_THAN_B INT_MIN
#define A_GREATER_THAN_B INT_MAX
#else
#define A_LESS_THAN_B (-1)
#define A_GREATER_THAN_B 1
#endif

Datum
btboolcmp(PG_FUNCTION_ARGS)
{
  bool a = PG_GETARG_BOOL(0);
  bool b = PG_GETARG_BOOL(1);

  PG_RETURN_INT32((int32)a - (int32)b);
}

Datum
btint2cmp(PG_FUNCTION_ARGS)
{
  int16 a = PG_GETARG_INT16(0);
  int16 b = PG_GETARG_INT16(1);

  PG_RETURN_INT32((int32)a - (int32)b);
}

static int
btint2fastcmp(Datum x, Datum y, SortSupport ssup)
{
  int16 a = DatumGetInt16(x);
  int16 b = DatumGetInt16(y);

  return (int)a - (int)b;
}

Datum
btint2sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = btint2fastcmp;
  PG_RETURN_VOID();
}

Datum
btint4cmp(PG_FUNCTION_ARGS)
{
  int32 a = PG_GETARG_INT32(0);
  int32 b = PG_GETARG_INT32(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

static int
btint4fastcmp(Datum x, Datum y, SortSupport ssup)
{
  int32 a = DatumGetInt32(x);
  int32 b = DatumGetInt32(y);

  if (a > b)
  {
    return A_GREATER_THAN_B;
  }
  else if (a == b)
  {
    return 0;
  }
  else
  {
    return A_LESS_THAN_B;
  }
}

Datum
btint4sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = btint4fastcmp;
  PG_RETURN_VOID();
}

Datum
btint8cmp(PG_FUNCTION_ARGS)
{
  int64 a = PG_GETARG_INT64(0);
  int64 b = PG_GETARG_INT64(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

static int
btint8fastcmp(Datum x, Datum y, SortSupport ssup)
{
  int64 a = DatumGetInt64(x);
  int64 b = DatumGetInt64(y);

  if (a > b)
  {
    return A_GREATER_THAN_B;
  }
  else if (a == b)
  {
    return 0;
  }
  else
  {
    return A_LESS_THAN_B;
  }
}

Datum
btint8sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = btint8fastcmp;
  PG_RETURN_VOID();
}

Datum
btint48cmp(PG_FUNCTION_ARGS)
{
  int32 a = PG_GETARG_INT32(0);
  int64 b = PG_GETARG_INT64(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

Datum
btint84cmp(PG_FUNCTION_ARGS)
{
  int64 a = PG_GETARG_INT64(0);
  int32 b = PG_GETARG_INT32(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

Datum
btint24cmp(PG_FUNCTION_ARGS)
{
  int16 a = PG_GETARG_INT16(0);
  int32 b = PG_GETARG_INT32(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

Datum
btint42cmp(PG_FUNCTION_ARGS)
{
  int32 a = PG_GETARG_INT32(0);
  int16 b = PG_GETARG_INT16(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

Datum
btint28cmp(PG_FUNCTION_ARGS)
{
  int16 a = PG_GETARG_INT16(0);
  int64 b = PG_GETARG_INT64(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

Datum
btint82cmp(PG_FUNCTION_ARGS)
{
  int64 a = PG_GETARG_INT64(0);
  int16 b = PG_GETARG_INT16(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

Datum
btoidcmp(PG_FUNCTION_ARGS)
{
  Oid a = PG_GETARG_OID(0);
  Oid b = PG_GETARG_OID(1);

  if (a > b)
  {
    PG_RETURN_INT32(A_GREATER_THAN_B);
  }
  else if (a == b)
  {
    PG_RETURN_INT32(0);
  }
  else
  {
    PG_RETURN_INT32(A_LESS_THAN_B);
  }
}

static int
btoidfastcmp(Datum x, Datum y, SortSupport ssup)
{
  Oid a = DatumGetObjectId(x);
  Oid b = DatumGetObjectId(y);

  if (a > b)
  {
    return A_GREATER_THAN_B;
  }
  else if (a == b)
  {
    return 0;
  }
  else
  {
    return A_LESS_THAN_B;
  }
}

Datum
btoidsortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = btoidfastcmp;
  PG_RETURN_VOID();
}

Datum
btoidvectorcmp(PG_FUNCTION_ARGS)
{
  oidvector *a = (oidvector *)PG_GETARG_POINTER(0);
  oidvector *b = (oidvector *)PG_GETARG_POINTER(1);
  int i;

                                                            
  if (a->dim1 != b->dim1)
  {
    PG_RETURN_INT32(a->dim1 - b->dim1);
  }

  for (i = 0; i < a->dim1; i++)
  {
    if (a->values[i] != b->values[i])
    {
      if (a->values[i] > b->values[i])
      {
        PG_RETURN_INT32(A_GREATER_THAN_B);
      }
      else
      {
        PG_RETURN_INT32(A_LESS_THAN_B);
      }
    }
  }
  PG_RETURN_INT32(0);
}

Datum
btcharcmp(PG_FUNCTION_ARGS)
{
  char a = PG_GETARG_CHAR(0);
  char b = PG_GETARG_CHAR(1);

                                               
  PG_RETURN_INT32((int32)((uint8)a) - (int32)((uint8)b));
}
