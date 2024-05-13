/*-------------------------------------------------------------------------
 *
 * jsonpath_exec.c
 *	 Routines for SQL/JSON path execution.
 *
 * Jsonpath is executed in the global context stored in JsonPathExecContext,
 * which is passed to almost every function involved into execution.  Entry
 * point for jsonpath execution is executeJsonPath() function, which
 * initializes execution context including initial JsonPathItem and JsonbValue,
 * flags, stack for calculation of @ in filters.
 *
 * The result of jsonpath query execution is enum JsonPathExecResult and
 * if succeeded sequence of JsonbValue, written to JsonValueList *found, which
 * is passed through the jsonpath items.  When found == NULL, we're inside
 * exists-query and we're interested only in whether result is empty.  In this
 * case execution is stopped once first result item is found, and the only
 * execution result is JsonPathExecResult.  The values of JsonPathExecResult
 * are following:
 * - jperOk			-- result sequence is not empty
 * - jperNotFound	-- result sequence is empty
 * - jperError		-- error occurred during execution
 *
 * Jsonpath is executed recursively (see executeItem()) starting form the
 * first path item (which in turn might be, for instance, an arithmetic
 * expression evaluated separately).  On each step single JsonbValue obtained
 * from previous path item is processed.  The result of processing is a
 * sequence of JsonbValue (probably empty), which is passed to the next path
 * item one by one.  When there is no next path item, then JsonbValue is added
 * to the 'found' list.  When found == NULL, then execution functions just
 * return jperOk (see executeNextItem()).
 *
 * Many of jsonpath operations require automatic unwrapping of arrays in lax
 * mode.  So, if input value is array, then corresponding operation is
 * processed not on array itself, but on all of its members one by one.
 * executeItemOptUnwrapTarget() function have 'unwrap' argument, which indicates
 * whether unwrapping of array is needed.  When unwrap == true, each of array
 * members is passed to executeItemOptUnwrapTarget() again but with unwrap ==
 *false in order to evade subsequent array unwrapping.
 *
 * All boolean expressions (predicates) are evaluated by executeBoolItem()
 * function, which returns tri-state JsonPathBool.  When error is occurred
 * during predicate execution, it returns jpbUnknown.  According to standard
 * predicates can be only inside filters.  But we support their usage as
 * jsonpath expression.  This helps us to implement @@ operator.  In this case
 * resulting JsonPathBool is transformed into jsonb bool or null.
 *
 * Arithmetic and boolean expression are evaluated recursively from expression
 * tree top down to the leaves.  Therefore, for binary arithmetic expressions
 * we calculate operands first.  Then we check that results are numeric
 * singleton lists, calculate the result and pass it to the next path item.
 *
 * Copyright (c) 2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	src/backend/utils/adt/jsonpath_exec.c
 *
 *-------------------------------------------------------------------------
 */

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

/*
 * Represents "base object" and it's "id" for .keyvalue() evaluation.
 */
typedef struct JsonBaseObjectInfo
{
  JsonbContainer *jbc;
  int id;
} JsonBaseObjectInfo;

/*
 * Context of jsonpath execution.
 */
typedef struct JsonPathExecContext
{
  Jsonb *vars;                   /* variables to substitute into jsonpath */
  JsonbValue *root;              /* for $ evaluation */
  JsonbValue *current;           /* for @ evaluation */
  JsonBaseObjectInfo baseObject; /* "base object" for .keyvalue()
                                  * evaluation */
  int lastGeneratedObjectId;     /* "id" counter for .keyvalue()
                                  * evaluation */
  int innermostArraySize;        /* for LAST array index evaluation */
  bool laxMode;                  /* true for "lax" mode, false for "strict"
                                  * mode */
  bool ignoreStructuralErrors;   /* with "true" structural errors such
                                  * as absence of required json item or
                                  * unexpected json item type are
                                  * ignored */
  bool throwErrors;              /* with "false" all suppressible errors are
                                  * suppressed */
} JsonPathExecContext;

/* Context for LIKE_REGEX execution. */
typedef struct JsonLikeRegexContext
{
  text *regex;
  int cflags;
} JsonLikeRegexContext;

/* Result of jsonpath predicate evaluation */
typedef enum JsonPathBool
{
  jpbFalse = 0,
  jpbTrue = 1,
  jpbUnknown = 2
} JsonPathBool;

