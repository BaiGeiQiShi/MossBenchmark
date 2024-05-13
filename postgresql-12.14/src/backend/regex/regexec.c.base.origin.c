/*
 * re_*exec and friends - match REs
 *
 * Copyright (c) 1998, 1999 Henry Spencer.  All rights reserved.
 *
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., Sun Microsystems Inc., and Scriptics
 * Corporation, none of whom are responsible for the results.  The author
 * thanks all of them.
 *
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications.
 *
 * I'd appreciate being given credit for this package in the documentation
 * of software which uses it, but that is not a requirement.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * HENRY SPENCER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * src/backend/regex/regexec.c
 *
 */

#include "regex/regguts.h"

/* lazy-DFA representation */
struct arcp
{ /* "pointer" to an outarc */
  struct sset *ss;
  color co;
};

struct sset
{                   /* state set */
  unsigned *states; /* pointer to bitvector */
  unsigned hash;    /* hash of bitvector */
#define HASH(bv, nw) (((nw) == 1) ? *(bv) : hash(bv, nw))
#define HIT(h, bv, ss, nw) ((ss)->hash == (h) && ((nw) == 1 || memcmp(VS(bv), VS((ss)->states), (nw) * sizeof(unsigned)) == 0))
  int flags;
#define STARTER 01      /* the initial state set */
#define POSTSTATE 02    /* includes the goal state */
#define LOCKED 04       /* locked in cache */
#define NOPROGRESS 010  /* zero-progress state set */
  struct arcp ins;      /* chain of inarcs pointing here */
  chr *lastseen;        /* last entered on arrival here */
  struct sset **outs;   /* outarc vector indexed by color */
  struct arcp *inchain; /* chain-pointer vector for outarcs */
};

struct dfa
{
  int nssets;             /* size of cache */
  int nssused;            /* how many entries occupied yet */
  int nstates;            /* number of states */
  int ncolors;            /* length of outarc and inchain vectors */
  int wordsper;           /* length of state-set bitvectors */
  struct sset *ssets;     /* state-set cache */
  unsigned *statesarea;   /* bitvector storage */
  unsigned *work;         /* pointer to work area within statesarea */
  struct sset **outsarea; /* outarc-vector storage */
  struct arcp *incarea;   /* inchain storage */
  struct cnfa *cnfa;
  struct colormap *cm;
  chr *lastpost;       /* location of last cache-flushed success */
  chr *lastnopr;       /* location of last cache-flushed NOPROGRESS */
  struct sset *search; /* replacement-search-pointer memory */
  int cptsmalloced;    /* were the areas individually malloced? */
  char *mallocarea;    /* self, or master malloced area, or NULL */
};

#define WORK 1 /* number of work bitvectors needed */

/* setup for non-malloc allocation for small cases */
#define FEWSTATES 20 /* must be less than UBITS */
#define FEWCOLORS 15
struct smalldfa
{
  struct dfa dfa;
  struct sset ssets[FEWSTATES * 2];
  unsigned statesarea[FEWSTATES * 2 + WORK];
  struct sset *outsarea[FEWSTATES * 2 * FEWCOLORS];
  struct arcp incarea[FEWSTATES * 2 * FEWCOLORS];
};

#define DOMALLOC ((struct smalldfa *)NULL) /* force malloc */

/* internal variables, bundled for easy passing around */
struct vars
{
  regex_t *re;
  struct guts *g;
  int eflags; /* copies of arguments */
  size_t nmatch;
  regmatch_t *pmatch;
  rm_detail_t *details;
  chr *start;              /* start of string */
  chr *search_start;       /* search start of string */
  chr *stop;               /* just past end of string */
  int err;                 /* error code if any (0 none) */
  struct dfa **subdfas;    /* per-tree-subre DFAs */
  struct dfa **ladfas;     /* per-lacon-subre DFAs */
  struct sset **lblastcss; /* per-lacon-subre lookbehind restart data */
  chr **lblastcp;          /* per-lacon-subre lookbehind restart data */
  struct smalldfa dfa1;
  struct smalldfa dfa2;
};

#define VISERR(vv) ((vv)->err != 0) /* have we seen an error yet? */
#define ISERR() VISERR(v)
#define VERR(vv, e) ((vv)->err = ((vv)->err ? (vv)->err : (e)))
#define ERR(e) VERR(v, e) /* record an error */
#define NOERR()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return v->err;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } /* if error seen, return it */
#define OFF(p) ((p)-v->start)
#define LOFF(p) ((long)OFF(p))

/*
 * forward declarations
 */
/* === regexec.c === */
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

/* === rege_dfa.c === */
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

/*
 * pg_regexec - match regular expression
 */
