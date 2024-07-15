                                                                            
   
                
                                                              
   
                                                                
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/jsonb.h"
#include "utils/memutils.h"
#include "utils/varlena.h"

   
                                                                             
                                                                        
                                                                               
                                                         
   
                                                                         
                                                                
   
#define JSONB_MAX_ELEMS (Min(MaxAllocSize / sizeof(JsonbValue), JB_CMASK))
#define JSONB_MAX_PAIRS (Min(MaxAllocSize / sizeof(JsonbPair), JB_CMASK))

static void
fillJsonbValue(JsonbContainer *container, int index, char *base_addr, uint32 offset, JsonbValue *result);
static bool
equalsJsonbScalarValue(JsonbValue *a, JsonbValue *b);
static int
compareJsonbScalarValue(JsonbValue *a, JsonbValue *b);
static Jsonb *
convertToJsonb(JsonbValue *val);
static void
convertJsonbValue(StringInfo buffer, JEntry *header, JsonbValue *val, int level);
static void
convertJsonbArray(StringInfo buffer, JEntry *header, JsonbValue *val, int level);
static void
convertJsonbObject(StringInfo buffer, JEntry *header, JsonbValue *val, int level);
static void
convertJsonbScalar(StringInfo buffer, JEntry *header, JsonbValue *scalarVal);

static int
reserveFromBuffer(StringInfo buffer, int len);
static void
appendToBuffer(StringInfo buffer, const char *data, int len);
static void
copyToBuffer(StringInfo buffer, int offset, const char *data, int len);
static short
padBufferToInt(StringInfo buffer);

static JsonbIterator *
iteratorFromContainer(JsonbContainer *container, JsonbIterator *parent);
static JsonbIterator *
freeAndGetParent(JsonbIterator *it);
static JsonbParseState *
pushState(JsonbParseState **pstate);
static void
appendKey(JsonbParseState *pstate, JsonbValue *scalarVal);
static void
appendValue(JsonbParseState *pstate, JsonbValue *scalarVal);
static void
appendElement(JsonbParseState *pstate, JsonbValue *scalarVal);
static int
lengthCompareJsonbStringValue(const void *a, const void *b);
static int
lengthCompareJsonbPair(const void *a, const void *b, void *arg);
static void
uniqueifyJsonbObject(JsonbValue *object);
static JsonbValue *
pushJsonbValueScalar(JsonbParseState **pstate, JsonbIteratorToken seq, JsonbValue *scalarVal);

   
                                                                  
   
                                                                        
                                                                            
                                                                                
                                                                             
                                                                            
                                                                               
                                                                               
                                                                            
                                            
   
Jsonb *
JsonbValueToJsonb(JsonbValue *val)
{
  Jsonb *out;

  if (IsAJsonbScalar(val))
  {
                      
    JsonbParseState *pstate = NULL;
    JsonbValue *res;
    JsonbValue scalarArray;

    scalarArray.type = jbvArray;
    scalarArray.val.array.rawScalar = true;
    scalarArray.val.array.nElems = 1;

    pushJsonbValue(&pstate, WJB_BEGIN_ARRAY, &scalarArray);
    pushJsonbValue(&pstate, WJB_ELEM, val);
    res = pushJsonbValue(&pstate, WJB_END_ARRAY, NULL);

    out = convertToJsonb(res);
  }
  else if (val->type == jbvObject || val->type == jbvArray)
  {
    out = convertToJsonb(val);
  }
  else
  {
    Assert(val->type == jbvBinary);
    out = palloc(VARHDRSZ + val->val.binary.len);
    SET_VARSIZE(out, VARHDRSZ + val->val.binary.len);
    memcpy(VARDATA(out), val->val.binary.data, val->val.binary.len);
  }

  return out;
}

   
                                                                        
                                                                           
                                                 
   
uint32
getJsonbOffset(const JsonbContainer *jc, int index)
{
  uint32 offset = 0;
  int i;

     
                                                                           
                                                                      
                                                                
     
  for (i = index - 1; i >= 0; i--)
  {
    offset += JBE_OFFLENFLD(jc->children[i]);
    if (JBE_HAS_OFF(jc->children[i]))
    {
      break;
    }
  }

  return offset;
}

   
                                                                  
                                                                        
   
uint32
getJsonbLength(const JsonbContainer *jc, int index)
{
  uint32 off;
  uint32 len;

     
                                                                     
                                                                          
                              
     
  if (JBE_HAS_OFF(jc->children[index]))
  {
    off = getJsonbOffset(jc, index);
    len = JBE_OFFLENFLD(jc->children[index]) - off;
  }
  else
  {
    len = JBE_OFFLENFLD(jc->children[index]);
  }

  return len;
}

   
                                                                              
                                                                              
                                                                         
   
                                                                                
                                                                               
                                                                          
                
   
