/*
 * re_*comp and friends - compile REs
 * This file #includes several others (see the bottom).
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
 * src/backend/regex/regcomp.c
 *
 */

#include "regex/regguts.h"

/*
 * forward declarations, up here so forward datatypes etc. are defined early
 */
/* === regcomp.c === */
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
/* === regc_lex.c === */
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

/* === regc_color.c === */
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
/* === regc_nfa.c === */
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

#define INCOMPATIBLE 1 /* destroys arc */
#define SATISFIED 2    /* constraint satisfied */
#define COMPATIBLE 3   /* compatible but not satisfied yet */
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
/* === regc_cvec.c === */
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

/* === regc_pg_locale.c === */
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

/* === regc_locale.c === */
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

/* internal variables, bundled for easy passing around */
struct vars
{
  regex_t *re;
  const chr *now;     /* scan pointer into string */
  const chr *stop;    /* end of string */
  const chr *savenow; /* saved now and stop for "subroutine call" */
  const chr *savestop;
  int err;                 /* error code (0 if none) */
  int cflags;              /* copy of compile flags */
  int lasttype;            /* type of previous token */
  int nexttype;            /* type of next token */
  chr nextvalue;           /* value (if any) of next token */
  int lexcon;              /* lexical context type (see lex.c) */
  int nsubexp;             /* subexpression count */
  struct subre **subs;     /* subRE pointer vector */
  size_t nsubs;            /* length of vector */
  struct subre *sub10[10]; /* initial vector, enough for most */
  struct nfa *nfa;         /* the NFA */
  struct colormap *cm;     /* character color map */
  color nlcolor;           /* color of newline */
  struct state *wordchrs;  /* state in nfa holding word-char outarcs */
  struct subre *tree;      /* subexpression tree */
  struct subre *treechain; /* all tree nodes allocated */
  struct subre *treefree;  /* any free tree nodes */
  int ntree;               /* number of tree nodes, plus one */
  struct cvec *cv;         /* interface cvec */
  struct cvec *cv2;        /* utility cvec */
  struct subre *lacons;    /* lookaround-constraint vector */
  int nlacons;             /* size of lacons[]; note that only slots
                            * numbered 1 .. nlacons-1 are used */
  size_t spaceused;        /* approx. space used for compilation */
};

/* parsing macros; most know that `v' is the struct vars pointer */
#define NEXT() (next(v))            /* advance by one token */
#define SEE(t) (v->nexttype == (t)) /* is next token this? */
#define EAT(t) (SEE(t) && next(v))  /* if next is this, swallow it */
#define VISERR(vv) ((vv)->err != 0) /* have we seen an error yet? */
#define ISERR() VISERR(v)
#define VERR(vv, e) ((vv)->nexttype = EOS, (vv)->err = ((vv)->err ? (vv)->err : (e)))
#define ERR(e) VERR(v, e) /* record an error */
#define NOERR()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } /* if error seen, return */
#define NOERRN()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  } /* NOERR with retval */
#define NOERRZ()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ISERR())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      return 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } /* NOERR with retval */
#define INSIST(c, e)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (!(c))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      ERR(e);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0) /* error if c false */
#define NOTE(b) (v->re->re_info |= (b)) /* note visible condition */
#define EMPTYARC(x, y) newarc(v->nfa, EMPTY, 0, x, y)

/* token type codes, some also used as NFA arc types */
#define EMPTY 'n'   /* no token present */
#define EOS 'e'     /* end of string */
#define PLAIN 'p'   /* ordinary character */
#define DIGIT 'd'   /* digit (in bound) */
#define BACKREF 'b' /* back reference */
#define COLLEL 'I'  /* start of [. */
#define ECLASS 'E'  /* start of [= */
#define CCLASS 'C'  /* start of [: */
#define END 'X'     /* end of [. [= [: */
#define RANGE 'R'   /* - within [] which might be range delim. */
#define LACON 'L'   /* lookaround constraint subRE */
#define AHEAD 'a'   /* color-lookahead arc */
#define BEHIND 'r'  /* color-lookbehind arc */
#define WBDRY 'w'   /* word boundary constraint */
#define NWBDRY 'W'  /* non-word-boundary constraint */
#define SBEGIN 'A'  /* beginning of string (even if not BOL) */
#define SEND 'Z'    /* end of string (even if not EOL) */
#define PREFER 'P'  /* length preference */

/* is an arc colored, and hence on a color chain? */
#define COLORED(a) ((a)->type == PLAIN || (a)->type == AHEAD || (a)->type == BEHIND)

/* static function list */
static const struct fns functions = {
    rfree,            /* regfree insides */
    rcancelrequested, /* check for cancel request */
    rstacktoodeep     /* check for stack getting dangerously deep */
};

/*
 * pg_regcomp - compile regular expression
 *
 * Note: on failure, no resources remain allocated, so pg_regfree()
 * need not be applied to re.
 */
int
pg_regcomp(regex_t *re, const chr *string, size_t len, int flags, Oid collation)
{































































































































































































}

/*
 * moresubs - enlarge subRE vector
 */
