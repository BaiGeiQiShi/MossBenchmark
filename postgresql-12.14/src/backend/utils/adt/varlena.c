                                                                            
   
             
                                                       
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <limits.h>

#include "access/tuptoaster.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "common/int.h"
#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "parser/scansup.h"
#include "port/pg_bswap.h"
#include "regex/regex.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/sortsupport.h"
#include "utils/varlena.h"

                  
int bytea_output = BYTEA_OUTPUT_HEX;

typedef struct varlena unknown;
typedef struct varlena VarString;

   
                                        
   
typedef struct
{
  bool is_multibyte;                              
  bool is_multibyte_char_in_char;

  char *str1;                      
  char *str2;                    
  int len1;                                
  int len2;

                                                             
  int skiptablemask;                                                 
  int skiptable[256];                                              

  char *last_match;                                      

     
                                                                    
                                                                            
                                                                            
                                               
     
  char *refpoint;                                              
  int refpos;                                                     
} TextPositionState;

typedef struct
{
  char *buf1;                                                       
                              
  char *buf2;                                                       
  int buflen1;                                     
  int buflen2;                                     
  int last_len1;                                                     
  int last_len2;                                                    
  int last_returned;                                     
  bool cache_blob;                                               
  bool collate_c;
  Oid typid;                                                                
  hyperLogLogState abbr_card;                                        
  hyperLogLogState full_card;                                 
  double prop_card;                                                
  pg_locale_t locale;
} VarStringSortSupport;

   
                                                                            
                                                    
   
#define TEXTBUFLEN 1024

#define DatumGetUnknownP(X) ((unknown *)PG_DETOAST_DATUM(X))
#define DatumGetUnknownPCopy(X) ((unknown *)PG_DETOAST_DATUM_COPY(X))
#define PG_GETARG_UNKNOWN_P(n) DatumGetUnknownP(PG_GETARG_DATUM(n))
#define PG_GETARG_UNKNOWN_P_COPY(n) DatumGetUnknownPCopy(PG_GETARG_DATUM(n))
#define PG_RETURN_UNKNOWN_P(x) PG_RETURN_POINTER(x)

#define DatumGetVarStringP(X) ((VarString *)PG_DETOAST_DATUM(X))
#define DatumGetVarStringPP(X) ((VarString *)PG_DETOAST_DATUM_PACKED(X))

static int
varstrfastcmp_c(Datum x, Datum y, SortSupport ssup);
static int
bpcharfastcmp_c(Datum x, Datum y, SortSupport ssup);
static int
namefastcmp_c(Datum x, Datum y, SortSupport ssup);
static int
varlenafastcmp_locale(Datum x, Datum y, SortSupport ssup);
static int
namefastcmp_locale(Datum x, Datum y, SortSupport ssup);
static int
varstrfastcmp_locale(char *a1p, int len1, char *a2p, int len2, SortSupport ssup);
static int
varstrcmp_abbrev(Datum x, Datum y, SortSupport ssup);
static Datum
varstr_abbrev_convert(Datum original, SortSupport ssup);
static bool
varstr_abbrev_abort(int memtupcount, SortSupport ssup);
static int32
text_length(Datum str);
static text *
text_catenate(text *t1, text *t2);
static text *
text_substring(Datum str, int32 start, int32 length, bool length_not_specified);
static text *
text_overlay(text *t1, text *t2, int sp, int sl);
static int
text_position(text *t1, text *t2, Oid collid);
static void
text_position_setup(text *t1, text *t2, Oid collid, TextPositionState *state);
static bool
text_position_next(TextPositionState *state);
static char *
text_position_next_internal(char *start_ptr, TextPositionState *state);
static char *
text_position_get_match_ptr(TextPositionState *state);
static int
text_position_get_match_pos(TextPositionState *state);
static void
text_position_cleanup(TextPositionState *state);
static void
check_collation_set(Oid collid);
static int
text_cmp(text *arg1, text *arg2, Oid collid);
static bytea *
bytea_catenate(bytea *t1, bytea *t2);
static bytea *
bytea_substring(Datum str, int S, int L, bool length_not_specified);
static bytea *
bytea_overlay(bytea *t1, bytea *t2, int sp, int sl);
static void
appendStringInfoText(StringInfo str, const text *t);
static Datum text_to_array_internal(PG_FUNCTION_ARGS);
static text *
array_to_text_internal(FunctionCallInfo fcinfo, ArrayType *v, const char *fldsep, const char *null_string);
static StringInfo
makeStringAggState(FunctionCallInfo fcinfo);
static bool
text_format_parse_digits(const char **ptr, const char *end_ptr, int *value);
static const char *
text_format_parse_format(const char *start_ptr, const char *end_ptr, int *argpos, int *widthpos, int *flags, int *width);
static void
text_format_string_conversion(StringInfo buf, char conversion, FmgrInfo *typOutputInfo, Datum value, bool isNull, int flags, int width);
static void
text_format_append_string(StringInfo buf, const char *str, int flags, int width);

                                                                               
                                                            
                                                                               

   
                   
   
                                                        
   
                                                                   
   
text *
cstring_to_text(const char *s)
{
  return cstring_to_text_with_len(s, strlen(s));
}

   
                            
   
                                                                          
                                           
   
text *
cstring_to_text_with_len(const char *s, int len)
{
  text *result = (text *)palloc(len + VARHDRSZ);

  SET_VARSIZE(result, len + VARHDRSZ);
  memcpy(VARDATA(result), s, len);

  return result;
}

   
                   
   
                                                                  
   
                                                               
                                                                            
                                                                           
                                                          
   
char *
text_to_cstring(const text *t)
{
                                               
  text *tunpacked = pg_detoast_datum_packed(unconstify(text *, t));
  int len = VARSIZE_ANY_EXHDR(tunpacked);
  char *result;

  result = (char *)palloc(len + 1);
  memcpy(result, VARDATA_ANY(tunpacked), len);
  result[len] = '\0';

  if (tunpacked != t)
  {
    pfree(tunpacked);
  }

  return result;
}

   
                          
   
                                                                    
   
                                                                    
                                                     
   
                                                               
                                                                            
                                                                           
                                                          
   
void
text_to_cstring_buffer(const text *src, char *dst, size_t dst_len)
{
                                               
  text *srcunpacked = pg_detoast_datum_packed(unconstify(text *, src));
  size_t src_len = VARSIZE_ANY_EXHDR(srcunpacked);

  if (dst_len > 0)
  {
    dst_len--;
    if (dst_len >= src_len)
    {
      dst_len = src_len;
    }
    else                                         
    {
      dst_len = pg_mbcliplen(VARDATA_ANY(srcunpacked), src_len, dst_len);
    }
    memcpy(dst, VARDATA_ANY(srcunpacked), dst_len);
    dst[dst_len] = '\0';
  }

  if (srcunpacked != src)
  {
    pfree(srcunpacked);
  }
}

                                                                               
                                      
                                                                               

#define VAL(CH) ((CH) - '0')
#define DIG(VAL) ((VAL) + '0')

   
                                                                     
   
                                                                      
                                                             
                                     
   
          
                                  
                                              
   
Datum
byteain(PG_FUNCTION_ARGS)
{
  char *inputText = PG_GETARG_CSTRING(0);
  char *tp;
  char *rp;
  int bc;
  bytea *result;

                           
  if (inputText[0] == '\\' && inputText[1] == 'x')
  {
    size_t len = strlen(inputText);

    bc = (len - 2) / 2 + VARHDRSZ;                              
    result = palloc(bc);
    bc = hex_decode(inputText + 2, len - 2, VARDATA(result));
    SET_VARSIZE(result, bc + VARHDRSZ);                    

    PG_RETURN_BYTEA_P(result);
  }

                                                
  for (bc = 0, tp = inputText; *tp != '\0'; bc++)
  {
    if (tp[0] != '\\')
    {
      tp++;
    }
    else if ((tp[0] == '\\') && (tp[1] >= '0' && tp[1] <= '3') && (tp[2] >= '0' && tp[2] <= '7') && (tp[3] >= '0' && tp[3] <= '7'))
    {
      tp += 4;
    }
    else if ((tp[0] == '\\') && (tp[1] == '\\'))
    {
      tp += 2;
    }
    else
    {
         
                                                                   
         
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "bytea")));
    }
  }

  bc += VARHDRSZ;

  result = (bytea *)palloc(bc);
  SET_VARSIZE(result, bc);

  tp = inputText;
  rp = VARDATA(result);
  while (*tp != '\0')
  {
    if (tp[0] != '\\')
    {
      *rp++ = *tp++;
    }
    else if ((tp[0] == '\\') && (tp[1] >= '0' && tp[1] <= '3') && (tp[2] >= '0' && tp[2] <= '7') && (tp[3] >= '0' && tp[3] <= '7'))
    {
      bc = VAL(tp[1]);
      bc <<= 3;
      bc += VAL(tp[2]);
      bc <<= 3;
      *rp++ = bc + VAL(tp[3]);

      tp += 4;
    }
    else if ((tp[0] == '\\') && (tp[1] == '\\'))
    {
      *rp++ = '\\';
      tp += 2;
    }
    else
    {
         
                                                                       
         
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s", "bytea")));
    }
  }

  PG_RETURN_BYTEA_P(result);
}

   
                                                                   
   
                                                                    
                                               
   
Datum
byteaout(PG_FUNCTION_ARGS)
{
  bytea *vlena = PG_GETARG_BYTEA_PP(0);
  char *result;
  char *rp;

  if (bytea_output == BYTEA_OUTPUT_HEX)
  {
                          
    rp = result = palloc(VARSIZE_ANY_EXHDR(vlena) * 2 + 2 + 1);
    *rp++ = '\\';
    *rp++ = 'x';
    rp += hex_encode(VARDATA_ANY(vlena), VARSIZE_ANY_EXHDR(vlena), rp);
  }
  else if (bytea_output == BYTEA_OUTPUT_ESCAPE)
  {
                                          
    char *vp;
    int len;
    int i;

    len = 1;                              
    vp = VARDATA_ANY(vlena);
    for (i = VARSIZE_ANY_EXHDR(vlena); i != 0; i--, vp++)
    {
      if (*vp == '\\')
      {
        len += 2;
      }
      else if ((unsigned char)*vp < 0x20 || (unsigned char)*vp > 0x7e)
      {
        len += 4;
      }
      else
      {
        len++;
      }
    }
    rp = result = (char *)palloc(len);
    vp = VARDATA_ANY(vlena);
    for (i = VARSIZE_ANY_EXHDR(vlena); i != 0; i--, vp++)
    {
      if (*vp == '\\')
      {
        *rp++ = '\\';
        *rp++ = '\\';
      }
      else if ((unsigned char)*vp < 0x20 || (unsigned char)*vp > 0x7e)
      {
        int val;                              

        val = *vp;
        rp[0] = '\\';
        rp[3] = DIG(val & 07);
        val >>= 3;
        rp[2] = DIG(val & 07);
        val >>= 3;
        rp[1] = DIG(val & 03);
        rp += 4;
      }
      else
      {
        *rp++ = *vp;
      }
    }
  }
  else
  {
    elog(ERROR, "unrecognized bytea_output setting: %d", bytea_output);
    rp = result = NULL;                          
  }
  *rp = '\0';
  PG_RETURN_CSTRING(result);
}

   
                                                           
   
Datum
bytearecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  bytea *result;
  int nbytes;

  nbytes = buf->len - buf->cursor;
  result = (bytea *)palloc(nbytes + VARHDRSZ);
  SET_VARSIZE(result, nbytes + VARHDRSZ);
  pq_copymsgbytes(buf, VARDATA(result), nbytes);
  PG_RETURN_BYTEA_P(result);
}

   
                                                  
   
                                                  
   
Datum
byteasend(PG_FUNCTION_ARGS)
{
  bytea *vlena = PG_GETARG_BYTEA_P_COPY(0);

  PG_RETURN_BYTEA_P(vlena);
}

