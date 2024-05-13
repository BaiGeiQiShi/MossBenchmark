/*
 * colorings of characters
 * This file is #included by regcomp.c.
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
 * src/backend/regex/regc_color.c
 *
 *
 * Note that there are some incestuous relationships between this code and
 * NFA arc maintenance, which perhaps ought to be cleaned up sometime.
 */

#define CISERR() VISERR(cm->v)


/*
 * initcm - set up new colormap
 */
static void
initcm(struct vars *v, struct colormap *cm)
{
  struct colordesc *cd;

  cm->magic = CMMAGIC;
  cm->v = v;

  cm->ncds = NINLINECDS;
  cm->cd = cm->cdspace;
  cm->max = 0;
  cm->free = 0;

  cd = cm->cd; /* cm->cd[WHITE] */
  cd->nschrs = MAX_SIMPLE_CHR - CHR_MIN + 1;
  cd->nuchrs = 1;
  cd->sub = NOSUB;
  cd->arcs = NULL;
  cd->firstchr = CHR_MIN;
  cd->flags = 0;

  cm->locolormap =
      (color *)MALLOC((MAX_SIMPLE_CHR - CHR_MIN + 1) * sizeof(color));
  if (cm->locolormap == NULL) {





  /* this memset relies on WHITE being zero: */
  memset(cm->locolormap, WHITE, (MAX_SIMPLE_CHR - CHR_MIN + 1) * sizeof(color));

  memset(cm->classbits, 0, sizeof(cm->classbits));
  cm->numcmranges = 0;
  cm->cmranges = NULL;
  cm->maxarrayrows = 4; /* arbitrary initial allocation */
  cm->hiarrayrows = 1;  /* but we have only one row/col initially */
  cm->hiarraycols = 1;
  cm->hicolormap = (color *)MALLOC(cm->maxarrayrows * sizeof(color));
  if (cm->hicolormap == NULL) {



  /* initialize the "all other characters" row to WHITE */
  cm->hicolormap[0] = WHITE;
}

/*
 * freecm - free dynamically-allocated things in a colormap
 */
static void
freecm(struct colormap *cm)
{













}

/*
 * pg_reg_getcolor - slow case of GETCOLOR()
 */
color
pg_reg_getcolor(struct colormap *cm, chr c)
{




































}

/*
 * maxcolor - report largest color number in use
 */
static color
maxcolor(struct colormap *cm)
{
  if (CISERR()) {



  return (color)cm->max;
}

/*
 * newcolor - find a new color (must be assigned at once)
 * Beware:	may relocate the colordescs.
 */
static color /* COLORLESS for error */
newcolor(struct colormap *cm)
{
  struct colordesc *cd;
  size_t n;

  if (CISERR()) {



  if (cm->free != 0) {






  } else if (cm->max < cm->ncds - 1) {
    cm->max++;
    cd = &cm->cd[cm->max];
  } else {
    /* oops, must allocate more */
    struct colordesc *newCd;

    if (cm->max == MAX_COLOR) {




    n = cm->ncds * 2;
    if (n > MAX_COLOR + 1) {


    if (cm->cd == cm->cdspace) {
      newCd = (struct colordesc *)MALLOC(n * sizeof(struct colordesc));
      if (newCd != NULL) {
        memcpy(VS(newCd), VS(cm->cdspace), cm->ncds * sizeof(struct colordesc));
      }
    } else {


    if (newCd == NULL) {



    cm->cd = newCd;
    cm->ncds = n;
    assert(cm->max < cm->ncds - 1);
    cm->max++;
    cd = &cm->cd[cm->max];
  }

  cd->nschrs = 0;
  cd->nuchrs = 0;
  cd->sub = NOSUB;
  cd->arcs = NULL;
  cd->firstchr = CHR_MIN; /* in case never set otherwise */
  cd->flags = 0;

  return (color)(cd - cm->cd);
}

/*
 * freecolor - free a color (must have no arcs or subcolor)
 */
static void
freecolor(struct colormap *cm, color co)
{










































}

/*
 * pseudocolor - allocate a false color, to be managed by other means
 */
static color
pseudocolor(struct colormap *cm)
{
  color co;
  struct colordesc *cd;

  co = newcolor(cm);
  if (CISERR()) {


  cd = &cm->cd[co];
  cd->nschrs = 0;
  cd->nuchrs = 1; /* pretend it is in the upper map */
  cd->sub = NOSUB;
  cd->arcs = NULL;
  cd->firstchr = CHR_MIN;
  cd->flags = PSEUDO;
  return co;
}

/*
 * subcolor - allocate a new subcolor (if necessary) to this chr
 *
 * This works only for chrs that map into the low color map.
 */
static color
subcolor(struct colormap *cm, chr c)
{
  color co;  /* current color of c */
  color sco; /* new subcolor */

  assert(c <= MAX_SIMPLE_CHR);

  co = cm->locolormap[c - CHR_MIN];
  sco = newsub(cm, co);
  if (CISERR()) {


  assert(sco != COLORLESS);

  if (co == sco) { /* already in an open subcolor */
    return co;     /* rest is redundant */
  }
  cm->cd[co].nschrs--;
  if (cm->cd[sco].nschrs == 0) {
    cm->cd[sco].firstchr = c;
  }
  cm->cd[sco].nschrs++;
  cm->locolormap[c - CHR_MIN] = sco;
  return sco;
}

/*
 * subcolorhi - allocate a new subcolor (if necessary) to this colormap entry
 *
 * This is the same processing as subcolor(), but for entries in the high
 * colormap, which do not necessarily correspond to exactly one chr code.
 */
static color
subcolorhi(struct colormap *cm, color *pco)
{

















}

/*
 * newsub - allocate a new subcolor (if necessary) for a color
 */
static color
newsub(struct colormap *cm, color co)
{
  color sco; /* new subcolor */

  sco = cm->cd[co].sub;
  if (sco == NOSUB) { /* color has no open subcolor */
    /* optimization: singly-referenced color need not be subcolored */
    if ((cm->cd[co].nschrs + cm->cd[co].nuchrs) == 1) {
      return co;
    }
    sco = newcolor(cm); /* must create subcolor */
    if (sco == COLORLESS) {



    cm->cd[co].sub = sco;
    cm->cd[sco].sub = sco; /* open subcolor points to self */
  }
  assert(sco != NOSUB);

  return sco;
}

/*
 * newhicolorrow - get a new row in the hicolormap, cloning it from oldrow
 *
 * Returns array index of new row.  Note the array might move.
 */
static int
newhicolorrow(struct colormap *cm, int oldrow)
{


































}

/*
 * newhicolorcols - create a new set of columns in the high colormap
 *
 * Essentially, extends the 2-D array to the right with a copy of itself.
 */
static void
newhicolorcols(struct colormap *cm)
{































}

/*
 * subcolorcvec - allocate new subcolors to cvec members, fill in arcs
 *
 * For each chr "c" represented by the cvec, do the equivalent of
 * newarc(v->nfa, PLAIN, subcolor(v->cm, c), lp, rp);
 *
 * Note that in typical cases, many of the subcolors are the same.
 * While newarc() would discard duplicate arc requests, we can save
 * some cycles by not calling it repetitively to begin with.  This is
 * mechanized with the "lastsubcolor" state variable.
 */
static void
subcolorcvec(struct vars *v, struct cvec *cv, struct state *lp,
             struct state *rp)
{
  struct colormap *cm = v->cm;
  color lastsubcolor = COLORLESS;
  chr ch, from, to;
  const chr *p;
  int i;

  /* ordinary characters */
  for (p = cv->chrs, i = cv->nchrs; i > 0; p++, i--) {





  /* and the ranges */
  for (p = cv->ranges, i = cv->nranges; i > 0; p += 2, i--) {
    from = *p;
    to = *(p + 1);
    if (from <= MAX_SIMPLE_CHR) {
      /* deal with simple chars one at a time */
      chr lim = (to <= MAX_SIMPLE_CHR) ? to : MAX_SIMPLE_CHR;

      while (from <= lim) {
        color sco = subcolor(cm, from);

        NOERR();
        if (sco != lastsubcolor) {
          newarc(v->nfa, PLAIN, sco, lp, rp);
          NOERR();
          lastsubcolor = sco;
        }
        from++;
      }
    }
    /* deal with any part of the range that's above MAX_SIMPLE_CHR */
    if (from < to) {

    } else if (from == to) {


    NOERR();
  }

  /* and deal with cclass if any */
  if (cv->cclasscode >= 0) {






























}

/*
 * subcoloronechr - do subcolorcvec's work for a singleton chr
 *
 * We could just let subcoloronerange do this, but it's a bit more efficient
 * if we exploit the single-chr case.  Also, callers find it useful for this
 * to be able to handle both low and high chr codes.
 */
static void
subcoloronechr(struct vars *v, chr ch, struct state *lp, struct state *rp,
               color *lastsubcolor)
{
  struct colormap *cm = v->cm;
  colormaprange *newranges;
  int numnewranges;
  colormaprange *oldrange;
  int oldrangen;
  int newrow;

  /* Easy case for low chr codes */
  if (ch <= MAX_SIMPLE_CHR) {
    color sco = subcolor(cm, ch);

    NOERR();
    if (sco != *lastsubcolor) {
      newarc(v->nfa, PLAIN, sco, lp, rp);
      *lastsubcolor = sco;
    }
    return;





















































































/*
 * subcoloronerange - do subcolorcvec's work for a high range
 */
static void
subcoloronerange(struct vars *v, chr from, chr to, struct state *lp,
                 struct state *rp, color *lastsubcolor)
{



















































































































}

/*
 * subcoloronerow - do subcolorcvec's work for one new row in the high colormap
 */
static void
subcoloronerow(struct vars *v, int rownum, struct state *lp, struct state *rp,
               color *lastsubcolor)
{

















}

/*
 * okcolors - promote subcolors to full colors
 */
static void
okcolors(struct nfa *nfa, struct colormap *cm)
{
  struct colordesc *cd;
  struct colordesc *end = CDEND(cm);
  struct colordesc *scd;
  struct arc *a;
  color co;
  color sco;

  for (cd = cm->cd, co = 0; cd < end; cd++, co++) {
    sco = cd->sub;
    if (UNUSEDCOLOR(cd) || sco == NOSUB) {
      /* has no subcolor, no further action */
    } else if (sco == co) {

    } else if (cd->nschrs == 0 && cd->nuchrs == 0) {













    } else {
      /* parent's arcs must gain parallel subcolor arcs */
      cd->sub = NOSUB;
      scd = &cm->cd[sco];
      assert(scd->nschrs > 0 || scd->nuchrs > 0);
      assert(scd->sub == sco);
      scd->sub = NOSUB;
      for (a = cd->arcs; a != NULL; a = a->colorchain) {
        assert(a->co == co);
        newarc(nfa, a->type, sco, a->from, a->to);
      }
    }
  }
}

/*
 * colorchain - add this arc to the color chain of its color
 */
static void
colorchain(struct colormap *cm, struct arc *a)
{
  struct colordesc *cd = &cm->cd[a->co];

  if (cd->arcs != NULL) {
    cd->arcs->colorchainRev = a;
  }
  a->colorchain = cd->arcs;
  a->colorchainRev = NULL;
  cd->arcs = a;
}

/*
 * uncolorchain - delete this arc from the color chain of its color
 */
static void
uncolorchain(struct colormap *cm, struct arc *a)
{
  struct colordesc *cd = &cm->cd[a->co];
  struct arc *aa = a->colorchainRev;

  if (aa == NULL) {
    assert(cd->arcs == a);
    cd->arcs = a->colorchain;
  } else {
    assert(aa->colorchain == a);
    aa->colorchain = a->colorchain;
  }
  if (a->colorchain != NULL) {
    a->colorchain->colorchainRev = aa;
  }
  a->colorchain = NULL; /* paranoia */
  a->colorchainRev = NULL;
}

/*
 * rainbow - add arcs of all full colors (but one) between specified states
 */
static void
rainbow(struct nfa *nfa, struct colormap *cm, int type,
        color but, /* COLORLESS if no exceptions */
        struct state *from, struct state *to)
{
  struct colordesc *cd;
  struct colordesc *end = CDEND(cm);
  color co;

  for (cd = cm->cd, co = 0; cd < end && !CISERR(); cd++, co++) {
    if (!UNUSEDCOLOR(cd) && cd->sub != co && co != but &&
        !(cd->flags & PSEUDO)) {
      newarc(nfa, type, co, from, to);
    }
  }
}

/*
 * colorcomplement - add arcs of complementary colors
 *
 * The calling sequence ought to be reconciled with cloneouts().
 */
static void
colorcomplement(struct nfa *nfa, struct colormap *cm, int type,
                struct state *of, /* complements of this guy's PLAIN outarcs */
                struct state *from, struct state *to)
{
  struct colordesc *cd;
  struct colordesc *end = CDEND(cm);
  color co;

  assert(of != from);
  for (cd = cm->cd, co = 0; cd < end && !CISERR(); cd++, co++) {
    if (!UNUSEDCOLOR(cd) && !(cd->flags & PSEUDO)) {
      if (findarc(of, PLAIN, co) == NULL) {
        newarc(nfa, type, co, from, to);
      }
    }
  }
}

#ifdef REG_DEBUG

/*
 * dumpcolors - debugging output
 */
static void
dumpcolors(struct colormap *cm, FILE *f)
{
  struct colordesc *cd;
  struct colordesc *end;
  color co;
  chr c;

  fprintf(f, "max %ld\n", (long)cm->max);
  end = CDEND(cm);
  for (cd = cm->cd + 1, co = 1; cd < end; cd++, co++) /* skip 0 */
  {
    if (!UNUSEDCOLOR(cd)) {
      assert(cd->nschrs > 0 || cd->nuchrs > 0);
      if (cd->flags & PSEUDO) {
        fprintf(f, "#%2ld(ps): ", (long)co);
      } else {
        fprintf(f, "#%2ld(%2d): ", (long)co, cd->nschrs + cd->nuchrs);
      }

      /*
       * Unfortunately, it's hard to do this next bit more efficiently.
       */
      for (c = CHR_MIN; c <= MAX_SIMPLE_CHR; c++) {
        if (GETCOLOR(cm, c) == co) {
          dumpchr(c, f);
        }
      }
      fprintf(f, "\n");
    }
  }
  /* dump the high colormap if it contains anything interesting */
  if (cm->hiarrayrows > 1 || cm->hiarraycols > 1) {
    int r, c;
    const color *rowptr;

    fprintf(f, "other:\t");
    for (c = 0; c < cm->hiarraycols; c++) {
      fprintf(f, "\t%ld", (long)cm->hicolormap[c]);
    }
    fprintf(f, "\n");
    for (r = 0; r < cm->numcmranges; r++) {
      dumpchr(cm->cmranges[r].cmin, f);
      fprintf(f, "..");
      dumpchr(cm->cmranges[r].cmax, f);
      fprintf(f, ":");
      rowptr = &cm->hicolormap[cm->cmranges[r].rownum * cm->hiarraycols];
      for (c = 0; c < cm->hiarraycols; c++) {
        fprintf(f, "\t%ld", (long)rowptr[c]);
      }
      fprintf(f, "\n");
    }
  }
}

/*
 * dumpchr - print a chr
 *
 * Kind of char-centric but works well enough for debug use.
 */
static void
dumpchr(chr c, FILE *f)
{
  if (c == '\\') {
    fprintf(f, "\\\\");
  } else if (c > ' ' && c <= '~') {
    putc((char)c, f);
  } else {
    fprintf(f, "\\u%04lx", (long)c);
  }
}

#endif /* REG_DEBUG */
