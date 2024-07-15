                                                                          
   
                               
   
                                                                         
   
                                     
   
                                                                          
   

             
                                                                    
                                                                        
                                                                     
                                                        
   
                                                            
                
   
                                                                       
                                                                      
                                                                    
                                                                      
                                                                     
                                                                
                                                               
                                                                   
                                                                         
                
   
                         
   
                                                                        
                                                                           
                                                                         
                                                   
                                       
                                      
                   
                                
                                                                         
                                                                         
                                                                     
   
                                                
   
                                                                          
             
   

#include "postgres.h"

#include <time.h>

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_control.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/hsearch.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/syscache.h"

#ifdef USE_ICU
#include <unicode/ucnv.h>
#endif

#ifdef WIN32
   
                                                                            
                                                                          
                                                  
   
#undef StrNCpy
#include <shlwapi.h>
#ifdef StrNCpy
#undef STrNCpy
#endif
#endif

#define MAX_L10N_DATA 80

                  
char *locale_messages;
char *locale_monetary;
char *locale_numeric;
char *locale_time;

                                
char *localized_abbrev_days[7];
char *localized_full_days[7];
char *localized_abbrev_months[12];
char *localized_full_months[12];

                                                         
static bool CurrentLocaleConvValid = false;
static bool CurrentLCTimeValid = false;

                                       

#define LC_ENV_BUFSIZE (NAMEDATALEN + 20)

static char lc_collate_envbuf[LC_ENV_BUFSIZE];
static char lc_ctype_envbuf[LC_ENV_BUFSIZE];

#ifdef LC_MESSAGES
static char lc_messages_envbuf[LC_ENV_BUFSIZE];
#endif
static char lc_monetary_envbuf[LC_ENV_BUFSIZE];
static char lc_numeric_envbuf[LC_ENV_BUFSIZE];
static char lc_time_envbuf[LC_ENV_BUFSIZE];

                                           

typedef struct
{
  Oid collid;                                         
  bool collate_is_c;                                    
  bool ctype_is_c;                                    
  bool flags_valid;                                      
  pg_locale_t locale;                                         
} collation_cache_entry;

static HTAB *collation_cache = NULL;

#if defined(WIN32) && defined(LC_MESSAGES)
static char *
IsoLocaleName(const char *);                    
#endif

#ifdef USE_ICU
static void
icu_set_collation_attributes(UCollator *collator, const char *loc);
#endif

   
                     
   
                                                                              
                                                                        
                                                                             
                                                                              
                                                                            
                                                                          
                                                                            
                                                                               
                                                                            
                                          
   
char *
pg_perm_setlocale(int category, const char *locale)
{
  char *result;
  const char *envvar;
  char *envbuf;

#ifndef WIN32
  result = setlocale(category, locale);
#else

     
                                                                           
                                                                         
                                                                        
                         
     
#ifdef LC_MESSAGES
  if (category == LC_MESSAGES)
  {
    result = (char *)locale;
    if (locale == NULL || locale[0] == '\0')
    {
      return result;
    }
  }
  else
#endif
    result = setlocale(category, locale);
#endif            

  if (result == NULL)
  {
    return result;                                      
  }

     
                                                                           
                                                                             
                                                                           
                                                                           
                        
     
  if (category == LC_CTYPE)
  {
    static char save_lc_ctype[LC_ENV_BUFSIZE];

                                                                      
    strlcpy(save_lc_ctype, result, sizeof(save_lc_ctype));
    result = save_lc_ctype;

#ifdef ENABLE_NLS
    SetMessageEncoding(pg_bind_textdomain_codeset(textdomain(NULL)));
#else
    SetMessageEncoding(GetDatabaseEncoding());
#endif
  }

  switch (category)
  {
  case LC_COLLATE:
    envvar = "LC_COLLATE";
    envbuf = lc_collate_envbuf;
    break;
  case LC_CTYPE:
    envvar = "LC_CTYPE";
    envbuf = lc_ctype_envbuf;
    break;
#ifdef LC_MESSAGES
  case LC_MESSAGES:
    envvar = "LC_MESSAGES";
    envbuf = lc_messages_envbuf;
#ifdef WIN32
    result = IsoLocaleName(locale);
    if (result == NULL)
    {
      result = (char *)locale;
    }
    elog(DEBUG3, "IsoLocaleName() executed; locale: \"%s\"", result);
#endif            
    break;
#endif                  
  case LC_MONETARY:
    envvar = "LC_MONETARY";
    envbuf = lc_monetary_envbuf;
    break;
  case LC_NUMERIC:
    envvar = "LC_NUMERIC";
    envbuf = lc_numeric_envbuf;
    break;
  case LC_TIME:
    envvar = "LC_TIME";
    envbuf = lc_time_envbuf;
    break;
  default:
    elog(FATAL, "unrecognized LC category: %d", category);
    envvar = NULL;                          
    envbuf = NULL;
    return NULL;
  }

  snprintf(envbuf, LC_ENV_BUFSIZE - 1, "%s=%s", envvar, result);

  if (putenv(envbuf))
  {
    return NULL;
  }

  return result;
}

   
                                                     
   
                                                                            
                                                                               
                                                                             
                                                                              
                                                                           
                                               
   
bool
check_locale(int category, const char *locale, char **canonname)
{
  char *save;
  char *res;

  if (canonname)
  {
    *canonname = NULL;                         
  }

  save = setlocale(category, NULL);
  if (!save)
  {
    return false;                            
  }

                                                                         
  save = pstrdup(save);

                                                               
  res = setlocale(category, locale);

                                         
  if (res && canonname)
  {
    *canonname = pstrdup(res);
  }

                          
  if (!setlocale(category, save))
  {
    elog(WARNING, "failed to restore old locale \"%s\"", save);
  }
  pfree(save);

  return (res != NULL);
}

   
                          
   
                                                                               
                                                                     
                                                                   
   
                                                                        
                                                                         
                                                                            
   
bool
check_locale_monetary(char **newval, void **extra, GucSource source)
{
  return check_locale(LC_MONETARY, *newval, NULL);
}

