/*-------------------------------------------------------------------------
 *
 * wparser_def.c
 *		Default text search parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/wparser_def.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_collation.h"
#include "commands/defrem.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_type.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

/* Define me to enable tracing of parser behavior */
/* #define WPARSER_TRACE */

/* Output token categories */

#define ASCIIWORD 1
#define WORD_T 2
#define NUMWORD 3
#define EMAIL 4
#define URL_T 5
#define HOST 6
#define SCIENTIFIC 7
#define VERSIONNUMBER 8
#define NUMPARTHWORD 9
#define PARTHWORD 10
#define ASCIIPARTHWORD 11
#define SPACE 12
#define TAG_T 13
#define PROTOCOL 14
#define NUMHWORD 15
#define ASCIIHWORD 16
#define HWORD 17
#define URLPATH 18
#define FILEPATH 19
#define DECIMAL_T 20
#define SIGNEDINT 21
#define UNSIGNEDINT 22
#define XMLENTITY 23

#define LASTNUM 23

static const char *const tok_alias[] = {"", "asciiword", "word", "numword", "email", "url", "host", "sfloat", "version", "hword_numpart", "hword_part", "hword_asciipart", "blank", "tag", "protocol", "numhword", "asciihword", "hword", "url_path", "file", "float", "int", "uint", "entity"};

static const char *const lex_descr[] = {"", "Word, all ASCII", "Word, all letters", "Word, letters and digits", "Email address", "URL", "Host", "Scientific notation", "Version number", "Hyphenated word part, letters and digits", "Hyphenated word part, all letters", "Hyphenated word part, all ASCII", "Space symbols", "XML tag", "Protocol head", "Hyphenated word, letters and digits", "Hyphenated word, all ASCII", "Hyphenated word, all letters", "URL path", "File or path name", "Decimal notation", "Signed integer", "Unsigned integer", "XML entity"};

/* Parser states */

typedef enum
{
  TPS_Base = 0,
  TPS_InNumWord,
  TPS_InAsciiWord,
  TPS_InWord,
  TPS_InUnsignedInt,
  TPS_InSignedIntFirst,
  TPS_InSignedInt,
  TPS_InSpace,
  TPS_InUDecimalFirst,
  TPS_InUDecimal,
  TPS_InDecimalFirst,
  TPS_InDecimal,
  TPS_InVerVersion,
  TPS_InSVerVersion,
  TPS_InVersionFirst,
  TPS_InVersion,
  TPS_InMantissaFirst,
  TPS_InMantissaSign,
  TPS_InMantissa,
  TPS_InXMLEntityFirst,
  TPS_InXMLEntity,
  TPS_InXMLEntityNumFirst,
  TPS_InXMLEntityNum,
  TPS_InXMLEntityHexNumFirst,
  TPS_InXMLEntityHexNum,
  TPS_InXMLEntityEnd,
  TPS_InTagFirst,
  TPS_InXMLBegin,
  TPS_InTagCloseFirst,
  TPS_InTagName,
  TPS_InTagBeginEnd,
  TPS_InTag,
  TPS_InTagEscapeK,
  TPS_InTagEscapeKK,
  TPS_InTagBackSleshed,
  TPS_InTagEnd,
  TPS_InCommentFirst,
  TPS_InCommentLast,
  TPS_InComment,
  TPS_InCloseCommentFirst,
  TPS_InCloseCommentLast,
  TPS_InCommentEnd,
  TPS_InHostFirstDomain,
  TPS_InHostDomainSecond,
  TPS_InHostDomain,
  TPS_InPortFirst,
  TPS_InPort,
  TPS_InHostFirstAN,
  TPS_InHost,
  TPS_InEmail,
  TPS_InFileFirst,
  TPS_InFileTwiddle,
  TPS_InPathFirst,
  TPS_InPathFirstFirst,
  TPS_InPathSecond,
  TPS_InFile,
  TPS_InFileNext,
  TPS_InURLPathFirst,
  TPS_InURLPathStart,
  TPS_InURLPath,
  TPS_InFURL,
  TPS_InProtocolFirst,
  TPS_InProtocolSecond,
  TPS_InProtocolEnd,
  TPS_InHyphenAsciiWordFirst,
  TPS_InHyphenAsciiWord,
  TPS_InHyphenWordFirst,
  TPS_InHyphenWord,
  TPS_InHyphenNumWordFirst,
  TPS_InHyphenNumWord,
  TPS_InHyphenDigitLookahead,
  TPS_InParseHyphen,
  TPS_InParseHyphenHyphen,
  TPS_InHyphenWordPart,
  TPS_InHyphenAsciiWordPart,
  TPS_InHyphenNumWordPart,
  TPS_InHyphenUnsignedInt,
  TPS_Null /* last state (fake value) */
} TParserState;

/* forward declaration */
struct TParser;

typedef int (*TParserCharTest)(struct TParser *); /* any p_is* functions
                                                   * except p_iseq */
