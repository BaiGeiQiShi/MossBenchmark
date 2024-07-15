                                                                            
   
               
                                               
   
   
                                                                
   
   
                  
                          
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include "mb/pg_wchar.h"

   
                                                                         
                                                                        
                                                                     
                                                                   
                    
   
                                                                  
                                                 
   
struct encoding_match
{
  enum pg_enc pg_enc_code;
  const char *system_enc_name;
};

static const struct encoding_match encoding_match_list[] = {
    {PG_EUC_JP, "EUC-JP"}, {PG_EUC_JP, "eucJP"}, {PG_EUC_JP, "IBM-eucJP"}, {PG_EUC_JP, "sdeckanji"}, {PG_EUC_JP, "CP20932"},

    {PG_EUC_CN, "EUC-CN"}, {PG_EUC_CN, "eucCN"}, {PG_EUC_CN, "IBM-eucCN"}, {PG_EUC_CN, "GB2312"}, {PG_EUC_CN, "dechanzi"}, {PG_EUC_CN, "CP20936"},

    {PG_EUC_KR, "EUC-KR"}, {PG_EUC_KR, "eucKR"}, {PG_EUC_KR, "IBM-eucKR"}, {PG_EUC_KR, "deckorean"}, {PG_EUC_KR, "5601"}, {PG_EUC_KR, "CP51949"},

    {PG_EUC_TW, "EUC-TW"}, {PG_EUC_TW, "eucTW"}, {PG_EUC_TW, "IBM-eucTW"}, {PG_EUC_TW, "cns11643"},
                                  

    {PG_UTF8, "UTF-8"}, {PG_UTF8, "utf8"}, {PG_UTF8, "CP65001"},

    {PG_LATIN1, "ISO-8859-1"}, {PG_LATIN1, "ISO8859-1"}, {PG_LATIN1, "iso88591"}, {PG_LATIN1, "CP28591"},

    {PG_LATIN2, "ISO-8859-2"}, {PG_LATIN2, "ISO8859-2"}, {PG_LATIN2, "iso88592"}, {PG_LATIN2, "CP28592"},

    {PG_LATIN3, "ISO-8859-3"}, {PG_LATIN3, "ISO8859-3"}, {PG_LATIN3, "iso88593"}, {PG_LATIN3, "CP28593"},

    {PG_LATIN4, "ISO-8859-4"}, {PG_LATIN4, "ISO8859-4"}, {PG_LATIN4, "iso88594"}, {PG_LATIN4, "CP28594"},

    {PG_LATIN5, "ISO-8859-9"}, {PG_LATIN5, "ISO8859-9"}, {PG_LATIN5, "iso88599"}, {PG_LATIN5, "CP28599"},

    {PG_LATIN6, "ISO-8859-10"}, {PG_LATIN6, "ISO8859-10"}, {PG_LATIN6, "iso885910"},

    {PG_LATIN7, "ISO-8859-13"}, {PG_LATIN7, "ISO8859-13"}, {PG_LATIN7, "iso885913"},

    {PG_LATIN8, "ISO-8859-14"}, {PG_LATIN8, "ISO8859-14"}, {PG_LATIN8, "iso885914"},

    {PG_LATIN9, "ISO-8859-15"}, {PG_LATIN9, "ISO8859-15"}, {PG_LATIN9, "iso885915"}, {PG_LATIN9, "CP28605"},

    {PG_LATIN10, "ISO-8859-16"}, {PG_LATIN10, "ISO8859-16"}, {PG_LATIN10, "iso885916"},

    {PG_KOI8R, "KOI8-R"}, {PG_KOI8R, "CP20866"},

    {PG_KOI8U, "KOI8-U"}, {PG_KOI8U, "CP21866"},

    {PG_WIN866, "CP866"}, {PG_WIN874, "CP874"}, {PG_WIN1250, "CP1250"}, {PG_WIN1251, "CP1251"}, {PG_WIN1251, "ansi-1251"}, {PG_WIN1252, "CP1252"}, {PG_WIN1253, "CP1253"}, {PG_WIN1254, "CP1254"}, {PG_WIN1255, "CP1255"}, {PG_WIN1256, "CP1256"}, {PG_WIN1257, "CP1257"}, {PG_WIN1258, "CP1258"},

    {PG_ISO_8859_5, "ISO-8859-5"}, {PG_ISO_8859_5, "ISO8859-5"}, {PG_ISO_8859_5, "iso88595"}, {PG_ISO_8859_5, "CP28595"},

    {PG_ISO_8859_6, "ISO-8859-6"}, {PG_ISO_8859_6, "ISO8859-6"}, {PG_ISO_8859_6, "iso88596"}, {PG_ISO_8859_6, "CP28596"},

    {PG_ISO_8859_7, "ISO-8859-7"}, {PG_ISO_8859_7, "ISO8859-7"}, {PG_ISO_8859_7, "iso88597"}, {PG_ISO_8859_7, "CP28597"},

    {PG_ISO_8859_8, "ISO-8859-8"}, {PG_ISO_8859_8, "ISO8859-8"}, {PG_ISO_8859_8, "iso88598"}, {PG_ISO_8859_8, "CP28598"},

    {PG_SJIS, "SJIS"}, {PG_SJIS, "PCK"}, {PG_SJIS, "CP932"}, {PG_SJIS, "SHIFT_JIS"},

    {PG_BIG5, "BIG5"}, {PG_BIG5, "BIG5HKSCS"}, {PG_BIG5, "Big5-HKSCS"}, {PG_BIG5, "CP950"},

    {PG_GBK, "GBK"}, {PG_GBK, "CP936"},

    {PG_UHC, "UHC"}, {PG_UHC, "CP949"},

    {PG_JOHAB, "JOHAB"}, {PG_JOHAB, "CP1361"},

    {PG_GB18030, "GB18030"}, {PG_GB18030, "CP54936"},

    {PG_SHIFT_JIS_2004, "SJIS_2004"},

    {PG_SQL_ASCII, "US-ASCII"},

    {PG_SQL_ASCII, NULL}                 
};

