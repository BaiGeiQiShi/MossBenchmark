                                                                            
   
                 
                                                              
   
                                                                           
                                                                           
                                                                           
                                                                          
                                                                             
   
                                                                          
                                                                         
                                                                    
                                                                            
            
   
                                                                           
                                                                             
                                                                               
                                                                              
                                                                            
   
                                                                        
                                                                          
                                                                     
                                                                           
                                                                          
                                                                   
   
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/spgist.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/pg_locale.h"
#include "utils/varlena.h"

   
                                                                             
                                                                        
                                                                           
                                                                            
                                                                             
                                                                           
                                                                    
                                                                              
                                                                              
                                                                           
                                                                               
                                                                               
                                                                       
                                                                            
                 
   
#define SPGIST_MAX_PREFIX_LENGTH Max((int)(BLCKSZ - 258 * 16 - 100), 32)

   
                                                                            
                     
   
                                                                                
                           
                                
                                   
                              
   
#define SPG_STRATEGY_ADDITION (10)
#define SPG_IS_COLLATION_AWARE_STRATEGY(s) ((s) > SPG_STRATEGY_ADDITION && (s) != RTPrefixStrategyNumber)

                                            
typedef struct spgNodePtr
{
  Datum d;
  int i;
  int16 c;
} spgNodePtr;

Datum
spg_text_config(PG_FUNCTION_ARGS)
{
                                                                  
  spgConfigOut *cfg = (spgConfigOut *)PG_GETARG_POINTER(1);

  cfg->prefixType = TEXTOID;
  cfg->labelType = INT2OID;
  cfg->canReturnData = true;
  cfg->longValuesOK = true;                                         
  PG_RETURN_VOID();
}

   
                                                                            
                                                 
   
static Datum
formTextDatum(const char *data, int datalen)
{
  char *p;

  p = (char *)palloc(datalen + VARHDRSZ);

  if (datalen + VARHDRSZ_SHORT <= VARATT_SHORT_MAX)
  {
    SET_VARSIZE_SHORT(p, datalen + VARHDRSZ_SHORT);
    if (datalen)
    {
      memcpy(p + VARHDRSZ_SHORT, data, datalen);
    }
  }
  else
  {
    SET_VARSIZE(p, datalen + VARHDRSZ);
    memcpy(p + VARHDRSZ, data, datalen);
  }

  return PointerGetDatum(p);
}

   
                                                   
   
static int
commonPrefix(const char *a, const char *b, int lena, int lenb)
{
  int i = 0;

  while (i < lena && i < lenb && *a == *b)
  {
    a++;
    b++;
    i++;
  }

  return i;
}

   
                                                           
   
                                                                               
   
static bool
searchChar(Datum *nodeLabels, int nNodes, int16 c, int *i)
{
  int StopLow = 0, StopHigh = nNodes;

  while (StopLow < StopHigh)
  {
    int StopMiddle = (StopLow + StopHigh) >> 1;
    int16 middle = DatumGetInt16(nodeLabels[StopMiddle]);

    if (c < middle)
    {
      StopHigh = StopMiddle;
    }
    else if (c > middle)
    {
      StopLow = StopMiddle + 1;
    }
    else
    {
      *i = StopMiddle;
      return true;
    }
  }

  *i = StopHigh;
  return false;
}