Datum
bytea_string_agg_transfn(PG_FUNCTION_ARGS)
{
  StringInfo state;

  state = PG_ARGISNULL(0) ? NULL : (StringInfo)PG_GETARG_POINTER(0);

                                     
  if (!PG_ARGISNULL(1))
  {
    bytea *value = PG_GETARG_BYTEA_PP(1);

                                                             
    if (state == NULL)
    {
      state = makeStringAggState(fcinfo);
    }
    else if (!PG_ARGISNULL(2))
    {
      bytea *delim = PG_GETARG_BYTEA_PP(2);

      appendBinaryStringInfo(state, VARDATA_ANY(delim), VARSIZE_ANY_EXHDR(delim));
    }

    appendBinaryStringInfo(state, VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
  }

     
                                                                        
                                                               
     
  PG_RETURN_POINTER(state);
}

Datum
bytea_string_agg_finalfn(PG_FUNCTION_ARGS)
{
  StringInfo state;

                                                                   
  Assert(AggCheckCallContext(fcinfo, NULL));

  state = PG_ARGISNULL(0) ? NULL : (StringInfo)PG_GETARG_POINTER(0);

  if (state != NULL)
  {
    bytea *result;

    result = (bytea *)palloc(state->len + VARHDRSZ);
    SET_VARSIZE(result, state->len + VARHDRSZ);
    memcpy(VARDATA(result), state->data, state->len);
    PG_RETURN_BYTEA_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                         
   
Datum
textin(PG_FUNCTION_ARGS)
{
  char *inputText = PG_GETARG_CSTRING(0);

  PG_RETURN_TEXT_P(cstring_to_text(inputText));
}

   
                                                          
   
Datum
textout(PG_FUNCTION_ARGS)
{
  Datum txt = PG_GETARG_DATUM(0);

  PG_RETURN_CSTRING(TextDatumGetCString(txt));
}

   
                                                         
   
Datum
textrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  text *result;
  char *str;
  int nbytes;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);

  result = cstring_to_text_with_len(str, nbytes);
  pfree(str);
  PG_RETURN_TEXT_P(result);
}

   
                                                
   
Datum
textsend(PG_FUNCTION_ARGS)
{
  text *t = PG_GETARG_TEXT_PP(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendtext(&buf, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                            
   
Datum
unknownin(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

                                         
  PG_RETURN_CSTRING(pstrdup(str));
}

   
                                                             
   
Datum
unknownout(PG_FUNCTION_ARGS)
{
                                         
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_CSTRING(pstrdup(str));
}

   
                                                               
   
Datum
unknownrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  char *str;
  int nbytes;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
                                         
  PG_RETURN_CSTRING(str);
}

   
                                                      
   
Datum
unknownsend(PG_FUNCTION_ARGS)
{
                                         
  char *str = PG_GETARG_CSTRING(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendtext(&buf, str, strlen(str));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                           

   
             
                                           
                                                    
   
Datum
textlen(PG_FUNCTION_ARGS)
{
  Datum str = PG_GETARG_DATUM(0);

                                           
  PG_RETURN_INT32(text_length(str));
}

   
                 
                                    
   
                                                                              
                                                                             
                                                                             
                  
   
static int32
text_length(Datum str)
{
                                                
  if (pg_database_encoding_max_length() == 1)
  {
    PG_RETURN_INT32(toast_raw_datum_size(str) - VARHDRSZ);
  }
  else
  {
    text *t = DatumGetTextPP(str);

    PG_RETURN_INT32(pg_mbstrlen_with_len(VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t)));
  }
}

   
                  
                                            
                                                    
   
Datum
textoctetlen(PG_FUNCTION_ARGS)
{
  Datum str = PG_GETARG_DATUM(0);

                                            
  PG_RETURN_INT32(toast_raw_datum_size(str) - VARHDRSZ);
}

   
             
                                                                      
              
   
                                                     
                                                               
                                           
                           
   
Datum
textcat(PG_FUNCTION_ARGS)
{
  text *t1 = PG_GETARG_TEXT_PP(0);
  text *t2 = PG_GETARG_TEXT_PP(1);

  PG_RETURN_TEXT_P(text_catenate(t1, t2));
}

   
                 
                                                                      
   
                                                                            
   
static text *
text_catenate(text *t1, text *t2)
{
  text *result;
  int len1, len2, len;
  char *ptr;

  len1 = VARSIZE_ANY_EXHDR(t1);
  len2 = VARSIZE_ANY_EXHDR(t2);

                                                         
  if (len1 < 0)
  {
    len1 = 0;
  }
  if (len2 < 0)
  {
    len2 = 0;
  }

  len = len1 + len2 + VARHDRSZ;
  result = (text *)palloc(len);

                                    
  SET_VARSIZE(result, len);

                                           
  ptr = VARDATA(result);
  if (len1 > 0)
  {
    memcpy(ptr, VARDATA_ANY(t1), len1);
  }
  if (len2 > 0)
  {
    memcpy(ptr + len1, VARDATA_ANY(t2), len2);
  }

  return result;
}

   
                        
                                                                       
   
                                                                       
                                           
   
static int
charlen_to_bytelen(const char *p, int n)
{
  if (pg_database_encoding_max_length() == 1)
  {
                                                
    return n;
  }
  else
  {
    const char *s;

    for (s = p; n > 0; n--)
    {
      s += pg_mblen(s);
    }

    return s - p;
  }
}

   
                 
                                                          
                       
   
          
            
                                      
                   
   
                                                                                      
                                                                            
                                                                 
   
                            
                            
                                                                                      
                                                               
                                
                                           
                          
                                                                              
                                                                      
                                                                             
                                                                  
                           
   
Datum
text_substr(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(text_substring(PG_GETARG_DATUM(0), PG_GETARG_INT32(1), PG_GETARG_INT32(2), false));
}

   
                        
                                                
                                                        
   
Datum
text_substr_no_len(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(text_substring(PG_GETARG_DATUM(0), PG_GETARG_INT32(1), -1, true));
}

   
                    
                                                                 
   
                                                                              
                                                                             
                                                                            
                        
   
                                                  
   
static text *
text_substring(Datum str, int32 start, int32 length, bool length_not_specified)
{
  int32 eml = pg_database_encoding_max_length();
  int32 S = start;                     
  int32 S1;                                     
  int32 L1;                                       
  int32 E;                           

     
                                                                            
                          
     
  S1 = Max(S, 1);

                                                    
  if (eml == 1)
  {
    if (length_not_specified)                                        
                                          
    {
      L1 = -1;
    }
    else if (length < 0)
    {
                                                                         
      ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
      L1 = -1;                                 
    }
    else if (pg_add_s32_overflow(S, length, &E))
    {
         
                                                                      
                                                  
         
      L1 = -1;
    }
    else
    {
         
                                                                         
                                                                       
                 
         
      if (E < 1)
      {
        return cstring_to_text("");
      }

      L1 = E - S1;
    }

       
                                                                          
                                                                        
                                                                         
       
    return DatumGetTextPSlice(str, S1 - 1, L1);
  }
  else if (eml > 1)
  {
       
                                                                
                                                                         
                                        
       
    int32 slice_start;
    int32 slice_size;
    int32 slice_strlen;
    text *slice;
    int32 E1;
    int32 i;
    char *p;
    char *s;
    text *ret;

       
                                                                         
                                                                      
                 
       
    slice_start = 0;

    if (length_not_specified)                                        
                                          
    {
      slice_size = L1 = -1;
    }
    else if (length < 0)
    {
                                                                         
      ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
      slice_size = L1 = -1;                                 
    }
    else if (pg_add_s32_overflow(S, length, &E))
    {
         
                                                                      
                                                  
         
      slice_size = L1 = -1;
    }
    else
    {
         
                                                                         
                                                                       
                 
         
      if (E < 1)
      {
        return cstring_to_text("");
      }

         
                                                                    
                                    
         
      L1 = E - S1;

         
                                                                      
                                                                       
                                                
         
      if (pg_mul_s32_overflow(E, eml, &slice_size))
      {
        slice_size = -1;
      }
    }

       
                                                                         
                     
       
    if (VARATT_IS_COMPRESSED(DatumGetPointer(str)) || VARATT_IS_EXTERNAL(DatumGetPointer(str)))
    {
      slice = DatumGetTextPSlice(str, slice_start, slice_size);
    }
    else
    {
      slice = (text *)DatumGetPointer(str);
    }

                                            
    if (VARSIZE_ANY_EXHDR(slice) == 0)
    {
      if (slice != (text *)DatumGetPointer(str))
      {
        pfree(slice);
      }
      return cstring_to_text("");
    }

                                                                        
    slice_strlen = pg_mbstrlen_with_len(VARDATA_ANY(slice), VARSIZE_ANY_EXHDR(slice));

       
                                                                         
                                            
       
    if (S1 > slice_strlen)
    {
      if (slice != (text *)DatumGetPointer(str))
      {
        pfree(slice);
      }
      return cstring_to_text("");
    }

       
                                                                        
                                                                     
       
    if (L1 > -1)
    {
      E1 = Min(S1 + L1, slice_start + 1 + slice_strlen);
    }
    else
    {
      E1 = slice_start + 1 + slice_strlen;
    }

       
                                                                           
       
    p = VARDATA_ANY(slice);
    for (i = 0; i < S1 - 1; i++)
    {
      p += pg_mblen(p);
    }

                                                   
    s = p;

       
                                                                     
               
       
    for (i = S1; i < E1; i++)
    {
      p += pg_mblen(p);
    }

    ret = (text *)palloc(VARHDRSZ + (p - s));
    SET_VARSIZE(ret, VARHDRSZ + (p - s));
    memcpy(VARDATA(ret), s, (p - s));

    if (slice != (text *)DatumGetPointer(str))
    {
      pfree(slice);
    }

    return ret;
  }
  else
  {
    elog(ERROR, "invalid backend encoding: encoding max length < 1");
  }

                                              
  return NULL;
}

   
               
                                                           
   
                                                                               
                                                                   
   
Datum
textoverlay(PG_FUNCTION_ARGS)
{
  text *t1 = PG_GETARG_TEXT_PP(0);
  text *t2 = PG_GETARG_TEXT_PP(1);
  int sp = PG_GETARG_INT32(2);                               
  int sl = PG_GETARG_INT32(3);                       

  PG_RETURN_TEXT_P(text_overlay(t1, t2, sp, sl));
}

Datum
textoverlay_no_len(PG_FUNCTION_ARGS)
{
  text *t1 = PG_GETARG_TEXT_PP(0);
  text *t2 = PG_GETARG_TEXT_PP(1);
  int sp = PG_GETARG_INT32(2);                               
  int sl;

  sl = text_length(PointerGetDatum(t2));                             
  PG_RETURN_TEXT_P(text_overlay(t1, t2, sp, sl));
}

static text *
text_overlay(text *t1, text *t2, int sp, int sl)
{
  text *result;
  text *s1;
  text *s2;
  int sp_pl_sl;

     
                                                                          
                                                                     
                                                      
     
  if (sp <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
  }
  if (pg_add_s32_overflow(sp, sl, &sp_pl_sl))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  s1 = text_substring(PointerGetDatum(t1), 1, sp - 1, false);
  s2 = text_substring(PointerGetDatum(t1), sp_pl_sl, -1, true);
  result = text_catenate(s1, t2);
  result = text_catenate(result, s2);

  return result;
}

   
             
                                                     
                                             
                                                           
                       
   
Datum
textpos(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  text *search_str = PG_GETARG_TEXT_PP(1);

  PG_RETURN_INT32((int32)text_position(str, search_str, PG_GET_COLLATION()));
}

   
                   
                                    
   
           
                               
                                    
           
                                                                
                      
   
                                                                              
              
   
static int
text_position(text *t1, text *t2, Oid collid)
{
  TextPositionState state;
  int result;

                                                 
  if (VARSIZE_ANY_EXHDR(t2) < 1)
  {
    return 1;
  }

                                                                 
  if (VARSIZE_ANY_EXHDR(t1) < VARSIZE_ANY_EXHDR(t2))
  {
    return 0;
  }

  text_position_setup(t1, t2, collid, &state);
  if (!text_position_next(&state))
  {
    result = 0;
  }
  else
  {
    result = text_position_get_match_pos(&state);
  }
  text_position_cleanup(&state);
  return result;
}

   
                                                                    
                                      
   
                                                                         
                                                                        
                                                                          
                                                                          
                                                                            
   
                                                                         
   
                                                                          
                                                                
   

static void
text_position_setup(text *t1, text *t2, Oid collid, TextPositionState *state)
{
  int len1 = VARSIZE_ANY_EXHDR(t1);
  int len2 = VARSIZE_ANY_EXHDR(t2);
  pg_locale_t mylocale = 0;

  check_collation_set(collid);

  if (!lc_collate_is_c(collid) && collid != DEFAULT_COLLATION_OID)
  {
    mylocale = pg_newlocale_from_collation(collid);
  }

  if (mylocale && !mylocale->deterministic)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations are not supported for substring searches")));
  }

  Assert(len1 > 0);
  Assert(len2 > 0);

     
                                                                          
                                                                            
                                                                        
                                                                          
                                                                       
                                                                           
                                                      
     
  if (pg_database_encoding_max_length() == 1)
  {
    state->is_multibyte = false;
    state->is_multibyte_char_in_char = false;
  }
  else if (GetDatabaseEncoding() == PG_UTF8)
  {
    state->is_multibyte = true;
    state->is_multibyte_char_in_char = false;
  }
  else
  {
    state->is_multibyte = true;
    state->is_multibyte_char_in_char = true;
  }

  state->str1 = VARDATA_ANY(t1);
  state->str2 = VARDATA_ANY(t2);
  state->len1 = len1;
  state->len2 = len2;
  state->last_match = NULL;
  state->refpoint = state->str1;
  state->refpos = 0;

     
                                                                          
                                                                          
                                                                      
     
                                                                         
                                                                            
                                                                            
                                 
     
  if (len1 >= len2 && len2 > 1)
  {
    int searchlength = len1 - len2;
    int skiptablemask;
    int last;
    int i;
    const char *str2 = state->str2;

       
                                                                       
                                                                           
                                                                           
                                                                     
                                                                           
                                                                           
                                                                          
       
                                                                         
                                                                       
       
    if (searchlength < 16)
    {
      skiptablemask = 3;
    }
    else if (searchlength < 64)
    {
      skiptablemask = 7;
    }
    else if (searchlength < 128)
    {
      skiptablemask = 15;
    }
    else if (searchlength < 512)
    {
      skiptablemask = 31;
    }
    else if (searchlength < 2048)
    {
      skiptablemask = 63;
    }
    else if (searchlength < 4096)
    {
      skiptablemask = 127;
    }
    else
    {
      skiptablemask = 255;
    }
    state->skiptablemask = skiptablemask;

       
                                                                     
                                                                         
                                
       
    for (i = 0; i <= skiptablemask; i++)
    {
      state->skiptable[i] = len2;
    }

       
                                                                        
                                                                   
                                                                          
                                                                  
                 
       
    last = len2 - 1;

    for (i = 0; i < last; i++)
    {
      state->skiptable[(unsigned char)str2[i] & skiptablemask] = last - i;
    }
  }
}

   
                                                                          
                                                                             
             
   
                                                                         
                                                                      
   
static bool
text_position_next(TextPositionState *state)
{
  int needle_len = state->len2;
  char *start_ptr;
  char *matchptr;

  if (needle_len <= 0)
  {
    return false;                               
  }

                                                            
  if (state->last_match)
  {
    start_ptr = state->last_match + needle_len;
  }
  else
  {
    start_ptr = state->str1;
  }

retry:
  matchptr = text_position_next_internal(start_ptr, state);

  if (!matchptr)
  {
    return false;
  }

     
                                                                            
                                                                    
                                                                     
                                                                      
     
  if (state->is_multibyte_char_in_char)
  {
                                                                 

                                                 
    Assert(state->refpoint <= matchptr);

    while (state->refpoint < matchptr)
    {
                                   
      state->refpoint += pg_mblen(state->refpoint);
      state->refpos++;

         
                                                                      
                                                                        
                                                                         
                                      
         
      if (state->refpoint > matchptr)
      {
        start_ptr = state->refpoint;
        goto retry;
      }
    }
  }

  state->last_match = matchptr;
  return true;
}

   
                                                                       
                                                                         
                                                                
   
static char *
text_position_next_internal(char *start_ptr, TextPositionState *state)
{
  int haystack_len = state->len1;
  int needle_len = state->len2;
  int skiptablemask = state->skiptablemask;
  const char *haystack = state->str1;
  const char *needle = state->str2;
  const char *haystack_end = &haystack[haystack_len];
  const char *hptr;

  Assert(start_ptr >= haystack && start_ptr <= haystack_end);

  if (needle_len == 1)
  {
                                                            
    char nchar = *needle;

    hptr = start_ptr;
    while (hptr < haystack_end)
    {
      if (*hptr == nchar)
      {
        return (char *)hptr;
      }
      hptr++;
    }
  }
  else
  {
    const char *needle_last = &needle[needle_len - 1];

                                                         
    hptr = start_ptr + needle_len - 1;
    while (hptr < haystack_end)
    {
                                                
      const char *nptr;
      const char *p;

      nptr = needle_last;
      p = hptr;
      while (*nptr == *p)
      {
                                                            
        if (nptr == needle)
        {
          return (char *)p;
        }
        nptr--, p--;
      }

         
                                                                         
                                                                      
                                                                   
                                                                        
                                                                     
                                                              
         
      hptr += state->skiptable[(unsigned char)*hptr & skiptablemask];
    }
  }

  return 0;                
}

   
                                          
   
                                                                     
                        
   
static char *
text_position_get_match_ptr(TextPositionState *state)
{
  return state->last_match;
}

   
                                           
   
                                         
   
static int
text_position_get_match_pos(TextPositionState *state)
{
  if (!state->is_multibyte)
  {
    return state->last_match - state->str1 + 1;
  }
  else
  {
                                                     
    while (state->refpoint < state->last_match)
    {
      state->refpoint += pg_mblen(state->refpoint);
      state->refpos++;
    }
    Assert(state->refpoint == state->last_match);
    return state->refpos + 1;
  }
}

static void
text_position_cleanup(TextPositionState *state)
{
                         
}

static void
check_collation_set(Oid collid)
{
  if (!OidIsValid(collid))
  {
       
                                                                         
                                                      
       
    ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for string comparison"), errhint("Use the COLLATE clause to set the collation explicitly.")));
  }
}

                
                                                            
                                                                      
                                                      
                                                                            
                                                              
   
                                                                             
                                                                          
                                                                          
                                                                           
                     
   
int
varstr_cmp(const char *arg1, int len1, const char *arg2, int len2, Oid collid)
{
  int result;

  check_collation_set(collid);

     
                                                                           
                                                                         
                                                                            
                                                                          
     
  if (lc_collate_is_c(collid))
  {
    result = memcmp(arg1, arg2, Min(len1, len2));
    if ((result == 0) && (len1 != len2))
    {
      result = (len1 < len2) ? -1 : 1;
    }
  }
  else
  {
    char a1buf[TEXTBUFLEN];
    char a2buf[TEXTBUFLEN];
    char *a1p, *a2p;
    pg_locale_t mylocale = 0;

    if (collid != DEFAULT_COLLATION_OID)
    {
      mylocale = pg_newlocale_from_collation(collid);
    }

       
                                                                        
                                                                          
                                                                    
                                                                           
                                                                        
                                                                          
                                    
       
    if (len1 == len2 && memcmp(arg1, arg2, len1) == 0)
    {
      return 0;
    }

#ifdef WIN32
                                                                
    if (GetDatabaseEncoding() == PG_UTF8 && (!mylocale || mylocale->provider == COLLPROVIDER_LIBC))
    {
      int a1len;
      int a2len;
      int r;

      if (len1 >= TEXTBUFLEN / 2)
      {
        a1len = len1 * 2 + 2;
        a1p = palloc(a1len);
      }
      else
      {
        a1len = TEXTBUFLEN;
        a1p = a1buf;
      }
      if (len2 >= TEXTBUFLEN / 2)
      {
        a2len = len2 * 2 + 2;
        a2p = palloc(a2len);
      }
      else
      {
        a2len = TEXTBUFLEN;
        a2p = a2buf;
      }

                                                                     
      if (len1 == 0)
      {
        r = 0;
      }
      else
      {
        r = MultiByteToWideChar(CP_UTF8, 0, arg1, len1, (LPWSTR)a1p, a1len / 2);
        if (!r)
        {
          ereport(ERROR, (errmsg("could not convert string to UTF-16: error code %lu", GetLastError())));
        }
      }
      ((LPWSTR)a1p)[r] = 0;

      if (len2 == 0)
      {
        r = 0;
      }
      else
      {
        r = MultiByteToWideChar(CP_UTF8, 0, arg2, len2, (LPWSTR)a2p, a2len / 2);
        if (!r)
        {
          ereport(ERROR, (errmsg("could not convert string to UTF-16: error code %lu", GetLastError())));
        }
      }
      ((LPWSTR)a2p)[r] = 0;

      errno = 0;
#ifdef HAVE_LOCALE_T
      if (mylocale)
      {
        result = wcscoll_l((LPWSTR)a1p, (LPWSTR)a2p, mylocale->info.lt);
      }
      else
#endif
        result = wcscoll((LPWSTR)a1p, (LPWSTR)a2p);
      if (result == 2147483647)                                     
                                             
      {
        ereport(ERROR, (errmsg("could not compare Unicode strings: %m")));
      }

                                   
      if (result == 0 && (!mylocale || mylocale->deterministic))
      {
        result = memcmp(arg1, arg2, Min(len1, len2));
        if ((result == 0) && (len1 != len2))
        {
          result = (len1 < len2) ? -1 : 1;
        }
      }

      if (a1p != a1buf)
      {
        pfree(a1p);
      }
      if (a2p != a2buf)
      {
        pfree(a2p);
      }

      return result;
    }
#endif            

    if (len1 >= TEXTBUFLEN)
    {
      a1p = (char *)palloc(len1 + 1);
    }
    else
    {
      a1p = a1buf;
    }
    if (len2 >= TEXTBUFLEN)
    {
      a2p = (char *)palloc(len2 + 1);
    }
    else
    {
      a2p = a2buf;
    }

    memcpy(a1p, arg1, len1);
    a1p[len1] = '\0';
    memcpy(a2p, arg2, len2);
    a2p[len2] = '\0';

    if (mylocale)
    {
      if (mylocale->provider == COLLPROVIDER_ICU)
      {
#ifdef USE_ICU
#ifdef HAVE_UCOL_STRCOLLUTF8
        if (GetDatabaseEncoding() == PG_UTF8)
        {
          UErrorCode status;

          status = U_ZERO_ERROR;
          result = ucol_strcollUTF8(mylocale->info.icu.ucol, arg1, len1, arg2, len2, &status);
          if (U_FAILURE(status))
          {
            ereport(ERROR, (errmsg("collation failed: %s", u_errorName(status))));
          }
        }
        else
#endif
        {
          int32_t ulen1, ulen2;
          UChar *uchar1, *uchar2;

          ulen1 = icu_to_uchar(&uchar1, arg1, len1);
          ulen2 = icu_to_uchar(&uchar2, arg2, len2);

          result = ucol_strcoll(mylocale->info.icu.ucol, uchar1, ulen1, uchar2, ulen2);

          pfree(uchar1);
          pfree(uchar2);
        }
#else                   
                              
        elog(ERROR, "unsupported collprovider: %c", mylocale->provider);
#endif                  
      }
      else
      {
#ifdef HAVE_LOCALE_T
        result = strcoll_l(a1p, a2p, mylocale->info.lt);
#else
                              
        elog(ERROR, "unsupported collprovider: %c", mylocale->provider);
#endif
      }
    }
    else
    {
      result = strcoll(a1p, a2p);
    }

                                 
    if (result == 0 && (!mylocale || mylocale->deterministic))
    {
      result = strcmp(a1p, a2p);
    }

    if (a1p != a1buf)
    {
      pfree(a1p);
    }
    if (a2p != a2buf)
    {
      pfree(a2p);
    }
  }

  return result;
}

              
                                                  
                      
   
static int
text_cmp(text *arg1, text *arg2, Oid collid)
{
  char *a1p, *a2p;
  int len1, len2;

  a1p = VARDATA_ANY(arg1);
  a2p = VARDATA_ANY(arg2);

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  return varstr_cmp(a1p, len1, a2p, len2, collid);
}

   
                                          
   
                                                                          
                                                                           
                          
   

Datum
texteq(PG_FUNCTION_ARGS)
{
  Oid collid = PG_GET_COLLATION();
  bool result;

  check_collation_set(collid);

  if (lc_collate_is_c(collid) || collid == DEFAULT_COLLATION_OID || pg_newlocale_from_collation(collid)->deterministic)
  {
    Datum arg1 = PG_GETARG_DATUM(0);
    Datum arg2 = PG_GETARG_DATUM(1);
    Size len1, len2;

       
                                                                           
                                                                          
                                                                          
                                                                        
                                             
       
    len1 = toast_raw_datum_size(arg1);
    len2 = toast_raw_datum_size(arg2);
    if (len1 != len2)
    {
      result = false;
    }
    else
    {
      text *targ1 = DatumGetTextPP(arg1);
      text *targ2 = DatumGetTextPP(arg2);

      result = (memcmp(VARDATA_ANY(targ1), VARDATA_ANY(targ2), len1 - VARHDRSZ) == 0);

      PG_FREE_IF_COPY(targ1, 0);
      PG_FREE_IF_COPY(targ2, 1);
    }
  }
  else
  {
    text *arg1 = PG_GETARG_TEXT_PP(0);
    text *arg2 = PG_GETARG_TEXT_PP(1);

    result = (text_cmp(arg1, arg2, collid) == 0);

    PG_FREE_IF_COPY(arg1, 0);
    PG_FREE_IF_COPY(arg2, 1);
  }

  PG_RETURN_BOOL(result);
}

Datum
textne(PG_FUNCTION_ARGS)
{
  Oid collid = PG_GET_COLLATION();
  bool result;

  check_collation_set(collid);

  if (lc_collate_is_c(collid) || collid == DEFAULT_COLLATION_OID || pg_newlocale_from_collation(collid)->deterministic)
  {
    Datum arg1 = PG_GETARG_DATUM(0);
    Datum arg2 = PG_GETARG_DATUM(1);
    Size len1, len2;

                                 
    len1 = toast_raw_datum_size(arg1);
    len2 = toast_raw_datum_size(arg2);
    if (len1 != len2)
    {
      result = true;
    }
    else
    {
      text *targ1 = DatumGetTextPP(arg1);
      text *targ2 = DatumGetTextPP(arg2);

      result = (memcmp(VARDATA_ANY(targ1), VARDATA_ANY(targ2), len1 - VARHDRSZ) != 0);

      PG_FREE_IF_COPY(targ1, 0);
      PG_FREE_IF_COPY(targ2, 1);
    }
  }
  else
  {
    text *arg1 = PG_GETARG_TEXT_PP(0);
    text *arg2 = PG_GETARG_TEXT_PP(1);

    result = (text_cmp(arg1, arg2, collid) != 0);

    PG_FREE_IF_COPY(arg1, 0);
    PG_FREE_IF_COPY(arg2, 1);
  }

  PG_RETURN_BOOL(result);
}

Datum
text_lt(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  bool result;

  result = (text_cmp(arg1, arg2, PG_GET_COLLATION()) < 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
text_le(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  bool result;

  result = (text_cmp(arg1, arg2, PG_GET_COLLATION()) <= 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
text_gt(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  bool result;

  result = (text_cmp(arg1, arg2, PG_GET_COLLATION()) > 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
text_ge(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  bool result;

  result = (text_cmp(arg1, arg2, PG_GET_COLLATION()) >= 0);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
text_starts_with(PG_FUNCTION_ARGS)
{
  Datum arg1 = PG_GETARG_DATUM(0);
  Datum arg2 = PG_GETARG_DATUM(1);
  Oid collid = PG_GET_COLLATION();
  pg_locale_t mylocale = 0;
  bool result;
  Size len1, len2;

  check_collation_set(collid);

  if (!lc_collate_is_c(collid) && collid != DEFAULT_COLLATION_OID)
  {
    mylocale = pg_newlocale_from_collation(collid);
  }

  if (mylocale && !mylocale->deterministic)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations are not supported for substring searches")));
  }

  len1 = toast_raw_datum_size(arg1);
  len2 = toast_raw_datum_size(arg2);
  if (len2 > len1)
  {
    result = false;
  }
  else
  {
    text *targ1 = text_substring(arg1, 1, len2, false);
    text *targ2 = DatumGetTextPP(arg2);

    result = (memcmp(VARDATA_ANY(targ1), VARDATA_ANY(targ2), VARSIZE_ANY_EXHDR(targ2)) == 0);

    PG_FREE_IF_COPY(targ1, 0);
    PG_FREE_IF_COPY(targ2, 1);
  }

  PG_RETURN_BOOL(result);
}

Datum
bttextcmp(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int32 result;

  result = text_cmp(arg1, arg2, PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(result);
}

Datum
bttextsortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);
  Oid collid = ssup->ssup_collation;
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

                                      
  varstr_sortsupport(ssup, TEXTOID, collid);

  MemoryContextSwitchTo(oldcontext);

  PG_RETURN_VOID();
}

   
                                                                        
                                                                            
                                       
   
                                                                               
                                                                       
                                                                                
                                                        
   
void
varstr_sortsupport(SortSupport ssup, Oid typid, Oid collid)
{
  bool abbreviate = ssup->abbreviate;
  bool collate_c = false;
  VarStringSortSupport *sss;
  pg_locale_t locale = 0;

  check_collation_set(collid);

     
                                                                          
                                                                      
                                                                           
                         
     
                                                                        
                                                                       
                                                                         
                                                                
                                                                          
                                     
     
  if (lc_collate_is_c(collid))
  {
    if (typid == BPCHAROID)
    {
      ssup->comparator = bpcharfastcmp_c;
    }
    else if (typid == NAMEOID)
    {
      ssup->comparator = namefastcmp_c;
                                                               
      abbreviate = false;
    }
    else
    {
      ssup->comparator = varstrfastcmp_c;
    }

    collate_c = true;
  }
  else
  {
       
                                                                         
                                                                           
               
       
    if (collid != DEFAULT_COLLATION_OID)
    {
      locale = pg_newlocale_from_collation(collid);
    }

       
                                                                   
                                                                       
                                                                      
                                                                           
                                                                       
                                                                        
       
#ifdef WIN32
    if (GetDatabaseEncoding() == PG_UTF8 && !(locale && locale->provider == COLLPROVIDER_ICU))
    {
      return;
    }
#endif

       
                                                          
       
    if (typid == NAMEOID)
    {
      ssup->comparator = namefastcmp_locale;
                                                               
      abbreviate = false;
    }
    else
    {
      ssup->comparator = varlenafastcmp_locale;
    }
  }

     
                                                                       
                                                                            
                                                                           
                                                                        
                                                                           
                                                                         
                                                                         
                                             
     
                                                                          
                                                                           
                                                                    
                                                                          
                                                                        
                                                                       
                                                                          
                                                                   
                                                                           
                
     
#ifndef TRUST_STRXFRM
  if (!collate_c && !(locale && locale->provider == COLLPROVIDER_ICU))
  {
    abbreviate = false;
  }
#endif

     
                                                                       
                                                                         
                                                                         
                                                                        
                                                                   
     
  if (abbreviate || !collate_c)
  {
    sss = palloc(sizeof(VarStringSortSupport));
    sss->buf1 = palloc(TEXTBUFLEN);
    sss->buflen1 = TEXTBUFLEN;
    sss->buf2 = palloc(TEXTBUFLEN);
    sss->buflen2 = TEXTBUFLEN;
                                   
    sss->last_len1 = -1;
    sss->last_len2 = -1;
                    
    sss->last_returned = 0;
    sss->locale = locale;

       
                                                                           
                                                                       
                          
       
                                                                          
                                                                         
                                                                   
                                                                         
                                                                           
                                             
       
                                                  
       
    sss->cache_blob = true;
    sss->collate_c = collate_c;
    sss->typid = typid;
    ssup->ssup_extra = sss;

       
                                                                        
                                                                    
                                
       
    if (abbreviate)
    {
      sss->prop_card = 0.20;
      initHyperLogLog(&sss->abbr_card, 10);
      initHyperLogLog(&sss->full_card, 10);
      ssup->abbrev_full_comparator = ssup->comparator;
      ssup->comparator = varstrcmp_abbrev;
      ssup->abbrev_converter = varstr_abbrev_convert;
      ssup->abbrev_abort = varstr_abbrev_abort;
    }
  }
}

   
                                                   
   
static int
varstrfastcmp_c(Datum x, Datum y, SortSupport ssup)
{
  VarString *arg1 = DatumGetVarStringPP(x);
  VarString *arg2 = DatumGetVarStringPP(y);
  char *a1p, *a2p;
  int len1, len2, result;

  a1p = VARDATA_ANY(arg1);
  a2p = VARDATA_ANY(arg2);

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  result = memcmp(a1p, a2p, Min(len1, len2));
  if ((result == 0) && (len1 != len2))
  {
    result = (len1 < len2) ? -1 : 1;
  }

                                            
  if (PointerGetDatum(arg1) != x)
  {
    pfree(arg1);
  }
  if (PointerGetDatum(arg2) != y)
  {
    pfree(arg2);
  }

  return result;
}

   
                                                          
   
                                                                             
                                              
                                      
   
static int
bpcharfastcmp_c(Datum x, Datum y, SortSupport ssup)
{
  BpChar *arg1 = DatumGetBpCharPP(x);
  BpChar *arg2 = DatumGetBpCharPP(y);
  char *a1p, *a2p;
  int len1, len2, result;

  a1p = VARDATA_ANY(arg1);
  a2p = VARDATA_ANY(arg2);

  len1 = bpchartruelen(a1p, VARSIZE_ANY_EXHDR(arg1));
  len2 = bpchartruelen(a2p, VARSIZE_ANY_EXHDR(arg2));

  result = memcmp(a1p, a2p, Min(len1, len2));
  if ((result == 0) && (len1 != len2))
  {
    result = (len1 < len2) ? -1 : 1;
  }

                                            
  if (PointerGetDatum(arg1) != x)
  {
    pfree(arg1);
  }
  if (PointerGetDatum(arg2) != y)
  {
    pfree(arg2);
  }

  return result;
}

   
                                                        
   
static int
namefastcmp_c(Datum x, Datum y, SortSupport ssup)
{
  Name arg1 = DatumGetName(x);
  Name arg2 = DatumGetName(y);

  return strncmp(NameStr(*arg1), NameStr(*arg2), NAMEDATALEN);
}

   
                                                                        
   
static int
varlenafastcmp_locale(Datum x, Datum y, SortSupport ssup)
{
  VarString *arg1 = DatumGetVarStringPP(x);
  VarString *arg2 = DatumGetVarStringPP(y);
  char *a1p, *a2p;
  int len1, len2, result;

  a1p = VARDATA_ANY(arg1);
  a2p = VARDATA_ANY(arg2);

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  result = varstrfastcmp_locale(a1p, len1, a2p, len2, ssup);

                                            
  if (PointerGetDatum(arg1) != x)
  {
    pfree(arg1);
  }
  if (PointerGetDatum(arg2) != y)
  {
    pfree(arg2);
  }

  return result;
}

   
                                                                
   
static int
namefastcmp_locale(Datum x, Datum y, SortSupport ssup)
{
  Name arg1 = DatumGetName(x);
  Name arg2 = DatumGetName(y);

  return varstrfastcmp_locale(NameStr(*arg1), strlen(NameStr(*arg1)), NameStr(*arg2), strlen(NameStr(*arg2)), ssup);
}

   
                                                
   
static int
varstrfastcmp_locale(char *a1p, int len1, char *a2p, int len2, SortSupport ssup)
{
  VarStringSortSupport *sss = (VarStringSortSupport *)ssup->ssup_extra;
  int result;
  bool arg1_match;

                                                                 
  if (len1 == len2 && memcmp(a1p, a2p, len1) == 0)
  {
       
                                                                          
                                                                       
                  
       
                                                                       
                                                                  
                                                                       
                                                                       
                                                                          
                                                                        
                                                             
       
    return 0;
  }

  if (sss->typid == BPCHAROID)
  {
                                                            
    len1 = bpchartruelen(a1p, len1);
    len2 = bpchartruelen(a2p, len2);
  }

  if (len1 >= sss->buflen1)
  {
    sss->buflen1 = Max(len1 + 1, Min(sss->buflen1 * 2, MaxAllocSize));
    sss->buf1 = repalloc(sss->buf1, sss->buflen1);
  }
  if (len2 >= sss->buflen2)
  {
    sss->buflen2 = Max(len2 + 1, Min(sss->buflen2 * 2, MaxAllocSize));
    sss->buf2 = repalloc(sss->buf2, sss->buflen2);
  }

     
                                                                          
                                                                             
                                                                          
                                                                          
                                                                             
                                                                           
                                                                             
                                                         
     
  arg1_match = true;
  if (len1 != sss->last_len1 || memcmp(sss->buf1, a1p, len1) != 0)
  {
    arg1_match = false;
    memcpy(sss->buf1, a1p, len1);
    sss->buf1[len1] = '\0';
    sss->last_len1 = len1;
  }

     
                                                                             
                                                                            
                                                                        
                                                            
     
  if (len2 != sss->last_len2 || memcmp(sss->buf2, a2p, len2) != 0)
  {
    memcpy(sss->buf2, a2p, len2);
    sss->buf2[len2] = '\0';
    sss->last_len2 = len2;
  }
  else if (arg1_match && !sss->cache_blob)
  {
                                                                
    return sss->last_returned;
  }

  if (sss->locale)
  {
    if (sss->locale->provider == COLLPROVIDER_ICU)
    {
#ifdef USE_ICU
#ifdef HAVE_UCOL_STRCOLLUTF8
      if (GetDatabaseEncoding() == PG_UTF8)
      {
        UErrorCode status;

        status = U_ZERO_ERROR;
        result = ucol_strcollUTF8(sss->locale->info.icu.ucol, a1p, len1, a2p, len2, &status);
        if (U_FAILURE(status))
        {
          ereport(ERROR, (errmsg("collation failed: %s", u_errorName(status))));
        }
      }
      else
#endif
      {
        int32_t ulen1, ulen2;
        UChar *uchar1, *uchar2;

        ulen1 = icu_to_uchar(&uchar1, a1p, len1);
        ulen2 = icu_to_uchar(&uchar2, a2p, len2);

        result = ucol_strcoll(sss->locale->info.icu.ucol, uchar1, ulen1, uchar2, ulen2);

        pfree(uchar1);
        pfree(uchar2);
      }
#else                   
                            
      elog(ERROR, "unsupported collprovider: %c", sss->locale->provider);
#endif                  
    }
    else
    {
#ifdef HAVE_LOCALE_T
      result = strcoll_l(sss->buf1, sss->buf2, sss->locale->info.lt);
#else
                            
      elog(ERROR, "unsupported collprovider: %c", sss->locale->provider);
#endif
    }
  }
  else
  {
    result = strcoll(sss->buf1, sss->buf2);
  }

                               
  if (result == 0 && (!sss->locale || sss->locale->deterministic))
  {
    result = strcmp(sss->buf1, sss->buf2);
  }

                                                                          
  sss->cache_blob = false;
  sss->last_returned = result;
  return result;
}

   
                                   
   
static int
varstrcmp_abbrev(Datum x, Datum y, SortSupport ssup)
{
     
                                                                     
                                                                            
                                                                              
                                                                    
                                                   
     
  if (x > y)
  {
    return 1;
  }
  else if (x == y)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}

   
                                                                             
                                                                              
                                                                                
                                                                                
                                                                             
   
static Datum
varstr_abbrev_convert(Datum original, SortSupport ssup)
{
  VarStringSortSupport *sss = (VarStringSortSupport *)ssup->ssup_extra;
  VarString *authoritative = DatumGetVarStringPP(original);
  char *authoritative_data = VARDATA_ANY(authoritative);

                     
  Datum res;
  char *pres;
  int len;
  uint32 hash;

  pres = (char *)&res;
                                                      
  memset(pres, 0, sizeof(Datum));
  len = VARSIZE_ANY_EXHDR(authoritative);

                                                     
  if (sss->typid == BPCHAROID)
  {
    len = bpchartruelen(authoritative_data, len);
  }

     
                                                                             
                                                                      
                                                                           
                                                                             
                                                                      
                                                                          
     
                                                                        
                                                                            
                                                                          
                                                                          
                                                                        
                                                                             
                                                                        
                                                                             
                                                                         
           
     
                                                                   
                                                                      
                                                                     
                                                                            
                                                                            
                                                                        
                                               
     
  if (sss->collate_c)
  {
    memcpy(pres, authoritative_data, Min(len, sizeof(Datum)));
  }
  else
  {
    Size bsize;
#ifdef USE_ICU
    int32_t ulen = -1;
    UChar *uchar = NULL;
#endif

       
                                                                       
                
       

                                                                   
    if (len >= sss->buflen1)
    {
      sss->buflen1 = Max(len + 1, Min(sss->buflen1 * 2, MaxAllocSize));
      sss->buf1 = repalloc(sss->buf1, sss->buflen1);
    }

                                                              
    if (sss->last_len1 == len && sss->cache_blob && memcmp(sss->buf1, authoritative_data, len) == 0)
    {
      memcpy(pres, sss->buf2, Min(sizeof(Datum), sss->last_len2));
                                                                   
      goto done;
    }

    memcpy(sss->buf1, authoritative_data, len);

       
                                                                           
                                            
       
    sss->buf1[len] = '\0';
    sss->last_len1 = len;

#ifdef USE_ICU
                                                               
    if (sss->locale && sss->locale->provider == COLLPROVIDER_ICU && GetDatabaseEncoding() != PG_UTF8)
    {
      ulen = icu_to_uchar(&uchar, sss->buf1, len);
    }
#endif

       
                                                                           
                                                                      
                                                                        
                                                                           
                                                                        
                                                  
       
    for (;;)
    {
#ifdef USE_ICU
      if (sss->locale && sss->locale->provider == COLLPROVIDER_ICU)
      {
           
                                                                   
                                                              
           
        if (GetDatabaseEncoding() == PG_UTF8)
        {
          UCharIterator iter;
          uint32_t state[2];
          UErrorCode status;

          uiter_setUTF8(&iter, sss->buf1, len);
          state[0] = state[1] = 0;                            
          status = U_ZERO_ERROR;
          bsize = ucol_nextSortKeyPart(sss->locale->info.icu.ucol, &iter, state, (uint8_t *)sss->buf2, Min(sizeof(Datum), sss->buflen2), &status);
          if (U_FAILURE(status))
          {
            ereport(ERROR, (errmsg("sort key generation failed: %s", u_errorName(status))));
          }
        }
        else
        {
          bsize = ucol_getSortKey(sss->locale->info.icu.ucol, uchar, ulen, (uint8_t *)sss->buf2, sss->buflen2);
        }
      }
      else
#endif
#ifdef HAVE_LOCALE_T
          if (sss->locale && sss->locale->provider == COLLPROVIDER_LIBC)
        bsize = strxfrm_l(sss->buf2, sss->buf1, sss->buflen2, sss->locale->info.lt);
      else
#endif
        bsize = strxfrm(sss->buf2, sss->buf1, sss->buflen2);

      sss->last_len2 = bsize;
      if (bsize < sss->buflen2)
      {
        break;
      }

         
                                
         
      sss->buflen2 = Max(bsize + 1, Min(sss->buflen2 * 2, MaxAllocSize));
      sss->buf2 = repalloc(sss->buf2, sss->buflen2);
    }

       
                                                                      
                                                                     
                                                                       
                                           
       
                                                                       
                                                
       
    memcpy(pres, sss->buf2, Min(sizeof(Datum), bsize));

#ifdef USE_ICU
    if (uchar)
    {
      pfree(uchar);
    }
#endif
  }

     
                                                                             
                                                                            
                                                                           
                                                                        
                            
     
                                                                             
                                                                 
                                                                       
     
  hash = DatumGetUInt32(hash_any((unsigned char *)authoritative_data, Min(len, PG_CACHE_LINE_SIZE)));

  if (len > PG_CACHE_LINE_SIZE)
  {
    hash ^= DatumGetUInt32(hash_uint32((uint32)len));
  }

  addHyperLogLog(&sss->full_card, hash);

                            
#if SIZEOF_DATUM == 8
  {
    uint32 lohalf, hihalf;

    lohalf = (uint32)res;
    hihalf = (uint32)(res >> 32);
    hash = DatumGetUInt32(hash_uint32(lohalf ^ hihalf));
  }
#else                        
  hash = DatumGetUInt32(hash_uint32((uint32)res));
#endif

  addHyperLogLog(&sss->abbr_card, hash);

                                                                          
  sss->cache_blob = true;
done:

     
                                         
     
                                                                          
                                                                          
                                                                           
                                                              
     
  res = DatumBigEndianToNative(res);

                              
  if (PointerGetDatum(authoritative) != original)
  {
    pfree(authoritative);
  }

  return res;
}

   
                                                                                
                                                                               
                                                            
   
static bool
varstr_abbrev_abort(int memtupcount, SortSupport ssup)
{
  VarStringSortSupport *sss = (VarStringSortSupport *)ssup->ssup_extra;
  double abbrev_distinct, key_distinct;

  Assert(ssup->abbreviate);

                              
  if (memtupcount < 100)
  {
    return false;
  }

  abbrev_distinct = estimateHyperLogLog(&sss->abbr_card);
  key_distinct = estimateHyperLogLog(&sss->full_card);

     
                                                                        
                                                                            
                                                          
     
  if (abbrev_distinct <= 1.0)
  {
    abbrev_distinct = 1.0;
  }

  if (key_distinct <= 1.0)
  {
    key_distinct = 1.0;
  }

     
                                                                             
                                                                        
                    
     
#ifdef TRACE_SORT
  if (trace_sort)
  {
    double norm_abbrev_card = abbrev_distinct / (double)memtupcount;

    elog(LOG,
        "varstr_abbrev: abbrev_distinct after %d: %f "
        "(key_distinct: %f, norm_abbrev_card: %f, prop_card: %f)",
        memtupcount, abbrev_distinct, key_distinct, norm_abbrev_card, sss->prop_card);
  }
#endif

     
                                                                          
                                                                             
                                                                       
                                                                    
                    
     
                                                                          
                                                                            
                                                                       
                                                                            
                                                                      
                                                                            
                                                                      
                                
     
  if (abbrev_distinct > key_distinct * sss->prop_card)
  {
       
                                                                       
                                   
       
                                                                    
                                                                       
                                                                          
                                                                           
                                                                           
                                                                       
                                                                         
                                                                          
                                                                          
                                                                          
                                                                        
                                          
       
                                                                       
                                                                    
                                                                       
                                                                           
                                                                        
                                                                    
                                                  
       
    if (memtupcount > 10000)
    {
      sss->prop_card *= 0.65;
    }

    return false;
  }

     
                                  
     
                                                                        
                                                                           
                                                                       
                                                                             
                                                                             
                                                                             
                                                         
     
#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG,
        "varstr_abbrev: aborted abbreviation at %d "
        "(abbrev_distinct: %f, key_distinct: %f, prop_card: %f)",
        memtupcount, abbrev_distinct, key_distinct, sss->prop_card);
  }
#endif

  return true;
}

Datum
text_larger(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  text *result;

  result = ((text_cmp(arg1, arg2, PG_GET_COLLATION()) > 0) ? arg1 : arg2);

  PG_RETURN_TEXT_P(result);
}

Datum
text_smaller(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  text *result;

  result = ((text_cmp(arg1, arg2, PG_GET_COLLATION()) < 0) ? arg1 : arg2);

  PG_RETURN_TEXT_P(result);
}

   
                                                            
   

Datum
nameeqtext(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  size_t len1 = strlen(NameStr(*arg1));
  size_t len2 = VARSIZE_ANY_EXHDR(arg2);
  Oid collid = PG_GET_COLLATION();
  bool result;

  check_collation_set(collid);

  if (collid == C_COLLATION_OID)
  {
    result = (len1 == len2 && memcmp(NameStr(*arg1), VARDATA_ANY(arg2), len1) == 0);
  }
  else
  {
    result = (varstr_cmp(NameStr(*arg1), len1, VARDATA_ANY(arg2), len2, collid) == 0);
  }

  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
texteqname(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  Name arg2 = PG_GETARG_NAME(1);
  size_t len1 = VARSIZE_ANY_EXHDR(arg1);
  size_t len2 = strlen(NameStr(*arg2));
  Oid collid = PG_GET_COLLATION();
  bool result;

  check_collation_set(collid);

  if (collid == C_COLLATION_OID)
  {
    result = (len1 == len2 && memcmp(VARDATA_ANY(arg1), NameStr(*arg2), len1) == 0);
  }
  else
  {
    result = (varstr_cmp(VARDATA_ANY(arg1), len1, NameStr(*arg2), len2, collid) == 0);
  }

  PG_FREE_IF_COPY(arg1, 0);

  PG_RETURN_BOOL(result);
}

Datum
namenetext(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  size_t len1 = strlen(NameStr(*arg1));
  size_t len2 = VARSIZE_ANY_EXHDR(arg2);
  Oid collid = PG_GET_COLLATION();
  bool result;

  check_collation_set(collid);

  if (collid == C_COLLATION_OID)
  {
    result = !(len1 == len2 && memcmp(NameStr(*arg1), VARDATA_ANY(arg2), len1) == 0);
  }
  else
  {
    result = !(varstr_cmp(NameStr(*arg1), len1, VARDATA_ANY(arg2), len2, collid) == 0);
  }

  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result);
}

Datum
textnename(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  Name arg2 = PG_GETARG_NAME(1);
  size_t len1 = VARSIZE_ANY_EXHDR(arg1);
  size_t len2 = strlen(NameStr(*arg2));
  Oid collid = PG_GET_COLLATION();
  bool result;

  check_collation_set(collid);

  if (collid == C_COLLATION_OID)
  {
    result = !(len1 == len2 && memcmp(VARDATA_ANY(arg1), NameStr(*arg2), len1) == 0);
  }
  else
  {
    result = !(varstr_cmp(VARDATA_ANY(arg1), len1, NameStr(*arg2), len2, collid) == 0);
  }

  PG_FREE_IF_COPY(arg1, 0);

  PG_RETURN_BOOL(result);
}

Datum
btnametextcmp(PG_FUNCTION_ARGS)
{
  Name arg1 = PG_GETARG_NAME(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int32 result;

  result = varstr_cmp(NameStr(*arg1), strlen(NameStr(*arg1)), VARDATA_ANY(arg2), VARSIZE_ANY_EXHDR(arg2), PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(result);
}

Datum
bttextnamecmp(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  Name arg2 = PG_GETARG_NAME(1);
  int32 result;

  result = varstr_cmp(VARDATA_ANY(arg1), VARSIZE_ANY_EXHDR(arg1), NameStr(*arg2), strlen(NameStr(*arg2)), PG_GET_COLLATION());

  PG_FREE_IF_COPY(arg1, 0);

  PG_RETURN_INT32(result);
}

#define CmpCall(cmpfunc) DatumGetInt32(DirectFunctionCall2Coll(cmpfunc, PG_GET_COLLATION(), PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)))

Datum
namelttext(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(btnametextcmp) < 0);
}

Datum
nameletext(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(btnametextcmp) <= 0);
}

Datum
namegttext(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(btnametextcmp) > 0);
}

Datum
namegetext(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(btnametextcmp) >= 0);
}

Datum
textltname(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(bttextnamecmp) < 0);
}

Datum
textlename(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(bttextnamecmp) <= 0);
}

Datum
textgtname(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(bttextnamecmp) > 0);
}

Datum
textgename(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(CmpCall(bttextnamecmp) >= 0);
}

#undef CmpCall

   
                                                                     
                                                                        
                                                                         
                                                                  
                          
   

static int
internal_text_pattern_compare(text *arg1, text *arg2)
{
  int result;
  int len1, len2;

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

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
text_pattern_lt(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int result;

  result = internal_text_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result < 0);
}

Datum
text_pattern_le(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int result;

  result = internal_text_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result <= 0);
}

