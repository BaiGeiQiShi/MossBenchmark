                                                                            
   
                     
                         
   
                                                                         
   
   
                  
                                             
   
                                                                            
   

#include "postgres.h"

#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"

   
                                                                               
                                                                             
                                                                       
                                                            
   
struct TSVectorParseStateData
{
  char *prsbuf;                              
  char *bufstart;                                           
  char *word;                                           
  int len;                                                 
  int eml;                                      
  bool oprisdelim;                                     
  bool is_tsquery;                                              
  bool is_web;                                          
};

   
                                                                      
                                                                             
             
   
TSVectorParseState
init_tsvector_parser(char *input, int flags)
{
  TSVectorParseState state;

  state = (TSVectorParseState)palloc(sizeof(struct TSVectorParseStateData));
  state->prsbuf = input;
  state->bufstart = input;
  state->len = 32;
  state->word = (char *)palloc(state->len);
  state->eml = pg_database_encoding_max_length();
  state->oprisdelim = (flags & P_TSV_OPR_IS_DELIM) != 0;
  state->is_tsquery = (flags & P_TSV_IS_TSQUERY) != 0;
  state->is_web = (flags & P_TSV_IS_WEB) != 0;

  return state;
}

   
                                                                     
   
void
reset_tsvector_parser(TSVectorParseState state, char *input)
{
  state->prsbuf = input;
}

   
                                 
   
void
close_tsvector_parser(TSVectorParseState state)
{
  pfree(state->word);
  pfree(state);
}

                                                                      
#define RESIZEPRSBUF                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int clen = curpos - state->word;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (clen + state->eml >= state->len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      state->len *= 2;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      state->word = (char *)repalloc(state->word, state->len);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      curpos = state->word + clen;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                                                   
#define RETURN_TOKEN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (pos_ptr != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *pos_ptr = pos;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      *poslen = npos;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else if (pos != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      pfree(pos);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (strval != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      *strval = state->word;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (lenval != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      *lenval = curpos - state->word;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    if (endptr != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      *endptr = state->prsbuf;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    return true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

                                           
#define WAITWORD 1
#define WAITENDWORD 2
#define WAITNEXTCHAR 3
#define WAITENDCMPLX 4
#define WAITPOSINFO 5
#define INPOSINFO 6
#define WAITPOSDELIM 7
#define WAITCHARCMPLX 8

#define PRSSYNTAXERROR prssyntaxerror(state)

static void
prssyntaxerror(TSVectorParseState state)
{
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), state->is_tsquery ? errmsg("syntax error in tsquery: \"%s\"", state->bufstart) : errmsg("syntax error in tsvector: \"%s\"", state->bufstart)));
}

   
                                                                        
                                                                      
                             
   
                             
                              
                                                                  
                                                                 
                                                          
                                                        
                                           
                                  
   
                                             
   
