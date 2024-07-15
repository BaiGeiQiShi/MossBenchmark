                                                                            
   
               
                                          
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/json.h"
#include "utils/jsonapi.h"
#include "utils/jsonb.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

                                      
#define JB_PATH_CREATE 0x0001
#define JB_PATH_DELETE 0x0002
#define JB_PATH_REPLACE 0x0004
#define JB_PATH_INSERT_BEFORE 0x0008
#define JB_PATH_INSERT_AFTER 0x0010
#define JB_PATH_CREATE_OR_INSERT (JB_PATH_INSERT_BEFORE | JB_PATH_INSERT_AFTER | JB_PATH_CREATE)

                                
typedef struct OkeysState
{
  JsonLexContext *lex;
  char **result;
  int result_size;
  int result_count;
  int sent_count;
} OkeysState;

                                            
typedef struct IterateJsonStringValuesState
{
  JsonLexContext *lex;
  JsonIterateStringValuesAction action;                                   
                                                                
  void *action_state;                                                            
  uint32 flags;                                                                      
                                                        
} IterateJsonStringValuesState;

                                                     
typedef struct TransformJsonStringValuesState
{
  JsonLexContext *lex;
  StringInfo strval;                                          
  JsonTransformStringValuesAction action;                                   
                                                                  
  void *action_state;                                                                   
} TransformJsonStringValuesState;

                                   
typedef struct GetState
{
  JsonLexContext *lex;
  text *tresult;
  char *result_start;
  bool normalize_results;
  bool next_scalar;
  int npath;                                                   
  char **path_names;                                    
  int *path_indexes;                                      
  bool *pathok;                                                
  int *array_cur_index;                                       
                                   
} GetState;

                                 
typedef struct AlenState
{
  JsonLexContext *lex;
  int count;
} AlenState;

                         
typedef struct EachState
{
  JsonLexContext *lex;
  Tuplestorestate *tuple_store;
  TupleDesc ret_tdesc;
  MemoryContext tmp_cxt;
  char *result_start;
  bool normalize_results;
  bool next_scalar;
  char *normalized_scalar;
} EachState;

                                   
typedef struct ElementsState
{
  JsonLexContext *lex;
  const char *function_name;
  Tuplestorestate *tuple_store;
  TupleDesc ret_tdesc;
  MemoryContext tmp_cxt;
  char *result_start;
  bool normalize_results;
  bool next_scalar;
  char *normalized_scalar;
} ElementsState;

                                       
typedef struct JHashState
{
  JsonLexContext *lex;
  const char *function_name;
  HTAB *hash;
  char *saved_scalar;
  char *save_json_start;
  JsonTokenType saved_token_type;
} JHashState;

                       
typedef struct JsonHashEntry
{
  char fname[NAMEDATALEN];                               
  char *val;
  JsonTokenType type;
} JsonHashEntry;

                                                                       
typedef struct ScalarIOData
{
  Oid typioparam;
  FmgrInfo typiofunc;
} ScalarIOData;

                                               
typedef struct ColumnIOData ColumnIOData;
typedef struct RecordIOData RecordIOData;

                                                             
typedef struct ArrayIOData
{
  ColumnIOData *element_info;                     
  Oid element_type;                                      
  int32 element_typmod;                                        
} ArrayIOData;

                                                                 
typedef struct CompositeIOData
{
     
                                                                          
                                                                  
     
  RecordIOData *record_io;                                           
  TupleDesc tupdesc;                                    
                                                                           
  Oid base_typid;                      
  int32 base_typmod;                         
                                                                        
  void *domain_info;                                     
} CompositeIOData;

                                                              
typedef struct DomainIOData
{
  ColumnIOData *base_io;                     
  Oid base_typid;                          
  int32 base_typmod;                             
  void *domain_info;                                         
} DomainIOData;

                                 
typedef enum TypeCat
{
  TYPECAT_SCALAR = 's',
  TYPECAT_ARRAY = 'a',
  TYPECAT_COMPOSITE = 'c',
  TYPECAT_COMPOSITE_DOMAIN = 'C',
  TYPECAT_DOMAIN = 'd'
} TypeCat;

                                                                             

                                                                           
struct ColumnIOData
{
  Oid typid;                                  
  int32 typmod;                                     
  TypeCat typcat;                                   
  ScalarIOData scalar_io;                                          
                                                      
  union
  {
    ArrayIOData array;
    CompositeIOData composite;
    DomainIOData domain;
  } io;                                           
                        
};

                                                                     
struct RecordIOData
{
  Oid record_type;
  int32 record_typmod;
  int ncolumns;
  ColumnIOData columns[FLEXIBLE_ARRAY_MEMBER];
};

                                                                              
typedef struct PopulateRecordCache
{
  Oid argtype;                                                     
  ColumnIOData c;                                                     
  MemoryContext fn_mcxt;                           
} PopulateRecordCache;

                                           
typedef struct PopulateRecordsetState
{
  JsonLexContext *lex;
  const char *function_name;
  HTAB *json_hash;
  char *saved_scalar;
  char *save_json_start;
  JsonTokenType saved_token_type;
  Tuplestorestate *tuple_store;
  HeapTupleHeader rec;
  PopulateRecordCache *cache;
} PopulateRecordsetState;

                                                                          
typedef struct PopulateArrayContext
{
  ArrayBuildState *astate;                        
  ArrayIOData *aio;                            
  MemoryContext acxt;                                      
  MemoryContext mcxt;                                
  const char *colname;                               
  int *dims;                               
  int *sizes;                                              
  int ndims;                                         
} PopulateArrayContext;

                                     
typedef struct PopulateArrayState
{
  JsonLexContext *lex;                        
  PopulateArrayContext *ctx;               
  char *element_start;                                                
  char *element_scalar;                                                 
                                          
  JsonTokenType element_type;                                 
} PopulateArrayState;

                                
typedef struct StripnullState
{
  JsonLexContext *lex;
  StringInfo strval;
  bool skip_next_null;
} StripnullState;

                                                        
typedef struct JsValue
{
  bool is_json;                 
  union
  {
    struct
    {
      char *str;                           
      int len;                                                             
      JsonTokenType type;                
    } json;                               

    JsonbValue *jsonb;                  
  } val;
} JsValue;

typedef struct JsObject
{
  bool is_json;                 
  union
  {
    HTAB *json_hash;
    JsonbContainer *jsonb_cont;
  } val;
} JsObject;

                                                  
#define JsValueIsNull(jsv) ((jsv)->is_json ? (!(jsv)->val.json.str || (jsv)->val.json.type == JSON_TOKEN_NULL) : (!(jsv)->val.jsonb || (jsv)->val.jsonb->type == jbvNull))

#define JsValueIsString(jsv) ((jsv)->is_json ? (jsv)->val.json.type == JSON_TOKEN_STRING : ((jsv)->val.jsonb && (jsv)->val.jsonb->type == jbvString))

#define JsObjectIsEmpty(jso) ((jso)->is_json ? hash_get_num_entries((jso)->val.json_hash) == 0 : ((jso)->val.jsonb_cont == NULL || JsonContainerSize((jso)->val.jsonb_cont) == 0))

#define JsObjectFree(jso)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((jso)->is_json)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      hash_destroy((jso)->val.json_hash);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  } while (0)

                                                    
static void
okeys_object_field_start(void *state, char *fname, bool isnull);
static void
okeys_array_start(void *state);
static void
okeys_scalar(void *state, char *token, JsonTokenType tokentype);

                                                       
static void
get_object_start(void *state);
static void
get_object_end(void *state);
static void
get_object_field_start(void *state, char *fname, bool isnull);
static void
get_object_field_end(void *state, char *fname, bool isnull);
static void
get_array_start(void *state);
static void
get_array_end(void *state);
static void
get_array_element_start(void *state, bool isnull);
static void
get_array_element_end(void *state, bool isnull);
static void
get_scalar(void *state, char *token, JsonTokenType tokentype);

                                                      
static Datum
get_path_all(FunctionCallInfo fcinfo, bool as_text);
static text *
get_worker(text *json, char **tpath, int *ipath, int npath, bool normalize_results);
static Datum
get_jsonb_path_all(FunctionCallInfo fcinfo, bool as_text);

                                                     
static void
alen_object_start(void *state);
static void
alen_scalar(void *state, char *token, JsonTokenType tokentype);
static void
alen_array_element_start(void *state, bool isnull);

                                                
static Datum
each_worker(FunctionCallInfo fcinfo, bool as_text);
static Datum
each_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text);

                                             
static void
each_object_field_start(void *state, char *fname, bool isnull);
static void
each_object_field_end(void *state, char *fname, bool isnull);
static void
each_array_start(void *state);
static void
each_scalar(void *state, char *token, JsonTokenType tokentype);

                                                           
static Datum
elements_worker(FunctionCallInfo fcinfo, const char *funcname, bool as_text);
static Datum
elements_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text);

                                                       
static void
elements_object_start(void *state);
static void
elements_array_element_start(void *state, bool isnull);
static void
elements_array_element_end(void *state, bool isnull);
static void
elements_scalar(void *state, char *token, JsonTokenType tokentype);

                                          
static HTAB *
get_json_object_as_hash(char *json, int len, const char *funcname);

                                              
static void
populate_array_object_start(void *_state);
static void
populate_array_array_end(void *_state);
static void
populate_array_element_start(void *_state, bool isnull);
static void
populate_array_element_end(void *_state, bool isnull);
static void
populate_array_scalar(void *_state, char *token, JsonTokenType tokentype);

                                                           
static void
hash_object_field_start(void *state, char *fname, bool isnull);
static void
hash_object_field_end(void *state, char *fname, bool isnull);
static void
hash_array_start(void *state);
static void
hash_scalar(void *state, char *token, JsonTokenType tokentype);

                                                      
static void
populate_recordset_object_field_start(void *state, char *fname, bool isnull);
static void
populate_recordset_object_field_end(void *state, char *fname, bool isnull);
static void
populate_recordset_scalar(void *state, char *token, JsonTokenType tokentype);
static void
populate_recordset_object_start(void *state);
static void
populate_recordset_object_end(void *state);
static void
populate_recordset_array_start(void *state);
static void
populate_recordset_array_element_start(void *state, bool isnull);

                                                    
static void
sn_object_start(void *state);
static void
sn_object_end(void *state);
static void
sn_array_start(void *state);
static void
sn_array_end(void *state);
static void
sn_object_field_start(void *state, char *fname, bool isnull);
static void
sn_array_element_start(void *state, bool isnull);
static void
sn_scalar(void *state, char *token, JsonTokenType tokentype);

                                                                                          
static Datum
populate_recordset_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg);
static Datum
populate_record_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg);

                                               
static HeapTupleHeader
populate_record(TupleDesc tupdesc, RecordIOData **record_p, HeapTupleHeader defaultval, MemoryContext mcxt, JsObject *obj);
static void
get_record_type_from_argument(FunctionCallInfo fcinfo, const char *funcname, PopulateRecordCache *cache);
static void
get_record_type_from_query(FunctionCallInfo fcinfo, const char *funcname, PopulateRecordCache *cache);
static void
JsValueToJsObject(JsValue *jsv, JsObject *jso);
static Datum
populate_composite(CompositeIOData *io, Oid typid, const char *colname, MemoryContext mcxt, HeapTupleHeader defaultval, JsValue *jsv, bool isnull);
static Datum
populate_scalar(ScalarIOData *io, Oid typid, int32 typmod, JsValue *jsv);
static void
prepare_column_cache(ColumnIOData *column, Oid typid, int32 typmod, MemoryContext mcxt, bool need_scalar);
static Datum
populate_record_field(ColumnIOData *col, Oid typid, int32 typmod, const char *colname, MemoryContext mcxt, Datum defaultval, JsValue *jsv, bool *isnull);
static RecordIOData *
allocate_record_info(MemoryContext mcxt, int ncolumns);
static bool
JsObjectGetField(JsObject *obj, char *field, JsValue *jsv);
static void
populate_recordset_record(PopulateRecordsetState *state, JsObject *obj);
static void
populate_array_json(PopulateArrayContext *ctx, char *json, int len);
static void
populate_array_dim_jsonb(PopulateArrayContext *ctx, JsonbValue *jbv, int ndim);
static void
populate_array_report_expected_array(PopulateArrayContext *ctx, int ndim);
static void
populate_array_assign_ndims(PopulateArrayContext *ctx, int ndims);
static void
populate_array_check_dimension(PopulateArrayContext *ctx, int ndim);
static void
populate_array_element(PopulateArrayContext *ctx, int ndim, JsValue *jsv);
static Datum
populate_array(ArrayIOData *aio, const char *colname, MemoryContext mcxt, JsValue *jsv);
static Datum
populate_domain(DomainIOData *io, Oid typid, const char *colname, MemoryContext mcxt, JsValue *jsv, bool isnull);

                                                   
static JsonbValue *
findJsonbValueFromContainerLen(JsonbContainer *container, uint32 flags, char *key, uint32 keylen);

                                                                   
static JsonbValue *
IteratorConcat(JsonbIterator **it1, JsonbIterator **it2, JsonbParseState **state);
static JsonbValue *
setPath(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, int op_type);
static void
setPathObject(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, uint32 npairs, int op_type);
static void
setPathArray(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, uint32 nelems, int op_type);
static void
addJsonbToParseState(JsonbParseState **jbps, Jsonb *jb);

                                             
static void
iterate_values_scalar(void *state, char *token, JsonTokenType tokentype);
static void
iterate_values_object_field_start(void *state, char *fname, bool isnull);

                                                       
static void
transform_string_values_object_start(void *state);
static void
transform_string_values_object_end(void *state);
static void
transform_string_values_array_start(void *state);
static void
transform_string_values_array_end(void *state);
static void
transform_string_values_object_field_start(void *state, char *fname, bool isnull);
static void
transform_string_values_array_element_start(void *state, bool isnull);
static void
transform_string_values_scalar(void *state, char *token, JsonTokenType tokentype);

   
                                 
   
                                                    
   
                                                              
                                                                 
                                                                      
                                                                     
                                                                        
                                                     
   
Datum
jsonb_object_keys(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  OkeysState *state;
  int i;

  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcontext;
    Jsonb *jb = PG_GETARG_JSONB_P(0);
    bool skipNested = false;
    JsonbIterator *it;
    JsonbValue v;
    JsonbIteratorToken r;

    if (JB_ROOT_IS_SCALAR(jb))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a scalar", "jsonb_object_keys")));
    }
    else if (JB_ROOT_IS_ARRAY(jb))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on an array", "jsonb_object_keys")));
    }

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    state = palloc(sizeof(OkeysState));

    state->result_size = JB_ROOT_COUNT(jb);
    state->result_count = 0;
    state->sent_count = 0;
    state->result = palloc(state->result_size * sizeof(char *));

    it = JsonbIteratorInit(&jb->root);

    while ((r = JsonbIteratorNext(&it, &v, skipNested)) != WJB_DONE)
    {
      skipNested = true;

      if (r == WJB_KEY)
      {
        char *cstr;

        cstr = palloc(v.val.string.len + 1 * sizeof(char));
        memcpy(cstr, v.val.string.val, v.val.string.len);
        cstr[v.val.string.len] = '\0';
        state->result[state->result_count++] = cstr;
      }
    }

    MemoryContextSwitchTo(oldcontext);
    funcctx->user_fctx = (void *)state;
  }

  funcctx = SRF_PERCALL_SETUP();
  state = (OkeysState *)funcctx->user_fctx;

  if (state->sent_count < state->result_count)
  {
    char *nxt = state->result[state->sent_count++];

    SRF_RETURN_NEXT(funcctx, CStringGetTextDatum(nxt));
  }

                                                   
  for (i = 0; i < state->result_count; i++)
  {
    pfree(state->result[i]);
  }
  pfree(state->result);
  pfree(state);

  SRF_RETURN_DONE(funcctx);
}