int
compareJsonbContainers(JsonbContainer *a, JsonbContainer *b)
{
  JsonbIterator *ita, *itb;
  int res = 0;

  ita = JsonbIteratorInit(a);
  itb = JsonbIteratorInit(b);

  do
  {
    JsonbValue va, vb;
    JsonbIteratorToken ra, rb;

    ra = JsonbIteratorNext(&ita, &va, false);
    rb = JsonbIteratorNext(&itb, &vb, false);

    if (ra == rb)
    {
      if (ra == WJB_DONE)
      {
                              
        break;
      }

      if (ra == WJB_END_ARRAY || ra == WJB_END_OBJECT)
      {
           
                                                                   
                                                               
                                                                  
                   
           
        continue;
      }

      if (va.type == vb.type)
      {
        switch (va.type)
        {
        case jbvString:
        case jbvNull:
        case jbvNumeric:
        case jbvBool:
          res = compareJsonbScalarValue(&va, &vb);
          break;
        case jbvArray:

             
                                                                
                                                                 
                                                                 
                                                                 
             
          if (va.val.array.rawScalar != vb.val.array.rawScalar)
          {
            res = (va.val.array.rawScalar) ? -1 : 1;
          }
          if (va.val.array.nElems != vb.val.array.nElems)
          {
            res = (va.val.array.nElems > vb.val.array.nElems) ? 1 : -1;
          }
          break;
        case jbvObject:
          if (va.val.object.nPairs != vb.val.object.nPairs)
          {
            res = (va.val.object.nPairs > vb.val.object.nPairs) ? 1 : -1;
          }
          break;
        case jbvBinary:
          elog(ERROR, "unexpected jbvBinary value");
        }
      }
      else
      {
                                
        res = (va.type > vb.type) ? 1 : -1;
      }
    }
    else
    {
         
                                                                      
                                        
         
                                                                         
                                                                      
                                                                         
                                                                         
                           
         
                                                                        
                                                                  
                                                                
                                          
         
      Assert(ra != WJB_END_ARRAY && ra != WJB_END_OBJECT);
      Assert(rb != WJB_END_ARRAY && rb != WJB_END_OBJECT);

      Assert(va.type != vb.type);
      Assert(va.type != jbvBinary);
      Assert(vb.type != jbvBinary);
                              
      res = (va.type > vb.type) ? 1 : -1;
    }
  } while (res == 0);

  while (ita != NULL)
  {
    JsonbIterator *i = ita->parent;

    pfree(ita);
    ita = i;
  }
  while (itb != NULL)
  {
    JsonbIterator *i = itb->parent;

    pfree(itb);
    itb = i;
  }

  return res;
}

   
                                                                            
                                                                              
                                                                         
                                                                         
                                                                                
   
                                                                               
                                                                           
                                                                       
                                                            
   
                                                                            
                                                                               
                                                                            
                                                                             
                                                                             
                                                                            
                                                         
   
                                                                          
                                                                               
                                                                         
                                                                           
                                                             
   
JsonbValue *
findJsonbValueFromContainer(JsonbContainer *container, uint32 flags, JsonbValue *key)
{
  JEntry *children = container->children;
  int count = JsonContainerSize(container);
  JsonbValue *result;

  Assert((flags & ~(JB_FARRAY | JB_FOBJECT)) == 0);

                                                                 
  if (count <= 0)
  {
    return NULL;
  }

  result = palloc(sizeof(JsonbValue));

  if ((flags & JB_FARRAY) && JsonContainerIsArray(container))
  {
    char *base_addr = (char *)(children + count);
    uint32 offset = 0;
    int i;

    for (i = 0; i < count; i++)
    {
      fillJsonbValue(container, i, base_addr, offset, result);

      if (key->type == result->type)
      {
        if (equalsJsonbScalarValue(key, result))
        {
          return result;
        }
      }

      JBE_ADVANCE_OFFSET(offset, children[i]);
    }
  }
  else if ((flags & JB_FOBJECT) && JsonContainerIsObject(container))
  {
                                                                 
    char *base_addr = (char *)(children + count * 2);
    uint32 stopLow = 0, stopHigh = count;

                                                      
    Assert(key->type == jbvString);

                                                  
    while (stopLow < stopHigh)
    {
      uint32 stopMiddle;
      int difference;
      JsonbValue candidate;

      stopMiddle = stopLow + (stopHigh - stopLow) / 2;

      candidate.type = jbvString;
      candidate.val.string.val = base_addr + getJsonbOffset(container, stopMiddle);
      candidate.val.string.len = getJsonbLength(container, stopMiddle);

      difference = lengthCompareJsonbStringValue(&candidate, key);

      if (difference == 0)
      {
                                                       
        int index = stopMiddle + count;

        fillJsonbValue(container, index, base_addr, getJsonbOffset(container, index), result);

        return result;
      }
      else
      {
        if (difference < 0)
        {
          stopLow = stopMiddle + 1;
        }
        else
        {
          stopHigh = stopMiddle;
        }
      }
    }
  }

                 
  pfree(result);
  return NULL;
}

   
                                    
   
                                                                       
   
JsonbValue *
getIthJsonbValueFromContainer(JsonbContainer *container, uint32 i)
{
  JsonbValue *result;
  char *base_addr;
  uint32 nelements;

  if (!JsonContainerIsArray(container))
  {
    elog(ERROR, "not a jsonb array");
  }

  nelements = JsonContainerSize(container);
  base_addr = (char *)&container->children[nelements];

  if (i >= nelements)
  {
    return NULL;
  }

  result = palloc(sizeof(JsonbValue));

  fillJsonbValue(container, i, base_addr, getJsonbOffset(container, i), result);

  return result;
}

   
                                                                           
                                          
   
                                                                               
                                                                           
                                                                         
                                                                
   
                                                                           
             
   
