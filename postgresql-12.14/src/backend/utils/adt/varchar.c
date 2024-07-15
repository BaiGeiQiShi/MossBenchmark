                                                                            
   
             
                                                              
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/tuptoaster.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/pg_locale.h"
#include "utils/varlena.h"
#include "mb/pg_wchar.h"

                                                        
static int32
anychar_typmodin(ArrayType *ta, const char *typename)
{
  int32 typmod;
  int32 *tl;
  int n;

  tl = ArrayGetIntegerTypmods(ta, &n);

     
                                                                       
                                                        
     
  if (n != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid type modifier")));
  }

  if (*tl < 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("length for type %s must be at least 1", typename)));
  }
  if (*tl > MaxAttrSize)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("length for type %s cannot exceed %d", typename, MaxAttrSize)));
  }

     
                                                                            
                                                                           
                                     
     
  typmod = VARHDRSZ + *tl;

  return typmod;
}

                                                          
static char *
anychar_typmodout(int32 typmod)
{
  char *res = (char *)palloc(64);

  if (typmod > VARHDRSZ)
  {
    snprintf(res, 64, "(%d)", (int)(typmod - VARHDRSZ));
  }
  else
  {
    *res = '\0';
  }

  return res;
}

   
                                                                   
                                                                         
                                                                              
                         
   
                                                                   
                                                                              
                                                                       
                                                                            
                                                                            
                                                                            
                                                                       
                                                
   
                                                                            
                                                                          
                                                                          
                       
   
                             
   

                                                                               
                                    
                                                                               

   
                                                          
   
                                                                  
                                          
   
                                                        
                                                       
   
                                                                     
                                                                      
   
static BpChar *
bpchar_input(const char *s, size_t len, int32 atttypmod)
{
  BpChar *result;
  char *r;
  size_t maxlen;

                                                                  
  if (atttypmod < (int32)VARHDRSZ)
  {
    maxlen = len;
  }
  else
  {
    size_t charlen;                                        

    maxlen = atttypmod - VARHDRSZ;
    charlen = pg_mbstrlen_with_len(s, len);
    if (charlen > maxlen)
    {
                                                                      
      size_t mbmaxlen = pg_mbcharcliplen(s, len, maxlen);
      size_t j;

         
                                                                   
                                                                         
                                                                      
         
      for (j = mbmaxlen; j < len; j++)
      {
        if (s[j] != ' ')
        {
          ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("value too long for type character(%d)", (int)maxlen)));
        }
      }

         
                                                                        
                        
         
      maxlen = len = mbmaxlen;
    }
    else
    {
         
                                                                        
                        
         
      maxlen = len + (maxlen - charlen);
    }
  }

  result = (BpChar *)palloc(maxlen + VARHDRSZ);
  SET_VARSIZE(result, maxlen + VARHDRSZ);
  r = VARDATA(result);
  memcpy(r, s, len);

                                         
  if (maxlen > len)
  {
    memset(r + len, ' ', maxlen - len);
  }

  return result;
}

   
                                                                       
                                                     
   
Datum
bpcharin(PG_FUNCTION_ARGS)
{
  char *s = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  BpChar *result;

  result = bpchar_input(s, strlen(s), atttypmod);
  PG_RETURN_BPCHAR_P(result);
}

   
                                            
   
                                                                           
                                  
   
Datum
bpcharout(PG_FUNCTION_ARGS)
{
  Datum txt = PG_GETARG_DATUM(0);

  PG_RETURN_CSTRING(TextDatumGetCString(txt));
}

   
                                                             
   
Datum
bpcharrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  BpChar *result;
  char *str;
  int nbytes;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
  result = bpchar_input(str, nbytes, atttypmod);
  pfree(str);
  PG_RETURN_BPCHAR_P(result);
}

   
                                                    
   
Datum
bpcharsend(PG_FUNCTION_ARGS)
{
                                                   
  return textsend(fcinfo);
}

   
                                                    
   
                                                                  
                                                                  
   
                                                                          
                                                                         
                                                                          
                                                                           
                               
   