typedef void (*TParserSpecial)(struct TParser *); /* special handler for
                                                   * special cases... */

typedef struct
{
  TParserCharTest isclass;
  char c;
  uint16 flags;
  TParserState tostate;
  int type;
  TParserSpecial special;
} TParserStateActionItem;

/* Flag bits in TParserStateActionItem.flags */
#define A_NEXT 0x0000
#define A_BINGO 0x0001
#define A_POP 0x0002
#define A_PUSH 0x0004
#define A_RERUN 0x0008
#define A_CLEAR 0x0010
#define A_MERGE 0x0020
#define A_CLRALL 0x0040

typedef struct TParserPosition
{
  int posbyte;      /* position of parser in bytes */
  int poschar;      /* position of parser in characters */
  int charlen;      /* length of current char */
  int lenbytetoken; /* length of token-so-far in bytes */
  int lenchartoken; /* and in chars */
  TParserState state;
  struct TParserPosition *prev;
  const TParserStateActionItem *pushedAtAction;
} TParserPosition;

typedef struct TParser
{
  /* string and position information */
  char *str;        /* multibyte string */
  int lenstr;       /* length of mbstring */
  wchar_t *wstr;    /* wide character string */
  pg_wchar *pgwstr; /* wide character string for C-locale */
  bool usewide;

  /* State of parse */
  int charmaxlen;
  TParserPosition *state;
  bool ignore;
  bool wanthost;

  /* silly char */
  char c;

  /* out */
  char *token;
  int lenbytetoken;
  int lenchartoken;
  int type;
} TParser;

/* forward decls here */
static bool
TParserGet(TParser *prs);

static TParserPosition *
newTParserPosition(TParserPosition *prev)
{
















}

static TParser *
TParserInit(char *str, int len)
{

















































}

/*
 * As an alternative to a full TParserInit one can create a
 * TParserCopy which basically is a regular TParser without a private
 * copy of the string - instead it uses the one from another TParser.
 * This is useful because at some places TParsers are created
 * recursively and the repeated copying around of the strings can
 * cause major inefficiency if the source string is long.
 * The new parser starts parsing at the original's current position.
 *
 * Obviously one must not close the original TParser before the copy.
 */
static TParser *
TParserCopyInit(const TParser *orig)
{

























}

static void
TParserClose(TParser *prs)
{





















}

/*
 * Close a parser created with TParserCopyInit
 */
static void
TParserCopyClose(TParser *prs)
{












}

/*
 * Character-type support functions, equivalent to is* macros, but
 * working with any possible encodings and locales. Notes:
 *	- with multibyte encoding and C-locale isw* function may fail
 *	  or give wrong result.
 *	- multibyte encoding and C-locale often are used for
 *	  Asian languages.
 *	- if locale is C then we use pgwstr instead of wstr.
 */

#define p_iswhat(type, nonascii)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  static int p_is##type(TParser *prs)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    Assert(prs->state);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    if (prs->usewide)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      if (prs->pgwstr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        unsigned int c = *(prs->pgwstr + prs->state->poschar);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
        if (c > 0x7f)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
          return nonascii;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
        return is##type(c);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      return isw##type(*(prs->wstr + prs->state->poschar));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    return is##type(*(unsigned char *)(prs->str + prs->state->posbyte));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  static int p_isnot##type(TParser *prs) { return !p_is##type(prs); }

/*
 * In C locale with a multibyte encoding, any non-ASCII symbol is considered
 * an alpha character, but not a member of other char classes.
 */
p_iswhat(alnum, 1) p_iswhat(alpha, 1) p_iswhat(digit, 0) p_iswhat(lower, 0) p_iswhat(print, 0) p_iswhat(punct, 0) p_iswhat(space, 0) p_iswhat(upper, 0) p_iswhat(xdigit, 0)

    /* p_iseq should be used only for ascii symbols */

    static int p_iseq(TParser *prs, char c)
{


}

static int
p_isEOF(TParser *prs)
{


}

static int
p_iseqC(TParser *prs)
{

}

static int
p_isneC(TParser *prs)
{

}

static int
p_isascii(TParser *prs)
{

}

static int
p_isasclet(TParser *prs)
{

}

static int
p_isurlchar(TParser *prs)
{




























}

/* deliberately suppress unused-function complaints for the above */
void
_make_compiler_happy(void);
void
_make_compiler_happy(void)
{





















}

static void
SpecialTags(TParser *prs)
{



























}

static void
SpecialFURL(TParser *prs)
{



}

static void
SpecialHyphen(TParser *prs)
{


}

static void
SpecialVerVersion(TParser *prs)
{




}

static int
p_isstophost(TParser *prs)
{






}

static int
p_isignore(TParser *prs)
{

}

static int
p_ishost(TParser *prs)
{

















}

static int
p_isURLPath(TParser *prs)
{


















}

/*
 * returns true if current character has zero display length or
 * it's a special sign in several languages. Such characters
 * aren't a word-breaker although they aren't an isalpha.
 * In beginning of word they aren't a part of it.
 */
