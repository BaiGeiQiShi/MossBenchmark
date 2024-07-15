                                                                            
   
           
                                 
   
                                                                         
   
                     
                     
   
                                                                        
                                                                            
   
                                                                             
                                                                                
                                                                             
                                                                                
   
                                                                         
                                                                             
                              
   
                               
                               
   
                                                                               
                                                   
                                                                    
                            
                                                                     
                                                                          
                                                                           
                                                                         
                 
                                                                          
                                                                           
                                                                            
              
                      
                                                                           
                          
                                                                                
                                                 
                                                 
   
                     
                     
   
                                                                              
                                                                           
                                                                    
                    
   
                                                                              
                               
   
                  
                                 
   
                                                                            
   

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "miscadmin.h"
#include "tsearch/dicts/spell.h"
#include "tsearch/ts_locale.h"
#include "utils/memutils.h"

   
                                                             
                                                             
                                                                    
                                                                   
                                  
   
#define tmpalloc(sz) MemoryContextAlloc(Conf->buildCxt, (sz))
#define tmpalloc0(sz) MemoryContextAllocZero(Conf->buildCxt, (sz))

   
                                                  
   
                                                                 
   
void
NIStartBuild(IspellDict *Conf)
{
     
                                                                           
                                     
     
  Conf->buildCxt = AllocSetContextCreate(CurTransactionContext, "Ispell dictionary init context", ALLOCSET_DEFAULT_SIZES);
}

   
                                                      
   
void
NIFinishBuild(IspellDict *Conf)
{
                                            
  MemoryContextDelete(Conf->buildCxt);
                                                            
  Conf->buildCxt = NULL;
  Conf->Spell = NULL;
  Conf->firstfree = NULL;
  Conf->CompoundAffixFlags = NULL;
}

   
                                                             
   
                                                                             
                                                                         
                                                                                
   
                                                                             
                                                                              
                                                           
   
#define COMPACT_ALLOC_CHUNK 8192                                        
#define COMPACT_MAX_REQ 1024                                        

static void *
compact_palloc0(IspellDict *Conf, size_t size)
{
  void *result;

                                         
  Assert(Conf->buildCxt != NULL);

                                         
  if (size > COMPACT_MAX_REQ)
  {
    return palloc0(size);
  }

                                  
  size = MAXALIGN(size);

                        
  if (size > Conf->avail)
  {
    Conf->firstfree = palloc0(COMPACT_ALLOC_CHUNK);
    Conf->avail = COMPACT_ALLOC_CHUNK;
  }

  result = (void *)Conf->firstfree;
  Conf->firstfree += size;
  Conf->avail -= size;

  return result;
}

#define cpalloc(size) compact_palloc0(Conf, size)
#define cpalloc0(size) compact_palloc0(Conf, size)

static char *
cpstrdup(IspellDict *Conf, const char *str)
{
  char *res = cpalloc(strlen(str) + 1);

  strcpy(res, str);
  return res;
}

   
                                                                     
   
static char *
lowerstr_ctx(IspellDict *Conf, const char *src)
{
  MemoryContext saveCtx;
  char *dst;

  saveCtx = MemoryContextSwitchTo(Conf->buildCxt);
  dst = lowerstr(src);
  MemoryContextSwitchTo(saveCtx);

  return dst;
}

#define MAX_NORM 1024
#define MAXNORMLEN 256

#define STRNCMP(s, p) strncmp((s), (p), strlen(p))
#define GETWCHAR(W, L, N, T) (((const uint8 *)(W))[((T) == FF_PREFIX) ? (N) : ((L)-1 - (N))])
#define GETCHAR(A, N, T) GETWCHAR((A)->repl, (A)->replen, N, T)

static char *VoidString = "";

static int
cmpspell(const void *s1, const void *s2)
{
  return strcmp((*(SPELL *const *)s1)->word, (*(SPELL *const *)s2)->word);
}

static int
cmpspellaffix(const void *s1, const void *s2)
{
  return strcmp((*(SPELL *const *)s1)->p.flag, (*(SPELL *const *)s2)->p.flag);
}

static int
cmpcmdflag(const void *f1, const void *f2)
{
  CompoundAffixFlag *fv1 = (CompoundAffixFlag *)f1, *fv2 = (CompoundAffixFlag *)f2;

  Assert(fv1->flagMode == fv2->flagMode);

  if (fv1->flagMode == FM_NUM)
  {
    if (fv1->flag.i == fv2->flag.i)
    {
      return 0;
    }

    return (fv1->flag.i > fv2->flag.i) ? 1 : -1;
  }

  return strcmp(fv1->flag.s, fv2->flag.s);
}

static char *
findchar(char *str, int c)
{
  while (*str)
  {
    if (t_iseq(str, c))
    {
      return str;
    }
    str += pg_mblen(str);
  }

  return NULL;
}

static char *
findchar2(char *str, int c1, int c2)
{
  while (*str)
  {
    if (t_iseq(str, c1) || t_iseq(str, c2))
    {
      return str;
    }
    str += pg_mblen(str);
  }

  return NULL;
}

                                                        
static int
strbcmp(const unsigned char *s1, const unsigned char *s2)
{
  int l1 = strlen((const char *)s1) - 1, l2 = strlen((const char *)s2) - 1;

  while (l1 >= 0 && l2 >= 0)
  {
    if (s1[l1] < s2[l2])
    {
      return -1;
    }
    if (s1[l1] > s2[l2])
    {
      return 1;
    }
    l1--;
    l2--;
  }
  if (l1 < l2)
  {
    return -1;
  }
  if (l1 > l2)
  {
    return 1;
  }

  return 0;
}

static int
strbncmp(const unsigned char *s1, const unsigned char *s2, size_t count)
{
  int l1 = strlen((const char *)s1) - 1, l2 = strlen((const char *)s2) - 1, l = count;

  while (l1 >= 0 && l2 >= 0 && l > 0)
  {
    if (s1[l1] < s2[l2])
    {
      return -1;
    }
    if (s1[l1] > s2[l2])
    {
      return 1;
    }
    l1--;
    l2--;
    l--;
  }
  if (l == 0)
  {
    return 0;
  }
  if (l1 < l2)
  {
    return -1;
  }
  if (l1 > l2)
  {
    return 1;
  }
  return 0;
}

   
                     
                                                                           
                                                        
   
static int
cmpaffix(const void *s1, const void *s2)
{
  const AFFIX *a1 = (const AFFIX *)s1;
  const AFFIX *a2 = (const AFFIX *)s2;

  if (a1->type < a2->type)
  {
    return -1;
  }
  if (a1->type > a2->type)
  {
    return 1;
  }
  if (a1->type == FF_PREFIX)
  {
    return strcmp(a1->repl, a2->repl);
  }
  else
  {
    return strbcmp((const unsigned char *)a1->repl, (const unsigned char *)a2->repl);
  }
}

   
                                                              
   
                                                                                
                                                        
                                                         
                                       
   
                                                                            
                   
                                        
                     
                                        
                        
                                          
   
                             
                                                                                
                  
                                               
   
static void
getNextFlagFromString(IspellDict *Conf, char **sflagset, char *sflag)
{
  int32 s;
  char *next, *sbuf = *sflagset;
  int maxstep;
  bool stop = false;
  bool met_comma = false;

  maxstep = (Conf->flagMode == FM_LONG) ? 2 : 1;

  while (**sflagset)
  {
    switch (Conf->flagMode)
    {
    case FM_LONG:
    case FM_CHAR:
      COPYCHAR(sflag, *sflagset);
      sflag += pg_mblen(*sflagset);

                                        
      *sflagset += pg_mblen(*sflagset);

                                                  
      maxstep--;
      stop = (maxstep == 0);
      break;
    case FM_NUM:
      s = strtol(*sflagset, &next, 10);
      if (*sflagset == next || errno == ERANGE)
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix flag \"%s\"", *sflagset)));
      }
      if (s < 0 || s > FLAGNUM_MAXSIZE)
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("affix flag \"%s\" is out of range", *sflagset)));
      }
      sflag += sprintf(sflag, "%0d", s);

                                        
      *sflagset = next;
      while (**sflagset)
      {
        if (t_isdigit(*sflagset))
        {
          if (!met_comma)
          {
            ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix flag \"%s\"", *sflagset)));
          }
          break;
        }
        else if (t_iseq(*sflagset, ','))
        {
          if (met_comma)
          {
            ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix flag \"%s\"", *sflagset)));
          }
          met_comma = true;
        }
        else if (!t_isspace(*sflagset))
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid character in affix flag \"%s\"", *sflagset)));
        }

        *sflagset += pg_mblen(*sflagset);
      }
      stop = true;
      break;
    default:
      elog(ERROR, "unrecognized type of Conf->flagMode: %d", Conf->flagMode);
    }

    if (stop)
    {
      break;
    }
  }

  if (Conf->flagMode == FM_LONG && maxstep > 0)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix flag \"%s\" with \"long\" flag value", sbuf)));
  }

  *sflag = '\0';
}

   
                                                                      
                                                                              
                               
   
                             
                                              
                              
   
                                                                         
                            
   
