                                                                            
   
              
                                                     
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include <time.h>

#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "commands/progress.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct BTSortArrayContext
{
  FmgrInfo flinfo;
  Oid collation;
  bool reverse;
} BTSortArrayContext;

static Datum
_bt_find_extreme_element(IndexScanDesc scan, ScanKey skey, StrategyNumber strat, Datum *elems, int nelems);
static int
_bt_sort_array_elements(IndexScanDesc scan, ScanKey skey, bool reverse, Datum *elems, int nelems);
static int
_bt_compare_array_elements(const void *a, const void *b, void *arg);
static bool
_bt_compare_scankey_args(IndexScanDesc scan, ScanKey op, ScanKey leftarg, ScanKey rightarg, bool *result);
static bool
_bt_fix_scankey_strategy(ScanKey skey, int16 *indoption);
static void
_bt_mark_scankey_required(ScanKey skey);
static bool
_bt_check_rowcompare(ScanKey skey, IndexTuple tuple, int tupnatts, TupleDesc tupdesc, ScanDirection dir, bool *continuescan);
static int
_bt_keep_natts(Relation rel, IndexTuple lastleft, IndexTuple firstright, BTScanInsert itup_key);

   
                 
                                                                        
                                                                     
   
                                                                       
                                                                        
                                                                        
                                                                         
                                                                      
                                                                          
                     
   
                                                                          
                                                                        
                                                                        
                                                                       
                                                                   
             
   
                                                                     
                                                                      
                                                                  
                                                                       
                                                                
   
BTScanInsert
_bt_mkscankey(Relation rel, IndexTuple itup)
{
  BTScanInsert key;
  ScanKey skey;
  TupleDesc itupdesc;
  int indnkeyatts;
  int16 *indoption;
  int tupnatts;
  int i;

  itupdesc = RelationGetDescr(rel);
  indnkeyatts = IndexRelationGetNumberOfKeyAttributes(rel);
  indoption = rel->rd_indoption;
  tupnatts = itup ? BTreeTupleGetNAtts(itup, rel) : 0;

  Assert(tupnatts <= IndexRelationGetNumberOfAttributes(rel));

     
                                                                     
                                                                            
               
     
  key = palloc(offsetof(BTScanInsertData, scankeys) + sizeof(ScanKeyData) * indnkeyatts);
  key->heapkeyspace = itup == NULL || _bt_heapkeyspace(rel);
  key->anynullkeys = false;                         
  key->nextkey = false;
  key->pivotsearch = false;
  key->keysz = Min(indnkeyatts, tupnatts);
  key->scantid = key->heapkeyspace && itup ? BTreeTupleGetHeapTID(itup) : NULL;
  skey = key->scankeys;
  for (i = 0; i < indnkeyatts; i++)
  {
    FmgrInfo *procinfo;
    Datum arg;
    bool null;
    int flags;

       
                                                                         
                                 
       
    procinfo = index_getprocinfo(rel, i + 1, BTORDER_PROC);

       
                                                                     
                                                                           
                             
       
    if (i < tupnatts)
    {
      arg = index_getattr(itup, i + 1, itupdesc, &null);
    }
    else
    {
      arg = (Datum)0;
      null = true;
    }
    flags = (null ? SK_ISNULL : 0) | (indoption[i] << SK_BT_INDOPTION_SHIFT);
    ScanKeyEntryInitializeWithInfo(&skey[i], flags, (AttrNumber)(i + 1), InvalidStrategy, InvalidOid, rel->rd_indcollation[i], procinfo, arg);
                                                            
    if (null)
    {
      key->anynullkeys = true;
    }
  }

  return key;
}

   
                                                
   
void
_bt_freestack(BTStack stack)
{
  BTStack ostack;

  while (stack != NULL)
  {
    ostack = stack;
    stack = stack->bts_parent;
    pfree(ostack);
  }
}

   
                                                                      
   
                                                                           
                                                                         
                                                                               
                                                                             
                                                                             
                                           
   
                                                                          
                                                                            
                                        
   
