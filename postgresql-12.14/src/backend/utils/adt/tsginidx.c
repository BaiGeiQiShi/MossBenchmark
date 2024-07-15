                                                                            
   
              
                                           
   
                                                                         
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/gin.h"
#include "access/stratnum.h"
#include "miscadmin.h"
#include "tsearch/ts_type.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

Datum
gin_cmp_tslexeme(PG_FUNCTION_ARGS)
{
  text *a = PG_GETARG_TEXT_PP(0);
  text *b = PG_GETARG_TEXT_PP(1);
  int cmp;

  cmp = tsCompareString(VARDATA_ANY(a), VARSIZE_ANY_EXHDR(a), VARDATA_ANY(b), VARSIZE_ANY_EXHDR(b), false);

  PG_FREE_IF_COPY(a, 0);
  PG_FREE_IF_COPY(b, 1);
  PG_RETURN_INT32(cmp);
}

Datum
gin_cmp_prefix(PG_FUNCTION_ARGS)
{
  text *a = PG_GETARG_TEXT_PP(0);
  text *b = PG_GETARG_TEXT_PP(1);

#ifdef NOT_USED
  StrategyNumber strategy = PG_GETARG_UINT16(2);
  Pointer extra_data = PG_GETARG_POINTER(3);
#endif
  int cmp;

  cmp = tsCompareString(VARDATA_ANY(a), VARSIZE_ANY_EXHDR(a), VARDATA_ANY(b), VARSIZE_ANY_EXHDR(b), true);

  if (cmp < 0)
  {
    cmp = 1;                            
  }

  PG_FREE_IF_COPY(a, 0);
  PG_FREE_IF_COPY(b, 1);
  PG_RETURN_INT32(cmp);
}

Datum
gin_extract_tsvector(PG_FUNCTION_ARGS)
{
  TSVector vector = PG_GETARG_TSVECTOR(0);
  int32 *nentries = (int32 *)PG_GETARG_POINTER(1);
  Datum *entries = NULL;

  *nentries = vector->size;
  if (vector->size > 0)
  {
    int i;
    WordEntry *we = ARRPTR(vector);

    entries = (Datum *)palloc(sizeof(Datum) * vector->size);

    for (i = 0; i < vector->size; i++)
    {
      text *txt;

      txt = cstring_to_text_with_len(STRPTR(vector) + we->pos, we->len);
      entries[i] = PointerGetDatum(txt);

      we++;
    }
  }

  PG_FREE_IF_COPY(vector, 0);
  PG_RETURN_POINTER(entries);
}

Datum
gin_extract_tsquery(PG_FUNCTION_ARGS)
{
  TSQuery query = PG_GETARG_TSQUERY(0);
  int32 *nentries = (int32 *)PG_GETARG_POINTER(1);

                                                      
  bool **ptr_partialmatch = (bool **)PG_GETARG_POINTER(3);
  Pointer **extra_data = (Pointer **)PG_GETARG_POINTER(4);

                                                            
  int32 *searchMode = (int32 *)PG_GETARG_POINTER(6);
  Datum *entries = NULL;

  *nentries = 0;

  if (query->size > 0)
  {
    QueryItem *item = GETQUERY(query);
    int32 i, j;
    bool *partialmatch;
    int *map_item_operand;

       
                                                                    
                                                                          
             
       
    if (tsquery_requires_match(item))
    {
      *searchMode = GIN_SEARCH_MODE_DEFAULT;
    }
    else
    {
      *searchMode = GIN_SEARCH_MODE_ALL;
    }

                                   
    j = 0;
    for (i = 0; i < query->size; i++)
    {
      if (item[i].type == QI_VAL)
      {
        j++;
      }
    }
    *nentries = j;

    entries = (Datum *)palloc(sizeof(Datum) * j);
    partialmatch = *ptr_partialmatch = (bool *)palloc(sizeof(bool) * j);

       
                                                                         
                                                                       
                                                              
       
    *extra_data = (Pointer *)palloc(sizeof(Pointer) * j);
    map_item_operand = (int *)palloc0(sizeof(int) * query->size);

                                                         
    j = 0;
    for (i = 0; i < query->size; i++)
    {
      if (item[i].type == QI_VAL)
      {
        QueryOperand *val = &item[i].qoperand;
        text *txt;

        txt = cstring_to_text_with_len(GETOPERAND(query) + val->distance, val->length);
        entries[j] = PointerGetDatum(txt);
        partialmatch[j] = val->prefix;
        (*extra_data)[j] = (Pointer)map_item_operand;
        map_item_operand[i] = j;
        j++;
      }
    }
  }

  PG_FREE_IF_COPY(query, 0);

  PG_RETURN_POINTER(entries);
}

typedef struct
{
  QueryItem *first_item;
  GinTernaryValue *check;
  int *map_item_operand;
  bool *need_recheck;
} GinChkVal;

static GinTernaryValue
checkcondition_gin_internal(GinChkVal *gcv, QueryOperand *val, ExecPhraseData *data)
{
  int j;

     
                                                                    
                                       
     
  if (val->weight != 0 || data != NULL)
  {
    *(gcv->need_recheck) = true;
  }

                                                                         
  j = gcv->map_item_operand[((QueryItem *)val) - gcv->first_item];

                                                         
  return gcv->check[j];
}

   
                                                       
   
static bool
checkcondition_gin(void *checkval, QueryOperand *val, ExecPhraseData *data)
{
  return checkcondition_gin_internal((GinChkVal *)checkval, val, data) != GIN_FALSE;
}

   
                                                            
   
                                                                       
                                                                      
                                                               
                                      
   
