                                                                            
   
                   
                                          
   
                                                                             
                                                                            
                                                                     
                                                                                
                                                 
   
                                                                         
                                                                               
                                                                           
                                                                               
                                                                           
                                                                             
                  
                                              
                                              
                                                   
   
                                                                          
                                                                        
                                                                              
                                                                        
                                                                             
                                                                               
                                                                           
                                          
   
                                                                             
                                                                       
                                                                        
                                                                                 
                                                                              
                                                                                    
                                                  
   
                                                                           
                                                                           
                                                                             
                                                                         
                                                                               
                                                                  
   
                                                                               
                                                                              
                                                                        
                                                                            
   
                                                           
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "regex/regex.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/formatting.h"
#include "utils/float.h"
#include "utils/guc.h"
#include "utils/json.h"
#include "utils/jsonpath.h"
#include "utils/date.h"
#include "utils/timestamp.h"
#include "utils/varlena.h"

   
                                                                      
   
typedef struct JsonBaseObjectInfo
{
  JsonbContainer *jbc;
  int id;
} JsonBaseObjectInfo;

   
                                  
   
typedef struct JsonPathExecContext
{
  Jsonb *vars;                                                              
  JsonbValue *root;                                    
  JsonbValue *current;                                 
  JsonBaseObjectInfo baseObject;                                  
                                                 
  int lastGeneratedObjectId;                                     
                                                 
  int innermostArraySize;                                             
  bool laxMode;                                                             
                                           
  bool ignoreStructuralErrors;                                         
                                                                        
                                                                  
                                              
  bool throwErrors;                                                          
                                                 
} JsonPathExecContext;

                                       
typedef struct JsonLikeRegexContext
{
  text *regex;
  int cflags;
} JsonLikeRegexContext;

                                             
typedef enum JsonPathBool
{
  jpbFalse = 0,
  jpbTrue = 1,
  jpbUnknown = 2
} JsonPathBool;

                                              
typedef enum JsonPathExecResult
{
  jperOk = 0,
  jperNotFound = 1,
  jperError = 2
} JsonPathExecResult;

#define jperIsError(jper) ((jper) == jperError)

   
                                                             
   
typedef struct JsonValueList
{
  JsonbValue *singleton;
  List *list;
} JsonValueList;

typedef struct JsonValueListIterator
{
  JsonbValue *value;
  ListCell *next;
} JsonValueListIterator;

                                                                   
#define jspStrictAbsenseOfErrors(cxt) (!(cxt)->laxMode)
#define jspAutoUnwrap(cxt) ((cxt)->laxMode)
#define jspAutoWrap(cxt) ((cxt)->laxMode)
#define jspIgnoreStructuralErrors(cxt) ((cxt)->ignoreStructuralErrors)
#define jspThrowErrors(cxt) ((cxt)->throwErrors)

                                                                   
#define RETURN_ERROR(throw_error)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (jspThrowErrors(cxt))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      throw_error;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      return jperError;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  } while (0)

typedef JsonPathBool (*JsonPathPredicateCallback)(JsonPathItem *jsp, JsonbValue *larg, JsonbValue *rarg, void *param);
typedef Numeric (*BinaryArithmFunc)(Numeric num1, Numeric num2, bool *error);

static JsonPathExecResult
executeJsonPath(JsonPath *path, Jsonb *vars, Jsonb *json, bool throwErrors, JsonValueList *result);
static JsonPathExecResult
executeItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found);
static JsonPathExecResult
executeItemOptUnwrapTarget(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found, bool unwrap);
static JsonPathExecResult
executeItemUnwrapTargetArray(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found, bool unwrapElements);
static JsonPathExecResult
executeNextItem(JsonPathExecContext *cxt, JsonPathItem *cur, JsonPathItem *next, JsonbValue *v, JsonValueList *found, bool copy);
static JsonPathExecResult
executeItemOptUnwrapResult(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, JsonValueList *found);
static JsonPathExecResult
executeItemOptUnwrapResultNoThrow(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, JsonValueList *found);
static JsonPathBool
executeBoolItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool canHaveNext);
static JsonPathBool
executeNestedBoolItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb);
static JsonPathExecResult
executeAnyItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbContainer *jbc, JsonValueList *found, uint32 level, uint32 first, uint32 last, bool ignoreStructuralErrors, bool unwrapNext);
static JsonPathBool
executePredicate(JsonPathExecContext *cxt, JsonPathItem *pred, JsonPathItem *larg, JsonPathItem *rarg, JsonbValue *jb, bool unwrapRightArg, JsonPathPredicateCallback exec, void *param);
static JsonPathExecResult
executeBinaryArithmExpr(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, BinaryArithmFunc func, JsonValueList *found);
static JsonPathExecResult
executeUnaryArithmExpr(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, PGFunction func, JsonValueList *found);
static JsonPathBool
executeStartsWith(JsonPathItem *jsp, JsonbValue *whole, JsonbValue *initial, void *param);
static JsonPathBool
executeLikeRegex(JsonPathItem *jsp, JsonbValue *str, JsonbValue *rarg, void *param);
static JsonPathExecResult
executeNumericItemMethod(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, PGFunction func, JsonValueList *found);
static JsonPathExecResult
executeKeyValueMethod(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found);
static JsonPathExecResult
appendBoolResult(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonValueList *found, JsonPathBool res);
static void
getJsonPathItem(JsonPathExecContext *cxt, JsonPathItem *item, JsonbValue *value);
static void
getJsonPathVariable(JsonPathExecContext *cxt, JsonPathItem *variable, Jsonb *vars, JsonbValue *value);
static int
JsonbArraySize(JsonbValue *jb);
static JsonPathBool
executeComparison(JsonPathItem *cmp, JsonbValue *lv, JsonbValue *rv, void *p);
static JsonPathBool
compareItems(int32 op, JsonbValue *jb1, JsonbValue *jb2);
static int
compareNumeric(Numeric a, Numeric b);
static JsonbValue *
copyJsonbValue(JsonbValue *src);
static JsonPathExecResult
getArrayIndex(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, int32 *index);
static JsonBaseObjectInfo
setBaseObject(JsonPathExecContext *cxt, JsonbValue *jbv, int32 id);
static void
JsonValueListAppend(JsonValueList *jvl, JsonbValue *jbv);
static int
JsonValueListLength(const JsonValueList *jvl);
static bool
JsonValueListIsEmpty(JsonValueList *jvl);
static JsonbValue *
JsonValueListHead(JsonValueList *jvl);
static List *
JsonValueListGetList(JsonValueList *jvl);
static void
JsonValueListInitIterator(const JsonValueList *jvl, JsonValueListIterator *it);
static JsonbValue *
JsonValueListNext(const JsonValueList *jvl, JsonValueListIterator *it);
static int
JsonbType(JsonbValue *jb);
static JsonbValue *
JsonbInitBinary(JsonbValue *jbv, Jsonb *jb);
static int
JsonbType(JsonbValue *jb);
static JsonbValue *
getScalar(JsonbValue *scalar, enum jbvType type);
static JsonbValue *
wrapItemsInArray(const JsonValueList *items);

                                                                             

   
                     
                                                                         
                                                                   
                                                                         
                                                                      
                                                                        
                                                                       
                                                                   
                                                                        
                                                                  
   