Datum
spg_text_choose(PG_FUNCTION_ARGS)
{
  spgChooseIn *in = (spgChooseIn *)PG_GETARG_POINTER(0);
  spgChooseOut *out = (spgChooseOut *)PG_GETARG_POINTER(1);
  text *inText = DatumGetTextPP(in->datum);
  char *inStr = VARDATA_ANY(inText);
  int inSize = VARSIZE_ANY_EXHDR(inText);
  char *prefixStr = NULL;
  int prefixSize = 0;
  int commonLen = 0;
  int16 nodeChar = 0;
  int i = 0;

                                                                       
  if (in->hasPrefix)
  {
    text *prefixText = DatumGetTextPP(in->prefixDatum);

    prefixStr = VARDATA_ANY(prefixText);
    prefixSize = VARSIZE_ANY_EXHDR(prefixText);

    commonLen = commonPrefix(inStr + in->level, prefixStr, inSize - in->level, prefixSize);

    if (commonLen == prefixSize)
    {
      if (inSize - in->level > commonLen)
      {
        nodeChar = *(unsigned char *)(inStr + in->level + commonLen);
      }
      else
      {
        nodeChar = -1;
      }
    }
    else
    {
                                                                        
      out->resultType = spgSplitTuple;

      if (commonLen == 0)
      {
        out->result.splitTuple.prefixHasPrefix = false;
      }
      else
      {
        out->result.splitTuple.prefixHasPrefix = true;
        out->result.splitTuple.prefixPrefixDatum = formTextDatum(prefixStr, commonLen);
      }
      out->result.splitTuple.prefixNNodes = 1;
      out->result.splitTuple.prefixNodeLabels = (Datum *)palloc(sizeof(Datum));
      out->result.splitTuple.prefixNodeLabels[0] = Int16GetDatum(*(unsigned char *)(prefixStr + commonLen));

      out->result.splitTuple.childNodeN = 0;

      if (prefixSize - commonLen == 1)
      {
        out->result.splitTuple.postfixHasPrefix = false;
      }
      else
      {
        out->result.splitTuple.postfixHasPrefix = true;
        out->result.splitTuple.postfixPrefixDatum = formTextDatum(prefixStr + commonLen + 1, prefixSize - commonLen - 1);
      }

      PG_RETURN_VOID();
    }
  }
  else if (inSize > in->level)
  {
    nodeChar = *(unsigned char *)(inStr + in->level);
  }
  else
  {
    nodeChar = -1;
  }

                                                
  if (searchChar(in->nodeLabels, in->nNodes, nodeChar, &i))
  {
       
                                                                         
                                                                          
                                                                           
                                                               
       
    int levelAdd;

    out->resultType = spgMatchNode;
    out->result.matchNode.nodeN = i;
    levelAdd = commonLen;
    if (nodeChar >= 0)
    {
      levelAdd++;
    }
    out->result.matchNode.levelAdd = levelAdd;
    if (inSize - in->level - levelAdd > 0)
    {
      out->result.matchNode.restDatum = formTextDatum(inStr + in->level + levelAdd, inSize - in->level - levelAdd);
    }
    else
    {
      out->result.matchNode.restDatum = formTextDatum(NULL, 0);
    }
  }
  else if (in->allTheSame)
  {
       
                                                                          
                                                                        
                                                                     
                                     
       
                                                                         
                                                                           
                                                                         
                                                                       
       
    out->resultType = spgSplitTuple;
    out->result.splitTuple.prefixHasPrefix = in->hasPrefix;
    out->result.splitTuple.prefixPrefixDatum = in->prefixDatum;
    out->result.splitTuple.prefixNNodes = 1;
    out->result.splitTuple.prefixNodeLabels = (Datum *)palloc(sizeof(Datum));
    out->result.splitTuple.prefixNodeLabels[0] = Int16GetDatum(-2);
    out->result.splitTuple.childNodeN = 0;
    out->result.splitTuple.postfixHasPrefix = false;
  }
  else
  {
                                                               
    out->resultType = spgAddNode;
    out->result.addNode.nodeLabel = Int16GetDatum(nodeChar);
    out->result.addNode.nodeN = i;
  }

  PG_RETURN_VOID();
}

                                                        
static int
cmpNodePtr(const void *a, const void *b)
{
  const spgNodePtr *aa = (const spgNodePtr *)a;
  const spgNodePtr *bb = (const spgNodePtr *)b;

  return aa->c - bb->c;
}

Datum
spg_text_picksplit(PG_FUNCTION_ARGS)
{
  spgPickSplitIn *in = (spgPickSplitIn *)PG_GETARG_POINTER(0);
  spgPickSplitOut *out = (spgPickSplitOut *)PG_GETARG_POINTER(1);
  text *text0 = DatumGetTextPP(in->datums[0]);
  int i, commonLen;
  spgNodePtr *nodes;

                                              
  commonLen = VARSIZE_ANY_EXHDR(text0);
  for (i = 1; i < in->nTuples && commonLen > 0; i++)
  {
    text *texti = DatumGetTextPP(in->datums[i]);
    int tmp = commonPrefix(VARDATA_ANY(text0), VARDATA_ANY(texti), VARSIZE_ANY_EXHDR(text0), VARSIZE_ANY_EXHDR(texti));

    if (tmp < commonLen)
    {
      commonLen = tmp;
    }
  }

     
                                                                         
                                     
     
  commonLen = Min(commonLen, SPGIST_MAX_PREFIX_LENGTH);

                                                            
  if (commonLen == 0)
  {
    out->hasPrefix = false;
  }
  else
  {
    out->hasPrefix = true;
    out->prefixDatum = formTextDatum(VARDATA_ANY(text0), commonLen);
  }

                                                                      
  nodes = (spgNodePtr *)palloc(sizeof(spgNodePtr) * in->nTuples);

  for (i = 0; i < in->nTuples; i++)
  {
    text *texti = DatumGetTextPP(in->datums[i]);

    if (commonLen < VARSIZE_ANY_EXHDR(texti))
    {
      nodes[i].c = *(unsigned char *)(VARDATA_ANY(texti) + commonLen);
    }
    else
    {
      nodes[i].c = -1;                                     
    }
    nodes[i].i = i;
    nodes[i].d = in->datums[i];
  }

     
                                                                            
                                                                          
                                         
     
  qsort(nodes, in->nTuples, sizeof(*nodes), cmpNodePtr);

                        
  out->nNodes = 0;
  out->nodeLabels = (Datum *)palloc(sizeof(Datum) * in->nTuples);
  out->mapTuplesToNodes = (int *)palloc(sizeof(int) * in->nTuples);
  out->leafTupleDatums = (Datum *)palloc(sizeof(Datum) * in->nTuples);

  for (i = 0; i < in->nTuples; i++)
  {
    text *texti = DatumGetTextPP(nodes[i].d);
    Datum leafD;

    if (i == 0 || nodes[i].c != nodes[i - 1].c)
    {
      out->nodeLabels[out->nNodes] = Int16GetDatum(nodes[i].c);
      out->nNodes++;
    }

    if (commonLen < VARSIZE_ANY_EXHDR(texti))
    {
      leafD = formTextDatum(VARDATA_ANY(texti) + commonLen + 1, VARSIZE_ANY_EXHDR(texti) - commonLen - 1);
    }
    else
    {
      leafD = formTextDatum(NULL, 0);
    }

    out->leafTupleDatums[nodes[i].i] = leafD;
    out->mapTuplesToNodes[nodes[i].i] = out->nNodes - 1;
  }

  PG_RETURN_VOID();
}

