                                                                            
   
              
                                                      
   
                                                                              
                                                                          
                                                                                
                 
   
                                                                
   
                  
   
                      
   
                              
   
                                                                      
                                                                            
   
                                                   
   
                                           
   
                      
                
           \
                                            
                  
                  
                    \
                          
                  
   
                                                                         
                                                                           
                                                                           
                                                                       
                                                                             
                                                                               
                                               
   
                                                            
                                                                
   
                  
                                 
                                                  
                                                   
                                                                               
                                         
                                                                      
                                                                             
   
                                                           
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

#include "funcapi.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonpath.h"

static Datum
jsonPathFromCstring(char *in, int len);
static char *
jsonPathToCstring(StringInfo out, JsonPath *in, int estimated_len);
static int
flattenJsonPathParseItem(StringInfo buf, JsonPathParseItem *item, int nestingLevel, bool insideArraySubscript);
static void
alignStringInfoInt(StringInfo buf);
static int32
reserveSpaceForItemPointer(StringInfo buf);
static void
printJsonPathItem(StringInfo buf, JsonPathItem *v, bool inKey, bool printBracketes);
static int
operationPriority(JsonPathItemType op);

                                                                            

   
                                
   
Datum
jsonpath_in(PG_FUNCTION_ARGS)
{
  char *in = PG_GETARG_CSTRING(0);
  int len = strlen(in);

  return jsonPathFromCstring(in, len);
}

   
                               
   
                                                                       
                                                                        
                                                                      
                                
   
Datum
jsonpath_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  int version = pq_getmsgint(buf, 1);
  char *str;
  int nbytes;

  if (version == JSONPATH_VERSION)
  {
    str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
  }
  else
  {
    elog(ERROR, "unsupported jsonpath version number: %d", version);
  }

  return jsonPathFromCstring(str, nbytes);
}

   
                                 
   
Datum
jsonpath_out(PG_FUNCTION_ARGS)
{
  JsonPath *in = PG_GETARG_JSONPATH_P(0);

  PG_RETURN_CSTRING(jsonPathToCstring(NULL, in, VARSIZE(in)));
}

   
                               
   
                                                                 
   
Datum
jsonpath_send(PG_FUNCTION_ARGS)
{
  JsonPath *in = PG_GETARG_JSONPATH_P(0);
  StringInfoData buf;
  StringInfoData jtext;
  int version = JSONPATH_VERSION;

  initStringInfo(&jtext);
  (void)jsonPathToCstring(&jtext, in, VARSIZE(in));

  pq_begintypsend(&buf);
  pq_sendint8(&buf, version);
  pq_sendtext(&buf, jtext.data, jtext.len);
  pfree(jtext.data);

  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                          
   
                                                         
                                                                       
                               
   
static Datum
jsonPathFromCstring(char *in, int len)
{
  JsonPathParseResult *jsonpath = parsejsonpath(in, len);
  JsonPath *res;
  StringInfoData buf;

  initStringInfo(&buf);
  enlargeStringInfo(&buf, 4 * len                 );

  appendStringInfoSpaces(&buf, JSONPATH_HDRSZ);

  if (!jsonpath)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "jsonpath", in)));
  }

  flattenJsonPathParseItem(&buf, jsonpath->expr, 0, false);

  res = (JsonPath *)buf.data;
  SET_VARSIZE(res, buf.len);
  res->header = JSONPATH_VERSION;
  if (jsonpath->lax)
  {
    res->header |= JSONPATH_LAX;
  }

  PG_RETURN_JSONPATH_P(res);
}

   
                                          
   
                                                                              
                                                           
   
static char *
jsonPathToCstring(StringInfo out, JsonPath *in, int estimated_len)
{
  StringInfoData buf;
  JsonPathItem v;

  if (!out)
  {
    out = &buf;
    initStringInfo(out);
  }
  enlargeStringInfo(out, estimated_len);

  if (!(in->header & JSONPATH_LAX))
  {
    appendBinaryStringInfo(out, "strict ", 7);
  }

  jspInit(&v, in);
  printJsonPathItem(out, &v, false, true);

  return out->data;
}

   
                                                                       
                                          
   