static bool
IsAffixFlagInUse(IspellDict *Conf, int affix, const char *affixflag)
{
  char *flagcur;
  char flag[BUFSIZ];

  if (*affixflag == 0)
  {
    return true;
  }

  Assert(affix < Conf->nAffixData);

  flagcur = Conf->AffixData[affix];

  while (*flagcur)
  {
    getNextFlagFromString(Conf, &flagcur, flag);
                                                            
    if (strcmp(flag, affixflag) == 0)
    {
      return true;
    }
  }

                                
  return false;
}

   
                                                     
   
                             
                   
                                                                                
   
static void
NIAddSpell(IspellDict *Conf, const char *word, const char *flag)
{
  if (Conf->nspell >= Conf->mspell)
  {
    if (Conf->mspell)
    {
      Conf->mspell *= 2;
      Conf->Spell = (SPELL **)repalloc(Conf->Spell, Conf->mspell * sizeof(SPELL *));
    }
    else
    {
      Conf->mspell = 1024 * 20;
      Conf->Spell = (SPELL **)tmpalloc(Conf->mspell * sizeof(SPELL *));
    }
  }
  Conf->Spell[Conf->nspell] = (SPELL *)tmpalloc(SPELLHDRSZ + strlen(word) + 1);
  strcpy(Conf->Spell[Conf->nspell]->word, word);
  Conf->Spell[Conf->nspell]->p.flag = (*flag != '\0') ? cpstrdup(Conf, flag) : VoidString;
  Conf->nspell++;
}

   
                                                      
   
                                                                      
   
                             
                                     
   
void
NIImportDictionary(IspellDict *Conf, const char *filename)
{
  tsearch_readline_state trst;
  char *line;

  if (!tsearch_readline_begin(&trst, filename))
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not open dictionary file \"%s\": %m", filename)));
  }

  while ((line = tsearch_readline(&trst)) != NULL)
  {
    char *s, *pstr;

                            
    const char *flag;

                                    
    flag = NULL;
    if ((s = findchar(line, '/')))
    {
      *s++ = '\0';
      flag = s;
      while (*s)
      {
                                                                 
        if (pg_mblen(s) == 1 && t_isprint(s) && !t_isspace(s))
        {
          s++;
        }
        else
        {
          *s = '\0';
          break;
        }
      }
    }
    else
    {
      flag = "";
    }

                                
    s = line;
    while (*s)
    {
      if (t_isspace(s))
      {
        *s = '\0';
        break;
      }
      s += pg_mblen(s);
    }
    pstr = lowerstr_ctx(Conf, line);

    NIAddSpell(Conf, pstr, flag);
    pfree(pstr);

    pfree(line);
  }
  tsearch_readline_end(&trst);
}

   
                                                                             
                                                                          
                         
   
                                                     
             
   
                                   
                             
                          
   
                                   
                           
                      
   
                             
                             
                                                                       
                                                                      
   
                                                                       
   
static int
FindWord(IspellDict *Conf, const char *word, const char *affixflag, int flag)
{
  SPNode *node = Conf->Dictionary;
  SPNodeData *StopLow, *StopHigh, *StopMiddle;
  const uint8 *ptr = (const uint8 *)word;

  flag &= FF_COMPOUNDFLAGMASK;

  while (node && *ptr)
  {
    StopLow = node->data;
    StopHigh = node->data + node->length;
    while (StopLow < StopHigh)
    {
      StopMiddle = StopLow + ((StopHigh - StopLow) >> 1);
      if (StopMiddle->val == *ptr)
      {
        if (*(ptr + 1) == '\0' && StopMiddle->isword)
        {
          if (flag == 0)
          {
               
                                                                  
                                                                 
                                      
               
            if (StopMiddle->compoundflag & FF_COMPOUNDONLY)
            {
              return 0;
            }
          }
          else if ((flag & StopMiddle->compoundflag) == 0)
          {
            return 0;
          }

             
                                                                    
                                           
             
          if (IsAffixFlagInUse(Conf, StopMiddle->affix, affixflag))
          {
            return 1;
          }
        }
        node = StopMiddle->node;
        ptr++;
        break;
      }
      else if (StopMiddle->val < *ptr)
      {
        StopLow = StopMiddle + 1;
      }
      else
      {
        StopHigh = StopMiddle;
      }
    }
    if (StopLow >= StopHigh)
    {
      break;
    }
  }
  return 0;
}

   
                                                                           
   
static void
regex_affix_deletion_callback(void *arg)
{
  aff_regex_struct *pregex = (aff_regex_struct *)arg;

  pg_regfree(&(pregex->regex));
}

   
                                             
   
                             
                                                
                                                                                
                                                                 
   
                                                        
                          
   
                                                             
                                                                            
                                                                      
                           
                                                                   
                                 
   
static void
NIAddAffix(IspellDict *Conf, const char *flag, char flagflags, const char *mask, const char *find, const char *repl, int type)
{
  AFFIX *Affix;

  if (Conf->naffixes >= Conf->maffixes)
  {
    if (Conf->maffixes)
    {
      Conf->maffixes *= 2;
      Conf->Affix = (AFFIX *)repalloc((void *)Conf->Affix, Conf->maffixes * sizeof(AFFIX));
    }
    else
    {
      Conf->maffixes = 16;
      Conf->Affix = (AFFIX *)palloc(Conf->maffixes * sizeof(AFFIX));
    }
  }

  Affix = Conf->Affix + Conf->naffixes;

                                                                
  if (strcmp(mask, ".") == 0 || *mask == '\0')
  {
    Affix->issimple = 1;
    Affix->isregis = 0;
  }
                                                            
  else if (RS_isRegis(mask))
  {
    Affix->issimple = 0;
    Affix->isregis = 1;
    RS_compile(&(Affix->reg.regis), (type == FF_SUFFIX), *mask ? mask : VoidString);
  }
                                                              
  else
  {
    int masklen;
    int wmasklen;
    int err;
    pg_wchar *wmask;
    char *tmask;
    aff_regex_struct *pregex;

    Affix->issimple = 0;
    Affix->isregis = 0;
    tmask = (char *)tmpalloc(strlen(mask) + 3);
    if (type == FF_SUFFIX)
    {
      sprintf(tmask, "%s$", mask);
    }
    else
    {
      sprintf(tmask, "^%s", mask);
    }

    masklen = strlen(tmask);
    wmask = (pg_wchar *)tmpalloc((masklen + 1) * sizeof(pg_wchar));
    wmasklen = pg_mb2wchar_with_len(tmask, wmask, masklen);

       
                                                                        
                                                                           
                                                                          
                                                                           
                                                                  
       
    Affix->reg.pregex = pregex = palloc(sizeof(aff_regex_struct));

    err = pg_regcomp(&(pregex->regex), wmask, wmasklen, REG_ADVANCED | REG_NOSUB, DEFAULT_COLLATION_OID);
    if (err)
    {
      char errstr[100];

      pg_regerror(err, &(pregex->regex), errstr, sizeof(errstr));
      ereport(ERROR, (errcode(ERRCODE_INVALID_REGULAR_EXPRESSION), errmsg("invalid regular expression: %s", errstr)));
    }

    pregex->mcallback.func = regex_affix_deletion_callback;
    pregex->mcallback.arg = (void *)pregex;
    MemoryContextRegisterResetCallback(CurrentMemoryContext, &pregex->mcallback);
  }

  Affix->flagflags = flagflags;
  if ((Affix->flagflags & FF_COMPOUNDONLY) || (Affix->flagflags & FF_COMPOUNDPERMITFLAG))
  {
    if ((Affix->flagflags & FF_COMPOUNDFLAG) == 0)
    {
      Affix->flagflags |= FF_COMPOUNDFLAG;
    }
  }
  Affix->flag = cpstrdup(Conf, flag);
  Affix->type = type;

  Affix->find = (find && *find) ? cpstrdup(Conf, find) : VoidString;
  if ((Affix->replen = strlen(repl)) > 0)
  {
    Affix->repl = cpstrdup(Conf, repl);
  }
  else
  {
    Affix->repl = VoidString;
  }
  Conf->naffixes++;
}

                                                     
#define PAE_WAIT_MASK 0
#define PAE_INMASK 1
#define PAE_WAIT_FIND 2
#define PAE_INFIND 3
#define PAE_WAIT_REPL 4
#define PAE_INREPL 5
#define PAE_WAIT_TYPE 6
#define PAE_WAIT_FLAG 7

   
                                                            
   
                                                           
                                                                   
   
                                                                              
   
                                                   
   
