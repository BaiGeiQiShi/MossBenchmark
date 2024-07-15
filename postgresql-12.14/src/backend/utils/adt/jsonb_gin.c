                                                                            
   
               
                                    
   
                                                                
   
                                                                              
                                                                
   
                                                                    
                                                                           
                                 
   
                                         
                                       
   
                                                                               
                                              
   
                                                                             
                                                                            
                                                 
   
                                 
                                 
   
                                                                              
                                     
   
                                                                          
                                                                           
                                                                       
                                                                            
                                                                            
                     
   
                                                                             
                                                                              
                                                                               
          
   
                                                                       
   
                                                                             
                
   
                                       
   
                                                                              
                                                                      
                                                                        
   
                  
                                       
   
                                                                            
   

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

                                      
struct JsonPathGinNode
{
  JsonPathGinNodeType type;
  union
  {
    int nargs;                                        
    int entryIndex;                                                 
                                                      
    Datum entryDatum;                                            
                                                             
  } val;
  JsonPathGinNode *args[FLEXIBLE_ARRAY_MEMBER];                         
                                                           
};

   
                                                                          
                                                    
                                         
   
typedef struct JsonPathGinPathItem
{
  struct JsonPathGinPathItem *parent;
  Datum keyName;                                                      
  JsonPathItemType type;                            
} JsonPathGinPathItem;

                                                   
typedef union JsonPathGinPath
{
  JsonPathGinPathItem *items;                                     
  uint32 hash;                                                       
} JsonPathGinPath;

typedef struct JsonPathGinContext JsonPathGinContext;

                                                                             
typedef bool (*JsonPathGinAddPathItemFunc)(JsonPathGinPath *path, JsonPathItem *jsp);

   
                                                                    
                                                               
   
typedef List *(*JsonPathGinExtractNodesFunc)(JsonPathGinContext *cxt, JsonPathGinPath path, JsonbValue *scalar, List *nodes);

                                             
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

                                  
static void
init_gin_entries(GinEntries *entries, int preallocated)
{
  entries->allocated = preallocated;
  entries->buf = preallocated ? palloc(sizeof(Datum) * preallocated) : NULL;
  entries->count = 0;
}

                                 
static int
add_gin_entry(GinEntries *entries, Datum entry)
{
  int id = entries->count;

  if (entries->count >= entries->allocated)
  {
    if (entries->allocated)
    {
      entries->allocated *= 2;
      entries->buf = repalloc(entries->buf, sizeof(Datum) * entries->allocated);
    }
    else
    {
      entries->allocated = 8;
      entries->buf = palloc(sizeof(Datum) * entries->allocated);
    }
  }

  entries->buf[entries->count++] = entry;

  return id;
}

   
   
                                           
   
   

Datum
gin_compare_jsonb(PG_FUNCTION_ARGS)
{
  text *arg1 = PG_GETARG_TEXT_PP(0);
  text *arg2 = PG_GETARG_TEXT_PP(1);
  int32 result;
  char *a1p, *a2p;
  int len1, len2;

  a1p = VARDATA_ANY(arg1);
  a2p = VARDATA_ANY(arg2);

  len1 = VARSIZE_ANY_EXHDR(arg1);
  len2 = VARSIZE_ANY_EXHDR(arg2);

                                                                    
  result = varstr_cmp(a1p, len1, a2p, len2, C_COLLATION_OID);

  PG_FREE_IF_COPY(arg1, 0);
  PG_FREE_IF_COPY(arg2, 1);

  PG_RETURN_INT32(result);
}