static int
flattenJsonPathParseItem(StringInfo buf, JsonPathParseItem *item, int nestingLevel, bool insideArraySubscript)
{
                                                
  int32 pos = buf->len - JSONPATH_HDRSZ;
  int32 chld;
  int32 next;
  int argNestingLevel = 0;

  check_stack_depth();
  CHECK_FOR_INTERRUPTS();

  appendStringInfoChar(buf, (char)(item->type));

     
                                                                          
                                                                          
                                            
     
  alignStringInfoInt(buf);

     
                                                                         
                                                      
     
  next = reserveSpaceForItemPointer(buf);

  switch (item->type)
  {
  case jpiString:
  case jpiVariable:
  case jpiKey:
    appendBinaryStringInfo(buf, (char *)&item->value.string.len, sizeof(item->value.string.len));
    appendBinaryStringInfo(buf, item->value.string.val, item->value.string.len);
    appendStringInfoChar(buf, '\0');
    break;
  case jpiNumeric:
    appendBinaryStringInfo(buf, (char *)item->value.numeric, VARSIZE(item->value.numeric));
    break;
  case jpiBool:
    appendBinaryStringInfo(buf, (char *)&item->value.boolean, sizeof(item->value.boolean));
    break;
  case jpiAnd:
  case jpiOr:
  case jpiEqual:
  case jpiNotEqual:
  case jpiLess:
  case jpiGreater:
  case jpiLessOrEqual:
  case jpiGreaterOrEqual:
  case jpiAdd:
  case jpiSub:
  case jpiMul:
  case jpiDiv:
  case jpiMod:
  case jpiStartsWith:
  {
       
                                                                 
                                                             
               
       
    int32 left = reserveSpaceForItemPointer(buf);
    int32 right = reserveSpaceForItemPointer(buf);

    chld = !item->value.args.left ? pos : flattenJsonPathParseItem(buf, item->value.args.left, nestingLevel + argNestingLevel, insideArraySubscript);
    *(int32 *)(buf->data + left) = chld - pos;

    chld = !item->value.args.right ? pos : flattenJsonPathParseItem(buf, item->value.args.right, nestingLevel + argNestingLevel, insideArraySubscript);
    *(int32 *)(buf->data + right) = chld - pos;
  }
  break;
  case jpiLikeRegex:
  {
    int32 offs;

    appendBinaryStringInfo(buf, (char *)&item->value.like_regex.flags, sizeof(item->value.like_regex.flags));
    offs = reserveSpaceForItemPointer(buf);
    appendBinaryStringInfo(buf, (char *)&item->value.like_regex.patternlen, sizeof(item->value.like_regex.patternlen));
    appendBinaryStringInfo(buf, item->value.like_regex.pattern, item->value.like_regex.patternlen);
    appendStringInfoChar(buf, '\0');

    chld = flattenJsonPathParseItem(buf, item->value.like_regex.expr, nestingLevel, insideArraySubscript);
    *(int32 *)(buf->data + offs) = chld - pos;
  }
  break;
  case jpiFilter:
    argNestingLevel++;
                     
  case jpiIsUnknown:
  case jpiNot:
  case jpiPlus:
  case jpiMinus:
  case jpiExists:
  {
    int32 arg = reserveSpaceForItemPointer(buf);

    chld = flattenJsonPathParseItem(buf, item->value.arg, nestingLevel + argNestingLevel, insideArraySubscript);
    *(int32 *)(buf->data + arg) = chld - pos;
  }
  break;
  case jpiNull:
    break;
  case jpiRoot:
    break;
  case jpiAnyArray:
  case jpiAnyKey:
    break;
  case jpiCurrent:
    if (nestingLevel <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("@ is not allowed in root expressions")));
    }
    break;
  case jpiLast:
    if (!insideArraySubscript)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("LAST is allowed only in array subscripts")));
    }
    break;
  case jpiIndexArray:
  {
    int32 nelems = item->value.array.nelems;
    int offset;
    int i;

    appendBinaryStringInfo(buf, (char *)&nelems, sizeof(nelems));

    offset = buf->len;

    appendStringInfoSpaces(buf, sizeof(int32) * 2 * nelems);

    for (i = 0; i < nelems; i++)
    {
      int32 *ppos;
      int32 topos;
      int32 frompos = flattenJsonPathParseItem(buf, item->value.array.elems[i].from, nestingLevel, true) - pos;

      if (item->value.array.elems[i].to)
      {
        topos = flattenJsonPathParseItem(buf, item->value.array.elems[i].to, nestingLevel, true) - pos;
      }
      else
      {
        topos = 0;
      }

      ppos = (int32 *)&buf->data[offset + i * 2 * sizeof(int32)];

      ppos[0] = frompos;
      ppos[1] = topos;
    }
  }
  break;
  case jpiAny:
    appendBinaryStringInfo(buf, (char *)&item->value.anybounds.first, sizeof(item->value.anybounds.first));
    appendBinaryStringInfo(buf, (char *)&item->value.anybounds.last, sizeof(item->value.anybounds.last));
    break;
  case jpiType:
  case jpiSize:
  case jpiAbs:
  case jpiFloor:
  case jpiCeiling:
  case jpiDouble:
  case jpiKeyValue:
    break;
  default:
    elog(ERROR, "unrecognized jsonpath item type: %d", item->type);
  }

  if (item->next)
  {
    chld = flattenJsonPathParseItem(buf, item->next, nestingLevel, insideArraySubscript) - pos;
    *(int32 *)(buf->data + next) = chld;
  }

  return pos;
}

   
                                                        
   
static void
alignStringInfoInt(StringInfo buf)
{
  switch (INTALIGN(buf->len) - buf->len)
  {
  case 3:
    appendStringInfoCharMacro(buf, 0);
                     
  case 2:
    appendStringInfoCharMacro(buf, 0);
                     
  case 1:
    appendStringInfoCharMacro(buf, 0);
                     
  default:
    break;
  }
}

   
                                                                               
                                                                       
   