static bool
get_nextfield(char **str, char *next)
{
  int state = PAE_WAIT_MASK;
  int avail = BUFSIZ;

  while (**str)
  {
    if (state == PAE_WAIT_MASK)
    {
      if (t_iseq(*str, '#'))
      {
        return false;
      }
      else if (!t_isspace(*str))
      {
        int clen = pg_mblen(*str);

        if (clen < avail)
        {
          COPYCHAR(next, *str);
          next += clen;
          avail -= clen;
        }
        state = PAE_INMASK;
      }
    }
    else                          
    {
      if (t_isspace(*str))
      {
        *next = '\0';
        return true;
      }
      else
      {
        int clen = pg_mblen(*str);

        if (clen < avail)
        {
          COPYCHAR(next, *str);
          next += clen;
          avail -= clen;
        }
      }
    }
    *str += pg_mblen(*str);
  }

  *next = '\0';

  return (state == PAE_INMASK);                                    
}

   
                                                                 
   
                                                  
            
                                               
                          
                                             
   
                         
                                                                                
   
                                                                                
   
static int
parse_ooaffentry(char *str, char *type, char *flag, char *find, char *repl, char *mask)
{
  int state = PAE_WAIT_TYPE;
  int fields_read = 0;
  bool valid = false;

  *type = *flag = *find = *repl = *mask = '\0';

  while (*str)
  {
    switch (state)
    {
    case PAE_WAIT_TYPE:
      valid = get_nextfield(&str, type);
      state = PAE_WAIT_FLAG;
      break;
    case PAE_WAIT_FLAG:
      valid = get_nextfield(&str, flag);
      state = PAE_WAIT_FIND;
      break;
    case PAE_WAIT_FIND:
      valid = get_nextfield(&str, find);
      state = PAE_WAIT_REPL;
      break;
    case PAE_WAIT_REPL:
      valid = get_nextfield(&str, repl);
      state = PAE_WAIT_MASK;
      break;
    case PAE_WAIT_MASK:
      valid = get_nextfield(&str, mask);
      state = -1;                      
      break;
    default:
      elog(ERROR, "unrecognized state in parse_ooaffentry: %d", state);
      break;
    }
    if (valid)
    {
      fields_read++;
    }
    else
    {
      break;                
    }
    if (state < 0)
    {
      break;                     
    }
  }

  return fields_read;
}

   
                                                   
   
                                                  
                                  
   
static bool
parse_affentry(char *str, char *mask, char *find, char *repl)
{
  int state = PAE_WAIT_MASK;
  char *pmask = mask, *pfind = find, *prepl = repl;

  *mask = *find = *repl = '\0';

  while (*str)
  {
    if (state == PAE_WAIT_MASK)
    {
      if (t_iseq(str, '#'))
      {
        return false;
      }
      else if (!t_isspace(str))
      {
        COPYCHAR(pmask, str);
        pmask += pg_mblen(str);
        state = PAE_INMASK;
      }
    }
    else if (state == PAE_INMASK)
    {
      if (t_iseq(str, '>'))
      {
        *pmask = '\0';
        state = PAE_WAIT_FIND;
      }
      else if (!t_isspace(str))
      {
        COPYCHAR(pmask, str);
        pmask += pg_mblen(str);
      }
    }
    else if (state == PAE_WAIT_FIND)
    {
      if (t_iseq(str, '-'))
      {
        state = PAE_INFIND;
      }
      else if (t_isalpha(str) || t_iseq(str, '\'')                 )
      {
        COPYCHAR(prepl, str);
        prepl += pg_mblen(str);
        state = PAE_INREPL;
      }
      else if (!t_isspace(str))
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("syntax error")));
      }
    }
    else if (state == PAE_INFIND)
    {
      if (t_iseq(str, ','))
      {
        *pfind = '\0';
        state = PAE_WAIT_REPL;
      }
      else if (t_isalpha(str))
      {
        COPYCHAR(pfind, str);
        pfind += pg_mblen(str);
      }
      else if (!t_isspace(str))
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("syntax error")));
      }
    }
    else if (state == PAE_WAIT_REPL)
    {
      if (t_iseq(str, '-'))
      {
        break;                
      }
      else if (t_isalpha(str))
      {
        COPYCHAR(prepl, str);
        prepl += pg_mblen(str);
        state = PAE_INREPL;
      }
      else if (!t_isspace(str))
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("syntax error")));
      }
    }
    else if (state == PAE_INREPL)
    {
      if (t_iseq(str, '#'))
      {
        *prepl = '\0';
        break;
      }
      else if (t_isalpha(str))
      {
        COPYCHAR(prepl, str);
        prepl += pg_mblen(str);
      }
      else if (!t_isspace(str))
      {
        ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("syntax error")));
      }
    }
    else
    {
      elog(ERROR, "unrecognized state in parse_affentry: %d", state);
    }

    str += pg_mblen(str);
  }

  *pmask = *pfind = *prepl = '\0';

  return (*mask && (*find || *repl));
}

   
                                                   
   
static void
setCompoundAffixFlagValue(IspellDict *Conf, CompoundAffixFlag *entry, char *s, uint32 val)
{
  if (Conf->flagMode == FM_NUM)
  {
    char *next;
    int i;

    i = strtol(s, &next, 10);
    if (s == next || errno == ERANGE)
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix flag \"%s\"", s)));
    }
    if (i < 0 || i > FLAGNUM_MAXSIZE)
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("affix flag \"%s\" is out of range", s)));
    }

    entry->flag.i = i;
  }
  else
  {
    entry->flag.s = cpstrdup(Conf, s);
  }

  entry->flagMode = Conf->flagMode;
  entry->value = val;
}

   
                                                                         
   
                             
                            
                         
   
static void
addCompoundAffixFlagValue(IspellDict *Conf, char *s, uint32 val)
{
  CompoundAffixFlag *newValue;
  char sbuf[BUFSIZ];
  char *sflag;
  int clen;

  while (*s && t_isspace(s))
  {
    s += pg_mblen(s);
  }

  if (!*s)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("syntax error")));
  }

                           
  sflag = sbuf;
  while (*s && !t_isspace(s) && *s != '\n')
  {
    clen = pg_mblen(s);
    COPYCHAR(sflag, s);
    sflag += clen;
    s += clen;
  }
  *sflag = '\0';

                                                                   
  if (Conf->nCompoundAffixFlag >= Conf->mCompoundAffixFlag)
  {
    if (Conf->mCompoundAffixFlag)
    {
      Conf->mCompoundAffixFlag *= 2;
      Conf->CompoundAffixFlags = (CompoundAffixFlag *)repalloc((void *)Conf->CompoundAffixFlags, Conf->mCompoundAffixFlag * sizeof(CompoundAffixFlag));
    }
    else
    {
      Conf->mCompoundAffixFlag = 10;
      Conf->CompoundAffixFlags = (CompoundAffixFlag *)tmpalloc(Conf->mCompoundAffixFlag * sizeof(CompoundAffixFlag));
    }
  }

  newValue = Conf->CompoundAffixFlags + Conf->nCompoundAffixFlag;

  setCompoundAffixFlagValue(Conf, newValue, sbuf, val);

  Conf->usecompound = true;
  Conf->nCompoundAffixFlag++;
}

   
                                                                              
            
   