Datum
bpchar(PG_FUNCTION_ARGS)
{
  BpChar *source = PG_GETARG_BPCHAR_PP(0);
  int32 maxlen = PG_GETARG_INT32(1);
  bool isExplicit = PG_GETARG_BOOL(2);
  BpChar *result;
  int32 len;
  char *r;
  char *s;
  int i;
  int charlen;                                               
                             

                                    
  if (maxlen < (int32)VARHDRSZ)
  {
    PG_RETURN_BPCHAR_P(source);
  }

  maxlen -= VARHDRSZ;

  len = VARSIZE_ANY_EXHDR(source);
  s = VARDATA_ANY(source);

  charlen = pg_mbstrlen_with_len(s, len);

                                                       
  if (charlen == maxlen)
  {
    PG_RETURN_BPCHAR_P(source);
  }

  if (charlen > maxlen)
  {
                                                                    
    size_t maxmblen;

    maxmblen = pg_mbcharcliplen(s, len, maxlen);

    if (!isExplicit)
    {
      for (i = maxmblen; i < len; i++)
      {
        if (s[i] != ' ')
        {
          ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("value too long for type character(%d)", maxlen)));
        }
      }
    }

    len = maxmblen;

       
                                                                          
                      
       
    maxlen = len;
  }
  else
  {
       
                                                                          
                      
       
    maxlen = len + (maxlen - charlen);
  }

  Assert(maxlen >= len);

  result = palloc(maxlen + VARHDRSZ);
  SET_VARSIZE(result, maxlen + VARHDRSZ);
  r = VARDATA(result);

  memcpy(r, s, len);

                                         
  if (maxlen > len)
  {
    memset(r + len, ' ', maxlen - len);
  }

  PG_RETURN_BPCHAR_P(result);
}

                 
                              
   
Datum
char_bpchar(PG_FUNCTION_ARGS)
{
  char c = PG_GETARG_CHAR(0);
  BpChar *result;

  result = (BpChar *)palloc(VARHDRSZ + 1);

  SET_VARSIZE(result, VARHDRSZ + 1);
  *(VARDATA(result)) = c;

  PG_RETURN_BPCHAR_P(result);
}

                 
                                                
   
Datum
bpchar_name(PG_FUNCTION_ARGS)
{
  BpChar *s = PG_GETARG_BPCHAR_PP(0);
  char *s_data;
  Name result;
  int len;

  len = VARSIZE_ANY_EXHDR(s);
  s_data = VARDATA_ANY(s);

                               
  if (len >= NAMEDATALEN)
  {
    len = pg_mbcliplen(s_data, len, NAMEDATALEN - 1);
  }

                              
  while (len > 0)
  {
    if (s_data[len - 1] != ' ')
    {
      break;
    }
    len--;
  }

                                                           
  result = (Name)palloc0(NAMEDATALEN);
  memcpy(NameStr(*result), s_data, len);

  PG_RETURN_NAME(result);
}

                 
                                              
   
                                                                           
                                  
   
Datum
name_bpchar(PG_FUNCTION_ARGS)
{
  Name s = PG_GETARG_NAME(0);
  BpChar *result;

  result = (BpChar *)cstring_to_text(NameStr(*s));
  PG_RETURN_BPCHAR_P(result);
}

Datum
bpchartypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anychar_typmodin(ta, "char"));
}

