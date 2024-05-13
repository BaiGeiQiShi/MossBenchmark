/*-------------------------------------------------------------------------
 *
 * jsonfuncs.c
 *		Functions to process JSON data types.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonfuncs.c
 *
 *-------------------------------------------------------------------------
 */

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

/* Operations available for setPath */
#define JB_PATH_CREATE 0x0001
#define JB_PATH_DELETE 0x0002
#define JB_PATH_REPLACE 0x0004
#define JB_PATH_INSERT_BEFORE 0x0008
#define JB_PATH_INSERT_AFTER 0x0010
#define JB_PATH_CREATE_OR_INSERT (JB_PATH_INSERT_BEFORE | JB_PATH_INSERT_AFTER | JB_PATH_CREATE)

/* state for json_object_keys */
typedef struct OkeysState
{
  JsonLexContext *lex;
  char **result;
  int result_size;
  int result_count;
  int sent_count;
} OkeysState;

/* state for iterate_json_values function */
typedef struct IterateJsonStringValuesState
{
  JsonLexContext *lex;
  JsonIterateStringValuesAction action; /* an action that will be applied
                                         * to each json value */
  void *action_state;                   /* any necessary context for iteration */
  uint32 flags;                         /* what kind of elements from a json we want
                                         * to iterate */
} IterateJsonStringValuesState;

/* state for transform_json_string_values function */
typedef struct TransformJsonStringValuesState
{
  JsonLexContext *lex;
  StringInfo strval;                      /* resulting json */
  JsonTransformStringValuesAction action; /* an action that will be applied
                                           * to each json value */
  void *action_state;                     /* any necessary context for transformation */
} TransformJsonStringValuesState;

/* state for json_get* functions */
typedef struct GetState
{
  JsonLexContext *lex;
  text *tresult;
  char *result_start;
  bool normalize_results;
  bool next_scalar;
  int npath;            /* length of each path-related array */
  char **path_names;    /* field name(s) being sought */
  int *path_indexes;    /* array index(es) being sought */
  bool *pathok;         /* is path matched to current depth? */
  int *array_cur_index; /* current element index at each path
                         * level */
} GetState;

/* state for json_array_length */
typedef struct AlenState
{
  JsonLexContext *lex;
  int count;
} AlenState;

/* state for json_each */
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

/* state for json_array_elements */
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

/* state for get_json_object_as_hash */
typedef struct JHashState
{
  JsonLexContext *lex;
  const char *function_name;
  HTAB *hash;
  char *saved_scalar;
  char *save_json_start;
  JsonTokenType saved_token_type;
} JHashState;

/* hashtable element */
typedef struct JsonHashEntry
{
  char fname[NAMEDATALEN]; /* hash key (MUST BE FIRST) */
  char *val;
  JsonTokenType type;
} JsonHashEntry;

/* structure to cache type I/O metadata needed for populate_scalar() */
typedef struct ScalarIOData
{
  Oid typioparam;
  FmgrInfo typiofunc;
} ScalarIOData;

/* these two structures are used recursively */
typedef struct ColumnIOData ColumnIOData;
typedef struct RecordIOData RecordIOData;

/* structure to cache metadata needed for populate_array() */
typedef struct ArrayIOData
{
  ColumnIOData *element_info; /* metadata cache */
  Oid element_type;           /* array element type id */
  int32 element_typmod;       /* array element type modifier */
} ArrayIOData;

/* structure to cache metadata needed for populate_composite() */
typedef struct CompositeIOData
{
  /*
   * We use pointer to a RecordIOData here because variable-length struct
   * RecordIOData can't be used directly in ColumnIOData.io union
   */
  RecordIOData *record_io; /* metadata cache for populate_record() */
  TupleDesc tupdesc;       /* cached tuple descriptor */
  /* these fields differ from target type only if domain over composite: */
  Oid base_typid;    /* base type id */
  int32 base_typmod; /* base type modifier */
  /* this field is used only if target type is domain over composite: */
  void *domain_info; /* opaque cache for domain checks */
} CompositeIOData;