Datum
text_pattern_ge(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int result;

  result = internal_text_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result >= 0);
}

Datum
text_pattern_gt(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int result;

  result = internal_text_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL(result > 0);
}

Datum
bttext_pattern_cmp(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int result;

  result = internal_text_pattern_compare(arg1, arg2);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(result);
}

Datum
bttext_pattern_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

                                                             
  varstr_sortsupport(ssup, TEXTOID, C_COLLATION_OID);

  MemoryContextSwitchTo(oldcontext);

  PG_RETURN_VOID();
}

                                                                
                 
   
                                                                    
                                                                
   
Datum
byteaoctetlen(PG_FUNCTION_ARGS)
{
  Datum str = PG_GETARG_DATUM(0);

                                            
  PG_RETURN_INT32(toast_raw_datum_size(str) - VARHDRSZ);
}

   
              
                                                                        
              
   
                                                 
   
Datum
byteacat(PG_FUNCTION_ARGS)
{
  bytea *t1 = PG_GETARG_BYTEA_PP(0);
  bytea *t2 = PG_GETARG_BYTEA_PP(1);

  PG_RETURN_BYTEA_P(bytea_catenate(t1, t2));
}

   
                  
                                                                       
   
                                                                            
   
static bytea *
bytea_catenate(bytea *t1, bytea *t2)
{
  bytea *result;
  int len1, len2, len;
  char *ptr;

  len1 = VARSIZE_ANY_EXHDR(t1);
  len2 = VARSIZE_ANY_EXHDR(t2);

                                                         
  if (len1 < 0)
  {
    len1 = 0;
  }
  if (len2 < 0)
  {
    len2 = 0;
  }

  len = len1 + len2 + VARHDRSZ;
  result = (bytea *)palloc(len);

                                    
  SET_VARSIZE(result, len);

                                           
  ptr = VARDATA(result);
  if (len1 > 0)
  {
    memcpy(ptr, VARDATA_ANY(t1), len1);
  }
  if (len2 > 0)
  {
    memcpy(ptr + len1, VARDATA_ANY(t2), len2);
  }

  return result;
}