static int32
reserveSpaceForItemPointer(StringInfo buf)
{
  int32 pos = buf->len;
  int32 ptr = 0;

  appendBinaryStringInfo(buf, (char *)&ptr, sizeof(ptr));

  return pos;
}

   
                                                                           
   
static void
printJsonPathItem(StringInfo buf, JsonPathItem *v, bool inKey, bool printBracketes)
{
  JsonPathItem elem;
  int i;

  check_stack_depth();
  CHECK_FOR_INTERRUPTS();

  switch (v->type)
  {
  case jpiNull:
    appendStringInfoString(buf, "null");
    break;
  case jpiKey:
    if (inKey)
    {
      appendStringInfoChar(buf, '.');
    }
    escape_json(buf, jspGetString(v, NULL));
    break;
  case jpiString:
    escape_json(buf, jspGetString(v, NULL));
    break;
  case jpiVariable:
    appendStringInfoChar(buf, '$');
    escape_json(buf, jspGetString(v, NULL));
    break;
  case jpiNumeric:
    appendStringInfoString(buf, DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(jspGetNumeric(v)))));
    break;
  case jpiBool:
    if (jspGetBool(v))
    {
      appendBinaryStringInfo(buf, "true", 4);
    }
    else
    {
      appendBinaryStringInfo(buf, "false", 5);
    }
    break;
  case jpiAnd:
  case jpiOr:
  case jpiEqual:
  case jpiNotEqual:
  case jpiLess:
  case jpiGreater:
  case jpiLessOrEqual:
  case jpiGreaterOrEqual:
  case jpiAdd:
  case jpiSub:
  case jpiMul:
  case jpiDiv:
  case jpiMod:
  case jpiStartsWith:
    if (printBracketes)
    {
      appendStringInfoChar(buf, '(');
    }
    jspGetLeftArg(v, &elem);
    printJsonPathItem(buf, &elem, false, operationPriority(elem.type) <= operationPriority(v->type));
    appendStringInfoChar(buf, ' ');
    appendStringInfoString(buf, jspOperationName(v->type));
    appendStringInfoChar(buf, ' ');
    jspGetRightArg(v, &elem);
    printJsonPathItem(buf, &elem, false, operationPriority(elem.type) <= operationPriority(v->type));
    if (printBracketes)
    {
      appendStringInfoChar(buf, ')');
    }
    break;
  case jpiLikeRegex:
    if (printBracketes)
    {
      appendStringInfoChar(buf, '(');
    }

    jspInitByBuffer(&elem, v->base, v->content.like_regex.expr);
    printJsonPathItem(buf, &elem, false, operationPriority(elem.type) <= operationPriority(v->type));

    appendBinaryStringInfo(buf, " like_regex ", 12);

    escape_json(buf, v->content.like_regex.pattern);

    if (v->content.like_regex.flags)
    {
      appendBinaryStringInfo(buf, " flag \"", 7);

      if (v->content.like_regex.flags & JSP_REGEX_ICASE)
      {
        appendStringInfoChar(buf, 'i');
      }
      if (v->content.like_regex.flags & JSP_REGEX_DOTALL)
      {
        appendStringInfoChar(buf, 's');
      }
      if (v->content.like_regex.flags & JSP_REGEX_MLINE)
      {
        appendStringInfoChar(buf, 'm');
      }
      if (v->content.like_regex.flags & JSP_REGEX_WSPACE)
      {
        appendStringInfoChar(buf, 'x');
      }
      if (v->content.like_regex.flags & JSP_REGEX_QUOTE)
      {
        appendStringInfoChar(buf, 'q');
      }

      appendStringInfoChar(buf, '"');
    }

    if (printBracketes)
    {
      appendStringInfoChar(buf, ')');
    }
    break;
  case jpiPlus:
  case jpiMinus:
    if (printBracketes)
    {
      appendStringInfoChar(buf, '(');
    }
    appendStringInfoChar(buf, v->type == jpiPlus ? '+' : '-');
    jspGetArg(v, &elem);
    printJsonPathItem(buf, &elem, false, operationPriority(elem.type) <= operationPriority(v->type));
    if (printBracketes)
    {
      appendStringInfoChar(buf, ')');
    }
    break;
  case jpiFilter:
    appendBinaryStringInfo(buf, "?(", 2);
    jspGetArg(v, &elem);
    printJsonPathItem(buf, &elem, false, false);
    appendStringInfoChar(buf, ')');
    break;
  case jpiNot:
    appendBinaryStringInfo(buf, "!(", 2);
    jspGetArg(v, &elem);
    printJsonPathItem(buf, &elem, false, false);
    appendStringInfoChar(buf, ')');
    break;
  case jpiIsUnknown:
    appendStringInfoChar(buf, '(');
    jspGetArg(v, &elem);
    printJsonPathItem(buf, &elem, false, false);
    appendBinaryStringInfo(buf, ") is unknown", 12);
    break;
  case jpiExists:
    appendBinaryStringInfo(buf, "exists (", 8);
    jspGetArg(v, &elem);
    printJsonPathItem(buf, &elem, false, false);
    appendStringInfoChar(buf, ')');
    break;
  case jpiCurrent:
    Assert(!inKey);
    appendStringInfoChar(buf, '@');
    break;
  case jpiRoot:
    Assert(!inKey);
    appendStringInfoChar(buf, '$');
    break;
  case jpiLast:
    appendBinaryStringInfo(buf, "last", 4);
    break;
  case jpiAnyArray:
    appendBinaryStringInfo(buf, "[*]", 3);
    break;
  case jpiAnyKey:
    if (inKey)
    {
      appendStringInfoChar(buf, '.');
    }
    appendStringInfoChar(buf, '*');
    break;
  case jpiIndexArray:
    appendStringInfoChar(buf, '[');
    for (i = 0; i < v->content.array.nelems; i++)
    {
      JsonPathItem from;
      JsonPathItem to;
      bool range = jspGetArraySubscript(v, &from, &to, i);

      if (i)
      {
        appendStringInfoChar(buf, ',');
      }

      printJsonPathItem(buf, &from, false, false);

      if (range)
      {
        appendBinaryStringInfo(buf, " to ", 4);
        printJsonPathItem(buf, &to, false, false);
      }
    }
    appendStringInfoChar(buf, ']');
    break;
  case jpiAny:
    if (inKey)
    {
      appendStringInfoChar(buf, '.');
    }

    if (v->content.anybounds.first == 0 && v->content.anybounds.last == PG_UINT32_MAX)
    {
      appendBinaryStringInfo(buf, "**", 2);
    }
    else if (v->content.anybounds.first == v->content.anybounds.last)
    {
      if (v->content.anybounds.first == PG_UINT32_MAX)
      {
        appendStringInfo(buf, "**{last}");
      }
      else
      {
        appendStringInfo(buf, "**{%u}", v->content.anybounds.first);
      }
    }
    else if (v->content.anybounds.first == PG_UINT32_MAX)
    {
      appendStringInfo(buf, "**{last to %u}", v->content.anybounds.last);
    }
    else if (v->content.anybounds.last == PG_UINT32_MAX)
    {
      appendStringInfo(buf, "**{%u to last}", v->content.anybounds.first);
    }
    else
    {
      appendStringInfo(buf, "**{%u to %u}", v->content.anybounds.first, v->content.anybounds.last);
    }
    break;
  case jpiType:
    appendBinaryStringInfo(buf, ".type()", 7);
    break;
  case jpiSize:
    appendBinaryStringInfo(buf, ".size()", 7);
    break;
  case jpiAbs:
    appendBinaryStringInfo(buf, ".abs()", 6);
    break;
  case jpiFloor:
    appendBinaryStringInfo(buf, ".floor()", 8);
    break;
  case jpiCeiling:
    appendBinaryStringInfo(buf, ".ceiling()", 10);
    break;
  case jpiDouble:
    appendBinaryStringInfo(buf, ".double()", 9);
    break;
  case jpiKeyValue:
    appendBinaryStringInfo(buf, ".keyvalue()", 11);
    break;
  default:
    elog(ERROR, "unrecognized jsonpath item type: %d", v->type);
  }

  if (jspGetNext(v, &elem))
  {
    printJsonPathItem(buf, &elem, true, true);
  }
}

