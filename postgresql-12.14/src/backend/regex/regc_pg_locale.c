                                                                            
   
                    
                                                              
                                                                    
   
                                                                              
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   

#include "catalog/pg_collation.h"
#include "utils/pg_locale.h"

   
                                                                           
                                                                        
                                                                 
   
                                                                         
                                                                            
                                                           
   
                                                                       
   
                                                                       
                                                                     
                                                                
                                                                             
   
                                                                           
                                                                           
                                                                       
                                                                           
                                                                            
                                                                    
                                                                         
                                                                        
                
   
                                                                           
                                                                            
                                                  
   
                                                                           
                                                                           
                                                                              
                                                                           
                                                                         
   
                                                                            
                                                                              
                                                                            
                                                                          
   
                                                             
   

typedef enum
{
  PG_REGEX_LOCALE_C,                                            
  PG_REGEX_LOCALE_WIDE,                                  
  PG_REGEX_LOCALE_1BYTE,                                
  PG_REGEX_LOCALE_WIDE_L,                                         
  PG_REGEX_LOCALE_1BYTE_L,                                       
  PG_REGEX_LOCALE_ICU                                     
} PG_Locale_Strategy;

static PG_Locale_Strategy pg_regex_strategy;
static pg_locale_t pg_regex_locale;
static Oid pg_regex_collation;

   
                                                
   
#define PG_ISDIGIT 0x01
#define PG_ISALPHA 0x02
#define PG_ISALNUM (PG_ISDIGIT | PG_ISALPHA)
#define PG_ISUPPER 0x04
#define PG_ISLOWER 0x08
#define PG_ISGRAPH 0x10
#define PG_ISPRINT 0x20
#define PG_ISPUNCT 0x40
#define PG_ISSPACE 0x80

static const unsigned char pg_char_properties[128] = {
              0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             PG_ISSPACE,
             PG_ISSPACE,
             PG_ISSPACE,
             PG_ISSPACE,
             PG_ISSPACE,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
             0,
          PG_ISPRINT | PG_ISSPACE,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISDIGIT | PG_ISGRAPH | PG_ISPRINT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISUPPER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISALPHA | PG_ISLOWER | PG_ISGRAPH | PG_ISPRINT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
             PG_ISGRAPH | PG_ISPRINT | PG_ISPUNCT,
              0};

   
                                                                     
   
                                                                       
                                                                        
                                             
   
void
pg_set_regex_collation(Oid collation)
{
  if (lc_ctype_is_c(collation))
  {
                                                                          
    pg_regex_strategy = PG_REGEX_LOCALE_C;
    pg_regex_locale = 0;
    pg_regex_collation = C_COLLATION_OID;
  }
  else
  {
    if (collation == DEFAULT_COLLATION_OID)
    {
      pg_regex_locale = 0;
    }
    else if (OidIsValid(collation))
    {
         
                                                                         
                                                                         
                                      
         
      pg_regex_locale = pg_newlocale_from_collation(collation);
    }
    else
    {
         
                                                                  
                                                                 
         
      ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for regular expression"), errhint("Use the COLLATE clause to set the collation explicitly.")));
    }

    if (pg_regex_locale && !pg_regex_locale->deterministic)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations are not supported for regular expressions")));
    }

#ifdef USE_ICU
    if (pg_regex_locale && pg_regex_locale->provider == COLLPROVIDER_ICU)
    {
      pg_regex_strategy = PG_REGEX_LOCALE_ICU;
    }
    else
#endif
        if (GetDatabaseEncoding() == PG_UTF8)
    {
      if (pg_regex_locale)
      {
        pg_regex_strategy = PG_REGEX_LOCALE_WIDE_L;
      }
      else
      {
        pg_regex_strategy = PG_REGEX_LOCALE_WIDE;
      }
    }
    else
    {
      if (pg_regex_locale)
      {
        pg_regex_strategy = PG_REGEX_LOCALE_1BYTE_L;
      }
      else
      {
        pg_regex_strategy = PG_REGEX_LOCALE_1BYTE;
      }
    }

    pg_regex_collation = collation;
  }
}

