                                                                            
   
                 
                               
   
                                                                         
   
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_collation.h"
#include "commands/defrem.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_type.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

                                                    
                           

                             

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
  TPS_Null                              
} TParserState;

                         
struct TParser;

typedef int (*TParserCharTest)(struct TParser *);                        
                                                                     
typedef void (*TParserSpecial)(struct TParser *);                        
                                                                        

typedef struct
{
  TParserCharTest isclass;
  char c;
  uint16 flags;
  TParserState tostate;
  int type;
  TParserSpecial special;
} TParserStateActionItem;

                                               
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
  int posbyte;                                       
  int poschar;                                            
  int charlen;                                  
  int lenbytetoken;                                      
  int lenchartoken;                   
  TParserState state;
  struct TParserPosition *prev;
  const TParserStateActionItem *pushedAtAction;
} TParserPosition;

typedef struct TParser
{
                                       
  char *str;                              
  int lenstr;                               
  wchar_t *wstr;                               
  pg_wchar *pgwstr;                                         
  bool usewide;

                      
  int charmaxlen;
  TParserPosition *state;
  bool ignore;
  bool wanthost;

                  
  char c;

           
  char *token;
  int lenbytetoken;
  int lenchartoken;
  int type;
} TParser;

                        
static bool
TParserGet(TParser *prs);

static TParserPosition *
newTParserPosition(TParserPosition *prev)
{
  TParserPosition *res = (TParserPosition *)palloc(sizeof(TParserPosition));

  if (prev)
  {
    memcpy(res, prev, sizeof(TParserPosition));
  }
  else
  {
    memset(res, 0, sizeof(TParserPosition));
  }

  res->prev = prev;

  res->pushedAtAction = NULL;

  return res;
}

static TParser *
TParserInit(char *str, int len)
{
  TParser *prs = (TParser *)palloc0(sizeof(TParser));

  prs->charmaxlen = pg_database_encoding_max_length();
  prs->str = str;
  prs->lenstr = len;

     
                                                           
     
  if (prs->charmaxlen > 1)
  {
    Oid collation = DEFAULT_COLLATION_OID;           
    pg_locale_t mylocale = 0;                        

    prs->usewide = true;
    if (lc_ctype_is_c(collation))
    {
         
                                                                         
                                           
         
      prs->pgwstr = (pg_wchar *)palloc(sizeof(pg_wchar) * (prs->lenstr + 1));
      pg_mb2wchar_with_len(prs->str, prs->pgwstr, prs->lenstr);
    }
    else
    {
      prs->wstr = (wchar_t *)palloc(sizeof(wchar_t) * (prs->lenstr + 1));
      char2wchar(prs->wstr, prs->lenstr + 1, prs->str, prs->lenstr, mylocale);
    }
  }
  else
  {
    prs->usewide = false;
  }

  prs->state = newTParserPosition(NULL);
  prs->state->state = TPS_Base;

#ifdef WPARSER_TRACE

     
                                                                           
                                                                         
                                                                
     
  fprintf(stderr, "parsing \"%.*s\"\n", len, str);
#endif

  return prs;
}

   
                                                            
                                                                      
                                                                      
                                                              
                                                                  
                                                          
                                                                     
   
                                                                      
   
static TParser *
TParserCopyInit(const TParser *orig)
{
  TParser *prs = (TParser *)palloc0(sizeof(TParser));

  prs->charmaxlen = orig->charmaxlen;
  prs->str = orig->str + orig->state->posbyte;
  prs->lenstr = orig->lenstr - orig->state->posbyte;
  prs->usewide = orig->usewide;

  if (orig->pgwstr)
  {
    prs->pgwstr = orig->pgwstr + orig->state->poschar;
  }
  if (orig->wstr)
  {
    prs->wstr = orig->wstr + orig->state->poschar;
  }

  prs->state = newTParserPosition(NULL);
  prs->state->state = TPS_Base;

#ifdef WPARSER_TRACE
                                 
  fprintf(stderr, "parsing copy of \"%.*s\"\n", prs->lenstr, prs->str);
#endif

  return prs;
}

static void
TParserClose(TParser *prs)
{
  while (prs->state)
  {
    TParserPosition *ptr = prs->state->prev;

    pfree(prs->state);
    prs->state = ptr;
  }

  if (prs->wstr)
  {
    pfree(prs->wstr);
  }
  if (prs->pgwstr)
  {
    pfree(prs->pgwstr);
  }

#ifdef WPARSER_TRACE
  fprintf(stderr, "closing parser\n");
#endif
  pfree(prs);
}

   
                                               
   
static void
TParserCopyClose(TParser *prs)
{
  while (prs->state)
  {
    TParserPosition *ptr = prs->state->prev;

    pfree(prs->state);
    prs->state = ptr;
  }

#ifdef WPARSER_TRACE
  fprintf(stderr, "closing parser copy\n");
#endif
  pfree(prs);
}

   
                                                                   
                                                           
                                                                 
                           
                                                        
                      
                                                        
   

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

   
                                                                             
                                                               
   
p_iswhat(alnum, 1) p_iswhat(alpha, 1) p_iswhat(digit, 0) p_iswhat(lower, 0) p_iswhat(print, 0) p_iswhat(punct, 0) p_iswhat(space, 0) p_iswhat(upper, 0) p_iswhat(xdigit, 0)

                                                      

    static int p_iseq(TParser *prs, char c)
{
  Assert(prs->state);
  return ((prs->state->charlen == 1 && *(prs->str + prs->state->posbyte) == c)) ? 1 : 0;
}

static int
p_isEOF(TParser *prs)
{
  Assert(prs->state);
  return (prs->state->posbyte == prs->lenstr || prs->state->charlen == 0) ? 1 : 0;
}

static int
p_iseqC(TParser *prs)
{
  return p_iseq(prs, prs->c);
}

static int
p_isneC(TParser *prs)
{
  return !p_iseq(prs, prs->c);
}

static int
p_isascii(TParser *prs)
{
  return (prs->state->charlen == 1 && isascii((unsigned char)*(prs->str + prs->state->posbyte))) ? 1 : 0;
}