int
pg_regexec(regex_t *re, const chr *string, size_t len, size_t search_start, rm_detail_t *details, size_t nmatch, regmatch_t pmatch[], int flags)
{


































































































































































































}

/*
 * getsubdfa - create or re-fetch the DFA for a tree subre node
 *
 * We only need to create the DFA once per overall regex execution.
 * The DFA will be freed by the cleanup step in pg_regexec().
 */
static struct dfa *
getsubdfa(struct vars *v, struct subre *t)
{









}

/*
 * getladfa - create or re-fetch the DFA for a LACON subre node
 *
 * Same as above, but for LACONs.
 */
static struct dfa *
getladfa(struct vars *v, int n)
{













}

/*
 * find - find a match for the main NFA (no-complications case)
 */
static int
find(struct vars *v, struct cnfa *cnfa, struct colormap *cm)
{




































































































}

/*
 * cfind - find a match for the main NFA (with complications)
 */
static int
cfind(struct vars *v, struct cnfa *cnfa, struct colormap *cm)
{


































}

/*
 * cfindloop - the heart of cfind
 */
static int
cfindloop(struct vars *v, struct cnfa *cnfa, struct colormap *cm, struct dfa *d, struct dfa *s, chr **coldp) /* where to put coldstart pointer */
{















































































































}

/*
 * zapallsubs - initialize all subexpression matches to "no match"
 *
 * Note that p[0], the overall-match location, is not touched.
 */
static void
zapallsubs(regmatch_t *p, size_t n)
{







}

/*
 * zaptreesubs - initialize subexpressions within subtree to "no match"
 */
static void
zaptreesubs(struct vars *v, struct subre *t)
{




















}

/*
 * subset - set subexpression match data for a successful subre
 */
static void
subset(struct vars *v, struct subre *sub, chr *begin, chr *end)
{











}

/*
 * cdissect - check backrefs and determine subexpression matches
 *
 * cdissect recursively processes a subre tree to check matching of backrefs
 * and/or identify submatch boundaries for capture nodes.  The proposed match
 * runs from "begin" to "end" (not including "end"), and we are basically
 * "dissecting" it to see where the submatches are.
 *
 * Before calling any level of cdissect, the caller must have run the node's
 * DFA and found that the proposed substring satisfies the DFA.  (We make
 * the caller do that because in concatenation and iteration nodes, it's
 * much faster to check all the substrings against the child DFAs before we
 * recurse.)
 *
 * A side-effect of a successful match is to save match locations for
 * capturing subexpressions in v->pmatch[].  This is a little bit tricky,
 * so we make the following rules:
 * 1. Before initial entry to cdissect, all match data must have been
 *    cleared (this is seen to by zapallsubs).
 * 2. Before any recursive entry to cdissect, the match data for that
 *    subexpression tree must be guaranteed clear (see zaptreesubs).
 * 3. When returning REG_OKAY, each level of cdissect will have saved
 *    any relevant match locations.
 * 4. When returning REG_NOMATCH, each level of cdissect will guarantee
 *    that its subexpression match locations are again clear.
 * 5. No guarantees are made for error cases (i.e., other result codes).
 * 6. When a level of cdissect abandons a successful sub-match, it will
 *    clear that subtree's match locations with zaptreesubs before trying
 *    any new DFA match or cdissect call for that subtree or any subtree
 *    to its right (that is, any subtree that could have a backref into the
 *    abandoned match).
 * This may seem overly complicated, but it's difficult to simplify it
 * because of the provision that match locations must be reset before
 * any fresh DFA match (a rule that is needed to make dfa_backref safe).
 * That means it won't work to just reset relevant match locations at the
 * start of each cdissect level.
 */
static int                                            /* regexec return code */
cdissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                         /* end of same */
{










































































}

/*
 * ccondissect - dissect match for concatenation node
 */
static int                                               /* regexec return code */
ccondissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                            /* end of same */
{







































































}

/*
 * crevcondissect - dissect match for concatenation node, shortest-first
 */
static int                                                  /* regexec return code */
crevcondissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                               /* end of same */
{







































































}

/*
 * cbrdissect - dissect match for backref node
 */
static int                                              /* regexec return code */
cbrdissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                           /* end of same */
{














































































}

/*
 * caltdissect - dissect match for alternation node
 */
static int                                               /* regexec return code */
caltdissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                            /* end of same */
{




























}

/*
 * citerdissect - dissect match for iteration node
 */
static int                                                /* regexec return code */
citerdissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                             /* end of same */
{













































































































































































































}

/*
 * creviterdissect - dissect match for iteration node, shortest-first
 */
static int                                                   /* regexec return code */
creviterdissect(struct vars *v, struct subre *t, chr *begin, /* beginning of relevant substring */
    chr *end)                                                /* end of same */
{


































































































































































































}

#include "rege_dfa.c"