static void
fillJsonbValue(JsonbContainer *container, int index, char *base_addr, uint32 offset, JsonbValue *result)
{
  JEntry entry = container->children[index];

  if (JBE_ISNULL(entry))
  {
    result->type = jbvNull;
  }
  else if (JBE_ISSTRING(entry))
  {
    result->type = jbvString;
    result->val.string.val = base_addr + offset;
    result->val.string.len = getJsonbLength(container, index);
    Assert(result->val.string.len >= 0);
  }
  else if (JBE_ISNUMERIC(entry))
  {
    result->type = jbvNumeric;
    result->val.numeric = (Numeric)(base_addr + INTALIGN(offset));
  }
  else if (JBE_ISBOOL_TRUE(entry))
  {
    result->type = jbvBool;
    result->val.boolean = true;
  }
  else if (JBE_ISBOOL_FALSE(entry))
  {
    result->type = jbvBool;
    result->val.boolean = false;
  }
  else
  {
    Assert(JBE_ISCONTAINER(entry));
    result->type = jbvBinary;
                                                               
    result->val.binary.data = (JsonbContainer *)(base_addr + INTALIGN(offset));
    result->val.binary.len = getJsonbLength(container, index) - (INTALIGN(offset) - offset);
  }
}

   
                                         
   
                                                                                
                          
   
                                                                            
                                                                   
   
                                                                          
                                                                             
                                                                               
                                                              
   
                                                                     
                                                  
   
JsonbValue *
pushJsonbValue(JsonbParseState **pstate, JsonbIteratorToken seq, JsonbValue *jbval)
{
  JsonbIterator *it;
  JsonbValue *res = NULL;
  JsonbValue v;
  JsonbIteratorToken tok;

  if (!jbval || (seq != WJB_ELEM && seq != WJB_VALUE) || jbval->type != jbvBinary)
  {
                      
    return pushJsonbValueScalar(pstate, seq, jbval);
  }

                                                          
  it = JsonbIteratorInit(jbval->val.binary.data);
  while ((tok = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    res = pushJsonbValueScalar(pstate, tok, tok < WJB_BEGIN_ARRAY ? &v : NULL);
  }

  return res;
}

   
                                                                         
             
   
static JsonbValue *
pushJsonbValueScalar(JsonbParseState **pstate, JsonbIteratorToken seq, JsonbValue *scalarVal)
{
  JsonbValue *result = NULL;

  switch (seq)
  {
  case WJB_BEGIN_ARRAY:
    Assert(!scalarVal || scalarVal->val.array.rawScalar);
    *pstate = pushState(pstate);
    result = &(*pstate)->contVal;
    (*pstate)->contVal.type = jbvArray;
    (*pstate)->contVal.val.array.nElems = 0;
    (*pstate)->contVal.val.array.rawScalar = (scalarVal && scalarVal->val.array.rawScalar);
    if (scalarVal && scalarVal->val.array.nElems > 0)
    {
                                                           
      Assert(scalarVal->type == jbvArray);
      (*pstate)->size = scalarVal->val.array.nElems;
    }
    else
    {
      (*pstate)->size = 4;
    }
    (*pstate)->contVal.val.array.elems = palloc(sizeof(JsonbValue) * (*pstate)->size);
    break;
  case WJB_BEGIN_OBJECT:
    Assert(!scalarVal);
    *pstate = pushState(pstate);
    result = &(*pstate)->contVal;
    (*pstate)->contVal.type = jbvObject;
    (*pstate)->contVal.val.object.nPairs = 0;
    (*pstate)->size = 4;
    (*pstate)->contVal.val.object.pairs = palloc(sizeof(JsonbPair) * (*pstate)->size);
    break;
  case WJB_KEY:
    Assert(scalarVal->type == jbvString);
    appendKey(*pstate, scalarVal);
    break;
  case WJB_VALUE:
    Assert(IsAJsonbScalar(scalarVal));
    appendValue(*pstate, scalarVal);
    break;
  case WJB_ELEM:
    Assert(IsAJsonbScalar(scalarVal));
    appendElement(*pstate, scalarVal);
    break;
  case WJB_END_OBJECT:
    uniqueifyJsonbObject(&(*pstate)->contVal);
                       
  case WJB_END_ARRAY:
                                                  
    Assert(!scalarVal);
    result = &(*pstate)->contVal;

       
                                                                  
                    
       
    *pstate = (*pstate)->next;
    if (*pstate)
    {
      switch ((*pstate)->contVal.type)
      {
      case jbvArray:
        appendElement(*pstate, result);
        break;
      case jbvObject:
        appendValue(*pstate, result);
        break;
      default:
        elog(ERROR, "invalid jsonb container type");
      }
    }
    break;
  default:
    elog(ERROR, "unrecognized jsonb sequential processing token");
  }

  return result;
}

   
                                                             
   
static JsonbParseState *
pushState(JsonbParseState **pstate)
{
  JsonbParseState *ns = palloc(sizeof(JsonbParseState));

  ns->next = *pstate;
  return ns;
}

   
                                                                                
   
static void
appendKey(JsonbParseState *pstate, JsonbValue *string)
{
  JsonbValue *object = &pstate->contVal;

  Assert(object->type == jbvObject);
  Assert(string->type == jbvString);

  if (object->val.object.nPairs >= JSONB_MAX_PAIRS)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("number of jsonb object pairs exceeds the maximum allowed (%zu)", JSONB_MAX_PAIRS)));
  }

  if (object->val.object.nPairs >= pstate->size)
  {
    pstate->size *= 2;
    object->val.object.pairs = repalloc(object->val.object.pairs, sizeof(JsonbPair) * pstate->size);
  }

  object->val.object.pairs[object->val.object.nPairs].key = *string;
  object->val.object.pairs[object->val.object.nPairs].order = object->val.object.nPairs;
}

   
                                                                            
         
   
