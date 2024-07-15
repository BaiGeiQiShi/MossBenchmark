                                                                            
   
            
                                                            
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                         
                                                                      
                                                                 
                                                                        
                                                                   
                 
   
                                                               
                                                                         
                                                                  
                                                          
                                                                     
                                                                    
                                                        
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "regex/regex.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/varlena.h"

#define PG_GETARG_TEXT_PP_IF_EXISTS(_n) (PG_NARGS() > (_n) ? PG_GETARG_TEXT_PP(_n) : NULL)

                                                     
typedef struct pg_re_flags
{
  int cflags;                                             
  bool glob;                                            
} pg_re_flags;

                                                                  
typedef struct regexp_matches_ctx
{
  text *orig_str;                                        
  int nmatches;                                               
  int npatterns;                                       
                                                                     
                                                                          
  int *match_locs;                                
  int next_match;                                              
                                                 
  Datum *elems;                                   
  bool *nulls;                                    
  pg_wchar *wide_str;                                           
  char *conv_buf;                            
  int conv_bufsiz;                      
} regexp_matches_ctx;

   
                                                                           
                                                                      
                                                                     
                                                                              
   
                                                                
                                                                         
                                                                           
                                                                           
                                                                              
                                                                           
                                                  
   
                                                                           
                                                                           
                                                                            
                                                                             
                                                                        
                                                                             
                                                                           
                                                                              
   

                                                              
#ifndef MAX_CACHED_RES
#define MAX_CACHED_RES 32
#endif

                                                            
typedef struct cached_re_str
{
  char *cre_pat;                                             
  int cre_pat_len;                                        
  int cre_flags;                                            
  Oid cre_collation;                       
  regex_t cre_re;                                         
} cached_re_str;

static int num_res = 0;                                              
static cached_re_str re_array[MAX_CACHED_RES];                  

                     
static regexp_matches_ctx *
setup_regexp_matches(text *orig_str, text *pattern, pg_re_flags *flags, Oid collation, bool use_subpatterns, bool ignore_degenerate, bool fetching_unmatched);
static ArrayType *
build_regexp_match_result(regexp_matches_ctx *matchctx);
static Datum
build_regexp_split_result(regexp_matches_ctx *splitctx);

   
                                                            
   
                     
   
                                                       
                                              
                                                                  
   
                                                                        
                                                                      
   
regex_t *
RE_compile_and_cache(text *text_re, int cflags, Oid collation)
{
  int text_re_len = VARSIZE_ANY_EXHDR(text_re);
  char *text_re_val = VARDATA_ANY(text_re);
  pg_wchar *pattern;
  int pattern_len;
  int i;
  int regcomp_result;
  cached_re_str re_temp;
  char errMsg[100];

     
                                                                     
                                                                           
                                                         
     
  for (i = 0; i < num_res; i++)
  {
    if (re_array[i].cre_pat_len == text_re_len && re_array[i].cre_flags == cflags && re_array[i].cre_collation == collation && memcmp(re_array[i].cre_pat, text_re_val, text_re_len) == 0)
    {
         
                                                               
         
      if (i > 0)
      {
        re_temp = re_array[i];
        memmove(&re_array[1], &re_array[0], i * sizeof(cached_re_str));
        re_array[0] = re_temp;
      }

      return &re_array[0].cre_re;
    }
  }

     
                                                                       
                                                            
     

                                                 
  pattern = (pg_wchar *)palloc((text_re_len + 1) * sizeof(pg_wchar));
  pattern_len = pg_mb2wchar_with_len(text_re_val, pattern, text_re_len);

  regcomp_result = pg_regcomp(&re_temp.cre_re, pattern, pattern_len, cflags, collation);

  pfree(pattern);

  if (regcomp_result != REG_OKAY)
  {
                                                           

       
                                                                      
                                                                     
                                                                      
                                                   
       
    CHECK_FOR_INTERRUPTS();

    pg_regerror(regcomp_result, &re_temp.cre_re, errMsg, sizeof(errMsg));
    ereport(ERROR, (errcode(ERRCODE_INVALID_REGULAR_EXPRESSION), errmsg("invalid regular expression: %s", errMsg)));
  }

     
                                                                         
                                                                             
                                                                             
                         
     
  re_temp.cre_pat = malloc(Max(text_re_len, 1));
  if (re_temp.cre_pat == NULL)
  {
    pg_regfree(&re_temp.cre_re);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
  }
  memcpy(re_temp.cre_pat, text_re_val, text_re_len);
  re_temp.cre_pat_len = text_re_len;
  re_temp.cre_flags = cflags;
  re_temp.cre_collation = collation;

     
                                                                           
                                           
     
  if (num_res >= MAX_CACHED_RES)
  {
    --num_res;
    Assert(num_res < MAX_CACHED_RES);
    pg_regfree(&re_array[num_res].cre_re);
    free(re_array[num_res].cre_pat);
  }

  if (num_res > 0)
  {
    memmove(&re_array[1], &re_array[0], num_res * sizeof(cached_re_str));
  }

  re_array[0] = re_temp;
  num_res++;

  return &re_array[0].cre_re;
}

   
                                                    
   
                                            
   
                                                                   
                                                                    
                                              
                                                             
                                                             
   
                                                                            
          
   
