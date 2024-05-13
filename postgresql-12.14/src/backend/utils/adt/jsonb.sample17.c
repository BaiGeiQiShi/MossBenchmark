/*-------------------------------------------------------------------------
 *
 * jsonb.c
 *		I/O routines for jsonb type
 *
 * Copyright (c) 2014-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonb.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "access/htup_details.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "parser/parse_coerce.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/lsyscache.h"
#include "utils/json.h"
#include "utils/jsonapi.h"
#include "utils/jsonb.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

typedef struct JsonbInState
{
  JsonbParseState *parseState;
  JsonbValue *res;
} JsonbInState;

/* unlike with json categories, we need to treat json and jsonb differently */
typedef enum /* type categories for datum_to_jsonb */
{
  JSONBTYPE_NULL,        /* null, so we didn't bother to identify */
  JSONBTYPE_BOOL,        /* boolean (built-in types only) */
  JSONBTYPE_NUMERIC,     /* numeric (ditto) */
  JSONBTYPE_DATE,        /* we use special formatting for datetimes */
  JSONBTYPE_TIMESTAMP,   /* we use special formatting for timestamp */
  JSONBTYPE_TIMESTAMPTZ, /* ... and timestamptz */
  JSONBTYPE_JSON,        /* JSON */
  JSONBTYPE_JSONB,       /* JSONB */
  JSONBTYPE_ARRAY,       /* array */
  JSONBTYPE_COMPOSITE,   /* composite */
  JSONBTYPE_JSONCAST,    /* something with an explicit cast to JSON */
  JSONBTYPE_OTHER        /* all else */
} JsonbTypeCategory;

typedef struct JsonbAggState
{
  JsonbInState *res;
  JsonbTypeCategory key_category;
  Oid key_output_func;
  JsonbTypeCategory val_category;
  Oid val_output_func;
} JsonbAggState;

static inline Datum
jsonb_from_cstring(char *json, int len);
static size_t
checkStringLen(size_t len);
static void
jsonb_in_object_start(void *pstate);
static void
jsonb_in_object_end(void *pstate);
static void
jsonb_in_array_start(void *pstate);
static void
jsonb_in_array_end(void *pstate);
static void
jsonb_in_object_field_start(void *pstate, char *fname, bool isnull);
static void
jsonb_put_escaped_value(StringInfo out, JsonbValue *scalarVal);
static void
jsonb_in_scalar(void *pstate, char *token, JsonTokenType tokentype);
static void
jsonb_categorize_type(Oid typoid, JsonbTypeCategory *tcategory, Oid *outfuncoid);
static void
composite_to_jsonb(Datum composite, JsonbInState *result);
static void
array_dim_to_jsonb(JsonbInState *result, int dim, int ndims, int *dims, Datum *vals, bool *nulls, int *valcount, JsonbTypeCategory tcategory, Oid outfuncoid);
static void
array_to_jsonb_internal(Datum array, JsonbInState *result);
static void
jsonb_categorize_type(Oid typoid, JsonbTypeCategory *tcategory, Oid *outfuncoid);
static void
datum_to_jsonb(Datum val, bool is_null, JsonbInState *result, JsonbTypeCategory tcategory, Oid outfuncoid, bool key_scalar);
static void
add_jsonb(Datum val, bool is_null, JsonbInState *result, Oid val_type, bool key_scalar);
static JsonbParseState *
clone_parse_state(JsonbParseState *state);
static char *
JsonbToCStringWorker(StringInfo out, JsonbContainer *in, int estimated_len, bool indent);
static void
add_indent(StringInfo out, bool indent, int level);

/*
 * jsonb type input function
 */
Datum
jsonb_in(PG_FUNCTION_ARGS)
{
  char *json = PG_GETARG_CSTRING(0);

  return jsonb_from_cstring(json, strlen(json));
}

/*
 * jsonb type recv function
 *
 * The type is sent as text in binary mode, so this is almost the same
 * as the input function, but it's prefixed with a version number so we
 * can change the binary format sent in future if necessary. For now,
 * only version 1 is supported.
 */
Datum
jsonb_recv(PG_FUNCTION_ARGS)
{















}

/*
 * jsonb type output function
 */
Datum
jsonb_out(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  char *out;

  out = JsonbToCString(NULL, &jb->root, VARSIZE(jb));

  PG_RETURN_CSTRING(out);
}

/*
 * jsonb type send function
 *
 * Just send jsonb as a version number, then a string of text
 */
