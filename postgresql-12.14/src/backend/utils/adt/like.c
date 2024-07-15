                                                                            
   
          
                                    
   
          
                                                     
                                                   
   
                                                                         
                                                                        
   
                  
                                
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "catalog/pg_collation.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"

#define LIKE_TRUE 1
#define LIKE_FALSE 0
#define LIKE_ABORT (-1)

static int
SB_MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);
static text *
SB_do_like_escape(text *, text *);

static int
MB_MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);
static text *
MB_do_like_escape(text *, text *);

static int
UTF8_MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);

static int
SB_IMatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);

static int
GenericMatchText(const char *s, int slen, const char *p, int plen, Oid collation);
static int
Generic_Text_IC_like(text *str, text *pat, Oid collation);

                       
                                                                   
                                                                     
                       
   
static inline int
wchareq(const char *p1, const char *p2)
{
  int p1_len;

                                                      
  if (*p1 != *p2)
  {
    return 0;
  }

  p1_len = pg_mblen(p1);
  if (pg_mblen(p2) != p1_len)
  {
    return 0;
  }

                                
  while (p1_len--)
  {
    if (*p1++ != *p2++)
    {
      return 0;
    }
  }
  return 1;
}

   
                                                                               
                                                                         
                                                                      
                                                                          
                                                                             
                                                                             
                                                                                
   

   
                                                                          
                                        
   
static char
SB_lower_char(unsigned char c, pg_locale_t locale, bool locale_is_c)
{
  if (locale_is_c)
  {
    return pg_ascii_tolower(c);
  }
#ifdef HAVE_LOCALE_T
  else if (locale)
  {
    return tolower_l(c, locale->info.lt);
  }
#endif
  else
  {
    return pg_tolower(c);
  }
}

#define NextByte(p, plen) ((p)++, (plen)--)

                                                             
#define CHAREQ(p1, p2) wchareq((p1), (p2))
#define NextChar(p, plen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int __l = pg_mblen(p);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    (p) += __l;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    (plen) -= __l;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  } while (0)
#define CopyAdvChar(dst, src, srclen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int __l = pg_mblen(src);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    (srclen) -= __l;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    while (__l-- > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *(dst)++ = *(src)++;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)

#define MatchText MB_MatchText
#define do_like_escape MB_do_like_escape

#include "like_match.c"

                                                               
#define CHAREQ(p1, p2) (*(p1) == *(p2))
#define NextChar(p, plen) NextByte((p), (plen))
#define CopyAdvChar(dst, src, srclen) (*(dst)++ = *(src)++, (srclen)--)

#define MatchText SB_MatchText
#define do_like_escape SB_do_like_escape

#include "like_match.c"

                                                                            
#define MATCH_LOWER(t) SB_lower_char((unsigned char)(t), locale, locale_is_c)
#define NextChar(p, plen) NextByte((p), (plen))
#define MatchText SB_IMatchText

#include "like_match.c"

                                                                          

#define NextChar(p, plen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (p)++;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    (plen)--;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while ((plen) > 0 && (*(p) & 0xC0) == 0x80)
#define MatchText UTF8_MatchText

#include "like_match.c"

                                                             
static inline int
GenericMatchText(const char *s, int slen, const char *p, int plen, Oid collation)
{
  if (collation && !lc_ctype_is_c(collation) && collation != DEFAULT_COLLATION_OID)
  {
    pg_locale_t locale = pg_newlocale_from_collation(collation);

    if (locale && !locale->deterministic)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations are not supported for LIKE")));
    }
  }

  if (pg_database_encoding_max_length() == 1)
  {
    return SB_MatchText(s, slen, p, plen, 0, true);
  }
  else if (GetDatabaseEncoding() == PG_UTF8)
  {
    return UTF8_MatchText(s, slen, p, plen, 0, true);
  }
  else
  {
    return MB_MatchText(s, slen, p, plen, 0, true);
  }
}

static inline int
Generic_Text_IC_like(text *str, text *pat, Oid collation)
{
  char *s, *p;
  int slen, plen;
  pg_locale_t locale = 0;
  bool locale_is_c = false;

  if (lc_ctype_is_c(collation))
  {
    locale_is_c = true;
  }
  else if (collation != DEFAULT_COLLATION_OID)
  {
    if (!OidIsValid(collation))
    {
         
                                                                  
                                                                 
         
      ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for ILIKE"), errhint("Use the COLLATE clause to set the collation explicitly.")));
    }
    locale = pg_newlocale_from_collation(collation);

    if (locale && !locale->deterministic)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations are not supported for ILIKE")));
    }
  }

     
                                                                           
                                                                     
                                                                             
                                                                           
          
     

  if (pg_database_encoding_max_length() > 1 || (locale && locale->provider == COLLPROVIDER_ICU))
  {
    pat = DatumGetTextPP(DirectFunctionCall1Coll(lower, collation, PointerGetDatum(pat)));
    p = VARDATA_ANY(pat);
    plen = VARSIZE_ANY_EXHDR(pat);
    str = DatumGetTextPP(DirectFunctionCall1Coll(lower, collation, PointerGetDatum(str)));
    s = VARDATA_ANY(str);
    slen = VARSIZE_ANY_EXHDR(str);
    if (GetDatabaseEncoding() == PG_UTF8)
    {
      return UTF8_MatchText(s, slen, p, plen, 0, true);
    }
    else
    {
      return MB_MatchText(s, slen, p, plen, 0, true);
    }
  }
  else
  {
    p = VARDATA_ANY(pat);
    plen = VARSIZE_ANY_EXHDR(pat);
    s = VARDATA_ANY(str);
    slen = VARSIZE_ANY_EXHDR(str);
    return SB_IMatchText(s, slen, p, plen, locale, locale_is_c);
  }
}

   
                                                     
   

