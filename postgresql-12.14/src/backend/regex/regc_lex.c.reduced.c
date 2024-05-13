/*
 * lexical analyzer
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
 * src/backend/regex/regc_lex.c
 *
 */

/* scanning macros (know about v) */
#define ATEOS() (v->now >= v->stop)
#define HAVE(n) (v->stop - v->now >= (n))
#define NEXT1(c) (!ATEOS() && *v->now == CHR(c))
#define NEXT2(a, b) (HAVE(2) && *v->now == CHR(a) && *(v->now + 1) == CHR(b))
#define NEXT3(a, b, c)                                                         \
  (HAVE(3) && *v->now == CHR(a) && *(v->now + 1) == CHR(b) &&                  \
   *(v->now + 2) == CHR(c))
#define SET(c) (v->nexttype = (c))
#define SETV(c, n) (v->nexttype = (c), v->nextvalue = (n))
#define RET(c) return (SET(c), 1)
#define RETV(c, n) return (SETV(c, n), 1)

#define LASTTYPE(t) (v->lasttype == (t))

/* lexical contexts */
#define L_ERE 1   /* mainline ERE/ARE */
#define L_BRE 2   /* mainline BRE */
#define L_Q 3     /* REG_QUOTE */


#define L_BRACK 6 /* brackets */



#define INTOCON(c) (v->lexcon = (c))


/* construct pointer past end of chr array */


/*
 * lexstart - set up lexical stuff, scan leading options
 */