Datum
jsonb_path_exists(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  JsonPath *jp = PG_GETARG_JSONPATH_P(1);
  JsonPathExecResult res;
  Jsonb *vars = NULL;
  bool silent = true;

  if (PG_NARGS() == 4)
  {
    vars = PG_GETARG_JSONB_P(2);
    silent = PG_GETARG_BOOL(3);
  }

  res = executeJsonPath(jp, vars, jb, !silent, NULL);

  PG_FREE_IF_COPY(jb, 0);
  PG_FREE_IF_COPY(jp, 1);

  if (jperIsError(res))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(res == jperOk);
}

   
                         
                                                                          
                          
   
Datum
jsonb_path_exists_opr(PG_FUNCTION_ARGS)
{
                                                           
  return jsonb_path_exists(fcinfo);
}

   
                    
                                                                          
                                                                          
   
Datum
jsonb_path_match(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  JsonPath *jp = PG_GETARG_JSONPATH_P(1);
  JsonValueList found = {0};
  Jsonb *vars = NULL;
  bool silent = true;

  if (PG_NARGS() == 4)
  {
    vars = PG_GETARG_JSONB_P(2);
    silent = PG_GETARG_BOOL(3);
  }

  (void)executeJsonPath(jp, vars, jb, !silent, &found);

  PG_FREE_IF_COPY(jb, 0);
  PG_FREE_IF_COPY(jp, 1);

  if (JsonValueListLength(&found) == 1)
  {
    JsonbValue *jbv = JsonValueListHead(&found);

    if (jbv->type == jbvBool)
    {
      PG_RETURN_BOOL(jbv->val.boolean);
    }

    if (jbv->type == jbvNull)
    {
      PG_RETURN_NULL();
    }
  }

  if (!silent)
  {
    ereport(ERROR, (errcode(ERRCODE_SINGLETON_SQL_JSON_ITEM_REQUIRED), errmsg("single boolean result is expected")));
  }

  PG_RETURN_NULL();
}

   
                        
                                                                          
                         
   
Datum
jsonb_path_match_opr(PG_FUNCTION_ARGS)
{
                                                           
  return jsonb_path_match(fcinfo);
}

   
                    
                                                                     
            
   
Datum
jsonb_path_query(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  List *found;
  JsonbValue *v;
  ListCell *c;

  if (SRF_IS_FIRSTCALL())
  {
    JsonPath *jp;
    Jsonb *jb;
    MemoryContext oldcontext;
    Jsonb *vars;
    bool silent;
    JsonValueList found = {0};

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    jb = PG_GETARG_JSONB_P_COPY(0);
    jp = PG_GETARG_JSONPATH_P_COPY(1);
    vars = PG_GETARG_JSONB_P_COPY(2);
    silent = PG_GETARG_BOOL(3);

    (void)executeJsonPath(jp, vars, jb, !silent, &found);

    funcctx->user_fctx = JsonValueListGetList(&found);

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  found = funcctx->user_fctx;

  c = list_head(found);

  if (c == NULL)
  {
    SRF_RETURN_DONE(funcctx);
  }

  v = lfirst(c);
  funcctx->user_fctx = list_delete_first(found);

  SRF_RETURN_NEXT(funcctx, JsonbPGetDatum(JsonbValueToJsonb(v)));
}

   
                          
                                                                     
                 
   
Datum
jsonb_path_query_array(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  JsonPath *jp = PG_GETARG_JSONPATH_P(1);
  JsonValueList found = {0};
  Jsonb *vars = PG_GETARG_JSONB_P(2);
  bool silent = PG_GETARG_BOOL(3);

  (void)executeJsonPath(jp, vars, jb, !silent, &found);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(wrapItemsInArray(&found)));
}

   
                          
                                                                        
                                                 
   
Datum
jsonb_path_query_first(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  JsonPath *jp = PG_GETARG_JSONPATH_P(1);
  JsonValueList found = {0};
  Jsonb *vars = PG_GETARG_JSONB_P(2);
  bool silent = PG_GETARG_BOOL(3);

  (void)executeJsonPath(jp, vars, jb, !silent, &found);

  if (JsonValueListLength(&found) >= 1)
  {
    PG_RETURN_JSONB_P(JsonbValueToJsonb(JsonValueListHead(&found)));
  }
  else
  {
    PG_RETURN_NULL();
  }
}

                                                                              

   
                                  
   
                                    
                                                    
                                                    
                                                               
                                              
   
                                                                              
                
   
                                                                            
                                                                             
                                                                         
                                                                          
                                                                             
                                                                  
   
static JsonPathExecResult
executeJsonPath(JsonPath *path, Jsonb *vars, Jsonb *json, bool throwErrors, JsonValueList *result)
{
  JsonPathExecContext cxt;
  JsonPathExecResult res;
  JsonPathItem jsp;
  JsonbValue jbv;

  jspInit(&jsp, path);

  if (!JsonbExtractScalar(&json->root, &jbv))
  {
    JsonbInitBinary(&jbv, json);
  }

  if (vars && !JsonContainerIsObject(&vars->root))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"vars\" argument is not an object"), errdetail("Jsonpath parameters should be encoded as key-value pairs of \"vars\" object.")));
  }

  cxt.vars = vars;
  cxt.laxMode = (path->header & JSONPATH_LAX) != 0;
  cxt.ignoreStructuralErrors = cxt.laxMode;
  cxt.root = &jbv;
  cxt.current = &jbv;
  cxt.baseObject.jbc = NULL;
  cxt.baseObject.id = 0;
  cxt.lastGeneratedObjectId = vars ? 2 : 1;
  cxt.innermostArraySize = -1;
  cxt.throwErrors = throwErrors;

  if (jspStrictAbsenseOfErrors(&cxt) && !result)
  {
       
                                                                          
                                   
       
    JsonValueList vals = {0};

    res = executeItem(&cxt, &jsp, &jbv, &vals);

    if (jperIsError(res))
    {
      return res;
    }

    return JsonValueListIsEmpty(&vals) ? jperNotFound : jperOk;
  }

  res = executeItem(&cxt, &jsp, &jbv, result);

  Assert(!throwErrors || !jperIsError(res));

  return res;
}

   
                                                                           
   
static JsonPathExecResult
executeItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found)
{
  return executeItemOptUnwrapTarget(cxt, jsp, jb, found, jspAutoUnwrap(cxt));
}

   
                                                                       
                                                                
                                                                               
   
static JsonPathExecResult
executeItemOptUnwrapTarget(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found, bool unwrap)
{
  JsonPathItem elem;
  JsonPathExecResult res = jperNotFound;
  JsonBaseObjectInfo baseObject;

  check_stack_depth();
  CHECK_FOR_INTERRUPTS();

  switch (jsp->type)
  {
                                 
  case jpiAnd:
  case jpiOr:
  case jpiNot:
  case jpiIsUnknown:
  case jpiEqual:
  case jpiNotEqual:
  case jpiLess:
  case jpiGreater:
  case jpiLessOrEqual:
  case jpiGreaterOrEqual:
  case jpiExists:
  case jpiStartsWith:
  case jpiLikeRegex:
  {
    JsonPathBool st = executeBoolItem(cxt, jsp, jb, true);

    res = appendBoolResult(cxt, jsp, found, st);
    break;
  }

  case jpiKey:
    if (JsonbType(jb) == jbvObject)
    {
      JsonbValue *v;
      JsonbValue key;

      key.type = jbvString;
      key.val.string.val = jspGetString(jsp, &key.val.string.len);

      v = findJsonbValueFromContainer(jb->val.binary.data, JB_FOBJECT, &key);

      if (v != NULL)
      {
        res = executeNextItem(cxt, jsp, NULL, v, found, false);

                                                          
        if (jspHasNext(jsp) || !found)
        {
          pfree(v);
        }
      }
      else if (!jspIgnoreStructuralErrors(cxt))
      {
        Assert(found);

        if (!jspThrowErrors(cxt))
        {
          return jperError;
        }

        ereport(ERROR, (errcode(ERRCODE_SQL_JSON_MEMBER_NOT_FOUND), errmsg("JSON object does not contain key \"%s\"", pnstrdup(key.val.string.val, key.val.string.len))));
      }
    }
    else if (unwrap && JsonbType(jb) == jbvArray)
    {
      return executeItemUnwrapTargetArray(cxt, jsp, jb, found, false);
    }
    else if (!jspIgnoreStructuralErrors(cxt))
    {
      Assert(found);
      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_MEMBER_NOT_FOUND), errmsg("jsonpath member accessor can only be applied to an object"))));
    }
    break;

  case jpiRoot:
    jb = cxt->root;
    baseObject = setBaseObject(cxt, jb, 0);
    res = executeNextItem(cxt, jsp, NULL, jb, found, true);
    cxt->baseObject = baseObject;
    break;

  case jpiCurrent:
    res = executeNextItem(cxt, jsp, NULL, cxt->current, found, true);
    break;

  case jpiAnyArray:
    if (JsonbType(jb) == jbvArray)
    {
      bool hasNext = jspGetNext(jsp, &elem);

      res = executeItemUnwrapTargetArray(cxt, hasNext ? &elem : NULL, jb, found, jspAutoUnwrap(cxt));
    }
    else if (jspAutoWrap(cxt))
    {
      res = executeNextItem(cxt, jsp, NULL, jb, found, true);
    }
    else if (!jspIgnoreStructuralErrors(cxt))
    {
      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_ARRAY_NOT_FOUND), errmsg("jsonpath wildcard array accessor can only be applied to an array"))));
    }
    break;

  case jpiIndexArray:
    if (JsonbType(jb) == jbvArray || jspAutoWrap(cxt))
    {
      int innermostArraySize = cxt->innermostArraySize;
      int i;
      int size = JsonbArraySize(jb);
      bool singleton = size < 0;
      bool hasNext = jspGetNext(jsp, &elem);

      if (singleton)
      {
        size = 1;
      }

      cxt->innermostArraySize = size;                          

      for (i = 0; i < jsp->content.array.nelems; i++)
      {
        JsonPathItem from;
        JsonPathItem to;
        int32 index;
        int32 index_from;
        int32 index_to;
        bool range = jspGetArraySubscript(jsp, &from, &to, i);

        res = getArrayIndex(cxt, &from, jb, &index_from);

        if (jperIsError(res))
        {
          break;
        }

        if (range)
        {
          res = getArrayIndex(cxt, &to, jb, &index_to);

          if (jperIsError(res))
          {
            break;
          }
        }
        else
        {
          index_to = index_from;
        }

        if (!jspIgnoreStructuralErrors(cxt) && (index_from < 0 || index_from > index_to || index_to >= size))
        {
          RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_INVALID_SQL_JSON_SUBSCRIPT), errmsg("jsonpath array subscript is out of bounds"))));
        }

        if (index_from < 0)
        {
          index_from = 0;
        }

        if (index_to >= size)
        {
          index_to = size - 1;
        }

        res = jperNotFound;

        for (index = index_from; index <= index_to; index++)
        {
          JsonbValue *v;
          bool copy;

          if (singleton)
          {
            v = jb;
            copy = true;
          }
          else
          {
            v = getIthJsonbValueFromContainer(jb->val.binary.data, (uint32)index);

            if (v == NULL)
            {
              continue;
            }

            copy = false;
          }

          if (!hasNext && !found)
          {
            return jperOk;
          }

          res = executeNextItem(cxt, jsp, &elem, v, found, copy);

          if (jperIsError(res))
          {
            break;
          }

          if (res == jperOk && !found)
          {
            break;
          }
        }

        if (jperIsError(res))
        {
          break;
        }

        if (res == jperOk && !found)
        {
          break;
        }
      }

      cxt->innermostArraySize = innermostArraySize;
    }
    else if (!jspIgnoreStructuralErrors(cxt))
    {
      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_ARRAY_NOT_FOUND), errmsg("jsonpath array accessor can only be applied to an array"))));
    }
    break;

  case jpiLast:
  {
    JsonbValue tmpjbv;
    JsonbValue *lastjbv;
    int last;
    bool hasNext = jspGetNext(jsp, &elem);

    if (cxt->innermostArraySize < 0)
    {
      elog(ERROR, "evaluating jsonpath LAST outside of array subscript");
    }

    if (!hasNext && !found)
    {
      res = jperOk;
      break;
    }

    last = cxt->innermostArraySize - 1;

    lastjbv = hasNext ? &tmpjbv : palloc(sizeof(*lastjbv));

    lastjbv->type = jbvNumeric;
    lastjbv->val.numeric = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(last)));

    res = executeNextItem(cxt, jsp, &elem, lastjbv, found, hasNext);
  }
  break;

  case jpiAnyKey:
    if (JsonbType(jb) == jbvObject)
    {
      bool hasNext = jspGetNext(jsp, &elem);

      if (jb->type != jbvBinary)
      {
        elog(ERROR, "invalid jsonb object type: %d", jb->type);
      }

      return executeAnyItem(cxt, hasNext ? &elem : NULL, jb->val.binary.data, found, 1, 1, 1, false, jspAutoUnwrap(cxt));
    }
    else if (unwrap && JsonbType(jb) == jbvArray)
    {
      return executeItemUnwrapTargetArray(cxt, jsp, jb, found, false);
    }
    else if (!jspIgnoreStructuralErrors(cxt))
    {
      Assert(found);
      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_OBJECT_NOT_FOUND), errmsg("jsonpath wildcard member accessor can only be applied to an object"))));
    }
    break;

  case jpiAdd:
    return executeBinaryArithmExpr(cxt, jsp, jb, numeric_add_opt_error, found);

  case jpiSub:
    return executeBinaryArithmExpr(cxt, jsp, jb, numeric_sub_opt_error, found);

  case jpiMul:
    return executeBinaryArithmExpr(cxt, jsp, jb, numeric_mul_opt_error, found);

  case jpiDiv:
    return executeBinaryArithmExpr(cxt, jsp, jb, numeric_div_opt_error, found);

  case jpiMod:
    return executeBinaryArithmExpr(cxt, jsp, jb, numeric_mod_opt_error, found);

  case jpiPlus:
    return executeUnaryArithmExpr(cxt, jsp, jb, NULL, found);

  case jpiMinus:
    return executeUnaryArithmExpr(cxt, jsp, jb, numeric_uminus, found);

  case jpiFilter:
  {
    JsonPathBool st;

    if (unwrap && JsonbType(jb) == jbvArray)
    {
      return executeItemUnwrapTargetArray(cxt, jsp, jb, found, false);
    }

    jspGetArg(jsp, &elem);
    st = executeNestedBoolItem(cxt, &elem, jb);
    if (st != jpbTrue)
    {
      res = jperNotFound;
    }
    else
    {
      res = executeNextItem(cxt, jsp, NULL, jb, found, true);
    }
    break;
  }

  case jpiAny:
  {
    bool hasNext = jspGetNext(jsp, &elem);

                                                  
    if (jsp->content.anybounds.first == 0)
    {
      bool savedIgnoreStructuralErrors;

      savedIgnoreStructuralErrors = cxt->ignoreStructuralErrors;
      cxt->ignoreStructuralErrors = true;
      res = executeNextItem(cxt, jsp, &elem, jb, found, true);
      cxt->ignoreStructuralErrors = savedIgnoreStructuralErrors;

      if (res == jperOk && !found)
      {
        break;
      }
    }

    if (jb->type == jbvBinary)
    {
      res = executeAnyItem(cxt, hasNext ? &elem : NULL, jb->val.binary.data, found, 1, jsp->content.anybounds.first, jsp->content.anybounds.last, true, jspAutoUnwrap(cxt));
    }
    break;
  }

  case jpiNull:
  case jpiBool:
  case jpiNumeric:
  case jpiString:
  case jpiVariable:
  {
    JsonbValue vbuf;
    JsonbValue *v;
    bool hasNext = jspGetNext(jsp, &elem);

    if (!hasNext && !found && jsp->type != jpiVariable)
    {
         
                                                          
                                                    
         
      res = jperOk;
      break;
    }

    v = hasNext ? &vbuf : palloc(sizeof(*v));

    baseObject = cxt->baseObject;
    getJsonPathItem(cxt, jsp, v);

    res = executeNextItem(cxt, jsp, &elem, v, found, hasNext);
    cxt->baseObject = baseObject;
  }
  break;

  case jpiType:
  {
    JsonbValue *jbv = palloc(sizeof(*jbv));

    jbv->type = jbvString;
    jbv->val.string.val = pstrdup(JsonbTypeName(jb));
    jbv->val.string.len = strlen(jbv->val.string.val);

    res = executeNextItem(cxt, jsp, NULL, jbv, found, false);
  }
  break;

  case jpiSize:
  {
    int size = JsonbArraySize(jb);

    if (size < 0)
    {
      if (!jspAutoWrap(cxt))
      {
        if (!jspIgnoreStructuralErrors(cxt))
        {
          RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_ARRAY_NOT_FOUND), errmsg("jsonpath item method .%s() can only be applied to an array", jspOperationName(jsp->type)))));
        }
        break;
      }

      size = 1;
    }

    jb = palloc(sizeof(*jb));

    jb->type = jbvNumeric;
    jb->val.numeric = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(size)));

    res = executeNextItem(cxt, jsp, NULL, jb, found, false);
  }
  break;

  case jpiAbs:
    return executeNumericItemMethod(cxt, jsp, jb, unwrap, numeric_abs, found);

  case jpiFloor:
    return executeNumericItemMethod(cxt, jsp, jb, unwrap, numeric_floor, found);

  case jpiCeiling:
    return executeNumericItemMethod(cxt, jsp, jb, unwrap, numeric_ceil, found);

  case jpiDouble:
  {
    JsonbValue jbv;

    if (unwrap && JsonbType(jb) == jbvArray)
    {
      return executeItemUnwrapTargetArray(cxt, jsp, jb, found, false);
    }

    if (jb->type == jbvNumeric)
    {
      char *tmp = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(jb->val.numeric)));
      double val;
      bool have_error = false;

      val = float8in_internal_opt_error(tmp, NULL, "double precision", tmp, &have_error);

      if (have_error || isinf(val) || isnan(val))
      {
        RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM), errmsg("numeric argument of jsonpath item method .%s() is out of range for type double precision", jspOperationName(jsp->type)))));
      }
      res = jperOk;
    }
    else if (jb->type == jbvString)
    {
                                 
      double val;
      char *tmp = pnstrdup(jb->val.string.val, jb->val.string.len);
      bool have_error = false;

      val = float8in_internal_opt_error(tmp, NULL, "double precision", tmp, &have_error);

      if (have_error || isinf(val) || isnan(val))
      {
        RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM), errmsg("string argument of jsonpath item method .%s() is not a valid representation of a double precision number", jspOperationName(jsp->type)))));
      }

      jb = &jbv;
      jb->type = jbvNumeric;
      jb->val.numeric = DatumGetNumeric(DirectFunctionCall1(float8_numeric, Float8GetDatum(val)));
      res = jperOk;
    }

    if (res == jperNotFound)
    {
      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM), errmsg("jsonpath item method .%s() can only be applied to a string or numeric value", jspOperationName(jsp->type)))));
    }

    res = executeNextItem(cxt, jsp, NULL, jb, found, true);
  }
  break;

  case jpiKeyValue:
    if (unwrap && JsonbType(jb) == jbvArray)
    {
      return executeItemUnwrapTargetArray(cxt, jsp, jb, found, false);
    }

    return executeKeyValueMethod(cxt, jsp, jb, found);

  default:
    elog(ERROR, "unrecognized jsonpath item type: %d", jsp->type);
  }

  return res;
}

   
                                                                            
   
static JsonPathExecResult
executeItemUnwrapTargetArray(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found, bool unwrapElements)
{
  if (jb->type != jbvBinary)
  {
    Assert(jb->type != jbvArray);
    elog(ERROR, "invalid jsonb array value type: %d", jb->type);
  }

  return executeAnyItem(cxt, jsp, jb->val.binary.data, found, 1, 1, 1, false, unwrapElements);
}

   
                                                                           
                     
   