static GinTernaryValue
TS_execute_ternary(GinChkVal *gcv, QueryItem *curitem, bool in_phrase)
{
  GinTernaryValue val1, val2, result;

                                                                          
  check_stack_depth();

  if (curitem->type == QI_VAL)
  {
    return checkcondition_gin_internal(gcv, (QueryOperand *)curitem, NULL                               );
  }

  switch (curitem->qoperator.oper)
  {
  case OP_NOT:

       
                                                                      
                                                                   
                                                                    
                                                                     
                                                                       
                                                                       
                                      
       
    if (in_phrase)
    {
      return GIN_MAYBE;
    }

    result = TS_execute_ternary(gcv, curitem + 1, in_phrase);
    if (result == GIN_MAYBE)
    {
      return result;
    }
    return !result;

  case OP_PHRASE:

       
                                                                     
                                                                
                                 
       
    *(gcv->need_recheck) = true;
                                                                 
    in_phrase = true;

                   

  case OP_AND:
    val1 = TS_execute_ternary(gcv, curitem + curitem->qoperator.left, in_phrase);
    if (val1 == GIN_FALSE)
    {
      return GIN_FALSE;
    }
    val2 = TS_execute_ternary(gcv, curitem + 1, in_phrase);
    if (val2 == GIN_FALSE)
    {
      return GIN_FALSE;
    }
    if (val1 == GIN_TRUE && val2 == GIN_TRUE && curitem->qoperator.oper != OP_PHRASE)
    {
      return GIN_TRUE;
    }
    else
    {
      return GIN_MAYBE;
    }

  case OP_OR:
    val1 = TS_execute_ternary(gcv, curitem + curitem->qoperator.left, in_phrase);
    if (val1 == GIN_TRUE)
    {
      return GIN_TRUE;
    }
    val2 = TS_execute_ternary(gcv, curitem + 1, in_phrase);
    if (val2 == GIN_TRUE)
    {
      return GIN_TRUE;
    }
    if (val1 == GIN_FALSE && val2 == GIN_FALSE)
    {
      return GIN_FALSE;
    }
    else
    {
      return GIN_MAYBE;
    }

  default:
    elog(ERROR, "unrecognized operator: %d", curitem->qoperator.oper);
  }

                                              
  return false;
}

Datum
gin_tsquery_consistent(PG_FUNCTION_ARGS)
{
  bool *check = (bool *)PG_GETARG_POINTER(0);

                                                      
  TSQuery query = PG_GETARG_TSQUERY(2);

                                         
  Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
  bool *recheck = (bool *)PG_GETARG_POINTER(5);
  bool res = false;

                                                      
  *recheck = false;

  if (query->size > 0)
  {
    GinChkVal gcv;

       
                                                                           
              
       
    gcv.first_item = GETQUERY(query);
    StaticAssertStmt(sizeof(GinTernaryValue) == sizeof(bool), "sizes of GinTernaryValue and bool are not equal");
    gcv.check = (GinTernaryValue *)check;
    gcv.map_item_operand = (int *)(extra_data[0]);
    gcv.need_recheck = recheck;

    res = TS_execute(GETQUERY(query), &gcv, TS_EXEC_CALC_NOT | TS_EXEC_PHRASE_NO_POS, checkcondition_gin);
  }

  PG_RETURN_BOOL(res);
}

Datum
gin_tsquery_triconsistent(PG_FUNCTION_ARGS)
{
  GinTernaryValue *check = (GinTernaryValue *)PG_GETARG_POINTER(0);

                                                      
  TSQuery query = PG_GETARG_TSQUERY(2);

                                         
  Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
  GinTernaryValue res = GIN_FALSE;
  bool recheck;

                                                      
  recheck = false;

  if (query->size > 0)
  {
    GinChkVal gcv;

       
                                                                           
              
       
    gcv.first_item = GETQUERY(query);
    gcv.check = check;
    gcv.map_item_operand = (int *)(extra_data[0]);
    gcv.need_recheck = &recheck;

    res = TS_execute_ternary(&gcv, GETQUERY(query), false);

    if (res == GIN_TRUE && recheck)
    {
      res = GIN_MAYBE;
    }
  }

  PG_RETURN_GIN_TERNARY_VALUE(res);
}

   
                                                                             
                                                                        
                                                                      
                                                                           
                                                                            
                                                                          
   
Datum
gin_extract_tsvector_2args(PG_FUNCTION_ARGS)
{
  if (PG_NARGS() < 3)                        
  {
    elog(ERROR, "gin_extract_tsvector requires three arguments");
  }
  return gin_extract_tsvector(fcinfo);
}

   
                                                                         
                        
   
Datum
gin_extract_tsquery_5args(PG_FUNCTION_ARGS)
{
  if (PG_NARGS() < 7)                        
  {
    elog(ERROR, "gin_extract_tsquery requires seven arguments");
  }
  return gin_extract_tsquery(fcinfo);
}

   
                                                                            
                       
   
Datum
gin_tsquery_consistent_6args(PG_FUNCTION_ARGS)
{
  if (PG_NARGS() < 8)                        
  {
    elog(ERROR, "gin_tsquery_consistent requires eight arguments");
  }
  return gin_tsquery_consistent(fcinfo);
}

   
                                                                          
                                                    
   
Datum
gin_extract_tsquery_oldsig(PG_FUNCTION_ARGS)
{
  return gin_extract_tsquery(fcinfo);
}

   
                                                                             
                                                    
   
Datum
gin_tsquery_consistent_oldsig(PG_FUNCTION_ARGS)
{
  return gin_tsquery_consistent(fcinfo);
}
