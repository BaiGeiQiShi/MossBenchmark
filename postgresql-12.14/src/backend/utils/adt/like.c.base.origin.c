/*-------------------------------------------------------------------------
 *
 * like.c
 *	  like expression handling code.
 *
 *	 NOTES
 *		A big hack of the regexp.c code!! Contributed by
 *		Keith Parks <emkxp01@mtcc.demon.co.uk> (7/95).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	src/backend/utils/adt/like.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "catalog/pg_collation.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"

#define LIKE_TRUE 1
#define LIKE_FALSE 0
#define LIKE_ABORT (-1)

static int
SB_MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);
static text *
SB_do_like_escape(text *, text *);

static int
MB_MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);
static text *
MB_do_like_escape(text *, text *);

static int
UTF8_MatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);

static int
SB_IMatchText(const char *t, int tlen, const char *p, int plen, pg_locale_t locale, bool locale_is_c);

static int
GenericMatchText(const char *s, int slen, const char *p, int plen, Oid collation);
static int
Generic_Text_IC_like(text *str, text *pat, Oid collation);

/*--------------------
 * Support routine for MatchText. Compares given multibyte streams
 * as wide characters. If they match, returns 1 otherwise returns 0.
 *--------------------
 */
static inline int
wchareq(const char *p1, const char *p2)
{























}

/*
 * Formerly we had a routine iwchareq() here that tried to do case-insensitive
 * comparison of multibyte characters.  It did not work at all, however,
 * because it relied on tolower() which has a single-byte API ... and
 * towlower() wouldn't be much better since we have no suitably cheap way
 * of getting a single character transformed to the system's wchar_t format.
 * So now, we just downcase the strings using lower() and apply regular LIKE
 * comparison.  This should be revisited when we install better locale support.
 */

/*
 * We do handle case-insensitive matching for single-byte encodings using
 * fold-on-the-fly processing, however.
 */
static char
SB_lower_char(unsigned char c, pg_locale_t locale, bool locale_is_c)
{














}

#define NextByte(p, plen) ((p)++, (plen)--)

/* Set up to compile like_match.c for multibyte characters */
#define CHAREQ(p1, p2) wchareq((p1), (p2))
#define NextChar(p, plen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int __l = pg_mblen(p);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    (p) += __l;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    (plen) -= __l;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  } while (0)
#define CopyAdvChar(dst, src, srclen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int __l = pg_mblen(src);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    (srclen) -= __l;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    while (__l-- > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *(dst)++ = *(src)++;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)

#define MatchText MB_MatchText
#define do_like_escape MB_do_like_escape

#include "like_match.c"

/* Set up to compile like_match.c for single-byte characters */
#define CHAREQ(p1, p2) (*(p1) == *(p2))
#define NextChar(p, plen) NextByte((p), (plen))
#define CopyAdvChar(dst, src, srclen) (*(dst)++ = *(src)++, (srclen)--)

#define MatchText SB_MatchText
#define do_like_escape SB_do_like_escape

#include "like_match.c"

/* setup to compile like_match.c for single byte case insensitive matches */
#define MATCH_LOWER(t) SB_lower_char((unsigned char)(t), locale, locale_is_c)
#define NextChar(p, plen) NextByte((p), (plen))
#define MatchText SB_IMatchText

#include "like_match.c"

/* setup to compile like_match.c for UTF8 encoding, using fast NextChar */

#define NextChar(p, plen)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (p)++;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    (plen)--;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while ((plen) > 0 && (*(p) & 0xC0) == 0x80)
#define MatchText UTF8_MatchText

#include "like_match.c"

/* Generic for all cases not requiring inline case-folding */
static inline int
GenericMatchText(const char *s, int slen, const char *p, int plen, Oid collation)
{






















}

static inline int
Generic_Text_IC_like(text *str, text *pat, Oid collation)
{




























































}

/*
 *	interface routines called by the function manager
 */

Datum
namelike(PG_FUNCTION_ARGS)
{














}

Datum
namenlike(PG_FUNCTION_ARGS)
{














}

Datum
textlike(PG_FUNCTION_ARGS)
{














}

Datum
textnlike(PG_FUNCTION_ARGS)
{














}

Datum
bytealike(PG_FUNCTION_ARGS)
{














}

Datum
byteanlike(PG_FUNCTION_ARGS)
{














}

/*
 * Case-insensitive versions
 */

Datum
nameiclike(PG_FUNCTION_ARGS)
{









}

Datum
nameicnlike(PG_FUNCTION_ARGS)
{









}

Datum
texticlike(PG_FUNCTION_ARGS)
{







}

Datum
texticnlike(PG_FUNCTION_ARGS)
{







}

/*
 * like_escape() --- given a pattern and an ESCAPE string,
 * convert the pattern to use Postgres' standard backslash escape convention.
 */
Datum
like_escape(PG_FUNCTION_ARGS)
{














}

/*
 * like_escape_bytea() --- given a pattern and an ESCAPE string,
 * convert the pattern to use Postgres' standard backslash escape convention.
 */
Datum
like_escape_bytea(PG_FUNCTION_ARGS)
{





}