   
                                      
                                                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                               
   
   

#include "regex/regguts.h"

   
                                                                             
   
                       
static void
moresubs(struct vars *, int);
static int
freev(struct vars *, int);
static void
makesearch(struct vars *, struct nfa *);
static struct subre *
parse(struct vars *, int, int, struct state *, struct state *);
static struct subre *
parsebranch(struct vars *, int, int, struct state *, struct state *, int);
static void
parseqatom(struct vars *, int, int, struct state *, struct state *, struct subre *);
static void
nonword(struct vars *, int, struct state *, struct state *);
static void
word(struct vars *, int, struct state *, struct state *);
static int
scannum(struct vars *);
static void
repeat(struct vars *, struct state *, struct state *, int, int);
static void
bracket(struct vars *, struct state *, struct state *);
static void
cbracket(struct vars *, struct state *, struct state *);
static void
brackpart(struct vars *, struct state *, struct state *);
static const chr *
scanplain(struct vars *);
static void
onechr(struct vars *, chr, struct state *, struct state *);
static void
wordchrs(struct vars *);
static void
processlacon(struct vars *, struct state *, struct state *, int, struct state *, struct state *);
static struct subre *
subre(struct vars *, int, int, struct state *, struct state *);
static void
freesubre(struct vars *, struct subre *);
static void
freesrnode(struct vars *, struct subre *);
static void
optst(struct vars *, struct subre *);
static int
numst(struct subre *, int);
static void
markst(struct subre *);
static void
cleanst(struct vars *);
static long
nfatree(struct vars *, struct subre *, FILE *);
static long
nfanode(struct vars *, struct subre *, int, FILE *);
static int
newlacon(struct vars *, struct state *, struct state *, int);
static void
freelacons(struct subre *, int);
static void
rfree(regex_t *);
static int
rcancelrequested(void);
static int
rstacktoodeep(void);

#ifdef REG_DEBUG
static void
dump(regex_t *, FILE *);
static void
dumpst(struct subre *, FILE *, int);
static void
stdump(struct subre *, FILE *, int);
static const char *
stid(struct subre *, char *, size_t);
#endif
                        
static void
lexstart(struct vars *);
static void
prefixes(struct vars *);
static void
lexnest(struct vars *, const chr *, const chr *);
static void
lexword(struct vars *);
static int
next(struct vars *);
static int
lexescape(struct vars *);
static chr
lexdigits(struct vars *, int, int, int);
static int
brenext(struct vars *, chr);
static void
skip(struct vars *);
static chr
newline(void);
static chr
chrnamed(struct vars *, const chr *, const chr *, chr);

                          
static void
initcm(struct vars *, struct colormap *);
static void
freecm(struct colormap *);
static color
maxcolor(struct colormap *);
static color
newcolor(struct colormap *);
static void
freecolor(struct colormap *, color);
static color
pseudocolor(struct colormap *);
static color
subcolor(struct colormap *, chr);
static color
subcolorhi(struct colormap *, color *);
static color
newsub(struct colormap *, color);
static int
newhicolorrow(struct colormap *, int);
static void
newhicolorcols(struct colormap *);
static void
subcolorcvec(struct vars *, struct cvec *, struct state *, struct state *);
static void
subcoloronechr(struct vars *, chr, struct state *, struct state *, color *);
static void
subcoloronerange(struct vars *, chr, chr, struct state *, struct state *, color *);
static void
subcoloronerow(struct vars *, int, struct state *, struct state *, color *);
static void
okcolors(struct nfa *, struct colormap *);
static void
colorchain(struct colormap *, struct arc *);
static void
uncolorchain(struct colormap *, struct arc *);
static void
rainbow(struct nfa *, struct colormap *, int, color, struct state *, struct state *);
static void
colorcomplement(struct nfa *, struct colormap *, int, struct state *, struct state *, struct state *);

#ifdef REG_DEBUG
static void
dumpcolors(struct colormap *, FILE *);
static void
dumpchr(chr, FILE *);
#endif
                        
static struct nfa *
newnfa(struct vars *, struct colormap *, struct nfa *);
static void
freenfa(struct nfa *);
static struct state *
newstate(struct nfa *);
static struct state *
newfstate(struct nfa *, int flag);
static void
dropstate(struct nfa *, struct state *);
static void
freestate(struct nfa *, struct state *);
static void
destroystate(struct nfa *, struct state *);
static void
newarc(struct nfa *, int, color, struct state *, struct state *);
static void
createarc(struct nfa *, int, color, struct state *, struct state *);
static struct arc *
allocarc(struct nfa *, struct state *);
static void
freearc(struct nfa *, struct arc *);
static void
changearctarget(struct arc *, struct state *);
static int
hasnonemptyout(struct state *);
static struct arc *
findarc(struct state *, int, color);
static void
cparc(struct nfa *, struct arc *, struct state *, struct state *);
static void
sortins(struct nfa *, struct state *);
static int
sortins_cmp(const void *, const void *);
static void
sortouts(struct nfa *, struct state *);
static int
sortouts_cmp(const void *, const void *);
static void
moveins(struct nfa *, struct state *, struct state *);
static void
copyins(struct nfa *, struct state *, struct state *);
static void
mergeins(struct nfa *, struct state *, struct arc **, int);
static void
moveouts(struct nfa *, struct state *, struct state *);
static void
copyouts(struct nfa *, struct state *, struct state *);
static void
cloneouts(struct nfa *, struct state *, struct state *, struct state *, int);
static void
delsub(struct nfa *, struct state *, struct state *);
static void
deltraverse(struct nfa *, struct state *, struct state *);
static void
dupnfa(struct nfa *, struct state *, struct state *, struct state *, struct state *);
static void
duptraverse(struct nfa *, struct state *, struct state *);
static void
cleartraverse(struct nfa *, struct state *);
static struct state *
single_color_transition(struct state *, struct state *);
static void
specialcolors(struct nfa *);
static long
optimize(struct nfa *, FILE *);
static void
pullback(struct nfa *, FILE *);
static int
pull(struct nfa *, struct arc *, struct state **);
static void
pushfwd(struct nfa *, FILE *);
static int
push(struct nfa *, struct arc *, struct state **);

#define INCOMPATIBLE 1                   
#define SATISFIED 2                              
#define COMPATIBLE 3                                         
static int
combine(struct arc *, struct arc *);
static void
fixempties(struct nfa *, FILE *);
static struct state *
emptyreachable(struct nfa *, struct state *, struct state *, struct arc **);
static int
isconstraintarc(struct arc *);
static int
hasconstraintout(struct state *);
static void
fixconstraintloops(struct nfa *, FILE *);
static int
findconstraintloop(struct nfa *, struct state *);
static void
breakconstraintloop(struct nfa *, struct state *);
static void
clonesuccessorstates(struct nfa *, struct state *, struct state *, struct state *, struct arc *, char *, char *, int);
static void
cleanup(struct nfa *);
static void
markreachable(struct nfa *, struct state *, struct state *, struct state *);
static void
markcanreach(struct nfa *, struct state *, struct state *, struct state *);
static long
analyze(struct nfa *);
static void
compact(struct nfa *, struct cnfa *);
static void
carcsort(struct carc *, size_t);
static int
carc_cmp(const void *, const void *);
static void
freecnfa(struct cnfa *);
static void
dumpnfa(struct nfa *, FILE *);