Datum
json_object_keys(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  OkeysState *state;
  int i;

  if (SRF_IS_FIRSTCALL())
  {
    text *json = PG_GETARG_TEXT_PP(0);
    JsonLexContext *lex = makeJsonLexContext(json, true);
    JsonSemAction *sem;
    MemoryContext oldcontext;

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    state = palloc(sizeof(OkeysState));
    sem = palloc0(sizeof(JsonSemAction));

    state->lex = lex;
    state->result_size = 256;
    state->result_count = 0;
    state->sent_count = 0;
    state->result = palloc(256 * sizeof(char *));

    sem->semstate = (void *)state;
    sem->array_start = okeys_array_start;
    sem->scalar = okeys_scalar;
    sem->object_field_start = okeys_object_field_start;
                                                           

    pg_parse_json(lex, sem);
                                       

    pfree(lex->strval->data);
    pfree(lex->strval);
    pfree(lex);
    pfree(sem);

    MemoryContextSwitchTo(oldcontext);
    funcctx->user_fctx = (void *)state;
  }

  funcctx = SRF_PERCALL_SETUP();
  state = (OkeysState *)funcctx->user_fctx;

  if (state->sent_count < state->result_count)
  {
    char *nxt = state->result[state->sent_count++];

    SRF_RETURN_NEXT(funcctx, CStringGetTextDatum(nxt));
  }

                                                   
  for (i = 0; i < state->result_count; i++)
  {
    pfree(state->result[i]);
  }
  pfree(state->result);
  pfree(state);

  SRF_RETURN_DONE(funcctx);
}

static void
okeys_object_field_start(void *state, char *fname, bool isnull)
{
  OkeysState *_state = (OkeysState *)state;

                                                     
  if (_state->lex->lex_level != 1)
  {
    return;
  }

                                         
  if (_state->result_count >= _state->result_size)
  {
    _state->result_size *= 2;
    _state->result = (char **)repalloc(_state->result, sizeof(char *) * _state->result_size);
  }

                                     
  _state->result[_state->result_count++] = pstrdup(fname);
}

static void
okeys_array_start(void *state)
{
  OkeysState *_state = (OkeysState *)state;

                                       
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on an array", "json_object_keys")));
  }
}

static void
okeys_scalar(void *state, char *token, JsonTokenType tokentype)
{
  OkeysState *_state = (OkeysState *)state;

                                       
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a scalar", "json_object_keys")));
  }
}

   
                                   
                                                   
                                                             
   

Datum
json_object_field(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_PP(0);
  text *fname = PG_GETARG_TEXT_PP(1);
  char *fnamestr = text_to_cstring(fname);
  text *result;

  result = get_worker(json, &fnamestr, NULL, 1, false);

  if (result != NULL)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

Datum
jsonb_object_field(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  text *key = PG_GETARG_TEXT_PP(1);
  JsonbValue *v;

  if (!JB_ROOT_IS_OBJECT(jb))
  {
    PG_RETURN_NULL();
  }

  v = findJsonbValueFromContainerLen(&jb->root, JB_FOBJECT, VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));

  if (v != NULL)
  {
    PG_RETURN_JSONB_P(JsonbValueToJsonb(v));
  }

  PG_RETURN_NULL();
}

Datum
json_object_field_text(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_PP(0);
  text *fname = PG_GETARG_TEXT_PP(1);
  char *fnamestr = text_to_cstring(fname);
  text *result;

  result = get_worker(json, &fnamestr, NULL, 1, true);

  if (result != NULL)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

Datum
jsonb_object_field_text(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  text *key = PG_GETARG_TEXT_PP(1);
  JsonbValue *v;

  if (!JB_ROOT_IS_OBJECT(jb))
  {
    PG_RETURN_NULL();
  }

  v = findJsonbValueFromContainerLen(&jb->root, JB_FOBJECT, VARDATA_ANY(key), VARSIZE_ANY_EXHDR(key));

  if (v != NULL)
  {
    text *result = NULL;

    switch (v->type)
    {
    case jbvNull:
      break;
    case jbvBool:
      result = cstring_to_text(v->val.boolean ? "true" : "false");
      break;
    case jbvString:
      result = cstring_to_text_with_len(v->val.string.val, v->val.string.len);
      break;
    case jbvNumeric:
      result = cstring_to_text(DatumGetCString(DirectFunctionCall1(numeric_out, PointerGetDatum(v->val.numeric))));
      break;
    case jbvBinary:
    {
      StringInfo jtext = makeStringInfo();

      (void)JsonbToCString(jtext, v->val.binary.data, -1);
      result = cstring_to_text_with_len(jtext->data, jtext->len);
    }
    break;
    default:
      elog(ERROR, "unrecognized jsonb type: %d", (int)v->type);
    }

    if (result)
    {
      PG_RETURN_TEXT_P(result);
    }
  }

  PG_RETURN_NULL();
}

Datum
json_array_element(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_PP(0);
  int element = PG_GETARG_INT32(1);
  text *result;

  result = get_worker(json, NULL, &element, 1, false);

  if (result != NULL)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

Datum
jsonb_array_element(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  int element = PG_GETARG_INT32(1);
  JsonbValue *v;

  if (!JB_ROOT_IS_ARRAY(jb))
  {
    PG_RETURN_NULL();
  }

                                 
  if (element < 0)
  {
    uint32 nelements = JB_ROOT_COUNT(jb);

    if (-element > nelements)
    {
      PG_RETURN_NULL();
    }
    else
    {
      element += nelements;
    }
  }

  v = getIthJsonbValueFromContainer(&jb->root, element);
  if (v != NULL)
  {
    PG_RETURN_JSONB_P(JsonbValueToJsonb(v));
  }

  PG_RETURN_NULL();
}

Datum
json_array_element_text(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_PP(0);
  int element = PG_GETARG_INT32(1);
  text *result;

  result = get_worker(json, NULL, &element, 1, true);

  if (result != NULL)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

Datum
jsonb_array_element_text(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  int element = PG_GETARG_INT32(1);
  JsonbValue *v;

  if (!JB_ROOT_IS_ARRAY(jb))
  {
    PG_RETURN_NULL();
  }

                                 
  if (element < 0)
  {
    uint32 nelements = JB_ROOT_COUNT(jb);

    if (-element > nelements)
    {
      PG_RETURN_NULL();
    }
    else
    {
      element += nelements;
    }
  }

  v = getIthJsonbValueFromContainer(&jb->root, element);
  if (v != NULL)
  {
    text *result = NULL;

    switch (v->type)
    {
    case jbvNull:
      break;
    case jbvBool:
      result = cstring_to_text(v->val.boolean ? "true" : "false");
      break;
    case jbvString:
      result = cstring_to_text_with_len(v->val.string.val, v->val.string.len);
      break;
    case jbvNumeric:
      result = cstring_to_text(DatumGetCString(DirectFunctionCall1(numeric_out, PointerGetDatum(v->val.numeric))));
      break;
    case jbvBinary:
    {
      StringInfo jtext = makeStringInfo();

      (void)JsonbToCString(jtext, v->val.binary.data, -1);
      result = cstring_to_text_with_len(jtext->data, jtext->len);
    }
    break;
    default:
      elog(ERROR, "unrecognized jsonb type: %d", (int)v->type);
    }

    if (result)
    {
      PG_RETURN_TEXT_P(result);
    }
  }

  PG_RETURN_NULL();
}

Datum
json_extract_path(PG_FUNCTION_ARGS)
{
  return get_path_all(fcinfo, false);
}

Datum
json_extract_path_text(PG_FUNCTION_ARGS)
{
  return get_path_all(fcinfo, true);
}

   
                                             
   
static Datum
get_path_all(FunctionCallInfo fcinfo, bool as_text)
{
  text *json = PG_GETARG_TEXT_PP(0);
  ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
  text *result;
  Datum *pathtext;
  bool *pathnulls;
  int npath;
  char **tpath;
  int *ipath;
  int i;

     
                                                                          
                                                                        
                                                                        
                                                                     
                                                           
     
  if (array_contains_nulls(path))
  {
    PG_RETURN_NULL();
  }

  deconstruct_array(path, TEXTOID, -1, false, 'i', &pathtext, &pathnulls, &npath);

  tpath = palloc(npath * sizeof(char *));
  ipath = palloc(npath * sizeof(int));

  for (i = 0; i < npath; i++)
  {
    Assert(!pathnulls[i]);
    tpath[i] = TextDatumGetCString(pathtext[i]);

       
                                                                       
                                                                           
                                                                 
       
    if (*tpath[i] != '\0')
    {
      long ind;
      char *endptr;

      errno = 0;
      ind = strtol(tpath[i], &endptr, 10);
      if (*endptr == '\0' && errno == 0 && ind <= INT_MAX && ind >= INT_MIN)
      {
        ipath[i] = (int)ind;
      }
      else
      {
        ipath[i] = INT_MIN;
      }
    }
    else
    {
      ipath[i] = INT_MIN;
    }
  }

  result = get_worker(json, tpath, ipath, npath, as_text);

  if (result != NULL)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
              
   
                                                   
   
                                    
                                     
                                                                       
                                           
                                                                
   
                                                                         
                                                                           
                                                                           
                                                                         
                                                                            
   
static text *
get_worker(text *json, char **tpath, int *ipath, int npath, bool normalize_results)
{
  JsonLexContext *lex = makeJsonLexContext(json, true);
  JsonSemAction *sem = palloc0(sizeof(JsonSemAction));
  GetState *state = palloc0(sizeof(GetState));

  Assert(npath >= 0);

  state->lex = lex;
                                 
  state->normalize_results = normalize_results;
  state->npath = npath;
  state->path_names = tpath;
  state->path_indexes = ipath;
  state->pathok = palloc0(sizeof(bool) * npath);
  state->array_cur_index = palloc(sizeof(int) * npath);

  if (npath > 0)
  {
    state->pathok[0] = true;
  }

  sem->semstate = (void *)state;

     
                                                                             
                                                 
     
  sem->scalar = get_scalar;
  if (npath == 0)
  {
    sem->object_start = get_object_start;
    sem->object_end = get_object_end;
    sem->array_start = get_array_start;
    sem->array_end = get_array_end;
  }
  if (tpath != NULL)
  {
    sem->object_field_start = get_object_field_start;
    sem->object_field_end = get_object_field_end;
  }
  if (ipath != NULL)
  {
    sem->array_start = get_array_start;
    sem->array_element_start = get_array_element_start;
    sem->array_element_end = get_array_element_end;
  }

  pg_parse_json(lex, sem);

  return state->tresult;
}

static void
get_object_start(void *state)
{
  GetState *_state = (GetState *)state;
  int lex_level = _state->lex->lex_level;

  if (lex_level == 0 && _state->npath == 0)
  {
       
                                                                           
                                                                       
                                                                  
       
    _state->result_start = _state->lex->token_start;
  }
}

static void
get_object_end(void *state)
{
  GetState *_state = (GetState *)state;
  int lex_level = _state->lex->lex_level;

  if (lex_level == 0 && _state->npath == 0)
  {
                                                
    char *start = _state->result_start;
    int len = _state->lex->prev_token_terminator - start;

    _state->tresult = cstring_to_text_with_len(start, len);
  }
}

static void
get_object_field_start(void *state, char *fname, bool isnull)
{
  GetState *_state = (GetState *)state;
  bool get_next = false;
  int lex_level = _state->lex->lex_level;

  if (lex_level <= _state->npath && _state->pathok[lex_level - 1] && _state->path_names != NULL && _state->path_names[lex_level - 1] != NULL && strcmp(fname, _state->path_names[lex_level - 1]) == 0)
  {
    if (lex_level < _state->npath)
    {
                                                   
      _state->pathok[lex_level] = true;
    }
    else
    {
                                              
      get_next = true;
    }
  }

  if (get_next)
  {
                                                            
    _state->tresult = NULL;
    _state->result_start = NULL;

    if (_state->normalize_results && _state->lex->token_type == JSON_TOKEN_STRING)
    {
                                                                  
      _state->next_scalar = true;
    }
    else
    {
                                                                       
      _state->result_start = _state->lex->token_start;
    }
  }
}

static void
get_object_field_end(void *state, char *fname, bool isnull)
{
  GetState *_state = (GetState *)state;
  bool get_last = false;
  int lex_level = _state->lex->lex_level;

                                               
  if (lex_level <= _state->npath && _state->pathok[lex_level - 1] && _state->path_names != NULL && _state->path_names[lex_level - 1] != NULL && strcmp(fname, _state->path_names[lex_level - 1]) == 0)
  {
    if (lex_level < _state->npath)
    {
                                                
      _state->pathok[lex_level] = false;
    }
    else
    {
                                              
      get_last = true;
    }
  }

                                                         
  if (get_last && _state->result_start != NULL)
  {
       
                                                                         
                                                                      
                                                                     
       
    if (isnull && _state->normalize_results)
    {
      _state->tresult = (text *)NULL;
    }
    else
    {
      char *start = _state->result_start;
      int len = _state->lex->prev_token_terminator - start;

      _state->tresult = cstring_to_text_with_len(start, len);
    }

                                                                     
    _state->result_start = NULL;
  }
}

static void
get_array_start(void *state)
{
  GetState *_state = (GetState *)state;
  int lex_level = _state->lex->lex_level;

  if (lex_level < _state->npath)
  {
                                                       
    _state->array_cur_index[lex_level] = -1;

                                                                  
    if (_state->path_indexes[lex_level] < 0 && _state->path_indexes[lex_level] != INT_MIN)
    {
                                                                    
      int nelements = json_count_array_elements(_state->lex);

      if (-_state->path_indexes[lex_level] <= nelements)
      {
        _state->path_indexes[lex_level] += nelements;
      }
    }
  }
  else if (lex_level == 0 && _state->npath == 0)
  {
       
                                                                          
                                                                           
                                                                  
       
    _state->result_start = _state->lex->token_start;
  }
}

static void
get_array_end(void *state)
{
  GetState *_state = (GetState *)state;
  int lex_level = _state->lex->lex_level;

  if (lex_level == 0 && _state->npath == 0)
  {
                                               
    char *start = _state->result_start;
    int len = _state->lex->prev_token_terminator - start;

    _state->tresult = cstring_to_text_with_len(start, len);
  }
}

static void
get_array_element_start(void *state, bool isnull)
{
  GetState *_state = (GetState *)state;
  bool get_next = false;
  int lex_level = _state->lex->lex_level;

                                    
  if (lex_level <= _state->npath)
  {
    _state->array_cur_index[lex_level - 1]++;
  }

  if (lex_level <= _state->npath && _state->pathok[lex_level - 1] && _state->path_indexes != NULL && _state->array_cur_index[lex_level - 1] == _state->path_indexes[lex_level - 1])
  {
    if (lex_level < _state->npath)
    {
                                                   
      _state->pathok[lex_level] = true;
    }
    else
    {
                                              
      get_next = true;
    }
  }

                                 
  if (get_next)
  {
    _state->tresult = NULL;
    _state->result_start = NULL;

    if (_state->normalize_results && _state->lex->token_type == JSON_TOKEN_STRING)
    {
      _state->next_scalar = true;
    }
    else
    {
      _state->result_start = _state->lex->token_start;
    }
  }
}

static void
get_array_element_end(void *state, bool isnull)
{
  GetState *_state = (GetState *)state;
  bool get_last = false;
  int lex_level = _state->lex->lex_level;

                                                
  if (lex_level <= _state->npath && _state->pathok[lex_level - 1] && _state->path_indexes != NULL && _state->array_cur_index[lex_level - 1] == _state->path_indexes[lex_level - 1])
  {
    if (lex_level < _state->npath)
    {
                                                  
      _state->pathok[lex_level] = false;
    }
    else
    {
                                              
      get_last = true;
    }
  }

                                 
  if (get_last && _state->result_start != NULL)
  {
    if (isnull && _state->normalize_results)
    {
      _state->tresult = (text *)NULL;
    }
    else
    {
      char *start = _state->result_start;
      int len = _state->lex->prev_token_terminator - start;

      _state->tresult = cstring_to_text_with_len(start, len);
    }

    _state->result_start = NULL;
  }
}

static void
get_scalar(void *state, char *token, JsonTokenType tokentype)
{
  GetState *_state = (GetState *)state;
  int lex_level = _state->lex->lex_level;

                                    
  if (lex_level == 0 && _state->npath == 0)
  {
    if (_state->normalize_results && tokentype == JSON_TOKEN_STRING)
    {
                                         
      _state->next_scalar = true;
    }
    else if (_state->normalize_results && tokentype == JSON_TOKEN_NULL)
    {
      _state->tresult = (text *)NULL;
    }
    else
    {
         
                                                                    
                                                                         
                                                     
         
      char *start = _state->lex->input;
      int len = _state->lex->prev_token_terminator - start;

      _state->tresult = cstring_to_text_with_len(start, len);
    }
  }

  if (_state->next_scalar)
  {
                                                         
    _state->tresult = cstring_to_text(token);
                                                                    
    _state->next_scalar = false;
  }
}

Datum
jsonb_extract_path(PG_FUNCTION_ARGS)
{
  return get_jsonb_path_all(fcinfo, false);
}

Datum
jsonb_extract_path_text(PG_FUNCTION_ARGS)
{
  return get_jsonb_path_all(fcinfo, true);
}

static Datum
get_jsonb_path_all(FunctionCallInfo fcinfo, bool as_text)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
  Jsonb *res;
  Datum *pathtext;
  bool *pathnulls;
  int npath;
  int i;
  bool have_object = false, have_array = false;
  JsonbValue *jbvp = NULL;
  JsonbValue tv;
  JsonbContainer *container;

     
                                                                          
                                                                        
                                                                        
                                                                     
                                                           
     
  if (array_contains_nulls(path))
  {
    PG_RETURN_NULL();
  }

  deconstruct_array(path, TEXTOID, -1, false, 'i', &pathtext, &pathnulls, &npath);

                                                                      
  container = &jb->root;

  if (JB_ROOT_IS_OBJECT(jb))
  {
    have_object = true;
  }
  else if (JB_ROOT_IS_ARRAY(jb) && !JB_ROOT_IS_SCALAR(jb))
  {
    have_array = true;
  }
  else
  {
    Assert(JB_ROOT_IS_ARRAY(jb) && JB_ROOT_IS_SCALAR(jb));
                                                              
    if (npath <= 0)
    {
      jbvp = getIthJsonbValueFromContainer(container, 0);
    }
  }

     
                                                                         
                                                                   
                                                                             
                                                                           
                                                                          
                                                     
     
  if (npath <= 0 && jbvp == NULL)
  {
    if (as_text)
    {
      PG_RETURN_TEXT_P(cstring_to_text(JsonbToCString(NULL, container, VARSIZE(jb))));
    }
    else
    {
                                                    
      PG_RETURN_JSONB_P(jb);
    }
  }

  for (i = 0; i < npath; i++)
  {
    if (have_object)
    {
      jbvp = findJsonbValueFromContainerLen(container, JB_FOBJECT, VARDATA(pathtext[i]), VARSIZE(pathtext[i]) - VARHDRSZ);
    }
    else if (have_array)
    {
      long lindex;
      uint32 index;
      char *indextext = TextDatumGetCString(pathtext[i]);
      char *endptr;

      errno = 0;
      lindex = strtol(indextext, &endptr, 10);
      if (endptr == indextext || *endptr != '\0' || errno != 0 || lindex > INT_MAX || lindex < INT_MIN)
      {
        PG_RETURN_NULL();
      }

      if (lindex >= 0)
      {
        index = (uint32)lindex;
      }
      else
      {
                                       
        uint32 nelements;

                                                    
        if (!JsonContainerIsArray(container))
        {
          elog(ERROR, "not a jsonb array");
        }

        nelements = JsonContainerSize(container);

        if (-lindex > nelements)
        {
          PG_RETURN_NULL();
        }
        else
        {
          index = nelements + lindex;
        }
      }

      jbvp = getIthJsonbValueFromContainer(container, index);
    }
    else
    {
                                            
      PG_RETURN_NULL();
    }

    if (jbvp == NULL)
    {
      PG_RETURN_NULL();
    }
    else if (i == npath - 1)
    {
      break;
    }

    if (jbvp->type == jbvBinary)
    {
      JsonbIterator *it = JsonbIteratorInit((JsonbContainer *)jbvp->val.binary.data);
      JsonbIteratorToken r;

      r = JsonbIteratorNext(&it, &tv, true);
      container = (JsonbContainer *)jbvp->val.binary.data;
      have_object = r == WJB_BEGIN_OBJECT;
      have_array = r == WJB_BEGIN_ARRAY;
    }
    else
    {
      have_object = jbvp->type == jbvObject;
      have_array = jbvp->type == jbvArray;
    }
  }

  if (as_text)
  {
                                                         
    if (jbvp->type == jbvString)
    {
      PG_RETURN_TEXT_P(cstring_to_text_with_len(jbvp->val.string.val, jbvp->val.string.len));
    }
    if (jbvp->type == jbvNull)
    {
      PG_RETURN_NULL();
    }
  }

  res = JsonbValueToJsonb(jbvp);

  if (as_text)
  {
    PG_RETURN_TEXT_P(cstring_to_text(JsonbToCString(NULL, &res->root, VARSIZE(res))));
  }
  else
  {
                                                  
    PG_RETURN_JSONB_P(res);
  }
}

   
                                               
   
Datum
json_array_length(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_PP(0);
  AlenState *state;
  JsonLexContext *lex;
  JsonSemAction *sem;

  lex = makeJsonLexContext(json, false);
  state = palloc0(sizeof(AlenState));
  sem = palloc0(sizeof(JsonSemAction));

                                
#if 0
	state->count = 0;
#endif
  state->lex = lex;

  sem->semstate = (void *)state;
  sem->object_start = alen_object_start;
  sem->scalar = alen_scalar;
  sem->array_element_start = alen_array_element_start;

  pg_parse_json(lex, sem);

  PG_RETURN_INT32(state->count);
}

Datum
jsonb_array_length(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);

  if (JB_ROOT_IS_SCALAR(jb))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot get array length of a scalar")));
  }
  else if (!JB_ROOT_IS_ARRAY(jb))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot get array length of a non-array")));
  }

  PG_RETURN_INT32(JB_ROOT_COUNT(jb));
}

   
                                                                             
                           
   