/* Result of jsonpath expression evaluation */
typedef enum JsonPathExecResult
{
  jperOk = 0,
  jperNotFound = 1,
  jperError = 2
} JsonPathExecResult;

#define jperIsError(jper) ((jper) == jperError)

/*
 * List of jsonb values with shortcut for single-value list.
 */
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

/* strict/lax flags is decomposed into four [un]wrap/error flags */
#define jspStrictAbsenseOfErrors(cxt) (!(cxt)->laxMode)
#define jspAutoUnwrap(cxt) ((cxt)->laxMode)
#define jspAutoWrap(cxt) ((cxt)->laxMode)
#define jspIgnoreStructuralErrors(cxt) ((cxt)->ignoreStructuralErrors)
#define jspThrowErrors(cxt) ((cxt)->throwErrors)

/* Convenience macro: return or throw error depending on context */
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

/****************** User interface to JsonPath executor ********************/

/*
 * jsonb_path_exists
 *		Returns true if jsonpath returns at least one item for the
 *specified jsonb value.  This function and jsonb_path_match() are used to
 *		implement @? and @@ operators, which in turn are intended to
 *have an index support.  Thus, it's desirable to make it easier to achieve
 *		consistency between index scan results and sequential scan
 *results. So, we throw as less errors as possible.  Regarding this function,
 *		such behavior also matches behavior of JSON_EXISTS() clause of
 *		SQL/JSON.  Regarding jsonb_path_match(), this function doesn't
 *have an analogy in SQL/JSON, so we define its behavior on our own.
 */
Datum
jsonb_path_exists(PG_FUNCTION_ARGS)
{























}

/*
 * jsonb_path_exists_opr
 *		Implementation of operator "jsonb @? jsonpath" (2-argument
 *version of jsonb_path_exists()).
 */
Datum
jsonb_path_exists_opr(PG_FUNCTION_ARGS)
{


}

/*
 * jsonb_path_match
 *		Returns jsonpath predicate result item for the specified jsonb
 *value. See jsonb_path_exists() comment for details regarding error handling.
 */
Datum
jsonb_path_match(PG_FUNCTION_ARGS)
{






































}

/*
 * jsonb_path_match_opr
 *		Implementation of operator "jsonb @@ jsonpath" (2-argument
 *version of jsonb_path_match()).
 */
Datum
jsonb_path_match_opr(PG_FUNCTION_ARGS)
{


}

/*
 * jsonb_path_query
 *		Executes jsonpath for given jsonb document and returns result as
 *		rowset.
 */
Datum
jsonb_path_query(PG_FUNCTION_ARGS)
{











































}

/*
 * jsonb_path_query_array
 *		Executes jsonpath for given jsonb document and returns result as
 *		jsonb array.
 */
Datum
jsonb_path_query_array(PG_FUNCTION_ARGS)
{









}

/*
 * jsonb_path_query_first
 *		Executes jsonpath for given jsonb document and returns first
 *result item.  If there are no items, NULL returned.
 */
Datum
jsonb_path_query_first(PG_FUNCTION_ARGS)
{
















}

/********************Execute functions for JsonPath**************************/

/*
 * Interface to jsonpath executor
 *
 * 'path' - jsonpath to be executed
 * 'vars' - variables to be substituted to jsonpath
 * 'json' - target document for jsonpath evaluation
 * 'throwErrors' - whether we should throw suppressible errors
 * 'result' - list to store result items into
 *
 * Returns an error if a recoverable error happens during processing, or NULL
 * on no error.
 *
 * Note, jsonb and jsonpath values should be available and untoasted during
 * work because JsonPathItem, JsonbValue and result item could have pointers
 * into input values.  If caller needs to just check if document matches
 * jsonpath, then it doesn't provide a result arg.  In this case executor
 * works till first positive result and does not check the rest if possible.
 * In other case it tries to find all the satisfied result items.
 */
static JsonPathExecResult
executeJsonPath(JsonPath *path, Jsonb *vars, Jsonb *json, bool throwErrors, JsonValueList *result)
{



















































}

/*
 * Execute jsonpath with automatic unwrapping of current item in lax mode.
 */
static JsonPathExecResult
executeItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found)
{

}

/*
 * Main jsonpath executor function: walks on jsonpath structure, finds
 * relevant parts of jsonb and evaluates expressions over them.
 * When 'unwrap' is true current SQL/JSON item is unwrapped if it is an array.
 */
