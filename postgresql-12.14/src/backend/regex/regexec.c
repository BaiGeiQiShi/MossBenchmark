   
                                    
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                               
   
   

#include "regex/regguts.h"

                             
struct arcp
{                             
  struct sset *ss;
  color co;
};

struct sset
{                                  
  unsigned *states;                           
  unsigned hash;                           
#define HASH(bv, nw) (((nw) == 1) ? *(bv) : hash(bv, nw))
#define HIT(h, bv, ss, nw) ((ss)->hash == (h) && ((nw) == 1 || memcmp(VS(bv), VS((ss)->states), (nw) * sizeof(unsigned)) == 0))
  int flags;
#define STARTER 01                                 
#define POSTSTATE 02                                 
#define LOCKED 04                            
#define NOPROGRESS 010                               
  struct arcp ins;                                         
  chr *lastseen;                                          
  struct sset **outs;                                       
  struct arcp *inchain;                                       
};

struct dfa
{
  int nssets;                                
  int nssused;                                               
  int nstates;                                  
  int ncolors;                                                      
  int wordsper;                                               
  struct sset *ssets;                          
  unsigned *statesarea;                          
  unsigned *work;                                                     
  struct sset **outsarea;                            
  struct arcp *incarea;                        
  struct cnfa *cnfa;
  struct colormap *cm;
  chr *lastpost;                                                   
  chr *lastnopr;                                                      
  struct sset *search;                                        
  int cptsmalloced;                                               
  char *mallocarea;                                                
};

#define WORK 1                                       

                                                     
#define FEWSTATES 20                              
#define FEWCOLORS 15
struct smalldfa
{
  struct dfa dfa;
  struct sset ssets[FEWSTATES * 2];
  unsigned statesarea[FEWSTATES * 2 + WORK];
  struct sset *outsarea[FEWSTATES * 2 * FEWCOLORS];
  struct arcp incarea[FEWSTATES * 2 * FEWCOLORS];
};

#define DOMALLOC ((struct smalldfa *)NULL)                   

                                                         
struct vars
{
  regex_t *re;
  struct guts *g;
  int eflags;                          
  size_t nmatch;
  regmatch_t *pmatch;
  rm_detail_t *details;
  chr *start;                                   
  chr *search_start;                                   
  chr *stop;                                            
  int err;                                                 
  struct dfa **subdfas;                             
  struct dfa **ladfas;                               
  struct sset **lblastcss;                                              
  chr **lblastcp;                                                       
  struct smalldfa dfa1;
  struct smalldfa dfa2;
};

#define VISERR(vv) ((vv)->err != 0)                                 
#define ISERR() VISERR(v)
#define VERR(vv, e) ((vv)->err = ((vv)->err ? (vv)->err : (e)))
#define ERR(e) VERR(v, e)                      
#define NOERR()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return v->err;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  }                               
#define OFF(p) ((p)-v->start)
#define LOFF(p) ((long)OFF(p))

   
                        
   
                       