static int
getCompoundAffixFlagValue(IspellDict *Conf, char *s)
{
  uint32 flag = 0;
  CompoundAffixFlag *found, key;
  char sflag[BUFSIZ];
  char *flagcur;

  if (Conf->nCompoundAffixFlag == 0)
  {
    return 0;
  }

  flagcur = s;
  while (*flagcur)
  {
    getNextFlagFromString(Conf, &flagcur, sflag);
    setCompoundAffixFlagValue(Conf, &key, sflag, 0);

    found = (CompoundAffixFlag *)bsearch(&key, (void *)Conf->CompoundAffixFlags, Conf->nCompoundAffixFlag, sizeof(CompoundAffixFlag), cmpcmdflag);
    if (found != NULL)
    {
      flag |= found->value;
    }
  }

  return flag;
}

   
                                             
   
                                                                        
                                                         
                                          
   
static char *
getAffixFlagSet(IspellDict *Conf, char *s)
{
  if (Conf->useFlagAliases && *s != '\0')
  {
    int curaffix;
    char *end;

    curaffix = strtol(s, &end, 10);
    if (s == end || errno == ERANGE)
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix alias \"%s\"", s)));
    }

    if (curaffix > 0 && curaffix < Conf->nAffixData)
    {

         
                                                                        
                              
         
      return Conf->AffixData[curaffix];
    }
    else if (curaffix > Conf->nAffixData)
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix alias \"%s\"", s)));
    }
    return VoidString;
  }
  else
  {
    return s;
  }
}

   
                                                                 
   
                             
                                      
   
static void
NIImportOOAffixes(IspellDict *Conf, const char *filename)
{
  char type[BUFSIZ], *ptype = NULL;
  char sflag[BUFSIZ];
  char mask[BUFSIZ], *pmask;
  char find[BUFSIZ], *pfind;
  char repl[BUFSIZ], *prepl;
  bool isSuffix = false;
  int naffix = 0, curaffix = 0;
  int sflaglen = 0;
  char flagflags = 0;
  tsearch_readline_state trst;
  char *recoded;

                                  
  Conf->usecompound = false;
  Conf->useFlagAliases = false;
  Conf->flagMode = FM_CHAR;

  if (!tsearch_readline_begin(&trst, filename))
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not open affix file \"%s\": %m", filename)));
  }

  while ((recoded = tsearch_readline(&trst)) != NULL)
  {
    if (*recoded == '\0' || t_isspace(recoded) || t_iseq(recoded, '#'))
    {
      pfree(recoded);
      continue;
    }

    if (STRNCMP(recoded, "COMPOUNDFLAG") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDFLAG"), FF_COMPOUNDFLAG);
    }
    else if (STRNCMP(recoded, "COMPOUNDBEGIN") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDBEGIN"), FF_COMPOUNDBEGIN);
    }
    else if (STRNCMP(recoded, "COMPOUNDLAST") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDLAST"), FF_COMPOUNDLAST);
    }
                                                   
    else if (STRNCMP(recoded, "COMPOUNDEND") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDEND"), FF_COMPOUNDLAST);
    }
    else if (STRNCMP(recoded, "COMPOUNDMIDDLE") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDMIDDLE"), FF_COMPOUNDMIDDLE);
    }
    else if (STRNCMP(recoded, "ONLYINCOMPOUND") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("ONLYINCOMPOUND"), FF_COMPOUNDONLY);
    }
    else if (STRNCMP(recoded, "COMPOUNDPERMITFLAG") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDPERMITFLAG"), FF_COMPOUNDPERMITFLAG);
    }
    else if (STRNCMP(recoded, "COMPOUNDFORBIDFLAG") == 0)
    {
      addCompoundAffixFlagValue(Conf, recoded + strlen("COMPOUNDFORBIDFLAG"), FF_COMPOUNDFORBIDFLAG);
    }
    else if (STRNCMP(recoded, "FLAG") == 0)
    {
      char *s = recoded + strlen("FLAG");

      while (*s && t_isspace(s))
      {
        s += pg_mblen(s);
      }

      if (*s)
      {
        if (STRNCMP(s, "long") == 0)
        {
          Conf->flagMode = FM_LONG;
        }
        else if (STRNCMP(s, "num") == 0)
        {
          Conf->flagMode = FM_NUM;
        }
        else if (STRNCMP(s, "default") != 0)
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("Ispell dictionary supports only "
                                                                     "\"default\", \"long\", "
                                                                     "and \"num\" flag values")));
        }
      }
    }

    pfree(recoded);
  }
  tsearch_readline_end(&trst);

  if (Conf->nCompoundAffixFlag > 1)
  {
    qsort((void *)Conf->CompoundAffixFlags, Conf->nCompoundAffixFlag, sizeof(CompoundAffixFlag), cmpcmdflag);
  }

  if (!tsearch_readline_begin(&trst, filename))
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not open affix file \"%s\": %m", filename)));
  }

  while ((recoded = tsearch_readline(&trst)) != NULL)
  {
    int fields_read;

    if (*recoded == '\0' || t_isspace(recoded) || t_iseq(recoded, '#'))
    {
      goto nextline;
    }

    fields_read = parse_ooaffentry(recoded, type, sflag, find, repl, mask);

    if (ptype)
    {
      pfree(ptype);
    }
    ptype = lowerstr_ctx(Conf, type);

                                                             
    if (STRNCMP(ptype, "af") == 0)
    {
                                               
      if (!Conf->useFlagAliases)
      {
        Conf->useFlagAliases = true;
        naffix = atoi(sflag);
        if (naffix <= 0)
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid number of flag vector aliases")));
        }

                                                   
        naffix++;

        Conf->AffixData = (char **)palloc0(naffix * sizeof(char *));
        Conf->lenAffixData = Conf->nAffixData = naffix;

                                               
        Conf->AffixData[curaffix] = VoidString;
        curaffix++;
      }
                                   
      else
      {
        if (curaffix < naffix)
        {
          Conf->AffixData[curaffix] = cpstrdup(Conf, sflag);
          curaffix++;
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("number of aliases exceeds specified number %d", naffix - 1)));
        }
      }
      goto nextline;
    }
                                                 
    if (fields_read < 4 || (STRNCMP(ptype, "sfx") != 0 && STRNCMP(ptype, "pfx") != 0))
    {
      goto nextline;
    }

    sflaglen = strlen(sflag);
    if (sflaglen == 0 || (sflaglen > 1 && Conf->flagMode == FM_CHAR) || (sflaglen > 2 && Conf->flagMode == FM_LONG))
    {
      goto nextline;
    }

               
                                  
                 
               
       
    if (fields_read == 4)
    {
      isSuffix = (STRNCMP(ptype, "sfx") == 0);
      if (t_iseq(find, 'y') || t_iseq(find, 'Y'))
      {
        flagflags = FF_CROSSPRODUCT;
      }
      else
      {
        flagflags = 0;
      }
    }
               
                                  
                          
               
       
    else
    {
      char *ptr;
      int aflg = 0;

                                                          
      if ((ptr = strchr(repl, '/')) != NULL)
      {
        aflg |= getCompoundAffixFlagValue(Conf, getAffixFlagSet(Conf, ptr + 1));
      }
                                                       
      prepl = lowerstr_ctx(Conf, repl);
      if ((ptr = strchr(prepl, '/')) != NULL)
      {
        *ptr = '\0';
      }
      pfind = lowerstr_ctx(Conf, find);
      pmask = lowerstr_ctx(Conf, mask);
      if (t_iseq(find, '0'))
      {
        *pfind = '\0';
      }
      if (t_iseq(repl, '0'))
      {
        *prepl = '\0';
      }

      NIAddAffix(Conf, sflag, flagflags | aflg, pmask, pfind, prepl, isSuffix ? FF_SUFFIX : FF_PREFIX);
      pfree(prepl);
      pfree(pfind);
      pfree(pmask);
    }

  nextline:
    pfree(recoded);
  }

  tsearch_readline_end(&trst);
  if (ptype)
  {
    pfree(ptype);
  }
}

   
                  
   
                                                                     
   
                                                                               
                                                                             
                                                                   
   
void
NIImportAffixes(IspellDict *Conf, const char *filename)
{
  char *pstr = NULL;
  char flag[BUFSIZ];
  char mask[BUFSIZ];
  char find[BUFSIZ];
  char repl[BUFSIZ];
  char *s;
  bool suffixes = false;
  bool prefixes = false;
  char flagflags = 0;
  tsearch_readline_state trst;
  bool oldformat = false;
  char *recoded = NULL;

  if (!tsearch_readline_begin(&trst, filename))
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not open affix file \"%s\": %m", filename)));
  }

  Conf->usecompound = false;
  Conf->useFlagAliases = false;
  Conf->flagMode = FM_CHAR;

  while ((recoded = tsearch_readline(&trst)) != NULL)
  {
    pstr = lowerstr(recoded);

                                       
    if (*pstr == '#' || *pstr == '\n')
    {
      goto nextline;
    }

    if (STRNCMP(pstr, "compoundwords") == 0)
    {
                                                                 
      s = findchar2(recoded, 'l', 'L');
      if (s)
      {
        while (*s && !t_isspace(s))
        {
          s += pg_mblen(s);
        }
        while (*s && t_isspace(s))
        {
          s += pg_mblen(s);
        }

        if (*s && pg_mblen(s) == 1)
        {
          addCompoundAffixFlagValue(Conf, s, FF_COMPOUNDFLAG);
          Conf->usecompound = true;
        }
        oldformat = true;
        goto nextline;
      }
    }
    if (STRNCMP(pstr, "suffixes") == 0)
    {
      suffixes = true;
      prefixes = false;
      oldformat = true;
      goto nextline;
    }
    if (STRNCMP(pstr, "prefixes") == 0)
    {
      suffixes = false;
      prefixes = true;
      oldformat = true;
      goto nextline;
    }
    if (STRNCMP(pstr, "flag") == 0)
    {
      s = recoded + 4;                                    
      flagflags = 0;

      while (*s && t_isspace(s))
      {
        s += pg_mblen(s);
      }

      if (*s == '*')
      {
        flagflags |= FF_CROSSPRODUCT;
        s++;
      }
      else if (*s == '~')
      {
        flagflags |= FF_COMPOUNDONLY;
        s++;
      }

      if (*s == '\\')
      {
        s++;
      }

         
                                                                         
                                                                      
                                  
         
      if (*s && pg_mblen(s) == 1)
      {
        COPYCHAR(flag, s);
        flag[1] = '\0';

        s++;
        if (*s == '\0' || *s == '#' || *s == '\n' || *s == ':' || t_isspace(s))
        {
          oldformat = true;
          goto nextline;
        }
      }
      goto isnewformat;
    }
    if (STRNCMP(recoded, "COMPOUNDFLAG") == 0 || STRNCMP(recoded, "COMPOUNDMIN") == 0 || STRNCMP(recoded, "PFX") == 0 || STRNCMP(recoded, "SFX") == 0)
    {
      goto isnewformat;
    }

    if ((!suffixes) && (!prefixes))
    {
      goto nextline;
    }

    if (!parse_affentry(pstr, mask, find, repl))
    {
      goto nextline;
    }

    NIAddAffix(Conf, flag, flagflags, mask, find, repl, suffixes ? FF_SUFFIX : FF_PREFIX);

  nextline:
    pfree(recoded);
    pfree(pstr);
  }
  tsearch_readline_end(&trst);
  return;