void
assign_locale_monetary(const char *newval, void *extra)
{
  CurrentLocaleConvValid = false;
}

bool
check_locale_numeric(char **newval, void **extra, GucSource source)
{
  return check_locale(LC_NUMERIC, *newval, NULL);
}

void
assign_locale_numeric(const char *newval, void *extra)
{
  CurrentLocaleConvValid = false;
}

bool
check_locale_time(char **newval, void **extra, GucSource source)
{
  return check_locale(LC_TIME, *newval, NULL);
}

void
assign_locale_time(const char *newval, void *extra)
{
  CurrentLCTimeValid = false;
}

   
                                                     
   
                                                                             
                                                                            
                                                                          
                                                                             
                                                                          
                                                                            
   
bool
check_locale_messages(char **newval, void **extra, GucSource source)
{
  if (**newval == '\0')
  {
    if (source == PGC_S_DEFAULT)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

     
                                                                          
     
                                                                  
     
#if defined(LC_MESSAGES) && !defined(WIN32)
  return check_locale(LC_MESSAGES, *newval, NULL);
#else
  return true;
#endif
}

void
assign_locale_messages(const char *newval, void *extra)
{
     
                                                                           
                                              
     
#ifdef LC_MESSAGES
  (void)pg_perm_setlocale(LC_MESSAGES, newval);
#endif
}

   
                                                                      
                                                             
   
static void
free_struct_lconv(struct lconv *s)
{
  if (s->decimal_point)
  {
    free(s->decimal_point);
  }
  if (s->thousands_sep)
  {
    free(s->thousands_sep);
  }
  if (s->grouping)
  {
    free(s->grouping);
  }
  if (s->int_curr_symbol)
  {
    free(s->int_curr_symbol);
  }
  if (s->currency_symbol)
  {
    free(s->currency_symbol);
  }
  if (s->mon_decimal_point)
  {
    free(s->mon_decimal_point);
  }
  if (s->mon_thousands_sep)
  {
    free(s->mon_thousands_sep);
  }
  if (s->mon_grouping)
  {
    free(s->mon_grouping);
  }
  if (s->positive_sign)
  {
    free(s->positive_sign);
  }
  if (s->negative_sign)
  {
    free(s->negative_sign);
  }
}

   
                                                                          
                                                                        
   
static bool
struct_lconv_is_valid(struct lconv *s)
{
  if (s->decimal_point == NULL)
  {
    return false;
  }
  if (s->thousands_sep == NULL)
  {
    return false;
  }
  if (s->grouping == NULL)
  {
    return false;
  }
  if (s->int_curr_symbol == NULL)
  {
    return false;
  }
  if (s->currency_symbol == NULL)
  {
    return false;
  }
  if (s->mon_decimal_point == NULL)
  {
    return false;
  }
  if (s->mon_thousands_sep == NULL)
  {
    return false;
  }
  if (s->mon_grouping == NULL)
  {
    return false;
  }
  if (s->positive_sign == NULL)
  {
    return false;
  }
  if (s->negative_sign == NULL)
  {
    return false;
  }
  return true;
}

   
                                                                          
                      
   
static void
db_encoding_convert(int encoding, char **str)
{
  char *pstr;
  char *mstr;

                                                   
  pstr = pg_any_to_server(*str, strlen(*str), encoding);
  if (pstr == *str)
  {
    return;                             
  }

                                     
  mstr = strdup(pstr);
  if (mstr == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }

                          
  free(*str);
  *str = mstr;

  pfree(pstr);
}

   
                                                                   
                                                            
   
struct lconv *
PGLC_localeconv(void)
{
  static struct lconv CurrentLocaleConv;
  static bool CurrentLocaleConvAllocated = false;
  struct lconv *extlconv;
  struct lconv worklconv;
  char *save_lc_monetary;
  char *save_lc_numeric;
#ifdef WIN32
  char *save_lc_ctype;
#endif

                             
  if (CurrentLocaleConvValid)
  {
    return &CurrentLocaleConv;
  }

                                          
  if (CurrentLocaleConvAllocated)
  {
    free_struct_lconv(&CurrentLocaleConv);
    CurrentLocaleConvAllocated = false;
  }

     
                                                                        
                                                                           
                                                                       
                                                                        
                                                                          
                                                                        
                                                                             
                                                                     
                                                                        
                                     
     
  memset(&worklconv, 0, sizeof(worklconv));

                                                              
  save_lc_monetary = setlocale(LC_MONETARY, NULL);
  if (!save_lc_monetary)
  {
    elog(ERROR, "setlocale(NULL) failed");
  }
  save_lc_monetary = pstrdup(save_lc_monetary);

  save_lc_numeric = setlocale(LC_NUMERIC, NULL);
  if (!save_lc_numeric)
  {
    elog(ERROR, "setlocale(NULL) failed");
  }
  save_lc_numeric = pstrdup(save_lc_numeric);

#ifdef WIN32

     
                                                                             
                                                                          
                                                                            
                                                                             
                                                                           
                                                                           
                                                                    
     
                                                                     
                                                                       
                                                                     
     

                                             
  save_lc_ctype = setlocale(LC_CTYPE, NULL);
  if (!save_lc_ctype)
  {
    elog(ERROR, "setlocale(NULL) failed");
  }
  save_lc_ctype = pstrdup(save_lc_ctype);

                                                                      

                                    
  setlocale(LC_CTYPE, locale_numeric);
#endif

                                              
  setlocale(LC_NUMERIC, locale_numeric);
  extlconv = localeconv();

                                                            
  worklconv.decimal_point = strdup(extlconv->decimal_point);
  worklconv.thousands_sep = strdup(extlconv->thousands_sep);
  worklconv.grouping = strdup(extlconv->grouping);

#ifdef WIN32
                                     
  setlocale(LC_CTYPE, locale_monetary);
#endif

                                               
  setlocale(LC_MONETARY, locale_monetary);
  extlconv = localeconv();

                                                            
  worklconv.int_curr_symbol = strdup(extlconv->int_curr_symbol);
  worklconv.currency_symbol = strdup(extlconv->currency_symbol);
  worklconv.mon_decimal_point = strdup(extlconv->mon_decimal_point);
  worklconv.mon_thousands_sep = strdup(extlconv->mon_thousands_sep);
  worklconv.mon_grouping = strdup(extlconv->mon_grouping);
  worklconv.positive_sign = strdup(extlconv->positive_sign);
  worklconv.negative_sign = strdup(extlconv->negative_sign);
                                  
  worklconv.int_frac_digits = extlconv->int_frac_digits;
  worklconv.frac_digits = extlconv->frac_digits;
  worklconv.p_cs_precedes = extlconv->p_cs_precedes;
  worklconv.p_sep_by_space = extlconv->p_sep_by_space;
  worklconv.n_cs_precedes = extlconv->n_cs_precedes;
  worklconv.n_sep_by_space = extlconv->n_sep_by_space;
  worklconv.p_sign_posn = extlconv->p_sign_posn;
  worklconv.n_sign_posn = extlconv->n_sign_posn;

     
                                                                        
                                                                             
                                                                            
                                                                          
                                                                             
                  
     
#ifdef WIN32
  if (!setlocale(LC_CTYPE, save_lc_ctype))
  {
    elog(FATAL, "failed to restore LC_CTYPE to \"%s\"", save_lc_ctype);
  }
#endif
  if (!setlocale(LC_MONETARY, save_lc_monetary))
  {
    elog(FATAL, "failed to restore LC_MONETARY to \"%s\"", save_lc_monetary);
  }
  if (!setlocale(LC_NUMERIC, save_lc_numeric))
  {
    elog(FATAL, "failed to restore LC_NUMERIC to \"%s\"", save_lc_numeric);
  }

     
                                                                           
                                                                          
                                                                       
     
  PG_TRY();
  {
    int encoding;

                                            
    pfree(save_lc_monetary);
    pfree(save_lc_numeric);
#ifdef WIN32
    pfree(save_lc_ctype);
#endif

                                                                    
    if (!struct_lconv_is_valid(&worklconv))
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }

       
                                                                          
                                                                          
                                                                         
                                                                       
                                                
       
    encoding = pg_get_encoding_from_locale(locale_numeric, true);
    if (encoding < 0)
    {
      encoding = PG_SQL_ASCII;
    }

    db_encoding_convert(encoding, &worklconv.decimal_point);
    db_encoding_convert(encoding, &worklconv.thousands_sep);
                                                              

    encoding = pg_get_encoding_from_locale(locale_monetary, true);
    if (encoding < 0)
    {
      encoding = PG_SQL_ASCII;
    }

    db_encoding_convert(encoding, &worklconv.int_curr_symbol);
    db_encoding_convert(encoding, &worklconv.currency_symbol);
    db_encoding_convert(encoding, &worklconv.mon_decimal_point);
    db_encoding_convert(encoding, &worklconv.mon_thousands_sep);
                                                                  
    db_encoding_convert(encoding, &worklconv.positive_sign);
    db_encoding_convert(encoding, &worklconv.negative_sign);
  }
  PG_CATCH();
  {
    free_struct_lconv(&worklconv);
    PG_RE_THROW();
  }
  PG_END_TRY();

     
                                              
     
  CurrentLocaleConv = worklconv;
  CurrentLocaleConvAllocated = true;
  CurrentLocaleConvValid = true;
  return &CurrentLocaleConv;
}