static void
appendValue(JsonbParseState *pstate, JsonbValue *scalarVal)
{
  JsonbValue *object = &pstate->contVal;

  Assert(object->type == jbvObject);

  object->val.object.pairs[object->val.object.nPairs++].value = *scalarVal;
}

   
                                                                                
   
static void
appendElement(JsonbParseState *pstate, JsonbValue *scalarVal)
{
  JsonbValue *array = &pstate->contVal;

  Assert(array->type == jbvArray);

  if (array->val.array.nElems >= JSONB_MAX_ELEMS)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("number of jsonb array elements exceeds the maximum allowed (%zu)", JSONB_MAX_ELEMS)));
  }

  if (array->val.array.nElems >= pstate->size)
  {
    pstate->size *= 2;
    array->val.array.elems = repalloc(array->val.array.elems, sizeof(JsonbValue) * pstate->size);
  }

  array->val.array.elems[array->val.array.nElems++] = *scalarVal;
}

   
                                                                         
                                                                
   
                                                           
   
JsonbIterator *
JsonbIteratorInit(JsonbContainer *container)
{
  return iteratorFromContainer(container, NULL);
}

   
                                       
   
                                                                            
                                                                              
                                                                          
                                                                               
                                                                             
   
                                                                          
                                                                             
                                                                             
                                                                                
                                                                              
         
   
                                                                        
                                                                                
                                                                          
              
   
                                                                           
                                                                              
                                                                               
                                                                                
                                                                         
                                                                        
                                                                            
                      
   
JsonbIteratorToken
JsonbIteratorNext(JsonbIterator **it, JsonbValue *val, bool skipNested)
{
  if (*it == NULL)
  {
    return WJB_DONE;
  }

     
                                                                       
                                                                            
                                                                  
                             
     
recurse:
  switch ((*it)->state)
  {
  case JBI_ARRAY_START:
                                            
    val->type = jbvArray;
    val->val.array.nElems = (*it)->nElems;

       
                                                                       
                         
       
    val->val.array.rawScalar = (*it)->isScalar;
    (*it)->curIndex = 0;
    (*it)->curDataOffset = 0;
    (*it)->curValueOffset = 0;                        
                                 
    (*it)->state = JBI_ARRAY_ELEM;
    return WJB_BEGIN_ARRAY;

  case JBI_ARRAY_ELEM:
    if ((*it)->curIndex >= (*it)->nElems)
    {
         
                                                                   
                                                                     
                                                                 
                   
         
      *it = freeAndGetParent(*it);
      return WJB_END_ARRAY;
    }

    fillJsonbValue((*it)->container, (*it)->curIndex, (*it)->dataProper, (*it)->curDataOffset, val);

    JBE_ADVANCE_OFFSET((*it)->curDataOffset, (*it)->children[(*it)->curIndex]);
    (*it)->curIndex++;

    if (!IsAJsonbScalar(val) && !skipNested)
    {
                                   
      *it = iteratorFromContainer(val->val.binary.data, *it);
      goto recurse;
    }
    else
    {
         
                                                                     
                                
         
      return WJB_ELEM;
    }

  case JBI_OBJECT_START:
                                              
    val->type = jbvObject;
    val->val.object.nPairs = (*it)->nElems;

       
                                                                  
                               
       
    (*it)->curIndex = 0;
    (*it)->curDataOffset = 0;
    (*it)->curValueOffset = getJsonbOffset((*it)->container, (*it)->nElems);
                                 
    (*it)->state = JBI_OBJECT_KEY;
    return WJB_BEGIN_OBJECT;

  case JBI_OBJECT_KEY:
    if ((*it)->curIndex >= (*it)->nElems)
    {
         
                                                                    
                                                               
                                                                     
                      
         
      *it = freeAndGetParent(*it);
      return WJB_END_OBJECT;
    }
    else
    {
                                            
      fillJsonbValue((*it)->container, (*it)->curIndex, (*it)->dataProper, (*it)->curDataOffset, val);
      if (val->type != jbvString)
      {
        elog(ERROR, "unexpected jsonb type as object key");
      }

                                   
      (*it)->state = JBI_OBJECT_VALUE;
      return WJB_KEY;
    }

  case JBI_OBJECT_VALUE:
                                 
    (*it)->state = JBI_OBJECT_KEY;

    fillJsonbValue((*it)->container, (*it)->curIndex + (*it)->nElems, (*it)->dataProper, (*it)->curValueOffset, val);

    JBE_ADVANCE_OFFSET((*it)->curDataOffset, (*it)->children[(*it)->curIndex]);
    JBE_ADVANCE_OFFSET((*it)->curValueOffset, (*it)->children[(*it)->curIndex + (*it)->nElems]);
    (*it)->curIndex++;

       
                                                                    
                                                                  
                    
       
    if (!IsAJsonbScalar(val) && !skipNested)
    {
      *it = iteratorFromContainer(val->val.binary.data, *it);
      goto recurse;
    }
    else
    {
      return WJB_VALUE;
    }
  }

  elog(ERROR, "invalid iterator state");
  return -1;
}

   
                                                                     
   
static JsonbIterator *
iteratorFromContainer(JsonbContainer *container, JsonbIterator *parent)
{
  JsonbIterator *it;

  it = palloc0(sizeof(JsonbIterator));
  it->container = container;
  it->parent = parent;
  it->nElems = JsonContainerSize(container);

                                      
  it->children = container->children;

  switch (container->header & (JB_FARRAY | JB_FOBJECT))
  {
  case JB_FARRAY:
    it->dataProper = (char *)it->children + it->nElems * sizeof(JEntry);
    it->isScalar = JsonContainerIsScalar(container);
                                                    
    Assert(!it->isScalar || it->nElems == 1);

    it->state = JBI_ARRAY_START;
    break;

  case JB_FOBJECT:
    it->dataProper = (char *)it->children + it->nElems * sizeof(JEntry) * 2;
    it->state = JBI_OBJECT_START;
    break;

  default:
    elog(ERROR, "unknown type of jsonb container");
  }

  return it;
}

   
                                                                               
            
   