isnewformat:
  if (oldformat)
  {
    ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("affix file contains both old-style and new-style commands")));
  }
  tsearch_readline_end(&trst);

  NIImportOOAffixes(Conf, filename);
}

   
                                                                   
                    
   
                                          
   
static int
MergeAffix(IspellDict *Conf, int a1, int a2)
{
  char **ptr;

  Assert(a1 < Conf->nAffixData && a2 < Conf->nAffixData);

                                                               
  if (*Conf->AffixData[a1] == '\0')
  {
    return a2;
  }
  else if (*Conf->AffixData[a2] == '\0')
  {
    return a1;
  }

  while (Conf->nAffixData + 1 >= Conf->lenAffixData)
  {
    Conf->lenAffixData *= 2;
    Conf->AffixData = (char **)repalloc(Conf->AffixData, sizeof(char *) * Conf->lenAffixData);
  }

  ptr = Conf->AffixData + Conf->nAffixData;
  if (Conf->flagMode == FM_NUM)
  {
    *ptr = cpalloc(strlen(Conf->AffixData[a1]) + strlen(Conf->AffixData[a2]) + 1             + 1 /* \0 */);
    sprintf(*ptr, "%s,%s", Conf->AffixData[a1], Conf->AffixData[a2]);
  }
  else
  {
    *ptr = cpalloc(strlen(Conf->AffixData[a1]) + strlen(Conf->AffixData[a2]) + 1         );
    sprintf(*ptr, "%s%s", Conf->AffixData[a1], Conf->AffixData[a2]);
  }
  ptr++;
  *ptr = NULL;
  Conf->nAffixData++;

  return Conf->nAffixData - 1;
}

   
                                                                              
                               
   
static uint32
makeCompoundFlags(IspellDict *Conf, int affix)
{
  Assert(affix < Conf->nAffixData);

  return (getCompoundAffixFlagValue(Conf, Conf->AffixData[affix]) & FF_COMPOUNDFLAGMASK);
}

   
                                            
   
                             
                                              
                                               
                                     
   
static SPNode *
mkSPNode(IspellDict *Conf, int low, int high, int level)
{
  int i;
  int nchar = 0;
  char lastchar = '\0';
  SPNode *rs;
  SPNodeData *data;
  int lownew = low;

  for (i = low; i < high; i++)
  {
    if (Conf->Spell[i]->p.d.len > level && lastchar != Conf->Spell[i]->word[level])
    {
      nchar++;
      lastchar = Conf->Spell[i]->word[level];
    }
  }

  if (!nchar)
  {
    return NULL;
  }

  rs = (SPNode *)cpalloc0(SPNHDRSZ + nchar * sizeof(SPNodeData));
  rs->length = nchar;
  data = rs->data;

  lastchar = '\0';
  for (i = low; i < high; i++)
  {
    if (Conf->Spell[i]->p.d.len > level)
    {
      if (lastchar != Conf->Spell[i]->word[level])
      {
        if (lastchar)
        {
                                             
          data->node = mkSPNode(Conf, lownew, i, level + 1);
          lownew = i;
          data++;
        }
        lastchar = Conf->Spell[i]->word[level];
      }
      data->val = ((uint8 *)(Conf->Spell[i]->word))[level];
      if (Conf->Spell[i]->p.d.len == level + 1)
      {
        bool clearCompoundOnly = false;

        if (data->isword && data->affix != Conf->Spell[i]->p.d.affix)
        {
             
                                                              
                                                                    
                                         
             

          clearCompoundOnly = (FF_COMPOUNDONLY & data->compoundflag & makeCompoundFlags(Conf, Conf->Spell[i]->p.d.affix)) ? false : true;
          data->affix = MergeAffix(Conf, data->affix, Conf->Spell[i]->p.d.affix);
        }
        else
        {
          data->affix = Conf->Spell[i]->p.d.affix;
        }
        data->isword = 1;

        data->compoundflag = makeCompoundFlags(Conf, data->affix);

        if ((data->compoundflag & FF_COMPOUNDONLY) && (data->compoundflag & FF_COMPOUNDFLAG) == 0)
        {
          data->compoundflag |= FF_COMPOUNDFLAG;
        }

        if (clearCompoundOnly)
        {
          data->compoundflag &= ~FF_COMPOUNDONLY;
        }
      }
    }
  }

                                     
  data->node = mkSPNode(Conf, lownew, high, level + 1);

  return rs;
}

   
                                                                               
                
   