#ifdef WIN32
   
                                                                             
                                                                          
                                                                            
                                                                        
                    
   
                                                                          
                                                                         
                                
   
                                                                               
                                                                        
                                                                             
   
static size_t
strftime_win32(char *dst, size_t dstlen, const char *format, const struct tm *tm)
{
  size_t len;
  wchar_t wformat[8];                                      
  wchar_t wbuf[MAX_L10N_DATA];

     
                                                                       
                                                                        
     
  len = MultiByteToWideChar(CP_UTF8, 0, format, -1, wformat, lengthof(wformat));
  if (len == 0)
  {
    elog(ERROR, "could not convert format string from UTF-8: error code %lu", GetLastError());
  }

  len = wcsftime(wbuf, MAX_L10N_DATA, wformat, tm);
  if (len == 0)
  {
       
                                                                     
                                                                      
       
    return 0;
  }

  len = WideCharToMultiByte(CP_UTF8, 0, wbuf, len, dst, dstlen - 1, NULL, NULL);
  if (len == 0)
  {
    elog(ERROR, "could not convert string to UTF-8: error code %lu", GetLastError());
  }

  dst[len] = '\0';

  return len;
}

                         
#define strftime(a, b, c, d) strftime_win32(a, b, c, d)
#endif            

   
                                       
                                                                     
                                                                         
   
static void
cache_single_string(char **dst, const char *src, int encoding)
{
  char *ptr;
  char *olddst;

                                                                        
  ptr = pg_any_to_server(src, strlen(src), encoding);

                                                                            
  olddst = *dst;
  *dst = MemoryContextStrdup(TopMemoryContext, ptr);
  if (olddst)
  {
    pfree(olddst);
  }

                                                                  
  if (ptr != src)
  {
    pfree(ptr);
  }
}

   
                                                              
   
void
cache_locale_time(void)
{
  char buf[(2 * 7 + 2 * 12) * MAX_L10N_DATA];
  char *bufptr;
  time_t timenow;
  struct tm *timeinfo;
  bool strftimefail = false;
  int encoding;
  int i;
  char *save_lc_time;
#ifdef WIN32
  char *save_lc_ctype;
#endif

                               
  if (CurrentLCTimeValid)
  {
    return;
  }

  elog(DEBUG3, "cache_locale_time() executed; locale: \"%s\"", locale_time);

     
                                                                          
                                                                         
                                                                           
                         
     

                                            
  save_lc_time = setlocale(LC_TIME, NULL);
  if (!save_lc_time)
  {
    elog(ERROR, "setlocale(NULL) failed");
  }
  save_lc_time = pstrdup(save_lc_time);

#ifdef WIN32

     
                                                                            
                                                                           
                                                                           
                                                        
     

                                             
  save_lc_ctype = setlocale(LC_CTYPE, NULL);
  if (!save_lc_ctype)
  {
    elog(ERROR, "setlocale(NULL) failed");
  }
  save_lc_ctype = pstrdup(save_lc_ctype);

                                    
  setlocale(LC_CTYPE, locale_time);
#endif

  setlocale(LC_TIME, locale_time);

                                                                  
  timenow = time(NULL);
  timeinfo = localtime(&timenow);

                                                                           
  bufptr = buf;

     
                                                                          
                                                                             
                                                                     
                                                                           
                                                                            
                                                                     
     
  errno = 0;

                      
  for (i = 0; i < 7; i++)
  {
    timeinfo->tm_wday = i;
    if (strftime(bufptr, MAX_L10N_DATA, "%a", timeinfo) <= 0)
    {
      strftimefail = true;
    }
    bufptr += MAX_L10N_DATA;
    if (strftime(bufptr, MAX_L10N_DATA, "%A", timeinfo) <= 0)
    {
      strftimefail = true;
    }
    bufptr += MAX_L10N_DATA;
  }

                        
  for (i = 0; i < 12; i++)
  {
    timeinfo->tm_mon = i;
    timeinfo->tm_mday = 1;                                           
    if (strftime(bufptr, MAX_L10N_DATA, "%b", timeinfo) <= 0)
    {
      strftimefail = true;
    }
    bufptr += MAX_L10N_DATA;
    if (strftime(bufptr, MAX_L10N_DATA, "%B", timeinfo) <= 0)
    {
      strftimefail = true;
    }
    bufptr += MAX_L10N_DATA;
  }

     
                                                                      
                                
     
#ifdef WIN32
  if (!setlocale(LC_CTYPE, save_lc_ctype))
  {
    elog(FATAL, "failed to restore LC_CTYPE to \"%s\"", save_lc_ctype);
  }
#endif
  if (!setlocale(LC_TIME, save_lc_time))
  {
    elog(FATAL, "failed to restore LC_TIME to \"%s\"", save_lc_time);
  }

     
                                                                             
                                                                      
     
  if (strftimefail)
  {
    elog(ERROR, "strftime() failed: %m");
  }

                                          
  pfree(save_lc_time);
#ifdef WIN32
  pfree(save_lc_ctype);
#endif

#ifndef WIN32

     
                                                                           
                                                                        
                                                                      
     
  encoding = pg_get_encoding_from_locale(locale_time, true);
  if (encoding < 0)
  {
    encoding = PG_SQL_ASCII;
  }

#else

     
                                                                            
                        
     
  encoding = PG_UTF8;

#endif            

  bufptr = buf;

                      
  for (i = 0; i < 7; i++)
  {
    cache_single_string(&localized_abbrev_days[i], bufptr, encoding);
    bufptr += MAX_L10N_DATA;
    cache_single_string(&localized_full_days[i], bufptr, encoding);
    bufptr += MAX_L10N_DATA;
  }

                        
  for (i = 0; i < 12; i++)
  {
    cache_single_string(&localized_abbrev_months[i], bufptr, encoding);
    bufptr += MAX_L10N_DATA;
    cache_single_string(&localized_full_months[i], bufptr, encoding);
    bufptr += MAX_L10N_DATA;
  }

  CurrentLCTimeValid = true;
}