void
_bt_preprocess_array_keys(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  int numberOfKeys = scan->numberOfKeys;
  int16 *indoption = scan->indexRelation->rd_indoption;
  int numArrayKeys;
  ScanKey cur;
  int i;
  MemoryContext oldContext;

                                                      
  numArrayKeys = 0;
  for (i = 0; i < numberOfKeys; i++)
  {
    cur = &scan->keyData[i];
    if (cur->sk_flags & SK_SEARCHARRAY)
    {
      numArrayKeys++;
      Assert(!(cur->sk_flags & (SK_ROW_HEADER | SK_SEARCHNULL | SK_SEARCHNOTNULL)));
                                                                     
      if (cur->sk_flags & SK_ISNULL)
      {
        so->numArrayKeys = -1;
        so->arrayKeyData = NULL;
        return;
      }
    }
  }

                              
  if (numArrayKeys == 0)
  {
    so->numArrayKeys = 0;
    so->arrayKeyData = NULL;
    return;
  }

     
                                                                             
                                                          
     
  if (so->arrayContext == NULL)
  {
    so->arrayContext = AllocSetContextCreate(CurrentMemoryContext, "BTree array context", ALLOCSET_SMALL_SIZES);
  }
  else
  {
    MemoryContextReset(so->arrayContext);
  }

  oldContext = MemoryContextSwitchTo(so->arrayContext);

                                                                        
  so->arrayKeyData = (ScanKey)palloc(scan->numberOfKeys * sizeof(ScanKeyData));
  memcpy(so->arrayKeyData, scan->keyData, scan->numberOfKeys * sizeof(ScanKeyData));

                                                                  
  so->arrayKeys = (BTArrayKeyInfo *)palloc0(numArrayKeys * sizeof(BTArrayKeyInfo));

                                  
  numArrayKeys = 0;
  for (i = 0; i < numberOfKeys; i++)
  {
    ArrayType *arrayval;
    int16 elmlen;
    bool elmbyval;
    char elmalign;
    int num_elems;
    Datum *elem_values;
    bool *elem_nulls;
    int num_nonnulls;
    int j;

    cur = &so->arrayKeyData[i];
    if (!(cur->sk_flags & SK_SEARCHARRAY))
    {
      continue;
    }

       
                                                                       
                                                                   
                          
       
    arrayval = DatumGetArrayTypeP(cur->sk_argument);
                                                               
    get_typlenbyvalalign(ARR_ELEMTYPE(arrayval), &elmlen, &elmbyval, &elmalign);
    deconstruct_array(arrayval, ARR_ELEMTYPE(arrayval), elmlen, elmbyval, elmalign, &elem_values, &elem_nulls, &num_elems);

       
                                                                           
                                       
       
    num_nonnulls = 0;
    for (j = 0; j < num_elems; j++)
    {
      if (!elem_nulls[j])
      {
        elem_values[num_nonnulls++] = elem_values[j];
      }
    }

                                                                  

                                                                 
    if (num_nonnulls == 0)
    {
      numArrayKeys = -1;
      break;
    }

       
                                                                       
                                                                          
                                               
       
    switch (cur->sk_strategy)
    {
    case BTLessStrategyNumber:
    case BTLessEqualStrategyNumber:
      cur->sk_argument = _bt_find_extreme_element(scan, cur, BTGreaterStrategyNumber, elem_values, num_nonnulls);
      continue;
    case BTEqualStrategyNumber:
                                     
      break;
    case BTGreaterEqualStrategyNumber:
    case BTGreaterStrategyNumber:
      cur->sk_argument = _bt_find_extreme_element(scan, cur, BTLessStrategyNumber, elem_values, num_nonnulls);
      continue;
    default:
      elog(ERROR, "unrecognized StrategyNumber: %d", (int)cur->sk_strategy);
      break;
    }

       
                                                                         
                                                                       
                                                                    
       
    num_elems = _bt_sort_array_elements(scan, cur, (indoption[cur->sk_attno - 1] & INDOPTION_DESC) != 0, elem_values, num_nonnulls);

       
                                           
       
    so->arrayKeys[numArrayKeys].scan_key = i;
    so->arrayKeys[numArrayKeys].num_elems = num_elems;
    so->arrayKeys[numArrayKeys].elem_values = elem_values;
    numArrayKeys++;
  }

  so->numArrayKeys = numArrayKeys;

  MemoryContextSwitchTo(oldContext);
}

   
                                                                     
   
                                                                          
                                                                          
                                                                  
   
static Datum
_bt_find_extreme_element(IndexScanDesc scan, ScanKey skey, StrategyNumber strat, Datum *elems, int nelems)
{
  Relation rel = scan->indexRelation;
  Oid elemtype, cmp_op;
  RegProcedure cmp_proc;
  FmgrInfo flinfo;
  Datum result;
  int i;

     
                                                                       
                                                                            
                                                                    
     
  elemtype = skey->sk_subtype;
  if (elemtype == InvalidOid)
  {
    elemtype = rel->rd_opcintype[skey->sk_attno - 1];
  }

     
                                                                  
     
                                                                  
                                                                         
                                                                           
             
     
  cmp_op = get_opfamily_member(rel->rd_opfamily[skey->sk_attno - 1], elemtype, elemtype, strat);
  if (!OidIsValid(cmp_op))
  {
    elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", strat, elemtype, elemtype, rel->rd_opfamily[skey->sk_attno - 1]);
  }
  cmp_proc = get_opcode(cmp_op);
  if (!RegProcedureIsValid(cmp_proc))
  {
    elog(ERROR, "missing oprcode for operator %u", cmp_op);
  }

  fmgr_info(cmp_proc, &flinfo);

  Assert(nelems > 0);
  result = elems[0];
  for (i = 1; i < nelems; i++)
  {
    if (DatumGetBool(FunctionCall2Coll(&flinfo, skey->sk_collation, elems[i], result)))
    {
      result = elems[i];
    }
  }

  return result;
}

   
                                                               
   
                                                                          
                                        
   
                                                                          
                                                                           
   
static int
_bt_sort_array_elements(IndexScanDesc scan, ScanKey skey, bool reverse, Datum *elems, int nelems)
{
  Relation rel = scan->indexRelation;
  Oid elemtype;
  RegProcedure cmp_proc;
  BTSortArrayContext cxt;
  int last_non_dup;
  int i;

  if (nelems <= 1)
  {
    return nelems;                    
  }

     
                                                                       
                                                                            
                                                                    
     
  elemtype = skey->sk_subtype;
  if (elemtype == InvalidOid)
  {
    elemtype = rel->rd_opcintype[skey->sk_attno - 1];
  }

     
                                                                  
     
                                                                  
                                                                         
                                                                           
          
     
  cmp_proc = get_opfamily_proc(rel->rd_opfamily[skey->sk_attno - 1], elemtype, elemtype, BTORDER_PROC);
  if (!RegProcedureIsValid(cmp_proc))
  {
    elog(ERROR, "missing support function %d(%u,%u) in opfamily %u", BTORDER_PROC, elemtype, elemtype, rel->rd_opfamily[skey->sk_attno - 1]);
  }

                               
  fmgr_info(cmp_proc, &cxt.flinfo);
  cxt.collation = skey->sk_collation;
  cxt.reverse = reverse;
  qsort_arg((void *)elems, nelems, sizeof(Datum), _bt_compare_array_elements, (void *)&cxt);

                                                          
  last_non_dup = 0;
  for (i = 1; i < nelems; i++)
  {
    int32 compare;

    compare = DatumGetInt32(FunctionCall2Coll(&cxt.flinfo, cxt.collation, elems[last_non_dup], elems[i]));
    if (compare != 0)
    {
      elems[++last_non_dup] = elems[i];
    }
  }

  return last_non_dup + 1;
}

   
                                                   
   
static int
_bt_compare_array_elements(const void *a, const void *b, void *arg)
{
  Datum da = *((const Datum *)a);
  Datum db = *((const Datum *)b);
  BTSortArrayContext *cxt = (BTSortArrayContext *)arg;
  int32 compare;

  compare = DatumGetInt32(FunctionCall2Coll(&cxt->flinfo, cxt->collation, da, db));
  if (cxt->reverse)
  {
    INVERT_COMPARE_RESULT(compare);
  }
  return compare;
}

   
                                                                      
   
                                                                            
                                                                           
   
void
_bt_start_array_keys(IndexScanDesc scan, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  int i;

  for (i = 0; i < so->numArrayKeys; i++)
  {
    BTArrayKeyInfo *curArrayKey = &so->arrayKeys[i];
    ScanKey skey = &so->arrayKeyData[curArrayKey->scan_key];

    Assert(curArrayKey->num_elems > 0);
    if (ScanDirectionIsBackward(dir))
    {
      curArrayKey->cur_elem = curArrayKey->num_elems - 1;
    }
    else
    {
      curArrayKey->cur_elem = 0;
    }
    skey->sk_argument = curArrayKey->elem_values[curArrayKey->cur_elem];
  }
}

   
                                                                     
   
                                                                             
                                                                             
   