#ifdef REG_DEBUG
static void
dumpstate(struct state *, FILE *);
static void
dumparcs(struct state *, FILE *);
static void
dumparc(struct arc *, struct state *, FILE *);
static void
dumpcnfa(struct cnfa *, FILE *);
static void
dumpcstate(int, struct cnfa *, FILE *);
#endif
                         
static struct cvec *
newcvec(int, int);
static struct cvec *
clearcvec(struct cvec *);
static void
addchr(struct cvec *, chr);
static void
addrange(struct cvec *, chr, chr);
static struct cvec *
getcvec(struct vars *, int, int);
static void
freecvec(struct cvec *);

                              
static int
pg_wc_isdigit(pg_wchar c);
static int
pg_wc_isalpha(pg_wchar c);
static int
pg_wc_isalnum(pg_wchar c);
static int
pg_wc_isupper(pg_wchar c);
static int
pg_wc_islower(pg_wchar c);
static int
pg_wc_isgraph(pg_wchar c);
static int
pg_wc_isprint(pg_wchar c);
static int
pg_wc_ispunct(pg_wchar c);
static int
pg_wc_isspace(pg_wchar c);
static pg_wchar
pg_wc_toupper(pg_wchar c);
static pg_wchar
pg_wc_tolower(pg_wchar c);

                           
static chr
element(struct vars *, const chr *, const chr *);
static struct cvec *
range(struct vars *, chr, chr, int);
static int before(chr, chr);
static struct cvec *
eclass(struct vars *, chr, int);
static struct cvec *
cclass(struct vars *, const chr *, const chr *, int);
static int
cclass_column_index(struct colormap *, chr);
static struct cvec *
allcases(struct vars *, chr);
static int
cmp(const chr *, const chr *, size_t);
static int
casecmp(const chr *, const chr *, size_t);

                                                         
struct vars
{
  regex_t *re;
  const chr *now;                                   
  const chr *stop;                       
  const chr *savenow;                                               
  const chr *savestop;
  int err;                                             
  int cflags;                                         
  int lasttype;                                        
  int nexttype;                                    
  chr nextvalue;                                             
  int lexcon;                                                    
  int nsubexp;                                      
  struct subre **subs;                               
  size_t nsubs;                                  
  struct subre *sub10[10];                                      
  struct nfa *nfa;                      
  struct colormap *cm;                              
  color nlcolor;                                 
  struct state *wordchrs;                                              
  struct subre *tree;                              
  struct subre *treechain;                               
  struct subre *treefree;                           
  int ntree;                                                   
  struct cvec *cv;                             
  struct cvec *cv2;                          
  struct subre *lacons;                                      
  int nlacons;                                                       
                                                                 
  size_t spaceused;                                                
};

                                                                   
#define NEXT() (next(v))                                      
#define SEE(t) (v->nexttype == (t))                          
#define EAT(t) (SEE(t) && next(v))                                   
#define VISERR(vv) ((vv)->err != 0)                                 
#define ISERR() VISERR(v)
#define VERR(vv, e) ((vv)->nexttype = EOS, (vv)->err = ((vv)->err ? (vv)->err : (e)))
#define ERR(e) VERR(v, e)                      
#define NOERR()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  }                            
#define NOERRN()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  }                        
#define NOERRZ()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  }                        
#define INSIST(c, e)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!(c))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      ERR(e);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)                       
#define NOTE(b) (v->re->re_info |= (b))                             
#define EMPTYARC(x, y) newarc(v->nfa, EMPTY, 0, x, y)

                                                       
#define EMPTY 'n'                         
#define EOS 'e'                        
#define PLAIN 'p'                           
#define DIGIT 'd'                         
#define BACKREF 'b'                     
#define COLLEL 'I'                   
#define ECLASS 'E'                   
#define CCLASS 'C'                   
#define END 'X'                          
#define RANGE 'R'                                                
#define LACON 'L'                                    
#define AHEAD 'a'                            
#define BEHIND 'r'                            
#define WBDRY 'w'                                 
#define NWBDRY 'W'                                    
#define SBEGIN 'A'                                             
#define SEND 'Z'                                         
#define PREFER 'P'                         

                                                    
#define COLORED(a) ((a)->type == PLAIN || (a)->type == AHEAD || (a)->type == BEHIND)

                          
static const struct fns functions = {
    rfree,                                 
    rcancelrequested,                               
    rstacktoodeep                                                   
};

   
                                           
   
                                                                    
                              
   