Datum
bpchartypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anychar_typmodout(typmod));
}

                                                                               
                         
   
                                                                            
                                                         
                                                                               

   
                                                             
   
                                                                  
                                          
   
                                                        
                                                       
   
                                                                     
                                                                      
   
                                                                            
                                             
   
static VarChar *
varchar_input(const char *s, size_t len, int32 atttypmod)
{
  VarChar *result;
  size_t maxlen;

  maxlen = atttypmod - VARHDRSZ;

  if (atttypmod >= (int32)VARHDRSZ && len > maxlen)
  {
                                                                    
    size_t mbmaxlen = pg_mbcharcliplen(s, len, maxlen);
    size_t j;

    for (j = mbmaxlen; j < len; j++)
    {
      if (s[j] != ' ')
      {
        ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("value too long for type character varying(%d)", (int)maxlen)));
      }
    }

    len = mbmaxlen;
  }

  result = (VarChar *)cstring_to_text_with_len(s, len);
  return result;
}

   
                                                                     
                                                     
   
Datum
varcharin(PG_FUNCTION_ARGS)
{
  char *s = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  VarChar *result;

  result = varchar_input(s, strlen(s), atttypmod);
  PG_RETURN_VARCHAR_P(result);
}

   
                                          
   
                                                                            
                                             
   
Datum
varcharout(PG_FUNCTION_ARGS)
{
  Datum txt = PG_GETARG_DATUM(0);

  PG_RETURN_CSTRING(TextDatumGetCString(txt));
}

   
                                                               
   
Datum
varcharrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 atttypmod = PG_GETARG_INT32(2);
  VarChar *result;
  char *str;
  int nbytes;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
  result = varchar_input(str, nbytes, atttypmod);
  pfree(str);
  PG_RETURN_VARCHAR_P(result);
}

   
                                                      
   
Datum
varcharsend(PG_FUNCTION_ARGS)
{
                                                   
  return textsend(fcinfo);
}

   
                     
   
                                                                        
   
                                                                             
                                                                             
                                                                  
   
Datum
varchar_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSimplify))
  {
    SupportRequestSimplify *req = (SupportRequestSimplify *)rawreq;
    FuncExpr *expr = req->fcall;
    Node *typmod;

    Assert(list_length(expr->args) >= 2);

    typmod = (Node *)lsecond(expr->args);

    if (IsA(typmod, Const) && !((Const *)typmod)->constisnull)
    {
      Node *source = (Node *)linitial(expr->args);
      int32 old_typmod = exprTypmod(source);
      int32 new_typmod = DatumGetInt32(((Const *)typmod)->constvalue);
      int32 old_max = old_typmod - VARHDRSZ;
      int32 new_max = new_typmod - VARHDRSZ;

      if (new_typmod < 0 || (old_typmod >= 0 && old_max <= new_max))
      {
        ret = relabel_to_typmod(source, new_typmod);
      }
    }
  }

  PG_RETURN_POINTER(ret);
}

   
                                                  
   
                                                                  
                                                                     
   
                                                                          
                                                                         
                                                                          
                                                                           
                               
   
Datum
varchar(PG_FUNCTION_ARGS)
{
  VarChar *source = PG_GETARG_VARCHAR_PP(0);
  int32 typmod = PG_GETARG_INT32(1);
  bool isExplicit = PG_GETARG_BOOL(2);
  int32 len, maxlen;
  size_t maxmblen;
  int i;
  char *s_data;

  len = VARSIZE_ANY_EXHDR(source);
  s_data = VARDATA_ANY(source);
  maxlen = typmod - VARHDRSZ;

                                                                     
  if (maxlen < 0 || len <= maxlen)
  {
    PG_RETURN_VARCHAR_P(source);
  }

                                                

                                                               
  maxmblen = pg_mbcharcliplen(s_data, len, maxlen);

  if (!isExplicit)
  {
    for (i = maxmblen; i < len; i++)
    {
      if (s_data[i] != ' ')
      {
        ereport(ERROR, (errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION), errmsg("value too long for type character varying(%d)", maxlen)));
      }
    }
  }

  PG_RETURN_VARCHAR_P((VarChar *)cstring_to_text_with_len(s_data, maxmblen));
}

Datum
varchartypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anychar_typmodin(ta, "varchar"));
}