static void
alen_object_start(void *state)
{
  AlenState *_state = (AlenState *)state;

                            
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot get array length of a non-array")));
  }
}

static void
alen_scalar(void *state, char *token, JsonTokenType tokentype)
{
  AlenState *_state = (AlenState *)state;

                            
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot get array length of a scalar")));
  }
}

static void
alen_array_element_start(void *state, bool isnull)
{
  AlenState *_state = (AlenState *)state;

                                              
  if (_state->lex->lex_level == 1)
  {
    _state->count++;
  }
}

   
                                             
   
                                                 
   
                                                                     
                                                         
                                                                       
                                                  
   
Datum
json_each(PG_FUNCTION_ARGS)
{
  return each_worker(fcinfo, false);
}

Datum
jsonb_each(PG_FUNCTION_ARGS)
{
  return each_worker_jsonb(fcinfo, "jsonb_each", false);
}

Datum
json_each_text(PG_FUNCTION_ARGS)
{
  return each_worker(fcinfo, true);
}

Datum
jsonb_each_text(PG_FUNCTION_ARGS)
{
  return each_worker_jsonb(fcinfo, "jsonb_each_text", true);
}

static Datum
each_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  ReturnSetInfo *rsi;
  Tuplestorestate *tuple_store;
  TupleDesc tupdesc;
  TupleDesc ret_tdesc;
  MemoryContext old_cxt, tmp_cxt;
  bool skipNested = false;
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken r;

  if (!JB_ROOT_IS_OBJECT(jb))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a non-object", funcname)));
  }

  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0 || rsi->expectedDesc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that "
                                                                   "cannot accept a set")));
  }

  rsi->returnMode = SFRM_Materialize;

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function returning record called in context "
                                                                   "that cannot accept type record")));
  }

  old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);

  ret_tdesc = CreateTupleDescCopy(tupdesc);
  BlessTupleDesc(ret_tdesc);
  tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

  MemoryContextSwitchTo(old_cxt);

  tmp_cxt = AllocSetContextCreate(CurrentMemoryContext, "jsonb_each temporary cxt", ALLOCSET_DEFAULT_SIZES);

  it = JsonbIteratorInit(&jb->root);

  while ((r = JsonbIteratorNext(&it, &v, skipNested)) != WJB_DONE)
  {
    skipNested = true;

    if (r == WJB_KEY)
    {
      text *key;
      HeapTuple tuple;
      Datum values[2];
      bool nulls[2] = {false, false};

                                                                           
      old_cxt = MemoryContextSwitchTo(tmp_cxt);

      key = cstring_to_text_with_len(v.val.string.val, v.val.string.len);

         
                                                                     
                                  
         
      r = JsonbIteratorNext(&it, &v, skipNested);
      Assert(r != WJB_DONE);

      values[0] = PointerGetDatum(key);

      if (as_text)
      {
        if (v.type == jbvNull)
        {
                                                       
          nulls[1] = true;
          values[1] = (Datum)NULL;
        }
        else
        {
          text *sv;

          if (v.type == jbvString)
          {
                                                                 
            sv = cstring_to_text_with_len(v.val.string.val, v.val.string.len);
          }
          else
          {
                                                       
            StringInfo jtext = makeStringInfo();
            Jsonb *jb = JsonbValueToJsonb(&v);

            (void)JsonbToCString(jtext, &jb->root, 0);
            sv = cstring_to_text_with_len(jtext->data, jtext->len);
          }

          values[1] = PointerGetDatum(sv);
        }
      }
      else
      {
                                                     
        Jsonb *val = JsonbValueToJsonb(&v);

        values[1] = PointerGetDatum(val);
      }

      tuple = heap_form_tuple(ret_tdesc, values, nulls);

      tuplestore_puttuple(tuple_store, tuple);

                                    
      MemoryContextSwitchTo(old_cxt);
      MemoryContextReset(tmp_cxt);
    }
  }

  MemoryContextDelete(tmp_cxt);

  rsi->setResult = tuple_store;
  rsi->setDesc = ret_tdesc;

  PG_RETURN_NULL();
}

static Datum
each_worker(FunctionCallInfo fcinfo, bool as_text)
{
  text *json = PG_GETARG_TEXT_PP(0);
  JsonLexContext *lex;
  JsonSemAction *sem;
  ReturnSetInfo *rsi;
  MemoryContext old_cxt;
  TupleDesc tupdesc;
  EachState *state;

  lex = makeJsonLexContext(json, true);
  state = palloc0(sizeof(EachState));
  sem = palloc0(sizeof(JsonSemAction));

  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0 || rsi->expectedDesc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that "
                                                                   "cannot accept a set")));
  }

  rsi->returnMode = SFRM_Materialize;

  (void)get_call_result_type(fcinfo, NULL, &tupdesc);

                                                              
  old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);

  state->ret_tdesc = CreateTupleDescCopy(tupdesc);
  BlessTupleDesc(state->ret_tdesc);
  state->tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

  MemoryContextSwitchTo(old_cxt);

  sem->semstate = (void *)state;
  sem->array_start = each_array_start;
  sem->scalar = each_scalar;
  sem->object_field_start = each_object_field_start;
  sem->object_field_end = each_object_field_end;

  state->normalize_results = as_text;
  state->next_scalar = false;
  state->lex = lex;
  state->tmp_cxt = AllocSetContextCreate(CurrentMemoryContext, "json_each temporary cxt", ALLOCSET_DEFAULT_SIZES);

  pg_parse_json(lex, sem);

  MemoryContextDelete(state->tmp_cxt);

  rsi->setResult = state->tuple_store;
  rsi->setDesc = state->ret_tdesc;

  PG_RETURN_NULL();
}

static void
each_object_field_start(void *state, char *fname, bool isnull)
{
  EachState *_state = (EachState *)state;

                                                
  if (_state->lex->lex_level == 1)
  {
       
                                                                      
                                                                          
                                         
       
    if (_state->normalize_results && _state->lex->token_type == JSON_TOKEN_STRING)
    {
      _state->next_scalar = true;
    }
    else
    {
      _state->result_start = _state->lex->token_start;
    }
  }
}

static void
each_object_field_end(void *state, char *fname, bool isnull)
{
  EachState *_state = (EachState *)state;
  MemoryContext old_cxt;
  int len;
  text *val;
  HeapTuple tuple;
  Datum values[2];
  bool nulls[2] = {false, false};

                                
  if (_state->lex->lex_level != 1)
  {
    return;
  }

                                                                       
  old_cxt = MemoryContextSwitchTo(_state->tmp_cxt);

  values[0] = CStringGetTextDatum(fname);

  if (isnull && _state->normalize_results)
  {
    nulls[1] = true;
    values[1] = (Datum)0;
  }
  else if (_state->next_scalar)
  {
    values[1] = CStringGetTextDatum(_state->normalized_scalar);
    _state->next_scalar = false;
  }
  else
  {
    len = _state->lex->prev_token_terminator - _state->result_start;
    val = cstring_to_text_with_len(_state->result_start, len);
    values[1] = PointerGetDatum(val);
  }

  tuple = heap_form_tuple(_state->ret_tdesc, values, nulls);

  tuplestore_puttuple(_state->tuple_store, tuple);

                                
  MemoryContextSwitchTo(old_cxt);
  MemoryContextReset(_state->tmp_cxt);
}

static void
each_array_start(void *state)
{
  EachState *_state = (EachState *)state;

                            
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot deconstruct an array as an object")));
  }
}

static void
each_scalar(void *state, char *token, JsonTokenType tokentype)
{
  EachState *_state = (EachState *)state;

                            
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot deconstruct a scalar")));
  }

                                           
  if (_state->next_scalar)
  {
    _state->normalized_scalar = token;
  }
}

   
                                                                  
   
                                      
   
                                                                   
   

Datum
jsonb_array_elements(PG_FUNCTION_ARGS)
{
  return elements_worker_jsonb(fcinfo, "jsonb_array_elements", false);
}

Datum
jsonb_array_elements_text(PG_FUNCTION_ARGS)
{
  return elements_worker_jsonb(fcinfo, "jsonb_array_elements_text", true);
}

static Datum
elements_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  ReturnSetInfo *rsi;
  Tuplestorestate *tuple_store;
  TupleDesc tupdesc;
  TupleDesc ret_tdesc;
  MemoryContext old_cxt, tmp_cxt;
  bool skipNested = false;
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken r;

  if (JB_ROOT_IS_SCALAR(jb))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot extract elements from a scalar")));
  }
  else if (!JB_ROOT_IS_ARRAY(jb))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot extract elements from an object")));
  }

  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0 || rsi->expectedDesc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that "
                                                                   "cannot accept a set")));
  }

  rsi->returnMode = SFRM_Materialize;

                                                               
  tupdesc = rsi->expectedDesc;

  old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);

  ret_tdesc = CreateTupleDescCopy(tupdesc);
  BlessTupleDesc(ret_tdesc);
  tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

  MemoryContextSwitchTo(old_cxt);

  tmp_cxt = AllocSetContextCreate(CurrentMemoryContext, "jsonb_array_elements temporary cxt", ALLOCSET_DEFAULT_SIZES);

  it = JsonbIteratorInit(&jb->root);

  while ((r = JsonbIteratorNext(&it, &v, skipNested)) != WJB_DONE)
  {
    skipNested = true;

    if (r == WJB_ELEM)
    {
      HeapTuple tuple;
      Datum values[1];
      bool nulls[1] = {false};

                                                                           
      old_cxt = MemoryContextSwitchTo(tmp_cxt);

      if (!as_text)
      {
        Jsonb *val = JsonbValueToJsonb(&v);

        values[0] = PointerGetDatum(val);
      }
      else
      {
        if (v.type == jbvNull)
        {
                                                       
          nulls[0] = true;
          values[0] = (Datum)NULL;
        }
        else
        {
          text *sv;

          if (v.type == jbvString)
          {
                                                                
            sv = cstring_to_text_with_len(v.val.string.val, v.val.string.len);
          }
          else
          {
                                                       
            StringInfo jtext = makeStringInfo();
            Jsonb *jb = JsonbValueToJsonb(&v);

            (void)JsonbToCString(jtext, &jb->root, 0);
            sv = cstring_to_text_with_len(jtext->data, jtext->len);
          }

          values[0] = PointerGetDatum(sv);
        }
      }

      tuple = heap_form_tuple(ret_tdesc, values, nulls);

      tuplestore_puttuple(tuple_store, tuple);

                                    
      MemoryContextSwitchTo(old_cxt);
      MemoryContextReset(tmp_cxt);
    }
  }

  MemoryContextDelete(tmp_cxt);

  rsi->setResult = tuple_store;
  rsi->setDesc = ret_tdesc;

  PG_RETURN_NULL();
}