int
pg_regcomp(regex_t *re, const chr *string, size_t len, int flags, Oid collation)
{
  struct vars var;
  struct vars *v = &var;
  struct guts *g;
  int i;
  size_t j;

#ifdef REG_DEBUG
  FILE *debug = (flags & REG_PROGRESS) ? stdout : (FILE *)NULL;
#else
  FILE *debug = (FILE *)NULL;
#endif

#define CNOERR()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return freev(v, v->err);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  }

                     

  if (re == NULL || string == NULL)
  {
    return REG_INVARG;
  }
  if ((flags & REG_QUOTE) && (flags & (REG_ADVANCED | REG_EXPANDED | REG_NEWLINE)))
  {
    return REG_INVARG;
  }
  if (!(flags & REG_EXTENDED) && (flags & REG_ADVF))
  {
    return REG_INVARG;
  }

                                           
  pg_set_regex_collation(collation);

                                                       
  v->re = re;
  v->now = string;
  v->stop = v->now + len;
  v->savenow = v->savestop = NULL;
  v->err = 0;
  v->cflags = flags;
  v->nsubexp = 0;
  v->subs = v->sub10;
  v->nsubs = 10;
  for (j = 0; j < v->nsubs; j++)
  {
    v->subs[j] = NULL;
  }
  v->nfa = NULL;
  v->cm = NULL;
  v->nlcolor = COLORLESS;
  v->wordchrs = NULL;
  v->tree = NULL;
  v->treechain = NULL;
  v->treefree = NULL;
  v->cv = NULL;
  v->cv2 = NULL;
  v->lacons = NULL;
  v->nlacons = 0;
  v->spaceused = 0;
  re->re_magic = REMAGIC;
  re->re_info = 0;                                
  re->re_csize = sizeof(chr);
  re->re_collation = collation;
  re->re_guts = NULL;
  re->re_fns = VS(&functions);

                                           
  re->re_guts = VS(MALLOC(sizeof(struct guts)));
  if (re->re_guts == NULL)
  {
    return freev(v, REG_ESPACE);
  }
  g = (struct guts *)re->re_guts;
  g->tree = NULL;
  initcm(v, &g->cmap);
  v->cm = &g->cmap;
  g->lacons = NULL;
  g->nlacons = 0;
  ZAPCNFA(g->search);
  v->nfa = newnfa(v, v->cm, (struct nfa *)NULL);
  CNOERR();
                                                                  
  v->cv = newcvec(100, 20);
  if (v->cv == NULL)
  {
    return freev(v, REG_ESPACE);
  }

               
  lexstart(v);                            
  if ((v->cflags & REG_NLSTOP) || (v->cflags & REG_NLANCH))
  {
                                       
    v->nlcolor = subcolor(v->cm, newline());
    okcolors(v->nfa, v->cm);
  }
  CNOERR();
  v->tree = parse(v, EOS, PLAIN, v->nfa->init, v->nfa->final);
  assert(SEE(EOS));                                         
  CNOERR();
  assert(v->tree != NULL);

                                              
  specialcolors(v->nfa);
  CNOERR();
#ifdef REG_DEBUG
  if (debug != NULL)
  {
    fprintf(debug, "\n\n\n========= RAW ==========\n");
    dumpnfa(v->nfa, debug);
    dumpst(v->tree, debug, 1);
  }
#endif
  optst(v, v->tree);
  v->ntree = numst(v->tree, 1);
  markst(v->tree);
  cleanst(v);
#ifdef REG_DEBUG
  if (debug != NULL)
  {
    fprintf(debug, "\n\n\n========= TREE FIXED ==========\n");
    dumpst(v->tree, debug, 1);
  }
#endif

                                                
  re->re_info |= nfatree(v, v->tree, debug);
  CNOERR();
  assert(v->nlacons == 0 || v->lacons != NULL);
  for (i = 1; i < v->nlacons; i++)
  {
    struct subre *lasub = &v->lacons[i];

#ifdef REG_DEBUG
    if (debug != NULL)
    {
      fprintf(debug, "\n\n\n========= LA%d ==========\n", i);
    }
#endif

                                                          
    nfanode(v, lasub, !LATYPE_IS_AHEAD(lasub->subno), debug);
  }
  CNOERR();
  if (v->tree->flags & SHORTER)
  {
    NOTE(REG_USHORTEST);
  }

                                                          
#ifdef REG_DEBUG
  if (debug != NULL)
  {
    fprintf(debug, "\n\n\n========= SEARCH ==========\n");
  }
#endif
                                                          
  (DISCARD) optimize(v->nfa, debug);
  CNOERR();
  makesearch(v, v->nfa);
  CNOERR();
  compact(v->nfa, &g->search);
  CNOERR();

                                 
  re->re_nsub = v->nsubexp;
  v->re = NULL;                               
  g->magic = GUTSMAGIC;
  g->cflags = v->cflags;
  g->info = re->re_info;
  g->nsub = re->re_nsub;
  g->tree = v->tree;
  v->tree = NULL;
  g->ntree = v->ntree;
  g->compare = (v->cflags & REG_ICASE) ? casecmp : cmp;
  g->lacons = v->lacons;
  v->lacons = NULL;
  g->nlacons = v->nlacons;

#ifdef REG_DEBUG
  if (flags & REG_DUMP)
  {
    dump(re, stdout);
  }
#endif

  assert(v->err == 0);
  return freev(v, 0);
}

   
                                   
   
static void
moresubs(struct vars *v, int wanted)                                    
{
  struct subre **p;
  size_t n;

  assert(wanted > 0 && (size_t)wanted >= v->nsubs);
  n = (size_t)wanted * 3 / 2 + 1;

  if (v->subs == v->sub10)
  {
    p = (struct subre **)MALLOC(n * sizeof(struct subre *));
    if (p != NULL)
    {
      memcpy(VS(p), VS(v->subs), v->nsubs * sizeof(struct subre *));
    }
  }
  else
  {
    p = (struct subre **)REALLOC(v->subs, n * sizeof(struct subre *));
  }
  if (p == NULL)
  {
    ERR(REG_ESPACE);
    return;
  }
  v->subs = p;
  for (p = &v->subs[v->nsubs]; v->nsubs < n; p++, v->nsubs++)
  {
    *p = NULL;
  }
  assert(v->nsubs == n);
  assert((size_t)wanted < v->nsubs);
}

   
                                                            
   
                                                                       
                                                 
   
static int
freev(struct vars *v, int err)
{
  if (v->re != NULL)
  {
    rfree(v->re);
  }
  if (v->subs != v->sub10)
  {
    FREE(v->subs);
  }
  if (v->nfa != NULL)
  {
    freenfa(v->nfa);
  }
  if (v->tree != NULL)
  {
    freesubre(v, v->tree);
  }
  if (v->treechain != NULL)
  {
    cleanst(v);
  }
  if (v->cv != NULL)
  {
    freecvec(v->cv);
  }
  if (v->cv2 != NULL)
  {
    freecvec(v->cv2);
  }
  if (v->lacons != NULL)
  {
    freelacons(v->lacons, v->nlacons);
  }
  ERR(err);                    

  return v->err;
}

   
                                                                        
                                           
   
static void
makesearch(struct vars *v, struct nfa *nfa)
{
  struct arc *a;
  struct arc *b;
  struct state *pre = nfa->pre;
  struct state *s;
  struct state *s2;
  struct state *slist;

                                            
  for (a = pre->outs; a != NULL; a = a->outchain)
  {
    assert(a->type == PLAIN);
    if (a->co != nfa->bos[0] && a->co != nfa->bos[1])
    {
      break;
    }
  }
  if (a != NULL)
  {
                                  
    rainbow(nfa, v->cm, PLAIN, COLORLESS, pre, pre);

                                                                  
    newarc(nfa, PLAIN, nfa->bos[0], pre, pre);
    newarc(nfa, PLAIN, nfa->bos[1], pre, pre);
  }

     
                                                                    
                                                                         
                                                                            
                                                                            
                                                                      
                                                                     
     

                                                                         
  slist = NULL;
  for (a = pre->outs; a != NULL; a = a->outchain)
  {
    s = a->to;
    for (b = s->ins; b != NULL; b = b->inchain)
    {
      if (b->from != pre)
      {
        break;
      }
    }

       
                                                                         
                                                                           
                                                                         
                                                     
       
    if (b != NULL && s->tmp == NULL)
    {
      s->tmp = (slist != NULL) ? slist : s;
      slist = s;
    }
  }

                     
  for (s = slist; s != NULL; s = s2)
  {
    s2 = newstate(nfa);
    NOERR();
    copyouts(nfa, s, s2);
    NOERR();
    for (a = s->ins; a != NULL; a = b)
    {
      b = a->inchain;
      if (a->from != pre)
      {
        cparc(nfa, a, a->from, s2);
        freearc(nfa, a);
      }
    }
    s2 = (s->tmp != s) ? s->tmp : NULL;
    s->tmp = NULL;                                 
  }
}

   
                       
   
                                                                         
                                                                         
                             
   
static struct subre *
parse(struct vars *v, int stopper,                 
    int type,                                                             
    struct state *init,                               
    struct state *final)                            
{
  struct state *left;                             
  struct state *right;
  struct subre *branches;                
  struct subre *branch;                       
  struct subre *t;                       
  int firstbranch;                                       

  assert(stopper == ')' || stopper == EOS);