static void
moresubs(struct vars *v, int wanted) /* want enough room for this one */
{






























}

/*
 * freev - free vars struct's substructures where necessary
 *
 * Optionally does error-number setting, and always returns error code
 * (if any), to make error-handling code terser.
 */
static int
freev(struct vars *v, int err)
{



































}

/*
 * makesearch - turn an NFA into a search NFA (implicit prepend of .*?)
 * NFA must have been optimize()d already.
 */
static void
makesearch(struct vars *v, struct nfa *nfa)
{
















































































}

/*
 * parse - parse an RE
 *
 * This is actually just the top level, which parses a bunch of branches
 * tied together with '|'.  They appear in the tree as the left children
 * of a chain of '|' subres.
 */
static struct subre *
parse(struct vars *v, int stopper, /* EOS or ')' */
    int type,                      /* LACON (lookaround subRE) or PLAIN */
    struct state *init,            /* initial state */
    struct state *final)           /* final state */
{



































































}

/*
 * parsebranch - parse one branch of an RE
 *
 * This mostly manages concatenation, working closely with parseqatom().
 * Concatenated things are bundled up as much as possible, with separate
 * ',' nodes introduced only when necessary due to substructure.
 */
static struct subre *
parsebranch(struct vars *v, int stopper, /* EOS or ')' */
    int type,                            /* LACON (lookaround subRE) or PLAIN */
    struct state *left,                  /* leftmost state */
    struct state *right,                 /* rightmost state */
    int partial)                         /* is this only part of a branch? */
{


































}

/*
 * parseqatom - parse one quantified atom or constraint of an RE
 *
 * The bookkeeping near the end cooperates very closely with parsebranch();
 * in particular, it contains a recursion that can involve parsing the rest
 * of the branch, making this function's name somewhat inaccurate.
 */
static void
parseqatom(struct vars *v, int stopper, /* EOS or ')' */
    int type,                           /* LACON (lookaround subRE) or PLAIN */
    struct state *lp,                   /* left state to hang it on */
    struct state *rp,                   /* right state to hang it on */
    struct subre *top)                  /* subtree top */
{




















































































































































































































































































































































































































































































}

/*
 * nonword - generate arcs for non-word-character ahead or behind
 */
static void
nonword(struct vars *v, int dir, /* AHEAD or BEHIND */
    struct state *lp, struct state *rp)
{







}

/*
 * word - generate arcs for word character ahead or behind
 */
static void
word(struct vars *v, int dir, /* AHEAD or BEHIND */
    struct state *lp, struct state *rp)
{



}

/*
 * scannum - scan a number
 */
static int /* value, <= DUPMAX */
scannum(struct vars *v)
{













}

/*
 * repeat - replicate subNFA for quantifiers
 *
 * The sub-NFA strung from lp to rp is modified to represent m to n
 * repetitions of its initial contents.
 *
 * The duplication sequences used here are chosen carefully so that any
 * pointers starting out pointing into the subexpression end up pointing into
 * the last occurrence.  (Note that it may not be strung between the same
 * left and right end states, however!)  This used to be important for the
 * subRE tree, although the important bits are now handled by the in-line
 * code in parse(), and when this is called, it doesn't matter any more.
 */
static void
repeat(struct vars *v, struct state *lp, struct state *rp, int m, int n)
{









































































}

/*
 * bracket - handle non-complemented bracket expression
 * Also called from cbracket for complemented bracket expressions.
 */
static void
bracket(struct vars *v, struct state *lp, struct state *rp)
{








}

/*
 * cbracket - handle complemented bracket expression
 * We do it by calling bracket() with dummy endpoints, and then complementing
 * the result.  The alternative would be to invoke rainbow(), and then delete
 * arcs as the b.e. is seen... but that gets messy.
 */
static void
cbracket(struct vars *v, struct state *lp, struct state *rp)
{






















}

/*
 * brackpart - handle one item (or range) within a bracket expression
 */
static void
brackpart(struct vars *v, struct state *lp, struct state *rp)
{








































































































}

/*
 * scanplain - scan PLAIN contents of [. etc.
 *
 * Certain bits of trickery in lex.c know that this code does not try
 * to look past the final bracket of the [. etc.
 */
static const chr * /* just after end of sequence */
scanplain(struct vars *v)
{
















}

/*
 * onechr - fill in arcs for a plain character, and possible case complements
 * This is mostly a shortcut for efficient handling of the common case.
 */
static void
onechr(struct vars *v, chr c, struct state *lp, struct state *rp)
{










}

/*
 * wordchrs - set up word-chr list for word-boundary stuff, if needed
 *
 * The list is kept as a bunch of arcs between two dummy states; it's
 * disposed of by the unreachable-states sweep in NFA optimization.
 * Does NEXT().  Must not be called from any unusual lexical context.
 * This should be reconciled with the \w etc. handling in lex.c, and
 * should be cleaned up to reduce dependencies on input scanning.
 */
static void
wordchrs(struct vars *v)
{





















}