Datum
json_array_elements(PG_FUNCTION_ARGS)
{
  return elements_worker(fcinfo, "json_array_elements", false);
}

Datum
json_array_elements_text(PG_FUNCTION_ARGS)
{
  return elements_worker(fcinfo, "json_array_elements_text", true);
}

static Datum
elements_worker(FunctionCallInfo fcinfo, const char *funcname, bool as_text)
{
  text *json = PG_GETARG_TEXT_PP(0);

                                                        
  JsonLexContext *lex = makeJsonLexContext(json, as_text);
  JsonSemAction *sem;
  ReturnSetInfo *rsi;
  MemoryContext old_cxt;
  TupleDesc tupdesc;
  ElementsState *state;

  state = palloc0(sizeof(ElementsState));
  sem = palloc0(sizeof(JsonSemAction));

  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0 || rsi->expectedDesc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that "
                                                                   "cannot accept a set")));
  }

  rsi->returnMode = SFRM_Materialize;

                                                               
  tupdesc = rsi->expectedDesc;

                                                              
  old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);

  state->ret_tdesc = CreateTupleDescCopy(tupdesc);
  BlessTupleDesc(state->ret_tdesc);
  state->tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

  MemoryContextSwitchTo(old_cxt);

  sem->semstate = (void *)state;
  sem->object_start = elements_object_start;
  sem->scalar = elements_scalar;
  sem->array_element_start = elements_array_element_start;
  sem->array_element_end = elements_array_element_end;

  state->function_name = funcname;
  state->normalize_results = as_text;
  state->next_scalar = false;
  state->lex = lex;
  state->tmp_cxt = AllocSetContextCreate(CurrentMemoryContext, "json_array_elements temporary cxt", ALLOCSET_DEFAULT_SIZES);

  pg_parse_json(lex, sem);

  MemoryContextDelete(state->tmp_cxt);

  rsi->setResult = state->tuple_store;
  rsi->setDesc = state->ret_tdesc;

  PG_RETURN_NULL();
}

static void
elements_array_element_start(void *state, bool isnull)
{
  ElementsState *_state = (ElementsState *)state;

                                                
  if (_state->lex->lex_level == 1)
  {
       
                                                                       
                                                                          
                                         
       
    if (_state->normalize_results && _state->lex->token_type == JSON_TOKEN_STRING)
    {
      _state->next_scalar = true;
    }
    else
    {
      _state->result_start = _state->lex->token_start;
    }
  }
}

static void
elements_array_element_end(void *state, bool isnull)
{
  ElementsState *_state = (ElementsState *)state;
  MemoryContext old_cxt;
  int len;
  text *val;
  HeapTuple tuple;
  Datum values[1];
  bool nulls[1] = {false};

                                
  if (_state->lex->lex_level != 1)
  {
    return;
  }

                                                                       
  old_cxt = MemoryContextSwitchTo(_state->tmp_cxt);

  if (isnull && _state->normalize_results)
  {
    nulls[0] = true;
    values[0] = (Datum)NULL;
  }
  else if (_state->next_scalar)
  {
    values[0] = CStringGetTextDatum(_state->normalized_scalar);
    _state->next_scalar = false;
  }
  else
  {
    len = _state->lex->prev_token_terminator - _state->result_start;
    val = cstring_to_text_with_len(_state->result_start, len);
    values[0] = PointerGetDatum(val);
  }

  tuple = heap_form_tuple(_state->ret_tdesc, values, nulls);

  tuplestore_puttuple(_state->tuple_store, tuple);

                                
  MemoryContextSwitchTo(old_cxt);
  MemoryContextReset(_state->tmp_cxt);
}

static void
elements_object_start(void *state)
{
  ElementsState *_state = (ElementsState *)state;

                            
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a non-array", _state->function_name)));
  }
}

static void
elements_scalar(void *state, char *token, JsonTokenType tokentype)
{
  ElementsState *_state = (ElementsState *)state;

                            
  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a scalar", _state->function_name)));
  }

                                           
  if (_state->next_scalar)
  {
    _state->normalized_scalar = token;
  }
}

   
                                     
   
                                                 
   
                                                          
                                                    
   
                                                           
                                                            
                                               
   
Datum
jsonb_populate_record(PG_FUNCTION_ARGS)
{
  return populate_record_worker(fcinfo, "jsonb_populate_record", false, true);
}

Datum
jsonb_to_record(PG_FUNCTION_ARGS)
{
  return populate_record_worker(fcinfo, "jsonb_to_record", false, false);
}

Datum
json_populate_record(PG_FUNCTION_ARGS)
{
  return populate_record_worker(fcinfo, "json_populate_record", true, true);
}

Datum
json_to_record(PG_FUNCTION_ARGS)
{
  return populate_record_worker(fcinfo, "json_to_record", true, false);
}

                                     
static void
populate_array_report_expected_array(PopulateArrayContext *ctx, int ndim)
{
  if (ndim <= 0)
  {
    if (ctx->colname)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected JSON array"), errhint("See the value of key \"%s\".", ctx->colname)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected JSON array")));
    }
  }
  else
  {
    StringInfoData indices;
    int i;

    initStringInfo(&indices);

    Assert(ctx->ndims > 0 && ndim < ctx->ndims);

    for (i = 0; i < ndim; i++)
    {
      appendStringInfo(&indices, "[%d]", ctx->sizes[i]);
    }

    if (ctx->colname)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected JSON array"), errhint("See the array element %s of key \"%s\".", indices.data, ctx->colname)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("expected JSON array"), errhint("See the array element %s.", indices.data)));
    }
  }
}

                                                                               
static void
populate_array_assign_ndims(PopulateArrayContext *ctx, int ndims)
{
  int i;

  Assert(ctx->ndims <= 0);

  if (ndims <= 0)
  {
    populate_array_report_expected_array(ctx, ndims);
  }

  ctx->ndims = ndims;
  ctx->dims = palloc(sizeof(int) * ndims);
  ctx->sizes = palloc0(sizeof(int) * ndims);

  for (i = 0; i < ndims; i++)
  {
    ctx->dims[i] = -1;                                 
  }
}

                                            
static void
populate_array_check_dimension(PopulateArrayContext *ctx, int ndim)
{
  int dim = ctx->sizes[ndim];                                

  if (ctx->dims[ndim] == -1)
  {
    ctx->dims[ndim] = dim;                                        
  }
  else if (ctx->dims[ndim] != dim)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed JSON array"),
                       errdetail("Multidimensional arrays must have "
                                 "sub-arrays with matching dimensions.")));
  }

                                                      
  ctx->sizes[ndim] = 0;

                                                                          
  if (ndim > 0)
  {
    ctx->sizes[ndim - 1]++;
  }
}

static void
populate_array_element(PopulateArrayContext *ctx, int ndim, JsValue *jsv)
{
  Datum element;
  bool element_isnull;

                                  
  element = populate_record_field(ctx->aio->element_info, ctx->aio->element_type, ctx->aio->element_typmod, NULL, ctx->mcxt, PointerGetDatum(NULL), jsv, &element_isnull);

  accumArrayResult(ctx->astate, element, element_isnull, ctx->aio->element_type, ctx->acxt);

  Assert(ndim > 0);
  ctx->sizes[ndim - 1]++;                                          
}

                                                         
static void
populate_array_object_start(void *_state)
{
  PopulateArrayState *state = (PopulateArrayState *)_state;
  int ndim = state->lex->lex_level;

  if (state->ctx->ndims <= 0)
  {
    populate_array_assign_ndims(state->ctx, ndim);
  }
  else if (ndim < state->ctx->ndims)
  {
    populate_array_report_expected_array(state->ctx, ndim);
  }
}

                                                      
static void
populate_array_array_end(void *_state)
{
  PopulateArrayState *state = (PopulateArrayState *)_state;
  PopulateArrayContext *ctx = state->ctx;
  int ndim = state->lex->lex_level;

  if (ctx->ndims <= 0)
  {
    populate_array_assign_ndims(ctx, ndim + 1);
  }

  if (ndim < ctx->ndims)
  {
    populate_array_check_dimension(ctx, ndim);
  }
}

                                                                
static void
populate_array_element_start(void *_state, bool isnull)
{
  PopulateArrayState *state = (PopulateArrayState *)_state;
  int ndim = state->lex->lex_level;

  if (state->ctx->ndims <= 0 || ndim == state->ctx->ndims)
  {
                                              
    state->element_start = state->lex->token_start;
    state->element_type = state->lex->token_type;
    state->element_scalar = NULL;
  }
}

                                                              
static void
populate_array_element_end(void *_state, bool isnull)
{
  PopulateArrayState *state = (PopulateArrayState *)_state;
  PopulateArrayContext *ctx = state->ctx;
  int ndim = state->lex->lex_level;

  Assert(ctx->ndims > 0);

  if (ndim == ctx->ndims)
  {
    JsValue jsv;

    jsv.is_json = true;
    jsv.val.json.type = state->element_type;

    if (isnull)
    {
      Assert(jsv.val.json.type == JSON_TOKEN_NULL);
      jsv.val.json.str = NULL;
      jsv.val.json.len = 0;
    }
    else if (state->element_scalar)
    {
      jsv.val.json.str = state->element_scalar;
      jsv.val.json.len = -1;                      
    }
    else
    {
      jsv.val.json.str = state->element_start;
      jsv.val.json.len = (state->lex->prev_token_terminator - state->element_start) * sizeof(char);
    }

    populate_array_element(ctx, ndim, &jsv);
  }
}

                                                   
static void
populate_array_scalar(void *_state, char *token, JsonTokenType tokentype)
{
  PopulateArrayState *state = (PopulateArrayState *)_state;
  PopulateArrayContext *ctx = state->ctx;
  int ndim = state->lex->lex_level;

  if (ctx->ndims <= 0)
  {
    populate_array_assign_ndims(ctx, ndim);
  }
  else if (ndim < ctx->ndims)
  {
    populate_array_report_expected_array(ctx, ndim);
  }

  if (ndim == ctx->ndims)
  {
                                           
    state->element_scalar = token;
                                                                            
    Assert(state->element_type == tokentype);
  }
}

                                           
static void
populate_array_json(PopulateArrayContext *ctx, char *json, int len)
{
  PopulateArrayState state;
  JsonSemAction sem;

  state.lex = makeJsonLexContextCstringLen(json, len, true);
  state.ctx = ctx;

  memset(&sem, 0, sizeof(sem));
  sem.semstate = (void *)&state;
  sem.object_start = populate_array_object_start;
  sem.array_end = populate_array_array_end;
  sem.array_element_start = populate_array_element_start;
  sem.array_element_end = populate_array_element_end;
  sem.scalar = populate_array_scalar;

  pg_parse_json(state.lex, &sem);

                                                    
  Assert(ctx->ndims > 0 && ctx->dims);

  pfree(state.lex);
}

   
                                                                             
                                                                
   
static void
populate_array_dim_jsonb(PopulateArrayContext *ctx,              
    JsonbValue *jbv,                                                     
    int ndim)                                                              
{
  JsonbContainer *jbc = jbv->val.binary.data;
  JsonbIterator *it;
  JsonbIteratorToken tok;
  JsonbValue val;
  JsValue jsv;

  check_stack_depth();

  if (jbv->type != jbvBinary || !JsonContainerIsArray(jbc))
  {
    populate_array_report_expected_array(ctx, ndim - 1);
  }

  Assert(!JsonContainerIsScalar(jbc));

  it = JsonbIteratorInit(jbc);

  tok = JsonbIteratorNext(&it, &val, true);
  Assert(tok == WJB_BEGIN_ARRAY);

  tok = JsonbIteratorNext(&it, &val, true);

     
                                                                           
                                                                            
                               
     
  if (ctx->ndims <= 0 && (tok == WJB_END_ARRAY || (tok == WJB_ELEM && (val.type != jbvBinary || !JsonContainerIsArray(val.val.binary.data)))))
  {
    populate_array_assign_ndims(ctx, ndim);
  }

  jsv.is_json = false;
  jsv.val.jsonb = &val;

                                      
  while (tok == WJB_ELEM)
  {
       
                                                                           
                                          
       
    if (ctx->ndims > 0 && ndim >= ctx->ndims)
    {
      populate_array_element(ctx, ndim, &jsv);
    }
    else
    {
                                    
      populate_array_dim_jsonb(ctx, &val, ndim + 1);

                                                        
      Assert(ctx->ndims > 0 && ctx->dims);

      populate_array_check_dimension(ctx, ndim);
    }

    tok = JsonbIteratorNext(&it, &val, true);
  }

  Assert(tok == WJB_END_ARRAY);

                                               
  tok = JsonbIteratorNext(&it, &val, true);
  Assert(tok == WJB_DONE && !it);
}

                                                   
static Datum
populate_array(ArrayIOData *aio, const char *colname, MemoryContext mcxt, JsValue *jsv)
{
  PopulateArrayContext ctx;
  Datum result;
  int *lbs;
  int i;

  ctx.aio = aio;
  ctx.mcxt = mcxt;
  ctx.acxt = CurrentMemoryContext;
  ctx.astate = initArrayResult(aio->element_type, ctx.acxt, true);
  ctx.colname = colname;
  ctx.ndims = 0;                  
  ctx.dims = NULL;
  ctx.sizes = NULL;

  if (jsv->is_json)
  {
    populate_array_json(&ctx, jsv->val.json.str, jsv->val.json.len >= 0 ? jsv->val.json.len : strlen(jsv->val.json.str));
  }
  else
  {
    populate_array_dim_jsonb(&ctx, jsv->val.jsonb, 1);
    ctx.dims[0] = ctx.sizes[0];
  }

  Assert(ctx.ndims > 0);

  lbs = palloc(sizeof(int) * ctx.ndims);

  for (i = 0; i < ctx.ndims; i++)
  {
    lbs[i] = 1;
  }

  result = makeMdArrayResult(ctx.astate, ctx.ndims, ctx.dims, lbs, ctx.acxt, true);

  pfree(ctx.dims);
  pfree(ctx.sizes);
  pfree(lbs);

  return result;
}