const char *
jspOperationName(JsonPathItemType type)
{
  switch (type)
  {
  case jpiAnd:
    return "&&";
  case jpiOr:
    return "||";
  case jpiEqual:
    return "==";
  case jpiNotEqual:
    return "!=";
  case jpiLess:
    return "<";
  case jpiGreater:
    return ">";
  case jpiLessOrEqual:
    return "<=";
  case jpiGreaterOrEqual:
    return ">=";
  case jpiPlus:
  case jpiAdd:
    return "+";
  case jpiMinus:
  case jpiSub:
    return "-";
  case jpiMul:
    return "*";
  case jpiDiv:
    return "/";
  case jpiMod:
    return "%";
  case jpiStartsWith:
    return "starts with";
  case jpiLikeRegex:
    return "like_regex";
  case jpiType:
    return "type";
  case jpiSize:
    return "size";
  case jpiKeyValue:
    return "keyvalue";
  case jpiDouble:
    return "double";
  case jpiAbs:
    return "abs";
  case jpiFloor:
    return "floor";
  case jpiCeiling:
    return "ceiling";
  default:
    elog(ERROR, "unrecognized jsonpath item type: %d", type);
    return NULL;
  }
}

static int
operationPriority(JsonPathItemType op)
{
  switch (op)
  {
  case jpiOr:
    return 0;
  case jpiAnd:
    return 1;
  case jpiEqual:
  case jpiNotEqual:
  case jpiLess:
  case jpiGreater:
  case jpiLessOrEqual:
  case jpiGreaterOrEqual:
  case jpiStartsWith:
    return 2;
  case jpiAdd:
  case jpiSub:
    return 3;
  case jpiMul:
  case jpiDiv:
  case jpiMod:
    return 4;
  case jpiPlus:
  case jpiMinus:
    return 5;
  default:
    return 6;
  }
}

                                                                              

   
                                        
   

