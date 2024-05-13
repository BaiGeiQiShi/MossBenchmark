/*-------------------------------------------------------------------------
 *
 * json.c
 *		JSON data type support.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/json.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "parser/parse_coerce.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/lsyscache.h"
#include "utils/json.h"
#include "utils/jsonapi.h"
#include "utils/typcache.h"
#include "utils/syscache.h"

/*
 * The context of the parser is maintained by the recursive descent
 * mechanism, but is passed explicitly to the error reporting routine
 * for better diagnostics.
 */
typedef enum /* contexts of JSON parser */
{
  JSON_PARSE_VALUE,        /* expecting a value */
  JSON_PARSE_STRING,       /* expecting a string (for a field name) */
  JSON_PARSE_ARRAY_START,  /* saw '[', expecting value or ']' */
  JSON_PARSE_ARRAY_NEXT,   /* saw array element, expecting ',' or ']' */
  JSON_PARSE_OBJECT_START, /* saw '{', expecting label or '}' */
  JSON_PARSE_OBJECT_LABEL, /* saw object label, expecting ':' */
  JSON_PARSE_OBJECT_NEXT,  /* saw object value, expecting ',' or '}' */
  JSON_PARSE_OBJECT_COMMA, /* saw object ',', expecting next label */
  JSON_PARSE_END           /* saw the end of a document, expect nothing */
} JsonParseContext;

typedef enum /* type categories for datum_to_json */
{
  JSONTYPE_NULL,    /* null, so we didn't bother to identify */
  JSONTYPE_BOOL,    /* boolean (built-in types only) */
  JSONTYPE_NUMERIC, /* numeric (ditto) */
  JSONTYPE_DATE,    /* we use special formatting for datetimes */
  JSONTYPE_TIMESTAMP,
  JSONTYPE_TIMESTAMPTZ,
  JSONTYPE_JSON,      /* JSON itself (and JSONB) */
  JSONTYPE_ARRAY,     /* array */
  JSONTYPE_COMPOSITE, /* composite */
  JSONTYPE_CAST,      /* something with an explicit cast to JSON */
  JSONTYPE_OTHER      /* all else */
} JsonTypeCategory;

typedef struct JsonAggState
{
  StringInfo str;
  JsonTypeCategory key_category;
  Oid key_output_func;
  JsonTypeCategory val_category;
  Oid val_output_func;
} JsonAggState;

