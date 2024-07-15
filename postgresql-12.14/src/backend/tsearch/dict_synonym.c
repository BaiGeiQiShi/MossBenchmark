                                                                            
   
                  
                                                    
   
                                                                         
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "commands/defrem.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

typedef struct
{
  char *in;
  char *out;
  int outlen;
  uint16 flags;
} Syn;

typedef struct
{
  int len;                          
  Syn *syn;
  bool case_sensitive;
} DictSyn;

   
                                                                    
                                                                       
                                                                    
                                                                 
                                   
   
static char *
findwrd(char *in, char **end, uint16 *flags)
{
  char *start;
  char *lastchar;

                           
  while (*in && t_isspace(in))
  {
    in += pg_mblen(in);
  }

                                  
  if (*in == '\0')
  {
    *end = NULL;
    return NULL;
  }

  lastchar = start = in;

                        
  while (*in && !t_isspace(in))
  {
    lastchar = in;
    in += pg_mblen(in);
  }

  if (in - lastchar == 1 && t_iseq(lastchar, '*') && flags)
  {
    *flags = TSL_PREFIX;
    *end = lastchar;
  }
  else
  {
    if (flags)
    {
      *flags = 0;
    }
    *end = in;
  }

  return start;
}

static int
compareSyn(const void *a, const void *b)
{
  return strcmp(((const Syn *)a)->in, ((const Syn *)b)->in);
}

Datum
dsynonym_init(PG_FUNCTION_ARGS)
{
  List *dictoptions = (List *)PG_GETARG_POINTER(0);
  DictSyn *d;
  ListCell *l;
  char *filename = NULL;
  bool case_sensitive = false;
  tsearch_readline_state trst;
  char *starti, *starto, *end = NULL;
  int cur = 0;
  char *line = NULL;
  uint16 flags = 0;

  foreach (l, dictoptions)
  {
    DefElem *defel = (DefElem *)lfirst(l);

    if (strcmp(defel->defname, "synonyms") == 0)
    {
      filename = defGetString(defel);
    }
    else if (strcmp(defel->defname, "casesensitive") == 0)
    {
      case_sensitive = defGetBoolean(defel);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized synonym parameter: \"%s\"", defel->defname)));
    }
  }

  if (!filename)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("missing Synonyms parameter")));
  }

  filename = get_tsearch_config_filename(filename, "syn");

  if (!tsearch_readline_begin(&trst, filename))
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not open synonym file \"%s\": %m", filename)));
  }

  d = (DictSyn *)palloc0(sizeof(DictSyn));

  while ((line = tsearch_readline(&trst)) != NULL)
  {
    starti = findwrd(line, &end, NULL);
    if (!starti)
    {
                      
      goto skipline;
    }
    if (*end == '\0')
    {
                                                       
      goto skipline;
    }
    *end = '\0';

    starto = findwrd(end + 1, &end, &flags);
    if (!starto)
    {
                                                                     
      goto skipline;
    }
    *end = '\0';

       
                                                                          
                                                                   
       

    if (cur >= d->len)
    {
      if (d->len == 0)
      {
        d->len = 64;
        d->syn = (Syn *)palloc(sizeof(Syn) * d->len);
      }
      else
      {
        d->len *= 2;
        d->syn = (Syn *)repalloc(d->syn, sizeof(Syn) * d->len);
      }
    }

    if (case_sensitive)
    {
      d->syn[cur].in = pstrdup(starti);
      d->syn[cur].out = pstrdup(starto);
    }
    else
    {
      d->syn[cur].in = lowerstr(starti);
      d->syn[cur].out = lowerstr(starto);
    }

    d->syn[cur].outlen = strlen(starto);
    d->syn[cur].flags = flags;

    cur++;

  skipline:
    pfree(line);
  }

  tsearch_readline_end(&trst);

  d->len = cur;
  qsort(d->syn, d->len, sizeof(Syn), compareSyn);

  d->case_sensitive = case_sensitive;

  PG_RETURN_POINTER(d);
}

Datum
dsynonym_lexize(PG_FUNCTION_ARGS)
{
  DictSyn *d = (DictSyn *)PG_GETARG_POINTER(0);
  char *in = (char *)PG_GETARG_POINTER(1);
  int32 len = PG_GETARG_INT32(2);
  Syn key, *found;
  TSLexeme *res;

                                                                          
  if (len <= 0 || d->len <= 0)
  {
    PG_RETURN_POINTER(NULL);
  }

  if (d->case_sensitive)
  {
    key.in = pnstrdup(in, len);
  }
  else
  {
    key.in = lowerstr_with_len(in, len);
  }

  key.out = NULL;

  found = (Syn *)bsearch(&key, d->syn, d->len, sizeof(Syn), compareSyn);
  pfree(key.in);

  if (!found)
  {
    PG_RETURN_POINTER(NULL);
  }

  res = palloc0(sizeof(TSLexeme) * 2);
  res[0].lexeme = pnstrdup(found->out, found->outlen);
  res[0].flags = found->flags;

  PG_RETURN_POINTER(res);
}