static int
p_isspecial(TParser *prs)
{

























































































































































































































































































}

/*
 * Table of state/action of parser
 */

static const TParserStateActionItem actionTPS_Base[] = {{p_isEOF, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '<', A_PUSH, TPS_InTagFirst, 0, NULL}, {p_isignore, 0, A_NEXT, TPS_InSpace, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InAsciiWord, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InWord, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InUnsignedInt, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InSignedIntFirst, 0, NULL}, {p_iseqC, '+', A_PUSH, TPS_InSignedIntFirst, 0, NULL}, {p_iseqC, '&', A_PUSH, TPS_InXMLEntityFirst, 0, NULL}, {p_iseqC, '~', A_PUSH, TPS_InFileTwiddle, 0, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFileFirst, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InPathFirstFirst, 0, NULL}, {NULL, 0, A_NEXT, TPS_InSpace, 0, NULL}};

static const TParserStateActionItem actionTPS_InNumWord[] = {{p_isEOF, 0, A_BINGO, TPS_Base, NUMWORD, NULL}, {p_isalnum, 0, A_NEXT, TPS_InNumWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InNumWord, 0, NULL}, {p_iseqC, '@', A_PUSH, TPS_InEmail, 0, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFileFirst, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InFileNext, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHyphenNumWordFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, NUMWORD, NULL}};

static const TParserStateActionItem actionTPS_InAsciiWord[] = {{p_isEOF, 0, A_BINGO, TPS_Base, ASCIIWORD, NULL}, {p_isasclet, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InHostFirstDomain, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InFileNext, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHyphenAsciiWordFirst, 0, NULL}, {p_iseqC, '_', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '@', A_PUSH, TPS_InEmail, 0, NULL}, {p_iseqC, ':', A_PUSH, TPS_InProtocolFirst, 0, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFileFirst, 0, NULL}, {p_isdigit, 0, A_PUSH, TPS_InHost, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InNumWord, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InWord, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, ASCIIWORD, NULL}};

static const TParserStateActionItem actionTPS_InWord[] = {{p_isEOF, 0, A_BINGO, TPS_Base, WORD_T, NULL}, {p_isalpha, 0, A_NEXT, TPS_Null, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InNumWord, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHyphenWordFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, WORD_T, NULL}};