Datum
varchartypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anychar_typmodout(typmod));
}

                                                                               
                      
                                                                               

                                                              
static inline int
bcTruelen(BpChar *arg)
{
  return bpchartruelen(VARDATA_ANY(arg), VARSIZE_ANY_EXHDR(arg));
}

int
bpchartruelen(char *s, int len)
{
  int i;

     
                                                                         
                                                
     
  for (i = len - 1; i >= 0; i--)
  {
    if (s[i] != ' ')
    {
      break;
    }
  }
  return i + 1;
}

Datum
bpcharlen(PG_FUNCTION_ARGS)
{
  BpChar *arg = PG_GETARG_BPCHAR_PP(0);
  int len;

                                                     
  len = bcTruelen(arg);

                                                              
  if (pg_database_encoding_max_length() != 1)
  {
    len = pg_mbstrlen_with_len(VARDATA_ANY(arg), len);
  }

  PG_RETURN_INT32(len);
}

Datum
bpcharoctetlen(PG_FUNCTION_ARGS)
{
  Datum arg = PG_GETARG_DATUM(0);

                                            
  PG_RETURN_INT32(toast_raw_datum_size(arg) - VARHDRSZ);
}

                                                                               
                                        
   
                                                                          
                                                                           
                          
                                                                               

static void
check_collation_set(Oid collid)
{
  if (!OidIsValid(collid))
  {
       
                                                                         
                                                      
       
    ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for string comparison"), errhint("Use the COLLATE clause to set the collation explicitly.")));
  }
}

Datum
bpchareq(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  bool result;
  Oid collid = PG_GET_COLLATION();

  check_collation_set(collid);

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  if (lc_collate_is_c(collid) || collid == DEFAULT_COLLATION_OID || pg_newlocale_from_collation(collid)->deterministic)
  {
       
                                                                           
                                                                      
       
    if (len1 != len2)
    {
      result = false;
    }
    else
    {
      result = (memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), len1) == 0);
    }
  }
  else
  {
    result = (varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, collid) == 0);
  }

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bpcharne(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  bool result;
  Oid collid = PG_GET_COLLATION();

  check_collation_set(collid);

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  if (lc_collate_is_c(collid) || collid == DEFAULT_COLLATION_OID || pg_newlocale_from_collation(collid)->deterministic)
  {
       
                                                                           
                                                                      
       
    if (len1 != len2)
    {
      result = true;
    }
    else
    {
      result = (memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), len1) != 0);
    }
  }
  else
  {
    result = (varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, collid) != 0);
  }

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
bpcharlt(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(cmp < 0);
}

Datum
bpcharle(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(cmp <= 0);
}

Datum
bpchargt(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(cmp > 0);
}

Datum
bpcharge(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(cmp >= 0);
}

Datum
bpcharcmp(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(cmp);
}

Datum
bpchar_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);
  Oid collid = ssup->ssup_collation;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

                                      
  varstr_sortsupport(ssup, BPCHAROID, collid);

  MemoryContextSwitchTo(oldcontext);

  PG_RETURN_VOID();
}

Datum
bpchar_larger(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_RETURN_BPCHAR_P((cmp >= 0) ? arg1 : arg2);
}

Datum
bpchar_smaller(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int len1, len2;
  int cmp;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  cmp = varstr_cmp(VARDATA_ANY(arg1), len1, VARDATA_ANY(arg2), len2, PG_GET_COLLATION());

  PG_RETURN_BPCHAR_P((cmp <= 0) ? arg1 : arg2);
}

   
                                                                      
                                   
   