static JsonPathExecResult
executeNextItem(JsonPathExecContext *cxt, JsonPathItem *cur, JsonPathItem *next, JsonbValue *v, JsonValueList *found, bool copy)
{
  JsonPathItem elem;
  bool hasNext;

  if (!cur)
  {
    hasNext = next != NULL;
  }
  else if (next)
  {
    hasNext = jspHasNext(cur);
  }
  else
  {
    next = &elem;
    hasNext = jspGetNext(cur, next);
  }

  if (hasNext)
  {
    return executeItem(cxt, next, v, found);
  }

  if (found)
  {
    JsonValueListAppend(found, copy ? copyJsonbValue(v) : v);
  }

  return jperOk;
}

   
                                                                          
                                                            
   
static JsonPathExecResult
executeItemOptUnwrapResult(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, JsonValueList *found)
{
  if (unwrap && jspAutoUnwrap(cxt))
  {
    JsonValueList seq = {0};
    JsonValueListIterator it;
    JsonPathExecResult res = executeItem(cxt, jsp, jb, &seq);
    JsonbValue *item;

    if (jperIsError(res))
    {
      return res;
    }

    JsonValueListInitIterator(&seq, &it);
    while ((item = JsonValueListNext(&seq, &it)))
    {
      Assert(item->type != jbvArray);

      if (JsonbType(item) == jbvArray)
      {
        executeItemUnwrapTargetArray(cxt, NULL, item, found, false);
      }
      else
      {
        JsonValueListAppend(found, item);
      }
    }

    return jperOk;
  }

  return executeItem(cxt, jsp, jb, found);
}

   
                                                                     
   
static JsonPathExecResult
executeItemOptUnwrapResultNoThrow(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, JsonValueList *found)
{
  JsonPathExecResult res;
  bool throwErrors = cxt->throwErrors;

  cxt->throwErrors = false;
  res = executeItemOptUnwrapResult(cxt, jsp, jb, unwrap, found);
  cxt->throwErrors = throwErrors;

  return res;
}

                                                 
static JsonPathBool
executeBoolItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool canHaveNext)
{
  JsonPathItem larg;
  JsonPathItem rarg;
  JsonPathBool res;
  JsonPathBool res2;

  if (!canHaveNext && jspHasNext(jsp))
  {
    elog(ERROR, "boolean jsonpath item cannot have next item");
  }

  switch (jsp->type)
  {
  case jpiAnd:
    jspGetLeftArg(jsp, &larg);
    res = executeBoolItem(cxt, &larg, jb, false);

    if (res == jpbFalse)
    {
      return jpbFalse;
    }

       
                                                                
                 
       

    jspGetRightArg(jsp, &rarg);
    res2 = executeBoolItem(cxt, &rarg, jb, false);

    return res2 == jpbTrue ? res : res2;

  case jpiOr:
    jspGetLeftArg(jsp, &larg);
    res = executeBoolItem(cxt, &larg, jb, false);

    if (res == jpbTrue)
    {
      return jpbTrue;
    }

    jspGetRightArg(jsp, &rarg);
    res2 = executeBoolItem(cxt, &rarg, jb, false);

    return res2 == jpbFalse ? res : res2;

  case jpiNot:
    jspGetArg(jsp, &larg);

    res = executeBoolItem(cxt, &larg, jb, false);

    if (res == jpbUnknown)
    {
      return jpbUnknown;
    }

    return res == jpbTrue ? jpbFalse : jpbTrue;

  case jpiIsUnknown:
    jspGetArg(jsp, &larg);
    res = executeBoolItem(cxt, &larg, jb, false);
    return res == jpbUnknown ? jpbTrue : jpbFalse;

  case jpiEqual:
  case jpiNotEqual:
  case jpiLess:
  case jpiGreater:
  case jpiLessOrEqual:
  case jpiGreaterOrEqual:
    jspGetLeftArg(jsp, &larg);
    jspGetRightArg(jsp, &rarg);
    return executePredicate(cxt, jsp, &larg, &rarg, jb, true, executeComparison, NULL);

  case jpiStartsWith:                                            
    jspGetLeftArg(jsp, &larg);               
    jspGetRightArg(jsp, &rarg);                
    return executePredicate(cxt, jsp, &larg, &rarg, jb, false, executeStartsWith, NULL);

  case jpiLikeRegex:                                            
  {
       
                                                                  
                                                                
                                                                
                                                                  
       
    JsonLikeRegexContext lrcxt = {0};

    jspInitByBuffer(&larg, jsp->base, jsp->content.like_regex.expr);

    return executePredicate(cxt, jsp, &larg, NULL, jb, false, executeLikeRegex, &lrcxt);
  }

  case jpiExists:
    jspGetArg(jsp, &larg);

    if (jspStrictAbsenseOfErrors(cxt))
    {
         
                                                                 
                                                
         
      JsonValueList vals = {0};
      JsonPathExecResult res = executeItemOptUnwrapResultNoThrow(cxt, &larg, jb, false, &vals);

      if (jperIsError(res))
      {
        return jpbUnknown;
      }

      return JsonValueListIsEmpty(&vals) ? jpbFalse : jpbTrue;
    }
    else
    {
      JsonPathExecResult res = executeItemOptUnwrapResultNoThrow(cxt, &larg, jb, false, NULL);

      if (jperIsError(res))
      {
        return jpbUnknown;
      }

      return res == jperOk ? jpbTrue : jpbFalse;
    }

  default:
    elog(ERROR, "invalid boolean jsonpath item type: %d", jsp->type);
    return jpbUnknown;
  }
}

   
                                                                             
                        
   