Datum
spg_text_inner_consistent(PG_FUNCTION_ARGS)
{
  spgInnerConsistentIn *in = (spgInnerConsistentIn *)PG_GETARG_POINTER(0);
  spgInnerConsistentOut *out = (spgInnerConsistentOut *)PG_GETARG_POINTER(1);
  bool collate_is_c = lc_collate_is_c(PG_GET_COLLATION());
  text *reconstructedValue;
  text *reconstrText;
  int maxReconstrLen;
  text *prefixText = NULL;
  int prefixSize = 0;
  int i;

     
                                                                          
                                                                        
                                                                           
                                                                         
     
                                                                           
                                                                          
                                                                          
                                       
     
  reconstructedValue = (text *)DatumGetPointer(in->reconstructedValue);
  Assert(reconstructedValue == NULL ? in->level == 0 : VARSIZE_ANY_EXHDR(reconstructedValue) == in->level);

  maxReconstrLen = in->level + 1;
  if (in->hasPrefix)
  {
    prefixText = DatumGetTextPP(in->prefixDatum);
    prefixSize = VARSIZE_ANY_EXHDR(prefixText);
    maxReconstrLen += prefixSize;
  }

  reconstrText = palloc(VARHDRSZ + maxReconstrLen);
  SET_VARSIZE(reconstrText, VARHDRSZ + maxReconstrLen);

  if (in->level)
  {
    memcpy(VARDATA(reconstrText), VARDATA(reconstructedValue), in->level);
  }
  if (prefixSize)
  {
    memcpy(((char *)VARDATA(reconstrText)) + in->level, VARDATA_ANY(prefixText), prefixSize);
  }
                                                         

     
                                                                           
                                                                           
                        
     
  out->nodeNumbers = (int *)palloc(sizeof(int) * in->nNodes);
  out->levelAdds = (int *)palloc(sizeof(int) * in->nNodes);
  out->reconstructedValues = (Datum *)palloc(sizeof(Datum) * in->nNodes);
  out->nNodes = 0;

  for (i = 0; i < in->nNodes; i++)
  {
    int16 nodeChar = DatumGetInt16(in->nodeLabels[i]);
    int thisLen;
    bool res = true;
    int j;

                                                                
    if (nodeChar <= 0)
    {
      thisLen = maxReconstrLen - 1;
    }
    else
    {
      ((unsigned char *)VARDATA(reconstrText))[maxReconstrLen - 1] = nodeChar;
      thisLen = maxReconstrLen;
    }

    for (j = 0; j < in->nkeys; j++)
    {
      StrategyNumber strategy = in->scankeys[j].sk_strategy;
      text *inText;
      int inSize;
      int r;

         
                                                                        
                                                                       
                                                                       
                                                                      
                                                                       
                                                                   
         
      if (SPG_IS_COLLATION_AWARE_STRATEGY(strategy))
      {
        if (collate_is_c)
        {
          strategy -= SPG_STRATEGY_ADDITION;
        }
        else
        {
          continue;
        }
      }

      inText = DatumGetTextPP(in->scankeys[j].sk_argument);
      inSize = VARSIZE_ANY_EXHDR(inText);

      r = memcmp(VARDATA(reconstrText), VARDATA_ANY(inText), Min(inSize, thisLen));

      switch (strategy)
      {
      case BTLessStrategyNumber:
      case BTLessEqualStrategyNumber:
        if (r > 0)
        {
          res = false;
        }
        break;
      case BTEqualStrategyNumber:
        if (r != 0 || inSize < thisLen)
        {
          res = false;
        }
        break;
      case BTGreaterEqualStrategyNumber:
      case BTGreaterStrategyNumber:
        if (r < 0)
        {
          res = false;
        }
        break;
      case RTPrefixStrategyNumber:
        if (r != 0)
        {
          res = false;
        }
        break;
      default:
        elog(ERROR, "unrecognized strategy number: %d", in->scankeys[j].sk_strategy);
        break;
      }

      if (!res)
      {
        break;                                               
      }
    }

    if (res)
    {
      out->nodeNumbers[out->nNodes] = i;
      out->levelAdds[out->nNodes] = thisLen - in->level;
      SET_VARSIZE(reconstrText, VARHDRSZ + thisLen);
      out->reconstructedValues[out->nNodes] = datumCopy(PointerGetDatum(reconstrText), false, -1);
      out->nNodes++;
    }
  }

  PG_RETURN_VOID();
}