/* structure to cache metadata needed for populate_domain() */
typedef struct DomainIOData
{
  ColumnIOData *base_io; /* metadata cache */
  Oid base_typid;        /* base type id */
  int32 base_typmod;     /* base type modifier */
  void *domain_info;     /* opaque cache for domain checks */
} DomainIOData;

/* enumeration type categories */
typedef enum TypeCat
{
  TYPECAT_SCALAR = 's',
  TYPECAT_ARRAY = 'a',
  TYPECAT_COMPOSITE = 'c',
  TYPECAT_COMPOSITE_DOMAIN = 'C',
  TYPECAT_DOMAIN = 'd'
} TypeCat;

/* these two are stolen from hstore / record_out, used in populate_record* */

/* structure to cache record metadata needed for populate_record_field() */
struct ColumnIOData
{
  Oid typid;              /* column type id */
  int32 typmod;           /* column type modifier */
  TypeCat typcat;         /* column type category */
  ScalarIOData scalar_io; /* metadata cache for directi conversion
                           * through input function */
  union
  {
    ArrayIOData array;
    CompositeIOData composite;
    DomainIOData domain;
  } io; /* metadata cache for various column type
         * categories */
};

/* structure to cache record metadata needed for populate_record() */
struct RecordIOData
{
  Oid record_type;
  int32 record_typmod;
  int ncolumns;
  ColumnIOData columns[FLEXIBLE_ARRAY_MEMBER];
};

/* per-query cache for populate_record_worker and populate_recordset_worker */
typedef struct PopulateRecordCache
{
  Oid argtype;           /* declared type of the record argument */
  ColumnIOData c;        /* metadata cache for populate_composite() */
  MemoryContext fn_mcxt; /* where this is stored */
} PopulateRecordCache;

/* per-call state for populate_recordset */
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

/* common data for populate_array_json() and populate_array_dim_jsonb() */
typedef struct PopulateArrayContext
{
  ArrayBuildState *astate; /* array build state */
  ArrayIOData *aio;        /* metadata cache */
  MemoryContext acxt;      /* array build memory context */
  MemoryContext mcxt;      /* cache memory context */
  const char *colname;     /* for diagnostics only */
  int *dims;               /* dimensions */
  int *sizes;              /* current dimension counters */
  int ndims;               /* number of dimensions */
} PopulateArrayContext;

/* state for populate_array_json() */
typedef struct PopulateArrayState
{
  JsonLexContext *lex;        /* json lexer */
  PopulateArrayContext *ctx;  /* context */
  char *element_start;        /* start of the current array element */
  char *element_scalar;       /* current array element token if it is a
                               * scalar */
  JsonTokenType element_type; /* current array element type */
} PopulateArrayState;

/* state for json_strip_nulls */
typedef struct StripnullState
{
  JsonLexContext *lex;
  StringInfo strval;
  bool skip_next_null;
} StripnullState;

/* structure for generalized json/jsonb value passing */
typedef struct JsValue
{
  bool is_json; /* json/jsonb */
  union
  {
    struct
    {
      char *str;          /* json string */
      int len;            /* json string length or -1 if null-terminated */
      JsonTokenType type; /* json type */
    } json;               /* json value */

    JsonbValue *jsonb; /* jsonb value */
  } val;
} JsValue;

typedef struct JsObject
{
  bool is_json; /* json/jsonb */
  union
  {
    HTAB *json_hash;
    JsonbContainer *jsonb_cont;
  } val;
} JsObject;

/* useful macros for testing JsValue properties */
#define JsValueIsNull(jsv) ((jsv)->is_json ? (!(jsv)->val.json.str || (jsv)->val.json.type == JSON_TOKEN_NULL) : (!(jsv)->val.jsonb || (jsv)->val.jsonb->type == jbvNull))

#define JsValueIsString(jsv) ((jsv)->is_json ? (jsv)->val.json.type == JSON_TOKEN_STRING : ((jsv)->val.jsonb && (jsv)->val.jsonb->type == jbvString))