bool
_bt_advance_array_keys(IndexScanDesc scan, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  bool found = false;
  int i;

     
                                                                    
                                                                     
                                                                            
                                         
     
  for (i = so->numArrayKeys - 1; i >= 0; i--)
  {
    BTArrayKeyInfo *curArrayKey = &so->arrayKeys[i];
    ScanKey skey = &so->arrayKeyData[curArrayKey->scan_key];
    int cur_elem = curArrayKey->cur_elem;
    int num_elems = curArrayKey->num_elems;

    if (ScanDirectionIsBackward(dir))
    {
      if (--cur_elem < 0)
      {
        cur_elem = num_elems - 1;
        found = false;                                     
      }
      else
      {
        found = true;
      }
    }
    else
    {
      if (++cur_elem >= num_elems)
      {
        cur_elem = 0;
        found = false;                                     
      }
      else
      {
        found = true;
      }
    }

    curArrayKey->cur_elem = cur_elem;
    skey->sk_argument = curArrayKey->elem_values[cur_elem];
    if (found)
    {
      break;
    }
  }

                             
  if (scan->parallel_scan != NULL)
  {
    _bt_parallel_advance_array_keys(scan);
  }

  return found;
}

   
                                                               
   
                                                                    
   
void
_bt_mark_array_keys(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  int i;

  for (i = 0; i < so->numArrayKeys; i++)
  {
    BTArrayKeyInfo *curArrayKey = &so->arrayKeys[i];

    curArrayKey->mark_elem = curArrayKey->cur_elem;
  }
}

   
                                                                   
   
                                                                    
   
void
_bt_restore_array_keys(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  bool changed = false;
  int i;

                                                                    
  for (i = 0; i < so->numArrayKeys; i++)
  {
    BTArrayKeyInfo *curArrayKey = &so->arrayKeys[i];
    ScanKey skey = &so->arrayKeyData[curArrayKey->scan_key];
    int mark_elem = curArrayKey->mark_elem;

    if (curArrayKey->cur_elem != mark_elem)
    {
      curArrayKey->cur_elem = mark_elem;
      skey->sk_argument = curArrayKey->elem_values[mark_elem];
      changed = true;
    }
  }

     
                                                                           
                                                                           
                                                       
     
  if (changed)
  {
    _bt_preprocess_keys(scan);
                                                                      
    Assert(so->qual_ok);
  }
}

   
                                                 
   
                                                                         
                                                             
                                                                         
                                                             
   
                                                                      
                                                                          
                                                                              
                                                                          
                                                     
   
                                                                          
                                                                            
                                                                        
                                                                            
                                                    
   
                                                                           
                                                                           
                                                                           
                                                                            
                                      
   
                                                                           
                                                                               
                                                                             
                                                                           
                                                                            
                                                                         
                                                                             
                                                                           
                                                                            
                                                                         
                                                                             
                                                                          
                                                                               
                                                                           
                               
   
                                                                         
                                                                        
                                                                        
                                                                         
                                                                       
                                                                       
                                                                          
                                                                        
                                                                            
                                               
   
                                                                          
                                                                           
                                                                           
                                                                             
                                                                     
                                                                             
                                                                              
                                                                             
                                                                               
                                                                              
                                                                             
                                                                     
   
                                                                       
                                                                         
                                                                         
                                                                    
                                                                           
                                                                               
   
                                                                      
                                                                 
                                                                           
                                                                             
                        
   
                                                                            
                                                                          
                                                                          
                                                                              
   
void
_bt_preprocess_keys(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  int numberOfKeys = scan->numberOfKeys;
  int16 *indoption = scan->indexRelation->rd_indoption;
  int new_numberOfKeys;
  int numberOfEqualCols;
  ScanKey inkeys;
  ScanKey outkeys;
  ScanKey cur;
  ScanKey xform[BTMaxStrategyNumber];
  bool test_result;
  int i, j;
  AttrNumber attno;

                                   
  so->qual_ok = true;
  so->numberOfKeys = 0;

  if (numberOfKeys < 1)
  {
    return;                             
  }

     
                                                                         
     
  if (so->arrayKeyData != NULL)
  {
    inkeys = so->arrayKeyData;
  }
  else
  {
    inkeys = scan->keyData;
  }

  outkeys = so->keyData;
  cur = &inkeys[0];
                                                      
  if (cur->sk_attno < 1)
  {
    elog(ERROR, "btree index keys must be ordered by attribute");
  }

                                                                     
  if (numberOfKeys == 1)
  {
                                                                
    if (!_bt_fix_scankey_strategy(cur, indoption))
    {
      so->qual_ok = false;
    }
    memcpy(outkeys, cur, sizeof(ScanKeyData));
    so->numberOfKeys = 1;
                                                                      
    if (cur->sk_attno == 1)
    {
      _bt_mark_scankey_required(outkeys);
    }
    return;
  }

     
                                            
     
  new_numberOfKeys = 0;
  numberOfEqualCols = 0;

     
                                                   
     
                                                                             
                                                               
     
  attno = 1;
  memset(xform, 0, sizeof(xform));

     
                                                                             
                                                                            
                              
     
  for (i = 0;; cur++, i++)
  {
    if (i < numberOfKeys)
    {
                                                                  
      if (!_bt_fix_scankey_strategy(cur, indoption))
      {
                                               
        so->qual_ok = false;
        return;
      }
    }

       
                                                                         
                                                
       
    if (i == numberOfKeys || cur->sk_attno != attno)
    {
      int priorNumberOfEqualCols = numberOfEqualCols;

                                                  
      if (i < numberOfKeys && cur->sk_attno < attno)
      {
        elog(ERROR, "btree index keys must be ordered by attribute");
      }

         
                                                                      
                                                                        
                                                              
         
                                                                       
                                                                         
                                                                     
                                                                        
                                                                     
         
      if (xform[BTEqualStrategyNumber - 1])
      {
        ScanKey eq = xform[BTEqualStrategyNumber - 1];

        for (j = BTMaxStrategyNumber; --j >= 0;)
        {
          ScanKey chk = xform[j];

          if (!chk || j == (BTEqualStrategyNumber - 1))
          {
            continue;
          }

          if (eq->sk_flags & SK_SEARCHNULL)
          {
                                                           
            so->qual_ok = false;
            return;
          }

          if (_bt_compare_scankey_args(scan, chk, eq, chk, &test_result))
          {
            if (!test_result)
            {
                                                      
              so->qual_ok = false;
              return;
            }
                                                             
            xform[j] = NULL;
          }
                                                                 
        }
                                                              
        numberOfEqualCols++;
      }

                                         
      if (xform[BTLessStrategyNumber - 1] && xform[BTLessEqualStrategyNumber - 1])
      {
        ScanKey lt = xform[BTLessStrategyNumber - 1];
        ScanKey le = xform[BTLessEqualStrategyNumber - 1];

        if (_bt_compare_scankey_args(scan, le, lt, le, &test_result))
        {
          if (test_result)
          {
            xform[BTLessEqualStrategyNumber - 1] = NULL;
          }
          else
          {
            xform[BTLessStrategyNumber - 1] = NULL;
          }
        }
      }

                                         
      if (xform[BTGreaterStrategyNumber - 1] && xform[BTGreaterEqualStrategyNumber - 1])
      {
        ScanKey gt = xform[BTGreaterStrategyNumber - 1];
        ScanKey ge = xform[BTGreaterEqualStrategyNumber - 1];

        if (_bt_compare_scankey_args(scan, ge, gt, ge, &test_result))
        {
          if (test_result)
          {
            xform[BTGreaterEqualStrategyNumber - 1] = NULL;
          }
          else
          {
            xform[BTGreaterStrategyNumber - 1] = NULL;
          }
        }
      }

         
                                                                     
                                                                      
                                                                      
         
      for (j = BTMaxStrategyNumber; --j >= 0;)
      {
        if (xform[j])
        {
          ScanKey outkey = &outkeys[new_numberOfKeys++];

          memcpy(outkey, xform[j], sizeof(ScanKeyData));
          if (priorNumberOfEqualCols == attno - 1)
          {
            _bt_mark_scankey_required(outkey);
          }
        }
      }

         
                                 
         
      if (i == numberOfKeys)
      {
        break;
      }

                                       
      attno = cur->sk_attno;
      memset(xform, 0, sizeof(xform));
    }

                                                           
    j = cur->sk_strategy - 1;

                                                                 
    if (cur->sk_flags & SK_ROW_HEADER)
    {
      ScanKey outkey = &outkeys[new_numberOfKeys++];

      memcpy(outkey, cur, sizeof(ScanKeyData));
      if (numberOfEqualCols == attno - 1)
      {
        _bt_mark_scankey_required(outkey);
      }

         
                                                                       
                                                 
         
      Assert(j != (BTEqualStrategyNumber - 1));
      continue;
    }

                                           
    if (xform[j] == NULL)
    {
                                          
      xform[j] = cur;
    }
    else
    {
                                                   
      if (_bt_compare_scankey_args(scan, cur, cur, xform[j], &test_result))
      {
        if (test_result)
        {
          xform[j] = cur;
        }
        else if (j == (BTEqualStrategyNumber - 1))
        {
                                                
          so->qual_ok = false;
          return;
        }
                                                       
      }
      else
      {
           
                                                                       
                                                                      
                         
           
        ScanKey outkey = &outkeys[new_numberOfKeys++];

        memcpy(outkey, cur, sizeof(ScanKeyData));
        if (numberOfEqualCols == attno - 1)
        {
          _bt_mark_scankey_required(outkey);
        }
      }
    }
  }

  so->numberOfKeys = new_numberOfKeys;
}

   
                                                          
   
                                                                         
                                                                          
                                                                      
                                                                             
                                                                             
                                                                       
   
                                                                            
                                                                          
                                                                             
                                        
   
                                                                             
                                                                          
            
   
                                                                         
                                                                           
                                                        
   
static bool
_bt_compare_scankey_args(IndexScanDesc scan, ScanKey op, ScanKey leftarg, ScanKey rightarg, bool *result)
{
  Relation rel = scan->indexRelation;
  Oid lefttype, righttype, optype, opcintype, cmp_op;
  StrategyNumber strat;

     
                                                                          
                                                                          
     
  if ((leftarg->sk_flags | rightarg->sk_flags) & SK_ISNULL)
  {
    bool leftnull, rightnull;

    if (leftarg->sk_flags & SK_ISNULL)
    {
      Assert(leftarg->sk_flags & (SK_SEARCHNULL | SK_SEARCHNOTNULL));
      leftnull = true;
    }
    else
    {
      leftnull = false;
    }
    if (rightarg->sk_flags & SK_ISNULL)
    {
      Assert(rightarg->sk_flags & (SK_SEARCHNULL | SK_SEARCHNOTNULL));
      rightnull = true;
    }
    else
    {
      rightnull = false;
    }

       
                                                                           
                                                                         
                                                                          
       
    strat = op->sk_strategy;
    if (op->sk_flags & SK_BT_NULLS_FIRST)
    {
      strat = BTCommuteStrategyNumber(strat);
    }

    switch (strat)
    {
    case BTLessStrategyNumber:
      *result = (leftnull < rightnull);
      break;
    case BTLessEqualStrategyNumber:
      *result = (leftnull <= rightnull);
      break;
    case BTEqualStrategyNumber:
      *result = (leftnull == rightnull);
      break;
    case BTGreaterEqualStrategyNumber:
      *result = (leftnull >= rightnull);
      break;
    case BTGreaterStrategyNumber:
      *result = (leftnull > rightnull);
      break;
    default:
      elog(ERROR, "unrecognized StrategyNumber: %d", (int)strat);
      *result = false;                          
      break;
    }
    return true;
  }

     
                                                                            
     
  Assert(leftarg->sk_attno == rightarg->sk_attno);

  opcintype = rel->rd_opcintype[leftarg->sk_attno - 1];

     
                                                                          
                                                                            
                                                                    
     
  lefttype = leftarg->sk_subtype;
  if (lefttype == InvalidOid)
  {
    lefttype = opcintype;
  }
  righttype = rightarg->sk_subtype;
  if (righttype == InvalidOid)
  {
    righttype = opcintype;
  }
  optype = op->sk_subtype;
  if (optype == InvalidOid)
  {
    optype = opcintype;
  }

     
                                                                            
                                                           
     
  if (lefttype == opcintype && righttype == optype)
  {
    *result = DatumGetBool(FunctionCall2Coll(&op->sk_func, op->sk_collation, leftarg->sk_argument, rightarg->sk_argument));
    return true;
  }

     
                                                                      
                                                                    
                                                                     
                 
     
                                                                            
                                                    
     
  strat = op->sk_strategy;
  if (op->sk_flags & SK_BT_DESC)
  {
    strat = BTCommuteStrategyNumber(strat);
  }

  cmp_op = get_opfamily_member(rel->rd_opfamily[leftarg->sk_attno - 1], lefttype, righttype, strat);
  if (OidIsValid(cmp_op))
  {
    RegProcedure cmp_proc = get_opcode(cmp_op);

    if (RegProcedureIsValid(cmp_proc))
    {
      *result = DatumGetBool(OidFunctionCall2Coll(cmp_proc, op->sk_collation, leftarg->sk_argument, rightarg->sk_argument));
      return true;
    }
  }

                                 
  *result = false;                                 
  return false;
}

   
                                                                           
   
                                                                     
                                                                      
                                                                        
   
                                                                            
                                          
   
                                                                        
                                                                         
                                                                          
                                                                          
   
                                                                        
                                                                        
                                                                              
                                                                              
                                                                              
                                                   
   