static JsonPathBool
executeNestedBoolItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb)
{
  JsonbValue *prev;
  JsonPathBool res;

  prev = cxt->current;
  cxt->current = jb;
  res = executeBoolItem(cxt, jsp, jb, false);
  cxt->current = prev;

  return res;
}

   
                                             
                             
                               
                                 
   
static JsonPathExecResult
executeAnyItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbContainer *jbc, JsonValueList *found, uint32 level, uint32 first, uint32 last, bool ignoreStructuralErrors, bool unwrapNext)
{
  JsonPathExecResult res = jperNotFound;
  JsonbIterator *it;
  int32 r;
  JsonbValue v;

  check_stack_depth();

  if (level > last)
  {
    return res;
  }

  it = JsonbIteratorInit(jbc);

     
                                                   
     
  while ((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
  {
    if (r == WJB_KEY)
    {
      r = JsonbIteratorNext(&it, &v, true);
      Assert(r == WJB_VALUE);
    }

    if (r == WJB_VALUE || r == WJB_ELEM)
    {

      if (level >= first || (first == PG_UINT32_MAX && last == PG_UINT32_MAX && v.type != jbvBinary))                            
      {
                              
        if (jsp)
        {
          if (ignoreStructuralErrors)
          {
            bool savedIgnoreStructuralErrors;

            savedIgnoreStructuralErrors = cxt->ignoreStructuralErrors;
            cxt->ignoreStructuralErrors = true;
            res = executeItemOptUnwrapTarget(cxt, jsp, &v, found, unwrapNext);
            cxt->ignoreStructuralErrors = savedIgnoreStructuralErrors;
          }
          else
          {
            res = executeItemOptUnwrapTarget(cxt, jsp, &v, found, unwrapNext);
          }

          if (jperIsError(res))
          {
            break;
          }

          if (res == jperOk && !found)
          {
            break;
          }
        }
        else if (found)
        {
          JsonValueListAppend(found, copyJsonbValue(&v));
        }
        else
        {
          return jperOk;
        }
      }

      if (level < last && v.type == jbvBinary)
      {
        res = executeAnyItem(cxt, jsp, v.val.binary.data, found, level + 1, first, last, ignoreStructuralErrors, unwrapNext);

        if (jperIsError(res))
        {
          break;
        }

        if (res == jperOk && found == NULL)
        {
          break;
        }
      }
    }
  }

  return res;
}

   
                                      
   
                                                                        
                                                                              
                                                                               
                                                                              
                                                                           
                                                        
   
static JsonPathBool
executePredicate(JsonPathExecContext *cxt, JsonPathItem *pred, JsonPathItem *larg, JsonPathItem *rarg, JsonbValue *jb, bool unwrapRightArg, JsonPathPredicateCallback exec, void *param)
{
  JsonPathExecResult res;
  JsonValueListIterator lseqit;
  JsonValueList lseq = {0};
  JsonValueList rseq = {0};
  JsonbValue *lval;
  bool error = false;
  bool found = false;

                                               
  res = executeItemOptUnwrapResultNoThrow(cxt, larg, jb, true, &lseq);
  if (jperIsError(res))
  {
    return jpbUnknown;
  }

  if (rarg)
  {
                                                         
    res = executeItemOptUnwrapResultNoThrow(cxt, rarg, jb, unwrapRightArg, &rseq);
    if (jperIsError(res))
    {
      return jpbUnknown;
    }
  }

  JsonValueListInitIterator(&lseq, &lseqit);
  while ((lval = JsonValueListNext(&lseq, &lseqit)))
  {
    JsonValueListIterator rseqit;
    JsonbValue *rval;
    bool first = true;

    JsonValueListInitIterator(&rseq, &rseqit);
    if (rarg)
    {
      rval = JsonValueListNext(&rseq, &rseqit);
    }
    else
    {
      rval = NULL;
    }

                                                                  
    while (rarg ? (rval != NULL) : first)
    {
      JsonPathBool res = exec(pred, lval, rval, param);

      if (res == jpbUnknown)
      {
        if (jspStrictAbsenseOfErrors(cxt))
        {
          return jpbUnknown;
        }

        error = true;
      }
      else if (res == jpbTrue)
      {
        if (!jspStrictAbsenseOfErrors(cxt))
        {
          return jpbTrue;
        }

        found = true;
      }

      first = false;
      if (rarg)
      {
        rval = JsonValueListNext(&rseq, &rseqit);
      }
    }
  }

  if (found)                                   
  {
    return jpbTrue;
  }

  if (error)                                
  {
    return jpbUnknown;
  }

  return jpbFalse;
}

   
                                                                       
                                                           
   
static JsonPathExecResult
executeBinaryArithmExpr(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, BinaryArithmFunc func, JsonValueList *found)
{
  JsonPathExecResult jper;
  JsonPathItem elem;
  JsonValueList lseq = {0};
  JsonValueList rseq = {0};
  JsonbValue *lval;
  JsonbValue *rval;
  Numeric res;

  jspGetLeftArg(jsp, &elem);

     
                                                                      
                                                                          
     
  jper = executeItemOptUnwrapResult(cxt, &elem, jb, true, &lseq);
  if (jperIsError(jper))
  {
    return jper;
  }

  jspGetRightArg(jsp, &elem);

  jper = executeItemOptUnwrapResult(cxt, &elem, jb, true, &rseq);
  if (jperIsError(jper))
  {
    return jper;
  }

  if (JsonValueListLength(&lseq) != 1 || !(lval = getScalar(JsonValueListHead(&lseq), jbvNumeric)))
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SINGLETON_SQL_JSON_ITEM_REQUIRED), errmsg("left operand of jsonpath operator %s is not a single numeric value", jspOperationName(jsp->type)))));
  }

  if (JsonValueListLength(&rseq) != 1 || !(rval = getScalar(JsonValueListHead(&rseq), jbvNumeric)))
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SINGLETON_SQL_JSON_ITEM_REQUIRED), errmsg("right operand of jsonpath operator %s is not a single numeric value", jspOperationName(jsp->type)))));
  }

  if (jspThrowErrors(cxt))
  {
    res = func(lval->val.numeric, rval->val.numeric, NULL);
  }
  else
  {
    bool error = false;

    res = func(lval->val.numeric, rval->val.numeric, &error);

    if (error)
    {
      return jperError;
    }
  }

  if (!jspGetNext(jsp, &elem) && !found)
  {
    return jperOk;
  }

  lval = palloc(sizeof(*lval));
  lval->type = jbvNumeric;
  lval->val.numeric = res;

  return executeNextItem(cxt, jsp, &elem, lval, found, false);
}

   
                                                                              
                                                                    
   
