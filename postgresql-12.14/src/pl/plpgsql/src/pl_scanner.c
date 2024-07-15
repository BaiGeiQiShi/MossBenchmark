                                                                            
   
                
                                   
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "mb/pg_wchar.h"
#include "parser/scanner.h"

#include "plpgsql.h"
#include "pl_gram.h"                                     

                                                           
IdentifierLookup plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;

   
                          
   
                                                                             
                                                                            
                                                                         
                                                                     
                                                                           
                                                                             
                                                                             
                                                                              
                                                                            
                                     
   
                                                                           
                                                                          
                                                                            
                                                                            
                                                                               
                                                                        
                                                                         
                                                                       
                                                                            
                                                                          
   
                                                                        
                                                                              
                                                                         
                                                         
   

                                                       
#include "pl_reserved_kwlist_d.h"
#include "pl_unreserved_kwlist_d.h"

                                       
#define PG_KEYWORD(kwname, value) value,

static const uint16 ReservedPLKeywordTokens[] = {
#include "pl_reserved_kwlist.h"
};

static const uint16 UnreservedPLKeywordTokens[] = {
#include "pl_unreserved_kwlist.h"
};

#undef PG_KEYWORD

   
                                                                       
                                                                         
                                                                           
                             
   
#define AT_STMT_START(prev_token) ((prev_token) == ';' || (prev_token) == K_BEGIN || (prev_token) == K_THEN || (prev_token) == K_ELSE || (prev_token) == K_LOOP)

                                                              
typedef struct
{
  YYSTYPE lval;                           
  YYLTYPE lloc;                        
  int leng;                          
} TokenAuxData;

   
                                                                        
                                                                           
                                                                            
                                                                     
   

                                    
static core_yyscan_t yyscanner = NULL;
static core_yy_extra_type core_yy;

                               
static const char *scanorig;

                                                                               
static int plpgsql_yyleng;

                                                                             
static int plpgsql_yytoken;

                          
#define MAX_PUSHBACKS 4