static bool
_bt_fix_scankey_strategy(ScanKey skey, int16 *indoption)
{
  int addflags;

  addflags = indoption[skey->sk_attno - 1] << SK_BT_INDOPTION_SHIFT;

     
                                                                           
                                                                             
                                                                            
           
     
                                                                            
                                                                   
                                                                          
                                                                             
               
     
                                                                            
                                                                         
                  
     
                                                                        
                                                                          
                                                            
     
  if (skey->sk_flags & SK_ISNULL)
  {
                                                            
    Assert(!(skey->sk_flags & SK_ROW_HEADER));

                                                                
    skey->sk_flags |= addflags;

                                                             
    if (skey->sk_flags & SK_SEARCHNULL)
    {
      skey->sk_strategy = BTEqualStrategyNumber;
      skey->sk_subtype = InvalidOid;
      skey->sk_collation = InvalidOid;
    }
    else if (skey->sk_flags & SK_SEARCHNOTNULL)
    {
      if (skey->sk_flags & SK_BT_NULLS_FIRST)
      {
        skey->sk_strategy = BTGreaterStrategyNumber;
      }
      else
      {
        skey->sk_strategy = BTLessStrategyNumber;
      }
      skey->sk_subtype = InvalidOid;
      skey->sk_collation = InvalidOid;
    }
    else
    {
                                                   
      return false;
    }

                             
    return true;
  }

                                                      
  if ((addflags & SK_BT_DESC) && !(skey->sk_flags & SK_BT_DESC))
  {
    skey->sk_strategy = BTCommuteStrategyNumber(skey->sk_strategy);
  }
  skey->sk_flags |= addflags;

                                                                           
  if (skey->sk_flags & SK_ROW_HEADER)
  {
    ScanKey subkey = (ScanKey)DatumGetPointer(skey->sk_argument);

    for (;;)
    {
      Assert(subkey->sk_flags & SK_ROW_MEMBER);
      addflags = indoption[subkey->sk_attno - 1] << SK_BT_INDOPTION_SHIFT;
      if ((addflags & SK_BT_DESC) && !(subkey->sk_flags & SK_BT_DESC))
      {
        subkey->sk_strategy = BTCommuteStrategyNumber(subkey->sk_strategy);
      }
      subkey->sk_flags |= addflags;
      if (subkey->sk_flags & SK_ROW_END)
      {
        break;
      }
      subkey++;
    }
  }

  return true;
}

   
                                                      
   
                                                                         
                                                                         
                                                                          
                                                                      
                                                                            
   
                                                                            
                                                                             
                                                                            
                                                                           
                                                            
   
static void
_bt_mark_scankey_required(ScanKey skey)
{
  int addflags;

  switch (skey->sk_strategy)
  {
  case BTLessStrategyNumber:
  case BTLessEqualStrategyNumber:
    addflags = SK_BT_REQFWD;
    break;
  case BTEqualStrategyNumber:
    addflags = SK_BT_REQFWD | SK_BT_REQBKWD;
    break;
  case BTGreaterEqualStrategyNumber:
  case BTGreaterStrategyNumber:
    addflags = SK_BT_REQBKWD;
    break;
  default:
    elog(ERROR, "unrecognized StrategyNumber: %d", (int)skey->sk_strategy);
    addflags = 0;                          
    break;
  }

  skey->sk_flags |= addflags;

  if (skey->sk_flags & SK_ROW_HEADER)
  {
    ScanKey subkey = (ScanKey)DatumGetPointer(skey->sk_argument);

                                                                   
    Assert(subkey->sk_flags & SK_ROW_MEMBER);
    Assert(subkey->sk_attno == skey->sk_attno);
    Assert(subkey->sk_strategy == skey->sk_strategy);
    subkey->sk_flags |= addflags;
  }
}

   
                                                                    
   
                                                                          
                                                                          
                                                                    
                                                         
   
                                                                         
                                                                       
                          
   
                                                                  
                              
                                                                          
                                     
                                                                       
   
