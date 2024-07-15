                                                                            
   
          
                                             
   
                                                                
                                               
                                            
                                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include "catalog/namespace.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/varlena.h"

                                                                               
                                           
                                                                               

   
                                                       
   
          
                                                                            
                                  
   
Datum
namein(PG_FUNCTION_ARGS)
{
  char *s = PG_GETARG_CSTRING(0);
  Name result;
  int len;

  len = strlen(s);

                               
  if (len >= NAMEDATALEN)
  {
    len = pg_mbcliplen(s, len, NAMEDATALEN - 1);
  }

                                                           
  result = (Name)palloc0(NAMEDATALEN);
  memcpy(NameStr(*result), s, len);

  PG_RETURN_NAME(result);
}

   
                                                        
   
Datum
nameout(PG_FUNCTION_ARGS)
{
  Name s = PG_GETARG_NAME(0);

  PG_RETURN_CSTRING(pstrdup(NameStr(*s)));
}

   
                                                         
   
Datum
namerecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  Name result;
  char *str;
  int nbytes;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
  if (nbytes >= NAMEDATALEN)
  {
    ereport(ERROR, (errcode(ERRCODE_NAME_TOO_LONG), errmsg("identifier too long"), errdetail("Identifier must be less than %d characters.", NAMEDATALEN)));
  }
  result = (NameData *)palloc0(NAMEDATALEN);
  memcpy(result, str, nbytes);
  pfree(str);
  PG_RETURN_NAME(result);
}

   
                                                
   
Datum
namesend(PG_FUNCTION_ARGS)
{
  Name s = PG_GETARG_NAME(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendtext(&buf, NameStr(*s), strlen(NameStr(*s)));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                                               
                                             
                                                                               

   
                                               
                                                   
                                 
                                  
                                 
                                  
   
                                                                             
                                                                           
                                                                         
                                       
   
static int
namecmp(Name arg1, Name arg2, Oid collid)
{
                                                         
  if (collid == C_COLLATION_OID)
  {
    return strncmp(NameStr(*arg1), NameStr(*arg2), NAMEDATALEN);
  }

                                              
  return varstr_cmp(NameStr(*arg1), strlen(NameStr(*arg1)), NameStr(*arg2), strlen(NameStr(*arg2)), collid);
}

Datum
nameeq(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_BOOL(namecmp(arg1, arg2, PG_GET_COLLATION()) == 0);
}

Datum
namene(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_BOOL(namecmp(arg1, arg2, PG_GET_COLLATION()) != 0);
}

Datum
namelt(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_BOOL(namecmp(arg1, arg2, PG_GET_COLLATION()) < 0);
}

Datum
namele(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_BOOL(namecmp(arg1, arg2, PG_GET_COLLATION()) <= 0);
}

Datum
namegt(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_BOOL(namecmp(arg1, arg2, PG_GET_COLLATION()) > 0);
}

Datum
namege(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_BOOL(namecmp(arg1, arg2, PG_GET_COLLATION()) >= 0);
}

Datum
btnamecmp(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  Name arg2 = PG_GETARG_NAME(1);

  PG_RETURN_INT32(namecmp(arg1, arg2, PG_GET_COLLATION()));
}

Datum
btnamesortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);
  Oid collid = ssup->ssup_collation;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

                                      
  varstr_sortsupport(ssup, NAMEOID, collid);

  MemoryContextSwitchTo(oldcontext);

  PG_RETURN_VOID();
}

                                                                               
                                               
                                                                               

int
namecpy(Name n1, const NameData *n2)
{
  if (!n1 || !n2)
  {
    return -1;
  }
  StrNCpy(NameStr(*n1), NameStr(*n2), NAMEDATALEN);
  return 0;
}

#ifdef NOT_USED
int
namecat(Name n1, Name n2)
{
  return namestrcat(n1, NameStr(*n2));                                     
}
#endif

int
namestrcpy(Name name, const char *str)
{
  if (!name || !str)
  {
    return -1;
  }
  StrNCpy(NameStr(*name), str, NAMEDATALEN);
  return 0;
}

#ifdef NOT_USED
int
namestrcat(Name name, const char *str)
{
  int i;
  char *p, *q;

  if (!name || !str)
  {
    return -1;
  }
  for (i = 0, p = NameStr(*name); i < NAMEDATALEN && *p; ++i, ++p)
    ;
  for (q = str; i < NAMEDATALEN; ++i, ++p, ++q)
  {
    *p = *q;
    if (!*q)
    {
      break;
    }
  }
  return 0;
}
#endif

   
                                
   
                                                              
                                 
   
int
namestrcmp(Name name, const char *str)
{
  if (!name && !str)
  {
    return 0;
  }
  if (!name)
  {
    return -1;                      
  }
  if (!str)
  {
    return 1;                      
  }
  return strncmp(NameStr(*name), str, NAMEDATALEN);
}

   
                                            
   
Datum
current_user(PG_FUNCTION_ARGS)
{
  PG_RETURN_DATUM(DirectFunctionCall1(namein, CStringGetDatum(GetUserNameFromId(GetUserId(), false))));
}

Datum
session_user(PG_FUNCTION_ARGS)
{
  PG_RETURN_DATUM(DirectFunctionCall1(namein, CStringGetDatum(GetUserNameFromId(GetSessionUserId(), false))));
}

   
                                                 
   
Datum
current_schema(PG_FUNCTION_ARGS)
{
  List *search_path = fetch_search_path(false);
  char *nspname;

  if (search_path == NIL)
  {
    PG_RETURN_NULL();
  }
  nspname = get_namespace_name(linitial_oid(search_path));
  list_free(search_path);
  if (!nspname)
  {
    PG_RETURN_NULL();                                  
  }
  PG_RETURN_DATUM(DirectFunctionCall1(namein, CStringGetDatum(nspname)));
}

Datum
current_schemas(PG_FUNCTION_ARGS)
{
  List *search_path = fetch_search_path(PG_GETARG_BOOL(0));
  ListCell *l;
  Datum *names;
  int i;
  ArrayType *array;

  names = (Datum *)palloc(list_length(search_path) * sizeof(Datum));
  i = 0;
  foreach (l, search_path)
  {
    char *nspname;

    nspname = get_namespace_name(lfirst_oid(l));
    if (nspname)                                      
    {
      names[i] = DirectFunctionCall1(namein, CStringGetDatum(nspname));
      i++;
    }
  }
  list_free(search_path);

  array = construct_array(names, i, NAMEOID, NAMEDATALEN,                   
      false,                                                                      
      'c');                                                                      

  PG_RETURN_POINTER(array);
}

   
                                                      
   
                                                                            
                                                                            
                                                                  
                                        
                                                                        
                                            
   
Datum
nameconcatoid(PG_FUNCTION_ARGS)
{
  Name nam = PG_GETARG_NAME(0);
  Oid oid = PG_GETARG_OID(1);
  Name result;
  char suffix[20];
  int suflen;
  int namlen;

  suflen = snprintf(suffix, sizeof(suffix), "_%u", oid);
  namlen = strlen(NameStr(*nam));

                                                                   
  if (namlen + suflen >= NAMEDATALEN)
  {
    namlen = pg_mbcliplen(NameStr(*nam), namlen, NAMEDATALEN - 1 - suflen);
  }

                                                           
  result = (Name)palloc0(NAMEDATALEN);
  memcpy(NameStr(*result), NameStr(*nam), namlen);
  memcpy(NameStr(*result) + namlen, suffix, suflen);

  PG_RETURN_NAME(result);
}