static JsonPathExecResult
executeUnaryArithmExpr(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, PGFunction func, JsonValueList *found)
{
  JsonPathExecResult jper;
  JsonPathExecResult jper2;
  JsonPathItem elem;
  JsonValueList seq = {0};
  JsonValueListIterator it;
  JsonbValue *val;
  bool hasNext;

  jspGetArg(jsp, &elem);
  jper = executeItemOptUnwrapResult(cxt, &elem, jb, true, &seq);

  if (jperIsError(jper))
  {
    return jper;
  }

  jper = jperNotFound;

  hasNext = jspGetNext(jsp, &elem);

  JsonValueListInitIterator(&seq, &it);
  while ((val = JsonValueListNext(&seq, &it)))
  {
    if ((val = getScalar(val, jbvNumeric)))
    {
      if (!found && !hasNext)
      {
        return jperOk;
      }
    }
    else
    {
      if (!found && !hasNext)
      {
        continue;                                   
      }

      RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_NUMBER_NOT_FOUND), errmsg("operand of unary jsonpath operator %s is not a numeric value", jspOperationName(jsp->type)))));
    }

    if (func)
    {
      val->val.numeric = DatumGetNumeric(DirectFunctionCall1(func, NumericGetDatum(val->val.numeric)));
    }

    jper2 = executeNextItem(cxt, jsp, &elem, val, found, false);

    if (jperIsError(jper2))
    {
      return jper2;
    }

    if (jper2 == jperOk)
    {
      if (!found)
      {
        return jperOk;
      }
      jper = jperOk;
    }
  }

  return jper;
}

   
                                   
   
                                                             
   
static JsonPathBool
executeStartsWith(JsonPathItem *jsp, JsonbValue *whole, JsonbValue *initial, void *param)
{
  if (!(whole = getScalar(whole, jbvString)))
  {
    return jpbUnknown;            
  }

  if (!(initial = getScalar(initial, jbvString)))
  {
    return jpbUnknown;            
  }

  if (whole->val.string.len >= initial->val.string.len && !memcmp(whole->val.string.val, initial->val.string.val, initial->val.string.len))
  {
    return jpbTrue;
  }

  return jpbFalse;
}

   
                                  
   
                                              
   
static JsonPathBool
executeLikeRegex(JsonPathItem *jsp, JsonbValue *str, JsonbValue *rarg, void *param)
{
  JsonLikeRegexContext *cxt = param;

  if (!(str = getScalar(str, jbvString)))
  {
    return jpbUnknown;
  }

                                             
  if (!cxt->regex)
  {
    cxt->regex = cstring_to_text_with_len(jsp->content.like_regex.pattern, jsp->content.like_regex.patternlen);
    cxt->cflags = jspConvertRegexFlags(jsp->content.like_regex.flags);
  }

  if (RE_compile_and_execute(cxt->regex, str->val.string.val, str->val.string.len, cxt->cflags, DEFAULT_COLLATION_OID, 0, NULL))
  {
    return jpbTrue;
  }

  return jpbFalse;
}

   
                                                                                
                         
   