static JsonPathExecResult
executeItemOptUnwrapTarget(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found, bool unwrap)
{









































































































































































































































































































































































































































































































}

/*
 * Unwrap current array item and execute jsonpath for each of its elements.
 */
static JsonPathExecResult
executeItemUnwrapTargetArray(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found, bool unwrapElements)
{







}

/*
 * Execute next jsonpath item if exists.  Otherwise put "v" to the "found"
 * list if provided.
 */
static JsonPathExecResult
executeNextItem(JsonPathExecContext *cxt, JsonPathItem *cur, JsonPathItem *next, JsonbValue *v, JsonValueList *found, bool copy)
{




























}

/*
 * Same as executeItem(), but when "unwrap == true" automatically unwraps
 * each array item from the resulting sequence in lax mode.
 */
static JsonPathExecResult
executeItemOptUnwrapResult(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, JsonValueList *found)
{































}

/*
 * Same as executeItemOptUnwrapResult(), but with error suppression.
 */
static JsonPathExecResult
executeItemOptUnwrapResultNoThrow(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, JsonValueList *found)
{








}

/* Execute boolean-valued jsonpath expression. */
static JsonPathBool
executeBoolItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool canHaveNext)
{































































































































}

/*
 * Execute nested (filters etc.) boolean expression pushing current SQL/JSON
 * item onto the stack.
 */
static JsonPathBool
executeNestedBoolItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb)
{









}

/*
 * Implementation of several jsonpath nodes:
 *  - jpiAny (.** accessor),
 *  - jpiAnyKey (.* accessor),
 *  - jpiAnyArray ([*] accessor)
 */
static JsonPathExecResult
executeAnyItem(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbContainer *jbc, JsonValueList *found, uint32 level, uint32 first, uint32 last, bool ignoreStructuralErrors, bool unwrapNext)
{





















































































}

/*
 * Execute unary or binary predicate.
 *
 * Predicates have existence semantics, because their operands are item
 * sequences.  Pairs of items from the left and right operand's sequences are
 * checked.  TRUE returned only if any pair satisfying the condition is found.
 * In strict mode, even if the desired pair has already been found, all pairs
 * still need to be examined to check the absence of errors.  If any error
 * occurs, UNKNOWN (analogous to SQL NULL) is returned.
 */
static JsonPathBool
executePredicate(JsonPathExecContext *cxt, JsonPathItem *pred, JsonPathItem *larg, JsonPathItem *rarg, JsonbValue *jb, bool unwrapRightArg, JsonPathPredicateCallback exec, void *param)
{





















































































}

/*
 * Execute binary arithmetic expression on singleton numeric operands.
 * Array operands are automatically unwrapped in lax mode.
 */
static JsonPathExecResult
executeBinaryArithmExpr(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, BinaryArithmFunc func, JsonValueList *found)
{
































































}

/*
 * Execute unary arithmetic expression for each numeric item in its operand's
 * sequence.  Array operand is automatically unwrapped in lax mode.
 */
static JsonPathExecResult
executeUnaryArithmExpr(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, PGFunction func, JsonValueList *found)
{































































}

/*
 * STARTS_WITH predicate callback.
 *
 * Check if the 'whole' string starts from 'initial' string.
 */
static JsonPathBool
executeStartsWith(JsonPathItem *jsp, JsonbValue *whole, JsonbValue *initial, void *param)
{
















}

/*
 * LIKE_REGEX predicate callback.
 *
 * Check if the string matches regex pattern.
 */
static JsonPathBool
executeLikeRegex(JsonPathItem *jsp, JsonbValue *str, JsonbValue *rarg, void *param)
{




















}

/*
 * Execute numeric item methods (.abs(), .floor(), .ceil()) using the specified
 * user function 'func'.
 */
static JsonPathExecResult
executeNumericItemMethod(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, bool unwrap, PGFunction func, JsonValueList *found)
{

























}