static bool
RE_wchar_execute(regex_t *re, pg_wchar *data, int data_len, int start_search, int nmatch, regmatch_t *pmatch)
{
  int regexec_result;
  char errMsg[100];

                                          
  regexec_result = pg_regexec(re, data, data_len, start_search, NULL,                 
      nmatch, pmatch, 0);

  if (regexec_result != REG_OKAY && regexec_result != REG_NOMATCH)
  {
                      
    CHECK_FOR_INTERRUPTS();
    pg_regerror(regexec_result, re, errMsg, sizeof(errMsg));
    ereport(ERROR, (errcode(ERRCODE_INVALID_REGULAR_EXPRESSION), errmsg("regular expression failed: %s", errMsg)));
  }

  return (regexec_result == REG_OKAY);
}

   
                             
   
                                            
   
                                                                   
                                                                   
                                             
                                                             
   
                                                          
                                                                             
   
static bool
RE_execute(regex_t *re, char *dat, int dat_len, int nmatch, regmatch_t *pmatch)
{
  pg_wchar *data;
  int data_len;
  bool match;

                                              
  data = (pg_wchar *)palloc((dat_len + 1) * sizeof(pg_wchar));
  data_len = pg_mb2wchar_with_len(dat, data, dat_len);

                                          
  match = RE_wchar_execute(re, data, data_len, 0, nmatch, pmatch);

  pfree(data);
  return match;
}

   
                                                     
   
                                            
   
                                                       
                                                                   
                                             
                                              
                                                                  
                                                             
   
                                                                            
                                                                             
   
bool
RE_compile_and_execute(text *text_re, char *dat, int dat_len, int cflags, Oid collation, int nmatch, regmatch_t *pmatch)
{
  regex_t *re;

                  
  re = RE_compile_and_cache(text_re, cflags, collation);

  return RE_execute(re, dat, dat_len, nmatch, pmatch);
}

   
                                                                           
   
                                                          
                                              
   
                                                                            
                                                       
   
static void
parse_re_flags(pg_re_flags *flags, text *opts)
{
                                                            
  flags->cflags = REG_ADVANCED;
  flags->glob = false;

  if (opts)
  {
    char *opt_p = VARDATA_ANY(opts);
    int opt_len = VARSIZE_ANY_EXHDR(opts);
    int i;

    for (i = 0; i < opt_len; i++)
    {
      switch (opt_p[i])
      {
      case 'g':
        flags->glob = true;
        break;
      case 'b':                        
        flags->cflags &= ~(REG_ADVANCED | REG_EXTENDED | REG_QUOTE);
        break;
      case 'c':                     
        flags->cflags &= ~REG_ICASE;
        break;
      case 'e':                 
        flags->cflags |= REG_EXTENDED;
        flags->cflags &= ~(REG_ADVANCED | REG_QUOTE);
        break;
      case 'i':                       
        flags->cflags |= REG_ICASE;
        break;
      case 'm':                            
      case 'n':                          
        flags->cflags |= REG_NEWLINE;
        break;
      case 'p':                             
        flags->cflags |= REG_NLSTOP;
        flags->cflags &= ~REG_NLANCH;
        break;
      case 'q':                     
        flags->cflags |= REG_QUOTE;
        flags->cflags &= ~(REG_ADVANCED | REG_EXTENDED);
        break;
      case 's':                               
        flags->cflags &= ~REG_NEWLINE;
        break;
      case 't':                   
        flags->cflags &= ~REG_EXPANDED;
        break;
      case 'w':                                 
        flags->cflags &= ~REG_NLSTOP;
        flags->cflags |= REG_NLANCH;
        break;
      case 'x':                      
        flags->cflags |= REG_EXPANDED;
        break;
      default:
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid regular expression option: \"%c\"", opt_p[i])));
        break;
      }
    }
  }
}

   
                                                     
   