static JsonPathExecResult
executeNumericItemMethod(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, PGFunction func, JsonValueList *found)
{
  JsonPathItem next;
  Datum datum;

  if (unwrap && JsonbType(jb) == jbvArray)
  {
    return executeItemUnwrapTargetArray(cxt, jsp, jb, found, false);
  }

  if (!(jb = getScalar(jb, jbvNumeric)))
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM), errmsg("jsonpath item method .%s() can only be applied to a numeric value", jspOperationName(jsp->type)))));
  }

  datum = DirectFunctionCall1(func, NumericGetDatum(jb->val.numeric));

  if (!jspGetNext(jsp, &next) && !found)
  {
    return jperOk;
  }

  jb = palloc(sizeof(*jb));
  jb->type = jbvNumeric;
  jb->val.numeric = DatumGetNumeric(datum);

  return executeNextItem(cxt, jsp, &next, jb, found, false);
}

   
                                         
   
                                                                            
                                                                 
   
                                                                               
                                                                
                                                                 
   
                                                                            
                                                                              
                               
   
                                                                              
                                                                             
                                                                           
                                                      
   
                                                                             
                                                                        
                                                                                
   
static JsonPathExecResult
executeKeyValueMethod(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found)
{
  JsonPathExecResult res = jperNotFound;
  JsonPathItem next;
  JsonbContainer *jbc;
  JsonbValue key;
  JsonbValue val;
  JsonbValue idval;
  JsonbValue keystr;
  JsonbValue valstr;
  JsonbValue idstr;
  JsonbIterator *it;
  JsonbIteratorToken tok;
  int64 id;
  bool hasNext;

  if (JsonbType(jb) != jbvObject || jb->type != jbvBinary)
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_SQL_JSON_OBJECT_NOT_FOUND), errmsg("jsonpath item method .%s() can only be applied to an object", jspOperationName(jsp->type)))));
  }

  jbc = jb->val.binary.data;

  if (!JsonContainerSize(jbc))
  {
    return jperNotFound;                         
  }

  hasNext = jspGetNext(jsp, &next);

  keystr.type = jbvString;
  keystr.val.string.val = "key";
  keystr.val.string.len = 3;

  valstr.type = jbvString;
  valstr.val.string.val = "value";
  valstr.val.string.len = 5;

  idstr.type = jbvString;
  idstr.val.string.val = "id";
  idstr.val.string.len = 2;

                                                                       
  id = jb->type != jbvBinary ? 0 : (int64)((char *)jbc - (char *)cxt->baseObject.jbc);
  id += (int64)cxt->baseObject.id * INT64CONST(10000000000);

  idval.type = jbvNumeric;
  idval.val.numeric = DatumGetNumeric(DirectFunctionCall1(int8_numeric, Int64GetDatum(id)));

  it = JsonbIteratorInit(jbc);

  while ((tok = JsonbIteratorNext(&it, &key, true)) != WJB_DONE)
  {
    JsonBaseObjectInfo baseObject;
    JsonbValue obj;
    JsonbParseState *ps;
    JsonbValue *keyval;
    Jsonb *jsonb;

    if (tok != WJB_KEY)
    {
      continue;
    }

    res = jperOk;

    if (!hasNext && !found)
    {
      break;
    }

    tok = JsonbIteratorNext(&it, &val, true);
    Assert(tok == WJB_VALUE);

    ps = NULL;
    pushJsonbValue(&ps, WJB_BEGIN_OBJECT, NULL);

    pushJsonbValue(&ps, WJB_KEY, &keystr);
    pushJsonbValue(&ps, WJB_VALUE, &key);

    pushJsonbValue(&ps, WJB_KEY, &valstr);
    pushJsonbValue(&ps, WJB_VALUE, &val);

    pushJsonbValue(&ps, WJB_KEY, &idstr);
    pushJsonbValue(&ps, WJB_VALUE, &idval);

    keyval = pushJsonbValue(&ps, WJB_END_OBJECT, NULL);

    jsonb = JsonbValueToJsonb(keyval);

    JsonbInitBinary(&obj, jsonb);

    baseObject = setBaseObject(cxt, &obj, cxt->lastGeneratedObjectId++);

    res = executeNextItem(cxt, jsp, &next, &obj, found, true);

    cxt->baseObject = baseObject;

    if (jperIsError(res))
    {
      return res;
    }

    if (res == jperOk && !found)
    {
      break;
    }
  }

  return res;
}

   
                                                                             
                  
   