Datum
hashbpchar(PG_FUNCTION_ARGS)
{
  BpChar *key = PG_GETARG_BPCHAR_PP(0);
  Oid collid = PG_GET_COLLATION();
  char *keydata;
  int keylen;
  pg_locale_t mylocale = 0;
  Datum result;

  if (!collid)
  {
    ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for string hashing"), errhint("Use the COLLATE clause to set the collation explicitly.")));
  }

  keydata = VARDATA_ANY(key);
  keylen = bcTruelen(key);

  if (!lc_collate_is_c(collid) && collid != DEFAULT_COLLATION_OID)
  {
    mylocale = pg_newlocale_from_collation(collid);
  }

  if (!mylocale || mylocale->deterministic)
  {
    result = hash_any((unsigned char *)keydata, keylen);
  }
  else
  {
#ifdef USE_ICU
    if (mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t ulen = -1;
      UChar *uchar = NULL;
      Size bsize;
      uint8_t *buf;

      ulen = icu_to_uchar(&uchar, keydata, keylen);

      bsize = ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, NULL, 0);
      buf = palloc(bsize);
      ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, buf, bsize);
      pfree(uchar);

      result = hash_any(buf, bsize);

      pfree(buf);
    }
    else
#endif
                            
      elog(ERROR, "unsupported collprovider: %c", mylocale->provider);
  }

                                               
  PG_FREE_IF_COPY(key, 0);

  return result;
}

Datum
hashbpcharextended(PG_FUNCTION_ARGS)
{
  BpChar *key = PG_GETARG_BPCHAR_PP(0);
  Oid collid = PG_GET_COLLATION();
  char *keydata;
  int keylen;
  pg_locale_t mylocale = 0;
  Datum result;

  if (!collid)
  {
    ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for string hashing"), errhint("Use the COLLATE clause to set the collation explicitly.")));
  }

  keydata = VARDATA_ANY(key);
  keylen = bcTruelen(key);

  if (!lc_collate_is_c(collid) && collid != DEFAULT_COLLATION_OID)
  {
    mylocale = pg_newlocale_from_collation(collid);
  }

  if (!mylocale || mylocale->deterministic)
  {
    result = hash_any_extended((unsigned char *)keydata, keylen, PG_GETARG_INT64(1));
  }
  else
  {
#ifdef USE_ICU
    if (mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t ulen = -1;
      UChar *uchar = NULL;
      Size bsize;
      uint8_t *buf;

      ulen = icu_to_uchar(&uchar, VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));

      bsize = ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, NULL, 0);
      buf = palloc(bsize);
      ucol_getSortKey(mylocale->info.icu.ucol, uchar, ulen, buf, bsize);
      pfree(uchar);

      result = hash_any_extended(buf, bsize, PG_GETARG_INT64(1));

      pfree(buf);
    }
    else
#endif
                            
      elog(ERROR, "unsupported collprovider: %c", mylocale->provider);
  }

  PG_FREE_IF_COPY(key, 0);

  return result;
}

   
                                                                     
                                                                          
                                                                     
                                                                          
                          
   

static int
internal_bpchar_pattern_compare(BpChar *arg1, BpChar *arg2)
{
  int result;
  int len1, len2;

  len1 = bcTruelen(arg1);
  len2 = bcTruelen(arg2);

  result = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));
  if (result != 0)
  {
    return result;
  }
  else if (len1 < len2)
  {
    return -1;
  }
  else if (len1 > len2)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

Datum
bpchar_pattern_lt(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int result;

  result = internal_bpchar_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result < 0);
}

Datum
bpchar_pattern_le(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int result;

  result = internal_bpchar_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result <= 0);
}

Datum
bpchar_pattern_ge(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int result;

  result = internal_bpchar_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result >= 0);
}

Datum
bpchar_pattern_gt(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int result;

  result = internal_bpchar_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result > 0);
}

Datum
btbpchar_pattern_cmp(PG_FUNCTION_ARGS)
{
  BpChar *arg1 = PG_GETARG_BPCHAR_PP(0);
  BpChar *arg2 = PG_GETARG_BPCHAR_PP(1);
  int result;

  result = internal_bpchar_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(result);
}

Datum
btbpchar_pattern_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

                                                             
  varstr_sortsupport(ssup, BPCHAROID, C_COLLATION_OID);

  MemoryContextSwitchTo(oldcontext);

  PG_RETURN_VOID();
}