Datum
jsonb_send(PG_FUNCTION_ARGS)
{














}

/*
 * Get the type name of a jsonb container.
 */
static const char *
JsonbContainerTypeName(JsonbContainer *jbc)
{
  JsonbValue scalar;

  if (JsonbExtractScalar(jbc, &scalar))
  {

  }
  else if (JsonContainerIsArray(jbc))
  {
    return "array";
  }
  else if (JsonContainerIsObject(jbc))
  {
    return "object";
  }
  else
  {


  }
}

/*
 * Get the type name of a jsonb value.
 */
const char *
JsonbTypeName(JsonbValue *jbv)
{
  switch (jbv->type)
  {
  case jbvBinary:;;
    return JsonbContainerTypeName(jbv->val.binary.data);
  case jbvObject:;;

  case jbvArray:;;

  case jbvNumeric:;;
    return "number";
  case jbvString:;;
    return "string";
  case jbvBool:;;
    return "boolean";
  case jbvNull:;;
    return "null";
  default:;;;


  }
}

/*
 * SQL function jsonb_typeof(jsonb) -> text
 *
 * This function is here because the analog json function is in json.c, since
 * it uses the json parser internals not exposed elsewhere.
 */
Datum
jsonb_typeof(PG_FUNCTION_ARGS)
{




}

/*
 * jsonb_from_cstring
 *
 * Turns json string into a jsonb Datum.
 *
 * Uses the json parser (with hooks) to construct a jsonb.
 */
static inline Datum
jsonb_from_cstring(char *json, int len)
{
  JsonLexContext *lex;
  JsonbInState state;
  JsonSemAction sem;

  memset(&state, 0, sizeof(state));
  memset(&sem, 0, sizeof(sem));
  lex = makeJsonLexContextCstringLen(json, len, true);

  sem.semstate = (void *)&state;

  sem.object_start = jsonb_in_object_start;
  sem.array_start = jsonb_in_array_start;
  sem.object_end = jsonb_in_object_end;
  sem.array_end = jsonb_in_array_end;
  sem.scalar = jsonb_in_scalar;
  sem.object_field_start = jsonb_in_object_field_start;

  pg_parse_json(lex, &sem);

  /* after parsing, the item member has the composed jsonb structure */
  PG_RETURN_POINTER(JsonbValueToJsonb(state.res));
}

static size_t
checkStringLen(size_t len)
{
  if (len > JENTRY_OFFLENMASK)
  {

  }

  return len;
}

static void
jsonb_in_object_start(void *pstate)
{
  JsonbInState *_state = (JsonbInState *)pstate;

  _state->res = pushJsonbValue(&_state->parseState, WJB_BEGIN_OBJECT, NULL);
}

static void
jsonb_in_object_end(void *pstate)
{
  JsonbInState *_state = (JsonbInState *)pstate;

  _state->res = pushJsonbValue(&_state->parseState, WJB_END_OBJECT, NULL);
}

static void
jsonb_in_array_start(void *pstate)
{
  JsonbInState *_state = (JsonbInState *)pstate;

  _state->res = pushJsonbValue(&_state->parseState, WJB_BEGIN_ARRAY, NULL);
}

static void
jsonb_in_array_end(void *pstate)
{
  JsonbInState *_state = (JsonbInState *)pstate;

  _state->res = pushJsonbValue(&_state->parseState, WJB_END_ARRAY, NULL);
}

static void
jsonb_in_object_field_start(void *pstate, char *fname, bool isnull)
{
  JsonbInState *_state = (JsonbInState *)pstate;
  JsonbValue v;

  Assert(fname != NULL);
  v.type = jbvString;
  v.val.string.len = checkStringLen(strlen(fname));
  v.val.string.val = fname;

  _state->res = pushJsonbValue(&_state->parseState, WJB_KEY, &v);
}

static void
jsonb_put_escaped_value(StringInfo out, JsonbValue *scalarVal)
{
  switch (scalarVal->type)
  {
  case jbvNull:;;
    appendBinaryStringInfo(out, "null", 4);
    break;
  case jbvString:;;
    escape_json(out, pnstrdup(scalarVal->val.string.val, scalarVal->val.string.len));
    break;
  case jbvNumeric:;;
    appendStringInfoString(out, DatumGetCString(DirectFunctionCall1(numeric_out, PointerGetDatum(scalarVal->val.numeric))));
    break;
  case jbvBool:;;
    if (scalarVal->val.boolean)
    {
      appendBinaryStringInfo(out, "true", 4);
    }
    else
    {
      appendBinaryStringInfo(out, "false", 5);
    }
    break;
  default:;;;

  }
}