void
NISortDictionary(IspellDict *Conf)
{
  int i;
  int naffix = 0;
  int curaffix;

                        

     
                                                                          
                              
     
  if (Conf->useFlagAliases)
  {
    for (i = 0; i < Conf->nspell; i++)
    {
      char *end;

      if (*Conf->Spell[i]->p.flag != '\0')
      {
        curaffix = strtol(Conf->Spell[i]->p.flag, &end, 10);
        if (Conf->Spell[i]->p.flag == end || errno == ERANGE)
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix alias \"%s\"", Conf->Spell[i]->p.flag)));
        }
        if (curaffix < 0 || curaffix >= Conf->nAffixData)
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix alias \"%s\"", Conf->Spell[i]->p.flag)));
        }
        if (*end != '\0' && !t_isdigit(end) && !t_isspace(end))
        {
          ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("invalid affix alias \"%s\"", Conf->Spell[i]->p.flag)));
        }
      }
      else
      {
           
                                                                       
                                      
           
        curaffix = 0;
      }

      Conf->Spell[i]->p.d.affix = curaffix;
      Conf->Spell[i]->p.d.len = strlen(Conf->Spell[i]->word);
    }
  }
                                           
  else
  {
                                                                    
    qsort((void *)Conf->Spell, Conf->nspell, sizeof(SPELL *), cmpspellaffix);

    naffix = 0;
    for (i = 0; i < Conf->nspell; i++)
    {
      if (i == 0 || strcmp(Conf->Spell[i]->p.flag, Conf->Spell[i - 1]->p.flag) != 0)
      {
        naffix++;
      }
    }

       
                                                                      
                                                                          
                                           
       
    Conf->AffixData = (char **)palloc0(naffix * sizeof(char *));

    curaffix = -1;
    for (i = 0; i < Conf->nspell; i++)
    {
      if (i == 0 || strcmp(Conf->Spell[i]->p.flag, Conf->AffixData[curaffix]) != 0)
      {
        curaffix++;
        Assert(curaffix < naffix);
        Conf->AffixData[curaffix] = cpstrdup(Conf, Conf->Spell[i]->p.flag);
      }

      Conf->Spell[i]->p.d.affix = curaffix;
      Conf->Spell[i]->p.d.len = strlen(Conf->Spell[i]->word);
    }

    Conf->lenAffixData = Conf->nAffixData = naffix;
  }

                                 
  qsort((void *)Conf->Spell, Conf->nspell, sizeof(SPELL *), cmpspell);
  Conf->Dictionary = mkSPNode(Conf, 0, Conf->nspell, 0);
}

   
                                                                             
                                                                              
                                               
   
                             
                                              
                                               
                                     
                                 
   
static AffixNode *
mkANode(IspellDict *Conf, int low, int high, int level, int type)
{
  int i;
  int nchar = 0;
  uint8 lastchar = '\0';
  AffixNode *rs;
  AffixNodeData *data;
  int lownew = low;
  int naff;
  AFFIX **aff;

  for (i = low; i < high; i++)
  {
    if (Conf->Affix[i].replen > level && lastchar != GETCHAR(Conf->Affix + i, level, type))
    {
      nchar++;
      lastchar = GETCHAR(Conf->Affix + i, level, type);
    }
  }

  if (!nchar)
  {
    return NULL;
  }

  aff = (AFFIX **)tmpalloc(sizeof(AFFIX *) * (high - low + 1));
  naff = 0;

  rs = (AffixNode *)cpalloc0(ANHRDSZ + nchar * sizeof(AffixNodeData));
  rs->length = nchar;
  data = rs->data;

  lastchar = '\0';
  for (i = low; i < high; i++)
  {
    if (Conf->Affix[i].replen > level)
    {
      if (lastchar != GETCHAR(Conf->Affix + i, level, type))
      {
        if (lastchar)
        {
                                             
          data->node = mkANode(Conf, lownew, i, level + 1, type);
          if (naff)
          {
            data->naff = naff;
            data->aff = (AFFIX **)cpalloc(sizeof(AFFIX *) * naff);
            memcpy(data->aff, aff, sizeof(AFFIX *) * naff);
            naff = 0;
          }
          data++;
          lownew = i;
        }
        lastchar = GETCHAR(Conf->Affix + i, level, type);
      }
      data->val = GETCHAR(Conf->Affix + i, level, type);
      if (Conf->Affix[i].replen == level + 1)
      {                    
        aff[naff++] = Conf->Affix + i;
      }
    }
  }

                                     
  data->node = mkANode(Conf, lownew, high, level + 1, type);
  if (naff)
  {
    data->naff = naff;
    data->aff = (AFFIX **)cpalloc(sizeof(AFFIX *) * naff);
    memcpy(data->aff, aff, sizeof(AFFIX *) * naff);
    naff = 0;
  }

  pfree(aff);

  return rs;
}

   
                                                                              
                                                               
   
static void
mkVoidAffix(IspellDict *Conf, bool issuffix, int startsuffix)
{
  int i, cnt = 0;
  int start = (issuffix) ? startsuffix : 0;
  int end = (issuffix) ? Conf->naffixes : startsuffix;
  AffixNode *Affix = (AffixNode *)palloc0(ANHRDSZ + sizeof(AffixNodeData));

  Affix->length = 1;
  Affix->isvoid = 1;

  if (issuffix)
  {
    Affix->data->node = Conf->Suffix;
    Conf->Suffix = Affix;
  }
  else
  {
    Affix->data->node = Conf->Prefix;
    Conf->Prefix = Affix;
  }

                                               
  for (i = start; i < end; i++)
  {
    if (Conf->Affix[i].replen == 0)
    {
      cnt++;
    }
  }

                                                      
  if (cnt == 0)
  {
    return;
  }

  Affix->data->aff = (AFFIX **)cpalloc(sizeof(AFFIX *) * cnt);
  Affix->data->naff = (uint32)cnt;

  cnt = 0;
  for (i = start; i < end; i++)
  {
    if (Conf->Affix[i].replen == 0)
    {
      Affix->data->aff[cnt] = Conf->Affix + i;
      cnt++;
    }
  }
}

   
                                                                           
                                                                          
   
                             
                          
   
                                                                           
                  
   
static bool
isAffixInUse(IspellDict *Conf, char *affixflag)
{
  int i;

  for (i = 0; i < Conf->nAffixData; i++)
  {
    if (IsAffixFlagInUse(Conf, i, affixflag))
    {
      return true;
    }
  }

  return false;
}

   
                                                                         
   
void
NISortAffixes(IspellDict *Conf)
{
  AFFIX *Affix;
  size_t i;
  CMPDAffix *ptr;
  int firstsuffix = Conf->naffixes;

  if (Conf->naffixes == 0)
  {
    return;
  }

                                                               
  if (Conf->naffixes > 1)
  {
    qsort((void *)Conf->Affix, Conf->naffixes, sizeof(AFFIX), cmpaffix);
  }
  Conf->CompoundAffix = ptr = (CMPDAffix *)palloc(sizeof(CMPDAffix) * Conf->naffixes);
  ptr->affix = NULL;

  for (i = 0; i < Conf->naffixes; i++)
  {
    Affix = &(((AFFIX *)Conf->Affix)[i]);
    if (Affix->type == FF_SUFFIX && i < firstsuffix)
    {
      firstsuffix = i;
    }

    if ((Affix->flagflags & FF_COMPOUNDFLAG) && Affix->replen > 0 && isAffixInUse(Conf, Affix->flag))
    {
      bool issuffix = (Affix->type == FF_SUFFIX);

      if (ptr == Conf->CompoundAffix || issuffix != (ptr - 1)->issuffix || strbncmp((const unsigned char *)(ptr - 1)->affix, (const unsigned char *)Affix->repl, (ptr - 1)->len))
      {
                                                     
        ptr->affix = Affix->repl;
        ptr->len = Affix->replen;
        ptr->issuffix = issuffix;
        ptr++;
      }
    }
  }
  ptr->affix = NULL;
  Conf->CompoundAffix = (CMPDAffix *)repalloc(Conf->CompoundAffix, sizeof(CMPDAffix) * (ptr - Conf->CompoundAffix + 1));

                                 
  Conf->Prefix = mkANode(Conf, 0, firstsuffix, 0, FF_PREFIX);
  Conf->Suffix = mkANode(Conf, firstsuffix, Conf->naffixes, 0, FF_SUFFIX);
  mkVoidAffix(Conf, true, firstsuffix);
  mkVoidAffix(Conf, false, firstsuffix);
}