#if defined(WIN32) && defined(LC_MESSAGES)
   
                                                               
   
                                                                          
                                                                              
                                                                 
   
                                                                              
                                                                              
                                                                        
                                                                        
                                                                              
                                                                               
                                                                           
                                                                
   
                                                                            
                                                                           
                                                                               
                                                                            
                                                                         
                                                                         
                                                                
   
                                                                           
                                                                             
                                                                      
                                                                           
                                                                             
                                                                          
                 
   
                                                                               
                                                                            
                                                                             
                                                                            
                                                                              
                                                                               
   
                                                                            
                                     
   
                                                                              
                                                                        
   

#if _MSC_VER >= 1900
   
                                                                        
   
                                                                               
                                                          
                           
   
                                                                             
                                                                           
                                                                            
                                                                      
   
static BOOL CALLBACK
search_locale_enum(LPWSTR pStr, DWORD dwFlags, LPARAM lparam)
{
  wchar_t test_locale[LOCALE_NAME_MAX_LENGTH];
  wchar_t **argv;

  (void)(dwFlags);

  argv = (wchar_t **)lparam;
  *argv[2] = (wchar_t)0;

  memset(test_locale, 0, sizeof(test_locale));

                                                 
  if (GetLocaleInfoEx(pStr, LOCALE_SENGLISHLANGUAGENAME, test_locale, LOCALE_NAME_MAX_LENGTH))
  {
       
                                                                      
                                                                         
                                            
       
    if (wcsrchr(pStr, '-') == NULL || wcsrchr(argv[0], '_') == NULL)
    {
      if (_wcsicmp(argv[0], test_locale) == 0)
      {
        wcscpy(argv[1], pStr);
        *argv[2] = (wchar_t)1;
        return FALSE;
      }
    }

       
                                                                        
                                                                      
                                
       
    else
    {
      size_t len;

      wcscat(test_locale, L"_");
      len = wcslen(test_locale);
      if (GetLocaleInfoEx(pStr, LOCALE_SENGLISHCOUNTRYNAME, test_locale + len, LOCALE_NAME_MAX_LENGTH - len))
      {
        if (_wcsicmp(argv[0], test_locale) == 0)
        {
          wcscpy(argv[1], pStr);
          *argv[2] = (wchar_t)1;
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}

   
                                                                            
                                      
   
                                                   
   
static char *
get_iso_localename(const char *winlocname)
{
  wchar_t wc_locale_name[LOCALE_NAME_MAX_LENGTH];
  wchar_t buffer[LOCALE_NAME_MAX_LENGTH];
  static char iso_lc_messages[LOCALE_NAME_MAX_LENGTH];
  char *period;
  int len;
  int ret_val;

     
                                              
                                         
     
                                                                             
                                                       
     
  period = strchr(winlocname, '.');
  if (period != NULL)
  {
    len = period - winlocname;
  }
  else
  {
    len = pg_mbstrlen(winlocname);
  }

  memset(wc_locale_name, 0, sizeof(wc_locale_name));
  memset(buffer, 0, sizeof(buffer));
  MultiByteToWideChar(CP_ACP, 0, winlocname, len, wc_locale_name, LOCALE_NAME_MAX_LENGTH);

     
                                                                          
                                                 
     
  ret_val = GetLocaleInfoEx(wc_locale_name, LOCALE_SNAME, (LPWSTR)&buffer, LOCALE_NAME_MAX_LENGTH);
  if (!ret_val)
  {
       
                                                                           
             
       
    wchar_t *argv[3];

    argv[0] = wc_locale_name;
    argv[1] = buffer;
    argv[2] = (wchar_t *)&ret_val;
    EnumSystemLocalesEx(search_locale_enum, LOCALE_WINDOWS, (LPARAM)argv, NULL);
  }

  if (ret_val)
  {
    size_t rc;
    char *hyphen;

                                                                      
    rc = wchar2char(iso_lc_messages, buffer, sizeof(iso_lc_messages), NULL);
    if (rc == -1 || rc == sizeof(iso_lc_messages))
    {
      return NULL;
    }

       
                                                                      
                      
       
    hyphen = strchr(iso_lc_messages, '-');
    if (hyphen)
    {
      *hyphen = '_';
    }

    return iso_lc_messages;
  }

  return NULL;
}
#endif                       

static char *
IsoLocaleName(const char *winlocname)
{
#if (_MSC_VER >= 1400)                     
  static char iso_lc_messages[32];
  _locale_t loct = NULL;

  if (pg_strcasecmp("c", winlocname) == 0 || pg_strcasecmp("posix", winlocname) == 0)
  {
    strcpy(iso_lc_messages, "C");
    return iso_lc_messages;
  }

#if (_MSC_VER >= 1900)                                  
  return get_iso_localename(winlocname);
#else
  loct = _create_locale(LC_CTYPE, winlocname);
  if (loct != NULL)
  {
#if (_MSC_VER >= 1700)                                  
    size_t rc;
    char *hyphen;

                                                                      
    rc = wchar2char(iso_lc_messages, loct->locinfo->locale_name[LC_CTYPE], sizeof(iso_lc_messages), NULL);
    _free_locale(loct);
    if (rc == -1 || rc == sizeof(iso_lc_messages))
    {
      return NULL;
    }

       
                                                                           
                                                                         
                                                                    
                                                                
                                                                      
       
                                                                        
                                                                        
                                                                         
                                                                          
                              
       
    hyphen = strchr(iso_lc_messages, '-');
    if (hyphen)
    {
      *hyphen = '_';
    }
#else
    char isolang[32], isocrty[32];
    LCID lcid;

    lcid = loct->locinfo->lc_handle[LC_CTYPE];
    if (lcid == 0)
    {
      lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    }
    _free_locale(loct);

    if (!GetLocaleInfoA(lcid, LOCALE_SISO639LANGNAME, isolang, sizeof(isolang)))
    {
      return NULL;
    }
    if (!GetLocaleInfoA(lcid, LOCALE_SISO3166CTRYNAME, isocrty, sizeof(isocrty)))
    {
      return NULL;
    }
    snprintf(iso_lc_messages, sizeof(iso_lc_messages) - 1, "%s_%s", isolang, isocrty);
#endif
    return iso_lc_messages;
  }
  return NULL;
#endif                                  
#else
  return NULL;                                                  
#endif                       
}
#endif                           

   
                                                                              
                                                                             
                                         
   
                                                                              
                                                                              
                                                                               
                                                                             
                                                                             
                                                      
   
void
check_strxfrm_bug(void)
{
  char buf[32];
  const int canary = 0x7F;
  bool ok = true;

     
                                                                          
                                                                          
                       
     
                                                                         
                                                                        
                                                                         
                                                               
     
  buf[7] = canary;
  (void)strxfrm(buf, "ab", 7);
  if (buf[7] != canary)
  {
    ok = false;
  }

     
                                                                         
                                                                          
                                                                           
                                                                             
                                                               
     
                                                                          
                                                                             
     
  buf[1] = canary;
  (void)strxfrm(buf, "a", 1);
  if (buf[1] != canary)
  {
    ok = false;
  }

  if (!ok)
  {
    ereport(ERROR, (errcode(ERRCODE_SYSTEM_ERROR), errmsg_internal("strxfrm(), in locale \"%s\", writes past the specified array length", setlocale(LC_COLLATE, NULL)), errhint("Apply system library package updates.")));
  }
}

   
                                              
   
                                                                           
                                                                      
                                                                          
                                                                         
                                                                           
                                                                           
   
                                                                            
                              
   
                                                                         
                                                                       
                                                                     
                 
   
                                                                           
                                                                        
                                                                         
             
   

static collation_cache_entry *
lookup_collation_cache(Oid collation, bool set_flags)
{
  collation_cache_entry *cache_entry;
  bool found;

  Assert(OidIsValid(collation));
  Assert(collation != DEFAULT_COLLATION_OID);

  if (collation_cache == NULL)
  {
                                                       
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(Oid);
    ctl.entrysize = sizeof(collation_cache_entry);
    collation_cache = hash_create("Collation cache", 100, &ctl, HASH_ELEM | HASH_BLOBS);
  }

  cache_entry = hash_search(collation_cache, &collation, HASH_ENTER, &found);
  if (!found)
  {
       
                                                                       
                       
       
    cache_entry->flags_valid = false;
    cache_entry->locale = 0;
  }

  if (set_flags && !cache_entry->flags_valid)
  {
                                  
    HeapTuple tp;
    Form_pg_collation collform;
    const char *collcollate;
    const char *collctype;

    tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(collation));
    if (!HeapTupleIsValid(tp))
    {
      elog(ERROR, "cache lookup failed for collation %u", collation);
    }
    collform = (Form_pg_collation)GETSTRUCT(tp);

    collcollate = NameStr(collform->collcollate);
    collctype = NameStr(collform->collctype);

    cache_entry->collate_is_c = ((strcmp(collcollate, "C") == 0) || (strcmp(collcollate, "POSIX") == 0));
    cache_entry->ctype_is_c = ((strcmp(collctype, "C") == 0) || (strcmp(collctype, "POSIX") == 0));

    cache_entry->flags_valid = true;

    ReleaseSysCache(tp);
  }

  return cache_entry;
}

   
                                                       
   
bool
lc_collate_is_c(Oid collation)
{
     
                                                                             
                                                                    
     
  if (!OidIsValid(collation))
  {
    return false;
  }

     
                                                                             
                                                                    
     
  if (collation == DEFAULT_COLLATION_OID)
  {
    static int result = -1;
    char *localeptr;

    if (result >= 0)
    {
      return (bool)result;
    }
    localeptr = setlocale(LC_COLLATE, NULL);
    if (!localeptr)
    {
      elog(ERROR, "invalid LC_COLLATE setting");
    }

    if (strcmp(localeptr, "C") == 0)
    {
      result = true;
    }
    else if (strcmp(localeptr, "POSIX") == 0)
    {
      result = true;
    }
    else
    {
      result = false;
    }
    return (bool)result;
  }

     
                                                                         
     
  if (collation == C_COLLATION_OID || collation == POSIX_COLLATION_OID)
  {
    return true;
  }

     
                                                                    
     
  return (lookup_collation_cache(collation, true))->collate_is_c;
}

   
                                                     
   
bool
lc_ctype_is_c(Oid collation)
{
     
                                                                             
                                                                    
     
  if (!OidIsValid(collation))
  {
    return false;
  }

     
                                                                             
                                                                    
     
  if (collation == DEFAULT_COLLATION_OID)
  {
    static int result = -1;
    char *localeptr;

    if (result >= 0)
    {
      return (bool)result;
    }
    localeptr = setlocale(LC_CTYPE, NULL);
    if (!localeptr)
    {
      elog(ERROR, "invalid LC_CTYPE setting");
    }

    if (strcmp(localeptr, "C") == 0)
    {
      result = true;
    }
    else if (strcmp(localeptr, "POSIX") == 0)
    {
      result = true;
    }
    else
    {
      result = false;
    }
    return (bool)result;
  }

     
                                                                         
     
  if (collation == C_COLLATION_OID || collation == POSIX_COLLATION_OID)
  {
    return true;
  }

     
                                                                    
     
  return (lookup_collation_cache(collation, true))->ctype_is_c;
}

                                                             
#ifdef HAVE_LOCALE_T
static void
report_newlocale_failure(const char *localename)
{
  int save_errno;

     
                                                              
                                                                         
                                                                      
                                                                           
                        
     
  if (errno == 0)
  {
    errno = ENOENT;
  }

     
                                                                        
                                      
     
  save_errno = errno;                                         
  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("could not create locale \"%s\": %m", localename), (save_errno == ENOENT ? errdetail("The operating system could not find any locale data for the locale name \"%s\".", localename) : 0)));
}
#endif                    

   
                                                                       
                                                                             
   
                                                                        
                                                                     
                                                                           
                                                                    
                                                                       
                                                                           
                                      
   
                                                                     
                                                                             
                           
   
pg_locale_t
pg_newlocale_from_collation(Oid collid)
{
  collation_cache_entry *cache_entry;

                                     
  Assert(OidIsValid(collid));

                                                                     
  if (collid == DEFAULT_COLLATION_OID)
  {
    return (pg_locale_t)0;
  }

  cache_entry = lookup_collation_cache(collid, false);

  if (cache_entry->locale == 0)
  {
                                                                
    HeapTuple tp;
    Form_pg_collation collform;
    const char *collcollate;
    const char *collctype pg_attribute_unused();
    struct pg_locale_struct result;
    pg_locale_t resultp;
    Datum collversion;
    bool isnull;

    tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(collid));
    if (!HeapTupleIsValid(tp))
    {
      elog(ERROR, "cache lookup failed for collation %u", collid);
    }
    collform = (Form_pg_collation)GETSTRUCT(tp);

    collcollate = NameStr(collform->collcollate);
    collctype = NameStr(collform->collctype);

                                                                          
    memset(&result, 0, sizeof(result));
    result.provider = collform->collprovider;
    result.deterministic = collform->collisdeterministic;

    if (collform->collprovider == COLLPROVIDER_LIBC)
    {
#ifdef HAVE_LOCALE_T
      locale_t loc;

      if (strcmp(collcollate, collctype) == 0)
      {
                                                
        errno = 0;
#ifndef WIN32
        loc = newlocale(LC_COLLATE_MASK | LC_CTYPE_MASK, collcollate, NULL);
#else
        loc = _create_locale(LC_ALL, collcollate);
#endif
        if (!loc)
        {
          report_newlocale_failure(collcollate);
        }
      }
      else
      {
#ifndef WIN32
                                           
        locale_t loc1;

        errno = 0;
        loc1 = newlocale(LC_COLLATE_MASK, collcollate, NULL);
        if (!loc1)
        {
          report_newlocale_failure(collcollate);
        }
        errno = 0;
        loc = newlocale(LC_CTYPE_MASK, collctype, loc1);
        if (!loc)
        {
          report_newlocale_failure(collctype);
        }
#else

           
                                                                  
                                                            
                                                       
           
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("collations with different collate and ctype values are not supported on this platform")));
#endif
      }

      result.info.lt = loc;
#else                         
                                                  
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("collation provider LIBC is not supported on this platform")));
#endif                        
    }
    else if (collform->collprovider == COLLPROVIDER_ICU)
    {
#ifdef USE_ICU
      UCollator *collator;
      UErrorCode status;

      if (strcmp(collcollate, collctype) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("collations with different collate and ctype values are not supported by ICU")));
      }

      status = U_ZERO_ERROR;
      collator = ucol_open(collcollate, &status);
      if (U_FAILURE(status))
      {
        ereport(ERROR, (errmsg("could not open collator for locale \"%s\": %s", collcollate, u_errorName(status))));
      }

      if (U_ICU_VERSION_MAJOR_NUM < 54)
      {
        icu_set_collation_attributes(collator, collcollate);
      }

                                                                 
      result.info.icu.locale = MemoryContextStrdup(TopMemoryContext, collcollate);
      result.info.icu.ucol = collator;