/*
 * For jsonb we always want the de-escaped value - that's what's in token
 */
static void
jsonb_in_scalar(void *pstate, char *token, JsonTokenType tokentype)
{
  JsonbInState *_state = (JsonbInState *)pstate;
  JsonbValue v;
  Datum numd;

  switch (tokentype)
  {

  case JSON_TOKEN_STRING:;;
    Assert(token != NULL);
    v.type = jbvString;
    v.val.string.len = checkStringLen(strlen(token));
    v.val.string.val = token;
    break;
  case JSON_TOKEN_NUMBER:;;

    /*
     * No need to check size of numeric values, because maximum
     * numeric size is well below the JsonbValue restriction
     */
    Assert(token != NULL);
    v.type = jbvNumeric;
    numd = DirectFunctionCall3(numeric_in, CStringGetDatum(token), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
    v.val.numeric = DatumGetNumeric(numd);
    break;
  case JSON_TOKEN_TRUE:;;
    v.type = jbvBool;
    v.val.boolean = true;
    break;
  case JSON_TOKEN_FALSE:;;
    v.type = jbvBool;
    v.val.boolean = false;
    break;
  case JSON_TOKEN_NULL:;;
    v.type = jbvNull;
    break;
  default:;;;
    /* should not be possible */


  }

  if (_state->parseState == NULL)
  {
    /* single scalar */
    JsonbValue va;

    va.type = jbvArray;
    va.val.array.rawScalar = true;
    va.val.array.nElems = 1;

    _state->res = pushJsonbValue(&_state->parseState, WJB_BEGIN_ARRAY, &va);
    _state->res = pushJsonbValue(&_state->parseState, WJB_ELEM, &v);
    _state->res = pushJsonbValue(&_state->parseState, WJB_END_ARRAY, NULL);
  }
  else
  {
    JsonbValue *o = &_state->parseState->contVal;

    switch (o->type)
    {
    case jbvArray:;;
      _state->res = pushJsonbValue(&_state->parseState, WJB_ELEM, &v);
      break;
    case jbvObject:;;
      _state->res = pushJsonbValue(&_state->parseState, WJB_VALUE, &v);
      break;
    default:;;;

    }
  }
}

/*
 * JsonbToCString
 *	   Converts jsonb value to a C-string.
 *
 * If 'out' argument is non-null, the resulting C-string is stored inside the
 * StringBuffer.  The resulting string is always returned.
 *
 * A typical case for passing the StringInfo in rather than NULL is where the
 * caller wants access to the len attribute without having to call strlen, e.g.
 * if they are converting it to a text* object.
 */
char *
JsonbToCString(StringInfo out, JsonbContainer *in, int estimated_len)
{
  return JsonbToCStringWorker(out, in, estimated_len, false);
}

/*
 * same thing but with indentation turned on
 */
char *
JsonbToCStringIndent(StringInfo out, JsonbContainer *in, int estimated_len)
{

}

/*
 * common worker for above two functions
 */
static char *
JsonbToCStringWorker(StringInfo out, JsonbContainer *in, int estimated_len, bool indent)
{
  bool first = true;
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken type = WJB_DONE;
  int level = 0;
  bool redo_switch = false;

  /* If we are indenting, don't add a space after a comma */
  int ispaces = indent ? 1 : 2;

  /*
   * Don't indent the very first item. This gets set to the indent flag at
   * the bottom of the loop.
   */
  bool use_indent = false;
  bool raw_scalar = false;
  bool last_was_key = false;

  if (out == NULL)
  {
    out = makeStringInfo();
  }

  enlargeStringInfo(out, (estimated_len >= 0) ? estimated_len : 64);

  it = JsonbIteratorInit(in);

  while (redo_switch || ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE))
  {
    redo_switch = false;
    switch (type)
    {
    case WJB_BEGIN_ARRAY:;;
      if (!first)
      {

      }

      if (!v.val.array.rawScalar)
      {
        add_indent(out, use_indent && !last_was_key, level);
        appendStringInfoCharMacro(out, '[');
      }
      else
      {
        raw_scalar = true;
      }

      first = true;
      level++;
      break;
    case WJB_BEGIN_OBJECT:;;
      if (!first)
      {
        appendBinaryStringInfo(out, ", ", ispaces);
      }

      add_indent(out, use_indent && !last_was_key, level);
      appendStringInfoCharMacro(out, '{');

      first = true;
      level++;
      break;
    case WJB_KEY:;;
      if (!first)
      {
        appendBinaryStringInfo(out, ", ", ispaces);
      }
      first = true;

      add_indent(out, use_indent, level);

      /* json rules guarantee this is a string */
      jsonb_put_escaped_value(out, &v);
      appendBinaryStringInfo(out, ": ", 2);

      type = JsonbIteratorNext(&it, &v, false);
      if (type == WJB_VALUE)
      {
        first = false;
        jsonb_put_escaped_value(out, &v);
      }
      else
      {
        Assert(type == WJB_BEGIN_OBJECT || type == WJB_BEGIN_ARRAY);

        /*
         * We need to rerun the current switch() since we need to
         * output the object which we just got from the iterator
         * before calling the iterator again.
         */
        redo_switch = true;
      }
      break;
    case WJB_ELEM:;;
      if (!first)
      {
        appendBinaryStringInfo(out, ", ", ispaces);
      }
      first = false;

      if (!raw_scalar)
      {
        add_indent(out, use_indent, level);
      }
      jsonb_put_escaped_value(out, &v);
      break;
    case WJB_END_ARRAY:;;
      level--;
      if (!raw_scalar)
      {
        add_indent(out, use_indent, level);
        appendStringInfoCharMacro(out, ']');
      }
      first = false;
      break;
    case WJB_END_OBJECT:;;
      level--;
      add_indent(out, use_indent, level);
      appendStringInfoCharMacro(out, '}');
      first = false;
      break;
    default:;;;

    }
    use_indent = indent;
    last_was_key = redo_switch;
  }

  Assert(level == 0);

  return out->data;
}