  branches = subre(v, '|', LONGER, init, final);
  NOERRN();
  branch = branches;
  firstbranch = 1;
  do
  {               
    if (!firstbranch)
    {
                                   
      branch->right = subre(v, '|', LONGER, init, final);
      NOERRN();
      branch = branch->right;
    }
    firstbranch = 0;
    left = newstate(v->nfa);
    right = newstate(v->nfa);
    NOERRN();
    EMPTYARC(init, left);
    EMPTYARC(right, final);
    NOERRN();
    branch->left = parsebranch(v, stopper, type, left, right, 0);
    NOERRN();
    branch->flags |= UP(branch->flags | branch->left->flags);
    if ((branch->flags & ~branches->flags) != 0)                
    {
      for (t = branches; t != branch; t = t->right)
      {
        t->flags |= branch->flags;
      }
    }
  } while (EAT('|'));
  assert(SEE(stopper) || SEE(EOS));

  if (!SEE(stopper))
  {
    assert(stopper == ')' && SEE(EOS));
    ERR(REG_EPAREN);
  }

                                 
  if (branch == branches)
  {                      
    assert(branch->right == NULL);
    t = branch->left;
    branch->left = NULL;
    freesubre(v, branches);
    branches = t;
  }
  else if (!MESSY(branches->flags))
  {                             
    freesubre(v, branches->left);
    branches->left = NULL;
    freesubre(v, branches->right);
    branches->right = NULL;
    branches->op = '=';
  }

  return branches;
}

   
                                           
   
                                                                         
                                                                         
                                                                 
   
static struct subre *
parsebranch(struct vars *v, int stopper,                 
    int type,                                                                   
    struct state *left,                                      
    struct state *right,                                      
    int partial)                                                             
{
  struct state *lp;                                    
  int seencontent;                                             
  struct subre *t;

  lp = left;
  seencontent = 0;
  t = subre(v, '=', 0, left, right);                          
  NOERRN();
  while (!SEE('|') && !SEE(stopper) && !SEE(EOS))
  {
    if (seencontent)
    {                               
      lp = newstate(v->nfa);
      NOERRN();
      moveins(v->nfa, right, lp);
    }
    seencontent = 1;

                                                                  
    parseqatom(v, stopper, type, lp, right, t);
    NOERRN();
  }

  if (!seencontent)
  {                   
    if (!partial)
    {
      NOTE(REG_UUNSPEC);
    }
    assert(lp == left);
    EMPTYARC(left, right);
  }

  return t;
}

   
                                                                 
   
                                                                            
                                                                            
                                                                   
   
static void
parseqatom(struct vars *v, int stopper,                 
    int type,                                                                  
    struct state *lp,                                                 
    struct state *rp,                                                  
    struct subre *top)                                   
{
  struct state *s;                                 
  struct state *s2;

#define ARCV(t, val) newarc(v->nfa, t, val, lp, rp)
  int m, n;
  struct subre *atom;                     
  struct subre *t;
  int cap;                           
  int latype;                                 
  int subno;                                          
  int atomtype;
  int qprefer;                                       
  int f;
  struct subre **atomp;                                   

                           
  atom = NULL;
  assert(lp->nouts == 0);                           
  assert(rp->nins == 0);                         
  subno = 0;                                        

                                
  atomtype = v->nexttype;
  switch (atomtype)
  {
                                                    
  case '^':
    ARCV('^', 1);
    if (v->cflags & REG_NLANCH)
    {
      ARCV(BEHIND, v->nlcolor);
    }
    NEXT();
    return;
    break;
  case '$':
    ARCV('$', 1);
    if (v->cflags & REG_NLANCH)
    {
      ARCV(AHEAD, v->nlcolor);
    }
    NEXT();
    return;
    break;
  case SBEGIN:
    ARCV('^', 1);          
    ARCV('^', 0);             
    NEXT();
    return;
    break;
  case SEND:
    ARCV('$', 1);          
    ARCV('$', 0);             
    NEXT();
    return;
    break;
  case '<':
    wordchrs(v);                  
    s = newstate(v->nfa);
    NOERR();
    nonword(v, BEHIND, lp, s);
    word(v, AHEAD, s, rp);
    return;
    break;
  case '>':
    wordchrs(v);                  
    s = newstate(v->nfa);
    NOERR();
    word(v, BEHIND, lp, s);
    nonword(v, AHEAD, s, rp);
    return;
    break;
  case WBDRY:
    wordchrs(v);                  
    s = newstate(v->nfa);
    NOERR();
    nonword(v, BEHIND, lp, s);
    word(v, AHEAD, s, rp);
    s = newstate(v->nfa);
    NOERR();
    word(v, BEHIND, lp, s);
    nonword(v, AHEAD, s, rp);
    return;
    break;
  case NWBDRY:
    wordchrs(v);                  
    s = newstate(v->nfa);
    NOERR();
    word(v, BEHIND, lp, s);
    word(v, AHEAD, s, rp);
    s = newstate(v->nfa);
    NOERR();
    nonword(v, BEHIND, lp, s);
    nonword(v, AHEAD, s, rp);
    return;
    break;
  case LACON:                            
    latype = v->nextvalue;
    NEXT();
    s = newstate(v->nfa);
    s2 = newstate(v->nfa);
    NOERR();
    t = parse(v, ')', LACON, s, s2);
    freesubre(v, t);                                    
    NOERR();
    assert(SEE(')'));
    NEXT();
    processlacon(v, s, s2, latype, lp, rp);
    return;
    break;
                                                 
  case '*':
  case '+':
  case '?':
  case '{':
    ERR(REG_BADRPT);
    return;
    break;
  default:
    ERR(REG_ASSERT);
    return;
    break;
                                                                 
  case ')':                       
    if ((v->cflags & REG_ADVANCED) != REG_EXTENDED)
    {
      ERR(REG_EPAREN);
      return;
    }
                                                  
    NOTE(REG_UPBOTCH);
                                      
                     
  case PLAIN:
    onechr(v, v->nextvalue, lp, rp);
    okcolors(v->nfa, v->cm);
    NOERR();
    NEXT();
    break;
  case '[':
    if (v->nextvalue == 1)
    {
      bracket(v, lp, rp);
    }
    else
    {
      cbracket(v, lp, rp);
    }
    assert(SEE(']') || ISERR());
    NEXT();
    break;
  case '.':
    rainbow(v->nfa, v->cm, PLAIN, (v->cflags & REG_NLSTOP) ? v->nlcolor : COLORLESS, lp, rp);
    NEXT();
    break;
                                    
  case '(':                                      
    cap = (type == LACON) ? 0 : v->nextvalue;
    if (cap)
    {
      v->nsubexp++;
      subno = v->nsubexp;
      if ((size_t)subno >= v->nsubs)
      {
        moresubs(v, subno);
      }
      assert((size_t)subno < v->nsubs);
    }
    else
    {
      atomtype = PLAIN;                               
    }
    NEXT();
                                                               
    s = newstate(v->nfa);
    s2 = newstate(v->nfa);
    NOERR();
    EMPTYARC(lp, s);
    EMPTYARC(s2, rp);
    NOERR();
    atom = parse(v, ')', type, s, s2);
    assert(SEE(')') || ISERR());
    NEXT();
    NOERR();
    if (cap)
    {
      v->subs[subno] = atom;
      t = subre(v, '(', atom->flags | CAP, s, s2);
      NOERR();
      t->subno = subno;
      t->left = atom;
      atom = t;
    }
                                                       
    break;
  case BACKREF:                                        
    INSIST(type != LACON, REG_ESUBREG);
    INSIST(v->nextvalue < v->nsubs, REG_ESUBREG);
    INSIST(v->subs[v->nextvalue] != NULL, REG_ESUBREG);
    NOERR();
    assert(v->nextvalue > 0);
    atom = subre(v, 'b', BACKR, lp, rp);
    NOERR();
    subno = v->nextvalue;
    atom->subno = subno;
    EMPTYARC(lp, rp);                                        
    NEXT();
    break;
  }

                                                      
  switch (v->nexttype)
  {
  case '*':
    m = 0;
    n = DUPINF;
    qprefer = (v->nextvalue) ? LONGER : SHORTER;
    NEXT();
    break;
  case '+':
    m = 1;
    n = DUPINF;
    qprefer = (v->nextvalue) ? LONGER : SHORTER;
    NEXT();
    break;
  case '?':
    m = 0;
    n = 1;
    qprefer = (v->nextvalue) ? LONGER : SHORTER;
    NEXT();
    break;
  case '{':
    NEXT();
    m = scannum(v);
    if (EAT(','))
    {
      if (SEE(DIGIT))
      {
        n = scannum(v);
      }
      else
      {
        n = DUPINF;
      }
      if (m > n)
      {
        ERR(REG_BADBR);
        return;
      }
                                                          
      qprefer = (v->nextvalue) ? LONGER : SHORTER;
    }
    else
    {
      n = m;
                                                   
      qprefer = 0;
    }
    if (!SEE('}'))
    {                         
      ERR(REG_BADBR);
      return;
    }
    NEXT();
    break;
  default:                    
    m = n = 1;
    qprefer = 0;
    break;
  }

                                                               
  if (m == 0 && n == 0)
  {
       
                                                                           
                                                                         
                                                                          
       
    if (atom != NULL && (atom->flags & CAP))
    {
      delsub(v->nfa, lp, atom->begin);
      delsub(v->nfa, atom->end, rp);
    }
    else
    {
                                                                       
      if (atom != NULL)
      {
        freesubre(v, atom);
      }
      delsub(v->nfa, lp, rp);
    }
    EMPTYARC(lp, rp);
    return;
  }

                                            
  assert(!MESSY(top->flags));
  f = top->flags | qprefer | ((atom != NULL) ? atom->flags : 0);
  if (atomtype != '(' && atomtype != BACKREF && !MESSY(UP(f)))
  {
    if (!(m == 1 && n == 1))
    {
      repeat(v, lp, rp, m, n);
    }
    if (atom != NULL)
    {
      freesubre(v, atom);
    }
    top->flags = f;
    return;
  }

     
                                 
     
                                                                             
                                                
     

                                                                      
  if (atom == NULL)
  {
    atom = subre(v, '=', 0, lp, rp);
    NOERR();
  }

               
                                               
     
                                            
     
                                                                            
     
                                                                            
     
                                                         
     
                                                               
     
                                                                         
               
     