bool
_bt_checkkeys(IndexScanDesc scan, IndexTuple tuple, int tupnatts, ScanDirection dir, bool *continuescan)
{
  TupleDesc tupdesc;
  BTScanOpaque so;
  int keysz;
  int ikey;
  ScanKey key;

  Assert(BTreeTupleGetNAtts(tuple, scan->indexRelation) == tupnatts);

  *continuescan = true;                         

  tupdesc = RelationGetDescr(scan->indexRelation);
  so = (BTScanOpaque)scan->opaque;
  keysz = so->numberOfKeys;

  for (key = so->keyData, ikey = 0; ikey < keysz; key++, ikey++)
  {
    Datum datum;
    bool isNull;
    Datum test;

    if (key->sk_attno > tupnatts)
    {
         
                                                                        
                                                                        
                                                                   
                                    
         
      Assert(ScanDirectionIsForward(dir));
      continue;
    }

                                                     
    if (key->sk_flags & SK_ROW_HEADER)
    {
      if (_bt_check_rowcompare(key, tuple, tupnatts, tupdesc, dir, continuescan))
      {
        continue;
      }
      return false;
    }

    datum = index_getattr(tuple, key->sk_attno, tupdesc, &isNull);

    if (key->sk_flags & SK_ISNULL)
    {
                                         
      if (key->sk_flags & SK_SEARCHNULL)
      {
        if (isNull)
        {
          continue;                                
        }
      }
      else
      {
        Assert(key->sk_flags & SK_SEARCHNOTNULL);
        if (!isNull)
        {
          continue;                                
        }
      }

         
                                                                         
                                                                     
                       
         
      if ((key->sk_flags & SK_BT_REQFWD) && ScanDirectionIsForward(dir))
      {
        *continuescan = false;
      }
      else if ((key->sk_flags & SK_BT_REQBKWD) && ScanDirectionIsBackward(dir))
      {
        *continuescan = false;
      }

         
                                                              
         
      return false;
    }

    if (isNull)
    {
      if (key->sk_flags & SK_BT_NULLS_FIRST)
      {
           
                                                                    
                                                                   
                                                                     
                                                                      
                                                                    
                                                                       
                                                                       
                                                                
           
        if ((key->sk_flags & (SK_BT_REQFWD | SK_BT_REQBKWD)) && ScanDirectionIsBackward(dir))
        {
          *continuescan = false;
        }
      }
      else
      {
           
                                                                   
                                                                   
                                                                       
                                                                      
                                                                 
                                                                       
                                                                    
                                                                  
           
        if ((key->sk_flags & (SK_BT_REQFWD | SK_BT_REQBKWD)) && ScanDirectionIsForward(dir))
        {
          *continuescan = false;
        }
      }

         
                                                              
         
      return false;
    }

    test = FunctionCall2Coll(&key->sk_func, key->sk_collation, datum, key->sk_argument);

    if (!DatumGetBool(test))
    {
         
                                                                         
                                                                     
                       
         
                                                                         
                                                                        
                                                                         
                                  
         
      if ((key->sk_flags & SK_BT_REQFWD) && ScanDirectionIsForward(dir))
      {
        *continuescan = false;
      }
      else if ((key->sk_flags & SK_BT_REQBKWD) && ScanDirectionIsBackward(dir))
      {
        *continuescan = false;
      }

         
                                                              
         
      return false;
    }
  }

                                                         
  return true;
}

   
                                                                         
   
                                                                         
                                                                         
                     
   
                                                                    
   
static bool
_bt_check_rowcompare(ScanKey skey, IndexTuple tuple, int tupnatts, TupleDesc tupdesc, ScanDirection dir, bool *continuescan)
{
  ScanKey subkey = (ScanKey)DatumGetPointer(skey->sk_argument);
  int32 cmpresult = 0;
  bool result;

                                                      
  Assert(subkey->sk_attno == skey->sk_attno);

                                              
  for (;;)
  {
    Datum datum;
    bool isNull;

    Assert(subkey->sk_flags & SK_ROW_MEMBER);

    if (subkey->sk_attno > tupnatts)
    {
         
                                                                        
                                                                        
                                                                   
                                    
         
      Assert(ScanDirectionIsForward(dir));
      cmpresult = 0;
      if (subkey->sk_flags & SK_ROW_END)
      {
        break;
      }
      subkey++;
      continue;
    }

    datum = index_getattr(tuple, subkey->sk_attno, tupdesc, &isNull);

    if (isNull)
    {
      if (subkey->sk_flags & SK_BT_NULLS_FIRST)
      {
           
                                                                    
                                                                   
                                                                     
                                                                      
                                                                    
                                                                       
                                                                       
                                                                
           
        if ((subkey->sk_flags & (SK_BT_REQFWD | SK_BT_REQBKWD)) && ScanDirectionIsBackward(dir))
        {
          *continuescan = false;
        }
      }
      else
      {
           
                                                                   
                                                                   
                                                                       
                                                                      
                                                                 
                                                                       
                                                                    
                                                                  
           
        if ((subkey->sk_flags & (SK_BT_REQFWD | SK_BT_REQBKWD)) && ScanDirectionIsForward(dir))
        {
          *continuescan = false;
        }
      }

         
                                                              
         
      return false;
    }

    if (subkey->sk_flags & SK_ISNULL)
    {
         
                                                                       
                                                                    
                                                                      
                                                                       
         
      if (subkey != (ScanKey)DatumGetPointer(skey->sk_argument))
      {
        subkey--;
      }
      if ((subkey->sk_flags & SK_BT_REQFWD) && ScanDirectionIsForward(dir))
      {
        *continuescan = false;
      }
      else if ((subkey->sk_flags & SK_BT_REQBKWD) && ScanDirectionIsBackward(dir))
      {
        *continuescan = false;
      }
      return false;
    }

                                                                     
    cmpresult = DatumGetInt32(FunctionCall2Coll(&subkey->sk_func, subkey->sk_collation, datum, subkey->sk_argument));

    if (subkey->sk_flags & SK_BT_DESC)
    {
      INVERT_COMPARE_RESULT(cmpresult);
    }

                                                                
    if (cmpresult != 0)
    {
      break;
    }

    if (subkey->sk_flags & SK_ROW_END)
    {
      break;
    }
    subkey++;
  }

     
                                                                     
                                                                       
                                   
     
  switch (subkey->sk_strategy)
  {
                                             
  case BTLessStrategyNumber:
    result = (cmpresult < 0);
    break;
  case BTLessEqualStrategyNumber:
    result = (cmpresult <= 0);
    break;
  case BTGreaterEqualStrategyNumber:
    result = (cmpresult >= 0);
    break;
  case BTGreaterStrategyNumber:
    result = (cmpresult > 0);
    break;
  default:
    elog(ERROR, "unrecognized RowCompareType: %d", (int)subkey->sk_strategy);
    result = 0;                          
    break;
  }

  if (!result)
  {
       
                                                                       
                                                                         
                                                                 
                                                                  
       
    if ((subkey->sk_flags & SK_BT_REQFWD) && ScanDirectionIsForward(dir))
    {
      *continuescan = false;
    }
    else if ((subkey->sk_flags & SK_BT_REQBKWD) && ScanDirectionIsBackward(dir))
    {
      *continuescan = false;
    }
  }

  return result;
}

   
                                                                       
                       
   
                                                                               
                                                                          
                                 
   
                                                                           
                                                                           
                                          
   
                                                                         
                                                                               
                                                                           
                                                                            
                                                      
   
                                                                             
                                                                              
                                                                            
                                                                              
                                                                             
                                                                              
                                                                       
   
                                                                              
                                                                              
                                                                           
                                                                      
   
void
_bt_killitems(IndexScanDesc scan)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  Page page;
  BTPageOpaque opaque;
  OffsetNumber minoff;
  OffsetNumber maxoff;
  int i;
  int numKilled = so->numKilled;
  bool killedsomething = false;

  Assert(BTScanPosIsValid(so->currPos));

     
                                                                           
            
     
  so->numKilled = 0;

  if (BTScanPosIsPinned(so->currPos))
  {
       
                                                                         
                                                                     
                                                                       
            
       
    LockBuffer(so->currPos.buf, BT_READ);

    page = BufferGetPage(so->currPos.buf);
  }
  else
  {
    Buffer buf;

                                                              
    buf = _bt_getbuf(scan->indexRelation, so->currPos.currPage, BT_READ);

                                                                     
    if (!BufferIsValid(buf))
    {
      return;
    }

    page = BufferGetPage(buf);
    if (BufferGetLSNAtomic(buf) == so->currPos.lsn)
    {
      so->currPos.buf = buf;
    }
    else
    {
                                                                
      _bt_relbuf(scan->indexRelation, buf);
      return;
    }
  }

  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  minoff = P_FIRSTDATAKEY(opaque);
  maxoff = PageGetMaxOffsetNumber(page);

  for (i = 0; i < numKilled; i++)
  {
    int itemIndex = so->killedItems[i];
    BTScanPosItem *kitem = &so->currPos.items[itemIndex];
    OffsetNumber offnum = kitem->indexOffset;

    Assert(itemIndex >= so->currPos.firstItem && itemIndex <= so->currPos.lastItem);
    if (offnum < minoff)
    {
      continue;                    
    }
    while (offnum <= maxoff)
    {
      ItemId iid = PageGetItemId(page, offnum);
      IndexTuple ituple = (IndexTuple)PageGetItem(page, iid);

      if (ItemPointerEquals(&ituple->t_tid, &kitem->heapTid))
      {
           
                                                                  
                                                                      
                                                                      
                                                                    
                                                                        
                                               
           
        if (!ItemIdIsDead(iid))
        {
          ItemIdMarkDead(iid);
          killedsomething = true;
        }
        break;                               
      }
      offnum = OffsetNumberNext(offnum);
    }
  }

     
                                                                   
     
                                                               
                                                          
     
  if (killedsomething)
  {
    opaque->btpo_flags |= BTP_HAS_GARBAGE;
    MarkBufferDirtyHint(so->currPos.buf, true);
  }

  LockBuffer(so->currPos.buf, BUFFER_LOCK_UNLOCK);
}

   
                                                                        
                                                                        
                                                                        
                                                                         
                                                                         
                                                                          
                                                                 
   
                                                                      
                                                                     
                                                                      
                                                                 
   