Datum
nameregexeq(PG_FUNCTION_ARGS)
{
  Name n = PG_GETARG_NAME(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(RE_compile_and_execute(p, NameStr(*n), strlen(NameStr(*n)), REG_ADVANCED, PG_GET_COLLATION(), 0, NULL));
}

Datum
nameregexne(PG_FUNCTION_ARGS)
{
  Name n = PG_GETARG_NAME(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(!RE_compile_and_execute(p, NameStr(*n), strlen(NameStr(*n)), REG_ADVANCED, PG_GET_COLLATION(), 0, NULL));
}

Datum
textregexeq(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(RE_compile_and_execute(p, VARDATA_ANY(s), VARSIZE_ANY_EXHDR(s), REG_ADVANCED, PG_GET_COLLATION(), 0, NULL));
}

Datum
textregexne(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(!RE_compile_and_execute(p, VARDATA_ANY(s), VARSIZE_ANY_EXHDR(s), REG_ADVANCED, PG_GET_COLLATION(), 0, NULL));
}

   
                                                            
                                                     
   

Datum
nameicregexeq(PG_FUNCTION_ARGS)
{
  Name n = PG_GETARG_NAME(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(RE_compile_and_execute(p, NameStr(*n), strlen(NameStr(*n)), REG_ADVANCED | REG_ICASE, PG_GET_COLLATION(), 0, NULL));
}

Datum
nameicregexne(PG_FUNCTION_ARGS)
{
  Name n = PG_GETARG_NAME(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(!RE_compile_and_execute(p, NameStr(*n), strlen(NameStr(*n)), REG_ADVANCED | REG_ICASE, PG_GET_COLLATION(), 0, NULL));
}

Datum
texticregexeq(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(RE_compile_and_execute(p, VARDATA_ANY(s), VARSIZE_ANY_EXHDR(s), REG_ADVANCED | REG_ICASE, PG_GET_COLLATION(), 0, NULL));
}

Datum
texticregexne(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);

  PG_RETURN_BOOL(!RE_compile_and_execute(p, VARDATA_ANY(s), VARSIZE_ANY_EXHDR(s), REG_ADVANCED | REG_ICASE, PG_GET_COLLATION(), 0, NULL));
}

   
                     
                                                        
   
Datum
textregexsubstr(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);
  regex_t *re;
  regmatch_t pmatch[2];
  int so, eo;

                  
  re = RE_compile_and_cache(p, REG_ADVANCED, PG_GET_COLLATION());

     
                                                                            
                                                                            
                                                                       
                                           
     
  if (!RE_execute(re, VARDATA_ANY(s), VARSIZE_ANY_EXHDR(s), 2, pmatch))
  {
    PG_RETURN_NULL();                          
  }

  if (re->re_nsub > 0)
  {
                                                             
    so = pmatch[1].rm_so;
    eo = pmatch[1].rm_eo;
  }
  else
  {
                                                         
    so = pmatch[0].rm_so;
    eo = pmatch[0].rm_eo;
  }

     
                                                                            
                                                                             
                                                                            
                       
     
  if (so < 0 || eo < 0)
  {
    PG_RETURN_NULL();
  }

  return DirectFunctionCall3(text_substr, PointerGetDatum(s), Int32GetDatum(so + 1), Int32GetDatum(eo - so));
}

   
                            
                                                                       
   
                                                                    
                                                     
   
Datum
textregexreplace_noopt(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);
  text *r = PG_GETARG_TEXT_PP(2);
  regex_t *re;

  re = RE_compile_and_cache(p, REG_ADVANCED, PG_GET_COLLATION());

  PG_RETURN_TEXT_P(replace_text_regexp(s, (void *)re, r, false));
}

   
                      
                                                                       
   
Datum
textregexreplace(PG_FUNCTION_ARGS)
{
  text *s = PG_GETARG_TEXT_PP(0);
  text *p = PG_GETARG_TEXT_PP(1);
  text *r = PG_GETARG_TEXT_PP(2);
  text *opt = PG_GETARG_TEXT_PP(3);
  regex_t *re;
  pg_re_flags flags;

  parse_re_flags(&flags, opt);

  re = RE_compile_and_cache(p, flags.cflags, PG_GET_COLLATION());

  PG_RETURN_TEXT_P(replace_text_regexp(s, (void *)re, r, flags.glob));
}

   
                    
                                                                          
                      
   
Datum
similar_escape(PG_FUNCTION_ARGS)
{
  text *pat_text;
  text *esc_text;
  text *result;
  char *p, *e, *r;
  int plen, elen;
  bool afterescape = false;
  bool incharclass = false;
  int nquotes = 0;

                                                            
  if (PG_ARGISNULL(0))
  {
    PG_RETURN_NULL();
  }
  pat_text = PG_GETARG_TEXT_PP(0);
  p = VARDATA_ANY(pat_text);
  plen = VARSIZE_ANY_EXHDR(pat_text);
  if (PG_ARGISNULL(1))
  {
                                                                   
    e = "\\";
    elen = 1;
  }
  else
  {
    esc_text = PG_GETARG_TEXT_PP(1);
    e = VARDATA_ANY(esc_text);
    elen = VARSIZE_ANY_EXHDR(esc_text);
    if (elen == 0)
    {
      e = NULL;                          
    }
    else
    {
      int escape_mblen = pg_mbstrlen_with_len(e, elen);

      if (escape_mblen > 1)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_ESCAPE_SEQUENCE), errmsg("invalid escape string"), errhint("Escape string must be empty or one character.")));
      }
    }
  }

               
                                                   
                   
                                                                    
                                                                       
                                                                          
                                                                       
                                                                        
                                                                           
                                                   
     
                                                                           
                     
                                              
                                                                           
                                                                           
                                                                         
                                                                           
                                                                           
                                                                     
                                                                             
                                                                           
                                                                          
     
                                                                         
                                                                          
                                                                         
                                                                        
                                                                         
                                                                        
                                                                
               
     

     
                                                                           
                                                                           
                            
     
  result = (text *)palloc(VARHDRSZ + 23 + 3 * (size_t)plen);
  r = VARDATA(result);

  *r++ = '^';
  *r++ = '(';
  *r++ = '?';
  *r++ = ':';

  while (plen > 0)
  {
    char pchar = *p;

       
                                                                       
                                                              
       
                                                                         
                                                                    
                                                                   
                                                              
                                                         
       

    if (elen > 1)
    {
      int mblen = pg_mblen(p);

      if (mblen > 1)
      {
                                   
        if (afterescape)
        {
          *r++ = '\\';
          memcpy(r, p, mblen);
          r += mblen;
          afterescape = false;
        }
        else if (e && elen == mblen && memcmp(e, p, mblen) == 0)
        {
                                                           
          afterescape = true;
        }
        else
        {
             
                                                                   
                                                                 
                               
             
          memcpy(r, p, mblen);
          r += mblen;
        }

        p += mblen;
        plen -= mblen;

        continue;
      }
    }

                   
    if (afterescape)
    {
      if (pchar == '"' && !incharclass)                           
      {
                                                              
        if (nquotes == 0)
        {
          *r++ = ')';
          *r++ = '{';
          *r++ = '1';
          *r++ = ',';
          *r++ = '1';
          *r++ = '}';
          *r++ = '?';
          *r++ = '(';
        }
        else if (nquotes == 1)
        {
          *r++ = ')';
          *r++ = '{';
          *r++ = '1';
          *r++ = ',';
          *r++ = '1';
          *r++ = '}';
          *r++ = '(';
          *r++ = '?';
          *r++ = ':';
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_USE_OF_ESCAPE_CHARACTER), errmsg("SQL regular expression may not contain more than two escape-double-quote separators")));
        }
        nquotes++;
      }
      else
      {
           
                                                                      
                                                                  
                                                                 
           
        *r++ = '\\';
        *r++ = pchar;
      }
      afterescape = false;
    }
    else if (e && pchar == *e)
    {
                                                       
      afterescape = true;
    }
    else if (incharclass)
    {
      if (pchar == '\\')
      {
        *r++ = '\\';
      }
      *r++ = pchar;
      if (pchar == ']')
      {
        incharclass = false;
      }
    }
    else if (pchar == '[')
    {
      *r++ = pchar;
      incharclass = true;
    }
    else if (pchar == '%')
    {
      *r++ = '.';
      *r++ = '*';
    }
    else if (pchar == '_')
    {
      *r++ = '.';
    }
    else if (pchar == '(')
    {
                                                
      *r++ = '(';
      *r++ = '?';
      *r++ = ':';
    }
    else if (pchar == '\\' || pchar == '.' || pchar == '^' || pchar == '$')
    {
      *r++ = '\\';
      *r++ = pchar;
    }
    else
    {
      *r++ = pchar;
    }
    p++, plen--;
  }

  *r++ = ')';
  *r++ = '$';

  SET_VARSIZE(result, r - ((char *)result));

  PG_RETURN_TEXT_P(result);
}

   
                  
                                                                      
   