#else                   
                                                                         
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ICU is not supported in this build"), errhint("You need to rebuild PostgreSQL using --with-icu.")));
#endif                  
    }

    collversion = SysCacheGetAttr(COLLOID, tp, Anum_pg_collation_collversion, &isnull);
    if (!isnull)
    {
      char *actual_versionstr;
      char *collversionstr;

      actual_versionstr = get_collation_actual_version(collform->collprovider, collcollate);
      if (!actual_versionstr)
      {
           
                                                                 
                                                                       
                         
           
        ereport(ERROR, (errmsg("collation \"%s\" has no actual version, but a version was specified", NameStr(collform->collname))));
      }
      collversionstr = TextDatumGetCString(collversion);

      if (strcmp(actual_versionstr, collversionstr) != 0)
      {
        ereport(WARNING, (errmsg("collation \"%s\" has version mismatch", NameStr(collform->collname)),
                             errdetail("The collation in the database was created using version %s, "
                                       "but the operating system provides version %s.",
                                 collversionstr, actual_versionstr),
                             errhint("Rebuild all objects affected by this collation and run "
                                     "ALTER COLLATION %s REFRESH VERSION, "
                                     "or build PostgreSQL with the right library version.",
                                 quote_qualified_identifier(get_namespace_name(collform->collnamespace), NameStr(collform->collname)))));
      }
    }

    ReleaseSysCache(tp);

                                                                   
    resultp = MemoryContextAlloc(TopMemoryContext, sizeof(*resultp));
    *resultp = result;

    cache_entry->locale = resultp;
  }

  return cache_entry->locale;
}

   
                                                                               
                                 
   
                                                                               
                                                                            
                                        
   