#define JsObjectIsEmpty(jso) ((jso)->is_json ? hash_get_num_entries((jso)->val.json_hash) == 0 : ((jso)->val.jsonb_cont == NULL || JsonContainerSize((jso)->val.jsonb_cont) == 0))

#define JsObjectFree(jso)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((jso)->is_json)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      hash_destroy((jso)->val.json_hash);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  } while (0)

/* semantic action functions for json_object_keys */
static void
okeys_object_field_start(void *state, char *fname, bool isnull);
static void
okeys_array_start(void *state);
static void
okeys_scalar(void *state, char *token, JsonTokenType tokentype);

/* semantic action functions for json_get* functions */
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

/* common worker function for json getter functions */
static Datum
get_path_all(FunctionCallInfo fcinfo, bool as_text);
static text *
get_worker(text *json, char **tpath, int *ipath, int npath, bool normalize_results);
static Datum
get_jsonb_path_all(FunctionCallInfo fcinfo, bool as_text);

/* semantic action functions for json_array_length */
static void
alen_object_start(void *state);
static void
alen_scalar(void *state, char *token, JsonTokenType tokentype);
static void
alen_array_element_start(void *state, bool isnull);

/* common workers for json{b}_each* functions */
static Datum
each_worker(FunctionCallInfo fcinfo, bool as_text);
static Datum
each_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text);

/* semantic action functions for json_each */
static void
each_object_field_start(void *state, char *fname, bool isnull);
static void
each_object_field_end(void *state, char *fname, bool isnull);
static void
each_array_start(void *state);
static void
each_scalar(void *state, char *token, JsonTokenType tokentype);

/* common workers for json{b}_array_elements_* functions */
static Datum
elements_worker(FunctionCallInfo fcinfo, const char *funcname, bool as_text);
static Datum
elements_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text);

/* semantic action functions for json_array_elements */
static void
elements_object_start(void *state);
static void
elements_array_element_start(void *state, bool isnull);
static void
elements_array_element_end(void *state, bool isnull);
static void
elements_scalar(void *state, char *token, JsonTokenType tokentype);

/* turn a json object into a hash table */
static HTAB *
get_json_object_as_hash(char *json, int len, const char *funcname);

/* semantic actions for populate_array_json */
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

/* semantic action functions for get_json_object_as_hash */
static void
hash_object_field_start(void *state, char *fname, bool isnull);
static void
hash_object_field_end(void *state, char *fname, bool isnull);
static void
hash_array_start(void *state);
static void
hash_scalar(void *state, char *token, JsonTokenType tokentype);

/* semantic action functions for populate_recordset */
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

/* semantic action functions for json_strip_nulls */
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

/* worker functions for populate_record, to_record, populate_recordset and
 * to_recordset */
static Datum
populate_recordset_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg);
static Datum
populate_record_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg);

/* helper functions for populate_record[set] */
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

/* Worker that takes care of common setup for us */
static JsonbValue *
findJsonbValueFromContainerLen(JsonbContainer *container, uint32 flags, char *key, uint32 keylen);

/* functions supporting jsonb_delete, jsonb_set and jsonb_concat */
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

/* function supporting iterate_json_values */
static void
iterate_values_scalar(void *state, char *token, JsonTokenType tokentype);
static void
iterate_values_object_field_start(void *state, char *fname, bool isnull);

/* functions supporting transform_json_string_values */
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

/*
 * SQL function json_object_keys
 *
 * Returns the set of keys for the object argument.
 *
 * This SRF operates in value-per-call mode. It processes the
 * object during the first call, and the keys are simply stashed
 * in an array, whose size is expanded as necessary. This is probably
 * safe enough for a list of keys of a single object, since they are
 * limited in size to NAMEDATALEN and the number of keys is unlikely to
 * be so huge that it has major memory implications.
 */
Datum
jsonb_object_keys(PG_FUNCTION_ARGS)
{








































































}

Datum
json_object_keys(PG_FUNCTION_ARGS)
{




























































}