static struct dfa *
getsubdfa(struct vars *, struct subre *);
static struct dfa *
getladfa(struct vars *, int);
static int
find(struct vars *, struct cnfa *, struct colormap *);
static int
cfind(struct vars *, struct cnfa *, struct colormap *);
static int
cfindloop(struct vars *, struct cnfa *, struct colormap *, struct dfa *, struct dfa *, chr **);
static void
zapallsubs(regmatch_t *, size_t);
static void
zaptreesubs(struct vars *, struct subre *);
static void
subset(struct vars *, struct subre *, chr *, chr *);
static int
cdissect(struct vars *, struct subre *, chr *, chr *);
static int
ccondissect(struct vars *, struct subre *, chr *, chr *);
static int
crevcondissect(struct vars *, struct subre *, chr *, chr *);
static int
cbrdissect(struct vars *, struct subre *, chr *, chr *);
static int
caltdissect(struct vars *, struct subre *, chr *, chr *);
static int
citerdissect(struct vars *, struct subre *, chr *, chr *);
static int
creviterdissect(struct vars *, struct subre *, chr *, chr *);

                        
static chr *
longest(struct vars *, struct dfa *, chr *, chr *, int *);
static chr *
shortest(struct vars *, struct dfa *, chr *, chr *, chr *, chr **, int *);
static int
matchuntil(struct vars *, struct dfa *, chr *, struct sset **, chr **);
static chr *
lastcold(struct vars *, struct dfa *);
static struct dfa *
newdfa(struct vars *, struct cnfa *, struct colormap *, struct smalldfa *);
static void
freedfa(struct dfa *);
static unsigned
hash(unsigned *, int);
static struct sset *
initialize(struct vars *, struct dfa *, chr *);
static struct sset *
miss(struct vars *, struct dfa *, struct sset *, color, chr *, chr *);
static int
lacon(struct vars *, struct cnfa *, chr *, color);
static struct sset *
getvacant(struct vars *, struct dfa *, chr *, chr *);
static struct sset *
pickss(struct vars *, struct dfa *, chr *, chr *);

   
                                         
   
int
pg_regexec(regex_t *re, const chr *string, size_t len, size_t search_start, rm_detail_t *details, size_t nmatch, regmatch_t pmatch[], int flags)
{
  struct vars var;
  register struct vars *v = &var;
  int st;
  size_t n;
  size_t i;
  int backref;

#define LOCALMAT 20
  regmatch_t mat[LOCALMAT];

#define LOCALDFAS 40
  struct dfa *subdfas[LOCALDFAS];

                     
  if (re == NULL || string == NULL || re->re_magic != REMAGIC)
  {
    return REG_INVARG;
  }
  if (re->re_csize != sizeof(chr))
  {
    return REG_MIXED;
  }
  if (search_start > len)
  {
    return REG_NOMATCH;
  }

                                           
  pg_set_regex_collation(re->re_collation);

             
  v->re = re;
  v->g = (struct guts *)re->re_guts;
  if ((v->g->cflags & REG_EXPECT) && details == NULL)
  {
    return REG_INVARG;
  }
  if (v->g->info & REG_UIMPOSSIBLE)
  {
    return REG_NOMATCH;
  }
  backref = (v->g->info & REG_UBACKREF) ? 1 : 0;
  v->eflags = flags;
  if (v->g->cflags & REG_NOSUB)
  {
    nmatch = 0;                      
  }
  v->nmatch = nmatch;
  if (backref)
  {
                        
    if (v->g->nsub + 1 <= LOCALMAT)
    {
      v->pmatch = mat;
    }
    else
    {
      v->pmatch = (regmatch_t *)MALLOC((v->g->nsub + 1) * sizeof(regmatch_t));
    }
    if (v->pmatch == NULL)
    {
      return REG_ESPACE;
    }
    v->nmatch = v->g->nsub + 1;
  }
  else
  {
    v->pmatch = pmatch;
  }
  if (v->nmatch > 0)
  {
    zapallsubs(v->pmatch, v->nmatch);
  }
  v->details = details;
  v->start = (chr *)string;
  v->search_start = (chr *)string + search_start;
  v->stop = (chr *)string + len;
  v->err = 0;
  v->subdfas = NULL;
  v->ladfas = NULL;
  v->lblastcss = NULL;
  v->lblastcp = NULL;
                                                           

  assert(v->g->ntree >= 0);
  n = (size_t)v->g->ntree;
  if (n <= LOCALDFAS)
  {
    v->subdfas = subdfas;
  }
  else
  {
    v->subdfas = (struct dfa **)MALLOC(n * sizeof(struct dfa *));
    if (v->subdfas == NULL)
    {
      st = REG_ESPACE;
      goto cleanup;
    }
  }
  for (i = 0; i < n; i++)
  {
    v->subdfas[i] = NULL;
  }

  assert(v->g->nlacons >= 0);
  n = (size_t)v->g->nlacons;
  if (n > 0)
  {
    v->ladfas = (struct dfa **)MALLOC(n * sizeof(struct dfa *));
    if (v->ladfas == NULL)
    {
      st = REG_ESPACE;
      goto cleanup;
    }
    for (i = 0; i < n; i++)
    {
      v->ladfas[i] = NULL;
    }
    v->lblastcss = (struct sset **)MALLOC(n * sizeof(struct sset *));
    v->lblastcp = (chr **)MALLOC(n * sizeof(chr *));
    if (v->lblastcss == NULL || v->lblastcp == NULL)
    {
      st = REG_ESPACE;
      goto cleanup;
    }
    for (i = 0; i < n; i++)
    {
      v->lblastcss[i] = NULL;
      v->lblastcp[i] = NULL;
    }
  }

             
  assert(v->g->tree != NULL);
  if (backref)
  {
    st = cfind(v, &v->g->tree->cnfa, &v->g->cmap);
  }
  else
  {
    st = find(v, &v->g->tree->cnfa, &v->g->cmap);
  }

                                                        
  if (st == REG_OKAY && v->pmatch != pmatch && nmatch > 0)
  {
    zapallsubs(pmatch, nmatch);
    n = (nmatch < v->nmatch) ? nmatch : v->nmatch;
    memcpy(VS(pmatch), VS(v->pmatch), n * sizeof(regmatch_t));
  }

                
cleanup:
  if (v->pmatch != pmatch && v->pmatch != mat)
  {
    FREE(v->pmatch);
  }
  if (v->subdfas != NULL)
  {
    n = (size_t)v->g->ntree;
    for (i = 0; i < n; i++)
    {
      if (v->subdfas[i] != NULL)
      {
        freedfa(v->subdfas[i]);
      }
    }
    if (v->subdfas != subdfas)
    {
      FREE(v->subdfas);
    }
  }
  if (v->ladfas != NULL)
  {
    n = (size_t)v->g->nlacons;
    for (i = 0; i < n; i++)
    {
      if (v->ladfas[i] != NULL)
      {
        freedfa(v->ladfas[i]);
      }
    }
    FREE(v->ladfas);
  }
  if (v->lblastcss != NULL)
  {
    FREE(v->lblastcss);
  }
  if (v->lblastcp != NULL)
  {
    FREE(v->lblastcp);
  }

  return st;
}

   
                                                                
   
                                                                    
                                                              
   