Datum
namelike(PG_FUNCTION_ARGS)
{
  Name str = PG_GETARG_NAME(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;
  char *s, *p;
  int slen, plen;

  s = NameStr(*str);
  slen = strlen(s);
  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);

  result = (GenericMatchText(s, slen, p, plen, PG_GET_COLLATION()) == LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
namenlike(PG_FUNCTION_ARGS)
{
  Name str = PG_GETARG_NAME(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;
  char *s, *p;
  int slen, plen;

  s = NameStr(*str);
  slen = strlen(s);
  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);

  result = (GenericMatchText(s, slen, p, plen, PG_GET_COLLATION()) != LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
textlike(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;
  char *s, *p;
  int slen, plen;

  s = VARDATA_ANY(str);
  slen = VARSIZE_ANY_EXHDR(str);
  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);

  result = (GenericMatchText(s, slen, p, plen, PG_GET_COLLATION()) == LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
textnlike(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;
  char *s, *p;
  int slen, plen;

  s = VARDATA_ANY(str);
  slen = VARSIZE_ANY_EXHDR(str);
  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);

  result = (GenericMatchText(s, slen, p, plen, PG_GET_COLLATION()) != LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
bytealike(PG_FUNCTION_ARGS)
{
  bytea *str = PG_GETARG_BYTEA_PP(0);
  bytea *pat = PG_GETARG_BYTEA_PP(1);
  bool result;
  char *s, *p;
  int slen, plen;

  s = VARDATA_ANY(str);
  slen = VARSIZE_ANY_EXHDR(str);
  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);

  result = (SB_MatchText(s, slen, p, plen, 0, true) == LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
byteanlike(PG_FUNCTION_ARGS)
{
  bytea *str = PG_GETARG_BYTEA_PP(0);
  bytea *pat = PG_GETARG_BYTEA_PP(1);
  bool result;
  char *s, *p;
  int slen, plen;

  s = VARDATA_ANY(str);
  slen = VARSIZE_ANY_EXHDR(str);
  p = VARDATA_ANY(pat);
  plen = VARSIZE_ANY_EXHDR(pat);

  result = (SB_MatchText(s, slen, p, plen, 0, true) != LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

   
                             
   

Datum
nameiclike(PG_FUNCTION_ARGS)
{
  Name str = PG_GETARG_NAME(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;
  text *strtext;

  strtext = DatumGetTextPP(DirectFunctionCall1(name_text, NameGetDatum(str)));
  result = (Generic_Text_IC_like(strtext, pat, PG_GET_COLLATION()) == LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
nameicnlike(PG_FUNCTION_ARGS)
{
  Name str = PG_GETARG_NAME(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;
  text *strtext;

  strtext = DatumGetTextPP(DirectFunctionCall1(name_text, NameGetDatum(str)));
  result = (Generic_Text_IC_like(strtext, pat, PG_GET_COLLATION()) != LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
texticlike(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;

  result = (Generic_Text_IC_like(str, pat, PG_GET_COLLATION()) == LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

Datum
texticnlike(PG_FUNCTION_ARGS)
{
  text *str = PG_GETARG_TEXT_PP(0);
  text *pat = PG_GETARG_TEXT_PP(1);
  bool result;

  result = (Generic_Text_IC_like(str, pat, PG_GET_COLLATION()) != LIKE_TRUE);

  PG_RETURN_BOOL(result);
}

   
                                                           
                                                                              
   
Datum
like_escape(PG_FUNCTION_ARGS)
{
  text *pat = PG_GETARG_TEXT_PP(0);
  text *esc = PG_GETARG_TEXT_PP(1);
  text *result;

  if (pg_database_encoding_max_length() == 1)
  {
    result = SB_do_like_escape(pat, esc);
  }
  else
  {
    result = MB_do_like_escape(pat, esc);
  }

  PG_RETURN_TEXT_P(result);
}

   
                                                                 
                                                                              
   
Datum
like_escape_bytea(PG_FUNCTION_ARGS)
{
  bytea *pat = PG_GETARG_BYTEA_PP(0);
  bytea *esc = PG_GETARG_BYTEA_PP(1);
  bytea *result = SB_do_like_escape((text *)pat, (text *)esc);

  PG_RETURN_BYTEA_P((bytea *)result);
}