/*
 * Implementation of .keyvalue() method.
 *
 * .keyvalue() method returns a sequence of object's key-value pairs in the
 * following format: '{ "key": key, "value": value, "id": id }'.
 *
 * "id" field is an object identifier which is constructed from the two parts:
 * base object id and its binary offset in base object's jsonb:
 * id = 10000000000 * base_object_id + obj_offset_in_base_object
 *
 * 10000000000 (10^10) -- is a first round decimal number greater than 2^32
 * (maximal offset in jsonb).  Decimal multiplier is used here to improve the
 * readability of identifiers.
 *
 * Base object is usually a root object of the path: context item '$' or path
 * variable '$var', literals can't produce objects for now.  But if the path
 * contains generated objects (.keyvalue() itself, for example), then they
 * become base object for the subsequent .keyvalue().
 *
 * Id of '$' is 0. Id of '$var' is its ordinal (positive) number in the list
 * of variables (see getJsonPathVariable()).  Ids for generated objects
 * are assigned using global counter JsonPathExecContext.lastGeneratedObjectId.
 */
static JsonPathExecResult
executeKeyValueMethod(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, JsonValueList *found)
{












































































































}

/*
 * Convert boolean execution status 'res' to a boolean JSON item and execute
 * next jsonpath.
 */
static JsonPathExecResult
appendBoolResult(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonValueList *found, JsonPathBool res)
{



















}

/*
 * Convert jsonpath's scalar or variable node to actual jsonb value.
 *
 * If node is a variable then its id returned, otherwise 0 returned.
 */
static void
getJsonPathItem(JsonPathExecContext *cxt, JsonPathItem *item, JsonbValue *value)
{























}

/*
 * Get the value of variable passed to jsonpath executor
 */
static void
getJsonPathVariable(JsonPathExecContext *cxt, JsonPathItem *variable, Jsonb *vars, JsonbValue *value)
{































}

/**************** Support functions for JsonPath execution *****************/

/*
 * Returns the size of an array item, or -1 if item is not an array.
 */
static int
JsonbArraySize(JsonbValue *jb)
{













}

/* Comparison predicate callback. */
static JsonPathBool
executeComparison(JsonPathItem *cmp, JsonbValue *lv, JsonbValue *rv, void *p)
{

}

/*
 * Perform per-byte comparison of two strings.
 */
static int
binaryCompareStrings(const char *s1, int len1, const char *s2, int len2)
{















}

/*
 * Compare two strings in the current server encoding using Unicode codepoint
 * collation.
 */
static int
compareStrings(const char *mbstr1, int mblen1, const char *mbstr2, int mblen2)
{
































































}

/*
 * Compare two SQL/JSON items using comparison operation 'op'.
 */
static JsonPathBool
compareItems(int32 op, JsonbValue *jb1, JsonbValue *jb2)
{










































































}

/* Compare two numerics */
static int
compareNumeric(Numeric a, Numeric b)
{

}

static JsonbValue *
copyJsonbValue(JsonbValue *src)
{





}

/*
 * Execute array subscript expression and convert resulting numeric item to
 * the integer type with truncation.
 */
static JsonPathExecResult
getArrayIndex(JsonPathExecContext *cxt, JsonPathItem *jsp, JsonbValue *jb, int32 *index)
{


























}

/* Save base object and its id needed for the execution of .keyvalue(). */
static JsonBaseObjectInfo
setBaseObject(JsonPathExecContext *cxt, JsonbValue *jbv, int32 id)
{






}

static void
JsonValueListAppend(JsonValueList *jvl, JsonbValue *jbv)
{













}

static int
JsonValueListLength(const JsonValueList *jvl)
{

}

static bool
JsonValueListIsEmpty(JsonValueList *jvl)
{

}

static JsonbValue *
JsonValueListHead(JsonValueList *jvl)
{

}

static List *
JsonValueListGetList(JsonValueList *jvl)
{






}

static void
JsonValueListInitIterator(const JsonValueList *jvl, JsonValueListIterator *it)
{















}

/*
 * Get the next item from the sequence advancing iterator.
 */
static JsonbValue *
JsonValueListNext(const JsonValueList *jvl, JsonValueListIterator *it)
{













}

/*
 * Initialize a binary JsonbValue with the given jsonb container.
 */
static JsonbValue *
JsonbInitBinary(JsonbValue *jbv, Jsonb *jb)
{





}

/*
 * Returns jbv* type of of JsonbValue. Note, it never returns jbvBinary as is.
 */
static int
JsonbType(JsonbValue *jb)
{
























}

/* Get scalar of given type or NULL on type mismatch */
static JsonbValue *
getScalar(JsonbValue *scalar, enum jbvType type)
{




}

/* Construct a JSON array from the item list */
static JsonbValue *
wrapItemsInArray(const JsonValueList *items)
{













}