static void
okeys_object_field_start(void *state, char *fname, bool isnull)
{

















}

static void
okeys_array_start(void *state)
{







}

static void
okeys_scalar(void *state, char *token, JsonTokenType tokentype)
{







}

/*
 * json and jsonb getter functions
 * these implement the -> ->> #> and #>> operators
 * and the json{b?}_extract_path*(json, text, ...) functions
 */

Datum
json_object_field(PG_FUNCTION_ARGS)
{















}

Datum
jsonb_object_field(PG_FUNCTION_ARGS)
{

















}

Datum
json_object_field_text(PG_FUNCTION_ARGS)
{















}

Datum
jsonb_object_field_text(PG_FUNCTION_ARGS)
{















































}

Datum
json_array_element(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_array_element(PG_FUNCTION_ARGS)
{































}

Datum
json_array_element_text(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_array_element_text(PG_FUNCTION_ARGS)
{





























































}

Datum
json_extract_path(PG_FUNCTION_ARGS)
{

}

Datum
json_extract_path_text(PG_FUNCTION_ARGS)
{

}

/*
 * common routine for extract_path functions
 */
static Datum
get_path_all(FunctionCallInfo fcinfo, bool as_text)
{





































































}

/*
 * get_worker
 *
 * common worker for all the json getter functions
 *
 * json: JSON object (in text form)
 * tpath[]: field name(s) to extract
 * ipath[]: array index(es) (zero-based) to extract, accepts negatives
 * npath: length of tpath[] and/or ipath[]
 * normalize_results: true to de-escape string and null scalars
 *
 * tpath can be NULL, or any one tpath[] entry can be NULL, if an object
 * field is not to be matched at that nesting level.  Similarly, ipath can
 * be NULL, or any one ipath[] entry can be INT_MIN if an array element is
 * not to be matched at that nesting level (a json datum should never be
 * large enough to have -INT_MIN elements due to MaxAllocSize restriction).
 */
static text *
get_worker(text *json, char **tpath, int *ipath, int npath, bool normalize_results)
{

















































}

static void
get_object_start(void *state)
{












}

static void
get_object_end(void *state)
{











}

static void
get_object_field_start(void *state, char *fname, bool isnull)
{



































}

static void
get_object_field_end(void *state, char *fname, bool isnull)
{










































}

static void
get_array_start(void *state)
{





























}

static void
get_array_end(void *state)
{











}

static void
get_array_element_start(void *state, bool isnull)
{







































}

static void
get_array_element_end(void *state, bool isnull)
{




































}

static void
get_scalar(void *state, char *token, JsonTokenType tokentype)
{




































}

Datum
jsonb_extract_path(PG_FUNCTION_ARGS)
{

}

Datum
jsonb_extract_path_text(PG_FUNCTION_ARGS)
{

}

static Datum
get_jsonb_path_all(FunctionCallInfo fcinfo, bool as_text)
{













































































































































































}

/*
 * SQL function json_array_length(json) -> int
 */
Datum
json_array_length(PG_FUNCTION_ARGS)
{























}

Datum
jsonb_array_length(PG_FUNCTION_ARGS)
{












}

/*
 * These next two checks ensure that the json is an array (since it can't be
 * a scalar or an object).
 */

static void
alen_object_start(void *state)
{







}

static void
alen_scalar(void *state, char *token, JsonTokenType tokentype)
{







}

static void
alen_array_element_start(void *state, bool isnull)
{







}

/*
 * SQL function json_each and json_each_text
 *
 * decompose a json object into key value pairs.
 *
 * Unlike json_object_keys() these SRFs operate in materialize mode,
 * stashing results into a Tuplestore object as they go.
 * The construction of tuples is done using a temporary memory context
 * that is cleared out after each tuple is built.
 */
Datum
json_each(PG_FUNCTION_ARGS)
{

}

Datum
jsonb_each(PG_FUNCTION_ARGS)
{

}

Datum
json_each_text(PG_FUNCTION_ARGS)
{

}

Datum
jsonb_each_text(PG_FUNCTION_ARGS)
{

}

static Datum
each_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text)
{

























































































































}

static Datum
each_worker(FunctionCallInfo fcinfo, bool as_text)
{



















































}

static void
each_object_field_start(void *state, char *fname, bool isnull)
{



















}

static void
each_object_field_end(void *state, char *fname, bool isnull)
{











































}

static void
each_array_start(void *state)
{







}

static void
each_scalar(void *state, char *token, JsonTokenType tokentype)
{













}

/*
 * SQL functions json_array_elements and json_array_elements_text
 *
 * get the elements from a json array
 *
 * a lot of this processing is similar to the json_each* functions
 */

Datum
jsonb_array_elements(PG_FUNCTION_ARGS)
{

}

Datum
jsonb_array_elements_text(PG_FUNCTION_ARGS)
{

}

static Datum
elements_worker_jsonb(FunctionCallInfo fcinfo, const char *funcname, bool as_text)
{














































































































}

Datum
json_array_elements(PG_FUNCTION_ARGS)
{

}

Datum
json_array_elements_text(PG_FUNCTION_ARGS)
{

}

static Datum
elements_worker(FunctionCallInfo fcinfo, const char *funcname, bool as_text)
{






















































}

static void
elements_array_element_start(void *state, bool isnull)
{



















}

static void
elements_array_element_end(void *state, bool isnull)
{









































}

static void
elements_object_start(void *state)
{







}

static void
elements_scalar(void *state, char *token, JsonTokenType tokentype)
{













}

/*
 * SQL function json_populate_record
 *
 * set fields in a record from the argument json
 *
 * Code adapted shamelessly from hstore's populate_record
 * which is in turn partly adapted from record_out.
 *
 * The json is decomposed into a hash table, in which each
 * field in the record is then looked up by name. For jsonb
 * we fetch the values direct from the object.
 */
Datum
jsonb_populate_record(PG_FUNCTION_ARGS)
{

}

Datum
jsonb_to_record(PG_FUNCTION_ARGS)
{

}

Datum
json_populate_record(PG_FUNCTION_ARGS)
{

}

Datum
json_to_record(PG_FUNCTION_ARGS)
{

}

/* helper function for diagnostics */
static void
populate_array_report_expected_array(PopulateArrayContext *ctx, int ndim)
{


































}

/* set the number of dimensions of the populated array when it becomes known */
static void
populate_array_assign_ndims(PopulateArrayContext *ctx, int ndims)
{

















}

/* check the populated subarray dimension */
static void
populate_array_check_dimension(PopulateArrayContext *ctx, int ndim)
{



















}

static void
populate_array_element(PopulateArrayContext *ctx, int ndim, JsValue *jsv)
{










}

/* json object start handler for populate_array_json() */
static void
populate_array_object_start(void *_state)
{











}

/* json array end handler for populate_array_json() */
static void
populate_array_array_end(void *_state)
{













}

/* json array element start handler for populate_array_json() */
static void
populate_array_element_start(void *_state, bool isnull)
{










}

/* json array element end handler for populate_array_json() */
static void
populate_array_element_end(void *_state, bool isnull)
{
































}

/* json scalar handler for populate_array_json() */
static void
populate_array_scalar(void *_state, char *token, JsonTokenType tokentype)
{




















}

/* parse a json array and populate array */
static void
populate_array_json(PopulateArrayContext *ctx, char *json, int len)
{




















}

/*
 * populate_array_dim_jsonb() -- Iterate recursively through jsonb sub-array
 *		elements and accumulate result using given ArrayBuildState.
 */
static void
populate_array_dim_jsonb(PopulateArrayContext *ctx, /* context */
    JsonbValue *jbv,                                /* jsonb sub-array */
    int ndim)                                       /* current dimension */
{

































































}

/* recursively populate an array from json/jsonb */
static Datum
populate_array(ArrayIOData *aio, const char *colname, MemoryContext mcxt, JsValue *jsv)
{








































}

static void
JsValueToJsObject(JsValue *jsv, JsObject *jso)
{























}

/* acquire or update cached tuple descriptor for a composite type */
static void
update_cached_tupdesc(CompositeIOData *io, MemoryContext mcxt)
{

















}

/* recursively populate a composite (row type) value from json/jsonb */
static Datum
populate_composite(CompositeIOData *io, Oid typid, const char *colname, MemoryContext mcxt, HeapTupleHeader defaultval, JsValue *jsv, bool isnull)
{



































}

/* populate non-null scalar value from json/jsonb value */
static Datum
populate_scalar(ScalarIOData *io, Oid typid, int32 typmod, JsValue *jsv)
{

























































































}

static Datum
populate_domain(DomainIOData *io, Oid typid, const char *colname, MemoryContext mcxt, JsValue *jsv, bool isnull)
{















}

/* prepare column metadata cache for the given type */
static void
prepare_column_cache(ColumnIOData *column, Oid typid, int32 typmod, MemoryContext mcxt, bool need_scalar)
{













































































}

/* recursively populate a record field or an array element from a json/jsonb
 * value */
static Datum
populate_record_field(ColumnIOData *col, Oid typid, int32 typmod, const char *colname, MemoryContext mcxt, Datum defaultval, JsValue *jsv, bool *isnull)
{
















































}

static RecordIOData *
allocate_record_info(MemoryContext mcxt, int ncolumns)
{








}

static bool
JsObjectGetField(JsObject *obj, char *field, JsValue *jsv)
{


















}

/* populate a record tuple from json/jsonb value */
static HeapTupleHeader
populate_record(TupleDesc tupdesc, RecordIOData **record_p, HeapTupleHeader defaultval, MemoryContext mcxt, JsObject *obj)
{































































































}

/*
 * Setup for json{b}_populate_record{set}: result type will be same as first
 * argument's type --- unless first argument is "null::record", which we can't
 * extract type info from; we handle that later.
 */
static void
get_record_type_from_argument(FunctionCallInfo fcinfo, const char *funcname, PopulateRecordCache *cache)
{






}

/*
 * Setup for json{b}_to_record{set}: result type is specified by calling
 * query.  We'll also use this code for json{b}_populate_record{set},
 * if we discover that the first argument is a null of type RECORD.
 *
 * Here it is syntactically impossible to specify the target type
 * as domain-over-composite.
 */
static void
get_record_type_from_query(FunctionCallInfo fcinfo, const char *funcname, PopulateRecordCache *cache)
{























}

/*
 * common worker for json{b}_populate_record() and json{b}_to_record()
 * is_json and have_record_arg identify the specific function
 */
static Datum
populate_record_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg)
{






































































































}