static JsonbIterator *
freeAndGetParent(JsonbIterator *it)
{
  JsonbIterator *v = it->parent;

  pfree(it);
  return v;
}

   
                                             
   
                                                                              
   
                                                                        
                                                                               
                                                                
   
                                                                               
                                                       
   
bool
JsonbDeepContains(JsonbIterator **val, JsonbIterator **mContained)
{
  JsonbValue vval, vcontained;
  JsonbIteratorToken rval, rcont;

     
                                                               
     
                                                                        
                                                                      
     
  check_stack_depth();

  rval = JsonbIteratorNext(val, &vval, false);
  rcont = JsonbIteratorNext(mContained, &vcontained, false);

  if (rval != rcont)
  {
       
                                                                          
                                                                     
                                                                        
                                               
       
    Assert(rval == WJB_BEGIN_OBJECT || rval == WJB_BEGIN_ARRAY);
    Assert(rcont == WJB_BEGIN_OBJECT || rcont == WJB_BEGIN_ARRAY);
    return false;
  }
  else if (rcont == WJB_BEGIN_OBJECT)
  {
    Assert(vval.type == jbvObject);
    Assert(vcontained.type == jbvObject);

       
                                                                          
                                                                       
                                                                     
                                                                       
                                                                        
       
    if (vval.val.object.nPairs < vcontained.val.object.nPairs)
    {
      return false;
    }

                                                           
    for (;;)
    {
      JsonbValue *lhsVal;                                        

      rcont = JsonbIteratorNext(mContained, &vcontained, false);

         
                                                                    
                                                                
                    
         
      if (rcont == WJB_END_OBJECT)
      {
        return true;
      }

      Assert(rcont == WJB_KEY);

                                       
      lhsVal = findJsonbValueFromContainer((*val)->container, JB_FOBJECT, &vcontained);

      if (!lhsVal)
      {
        return false;
      }

         
                                                                      
                                  
         
      rcont = JsonbIteratorNext(mContained, &vcontained, true);

      Assert(rcont == WJB_VALUE);

         
                                                                         
             
         
      if (lhsVal->type != vcontained.type)
      {
        return false;
      }
      else if (IsAJsonbScalar(lhsVal))
      {
        if (!equalsJsonbScalarValue(lhsVal, &vcontained))
        {
          return false;
        }
      }
      else
      {
                                                      
        JsonbIterator *nestval, *nestContained;

        Assert(lhsVal->type == jbvBinary);
        Assert(vcontained.type == jbvBinary);

        nestval = JsonbIteratorInit(lhsVal->val.binary.data);
        nestContained = JsonbIteratorInit(vcontained.val.binary.data);

           
                                                                      
                                    
           
                                                                  
                                                                    
                                                                   
                                                                  
                                                                   
                                                                 
                                                                     
                                                         
           
                                                                     
                                                              
                                                                       
                                                                     
                                                                   
                   
           
        if (!JsonbDeepContains(&nestval, &nestContained))
        {
          return false;
        }
      }
    }
  }
  else if (rcont == WJB_BEGIN_ARRAY)
  {
    JsonbValue *lhsConts = NULL;
    uint32 nLhsElems = vval.val.array.nElems;

    Assert(vval.type == jbvArray);
    Assert(vcontained.type == jbvArray);

       
                                                                       
               
       
                                                                     
                                                                           
                                                                           
                                                                          
                                                          
       
    if (vval.val.array.rawScalar && !vcontained.val.array.rawScalar)
    {
      return false;
    }

                                                          
    for (;;)
    {
      rcont = JsonbIteratorNext(mContained, &vcontained, true);

         
                                                                    
                                                               
                    
         
      if (rcont == WJB_END_ARRAY)
      {
        return true;
      }

      Assert(rcont == WJB_ELEM);

      if (IsAJsonbScalar(&vcontained))
      {
        if (!findJsonbValueFromContainer((*val)->container, JB_FARRAY, &vcontained))
        {
          return false;
        }
      }
      else
      {
        uint32 i;

           
                                                                  
                                                           
           
        if (lhsConts == NULL)
        {
          uint32 j = 0;

                                                 
          lhsConts = palloc(sizeof(JsonbValue) * nLhsElems);

          for (i = 0; i < nLhsElems; i++)
          {
                                                      
            rcont = JsonbIteratorNext(val, &vval, true);
            Assert(rcont == WJB_ELEM);

            if (vval.type == jbvBinary)
            {
              lhsConts[j++] = vval;
            }
          }

                                                                   
          if (j == 0)
          {
            return false;
          }

                                                       
          nLhsElems = j;
        }

                                                     
        for (i = 0; i < nLhsElems; i++)
        {
                                                        
          JsonbIterator *nestval, *nestContained;
          bool contains;

          nestval = JsonbIteratorInit(lhsConts[i].val.binary.data);
          nestContained = JsonbIteratorInit(vcontained.val.binary.data);

          contains = JsonbDeepContains(&nestval, &nestContained);

          if (nestval)
          {
            pfree(nestval);
          }
          if (nestContained)
          {
            pfree(nestContained);
          }
          if (contains)
          {
            break;
          }
        }

           
                                                                   
                                                  
           
        if (i == nLhsElems)
        {
          return false;
        }
      }
    }
  }
  else
  {
    elog(ERROR, "invalid jsonb container type");
  }

  elog(ERROR, "unexpectedly fell off end of jsonb container");
  return false;
}

   
                                                                          
                                
   
                                                                          
          
   
void
JsonbHashScalarValue(const JsonbValue *scalarVal, uint32 *hash)
{
  uint32 tmp;

                                        
  switch (scalarVal->type)
  {
  case jbvNull:
    tmp = 0x01;
    break;
  case jbvString:
    tmp = DatumGetUInt32(hash_any((const unsigned char *)scalarVal->val.string.val, scalarVal->val.string.len));
    break;
  case jbvNumeric:
                                                      
    tmp = DatumGetUInt32(DirectFunctionCall1(hash_numeric, NumericGetDatum(scalarVal->val.numeric)));
    break;
  case jbvBool:
    tmp = scalarVal->val.boolean ? 0x02 : 0x04;

    break;
  default:
    elog(ERROR, "invalid jsonb scalar type");
    tmp = 0;                          
    break;
  }

     
                                                                             
                                                            
                                     
     
  *hash = (*hash << 1) | (*hash >> 31);
  *hash ^= tmp;
}

   
                                                                      
                         
   
