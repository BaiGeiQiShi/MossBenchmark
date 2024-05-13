/*-------------------------------------------------------------------------
 *
 * jsonb_gin.c
 *	 GIN support functions for jsonb
 *
 * Copyright (c) 2014-2019, PostgreSQL Global Development Group
 *
 * We provide two opclasses for jsonb indexing: jsonb_ops and jsonb_path_ops.
 * For their description see json.sgml and comments in jsonb.h.
 *
 * The operators support, among the others, "jsonb @? jsonpath" and
 * "jsonb @@ jsonpath".  Expressions containing these operators are easily
 * expressed through each other.
 *
 *	jb @? 'path' <=> jb @@ 'EXISTS(path)'
 *	jb @@ 'expr' <=> jb @? '$ ? (expr)'
 *
 * Thus, we're going to consider only @@ operator, while regarding @? operator
 * the same is true for jb @@ 'EXISTS(path)'.
 *
 * Result of jsonpath query extraction is a tree, which leaf nodes are index
 * entries and non-leaf nodes are AND/OR logical expressions.  Basically we
 * extract following statements out of jsonpath:
 *
 *	1) "accessors_chain = const",
 *	2) "EXISTS(accessors_chain)".
 *
 * Accessors chain may consist of .key, [*] and [index] accessors.  jsonb_ops
 * additionally supports .* and .**.
 *
 * For now, both jsonb_ops and jsonb_path_ops supports only statements of
 * the 1st find.  jsonb_ops might also support statements of the 2nd kind,
 * but given we have no statistics keys extracted from accessors chain
 * are likely non-selective.  Therefore, we choose to not confuse optimizer
 * and skip statements of the 2nd kind altogether.  In future versions that
 * might be changed.
 *
 * In jsonb_ops statement of the 1st kind is split into expression of AND'ed
 * keys and const.  Sometimes const might be interpreted as both value or key
 * in jsonb_ops.  Then statement of 1st kind is decomposed into the expression
 * below.
 *
 *	key1 AND key2 AND ... AND keyN AND (const_as_value OR const_as_key)
 *
 * jsonb_path_ops transforms each statement of the 1st kind into single hash
 * entry below.
 *
 *	HASH(key1, key2, ... , keyN, const)
 *
 * Despite statements of the 2nd kind are not supported by both jsonb_ops and
 * jsonb_path_ops, EXISTS(path) expressions might be still supported,
 * when statements of 1st kind could be extracted out of their filters.
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonb_gin.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin.h"
#include "access/stratnum.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/jsonb.h"
#include "utils/jsonpath.h"
#include "utils/varlena.h"

typedef struct PathHashStack
{
  uint32 hash;
  struct PathHashStack *parent;
} PathHashStack;

/* Buffer for GIN entries */
typedef struct GinEntries
{
  Datum *buf;
  int count;
  int allocated;
} GinEntries;

typedef enum JsonPathGinNodeType
{
  JSP_GIN_OR,
  JSP_GIN_AND,
  JSP_GIN_ENTRY
} JsonPathGinNodeType;

typedef struct JsonPathGinNode JsonPathGinNode;

/* Node in jsonpath expression tree */
struct JsonPathGinNode
{
  JsonPathGinNodeType type;
  union
  {
    int nargs;        /* valid for OR and AND nodes */
    int entryIndex;   /* index in GinEntries array, valid for ENTRY
                       * nodes after entries output */
    Datum entryDatum; /* path hash or key name/scalar, valid for
                       * ENTRY nodes before entries output */
  } val;
  JsonPathGinNode *args[FLEXIBLE_ARRAY_MEMBER]; /* valid for OR and AND
                                                 * nodes */
};

/*
 * jsonb_ops entry extracted from jsonpath item.  Corresponding path item
 * may be: '.key', '.*', '.**', '[index]' or '[*]'.
 * Entry type is stored in 'type' field.
 */
typedef struct JsonPathGinPathItem
{
  struct JsonPathGinPathItem *parent;
  Datum keyName;         /* key name (for '.key' path item) or NULL */
  JsonPathItemType type; /* type of jsonpath item */
} JsonPathGinPathItem;

/* GIN representation of the extracted json path */
typedef union JsonPathGinPath
{
  JsonPathGinPathItem *items; /* list of path items (jsonb_ops) */
  uint32 hash;                /* hash of the path (jsonb_path_ops) */
} JsonPathGinPath;

typedef struct JsonPathGinContext JsonPathGinContext;

/* Callback, which stores information about path item into JsonPathGinPath */
typedef bool (*JsonPathGinAddPathItemFunc)(JsonPathGinPath *path, JsonPathItem *jsp);

/*
 * Callback, which extracts set of nodes from statement of 1st kind
 * (scalar != NULL) or statement of 2nd kind (scalar == NULL).
 */
typedef List *(*JsonPathGinExtractNodesFunc)(JsonPathGinContext *cxt, JsonPathGinPath path, JsonbValue *scalar, List *nodes);

/* Context for jsonpath entries extraction */
struct JsonPathGinContext
{
  JsonPathGinAddPathItemFunc add_path_item;
  JsonPathGinExtractNodesFunc extract_nodes;
  bool lax;
};

static Datum
make_text_key(char flag, const char *str, int len);
static Datum
make_scalar_key(const JsonbValue *scalarVal, bool is_key);

static JsonPathGinNode *
extract_jsp_bool_expr(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, bool not );