/*
 * get_json_object_as_hash
 *
 * decompose a json object into a hash table.
 */
static HTAB *
get_json_object_as_hash(char *json, int len, const char *funcname)
{




























}

static void
hash_object_field_start(void *state, char *fname, bool isnull)
{




















}

static void
hash_object_field_end(void *state, char *fname, bool isnull)
{
















































}

static void
hash_array_start(void *state)
{






}

static void
hash_scalar(void *state, char *token, JsonTokenType tokentype)
{













}

/*
 * SQL function json_populate_recordset
 *
 * set fields in a set of records from the argument json,
 * which must be an array of objects.
 *
 * similar to json_populate_record, but the tuple-building code
 * is pushed down into the semantic action handlers so it's done
 * per object in the array.
 */
Datum
jsonb_populate_recordset(PG_FUNCTION_ARGS)
{

}

Datum
jsonb_to_recordset(PG_FUNCTION_ARGS)
{

}

Datum
json_populate_recordset(PG_FUNCTION_ARGS)
{

}

Datum
json_to_recordset(PG_FUNCTION_ARGS)
{

}

static void
populate_recordset_record(PopulateRecordsetState *state, JsObject *obj)
{























}

/*
 * common worker for json{b}_populate_recordset() and json{b}_to_recordset()
 * is_json and have_record_arg identify the specific function
 */