#ifdef WIN32
   
                                                                            
   
                                                                             
                                                                               
                                                                
   
                                                                               
                                                                            
                                                                             
                                                                            
                                                                            
                                                                         
                                                                                
                                                                  
                                                    
   
                                                       
   
static char *
win32_langinfo(const char *ctype)
{
  char *r = NULL;

#if (_MSC_VER >= 1700) && (_MSC_VER < 1900)
  _locale_t loct = NULL;

  loct = _create_locale(LC_CTYPE, ctype);
  if (loct != NULL)
  {
    r = malloc(16);             
    if (r != NULL)
    {
      sprintf(r, "CP%u", loct->locinfo->lc_codepage);
    }
    _free_locale(loct);
  }
#else
  char *codepage;

#if (_MSC_VER >= 1900)
  uint32 cp;
  WCHAR wctype[LOCALE_NAME_MAX_LENGTH];

  memset(wctype, 0, sizeof(wctype));
  MultiByteToWideChar(CP_ACP, 0, ctype, -1, wctype, LOCALE_NAME_MAX_LENGTH);

  if (GetLocaleInfoEx(wctype, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER, (LPWSTR)&cp, sizeof(cp) / sizeof(WCHAR)) > 0)
  {
    r = malloc(16);             
    if (r != NULL)
    {
         
                                                                       
                                                                
         
      if (cp == CP_ACP)
      {
        strcpy(r, "utf8");
      }
      else
      {
        sprintf(r, "CP%u", cp);
      }
    }
  }
  else
#endif
  {
       
                                                                       
                                                                        
                                                                        
                                                                          
                                                                          
                                                                           
                             
       
    codepage = strrchr(ctype, '.');
    if (codepage != NULL)
    {
      size_t ln;

      codepage++;
      ln = strlen(codepage);
      r = malloc(ln + 3);
      if (r != NULL)
      {
        if (strspn(codepage, "0123456789") == ln)
        {
          sprintf(r, "CP%s", codepage);
        }
        else
        {
          strcpy(r, codepage);
        }
      }
    }
  }
#endif

  return r;
}