#define PG_STR_GET_BYTEA(str_) DatumGetByteaPP(DirectFunctionCall1(byteain, CStringGetDatum(str_)))

   
                  
                                                          
                                                     
   
          
            
                                      
                              
   
                                                                                      
                                                                            
                                                                             
                                                                         
   
Datum
bytea_substr(PG_FUNCTION_ARGS)
{
  PG_RETURN_BYTEA_P(bytea_substring(PG_GETARG_DATUM(0), PG_GETARG_INT32(1), PG_GETARG_INT32(2), false));
}

   
                         
                                                
                                                        
   
Datum
bytea_substr_no_len(PG_FUNCTION_ARGS)
{
  PG_RETURN_BYTEA_P(bytea_substring(PG_GETARG_DATUM(0), PG_GETARG_INT32(1), -1, true));
}

static bytea *
bytea_substring(Datum str, int S, int L, bool length_not_specified)
{
  int32 S1;                              
  int32 L1;                                
  int32 E;                    

     
                                                             
     
  S1 = Max(S, 1);

  if (length_not_specified)
  {
       
                                                                           
                                                                    
       
    L1 = -1;
  }
  else if (L < 0)
  {
                                                                       
    ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
    L1 = -1;                                 
  }
  else if (pg_add_s32_overflow(S, L, &E))
  {
       
                                                                        
                                            
       
    L1 = -1;
  }
  else
  {
       
                                                                       
                                                                     
               
       
    if (E < 1)
    {
      return PG_STR_GET_BYTEA("");
    }

    L1 = E - S1;
  }

     
                                                                        
                                                                           
                                                                   
     
  return DatumGetByteaPSlice(str, S1 - 1, L1);
}

   
                
                                                           
   
                                                                               
                                                                   
   
Datum
byteaoverlay(PG_FUNCTION_ARGS)
{
  bytea *t1 = PG_GETARG_BYTEA_PP(0);
  bytea *t2 = PG_GETARG_BYTEA_PP(1);
  int sp = PG_GETARG_INT32(2);                               
  int sl = PG_GETARG_INT32(3);                       

  PG_RETURN_BYTEA_P(bytea_overlay(t1, t2, sp, sl));
}