static Datum
populate_recordset_worker(FunctionCallInfo fcinfo, const char *funcname, bool is_json, bool have_record_arg)
{


































































































































































}

static void
populate_recordset_object_start(void *state)
{






















}

static void
populate_recordset_object_end(void *state)
{


















}

static void
populate_recordset_array_element_start(void *state, bool isnull)
{






}

static void
populate_recordset_array_start(void *state)
{
}

static void
populate_recordset_scalar(void *state, char *token, JsonTokenType tokentype)
{











}

static void
populate_recordset_object_field_start(void *state, char *fname, bool isnull)
{

















}

static void
populate_recordset_object_field_end(void *state, char *fname, bool isnull)
{
















































}

/*
 * findJsonbValueFromContainer() wrapper that sets up JsonbValue key string.
 */
static JsonbValue *
findJsonbValueFromContainerLen(JsonbContainer *container, uint32 flags, char *key, uint32 keylen)
{







}

/*
 * Semantic actions for json_strip_nulls.
 *
 * Simply repeat the input on the output unless we encounter
 * a null object field. State for this is set when the field
 * is started and reset when the scalar action (which must be next)
 * is called.
 */

static void
sn_object_start(void *state)
{



}

static void
sn_object_end(void *state)
{



}

static void
sn_array_start(void *state)
{



}