static void
JsValueToJsObject(JsValue *jsv, JsObject *jso)
{
  jso->is_json = jsv->is_json;

  if (jsv->is_json)
  {
                                                   
    jso->val.json_hash = get_json_object_as_hash(jsv->val.json.str, jsv->val.json.len >= 0 ? jsv->val.json.len : strlen(jsv->val.json.str), "populate_composite");
  }
  else
  {
    JsonbValue *jbv = jsv->val.jsonb;

    if (jbv->type == jbvBinary && JsonContainerIsObject(jbv->val.binary.data))
    {
      jso->val.jsonb_cont = jbv->val.binary.data;
    }
    else
    {
      bool is_scalar;

      is_scalar = IsAJsonbScalar(jbv) || (jbv->type == jbvBinary && JsonContainerIsScalar(jbv->val.binary.data));
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), is_scalar ? errmsg("cannot call %s on a scalar", "populate_composite") : errmsg("cannot call %s on an array", "populate_composite")));
    }
  }
}

                                                                    
static void
update_cached_tupdesc(CompositeIOData *io, MemoryContext mcxt)
{
  if (!io->tupdesc || io->tupdesc->tdtypeid != io->base_typid || io->tupdesc->tdtypmod != io->base_typmod)
  {
    TupleDesc tupdesc = lookup_rowtype_tupdesc(io->base_typid, io->base_typmod);
    MemoryContext oldcxt;

    if (io->tupdesc)
    {
      FreeTupleDesc(io->tupdesc);
    }

                                                                       
    oldcxt = MemoryContextSwitchTo(mcxt);
    io->tupdesc = CreateTupleDescCopy(tupdesc);
    MemoryContextSwitchTo(oldcxt);

    ReleaseTupleDesc(tupdesc);
  }
}

                                                                       
static Datum
populate_composite(CompositeIOData *io, Oid typid, const char *colname, MemoryContext mcxt, HeapTupleHeader defaultval, JsValue *jsv, bool isnull)
{
  Datum result;

                                              
  update_cached_tupdesc(io, mcxt);

  if (isnull)
  {
    result = (Datum)0;
  }
  else
  {
    HeapTupleHeader tuple;
    JsObject jso;

                             
    JsValueToJsObject(jsv, &jso);

                                         
    tuple = populate_record(io->tupdesc, &io->record_io, defaultval, mcxt, &jso);
    result = HeapTupleHeaderGetDatum(tuple);

    JsObjectFree(&jso);
  }

     
                                                                            
                                                                           
                                                         
     
  if (typid != io->base_typid && typid != RECORDOID)
  {
    domain_check(result, isnull, typid, &io->domain_info, mcxt);
  }

  return result;
}

                                                          
static Datum
populate_scalar(ScalarIOData *io, Oid typid, int32 typmod, JsValue *jsv)
{
  Datum res;
  char *str = NULL;
  char *json = NULL;

  if (jsv->is_json)
  {
    int len = jsv->val.json.len;

    json = jsv->val.json.str;
    Assert(json);
    if (len >= 0)
    {
                                                   
      str = palloc(len + 1 * sizeof(char));
      memcpy(str, json, len);
      str[len] = '\0';
    }
    else
    {
      str = json;                                        
    }

                                                                          
    if ((typid == JSONOID || typid == JSONBOID) && jsv->val.json.type == JSON_TOKEN_STRING)
    {
      StringInfoData buf;

      initStringInfo(&buf);
      escape_json(&buf, str);
                                 
      if (str != json)
      {
        pfree(str);
      }
      str = buf.data;
    }
  }
  else
  {
    JsonbValue *jbv = jsv->val.jsonb;

    if (typid == JSONBOID)
    {
      Jsonb *jsonb = JsonbValueToJsonb(jbv);                         

      return JsonbPGetDatum(jsonb);
    }
                                                
    else if (typid == JSONOID && jbv->type != jbvBinary)
    {
         
                                                                         
                                                                     
         
      Jsonb *jsonb = JsonbValueToJsonb(jbv);

      str = JsonbToCString(NULL, &jsonb->root, VARSIZE(jsonb));
    }
    else if (jbv->type == jbvString)                          
    {
      str = pnstrdup(jbv->val.string.val, jbv->val.string.len);
    }
    else if (jbv->type == jbvBool)
    {
      str = pstrdup(jbv->val.boolean ? "true" : "false");
    }
    else if (jbv->type == jbvNumeric)
    {
      str = DatumGetCString(DirectFunctionCall1(numeric_out, PointerGetDatum(jbv->val.numeric)));
    }
    else if (jbv->type == jbvBinary)
    {
      str = JsonbToCString(NULL, jbv->val.binary.data, jbv->val.binary.len);
    }
    else
    {
      elog(ERROR, "unrecognized jsonb type: %d", (int)jbv->type);
    }
  }

  res = InputFunctionCall(&io->typiofunc, str, io->typioparam, typmod);

                             
  if (str != json)
  {
    pfree(str);
  }

  return res;
}

static Datum
populate_domain(DomainIOData *io, Oid typid, const char *colname, MemoryContext mcxt, JsValue *jsv, bool isnull)
{
  Datum res;

  if (isnull)
  {
    res = (Datum)0;
  }
  else
  {
    res = populate_record_field(io->base_io, io->base_typid, io->base_typmod, colname, mcxt, PointerGetDatum(NULL), jsv, &isnull);
    Assert(!isnull);
  }

  domain_check(res, isnull, typid, &io->domain_info, mcxt);

  return res;
}

                                                      
static void
prepare_column_cache(ColumnIOData *column, Oid typid, int32 typmod, MemoryContext mcxt, bool need_scalar)
{
  HeapTuple tup;
  Form_pg_type type;

  column->typid = typid;
  column->typmod = typmod;

  tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }

  type = (Form_pg_type)GETSTRUCT(tup);

  if (type->typtype == TYPTYPE_DOMAIN)
  {
       
                                                                         
                                                                     
       
    Oid base_typid;
    int32 base_typmod = typmod;

    base_typid = getBaseTypeAndTypmod(typid, &base_typmod);
    if (get_typtype(base_typid) == TYPTYPE_COMPOSITE)
    {
                                                       
      column->typcat = TYPECAT_COMPOSITE_DOMAIN;
      column->io.composite.record_io = NULL;
      column->io.composite.tupdesc = NULL;
      column->io.composite.base_typid = base_typid;
      column->io.composite.base_typmod = base_typmod;
      column->io.composite.domain_info = NULL;
    }
    else
    {
                                     
      column->typcat = TYPECAT_DOMAIN;
      column->io.domain.base_typid = base_typid;
      column->io.domain.base_typmod = base_typmod;
      column->io.domain.base_io = MemoryContextAllocZero(mcxt, sizeof(ColumnIOData));
      column->io.domain.domain_info = NULL;
    }
  }
  else if (type->typtype == TYPTYPE_COMPOSITE || typid == RECORDOID)
  {
    column->typcat = TYPECAT_COMPOSITE;
    column->io.composite.record_io = NULL;
    column->io.composite.tupdesc = NULL;
    column->io.composite.base_typid = typid;
    column->io.composite.base_typmod = typmod;
    column->io.composite.domain_info = NULL;
  }
  else if (type->typlen == -1 && OidIsValid(type->typelem))
  {
    column->typcat = TYPECAT_ARRAY;
    column->io.array.element_info = MemoryContextAllocZero(mcxt, sizeof(ColumnIOData));
    column->io.array.element_type = type->typelem;
                                                            
    column->io.array.element_typmod = typmod;
  }
  else
  {
    column->typcat = TYPECAT_SCALAR;
    need_scalar = true;
  }

                                                                          
  if (need_scalar)
  {
    Oid typioproc;

    getTypeInputInfo(typid, &typioproc, &column->scalar_io.typioparam);
    fmgr_info_cxt(typioproc, &column->scalar_io.typiofunc, mcxt);
  }

  ReleaseSysCache(tup);
}

                                                                                     
static Datum
populate_record_field(ColumnIOData *col, Oid typid, int32 typmod, const char *colname, MemoryContext mcxt, Datum defaultval, JsValue *jsv, bool *isnull)
{
  TypeCat typcat;

  check_stack_depth();

     
                                                                            
                                                                  
     
  if (col->typid != typid || col->typmod != typmod)
  {
    prepare_column_cache(col, typid, typmod, mcxt, true);
  }

  *isnull = JsValueIsNull(jsv);

  typcat = col->typcat;

                                                                              
  if (JsValueIsString(jsv) && (typcat == TYPECAT_ARRAY || typcat == TYPECAT_COMPOSITE || typcat == TYPECAT_COMPOSITE_DOMAIN))
  {
    typcat = TYPECAT_SCALAR;
  }

                                                                           
  if (*isnull && typcat != TYPECAT_DOMAIN && typcat != TYPECAT_COMPOSITE_DOMAIN)
  {
    return (Datum)0;
  }

  switch (typcat)
  {
  case TYPECAT_SCALAR:
    return populate_scalar(&col->scalar_io, typid, typmod, jsv);

  case TYPECAT_ARRAY:
    return populate_array(&col->io.array, colname, mcxt, jsv);

  case TYPECAT_COMPOSITE:
  case TYPECAT_COMPOSITE_DOMAIN:
    return populate_composite(&col->io.composite, typid, colname, mcxt, DatumGetPointer(defaultval) ? DatumGetHeapTupleHeader(defaultval) : NULL, jsv, *isnull);

  case TYPECAT_DOMAIN:
    return populate_domain(&col->io.domain, typid, colname, mcxt, jsv, *isnull);

  default:
    elog(ERROR, "unrecognized type category '%c'", typcat);
    return (Datum)0;
  }
}

static RecordIOData *
allocate_record_info(MemoryContext mcxt, int ncolumns)
{
  RecordIOData *data = (RecordIOData *)MemoryContextAlloc(mcxt, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));

  data->record_type = InvalidOid;
  data->record_typmod = 0;
  data->ncolumns = ncolumns;
  MemSet(data->columns, 0, sizeof(ColumnIOData) * ncolumns);

  return data;
}

static bool
JsObjectGetField(JsObject *obj, char *field, JsValue *jsv)
{
  jsv->is_json = obj->is_json;

  if (jsv->is_json)
  {
    JsonHashEntry *hashentry = hash_search(obj->val.json_hash, field, HASH_FIND, NULL);

    jsv->val.json.type = hashentry ? hashentry->type : JSON_TOKEN_NULL;
    jsv->val.json.str = jsv->val.json.type == JSON_TOKEN_NULL ? NULL : hashentry->val;
    jsv->val.json.len = jsv->val.json.str ? -1 : 0;                      

    return hashentry != NULL;
  }
  else
  {
    jsv->val.jsonb = !obj->val.jsonb_cont ? NULL : findJsonbValueFromContainerLen(obj->val.jsonb_cont, JB_FOBJECT, field, strlen(field));

    return jsv->val.jsonb != NULL;
  }
}

                                                   
static HeapTupleHeader
populate_record(TupleDesc tupdesc, RecordIOData **record_p, HeapTupleHeader defaultval, MemoryContext mcxt, JsObject *obj)
{
  RecordIOData *record = *record_p;
  Datum *values;
  bool *nulls;
  HeapTuple res;
  int ncolumns = tupdesc->natts;
  int i;

     
                                                                             
                                                                           
            
     
  if (defaultval && JsObjectIsEmpty(obj))
  {
    return defaultval;
  }

                                   
  if (record == NULL || record->ncolumns != ncolumns)
  {
    *record_p = record = allocate_record_info(mcxt, ncolumns);
  }

                                                                
  if (record->record_type != tupdesc->tdtypeid || record->record_typmod != tupdesc->tdtypmod)
  {
    MemSet(record, 0, offsetof(RecordIOData, columns) + ncolumns * sizeof(ColumnIOData));
    record->record_type = tupdesc->tdtypeid;
    record->record_typmod = tupdesc->tdtypmod;
    record->ncolumns = ncolumns;
  }

  values = (Datum *)palloc(ncolumns * sizeof(Datum));
  nulls = (bool *)palloc(ncolumns * sizeof(bool));

  if (defaultval)
  {
    HeapTupleData tuple;

                                                       
    tuple.t_len = HeapTupleHeaderGetDatumLength(defaultval);
    ItemPointerSetInvalid(&(tuple.t_self));
    tuple.t_tableOid = InvalidOid;
    tuple.t_data = defaultval;

                                          
    heap_deform_tuple(&tuple, tupdesc, values, nulls);
  }
  else
  {
    for (i = 0; i < ncolumns; ++i)
    {
      values[i] = (Datum)0;
      nulls[i] = true;
    }
  }

  for (i = 0; i < ncolumns; ++i)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);
    char *colname = NameStr(att->attname);
    JsValue field = {0};
    bool found;

                                            
    if (att->attisdropped)
    {
      nulls[i] = true;
      continue;
    }

    found = JsObjectGetField(obj, colname, &field);

       
                                                                           
                                                                     
                                                                       
                                                                       
                                                                        
                                                           
       
    if (defaultval && !found)
    {
      continue;
    }

    values[i] = populate_record_field(&record->columns[i], att->atttypid, att->atttypmod, colname, mcxt, nulls[i] ? (Datum)0 : values[i], &field, &nulls[i]);
  }

  res = heap_form_tuple(tupdesc, values, nulls);

  pfree(values);
  pfree(nulls);

  return res->t_data;
}

   
                                                                             
                                                                               
                                                 
   
static void
get_record_type_from_argument(FunctionCallInfo fcinfo, const char *funcname, PopulateRecordCache *cache)
{
  cache->argtype = get_fn_expr_argtype(fcinfo->flinfo, 0);
  prepare_column_cache(&cache->c, cache->argtype, -1, cache->fn_mcxt, false);
  if (cache->c.typcat != TYPECAT_COMPOSITE && cache->c.typcat != TYPECAT_COMPOSITE_DOMAIN)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                                                                                 
                       errmsg("first argument of %s must be a row type", funcname)));
  }
}

   
                                                                         
                                                                      
                                                                    
   
                                                                  
                             
   
static void
get_record_type_from_query(FunctionCallInfo fcinfo, const char *funcname, PopulateRecordCache *cache)
{
  TupleDesc tupdesc;
  MemoryContext old_cxt;

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                                 
                       errmsg("could not determine row type for result of %s", funcname),
                       errhint("Provide a non-null record argument, "
                               "or call the function in the FROM clause "
                               "using a column definition list.")));
  }

  Assert(tupdesc);
  cache->argtype = tupdesc->tdtypeid;

                                                               
  if (cache->c.io.composite.tupdesc)
  {
    FreeTupleDesc(cache->c.io.composite.tupdesc);
  }

                               
  old_cxt = MemoryContextSwitchTo(cache->fn_mcxt);
  cache->c.io.composite.tupdesc = CreateTupleDescCopy(tupdesc);
  cache->c.io.composite.base_typid = tupdesc->tdtypeid;
  cache->c.io.composite.base_typmod = tupdesc->tdtypmod;
  MemoryContextSwitchTo(old_cxt);
}

   
                                                                       
                                                              
   
static Datum
populate_record_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg)
{
  int json_arg_num = have_record_arg ? 1 : 0;
  JsValue jsv = {0};
  HeapTupleHeader rec;
  Datum rettuple;
  JsonbValue jbv;
  MemoryContext fnmcxt = fcinfo->flinfo->fn_mcxt;
  PopulateRecordCache *cache = fcinfo->flinfo->fn_extra;

     
                                                                          
                                                                             
                                                                            
     
  if (!cache)
  {
    fcinfo->flinfo->fn_extra = cache = MemoryContextAllocZero(fnmcxt, sizeof(*cache));
    cache->fn_mcxt = fnmcxt;

    if (have_record_arg)
    {
      get_record_type_from_argument(fcinfo, funcname, cache);
    }
    else
    {
      get_record_type_from_query(fcinfo, funcname, cache);
    }
  }

                                         
  if (!have_record_arg)
  {
    rec = NULL;                               
  }
  else if (!PG_ARGISNULL(0))
  {
    rec = PG_GETARG_HEAPTUPLEHEADER(0);

       
                                                                          
                         
       
    if (cache->argtype == RECORDOID)
    {
      cache->c.io.composite.base_typid = HeapTupleHeaderGetTypeId(rec);
      cache->c.io.composite.base_typmod = HeapTupleHeaderGetTypMod(rec);
    }
  }
  else
  {
    rec = NULL;

       
                                                                          
                                           
       
    if (cache->argtype == RECORDOID)
    {
      get_record_type_from_query(fcinfo, funcname, cache);
                                                                       
      Assert(cache->argtype == RECORDOID);
    }
  }

                                                                      
  if (PG_ARGISNULL(json_arg_num))
  {
    if (rec)
    {
      PG_RETURN_POINTER(rec);
    }
    else
    {
      PG_RETURN_NULL();
    }
  }

  jsv.is_json = is_json;

  if (is_json)
  {
    text *json = PG_GETARG_TEXT_PP(json_arg_num);

    jsv.val.json.str = VARDATA_ANY(json);
    jsv.val.json.len = VARSIZE_ANY_EXHDR(json);
    jsv.val.json.type = JSON_TOKEN_INVALID;                
                                                                      
  }
  else
  {
    Jsonb *jb = PG_GETARG_JSONB_P(json_arg_num);

    jsv.val.jsonb = &jbv;

                                                
    jbv.type = jbvBinary;
    jbv.val.binary.data = &jb->root;
    jbv.val.binary.len = VARSIZE(jb) - VARHDRSZ;
  }

  rettuple = populate_composite(&cache->c.io.composite, cache->argtype, NULL, fnmcxt, rec, &jsv, false);

  PG_RETURN_DATUM(rettuple);
}

   
                           
   
                                              
   
static HTAB *
get_json_object_as_hash(char *json, int len, const char *funcname)
{
  HASHCTL ctl;
  HTAB *tab;
  JHashState *state;
  JsonLexContext *lex = makeJsonLexContextCstringLen(json, len, true);
  JsonSemAction *sem;

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = NAMEDATALEN;
  ctl.entrysize = sizeof(JsonHashEntry);
  ctl.hcxt = CurrentMemoryContext;
  tab = hash_create("json object hashtable", 100, &ctl, HASH_ELEM | HASH_CONTEXT);

  state = palloc0(sizeof(JHashState));
  sem = palloc0(sizeof(JsonSemAction));

  state->function_name = funcname;
  state->hash = tab;
  state->lex = lex;

  sem->semstate = (void *)state;
  sem->array_start = hash_array_start;
  sem->scalar = hash_scalar;
  sem->object_field_start = hash_object_field_start;
  sem->object_field_end = hash_object_field_end;

  pg_parse_json(lex, sem);

  return tab;
}