Datum
byteaoverlay_no_len(PG_FUNCTION_ARGS)
{
  bytea *t1 = PG_GETARG_BYTEA_PP(0);
  bytea *t2 = PG_GETARG_BYTEA_PP(1);
  int sp = PG_GETARG_INT32(2);                               
  int sl;

  sl = VARSIZE_ANY_EXHDR(t2);                             
  PG_RETURN_BYTEA_P(bytea_overlay(t1, t2, sp, sl));
}

static bytea *
bytea_overlay(bytea *t1, bytea *t2, int sp, int sl)
{
  bytea *result;
  bytea *s1;
  bytea *s2;
  int sp_pl_sl;

     
                                                                          
                                                                     
                                                      
     
  if (sp <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_SUBSTRING_ERROR), errmsg("negative substring length not allowed")));
  }
  if (pg_add_s32_overflow(sp, sl, &sp_pl_sl))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  s1 = bytea_substring(PointerGetDatum(t1), 1, sp - 1, false);
  s2 = bytea_substring(PointerGetDatum(t1), sp_pl_sl, -1, true);
  result = bytea_catenate(s1, t2);
  result = bytea_catenate(result, s2);

  return result;
}

   
              
                                                     
                                             
                                                 
   
Datum
byteapos(PG_FUNCTION_ARGS)
{
  bytea *t1 = PG_GETARG_BYTEA_PP(0);
  bytea *t2 = PG_GETARG_BYTEA_PP(1);
  int pos;
  int px, p;
  int len1, len2;
  char *p1, *p2;

  len1 = VARSIZE_ANY_EXHDR(t1);
  len2 = VARSIZE_ANY_EXHDR(t2);

  if (len2 <= 0)
  {
    PG_RETURN_INT32(1);                               
  }

  p1 = VARDATA_ANY(t1);
  p2 = VARDATA_ANY(t2);

  pos = 0;
  px = (len1 - len2);
  for (p = 0; p <= px; p++)
  {
    if ((*p2 == *p1) && (memcmp(p1, p2, len2) == 0))
    {
      pos = p + 1;
      break;
    };
    p1++;
  };

  PG_RETURN_INT32(pos);
}

                                                                
                
   
                                                     
                                                         
                                                                
   
Datum
byteaGetByte(PG_FUNCTION_ARGS)
{
  bytea *v = PG_GETARG_BYTEA_PP(0);
  int32 n = PG_GETARG_INT32(1);
  int len;
  int byte;

  len = VARSIZE_ANY_EXHDR(v);

  if (n < 0 || n >= len)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("index %d out of valid range, 0..%d", n, len - 1)));
  }

  byte = ((unsigned char *)VARDATA_ANY(v))[n];

  PG_RETURN_INT32(byte);
}

                                                                
               
   
                                                             
                                                 
   
                                                                
   
Datum
byteaGetBit(PG_FUNCTION_ARGS)
{
  bytea *v = PG_GETARG_BYTEA_PP(0);
  int32 n = PG_GETARG_INT32(1);
  int byteNo, bitNo;
  int len;
  int byte;

  len = VARSIZE_ANY_EXHDR(v);

                                                                       
  if (n < 0 || n >= (int64)len * 8)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("index %d out of valid range, 0..%d", n, (int)Min((int64)len * 8 - 1, INT_MAX))));
  }

  byteNo = n / 8;
  bitNo = n % 8;

  byte = ((unsigned char *)VARDATA_ANY(v))[byteNo];

  if (byte & (1 << bitNo))
  {
    PG_RETURN_INT32(1);
  }
  else
  {
    PG_RETURN_INT32(0);
  }
}

                                                                
                
   
                                                            
                                        
   
                                                                
   
Datum
byteaSetByte(PG_FUNCTION_ARGS)
{
  bytea *res = PG_GETARG_BYTEA_P_COPY(0);
  int32 n = PG_GETARG_INT32(1);
  int32 newByte = PG_GETARG_INT32(2);
  int len;

  len = VARSIZE(res) - VARHDRSZ;

  if (n < 0 || n >= len)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("index %d out of valid range, 0..%d", n, len - 1)));
  }

     
                       
     
  ((unsigned char *)VARDATA(res))[n] = newByte;

  PG_RETURN_BYTEA_P(res);
}

                                                                
               
   
                                                            
                                       
   
                                                                
   
Datum
byteaSetBit(PG_FUNCTION_ARGS)
{
  bytea *res = PG_GETARG_BYTEA_P_COPY(0);
  int32 n = PG_GETARG_INT32(1);
  int32 newBit = PG_GETARG_INT32(2);
  int len;
  int oldByte, newByte;
  int byteNo, bitNo;

  len = VARSIZE(res) - VARHDRSZ;

                                                                       
  if (n < 0 || n >= (int64)len * 8)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("index %d out of valid range, 0..%d", n, (int)Min((int64)len * 8 - 1, INT_MAX))));
  }

  byteNo = n / 8;
  bitNo = n % 8;

     
                   
     
  if (newBit != 0 && newBit != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("new bit must be 0 or 1")));
  }

     
                      
     
  oldByte = ((unsigned char *)VARDATA(res))[byteNo];

  if (newBit == 0)
  {
    newByte = oldByte & (~(1 << bitNo));
  }
  else
  {
    newByte = oldByte | (1 << bitNo);
  }

  ((unsigned char *)VARDATA(res))[byteNo] = newByte;

  PG_RETURN_BYTEA_P(res);
}

               
                                        
   
Datum
text_name(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  Name result;
  int len;

  len = VARSIZE_ANY_EXHDR(s);

                               
  if (len >= NAMEDATALEN)
  {
    len = pg_mbcliplen(VARDATA_ANY(s), len, NAMEDATALEN - 1);
  }

                                                           
  result = (Name)palloc0(NAMEDATALEN);
  memcpy(NameStr(*result), VARDATA_ANY(s), len);

  PG_RETURN_NAME(result);
}

               
                                        
   
Datum
name_text(PG_FUNCTION_ARGS)
{
  Name s = PG_GETARG_NAME(0);

  PG_RETURN_TEXT_P(cstring_to_text(NameStr(*s)));
}

   
                                                                    
   
                                                                   
                                                                       
                                                                 
                                       
   