static int num_pushbacks;
static int pushback_token[MAX_PUSHBACKS];
static TokenAuxData pushback_auxdata[MAX_PUSHBACKS];

                                            
static const char *cur_line_start;
static const char *cur_line_end;
static int cur_line_num;

                        
static int
internal_yylex(TokenAuxData *auxdata);
static void
push_back_token(int token, TokenAuxData *auxdata);
static void
location_lineno_init(void);

   
                                                               
                                                                        
                                                                       
                                                                          
                                                                
                                                                     
                         
   
int
plpgsql_yylex(void)
{
  int tok1;
  TokenAuxData aux1;
  int kwnum;

  tok1 = internal_yylex(&aux1);
  if (tok1 == IDENT || tok1 == PARAM)
  {
    int tok2;
    TokenAuxData aux2;

    tok2 = internal_yylex(&aux2);
    if (tok2 == '.')
    {
      int tok3;
      TokenAuxData aux3;

      tok3 = internal_yylex(&aux3);
      if (tok3 == IDENT)
      {
        int tok4;
        TokenAuxData aux4;

        tok4 = internal_yylex(&aux4);
        if (tok4 == '.')
        {
          int tok5;
          TokenAuxData aux5;

          tok5 = internal_yylex(&aux5);
          if (tok5 == IDENT)
          {
            if (plpgsql_parse_tripword(aux1.lval.str, aux3.lval.str, aux5.lval.str, &aux1.lval.wdatum, &aux1.lval.cword))
            {
              tok1 = T_DATUM;
            }
            else
            {
              tok1 = T_CWORD;
            }
          }
          else
          {
                                                
            push_back_token(tok5, &aux5);
            push_back_token(tok4, &aux4);
            if (plpgsql_parse_dblword(aux1.lval.str, aux3.lval.str, &aux1.lval.wdatum, &aux1.lval.cword))
            {
              tok1 = T_DATUM;
            }
            else
            {
              tok1 = T_CWORD;
            }
          }
        }
        else
        {
                                              
          push_back_token(tok4, &aux4);
          if (plpgsql_parse_dblword(aux1.lval.str, aux3.lval.str, &aux1.lval.wdatum, &aux1.lval.cword))
          {
            tok1 = T_DATUM;
          }
          else
          {
            tok1 = T_CWORD;
          }
        }
      }
      else
      {
                                        
        push_back_token(tok3, &aux3);
        push_back_token(tok2, &aux2);
        if (plpgsql_parse_word(aux1.lval.str, core_yy.scanbuf + aux1.lloc, true, &aux1.lval.wdatum, &aux1.lval.word))
        {
          tok1 = T_DATUM;
        }
        else if (!aux1.lval.word.quoted && (kwnum = ScanKeywordLookup(aux1.lval.word.ident, &UnreservedPLKeywords)) >= 0)
        {
          aux1.lval.keyword = GetScanKeyword(kwnum, &UnreservedPLKeywords);
          tok1 = UnreservedPLKeywordTokens[kwnum];
        }
        else
        {
          tok1 = T_WORD;
        }
      }
    }
    else
    {
                                      
      push_back_token(tok2, &aux2);

         
                                                                        
                                                               
                                                                    
                                                                         
                                                                        
                                                                       
                                                                         
                                                          
         
                                                                      
                                                                     
         
                                                                        
                                                                       
                             
         
      if (plpgsql_parse_word(aux1.lval.str, core_yy.scanbuf + aux1.lloc, (!AT_STMT_START(plpgsql_yytoken) || (tok2 == '=' || tok2 == COLON_EQUALS || tok2 == '[')), &aux1.lval.wdatum, &aux1.lval.word))
      {
        tok1 = T_DATUM;
      }
      else if (!aux1.lval.word.quoted && (kwnum = ScanKeywordLookup(aux1.lval.word.ident, &UnreservedPLKeywords)) >= 0)
      {
        aux1.lval.keyword = GetScanKeyword(kwnum, &UnreservedPLKeywords);
        tok1 = UnreservedPLKeywordTokens[kwnum];
      }
      else
      {
        tok1 = T_WORD;
      }
    }
  }
  else
  {
       
                                                                    
       
                                                                        
                                                                           
                                                                        
                                                                           
                                                                    
                                                                     
       
  }

  plpgsql_yylval = aux1.lval;
  plpgsql_yylloc = aux1.lloc;
  plpgsql_yyleng = aux1.leng;
  plpgsql_yytoken = tok1;
  return tok1;
}

   
                                                                             
                                                                          
                                                                             
                                                       
   
static int
internal_yylex(TokenAuxData *auxdata)
{
  int token;
  const char *yytext;

  if (num_pushbacks > 0)
  {
    num_pushbacks--;
    token = pushback_token[num_pushbacks];
    *auxdata = pushback_auxdata[num_pushbacks];
  }
  else
  {
    token = core_yylex(&auxdata->lval.core_yystype, &auxdata->lloc, yyscanner);

                                                              
    yytext = core_yy.scanbuf + auxdata->lloc;
    auxdata->leng = strlen(yytext);

                                                                   
    if (token == Op)
    {
      if (strcmp(auxdata->lval.str, "<<") == 0)
      {
        token = LESS_LESS;
      }
      else if (strcmp(auxdata->lval.str, ">>") == 0)
      {
        token = GREATER_GREATER;
      }
      else if (strcmp(auxdata->lval.str, "#") == 0)
      {
        token = '#';
      }
    }

                                                                    
    else if (token == PARAM)
    {
      auxdata->lval.str = pstrdup(yytext);
    }
  }

  return token;
}

   
                                                                  
   
static void
push_back_token(int token, TokenAuxData *auxdata)
{
  if (num_pushbacks >= MAX_PUSHBACKS)
  {
    elog(ERROR, "too many tokens pushed back");
  }
  pushback_token[num_pushbacks] = token;
  pushback_auxdata[num_pushbacks] = *auxdata;
  num_pushbacks++;
}

   
                                                                        
   
                                                                      
                                                                          
   
void
plpgsql_push_back_token(int token)
{
  TokenAuxData auxdata;

  auxdata.lval = plpgsql_yylval;
  auxdata.lloc = plpgsql_yylloc;
  auxdata.leng = plpgsql_yyleng;
  push_back_token(token, &auxdata);
}

   
                                                  
   
                                                                         
                                         
   