void
JsonbHashScalarValueExtended(const JsonbValue *scalarVal, uint64 *hash, uint64 seed)
{
  uint64 tmp;

  switch (scalarVal->type)
  {
  case jbvNull:
    tmp = seed + 0x01;
    break;
  case jbvString:
    tmp = DatumGetUInt64(hash_any_extended((const unsigned char *)scalarVal->val.string.val, scalarVal->val.string.len, seed));
    break;
  case jbvNumeric:
    tmp = DatumGetUInt64(DirectFunctionCall2(hash_numeric_extended, NumericGetDatum(scalarVal->val.numeric), UInt64GetDatum(seed)));
    break;
  case jbvBool:
    if (seed)
    {
      tmp = DatumGetUInt64(DirectFunctionCall2(hashcharextended, BoolGetDatum(scalarVal->val.boolean), UInt64GetDatum(seed)));
    }
    else
    {
      tmp = scalarVal->val.boolean ? 0x02 : 0x04;
    }

    break;
  default:
    elog(ERROR, "invalid jsonb scalar type");
    break;
  }

  *hash = ROTATE_HIGH_AND_LOW_32BITS(*hash);
  *hash ^= tmp;
}

   
                                                              
   
static bool
equalsJsonbScalarValue(JsonbValue *aScalar, JsonbValue *bScalar)
{
  if (aScalar->type == bScalar->type)
  {
    switch (aScalar->type)
    {
    case jbvNull:
      return true;
    case jbvString:
      return lengthCompareJsonbStringValue(aScalar, bScalar) == 0;
    case jbvNumeric:
      return DatumGetBool(DirectFunctionCall2(numeric_eq, PointerGetDatum(aScalar->val.numeric), PointerGetDatum(bScalar->val.numeric)));
    case jbvBool:
      return aScalar->val.boolean == bScalar->val.boolean;

    default:
      elog(ERROR, "invalid jsonb scalar type");
    }
  }
  elog(ERROR, "jsonb scalar type mismatch");
  return false;
}

   
                                                          
   
                                                                     
                                                                
   
static int
compareJsonbScalarValue(JsonbValue *aScalar, JsonbValue *bScalar)
{
  if (aScalar->type == bScalar->type)
  {
    switch (aScalar->type)
    {
    case jbvNull:
      return 0;
    case jbvString:
      return varstr_cmp(aScalar->val.string.val, aScalar->val.string.len, bScalar->val.string.val, bScalar->val.string.len, DEFAULT_COLLATION_OID);
    case jbvNumeric:
      return DatumGetInt32(DirectFunctionCall2(numeric_cmp, PointerGetDatum(aScalar->val.numeric), PointerGetDatum(bScalar->val.numeric)));
    case jbvBool:
      if (aScalar->val.boolean == bScalar->val.boolean)
      {
        return 0;
      }
      else if (aScalar->val.boolean > bScalar->val.boolean)
      {
        return 1;
      }
      else
      {
        return -1;
      }
    default:
      elog(ERROR, "invalid jsonb scalar type");
    }
  }
  elog(ERROR, "jsonb scalar type mismatch");
  return -1;
}

   
                                                                            
                    
   

   
                                                                             
                                                                           
                                                
   
static int
reserveFromBuffer(StringInfo buffer, int len)
{
  int offset;

                                
  enlargeStringInfo(buffer, len);

                               
  offset = buffer->len;

                         
  buffer->len += len;

     
                                                                           
                                                           
     
  buffer->data[buffer->len] = '\0';

  return offset;
}

   
                                                             
   
static void
copyToBuffer(StringInfo buffer, int offset, const char *data, int len)
{
  memcpy(buffer->data + offset, data, len);
}

   
                                                     
   
static void
appendToBuffer(StringInfo buffer, const char *data, int len)
{
  int offset;

  offset = reserveFromBuffer(buffer, len);
  copyToBuffer(buffer, offset, data, len);
}

   
                                                                        
                                                 
   