List *
textToQualifiedNameList(text *textval)
{
  char *rawname;
  List *result = NIL;
  List *namelist;
  ListCell *l;

                                                          
                                                           
  rawname = text_to_cstring(textval);

  if (!SplitIdentifierString(rawname, '.', &namelist))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  if (namelist == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid name syntax")));
  }

  foreach (l, namelist)
  {
    char *curname = (char *)lfirst(l);

    result = lappend(result, makeString(pstrdup(curname)));
  }

  pfree(rawname);
  list_free(namelist);

  return result;
}

   
                                                                   
   
                                                                           
                                                                          
                                                                           
                                                         
   
           
                                                                      
                                                            
                                                                     
                                                                   
                     
            
                                                                           
                                                                        
   
                                                                         
   
                                                                    
                            
   
bool
SplitIdentifierString(char *rawstring, char separator, List **namelist)
{
  char *nextp = rawstring;
  bool done = false;

  *namelist = NIL;

  while (scanner_isspace(*nextp))
  {
    nextp++;                              
  }

  if (*nextp == '\0')
  {
    return true;                         
  }

                                                                    
  do
  {
    char *curname;
    char *endp;

    if (*nextp == '"')
    {
                                                                     
      curname = nextp + 1;
      for (;;)
      {
        endp = strchr(nextp + 1, '"');
        if (endp == NULL)
        {
          return false;                        
        }
        if (endp[1] != '"')
        {
          break;                               
        }
                                                                     
        memmove(endp, endp + 1, strlen(endp));
        nextp = endp;
      }
                                                    
      nextp = endp + 1;
    }
    else
    {
                                                                
      char *downname;
      int len;

      curname = nextp;
      while (*nextp && *nextp != separator && !scanner_isspace(*nextp))
      {
        nextp++;
      }
      endp = nextp;
      if (curname == nextp)
      {
        return false;                                      
      }

         
                                                                      
         
                                                                        
                                                                       
                                                                         
                                                                        
                                       
         
      len = endp - curname;
      downname = downcase_truncate_identifier(curname, len, false);
      Assert(strlen(downname) <= len);
      strncpy(curname, downname, len);                               
      pfree(downname);
    }

    while (scanner_isspace(*nextp))
    {
      nextp++;                               
    }

    if (*nextp == separator)
    {
      nextp++;
      while (scanner_isspace(*nextp))
      {
        nextp++;                                       
      }
                                                         
    }
    else if (*nextp == '\0')
    {
      done = true;
    }
    else
    {
      return false;                     
    }

                                                     
    *endp = '\0';

                                          
    truncate_identifier(curname, strlen(curname), false);

       
                                                          
       
    *namelist = lappend(*namelist, curname);

                                                    
  } while (!done);

  return true;
}

   
                                                                             
   
                                                                       
   
                                                                     
                                                                        
                                                                              
                                                                              
                                                                        
                                                                   
   
           
                                                    
                                                                     
                                                                   
                     
            
                                                             
                                                                 
   
                                                                         
   
                                                      
   
bool
SplitDirectoriesString(char *rawstring, char separator, List **namelist)
{
  char *nextp = rawstring;
  bool done = false;

  *namelist = NIL;

  while (scanner_isspace(*nextp))
  {
    nextp++;                              
  }

  if (*nextp == '\0')
  {
    return true;                         
  }

                                                                   
  do
  {
    char *curname;
    char *endp;

    if (*nextp == '"')
    {
                                                      
      curname = nextp + 1;
      for (;;)
      {
        endp = strchr(nextp + 1, '"');
        if (endp == NULL)
        {
          return false;                        
        }
        if (endp[1] != '"')
        {
          break;                               
        }
                                                                     
        memmove(endp, endp + 1, strlen(endp));
        nextp = endp;
      }
                                                    
      nextp = endp + 1;
    }
    else
    {
                                                                   
      curname = endp = nextp;
      while (*nextp && *nextp != separator)
      {
                                                                
        if (!scanner_isspace(*nextp))
        {
          endp = nextp + 1;
        }
        nextp++;
      }
      if (curname == endp)
      {
        return false;                                      
      }
    }

    while (scanner_isspace(*nextp))
    {
      nextp++;                               
    }

    if (*nextp == separator)
    {
      nextp++;
      while (scanner_isspace(*nextp))
      {
        nextp++;                                       
      }
                                                         
    }
    else if (*nextp == '\0')
    {
      done = true;
    }
    else
    {
      return false;                     
    }

                                                     
    *endp = '\0';

                                          
    if (strlen(curname) >= MAXPGPATH)
    {
      curname[MAXPGPATH - 1] = '\0';
    }

       
                                                          
       
    curname = pstrdup(curname);
    canonicalize_path(curname);
    *namelist = lappend(*namelist, curname);

                                                    
  } while (!done);

  return true;
}

   
                                                                        
   
                                                                             
                                                                              
                                                                             
                                                                           
                                                                        
                                                                        
                                                                       
                                                            
   
                                                                            
                                                                           
                               
   
                                                                           
                                                      
   
           
                                                                      
                                                            
                                                                     
                                                                   
                     
            
                                                                           
                                                                        
   
                                                                         
   
bool
SplitGUCList(char *rawstring, char separator, List **namelist)
{
  char *nextp = rawstring;
  bool done = false;

  *namelist = NIL;

  while (scanner_isspace(*nextp))
  {
    nextp++;                              
  }

  if (*nextp == '\0')
  {
    return true;                         
  }

                                                                    
  do
  {
    char *curname;
    char *endp;

    if (*nextp == '"')
    {
                                                      
      curname = nextp + 1;
      for (;;)
      {
        endp = strchr(nextp + 1, '"');
        if (endp == NULL)
        {
          return false;                        
        }
        if (endp[1] != '"')
        {
          break;                               
        }
                                                                     
        memmove(endp, endp + 1, strlen(endp));
        nextp = endp;
      }
                                                    
      nextp = endp + 1;
    }
    else
    {
                                                                
      curname = nextp;
      while (*nextp && *nextp != separator && !scanner_isspace(*nextp))
      {
        nextp++;
      }
      endp = nextp;
      if (curname == nextp)
      {
        return false;                                      
      }
    }

    while (scanner_isspace(*nextp))
    {
      nextp++;                               
    }

    if (*nextp == separator)
    {
      nextp++;
      while (scanner_isspace(*nextp))
      {
        nextp++;                                       
      }
                                                         
    }
    else if (*nextp == '\0')
    {
      done = true;
    }
    else
    {
      return false;                     
    }

                                                     
    *endp = '\0';

       
                                                          
       
    *namelist = lappend(*namelist, curname);

                                                    
  } while (!done);

  return true;
}

                                                                               
                                       
   
                                                                          
                                                                           
                          
                                                                               

Datum
byteaeq(PG_FUNCTION_ARGS)
{
  Datum arg1 = PG_GETARG_DATUM(0);
  Datum arg2 = PG_GETARG_DATUM(1);
  bool result;
  Size len1, len2;

     
                                                                          
                                           
     
  len1 = toast_raw_datum_size(arg1);
  len2 = toast_raw_datum_size(arg2);
  if (len1 != len2)
  {
    result = false;
  }
  else
  {
    bytea *barg1 = DatumGetByteaPP(arg1);
    bytea *barg2 = DatumGetByteaPP(arg2);

    result = (memcmp(VARDATA_ANY(barg1), VARDATA_ANY(barg2), len1 - VARHDRSZ) == 0);

    PG_FREE_IF_COPY(barg1, 0);
    PG_FREE_IF_COPY(barg2, 1);
  }

  PG_RETURN_BOOL(result);
}

Datum
byteane(PG_FUNCTION_ARGS)
{
  Datum arg1 = PG_GETARG_DATUM(0);
  Datum arg2 = PG_GETARG_DATUM(1);
  bool result;
  Size len1, len2;

     
                                                                          
                                           
     
  len1 = toast_raw_datum_size(arg1);
  len2 = toast_raw_datum_size(arg2);
  if (len1 != len2)
  {
    result = true;
  }
  else
  {
    bytea *barg1 = DatumGetByteaPP(arg1);
    bytea *barg2 = DatumGetByteaPP(arg2);

    result = (memcmp(VARDATA_ANY(barg1), VARDATA_ANY(barg2), len1 - VARHDRSZ) != 0);

    PG_FREE_IF_COPY(barg1, 0);
    PG_FREE_IF_COPY(barg2, 1);
  }

  PG_RETURN_BOOL(result);
}

Datum
bytealt(PG_FUNCTION_ARGS)
{
  bytea *arg1 = PG_GETARG_BYTEA_PP(0);
  bytea *arg2 = PG_GETARG_BYTEA_PP(1);
  int len1, len2;
  int cmp;

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  cmp = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL((cmp < 0) || ((cmp == 0) && (len1 < len2)));
}

Datum
byteale(PG_FUNCTION_ARGS)
{
  bytea *arg1 = PG_GETARG_BYTEA_PP(0);
  bytea *arg2 = PG_GETARG_BYTEA_PP(1);
  int len1, len2;
  int cmp;

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  cmp = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL((cmp < 0) || ((cmp == 0) && (len1 <= len2)));
}

Datum
byteagt(PG_FUNCTION_ARGS)
{
  bytea *arg1 = PG_GETARG_BYTEA_PP(0);
  bytea *arg2 = PG_GETARG_BYTEA_PP(1);
  int len1, len2;
  int cmp;

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  cmp = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL((cmp > 0) || ((cmp == 0) && (len1 > len2)));
}

Datum
byteage(PG_FUNCTION_ARGS)
{
  bytea *arg1 = PG_GETARG_BYTEA_PP(0);
  bytea *arg2 = PG_GETARG_BYTEA_PP(1);
  int len1, len2;
  int cmp;

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  cmp = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_BOOL((cmp > 0) || ((cmp == 0) && (len1 >= len2)));
}

Datum
byteacmp(PG_FUNCTION_ARGS)
{
  bytea *arg1 = PG_GETARG_BYTEA_PP(0);
  bytea *arg2 = PG_GETARG_BYTEA_PP(1);
  int len1, len2;
  int cmp;

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

  cmp = memcmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));
  if ((cmp == 0) && (len1 != len2))
  {
    cmp = (len1 < len2) ? -1 : 1;
  }

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(cmp);
}

Datum
bytea_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

                                                             
  varstr_sortsupport(ssup, BYTEAOID, C_COLLATION_OID);

  MemoryContextSwitchTo(oldcontext);

  PG_RETURN_VOID();
}

   
                        
   
                         
                                                                    
   
static void
appendStringInfoText(StringInfo str, const text *t)
{
  appendBinaryStringInfo(str, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
}

   
                
                                                          
                                        
   
                                                                 
                               
   
Datum
replace_text(PG_FUNCTION_ARGS)
{
  text *src_text = PG_GETARG_TEXT_PP(0);
  text *from_sub_text = PG_GETARG_TEXT_PP(1);
  text *to_sub_text = PG_GETARG_TEXT_PP(2);
  int src_text_len;
  int from_sub_text_len;
  TextPositionState state;
  text *ret_text;
  int chunk_len;
  char *curr_ptr;
  char *start_ptr;
  StringInfoData str;
  bool found;

  src_text_len = VARSIZE_ANY_EXHDR(src_text);
  from_sub_text_len = VARSIZE_ANY_EXHDR(from_sub_text);

                                                                  
  if (src_text_len < 1 || from_sub_text_len < 1)
  {
    PG_RETURN_TEXT_P(src_text);
  }

  text_position_setup(src_text, from_sub_text, PG_GET_COLLATION(), &state);

  found = text_position_next(&state);

                                                                    
  if (!found)
  {
    text_position_cleanup(&state);
    PG_RETURN_TEXT_P(src_text);
  }
  curr_ptr = text_position_get_match_ptr(&state);
  start_ptr = VARDATA_ANY(src_text);

  initStringInfo(&str);

  do
  {
    CHECK_FOR_INTERRUPTS();

                                                                 
    chunk_len = curr_ptr - start_ptr;
    appendBinaryStringInfo(&str, start_ptr, chunk_len);

    appendStringInfoText(&str, to_sub_text);

    start_ptr = curr_ptr + from_sub_text_len;

    found = text_position_next(&state);
    if (found)
    {
      curr_ptr = text_position_get_match_ptr(&state);
    }
  } while (found);

                          
  chunk_len = ((char *)src_text + VARSIZE_ANY(src_text)) - start_ptr;
  appendBinaryStringInfo(&str, start_ptr, chunk_len);

  text_position_cleanup(&state);

  ret_text = cstring_to_text_with_len(str.data, str.len);
  pfree(str.data);

  PG_RETURN_TEXT_P(ret_text);
}

   
                                      
   
                                                    
   
static bool
check_replace_text_has_escape_char(const text *replace_text)
{
  const char *p = VARDATA_ANY(replace_text);
  const char *p_end = p + VARSIZE_ANY_EXHDR(replace_text);

  if (pg_database_encoding_max_length() == 1)
  {
    for (; p < p_end; p++)
    {
      if (*p == '\\')
      {
        return true;
      }
    }
  }
  else
  {
    for (; p < p_end; p += pg_mblen(p))
    {
      if (*p == '\\')
      {
        return true;
      }
    }
  }

  return false;
}

   
                                
   
                                                                       
                                                                          
                                           
   
static void
appendStringInfoRegexpSubstr(StringInfo str, text *replace_text, regmatch_t *pmatch, char *start_ptr, int data_pos)
{
  const char *p = VARDATA_ANY(replace_text);
  const char *p_end = p + VARSIZE_ANY_EXHDR(replace_text);
  int eml = pg_database_encoding_max_length();

  for (;;)
  {
    const char *chunk_start = p;
    int so;
    int eo;

                                
    if (eml == 1)
    {
      for (; p < p_end && *p != '\\'; p++)
                     ;
    }
    else
    {
      for (; p < p_end && *p != '\\'; p += pg_mblen(p))
                     ;
    }

                                                     
    if (p > chunk_start)
    {
      appendBinaryStringInfo(str, chunk_start, p - chunk_start);
    }

                                                                  
    if (p >= p_end)
    {
      break;
    }
    p++;

    if (p >= p_end)
    {
                                                                       
      appendStringInfoChar(str, '\\');
      break;
    }

    if (*p >= '1' && *p <= '9')
    {
                                             
      int idx = *p - '0';

      so = pmatch[idx].rm_so;
      eo = pmatch[idx].rm_eo;
      p++;
    }
    else if (*p == '&')
    {
                                          
      so = pmatch[0].rm_so;
      eo = pmatch[0].rm_eo;
      p++;
    }
    else if (*p == '\\')
    {
                                              
      appendStringInfoChar(str, '\\');
      p++;
      continue;
    }
    else
    {
         
                                                                         
                                                                        
                    
         
      appendStringInfoChar(str, '\\');
      continue;
    }

    if (so != -1 && eo != -1)
    {
         
                                                                         
                                              
         
      char *chunk_start;
      int chunk_len;

      Assert(so >= data_pos);
      chunk_start = start_ptr;
      chunk_start += charlen_to_bytelen(chunk_start, so - data_pos);
      chunk_len = charlen_to_bytelen(chunk_start, eo - so);
      appendBinaryStringInfo(str, chunk_start, chunk_len);
    }
  }
}

#define REGEXP_REPLACE_BACKREF_CNT 10

   
                       
   
                                                                    
   
                                                                      
                                                             
   
text *
replace_text_regexp(text *src_text, void *regexp, text *replace_text, bool glob)
{
  text *ret_text;
  regex_t *re = (regex_t *)regexp;
  int src_text_len = VARSIZE_ANY_EXHDR(src_text);
  StringInfoData buf;
  regmatch_t pmatch[REGEXP_REPLACE_BACKREF_CNT];
  pg_wchar *data;
  size_t data_len;
  int search_start;
  int data_pos;
  char *start_ptr;
  bool have_escape;

  initStringInfo(&buf);

                                               
  data = (pg_wchar *)palloc((src_text_len + 1) * sizeof(pg_wchar));
  data_len = pg_mb2wchar_with_len(VARDATA_ANY(src_text), data, src_text_len);

                                                   
  have_escape = check_replace_text_has_escape_char(replace_text);

                                                                 
  start_ptr = (char *)VARDATA_ANY(src_text);
  data_pos = 0;

  search_start = 0;
  while (search_start <= data_len)
  {
    int regexec_result;

    CHECK_FOR_INTERRUPTS();

    regexec_result = pg_regexec(re, data, data_len, search_start, NULL,                 
        REGEXP_REPLACE_BACKREF_CNT, pmatch, 0);

    if (regexec_result == REG_NOMATCH)
    {
      break;
    }

    if (regexec_result != REG_OKAY)
    {
      char errMsg[100];

      CHECK_FOR_INTERRUPTS();
      pg_regerror(regexec_result, re, errMsg, sizeof(errMsg));
      ereport(ERROR, (errcode(ERRCODE_INVALID_REGULAR_EXPRESSION), errmsg("regular expression failed: %s", errMsg)));
    }

       
                                                                           
                                   
       
    if (pmatch[0].rm_so - data_pos > 0)
    {
      int chunk_len;

      chunk_len = charlen_to_bytelen(start_ptr, pmatch[0].rm_so - data_pos);
      appendBinaryStringInfo(&buf, start_ptr, chunk_len);

         
                                                                        
                                                                   
         
      start_ptr += chunk_len;
      data_pos = pmatch[0].rm_so;
    }

       
                                                               
                                           
       
    if (have_escape)
    {
      appendStringInfoRegexpSubstr(&buf, replace_text, pmatch, start_ptr, data_pos);
    }
    else
    {
      appendStringInfoText(&buf, replace_text);
    }

                                                               
    start_ptr += charlen_to_bytelen(start_ptr, pmatch[0].rm_eo - data_pos);
    data_pos = pmatch[0].rm_eo;

       
                                                                   
       
    if (!glob)
    {
      break;
    }

       
                                                                          
                                                                          
                                                                          
              
       
    search_start = data_pos;
    if (pmatch[0].rm_so == pmatch[0].rm_eo)
    {
      search_start++;
    }
  }

     
                                                   
     
  if (data_pos < data_len)
  {
    int chunk_len;

    chunk_len = ((char *)src_text + VARSIZE_ANY(src_text)) - start_ptr;
    appendBinaryStringInfo(&buf, start_ptr, chunk_len);
  }

  ret_text = cstring_to_text_with_len(buf.data, buf.len);
  pfree(buf.data);
  pfree(data);

  return ret_text;
}

   
              
                      
                             
                                     
   
Datum
split_text(PG_FUNCTION_ARGS)
{
  text *inputstring = PG_GETARG_TEXT_PP(0);
  text *fldsep = PG_GETARG_TEXT_PP(1);
  int fldnum = PG_GETARG_INT32(2);
  int inputstring_len;
  int fldsep_len;
  TextPositionState state;
  char *start_ptr;
  char *end_ptr;
  text *result_text;
  bool found;

                               
  if (fldnum < 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("field position must be greater than zero")));
  }

  inputstring_len = VARSIZE_ANY_EXHDR(inputstring);
  fldsep_len = VARSIZE_ANY_EXHDR(fldsep);

                                                  
  if (inputstring_len < 1)
  {
    PG_RETURN_TEXT_P(cstring_to_text(""));
  }

                             
  if (fldsep_len < 1)
  {
    text_position_cleanup(&state);
                                                                
    if (fldnum == 1)
    {
      PG_RETURN_TEXT_P(inputstring);
    }
    else
    {
      PG_RETURN_TEXT_P(cstring_to_text(""));
    }
  }

  text_position_setup(inputstring, fldsep, PG_GET_COLLATION(), &state);

                                      
  start_ptr = VARDATA_ANY(inputstring);
  found = text_position_next(&state);

                                               
  if (!found)
  {
    text_position_cleanup(&state);
                                                                      
    if (fldnum == 1)
    {
      PG_RETURN_TEXT_P(inputstring);
    }
    else
    {
      PG_RETURN_TEXT_P(cstring_to_text(""));
    }
  }
  end_ptr = text_position_get_match_ptr(&state);

  while (found && --fldnum > 0)
  {
                                       
    start_ptr = end_ptr + fldsep_len;
    found = text_position_next(&state);
    if (found)
    {
      end_ptr = text_position_get_match_ptr(&state);
    }
  }

  text_position_cleanup(&state);

  if (fldnum > 0)
  {
                                        
                                                               
    if (fldnum == 1)
    {
      int last_len = start_ptr - VARDATA_ANY(inputstring);

      result_text = cstring_to_text_with_len(start_ptr, inputstring_len - last_len);
    }
    else
    {
      result_text = cstring_to_text("");
    }
  }
  else
  {
                                  
    result_text = cstring_to_text_with_len(start_ptr, end_ptr - start_ptr);
  }

  PG_RETURN_TEXT_P(result_text);
}

   
                                                                       
   
static bool
text_isequal(text *txt1, text *txt2, Oid collid)
{
  return DatumGetBool(DirectFunctionCall2Coll(texteq, collid, PointerGetDatum(txt1), PointerGetDatum(txt2)));
}

   
                 
                                                         
                                     
   
Datum
text_to_array(PG_FUNCTION_ARGS)
{
  return text_to_array_internal(fcinfo);
}

   
                      
                                                         
                                                     
   
                                                                            
                                                                             
   
