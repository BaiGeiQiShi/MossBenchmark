                                                                            
   
            
                                                   
   
                                                                    
                                                                      
                                                                     
                                                                      
                                
   
   
                                                                         
                                                                        
   
                  
                                 
   
                                                                            
   

#include "postgres.h"

#include "parser/gramparse.h"
#include "parser/parser.h"

   
              
                                                                       
   
                                                                            
                                         
   
List *
raw_parser(const char *str)
{
  core_yyscan_t yyscanner;
  base_yy_extra_type yyextra;
  int yyresult;

                                   
  yyscanner = scanner_init(str, &yyextra.core_yy_extra, &ScanKeywords, ScanKeywordTokens);

                                                        
  yyextra.have_lookahead = false;

                                   
  parser_init(&yyextra);

              
  yyresult = base_yyparse(yyscanner);

                                 
  scanner_finish(yyscanner);

  if (yyresult)            
  {
    return NIL;
  }

  return yyextra.parsetree;
}

   
                                                                             
   
                                                                        
                                                                               
                                                                             
   
                                                                       
                                                                           
                                                                            
                                                                         
               
   
                                                                    
                                                                      
                                                           
   
int
base_yylex(YYSTYPE *lvalp, YYLTYPE *llocp, core_yyscan_t yyscanner)
{
  base_yy_extra_type *yyextra = pg_yyget_extra(yyscanner);
  int cur_token;
  int next_token;
  int cur_token_length;
  YYLTYPE cur_yylloc;

                                                   
  if (yyextra->have_lookahead)
  {
    cur_token = yyextra->lookahead_token;
    lvalp->core_yystype = yyextra->lookahead_yylval;
    *llocp = yyextra->lookahead_yylloc;
    *(yyextra->lookahead_end) = yyextra->lookahead_hold_char;
    yyextra->have_lookahead = false;
  }
  else
  {
    cur_token = core_yylex(&(lvalp->core_yystype), llocp, yyscanner);
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

     
                                                                             
                                                                           
                                                                         
     
  yyextra->lookahead_end = yyextra->core_yy_extra.scanbuf + *llocp + cur_token_length;
  Assert(*(yyextra->lookahead_end) == '\0');

     
                                                                           
                                                                            
                                                                            
                                                                           
                                                              
     
  cur_yylloc = *llocp;

                                                               
  next_token = core_yylex(&(yyextra->lookahead_yylval), llocp, yyscanner);
  yyextra->lookahead_token = next_token;
  yyextra->lookahead_yylloc = *llocp;

  *llocp = cur_yylloc;

                                                         
  yyextra->lookahead_hold_char = *(yyextra->lookahead_end);
  *(yyextra->lookahead_end) = '\0';

  yyextra->have_lookahead = true;

                                                       
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