Datum
spg_text_leaf_consistent(PG_FUNCTION_ARGS)
{
  spgLeafConsistentIn *in = (spgLeafConsistentIn *)PG_GETARG_POINTER(0);
  spgLeafConsistentOut *out = (spgLeafConsistentOut *)PG_GETARG_POINTER(1);
  int level = in->level;
  text *leafValue, *reconstrValue = NULL;
  char *fullValue;
  int fullLen;
  bool res;
  int j;

                           
  out->recheck = false;

  leafValue = DatumGetTextPP(in->leafDatum);

                                                                
  if (DatumGetPointer(in->reconstructedValue))
  {
    reconstrValue = (text *)DatumGetPointer(in->reconstructedValue);
  }

  Assert(reconstrValue == NULL ? level == 0 : VARSIZE_ANY_EXHDR(reconstrValue) == level);

                                                                  
  fullLen = level + VARSIZE_ANY_EXHDR(leafValue);
  if (VARSIZE_ANY_EXHDR(leafValue) == 0 && level > 0)
  {
    fullValue = VARDATA(reconstrValue);
    out->leafValue = PointerGetDatum(reconstrValue);
  }
  else
  {
    text *fullText = palloc(VARHDRSZ + fullLen);

    SET_VARSIZE(fullText, VARHDRSZ + fullLen);
    fullValue = VARDATA(fullText);
    if (level)
    {
      memcpy(fullValue, VARDATA(reconstrValue), level);
    }
    if (VARSIZE_ANY_EXHDR(leafValue) > 0)
    {
      memcpy(fullValue + level, VARDATA_ANY(leafValue), VARSIZE_ANY_EXHDR(leafValue));
    }
    out->leafValue = PointerGetDatum(fullText);
  }

                                          
  res = true;
  for (j = 0; j < in->nkeys; j++)
  {
    StrategyNumber strategy = in->scankeys[j].sk_strategy;
    text *query = DatumGetTextPP(in->scankeys[j].sk_argument);
    int queryLen = VARSIZE_ANY_EXHDR(query);
    int r;

    if (strategy == RTPrefixStrategyNumber)
    {
         
                                                                        
                                                                    
         
      res = (level >= queryLen) || DatumGetBool(DirectFunctionCall2Coll(text_starts_with, PG_GET_COLLATION(), out->leafValue, PointerGetDatum(query)));

      if (!res)                                               
      {
        break;
      }

      continue;
    }

    if (SPG_IS_COLLATION_AWARE_STRATEGY(strategy))
    {
                                      
      strategy -= SPG_STRATEGY_ADDITION;

                                                                       
      Assert(pg_verifymbstr(fullValue, fullLen, false));

      r = varstr_cmp(fullValue, fullLen, VARDATA_ANY(query), queryLen, PG_GET_COLLATION());
    }
    else
    {
                                          
      r = memcmp(fullValue, VARDATA_ANY(query), Min(queryLen, fullLen));

      if (r == 0)
      {
        if (queryLen > fullLen)
        {
          r = -1;
        }
        else if (queryLen < fullLen)
        {
          r = 1;
        }
      }
    }

    switch (strategy)
    {
    case BTLessStrategyNumber:
      res = (r < 0);
      break;
    case BTLessEqualStrategyNumber:
      res = (r <= 0);
      break;
    case BTEqualStrategyNumber:
      res = (r == 0);
      break;
    case BTGreaterEqualStrategyNumber:
      res = (r >= 0);
      break;
    case BTGreaterStrategyNumber:
      res = (r > 0);
      break;
    default:
      elog(ERROR, "unrecognized strategy number: %d", in->scankeys[j].sk_strategy);
      res = false;
      break;
    }

    if (!res)
    {
      break;                                               
    }
  }

  PG_RETURN_BOOL(res);
}