Datum
regexp_match(PG_FUNCTION_ARGS)
{
  text *orig_str = PG_GETARG_TEXT_PP(0);
  text *pattern = PG_GETARG_TEXT_PP(1);
  text *flags = PG_GETARG_TEXT_PP_IF_EXISTS(2);
  pg_re_flags re_flags;
  regexp_matches_ctx *matchctx;

                         
  parse_re_flags(&re_flags, flags);
                                
  if (re_flags.glob)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                                                  
                       errmsg("%s does not support the \"global\" option", "regexp_match()"), errhint("Use the regexp_matches function instead.")));
  }

  matchctx = setup_regexp_matches(orig_str, pattern, &re_flags, PG_GET_COLLATION(), true, false, false);

  if (matchctx->nmatches == 0)
  {
    PG_RETURN_NULL();
  }

  Assert(matchctx->nmatches == 1);

                                                             
  matchctx->elems = (Datum *)palloc(sizeof(Datum) * matchctx->npatterns);
  matchctx->nulls = (bool *)palloc(sizeof(bool) * matchctx->npatterns);

  PG_RETURN_DATUM(PointerGetDatum(build_regexp_match_result(matchctx)));
}

                                                                              
Datum
regexp_match_no_flags(PG_FUNCTION_ARGS)
{
  return regexp_match(fcinfo);
}

   
                    
                                                                
   