typedef struct BTOneVacInfo
{
  LockRelId relid;                                      
  BTCycleId cycleid;                                     
} BTOneVacInfo;

typedef struct BTVacInfo
{
  BTCycleId cycle_ctr;                                      
  int num_vacuums;                                             
  int max_vacuums;                                              
  BTOneVacInfo vacuums[FLEXIBLE_ARRAY_MEMBER];
} BTVacInfo;

static BTVacInfo *btvacinfo;

   
                                                                       
                                         
   
                                                                        
                                                                        
                                                                          
                                                         
   
BTCycleId
_bt_vacuum_cycleid(Relation rel)
{
  BTCycleId result = 0;
  int i;

                                                                
  LWLockAcquire(BtreeVacuumLock, LW_SHARED);

  for (i = 0; i < btvacinfo->num_vacuums; i++)
  {
    BTOneVacInfo *vac = &btvacinfo->vacuums[i];

    if (vac->relid.relId == rel->rd_lockInfo.lockRelId.relId && vac->relid.dbId == rel->rd_lockInfo.lockRelId.dbId)
    {
      result = vac->cycleid;
      break;
    }
  }

  LWLockRelease(BtreeVacuumLock);
  return result;
}

   
                                                                              
   
                                                                
                                                                         
                                                                           
                             
                                                                           
   
BTCycleId
_bt_start_vacuum(Relation rel)
{
  BTCycleId result;
  int i;
  BTOneVacInfo *vac;

  LWLockAcquire(BtreeVacuumLock, LW_EXCLUSIVE);

     
                                                                          
                           
     
  result = ++(btvacinfo->cycle_ctr);
  if (result == 0 || result > MAX_BT_CYCLE_ID)
  {
    result = btvacinfo->cycle_ctr = 1;
  }

                                                                    
  for (i = 0; i < btvacinfo->num_vacuums; i++)
  {
    vac = &btvacinfo->vacuums[i];
    if (vac->relid.relId == rel->rd_lockInfo.lockRelId.relId && vac->relid.dbId == rel->rd_lockInfo.lockRelId.dbId)
    {
         
                                                                  
                                                                       
                                                                    
                                                   
         
      LWLockRelease(BtreeVacuumLock);
      elog(ERROR, "multiple active vacuums for index \"%s\"", RelationGetRelationName(rel));
    }
  }

                        
  if (btvacinfo->num_vacuums >= btvacinfo->max_vacuums)
  {
    LWLockRelease(BtreeVacuumLock);
    elog(ERROR, "out of btvacinfo slots");
  }
  vac = &btvacinfo->vacuums[btvacinfo->num_vacuums];
  vac->relid = rel->rd_lockInfo.lockRelId;
  vac->cycleid = result;
  btvacinfo->num_vacuums++;

  LWLockRelease(BtreeVacuumLock);
  return result;
}

   
                                                            
   
                                                                          
                                                                           
   