static int
p_isasclet(TParser *prs)
{
  return (p_isascii(prs) && p_isalpha(prs)) ? 1 : 0;
}

static int
p_isurlchar(TParser *prs)
{
  char ch;

                               
  if (prs->state->charlen != 1)
  {
    return 0;
  }
  ch = *(prs->str + prs->state->posbyte);
                                       
  if (ch <= 0x20 || ch >= 0x7F)
  {
    return 0;
  }
                                                
  switch (ch)
  {
  case '"':
  case '<':
  case '>':
  case '\\':
  case '^':
  case '`':
  case '{':
  case '|':
  case '}':
    return 0;
  }
  return 1;
}

                                                                    
void
_make_compiler_happy(void);
void
_make_compiler_happy(void)
{
  p_isalnum(NULL);
  p_isnotalnum(NULL);
  p_isalpha(NULL);
  p_isnotalpha(NULL);
  p_isdigit(NULL);
  p_isnotdigit(NULL);
  p_islower(NULL);
  p_isnotlower(NULL);
  p_isprint(NULL);
  p_isnotprint(NULL);
  p_ispunct(NULL);
  p_isnotpunct(NULL);
  p_isspace(NULL);
  p_isnotspace(NULL);
  p_isupper(NULL);
  p_isnotupper(NULL);
  p_isxdigit(NULL);
  p_isnotxdigit(NULL);
  p_isEOF(NULL);
  p_iseqC(NULL);
  p_isneC(NULL);
}

static void
SpecialTags(TParser *prs)
{
  switch (prs->state->lenchartoken)
  {
  case 8:               
    if (pg_strncasecmp(prs->token, "</script", 8) == 0)
    {
      prs->ignore = false;
    }
    break;
  case 7:                         
    if (pg_strncasecmp(prs->token, "</style", 7) == 0)
    {
      prs->ignore = false;
    }
    else if (pg_strncasecmp(prs->token, "<script", 7) == 0)
    {
      prs->ignore = true;
    }
    break;
  case 6:             
    if (pg_strncasecmp(prs->token, "<style", 6) == 0)
    {
      prs->ignore = true;
    }
    break;
  default:
    break;
  }
}

static void
SpecialFURL(TParser *prs)
{
  prs->wanthost = true;
  prs->state->posbyte -= prs->state->lenbytetoken;
  prs->state->poschar -= prs->state->lenchartoken;
}

static void
SpecialHyphen(TParser *prs)
{
  prs->state->posbyte -= prs->state->lenbytetoken;
  prs->state->poschar -= prs->state->lenchartoken;
}

static void
SpecialVerVersion(TParser *prs)
{
  prs->state->posbyte -= prs->state->lenbytetoken;
  prs->state->poschar -= prs->state->lenchartoken;
  prs->state->lenbytetoken = 0;
  prs->state->lenchartoken = 0;
}

static int
p_isstophost(TParser *prs)
{
  if (prs->wanthost)
  {
    prs->wanthost = false;
    return 1;
  }
  return 0;
}

static int
p_isignore(TParser *prs)
{
  return (prs->ignore) ? 1 : 0;
}

static int
p_ishost(TParser *prs)
{
  TParser *tmpprs = TParserCopyInit(prs);
  int res = 0;

  tmpprs->wanthost = true;

  if (TParserGet(tmpprs) && tmpprs->type == HOST)
  {
    prs->state->posbyte += tmpprs->lenbytetoken;
    prs->state->poschar += tmpprs->lenchartoken;
    prs->state->lenbytetoken += tmpprs->lenbytetoken;
    prs->state->lenchartoken += tmpprs->lenchartoken;
    prs->state->charlen = tmpprs->state->charlen;
    res = 1;
  }
  TParserCopyClose(tmpprs);

  return res;
}

static int
p_isURLPath(TParser *prs)
{
  TParser *tmpprs = TParserCopyInit(prs);
  int res = 0;

  tmpprs->state = newTParserPosition(tmpprs->state);
  tmpprs->state->state = TPS_InURLPathFirst;

  if (TParserGet(tmpprs) && tmpprs->type == URLPATH)
  {
    prs->state->posbyte += tmpprs->lenbytetoken;
    prs->state->poschar += tmpprs->lenchartoken;
    prs->state->lenbytetoken += tmpprs->lenbytetoken;
    prs->state->lenchartoken += tmpprs->lenchartoken;
    prs->state->charlen = tmpprs->state->charlen;
    res = 1;
  }
  TParserCopyClose(tmpprs);

  return res;
}

   
                                                                
                                                             
                                                          
                                                  
   