#define read_byte(v, b, p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (v) = *(uint8 *)((b) + (p));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    (p) += 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

#define read_int32(v, b, p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (v) = *(uint32 *)((b) + (p));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    (p) += sizeof(int32);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  } while (0)

#define read_int32_n(v, b, p, n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (v) = (void *)((b) + (p));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    (p) += sizeof(int32) * (n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } while (0)

   
                                                    
   
void
jspInit(JsonPathItem *v, JsonPath *js)
{
  Assert((js->header & ~JSONPATH_LAX) == JSONPATH_VERSION);
  jspInitByBuffer(v, js->data, 0);
}

   
                                                     
   
void
jspInitByBuffer(JsonPathItem *v, char *base, int32 pos)
{
  v->base = base + pos;

  read_byte(v->type, base, pos);
  pos = INTALIGN((uintptr_t)(base + pos)) - (uintptr_t)base;
  read_int32(v->nextPos, base, pos);

  switch (v->type)
  {
  case jpiNull:
  case jpiRoot:
  case jpiCurrent:
  case jpiAnyArray:
  case jpiAnyKey:
  case jpiType:
  case jpiSize:
  case jpiAbs:
  case jpiFloor:
  case jpiCeiling:
  case jpiDouble:
  case jpiKeyValue:
  case jpiLast:
    break;
  case jpiKey:
  case jpiString:
  case jpiVariable:
    read_int32(v->content.value.datalen, base, pos);
                     
  case jpiNumeric:
  case jpiBool:
    v->content.value.data = base + pos;
    break;
  case jpiAnd:
  case jpiOr:
  case jpiAdd:
  case jpiSub:
  case jpiMul:
  case jpiDiv:
  case jpiMod:
  case jpiEqual:
  case jpiNotEqual:
  case jpiLess:
  case jpiGreater:
  case jpiLessOrEqual:
  case jpiGreaterOrEqual:
  case jpiStartsWith:
    read_int32(v->content.args.left, base, pos);
    read_int32(v->content.args.right, base, pos);
    break;
  case jpiLikeRegex:
    read_int32(v->content.like_regex.flags, base, pos);
    read_int32(v->content.like_regex.expr, base, pos);
    read_int32(v->content.like_regex.patternlen, base, pos);
    v->content.like_regex.pattern = base + pos;
    break;
  case jpiNot:
  case jpiExists:
  case jpiIsUnknown:
  case jpiPlus:
  case jpiMinus:
  case jpiFilter:
    read_int32(v->content.arg, base, pos);
    break;
  case jpiIndexArray:
    read_int32(v->content.array.nelems, base, pos);
    read_int32_n(v->content.array.elems, base, pos, v->content.array.nelems * 2);
    break;
  case jpiAny:
    read_int32(v->content.anybounds.first, base, pos);
    read_int32(v->content.anybounds.last, base, pos);
    break;
  default:
    elog(ERROR, "unrecognized jsonpath item type: %d", v->type);
  }
}