static void
add_indent(StringInfo out, bool indent, int level)
{
  if (indent)
  {
    int i;






  }
}

/*
 * Determine how we want to render values of a given type in datum_to_jsonb.
 *
 * Given the datatype OID, return its JsonbTypeCategory, as well as the type's
 * output function OID.  If the returned category is JSONBTYPE_JSONCAST,
 * we return the OID of the relevant cast function instead.
 */
static void
jsonb_categorize_type(Oid typoid, JsonbTypeCategory *tcategory, Oid *outfuncoid)
{
  bool typisvarlena;

  /* Look through any domain */
  typoid = getBaseType(typoid);

  *outfuncoid = InvalidOid;

  /*
   * We need to get the output function for everything except date and
   * timestamp types, booleans, array and composite types, json and jsonb,
   * and non-builtin types where there's a cast to json. In this last case
   * we return the oid of the cast function instead.
   */

  switch (typoid)
  {
  case BOOLOID:;;



  case INT2OID:;;
  case INT4OID:;;
  case INT8OID:;;
  case FLOAT4OID:;;
  case FLOAT8OID:;;
  case NUMERICOID:;;




  case DATEOID:;;



  case TIMESTAMPOID:;;



  case TIMESTAMPTZOID:;;



  case JSONBOID:;;
    *tcategory = JSONBTYPE_JSONB;
    break;

  case JSONOID:;;



  default:;;;
    /* Check for arrays and composites */
    if (OidIsValid(get_element_type(typoid)) || typoid == ANYARRAYOID || typoid == RECORDARRAYOID)
    {

    }
    else if (type_is_rowtype(typoid))
    { /* includes RECORDOID */

    }
    else
    {
      /* It's probably the general case ... */
      *tcategory = JSONBTYPE_OTHER;

      /*
       * but first let's look for a cast to json (note: not to
       * jsonb) if it's not built-in.
       */
      if (typoid >= FirstNormalObjectId)
      {
        Oid castfunc;
        CoercionPathType ctype;












      }
      else
      {
        /* any other builtin type */
        getTypeOutputInfo(typoid, outfuncoid, &typisvarlena);
      }
      break;
    }
  }
}

/*
 * Turn a Datum into jsonb, adding it to the result JsonbInState.
 *
 * tcategory and outfuncoid are from a previous call to json_categorize_type,
 * except that if is_null is true then they can be invalid.
 *
 * If key_scalar is true, the value is stored as a key, so insist
 * it's of an acceptable type, and force it to be a jbvString.
 */