static void
lexstart(struct vars *v)
{
  prefixes(v); /* may turn on new type bits etc. */
  NOERR();

  if (v->cflags & REG_QUOTE) {


  } else if (v->cflags & REG_EXTENDED) {
    assert(!(v->cflags & REG_QUOTE));
    INTOCON(L_ERE);
  } else {




  v->nexttype = EMPTY; /* remember we were at the start */
  next(v);             /* set up the first token */
}

/*
 * prefixes - implement various special prefixes
 */
static void
prefixes(struct vars *v)
{
  /* literal string doesn't get any of this stuff */
  if (v->cflags & REG_QUOTE) {



  /* initial "***" gets special things */
  if (HAVE(4) && NEXT3('*', '*', '*')) {





















    }
  }

  /* BREs and EREs don't get embedded options */
  if ((v->cflags & REG_ADVANCED) != REG_ADVANCED) {



  /* embedded options (AREs only) */
  if (HAVE(3) && NEXT2('(', '?') && iscalpha(*(v->now + 2))) {
























































}

/*
 * lexnest - "call a subroutine", interpolating string at the lexical level
 *
 * Note, this is not a very general facility.  There are a number of
 * implicit assumptions about what sorts of strings can be subroutines.
 */
static void
lexnest(struct vars *v, const chr *beginp, /* start of interpolation */
        const chr *endp)                   /* one past end of interpolation */
{





}

/*
 * string constants to interpolate as expansions of things like \d
 */
static const chr backd[] = {/* \d */
                            CHR('['), CHR('['), CHR(':'), CHR('d'),
                            CHR('i'), CHR('g'), CHR('i'), CHR('t'),
                            CHR(':'), CHR(']'), CHR(']')};
static const chr backD[] = {/* \D */
                            CHR('['), CHR('^'), CHR('['), CHR(':'),
                            CHR('d'), CHR('i'), CHR('g'), CHR('i'),
                            CHR('t'), CHR(':'), CHR(']'), CHR(']')};
static const chr brbackd[] = {/* \d within brackets */
                              CHR('['), CHR(':'), CHR('d'), CHR('i'), CHR('g'),
                              CHR('i'), CHR('t'), CHR(':'), CHR(']')};
static const chr backs[] = {/* \s */
                            CHR('['), CHR('['), CHR(':'), CHR('s'),
                            CHR('p'), CHR('a'), CHR('c'), CHR('e'),
                            CHR(':'), CHR(']'), CHR(']')};
static const chr backS[] = {/* \S */
                            CHR('['), CHR('^'), CHR('['), CHR(':'),
                            CHR('s'), CHR('p'), CHR('a'), CHR('c'),
                            CHR('e'), CHR(':'), CHR(']'), CHR(']')};
static const chr brbacks[] = {/* \s within brackets */
                              CHR('['), CHR(':'), CHR('s'), CHR('p'), CHR('a'),
                              CHR('c'), CHR('e'), CHR(':'), CHR(']')};
static const chr backw[] = {/* \w */
                            CHR('['), CHR('['), CHR(':'), CHR('a'),
                            CHR('l'), CHR('n'), CHR('u'), CHR('m'),
                            CHR(':'), CHR(']'), CHR('_'), CHR(']')};
static const chr backW[] = {/* \W */
                            CHR('['), CHR('^'), CHR('['), CHR(':'), CHR('a'),
                            CHR('l'), CHR('n'), CHR('u'), CHR('m'), CHR(':'),
                            CHR(']'), CHR('_'), CHR(']')};
static const chr brbackw[] = {/* \w within brackets */
                              CHR('['), CHR(':'), CHR('a'), CHR('l'), CHR('n'),
                              CHR('u'), CHR('m'), CHR(':'), CHR(']'), CHR('_')};

/*
 * lexword - interpolate a bracket expression for word characters
 * Possibly ought to inquire whether there is a "word" character class.
 */
static void
lexword(struct vars *v)
{

}

/*
 * next - get next token
 */
static int /* 1 normal, 0 failure */
next(struct vars *v)
{
  chr c;

next_restart: ;/* loop here after eating a comment */

  /* errors yield an infinite sequence of failures */
  if (ISERR()) {



  /* remember flavor of last token */
  v->lasttype = v->nexttype;

  /* REG_BOSONLY */
  if (v->nexttype == EMPTY && (v->cflags & REG_BOSONLY)) {




  /* if we're nested and we've hit end, return to outer level */
  if (v->savenow != NULL && ATEOS()) {





  /* skip white space etc. if appropriate (not in literal or []) */
  if (v->cflags & REG_EXPANDED) {







    }
  }

  /* handle EOS, depending on context */
  if (ATEOS()) {
    switch (v->lexcon) {
    case L_ERE:
    case L_BRE:
    case L_Q:
      RET(EOS);
      break;
    case L_EBND:













  /* okay, time to actually get a character */
  c = *v->now++;

  /* deal with the easy contexts, punt EREs to code below */
  switch (v->lexcon) {
  case L_BRE: /* punt BREs to separate function */

    break;
  case L_ERE: /* see below */
    break;
  case L_Q: /* literal strings are easy */

    break;
  case L_BBND: /* bounds are fairly simple */













































  case L_BRACK: /* brackets are not too hard */
    switch (c) {
    case CHR(']'):
      if (LASTTYPE('[')) {

      } else {
        INTOCON((v->cflags & REG_EXTENDED) ? L_ERE : L_BRE);
        RET(']');
      }






































    case CHR('-'):
      if (LASTTYPE('[') || NEXT1(']')) {

      } else {
        RETV(RANGE, c);
      }




























    default:;
      RETV(PLAIN, c);


































  }

  /* that got rid of everything except EREs and AREs */
  assert(INCON(L_ERE));

  /* deal with EREs and AREs, except for backslashes */
  switch (c) {
  case CHR('|'):

    break;
  case CHR('*'):
    if ((v->cflags & REG_ADVF) && NEXT1('?')) {




    RETV('*', 1);
    break;
  case CHR('+'):






























  case CHR('('): /* parenthesis, or advanced extension */
    if ((v->cflags & REG_ADVF) && NEXT1('?')) {



















































    if (v->cflags & REG_NOSUB) {

    } else {
      RETV('(', 1);
    }

  case CHR(')'):
    if (LASTTYPE('(')) {


    RETV(')', c);

  case CHR('['): /* easy except for [[:<:]] and [[:>:]] */
    if (HAVE(6) && *(v->now + 0) == CHR('[') && *(v->now + 1) == CHR(':') &&
        (*(v->now + 2) == CHR('<') || *(v->now + 2) == CHR('>')) &&
        *(v->now + 3) == CHR(':') && *(v->now + 4) == CHR(']') &&
        *(v->now + 5) == CHR(']')) {





    INTOCON(L_BRACK);
    if (NEXT1('^')) {
      v->now++;
      RETV('[', 0);
    }


  case CHR('.'):
    RET('.');

  case CHR('^'):
    RET('^');

  case CHR('$'):
    RET('$');

  case CHR('\\'): /* mostly punt backslashes to code below */
    if (ATEOS()) {


    break;
  default:; /* ordinary character */
    RETV(PLAIN, c);
    break;
  }

  /* ERE/ARE backslash handling; backslash already eaten */
  assert(!ATEOS());
  if (!(v->cflags & REG_ADVF)) { /* only AREs have non-trivial escapes */






  (DISCARD) lexescape(v);
  if (ISERR()) {


  if (v->nexttype == CCLASS) { /* fudge at lexical level */




























  /* otherwise, lexescape has already done the work */
  return !ISERR();
}

/*
 * lexescape - parse an ARE backslash escape (backslash already eaten)
 * Note slightly nonstandard use of the CCLASS type code.
 */
static int /* not actually used, but convenient for RETV */
lexescape(struct vars *v)
{
  chr c;
  static const chr alert[] = {CHR('a'), CHR('l'), CHR('e'), CHR('r'), CHR('t')};
  static const chr esc[] = {CHR('E'), CHR('S'), CHR('C')};
  const chr *save;

  assert(v->cflags & REG_ADVF);

  assert(!ATEOS());
  c = *v->now++;
  if (!iscalnum(c)) {
    RETV(PLAIN, c);
  }






















































































































































/*
 * lexdigits - slurp up digits and return chr value
 *
 * This does not account for overflow; callers should range-check the result
 * if maxlen is large enough to make that possible.
 */
static chr /* chr value; errors signalled via ERR */
lexdigits(struct vars *v, int base, int minlen, int maxlen)
{


































































}

/*
 * brenext - get next BRE token
 *
 * This is much like EREs except for all the stupid backslashes and the
 * context-dependency of some things.
 */
static int /* 1 normal, 0 failure */
brenext(struct vars *v, chr c)
{











































































































}

/*
 * skip - skip white space and comments in expanded form
 */
static void
skip(struct vars *v)
{





















}

/*
 * newline - return the chr for a newline
 *
 * This helps confine use of CHR to this source file.
 */
static chr
newline(void)
{
  return CHR('\n');
}

/*
 * chrnamed - return the chr known by a given (chr string) name
 *
 * The code is a bit clumsy, but this routine gets only such specialized
 * use that it hardly matters.
 */
static chr
chrnamed(struct vars *v, const chr *startp, /* start of name */
         const chr *endp,                   /* just past end of name */
         chr lastresort) /* what to return if name lookup fails */
{




















}