static void
hash_object_field_start(void *state, char *fname, bool isnull)
{
  JHashState *_state = (JHashState *)state;

  if (_state->lex->lex_level > 1)
  {
    return;
  }

                           
  _state->saved_token_type = _state->lex->token_type;

  if (_state->lex->token_type == JSON_TOKEN_ARRAY_START || _state->lex->token_type == JSON_TOKEN_OBJECT_START)
  {
                                                                    
    _state->save_json_start = _state->lex->token_start;
  }
  else
  {
                          
    _state->save_json_start = NULL;
  }
}

static void
hash_object_field_end(void *state, char *fname, bool isnull)
{
  JHashState *_state = (JHashState *)state;
  JsonHashEntry *hashentry;
  bool found;

     
                           
     
  if (_state->lex->lex_level > 1)
  {
    return;
  }

     
                                                                          
                                                                          
                                                                       
                                                                            
                                                                           
     
  if (strlen(fname) >= NAMEDATALEN)
  {
    return;
  }

  hashentry = hash_search(_state->hash, fname, HASH_ENTER, &found);

     
                                                                        
                                                                         
     

  hashentry->type = _state->saved_token_type;
  Assert(isnull == (hashentry->type == JSON_TOKEN_NULL));

  if (_state->save_json_start != NULL)
  {
    int len = _state->lex->prev_token_terminator - _state->save_json_start;
    char *val = palloc((len + 1) * sizeof(char));

    memcpy(val, _state->save_json_start, len);
    val[len] = '\0';
    hashentry->val = val;
  }
  else
  {
                                        
    hashentry->val = _state->saved_scalar;
  }
}

static void
hash_array_start(void *state)
{
  JHashState *_state = (JHashState *)state;

  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on an array", _state->function_name)));
  }
}

static void
hash_scalar(void *state, char *token, JsonTokenType tokentype)
{
  JHashState *_state = (JHashState *)state;

  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a scalar", _state->function_name)));
  }

  if (_state->lex->lex_level == 1)
  {
    _state->saved_scalar = token;
                                                                           
    Assert(_state->saved_token_type == tokentype);
  }
}

   
                                        
   
                                                          
                                      
   
                                                                
                                                                 
                            
   
Datum
jsonb_populate_recordset(PG_FUNCTION_ARGS)
{
  return populate_recordset_worker(fcinfo, "jsonb_populate_recordset", false, true);
}

Datum
jsonb_to_recordset(PG_FUNCTION_ARGS)
{
  return populate_recordset_worker(fcinfo, "jsonb_to_recordset", false, false);
}

Datum
json_populate_recordset(PG_FUNCTION_ARGS)
{
  return populate_recordset_worker(fcinfo, "json_populate_recordset", true, true);
}

Datum
json_to_recordset(PG_FUNCTION_ARGS)
{
  return populate_recordset_worker(fcinfo, "json_to_recordset", true, false);
}

static void
populate_recordset_record(PopulateRecordsetState *state, JsObject *obj)
{
  PopulateRecordCache *cache = state->cache;
  HeapTupleHeader tuphead;
  HeapTupleData tuple;

                                              
  update_cached_tupdesc(&cache->c.io.composite, cache->fn_mcxt);

                                       
  tuphead = populate_record(cache->c.io.composite.tupdesc, &cache->c.io.composite.record_io, state->rec, cache->fn_mcxt, obj);

                                                               
  if (cache->c.typcat == TYPECAT_COMPOSITE_DOMAIN)
  {
    domain_check(HeapTupleHeaderGetDatum(tuphead), false, cache->argtype, &cache->c.io.composite.domain_info, cache->fn_mcxt);
  }

                                
  tuple.t_len = HeapTupleHeaderGetDatumLength(tuphead);
  ItemPointerSetInvalid(&(tuple.t_self));
  tuple.t_tableOid = InvalidOid;
  tuple.t_data = tuphead;

  tuplestore_puttuple(state->tuple_store, &tuple);
}

   
                                                                             
                                                              
   
static Datum
populate_recordset_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg)
{
  int json_arg_num = have_record_arg ? 1 : 0;
  ReturnSetInfo *rsi;
  MemoryContext old_cxt;
  HeapTupleHeader rec;
  PopulateRecordCache *cache = fcinfo->flinfo->fn_extra;
  PopulateRecordsetState *state;

  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that "
                                                                   "cannot accept a set")));
  }

  rsi->returnMode = SFRM_Materialize;

     
                                                                          
                                                                             
                                                                            
     
  if (!cache)
  {
    fcinfo->flinfo->fn_extra = cache = MemoryContextAllocZero(fcinfo->flinfo->fn_mcxt, sizeof(*cache));
    cache->fn_mcxt = fcinfo->flinfo->fn_mcxt;

    if (have_record_arg)
    {
      get_record_type_from_argument(fcinfo, funcname, cache);
    }
    else
    {
      get_record_type_from_query(fcinfo, funcname, cache);
    }
  }

                                         
  if (!have_record_arg)
  {
    rec = NULL;                                  
  }
  else if (!PG_ARGISNULL(0))
  {
    rec = PG_GETARG_HEAPTUPLEHEADER(0);

       
                                                                          
                         
       
    if (cache->argtype == RECORDOID)
    {
      cache->c.io.composite.base_typid = HeapTupleHeaderGetTypeId(rec);
      cache->c.io.composite.base_typmod = HeapTupleHeaderGetTypMod(rec);
    }
  }
  else
  {
    rec = NULL;

       
                                                                          
                                           
       
    if (cache->argtype == RECORDOID)
    {
      get_record_type_from_query(fcinfo, funcname, cache);
                                                                       
      Assert(cache->argtype == RECORDOID);
    }
  }

                                                  
  if (PG_ARGISNULL(json_arg_num))
  {
    PG_RETURN_NULL();
  }

     
                                                                             
                                                  
     
  update_cached_tupdesc(&cache->c.io.composite, cache->fn_mcxt);

  state = palloc0(sizeof(PopulateRecordsetState));

                                                                   
  old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);
  state->tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);
  MemoryContextSwitchTo(old_cxt);

  state->function_name = funcname;
  state->cache = cache;
  state->rec = rec;

  if (is_json)
  {
    text *json = PG_GETARG_TEXT_PP(json_arg_num);
    JsonLexContext *lex;
    JsonSemAction *sem;

    sem = palloc0(sizeof(JsonSemAction));

    lex = makeJsonLexContext(json, true);

    sem->semstate = (void *)state;
    sem->array_start = populate_recordset_array_start;
    sem->array_element_start = populate_recordset_array_element_start;
    sem->scalar = populate_recordset_scalar;
    sem->object_field_start = populate_recordset_object_field_start;
    sem->object_field_end = populate_recordset_object_field_end;
    sem->object_start = populate_recordset_object_start;
    sem->object_end = populate_recordset_object_end;

    state->lex = lex;

    pg_parse_json(lex, sem);
  }
  else
  {
    Jsonb *jb = PG_GETARG_JSONB_P(json_arg_num);
    JsonbIterator *it;
    JsonbValue v;
    bool skipNested = false;
    JsonbIteratorToken r;

    if (JB_ROOT_IS_SCALAR(jb) || !JB_ROOT_IS_ARRAY(jb))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a non-array", funcname)));
    }

    it = JsonbIteratorInit(&jb->root);

    while ((r = JsonbIteratorNext(&it, &v, skipNested)) != WJB_DONE)
    {
      skipNested = true;

      if (r == WJB_ELEM)
      {
        JsObject obj;

        if (v.type != jbvBinary || !JsonContainerIsObject(v.val.binary.data))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument of %s must be an array of objects", funcname)));
        }

        obj.is_json = false;
        obj.val.jsonb_cont = v.val.binary.data;

        populate_recordset_record(state, &obj);
      }
    }
  }

     
                                                                          
                                                                         
                                           
     
  rsi->setResult = state->tuple_store;
  rsi->setDesc = CreateTupleDescCopy(cache->c.io.composite.tupdesc);

  PG_RETURN_NULL();
}

static void
populate_recordset_object_start(void *state)
{
  PopulateRecordsetState *_state = (PopulateRecordsetState *)state;
  int lex_level = _state->lex->lex_level;
  HASHCTL ctl;

                                                                    
  if (lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on an object", _state->function_name)));
  }

                                                    
  if (lex_level > 1)
  {
    return;
  }

                                                                  
  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = NAMEDATALEN;
  ctl.entrysize = sizeof(JsonHashEntry);
  ctl.hcxt = CurrentMemoryContext;
  _state->json_hash = hash_create("json object hashtable", 100, &ctl, HASH_ELEM | HASH_CONTEXT);
}

static void
populate_recordset_object_end(void *state)
{
  PopulateRecordsetState *_state = (PopulateRecordsetState *)state;
  JsObject obj;

                                                    
  if (_state->lex->lex_level > 1)
  {
    return;
  }

  obj.is_json = true;
  obj.val.json_hash = _state->json_hash;

                                                                            
  populate_recordset_record(_state, &obj);

                                      
  hash_destroy(_state->json_hash);
  _state->json_hash = NULL;
}

static void
populate_recordset_array_element_start(void *state, bool isnull)
{
  PopulateRecordsetState *_state = (PopulateRecordsetState *)state;

  if (_state->lex->lex_level == 1 && _state->lex->token_type != JSON_TOKEN_OBJECT_START)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("argument of %s must be an array of objects", _state->function_name)));
  }
}

static void
populate_recordset_array_start(void *state)
{
                     
}

static void
populate_recordset_scalar(void *state, char *token, JsonTokenType tokentype)
{
  PopulateRecordsetState *_state = (PopulateRecordsetState *)state;

  if (_state->lex->lex_level == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot call %s on a scalar", _state->function_name)));
  }

  if (_state->lex->lex_level == 2)
  {
    _state->saved_scalar = token;
  }
}

static void
populate_recordset_object_field_start(void *state, char *fname, bool isnull)
{
  PopulateRecordsetState *_state = (PopulateRecordsetState *)state;

  if (_state->lex->lex_level > 2)
  {
    return;
  }

  _state->saved_token_type = _state->lex->token_type;

  if (_state->lex->token_type == JSON_TOKEN_ARRAY_START || _state->lex->token_type == JSON_TOKEN_OBJECT_START)
  {
    _state->save_json_start = _state->lex->token_start;
  }
  else
  {
    _state->save_json_start = NULL;
  }
}

static void
populate_recordset_object_field_end(void *state, char *fname, bool isnull)
{
  PopulateRecordsetState *_state = (PopulateRecordsetState *)state;
  JsonHashEntry *hashentry;
  bool found;

     
                           
     
  if (_state->lex->lex_level > 2)
  {
    return;
  }

     
                                                                          
                                                                          
                                                                       
                                                                            
                                                                           
     
  if (strlen(fname) >= NAMEDATALEN)
  {
    return;
  }

  hashentry = hash_search(_state->json_hash, fname, HASH_ENTER, &found);

     
                                                                        
                                                                         
     

  hashentry->type = _state->saved_token_type;
  Assert(isnull == (hashentry->type == JSON_TOKEN_NULL));

  if (_state->save_json_start != NULL)
  {
    int len = _state->lex->prev_token_terminator - _state->save_json_start;
    char *val = palloc((len + 1) * sizeof(char));

    memcpy(val, _state->save_json_start, len);
    val[len] = '\0';
    hashentry->val = val;
  }
  else
  {
                                        
    hashentry->val = _state->saved_scalar;
  }
}

   
                                                                             
   
static JsonbValue *
findJsonbValueFromContainerLen(JsonbContainer *container, uint32 flags, char *key, uint32 keylen)
{
  JsonbValue k;

  k.type = jbvString;
  k.val.string.val = key;
  k.val.string.len = keylen;

  return findJsonbValueFromContainer(container, flags, &k);
}

   
                                          
   
                                                             
                                                             
                                                                    
              
   

static void
sn_object_start(void *state)
{
  StripnullState *_state = (StripnullState *)state;

  appendStringInfoCharMacro(_state->strval, '{');
}

static void
sn_object_end(void *state)
{
  StripnullState *_state = (StripnullState *)state;

  appendStringInfoCharMacro(_state->strval, '}');
}

static void
sn_array_start(void *state)
{
  StripnullState *_state = (StripnullState *)state;

  appendStringInfoCharMacro(_state->strval, '[');
}

static void
sn_array_end(void *state)
{
  StripnullState *_state = (StripnullState *)state;

  appendStringInfoCharMacro(_state->strval, ']');
}

static void
sn_object_field_start(void *state, char *fname, bool isnull)
{
  StripnullState *_state = (StripnullState *)state;

  if (isnull)
  {
       
                                                                      
                                                                         
                                                                     
       
    _state->skip_next_null = true;
    return;
  }

  if (_state->strval->data[_state->strval->len - 1] != '{')
  {
    appendStringInfoCharMacro(_state->strval, ',');
  }

     
                                                                            
                              
     
  escape_json(_state->strval, fname);

  appendStringInfoCharMacro(_state->strval, ':');
}

static void
sn_array_element_start(void *state, bool isnull)
{
  StripnullState *_state = (StripnullState *)state;

  if (_state->strval->data[_state->strval->len - 1] != '[')
  {
    appendStringInfoCharMacro(_state->strval, ',');
  }
}