char *
get_collation_actual_version(char collprovider, const char *collcollate)
{
  char *collversion;

#ifdef USE_ICU
  if (collprovider == COLLPROVIDER_ICU)
  {
    UCollator *collator;
    UErrorCode status;
    UVersionInfo versioninfo;
    char buf[U_MAX_VERSION_STRING_LENGTH];

    status = U_ZERO_ERROR;
    collator = ucol_open(collcollate, &status);
    if (U_FAILURE(status))
    {
      ereport(ERROR, (errmsg("could not open collator for locale \"%s\": %s", collcollate, u_errorName(status))));
    }
    ucol_getVersion(collator, versioninfo);
    ucol_close(collator);

    u_versionToString(versioninfo, buf);
    collversion = pstrdup(buf);
  }
  else
#endif
    collversion = NULL;

  return collversion;
}

#ifdef USE_ICU
   
                                                                             
                                                                              
                                  
   
static UConverter *icu_converter = NULL;

static void
init_icu_converter(void)
{
  const char *icu_encoding_name;
  UErrorCode status;
  UConverter *conv;

  if (icu_converter)
  {
    return;
  }

  icu_encoding_name = get_encoding_name_for_icu(GetDatabaseEncoding());

  status = U_ZERO_ERROR;
  conv = ucnv_open(icu_encoding_name, &status);
  if (U_FAILURE(status))
  {
    ereport(ERROR, (errmsg("could not open ICU converter for encoding \"%s\": %s", icu_encoding_name, u_errorName(status))));
  }

  icu_converter = conv;
}

   
                                                                      
   
                                                 
                                  
   
                                                                     
                                                            
   
                                                                        
                          
   