static JsonPathExecResult
appendBoolResult(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonValueList *found, JsonPathBool res)
{
  JsonPathItem next;
  JsonbValue jbv;

  if (!jspGetNext(jsp, &next) && !found)
  {
    return jperOk;                                    
  }

  if (res == jpbUnknown)
  {
    jbv.type = jbvNull;
  }
  else
  {
    jbv.type = jbvBool;
    jbv.val.boolean = res == jpbTrue;
  }

  return executeNextItem(cxt, jsp, &next, &jbv, found, true);
}

   
                                                                     
   
                                                                     
   
static void
getJsonPathItem(JsonPathExecContext *cxt, JsonPathItem *item, JsonbValue *value)
{
  switch (item->type)
  {
  case jpiNull:
    value->type = jbvNull;
    break;
  case jpiBool:
    value->type = jbvBool;
    value->val.boolean = jspGetBool(item);
    break;
  case jpiNumeric:
    value->type = jbvNumeric;
    value->val.numeric = jspGetNumeric(item);
    break;
  case jpiString:
    value->type = jbvString;
    value->val.string.val = jspGetString(item, &value->val.string.len);
    break;
  case jpiVariable:
    getJsonPathVariable(cxt, item, cxt->vars, value);
    return;
  default:
    elog(ERROR, "unexpected jsonpath item type");
  }
}

   
                                                         
   
static void
getJsonPathVariable(JsonPathExecContext *cxt, JsonPathItem *variable, Jsonb *vars, JsonbValue *value)
{
  char *varName;
  int varNameLength;
  JsonbValue tmp;
  JsonbValue *v;

  if (!vars)
  {
    value->type = jbvNull;
    return;
  }

  Assert(variable->type == jpiVariable);
  varName = jspGetString(variable, &varNameLength);
  tmp.type = jbvString;
  tmp.val.string.val = varName;
  tmp.val.string.len = varNameLength;

  v = findJsonbValueFromContainer(&vars->root, JB_FOBJECT, &tmp);

  if (v)
  {
    *value = *v;
    pfree(v);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find jsonpath variable \"%s\"", pnstrdup(varName, varNameLength))));
  }

  JsonbInitBinary(&tmp, vars);
  setBaseObject(cxt, &tmp, 1);
}

                                                                             

   
                                                                     
   
static int
JsonbArraySize(JsonbValue *jb)
{
  Assert(jb->type != jbvArray);

  if (jb->type == jbvBinary)
  {
    JsonbContainer *jbc = jb->val.binary.data;

    if (JsonContainerIsArray(jbc) && !JsonContainerIsScalar(jbc))
    {
      return JsonContainerSize(jbc);
    }
  }

  return -1;
}

                                    
static JsonPathBool
executeComparison(JsonPathItem *cmp, JsonbValue *lv, JsonbValue *rv, void *p)
{
  return compareItems(cmp->type, lv, rv);
}

   
                                               
   
static int
binaryCompareStrings(const char *s1, int len1, const char *s2, int len2)
{
  int cmp;

  cmp = memcmp(s1, s2, Min(len1, len2));

  if (cmp != 0)
  {
    return cmp;
  }

  if (len1 == len2)
  {
    return 0;
  }

  return len1 < len2 ? -1 : 1;
}

   
                                                                              
              
   
