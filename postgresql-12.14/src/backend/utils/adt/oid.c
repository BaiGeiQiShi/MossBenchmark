                                                                            
   
         
                                                             
   
                                                                         
                                                                        
   
   
                  
                                 
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <limits.h>

#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "nodes/value.h"
#include "utils/array.h"
#include "utils/builtins.h"

#define OidVectorSize(n) (offsetof(oidvector, values) + (n) * sizeof(Oid))

                                                                               
                                      
                                                                               

static Oid
oidin_subr(const char *s, char **endloc)
{
  unsigned long cvt;
  char *endptr;
  Oid result;

  if (*s == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "oid", s)));
  }

  errno = 0;
  cvt = strtoul(s, &endptr, 10);

     
                                                                           
                                                                            
                                                             
     
  if (errno && errno != ERANGE && errno != EINVAL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "oid", s)));
  }

  if (endptr == s && *s != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "oid", s)));
  }

  if (errno == ERANGE)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", s, "oid")));
  }

  if (endloc)
  {
                                                  
    *endloc = endptr;
  }
  else
  {
                                            
    while (*endptr && isspace((unsigned char)*endptr))
    {
      endptr++;
    }
    if (*endptr)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "oid", s)));
    }
  }

  result = (Oid)cvt;

     
                                                                          
                                                                          
                       
     
                                                                          
                                                                            
                                           
     
                                                                            
                                                                        
     
#if OID_MAX != ULONG_MAX
  if (cvt != (unsigned long)result && cvt != (unsigned long)((int)result))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value \"%s\" is out of range for type %s", s, "oid")));
  }
#endif

  return result;
}

Datum
oidin(PG_FUNCTION_ARGS)
{
  char *s = PG_GETARG_CSTRING(0);
  Oid result;

  result = oidin_subr(s, NULL);
  PG_RETURN_OID(result);
}

Datum
oidout(PG_FUNCTION_ARGS)
{
  Oid o = PG_GETARG_OID(0);
  char *result = (char *)palloc(12);

  snprintf(result, 12, "%u", o);
  PG_RETURN_CSTRING(result);
}

   
                                                       
   
Datum
oidrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_OID((Oid)pq_getmsgint(buf, sizeof(Oid)));
}

   
                                              
   
Datum
oidsend(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                 
   
                                                            
   
oidvector *
buildoidvector(const Oid *oids, int n)
{
  oidvector *result;

  result = (oidvector *)palloc0(OidVectorSize(n));

  if (n > 0 && oids)
  {
    memcpy(result->values, oids, n * sizeof(Oid));
  }

     
                                                                             
                             
     
  SET_VARSIZE(result, OidVectorSize(n));
  result->ndim = 1;
  result->dataoffset = 0;                      
  result->elemtype = OIDOID;
  result->dim1 = n;
  result->lbound1 = 0;

  return result;
}

   
                                                            
   
Datum
oidvectorin(PG_FUNCTION_ARGS)
{
  char *oidString = PG_GETARG_CSTRING(0);
  oidvector *result;
  int n;

  result = (oidvector *)palloc0(OidVectorSize(FUNC_MAX_ARGS));

  for (n = 0; n < FUNC_MAX_ARGS; n++)
  {
    while (*oidString && isspace((unsigned char)*oidString))
    {
      oidString++;
    }
    if (*oidString == '\0')
    {
      break;
    }
    result->values[n] = oidin_subr(oidString, &oidString);
  }
  while (*oidString && isspace((unsigned char)*oidString))
  {
    oidString++;
  }
  if (*oidString)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("oidvector has too many elements")));
  }

  SET_VARSIZE(result, OidVectorSize(n));
  result->ndim = 1;
  result->dataoffset = 0;                      
  result->elemtype = OIDOID;
  result->dim1 = n;
  result->lbound1 = 0;

  PG_RETURN_POINTER(result);
}

   
                                                           
   
Datum
oidvectorout(PG_FUNCTION_ARGS)
{
  oidvector *oidArray = (oidvector *)PG_GETARG_POINTER(0);
  int num, nnums = oidArray->dim1;
  char *rp;
  char *result;

                                    
  rp = result = (char *)palloc(nnums * 12 + 1);
  for (num = 0; num < nnums; num++)
  {
    if (num != 0)
    {
      *rp++ = ' ';
    }
    sprintf(rp, "%u", oidArray->values[num]);
    while (*++rp != '\0')
      ;
  }
  *rp = '\0';
  PG_RETURN_CSTRING(result);
}

   
                                                                   
   
Datum
oidvectorrecv(PG_FUNCTION_ARGS)
{
  LOCAL_FCINFO(locfcinfo, 3);
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  oidvector *result;

     
                                                                         
                                                                        
                                                                     
                
     
  InitFunctionCallInfoData(*locfcinfo, fcinfo->flinfo, 3, InvalidOid, NULL, NULL);

  locfcinfo->args[0].value = PointerGetDatum(buf);
  locfcinfo->args[0].isnull = false;
  locfcinfo->args[1].value = ObjectIdGetDatum(OIDOID);
  locfcinfo->args[1].isnull = false;
  locfcinfo->args[2].value = Int32GetDatum(-1);
  locfcinfo->args[2].isnull = false;

  result = (oidvector *)DatumGetPointer(array_recv(locfcinfo));

  Assert(!locfcinfo->isnull);

                                                               
  if (ARR_NDIM(result) != 1 || ARR_HASNULL(result) || ARR_ELEMTYPE(result) != OIDOID || ARR_LBOUND(result)[0] != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid oidvector data")));
  }

                                                       
  if (ARR_DIMS(result)[0] > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("oidvector has too many elements")));
  }

  PG_RETURN_POINTER(result);
}

   
                                                          
   
Datum
oidvectorsend(PG_FUNCTION_ARGS)
{
  return array_send(fcinfo);
}

   
                                                  
   
Oid
oidparse(Node *node)
{
  switch (nodeTag(node))
  {
  case T_Integer:
    return intVal(node);
  case T_Float:

       
                                                              
                                                                   
                
       
    return oidin_subr(strVal(node), NULL);
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(node));
  }
  return InvalidOid;                          
}

                                        
int
oid_cmp(const void *p1, const void *p2)
{
  Oid v1 = *((const Oid *)p1);
  Oid v2 = *((const Oid *)p2);

  if (v1 < v2)
  {
    return -1;
  }
  if (v1 > v2)
  {
    return 1;
  }
  return 0;
}

                                                                               
                                    
                                                                               