Datum
gin_extract_jsonb(PG_FUNCTION_ARGS)
{
  Jsonb *jb = (Jsonb *)PG_GETARG_JSONB_P(0);
  int32 *nentries = (int32 *)PG_GETARG_POINTER(1);
  int total = JB_ROOT_COUNT(jb);
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken r;
  GinEntries entries;

                                                             
  if (total == 0)
  {
    *nentries = 0;
    PG_RETURN_POINTER(NULL);
  }

                                                                        
  init_gin_entries(&entries, 2 * total);

  it = JsonbIteratorInit(&jb->root);

  while ((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    switch (r)
    {
    case WJB_KEY:
      add_gin_entry(&entries, make_scalar_key(&v, true));
      break;
    case WJB_ELEM:
                                                               
      add_gin_entry(&entries, make_scalar_key(&v, v.type == jbvString));
      break;
    case WJB_VALUE:
      add_gin_entry(&entries, make_scalar_key(&v, false));
      break;
    default:
                                          
      break;
    }
  }

  *nentries = entries.count;

  PG_RETURN_POINTER(entries.buf);
}

                                                               
static bool
jsonb_ops__add_path_item(JsonPathGinPath *path, JsonPathItem *jsp)
{
  JsonPathGinPathItem *pentry;
  Datum keyName;

  switch (jsp->type)
  {
  case jpiRoot:
    path->items = NULL;                 
    return true;

  case jpiKey:
  {
    int len;
    char *key = jspGetString(jsp, &len);

    keyName = make_text_key(JGINFLAG_KEY, key, len);
    break;
  }

  case jpiAny:
  case jpiAnyKey:
  case jpiAnyArray:
  case jpiIndexArray:
    keyName = PointerGetDatum(NULL);
    break;

  default:
                                                              
    return false;
  }

  pentry = palloc(sizeof(*pentry));

  pentry->type = jsp->type;
  pentry->keyName = keyName;
  pentry->parent = path->items;

  path->items = pentry;

  return true;
}

                                                                    
static bool
jsonb_path_ops__add_path_item(JsonPathGinPath *path, JsonPathItem *jsp)
{
  switch (jsp->type)
  {
  case jpiRoot:
    path->hash = 0;                      
    return true;

  case jpiKey:
  {
    JsonbValue jbv;

    jbv.type = jbvString;
    jbv.val.string.val = jspGetString(jsp, &jbv.val.string.len);

    JsonbHashScalarValue(&jbv, &path->hash);
    return true;
  }

  case jpiIndexArray:
  case jpiAnyArray:
    return true;                             

  default:
                                                                      
    return false;
  }
}

static JsonPathGinNode *
make_jsp_entry_node(Datum entry)
{
  JsonPathGinNode *node = palloc(offsetof(JsonPathGinNode, args));

  node->type = JSP_GIN_ENTRY;
  node->val.entryDatum = entry;

  return node;
}

static JsonPathGinNode *
make_jsp_entry_node_scalar(JsonbValue *scalar, bool iskey)
{
  return make_jsp_entry_node(make_scalar_key(scalar, iskey));
}

static JsonPathGinNode *
make_jsp_expr_node(JsonPathGinNodeType type, int nargs)
{
  JsonPathGinNode *node = palloc(offsetof(JsonPathGinNode, args) + sizeof(node->args[0]) * nargs);

  node->type = type;
  node->val.nargs = nargs;

  return node;
}

static JsonPathGinNode *
make_jsp_expr_node_args(JsonPathGinNodeType type, List *args)
{
  JsonPathGinNode *node = make_jsp_expr_node(type, list_length(args));
  ListCell *lc;
  int i = 0;

  foreach (lc, args)
  {
    node->args[i++] = lfirst(lc);
  }

  return node;
}

static JsonPathGinNode *
make_jsp_expr_node_binary(JsonPathGinNodeType type, JsonPathGinNode *arg1, JsonPathGinNode *arg2)
{
  JsonPathGinNode *node = make_jsp_expr_node(type, 2);

  node->args[0] = arg1;
  node->args[1] = arg2;

  return node;
}

                                                           
static List *
jsonb_ops__extract_nodes(JsonPathGinContext *cxt, JsonPathGinPath path, JsonbValue *scalar, List *nodes)
{
  JsonPathGinPathItem *pentry;

  if (scalar)
  {
    JsonPathGinNode *node;

       
                                                                       
                            
       
    for (pentry = path.items; pentry; pentry = pentry->parent)
    {
      if (pentry->type == jpiKey)                            
      {
        nodes = lappend(nodes, make_jsp_entry_node(pentry->keyName));
      }
    }

                                                  
    if (scalar->type == jbvString)
    {
      JsonPathGinPathItem *last = path.items;
      GinTernaryValue key_entry;

         
                                                                     
                                                                         
                                                                    
                                                                         
                       
         

      if (cxt->lax)
      {
        key_entry = GIN_MAYBE;
      }
      else if (!last)               
      {
        key_entry = GIN_FALSE;
      }
      else if (last->type == jpiAnyArray || last->type == jpiIndexArray)
      {
        key_entry = GIN_TRUE;
      }
      else if (last->type == jpiAny)
      {
        key_entry = GIN_MAYBE;
      }
      else
      {
        key_entry = GIN_FALSE;
      }

      if (key_entry == GIN_MAYBE)
      {
        JsonPathGinNode *n1 = make_jsp_entry_node_scalar(scalar, true);
        JsonPathGinNode *n2 = make_jsp_entry_node_scalar(scalar, false);

        node = make_jsp_expr_node_binary(JSP_GIN_OR, n1, n2);
      }
      else
      {
        node = make_jsp_entry_node_scalar(scalar, key_entry == GIN_TRUE);
      }
    }
    else
    {
      node = make_jsp_entry_node_scalar(scalar, false);
    }

    nodes = lappend(nodes, node);
  }

  return nodes;
}

                                                                
static List *
jsonb_path_ops__extract_nodes(JsonPathGinContext *cxt, JsonPathGinPath path, JsonbValue *scalar, List *nodes)
{
  if (scalar)
  {
                                                    
    uint32 hash = path.hash;

    JsonbHashScalarValue(scalar, &hash);

    return lappend(nodes, make_jsp_entry_node(UInt32GetDatum(hash)));
  }
  else
  {
                                                                            
    return nodes;
  }
}

   
                                                                            
                                                                         
                             
   
static List *
extract_jsp_path_expr_nodes(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, JsonbValue *scalar)
{
  JsonPathItem next;
  List *nodes = NIL;

  for (;;)
  {
    switch (jsp->type)
    {
    case jpiCurrent:
      break;

    case jpiFilter:
    {
      JsonPathItem arg;
      JsonPathGinNode *filter;

      jspGetArg(jsp, &arg);

      filter = extract_jsp_bool_expr(cxt, path, &arg, false);

      if (filter)
      {
        nodes = lappend(nodes, filter);
      }

      break;
    }

    default:
      if (!cxt->add_path_item(&path, jsp))
      {

           
                                                                   
                                       
           
        return nodes;
      }
      break;
    }

    if (!jspGetNext(jsp, &next))
    {
      break;
    }

    jsp = &next;
  }

     
                                                                           
                           
     
  return cxt->extract_nodes(cxt, path, scalar, nodes);
}

   
                                                                               
                                            
                                                 
   
                                             
   
static JsonPathGinNode *
extract_jsp_path_expr(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, JsonbValue *scalar)
{
                                            
  List *nodes = extract_jsp_path_expr_nodes(cxt, path, jsp, scalar);

  if (list_length(nodes) <= 0)
  {
                                                                      
    return NULL;
  }

  if (list_length(nodes) == 1)
  {
    return linitial(nodes);                           
  }

                                                
  return make_jsp_expr_node_args(JSP_GIN_AND, nodes);
}

                                                                     
static JsonPathGinNode *
extract_jsp_bool_expr(JsonPathGinContext *cxt, JsonPathGinPath path, JsonPathItem *jsp, bool not )
{
  check_stack_depth();

  switch (jsp->type)
  {
  case jpiAnd:                   
  case jpiOr:                    
  {
    JsonPathItem arg;
    JsonPathGinNode *larg;
    JsonPathGinNode *rarg;
    JsonPathGinNodeType type;

    jspGetLeftArg(jsp, &arg);
    larg = extract_jsp_bool_expr(cxt, path, &arg, not );

    jspGetRightArg(jsp, &arg);
    rarg = extract_jsp_bool_expr(cxt, path, &arg, not );

    if (!larg || !rarg)
    {
      if (jsp->type == jpiOr)
      {
        return NULL;
      }

      return larg ? larg : rarg;
    }

    type = not ^(jsp->type == jpiAnd) ? JSP_GIN_AND : JSP_GIN_OR;

    return make_jsp_expr_node_binary(type, larg, rarg);
  }

  case jpiNot:             
  {
    JsonPathItem arg;

    jspGetArg(jsp, &arg);

                                                       
    return extract_jsp_bool_expr(cxt, path, &arg, !not );
  }

  case jpiExists:                   
  {
    JsonPathItem arg;

    if (not )
    {
      return NULL;                                  
    }

    jspGetArg(jsp, &arg);

    return extract_jsp_path_expr(cxt, path, &arg, NULL);
  }

  case jpiNotEqual:

       
                                                                   
                                                                     
                                                                       
                                                                 
                                                                     
                                                                       
                                                                 
                                                                      
                                                                 
                                                                   
              
       
    return NULL;

  case jpiEqual:                     
  {
    JsonPathItem left_item;
    JsonPathItem right_item;
    JsonPathItem *path_item;
    JsonPathItem *scalar_item;
    JsonbValue scalar;

    if (not )
    {
      return NULL;
    }

    jspGetLeftArg(jsp, &left_item);
    jspGetRightArg(jsp, &right_item);

    if (jspIsScalar(left_item.type))
    {
      scalar_item = &left_item;
      path_item = &right_item;
    }
    else if (jspIsScalar(right_item.type))
    {
      scalar_item = &right_item;
      path_item = &left_item;
    }
    else
    {
      return NULL;                                              
    }

    switch (scalar_item->type)
    {
    case jpiNull:
      scalar.type = jbvNull;
      break;
    case jpiBool:
      scalar.type = jbvBool;
      scalar.val.boolean = !!*scalar_item->content.value.data;
      break;
    case jpiNumeric:
      scalar.type = jbvNumeric;
      scalar.val.numeric = (Numeric)scalar_item->content.value.data;
      break;
    case jpiString:
      scalar.type = jbvString;
      scalar.val.string.val = scalar_item->content.value.data;
      scalar.val.string.len = scalar_item->content.value.datalen;
      break;
    default:
      elog(ERROR, "invalid scalar jsonpath item type: %d", scalar_item->type);
      return NULL;
    }

    return extract_jsp_path_expr(cxt, path, path_item, &scalar);
  }

  default:
    return NULL;                               
  }
}

                                                             
static void
emit_jsp_gin_entries(JsonPathGinNode *node, GinEntries *entries)
{
  check_stack_depth();

  switch (node->type)
  {
  case JSP_GIN_ENTRY:
                                                   
    node->val.entryIndex = add_gin_entry(entries, node->val.entryDatum);
    break;

  case JSP_GIN_OR:
  case JSP_GIN_AND:
  {
    int i;

    for (i = 0; i < node->val.nargs; i++)
    {
      emit_jsp_gin_entries(node->args[i], entries);
    }

    break;
  }
  }
}

   
                                                        
                                                      
   
static Datum *
extract_jsp_query(JsonPath *jp, StrategyNumber strat, bool pathOps, int32 *nentries, Pointer **extra_data)
{
  JsonPathGinContext cxt;
  JsonPathItem root;
  JsonPathGinNode *node;
  JsonPathGinPath path = {0};
  GinEntries entries = {0};

  cxt.lax = (jp->header & JSONPATH_LAX) != 0;

  if (pathOps)
  {
    cxt.add_path_item = jsonb_path_ops__add_path_item;
    cxt.extract_nodes = jsonb_path_ops__extract_nodes;
  }
  else
  {
    cxt.add_path_item = jsonb_ops__add_path_item;
    cxt.extract_nodes = jsonb_ops__extract_nodes;
  }

  jspInit(&root, jp);

  node = strat == JsonbJsonpathExistsStrategyNumber ? extract_jsp_path_expr(&cxt, path, &root, NULL) : extract_jsp_bool_expr(&cxt, path, &root, false);

  if (!node)
  {
    *nentries = 0;
    return NULL;
  }

  emit_jsp_gin_entries(node, &entries);

  *nentries = entries.count;
  if (!*nentries)
  {
    return NULL;
  }

  *extra_data = palloc0(sizeof(**extra_data) * entries.count);
  **extra_data = (Pointer)node;

  return entries.buf;
}

   
                                            
                                                                           
   
static GinTernaryValue
execute_jsp_gin_node(JsonPathGinNode *node, void *check, bool ternary)
{
  GinTernaryValue res;
  GinTernaryValue v;
  int i;

  switch (node->type)
  {
  case JSP_GIN_AND:
    res = GIN_TRUE;
    for (i = 0; i < node->val.nargs; i++)
    {
      v = execute_jsp_gin_node(node->args[i], check, ternary);
      if (v == GIN_FALSE)
      {
        return GIN_FALSE;
      }
      else if (v == GIN_MAYBE)
      {
        res = GIN_MAYBE;
      }
    }
    return res;

  case JSP_GIN_OR:
    res = GIN_FALSE;
    for (i = 0; i < node->val.nargs; i++)
    {
      v = execute_jsp_gin_node(node->args[i], check, ternary);
      if (v == GIN_TRUE)
      {
        return GIN_TRUE;
      }
      else if (v == GIN_MAYBE)
      {
        res = GIN_MAYBE;
      }
    }
    return res;

  case JSP_GIN_ENTRY:
  {
    int index = node->val.entryIndex;

    if (ternary)
    {
      return ((GinTernaryValue *)check)[index];
    }
    else
    {
      return ((bool *)check)[index] ? GIN_TRUE : GIN_FALSE;
    }
  }

  default:
    elog(ERROR, "invalid jsonpath gin node type: %d", node->type);
    return GIN_FALSE;                          
  }
}

Datum
gin_extract_jsonb_query(PG_FUNCTION_ARGS)
{
  int32 *nentries = (int32 *)PG_GETARG_POINTER(1);
  StrategyNumber strategy = PG_GETARG_UINT16(2);
  int32 *searchMode = (int32 *)PG_GETARG_POINTER(6);
  Datum *entries;

  if (strategy == JsonbContainsStrategyNumber)
  {
                                                              
    entries = (Datum *)DatumGetPointer(DirectFunctionCall2(gin_extract_jsonb, PG_GETARG_DATUM(0), PointerGetDatum(nentries)));
                                                              
    if (*nentries == 0)
    {
      *searchMode = GIN_SEARCH_MODE_ALL;
    }
  }
  else if (strategy == JsonbExistsStrategyNumber)
  {
                                                         
    text *query = PG_GETARG_TEXT_PP(0);

    *nentries = 1;
    entries = (Datum *)palloc(sizeof(Datum));
    entries[0] = make_text_key(JGINFLAG_KEY, VARDATA_ANY(query), VARSIZE_ANY_EXHDR(query));
  }
  else if (strategy == JsonbExistsAnyStrategyNumber || strategy == JsonbExistsAllStrategyNumber)
  {
                                                                 
    ArrayType *query = PG_GETARG_ARRAYTYPE_P(0);
    Datum *key_datums;
    bool *key_nulls;
    int key_count;
    int i, j;

    deconstruct_array(query, TEXTOID, -1, false, 'i', &key_datums, &key_nulls, &key_count);

    entries = (Datum *)palloc(sizeof(Datum) * key_count);

    for (i = 0, j = 0; i < key_count; i++)
    {
                                          
      if (key_nulls[i])
      {
        continue;
      }
      entries[j++] = make_text_key(JGINFLAG_KEY, VARDATA(key_datums[i]), VARSIZE(key_datums[i]) - VARHDRSZ);
    }

    *nentries = j;
                                                        
    if (j == 0 && strategy == JsonbExistsAllStrategyNumber)
    {
      *searchMode = GIN_SEARCH_MODE_ALL;
    }
  }
  else if (strategy == JsonbJsonpathPredicateStrategyNumber || strategy == JsonbJsonpathExistsStrategyNumber)
  {
    JsonPath *jp = PG_GETARG_JSONPATH_P(0);
    Pointer **extra_data = (Pointer **)PG_GETARG_POINTER(4);

    entries = extract_jsp_query(jp, strategy, false, nentries, extra_data);

    if (!entries)
    {
      *searchMode = GIN_SEARCH_MODE_ALL;
    }
  }
  else
  {
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    entries = NULL;                          
  }

  PG_RETURN_POINTER(entries);
}

Datum
gin_consistent_jsonb(PG_FUNCTION_ARGS)
{
  bool *check = (bool *)PG_GETARG_POINTER(0);
  StrategyNumber strategy = PG_GETARG_UINT16(1);

                                               
  int32 nkeys = PG_GETARG_INT32(3);

  Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
  bool *recheck = (bool *)PG_GETARG_POINTER(5);
  bool res = true;
  int32 i;

  if (strategy == JsonbContainsStrategyNumber)
  {
       
                                                                          
                                                                           
                                                                        
                                                                          
                                                                        
                                    
       
    *recheck = true;
    for (i = 0; i < nkeys; i++)
    {
      if (!check[i])
      {
        res = false;
        break;
      }
    }
  }
  else if (strategy == JsonbExistsStrategyNumber)
  {
       
                                                                           
                                                                          
                                                                          
                                                                    
                                                                          
       
    *recheck = true;
    res = true;
  }
  else if (strategy == JsonbExistsAnyStrategyNumber)
  {
                                              
    *recheck = true;
    res = true;
  }
  else if (strategy == JsonbExistsAllStrategyNumber)
  {
                                              
    *recheck = true;
                                                                     
    for (i = 0; i < nkeys; i++)
    {
      if (!check[i])
      {
        res = false;
        break;
      }
    }
  }
  else if (strategy == JsonbJsonpathPredicateStrategyNumber || strategy == JsonbJsonpathExistsStrategyNumber)
  {
    *recheck = true;

    if (nkeys > 0)
    {
      Assert(extra_data && extra_data[0]);
      res = execute_jsp_gin_node((JsonPathGinNode *)extra_data[0], check, false) != GIN_FALSE;
    }
  }
  else
  {
    elog(ERROR, "unrecognized strategy number: %d", strategy);
  }

  PG_RETURN_BOOL(res);
}

Datum
gin_triconsistent_jsonb(PG_FUNCTION_ARGS)
{
  GinTernaryValue *check = (GinTernaryValue *)PG_GETARG_POINTER(0);
  StrategyNumber strategy = PG_GETARG_UINT16(1);

                                               
  int32 nkeys = PG_GETARG_INT32(3);
  Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
  GinTernaryValue res = GIN_MAYBE;
  int32 i;

     
                                                                           
                                                                     
                                             
     
  if (strategy == JsonbContainsStrategyNumber || strategy == JsonbExistsAllStrategyNumber)
  {
                                            
    for (i = 0; i < nkeys; i++)
    {
      if (check[i] == GIN_FALSE)
      {
        res = GIN_FALSE;
        break;
      }
    }
  }
  else if (strategy == JsonbExistsStrategyNumber || strategy == JsonbExistsAnyStrategyNumber)
  {
                                                    
    res = GIN_FALSE;
    for (i = 0; i < nkeys; i++)
    {
      if (check[i] == GIN_TRUE || check[i] == GIN_MAYBE)
      {
        res = GIN_MAYBE;
        break;
      }
    }
  }
  else if (strategy == JsonbJsonpathPredicateStrategyNumber || strategy == JsonbJsonpathExistsStrategyNumber)
  {
    if (nkeys > 0)
    {
      Assert(extra_data && extra_data[0]);
      res = execute_jsp_gin_node((JsonPathGinNode *)extra_data[0], check, true);

                                            
      if (res == GIN_TRUE)
      {
        res = GIN_MAYBE;
      }
    }
  }
  else
  {
    elog(ERROR, "unrecognized strategy number: %d", strategy);
  }

  PG_RETURN_GIN_TERNARY_VALUE(res);
}

   
   
                                                
   
                                                                           
                                                                             
                                                                          
                                                                            
                                             
   
   

Datum
gin_extract_jsonb_path(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  int32 *nentries = (int32 *)PG_GETARG_POINTER(1);
  int total = JB_ROOT_COUNT(jb);
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken r;
  PathHashStack tail;
  PathHashStack *stack;
  GinEntries entries;

                                                             
  if (total == 0)
  {
    *nentries = 0;
    PG_RETURN_POINTER(NULL);
  }

                                                                        
  init_gin_entries(&entries, 2 * total);

                                                                            
  tail.parent = NULL;
  tail.hash = 0;
  stack = &tail;

  it = JsonbIteratorInit(&jb->root);

  while ((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    PathHashStack *parent;

    switch (r)
    {
    case WJB_BEGIN_ARRAY:
    case WJB_BEGIN_OBJECT:
                                              
      parent = stack;
      stack = (PathHashStack *)palloc(sizeof(PathHashStack));

         
                                                                  
                                                                 
                                 
         
                                                              
                                                              
                          
         
      stack->hash = parent->hash;
      stack->parent = parent;
      break;
    case WJB_KEY:
                                                    
      JsonbHashScalarValue(&v, &stack->hash);
                                                      
      break;
    case WJB_ELEM:
    case WJB_VALUE:
                                                                  
      JsonbHashScalarValue(&v, &stack->hash);
                                   
      add_gin_entry(&entries, UInt32GetDatum(stack->hash));
                                                         
      stack->hash = stack->parent->hash;
      break;
    case WJB_END_ARRAY:
    case WJB_END_OBJECT:
                         
      parent = stack->parent;
      pfree(stack);
      stack = parent;
                                                         
      if (stack->parent)
      {
        stack->hash = stack->parent->hash;
      }
      else
      {
        stack->hash = 0;
      }
      break;
    default:
      elog(ERROR, "invalid JsonbIteratorNext rc: %d", (int)r);
    }
  }

  *nentries = entries.count;

  PG_RETURN_POINTER(entries.buf);
}

Datum
gin_extract_jsonb_query_path(PG_FUNCTION_ARGS)
{
  int32 *nentries = (int32 *)PG_GETARG_POINTER(1);
  StrategyNumber strategy = PG_GETARG_UINT16(2);
  int32 *searchMode = (int32 *)PG_GETARG_POINTER(6);
  Datum *entries;

  if (strategy == JsonbContainsStrategyNumber)
  {
                                                                    
    entries = (Datum *)DatumGetPointer(DirectFunctionCall2(gin_extract_jsonb_path, PG_GETARG_DATUM(0), PointerGetDatum(nentries)));

                                                               
    if (*nentries == 0)
    {
      *searchMode = GIN_SEARCH_MODE_ALL;
    }
  }
  else if (strategy == JsonbJsonpathPredicateStrategyNumber || strategy == JsonbJsonpathExistsStrategyNumber)
  {
    JsonPath *jp = PG_GETARG_JSONPATH_P(0);
    Pointer **extra_data = (Pointer **)PG_GETARG_POINTER(4);

    entries = extract_jsp_query(jp, strategy, true, nentries, extra_data);

    if (!entries)
    {
      *searchMode = GIN_SEARCH_MODE_ALL;
    }
  }
  else
  {
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    entries = NULL;
  }

  PG_RETURN_POINTER(entries);
}

Datum
gin_consistent_jsonb_path(PG_FUNCTION_ARGS)
{
  bool *check = (bool *)PG_GETARG_POINTER(0);
  StrategyNumber strategy = PG_GETARG_UINT16(1);

                                               
  int32 nkeys = PG_GETARG_INT32(3);
  Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
  bool *recheck = (bool *)PG_GETARG_POINTER(5);
  bool res = true;
  int32 i;

  if (strategy == JsonbContainsStrategyNumber)
  {
       
                                                                     
                                                                
                                                                           
                                                                       
                                                                      
                                                                      
                                
       
    *recheck = true;
    for (i = 0; i < nkeys; i++)
    {
      if (!check[i])
      {
        res = false;
        break;
      }
    }
  }
  else if (strategy == JsonbJsonpathPredicateStrategyNumber || strategy == JsonbJsonpathExistsStrategyNumber)
  {
    *recheck = true;

    if (nkeys > 0)
    {
      Assert(extra_data && extra_data[0]);
      res = execute_jsp_gin_node((JsonPathGinNode *)extra_data[0], check, false) != GIN_FALSE;
    }
  }
  else
  {
    elog(ERROR, "unrecognized strategy number: %d", strategy);
  }

  PG_RETURN_BOOL(res);
}

Datum
gin_triconsistent_jsonb_path(PG_FUNCTION_ARGS)
{
  GinTernaryValue *check = (GinTernaryValue *)PG_GETARG_POINTER(0);
  StrategyNumber strategy = PG_GETARG_UINT16(1);

                                               
  int32 nkeys = PG_GETARG_INT32(3);
  Pointer *extra_data = (Pointer *)PG_GETARG_POINTER(4);
  GinTernaryValue res = GIN_MAYBE;
  int32 i;

  if (strategy == JsonbContainsStrategyNumber)
  {
       
                                                                        
                                                                 
                                                          
       
    for (i = 0; i < nkeys; i++)
    {
      if (check[i] == GIN_FALSE)
      {
        res = GIN_FALSE;
        break;
      }
    }
  }
  else if (strategy == JsonbJsonpathPredicateStrategyNumber || strategy == JsonbJsonpathExistsStrategyNumber)
  {
    if (nkeys > 0)
    {
      Assert(extra_data && extra_data[0]);
      res = execute_jsp_gin_node((JsonPathGinNode *)extra_data[0], check, true);

                                            
      if (res == GIN_TRUE)
      {
        res = GIN_MAYBE;
      }
    }
  }
  else
  {
    elog(ERROR, "unrecognized strategy number: %d", strategy);
  }

  PG_RETURN_GIN_TERNARY_VALUE(res);
}

   
                                                                               
                                                                      
                                                                
                                                          
   
static Datum
make_text_key(char flag, const char *str, int len)
{
  text *item;
  char hashbuf[10];

  if (len > JGIN_MAXLENGTH)
  {
    uint32 hashval;

    hashval = DatumGetUInt32(hash_any((const unsigned char *)str, len));
    snprintf(hashbuf, sizeof(hashbuf), "%08x", hashval);
    str = hashbuf;
    len = 8;
    flag |= JGINFLAG_HASHED;
  }

     
                                                                        
                                                                           
                                             
     
  item = (text *)palloc(VARHDRSZ + len + 1);
  SET_VARSIZE(item, VARHDRSZ + len + 1);

  *VARDATA(item) = flag;

  memcpy(VARDATA(item) + 1, str, len);

  return PointerGetDatum(item);
}

   
                                                                            
                                                                         
                                                                        
                 
   
static Datum
make_scalar_key(const JsonbValue *scalarVal, bool is_key)
{
  Datum item;
  char *cstr;

  switch (scalarVal->type)
  {
  case jbvNull:
    Assert(!is_key);
    item = make_text_key(JGINFLAG_NULL, "", 0);
    break;
  case jbvBool:
    Assert(!is_key);
    item = make_text_key(JGINFLAG_BOOL, scalarVal->val.boolean ? "t" : "f", 1);
    break;
  case jbvNumeric:
    Assert(!is_key);

       
                                                                     
                                                                       
                
       
                                                                     
                                                                       
                                                                    
                                 
       
    cstr = numeric_normalize(scalarVal->val.numeric);
    item = make_text_key(JGINFLAG_NUM, cstr, strlen(cstr));
    pfree(cstr);
    break;
  case jbvString:
    item = make_text_key(is_key ? JGINFLAG_KEY : JGINFLAG_STR, scalarVal->val.string.val, scalarVal->val.string.len);
    break;
  default:
    elog(ERROR, "unrecognized jsonb scalar type: %d", scalarVal->type);
    item = 0;                          
    break;
  }

  return item;
}
