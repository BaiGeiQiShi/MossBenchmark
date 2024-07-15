                                                                            
   
            
                                                   
   
                                                                        
                                              
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   

#include "postgres_fe.h"

#include "preproc_extern.h"
#include "preproc.h"

static bool have_lookahead;                                    
static int lookahead_token;                               
static YYSTYPE lookahead_yylval;                                 
static YYLTYPE lookahead_yylloc;                                 
static char *lookahead_yytext;                            
static char *lookahead_end;                                
static char lookahead_hold_char;                                       

   
                                                                             
   
                                                                        
                                                                               
                                                                             
   
                                                                       
                                                                           
                                                                            
                                                                         
               
   
int
filtered_base_yylex(void)
{
  int cur_token;
  int next_token;
  int cur_token_length;
  YYSTYPE cur_yylval;
  YYLTYPE cur_yylloc;
  char *cur_yytext;

                                                   
  if (have_lookahead)
  {
    cur_token = lookahead_token;
    base_yylval = lookahead_yylval;
    base_yylloc = lookahead_yylloc;
    base_yytext = lookahead_yytext;
    *lookahead_end = lookahead_hold_char;
    have_lookahead = false;
  }
  else
  {
    cur_token = base_yylex();
  }

     
                                                                             
                                                                             
                                                                       
                                   
     
  switch (cur_token)
  {
  case NOT:
    cur_token_length = 3;
    break;
  case NULLS_P:
    cur_token_length = 5;
    break;
  case WITH:
    cur_token_length = 4;
    break;
  default:
    return cur_token;
  }

     
                                                                             
                                                                           
                                                                         
     
  lookahead_end = base_yytext + cur_token_length;
  Assert(*lookahead_end == '\0');

                                                               
  cur_yylval = base_yylval;
  cur_yylloc = base_yylloc;
  cur_yytext = base_yytext;

                                                               
  next_token = base_yylex();

  lookahead_token = next_token;
  lookahead_yylval = base_yylval;
  lookahead_yylloc = base_yylloc;
  lookahead_yytext = base_yytext;

  base_yylval = cur_yylval;
  base_yylloc = cur_yylloc;
  base_yytext = cur_yytext;

                                                         
  lookahead_hold_char = *lookahead_end;
  *lookahead_end = '\0';

  have_lookahead = true;

                                                       
  switch (cur_token)
  {
  case NOT:
                                                                    
    switch (next_token)
    {
    case BETWEEN:
    case IN_P:
    case LIKE:
    case ILIKE:
    case SIMILAR:
      cur_token = NOT_LA;
      break;
    }
    break;

  case NULLS_P:
                                                                       
    switch (next_token)
    {
    case FIRST_P:
    case LAST_P:
      cur_token = NULLS_LA;
      break;
    }
    break;

  case WITH:
                                                                        
    switch (next_token)
    {
    case TIME:
    case ORDINALITY:
      cur_token = WITH_LA;
      break;
    }
    break;
  }

  return cur_token;
}