#ifndef FRONTEND
   
                                                                           
                                                           
   
int
pg_codepage_to_encoding(UINT cp)
{
  char sys[16];
  int i;

  sprintf(sys, "CP%u", cp);

                       
  for (i = 0; encoding_match_list[i].system_enc_name; i++)
  {
    if (pg_strcasecmp(sys, encoding_match_list[i].system_enc_name) == 0)
    {
      return encoding_match_list[i].pg_enc_code;
    }
  }

  ereport(WARNING, (errmsg("could not determine encoding for codeset \"%s\"", sys)));

  return -1;
}
#endif
#endif            

#if (defined(HAVE_LANGINFO_H) && defined(CODESET)) || defined(WIN32)

   
                                                                          
                                                                          
   
                                                                    
                                                                     
   
                                                                              
                              
   
                                                                            
                                                                          
   
int
pg_get_encoding_from_locale(const char *ctype, bool write_message)
{
  char *sys;
  int i;

                                                                    
  if (ctype)
  {
    char *save;
    char *name;

                                                             
    if (pg_strcasecmp(ctype, "C") == 0 || pg_strcasecmp(ctype, "POSIX") == 0)
    {
      return PG_SQL_ASCII;
    }

    save = setlocale(LC_CTYPE, NULL);
    if (!save)
    {
      return -1;                          
    }
                                                              
    save = strdup(save);
    if (!save)
    {
      return -1;                              
    }

    name = setlocale(LC_CTYPE, ctype);
    if (!name)
    {
      free(save);
      return -1;                             
    }

#ifndef WIN32
    sys = nl_langinfo(CODESET);
    if (sys)
    {
      sys = strdup(sys);
    }
#else
    sys = win32_langinfo(name);
#endif

    setlocale(LC_CTYPE, save);
    free(save);
  }
  else
  {
                        
    ctype = setlocale(LC_CTYPE, NULL);
    if (!ctype)
    {
      return -1;                          
    }

                                                             
    if (pg_strcasecmp(ctype, "C") == 0 || pg_strcasecmp(ctype, "POSIX") == 0)
    {
      return PG_SQL_ASCII;
    }

#ifndef WIN32
    sys = nl_langinfo(CODESET);
    if (sys)
    {
      sys = strdup(sys);
    }
#else
    sys = win32_langinfo(ctype);
#endif
  }

  if (!sys)
  {
    return -1;                              
  }

                       
  for (i = 0; encoding_match_list[i].system_enc_name; i++)
  {
    if (pg_strcasecmp(sys, encoding_match_list[i].system_enc_name) == 0)
    {
      free(sys);
      return encoding_match_list[i].pg_enc_code;
    }
  }

                                                            

#ifdef __darwin__

     
                                                                             
                                              
     
  if (strlen(sys) == 0)
  {
    free(sys);
    return PG_UTF8;
  }
#endif

     
                                                                          
                                                         
     
  if (write_message)
  {
#ifdef FRONTEND
    fprintf(stderr, _("could not determine encoding for locale \"%s\": codeset is \"%s\""), ctype, sys);
                                                                       
    fputc('\n', stderr);
#else
    ereport(WARNING, (errmsg("could not determine encoding for locale \"%s\": codeset is \"%s\"", ctype, sys)));
#endif
  }

  free(sys);
  return -1;
}
#else                                            

   
                                              
   
                                                                    
                                                                     
                                                     
   
int
pg_get_encoding_from_locale(const char *ctype, bool write_message)
{
  return PG_SQL_ASCII;
}

#endif                                            