Datum
text_to_array_null(PG_FUNCTION_ARGS)
{
  return text_to_array_internal(fcinfo);
}

   
                                                                  
   
                                                                       
   
static Datum
text_to_array_internal(PG_FUNCTION_ARGS)
{
  text *inputstring;
  text *fldsep;
  text *null_string;
  int inputstring_len;
  int fldsep_len;
  char *start_ptr;
  text *result_text;
  bool is_null;
  ArrayBuildState *astate = NULL;

                                                          
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

  inputstring = PG_GETARG_TEXT_PP(0);

                          
  if (!PG_ARGISNULL(1))
  {
    fldsep = PG_GETARG_TEXT_PP(1);
  }
  else
  {
    fldsep = NULL;
  }

                                          
  if (PG_NARGS() > 2 && !PG_ARGISNULL(2))
  {
    null_string = PG_GETARG_TEXT_PP(2);
  }
  else
  {
    null_string = NULL;
  }

  if (fldsep != NULL)
  {
       
                                                                          
                                            
       
    TextPositionState state;

    inputstring_len = VARSIZE_ANY_EXHDR(inputstring);
    fldsep_len = VARSIZE_ANY_EXHDR(fldsep);

                                                   
    if (inputstring_len < 1)
    {
      PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
    }

       
                                                                       
             
       
    if (fldsep_len < 1)
    {
      Datum elems[1];
      bool nulls[1];
      int dims[1];
      int lbs[1];

                                            
      is_null = null_string ? text_isequal(inputstring, null_string, PG_GET_COLLATION()) : false;

      elems[0] = PointerGetDatum(inputstring);
      nulls[0] = is_null;
      dims[0] = 1;
      lbs[0] = 1;
                                                               
      PG_RETURN_ARRAYTYPE_P(construct_md_array(elems, nulls, 1, dims, lbs, TEXTOID, -1, false, 'i'));
    }

    text_position_setup(inputstring, fldsep, PG_GET_COLLATION(), &state);

    start_ptr = VARDATA_ANY(inputstring);

    for (;;)
    {
      bool found;
      char *end_ptr;
      int chunk_len;

      CHECK_FOR_INTERRUPTS();

      found = text_position_next(&state);
      if (!found)
      {
                              
        chunk_len = ((char *)inputstring + VARSIZE_ANY(inputstring)) - start_ptr;
        end_ptr = NULL;                                            
      }
      else
      {
                                  
        end_ptr = text_position_get_match_ptr(&state);
        chunk_len = end_ptr - start_ptr;
      }

                                                                    
      result_text = cstring_to_text_with_len(start_ptr, chunk_len);
      is_null = null_string ? text_isequal(result_text, null_string, PG_GET_COLLATION()) : false;

                                 
      astate = accumArrayResult(astate, PointerGetDatum(result_text), is_null, TEXTOID, CurrentMemoryContext);

      pfree(result_text);

      if (!found)
      {
        break;
      }

      start_ptr = end_ptr + fldsep_len;
    }

    text_position_cleanup(&state);
  }
  else
  {
       
                                                                         
                                                                      
                                 
       
    inputstring_len = VARSIZE_ANY_EXHDR(inputstring);

                                                   
    if (inputstring_len < 1)
    {
      PG_RETURN_ARRAYTYPE_P(construct_empty_array(TEXTOID));
    }

    start_ptr = VARDATA_ANY(inputstring);

    while (inputstring_len > 0)
    {
      int chunk_len = pg_mblen(start_ptr);

      CHECK_FOR_INTERRUPTS();

                                                                    
      result_text = cstring_to_text_with_len(start_ptr, chunk_len);
      is_null = null_string ? text_isequal(result_text, null_string, PG_GET_COLLATION()) : false;

                                 
      astate = accumArrayResult(astate, PointerGetDatum(result_text), is_null, TEXTOID, CurrentMemoryContext);

      pfree(result_text);

      start_ptr += chunk_len;
      inputstring_len -= chunk_len;
    }
  }

  PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}

   
                 
                                                              
                                  
   
Datum
array_to_text(PG_FUNCTION_ARGS)
{
  ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);
  char *fldsep = text_to_cstring(PG_GETARG_TEXT_PP(1));

  PG_RETURN_TEXT_P(array_to_text_internal(fcinfo, v, fldsep, NULL));
}

   
                      
                                                              
                                                  
   
                                                                             
   
Datum
array_to_text_null(PG_FUNCTION_ARGS)
{
  ArrayType *v;
  char *fldsep;
  char *null_string;

                                                           
  if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
  {
    PG_RETURN_NULL();
  }

  v = PG_GETARG_ARRAYTYPE_P(0);
  fldsep = text_to_cstring(PG_GETARG_TEXT_PP(1));

                                                            
  if (!PG_ARGISNULL(2))
  {
    null_string = text_to_cstring(PG_GETARG_TEXT_PP(2));
  }
  else
  {
    null_string = NULL;
  }

  PG_RETURN_TEXT_P(array_to_text_internal(fcinfo, v, fldsep, null_string));
}

   
                                                                  
   
static text *
array_to_text_internal(FunctionCallInfo fcinfo, ArrayType *v, const char *fldsep, const char *null_string)
{
  text *result;
  int nitems, *dims, ndims;
  Oid element_type;
  int typlen;
  bool typbyval;
  char typalign;
  StringInfoData buf;
  bool printed = false;
  char *p;
  bits8 *bitmap;
  int bitmask;
  int i;
  ArrayMetaState *my_extra;

  ndims = ARR_NDIM(v);
  dims = ARR_DIMS(v);
  nitems = ArrayGetNItems(ndims, dims);

                                                        
  if (nitems == 0)
  {
    return cstring_to_text_with_len("", 0);
  }

  element_type = ARR_ELEMTYPE(v);
  initStringInfo(&buf);

     
                                                                         
                                                                          
                                        
     
  my_extra = (ArrayMetaState *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL)
  {
    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, sizeof(ArrayMetaState));
    my_extra = (ArrayMetaState *)fcinfo->flinfo->fn_extra;
    my_extra->element_type = ~element_type;
  }

  if (my_extra->element_type != element_type)
  {
       
                                                                         
       
    get_type_io_data(element_type, IOFunc_output, &my_extra->typlen, &my_extra->typbyval, &my_extra->typalign, &my_extra->typdelim, &my_extra->typioparam, &my_extra->typiofunc);
    fmgr_info_cxt(my_extra->typiofunc, &my_extra->proc, fcinfo->flinfo->fn_mcxt);
    my_extra->element_type = element_type;
  }
  typlen = my_extra->typlen;
  typbyval = my_extra->typbyval;
  typalign = my_extra->typalign;

  p = ARR_DATA_PTR(v);
  bitmap = ARR_NULLBITMAP(v);
  bitmask = 1;

  for (i = 0; i < nitems; i++)
  {
    Datum itemvalue;
    char *value;

                                               
    if (bitmap && (*bitmap & bitmask) == 0)
    {
                                                                
      if (null_string != NULL)
      {
        if (printed)
        {
          appendStringInfo(&buf, "%s%s", fldsep, null_string);
        }
        else
        {
          appendStringInfoString(&buf, null_string);
        }
        printed = true;
      }
    }
    else
    {
      itemvalue = fetch_att(p, typbyval, typlen);

      value = OutputFunctionCall(&my_extra->proc, itemvalue);

      if (printed)
      {
        appendStringInfo(&buf, "%s%s", fldsep, value);
      }
      else
      {
        appendStringInfoString(&buf, value);
      }
      printed = true;

      p = att_addlength_pointer(p, typlen, p);
      p = (char *)att_align_nominal(p, typalign);
    }

                                       
    if (bitmap)
    {
      bitmask <<= 1;
      if (bitmask == 0x100)
      {
        bitmap++;
        bitmask = 1;
      }
    }
  }

  result = cstring_to_text_with_len(buf.data, buf.len);
  pfree(buf.data);

  return result;
}

#define HEXBASE 16
   
                                                                             
               
   
Datum
to_hex32(PG_FUNCTION_ARGS)
{
  uint32 value = (uint32)PG_GETARG_INT32(0);
  char *ptr;
  const char *digits = "0123456789abcdef";
  char buf[32];                                         

  ptr = buf + sizeof(buf) - 1;
  *ptr = '\0';

  do
  {
    *--ptr = digits[value % HEXBASE];
    value /= HEXBASE;
  } while (ptr > buf && value);

  PG_RETURN_TEXT_P(cstring_to_text(ptr));
}

   
                                                                             
               
   
Datum
to_hex64(PG_FUNCTION_ARGS)
{
  uint64 value = (uint64)PG_GETARG_INT64(0);
  char *ptr;
  const char *digits = "0123456789abcdef";
  char buf[32];                                         

  ptr = buf + sizeof(buf) - 1;
  *ptr = '\0';

  do
  {
    *--ptr = digits[value % HEXBASE];
    value /= HEXBASE;
  } while (ptr > buf && value);

  PG_RETURN_TEXT_P(cstring_to_text(ptr));
}

   
                                                   
   
                          
   
Datum
pg_column_size(PG_FUNCTION_ARGS)
{
  Datum value = PG_GETARG_DATUM(0);
  int32 result;
  int typlen;

                                                                         
  if (fcinfo->flinfo->fn_extra == NULL)
  {
                                                      
    Oid argtypeid = get_fn_expr_argtype(fcinfo->flinfo, 0);

    typlen = get_typlen(argtypeid);
    if (typlen == 0)                        
    {
      elog(ERROR, "cache lookup failed for type %u", argtypeid);
    }

    fcinfo->flinfo->fn_extra = MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, sizeof(int));
    *((int *)fcinfo->flinfo->fn_extra) = typlen;
  }
  else
  {
    typlen = *((int *)fcinfo->flinfo->fn_extra);
  }

  if (typlen == -1)
  {
                                        
    result = toast_datum_size(value);
  }
  else if (typlen == -2)
  {
                 
    result = strlen(DatumGetCString(value)) + 1;
  }
  else
  {
                                   
    result = typlen;
  }

  PG_RETURN_INT32(result);
}

   
                                                        
   
                                                               
   
                                                                     
                                                                        
                         
   

                                    
static StringInfo
makeStringAggState(FunctionCallInfo fcinfo)
{
  StringInfo state;
  MemoryContext aggcontext;
  MemoryContext oldcontext;

  if (!AggCheckCallContext(fcinfo, &aggcontext))
  {
                                                                     
    elog(ERROR, "string_agg_transfn called in non-aggregate context");
  }

     
                                                                            
            
     
  oldcontext = MemoryContextSwitchTo(aggcontext);
  state = makeStringInfo();
  MemoryContextSwitchTo(oldcontext);

  return state;
}

Datum
string_agg_transfn(PG_FUNCTION_ARGS)
{
  StringInfo state;

  state = PG_ARGISNULL(0) ? NULL : (StringInfo)PG_GETARG_POINTER(0);

                                     
  if (!PG_ARGISNULL(1))
  {
                                                             
    if (state == NULL)
    {
      state = makeStringAggState(fcinfo);
    }
    else if (!PG_ARGISNULL(2))
    {
      appendStringInfoText(state, PG_GETARG_TEXT_PP(2));                
    }

    appendStringInfoText(state, PG_GETARG_TEXT_PP(1));            
  }

     
                                                                        
                                                               
     
  PG_RETURN_POINTER(state);
}

