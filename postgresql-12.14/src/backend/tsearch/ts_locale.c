                                                                            
   
               
                                           
   
                                                                         
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "storage/fd.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_public.h"

static void
tsearch_readline_callback(void *arg);

   
                                                                          
                                                                           
                                                                           
                                                                        
                                                                            
                                                                               
                                                                            
   
#define WC_BUF_LEN 3

int
t_isdigit(const char *ptr)
{
  int clen = pg_mblen(ptr);
  wchar_t character[WC_BUF_LEN];
  Oid collation = DEFAULT_COLLATION_OID;           
  pg_locale_t mylocale = 0;                        

  if (clen == 1 || lc_ctype_is_c(collation))
  {
    return isdigit(TOUCHAR(ptr));
  }

  char2wchar(character, WC_BUF_LEN, ptr, clen, mylocale);

  return iswdigit((wint_t)character[0]);
}

int
t_isspace(const char *ptr)
{
  int clen = pg_mblen(ptr);
  wchar_t character[WC_BUF_LEN];
  Oid collation = DEFAULT_COLLATION_OID;           
  pg_locale_t mylocale = 0;                        

  if (clen == 1 || lc_ctype_is_c(collation))
  {
    return isspace(TOUCHAR(ptr));
  }

  char2wchar(character, WC_BUF_LEN, ptr, clen, mylocale);

  return iswspace((wint_t)character[0]);
}

int
t_isalpha(const char *ptr)
{
  int clen = pg_mblen(ptr);
  wchar_t character[WC_BUF_LEN];
  Oid collation = DEFAULT_COLLATION_OID;           
  pg_locale_t mylocale = 0;                        

  if (clen == 1 || lc_ctype_is_c(collation))
  {
    return isalpha(TOUCHAR(ptr));
  }

  char2wchar(character, WC_BUF_LEN, ptr, clen, mylocale);

  return iswalpha((wint_t)character[0]);
}

int
t_isprint(const char *ptr)
{
  int clen = pg_mblen(ptr);
  wchar_t character[WC_BUF_LEN];
  Oid collation = DEFAULT_COLLATION_OID;           
  pg_locale_t mylocale = 0;                        

  if (clen == 1 || lc_ctype_is_c(collation))
  {
    return isprint(TOUCHAR(ptr));
  }

  char2wchar(character, WC_BUF_LEN, ptr, clen, mylocale);

  return iswprint((wint_t)character[0]);
}

   
                                                                     
                                                                        
                                                                      
   
                      
   
                                 
   
                                                  
                    
                                            
                                                           
                      
                                                     
                   
                                 
   
                                                                      
                                                                          
                                                                
                           
   
bool
tsearch_readline_begin(tsearch_readline_state *stp, const char *filename)
{
  if ((stp->fp = AllocateFile(filename, "r")) == NULL)
  {
    return false;
  }
  stp->filename = filename;
  stp->lineno = 0;
  stp->curline = NULL;
                                                   
  stp->cb.callback = tsearch_readline_callback;
  stp->cb.arg = (void *)stp;
  stp->cb.previous = error_context_stack;
  error_context_stack = &stp->cb;
  return true;
}

   
                                                                              
                                                                               
                          
   
char *
tsearch_readline(tsearch_readline_state *stp)
{
  char *result;

                                                   
  stp->lineno++;

                                              
  if (stp->curline)
  {
    pfree(stp->curline);
    stp->curline = NULL;
  }

                                          
  result = t_readline(stp->fp);
  if (!result)
  {
    return NULL;
  }

     
                                                                            
                                                                           
                                                                      
     
  stp->curline = pstrdup(result);

  return result;
}

   
                                                           
   
void
tsearch_readline_end(tsearch_readline_state *stp)
{
                                                           
  if (stp->curline)
  {
    pfree(stp->curline);
    stp->curline = NULL;
  }

                               
  FreeFile(stp->fp);

                                   
  error_context_stack = stp->cb.previous;
}

   
                                                                       
                       
   
static void
tsearch_readline_callback(void *arg)
{
  tsearch_readline_state *stp = (tsearch_readline_state *)arg;

     
                                                                        
                                                                           
                                                                        
                                                                     
                                    
     
  if (stp->curline)
  {
    errcontext("line %d of configuration file \"%s\": \"%s\"", stp->lineno, stp->filename, stp->curline);
  }
  else
  {
    errcontext("line %d of configuration file \"%s\"", stp->lineno, stp->filename);
  }
}

   
                                                                              
                                                                               
                          
   
                                                                    
                                                         
   
char *
t_readline(FILE *fp)
{
  int len;
  char *recoded;
  char buf[4096];                                         

  if (fgets(buf, sizeof(buf), fp) == NULL)
  {
    return NULL;
  }

  len = strlen(buf);

                                          
  (void)pg_verify_mbstr(PG_UTF8, buf, len, false);

                   
  recoded = pg_any_to_server(buf, len, PG_UTF8);
  if (recoded == buf)
  {
       
                                                                           
                                                        
       
    recoded = pnstrdup(recoded, len);
  }

  return recoded;
}

   
                                                          
   
                               
   
char *
lowerstr(const char *str)
{
  return lowerstr_with_len(str, strlen(str));
}

   
                                                   
   
                                             
   
                               
   
char *
lowerstr_with_len(const char *str, int len)
{
  char *out;
  Oid collation = DEFAULT_COLLATION_OID;           
  pg_locale_t mylocale = 0;                        

  if (len == 0)
  {
    return pstrdup("");
  }

     
                                                                          
                                                                           
                                                                         
                                              
     
  if (pg_database_encoding_max_length() > 1 && !lc_ctype_is_c(collation))
  {
    wchar_t *wstr, *wptr;
    int wlen;

       
                                                                      
                                                                        
                                               
       
    wptr = wstr = (wchar_t *)palloc(sizeof(wchar_t) * (len + 1));

    wlen = char2wchar(wstr, len + 1, str, len, mylocale);
    Assert(wlen <= len);

    while (*wptr)
    {
      *wptr = towlower((wint_t)*wptr);
      wptr++;
    }

       
                                                 
       
    len = pg_database_encoding_max_length() * wlen + 1;
    out = (char *)palloc(len);

    wlen = wchar2char(out, wstr, len, mylocale);

    pfree(wstr);

    if (wlen < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE), errmsg("conversion from wchar_t to server encoding failed: %m")));
    }
    Assert(wlen < len);
  }
  else
  {
    const char *ptr = str;
    char *outptr;

    outptr = out = (char *)palloc(sizeof(char) * (len + 1));
    while ((ptr - str) < len && *ptr)
    {
      *outptr++ = tolower(TOUCHAR(ptr));
      ptr++;
    }
    *outptr = '\0';
  }

  return out;
}