static AffixNodeData *
FindAffixes(AffixNode *node, const char *word, int wrdlen, int *level, int type)
{
  AffixNodeData *StopLow, *StopHigh, *StopMiddle;
  uint8 symbol;

  if (node->isvoid)
  {                          
    if (node->data->naff)
    {
      return node->data;
    }
    node = node->data->node;
  }

  while (node && *level < wrdlen)
  {
    StopLow = node->data;
    StopHigh = node->data + node->length;
    while (StopLow < StopHigh)
    {
      StopMiddle = StopLow + ((StopHigh - StopLow) >> 1);
      symbol = GETWCHAR(word, wrdlen, *level, type);

      if (StopMiddle->val == symbol)
      {
        (*level)++;
        if (StopMiddle->naff)
        {
          return StopMiddle;
        }
        node = StopMiddle->node;
        break;
      }
      else if (StopMiddle->val < symbol)
      {
        StopLow = StopMiddle + 1;
      }
      else
      {
        StopHigh = StopMiddle;
      }
    }
    if (StopLow >= StopHigh)
    {
      break;
    }
  }
  return NULL;
}

static char *
CheckAffix(const char *word, size_t len, AFFIX *Affix, int flagflags, char *newword, int *baselen)
{
     
                                
     

  if (flagflags == 0)
  {
    if (Affix->flagflags & FF_COMPOUNDONLY)
    {
      return NULL;
    }
  }
  else if (flagflags & FF_COMPOUNDBEGIN)
  {
    if (Affix->flagflags & FF_COMPOUNDFORBIDFLAG)
    {
      return NULL;
    }
    if ((Affix->flagflags & FF_COMPOUNDBEGIN) == 0)
    {
      if (Affix->type == FF_SUFFIX)
      {
        return NULL;
      }
    }
  }
  else if (flagflags & FF_COMPOUNDMIDDLE)
  {
    if ((Affix->flagflags & FF_COMPOUNDMIDDLE) == 0 || (Affix->flagflags & FF_COMPOUNDFORBIDFLAG))
    {
      return NULL;
    }
  }
  else if (flagflags & FF_COMPOUNDLAST)
  {
    if (Affix->flagflags & FF_COMPOUNDFORBIDFLAG)
    {
      return NULL;
    }
    if ((Affix->flagflags & FF_COMPOUNDLAST) == 0)
    {
      if (Affix->type == FF_PREFIX)
      {
        return NULL;
      }
    }
  }

     
                                   
     
  if (Affix->type == FF_SUFFIX)
  {
    strcpy(newword, word);
    strcpy(newword + len - Affix->replen, Affix->find);
    if (baselen)                                               
    {
      *baselen = len - Affix->replen;
    }
  }
  else
  {
       
                                                                   
                                               
       
    if (baselen && *baselen + strlen(Affix->find) <= Affix->replen)
    {
      return NULL;
    }
    strcpy(newword, Affix->find);
    strcat(newword, word + Affix->replen);
  }

     
                          
     
  if (Affix->issimple)
  {
    return newword;
  }
  else if (Affix->isregis)
  {
    if (RS_execute(&(Affix->reg.regis), newword))
    {
      return newword;
    }
  }
  else
  {
    pg_wchar *data;
    size_t data_len;
    int newword_len;

                                                
    newword_len = strlen(newword);
    data = (pg_wchar *)palloc((newword_len + 1) * sizeof(pg_wchar));
    data_len = pg_mb2wchar_with_len(newword, data, newword_len);

    if (pg_regexec(&(Affix->reg.pregex->regex), data, data_len, 0, NULL, 0, NULL, 0) == REG_OKAY)
    {
      pfree(data);
      return newword;
    }
    pfree(data);
  }

  return NULL;
}

static int
addToResult(char **forms, char **cur, char *word)
{
  if (cur - forms >= MAX_NORM - 1)
  {
    return 0;
  }
  if (forms == cur || strcmp(word, *(cur - 1)) != 0)
  {
    *cur = pstrdup(word);
    *(cur + 1) = NULL;
    return 1;
  }

  return 0;
}

static char **
NormalizeSubWord(IspellDict *Conf, char *word, int flag)
{
  AffixNodeData *suffix = NULL, *prefix = NULL;
  int slevel = 0, plevel = 0;
  int wrdlen = strlen(word), swrdlen;
  char **forms;
  char **cur;
  char newword[2 * MAXNORMLEN] = "";
  char pnewword[2 * MAXNORMLEN] = "";
  AffixNode *snode = Conf->Suffix, *pnode;
  int i, j;

  if (wrdlen > MAXNORMLEN)
  {
    return NULL;
  }
  cur = forms = (char **)palloc(MAX_NORM * sizeof(char *));
  *cur = NULL;

                                                 
  if (FindWord(Conf, word, VoidString, flag))
  {
    *cur = pstrdup(word);
    cur++;
    *cur = NULL;
  }

                                                                     
  pnode = Conf->Prefix;
  plevel = 0;
  while (pnode)
  {
    prefix = FindAffixes(pnode, word, wrdlen, &plevel, FF_PREFIX);
    if (!prefix)
    {
      break;
    }
    for (j = 0; j < prefix->naff; j++)
    {
      if (CheckAffix(word, wrdlen, prefix->aff[j], flag, newword, NULL))
      {
                            
        if (FindWord(Conf, newword, prefix->aff[j]->flag, flag))
        {
          cur += addToResult(forms, cur, newword);
        }
      }
    }
    pnode = prefix->node;
  }

     
                                                                      
             
     
  while (snode)
  {
    int baselen = 0;

                              
    suffix = FindAffixes(snode, word, wrdlen, &slevel, FF_SUFFIX);
    if (!suffix)
    {
      break;
    }
                                    
    for (i = 0; i < suffix->naff; i++)
    {
      if (CheckAffix(word, wrdlen, suffix->aff[i], flag, newword, &baselen))
      {
                            
        if (FindWord(Conf, newword, suffix->aff[i]->flag, flag))
        {
          cur += addToResult(forms, cur, newword);
        }

                                                         
        pnode = Conf->Prefix;
        plevel = 0;
        swrdlen = strlen(newword);
        while (pnode)
        {
          prefix = FindAffixes(pnode, newword, swrdlen, &plevel, FF_PREFIX);
          if (!prefix)
          {
            break;
          }
          for (j = 0; j < prefix->naff; j++)
          {
            if (CheckAffix(newword, swrdlen, prefix->aff[j], flag, pnewword, &baselen))
            {
                                  
              char *ff = (prefix->aff[j]->flagflags & suffix->aff[i]->flagflags & FF_CROSSPRODUCT) ? VoidString : prefix->aff[j]->flag;

              if (FindWord(Conf, pnewword, ff, flag))
              {
                cur += addToResult(forms, cur, pnewword);
              }
            }
          }
          pnode = prefix->node;
        }
      }
    }

    snode = suffix->node;
  }

  if (cur == forms)
  {
    pfree(forms);
    return NULL;
  }
  return forms;
}

typedef struct SplitVar
{
  int nstem;
  int lenstem;
  char **stem;
  struct SplitVar *next;
} SplitVar;