static void
sn_array_end(void *state)
{



}

static void
sn_object_field_start(void *state, char *fname, bool isnull)
{

























}

static void
sn_array_element_start(void *state, bool isnull)
{






}

static void
sn_scalar(void *state, char *token, JsonTokenType tokentype)
{

















}

/*
 * SQL function json_strip_nulls(json) -> json
 */
Datum
json_strip_nulls(PG_FUNCTION_ARGS)
{

























}

/*
 * SQL function jsonb_strip_nulls(jsonb) -> jsonb
 */
Datum
jsonb_strip_nulls(PG_FUNCTION_ARGS)
{























































}

/*
 * Add values from the jsonb to the parse state.
 *
 * If the parse state container is an object, the jsonb is pushed as
 * a value, not a key.
 *
 * This needs to be done using an iterator because pushJsonbValue doesn't
 * like getting jbvBinary values, so we can't just push jb as a whole.
 */
static void
addJsonbToParseState(JsonbParseState **jbps, Jsonb *jb)
{









































}

/*
 * SQL function jsonb_pretty (jsonb)
 *
 * Pretty-printed text for the jsonb
 */
Datum
jsonb_pretty(PG_FUNCTION_ARGS)
{






}

/*
 * SQL function jsonb_concat (jsonb, jsonb)
 *
 * function for || operator
 */
Datum
jsonb_concat(PG_FUNCTION_ARGS)
{
































}

/*
 * SQL function jsonb_delete (jsonb, text)
 *
 * return a copy of the jsonb with the indicated item
 * removed.
 */
Datum
jsonb_delete(PG_FUNCTION_ARGS)
{











































}

/*
 * SQL function jsonb_delete (jsonb, variadic text[])
 *
 * return a copy of the jsonb with the indicated items
 * removed.
 */
Datum
jsonb_delete_array(PG_FUNCTION_ARGS)
{
















































































}

/*
 * SQL function jsonb_delete (jsonb, int)
 *
 * return a copy of the jsonb with the indicated item
 * removed. Negative int means count back from the
 * end of the items.
 */
Datum
jsonb_delete_idx(PG_FUNCTION_ARGS)
{
































































}

/*
 * SQL function jsonb_set(jsonb, text[], jsonb, boolean)
 *
 */
Datum
jsonb_set(PG_FUNCTION_ARGS)
{








































}

/*
 * SQL function jsonb_delete_path(jsonb, text[])
 */
Datum
jsonb_delete_path(PG_FUNCTION_ARGS)
{






































}

/*
 * SQL function jsonb_insert(jsonb, text[], jsonb, boolean)
 *
 */
Datum
jsonb_insert(PG_FUNCTION_ARGS)
{



































}