bool
gettoken_tsvector(TSVectorParseState state, char **strval, int *lenval, WordEntryPos **pos_ptr, int *poslen, char **endptr)
{
  int oldstate = 0;
  char *curpos = state->word;
  int statecode = WAITWORD;

     
                                                                             
                       
     
  WordEntryPos *pos = NULL;
  int npos = 0;                              
  int posalen = 0;                            

  while (1)
  {
    if (statecode == WAITWORD)
    {
      if (*(state->prsbuf) == '\0')
      {
        return false;
      }
      else if (!state->is_web && t_iseq(state->prsbuf, '\''))
      {
        statecode = WAITENDCMPLX;
      }
      else if (!state->is_web && t_iseq(state->prsbuf, '\\'))
      {
        statecode = WAITNEXTCHAR;
        oldstate = WAITENDWORD;
      }
      else if ((state->oprisdelim && ISOPERATOR(state->prsbuf)) || (state->is_web && t_iseq(state->prsbuf, '"')))
      {
        PRSSYNTAXERROR;
      }
      else if (!t_isspace(state->prsbuf))
      {
        COPYCHAR(curpos, state->prsbuf);
        curpos += pg_mblen(state->prsbuf);
        statecode = WAITENDWORD;
      }
    }
    else if (statecode == WAITNEXTCHAR)
    {
      if (*(state->prsbuf) == '\0')
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("there is no escaped character: \"%s\"", state->bufstart)));
      }
      else
      {
        RESIZEPRSBUF;
        COPYCHAR(curpos, state->prsbuf);
        curpos += pg_mblen(state->prsbuf);
        Assert(oldstate != 0);
        statecode = oldstate;
      }
    }
    else if (statecode == WAITENDWORD)
    {
      if (!state->is_web && t_iseq(state->prsbuf, '\\'))
      {
        statecode = WAITNEXTCHAR;
        oldstate = WAITENDWORD;
      }
      else if (t_isspace(state->prsbuf) || *(state->prsbuf) == '\0' || (state->oprisdelim && ISOPERATOR(state->prsbuf)) || (state->is_web && t_iseq(state->prsbuf, '"')))
      {
        RESIZEPRSBUF;
        if (curpos == state->word)
        {
          PRSSYNTAXERROR;
        }
        *(curpos) = '\0';
        RETURN_TOKEN;
      }
      else if (t_iseq(state->prsbuf, ':'))
      {
        if (curpos == state->word)
        {
          PRSSYNTAXERROR;
        }
        *(curpos) = '\0';
        if (state->oprisdelim)
        {
          RETURN_TOKEN;
        }
        else
        {
          statecode = INPOSINFO;
        }
      }
      else
      {
        RESIZEPRSBUF;
        COPYCHAR(curpos, state->prsbuf);
        curpos += pg_mblen(state->prsbuf);
      }
    }
    else if (statecode == WAITENDCMPLX)
    {
      if (!state->is_web && t_iseq(state->prsbuf, '\''))
      {
        statecode = WAITCHARCMPLX;
      }
      else if (!state->is_web && t_iseq(state->prsbuf, '\\'))
      {
        statecode = WAITNEXTCHAR;
        oldstate = WAITENDCMPLX;
      }
      else if (*(state->prsbuf) == '\0')
      {
        PRSSYNTAXERROR;
      }
      else
      {
        RESIZEPRSBUF;
        COPYCHAR(curpos, state->prsbuf);
        curpos += pg_mblen(state->prsbuf);
      }
    }
    else if (statecode == WAITCHARCMPLX)
    {
      if (!state->is_web && t_iseq(state->prsbuf, '\''))
      {
        RESIZEPRSBUF;
        COPYCHAR(curpos, state->prsbuf);
        curpos += pg_mblen(state->prsbuf);
        statecode = WAITENDCMPLX;
      }
      else
      {
        RESIZEPRSBUF;
        *(curpos) = '\0';
        if (curpos == state->word)
        {
          PRSSYNTAXERROR;
        }
        if (state->oprisdelim)
        {
                                                       
          RETURN_TOKEN;
        }
        else
        {
          statecode = WAITPOSINFO;
        }
        continue;                                
      }
    }
    else if (statecode == WAITPOSINFO)
    {
      if (t_iseq(state->prsbuf, ':'))
      {
        statecode = INPOSINFO;
      }
      else
      {
        RETURN_TOKEN;
      }
    }
    else if (statecode == INPOSINFO)
    {
      if (t_isdigit(state->prsbuf))
      {
        if (posalen == 0)
        {
          posalen = 4;
          pos = (WordEntryPos *)palloc(sizeof(WordEntryPos) * posalen);
          npos = 0;
        }
        else if (npos + 1 >= posalen)
        {
          posalen *= 2;
          pos = (WordEntryPos *)repalloc(pos, sizeof(WordEntryPos) * posalen);
        }
        npos++;
        WEP_SETPOS(pos[npos - 1], LIMITPOS(atoi(state->prsbuf)));
                                                                     
        if (WEP_GETPOS(pos[npos - 1]) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("wrong position info in tsvector: \"%s\"", state->bufstart)));
        }
        WEP_SETWEIGHT(pos[npos - 1], 0);
        statecode = WAITPOSDELIM;
      }
      else
      {
        PRSSYNTAXERROR;
      }
    }
    else if (statecode == WAITPOSDELIM)
    {
      if (t_iseq(state->prsbuf, ','))
      {
        statecode = INPOSINFO;
      }
      else if (t_iseq(state->prsbuf, 'a') || t_iseq(state->prsbuf, 'A') || t_iseq(state->prsbuf, '*'))
      {
        if (WEP_GETWEIGHT(pos[npos - 1]))
        {
          PRSSYNTAXERROR;
        }
        WEP_SETWEIGHT(pos[npos - 1], 3);
      }
      else if (t_iseq(state->prsbuf, 'b') || t_iseq(state->prsbuf, 'B'))
      {
        if (WEP_GETWEIGHT(pos[npos - 1]))
        {
          PRSSYNTAXERROR;
        }
        WEP_SETWEIGHT(pos[npos - 1], 2);
      }
      else if (t_iseq(state->prsbuf, 'c') || t_iseq(state->prsbuf, 'C'))
      {
        if (WEP_GETWEIGHT(pos[npos - 1]))
        {
          PRSSYNTAXERROR;
        }
        WEP_SETWEIGHT(pos[npos - 1], 1);
      }
      else if (t_iseq(state->prsbuf, 'd') || t_iseq(state->prsbuf, 'D'))
      {
        if (WEP_GETWEIGHT(pos[npos - 1]))
        {
          PRSSYNTAXERROR;
        }
        WEP_SETWEIGHT(pos[npos - 1], 0);
      }
      else if (t_isspace(state->prsbuf) || *(state->prsbuf) == '\0')
      {
        RETURN_TOKEN;
      }
      else if (!t_isdigit(state->prsbuf))
      {
        PRSSYNTAXERROR;
      }
    }
    else                     
    {
      elog(ERROR, "unrecognized state in gettoken_tsvector: %d", statecode);
    }

                       
    state->prsbuf += pg_mblen(state->prsbuf);
  }
}
