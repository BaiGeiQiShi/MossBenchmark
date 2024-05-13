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



















}

/*
 * Get the type name of a jsonb value.
 */
const char *
JsonbTypeName(JsonbValue *jbv)
{




















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





















}

static size_t
checkStringLen(size_t len)
{






}

static void
jsonb_in_object_start(void *pstate)
{



}

static void
jsonb_in_object_end(void *pstate)
{



}

static void
jsonb_in_array_start(void *pstate)
{



}

static void
jsonb_in_array_end(void *pstate)
{



}

static void
jsonb_in_object_field_start(void *pstate, char *fname, bool isnull)
{









}

static void
jsonb_put_escaped_value(StringInfo out, JsonbValue *scalarVal)
{
























}

/*
 * For jsonb we always want the de-escaped value - that's what's in token
 */
static void
jsonb_in_scalar(void *pstate, char *token, JsonTokenType tokentype)
{






































































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




































































































































}

static void
add_indent(StringInfo out, bool indent, int level)
{










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