static struct dfa *
getsubdfa(struct vars *v, struct subre *t)
{
  if (v->subdfas[t->id] == NULL)
  {
    v->subdfas[t->id] = newdfa(v, &t->cnfa, &v->g->cmap, DOMALLOC);
    if (ISERR())
    {
      return NULL;
    }
  }
  return v->subdfas[t->id];
}

   
                                                                
   
                                  
   
static struct dfa *
getladfa(struct vars *v, int n)
{
  assert(n > 0 && n < v->g->nlacons && v->g->lacons != NULL);

  if (v->ladfas[n] == NULL)
  {
    struct subre *sub = &v->g->lacons[n];

    v->ladfas[n] = newdfa(v, &sub->cnfa, &v->g->cmap, DOMALLOC);
    if (ISERR())
    {
      return NULL;
    }
  }
  return v->ladfas[n];
}

   
                                                                
   
static int
find(struct vars *v, struct cnfa *cnfa, struct colormap *cm)
{
  struct dfa *s;
  struct dfa *d;
  chr *begin;
  chr *end = NULL;
  chr *cold;
  chr *open;                                                 
  chr *close;
  int hitend;
  int shorter = (v->g->tree->flags & SHORTER) ? 1 : 0;

                                        
  s = newdfa(v, &v->g->search, cm, &v->dfa1);
  assert(!(ISERR() && s != NULL));
  NOERR();
  MDEBUG(("\nsearch at %ld\n", LOFF(v->start)));
  cold = NULL;
  close = shortest(v, s, v->search_start, v->search_start, v->stop, &cold, (int *)NULL);
  freedfa(s);
  NOERR();
  if (v->g->cflags & REG_EXPECT)
  {
    assert(v->details != NULL);
    if (cold != NULL)
    {
      v->details->rm_extend.rm_so = OFF(cold);
    }
    else
    {
      v->details->rm_extend.rm_so = OFF(v->stop);
    }
    v->details->rm_extend.rm_eo = OFF(v->stop);              
  }
  if (close == NULL)                
  {
    return REG_NOMATCH;
  }
  if (v->nmatch == 0)                                       
  {
    return REG_OKAY;
  }

                                     
  assert(cold != NULL);
  open = cold;
  cold = NULL;
  MDEBUG(("between %ld and %ld\n", LOFF(open), LOFF(close)));
  d = newdfa(v, cnfa, cm, &v->dfa1);
  assert(!(ISERR() && d != NULL));
  NOERR();
  for (begin = open; begin <= close; begin++)
  {
    MDEBUG(("\nfind trying at %ld\n", LOFF(begin)));
    if (shorter)
    {
      end = shortest(v, d, begin, begin, v->stop, (chr **)NULL, &hitend);
    }
    else
    {
      end = longest(v, d, begin, v->stop, &hitend);
    }
    if (ISERR())
    {
      freedfa(d);
      return v->err;
    }
    if (hitend && cold == NULL)
    {
      cold = begin;
    }
    if (end != NULL)
    {
      break;                     
    }
  }
  assert(end != NULL);                                         
  freedfa(d);

                            
  assert(v->nmatch > 0);
  v->pmatch[0].rm_so = OFF(begin);
  v->pmatch[0].rm_eo = OFF(end);
  if (v->g->cflags & REG_EXPECT)
  {
    if (cold != NULL)
    {
      v->details->rm_extend.rm_so = OFF(cold);
    }
    else
    {
      v->details->rm_extend.rm_so = OFF(v->stop);
    }
    v->details->rm_extend.rm_eo = OFF(v->stop);              
  }
  if (v->nmatch == 1)                             
  {
    return REG_OKAY;
  }

                       
  return cdissect(v, v->g->tree, begin, end);
}

   
                                                              
   
static int
cfind(struct vars *v, struct cnfa *cnfa, struct colormap *cm)
{
  struct dfa *s;
  struct dfa *d;
  chr *cold;
  int ret;

  s = newdfa(v, &v->g->search, cm, &v->dfa1);
  NOERR();
  d = newdfa(v, cnfa, cm, &v->dfa2);
  if (ISERR())
  {
    assert(d == NULL);
    freedfa(s);
    return v->err;
  }

  ret = cfindloop(v, cnfa, cm, d, s, &cold);

  freedfa(d);
  freedfa(s);
  NOERR();
  if (v->g->cflags & REG_EXPECT)
  {
    assert(v->details != NULL);
    if (cold != NULL)
    {
      v->details->rm_extend.rm_so = OFF(cold);
    }
    else
    {
      v->details->rm_extend.rm_so = OFF(v->stop);
    }
    v->details->rm_extend.rm_eo = OFF(v->stop);              
  }
  return ret;
}

   
                                  
   
static int
cfindloop(struct vars *v, struct cnfa *cnfa, struct colormap *cm, struct dfa *d, struct dfa *s, chr **coldp)                                     
{
  chr *begin;
  chr *end;
  chr *cold;
  chr *open;                                                 
  chr *close;
  chr *estart;
  chr *estop;
  int er;
  int shorter = v->g->tree->flags & SHORTER;
  int hitend;

  assert(d != NULL && s != NULL);
  cold = NULL;
  close = v->search_start;
  do
  {
                                                                     
    MDEBUG(("\ncsearch at %ld\n", LOFF(close)));
    close = shortest(v, s, close, close, v->stop, &cold, (int *)NULL);
    if (ISERR())
    {
      *coldp = cold;
      return v->err;
    }
    if (close == NULL)
    {
      break;                                      
    }
    assert(cold != NULL);
    open = cold;
    cold = NULL;
                                                                          
    MDEBUG(("cbetween %ld and %ld\n", LOFF(open), LOFF(close)));
    for (begin = open; begin <= close; begin++)
    {
      MDEBUG(("\ncfind trying at %ld\n", LOFF(begin)));
      estart = begin;
      estop = v->stop;
      for (;;)
      {
                                                    
        if (shorter)
        {
          end = shortest(v, d, begin, estart, estop, (chr **)NULL, &hitend);
        }
        else
        {
          end = longest(v, d, begin, estop, &hitend);
        }
        if (ISERR())
        {
          *coldp = cold;
          return v->err;
        }
        if (hitend && cold == NULL)
        {
          cold = begin;
        }
        if (end == NULL)
        {
          break;                                               
        }
        MDEBUG(("tentative end %ld\n", LOFF(end)));
                                                                     
        er = cdissect(v, v->g->tree, begin, end);
        if (er == REG_OKAY)
        {
          if (v->nmatch > 0)
          {
            v->pmatch[0].rm_so = OFF(begin);
            v->pmatch[0].rm_eo = OFF(end);
          }
          *coldp = cold;
          return REG_OKAY;
        }
        if (er != REG_NOMATCH)
        {
          ERR(er);
          *coldp = cold;
          return er;
        }
                                                                 
        if (shorter)
        {
          if (end == estop)
          {
            break;                                       
          }
          estart = end + 1;
        }
        else
        {
          if (end == begin)
          {
            break;                                       
          }
          estop = end - 1;
        }
      }                                       
    }                                        

       
                                                                        
                                                                          
                                                               
       
    close++;
  } while (close < v->stop);

  *coldp = cold;
  return REG_NOMATCH;
}

   
                                                                   
   
                                                               
   
static void
zapallsubs(regmatch_t *p, size_t n)
{
  size_t i;

  for (i = n - 1; i > 0; i--)
  {
    p[i].rm_so = -1;
    p[i].rm_eo = -1;
  }
}

   
                                                                        
   
static void
zaptreesubs(struct vars *v, struct subre *t)
{
  if (t->op == '(')
  {
    int n = t->subno;

    assert(n > 0);
    if ((size_t)n < v->nmatch)
    {
      v->pmatch[n].rm_so = -1;
      v->pmatch[n].rm_eo = -1;
    }
  }

  if (t->left != NULL)
  {
    zaptreesubs(v, t->left);
  }
  if (t->right != NULL)
  {
    zaptreesubs(v, t->right);
  }
}

   
                                                                
   
static void
subset(struct vars *v, struct subre *sub, chr *begin, chr *end)
{
  int n = sub->subno;

  assert(n > 0);
  if ((size_t)n >= v->nmatch)
  {
    return;
  }

  MDEBUG(("setting %d\n", n));
  v->pmatch[n].rm_so = OFF(begin);
  v->pmatch[n].rm_eo = OFF(end);
}

   
                                                                 
   
                                                                             
                                                                              
                                                                          
                                                    
   
                                                                             
                                                                          
                                                                         
                                                                            
             
   
                                                                      
                                                                          
                                   
                                                                      
                                               
                                                                      
                                                                     
                                                                      
                                    
                                                                        
                                                              
                                                                         
                                                                        
                                                                          
                                                                         
                                                                            
                        
                                                                       
                                                                      
                                                                         
                                                                          
                                 
   
static int                                                                     
cdissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                          
{
  int er;

  assert(t != NULL);
  MDEBUG(("cdissect %ld-%ld %c\n", LOFF(begin), LOFF(end), t->op));

                                                 
  if (CANCEL_REQUESTED(v->re))
  {
    return REG_CANCEL;
  }
                             
  if (STACK_TOO_DEEP(v->re))
  {
    return REG_ETOOBIG;
  }

  switch (t->op)
  {
  case '=':                    
    assert(t->left == NULL && t->right == NULL);
    er = REG_OKAY;                                     
    break;
  case 'b':                     
    assert(t->left == NULL && t->right == NULL);
    er = cbrdissect(v, t, begin, end);
    break;
  case '.':                    
    assert(t->left != NULL && t->right != NULL);
    if (t->left->flags & SHORTER)                   
    {
      er = crevcondissect(v, t, begin, end);
    }
    else
    {
      er = ccondissect(v, t, begin, end);
    }
    break;
  case '|':                  
    assert(t->left != NULL);
    er = caltdissect(v, t, begin, end);
    break;
  case '*':                
    assert(t->left != NULL);
    if (t->left->flags & SHORTER)                   
    {
      er = creviterdissect(v, t, begin, end);
    }
    else
    {
      er = citerdissect(v, t, begin, end);
    }
    break;
  case '(':                
    assert(t->left != NULL && t->right == NULL);
    assert(t->subno > 0);
    er = cdissect(v, t->left, begin, end);
    if (er == REG_OKAY)
    {
      subset(v, t, begin, end);
    }
    break;
  default:
    er = REG_ASSERT;
    break;
  }

     
                                                                      
                                                                       
                                                           
     
  assert(er != REG_NOMATCH || (t->flags & BACKR));

  return er;
}

   
                                                      
   
static int                                                                        
ccondissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                             
{
  struct dfa *d;
  struct dfa *d2;
  chr *mid;
  int er;

  assert(t->op == '.');
  assert(t->left != NULL && t->left->cnfa.nstates > 0);
  assert(t->right != NULL && t->right->cnfa.nstates > 0);
  assert(!(t->left->flags & SHORTER));

  d = getsubdfa(v, t->left);
  NOERR();
  d2 = getsubdfa(v, t->right);
  NOERR();
  MDEBUG(("cconcat %d\n", t->id));

                                 
  mid = longest(v, d, begin, end, (int *)NULL);
  NOERR();
  if (mid == NULL)
  {
    return REG_NOMATCH;
  }
  MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));

                                             
  for (;;)
  {
                                       
    if (longest(v, d2, mid, end, (int *)NULL) == end)
    {
      er = cdissect(v, t->left, begin, mid);
      if (er == REG_OKAY)
      {
        er = cdissect(v, t->right, mid, end);
        if (er == REG_OKAY)
        {
                            
          MDEBUG(("successful\n"));
          return REG_OKAY;
        }
                                                                     
        zaptreesubs(v, t->left);
      }
      if (er != REG_NOMATCH)
      {
        return er;
      }
    }
    NOERR();

                                                   
    if (mid == begin)
    {
                                       
      MDEBUG(("%d no midpoint\n", t->id));
      return REG_NOMATCH;
    }
    mid = longest(v, d, begin, mid - 1, (int *)NULL);
    NOERR();
    if (mid == NULL)
    {
                                    
      MDEBUG(("%d failed midpoint\n", t->id));
      return REG_NOMATCH;
    }
    MDEBUG(("%d: new midpoint %ld\n", t->id, LOFF(mid)));
  }

                      
  return REG_ASSERT;
}

   
                                                                         
   
static int                                                                           
crevcondissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                                
{
  struct dfa *d;
  struct dfa *d2;
  chr *mid;
  int er;

  assert(t->op == '.');
  assert(t->left != NULL && t->left->cnfa.nstates > 0);
  assert(t->right != NULL && t->right->cnfa.nstates > 0);
  assert(t->left->flags & SHORTER);

  d = getsubdfa(v, t->left);
  NOERR();
  d2 = getsubdfa(v, t->right);
  NOERR();
  MDEBUG(("crevcon %d\n", t->id));

                                 
  mid = shortest(v, d, begin, begin, end, (chr **)NULL, (int *)NULL);
  NOERR();
  if (mid == NULL)
  {
    return REG_NOMATCH;
  }
  MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));

                                             
  for (;;)
  {
                                       
    if (longest(v, d2, mid, end, (int *)NULL) == end)
    {
      er = cdissect(v, t->left, begin, mid);
      if (er == REG_OKAY)
      {
        er = cdissect(v, t->right, mid, end);
        if (er == REG_OKAY)
        {
                            
          MDEBUG(("successful\n"));
          return REG_OKAY;
        }
                                                                     
        zaptreesubs(v, t->left);
      }
      if (er != REG_NOMATCH)
      {
        return er;
      }
    }
    NOERR();

                                                   
    if (mid == end)
    {
                                       
      MDEBUG(("%d no midpoint\n", t->id));
      return REG_NOMATCH;
    }
    mid = shortest(v, d, begin, mid + 1, end, (chr **)NULL, (int *)NULL);
    NOERR();
    if (mid == NULL)
    {
                                    
      MDEBUG(("%d failed midpoint\n", t->id));
      return REG_NOMATCH;
    }
    MDEBUG(("%d: new midpoint %ld\n", t->id, LOFF(mid)));
  }

                      
  return REG_ASSERT;
}

   
                                               
   
static int                                                                       
cbrdissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                            
{
  int n = t->subno;
  size_t numreps;
  size_t tlen;
  size_t brlen;
  chr *brstring;
  chr *p;
  int min = t->min;
  int max = t->max;

  assert(t != NULL);
  assert(t->op == 'b');
  assert(n >= 0);
  assert((size_t)n < v->nmatch);

  MDEBUG(("cbackref n%d %d{%d-%d}\n", t->id, n, min, max));

                                     
  if (v->pmatch[n].rm_so == -1)
  {
    return REG_NOMATCH;
  }
  brstring = v->start + v->pmatch[n].rm_so;
  brlen = v->pmatch[n].rm_eo - v->pmatch[n].rm_so;

                                             
  if (brlen == 0)
  {
       
                                                                
                                                   
       
    if (begin == end && min <= max)
    {
      MDEBUG(("cbackref matched trivially\n"));
      return REG_OKAY;
    }
    return REG_NOMATCH;
  }
  if (begin == end)
  {
                                                   
    if (min == 0)
    {
      MDEBUG(("cbackref matched trivially\n"));
      return REG_OKAY;
    }
    return REG_NOMATCH;
  }

     
                                                                             
                             
     
  assert(end > begin);
  tlen = end - begin;
  if (tlen % brlen != 0)
  {
    return REG_NOMATCH;
  }
  numreps = tlen / brlen;
  if (numreps < min || (numreps > max && max != DUPINF))
  {
    return REG_NOMATCH;
  }

                                                
  p = begin;
  while (numreps-- > 0)
  {
    if ((*v->g->compare)(brstring, p, brlen) != 0)
    {
      return REG_NOMATCH;
    }
    p += brlen;
  }

  MDEBUG(("cbackref matched\n"));
  return REG_OKAY;
}

   
                                                    
   
static int                                                                        
caltdissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                             
{
  struct dfa *d;
  int er;

                                                                            
  while (t != NULL)
  {
    assert(t->op == '|');
    assert(t->left != NULL && t->left->cnfa.nstates > 0);

    MDEBUG(("calt n%d\n", t->id));

    d = getsubdfa(v, t->left);
    NOERR();
    if (longest(v, d, begin, end, (int *)NULL) == end)
    {
      MDEBUG(("calt matched\n"));
      er = cdissect(v, t->left, begin, end);
      if (er != REG_NOMATCH)
      {
        return er;
      }
    }
    NOERR();

    t = t->right;
  }

  return REG_NOMATCH;
}

   
                                                   
   
static int                                                                         
citerdissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                              
{
  struct dfa *d;
  chr **endpts;
  chr *limit;
  int min_matches;
  size_t max_matches;
  int nverified;
  int k;
  int i;
  int er;

  assert(t->op == '*');
  assert(t->left != NULL && t->left->cnfa.nstates > 0);
  assert(!(t->left->flags & SHORTER));
  assert(begin <= end);

     
                                                                         
                                                                            
                                                                        
                                                                             
                                                                         
                                                                             
                                                                       
     
  min_matches = t->min;
  if (min_matches <= 0)
  {
    min_matches = 1;
  }

     
                                                                           
                                                                          
                                                                           
                                                                       
     
                                                                          
                                                    
     
  max_matches = end - begin;
  if (max_matches > t->max && t->max != DUPINF)
  {
    max_matches = t->max;
  }
  if (max_matches < min_matches)
  {
    max_matches = min_matches;
  }
  endpts = (chr **)MALLOC((max_matches + 1) * sizeof(chr *));
  if (endpts == NULL)
  {
    return REG_ESPACE;
  }
  endpts[0] = begin;

  d = getsubdfa(v, t->left);
  if (ISERR())
  {
    FREE(endpts);
    return v->err;
  }
  MDEBUG(("citer %d\n", t->id));

     
                                                                         
                                                                           
                                                                       
                                                                          
                                                                   
                                                                            
                                                    
     

                                              
  nverified = 0;
  k = 1;
  limit = end;

                                             
  while (k > 0)
  {
                                                        
    endpts[k] = longest(v, d, endpts[k - 1], limit, (int *)NULL);
    if (ISERR())
    {
      FREE(endpts);
      return v->err;
    }
    if (endpts[k] == NULL)
    {
                                                                    
      k--;
      goto backtrack;
    }
    MDEBUG(("%d: working endpoint %d: %ld\n", t->id, k, LOFF(endpts[k])));

                                                             
    if (nverified >= k)
    {
      nverified = k - 1;
    }

    if (endpts[k] != end)
    {
                                                                     
      if (k >= max_matches)
      {
                                                     
        k--;
        goto backtrack;
      }

                                                                    
      if (endpts[k] == endpts[k - 1] && (k >= min_matches || min_matches - k < end - endpts[k]))
      {
        goto backtrack;
      }

      k++;
      limit = end;
      continue;
    }

       
                                                                           
                                                                          
                                                                          
                                                            
       
    if (k < min_matches)
    {
      goto backtrack;
    }

    MDEBUG(("%d: verifying %d..%d\n", t->id, nverified + 1, k));

    for (i = nverified + 1; i <= k; i++)
    {
                                                        
      zaptreesubs(v, t->left);
      er = cdissect(v, t->left, endpts[i - 1], endpts[i]);
      if (er == REG_OKAY)
      {
        nverified = i;
        continue;
      }
      if (er == REG_NOMATCH)
      {
        break;
      }
                                  
      FREE(endpts);
      return er;
    }

    if (i > k)
    {
                        
      MDEBUG(("%d successful\n", t->id));
      FREE(endpts);
      return REG_OKAY;
    }

                                                      
    k = i;

  backtrack:

       
                                                                       
                                                            
       
    while (k > 0)
    {
      chr *prev_end = endpts[k - 1];

      if (endpts[k] > prev_end)
      {
        limit = endpts[k] - 1;
        if (limit > prev_end || (k < min_matches && min_matches - k >= end - prev_end))
        {
                                                                   
          break;
        }
      }
                                                                        
      k--;
    }
  }

                                   
  FREE(endpts);

     
                                                                            
                                
     
  if (t->min == 0 && begin == end)
  {
    MDEBUG(("%d allowing zero matches\n", t->id));
    return REG_OKAY;
  }

  MDEBUG(("%d failed\n", t->id));
  return REG_NOMATCH;
}

   
                                                                      
   
static int                                                                            
creviterdissect(struct vars *v, struct subre *t, chr *begin,                                      
    chr *end)                                                                 
{
  struct dfa *d;
  chr **endpts;
  chr *limit;
  int min_matches;
  size_t max_matches;
  int nverified;
  int k;
  int i;
  int er;

  assert(t->op == '*');
  assert(t->left != NULL && t->left->cnfa.nstates > 0);
  assert(t->left->flags & SHORTER);
  assert(begin <= end);

     
                                                                           
                                                                           
                                 
     
  min_matches = t->min;
  if (min_matches <= 0)
  {
    if (begin == end)
    {
      return REG_OKAY;
    }
    min_matches = 1;
  }

     
                                                                           
                                                                          
                                                                           
                                                                       
     
                                                                          
                                                    
     
  max_matches = end - begin;
  if (max_matches > t->max && t->max != DUPINF)
  {
    max_matches = t->max;
  }
  if (max_matches < min_matches)
  {
    max_matches = min_matches;
  }
  endpts = (chr **)MALLOC((max_matches + 1) * sizeof(chr *));
  if (endpts == NULL)
  {
    return REG_ESPACE;
  }
  endpts[0] = begin;

  d = getsubdfa(v, t->left);
  if (ISERR())
  {
    FREE(endpts);
    return v->err;
  }
  MDEBUG(("creviter %d\n", t->id));

     
                                                                         
                                                                           
                                                                       
                                                                          
                                                                   
                                                                            
                                                    
     

                                              
  nverified = 0;
  k = 1;
  limit = begin;

                                             
  while (k > 0)
  {
                                                                    
    if (limit == endpts[k - 1] && limit != end && (k >= min_matches || min_matches - k < end - limit))
    {
      limit++;
    }

                                                                         
    if (k >= max_matches)
    {
      limit = end;
    }

                                                        
    endpts[k] = shortest(v, d, endpts[k - 1], limit, end, (chr **)NULL, (int *)NULL);
    if (ISERR())
    {
      FREE(endpts);
      return v->err;
    }
    if (endpts[k] == NULL)
    {
                                                                     
      k--;
      goto backtrack;
    }
    MDEBUG(("%d: working endpoint %d: %ld\n", t->id, k, LOFF(endpts[k])));

                                                             
    if (nverified >= k)
    {
      nverified = k - 1;
    }

    if (endpts[k] != end)
    {
                                                                     
      if (k >= max_matches)
      {
                                                      
        k--;
        goto backtrack;
      }

      k++;
      limit = endpts[k - 1];
      continue;
    }

       
                                                                           
                                                                          
                                                                          
                                                            
       
    if (k < min_matches)
    {
      goto backtrack;
    }

    MDEBUG(("%d: verifying %d..%d\n", t->id, nverified + 1, k));

    for (i = nverified + 1; i <= k; i++)
    {
                                                        
      zaptreesubs(v, t->left);
      er = cdissect(v, t->left, endpts[i - 1], endpts[i]);
      if (er == REG_OKAY)
      {
        nverified = i;
        continue;
      }
      if (er == REG_NOMATCH)
      {
        break;
      }
                                  
      FREE(endpts);
      return er;
    }

    if (i > k)
    {
                        
      MDEBUG(("%d successful\n", t->id));
      FREE(endpts);
      return REG_OKAY;
    }

                                                      
    k = i;

  backtrack:

       
                                                            
       
    while (k > 0)
    {
      if (endpts[k] < end)
      {
        limit = endpts[k] + 1;
                                                                 
        break;
      }
                                                                         
      k--;
    }
  }

                                   
  MDEBUG(("%d failed\n", t->id));
  FREE(endpts);
  return REG_NOMATCH;
}

#include "rege_dfa.c"