static short
padBufferToInt(StringInfo buffer)
{
  int padlen, p, offset;

  padlen = INTALIGN(buffer->len) - buffer->len;

  offset = reserveFromBuffer(buffer, padlen);

                                                                      
  for (p = 0; p < padlen; p++)
  {
    buffer->data[offset + p] = '\0';
  }

  return padlen;
}

   
                                                                 
   
static Jsonb *
convertToJsonb(JsonbValue *val)
{
  StringInfoData buffer;
  JEntry jentry;
  Jsonb *res;

                                                     
  Assert(val->type != jbvBinary);

                                                                
  initStringInfo(&buffer);

                                        
  reserveFromBuffer(&buffer, VARHDRSZ);

  convertJsonbValue(&buffer, &jentry, val, 0);

     
                                                                   
                                                                             
                     
     

  res = (Jsonb *)buffer.data;

  SET_VARSIZE(res, buffer.len);

  return res;
}

   
                                                                          
   
                                                                            
                                                                           
                                                                               
                       
   
                                                                              
                           
   
static void
convertJsonbValue(StringInfo buffer, JEntry *header, JsonbValue *val, int level)
{
  check_stack_depth();

  if (!val)
  {
    return;
  }

     
                                                                           
                                                                             
                                                                            
                                                         
     

  if (IsAJsonbScalar(val))
  {
    convertJsonbScalar(buffer, header, val);
  }
  else if (val->type == jbvArray)
  {
    convertJsonbArray(buffer, header, val, level);
  }
  else if (val->type == jbvObject)
  {
    convertJsonbObject(buffer, header, val, level);
  }
  else
  {
    elog(ERROR, "unknown type of jsonb container to convert");
  }
}

static void
convertJsonbArray(StringInfo buffer, JEntry *pheader, JsonbValue *val, int level)
{
  int base_offset;
  int jentry_offset;
  int i;
  int totallen;
  uint32 header;
  int nElems = val->val.array.nElems;

                                                       
  base_offset = buffer->len;

                                                                        
  padBufferToInt(buffer);

     
                                                                      
                              
     
  header = nElems | JB_FARRAY;
  if (val->val.array.rawScalar)
  {
    Assert(nElems == 1);
    Assert(level == 0);
    header |= JB_FSCALAR;
  }

  appendToBuffer(buffer, (char *)&header, sizeof(uint32));

                                                       
  jentry_offset = reserveFromBuffer(buffer, sizeof(JEntry) * nElems);

  totallen = 0;
  for (i = 0; i < nElems; i++)
  {
    JsonbValue *elem = &val->val.array.elems[i];
    int len;
    JEntry meta;

       
                                                             
                                      
       
    convertJsonbValue(buffer, &meta, elem, level + 1);

    len = JBE_OFFLENFLD(meta);
    totallen += len;

       
                                                                         
                                                                       
                                                                
       
    if (totallen > JENTRY_OFFLENMASK)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("total size of jsonb array elements exceeds the maximum of %u bytes", JENTRY_OFFLENMASK)));
    }

       
                                                             
       
    if ((i % JB_OFFSET_STRIDE) == 0)
    {
      meta = (meta & JENTRY_TYPEMASK) | totallen | JENTRY_HAS_OFF;
    }

    copyToBuffer(buffer, jentry_offset, (char *)&meta, sizeof(JEntry));
    jentry_offset += sizeof(JEntry);
  }

                                                              
  totallen = buffer->len - base_offset;

                                                                      
  if (totallen > JENTRY_OFFLENMASK)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("total size of jsonb array elements exceeds the maximum of %u bytes", JENTRY_OFFLENMASK)));
  }

                                                                          
  *pheader = JENTRY_ISCONTAINER | totallen;
}