static int
compareStrings(const char *mbstr1, int mblen1, const char *mbstr2, int mblen2)
{
  if (GetDatabaseEncoding() == PG_SQL_ASCII || GetDatabaseEncoding() == PG_UTF8)
  {
       
                                                                           
                                                                  
                                            
       
    return binaryCompareStrings(mbstr1, mblen1, mbstr2, mblen2);
  }
  else
  {
    char *utf8str1, *utf8str2;
    int cmp, utf8len1, utf8len2;

       
                                                                        
                                                                           
                                                                    
                   
       
    utf8str1 = pg_server_to_any(mbstr1, mblen1, PG_UTF8);
    utf8str2 = pg_server_to_any(mbstr2, mblen2, PG_UTF8);
    utf8len1 = (mbstr1 == utf8str1) ? mblen1 : strlen(utf8str1);
    utf8len2 = (mbstr2 == utf8str2) ? mblen2 : strlen(utf8str2);

    cmp = binaryCompareStrings(utf8str1, utf8len1, utf8str2, utf8len2);

       
                                                                      
                                                        
       
    if (mbstr1 == utf8str1 && mbstr2 == utf8str2)
    {
      return cmp;
    }

                               
    if (mbstr1 != utf8str1)
    {
      pfree(utf8str1);
    }
    if (mbstr2 != utf8str2)
    {
      pfree(utf8str2);
    }

       
                                                                      
                                                                           
                                                                          
                                                                        
                                                                          
                                                                     
                                            
       
    if (cmp == 0)
    {
      return binaryCompareStrings(mbstr1, mblen1, mbstr2, mblen2);
    }
    else
    {
      return cmp;
    }
  }
}

   
                                                               
   
static JsonPathBool
compareItems(int32 op, JsonbValue *jb1, JsonbValue *jb2)
{
  int cmp;
  bool res;

  if (jb1->type != jb2->type)
  {
    if (jb1->type == jbvNull || jb2->type == jbvNull)
    {

         
                                                                     
                                                               
         
      return op == jpiNotEqual ? jpbTrue : jpbFalse;
    }

                                                               
    return jpbUnknown;
  }

  switch (jb1->type)
  {
  case jbvNull:
    cmp = 0;
    break;
  case jbvBool:
    cmp = jb1->val.boolean == jb2->val.boolean ? 0 : jb1->val.boolean ? 1 : -1;
    break;
  case jbvNumeric:
    cmp = compareNumeric(jb1->val.numeric, jb2->val.numeric);
    break;
  case jbvString:
    if (op == jpiEqual)
    {
      return jb1->val.string.len != jb2->val.string.len || memcmp(jb1->val.string.val, jb2->val.string.val, jb1->val.string.len) ? jpbFalse : jpbTrue;
    }

    cmp = compareStrings(jb1->val.string.val, jb1->val.string.len, jb2->val.string.val, jb2->val.string.len);
    break;

  case jbvBinary:
  case jbvArray:
  case jbvObject:
    return jpbUnknown;                                     

  default:
    elog(ERROR, "invalid jsonb value type %d", jb1->type);
  }

  switch (op)
  {
  case jpiEqual:
    res = (cmp == 0);
    break;
  case jpiNotEqual:
    res = (cmp != 0);
    break;
  case jpiLess:
    res = (cmp < 0);
    break;
  case jpiGreater:
    res = (cmp > 0);
    break;
  case jpiLessOrEqual:
    res = (cmp <= 0);
    break;
  case jpiGreaterOrEqual:
    res = (cmp >= 0);
    break;
  default:
    elog(ERROR, "unrecognized jsonpath operation: %d", op);
    return jpbUnknown;
  }

  return res ? jpbTrue : jpbFalse;
}

                          
static int
compareNumeric(Numeric a, Numeric b)
{
  return DatumGetInt32(DirectFunctionCall2(numeric_cmp, NumericGetDatum(a), NumericGetDatum(b)));
}

static JsonbValue *
copyJsonbValue(JsonbValue *src)
{
  JsonbValue *dst = palloc(sizeof(*dst));

  *dst = *src;

  return dst;
}

   
                                                                            
                                     
   
static JsonPathExecResult
getArrayIndex(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, int32 *index)
{
  JsonbValue *jbv;
  JsonValueList found = {0};
  JsonPathExecResult res = executeItem(cxt, jsp, jb, &found);
  Datum numeric_index;
  bool have_error = false;

  if (jperIsError(res))
  {
    return res;
  }

  if (JsonValueListLength(&found) != 1 || !(jbv = getScalar(JsonValueListHead(&found), jbvNumeric)))
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_INVALID_SQL_JSON_SUBSCRIPT), errmsg("jsonpath array subscript is not a single numeric value"))));
  }

  numeric_index = DirectFunctionCall2(numeric_trunc, NumericGetDatum(jbv->val.numeric), Int32GetDatum(0));

  *index = numeric_int4_opt_error(DatumGetNumeric(numeric_index), &have_error);

  if (have_error)
  {
    RETURN_ERROR(ereport(ERROR, (errcode(ERRCODE_INVALID_SQL_JSON_SUBSCRIPT), errmsg("jsonpath array subscript is out of integer range"))));
  }

  return jperOk;
}

                                                                          
static JsonBaseObjectInfo
setBaseObject(JsonPathExecContext *cxt, JsonbValue *jbv, int32 id)
{
  JsonBaseObjectInfo baseObject = cxt->baseObject;

  cxt->baseObject.jbc = jbv->type != jbvBinary ? NULL : (JsonbContainer *)jbv->val.binary.data;
  cxt->baseObject.id = id;

  return baseObject;
}

static void
JsonValueListAppend(JsonValueList *jvl, JsonbValue *jbv)
{
  if (jvl->singleton)
  {
    jvl->list = list_make2(jvl->singleton, jbv);
    jvl->singleton = NULL;
  }
  else if (!jvl->list)
  {
    jvl->singleton = jbv;
  }
  else
  {
    jvl->list = lappend(jvl->list, jbv);
  }
}

static int
JsonValueListLength(const JsonValueList *jvl)
{
  return jvl->singleton ? 1 : list_length(jvl->list);
}

static bool
JsonValueListIsEmpty(JsonValueList *jvl)
{
  return !jvl->singleton && list_length(jvl->list) <= 0;
}

static JsonbValue *
JsonValueListHead(JsonValueList *jvl)
{
  return jvl->singleton ? jvl->singleton : linitial(jvl->list);
}

static List *
JsonValueListGetList(JsonValueList *jvl)
{
  if (jvl->singleton)
  {
    return list_make1(jvl->singleton);
  }

  return jvl->list;
}

static void
JsonValueListInitIterator(const JsonValueList *jvl, JsonValueListIterator *it)
{
  if (jvl->singleton)
  {
    it->value = jvl->singleton;
    it->next = NULL;
  }
  else if (list_head(jvl->list) != NULL)
  {
    it->value = (JsonbValue *)linitial(jvl->list);
    it->next = lnext(list_head(jvl->list));
  }
  else
  {
    it->value = NULL;
    it->next = NULL;
  }
}

   
                                                           
   
static JsonbValue *
JsonValueListNext(const JsonValueList *jvl, JsonValueListIterator *it)
{
  JsonbValue *result = it->value;

  if (it->next)
  {
    it->value = lfirst(it->next);
    it->next = lnext(it->next);
  }
  else
  {
    it->value = NULL;
  }

  return result;
}

   
                                                                  
   
static JsonbValue *
JsonbInitBinary(JsonbValue *jbv, Jsonb *jb)
{
  jbv->type = jbvBinary;
  jbv->val.binary.data = &jb->root;
  jbv->val.binary.len = VARSIZE_ANY_EXHDR(jb);

  return jbv;
}

   
                                                                               
   
static int
JsonbType(JsonbValue *jb)
{
  int type = jb->type;

  if (jb->type == jbvBinary)
  {
    JsonbContainer *jbc = (void *)jb->val.binary.data;

                                                                       
    Assert(!JsonContainerIsScalar(jbc));

    if (JsonContainerIsObject(jbc))
    {
      type = jbvObject;
    }
    else if (JsonContainerIsArray(jbc))
    {
      type = jbvArray;
    }
    else
    {
      elog(ERROR, "invalid jsonb container type: 0x%08x", jbc->header);
    }
  }

  return type;
}

                                                       
static JsonbValue *
getScalar(JsonbValue *scalar, enum jbvType type)
{
                                                                     
  Assert(scalar->type != jbvBinary || !JsonContainerIsScalar(scalar->val.binary.data));

  return scalar->type == type ? scalar : NULL;
}

                                               
static JsonbValue *
wrapItemsInArray(const JsonValueList *items)
{
  JsonbParseState *ps = NULL;
  JsonValueListIterator it;
  JsonbValue *jbv;

  pushJsonbValue(&ps, WJB_BEGIN_ARRAY, NULL);

  JsonValueListInitIterator(items, &it);
  while ((jbv = JsonValueListNext(items, &it)))
  {
    pushJsonbValue(&ps, WJB_ELEM, jbv);
  }

  return pushJsonbValue(&ps, WJB_END_ARRAY, NULL);
}