static int
CheckCompoundAffixes(CMPDAffix **ptr, char *word, int len, bool CheckInPlace)
{
  bool issuffix;

                                      
  if (*ptr == NULL)
  {
    return -1;
  }

  if (CheckInPlace)
  {
    while ((*ptr)->affix)
    {
      if (len > (*ptr)->len && strncmp((*ptr)->affix, word, (*ptr)->len) == 0)
      {
        len = (*ptr)->len;
        issuffix = (*ptr)->issuffix;
        (*ptr)++;
        return (issuffix) ? len : 0;
      }
      (*ptr)++;
    }
  }
  else
  {
    char *affbegin;

    while ((*ptr)->affix)
    {
      if (len > (*ptr)->len && (affbegin = strstr(word, (*ptr)->affix)) != NULL)
      {
        len = (*ptr)->len + (affbegin - word);
        issuffix = (*ptr)->issuffix;
        (*ptr)++;
        return (issuffix) ? len : 0;
      }
      (*ptr)++;
    }
  }
  return -1;
}

static SplitVar *
CopyVar(SplitVar *s, int makedup)
{
  SplitVar *v = (SplitVar *)palloc(sizeof(SplitVar));

  v->next = NULL;
  if (s)
  {
    int i;

    v->lenstem = s->lenstem;
    v->stem = (char **)palloc(sizeof(char *) * v->lenstem);
    v->nstem = s->nstem;
    for (i = 0; i < s->nstem; i++)
    {
      v->stem[i] = (makedup) ? pstrdup(s->stem[i]) : s->stem[i];
    }
  }
  else
  {
    v->lenstem = 16;
    v->stem = (char **)palloc(sizeof(char *) * v->lenstem);
    v->nstem = 0;
  }
  return v;
}

static void
AddStem(SplitVar *v, char *word)
{
  if (v->nstem >= v->lenstem)
  {
    v->lenstem *= 2;
    v->stem = (char **)repalloc(v->stem, sizeof(char *) * v->lenstem);
  }

  v->stem[v->nstem] = word;
  v->nstem++;
}

static SplitVar *
SplitToVariants(IspellDict *Conf, SPNode *snode, SplitVar *orig, char *word, int wordlen, int startpos, int minpos)
{
  SplitVar *var = NULL;
  SPNodeData *StopLow, *StopHigh, *StopMiddle = NULL;
  SPNode *node = (snode) ? snode : Conf->Dictionary;
  int level = (snode) ? minpos : startpos;              
                                                              
  int lenaff;
  CMPDAffix *caff;
  char *notprobed;
  int compoundflag = 0;

                                                                          
  check_stack_depth();

  notprobed = (char *)palloc(wordlen);
  memset(notprobed, 1, wordlen);
  var = CopyVar(orig, 1);

  while (level < wordlen)
  {
                                                         
    caff = Conf->CompoundAffix;
    while (level > startpos && (lenaff = CheckCompoundAffixes(&caff, word + level, wordlen - level, (node) ? true : false)) >= 0)
    {
         
                                                                       
         
      char buf[MAXNORMLEN];
      char **subres;

      lenaff = level - startpos + lenaff;

      if (!notprobed[startpos + lenaff - 1])
      {
        continue;
      }

      if (level + lenaff - 1 <= minpos)
      {
        continue;
      }

      if (lenaff >= MAXNORMLEN)
      {
        continue;                         
      }
      if (lenaff > 0)
      {
        memcpy(buf, word + startpos, lenaff);
      }
      buf[lenaff] = '\0';

      if (level == 0)
      {
        compoundflag = FF_COMPOUNDBEGIN;
      }
      else if (level == wordlen - 1)
      {
        compoundflag = FF_COMPOUNDLAST;
      }
      else
      {
        compoundflag = FF_COMPOUNDMIDDLE;
      }
      subres = NormalizeSubWord(Conf, buf, compoundflag);
      if (subres)
      {
                                                
        SplitVar *new = CopyVar(var, 0);
        SplitVar *ptr = var;
        char **sptr = subres;

        notprobed[startpos + lenaff - 1] = 0;

        while (*sptr)
        {
          AddStem(new, *sptr);
          sptr++;
        }
        pfree(subres);

        while (ptr->next)
        {
          ptr = ptr->next;
        }
        ptr->next = SplitToVariants(Conf, NULL, new, word, wordlen, startpos + lenaff, startpos + lenaff);

        pfree(new->stem);
        pfree(new);
      }
    }

    if (!node)
    {
      break;
    }

    StopLow = node->data;
    StopHigh = node->data + node->length;
    while (StopLow < StopHigh)
    {
      StopMiddle = StopLow + ((StopHigh - StopLow) >> 1);
      if (StopMiddle->val == ((uint8 *)(word))[level])
      {
        break;
      }
      else if (StopMiddle->val < ((uint8 *)(word))[level])
      {
        StopLow = StopMiddle + 1;
      }
      else
      {
        StopHigh = StopMiddle;
      }
    }

    if (StopLow < StopHigh)
    {
      if (startpos == 0)
      {
        compoundflag = FF_COMPOUNDBEGIN;
      }
      else if (level == wordlen - 1)
      {
        compoundflag = FF_COMPOUNDLAST;
      }
      else
      {
        compoundflag = FF_COMPOUNDMIDDLE;
      }

                           
      if (StopMiddle->isword && (StopMiddle->compoundflag & compoundflag) && notprobed[level])
      {
                                                    
        if (level > minpos)
        {
                                                
          if (wordlen == level + 1)
          {
                                        
            AddStem(var, pnstrdup(word + startpos, wordlen - startpos));
            pfree(notprobed);
            return var;
          }
          else
          {
                                                                     
            SplitVar *ptr = var;

            while (ptr->next)
            {
              ptr = ptr->next;
            }
            ptr->next = SplitToVariants(Conf, node, var, word, wordlen, startpos, level);
                                       
            level++;
            AddStem(var, pnstrdup(word + startpos, level - startpos));
            node = Conf->Dictionary;
            startpos = level;
            continue;
          }
        }
      }
      node = StopMiddle->node;
    }
    else
    {
      node = NULL;
    }
    level++;
  }

  AddStem(var, pnstrdup(word + startpos, wordlen - startpos));
  pfree(notprobed);
  return var;
}

static void
addNorm(TSLexeme **lres, TSLexeme **lcur, char *word, int flags, uint16 NVariant)
{
  if (*lres == NULL)
  {
    *lcur = *lres = (TSLexeme *)palloc(MAX_NORM * sizeof(TSLexeme));
  }

  if (*lcur - *lres < MAX_NORM - 1)
  {
    (*lcur)->lexeme = word;
    (*lcur)->flags = flags;
    (*lcur)->nvariant = NVariant;
    (*lcur)++;
    (*lcur)->lexeme = NULL;
  }
}

TSLexeme *
NINormalizeWord(IspellDict *Conf, char *word)
{
  char **res;
  TSLexeme *lcur = NULL, *lres = NULL;
  uint16 NVariant = 1;

  res = NormalizeSubWord(Conf, word, 0);

  if (res)
  {
    char **ptr = res;

    while (*ptr && (lcur - lres) < MAX_NORM)
    {
      addNorm(&lres, &lcur, *ptr, 0, NVariant++);
      ptr++;
    }
    pfree(res);
  }

  if (Conf->usecompound)
  {
    int wordlen = strlen(word);
    SplitVar *ptr, *var = SplitToVariants(Conf, NULL, NULL, word, wordlen, 0, -1);
    int i;

    while (var)
    {
      if (var->nstem > 1)
      {
        char **subres = NormalizeSubWord(Conf, var->stem[var->nstem - 1], FF_COMPOUNDLAST);

        if (subres)
        {
          char **subptr = subres;

          while (*subptr)
          {
            for (i = 0; i < var->nstem - 1; i++)
            {
              addNorm(&lres, &lcur, (subptr == subres) ? var->stem[i] : pstrdup(var->stem[i]), 0, NVariant);
            }

            addNorm(&lres, &lcur, *subptr, 0, NVariant);
            subptr++;
            NVariant++;
          }

          pfree(subres);
          var->stem[0] = NULL;
          pfree(var->stem[var->nstem - 1]);
        }
      }

      for (i = 0; i < var->nstem && var->stem[i]; i++)
      {
        pfree(var->stem[i]);
      }
      ptr = var->next;
      pfree(var->stem);
      pfree(var);
      var = ptr;
    }
  }

  return lres;
}