/*
 * Iterate over all jsonb objects and merge them into one.
 * The logic of this function copied from the same hstore function,
 * except the case, when it1 & it2 represents jbvObject.
 * In that case we just append the content of it2 to it1 without any
 * verifications.
 */
static JsonbValue *
IteratorConcat(JsonbIterator **it1, JsonbIterator **it2, JsonbParseState **state)
{




































































































}

/*
 * Do most of the heavy work for jsonb_set/jsonb_insert
 *
 * If JB_PATH_DELETE bit is set in op_type, the element is to be removed.
 *
 * If any bit mentioned in JB_PATH_CREATE_OR_INSERT is set in op_type,
 * we create the new value if the key or array index does not exist.
 *
 * Bits JB_PATH_INSERT_BEFORE and JB_PATH_INSERT_AFTER in op_type
 * behave as JB_PATH_CREATE if new value is inserted in JsonbObject.
 *
 * All path elements before the last must already exist
 * whatever bits in op_type are set, or nothing is done.
 */
static JsonbValue *
setPath(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, int op_type)
{








































}

/*
 * Object walker for setPath
 */
static void
setPathObject(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, uint32 npairs, int op_type)
{































































































}

/*
 * Array walker for setPath
 */
static void
setPathArray(JsonbIterator **it, Datum *path_elems, bool *path_nulls, int path_len, JsonbParseState **st, int level, Jsonb *newval, uint32 nelems, int op_type)
{





























































































































}

/*
 * Parse information about what elements of a jsonb document we want to iterate
 * in functions iterate_json(b)_values. This information is presented in jsonb
 * format, so that it can be easily extended in the future.
 */
uint32
parse_jsonb_index_flags(Jsonb *jb)
{


































































}

/*
 * Iterate over jsonb values or elements, specified by flags, and pass them
 * together with an iteration state to a specified
 * JsonIterateStringValuesAction.
 */
void
iterate_jsonb_values(Jsonb *jb, uint32 flags, void *state, JsonIterateStringValuesAction action)
{

































































}

/*
 * Iterate over json values and elements, specified by flags, and pass them
 * together with an iteration state to a specified
 * JsonIterateStringValuesAction.
 */
void
iterate_json_values(text *json, uint32 flags, void *action_state, JsonIterateStringValuesAction action)
{














}

/*
 * An auxiliary function for iterate_json_values to invoke a specified
 * JsonIterateStringValuesAction for specified values.
 */
static void
iterate_values_scalar(void *state, char *token, JsonTokenType tokentype)
{



























}

static void
iterate_values_object_field_start(void *state, char *fname, bool isnull)
{








}

/*
 * Iterate over a jsonb, and apply a specified JsonTransformStringValuesAction
 * to every string value or element. Any necessary context for a
 * JsonTransformStringValuesAction can be passed in the action_state variable.
 * Function returns a copy of an original jsonb object with transformed values.
 */
Jsonb *
transform_jsonb_string_values(Jsonb *jsonb, void *action_state, JsonTransformStringValuesAction transform_action)
{































}

/*
 * Iterate over a json, and apply a specified JsonTransformStringValuesAction
 * to every string value or element. Any necessary context for a
 * JsonTransformStringValuesAction can be passed in the action_state variable.
 * Function returns a StringInfo, which is a copy of an original json with
 * transformed values.
 */
text *
transform_json_string_values(text *json, void *action_state, JsonTransformStringValuesAction transform_action)
{






















}

/*
 * Set of auxiliary functions for transform_json_string_values to invoke a
 * specified JsonTransformStringValuesAction for all values and left everything
 * else untouched.
 */
static void
transform_string_values_object_start(void *state)
{



}

static void
transform_string_values_object_end(void *state)
{



}

static void
transform_string_values_array_start(void *state)
{



}

static void
transform_string_values_array_end(void *state)
{



}

static void
transform_string_values_object_field_start(void *state, char *fname, bool isnull)
{













}

static void
transform_string_values_array_element_start(void *state, bool isnull)
{






}

static void
transform_string_values_scalar(void *state, char *token, JsonTokenType tokentype)
{












}