void
_bt_end_vacuum(Relation rel)
{
  int i;

  LWLockAcquire(BtreeVacuumLock, LW_EXCLUSIVE);

                            
  for (i = 0; i < btvacinfo->num_vacuums; i++)
  {
    BTOneVacInfo *vac = &btvacinfo->vacuums[i];

    if (vac->relid.relId == rel->rd_lockInfo.lockRelId.relId && vac->relid.dbId == rel->rd_lockInfo.lockRelId.dbId)
    {
                                                     
      *vac = btvacinfo->vacuums[btvacinfo->num_vacuums - 1];
      btvacinfo->num_vacuums--;
      break;
    }
  }

  LWLockRelease(BtreeVacuumLock);
}

   
                                                                
   
void
_bt_end_vacuum_callback(int code, Datum arg)
{
  _bt_end_vacuum((Relation)DatumGetPointer(arg));
}

   
                                                                  
   
Size
BTreeShmemSize(void)
{
  Size size;

  size = offsetof(BTVacInfo, vacuums);
  size = add_size(size, mul_size(MaxBackends, sizeof(BTOneVacInfo)));
  return size;
}

   
                                                             
   
void
BTreeShmemInit(void)
{
  bool found;

  btvacinfo = (BTVacInfo *)ShmemInitStruct("BTree Vacuum State", BTreeShmemSize(), &found);

  if (!IsUnderPostmaster)
  {
                                       
    Assert(!found);

       
                                                                      
                                                                     
                                         
       
    btvacinfo->cycle_ctr = (BTCycleId)time(NULL);

    btvacinfo->num_vacuums = 0;
    btvacinfo->max_vacuums = MaxBackends;
  }
  else
  {
    Assert(found);
  }
}

bytea *
btoptions(Datum reloptions, bool validate)
{
  return default_reloptions(reloptions, validate, RELOPT_KIND_BTREE);
}

   
                                                        
   
                                                                               
                        
   
bool
btproperty(Oid index_oid, int attno, IndexAMProperty prop, const char *propname, bool *res, bool *isnull)
{
  switch (prop)
  {
  case AMPROP_RETURNABLE:
                                                        
    if (attno == 0)
    {
      return false;
    }
                                                 
    *res = true;
    return true;

  default:
    return false;                           
  }
}

   
                                                           
   
char *
btbuildphasename(int64 phasenum)
{
  switch (phasenum)
  {
  case PROGRESS_CREATEIDX_SUBPHASE_INITIALIZE:
    return "initializing";
  case PROGRESS_BTREE_PHASE_INDEXBUILD_TABLESCAN:
    return "scanning table";
  case PROGRESS_BTREE_PHASE_PERFORMSORT_1:
    return "sorting live tuples";
  case PROGRESS_BTREE_PHASE_PERFORMSORT_2:
    return "sorting dead tuples";
  case PROGRESS_BTREE_PHASE_LEAF_LOAD:
    return "loading tuples in tree";
  default:
    return NULL;
  }
}

   
                                                                      
   
                                                                             
                                                                            
                                                                           
                                                                    
                                                                              
                                                                            
                                                                          
                                                                           
                                            
   
                                                                           
                                                                              
                                                                           
                                                                           
                                       
   
                                                                              
                                                                            
                                                                            
                                                                       
                  
   
                                                                            
                                                                              
                                                                          
                                                                               
                                                                       
                                                                             
                                      
   
IndexTuple
_bt_truncate(Relation rel, IndexTuple lastleft, IndexTuple firstright, BTScanInsert itup_key)
{
  TupleDesc itupdesc = RelationGetDescr(rel);
  int16 natts = IndexRelationGetNumberOfAttributes(rel);
  int16 nkeyatts = IndexRelationGetNumberOfKeyAttributes(rel);
  int keepnatts;
  IndexTuple pivot;
  ItemPointer pivotheaptid;
  Size newsize;

     
                                                                         
                             
     
  Assert(BTreeTupleGetNAtts(lastleft, rel) == natts);
  Assert(BTreeTupleGetNAtts(firstright, rel) == natts);

                                                                     
  keepnatts = _bt_keep_natts(rel, lastleft, firstright, itup_key);

#ifdef DEBUG_NO_TRUNCATE
                                                               
  keepnatts = nkeyatts + 1;
#endif

  if (keepnatts <= natts)
  {
    IndexTuple tidpivot;

    pivot = index_truncate_tuple(itupdesc, firstright, Min(keepnatts, nkeyatts));

       
                                                                          
                                                              
       
    if (keepnatts <= nkeyatts)
    {
      BTreeTupleSetNAtts(pivot, keepnatts);
      return pivot;
    }

       
                                                                     
                                                                   
                                         
       
    Assert(natts != nkeyatts);
    newsize = IndexTupleSize(pivot) + MAXALIGN(sizeof(ItemPointerData));
    tidpivot = palloc0(newsize);
    memcpy(tidpivot, pivot, IndexTupleSize(pivot));
                                 
    pfree(pivot);
    pivot = tidpivot;
  }
  else
  {
       
                                                                       
                                                                          
       
                                                                     
                                                                        
                        
       
    Assert(natts == nkeyatts);
    newsize = IndexTupleSize(firstright) + MAXALIGN(sizeof(ItemPointerData));
    pivot = palloc0(newsize);
    memcpy(pivot, firstright, IndexTupleSize(firstright));
  }

     
                                                                             
                                                                          
                                                                          
                                                                          
                                          
     
                                                                          
                                                                           
                                                                           
                                                    
     
  Assert(itup_key->heapkeyspace);
  pivot->t_info &= ~INDEX_SIZE_MASK;
  pivot->t_info |= newsize;

     
                                                                            
                                                                           
                                                                             
                                                                       
                      
     
  pivotheaptid = (ItemPointer)((char *)pivot + newsize - sizeof(ItemPointerData));
  ItemPointerCopy(&lastleft->t_tid, pivotheaptid);

     
                                                                             
                                                                             
                                                                             
                                                                           
                                                                  
                 
     
#ifndef DEBUG_NO_TRUNCATE
  Assert(ItemPointerCompare(&lastleft->t_tid, &firstright->t_tid) < 0);
  Assert(ItemPointerCompare(pivotheaptid, &lastleft->t_tid) >= 0);
  Assert(ItemPointerCompare(pivotheaptid, &firstright->t_tid) < 0);
#else

     
                                                                          
                                                                         
                                                                       
                                                                           
                                                                          
                                                                         
                                                                           
                                                      
     
  ItemPointerCopy(&firstright->t_tid, pivotheaptid);

     
                                                                          
                                                                            
                                   
     
  ItemPointerSetOffsetNumber(pivotheaptid, OffsetNumberPrev(ItemPointerGetOffsetNumber(pivotheaptid)));
  Assert(ItemPointerCompare(pivotheaptid, &firstright->t_tid) < 0);
#endif

  BTreeTupleSetNAtts(pivot, nkeyatts);
  BTreeTupleSetAltHeapTID(pivot);

  return pivot;
}

   
                                                                     
   
                                                                              
                                                                            
                        
   
                                                                       
                                                                             
                                                                    
   
static int
_bt_keep_natts(Relation rel, IndexTuple lastleft, IndexTuple firstright, BTScanInsert itup_key)
{
  int nkeyatts = IndexRelationGetNumberOfKeyAttributes(rel);
  TupleDesc itupdesc = RelationGetDescr(rel);
  int keepnatts;
  ScanKey scankey;

     
                                                                        
                                                                    
                                                                         
                                                                      
                                            
     
  if (!itup_key->heapkeyspace)
  {
    Assert(nkeyatts != IndexRelationGetNumberOfAttributes(rel));
    return nkeyatts;
  }

  scankey = itup_key->scankeys;
  keepnatts = 1;
  for (int attnum = 1; attnum <= nkeyatts; attnum++, scankey++)
  {
    Datum datum1, datum2;
    bool isNull1, isNull2;

    datum1 = index_getattr(lastleft, attnum, itupdesc, &isNull1);
    datum2 = index_getattr(firstright, attnum, itupdesc, &isNull2);

    if (isNull1 != isNull2)
    {
      break;
    }

    if (!isNull1 && DatumGetInt32(FunctionCall2Coll(&scankey->sk_func, scankey->sk_collation, datum1, datum2)) != 0)
    {
      break;
    }

    keepnatts++;
  }

  return keepnatts;
}

   
                                                                 
   
                                                                           
                                                                          
                                                                             
                
   
                                                                              
                                                                           
                                                                            
                                                                         
                                                                               
                                               
   
                                                                              
                                                                             
                                                                         
                                                                          
                                                      
   
int
_bt_keep_natts_fast(Relation rel, IndexTuple lastleft, IndexTuple firstright)
{
  TupleDesc itupdesc = RelationGetDescr(rel);
  int keysz = IndexRelationGetNumberOfKeyAttributes(rel);
  int keepnatts;

  keepnatts = 1;
  for (int attnum = 1; attnum <= keysz; attnum++)
  {
    Datum datum1, datum2;
    bool isNull1, isNull2;
    Form_pg_attribute att;

    datum1 = index_getattr(lastleft, attnum, itupdesc, &isNull1);
    datum2 = index_getattr(firstright, attnum, itupdesc, &isNull2);
    att = TupleDescAttr(itupdesc, attnum - 1);

    if (isNull1 != isNull2)
    {
      break;
    }

    if (!isNull1 && !datumIsEqual(datum1, datum2, att->attbyval, att->attlen))
    {
      break;
    }

    keepnatts++;
  }

  return keepnatts;
}

   
                                                                         
   
                                                                            
                                                                           
                 
   
                                                                          
                                                                             
                                                                           
                                                                              
                                                                              
                                              
   