Datum
regexp_matches(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  regexp_matches_ctx *matchctx;

  if (SRF_IS_FIRSTCALL())
  {
    text *pattern = PG_GETARG_TEXT_PP(1);
    text *flags = PG_GETARG_TEXT_PP_IF_EXISTS(2);
    pg_re_flags re_flags;
    MemoryContext oldcontext;

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                           
    parse_re_flags(&re_flags, flags);

                                                                  
    matchctx = setup_regexp_matches(PG_GETARG_TEXT_P_COPY(0), pattern, &re_flags, PG_GET_COLLATION(), true, false, false);

                                                                   
    matchctx->elems = (Datum *)palloc(sizeof(Datum) * matchctx->npatterns);
    matchctx->nulls = (bool *)palloc(sizeof(bool) * matchctx->npatterns);

    MemoryContextSwitchTo(oldcontext);
    funcctx->user_fctx = (void *)matchctx;
  }

  funcctx = SRF_PERCALL_SETUP();
  matchctx = (regexp_matches_ctx *)funcctx->user_fctx;

  if (matchctx->next_match < matchctx->nmatches)
  {
    ArrayType *result_ary;

    result_ary = build_regexp_match_result(matchctx);
    matchctx->next_match++;
    SRF_RETURN_NEXT(funcctx, PointerGetDatum(result_ary));
  }

  SRF_RETURN_DONE(funcctx);
}

                                                                              
Datum
regexp_matches_no_flags(PG_FUNCTION_ARGS)
{
  return regexp_matches(fcinfo);
}

   
                                                                     
                               
   
                                                                       
                                                                            
                                                             
   
                                                                               
                                                                             
                                                                              
                                                                             
                                                                             
                                                                                
                                                                               
                      
   
static regexp_matches_ctx *
setup_regexp_matches(text *orig_str, text *pattern, pg_re_flags *re_flags, Oid collation, bool use_subpatterns, bool ignore_degenerate, bool fetching_unmatched)
{
  regexp_matches_ctx *matchctx = palloc0(sizeof(regexp_matches_ctx));
  int eml = pg_database_encoding_max_length();
  int orig_len;
  pg_wchar *wide_str;
  int wide_len;
  regex_t *cpattern;
  regmatch_t *pmatch;
  int pmatch_len;
  int array_len;
  int array_idx;
  int prev_match_end;
  int prev_valid_match_end;
  int start_search;
  int maxlen = 0;                                         

                                                                        
  matchctx->orig_str = orig_str;

                                                    
  orig_len = VARSIZE_ANY_EXHDR(orig_str);
  wide_str = (pg_wchar *)palloc(sizeof(pg_wchar) * (orig_len + 1));
  wide_len = pg_mb2wchar_with_len(VARDATA_ANY(orig_str), wide_str, orig_len);

                                   
  cpattern = RE_compile_and_cache(pattern, re_flags->cflags, collation);

                                           
  if (use_subpatterns && cpattern->re_nsub > 0)
  {
    matchctx->npatterns = cpattern->re_nsub;
    pmatch_len = cpattern->re_nsub + 1;
  }
  else
  {
    use_subpatterns = false;
    matchctx->npatterns = 1;
    pmatch_len = 1;
  }

                                             
  pmatch = palloc(sizeof(regmatch_t) * pmatch_len);

     
                                                         
     
                                                                          
                  
     
  array_len = re_flags->glob ? 255 : 31;
  matchctx->match_locs = (int *)palloc(sizeof(int) * array_len);
  array_idx = 0;

                                                  
  prev_match_end = 0;
  prev_valid_match_end = 0;
  start_search = 0;
  while (RE_wchar_execute(cpattern, wide_str, wide_len, start_search, pmatch_len, pmatch))
  {
       
                                                                      
                                                                         
                       
       
    if (!ignore_degenerate || (pmatch[0].rm_so < wide_len && pmatch[0].rm_eo > prev_match_end))
    {
                                          
      while (array_idx + matchctx->npatterns * 2 + 1 > array_len)
      {
        array_len += array_len + 1;                         
        if (array_len > MaxAllocSize / sizeof(int))
        {
          ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many regular expression matches")));
        }
        matchctx->match_locs = (int *)repalloc(matchctx->match_locs, sizeof(int) * array_len);
      }

                                       
      if (use_subpatterns)
      {
        int i;

        for (i = 1; i <= matchctx->npatterns; i++)
        {
          int so = pmatch[i].rm_so;
          int eo = pmatch[i].rm_eo;

          matchctx->match_locs[array_idx++] = so;
          matchctx->match_locs[array_idx++] = eo;
          if (so >= 0 && eo >= 0 && (eo - so) > maxlen)
          {
            maxlen = (eo - so);
          }
        }
      }
      else
      {
        int so = pmatch[0].rm_so;
        int eo = pmatch[0].rm_eo;

        matchctx->match_locs[array_idx++] = so;
        matchctx->match_locs[array_idx++] = eo;
        if (so >= 0 && eo >= 0 && (eo - so) > maxlen)
        {
          maxlen = (eo - so);
        }
      }
      matchctx->nmatches++;

         
                                                                         
                                                                        
                        
         
      if (fetching_unmatched && pmatch[0].rm_so >= 0 && (pmatch[0].rm_so - prev_valid_match_end) > maxlen)
      {
        maxlen = (pmatch[0].rm_so - prev_valid_match_end);
      }
      prev_valid_match_end = pmatch[0].rm_eo;
    }
    prev_match_end = pmatch[0].rm_eo;

                                           
    if (!re_flags->glob)
    {
      break;
    }

       
                                                                          
                                                                          
                                                                          
              
       
    start_search = prev_match_end;
    if (pmatch[0].rm_so == pmatch[0].rm_eo)
    {
      start_search++;
    }
    if (start_search > wide_len)
    {
      break;
    }
  }

     
                                                                            
                  
     
  if (fetching_unmatched && (wide_len - prev_valid_match_end) > maxlen)
  {
    maxlen = (wide_len - prev_valid_match_end);
  }

     
                                                                      
                     
     
  matchctx->match_locs[array_idx] = wide_len;

  if (eml > 1)
  {
    int64 maxsiz = eml * (int64)maxlen;
    int conv_bufsiz;

       
                                                                    
                 
       
                                                                          
                                                                         
                                                                          
                                                                         
                            
       
    if (maxsiz > orig_len)
    {
      conv_bufsiz = orig_len + 1;
    }
    else
    {
      conv_bufsiz = maxsiz + 1;                               
    }

    matchctx->conv_buf = palloc(conv_bufsiz);
    matchctx->conv_bufsiz = conv_bufsiz;
    matchctx->wide_str = wide_str;
  }
  else
  {
                                                                            
    pfree(wide_str);
    matchctx->wide_str = NULL;
    matchctx->conv_buf = NULL;
    matchctx->conv_bufsiz = 0;
  }

                             
  pfree(pmatch);

  return matchctx;
}

   
                                                                    
   
static ArrayType *
build_regexp_match_result(regexp_matches_ctx *matchctx)
{
  char *buf = matchctx->conv_buf;
  int bufsiz PG_USED_FOR_ASSERTS_ONLY = matchctx->conv_bufsiz;
  Datum *elems = matchctx->elems;
  bool *nulls = matchctx->nulls;
  int dims[1];
  int lbs[1];
  int loc;
  int i;

                                                            
  loc = matchctx->next_match * matchctx->npatterns * 2;
  for (i = 0; i < matchctx->npatterns; i++)
  {
    int so = matchctx->match_locs[loc++];
    int eo = matchctx->match_locs[loc++];

    if (so < 0 || eo < 0)
    {
      elems[i] = (Datum)0;
      nulls[i] = true;
    }
    else if (buf)
    {
      int len = pg_wchar2mb_with_len(matchctx->wide_str + so, buf, eo - so);

      Assert(len < bufsiz);
      elems[i] = PointerGetDatum(cstring_to_text_with_len(buf, len));
      nulls[i] = false;
    }
    else
    {
      elems[i] = DirectFunctionCall3(text_substr, PointerGetDatum(matchctx->orig_str), Int32GetDatum(so + 1), Int32GetDatum(eo - so));
      nulls[i] = false;
    }
  }

                         
  dims[0] = matchctx->npatterns;
  lbs[0] = 1;
                                                           
  return construct_md_array(elems, nulls, 1, dims, lbs, TEXTOID, -1, false, 'i');
}

   
                           
                                                              
                                     
   
Datum
regexp_split_to_table(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  regexp_matches_ctx *splitctx;

  if (SRF_IS_FIRSTCALL())
  {
    text *pattern = PG_GETARG_TEXT_PP(1);
    text *flags = PG_GETARG_TEXT_PP_IF_EXISTS(2);
    pg_re_flags re_flags;
    MemoryContext oldcontext;

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                           
    parse_re_flags(&re_flags, flags);
                                  
    if (re_flags.glob)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                                                    
                         errmsg("%s does not support the \"global\" option", "regexp_split_to_table()")));
    }
                                            
    re_flags.glob = true;

                                                                  
    splitctx = setup_regexp_matches(PG_GETARG_TEXT_P_COPY(0), pattern, &re_flags, PG_GET_COLLATION(), false, true, true);

    MemoryContextSwitchTo(oldcontext);
    funcctx->user_fctx = (void *)splitctx;
  }

  funcctx = SRF_PERCALL_SETUP();
  splitctx = (regexp_matches_ctx *)funcctx->user_fctx;

  if (splitctx->next_match <= splitctx->nmatches)
  {
    Datum result = build_regexp_split_result(splitctx);

    splitctx->next_match++;
    SRF_RETURN_NEXT(funcctx, result);
  }

  SRF_RETURN_DONE(funcctx);
}

                                                                              
Datum
regexp_split_to_table_no_flags(PG_FUNCTION_ARGS)
{
  return regexp_split_to_table(fcinfo);
}

   
                           
                                                              
                                      
   
Datum
regexp_split_to_array(PG_FUNCTION_ARGS)
{
  ArrayBuildState *astate = NULL;
  pg_re_flags re_flags;
  regexp_matches_ctx *splitctx;

                         
  parse_re_flags(&re_flags, PG_GETARG_TEXT_PP_IF_EXISTS(2));
                                
  if (re_flags.glob)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                                                                  
                       errmsg("%s does not support the \"global\" option", "regexp_split_to_array()")));
  }
                                          
  re_flags.glob = true;

  splitctx = setup_regexp_matches(PG_GETARG_TEXT_PP(0), PG_GETARG_TEXT_PP(1), &re_flags, PG_GET_COLLATION(), false, true, true);

  while (splitctx->next_match <= splitctx->nmatches)
  {
    astate = accumArrayResult(astate, build_regexp_split_result(splitctx), false, TEXTOID, CurrentMemoryContext);
    splitctx->next_match++;
  }

  PG_RETURN_ARRAYTYPE_P(makeArrayResult(astate, CurrentMemoryContext));
}

                                                                              
Datum
regexp_split_to_array_no_flags(PG_FUNCTION_ARGS)
{
  return regexp_split_to_array(fcinfo);
}

   
                                                                     
   
                                                                        
                                                                   
   