Datum
oideq(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
oidne(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
oidlt(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_BOOL(arg1 < arg2);
}

Datum
oidle(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_BOOL(arg1 <= arg2);
}

Datum
oidge(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_BOOL(arg1 >= arg2);
}

Datum
oidgt(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_BOOL(arg1 > arg2);
}

Datum
oidlarger(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_OID((arg1 > arg2) ? arg1 : arg2);
}

Datum
oidsmaller(PG_FUNCTION_ARGS)
{
  Oid arg1 = PG_GETARG_OID(0);
  Oid arg2 = PG_GETARG_OID(1);

  PG_RETURN_OID((arg1 < arg2) ? arg1 : arg2);
}

Datum
oidvectoreq(PG_FUNCTION_ARGS)
{
  int32 cmp = DatumGetInt32(btoidvectorcmp(fcinfo));

  PG_RETURN_BOOL(cmp == 0);
}

Datum
oidvectorne(PG_FUNCTION_ARGS)
{
  int32 cmp = DatumGetInt32(btoidvectorcmp(fcinfo));

  PG_RETURN_BOOL(cmp != 0);
}

Datum
oidvectorlt(PG_FUNCTION_ARGS)
{
  int32 cmp = DatumGetInt32(btoidvectorcmp(fcinfo));

  PG_RETURN_BOOL(cmp < 0);
}

Datum
oidvectorle(PG_FUNCTION_ARGS)
{
  int32 cmp = DatumGetInt32(btoidvectorcmp(fcinfo));

  PG_RETURN_BOOL(cmp <= 0);
}

Datum
oidvectorge(PG_FUNCTION_ARGS)
{
  int32 cmp = DatumGetInt32(btoidvectorcmp(fcinfo));

  PG_RETURN_BOOL(cmp >= 0);
}

Datum
oidvectorgt(PG_FUNCTION_ARGS)
{
  int32 cmp = DatumGetInt32(btoidvectorcmp(fcinfo));

  PG_RETURN_BOOL(cmp > 0);
}