/* Initialize GinEntries struct */
static void
init_gin_entries(GinEntries *entries, int preallocated)
{



}

/* Add new entry to GinEntries */
static int
add_gin_entry(GinEntries *entries, Datum entry)
{



















}

/*
 *
 * jsonb_ops GIN opclass support functions
 *
 */

Datum
gin_compare_jsonb(PG_FUNCTION_ARGS)
{



















}

Datum
gin_extract_jsonb(PG_FUNCTION_ARGS)
{











































}

/* Append JsonPathGinPathItem to JsonPathGinPath (jsonb_ops) */
static bool
jsonb_ops__add_path_item(JsonPathGinPath *path, JsonPathItem *jsp)
{







































}

/* Combine existing path hash with next key hash (jsonb_path_ops) */
static bool
jsonb_path_ops__add_path_item(JsonPathGinPath *path, JsonPathItem *jsp)
{

























}

static JsonPathGinNode *
make_jsp_entry_node(Datum entry)
{






}

static JsonPathGinNode *
make_jsp_entry_node_scalar(JsonbValue *scalar, bool iskey)
{

}

static JsonPathGinNode *
make_jsp_expr_node(JsonPathGinNodeType type, int nargs)
{






}

static JsonPathGinNode *
make_jsp_expr_node_args(JsonPathGinNodeType type, List *args)
{










}

static JsonPathGinNode *
make_jsp_expr_node_binary(JsonPathGinNodeType type, JsonPathGinNode *arg1, JsonPathGinNode *arg2)
{






}

/* Append a list of nodes from the jsonpath (jsonb_ops). */
static List *
jsonb_ops__extract_nodes(JsonPathGinContext *cxt, JsonPathGinPath path, JsonbValue *scalar, List *nodes)
{










































































}

/* Append a list of nodes from the jsonpath (jsonb_path_ops). */
static List *
jsonb_path_ops__extract_nodes(JsonPathGinContext *cxt, JsonPathGinPath path, JsonbValue *scalar, List *nodes)
{














}

/*
 * Extract a list of expression nodes that need to be AND-ed by the caller.
 * Extracted expression is 'path == scalar' if 'scalar' is non-NULL, and
 * 'EXISTS(path)' otherwise.
 */
static List *
extract_jsp_path_expr_nodes(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, JsonbValue *scalar)
{





















































}

/*
 * Extract an expression node from one of following jsonpath path expressions:
 *   EXISTS(jsp)    (when 'scalar' is NULL)
 *   jsp == scalar  (when 'scalar' is not NULL).
 *
 * The current path (@) is passed in 'path'.
 */
static JsonPathGinNode *
extract_jsp_path_expr(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, JsonbValue *scalar)
{
















}

/* Recursively extract nodes from the boolean jsonpath expression. */
static JsonPathGinNode *
extract_jsp_bool_expr(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, bool not )
{






































































































































}

/* Recursively emit all GIN entries found in the node tree */
static void
emit_jsp_gin_entries(JsonPathGinNode *node, GinEntries *entries)
{






















}

/*
 * Recursively extract GIN entries from jsonpath query.
 * Root expression node is put into (*extra_data)[0].
 */
static Datum *
extract_jsp_query(JsonPath *jp, StrategyNumber strat, bool pathOps, int32 *nentries, Pointer **extra_data)
{









































}

/*
 * Recursively execute jsonpath expression.
 * 'check' is a bool[] or a GinTernaryValue[] depending on 'ternary' flag.
 */
static GinTernaryValue
execute_jsp_gin_node(JsonPathGinNode *node, void *check, bool ternary)
{
























































}

Datum
gin_extract_jsonb_query(PG_FUNCTION_ARGS)
{









































































}

Datum
gin_consistent_jsonb(PG_FUNCTION_ARGS)
{















































































}

Datum
gin_triconsistent_jsonb(PG_FUNCTION_ARGS)
{



























































}

/*
 *
 * jsonb_path_ops GIN opclass support functions
 *
 * In a jsonb_path_ops index, the GIN keys are uint32 hashes, one per JSON
 * value; but the JSON key(s) leading to each value are also included in its
 * hash computation.  This means we can only support containment queries,
 * but the index can distinguish, for example, {"foo": 42} from {"bar": 42}
 * since different hashes will be generated.
 *
 */

Datum
gin_extract_jsonb_path(PG_FUNCTION_ARGS)
{

























































































}

Datum
gin_extract_jsonb_query_path(PG_FUNCTION_ARGS)
{



































}

Datum
gin_consistent_jsonb_path(PG_FUNCTION_ARGS)
{















































}

Datum
gin_triconsistent_jsonb_path(PG_FUNCTION_ARGS)
{













































}

/*
 * Construct a jsonb_ops GIN key from a flag byte and a textual representation
 * (which need not be null-terminated).  This function is responsible
 * for hashing overlength text representations; it will add the
 * JGINFLAG_HASHED bit to the flag value if it does that.
 */
static Datum
make_text_key(char flag, const char *str, int len)
{



























}

/*
 * Create a textual representation of a JsonbValue that will serve as a GIN
 * key in a jsonb_ops index.  is_key is true if the JsonbValue is a key,
 * or if it is a string array element (since we pretend those are keys,
 * see jsonb.h).
 */
static Datum
make_scalar_key(const JsonbValue *scalarVal, bool is_key)
{








































}