void
jspGetArg(JsonPathItem *v, JsonPathItem *a)
{
  Assert(v->type == jpiFilter || v->type == jpiNot || v->type == jpiIsUnknown || v->type == jpiExists || v->type == jpiPlus || v->type == jpiMinus);

  jspInitByBuffer(a, v->base, v->content.arg);
}

bool
jspGetNext(JsonPathItem *v, JsonPathItem *a)
{
  if (jspHasNext(v))
  {
    Assert(v->type == jpiString || v->type == jpiNumeric || v->type == jpiBool || v->type == jpiNull || v->type == jpiKey || v->type == jpiAny || v->type == jpiAnyArray || v->type == jpiAnyKey || v->type == jpiIndexArray || v->type == jpiFilter || v->type == jpiCurrent || v->type == jpiExists || v->type == jpiRoot || v->type == jpiVariable || v->type == jpiLast || v->type == jpiAdd || v->type == jpiSub || v->type == jpiMul || v->type == jpiDiv || v->type == jpiMod || v->type == jpiPlus || v->type == jpiMinus || v->type == jpiEqual || v->type == jpiNotEqual || v->type == jpiGreater || v->type == jpiGreaterOrEqual || v->type == jpiLess || v->type == jpiLessOrEqual || v->type == jpiAnd || v->type == jpiOr || v->type == jpiNot || v->type == jpiIsUnknown || v->type == jpiType || v->type == jpiSize || v->type == jpiAbs || v->type == jpiFloor || v->type == jpiCeiling || v->type == jpiDouble || v->type == jpiKeyValue || v->type == jpiStartsWith);

    if (a)
    {
      jspInitByBuffer(a, v->base, v->nextPos);
    }
    return true;
  }

  return false;
}