Datum
string_agg_finalfn(PG_FUNCTION_ARGS)
{
  StringInfo state;

                                                                   
  Assert(AggCheckCallContext(fcinfo, NULL));

  state = PG_ARGISNULL(0) ? NULL : (StringInfo)PG_GETARG_POINTER(0);

  if (state != NULL)
  {
    PG_RETURN_TEXT_P(cstring_to_text_with_len(state->data, state->len));
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                                             
                                                                              
                                                                         
                                                   
   
static FmgrInfo *
build_concat_foutcache(FunctionCallInfo fcinfo, int argidx)
{
  FmgrInfo *foutcache;
  int i;

                                                               
  foutcache = (FmgrInfo *)MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, PG_NARGS() * sizeof(FmgrInfo));

  for (i = argidx; i < PG_NARGS(); i++)
  {
    Oid valtype;
    Oid typOutput;
    bool typIsVarlena;

    valtype = get_fn_expr_argtype(fcinfo->flinfo, i);
    if (!OidIsValid(valtype))
    {
      elog(ERROR, "could not determine data type of concat() input");
    }

    getTypeOutputInfo(valtype, &typOutput, &typIsVarlena);
    fmgr_info_cxt(typOutput, &foutcache[i], fcinfo->flinfo->fn_mcxt);
  }

  fcinfo->flinfo->fn_extra = foutcache;

  return foutcache;
}

   
                                                    
   
                                                           
                                                                             
                                                                   
   
                                                           
   
static text *
concat_internal(const char *sepstr, int argidx, FunctionCallInfo fcinfo)
{
  text *result;
  StringInfoData str;
  FmgrInfo *foutcache;
  bool first_arg = true;
  int i;

     
                                                              
                                                                             
                                                
     
  if (get_fn_expr_variadic(fcinfo->flinfo))
  {
    ArrayType *arr;

                                           
    Assert(argidx == PG_NARGS() - 1);

                                                  
    if (PG_ARGISNULL(argidx))
    {
      return NULL;
    }

       
                                                                          
                                                                         
                                                                           
                                                                       
                                         
       
    Assert(OidIsValid(get_base_element_type(get_fn_expr_argtype(fcinfo->flinfo, argidx))));

                                           
    arr = PG_GETARG_ARRAYTYPE_P(argidx);

       
                                                                      
                                                               
       
    return array_to_text_internal(fcinfo, arr, sepstr, NULL);
  }

                                                    
  initStringInfo(&str);

                                                                   
  foutcache = (FmgrInfo *)fcinfo->flinfo->fn_extra;
  if (foutcache == NULL)
  {
    foutcache = build_concat_foutcache(fcinfo, argidx);
  }

  for (i = argidx; i < PG_NARGS(); i++)
  {
    if (!PG_ARGISNULL(i))
    {
      Datum value = PG_GETARG_DATUM(i);

                                        
      if (first_arg)
      {
        first_arg = false;
      }
      else
      {
        appendStringInfoString(&str, sepstr);
      }

                                                                        
      appendStringInfoString(&str, OutputFunctionCall(&foutcache[i], value));
    }
  }

  result = cstring_to_text_with_len(str.data, str.len);
  pfree(str.data);

  return result;
}

   
                                                          
   
Datum
text_concat(PG_FUNCTION_ARGS)
{
  text *result;

  result = concat_internal("", 0, fcinfo);
  if (result == NULL)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_TEXT_P(result);
}

   
                                                                       
                                                                   
   
Datum
text_concat_ws(PG_FUNCTION_ARGS)
{
  char *sep;
  text *result;

                                          
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }
  sep = text_to_cstring(PG_GETARG_TEXT_PP(0));

  result = concat_internal(sep, 1, fcinfo);
  if (result == NULL)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_TEXT_P(result);
}

   
                                                                
                                       
   
Datum
text_left(PG_FUNCTION_ARGS)
{
  int n = PG_GETARG_INT32(1);

  if (n < 0)
  {
    text *str = PG_GETARG_TEXT_PP(0);
    const char *p = VARDATA_ANY(str);
    int len = VARSIZE_ANY_EXHDR(str);
    int rlen;

    n = pg_mbstrlen_with_len(p, len) + n;
    rlen = pg_mbcharcliplen(p, len, n);
    PG_RETURN_TEXT_P(cstring_to_text_with_len(p, rlen));
  }
  else
  {
    PG_RETURN_TEXT_P(text_substring(PG_GETARG_DATUM(0), 1, n, false));
  }
}

   
                                                               
                                        
   
Datum
text_right(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  const char *p = VARDATA_ANY(str);
  int len = VARSIZE_ANY_EXHDR(str);
  int n = PG_GETARG_INT32(1);
  int off;

  if (n < 0)
  {
    n = -n;
  }
  else
  {
    n = pg_mbstrlen_with_len(p, len) - n;
  }
  off = pg_mbcharcliplen(p, len, n);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(p + off, len - off));
}

   
                          
   
Datum
text_reverse(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  const char *p = VARDATA_ANY(str);
  int len = VARSIZE_ANY_EXHDR(str);
  const char *endp = p + len;
  text *result;
  char *dst;

  result = palloc(len + VARHDRSZ);
  dst = (char *)VARDATA(result) + len;
  SET_VARSIZE(result, len + VARHDRSZ);

  if (pg_database_encoding_max_length() > 1)
  {
                           
    while (p < endp)
    {
      int sz;

      sz = pg_mblen(p);
      dst -= sz;
      memcpy(dst, p, sz);
      p += sz;
    }
  }
  else
  {
                             
    while (p < endp)
    {
      *(--dst) = *p++;
    }
  }

  PG_RETURN_TEXT_P(result);
}

   
                                    
   
#define TEXT_FORMAT_FLAG_MINUS 0x0001                             

#define ADVANCE_PARSE_POINTER(ptr, end_ptr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (++(ptr) >= (end_ptr))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unterminated format() type specifier"), errhint("For a single \"%%\" use \"%%%%\".")));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } while (0)

   
                              
   
Datum
text_format(PG_FUNCTION_ARGS)
{
  text *fmt;
  StringInfoData str;
  const char *cp;
  const char *start_ptr;
  const char *end_ptr;
  text *result;
  int arg;
  bool funcvariadic;
  int nargs;
  Datum *elements = NULL;
  bool *nulls = NULL;
  Oid element_type = InvalidOid;
  Oid prev_type = InvalidOid;
  Oid prev_width_type = InvalidOid;
  FmgrInfo typoutputfinfo;
  FmgrInfo typoutputinfo_width;

                                                           
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }

                                                                  
  if (get_fn_expr_variadic(fcinfo->flinfo))
  {
    ArrayType *arr;
    int16 elmlen;
    bool elmbyval;
    char elmalign;
    int nitems;

                                           
    Assert(PG_NARGS() == 2);

                                                               
    if (PG_ARGISNULL(1))
    {
      nitems = 0;
    }
    else
    {
         
                                                                       
                                                                      
                                                                         
                                                                     
                                                             
         
      Assert(OidIsValid(get_base_element_type(get_fn_expr_argtype(fcinfo->flinfo, 1))));

                                             
      arr = PG_GETARG_ARRAYTYPE_P(1);

                                             
      element_type = ARR_ELEMTYPE(arr);
      get_typlenbyvalalign(element_type, &elmlen, &elmbyval, &elmalign);

                                      
      deconstruct_array(arr, element_type, elmlen, elmbyval, elmalign, &elements, &nulls, &nitems);
    }

    nargs = nitems + 1;
    funcvariadic = true;
  }
  else
  {
                                                                     
    nargs = PG_NARGS();
    funcvariadic = false;
  }

                            
  fmt = PG_GETARG_TEXT_PP(0);
  start_ptr = VARDATA_ANY(fmt);
  end_ptr = start_ptr + VARSIZE_ANY_EXHDR(fmt);
  initStringInfo(&str);
  arg = 1;                                      

                                                              
  for (cp = start_ptr; cp < end_ptr; cp++)
  {
    int argpos;
    int widthpos;
    int flags;
    int width;
    Datum value;
    bool isNull;
    Oid typid;

       
                                                                        
                          
       
    if (*cp != '%')
    {
      appendStringInfoCharMacro(&str, *cp);
      continue;
    }

    ADVANCE_PARSE_POINTER(cp, end_ptr);

                                          
    if (*cp == '%')
    {
      appendStringInfoCharMacro(&str, *cp);
      continue;
    }

                                                             
    cp = text_format_parse_format(cp, end_ptr, &argpos, &widthpos, &flags, &width);

       
                                                                         
                                                                      
                                                                           
                                                                           
                                                                         
                                                        
       
    if (strchr("sIL", *cp) == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized format() type specifier \"%c\"", *cp), errhint("For a single \"%%\" use \"%%%%\".")));
    }

                                                        
    if (widthpos >= 0)
    {
                                                           
      if (widthpos > 0)
      {
        arg = widthpos;
      }
      if (arg >= nargs)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("too few arguments for format()")));
      }

                                                           
      if (!funcvariadic)
      {
        value = PG_GETARG_DATUM(arg);
        isNull = PG_ARGISNULL(arg);
        typid = get_fn_expr_argtype(fcinfo->flinfo, arg);
      }
      else
      {
        value = elements[arg - 1];
        isNull = nulls[arg - 1];
        typid = element_type;
      }
      if (!OidIsValid(typid))
      {
        elog(ERROR, "could not determine data type of format() input");
      }

      arg++;

                                                    
      if (isNull)
      {
        width = 0;
      }
      else if (typid == INT4OID)
      {
        width = DatumGetInt32(value);
      }
      else if (typid == INT2OID)
      {
        width = DatumGetInt16(value);
      }
      else
      {
                                                                   
        char *str;

        if (typid != prev_width_type)
        {
          Oid typoutputfunc;
          bool typIsVarlena;

          getTypeOutputInfo(typid, &typoutputfunc, &typIsVarlena);
          fmgr_info(typoutputfunc, &typoutputinfo_width);
          prev_width_type = typid;
        }

        str = OutputFunctionCall(&typoutputinfo_width, value);

                                                                    
        width = pg_strtoint32(str);

        pfree(str);
      }
    }

                                                         
    if (argpos > 0)
    {
      arg = argpos;
    }
    if (arg >= nargs)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("too few arguments for format()")));
    }

                                                         
    if (!funcvariadic)
    {
      value = PG_GETARG_DATUM(arg);
      isNull = PG_ARGISNULL(arg);
      typid = get_fn_expr_argtype(fcinfo->flinfo, arg);
    }
    else
    {
      value = elements[arg - 1];
      isNull = nulls[arg - 1];
      typid = element_type;
    }
    if (!OidIsValid(typid))
    {
      elog(ERROR, "could not determine data type of format() input");
    }

    arg++;

       
                                                                       
                                                                          
                                                                          
       
    if (typid != prev_type)
    {
      Oid typoutputfunc;
      bool typIsVarlena;

      getTypeOutputInfo(typid, &typoutputfunc, &typIsVarlena);
      fmgr_info(typoutputfunc, &typoutputfinfo);
      prev_type = typid;
    }

       
                                        
       
    switch (*cp)
    {
    case 's':
    case 'I':
    case 'L':
      text_format_string_conversion(&str, *cp, &typoutputfinfo, value, isNull, flags, width);
      break;
    default:
                                                          
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized format() type specifier \"%c\"", *cp), errhint("For a single \"%%\" use \"%%%%\".")));
      break;
    }
  }

                                                     
  if (elements != NULL)
  {
    pfree(elements);
  }
  if (nulls != NULL)
  {
    pfree(nulls);
  }

                         
  result = cstring_to_text_with_len(str.data, str.len);
  pfree(str.data);

  PG_RETURN_TEXT_P(result);
}

   
                                                
   
                                                
                                                                       
                           
   
                                                                            
                                                                  
   
static bool
text_format_parse_digits(const char **ptr, const char *end_ptr, int *value)
{
  bool found = false;
  const char *cp = *ptr;
  int val = 0;

  while (*cp >= '0' && *cp <= '9')
  {
    int8 digit = (*cp - '0');

    if (unlikely(pg_mul_s32_overflow(val, 10, &val)) || unlikely(pg_add_s32_overflow(val, digit, &val)))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("number is out of range")));
    }
    ADVANCE_PARSE_POINTER(cp, end_ptr);
    found = true;
  }

  *ptr = cp;
  *value = val;

  return found;
}

   
                                                                       
   
                                                                         
                                                                             
   
                                                                               
                      
                                                                             
                                                                            
                                                                   
                                                              
                            
                                                                            
                                                                        
                        
   
                                                                            
                                                   
   
                                                                            
                                                                  
   
static const char *
text_format_parse_format(const char *start_ptr, const char *end_ptr, int *argpos, int *widthpos, int *flags, int *width)
{
  const char *cp = start_ptr;
  int n;

                                          
  *argpos = -1;
  *widthpos = -1;
  *flags = 0;
  *width = 0;

                                    
  if (text_format_parse_digits(&cp, end_ptr, &n))
  {
    if (*cp != '$')
    {
                                                          
      *width = n;
      return cp;
    }
                                          
    *argpos = n;
                                                              
    if (n == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("format specifies argument 0, but arguments are numbered from 1")));
    }
    ADVANCE_PARSE_POINTER(cp, end_ptr);
  }

                                                  
  while (*cp == '-')
  {
    *flags |= TEXT_FORMAT_FLAG_MINUS;
    ADVANCE_PARSE_POINTER(cp, end_ptr);
  }

  if (*cp == '*')
  {
                               
    ADVANCE_PARSE_POINTER(cp, end_ptr);
    if (text_format_parse_digits(&cp, end_ptr, &n))
    {
                                                       
      if (*cp != '$')
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("width argument position must be ended by \"$\"")));
      }
                                                  
      *widthpos = n;
                                                                
      if (n == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("format specifies argument 0, but arguments are numbered from 1")));
      }
      ADVANCE_PARSE_POINTER(cp, end_ptr);
    }
    else
    {
      *widthpos = 0;                                               
    }
  }
  else
  {
                                              
    if (text_format_parse_digits(&cp, end_ptr, &n))
    {
      *width = n;
    }
  }

                                                   
  return cp;
}

   
                                     
   
static void
text_format_string_conversion(StringInfo buf, char conversion, FmgrInfo *typOutputInfo, Datum value, bool isNull, int flags, int width)
{
  char *str;

                                                                   
  if (isNull)
  {
    if (conversion == 's')
    {
      text_format_append_string(buf, "", flags, width);
    }
    else if (conversion == 'L')
    {
      text_format_append_string(buf, "NULL", flags, width);
    }
    else if (conversion == 'I')
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("null values cannot be formatted as an SQL identifier")));
    }
    return;
  }

                  
  str = OutputFunctionCall(typOutputInfo, value);

               
  if (conversion == 'I')
  {
                                                                
    text_format_append_string(buf, quote_identifier(str), flags, width);
  }
  else if (conversion == 'L')
  {
    char *qstr = quote_literal_cstr(str);

    text_format_append_string(buf, qstr, flags, width);
                                                            
    pfree(qstr);
  }
  else
  {
    text_format_append_string(buf, str, flags, width);
  }

                
  pfree(str);
}

   
                                                         
   
static void
text_format_append_string(StringInfo buf, const char *str, int flags, int width)
{
  bool align_to_left = false;
  int len;

                                       
  if (width == 0)
  {
    appendStringInfoString(buf, str);
    return;
  }

  if (width < 0)
  {
                                                                     
    align_to_left = true;
                               
    if (width <= INT_MIN)
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("number is out of range")));
    }
    width = -width;
  }
  else if (flags & TEXT_FORMAT_FLAG_MINUS)
  {
    align_to_left = true;
  }

  len = pg_mbstrlen(str);
  if (align_to_left)
  {
                      
    appendStringInfoString(buf, str);
    if (len < width)
    {
      appendStringInfoSpaces(buf, width - len);
    }
  }
  else
  {
                       
    if (len < width)
    {
      appendStringInfoSpaces(buf, width - len);
    }
    appendStringInfoString(buf, str);
  }
}

   
                                                                  
   
                                                                           
                                                                          
                                               
   
Datum
text_format_nv(PG_FUNCTION_ARGS)
{
  return text_format(fcinfo);
}

   
                                                                             
                      
   
static inline bool
rest_of_char_same(const char *s1, const char *s2, int len)
{
  while (len > 0)
  {
    len--;
    if (s1[len] != s2[len])
    {
      return false;
    }
  }
  return true;
}

                                              
#include "levenshtein.c"
#define LEVENSHTEIN_LESS_EQUAL
#include "levenshtein.c"