  s = newstate(v->nfa);                                        
  s2 = newstate(v->nfa);
  NOERR();
  moveouts(v->nfa, lp, s);
  moveins(v->nfa, rp, s2);
  NOERR();
  atom->begin = s;
  atom->end = s2;
  s = newstate(v->nfa);                            
  NOERR();
  EMPTYARC(lp, s);
  NOERR();

                                                          
  t = subre(v, '.', COMBINE(qprefer, atom->flags), lp, rp);
  NOERR();
  t->left = atom;
  atomp = &t->left;

                                                                      

                                           
  assert(top->op == '=' && top->left == NULL && top->right == NULL);
  top->left = subre(v, '=', top->flags, top->begin, lp);
  NOERR();
  top->op = '.';
  top->right = t;

                                                                  
  if (atomtype == BACKREF)
  {
    assert(atom->begin->nouts == 1);                     
    delsub(v->nfa, atom->begin, atom->end);
    assert(v->subs[subno] != NULL);

       
                                                                          
                                                                         
                                    
       
    dupnfa(v->nfa, v->subs[subno]->begin, v->subs[subno]->end, atom->begin, atom->end);
    NOERR();
  }

     
                                                                             
                                  
     
  if (atomtype == BACKREF)
  {
                                                           
    EMPTYARC(s, atom->begin);                   
                                         
    repeat(v, atom->begin, atom->end, m, n);
    atom->min = (short)m;
    atom->max = (short)n;
    atom->flags |= COMBINE(qprefer, atom->flags);
                                                              
    s2 = atom->end;
  }
  else if (m == 1 && n == 1 && (qprefer == 0 || (atom->flags & (LONGER | SHORTER | MIXED)) == 0 || qprefer == (atom->flags & (LONGER | SHORTER | MIXED))))
  {
                                      
    EMPTYARC(s, atom->begin);                   
                                                              
    s2 = atom->end;
  }
  else if (m > 0 && !(atom->flags & BACKR))
  {
       
                                                                
                                                                         
                                                                         
                                                                         
                                                                      
                                             
       
    dupnfa(v->nfa, atom->begin, atom->end, s, atom->begin);
    assert(m >= 1 && m != DUPINF && n >= 1);
    repeat(v, s, atom->begin, m - 1, (n == DUPINF) ? n : n - 1);
    f = COMBINE(qprefer, atom->flags);
    t = subre(v, '.', f, s, atom->end);                      
    NOERR();
    t->left = subre(v, '=', PREF(f), s, atom->begin);
    NOERR();
    t->right = atom;
    *atomp = t;
                                                              
    s2 = atom->end;
  }
  else
  {
                                              
    s2 = newstate(v->nfa);
    NOERR();
    moveouts(v->nfa, atom->end, s2);
    NOERR();
    dupnfa(v->nfa, atom->begin, atom->end, s, s2);
    repeat(v, s, s2, m, n);
    f = COMBINE(qprefer, atom->flags);
    t = subre(v, '*', f, s, s2);
    NOERR();
    t->min = (short)m;
    t->max = (short)n;
    t->left = atom;
    *atomp = t;
                                                                   
  }

                                                        
  t = top->right;
  if (!(SEE('|') || SEE(stopper) || SEE(EOS)))
  {
    t->right = parsebranch(v, stopper, type, s2, rp, 1);
  }
  else
  {
    EMPTYARC(s2, rp);
    t->right = subre(v, '=', 0, s2, rp);
  }
  NOERR();
  assert(SEE('|') || SEE(stopper) || SEE(EOS));
  t->flags |= COMBINE(t->flags, t->right->flags);
  top->flags |= COMBINE(top->flags, t->flags);
}

   
                                                                  
   
static void
nonword(struct vars *v, int dir,                      
    struct state *lp, struct state *rp)
{
  int anchor = (dir == AHEAD) ? '$' : '^';