void
jspGetLeftArg(JsonPathItem *v, JsonPathItem *a)
{
  Assert(v->type == jpiAnd || v->type == jpiOr || v->type == jpiEqual || v->type == jpiNotEqual || v->type == jpiLess || v->type == jpiGreater || v->type == jpiLessOrEqual || v->type == jpiGreaterOrEqual || v->type == jpiAdd || v->type == jpiSub || v->type == jpiMul || v->type == jpiDiv || v->type == jpiMod || v->type == jpiStartsWith);

  jspInitByBuffer(a, v->base, v->content.args.left);
}

void
jspGetRightArg(JsonPathItem *v, JsonPathItem *a)
{
  Assert(v->type == jpiAnd || v->type == jpiOr || v->type == jpiEqual || v->type == jpiNotEqual || v->type == jpiLess || v->type == jpiGreater || v->type == jpiLessOrEqual || v->type == jpiGreaterOrEqual || v->type == jpiAdd || v->type == jpiSub || v->type == jpiMul || v->type == jpiDiv || v->type == jpiMod || v->type == jpiStartsWith);

  jspInitByBuffer(a, v->base, v->content.args.right);
}

bool
jspGetBool(JsonPathItem *v)
{
  Assert(v->type == jpiBool);

  return (bool)*v->content.value.data;
}

Numeric
jspGetNumeric(JsonPathItem *v)
{
  Assert(v->type == jpiNumeric);

  return (Numeric)v->content.value.data;
}

char *
jspGetString(JsonPathItem *v, int32 *len)
{
  Assert(v->type == jpiKey || v->type == jpiString || v->type == jpiVariable);

  if (len)
  {
    *len = v->content.value.datalen;
  }
  return v->content.value.data;
}

bool
jspGetArraySubscript(JsonPathItem *v, JsonPathItem *from, JsonPathItem *to, int i)
{
  Assert(v->type == jpiIndexArray);

  jspInitByBuffer(from, v->base, v->content.array.elems[i].from);

  if (!v->content.array.elems[i].to)
  {
    return false;
  }

  jspInitByBuffer(to, v->base, v->content.array.elems[i].to);

  return true;
}