bool
_bt_check_natts(Relation rel, bool heapkeyspace, Page page, OffsetNumber offnum)
{
  int16 natts = IndexRelationGetNumberOfAttributes(rel);
  int16 nkeyatts = IndexRelationGetNumberOfKeyAttributes(rel);
  BTPageOpaque opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  IndexTuple itup;
  int tupnatts;

     
                                                                             
                     
     
  if (P_IGNORE(opaque))
  {
    return true;
  }

  Assert(offnum >= FirstOffsetNumber && offnum <= PageGetMaxOffsetNumber(page));

     
                                                                          
                                                 
     
  StaticAssertStmt(BT_N_KEYS_OFFSET_MASK >= INDEX_MAX_KEYS, "BT_N_KEYS_OFFSET_MASK can't fit INDEX_MAX_KEYS");

  itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offnum));
  tupnatts = BTreeTupleGetNAtts(itup, rel);

  if (P_ISLEAF(opaque))
  {
    if (offnum >= P_FIRSTDATAKEY(opaque))
    {
         
                                                                   
                                                                  
         
      if ((itup->t_info & INDEX_ALT_TID_MASK) != 0)
      {
        return false;
      }

         
                                                                       
                                                                        
                                                               
                          
         
      return tupnatts == natts;
    }
    else
    {
         
                                                                      
                                              
         
      Assert(!P_RIGHTMOST(opaque));

         
                                                                         
                                                                     
                                                                       
         
      if (!heapkeyspace)
      {
        return tupnatts == nkeyatts;
      }

                                                         
    }
  }
  else                        
  {
    if (offnum == P_FIRSTDATAKEY(opaque))
    {
         
                                                                        
                                                                 
                                                                        
                                               
         
      if (heapkeyspace)
      {
        return tupnatts == 0;
      }

         
                                                                         
                                                                        
                                                                        
                                                                    
                                                                       
                                                        
         
                                                                         
                                                         
         
      return tupnatts == 0 || ((itup->t_info & INDEX_ALT_TID_MASK) == 0 && ItemPointerGetOffsetNumber(&(itup->t_tid)) == P_HIKEY);
    }
    else
    {
         
                                                                       
                                                                 
                                                                        
                                  
         
      if (!heapkeyspace)
      {
        return tupnatts == nkeyatts;
      }

                                                         
    }
  }

                                                                         
  Assert(heapkeyspace);

     
                                                                           
                                                                             
                         
     
  if ((itup->t_info & INDEX_ALT_TID_MASK) == 0)
  {
    return false;
  }

     
                                                                         
                                               
     
  if (BTreeTupleGetHeapTID(itup) != NULL && tupnatts != nkeyatts)
  {
    return false;
  }

     
                                                                         
                                                                            
                                                                      
                                                 
     
  return tupnatts > 0 && tupnatts <= nkeyatts;
}

   
   
                                                                               
   
                                                                             
                                                                              
                                    
   
                                                                              
                                                                               
                                   
   
void
_bt_check_third_page(Relation rel, Relation heap, bool needheaptidspace, Page page, IndexTuple newtup)
{
  Size itemsz;
  BTPageOpaque opaque;

  itemsz = MAXALIGN(IndexTupleSize(newtup));

                                            
  if (itemsz <= BTMaxItemSize(page))
  {
    return;
  }

     
                                                                            
                                                                             
                                                 
     
  if (!needheaptidspace && itemsz <= BTMaxItemSizeNoHeapTid(page))
  {
    return;
  }

     
                                                                             
                                                                    
     
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  if (!P_ISLEAF(opaque))
  {
    elog(ERROR, "cannot insert oversized tuple of size %zu on internal page of index \"%s\"", itemsz, RelationGetRelationName(rel));
  }

  ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds btree version %u maximum %zu for index \"%s\"", itemsz, needheaptidspace ? BTREE_VERSION : BTREE_NOVAC_VERSION, needheaptidspace ? BTMaxItemSize(page) : BTMaxItemSizeNoHeapTid(page), RelationGetRelationName(rel)), errdetail("Index row references tuple (%u,%u) in relation \"%s\".", ItemPointerGetBlockNumber(&newtup->t_tid), ItemPointerGetOffsetNumber(&newtup->t_tid), RelationGetRelationName(heap)),
                     errhint("Values larger than 1/3 of a buffer page cannot be indexed.\n"
                             "Consider a function index of an MD5 hash of the value, "
                             "or use full text indexing."),
                     errtableconstraint(heap, RelationGetRelationName(rel))));
}