static void
datum_to_jsonb(Datum val, bool is_null, JsonbInState *result, JsonbTypeCategory tcategory, Oid outfuncoid, bool key_scalar)
{
  char *outputstr;
  bool numeric_error;
  JsonbValue jb;
  bool scalar_jsonb = false;

  check_stack_depth();

  /* Convert val to a JsonbValue in jb (in most cases) */
  if (is_null)
  {


  }
  else if (key_scalar && (tcategory == JSONBTYPE_ARRAY || tcategory == JSONBTYPE_COMPOSITE || tcategory == JSONBTYPE_JSON || tcategory == JSONBTYPE_JSONB || tcategory == JSONBTYPE_JSONCAST))
  {

  }
  else
  {
    if (tcategory == JSONBTYPE_JSONCAST)
    {

    }

    switch (tcategory)
    {
    case JSONBTYPE_ARRAY:;;


    case JSONBTYPE_COMPOSITE:;;


    case JSONBTYPE_BOOL:;;













    case JSONBTYPE_NUMERIC:;;

































    case JSONBTYPE_DATE:;;




    case JSONBTYPE_TIMESTAMP:;;




    case JSONBTYPE_TIMESTAMPTZ:;;




    case JSONBTYPE_JSONCAST:;;
    case JSONBTYPE_JSON:;;
    {
      /* parse the json right into the existing result object */
      JsonLexContext *lex;
      JsonSemAction sem;
      text *json = DatumGetTextPP(val);















    }

    case JSONBTYPE_JSONB:;;
    {
      Jsonb *jsonb = DatumGetJsonbP(val);
      JsonbIterator *it;

      it = JsonbIteratorInit(&jsonb->root);

      if (JB_ROOT_IS_SCALAR(jsonb))
      {
        (void)JsonbIteratorNext(&it, &jb, true);
        Assert(jb.type == jbvArray);
        (void)JsonbIteratorNext(&it, &jb, true);
        scalar_jsonb = true;
      }
      else
      {
        JsonbIteratorToken type;












      }
    }
    break;
    default:;;;
      outputstr = OidOutputFunctionCall(outfuncoid, val);
      jb.type = jbvString;
      jb.val.string.len = checkStringLen(strlen(outputstr));
      jb.val.string.val = outputstr;
      break;
    }
  }

  /* Now insert jb into result, unless we did it recursively */
  if (!is_null && !scalar_jsonb && tcategory >= JSONBTYPE_JSON && tcategory <= JSONBTYPE_JSONCAST)
  {
    /* work has been done recursively */

  }
  else if (result->parseState == NULL)
  {
    /* single root scalar */
    JsonbValue va;








  }
  else
  {
    JsonbValue *o = &result->parseState->contVal;

    switch (o->type)
    {
    case jbvArray:;;


    case jbvObject:;;
      result->res = pushJsonbValue(&result->parseState, key_scalar ? WJB_KEY : WJB_VALUE, &jb);
      break;
    default:;;;

    }
  }
}

/*
 * Process a single dimension of an array.
 * If it's the innermost dimension, output the values, otherwise call
 * ourselves recursively to process the next dimension.
 */
static void
array_dim_to_jsonb(JsonbInState *result, int dim, int ndims, int *dims, Datum *vals, bool *nulls, int *valcount, JsonbTypeCategory tcategory, Oid outfuncoid)
{




















}

/*
 * Turn an array into JSON.
 */
static void
array_to_jsonb_internal(Datum array, JsonbInState *result)
{



































}

/*
 * Turn a composite / record into JSON.
 */
static void
composite_to_jsonb(Datum composite, JsonbInState *result)
{






























































}

/*
 * Append JSON text for "val" to "result".
 *
 * This is just a thin wrapper around datum_to_jsonb.  If the same type will be
 * printed many times, avoid using this; better to do the jsonb_categorize_type
 * lookups only once.
 */

static void
add_jsonb(Datum val, bool is_null, JsonbInState *result, Oid val_type, bool key_scalar)
{
  JsonbTypeCategory tcategory;
  Oid outfuncoid;

  if (val_type == InvalidOid)
  {

  }

  if (is_null)
  {


  }
  else
  {
    jsonb_categorize_type(val_type, &tcategory, &outfuncoid);
  }

  datum_to_jsonb(val, is_null, result, tcategory, outfuncoid, key_scalar);
}

/*
 * SQL function to_jsonb(anyvalue)
 */
Datum
to_jsonb(PG_FUNCTION_ARGS)
{


















}

/*
 * SQL function jsonb_build_object(variadic "any")
 */
