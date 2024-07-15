                                                                            
   
                 
                                            
   
                                                                
   
                  
                                                
   
                                                                            
   
#include "postgres.h"

#include "fmgr.h"

PG_MODULE_MAGIC;

   
         
   

                       
typedef struct
{
  char *buffer;                    
  int len;                                        
  int pos;                                  
} ParserState;

typedef struct
{
  int lexid;
  char *alias;
  char *descr;
} LexDescr;

   
             
   
PG_FUNCTION_INFO_V1(testprs_start);
PG_FUNCTION_INFO_V1(testprs_getlexeme);
PG_FUNCTION_INFO_V1(testprs_end);
PG_FUNCTION_INFO_V1(testprs_lextype);

Datum
testprs_start(PG_FUNCTION_ARGS)
{
  ParserState *pst = (ParserState *)palloc0(sizeof(ParserState));

  pst->buffer = (char *)PG_GETARG_POINTER(0);
  pst->len = PG_GETARG_INT32(1);
  pst->pos = 0;

  PG_RETURN_POINTER(pst);
}

Datum
testprs_getlexeme(PG_FUNCTION_ARGS)
{
  ParserState *pst = (ParserState *)PG_GETARG_POINTER(0);
  char **t = (char **)PG_GETARG_POINTER(1);
  int *tlen = (int *)PG_GETARG_POINTER(2);
  int startpos = pst->pos;
  int type;

  *t = pst->buffer + pst->pos;

  if (pst->pos < pst->len && (pst->buffer)[pst->pos] == ' ')
  {
                    
    type = 12;
                                            
    while (pst->pos < pst->len && (pst->buffer)[pst->pos] == ' ')
    {
      (pst->pos)++;
    }
  }
  else
  {
                   
    type = 3;
                                        
    while (pst->pos < pst->len && (pst->buffer)[pst->pos] != ' ')
    {
      (pst->pos)++;
    }
  }

  *tlen = pst->pos - startpos;

                                       
  if (*tlen == 0)
  {
    type = 0;
  }

  PG_RETURN_INT32(type);
}

Datum
testprs_end(PG_FUNCTION_ARGS)
{
  ParserState *pst = (ParserState *)PG_GETARG_POINTER(0);

  pfree(pst);
  PG_RETURN_VOID();
}

Datum
testprs_lextype(PG_FUNCTION_ARGS)
{
     
                                                                          
                                                                            
                                                                 
     
  LexDescr *descr = (LexDescr *)palloc(sizeof(LexDescr) * (2 + 1));

                                               
  descr[0].lexid = 3;
  descr[0].alias = pstrdup("word");
  descr[0].descr = pstrdup("Word");
  descr[1].lexid = 12;
  descr[1].alias = pstrdup("blank");
  descr[1].descr = pstrdup("Space symbols");
  descr[2].lexid = 0;

  PG_RETURN_POINTER(descr);
}