static Datum
build_regexp_split_result(regexp_matches_ctx *splitctx)
{
  char *buf = splitctx->conv_buf;
  int startpos;
  int endpos;

  if (splitctx->next_match > 0)
  {
    startpos = splitctx->match_locs[splitctx->next_match * 2 - 1];
  }
  else
  {
    startpos = 0;
  }
  if (startpos < 0)
  {
    elog(ERROR, "invalid match ending position");
  }

  if (buf)
  {
    int bufsiz PG_USED_FOR_ASSERTS_ONLY = splitctx->conv_bufsiz;
    int len;

    endpos = splitctx->match_locs[splitctx->next_match * 2];
    if (endpos < startpos)
    {
      elog(ERROR, "invalid match starting position");
    }
    len = pg_wchar2mb_with_len(splitctx->wide_str + startpos, buf, endpos - startpos);
    Assert(len < bufsiz);
    return PointerGetDatum(cstring_to_text_with_len(buf, len));
  }
  else
  {
    endpos = splitctx->match_locs[splitctx->next_match * 2];
    if (endpos < startpos)
    {
      elog(ERROR, "invalid match starting position");
    }
    return DirectFunctionCall3(text_substr, PointerGetDatum(splitctx->orig_str), Int32GetDatum(startpos + 1), Int32GetDatum(endpos - startpos));
  }
}

   
                                                                    
   
                                                                           
                                                                           
   