static const TParserStateActionItem actionTPS_InUnsignedInt[] = {{p_isEOF, 0, A_BINGO, TPS_Base, UNSIGNEDINT, NULL}, {p_isdigit, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InHostFirstDomain, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InUDecimalFirst, 0, NULL}, {p_iseqC, 'e', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {p_iseqC, 'E', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '_', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '@', A_PUSH, TPS_InEmail, 0, NULL}, {p_isasclet, 0, A_PUSH, TPS_InHost, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InNumWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InNumWord, 0, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFileFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, UNSIGNEDINT, NULL}};

static const TParserStateActionItem actionTPS_InSignedIntFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT | A_CLEAR, TPS_InSignedInt, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InSignedInt[] = {{p_isEOF, 0, A_BINGO, TPS_Base, SIGNEDINT, NULL}, {p_isdigit, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InDecimalFirst, 0, NULL}, {p_iseqC, 'e', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {p_iseqC, 'E', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, SIGNEDINT, NULL}};

static const TParserStateActionItem actionTPS_InSpace[] = {{p_isEOF, 0, A_BINGO, TPS_Base, SPACE, NULL}, {p_iseqC, '<', A_BINGO, TPS_Base, SPACE, NULL}, {p_isignore, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '-', A_BINGO, TPS_Base, SPACE, NULL}, {p_iseqC, '+', A_BINGO, TPS_Base, SPACE, NULL}, {p_iseqC, '&', A_BINGO, TPS_Base, SPACE, NULL}, {p_iseqC, '/', A_BINGO, TPS_Base, SPACE, NULL}, {p_isnotalnum, 0, A_NEXT, TPS_InSpace, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, SPACE, NULL}};

static const TParserStateActionItem actionTPS_InUDecimalFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_CLEAR, TPS_InUDecimal, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InUDecimal[] = {{p_isEOF, 0, A_BINGO, TPS_Base, DECIMAL_T, NULL}, {p_isdigit, 0, A_NEXT, TPS_InUDecimal, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InVersionFirst, 0, NULL}, {p_iseqC, 'e', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {p_iseqC, 'E', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, DECIMAL_T, NULL}};

static const TParserStateActionItem actionTPS_InDecimalFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_CLEAR, TPS_InDecimal, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InDecimal[] = {{p_isEOF, 0, A_BINGO, TPS_Base, DECIMAL_T, NULL}, {p_isdigit, 0, A_NEXT, TPS_InDecimal, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InVerVersion, 0, NULL}, {p_iseqC, 'e', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {p_iseqC, 'E', A_PUSH, TPS_InMantissaFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, DECIMAL_T, NULL}};

static const TParserStateActionItem actionTPS_InVerVersion[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_RERUN, TPS_InSVerVersion, 0, SpecialVerVersion}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InSVerVersion[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_BINGO | A_CLRALL, TPS_InUnsignedInt, SPACE, NULL}, {NULL, 0, A_NEXT, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InVersionFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_CLEAR, TPS_InVersion, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InVersion[] = {{p_isEOF, 0, A_BINGO, TPS_Base, VERSIONNUMBER, NULL}, {p_isdigit, 0, A_NEXT, TPS_InVersion, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InVersionFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, VERSIONNUMBER, NULL}};

static const TParserStateActionItem actionTPS_InMantissaFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_CLEAR, TPS_InMantissa, 0, NULL}, {p_iseqC, '+', A_NEXT, TPS_InMantissaSign, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InMantissaSign, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InMantissaSign[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_CLEAR, TPS_InMantissa, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InMantissa[] = {{p_isEOF, 0, A_BINGO, TPS_Base, SCIENTIFIC, NULL}, {p_isdigit, 0, A_NEXT, TPS_InMantissa, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, SCIENTIFIC, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntityFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '#', A_NEXT, TPS_InXMLEntityNumFirst, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, ':', A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_InXMLEntity, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntity[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isalnum, 0, A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, ':', A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InXMLEntity, 0, NULL}, {p_iseqC, ';', A_NEXT, TPS_InXMLEntityEnd, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntityNumFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, 'x', A_NEXT, TPS_InXMLEntityHexNumFirst, 0, NULL}, {p_iseqC, 'X', A_NEXT, TPS_InXMLEntityHexNumFirst, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InXMLEntityNum, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntityHexNumFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isxdigit, 0, A_NEXT, TPS_InXMLEntityHexNum, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntityNum[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InXMLEntityNum, 0, NULL}, {p_iseqC, ';', A_NEXT, TPS_InXMLEntityEnd, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntityHexNum[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isxdigit, 0, A_NEXT, TPS_InXMLEntityHexNum, 0, NULL}, {p_iseqC, ';', A_NEXT, TPS_InXMLEntityEnd, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLEntityEnd[] = {{NULL, 0, A_BINGO | A_CLEAR, TPS_Base, XMLENTITY, NULL}};

static const TParserStateActionItem actionTPS_InTagFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '/', A_PUSH, TPS_InTagCloseFirst, 0, NULL}, {p_iseqC, '!', A_PUSH, TPS_InCommentFirst, 0, NULL}, {p_iseqC, '?', A_PUSH, TPS_InXMLBegin, 0, NULL}, {p_isasclet, 0, A_PUSH, TPS_InTagName, 0, NULL}, {p_iseqC, ':', A_PUSH, TPS_InTagName, 0, NULL}, {p_iseqC, '_', A_PUSH, TPS_InTagName, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InXMLBegin[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL},
    /* <?xml ... */
    /* XXX do we wants states for the m and l ?  Right now this accepts <?xZ */
    {p_iseqC, 'x', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagCloseFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InTagName, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagName[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL},
    /* <br/> case */
    {p_iseqC, '/', A_NEXT, TPS_InTagBeginEnd, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InTagEnd, 0, SpecialTags}, {p_isspace, 0, A_NEXT, TPS_InTag, 0, SpecialTags}, {p_isalnum, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, ':', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_Null, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagBeginEnd[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InTagEnd, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTag[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InTagEnd, 0, SpecialTags}, {p_iseqC, '\'', A_NEXT, TPS_InTagEscapeK, 0, NULL}, {p_iseqC, '"', A_NEXT, TPS_InTagEscapeKK, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '=', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '#', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, ':', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '&', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '?', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '%', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '~', A_NEXT, TPS_Null, 0, NULL}, {p_isspace, 0, A_NEXT, TPS_Null, 0, SpecialTags}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagEscapeK[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '\\', A_PUSH, TPS_InTagBackSleshed, 0, NULL}, {p_iseqC, '\'', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_NEXT, TPS_InTagEscapeK, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagEscapeKK[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '\\', A_PUSH, TPS_InTagBackSleshed, 0, NULL}, {p_iseqC, '"', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_NEXT, TPS_InTagEscapeKK, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagBackSleshed[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {NULL, 0, A_MERGE, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagEnd[] = {{NULL, 0, A_BINGO | A_CLRALL, TPS_Base, TAG_T, NULL}};

static const TParserStateActionItem actionTPS_InCommentFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InCommentLast, 0, NULL},
    /* <!DOCTYPE ...> */
    {p_iseqC, 'D', A_NEXT, TPS_InTag, 0, NULL}, {p_iseqC, 'd', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InCommentLast[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InComment, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InComment[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InCloseCommentFirst, 0, NULL}, {NULL, 0, A_NEXT, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InCloseCommentFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InCloseCommentLast, 0, NULL}, {NULL, 0, A_NEXT, TPS_InComment, 0, NULL}};

static const TParserStateActionItem actionTPS_InCloseCommentLast[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InCommentEnd, 0, NULL}, {NULL, 0, A_NEXT, TPS_InComment, 0, NULL}};

static const TParserStateActionItem actionTPS_InCommentEnd[] = {{NULL, 0, A_BINGO | A_CLRALL, TPS_Base, TAG_T, NULL}};

static const TParserStateActionItem actionTPS_InHostFirstDomain[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHostDomainSecond, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHost, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHostDomainSecond[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHostDomain, 0, NULL}, {p_isdigit, 0, A_PUSH, TPS_InHost, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '_', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InHostFirstDomain, 0, NULL}, {p_iseqC, '@', A_PUSH, TPS_InEmail, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHostDomain[] = {{p_isEOF, 0, A_BINGO | A_CLRALL, TPS_Base, HOST, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHostDomain, 0, NULL}, {p_isdigit, 0, A_PUSH, TPS_InHost, 0, NULL}, {p_iseqC, ':', A_PUSH, TPS_InPortFirst, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '_', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InHostFirstDomain, 0, NULL}, {p_iseqC, '@', A_PUSH, TPS_InEmail, 0, NULL}, {p_isdigit, 0, A_POP, TPS_Null, 0, NULL}, {p_isstophost, 0, A_BINGO | A_CLRALL, TPS_InURLPathStart, HOST, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFURL, 0, NULL}, {NULL, 0, A_BINGO | A_CLRALL, TPS_Base, HOST, NULL}};

static const TParserStateActionItem actionTPS_InPortFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InPort, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InPort[] = {{p_isEOF, 0, A_BINGO | A_CLRALL, TPS_Base, HOST, NULL}, {p_isdigit, 0, A_NEXT, TPS_InPort, 0, NULL}, {p_isstophost, 0, A_BINGO | A_CLRALL, TPS_InURLPathStart, HOST, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFURL, 0, NULL}, {NULL, 0, A_BINGO | A_CLRALL, TPS_Base, HOST, NULL}};

static const TParserStateActionItem actionTPS_InHostFirstAN[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHost, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHost, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHost[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHost, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHost, 0, NULL}, {p_iseqC, '@', A_PUSH, TPS_InEmail, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InHostFirstDomain, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {p_iseqC, '_', A_PUSH, TPS_InHostFirstAN, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InEmail[] = {{p_isstophost, 0, A_POP, TPS_Null, 0, NULL}, {p_ishost, 0, A_BINGO | A_CLRALL, TPS_Base, EMAIL, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InFileFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_InPathFirst, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '~', A_PUSH, TPS_InFileTwiddle, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InFileTwiddle[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_InFileFirst, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InPathFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_InPathSecond, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_InFileFirst, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InPathFirstFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_InPathSecond, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_InFileFirst, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InPathSecond[] = {{p_isEOF, 0, A_BINGO | A_CLEAR, TPS_Base, FILEPATH, NULL}, {p_iseqC, '/', A_NEXT | A_PUSH, TPS_InFileFirst, 0, NULL}, {p_iseqC, '/', A_BINGO | A_CLEAR, TPS_Base, FILEPATH, NULL}, {p_isspace, 0, A_BINGO | A_CLEAR, TPS_Base, FILEPATH, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InFile[] = {{p_isEOF, 0, A_BINGO, TPS_Base, FILEPATH, NULL}, {p_isasclet, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '.', A_PUSH, TPS_InFileNext, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InFile, 0, NULL}, {p_iseqC, '/', A_PUSH, TPS_InFileFirst, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, FILEPATH, NULL}};

static const TParserStateActionItem actionTPS_InFileNext[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_CLEAR, TPS_InFile, 0, NULL}, {p_isdigit, 0, A_CLEAR, TPS_InFile, 0, NULL}, {p_iseqC, '_', A_CLEAR, TPS_InFile, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InURLPathFirst[] = {
    {p_isEOF, 0, A_POP, TPS_Null, 0, NULL},
    {p_isurlchar, 0, A_NEXT, TPS_InURLPath, 0, NULL},
    {NULL, 0, A_POP, TPS_Null, 0, NULL},
};

static const TParserStateActionItem actionTPS_InURLPathStart[] = {{NULL, 0, A_NEXT, TPS_InURLPath, 0, NULL}};

static const TParserStateActionItem actionTPS_InURLPath[] = {{p_isEOF, 0, A_BINGO, TPS_Base, URLPATH, NULL}, {p_isurlchar, 0, A_NEXT, TPS_InURLPath, 0, NULL}, {NULL, 0, A_BINGO, TPS_Base, URLPATH, NULL}};

static const TParserStateActionItem actionTPS_InFURL[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isURLPath, 0, A_BINGO | A_CLRALL, TPS_Base, URL_T, SpecialFURL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InProtocolFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_InProtocolSecond, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InProtocolSecond[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_InProtocolEnd, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InProtocolEnd[] = {{NULL, 0, A_BINGO | A_CLRALL, TPS_Base, PROTOCOL, NULL}};

static const TParserStateActionItem actionTPS_InHyphenAsciiWordFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHyphenAsciiWord, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWord, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenDigitLookahead, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHyphenAsciiWord[] = {{p_isEOF, 0, A_BINGO | A_CLRALL, TPS_InParseHyphen, ASCIIHWORD, SpecialHyphen}, {p_isasclet, 0, A_NEXT, TPS_InHyphenAsciiWord, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenWord, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHyphenAsciiWordFirst, 0, NULL}, {NULL, 0, A_BINGO | A_CLRALL, TPS_InParseHyphen, ASCIIHWORD, SpecialHyphen}};

static const TParserStateActionItem actionTPS_InHyphenWordFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWord, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenDigitLookahead, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHyphenWord[] = {{p_isEOF, 0, A_BINGO | A_CLRALL, TPS_InParseHyphen, HWORD, SpecialHyphen}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenWord, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHyphenWordFirst, 0, NULL}, {NULL, 0, A_BINGO | A_CLRALL, TPS_InParseHyphen, HWORD, SpecialHyphen}};

static const TParserStateActionItem actionTPS_InHyphenNumWordFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenDigitLookahead, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHyphenNumWord[] = {{p_isEOF, 0, A_BINGO | A_CLRALL, TPS_InParseHyphen, NUMHWORD, SpecialHyphen}, {p_isalnum, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InHyphenNumWordFirst, 0, NULL}, {NULL, 0, A_BINGO | A_CLRALL, TPS_InParseHyphen, NUMHWORD, SpecialHyphen}};

static const TParserStateActionItem actionTPS_InHyphenDigitLookahead[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenDigitLookahead, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenNumWord, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InParseHyphen[] = {{p_isEOF, 0, A_RERUN, TPS_Base, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHyphenAsciiWordPart, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWordPart, 0, NULL}, {p_isdigit, 0, A_PUSH, TPS_InHyphenUnsignedInt, 0, NULL}, {p_iseqC, '-', A_PUSH, TPS_InParseHyphenHyphen, 0, NULL}, {NULL, 0, A_RERUN, TPS_Base, 0, NULL}};

static const TParserStateActionItem actionTPS_InParseHyphenHyphen[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isalnum, 0, A_BINGO | A_CLEAR, TPS_InParseHyphen, SPACE, NULL}, {p_isspecial, 0, A_BINGO | A_CLEAR, TPS_InParseHyphen, SPACE, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InHyphenWordPart[] = {{p_isEOF, 0, A_BINGO, TPS_Base, PARTHWORD, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWordPart, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenWordPart, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenNumWordPart, 0, NULL}, {NULL, 0, A_BINGO, TPS_InParseHyphen, PARTHWORD, NULL}};

static const TParserStateActionItem actionTPS_InHyphenAsciiWordPart[] = {{p_isEOF, 0, A_BINGO, TPS_Base, ASCIIPARTHWORD, NULL}, {p_isasclet, 0, A_NEXT, TPS_InHyphenAsciiWordPart, 0, NULL}, {p_isalpha, 0, A_NEXT, TPS_InHyphenWordPart, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenWordPart, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_InHyphenNumWordPart, 0, NULL}, {NULL, 0, A_BINGO, TPS_InParseHyphen, ASCIIPARTHWORD, NULL}};

static const TParserStateActionItem actionTPS_InHyphenNumWordPart[] = {{p_isEOF, 0, A_BINGO, TPS_Base, NUMPARTHWORD, NULL}, {p_isalnum, 0, A_NEXT, TPS_InHyphenNumWordPart, 0, NULL}, {p_isspecial, 0, A_NEXT, TPS_InHyphenNumWordPart, 0, NULL}, {NULL, 0, A_BINGO, TPS_InParseHyphen, NUMPARTHWORD, NULL}};

static const TParserStateActionItem actionTPS_InHyphenUnsignedInt[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_Null, 0, NULL}, {p_isalpha, 0, A_CLEAR, TPS_InHyphenNumWordPart, 0, NULL}, {p_isspecial, 0, A_CLEAR, TPS_InHyphenNumWordPart, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

/*
 * main table of per-state parser actions
 */
typedef struct
{
  const TParserStateActionItem *action; /* the actual state info */
  TParserState state;                   /* only for Assert crosscheck */
#ifdef WPARSER_TRACE
  const char *state_name; /* only for debug printout */
#endif
} TParserStateAction;

#ifdef WPARSER_TRACE
#define TPARSERSTATEACTION(state)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    CppConcat(action, state), state, CppAsString(state)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  }
#else
#define TPARSERSTATEACTION(state)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    CppConcat(action, state), state                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  }
#endif

/*
 * order must be the same as in typedef enum {} TParserState!!
 */

static const TParserStateAction Actions[] = {TPARSERSTATEACTION(TPS_Base), TPARSERSTATEACTION(TPS_InNumWord), TPARSERSTATEACTION(TPS_InAsciiWord), TPARSERSTATEACTION(TPS_InWord), TPARSERSTATEACTION(TPS_InUnsignedInt), TPARSERSTATEACTION(TPS_InSignedIntFirst), TPARSERSTATEACTION(TPS_InSignedInt), TPARSERSTATEACTION(TPS_InSpace), TPARSERSTATEACTION(TPS_InUDecimalFirst), TPARSERSTATEACTION(TPS_InUDecimal), TPARSERSTATEACTION(TPS_InDecimalFirst), TPARSERSTATEACTION(TPS_InDecimal), TPARSERSTATEACTION(TPS_InVerVersion), TPARSERSTATEACTION(TPS_InSVerVersion), TPARSERSTATEACTION(TPS_InVersionFirst), TPARSERSTATEACTION(TPS_InVersion), TPARSERSTATEACTION(TPS_InMantissaFirst), TPARSERSTATEACTION(TPS_InMantissaSign), TPARSERSTATEACTION(TPS_InMantissa), TPARSERSTATEACTION(TPS_InXMLEntityFirst), TPARSERSTATEACTION(TPS_InXMLEntity), TPARSERSTATEACTION(TPS_InXMLEntityNumFirst), TPARSERSTATEACTION(TPS_InXMLEntityNum), TPARSERSTATEACTION(TPS_InXMLEntityHexNumFirst),
    TPARSERSTATEACTION(TPS_InXMLEntityHexNum), TPARSERSTATEACTION(TPS_InXMLEntityEnd), TPARSERSTATEACTION(TPS_InTagFirst), TPARSERSTATEACTION(TPS_InXMLBegin), TPARSERSTATEACTION(TPS_InTagCloseFirst), TPARSERSTATEACTION(TPS_InTagName), TPARSERSTATEACTION(TPS_InTagBeginEnd), TPARSERSTATEACTION(TPS_InTag), TPARSERSTATEACTION(TPS_InTagEscapeK), TPARSERSTATEACTION(TPS_InTagEscapeKK), TPARSERSTATEACTION(TPS_InTagBackSleshed), TPARSERSTATEACTION(TPS_InTagEnd), TPARSERSTATEACTION(TPS_InCommentFirst), TPARSERSTATEACTION(TPS_InCommentLast), TPARSERSTATEACTION(TPS_InComment), TPARSERSTATEACTION(TPS_InCloseCommentFirst), TPARSERSTATEACTION(TPS_InCloseCommentLast), TPARSERSTATEACTION(TPS_InCommentEnd), TPARSERSTATEACTION(TPS_InHostFirstDomain), TPARSERSTATEACTION(TPS_InHostDomainSecond), TPARSERSTATEACTION(TPS_InHostDomain), TPARSERSTATEACTION(TPS_InPortFirst), TPARSERSTATEACTION(TPS_InPort), TPARSERSTATEACTION(TPS_InHostFirstAN), TPARSERSTATEACTION(TPS_InHost), TPARSERSTATEACTION(TPS_InEmail),
    TPARSERSTATEACTION(TPS_InFileFirst), TPARSERSTATEACTION(TPS_InFileTwiddle), TPARSERSTATEACTION(TPS_InPathFirst), TPARSERSTATEACTION(TPS_InPathFirstFirst), TPARSERSTATEACTION(TPS_InPathSecond), TPARSERSTATEACTION(TPS_InFile), TPARSERSTATEACTION(TPS_InFileNext), TPARSERSTATEACTION(TPS_InURLPathFirst), TPARSERSTATEACTION(TPS_InURLPathStart), TPARSERSTATEACTION(TPS_InURLPath), TPARSERSTATEACTION(TPS_InFURL), TPARSERSTATEACTION(TPS_InProtocolFirst), TPARSERSTATEACTION(TPS_InProtocolSecond), TPARSERSTATEACTION(TPS_InProtocolEnd), TPARSERSTATEACTION(TPS_InHyphenAsciiWordFirst), TPARSERSTATEACTION(TPS_InHyphenAsciiWord), TPARSERSTATEACTION(TPS_InHyphenWordFirst), TPARSERSTATEACTION(TPS_InHyphenWord), TPARSERSTATEACTION(TPS_InHyphenNumWordFirst), TPARSERSTATEACTION(TPS_InHyphenNumWord), TPARSERSTATEACTION(TPS_InHyphenDigitLookahead), TPARSERSTATEACTION(TPS_InParseHyphen), TPARSERSTATEACTION(TPS_InParseHyphenHyphen), TPARSERSTATEACTION(TPS_InHyphenWordPart),
    TPARSERSTATEACTION(TPS_InHyphenAsciiWordPart), TPARSERSTATEACTION(TPS_InHyphenNumWordPart), TPARSERSTATEACTION(TPS_InHyphenUnsignedInt)};

static bool
TParserGet(TParser *prs)
{








































































































































































}

Datum
prsd_lextype(PG_FUNCTION_ARGS)
{













}

Datum
prsd_start(PG_FUNCTION_ARGS)
{

}

Datum
prsd_nexttoken(PG_FUNCTION_ARGS)
{













}

Datum
prsd_end(PG_FUNCTION_ARGS)
{




}

/*
 * ts_headline support begins here
 */

/* token type classification macros */
#define TS_IDIGNORE(x) ((x) == TAG_T || (x) == PROTOCOL || (x) == SPACE || (x) == XMLENTITY)
#define HLIDREPLACE(x) ((x) == TAG_T)
#define HLIDSKIP(x) ((x) == URL_T || (x) == NUMHWORD || (x) == ASCIIHWORD || (x) == HWORD)
#define XMLHLIDSKIP(x) ((x) == URL_T || (x) == NUMHWORD || (x) == ASCIIHWORD || (x) == HWORD)
#define NONWORDTOKEN(x) ((x) == SPACE || HLIDREPLACE(x) || HLIDSKIP(x))
#define NOENDTOKEN(x) (NONWORDTOKEN(x) || (x) == SCIENTIFIC || (x) == VERSIONNUMBER || (x) == DECIMAL_T || (x) == SIGNEDINT || (x) == UNSIGNEDINT || TS_IDIGNORE(x))

/*
 * Macros useful in headline selection.  These rely on availability of
 * "HeadlineParsedText *prs" describing some text, and "int shortword"
 * describing the "short word" length parameter.
 */

/* Interesting words are non-repeated search terms */
#define INTERESTINGWORD(j) (prs->words[j].item && !prs->words[j].repeated)

/* Don't want to end at a non-word or a short word, unless interesting */
#define BADENDPOINT(j) ((NOENDTOKEN(prs->words[j].type) || prs->words[j].len <= shortword) && !INTERESTINGWORD(j))

typedef struct
{
  /* one cover (well, really one fragment) for mark_hl_fragments */
  int32 startpos; /* fragment's starting word index */
  int32 endpos;   /* ending word index (inclusive) */
  int32 poslen;   /* number of interesting words */
  int32 curlen;   /* total number of words */
  bool chosen;    /* chosen? */
  bool excluded;  /* excluded? */
} CoverPos;

typedef struct
{
  /* callback data for checkcondition_HL */
  HeadlineWordEntry *words;
  int len;
} hlCheck;

/*
 * TS_execute callback for matching a tsquery operand to headline words
 */
static bool
checkcondition_HL(void *opaque, QueryOperand *val, ExecPhraseData *data)
{


































}

/*
 * hlFirstIndex: find first index >= pos containing any word used in query
 *
 * Returns -1 if no such index
 */
static int
hlFirstIndex(HeadlineParsedText *prs, int pos)
{










}

/*
 * hlCover: try to find a substring of prs' word list that satisfies query
 *
 * At entry, *p must be the first word index to consider (initialize this
 * to zero, or to the next index after a previous successful search).
 * We will consider all substrings starting at or after that word, and
 * containing no more than max_cover words.  (We need a length limit to
 * keep this from taking O(N^2) time for a long document with many query
 * words but few complete matches.  Actually, since checkcondition_HL is
 * roughly O(N) in the length of the substring being checked, it's even
 * worse than that.)
 *
 * On success, sets *p to first word index and *q to last word index of the
 * cover substring, and returns true.
 *
 * The result is a minimal cover, in the sense that both *p and *q will be
 * words used in the query.
 */
static bool
hlCover(HeadlineParsedText *prs, TSQuery query, int max_cover, int *p, int *q)
{














































}

/*
 * Apply suitable highlight marking to words selected by headline selector
 *
 * The words from startpos to endpos inclusive are marked per highlightall
 */
static void
mark_fragment(HeadlineParsedText *prs, bool highlightall, int startpos, int endpos)
{





























}

/*
 * split a cover substring into fragments not longer than max_words
 *
 * At entry, *startpos and *endpos are the (remaining) bounds of the cover
 * substring.  They are updated to hold the bounds of the next fragment.
 *
 * *curlen and *poslen are set to the fragment's length, in words and
 * interesting words respectively.
 */
static void
get_next_fragment(HeadlineParsedText *prs, int *startpos, int *endpos, int *curlen, int *poslen, int max_words)
{

















































}

/*
 * Headline selector used when MaxFragments > 0
 *
 * Note: in this mode, highlightall is disregarded for phrase selection;
 * it only controls presentation details.
 */
static void
mark_hl_fragments(HeadlineParsedText *prs, TSQuery query, bool highlightall, int shortword, int min_words, int max_words, int max_fragments, int max_cover)
{




































































































































































}

/*
 * Headline selector used when MaxFragments == 0
 */
static void
mark_hl_words(HeadlineParsedText *prs, TSQuery query, bool highlightall, int shortword, int min_words, int max_words, int max_cover)
{


















































































































































































}

/*
 * Default parser's prsheadline function
 */
Datum
prsd_headline(PG_FUNCTION_ARGS)
{


























































































































}