int32_t
icu_to_uchar(UChar **buff_uchar, const char *buff, size_t nbytes)
{
  UErrorCode status;
  int32_t len_uchar;

  init_icu_converter();

  status = U_ZERO_ERROR;
  len_uchar = ucnv_toUChars(icu_converter, NULL, 0, buff, nbytes, &status);
  if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
  {
    ereport(ERROR, (errmsg("%s failed: %s", "ucnv_toUChars", u_errorName(status))));
  }

  *buff_uchar = palloc((len_uchar + 1) * sizeof(**buff_uchar));

  status = U_ZERO_ERROR;
  len_uchar = ucnv_toUChars(icu_converter, *buff_uchar, len_uchar + 1, buff, nbytes, &status);
  if (U_FAILURE(status))
  {
    ereport(ERROR, (errmsg("%s failed: %s", "ucnv_toUChars", u_errorName(status))));
  }

  return len_uchar;
}

   
                                                          
   
                                                          
                                  
   
                                                                     
                                                                          
   
                                        
   
int32_t
icu_from_uchar(char **result, const UChar *buff_uchar, int32_t len_uchar)
{
  UErrorCode status;
  int32_t len_result;

  init_icu_converter();

  status = U_ZERO_ERROR;
  len_result = ucnv_fromUChars(icu_converter, NULL, 0, buff_uchar, len_uchar, &status);
  if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
  {
    ereport(ERROR, (errmsg("%s failed: %s", "ucnv_fromUChars", u_errorName(status))));
  }

  *result = palloc(len_result + 1);

  status = U_ZERO_ERROR;
  len_result = ucnv_fromUChars(icu_converter, *result, len_result + 1, buff_uchar, len_uchar, &status);
  if (U_FAILURE(status))
  {
    ereport(ERROR, (errmsg("%s failed: %s", "ucnv_fromUChars", u_errorName(status))));
  }

  return len_result;
}

   
                                                                               
                                                                           
                                    
   
                                                                               
                                                                               
             
   
pg_attribute_unused() static void icu_set_collation_attributes(UCollator *collator, const char *loc)
{
  char *str = asc_tolower(loc, strlen(loc));

  str = strchr(str, '@');
  if (!str)
  {
    return;
  }
  str++;

  for (char *token = strtok(str, ";"); token; token = strtok(NULL, ";"))
  {
    char *e = strchr(token, '=');

    if (e)
    {
      char *name;
      char *value;
      UColAttribute uattr;
      UColAttributeValue uvalue;
      UErrorCode status;

      status = U_ZERO_ERROR;

      *e = '\0';
      name = token;
      value = e + 1;

         
                                                                 
         
      if (strcmp(name, "colstrength") == 0)
      {
        uattr = UCOL_STRENGTH;
      }
      else if (strcmp(name, "colbackwards") == 0)
      {
        uattr = UCOL_FRENCH_COLLATION;
      }
      else if (strcmp(name, "colcaselevel") == 0)
      {
        uattr = UCOL_CASE_LEVEL;
      }
      else if (strcmp(name, "colcasefirst") == 0)
      {
        uattr = UCOL_CASE_FIRST;
      }
      else if (strcmp(name, "colalternate") == 0)
      {
        uattr = UCOL_ALTERNATE_HANDLING;
      }
      else if (strcmp(name, "colnormalization") == 0)
      {
        uattr = UCOL_NORMALIZATION_MODE;
      }
      else if (strcmp(name, "colnumeric") == 0)
      {
        uattr = UCOL_NUMERIC_COLLATION;
      }
      else
      {
                               
        continue;
      }

      if (strcmp(value, "primary") == 0)
      {
        uvalue = UCOL_PRIMARY;
      }
      else if (strcmp(value, "secondary") == 0)
      {
        uvalue = UCOL_SECONDARY;
      }
      else if (strcmp(value, "tertiary") == 0)
      {
        uvalue = UCOL_TERTIARY;
      }
      else if (strcmp(value, "quaternary") == 0)
      {
        uvalue = UCOL_QUATERNARY;
      }
      else if (strcmp(value, "identical") == 0)
      {
        uvalue = UCOL_IDENTICAL;
      }
      else if (strcmp(value, "no") == 0)
      {
        uvalue = UCOL_OFF;
      }
      else if (strcmp(value, "yes") == 0)
      {
        uvalue = UCOL_ON;
      }
      else if (strcmp(value, "shifted") == 0)
      {
        uvalue = UCOL_SHIFTED;
      }
      else if (strcmp(value, "non-ignorable") == 0)
      {
        uvalue = UCOL_NON_IGNORABLE;
      }
      else if (strcmp(value, "lower") == 0)
      {
        uvalue = UCOL_LOWER_FIRST;
      }
      else if (strcmp(value, "upper") == 0)
      {
        uvalue = UCOL_UPPER_FIRST;
      }
      else
      {
        status = U_ILLEGAL_ARGUMENT_ERROR;
      }

      if (status == U_ZERO_ERROR)
      {
        ucol_setAttribute(collator, uattr, uvalue, &status);
      }

         
                                                                       
                                      
         
      if (U_FAILURE(status))
      {
        ereport(ERROR, (errmsg("could not open collator for locale \"%s\": %s", loc, u_errorName(status))));
      }
    }
  }
}