char *
regexp_fixed_prefix(text *text_re, bool case_insensitive, Oid collation, bool *exact)
{
  char *result;
  regex_t *re;
  int cflags;
  int re_result;
  pg_wchar *str;
  size_t slen;
  size_t maxlen;
  char errMsg[100];

  *exact = false;                     

                  
  cflags = REG_ADVANCED;
  if (case_insensitive)
  {
    cflags |= REG_ICASE;
  }

  re = RE_compile_and_cache(text_re, cflags, collation);

                                                   
  re_result = pg_regprefix(re, &str, &slen);

  switch (re_result)
  {
  case REG_NOMATCH:
    return NULL;

  case REG_PREFIX:
                                        
    break;

  case REG_EXACT:
    *exact = true;
                                        
    break;

  default:
                      
    CHECK_FOR_INTERRUPTS();
    pg_regerror(re_result, re, errMsg, sizeof(errMsg));
    ereport(ERROR, (errcode(ERRCODE_INVALID_REGULAR_EXPRESSION), errmsg("regular expression failed: %s", errMsg)));
    break;
  }

                                                         
  maxlen = pg_database_encoding_max_length() * slen + 1;
  result = (char *)palloc(maxlen);
  slen = pg_wchar2mb_with_len(str, result, slen);
  Assert(slen < maxlen);

  free(str);

  return result;
}