static int
p_isspecial(TParser *prs)
{
     
                                                                      
     
  if (pg_dsplen(prs->str + prs->state->posbyte) == 0)
  {
    return 1;
  }

     
                                                                       
                                                                          
                                                                     
                                           
     
  if (GetDatabaseEncoding() == PG_UTF8 && prs->usewide)
  {
    static const pg_wchar strange_letter[] = {
           
                                                            
           
        0x0903,                              
        0x093E,                               
        0x093F,                              
        0x0940,                               
        0x0949,                                     
        0x094A,                                    
        0x094B,                              
        0x094C,                               
        0x0982,                            
        0x0983,                           
        0x09BE,                            
        0x09BF,                           
        0x09C0,                            
        0x09C7,                           
        0x09C8,                            
        0x09CB,                           
        0x09CC,                            
        0x09D7,                             
        0x0A03,                            
        0x0A3E,                             
        0x0A3F,                            
        0x0A40,                             
        0x0A83,                            
        0x0ABE,                             
        0x0ABF,                            
        0x0AC0,                             
        0x0AC9,                                   
        0x0ACB,                            
        0x0ACC,                             
        0x0B02,                          
        0x0B03,                         
        0x0B3E,                          
        0x0B40,                          
        0x0B47,                         
        0x0B48,                          
        0x0B4B,                         
        0x0B4C,                          
        0x0B57,                           
        0x0BBE,                          
        0x0BBF,                         
        0x0BC1,                         
        0x0BC2,                          
        0x0BC6,                         
        0x0BC7,                          
        0x0BC8,                          
        0x0BCA,                         
        0x0BCB,                          
        0x0BCC,                          
        0x0BD7,                           
        0x0C01,                              
        0x0C02,                           
        0x0C03,                          
        0x0C41,                          
        0x0C42,                           
        0x0C43,                                  
        0x0C44,                                   
        0x0C82,                            
        0x0C83,                           
        0x0CBE,                            
        0x0CC0,                            
        0x0CC1,                           
        0x0CC2,                            
        0x0CC3,                                   
        0x0CC4,                                    
        0x0CC7,                            
        0x0CC8,                            
        0x0CCA,                           
        0x0CCB,                            
        0x0CD5,                          
        0x0CD6,                             
        0x0D02,                              
        0x0D03,                             
        0x0D3E,                              
        0x0D3F,                             
        0x0D40,                              
        0x0D46,                             
        0x0D47,                              
        0x0D48,                              
        0x0D4A,                             
        0x0D4B,                              
        0x0D4C,                              
        0x0D57,                               
        0x0D82,                              
        0x0D83,                             
        0x0DCF,                                    
        0x0DD0,                                          
        0x0DD1,                                         
        0x0DD8,                                      
        0x0DD9,                                 
        0x0DDA,                                      
        0x0DDB,                                    
        0x0DDC,                                                
        0x0DDD,                                        
                                
        0x0DDE,                                                 
        0x0DDF,                                     
        0x0DF2,                                           
        0x0DF3,                                          
        0x0F3E,                             
        0x0F3F,                             
        0x0F7F,                             
        0x102B,                                 
        0x102C,                            
        0x1031,                           
        0x1038,                           
        0x103B,                                       
        0x103C,                                       
        0x1056,                                   
        0x1057,                                    
        0x1062,                                       
        0x1063,                                         
        0x1064,                                          
        0x1067,                                              
        0x1068,                                              
        0x1069,                                            
        0x106A,                                            
        0x106B,                                            
        0x106C,                                            
        0x106D,                                            
        0x1083,                                 
        0x1084,                                
        0x1087,                               
        0x1088,                               
        0x1089,                               
        0x108A,                               
        0x108B,                                       
        0x108C,                                       
        0x108F,                                        
        0x17B6,                          
        0x17BE,                          
        0x17BF,                          
        0x17C0,                          
        0x17C1,                         
        0x17C2,                          
        0x17C3,                          
        0x17C4,                          
        0x17C5,                          
        0x17C7,                         
        0x17C8,                               
        0x1923,                          
        0x1924,                          
        0x1925,                          
        0x1926,                          
        0x1929,                                
        0x192A,                                
        0x192B,                                
        0x1930,                            
        0x1931,                             
        0x1933,                            
        0x1934,                            
        0x1935,                            
        0x1936,                            
        0x1937,                            
        0x1938,                            
        0x19B0,                                             
        0x19B1,                                
        0x19B2,                                
        0x19B3,                               
        0x19B4,                                
        0x19B5,                               
        0x19B6,                                
        0x19B7,                               
        0x19B8,                                
        0x19B9,                                
        0x19BA,                                
        0x19BB,                                 
        0x19BC,                                
        0x19BD,                                
        0x19BE,                                 
        0x19BF,                                 
        0x19C0,                                
        0x19C8,                              
        0x19C9,                              
        0x1A19,                            
        0x1A1A,                            
        0x1A1B,                             
        0x1B04,                          
        0x1B35,                                 
        0x1B3B,                                         
        0x1B3D,                                          
        0x1B3E,                                 
        0x1B3F,                                      
        0x1B40,                                        
        0x1B41,                                             
        0x1B43,                                       
        0x1B44,                         
        0x1B82,                               
        0x1BA1,                                         
        0x1BA6,                                      
        0x1BA7,                                    
        0x1BAA,                             
        0x1C24,                                 
        0x1C25,                                 
        0x1C26,                           
        0x1C27,                          
        0x1C28,                          
        0x1C29,                           
        0x1C2A,                          
        0x1C2B,                           
        0x1C34,                                    
        0x1C35,                                 
        0xA823,                                
        0xA824,                                
        0xA827,                                 
        0xA880,                               
        0xA881,                              
        0xA8B4,                                      
        0xA8B5,                               
        0xA8B6,                              
        0xA8B7,                               
        0xA8B8,                              
        0xA8B9,                               
        0xA8BA,                                      
        0xA8BB,                                       
        0xA8BC,                                      
        0xA8BD,                                       
        0xA8BE,                              
        0xA8BF,                               
        0xA8C0,                               
        0xA8C1,                              
        0xA8C2,                               
        0xA8C3,                               
        0xA952,                              
        0xA953,                    
        0xAA2F,                        
        0xAA30,                         
        0xAA33,                             
        0xAA34,                             
        0xAA4D                                   
    };
    const pg_wchar *StopLow = strange_letter, *StopHigh = strange_letter + lengthof(strange_letter), *StopMiddle;
    pg_wchar c;

    if (prs->pgwstr)
    {
      c = *(prs->pgwstr + prs->state->poschar);
    }
    else
    {
      c = (pg_wchar) * (prs->wstr + prs->state->poschar);
    }

    while (StopLow < StopHigh)
    {
      StopMiddle = StopLow + ((StopHigh - StopLow) >> 1);
      if (*StopMiddle == c)
      {
        return 1;
      }
      else if (*StopMiddle < c)
      {
        StopLow = StopMiddle + 1;
      }
      else
      {
        StopHigh = StopMiddle;
      }
    }
  }

  return 0;
}

   
                                   
   

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
                   
                                                                               
    {p_iseqC, 'x', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagCloseFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_InTagName, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagName[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL},
                    
    {p_iseqC, '/', A_NEXT, TPS_InTagBeginEnd, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InTagEnd, 0, SpecialTags}, {p_isspace, 0, A_NEXT, TPS_InTag, 0, SpecialTags}, {p_isalnum, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, ':', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_Null, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagBeginEnd[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InTagEnd, 0, NULL}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTag[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '>', A_NEXT, TPS_InTagEnd, 0, SpecialTags}, {p_iseqC, '\'', A_NEXT, TPS_InTagEscapeK, 0, NULL}, {p_iseqC, '"', A_NEXT, TPS_InTagEscapeKK, 0, NULL}, {p_isasclet, 0, A_NEXT, TPS_Null, 0, NULL}, {p_isdigit, 0, A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '=', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '_', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '#', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '/', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, ':', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '.', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '&', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '?', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '%', A_NEXT, TPS_Null, 0, NULL}, {p_iseqC, '~', A_NEXT, TPS_Null, 0, NULL}, {p_isspace, 0, A_NEXT, TPS_Null, 0, SpecialTags}, {NULL, 0, A_POP, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagEscapeK[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '\\', A_PUSH, TPS_InTagBackSleshed, 0, NULL}, {p_iseqC, '\'', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_NEXT, TPS_InTagEscapeK, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagEscapeKK[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '\\', A_PUSH, TPS_InTagBackSleshed, 0, NULL}, {p_iseqC, '"', A_NEXT, TPS_InTag, 0, NULL}, {NULL, 0, A_NEXT, TPS_InTagEscapeKK, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagBackSleshed[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {NULL, 0, A_MERGE, TPS_Null, 0, NULL}};

static const TParserStateActionItem actionTPS_InTagEnd[] = {{NULL, 0, A_BINGO | A_CLRALL, TPS_Base, TAG_T, NULL}};

static const TParserStateActionItem actionTPS_InCommentFirst[] = {{p_isEOF, 0, A_POP, TPS_Null, 0, NULL}, {p_iseqC, '-', A_NEXT, TPS_InCommentLast, 0, NULL},
                        
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

   
                                          
   
typedef struct
{
  const TParserStateActionItem *action;                            
  TParserState state;                                                   
#ifdef WPARSER_TRACE
  const char *state_name;                              
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

   
                                                               
   

static const TParserStateAction Actions[] = {TPARSERSTATEACTION(TPS_Base), TPARSERSTATEACTION(TPS_InNumWord), TPARSERSTATEACTION(TPS_InAsciiWord), TPARSERSTATEACTION(TPS_InWord), TPARSERSTATEACTION(TPS_InUnsignedInt), TPARSERSTATEACTION(TPS_InSignedIntFirst), TPARSERSTATEACTION(TPS_InSignedInt), TPARSERSTATEACTION(TPS_InSpace), TPARSERSTATEACTION(TPS_InUDecimalFirst), TPARSERSTATEACTION(TPS_InUDecimal), TPARSERSTATEACTION(TPS_InDecimalFirst), TPARSERSTATEACTION(TPS_InDecimal), TPARSERSTATEACTION(TPS_InVerVersion), TPARSERSTATEACTION(TPS_InSVerVersion), TPARSERSTATEACTION(TPS_InVersionFirst), TPARSERSTATEACTION(TPS_InVersion), TPARSERSTATEACTION(TPS_InMantissaFirst), TPARSERSTATEACTION(TPS_InMantissaSign), TPARSERSTATEACTION(TPS_InMantissa), TPARSERSTATEACTION(TPS_InXMLEntityFirst), TPARSERSTATEACTION(TPS_InXMLEntity), TPARSERSTATEACTION(TPS_InXMLEntityNumFirst), TPARSERSTATEACTION(TPS_InXMLEntityNum), TPARSERSTATEACTION(TPS_InXMLEntityHexNumFirst),
    TPARSERSTATEACTION(TPS_InXMLEntityHexNum), TPARSERSTATEACTION(TPS_InXMLEntityEnd), TPARSERSTATEACTION(TPS_InTagFirst), TPARSERSTATEACTION(TPS_InXMLBegin), TPARSERSTATEACTION(TPS_InTagCloseFirst), TPARSERSTATEACTION(TPS_InTagName), TPARSERSTATEACTION(TPS_InTagBeginEnd), TPARSERSTATEACTION(TPS_InTag), TPARSERSTATEACTION(TPS_InTagEscapeK), TPARSERSTATEACTION(TPS_InTagEscapeKK), TPARSERSTATEACTION(TPS_InTagBackSleshed), TPARSERSTATEACTION(TPS_InTagEnd), TPARSERSTATEACTION(TPS_InCommentFirst), TPARSERSTATEACTION(TPS_InCommentLast), TPARSERSTATEACTION(TPS_InComment), TPARSERSTATEACTION(TPS_InCloseCommentFirst), TPARSERSTATEACTION(TPS_InCloseCommentLast), TPARSERSTATEACTION(TPS_InCommentEnd), TPARSERSTATEACTION(TPS_InHostFirstDomain), TPARSERSTATEACTION(TPS_InHostDomainSecond), TPARSERSTATEACTION(TPS_InHostDomain), TPARSERSTATEACTION(TPS_InPortFirst), TPARSERSTATEACTION(TPS_InPort), TPARSERSTATEACTION(TPS_InHostFirstAN), TPARSERSTATEACTION(TPS_InHost), TPARSERSTATEACTION(TPS_InEmail),
    TPARSERSTATEACTION(TPS_InFileFirst), TPARSERSTATEACTION(TPS_InFileTwiddle), TPARSERSTATEACTION(TPS_InPathFirst), TPARSERSTATEACTION(TPS_InPathFirstFirst), TPARSERSTATEACTION(TPS_InPathSecond), TPARSERSTATEACTION(TPS_InFile), TPARSERSTATEACTION(TPS_InFileNext), TPARSERSTATEACTION(TPS_InURLPathFirst), TPARSERSTATEACTION(TPS_InURLPathStart), TPARSERSTATEACTION(TPS_InURLPath), TPARSERSTATEACTION(TPS_InFURL), TPARSERSTATEACTION(TPS_InProtocolFirst), TPARSERSTATEACTION(TPS_InProtocolSecond), TPARSERSTATEACTION(TPS_InProtocolEnd), TPARSERSTATEACTION(TPS_InHyphenAsciiWordFirst), TPARSERSTATEACTION(TPS_InHyphenAsciiWord), TPARSERSTATEACTION(TPS_InHyphenWordFirst), TPARSERSTATEACTION(TPS_InHyphenWord), TPARSERSTATEACTION(TPS_InHyphenNumWordFirst), TPARSERSTATEACTION(TPS_InHyphenNumWord), TPARSERSTATEACTION(TPS_InHyphenDigitLookahead), TPARSERSTATEACTION(TPS_InParseHyphen), TPARSERSTATEACTION(TPS_InParseHyphenHyphen), TPARSERSTATEACTION(TPS_InHyphenWordPart),
    TPARSERSTATEACTION(TPS_InHyphenAsciiWordPart), TPARSERSTATEACTION(TPS_InHyphenNumWordPart), TPARSERSTATEACTION(TPS_InHyphenUnsignedInt)};

static bool
TParserGet(TParser *prs)
{
  const TParserStateActionItem *item = NULL;

  Assert(prs->state);

  if (prs->state->posbyte >= prs->lenstr)
  {
    return false;
  }

  prs->token = prs->str + prs->state->posbyte;
  prs->state->pushedAtAction = NULL;

                      
  while (prs->state->posbyte <= prs->lenstr)
  {
    if (prs->state->posbyte == prs->lenstr)
    {
      prs->state->charlen = 0;
    }
    else
    {
      prs->state->charlen = (prs->charmaxlen == 1) ? prs->charmaxlen : pg_mblen(prs->str + prs->state->posbyte);
    }

    Assert(prs->state->posbyte + prs->state->charlen <= prs->lenstr);
    Assert(prs->state->state >= TPS_Base && prs->state->state < TPS_Null);
    Assert(Actions[prs->state->state].state == prs->state->state);

    if (prs->state->pushedAtAction)
    {
                                                 
      item = prs->state->pushedAtAction + 1;
      prs->state->pushedAtAction = NULL;
    }
    else
    {
      item = Actions[prs->state->state].action;
      Assert(item != NULL);
    }

                                        
    while (item->isclass)
    {
      prs->c = item->c;
      if (item->isclass(prs) != 0)
      {
        break;
      }
      item++;
    }

#ifdef WPARSER_TRACE
    {
      TParserPosition *ptr;

      fprintf(stderr, "state ");
                                           
      for (ptr = prs->state->prev; ptr; ptr = ptr->prev)
      {
        fprintf(stderr, "  ");
      }
      fprintf(stderr, "%s ", Actions[prs->state->state].state_name);
      if (prs->state->posbyte < prs->lenstr)
      {
        fprintf(stderr, "at %c", *(prs->str + prs->state->posbyte));
      }
      else
      {
        fprintf(stderr, "at EOF");
      }
      fprintf(stderr, " matched rule %d flags%s%s%s%s%s%s%s%s%s%s%s\n", (int)(item - Actions[prs->state->state].action), (item->flags & A_BINGO) ? " BINGO" : "", (item->flags & A_POP) ? " POP" : "", (item->flags & A_PUSH) ? " PUSH" : "", (item->flags & A_RERUN) ? " RERUN" : "", (item->flags & A_CLEAR) ? " CLEAR" : "", (item->flags & A_MERGE) ? " MERGE" : "", (item->flags & A_CLRALL) ? " CLRALL" : "", (item->tostate != TPS_Null) ? " tostate " : "", (item->tostate != TPS_Null) ? Actions[item->tostate].state_name : "", (item->type > 0) ? " type " : "", tok_alias[item->type]);
    }
#endif

                                        
    if (item->special)
    {
      item->special(prs);
    }

                               
    if (item->flags & A_BINGO)
    {
      Assert(item->type > 0);
      prs->lenbytetoken = prs->state->lenbytetoken;
      prs->lenchartoken = prs->state->lenchartoken;
      prs->state->lenbytetoken = prs->state->lenchartoken = 0;
      prs->type = item->type;
    }

                                     
    if (item->flags & A_POP)
    {                                
      TParserPosition *ptr = prs->state->prev;

      pfree(prs->state);
      prs->state = ptr;
      Assert(prs->state);
    }
    else if (item->flags & A_PUSH)
    {                                                                     
      prs->state->pushedAtAction = item;                             
      prs->state = newTParserPosition(prs->state);
    }
    else if (item->flags & A_CLEAR)
    {                                  
      TParserPosition *ptr;

      Assert(prs->state->prev);
      ptr = prs->state->prev->prev;
      pfree(prs->state->prev);
      prs->state->prev = ptr;
    }
    else if (item->flags & A_CLRALL)
    {                                      
      TParserPosition *ptr;

      while (prs->state->prev)
      {
        ptr = prs->state->prev->prev;
        pfree(prs->state->prev);
        prs->state->prev = ptr;
      }
    }
    else if (item->flags & A_MERGE)
    {                                                  
      TParserPosition *ptr = prs->state;

      Assert(prs->state->prev);
      prs->state = prs->state->prev;

      prs->state->posbyte = ptr->posbyte;
      prs->state->poschar = ptr->poschar;
      prs->state->charlen = ptr->charlen;
      prs->state->lenbytetoken = ptr->lenbytetoken;
      prs->state->lenchartoken = ptr->lenchartoken;
      pfree(ptr);
    }

                                  
    if (item->tostate != TPS_Null)
    {
      prs->state->state = item->tostate;
    }

                           
    if ((item->flags & A_BINGO) || (prs->state->posbyte >= prs->lenstr && (item->flags & A_RERUN) == 0))
    {
      break;
    }

                                                                             
    if (item->flags & (A_RERUN | A_POP))
    {
      continue;
    }

                      
    if (prs->state->charlen)
    {
      prs->state->posbyte += prs->state->charlen;
      prs->state->lenbytetoken += prs->state->charlen;
      prs->state->poschar++;
      prs->state->lenchartoken++;
    }
  }

  return (item && (item->flags & A_BINGO)) ? true : false;
}

Datum
prsd_lextype(PG_FUNCTION_ARGS)
{
  LexDescr *descr = (LexDescr *)palloc(sizeof(LexDescr) * (LASTNUM + 1));
  int i;

  for (i = 1; i <= LASTNUM; i++)
  {
    descr[i - 1].lexid = i;
    descr[i - 1].alias = pstrdup(tok_alias[i]);
    descr[i - 1].descr = pstrdup(lex_descr[i]);
  }

  descr[LASTNUM].lexid = 0;

  PG_RETURN_POINTER(descr);
}

Datum
prsd_start(PG_FUNCTION_ARGS)
{
  PG_RETURN_POINTER(TParserInit((char *)PG_GETARG_POINTER(0), PG_GETARG_INT32(1)));
}

Datum
prsd_nexttoken(PG_FUNCTION_ARGS)
{
  TParser *p = (TParser *)PG_GETARG_POINTER(0);
  char **t = (char **)PG_GETARG_POINTER(1);
  int *tlen = (int *)PG_GETARG_POINTER(2);

  if (!TParserGet(p))
  {
    PG_RETURN_INT32(0);
  }

  *t = p->token;
  *tlen = p->lenbytetoken;

  PG_RETURN_INT32(p->type);
}

Datum
prsd_end(PG_FUNCTION_ARGS)
{
  TParser *p = (TParser *)PG_GETARG_POINTER(0);

  TParserClose(p);
  PG_RETURN_VOID();
}

   
                                   
   

                                      
#define TS_IDIGNORE(x) ((x) == TAG_T || (x) == PROTOCOL || (x) == SPACE || (x) == XMLENTITY)
#define HLIDREPLACE(x) ((x) == TAG_T)
#define HLIDSKIP(x) ((x) == URL_T || (x) == NUMHWORD || (x) == ASCIIHWORD || (x) == HWORD)
#define XMLHLIDSKIP(x) ((x) == URL_T || (x) == NUMHWORD || (x) == ASCIIHWORD || (x) == HWORD)
#define NONWORDTOKEN(x) ((x) == SPACE || HLIDREPLACE(x) || HLIDSKIP(x))
#define NOENDTOKEN(x) (NONWORDTOKEN(x) || (x) == SCIENTIFIC || (x) == VERSIONNUMBER || (x) == DECIMAL_T || (x) == SIGNEDINT || (x) == UNSIGNEDINT || TS_IDIGNORE(x))

   
                                                                       
                                                                       
                                                 
   

                                                     
#define INTERESTINGWORD(j) (prs->words[j].item && !prs->words[j].repeated)

                                                                         
#define BADENDPOINT(j) ((NOENDTOKEN(prs->words[j].type) || prs->words[j].len <= shortword) && !INTERESTINGWORD(j))

typedef struct
{
                                                                   
  int32 startpos;                                     
  int32 endpos;                                      
  int32 poslen;                                    
  int32 curlen;                              
  bool chosen;                 
  bool excluded;                 
} CoverPos;

typedef struct
{
                                           
  HeadlineWordEntry *words;
  int len;
} hlCheck;

   
                                                                        
   
static bool
checkcondition_HL(void *opaque, QueryOperand *val, ExecPhraseData *data)
{
  hlCheck *checkval = (hlCheck *)opaque;
  int i;

                                           
  for (i = 0; i < checkval->len; i++)
  {
    if (checkval->words[i].item == val)
    {
                                                           
      if (!data)
      {
        return true;
      }

      if (!data->pos)
      {
        data->pos = palloc(sizeof(WordEntryPos) * checkval->len);
        data->allocated = true;
        data->npos = 1;
        data->pos[0] = checkval->words[i].pos;
      }
      else if (data->pos[data->npos - 1] < checkval->words[i].pos)
      {
        data->pos[data->npos++] = checkval->words[i].pos;
      }
    }
  }

  if (data && data->npos > 0)
  {
    return true;
  }

  return false;
}

   
                                                                           
   
                               
   
static int
hlFirstIndex(HeadlineParsedText *prs, int pos)
{
  int i;

  for (i = pos; i < prs->curwords; i++)
  {
    if (prs->words[i].item != NULL)
    {
      return i;
    }
  }
  return -1;
}

   
                                                                           
   
                                                                          
                                                                      
                                                                       
                                                                        
                                                                         
                                                                         
                                                                        
                     
   
                                                                            
                                      
   
                                                                           
                            
   
static bool
hlCover(HeadlineParsedText *prs, TSQuery query, int max_cover, int *p, int *q)
{
  int pmin, pmax, nextpmin, nextpmax;
  hlCheck ch;

     
                                                                     
                                                                        
                                                                             
                  
     
  pmin = hlFirstIndex(prs, *p);
  while (pmin >= 0)
  {
                                                                     
    nextpmin = -1;
                                              
    ch.words = &(prs->words[pmin]);
                                                                         
    pmax = pmin;
    do
    {
                                                             
      ch.len = pmax - pmin + 1;
      if (TS_execute(GETQUERY(query), &ch, TS_EXEC_EMPTY, checkcondition_HL))
      {
        *p = pmin;
        *q = pmax;
        return true;
      }
                                                           
      nextpmax = hlFirstIndex(prs, pmax + 1);

         
                                                                         
                                                                
                           
         
      if (pmax == pmin)
      {
        nextpmin = nextpmax;
      }
      pmax = nextpmax;
    } while (pmax >= 0 && pmax - pmin < max_cover);
                                                       
    pmin = nextpmin;
  }
  return false;
}

   
                                                                           
   
                                                                           
   
static void
mark_fragment(HeadlineParsedText *prs, bool highlightall, int startpos, int endpos)
{
  int i;

  for (i = startpos; i <= endpos; i++)
  {
    if (prs->words[i].item)
    {
      prs->words[i].selected = 1;
    }
    if (!highlightall)
    {
      if (HLIDREPLACE(prs->words[i].type))
      {
        prs->words[i].replace = 1;
      }
      else if (HLIDSKIP(prs->words[i].type))
      {
        prs->words[i].skip = 1;
      }
    }
    else
    {
      if (XMLHLIDSKIP(prs->words[i].type))
      {
        prs->words[i].skip = 1;
      }
    }

    prs->words[i].in = (prs->words[i].repeated) ? 0 : 1;
  }
}

   
                                                                    
   
                                                                           
                                                                         
   
                                                                      
                                   
   
static void
get_next_fragment(HeadlineParsedText *prs, int *startpos, int *endpos, int *curlen, int *poslen, int max_words)
{
  int i;

     
                                                                            
                                                                          
                                                                          
                                                                           
           
     
                                      
  for (i = *startpos; i <= *endpos; i++)
  {
    *startpos = i;
    if (INTERESTINGWORD(i))
    {
      break;
    }
  }
                                         
  *curlen = 0;
  *poslen = 0;
  for (i = *startpos; i <= *endpos && *curlen < max_words; i++)
  {
    if (!NONWORDTOKEN(prs->words[i].type))
    {
      *curlen += 1;
    }
    if (INTERESTINGWORD(i))
    {
      *poslen += 1;
    }
  }
                                                                  
  if (*endpos > i)
  {
    *endpos = i;
    for (i = *endpos; i >= *startpos; i--)
    {
      *endpos = i;
      if (INTERESTINGWORD(i))
      {
        break;
      }
      if (!NONWORDTOKEN(prs->words[i].type))
      {
        *curlen -= 1;
      }
    }
  }
}

   
                                                
   
                                                                         
                                          
   
static void
mark_hl_fragments(HeadlineParsedText *prs, TSQuery query, bool highlightall, int shortword, int min_words, int max_words, int max_fragments, int max_cover)
{
  int32 poslen, curlen, i, f, num_f = 0;
  int32 stretch, maxstretch, posmarker;

  int32 startpos = 0, endpos = 0, p = 0, q = 0;

  int32 numcovers = 0, maxcovers = 32;

  int32 minI, minwords, maxitems;
  CoverPos *covers;

  covers = palloc(maxcovers * sizeof(CoverPos));

                      
  while (hlCover(prs, query, max_cover, &p, &q))
  {
    startpos = p;
    endpos = q;

       
                                                                          
                                                                          
                                                                        
                 
       

    while (startpos <= endpos)
    {
      get_next_fragment(prs, &startpos, &endpos, &curlen, &poslen, max_words);
      if (numcovers >= maxcovers)
      {
        maxcovers *= 2;
        covers = repalloc(covers, sizeof(CoverPos) * maxcovers);
      }
      covers[numcovers].startpos = startpos;
      covers[numcovers].endpos = endpos;
      covers[numcovers].curlen = curlen;
      covers[numcovers].poslen = poslen;
      covers[numcovers].chosen = false;
      covers[numcovers].excluded = false;
      numcovers++;
      startpos = endpos + 1;
      endpos = q;
    }

                                           
    p++;
  }

                          
  for (f = 0; f < max_fragments; f++)
  {
    maxitems = 0;
    minwords = PG_INT32_MAX;
    minI = -1;

       
                                                                           
                                         
       
    for (i = 0; i < numcovers; i++)
    {
      if (!covers[i].chosen && !covers[i].excluded && (maxitems < covers[i].poslen || (maxitems == covers[i].poslen && minwords > covers[i].curlen)))
      {
        maxitems = covers[i].poslen;
        minwords = covers[i].curlen;
        minI = i;
      }
    }
                                      
    if (minI >= 0)
    {
      covers[minI].chosen = true;
                                    
      startpos = covers[minI].startpos;
      endpos = covers[minI].endpos;
      curlen = covers[minI].curlen;
                                                                   
      if (curlen < max_words)
      {
                                                       
        maxstretch = (max_words - curlen) / 2;

           
                                                                       
                                                                   
                                   
           
        stretch = 0;
        posmarker = startpos;
        for (i = startpos - 1; i >= 0 && stretch < maxstretch && !prs->words[i].in; i--)
        {
          if (!NONWORDTOKEN(prs->words[i].type))
          {
            curlen++;
            stretch++;
          }
          posmarker = i;
        }
                                                            
        for (i = posmarker; i < startpos && BADENDPOINT(i); i++)
        {
          if (!NONWORDTOKEN(prs->words[i].type))
          {
            curlen--;
          }
        }
        startpos = i;
                                                        
        posmarker = endpos;
        for (i = endpos + 1; i < prs->curwords && curlen < max_words && !prs->words[i].in; i++)
        {
          if (!NONWORDTOKEN(prs->words[i].type))
          {
            curlen++;
          }
          posmarker = i;
        }
                                                          
        for (i = posmarker; i > endpos && BADENDPOINT(i); i--)
        {
          if (!NONWORDTOKEN(prs->words[i].type))
          {
            curlen--;
          }
        }
        endpos = i;
      }
      covers[minI].startpos = startpos;
      covers[minI].endpos = endpos;
      covers[minI].curlen = curlen;
                                              
      mark_fragment(prs, highlightall, startpos, endpos);
      num_f++;
                                                                         
      for (i = 0; i < numcovers; i++)
      {
        if (i != minI && ((covers[i].startpos >= startpos && covers[i].startpos <= endpos) || (covers[i].endpos >= startpos && covers[i].endpos <= endpos) || (covers[i].startpos < startpos && covers[i].endpos > endpos)))
        {
          covers[i].excluded = true;
        }
      }
    }
    else
    {
      break;                                  
    }
  }

                                                                     
  if (num_f <= 0)
  {
    startpos = endpos = curlen = 0;
    for (i = 0; i < prs->curwords && curlen < min_words; i++)
    {
      if (!NONWORDTOKEN(prs->words[i].type))
      {
        curlen++;
      }
      endpos = i;
    }
    mark_fragment(prs, highlightall, startpos, endpos);
  }

  pfree(covers);
}

   
                                                 
   
static void
mark_hl_words(HeadlineParsedText *prs, TSQuery query, bool highlightall, int shortword, int min_words, int max_words, int max_cover)
{
  int p = 0, q = 0;
  int bestb = -1, beste = -1;
  int bestlen = -1;
  bool bestcover = false;
  int pose, posb, poslen, curlen;
  bool poscover;
  int i;

  if (!highlightall)
  {
                                                                  
    while (hlCover(prs, query, max_cover, &p, &q))
    {
         
                                                                    
                                                                     
                                                                       
                                                                        
         
      curlen = 0;
      poslen = 0;
      posb = pose = p;
      for (i = p; i <= q && curlen < max_words; i++)
      {
        if (!NONWORDTOKEN(prs->words[i].type))
        {
          curlen++;
        }
        if (INTERESTINGWORD(i))
        {
          poslen++;
        }
        pose = i;
      }

      if (curlen < max_words)
      {
           
                                                                    
                                                                    
                                                          
           
        for (i = i - 1; i < prs->curwords && curlen < max_words; i++)
        {
          if (i > q)
          {
            if (!NONWORDTOKEN(prs->words[i].type))
            {
              curlen++;
            }
            if (INTERESTINGWORD(i))
            {
              poslen++;
            }
          }
          pose = i;
          if (BADENDPOINT(i))
          {
            continue;
          }
          if (curlen >= min_words)
          {
            break;
          }
        }
        if (curlen < min_words)
        {
             
                                                                   
                                                              
             
          for (i = p - 1; i >= 0; i--)
          {
            if (!NONWORDTOKEN(prs->words[i].type))
            {
              curlen++;
            }
            if (INTERESTINGWORD(i))
            {
              poslen++;
            }
            if (curlen >= max_words)
            {
              break;
            }
            if (BADENDPOINT(i))
            {
              continue;
            }
            if (curlen >= min_words)
            {
              break;
            }
          }
          posb = (i >= 0) ? i : 0;
        }
      }
      else
      {
           
                                                                     
                                              
           
        if (i > q)
        {
          i = q;
        }
        for (; curlen > min_words; i--)
        {
          if (!BADENDPOINT(i))
          {
            break;
          }
          if (!NONWORDTOKEN(prs->words[i].type))
          {
            curlen--;
          }
          if (INTERESTINGWORD(i))
          {
            poslen--;
          }
          pose = i - 1;
        }
      }

         
                                                                   
                                                                
         
      poscover = (posb <= p && pose >= q);

         
                                                                      
                                                                    
                                                                       
                                                                        
                                              
         
      if (poscover > bestcover || (poscover == bestcover && poslen > bestlen) || (poscover == bestcover && poslen == bestlen && !BADENDPOINT(pose) && BADENDPOINT(beste)))
      {
        bestb = posb;
        beste = pose;
        bestlen = poslen;
        bestcover = poscover;
      }

                                             
      p++;
    }

       
                                                                          
                      
       
    if (bestlen < 0)
    {
      curlen = 0;
      pose = 0;
      for (i = 0; i < prs->curwords && curlen < min_words; i++)
      {
        if (!NONWORDTOKEN(prs->words[i].type))
        {
          curlen++;
        }
        pose = i;
      }
      bestb = 0;
      beste = pose;
    }
  }
  else
  {
                                                       
    bestb = 0;
    beste = prs->curwords - 1;
  }

  mark_fragment(prs, highlightall, bestb, beste);
}

   
                                         
   
Datum
prsd_headline(PG_FUNCTION_ARGS)
{
  HeadlineParsedText *prs = (HeadlineParsedText *)PG_GETARG_POINTER(0);
  List *prsoptions = (List *)PG_GETARG_POINTER(1);
  TSQuery query = PG_GETARG_TSQUERY(2);

                              
  int min_words = 15;
  int max_words = 35;
  int shortword = 3;
  int max_fragments = 0;
  bool highlightall = false;
  int max_cover;
  ListCell *l;

                                           
  prs->startsel = NULL;
  prs->stopsel = NULL;
  prs->fragdelim = NULL;
  foreach (l, prsoptions)
  {
    DefElem *defel = (DefElem *)lfirst(l);
    char *val = defGetString(defel);

    if (pg_strcasecmp(defel->defname, "MaxWords") == 0)
    {
      max_words = pg_strtoint32(val);
    }
    else if (pg_strcasecmp(defel->defname, "MinWords") == 0)
    {
      min_words = pg_strtoint32(val);
    }
    else if (pg_strcasecmp(defel->defname, "ShortWord") == 0)
    {
      shortword = pg_strtoint32(val);
    }
    else if (pg_strcasecmp(defel->defname, "MaxFragments") == 0)
    {
      max_fragments = pg_strtoint32(val);
    }
    else if (pg_strcasecmp(defel->defname, "StartSel") == 0)
    {
      prs->startsel = pstrdup(val);
    }
    else if (pg_strcasecmp(defel->defname, "StopSel") == 0)
    {
      prs->stopsel = pstrdup(val);
    }
    else if (pg_strcasecmp(defel->defname, "FragmentDelimiter") == 0)
    {
      prs->fragdelim = pstrdup(val);
    }
    else if (pg_strcasecmp(defel->defname, "HighlightAll") == 0)
    {
      highlightall = (pg_strcasecmp(val, "1") == 0 || pg_strcasecmp(val, "on") == 0 || pg_strcasecmp(val, "true") == 0 || pg_strcasecmp(val, "t") == 0 || pg_strcasecmp(val, "y") == 0 || pg_strcasecmp(val, "yes") == 0);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized headline parameter: \"%s\"", defel->defname)));
    }
  }

     
                                                                           
                                                                 
                    
     
  max_cover = Max(max_words * 10, 100);
  if (max_fragments > 0)
  {
    max_cover *= max_fragments;
  }

                                                         
  if (!highlightall)
  {
    if (min_words >= max_words)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("MinWords should be less than MaxWords")));
    }
    if (min_words <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("MinWords should be positive")));
    }
    if (shortword < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ShortWord should be >= 0")));
    }
    if (max_fragments < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("MaxFragments should be >= 0")));
    }
  }

                                           
  if (max_fragments == 0)
  {
    mark_hl_words(prs, query, highlightall, shortword, min_words, max_words, max_cover);
  }
  else
  {
    mark_hl_fragments(prs, query, highlightall, shortword, min_words, max_words, max_fragments, max_cover);
  }

                                                 
  if (!prs->startsel)
  {
    prs->startsel = pstrdup("<b>");
  }
  if (!prs->stopsel)
  {
    prs->stopsel = pstrdup("</b>");
  }
  if (!prs->fragdelim)
  {
    prs->fragdelim = pstrdup(" ... ");
  }

                                           
  prs->startsellen = strlen(prs->startsel);
  prs->stopsellen = strlen(prs->stopsel);
  prs->fragdelimlen = strlen(prs->fragdelim);

  PG_RETURN_POINTER(prs);
}