static int
pg_wc_isdigit(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISDIGIT));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswdigit((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isdigit((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswdigit_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isdigit_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isdigit(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_isalpha(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISALPHA));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswalpha((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isalpha((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswalpha_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isalpha_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isalpha(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_isalnum(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISALNUM));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswalnum((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isalnum((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswalnum_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isalnum_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isalnum(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_isupper(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISUPPER));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswupper((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isupper((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswupper_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isupper_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isupper(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_islower(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISLOWER));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswlower((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && islower((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswlower_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && islower_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_islower(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_isgraph(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISGRAPH));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswgraph((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isgraph((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswgraph_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isgraph_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isgraph(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_isprint(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISPRINT));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswprint((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isprint((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswprint_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isprint_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isprint(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_ispunct(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISPUNCT));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswpunct((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && ispunct((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswpunct_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && ispunct_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_ispunct(c);
#endif
    break;
  }
  return 0;                                              
}

static int
pg_wc_isspace(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    return (c <= (pg_wchar)127 && (pg_char_properties[c] & PG_ISSPACE));
  case PG_REGEX_LOCALE_WIDE:
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswspace((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
    return (c <= (pg_wchar)UCHAR_MAX && isspace((unsigned char)c));
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return iswspace_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    return (c <= (pg_wchar)UCHAR_MAX && isspace_l((unsigned char)c, pg_regex_locale->info.lt));
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_isspace(c);
#endif
    break;
  }
  return 0;                                              
}

static pg_wchar
pg_wc_toupper(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    if (c <= (pg_wchar)127)
    {
      return pg_ascii_toupper((unsigned char)c);
    }
    return c;
  case PG_REGEX_LOCALE_WIDE:
                                                                   
    if (c <= (pg_wchar)127)
    {
      return pg_ascii_toupper((unsigned char)c);
    }
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return towupper((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
                                                                   
    if (c <= (pg_wchar)127)
    {
      return pg_ascii_toupper((unsigned char)c);
    }
    if (c <= (pg_wchar)UCHAR_MAX)
    {
      return toupper((unsigned char)c);
    }
    return c;
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return towupper_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    if (c <= (pg_wchar)UCHAR_MAX)
    {
      return toupper_l((unsigned char)c, pg_regex_locale->info.lt);
    }
#endif
    return c;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_toupper(c);
#endif
    break;
  }
  return 0;                                              
}

static pg_wchar
pg_wc_tolower(pg_wchar c)
{
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
    if (c <= (pg_wchar)127)
    {
      return pg_ascii_tolower((unsigned char)c);
    }
    return c;
  case PG_REGEX_LOCALE_WIDE:
                                                                   
    if (c <= (pg_wchar)127)
    {
      return pg_ascii_tolower((unsigned char)c);
    }
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return towlower((wint_t)c);
    }
                   
  case PG_REGEX_LOCALE_1BYTE:
                                                                   
    if (c <= (pg_wchar)127)
    {
      return pg_ascii_tolower((unsigned char)c);
    }
    if (c <= (pg_wchar)UCHAR_MAX)
    {
      return tolower((unsigned char)c);
    }
    return c;
  case PG_REGEX_LOCALE_WIDE_L:
#ifdef HAVE_LOCALE_T
    if (sizeof(wchar_t) >= 4 || c <= (pg_wchar)0xFFFF)
    {
      return towlower_l((wint_t)c, pg_regex_locale->info.lt);
    }
#endif
                   
  case PG_REGEX_LOCALE_1BYTE_L:
#ifdef HAVE_LOCALE_T
    if (c <= (pg_wchar)UCHAR_MAX)
    {
      return tolower_l((unsigned char)c, pg_regex_locale->info.lt);
    }
#endif
    return c;
  case PG_REGEX_LOCALE_ICU:
#ifdef USE_ICU
    return u_tolower(c);
#endif
    break;
  }
  return 0;                                              
}

   
                                                                          
                                                                       
                                                                             
                                                                            
                                                                        
                                                                        
                                        
   
                                                                            
                                                                          
   

typedef int (*pg_wc_probefunc)(pg_wchar c);

typedef struct pg_ctype_cache
{
  pg_wc_probefunc probefunc;                                   
  Oid collation;                                                
  struct cvec cv;                                        
  struct pg_ctype_cache *next;                 
} pg_ctype_cache;

static pg_ctype_cache *pg_ctype_cache_list = NULL;

   
                                                                    
   
static bool
store_match(pg_ctype_cache *pcc, pg_wchar chr1, int nchrs)
{
  chr *newchrs;

  if (nchrs > 1)
  {
    if (pcc->cv.nranges >= pcc->cv.rangespace)
    {
      pcc->cv.rangespace *= 2;
      newchrs = (chr *)realloc(pcc->cv.ranges, pcc->cv.rangespace * sizeof(chr) * 2);
      if (newchrs == NULL)
      {
        return false;
      }
      pcc->cv.ranges = newchrs;
    }
    pcc->cv.ranges[pcc->cv.nranges * 2] = chr1;
    pcc->cv.ranges[pcc->cv.nranges * 2 + 1] = chr1 + nchrs - 1;
    pcc->cv.nranges++;
  }
  else
  {
    assert(nchrs == 1);
    if (pcc->cv.nchrs >= pcc->cv.chrspace)
    {
      pcc->cv.chrspace *= 2;
      newchrs = (chr *)realloc(pcc->cv.chrs, pcc->cv.chrspace * sizeof(chr));
      if (newchrs == NULL)
      {
        return false;
      }
      pcc->cv.chrs = newchrs;
    }
    pcc->cv.chrs[pcc->cv.nchrs++] = chr1;
  }
  return true;
}

   
                                                                          
                                                                        
                                                                            
   
                                                                 
   
static struct cvec *
pg_ctype_get_cache(pg_wc_probefunc probefunc, int cclasscode)
{
  pg_ctype_cache *pcc;
  pg_wchar max_chr;
  pg_wchar cur_chr;
  int nmatches;
  chr *newchrs;

     
                                           
     
  for (pcc = pg_ctype_cache_list; pcc != NULL; pcc = pcc->next)
  {
    if (pcc->probefunc == probefunc && pcc->collation == pg_regex_collation)
    {
      return &pcc->cv;
    }
  }

     
                                            
     
  pcc = (pg_ctype_cache *)malloc(sizeof(pg_ctype_cache));
  if (pcc == NULL)
  {
    return NULL;
  }
  pcc->probefunc = probefunc;
  pcc->collation = pg_regex_collation;
  pcc->cv.nchrs = 0;
  pcc->cv.chrspace = 128;
  pcc->cv.chrs = (chr *)malloc(pcc->cv.chrspace * sizeof(chr));
  pcc->cv.nranges = 0;
  pcc->cv.rangespace = 64;
  pcc->cv.ranges = (chr *)malloc(pcc->cv.rangespace * sizeof(chr) * 2);
  if (pcc->cv.chrs == NULL || pcc->cv.ranges == NULL)
  {
    goto out_of_memory;
  }
  pcc->cv.cclasscode = cclasscode;

     
                                                                           
                                                                          
                                                                        
                                                                          
                                                                       
     
                                                                         
                                                                          
                                                                        
                                                                            
                                                           
     
  switch (pg_regex_strategy)
  {
  case PG_REGEX_LOCALE_C:
#if MAX_SIMPLE_CHR >= 127
    max_chr = (pg_wchar)127;
    pcc->cv.cclasscode = -1;
#else
    max_chr = (pg_wchar)MAX_SIMPLE_CHR;
#endif
    break;
  case PG_REGEX_LOCALE_WIDE:
  case PG_REGEX_LOCALE_WIDE_L:
    max_chr = (pg_wchar)MAX_SIMPLE_CHR;
    break;
  case PG_REGEX_LOCALE_1BYTE:
  case PG_REGEX_LOCALE_1BYTE_L:
#if MAX_SIMPLE_CHR >= UCHAR_MAX
    max_chr = (pg_wchar)UCHAR_MAX;
    pcc->cv.cclasscode = -1;
#else
    max_chr = (pg_wchar)MAX_SIMPLE_CHR;
#endif
    break;
  case PG_REGEX_LOCALE_ICU:
    max_chr = (pg_wchar)MAX_SIMPLE_CHR;
    break;
  default:
    max_chr = 0;                                              
    break;
  }

     
                      
     
  nmatches = 0;                                    

  for (cur_chr = 0; cur_chr <= max_chr; cur_chr++)
  {
    if ((*probefunc)(cur_chr))
    {
      nmatches++;
    }
    else if (nmatches > 0)
    {
      if (!store_match(pcc, cur_chr - nmatches, nmatches))
      {
        goto out_of_memory;
      }
      nmatches = 0;
    }
  }

  if (nmatches > 0)
  {
    if (!store_match(pcc, cur_chr - nmatches, nmatches))
    {
      goto out_of_memory;
    }
  }

     
                                                                    
     
  if (pcc->cv.nchrs == 0)
  {
    free(pcc->cv.chrs);
    pcc->cv.chrs = NULL;
    pcc->cv.chrspace = 0;
  }
  else if (pcc->cv.nchrs < pcc->cv.chrspace)
  {
    newchrs = (chr *)realloc(pcc->cv.chrs, pcc->cv.nchrs * sizeof(chr));
    if (newchrs == NULL)
    {
      goto out_of_memory;
    }
    pcc->cv.chrs = newchrs;
    pcc->cv.chrspace = pcc->cv.nchrs;
  }
  if (pcc->cv.nranges == 0)
  {
    free(pcc->cv.ranges);
    pcc->cv.ranges = NULL;
    pcc->cv.rangespace = 0;
  }
  else if (pcc->cv.nranges < pcc->cv.rangespace)
  {
    newchrs = (chr *)realloc(pcc->cv.ranges, pcc->cv.nranges * sizeof(chr) * 2);
    if (newchrs == NULL)
    {
      goto out_of_memory;
    }
    pcc->cv.ranges = newchrs;
    pcc->cv.rangespace = pcc->cv.nranges;
  }

     
                                       
     
  pcc->next = pg_ctype_cache_list;
  pg_ctype_cache_list = pcc;

  return &pcc->cv;

     
                       
     
out_of_memory:
  if (pcc->cv.chrs)
  {
    free(pcc->cv.chrs);
  }
  if (pcc->cv.ranges)
  {
    free(pcc->cv.ranges);
  }
  free(pcc);

  return NULL;
}