bool
plpgsql_token_is_unreserved_keyword(int token)
{
  int i;

  for (i = 0; i < lengthof(UnreservedPLKeywordTokens); i++)
  {
    if (UnreservedPLKeywordTokens[i] == token)
    {
      return true;
    }
  }
  return false;
}

   
                                                                       
                                                                    
   
void
plpgsql_append_source_text(StringInfo buf, int startlocation, int endlocation)
{
  Assert(startlocation <= endlocation);
  appendBinaryStringInfo(buf, scanorig + startlocation, endlocation - startlocation);
}

   
                                                                     
                                                                   
   
                                                                             
                                                                  
   
int
plpgsql_peek(void)
{
  int tok1;
  TokenAuxData aux1;

  tok1 = internal_yylex(&aux1);
  push_back_token(tok1, &aux1);
  return tok1;
}

   
                                                                      
                                                                             
                                              
   
                                                                             
                                                                  
   
void
plpgsql_peek2(int *tok1_p, int *tok2_p, int *tok1_loc, int *tok2_loc)
{
  int tok1, tok2;
  TokenAuxData aux1, aux2;

  tok1 = internal_yylex(&aux1);
  tok2 = internal_yylex(&aux2);

  *tok1_p = tok1;
  if (tok1_loc)
  {
    *tok1_loc = aux1.lloc;
  }
  *tok2_p = tok2;
  if (tok2_loc)
  {
    *tok2_loc = aux2.lloc;
  }

  push_back_token(tok2, &aux2);
  push_back_token(tok1, &aux1);
}

   
                               
                                                  
   
                                                                           
                                   
   
                                                                       
                                                                        
                          
   
int
plpgsql_scanner_errposition(int location)
{
  int pos;

  if (location < 0 || scanorig == NULL)
  {
    return 0;                                   
  }

                                               
  pos = pg_mbstrlen_with_len(scanorig, location) + 1;
                                            
  (void)internalerrposition(pos);
                                          
  return internalerrquery(scanorig);
}

   
                   
                                     
   
                                                                      
                                      
                                                                             
                                                                          
                                                                            
                  
   
void
plpgsql_yyerror(const char *message)
{
  char *yytext = core_yy.scanbuf + plpgsql_yylloc;

  if (*yytext == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                                          
                       errmsg("%s at end of input", _(message)), plpgsql_scanner_errposition(plpgsql_yylloc)));
  }
  else
  {
       
                                                                      
                                                                         
                                                                           
                        
       
    yytext[plpgsql_yyleng] = '\0';

    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                                                
                       errmsg("%s at or near \"%s\"", _(message), yytext), plpgsql_scanner_errposition(plpgsql_yylloc)));
  }
}

   
                                                                 
                         
   
                                                                        
                                                                      
                          
   
int
plpgsql_location_to_lineno(int location)
{
  const char *loc;

  if (location < 0 || scanorig == NULL)
  {
    return 0;                              
  }
  loc = scanorig + location;

                                                                  
  if (loc < cur_line_start)
  {
    location_lineno_init();
  }

  while (cur_line_end != NULL && loc > cur_line_end)
  {
    cur_line_start = cur_line_end + 1;
    cur_line_num++;
    cur_line_end = strchr(cur_line_start, '\n');
  }

  return cur_line_num;
}

                                                                  
static void
location_lineno_init(void)
{
  cur_line_start = scanorig;
  cur_line_num = 1;

  cur_line_end = strchr(cur_line_start, '\n');
}

                                              
int
plpgsql_latest_lineno(void)
{
  return cur_line_num;
}

   
                                            
   
                                                                            
                                                                        
                              
   
void
plpgsql_scanner_init(const char *str)
{
                                 
  yyscanner = scanner_init(str, &core_yy, &ReservedPLKeywords, ReservedPLKeywordTokens);

     
                                                                        
                                                                         
                                                                          
                                                      
     
  scanorig = str;

                   
  plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
  plpgsql_yytoken = 0;

  num_pushbacks = 0;

  location_lineno_init();
}

   
                                                                         
   
void
plpgsql_scanner_finish(void)
{
                       
  scanner_finish(yyscanner);
                                           
  yyscanner = NULL;
  scanorig = NULL;
}