  assert(dir == AHEAD || dir == BEHIND);
  newarc(v->nfa, anchor, 1, lp, rp);
  newarc(v->nfa, anchor, 0, lp, rp);
  colorcomplement(v->nfa, v->cm, dir, v->wordchrs, lp, rp);
                                             
}

   
                                                           
   
static void
word(struct vars *v, int dir,                      
    struct state *lp, struct state *rp)
{
  assert(dir == AHEAD || dir == BEHIND);
  cloneouts(v->nfa, v->wordchrs, lp, rp, dir);
                                             
}

   
                           
   
static int                       
scannum(struct vars *v)
{
  int n = 0;

  while (SEE(DIGIT) && n < DUPMAX)
  {
    n = n * 10 + v->nextvalue;
    NEXT();
  }
  if (SEE(DIGIT) || n > DUPMAX)
  {
    ERR(REG_BADBR);
    return 0;
  }
  return n;
}

   
                                             
   
                                                                    
                                        
   
                                                                        
                                                                              
                                                                          
                                                                           
                                                                          
                                                                         
   
static void
repeat(struct vars *v, struct state *lp, struct state *rp, int m, int n)
{
#define SOME 2
#define INF 3
#define PAIR(x, y) ((x) * 4 + (y))
#define REDUCE(x) (((x) == DUPINF) ? INF : (((x) > 1) ? SOME : (x)))
  const int rm = REDUCE(m);
  const int rn = REDUCE(n);
  struct state *s;
  struct state *s2;

  switch (PAIR(rm, rn))
  {
  case PAIR(0, 0):                   
    delsub(v->nfa, lp, rp);
    EMPTYARC(lp, rp);
    break;
  case PAIR(0, 1):               
    EMPTYARC(lp, rp);
    break;
  case PAIR(0, SOME):                    
    repeat(v, lp, rp, 1, n);
    NOERR();
    EMPTYARC(lp, rp);
    break;
  case PAIR(0, INF):                    
    s = newstate(v->nfa);
    NOERR();
    moveouts(v->nfa, lp, s);
    moveins(v->nfa, rp, s);
    EMPTYARC(lp, s);
    EMPTYARC(s, rp);
    break;
  case PAIR(1, 1):                         
    break;
  case PAIR(1, SOME):                                     
    s = newstate(v->nfa);
    NOERR();
    moveouts(v->nfa, lp, s);
    dupnfa(v->nfa, s, rp, lp, s);
    NOERR();
    repeat(v, lp, s, 1, n - 1);
    NOERR();
    EMPTYARC(lp, s);
    break;
  case PAIR(1, INF):                       
    s = newstate(v->nfa);
    s2 = newstate(v->nfa);
    NOERR();
    moveouts(v->nfa, lp, s);
    moveins(v->nfa, rp, s2);
    EMPTYARC(lp, s);
    EMPTYARC(s2, rp);
    EMPTYARC(s2, s);
    break;
  case PAIR(SOME, SOME):                        
    s = newstate(v->nfa);
    NOERR();
    moveouts(v->nfa, lp, s);
    dupnfa(v->nfa, s, rp, lp, s);
    NOERR();
    repeat(v, lp, s, m - 1, n - 1);
    break;
  case PAIR(SOME, INF):                     
    s = newstate(v->nfa);
    NOERR();
    moveouts(v->nfa, lp, s);
    dupnfa(v->nfa, s, rp, lp, s);
    NOERR();
    repeat(v, lp, s, m - 1, n);
    break;
  default:
    ERR(REG_ASSERT);
    break;
  }
}

   
                                                        
                                                                   
   
static void
bracket(struct vars *v, struct state *lp, struct state *rp)
{
  assert(SEE('['));
  NEXT();
  while (!SEE(']') && !SEE(EOS))
  {
    brackpart(v, lp, rp);
  }
  assert(SEE(']') || ISERR());
  okcolors(v->nfa, v->cm);
}

   
                                                     
                                                                              
                                                                              
                                                    
   
static void
cbracket(struct vars *v, struct state *lp, struct state *rp)
{
  struct state *left = newstate(v->nfa);
  struct state *right = newstate(v->nfa);

  NOERR();
  bracket(v, left, right);
  if (v->cflags & REG_NLSTOP)
  {
    newarc(v->nfa, PLAIN, v->nlcolor, left, right);
  }
  NOERR();

  assert(lp->nouts == 0);                               

     
                                                                            
                  
     
  colorcomplement(v->nfa, v->cm, PLAIN, left, lp, rp);
  NOERR();
  dropstate(v->nfa, left);
  assert(right->nins == 0);
  freestate(v->nfa, right);
}

   
                                                                      
   
static void
brackpart(struct vars *v, struct state *lp, struct state *rp)
{
  chr startc;
  chr endc;
  struct cvec *cv;
  const chr *startp;
  const chr *endp;
  chr c[1];

                                                                 
  switch (v->nexttype)
  {
  case RANGE:                           
    ERR(REG_ERANGE);
    return;
    break;
  case PLAIN:
    c[0] = v->nextvalue;
    NEXT();
                                               
    if (!SEE(RANGE))
    {
      onechr(v, c[0], lp, rp);
      return;
    }
    startc = element(v, c, c + 1);
    NOERR();
    break;
  case COLLEL:
    startp = v->now;
    endp = scanplain(v);
    INSIST(startp < endp, REG_ECOLLATE);
    NOERR();
    startc = element(v, startp, endp);
    NOERR();
    break;
  case ECLASS:
    startp = v->now;
    endp = scanplain(v);
    INSIST(startp < endp, REG_ECOLLATE);
    NOERR();
    startc = element(v, startp, endp);
    NOERR();
    cv = eclass(v, startc, (v->cflags & REG_ICASE));
    NOERR();
    subcolorcvec(v, cv, lp, rp);
    return;
    break;
  case CCLASS:
    startp = v->now;
    endp = scanplain(v);
    INSIST(startp < endp, REG_ECTYPE);
    NOERR();
    cv = cclass(v, startp, endp, (v->cflags & REG_ICASE));
    NOERR();
    subcolorcvec(v, cv, lp, rp);
    return;
    break;
  default:
    ERR(REG_ASSERT);
    return;
    break;
  }

  if (SEE(RANGE))
  {
    NEXT();
    switch (v->nexttype)
    {
    case PLAIN:
    case RANGE:
      c[0] = v->nextvalue;
      NEXT();
      endc = element(v, c, c + 1);
      NOERR();
      break;
    case COLLEL:
      startp = v->now;
      endp = scanplain(v);
      INSIST(startp < endp, REG_ECOLLATE);
      NOERR();
      endc = element(v, startp, endp);
      NOERR();
      break;
    default:
      ERR(REG_ERANGE);
      return;
      break;
    }
  }
  else
  {
    endc = startc;
  }

     
                                                                             
                                                                           
     
  if (startc != endc)
  {
    NOTE(REG_UUNPORT);
  }
  cv = range(v, startc, endc, (v->cflags & REG_ICASE));
  NOERR();
  subcolorcvec(v, cv, lp, rp);
}

   
                                              
   
                                                                      
                                                 
   
static const chr *                                 
scanplain(struct vars *v)
{
  const chr *endp;

  assert(SEE(COLLEL) || SEE(ECLASS) || SEE(CCLASS));
  NEXT();

  endp = v->now;
  while (SEE(PLAIN))
  {
    endp = v->now;
    NEXT();
  }

  assert(SEE(END) || ISERR());
  NEXT();

  return endp;
}

   
                                                                              
                                                                        
   