#endif              

   
                                                                     
                                                                  
   

   
                                                              
   
                                                                               
                                                                           
                                                                           
   
size_t
wchar2char(char *to, const wchar_t *from, size_t tolen, pg_locale_t locale)
{
  size_t result;

  Assert(!locale || locale->provider == COLLPROVIDER_LIBC);

  if (tolen == 0)
  {
    return 0;
  }

#ifdef WIN32

     
                                                                           
                                                                           
                            
     
  if (GetDatabaseEncoding() == PG_UTF8)
  {
    result = WideCharToMultiByte(CP_UTF8, 0, from, -1, to, tolen, NULL, NULL);
                                  
    if (result <= 0)
    {
      result = -1;
    }
    else
    {
      Assert(result <= tolen);
                                                              
      result--;
    }
  }
  else
#endif            
    if (locale == (pg_locale_t)0)
    {
                                                        
      result = wcstombs(to, from, tolen);
    }
    else
    {
#ifdef HAVE_LOCALE_T
#ifdef HAVE_WCSTOMBS_L
                                                 
      result = wcstombs_l(to, from, tolen, locale->info.lt);
#else                        
                                                                    
      locale_t save_locale = uselocale(locale->info.lt);

      result = wcstombs(to, from, tolen);

      uselocale(save_locale);
#endif                      
#else                      
                                                      
    elog(ERROR, "wcstombs_l is not available");
    result = 0;                          
#endif                    
    }

  return result;
}

   
                                                                  
   
                                                                          
                                                                       
                                                                     
                                                                              
                                                         
   
size_t
char2wchar(wchar_t *to, size_t tolen, const char *from, size_t fromlen, pg_locale_t locale)
{
  size_t result;

  Assert(!locale || locale->provider == COLLPROVIDER_LIBC);

  if (tolen == 0)
  {
    return 0;
  }

#ifdef WIN32
                                         
  if (GetDatabaseEncoding() == PG_UTF8)
  {
                                                       
    if (fromlen == 0)
    {
      result = 0;
    }
    else
    {
      result = MultiByteToWideChar(CP_UTF8, 0, from, fromlen, to, tolen - 1);
                                    
      if (result == 0)
      {
        result = -1;
      }
    }

    if (result != -1)
    {
      Assert(result < tolen);
                                                                       
      to[result] = 0;
    }
  }
  else
#endif            
  {
                                       
    char *str = pnstrdup(from, fromlen);

    if (locale == (pg_locale_t)0)
    {
                                                        
      result = mbstowcs(to, str, tolen);
    }
    else
    {
#ifdef HAVE_LOCALE_T
#ifdef HAVE_MBSTOWCS_L
                                                 
      result = mbstowcs_l(to, str, tolen, locale->info.lt);
#else                        
                                                                    
      locale_t save_locale = uselocale(locale->info.lt);

      result = mbstowcs(to, str, tolen);

      uselocale(save_locale);
#endif                      
#else                      
                                                        
      elog(ERROR, "mbstowcs_l is not available");
      result = 0;                          
#endif                    
    }

    pfree(str);
  }

  if (result == -1)
  {
       
                                                                         
                                                                           
                                                                        
                                                                    
                                                                       
                                  
       
    pg_verifymbstr(from, fromlen, false);                       
                            
    ereport(ERROR, (errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE), errmsg("invalid multibyte character for locale"), errhint("The server's LC_CTYPE locale is probably incompatible with the database encoding.")));
  }

  return result;
}