static void
sn_scalar(void *state, char *token, JsonTokenType tokentype)
{
  StripnullState *_state = (StripnullState *)state;

  if (_state->skip_next_null)
  {
    Assert(tokentype == JSON_TOKEN_NULL);
    _state->skip_next_null = false;
    return;
  }

  if (tokentype == JSON_TOKEN_STRING)
  {
    escape_json(_state->strval, token);
  }
  else
  {
    appendStringInfoString(_state->strval, token);
  }
}

   
                                               
   
Datum
json_strip_nulls(PG_FUNCTION_ARGS)
{
  text *json = PG_GETARG_TEXT_PP(0);
  StripnullState *state;
  JsonLexContext *lex;
  JsonSemAction *sem;

  lex = makeJsonLexContext(json, true);
  state = palloc0(sizeof(StripnullState));
  sem = palloc0(sizeof(JsonSemAction));

  state->strval = makeStringInfo();
  state->skip_next_null = false;
  state->lex = lex;

  sem->semstate = (void *)state;
  sem->object_start = sn_object_start;
  sem->object_end = sn_object_end;
  sem->array_start = sn_array_start;
  sem->array_end = sn_array_end;
  sem->scalar = sn_scalar;
  sem->array_element_start = sn_array_element_start;
  sem->object_field_start = sn_object_field_start;

  pg_parse_json(lex, sem);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(state->strval->data, state->strval->len));
}

   
                                                  
   
Datum
jsonb_strip_nulls(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  JsonbIterator *it;
  JsonbParseState *parseState = NULL;
  JsonbValue *res = NULL;
  JsonbValue v, k;
  JsonbIteratorToken type;
  bool last_was_key = false;

  if (JB_ROOT_IS_SCALAR(jb))
  {
    PG_RETURN_POINTER(jb);
  }

  it = JsonbIteratorInit(&jb->root);

  while ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    Assert(!(type == WJB_KEY && last_was_key));

    if (type == WJB_KEY)
    {
                                                              
      k = v;
      last_was_key = true;
      continue;
    }

    if (last_was_key)
    {
                                                           
      last_was_key = false;

                                            
      if (type == WJB_VALUE && v.type == jbvNull)
      {
        continue;
      }

                                                   
      (void)pushJsonbValue(&parseState, WJB_KEY, &k);
    }

    if (type == WJB_VALUE || type == WJB_ELEM)
    {
      res = pushJsonbValue(&parseState, type, &v);
    }
    else
    {
      res = pushJsonbValue(&parseState, type, NULL);
    }
  }

  Assert(res != NULL);

  PG_RETURN_POINTER(JsonbValueToJsonb(res));
}

   
                                                 
   
                                                                     
                       
   
                                                                          
                                                                       
   
static void
addJsonbToParseState(JsonbParseState **jbps, Jsonb *jb)
{
  JsonbIterator *it;
  JsonbValue *o = &(*jbps)->contVal;
  JsonbValue v;
  JsonbIteratorToken type;

  it = JsonbIteratorInit(&jb->root);

  Assert(o->type == jbvArray || o->type == jbvObject);

  if (JB_ROOT_IS_SCALAR(jb))
  {
    (void)JsonbIteratorNext(&it, &v, false);                        
    Assert(v.type == jbvArray);
    (void)JsonbIteratorNext(&it, &v, false);                         

    switch (o->type)
    {
    case jbvArray:
      (void)pushJsonbValue(jbps, WJB_ELEM, &v);
      break;
    case jbvObject:
      (void)pushJsonbValue(jbps, WJB_VALUE, &v);
      break;
    default:
      elog(ERROR, "unexpected parent of nested structure");
    }
  }
  else
  {
    while ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
    {
      if (type == WJB_KEY || type == WJB_VALUE || type == WJB_ELEM)
      {
        (void)pushJsonbValue(jbps, type, &v);
      }
      else
      {
        (void)pushJsonbValue(jbps, type, NULL);
      }
    }
  }
}

   
                                     
   
                                     
   
Datum
jsonb_pretty(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  StringInfo str = makeStringInfo();

  JsonbToCStringIndent(str, &jb->root, VARSIZE(jb));

  PG_RETURN_TEXT_P(cstring_to_text_with_len(str->data, str->len));
}

   
                                            
   
                            
   
Datum
jsonb_concat(PG_FUNCTION_ARGS)
{
  Jsonb *jb1 = PG_GETARG_JSONB_P(0);
  Jsonb *jb2 = PG_GETARG_JSONB_P(1);
  JsonbParseState *state = NULL;
  JsonbValue *res;
  JsonbIterator *it1, *it2;

     
                                                                            
                                                                     
                                                                         
            
     
  if (JB_ROOT_IS_OBJECT(jb1) == JB_ROOT_IS_OBJECT(jb2))
  {
    if (JB_ROOT_COUNT(jb1) == 0 && !JB_ROOT_IS_SCALAR(jb2))
    {
      PG_RETURN_JSONB_P(jb2);
    }
    else if (JB_ROOT_COUNT(jb2) == 0 && !JB_ROOT_IS_SCALAR(jb1))
    {
      PG_RETURN_JSONB_P(jb1);
    }
  }

  it1 = JsonbIteratorInit(&jb1->root);
  it2 = JsonbIteratorInit(&jb2->root);

  res = IteratorConcat(&it1, &it2, &state);

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                           
   
                                                      
            
   
Datum
jsonb_delete(PG_FUNCTION_ARGS)
{
  Jsonb *in = PG_GETARG_JSONB_P(0);
  text *key = PG_GETARG_TEXT_PP(1);
  char *keyptr = VARDATA_ANY(key);
  int keylen = VARSIZE_ANY_EXHDR(key);
  JsonbParseState *state = NULL;
  JsonbIterator *it;
  JsonbValue v, *res = NULL;
  bool skipNested = false;
  JsonbIteratorToken r;

  if (JB_ROOT_IS_SCALAR(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot delete from scalar")));
  }

  if (JB_ROOT_COUNT(in) == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  it = JsonbIteratorInit(&in->root);

  while ((r = JsonbIteratorNext(&it, &v, skipNested)) != WJB_DONE)
  {
    skipNested = true;

    if ((r == WJB_ELEM || r == WJB_KEY) && (v.type == jbvString && keylen == v.val.string.len && memcmp(keyptr, v.val.string.val, keylen) == 0))
    {
                                            
      if (r == WJB_KEY)
      {
        (void)JsonbIteratorNext(&it, &v, true);
      }

      continue;
    }

    res = pushJsonbValue(&state, r, r < WJB_BEGIN_ARRAY ? &v : NULL);
  }

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                                      
   
                                                       
            
   
Datum
jsonb_delete_array(PG_FUNCTION_ARGS)
{
  Jsonb *in = PG_GETARG_JSONB_P(0);
  ArrayType *keys = PG_GETARG_ARRAYTYPE_P(1);
  Datum *keys_elems;
  bool *keys_nulls;
  int keys_len;
  JsonbParseState *state = NULL;
  JsonbIterator *it;
  JsonbValue v, *res = NULL;
  bool skipNested = false;
  JsonbIteratorToken r;

  if (ARR_NDIM(keys) > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("wrong number of array subscripts")));
  }

  if (JB_ROOT_IS_SCALAR(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot delete from scalar")));
  }

  if (JB_ROOT_COUNT(in) == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  deconstruct_array(keys, TEXTOID, -1, false, 'i', &keys_elems, &keys_nulls, &keys_len);

  if (keys_len == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  it = JsonbIteratorInit(&in->root);

  while ((r = JsonbIteratorNext(&it, &v, skipNested)) != WJB_DONE)
  {
    skipNested = true;

    if ((r == WJB_ELEM || r == WJB_KEY) && v.type == jbvString)
    {
      int i;
      bool found = false;

      for (i = 0; i < keys_len; i++)
      {
        char *keyptr;
        int keylen;

        if (keys_nulls[i])
        {
          continue;
        }

        keyptr = VARDATA_ANY(keys_elems[i]);
        keylen = VARSIZE_ANY_EXHDR(keys_elems[i]);
        if (keylen == v.val.string.len && memcmp(keyptr, v.val.string.val, keylen) == 0)
        {
          found = true;
          break;
        }
      }
      if (found)
      {
                                              
        if (r == WJB_KEY)
        {
          (void)JsonbIteratorNext(&it, &v, true);
        }

        continue;
      }
    }

    res = pushJsonbValue(&state, r, r < WJB_BEGIN_ARRAY ? &v : NULL);
  }

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                          
   
                                                      
                                                   
                     
   
Datum
jsonb_delete_idx(PG_FUNCTION_ARGS)
{
  Jsonb *in = PG_GETARG_JSONB_P(0);
  int idx = PG_GETARG_INT32(1);
  JsonbParseState *state = NULL;
  JsonbIterator *it;
  uint32 i = 0, n;
  JsonbValue v, *res = NULL;
  JsonbIteratorToken r;

  if (JB_ROOT_IS_SCALAR(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot delete from scalar")));
  }

  if (JB_ROOT_IS_OBJECT(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot delete from object using integer index")));
  }

  if (JB_ROOT_COUNT(in) == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  it = JsonbIteratorInit(&in->root);

  r = JsonbIteratorNext(&it, &v, false);
  Assert(r == WJB_BEGIN_ARRAY);
  n = v.val.array.nElems;

  if (idx < 0)
  {
    if (-idx > n)
    {
      idx = n;
    }
    else
    {
      idx = n + idx;
    }
  }

  if (idx >= n)
  {
    PG_RETURN_JSONB_P(in);
  }

  pushJsonbValue(&state, r, NULL);

  while ((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
  {
    if (r == WJB_ELEM)
    {
      if (i++ == idx)
      {
        continue;
      }
    }

    res = pushJsonbValue(&state, r, r < WJB_BEGIN_ARRAY ? &v : NULL);
  }

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                                         
   
   
Datum
jsonb_set(PG_FUNCTION_ARGS)
{
  Jsonb *in = PG_GETARG_JSONB_P(0);
  ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
  Jsonb *newval = PG_GETARG_JSONB_P(2);
  bool create = PG_GETARG_BOOL(3);
  JsonbValue *res = NULL;
  Datum *path_elems;
  bool *path_nulls;
  int path_len;
  JsonbIterator *it;
  JsonbParseState *st = NULL;

  if (ARR_NDIM(path) > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("wrong number of array subscripts")));
  }

  if (JB_ROOT_IS_SCALAR(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot set path in scalar")));
  }

  if (JB_ROOT_COUNT(in) == 0 && !create)
  {
    PG_RETURN_JSONB_P(in);
  }

  deconstruct_array(path, TEXTOID, -1, false, 'i', &path_elems, &path_nulls, &path_len);

  if (path_len == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  it = JsonbIteratorInit(&in->root);

  res = setPath(&it, path_elems, path_nulls, path_len, &st, 0, newval, create ? JB_PATH_CREATE : JB_PATH_REPLACE);

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                                 
   
Datum
jsonb_delete_path(PG_FUNCTION_ARGS)
{
  Jsonb *in = PG_GETARG_JSONB_P(0);
  ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
  JsonbValue *res = NULL;
  Datum *path_elems;
  bool *path_nulls;
  int path_len;
  JsonbIterator *it;
  JsonbParseState *st = NULL;

  if (ARR_NDIM(path) > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("wrong number of array subscripts")));
  }

  if (JB_ROOT_IS_SCALAR(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot delete path in scalar")));
  }

  if (JB_ROOT_COUNT(in) == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  deconstruct_array(path, TEXTOID, -1, false, 'i', &path_elems, &path_nulls, &path_len);

  if (path_len == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  it = JsonbIteratorInit(&in->root);

  res = setPath(&it, path_elems, path_nulls, path_len, &st, 0, NULL, JB_PATH_DELETE);

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                                            
   
   
Datum
jsonb_insert(PG_FUNCTION_ARGS)
{
  Jsonb *in = PG_GETARG_JSONB_P(0);
  ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
  Jsonb *newval = PG_GETARG_JSONB_P(2);
  bool after = PG_GETARG_BOOL(3);
  JsonbValue *res = NULL;
  Datum *path_elems;
  bool *path_nulls;
  int path_len;
  JsonbIterator *it;
  JsonbParseState *st = NULL;

  if (ARR_NDIM(path) > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR), errmsg("wrong number of array subscripts")));
  }

  if (JB_ROOT_IS_SCALAR(in))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot set path in scalar")));
  }

  deconstruct_array(path, TEXTOID, -1, false, 'i', &path_elems, &path_nulls, &path_len);

  if (path_len == 0)
  {
    PG_RETURN_JSONB_P(in);
  }

  it = JsonbIteratorInit(&in->root);

  res = setPath(&it, path_elems, path_nulls, path_len, &st, 0, newval, after ? JB_PATH_INSERT_AFTER : JB_PATH_INSERT_BEFORE);

  Assert(res != NULL);

  PG_RETURN_JSONB_P(JsonbValueToJsonb(res));
}

   
                                                           
                                                                    
                                                         
                                                                     
                  
   
static JsonbValue *
IteratorConcat(JsonbIterator **it1, JsonbIterator **it2, JsonbParseState **state)
{
  JsonbValue v1, v2, *res = NULL;
  JsonbIteratorToken r1, r2, rk1, rk2;

  rk1 = JsonbIteratorNext(it1, &v1, false);
  rk2 = JsonbIteratorNext(it2, &v2, false);

     
                                                                          
                                                                          
     
  if (rk1 == WJB_BEGIN_OBJECT && rk2 == WJB_BEGIN_OBJECT)
  {
       
                                
       
                                                                        
                                               
       
    pushJsonbValue(state, rk1, NULL);
    while ((r1 = JsonbIteratorNext(it1, &v1, true)) != WJB_END_OBJECT)
    {
      pushJsonbValue(state, r1, &v1);
    }

       
                                                                           
                                                                       
                                                               
       
    while ((r2 = JsonbIteratorNext(it2, &v2, true)) != WJB_DONE)
    {
      res = pushJsonbValue(state, r2, r2 != WJB_END_OBJECT ? &v2 : NULL);
    }
  }
  else if (rk1 == WJB_BEGIN_ARRAY && rk2 == WJB_BEGIN_ARRAY)
  {
       
                               
       
    pushJsonbValue(state, rk1, NULL);

    while ((r1 = JsonbIteratorNext(it1, &v1, true)) != WJB_END_ARRAY)
    {
      Assert(r1 == WJB_ELEM);
      pushJsonbValue(state, r1, &v1);
    }

    while ((r2 = JsonbIteratorNext(it2, &v2, true)) != WJB_END_ARRAY)
    {
      Assert(r2 == WJB_ELEM);
      pushJsonbValue(state, WJB_ELEM, &v2);
    }

    res = pushJsonbValue(state, WJB_END_ARRAY, NULL                     );
  }
  else if (rk1 == WJB_BEGIN_OBJECT)
  {
       
                                
       
    Assert(rk2 == WJB_BEGIN_ARRAY);

    pushJsonbValue(state, WJB_BEGIN_ARRAY, NULL);

    pushJsonbValue(state, WJB_BEGIN_OBJECT, NULL);
    while ((r1 = JsonbIteratorNext(it1, &v1, true)) != WJB_DONE)
    {
      pushJsonbValue(state, r1, r1 != WJB_END_OBJECT ? &v1 : NULL);
    }

    while ((r2 = JsonbIteratorNext(it2, &v2, true)) != WJB_DONE)
    {
      res = pushJsonbValue(state, r2, r2 != WJB_END_ARRAY ? &v2 : NULL);
    }
  }
  else
  {
       
                                
       
    Assert(rk1 == WJB_BEGIN_ARRAY);
    Assert(rk2 == WJB_BEGIN_OBJECT);

    pushJsonbValue(state, WJB_BEGIN_ARRAY, NULL);

    while ((r1 = JsonbIteratorNext(it1, &v1, true)) != WJB_END_ARRAY)
    {
      pushJsonbValue(state, r1, &v1);
    }

    pushJsonbValue(state, WJB_BEGIN_OBJECT, NULL);
    while ((r2 = JsonbIteratorNext(it2, &v2, true)) != WJB_DONE)
    {
      pushJsonbValue(state, r2, r2 != WJB_END_OBJECT ? &v2 : NULL);
    }

    res = pushJsonbValue(state, WJB_END_ARRAY, NULL);
  }

  return res;
}

   
                                                        
   
                                                                          
   
                                                                       
                                                                     
   
                                                                  
                                                                     
   
                                                        
                                                         
   
static JsonbValue *
setPath(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, int op_type)
{
  JsonbValue v;
  JsonbIteratorToken r;
  JsonbValue *res;

  check_stack_depth();

  if (path_nulls[level])
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("path element at position %d is null", level + 1)));
  }

  r = JsonbIteratorNext(it, &v, false);

  switch (r)
  {
  case WJB_BEGIN_ARRAY:
    (void)pushJsonbValue(st, r, NULL);
    setPathArray(it, path_elems, path_nulls, path_len, st, level, newval, v.val.array.nElems, op_type);
    r = JsonbIteratorNext(it, &v, false);
    Assert(r == WJB_END_ARRAY);
    res = pushJsonbValue(st, r, NULL);
    break;
  case WJB_BEGIN_OBJECT:
    (void)pushJsonbValue(st, r, NULL);
    setPathObject(it, path_elems, path_nulls, path_len, st, level, newval, v.val.object.nPairs, op_type);
    r = JsonbIteratorNext(it, &v, true);
    Assert(r == WJB_END_OBJECT);
    res = pushJsonbValue(st, r, NULL);
    break;
  case WJB_ELEM:
  case WJB_VALUE:
    res = pushJsonbValue(st, r, &v);
    break;
  default:
    elog(ERROR, "unrecognized iterator result: %d", (int)r);
    res = NULL;                          
    break;
  }

  return res;
}

   
                             
   
static void
setPathObject(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, uint32 npairs, int op_type)
{
  JsonbValue v;
  int i;
  JsonbValue k;
  bool done = false;

  if (level >= path_len || path_nulls[level])
  {
    done = true;
  }

                                                 
  if ((npairs == 0) && (op_type & JB_PATH_CREATE_OR_INSERT) && (level == path_len - 1))
  {
    JsonbValue newkey;

    newkey.type = jbvString;
    newkey.val.string.len = VARSIZE_ANY_EXHDR(path_elems[level]);
    newkey.val.string.val = VARDATA_ANY(path_elems[level]);

    (void)pushJsonbValue(st, WJB_KEY, &newkey);
    addJsonbToParseState(st, newval);
  }

  for (i = 0; i < npairs; i++)
  {
    JsonbIteratorToken r = JsonbIteratorNext(it, &k, true);

    Assert(r == WJB_KEY);

    if (!done && k.val.string.len == VARSIZE_ANY_EXHDR(path_elems[level]) && memcmp(k.val.string.val, VARDATA_ANY(path_elems[level]), k.val.string.len) == 0)
    {
      if (level == path_len - 1)
      {
           
                                                                
                          
           
        if (op_type & (JB_PATH_INSERT_BEFORE | JB_PATH_INSERT_AFTER))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot replace existing key"),
                             errhint("Try using the function jsonb_set "
                                     "to replace key value.")));
        }

        r = JsonbIteratorNext(it, &v, true);                 
        if (!(op_type & JB_PATH_DELETE))
        {
          (void)pushJsonbValue(st, WJB_KEY, &k);
          addJsonbToParseState(st, newval);
        }
        done = true;
      }
      else
      {
        (void)pushJsonbValue(st, r, &k);
        setPath(it, path_elems, path_nulls, path_len, st, level + 1, newval, op_type);
      }
    }
    else
    {
      if ((op_type & JB_PATH_CREATE_OR_INSERT) && !done && level == path_len - 1 && i == npairs - 1)
      {
        JsonbValue newkey;

        newkey.type = jbvString;
        newkey.val.string.len = VARSIZE_ANY_EXHDR(path_elems[level]);
        newkey.val.string.val = VARDATA_ANY(path_elems[level]);

        (void)pushJsonbValue(st, WJB_KEY, &newkey);
        addJsonbToParseState(st, newval);
      }

      (void)pushJsonbValue(st, r, &k);
      r = JsonbIteratorNext(it, &v, false);
      (void)pushJsonbValue(st, r, r < WJB_BEGIN_ARRAY ? &v : NULL);
      if (r == WJB_BEGIN_ARRAY || r == WJB_BEGIN_OBJECT)
      {
        int walking_level = 1;

        while (walking_level != 0)
        {
          r = JsonbIteratorNext(it, &v, false);

          if (r == WJB_BEGIN_ARRAY || r == WJB_BEGIN_OBJECT)
          {
            ++walking_level;
          }
          if (r == WJB_END_ARRAY || r == WJB_END_OBJECT)
          {
            --walking_level;
          }

          (void)pushJsonbValue(st, r, r < WJB_BEGIN_ARRAY ? &v : NULL);
        }
      }
    }
  }
}

   
                            
   
static void
setPathArray(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, uint32 nelems, int op_type)
{
  JsonbValue v;
  int idx, i;
  bool done = false;

                          
  if (level < path_len && !path_nulls[level])
  {
    char *c = TextDatumGetCString(path_elems[level]);
    long lindex;
    char *badp;

    errno = 0;
    lindex = strtol(c, &badp, 10);
    if (errno != 0 || badp == c || *badp != '\0' || lindex > INT_MAX || lindex < INT_MIN)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("path element at position %d is not an integer: \"%s\"", level + 1, c)));
    }
    idx = lindex;
  }
  else
  {
    idx = nelems;
  }

  if (idx < 0)
  {
    if (-idx > nelems)
    {
      idx = INT_MIN;
    }
    else
    {
      idx = nelems + idx;
    }
  }

  if (idx > 0 && idx > nelems)
  {
    idx = nelems;
  }

     
                                                                            
                                                                           
                           
     

  if ((idx == INT_MIN || nelems == 0) && (level == path_len - 1) && (op_type & JB_PATH_CREATE_OR_INSERT))
  {
    Assert(newval != NULL);
    addJsonbToParseState(st, newval);
    done = true;
  }

                                       
  for (i = 0; i < nelems; i++)
  {
    JsonbIteratorToken r;

    if (i == idx && level < path_len)
    {
      if (level == path_len - 1)
      {
        r = JsonbIteratorNext(it, &v, true);           

        if (op_type & (JB_PATH_INSERT_BEFORE | JB_PATH_CREATE))
        {
          addJsonbToParseState(st, newval);
        }

           
                                                        
                                                                 
                                                      
           
        if (op_type & (JB_PATH_INSERT_AFTER | JB_PATH_INSERT_BEFORE))
        {
          (void)pushJsonbValue(st, r, &v);
        }

        if (op_type & (JB_PATH_INSERT_AFTER | JB_PATH_REPLACE))
        {
          addJsonbToParseState(st, newval);
        }

        done = true;
      }
      else
      {
        (void)setPath(it, path_elems, path_nulls, path_len, st, level + 1, newval, op_type);
      }
    }
    else
    {
      r = JsonbIteratorNext(it, &v, false);

      (void)pushJsonbValue(st, r, r < WJB_BEGIN_ARRAY ? &v : NULL);

      if (r == WJB_BEGIN_ARRAY || r == WJB_BEGIN_OBJECT)
      {
        int walking_level = 1;

        while (walking_level != 0)
        {
          r = JsonbIteratorNext(it, &v, false);

          if (r == WJB_BEGIN_ARRAY || r == WJB_BEGIN_OBJECT)
          {
            ++walking_level;
          }
          if (r == WJB_END_ARRAY || r == WJB_END_OBJECT)
          {
            --walking_level;
          }

          (void)pushJsonbValue(st, r, r < WJB_BEGIN_ARRAY ? &v : NULL);
        }
      }

      if ((op_type & JB_PATH_CREATE_OR_INSERT) && !done && level == path_len - 1 && i == nelems - 1)
      {
        addJsonbToParseState(st, newval);
      }
    }
  }
}

   
                                                                                
                                                                               
                                                            
   
uint32
parse_jsonb_index_flags(Jsonb *jb)
{
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken type;
  uint32 flags = 0;

  it = JsonbIteratorInit(&jb->root);

  type = JsonbIteratorNext(&it, &v, false);

     
                                                                           
                                                                       
                                           
     
  if (type != WJB_BEGIN_ARRAY)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("wrong flag type, only arrays and scalars are allowed")));
  }

  while ((type = JsonbIteratorNext(&it, &v, false)) == WJB_ELEM)
  {
    if (v.type != jbvString)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("flag array element is not a string"), errhint("Possible values are: \"string\", \"numeric\", \"boolean\", \"key\", and \"all\".")));
    }

    if (v.val.string.len == 3 && pg_strncasecmp(v.val.string.val, "all", 3) == 0)
    {
      flags |= jtiAll;
    }
    else if (v.val.string.len == 3 && pg_strncasecmp(v.val.string.val, "key", 3) == 0)
    {
      flags |= jtiKey;
    }
    else if (v.val.string.len == 6 && pg_strncasecmp(v.val.string.val, "string", 5) == 0)
    {
      flags |= jtiString;
    }
    else if (v.val.string.len == 7 && pg_strncasecmp(v.val.string.val, "numeric", 7) == 0)
    {
      flags |= jtiNumeric;
    }
    else if (v.val.string.len == 7 && pg_strncasecmp(v.val.string.val, "boolean", 7) == 0)
    {
      flags |= jtiBool;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("wrong flag in flag array: \"%s\"", pnstrdup(v.val.string.val, v.val.string.len)), errhint("Possible values are: \"string\", \"numeric\", \"boolean\", \"key\", and \"all\".")));
    }
  }

                               
  if (type != WJB_END_ARRAY)
  {
    elog(ERROR, "unexpected end of flag array");
  }

                                            
  type = JsonbIteratorNext(&it, &v, false);
  if (type != WJB_DONE)
  {
    elog(ERROR, "unexpected end of flag array");
  }

  return flags;
}

   
                                                                            
                                                                                  
   