static void
convertJsonbObject(StringInfo buffer, JEntry *pheader, JsonbValue *val, int level)
{
  int base_offset;
  int jentry_offset;
  int i;
  int totallen;
  uint32 header;
  int nPairs = val->val.object.nPairs;

                                                        
  base_offset = buffer->len;

                                                                        
  padBufferToInt(buffer);

     
                                                                      
                              
     
  header = nPairs | JB_FOBJECT;
  appendToBuffer(buffer, (char *)&header, sizeof(uint32));

                                                              
  jentry_offset = reserveFromBuffer(buffer, sizeof(JEntry) * nPairs * 2);

     
                                                                             
                                            
     
  totallen = 0;
  for (i = 0; i < nPairs; i++)
  {
    JsonbPair *pair = &val->val.object.pairs[i];
    int len;
    JEntry meta;

       
                                                                         
                      
       
    convertJsonbScalar(buffer, &meta, &pair->key);

    len = JBE_OFFLENFLD(meta);
    totallen += len;

       
                                                                         
                                                                       
                                                                
       
    if (totallen > JENTRY_OFFLENMASK)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("total size of jsonb object elements exceeds the maximum of %u bytes", JENTRY_OFFLENMASK)));
    }

       
                                                             
       
    if ((i % JB_OFFSET_STRIDE) == 0)
    {
      meta = (meta & JENTRY_TYPEMASK) | totallen | JENTRY_HAS_OFF;
    }

    copyToBuffer(buffer, jentry_offset, (char *)&meta, sizeof(JEntry));
    jentry_offset += sizeof(JEntry);
  }
  for (i = 0; i < nPairs; i++)
  {
    JsonbPair *pair = &val->val.object.pairs[i];
    int len;
    JEntry meta;

       
                                                                           
                      
       
    convertJsonbValue(buffer, &meta, &pair->value, level + 1);

    len = JBE_OFFLENFLD(meta);
    totallen += len;

       
                                                                         
                                                                       
                                                                
       
    if (totallen > JENTRY_OFFLENMASK)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("total size of jsonb object elements exceeds the maximum of %u bytes", JENTRY_OFFLENMASK)));
    }

       
                                                             
       
    if (((i + nPairs) % JB_OFFSET_STRIDE) == 0)
    {
      meta = (meta & JENTRY_TYPEMASK) | totallen | JENTRY_HAS_OFF;
    }

    copyToBuffer(buffer, jentry_offset, (char *)&meta, sizeof(JEntry));
    jentry_offset += sizeof(JEntry);
  }

                                                              
  totallen = buffer->len - base_offset;

                                                                      
  if (totallen > JENTRY_OFFLENMASK)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("total size of jsonb object elements exceeds the maximum of %u bytes", JENTRY_OFFLENMASK)));
  }

                                                                          
  *pheader = JENTRY_ISCONTAINER | totallen;
}

static void
convertJsonbScalar(StringInfo buffer, JEntry *jentry, JsonbValue *scalarVal)
{
  int numlen;
  short padlen;

  switch (scalarVal->type)
  {
  case jbvNull:
    *jentry = JENTRY_ISNULL;
    break;

  case jbvString:
    appendToBuffer(buffer, scalarVal->val.string.val, scalarVal->val.string.len);

    *jentry = scalarVal->val.string.len;
    break;

  case jbvNumeric:
    numlen = VARSIZE_ANY(scalarVal->val.numeric);
    padlen = padBufferToInt(buffer);

    appendToBuffer(buffer, (char *)scalarVal->val.numeric, numlen);

    *jentry = JENTRY_ISNUMERIC | (padlen + numlen);
    break;

  case jbvBool:
    *jentry = (scalarVal->val.boolean) ? JENTRY_ISBOOL_TRUE : JENTRY_ISBOOL_FALSE;
    break;

  default:
    elog(ERROR, "invalid jsonb scalar type");
  }
}

   
                                                     
   
                                                                        
                                                                               
                                                                            
                                                                           
          
   
                                                                        
                                                              
   
static int
lengthCompareJsonbStringValue(const void *a, const void *b)
{
  const JsonbValue *va = (const JsonbValue *)a;
  const JsonbValue *vb = (const JsonbValue *)b;
  int res;

  Assert(va->type == jbvString);
  Assert(vb->type == jbvString);

  if (va->val.string.len == vb->val.string.len)
  {
    res = memcmp(va->val.string.val, vb->val.string.val, va->val.string.len);
  }
  else
  {
    res = (va->val.string.len > vb->val.string.len) ? 1 : -1;
  }

  return res;
}

   
                                                       
   
                                                                                
                                                                             
                                                                      
   
                                                  
   
                                                                              
   
static int
lengthCompareJsonbPair(const void *a, const void *b, void *binequal)
{
  const JsonbPair *pa = (const JsonbPair *)a;
  const JsonbPair *pb = (const JsonbPair *)b;
  int res;

  res = lengthCompareJsonbStringValue(&pa->key, &pb->key);
  if (res == 0 && binequal)
  {
    *((bool *)binequal) = true;
  }

     
                                                                          
                             
     
  if (res == 0)
  {
    res = (pa->order > pb->order) ? -1 : 1;
  }

  return res;
}

   
                                                  
   
static void
uniqueifyJsonbObject(JsonbValue *object)
{
  bool hasNonUniq = false;

  Assert(object->type == jbvObject);

  if (object->val.object.nPairs > 1)
  {
    qsort_arg(object->val.object.pairs, object->val.object.nPairs, sizeof(JsonbPair), lengthCompareJsonbPair, &hasNonUniq);
  }

  if (hasNonUniq)
  {
    JsonbPair *ptr = object->val.object.pairs + 1, *res = object->val.object.pairs;

    while (ptr - object->val.object.pairs < object->val.object.nPairs)
    {
                                        
      if (lengthCompareJsonbStringValue(ptr, res) != 0)
      {
        res++;
        if (ptr != res)
        {
          memcpy(res, ptr, sizeof(JsonbPair));
        }
      }
      ptr++;
    }

    object->val.object.nPairs = res + 1 - object->val.object.pairs;
  }
}