/*
 * processlacon - generate the NFA representation of a LACON
 *
 * In the general case this is just newlacon() + newarc(), but some cases
 * can be optimized.
 */
static void
processlacon(struct vars *v, struct state *begin, /* start of parsed LACON sub-re */
    struct state *end,                            /* end of parsed LACON sub-re */
    int latype, struct state *lp,                 /* left state to hang it on */
    struct state *rp)                             /* right state to hang it on */
{





















































}

/*
 * subre - allocate a subre
 */
static struct subre *
subre(struct vars *v, int op, int flags, struct state *begin, struct state *end)
{










































}

/*
 * freesubre - free a subRE subtree
 */
static void
freesubre(struct vars *v, /* might be NULL */
    struct subre *sr)
{















}

/*
 * freesrnode - free one node in a subRE subtree
 */
static void
freesrnode(struct vars *v, /* might be NULL */
    struct subre *sr)
{





















}

/*
 * optst - optimize a subRE subtree
 */
static void
optst(struct vars *v, struct subre *t)
{







}

/*
 * numst - number tree nodes (assigning "id" indexes)
 */
static int                        /* next number */
numst(struct subre *t, int start) /* starting point for subtree numbers */
{















}

/*
 * markst - mark tree nodes as INUSE
 *
 * Note: this is a great deal more subtle than it looks.  During initial
 * parsing of a regex, all subres are linked into the treechain list;
 * discarded ones are also linked into the treefree list for possible reuse.
 * After we are done creating all subres required for a regex, we run markst()
 * then cleanst(), which results in discarding all subres not reachable from
 * v->tree.  We then clear v->treechain, indicating that subres must be found
 * by descending from v->tree.  This changes the behavior of freesubre(): it
 * will henceforth FREE() unwanted subres rather than sticking them into the
 * treefree list.  (Doing that any earlier would result in dangling links in
 * the treechain list.)  This all means that freev() will clean up correctly
 * if invoked before or after markst()+cleanst(); but it would not work if
 * called partway through this state conversion, so we mustn't error out
 * in or between these two functions.
 */
static void
markst(struct subre *t)
{











}

/*
 * cleanst - free any tree nodes not marked INUSE
 */
static void
cleanst(struct vars *v)
{













}

/*
 * nfatree - turn a subRE subtree into a tree of compacted NFAs
 */
static long                                       /* optimize results from top node */
nfatree(struct vars *v, struct subre *t, FILE *f) /* for debug output */
{












}

/*
 * nfanode - do one NFA for nfatree or lacons
 *
 * If converttosearch is true, apply makesearch() to the NFA.
 */
static long                                                            /* optimize results */
nfanode(struct vars *v, struct subre *t, int converttosearch, FILE *f) /* for debug output */
{



































}

/*
 * newlacon - allocate a lookaround-constraint subRE
 */
static int /* lacon number */
newlacon(struct vars *v, struct state *begin, struct state *end, int latype)
{



























}

/*
 * freelacons - free lookaround-constraint subRE vector
 */
static void
freelacons(struct subre *subs, int n)
{












}

/*
 * rfree - free a whole RE (insides of regfree)
 */
static void
rfree(regex_t *re)
{





























}

/*
 * rcancelrequested - check for external request to cancel regex operation
 *
 * Return nonzero to fail the operation with error code REG_CANCEL,
 * zero to keep going
 *
 * The current implementation is Postgres-specific.  If we ever get around
 * to splitting the regex code out as a standalone library, there will need
 * to be some API to let applications define a callback function for this.
 */
static int
rcancelrequested(void)
{

}

/*
 * rstacktoodeep - check for stack getting dangerously deep
 *
 * Return nonzero to fail the operation with error code REG_ETOOBIG,
 * zero to keep going
 *
 * The current implementation is Postgres-specific.  If we ever get around
 * to splitting the regex code out as a standalone library, there will need
 * to be some API to let applications define a callback function for this.
 */
static int
rstacktoodeep(void)
{

}

#ifdef REG_DEBUG

/*
 * dump - dump an RE in human-readable form
 */
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
    default:;
      latype = "???";
      break;
    }
    fprintf(f, "\nla%d (%s):\n", i, latype);
    dumpcnfa(&lasub->cnfa, f);
  }
  fprintf(f, "\n");
  dumpst(g->tree, f, 0);
}

/*
 * dumpst - dump a subRE tree
 */
static void
dumpst(struct subre *t, FILE *f, int nfapresent) /* is the original NFA still around? */
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

/*
 * stdump - recursive guts of dumpst
 */
static void
stdump(struct subre *t, FILE *f, int nfapresent) /* is the original NFA still around? */
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

/*
 * stid - identify a subtree node for dumping
 */
static const char * /* points to buf or constant string */
stid(struct subre *t, char *buf, size_t bufsize)
{
  /* big enough for hex int or decimal t->id? */
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
#endif /* REG_DEBUG */

#include "regc_lex.c"
#include "regc_color.c"
#include "regc_nfa.c"
#include "regc_cvec.c"
#include "regc_pg_locale.c"
#include "regc_locale.c"