static void
onechr(struct vars *v, chr c, struct state *lp, struct state *rp)
{
  if (!(v->cflags & REG_ICASE))
  {
    color lastsubcolor = COLORLESS;

    subcoloronechr(v, c, lp, rp, &lastsubcolor);
    return;
  }

                                         
  subcolorcvec(v, allcases(v, c), lp, rp);
}

   
                                                                      
   
                                                                      
                                                                    
                                                                      
                                                                     
                                                                  
   
static void
wordchrs(struct vars *v)
{
  struct state *left;
  struct state *right;

  if (v->wordchrs != NULL)
  {
    NEXT();                      
    return;
  }

  left = newstate(v->nfa);
  right = newstate(v->nfa);
  NOERR();
                                                                         
  lexword(v);
  NEXT();
  assert(v->savenow != NULL && SEE('['));
  bracket(v, left, right);
  assert((v->savenow != NULL && SEE(']')) || ISERR());
  NEXT();
  NOERR();
  v->wordchrs = left;
}

   
                                                             
   
                                                                          
                     
   
static void
processlacon(struct vars *v, struct state *begin,                                   
    struct state *end,                                                            
    int latype, struct state *lp,                                               
    struct state *rp)                                                            
{
  struct state *s1;
  int n;

     
                                                                            
                                                                             
     
  s1 = single_color_transition(begin, end);
  switch (latype)
  {
  case LATYPE_AHEAD_POS:
                                                                 
    if (s1 != NULL)
    {
      cloneouts(v->nfa, s1, lp, rp, AHEAD);
      return;
    }
    break;
  case LATYPE_AHEAD_NEG:
                                                                    
    if (s1 != NULL)
    {
      colorcomplement(v->nfa, v->cm, AHEAD, s1, lp, rp);
      newarc(v->nfa, '$', 1, lp, rp);
      newarc(v->nfa, '$', 0, lp, rp);
      return;
    }
    break;
  case LATYPE_BEHIND_POS:
                                                                   
    if (s1 != NULL)
    {
      cloneouts(v->nfa, s1, lp, rp, BEHIND);
      return;
    }
    break;
  case LATYPE_BEHIND_NEG:
                                                                      
    if (s1 != NULL)
    {
      colorcomplement(v->nfa, v->cm, BEHIND, s1, lp, rp);
      newarc(v->nfa, '^', 1, lp, rp);
      newarc(v->nfa, '^', 0, lp, rp);
      return;
    }
    break;
  default:
    assert(NOTREACHED);
  }

                                                   
  n = newlacon(v, begin, end, latype);
  newarc(v->nfa, LACON, n, lp, rp);
}

   
                            
   
static struct subre *
subre(struct vars *v, int op, int flags, struct state *begin, struct state *end)
{
  struct subre *ret = v->treefree;

     
                                                                           
                                
     
  if (STACK_TOO_DEEP(v->re))
  {
    ERR(REG_ETOOBIG);
    return NULL;
  }

  if (ret != NULL)
  {
    v->treefree = ret->left;
  }
  else
  {
    ret = (struct subre *)MALLOC(sizeof(struct subre));
    if (ret == NULL)
    {
      ERR(REG_ESPACE);
      return NULL;
    }
    ret->chain = v->treechain;
    v->treechain = ret;
  }

  assert(strchr("=b|.*(", op) != NULL);

  ret->op = op;
  ret->flags = flags;
  ret->id = 0;                             
  ret->subno = 0;
  ret->min = ret->max = 1;
  ret->left = NULL;
  ret->right = NULL;
  ret->begin = begin;
  ret->end = end;
  ZAPCNFA(ret->cnfa);

  return ret;
}

   
                                    
   
static void
freesubre(struct vars *v,                    
    struct subre *sr)
{
  if (sr == NULL)
  {
    return;
  }

  if (sr->left != NULL)
  {
    freesubre(v, sr->left);
  }
  if (sr->right != NULL)
  {
    freesubre(v, sr->right);
  }

  freesrnode(v, sr);
}

   
                                                 
   
static void
freesrnode(struct vars *v,                    
    struct subre *sr)
{
  if (sr == NULL)
  {
    return;
  }

  if (!NULLCNFA(sr->cnfa))
  {
    freecnfa(&sr->cnfa);
  }
  sr->flags = 0;

  if (v != NULL && v->treechain != NULL)
  {
                                                           
    sr->left = v->treefree;
    v->treefree = sr;
  }
  else
  {
    FREE(sr);
  }
}

   
                                    
   
static void
optst(struct vars *v, struct subre *t)
{
     
                                                                             
                                                                           
                                                                         
                               
     
  return;
}

   
                                                      
   
static int                                         
numst(struct subre *t, int start)                                         
{
  int i;

  assert(t != NULL);

  i = start;
  t->id = (short)i++;
  if (t->left != NULL)
  {
    i = numst(t->left, i);
  }
  if (t->right != NULL)
  {
    i = numst(t->right, i);
  }
  return i;
}

   
                                     
   
                                                                         
                                                                      
                                                                             
                                                                               
                                                                             
                                                                              
                                                                             
                                                                             
                                                                             
                                                                             
                                                                           
                                                                         
                                      
   
static void
markst(struct subre *t)
{
  assert(t != NULL);

  t->flags |= INUSE;
  if (t->left != NULL)
  {
    markst(t->left);
  }
  if (t->right != NULL)
  {
    markst(t->right);
  }
}

   
                                                  
   
static void
cleanst(struct vars *v)
{
  struct subre *t;
  struct subre *next;

  for (t = v->treechain; t != NULL; t = next)
  {
    next = t->chain;
    if (!(t->flags & INUSE))
    {
      FREE(t);
    }
  }
  v->treechain = NULL;
  v->treefree = NULL;                                 
}

   
                                                                
   
static long                                                                           
nfatree(struct vars *v, struct subre *t, FILE *f)                       
{
  assert(t != NULL && t->begin != NULL);

  if (t->left != NULL)
  {
    (DISCARD) nfatree(v, t->left, f);
  }
  if (t->right != NULL)
  {
    (DISCARD) nfatree(v, t->right, f);
  }

  return nfanode(v, t, 0, f);
}

   
                                              
   
                                                              
   
static long                                                                                  
nfanode(struct vars *v, struct subre *t, int converttosearch, FILE *f)                       
{
  struct nfa *nfa;
  long ret = 0;

  assert(t->begin != NULL);

#ifdef REG_DEBUG
  if (f != NULL)
  {
    char idbuf[50];

    fprintf(f, "\n\n\n========= TREE NODE %s ==========\n", stid(t, idbuf, sizeof(idbuf)));
  }
#endif
  nfa = newnfa(v, v->cm, v->nfa);
  NOERRZ();
  dupnfa(nfa, t->begin, t->end, nfa->init, nfa->final);
  if (!ISERR())
  {
    specialcolors(nfa);
  }
  if (!ISERR())
  {
    ret = optimize(nfa, f);
  }
  if (converttosearch && !ISERR())
  {
    makesearch(v, nfa);
  }
  if (!ISERR())
  {
    compact(nfa, &t->cnfa);
  }

  freenfa(nfa);
  return ret;
}

   
                                                     
   
static int                   
newlacon(struct vars *v, struct state *begin, struct state *end, int latype)
{
  int n;
  struct subre *newlacons;
  struct subre *sub;

  if (v->nlacons == 0)
  {
    n = 1;               
    newlacons = (struct subre *)MALLOC(2 * sizeof(struct subre));
  }
  else
  {
    n = v->nlacons;
    newlacons = (struct subre *)REALLOC(v->lacons, (n + 1) * sizeof(struct subre));
  }
  if (newlacons == NULL)
  {
    ERR(REG_ESPACE);
    return 0;
  }
  v->lacons = newlacons;
  v->nlacons = n + 1;
  sub = &v->lacons[n];
  sub->begin = begin;
  sub->end = end;
  sub->subno = latype;
  ZAPCNFA(sub->cnfa);
  return n;
}

   
                                                        
   
static void
freelacons(struct subre *subs, int n)
{
  struct subre *sub;
  int i;

  assert(n > 0);
  for (sub = subs + 1, i = n - 1; i > 0; sub++, i--)             
  {
    if (!NULLCNFA(sub->cnfa))
    {
      freecnfa(&sub->cnfa);
    }
  }
  FREE(subs);
}

   
                                                
   
static void
rfree(regex_t *re)
{
  struct guts *g;

  if (re == NULL || re->re_magic != REMAGIC)
  {
    return;
  }

  re->re_magic = 0;                    
  g = (struct guts *)re->re_guts;
  re->re_guts = NULL;
  re->re_fns = NULL;
  if (g != NULL)
  {
    g->magic = 0;
    freecm(&g->cmap);
    if (g->tree != NULL)
    {
      freesubre((struct vars *)NULL, g->tree);
    }
    if (g->lacons != NULL)
    {
      freelacons(g->lacons, g->nlacons);
    }
    if (!NULLCNFA(g->search))
    {
      freecnfa(&g->search);
    }
    FREE(g);
  }
}

   
                                                                           
   
                                                                    
                      
   
                                                                           
                                                                            
                                                                           
   
