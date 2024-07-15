                                                                            
   
          
                                             
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "libpq/pqformat.h"
#include "utils/builtins.h"

   
                                                                     
                                                                      
                                                       
                                                                
   
bool
parse_bool(const char *value, bool *result)
{
  return parse_bool_with_len(value, strlen(value), result);
}

bool
parse_bool_with_len(const char *value, size_t len, bool *result)
{
  switch (*value)
  {
  case 't':
  case 'T':
    if (pg_strncasecmp(value, "true", len) == 0)
    {
      if (result)
      {
        *result = true;
      }
      return true;
    }
    break;
  case 'f':
  case 'F':
    if (pg_strncasecmp(value, "false", len) == 0)
    {
      if (result)
      {
        *result = false;
      }
      return true;
    }
    break;
  case 'y':
  case 'Y':
    if (pg_strncasecmp(value, "yes", len) == 0)
    {
      if (result)
      {
        *result = true;
      }
      return true;
    }
    break;
  case 'n':
  case 'N':
    if (pg_strncasecmp(value, "no", len) == 0)
    {
      if (result)
      {
        *result = false;
      }
      return true;
    }
    break;
  case 'o':
  case 'O':
                                  
    if (pg_strncasecmp(value, "on", (len > 2 ? len : 2)) == 0)
    {
      if (result)
      {
        *result = true;
      }
      return true;
    }
    else if (pg_strncasecmp(value, "off", (len > 2 ? len : 2)) == 0)
    {
      if (result)
      {
        *result = false;
      }
      return true;
    }
    break;
  case '1':
    if (len == 1)
    {
      if (result)
      {
        *result = true;
      }
      return true;
    }
    break;
  case '0':
    if (len == 1)
    {
      if (result)
      {
        *result = false;
      }
      return true;
    }
    break;
  default:
    break;
  }

  if (result)
  {
    *result = false;                                
  }
  return false;
}

                                                                               
                                      
                                                                               

   
                                             
   
                                                                          
                        
   
                                                                     
   
Datum
boolin(PG_FUNCTION_ARGS)
{
  const char *in_str = PG_GETARG_CSTRING(0);
  const char *str;
  size_t len;
  bool result;

     
                                          
     
  str = in_str;
  while (isspace((unsigned char)*str))
  {
    str++;
  }

  len = strlen(str);
  while (len > 0 && isspace((unsigned char)str[len - 1]))
  {
    len--;
  }

  if (parse_bool_with_len(str, len, &result))
  {
    PG_RETURN_BOOL(result);
  }

  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "boolean", in_str)));

                   
  PG_RETURN_BOOL(false);
}

   
                                              
   
Datum
boolout(PG_FUNCTION_ARGS)
{
  bool b = PG_GETARG_BOOL(0);
  char *result = (char *)palloc(2);

  result[0] = (b) ? 't' : 'f';
  result[1] = '\0';
  PG_RETURN_CSTRING(result);
}

   
                                                         
   
                                                                        
              
   
Datum
boolrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  int ext;

  ext = pq_getmsgbyte(buf);
  PG_RETURN_BOOL((ext != 0) ? true : false);
}

   
                                                
   
Datum
boolsend(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendbyte(&buf, arg1 ? 1 : 0);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                
   
                                                                       
                                                                               
   
Datum
booltext(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  const char *str;

  if (arg1)
  {
    str = "true";
  }
  else
  {
    str = "false";
  }

  PG_RETURN_TEXT_P(cstring_to_text(str));
}

                                                                               
                                    
                                                                               

Datum
booleq(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  bool arg2 = PG_GETARG_BOOL(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
boolne(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  bool arg2 = PG_GETARG_BOOL(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
boollt(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  bool arg2 = PG_GETARG_BOOL(1);

  PG_RETURN_BOOL(arg1 < arg2);
}

Datum
boolgt(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  bool arg2 = PG_GETARG_BOOL(1);

  PG_RETURN_BOOL(arg1 > arg2);
}

Datum
boolle(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  bool arg2 = PG_GETARG_BOOL(1);

  PG_RETURN_BOOL(arg1 <= arg2);
}

Datum
boolge(PG_FUNCTION_ARGS)
{
  bool arg1 = PG_GETARG_BOOL(0);
  bool arg2 = PG_GETARG_BOOL(1);

  PG_RETURN_BOOL(arg1 >= arg2);
}

   
                                          
   

   
                                                                 
                                                         
   
                                                                               
   
Datum
booland_statefunc(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(PG_GETARG_BOOL(0) && PG_GETARG_BOOL(1));
}

   
                                                                    
                                                                            
   
                                                                               
   
Datum
boolor_statefunc(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(PG_GETARG_BOOL(0) || PG_GETARG_BOOL(1));
}

typedef struct BoolAggState
{
  int64 aggcount;                                           
  int64 aggtrue;                                                 
} BoolAggState;

static BoolAggState *
makeBoolAggState(FunctionCallInfo fcinfo)
{
  BoolAggState *state;
  MemoryContext agg_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state = (BoolAggState *)MemoryContextAlloc(agg_context, sizeof(BoolAggState));
  state->aggcount = 0;
  state->aggtrue = 0;

  return state;
}

Datum
bool_accum(PG_FUNCTION_ARGS)
{
  BoolAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (BoolAggState *)PG_GETARG_POINTER(0);

                                           
  if (state == NULL)
  {
    state = makeBoolAggState(fcinfo);
  }

  if (!PG_ARGISNULL(1))
  {
    state->aggcount++;
    if (PG_GETARG_BOOL(1))
    {
      state->aggtrue++;
    }
  }

  PG_RETURN_POINTER(state);
}

Datum
bool_accum_inv(PG_FUNCTION_ARGS)
{
  BoolAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (BoolAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL)
  {
    elog(ERROR, "bool_accum_inv called with NULL state");
  }

  if (!PG_ARGISNULL(1))
  {
    state->aggcount--;
    if (PG_GETARG_BOOL(1))
    {
      state->aggtrue--;
    }
  }

  PG_RETURN_POINTER(state);
}

Datum
bool_alltrue(PG_FUNCTION_ARGS)
{
  BoolAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (BoolAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL || state->aggcount == 0)
  {
    PG_RETURN_NULL();
  }

                                            
  PG_RETURN_BOOL(state->aggtrue == state->aggcount);
}

Datum
bool_anytrue(PG_FUNCTION_ARGS)
{
  BoolAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (BoolAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL || state->aggcount == 0)
  {
    PG_RETURN_NULL();
  }

                                          
  PG_RETURN_BOOL(state->aggtrue > 0);
}