static inline void
json_lex(JsonLexContext *lex);
static inline void
json_lex_string(JsonLexContext *lex);
static inline void
json_lex_number(JsonLexContext *lex, char *s, bool *num_err, int *total_len);
static inline void
parse_scalar(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_object_field(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_object(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_array_element(JsonLexContext *lex, JsonSemAction *sem);
static void
parse_array(JsonLexContext *lex, JsonSemAction *sem);
static void
report_parse_error(JsonParseContext ctx, JsonLexContext *lex) pg_attribute_noreturn();
static void
report_invalid_token(JsonLexContext *lex) pg_attribute_noreturn();
static int
report_json_context(JsonLexContext *lex);
static char *
extract_mb_char(char *s);
static void
composite_to_json(Datum composite, StringInfo result, bool use_line_feeds);
static void
array_dim_to_json(StringInfo result, int dim, int ndims, int *dims, Datum *vals, bool *nulls, int *valcount, JsonTypeCategory tcategory, Oid outfuncoid, bool use_line_feeds);
static void
array_to_json_internal(Datum array, StringInfo result, bool use_line_feeds);
static void
json_categorize_type(Oid typoid, JsonTypeCategory *tcategory, Oid *outfuncoid);
static void
datum_to_json(Datum val, bool is_null, StringInfo result, JsonTypeCategory tcategory, Oid outfuncoid, bool key_scalar);
static void
add_json(Datum val, bool is_null, StringInfo result, Oid val_type, bool key_scalar);
static text *
catenate_stringinfo_string(StringInfo buffer, const char *addon);

/* the null action object used for pure validation */
static JsonSemAction nullSemAction = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/* Recursive Descent parser support routines */

/*
 * lex_peek
 *
 * what is the current look_ahead token?
 */
static inline JsonTokenType
lex_peek(JsonLexContext *lex)
{

}

/*
 * lex_accept
 *
 * accept the look_ahead token and move the lexer to the next token if the
 * look_ahead token matches the token parameter. In that case, and if required,
 * also hand back the de-escaped lexeme.
 *
 * returns true if the token matched, false otherwise.
 */
static inline bool
lex_accept(JsonLexContext *lex, JsonTokenType token, char **lexeme)
{

























}

/*
 * lex_accept
 *
 * move the lexer to the next token if the current look_ahead token matches
 * the parameter token. Otherwise, report an error.
 */
static inline void
lex_expect(JsonParseContext ctx, JsonLexContext *lex, JsonTokenType token)
{




}

/* chars to consider as part of an alphanumeric token */
#define JSON_ALPHANUMERIC_CHAR(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') || (c) == '_' || IS_HIGHBIT_SET(c))

/*
 * Utility function to check if a string is a valid JSON number.
 *
 * str is of length len, and need not be null-terminated.
 */
bool
IsValidJsonNumber(const char *str, int len)
{





























}

/*
 * Input.
 */
Datum
json_in(PG_FUNCTION_ARGS)
{










}

/*
 * Output.
 */
Datum
json_out(PG_FUNCTION_ARGS)
{




}

/*
 * Binary send.
 */
Datum
json_send(PG_FUNCTION_ARGS)
{






}

/*
 * Binary receive.
 */
Datum
json_recv(PG_FUNCTION_ARGS)
{












}

/*
 * makeJsonLexContext
 *
 * lex constructor, with or without StringInfo object
 * for de-escaped lexemes.
 *
 * Without is better as it makes the processing faster, so only make one
 * if really required.
 *
 * If you already have the json as a text* value, use the first of these
 * functions, otherwise use  makeJsonLexContextCstringLen().
 */
JsonLexContext *
makeJsonLexContext(text *json, bool need_escapes)
{

}

JsonLexContext *
makeJsonLexContextCstringLen(char *json, int len, bool need_escapes)
{










}

/*
 * pg_parse_json
 *
 * Publicly visible entry point for the JSON parser.
 *
 * lex is a lexing context, set up for the json to be processed by calling
 * makeJsonLexContext(). sem is a structure of function pointers to semantic
 * action routines to be called at appropriate spots during parsing, and a
 * pointer to a state object to be passed to those routines.
 */
void
pg_parse_json(JsonLexContext *lex, JsonSemAction *sem)
{





















}

/*
 * json_count_array_elements
 *
 * Returns number of array elements in lex context at start of array token
 * until end of array token at same nesting level.
 *
 * Designed to be called from array_start routines.
 */
int
json_count_array_elements(JsonLexContext *lex)
{

























}

/*
 *	Recursive Descent parse routines. There is one for each structural
 *	element in a json document:
 *	  - scalar (string, number, true, false, null)
 *	  - array  ( [ ] )
 *	  - array element
 *	  - object ( { } )
 *	  - object field
 */
static inline void
parse_scalar(JsonLexContext *lex, JsonSemAction *sem)
{

































}

static void
parse_object_field(JsonLexContext *lex, JsonSemAction *sem)
{

















































}

static void
parse_object(JsonLexContext *lex, JsonSemAction *sem)
{



















































}

static void
parse_array_element(JsonLexContext *lex, JsonSemAction *sem)
{






























}

static void
parse_array(JsonLexContext *lex, JsonSemAction *sem)
{










































}

/*
 * Lex one token from the input stream.
 */
static inline void
json_lex(JsonLexContext *lex)
{

















































































































































}

/*
 * The next token in the input stream is known to be a string; lex it.
 */
static inline void
json_lex_string(JsonLexContext *lex)
{












































































































































































































}

/*
 * The next token in the input stream is known to be a number; lex it.
 *
 * In JSON, a number consists of four parts:
 *
 * (1) An optional minus sign ('-').
 *
 * (2) Either a single '0', or a string of one or more digits that does not
 *	   begin with a '0'.
 *
 * (3) An optional decimal part, consisting of a period ('.') followed by
 *	   one or more digits.  (Note: While this part can be omitted
 *	   completely, it's not OK to have only the decimal point without
 *	   any digits afterwards.)
 *
 * (4) An optional exponent part, consisting of 'e' or 'E', optionally
 *	   followed by '+' or '-', followed by one or more digits.  (Note:
 *	   As with the decimal part, if 'e' or 'E' is present, it must be
 *	   followed by at least one digit.)
 *
 * The 's' argument to this function points to the ostensible beginning
 * of part 2 - i.e. the character after any optional minus sign, or the
 * first character of the string if there is none.
 *
 * If num_err is not NULL, we return an error flag to *num_err rather than
 * raising an error for a badly-formed number.  Also, if total_len is not NULL
 * the distance from lex->input to the token end+1 is returned to *total_len.
 */
static inline void
json_lex_number(JsonLexContext *lex, char *s, bool *num_err, int *total_len)
{



































































































}

/*
 * Report a parse error.
 *
 * lex->token_start and lex->token_terminator must identify the current token.
 */
static void
report_parse_error(JsonParseContext ctx, JsonLexContext *lex)
{




















































}

/*
 * Report an invalid input token.
 *
 * lex->token_start and lex->token_terminator must identify the token.
 */
static void
report_invalid_token(JsonLexContext *lex)
{










}

/*
 * Report a CONTEXT line for bogus JSON input.
 *
 * lex->token_terminator must be set to identify the spot where we detected
 * the error.  Note that lex->token_start might be NULL, in case we recognized
 * error at EOF.
 *
 * The return value isn't meaningful, but we make it non-void so that this
 * can be invoked inside ereport().
 */
static int
report_json_context(JsonLexContext *lex)
{
































































}

/*
 * Extract a single, possibly multi-byte char from the input string.
 */
static char *
extract_mb_char(char *s)
{









}

/*
 * Determine how we want to print values of a given type in datum_to_json.
 *
 * Given the datatype OID, return its JsonTypeCategory, as well as the type's
 * output function OID.  If the returned category is JSONTYPE_CAST, we
 * return the OID of the type->JSON cast function instead.
 */
static void
json_categorize_type(Oid typoid, JsonTypeCategory *tcategory, Oid *outfuncoid)
{























































































}

/*
 * Turn a Datum into JSON text, appending the string to "result".
 *
 * tcategory and outfuncoid are from a previous call to json_categorize_type,
 * except that if is_null is true then they can be invalid.
 *
 * If key_scalar is true, the value is being printed as a key, so insist
 * it's of an acceptable type, and force it to be quoted.
 */
static void
datum_to_json(Datum val, bool is_null, StringInfo result, JsonTypeCategory tcategory, Oid outfuncoid, bool key_scalar)
{



































































































}

/*
 * Encode 'value' of datetime type 'typid' into JSON string in ISO format using
 * optionally preallocated buffer 'buf'.
 */
char *
JsonEncodeDateTime(char *buf, Datum value, Oid typid)
{





































































































}

/*
 * Process a single dimension of an array.
 * If it's the innermost dimension, output the values, otherwise call
 * ourselves recursively to process the next dimension.
 */
static void
array_dim_to_json(StringInfo result, int dim, int ndims, int *dims, Datum *vals, bool *nulls, int *valcount, JsonTypeCategory tcategory, Oid outfuncoid, bool use_line_feeds)
{
































}

/*
 * Turn an array into JSON.
 */
static void
array_to_json_internal(Datum array, StringInfo result, bool use_line_feeds)
{


































}

/*
 * Turn a composite / record into JSON.
 */
static void
composite_to_json(Datum composite, StringInfo result, bool use_line_feeds)
{


































































}

/*
 * Append JSON text for "val" to "result".
 *
 * This is just a thin wrapper around datum_to_json.  If the same type will be
 * printed many times, avoid using this; better to do the json_categorize_type
 * lookups only once.
 */
static void
add_json(Datum val, bool is_null, StringInfo result, Oid val_type, bool key_scalar)
{



















}

/*
 * SQL function array_to_json(row)
 */
Datum
array_to_json(PG_FUNCTION_ARGS)
{








}

/*
 * SQL function array_to_json(row, prettybool)
 */
Datum
array_to_json_pretty(PG_FUNCTION_ARGS)
{









}

/*
 * SQL function row_to_json(row)
 */
Datum
row_to_json(PG_FUNCTION_ARGS)
{








}

/*
 * SQL function row_to_json(row, prettybool)
 */
Datum
row_to_json_pretty(PG_FUNCTION_ARGS)
{









}

/*
 * SQL function to_json(anyvalue)
 */
Datum
to_json(PG_FUNCTION_ARGS)
{


















}

/*
 * json_agg transition function
 *
 * aggregate input column as a json array value.
 */
Datum
json_agg_transfn(PG_FUNCTION_ARGS)
{






























































}

/*
 * json_agg final function
 */
Datum
json_agg_finalfn(PG_FUNCTION_ARGS)
{















}

/*
 * json_object_agg transition function.
 *
 * aggregate two input columns as a single json object value.
 */
Datum
json_object_agg_transfn(PG_FUNCTION_ARGS)
{


















































































}

/*
 * json_object_agg final function.
 */
Datum
json_object_agg_finalfn(PG_FUNCTION_ARGS)
{















}

/*
 * Helper function for aggregates: return given StringInfo's contents plus
 * specified trailing string, as a text datum.  We need this because aggregate
 * final functions are not allowed to modify the aggregate state.
 */
static text *
catenate_stringinfo_string(StringInfo buffer, const char *addon)
{










}

/*
 * SQL function json_build_object(variadic "any")
 */
Datum
json_build_object(PG_FUNCTION_ARGS)
{















































}

/*
 * degenerate case of json_build_object where it gets 0 arguments.
 */
Datum
json_build_object_noargs(PG_FUNCTION_ARGS)
{

}

/*
 * SQL function json_build_array(variadic "any")
 */
Datum
json_build_array(PG_FUNCTION_ARGS)
{






























}

/*
 * degenerate case of json_build_array where it gets 0 arguments.
 */
Datum
json_build_array_noargs(PG_FUNCTION_ARGS)
{

}

/*
 * SQL function json_object(text[])
 *
 * take a one or two dimensional array of text as key/value pairs
 * for a json object.
 */
Datum
json_object(PG_FUNCTION_ARGS)
{













































































}

/*
 * SQL function json_object(text[], text[])
 *
 * take separate key and value arrays of text to construct a json object
 * pairwise.
 */
Datum
json_object_two_arg(PG_FUNCTION_ARGS)
{








































































}

/*
 * Produce a JSON string literal, properly escaping characters in the text.
 */
void
escape_json(StringInfo buf, const char *str)
{









































}

/*
 * SQL function json_typeof(json) -> text
 *
 * Returns the type of the outermost JSON value as TEXT.  Possible types are
 * "object", "array", "string", "number", "boolean", and "null".
 *
 * Performs a single call to json_lex() to get the first token of the supplied
 * value.  This initial token uniquely determines the value's type.  As our
 * input must already have been validated by json_in() or json_recv(), the
 * initial token should never be JSON_TOKEN_OBJECT_END, JSON_TOKEN_ARRAY_END,
 * JSON_TOKEN_COLON, JSON_TOKEN_COMMA, or JSON_TOKEN_END.
 */
Datum
json_typeof(PG_FUNCTION_ARGS)
{






































}