void
iterate_jsonb_values(Jsonb *jb, uint32 flags, void *state, JsonIterateStringValuesAction action)
{
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken type;

  it = JsonbIteratorInit(&jb->root);

     
                                                                    
                            
     
  while ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    if (type == WJB_KEY)
    {
      if (flags & jtiKey)
      {
        action(state, v.val.string.val, v.val.string.len);
      }

      continue;
    }
    else if (!(type == WJB_VALUE || type == WJB_ELEM))
    {
                                                         
      continue;
    }

                                                             
    switch (v.type)
    {
    case jbvString:
      if (flags & jtiString)
      {
        action(state, v.val.string.val, v.val.string.len);
      }
      break;
    case jbvNumeric:
      if (flags & jtiNumeric)
      {
        char *val;

        val = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(v.val.numeric)));

        action(state, val, strlen(val));
        pfree(val);
      }
      break;
    case jbvBool:
      if (flags & jtiBool)
      {
        if (v.val.boolean)
        {
          action(state, "true", 4);
        }
        else
        {
          action(state, "false", 5);
        }
      }
      break;
    default:
                                                         
      break;
    }
  }
}

   
                                                                            
                                                                                  
   
void
iterate_json_values(text *json, uint32 flags, void *action_state, JsonIterateStringValuesAction action)
{
  JsonLexContext *lex = makeJsonLexContext(json, true);
  JsonSemAction *sem = palloc0(sizeof(JsonSemAction));
  IterateJsonStringValuesState *state = palloc0(sizeof(IterateJsonStringValuesState));

  state->lex = lex;
  state->action = action;
  state->action_state = action_state;
  state->flags = flags;

  sem->semstate = (void *)state;
  sem->scalar = iterate_values_scalar;
  sem->object_field_start = iterate_values_object_field_start;

  pg_parse_json(lex, sem);
}

   
                                                                       
                                                       
   
static void
iterate_values_scalar(void *state, char *token, JsonTokenType tokentype)
{
  IterateJsonStringValuesState *_state = (IterateJsonStringValuesState *)state;

  switch (tokentype)
  {
  case JSON_TOKEN_STRING:
    if (_state->flags & jtiString)
    {
      _state->action(_state->action_state, token, strlen(token));
    }
    break;
  case JSON_TOKEN_NUMBER:
    if (_state->flags & jtiNumeric)
    {
      _state->action(_state->action_state, token, strlen(token));
    }
    break;
  case JSON_TOKEN_TRUE:
  case JSON_TOKEN_FALSE:
    if (_state->flags & jtiBool)
    {
      _state->action(_state->action_state, token, strlen(token));
    }
    break;
  default:
                                                  
    break;
  }
}

static void
iterate_values_object_field_start(void *state, char *fname, bool isnull)
{
  IterateJsonStringValuesState *_state = (IterateJsonStringValuesState *)state;

  if (_state->flags & jtiKey)
  {
    char *val = pstrdup(fname);

    _state->action(_state->action_state, val, strlen(val));
  }
}

   
                                                                               
                                                                 
                                                                               
                                                                                
   
Jsonb *
transform_jsonb_string_values(Jsonb *jsonb, void *action_state, JsonTransformStringValuesAction transform_action)
{
  JsonbIterator *it;
  JsonbValue v, *res = NULL;
  JsonbIteratorToken type;
  JsonbParseState *st = NULL;
  text *out;
  bool is_scalar = false;

  it = JsonbIteratorInit(&jsonb->root);
  is_scalar = it->isScalar;

  while ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    if ((type == WJB_VALUE || type == WJB_ELEM) && v.type == jbvString)
    {
      out = transform_action(action_state, v.val.string.val, v.val.string.len);
      v.val.string.val = VARDATA_ANY(out);
      v.val.string.len = VARSIZE_ANY_EXHDR(out);
      res = pushJsonbValue(&st, type, type < WJB_BEGIN_ARRAY ? &v : NULL);
    }
    else
    {
      res = pushJsonbValue(&st, type, (type == WJB_KEY || type == WJB_VALUE || type == WJB_ELEM) ? &v : NULL);
    }
  }

  if (res->type == jbvArray)
  {
    res->val.array.rawScalar = is_scalar;
  }

  return JsonbValueToJsonb(res);
}

   
                                                                              
                                                                 
                                                                               
                                                                           
                       
   
text *
transform_json_string_values(text *json, void *action_state, JsonTransformStringValuesAction transform_action)
{
  JsonLexContext *lex = makeJsonLexContext(json, true);
  JsonSemAction *sem = palloc0(sizeof(JsonSemAction));
  TransformJsonStringValuesState *state = palloc0(sizeof(TransformJsonStringValuesState));

  state->lex = lex;
  state->strval = makeStringInfo();
  state->action = transform_action;
  state->action_state = action_state;

  sem->semstate = (void *)state;
  sem->scalar = transform_string_values_scalar;
  sem->object_start = transform_string_values_object_start;
  sem->object_end = transform_string_values_object_end;
  sem->array_start = transform_string_values_array_start;
  sem->array_end = transform_string_values_array_end;
  sem->scalar = transform_string_values_scalar;
  sem->array_element_start = transform_string_values_array_element_start;
  sem->object_field_start = transform_string_values_object_field_start;

  pg_parse_json(lex, sem);

  return cstring_to_text_with_len(state->strval->data, state->strval->len);
}

   
                                                                           
                                                                                
                   
   
static void
transform_string_values_object_start(void *state)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  appendStringInfoCharMacro(_state->strval, '{');
}

static void
transform_string_values_object_end(void *state)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  appendStringInfoCharMacro(_state->strval, '}');
}

static void
transform_string_values_array_start(void *state)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  appendStringInfoCharMacro(_state->strval, '[');
}

static void
transform_string_values_array_end(void *state)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  appendStringInfoCharMacro(_state->strval, ']');
}

static void
transform_string_values_object_field_start(void *state, char *fname, bool isnull)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  if (_state->strval->data[_state->strval->len - 1] != '{')
  {
    appendStringInfoCharMacro(_state->strval, ',');
  }

     
                                                                            
                              
     
  escape_json(_state->strval, fname);
  appendStringInfoCharMacro(_state->strval, ':');
}

static void
transform_string_values_array_element_start(void *state, bool isnull)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  if (_state->strval->data[_state->strval->len - 1] != '[')
  {
    appendStringInfoCharMacro(_state->strval, ',');
  }
}

static void
transform_string_values_scalar(void *state, char *token, JsonTokenType tokentype)
{
  TransformJsonStringValuesState *_state = (TransformJsonStringValuesState *)state;

  if (tokentype == JSON_TOKEN_STRING)
  {
    text *out = _state->action(_state->action_state, token, strlen(token));

    escape_json(_state->strval, text_to_cstring(out));
  }
  else
  {
    appendStringInfoString(_state->strval, token);
  }
}