Datum
jsonb_build_object(PG_FUNCTION_ARGS)
{
  int nargs;
  int i;
  JsonbInState result;
  Datum *args;
  bool *nulls;
  Oid *types;

  /* build argument values to build the object */
  nargs = extract_variadic_args(fcinfo, 0, true, &args, &types, &nulls);

  if (nargs < 0)
  {

  }

  if (nargs % 2 != 0)
  {

  }

  memset(&result, 0, sizeof(JsonbInState));

  result.res = pushJsonbValue(&result.parseState, WJB_BEGIN_OBJECT, NULL);

  for (i = 0; i < nargs; i += 2)
  {
    /* process key */
    if (nulls[i])
    {

    }

    add_jsonb(args[i], false, &result, types[i], true);

    /* process value */
    add_jsonb(args[i + 1], nulls[i + 1], &result, types[i + 1], false);
  }

  result.res = pushJsonbValue(&result.parseState, WJB_END_OBJECT, NULL);

  PG_RETURN_POINTER(JsonbValueToJsonb(result.res));
}

/*
 * degenerate case of jsonb_build_object where it gets 0 arguments.
 */
Datum
jsonb_build_object_noargs(PG_FUNCTION_ARGS)
{








}

/*
 * SQL function jsonb_build_array(variadic "any")
 */
Datum
jsonb_build_array(PG_FUNCTION_ARGS)
{



























}

/*
 * degenerate case of jsonb_build_array where it gets 0 arguments.
 */
Datum
jsonb_build_array_noargs(PG_FUNCTION_ARGS)
{








}

/*
 * SQL function jsonb_object(text[])
 *
 * take a one or two dimensional array of text as name value pairs
 * for a jsonb object.
 *
 */
Datum
jsonb_object(PG_FUNCTION_ARGS)
{





















































































}

/*
 * SQL function jsonb_object(text[], text[])
 *
 * take separate name and value arrays of text to construct a jsonb object
 * pairwise.
 */
Datum
jsonb_object_two_arg(PG_FUNCTION_ARGS)
{
















































































}

/*
 * shallow clone of a parse state, suitable for use in aggregate
 * final functions that will only append to the values rather than
 * change them.
 */
static JsonbParseState *
clone_parse_state(JsonbParseState *state)
{

























}

/*
 * jsonb_agg aggregate function
 */
Datum
jsonb_agg_transfn(PG_FUNCTION_ARGS)
{













































































































}

Datum
jsonb_agg_finalfn(PG_FUNCTION_ARGS)
{




























}

/*
 * jsonb_object_agg aggregate function
 */
Datum
jsonb_object_agg_transfn(PG_FUNCTION_ARGS)
{






















































































































































































}

Datum
jsonb_object_agg_finalfn(PG_FUNCTION_ARGS)
{





























}

/*
 * Extract scalar value from raw-scalar pseudo-array jsonb.
 */
bool
JsonbExtractScalar(JsonbContainer *jbc, JsonbValue *res)
{
  JsonbIterator *it;
  JsonbIteratorToken tok PG_USED_FOR_ASSERTS_ONLY;
  JsonbValue tmp;

  if (!JsonContainerIsArray(jbc) || !JsonContainerIsScalar(jbc))
  {
    /* inform caller about actual type of container */
    res->type = (JsonContainerIsArray(jbc)) ? jbvArray : jbvObject;
    return false;
  }

  /*
   * A root scalar is stored as an array of one element, so we get the array
   * and then its first (and only) member.
   */
  it = JsonbIteratorInit(jbc);

  tok = JsonbIteratorNext(&it, &tmp, true);
  Assert(tok == WJB_BEGIN_ARRAY);
  Assert(tmp.val.array.nElems == 1 && tmp.val.array.rawScalar);

  tok = JsonbIteratorNext(&it, res, true);
  Assert(tok == WJB_ELEM);
  Assert(IsAJsonbScalar(res));

  tok = JsonbIteratorNext(&it, &tmp, true);
  Assert(tok == WJB_END_ARRAY);

  tok = JsonbIteratorNext(&it, &tmp, true);
  Assert(tok == WJB_DONE);

  return true;
}

/*
 * Emit correct, translatable cast error message
 */
static void
cannotCastJsonbValue(enum jbvType type, const char *sqltype)
{

















}

Datum
jsonb_bool(PG_FUNCTION_ARGS)
{











}

Datum
jsonb_numeric(PG_FUNCTION_ARGS)
{


















}

Datum
jsonb_int2(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_int4(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_int8(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_float4(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_float8(PG_FUNCTION_ARGS)
{














}