static int
rcancelrequested(void)
{
  return InterruptPending && (QueryCancelPending || ProcDiePending);
}

   
                                                            
   
                                                                     
                      
   
                                                                           
                                                                            
                                                                           
   
static int
rstacktoodeep(void)
{
  return stack_is_too_deep();
}

#ifdef REG_DEBUG

   
                                            
   
static void
dump(regex_t *re, FILE *f)
{
  struct guts *g;
  int i;

  if (re->re_magic != REMAGIC)
  {
    fprintf(f, "bad magic number (0x%x not 0x%x)\n", re->re_magic, REMAGIC);
  }
  if (re->re_guts == NULL)
  {
    fprintf(f, "NULL guts!!!\n");
    return;
  }
  g = (struct guts *)re->re_guts;
  if (g->magic != GUTSMAGIC)
  {
    fprintf(f, "bad guts magic number (0x%x not 0x%x)\n", g->magic, GUTSMAGIC);
  }

  fprintf(f, "\n\n\n========= DUMP ==========\n");
  fprintf(f, "nsub %d, info 0%lo, csize %d, ntree %d\n", (int)re->re_nsub, re->re_info, re->re_csize, g->ntree);

  dumpcolors(&g->cmap, f);
  if (!NULLCNFA(g->search))
  {
    fprintf(f, "\nsearch:\n");
    dumpcnfa(&g->search, f);
  }
  for (i = 1; i < g->nlacons; i++)
  {
    struct subre *lasub = &g->lacons[i];
    const char *latype;

    switch (lasub->subno)
    {
    case LATYPE_AHEAD_POS:
      latype = "positive lookahead";
      break;
    case LATYPE_AHEAD_NEG:
      latype = "negative lookahead";
      break;
    case LATYPE_BEHIND_POS:
      latype = "positive lookbehind";
      break;
    case LATYPE_BEHIND_NEG:
      latype = "negative lookbehind";
      break;
    default:
      latype = "???";
      break;
    }
    fprintf(f, "\nla%d (%s):\n", i, latype);
    dumpcnfa(&lasub->cnfa, f);
  }
  fprintf(f, "\n");
  dumpst(g->tree, f, 0);
}

   
                              
   
static void
dumpst(struct subre *t, FILE *f, int nfapresent)                                        
{
  if (t == NULL)
  {
    fprintf(f, "null tree\n");
  }
  else
  {
    stdump(t, f, nfapresent);
  }
  fflush(f);
}

   
                                     
   
static void
stdump(struct subre *t, FILE *f, int nfapresent)                                        
{
  char idbuf[50];

  fprintf(f, "%s. `%c'", stid(t, idbuf, sizeof(idbuf)), t->op);
  if (t->flags & LONGER)
  {
    fprintf(f, " longest");
  }
  if (t->flags & SHORTER)
  {
    fprintf(f, " shortest");
  }
  if (t->flags & MIXED)
  {
    fprintf(f, " hasmixed");
  }
  if (t->flags & CAP)
  {
    fprintf(f, " hascapture");
  }
  if (t->flags & BACKR)
  {
    fprintf(f, " hasbackref");
  }
  if (!(t->flags & INUSE))
  {
    fprintf(f, " UNUSED");
  }
  if (t->subno != 0)
  {
    fprintf(f, " (#%d)", t->subno);
  }
  if (t->min != 1 || t->max != 1)
  {
    fprintf(f, " {%d,", t->min);
    if (t->max != DUPINF)
    {
      fprintf(f, "%d", t->max);
    }
    fprintf(f, "}");
  }
  if (nfapresent)
  {
    fprintf(f, " %ld-%ld", (long)t->begin->no, (long)t->end->no);
  }
  if (t->left != NULL)
  {
    fprintf(f, " L:%s", stid(t->left, idbuf, sizeof(idbuf)));
  }
  if (t->right != NULL)
  {
    fprintf(f, " R:%s", stid(t->right, idbuf, sizeof(idbuf)));
  }
  if (!NULLCNFA(t->cnfa))
  {
    fprintf(f, "\n");
    dumpcnfa(&t->cnfa, f);
  }
  fprintf(f, "\n");
  if (t->left != NULL)
  {
    stdump(t->left, f, nfapresent);
  }
  if (t->right != NULL)
  {
    stdump(t->right, f, nfapresent);
  }
}

   
                                              
   
static const char *                                       
stid(struct subre *t, char *buf, size_t bufsize)
{
                                                
  if (bufsize < sizeof(void *) * 2 + 3 || bufsize < sizeof(t->id) * 3 + 1)
  {
    return "unable";
  }
  if (t->id != 0)
  {
    sprintf(buf, "%d", t->id);
  }
  else
  {
    sprintf(buf, "%p", t);
  }
  return buf;
}
#endif                

#include "regc_lex.c"
#include "regc_color.c"
#include "regc_nfa.c"
#include "regc_cvec.c"
#include "regc_pg_locale.c"
#include "regc_locale.c"
