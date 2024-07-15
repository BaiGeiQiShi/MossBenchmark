                                                                            
   
               
                                      
   
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/nbtree.h"
#include "access/relscan.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/predicate.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

static void
_bt_drop_lock_and_maybe_pin(IndexScanDesc scan, BTScanPos sp);
static OffsetNumber
_bt_binsrch(Relation rel, BTScanInsert key, Buffer buf);
static bool
_bt_readpage(IndexScanDesc scan, ScanDirection dir, OffsetNumber offnum);
static void
_bt_saveitem(BTScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup);
static bool
_bt_steppage(IndexScanDesc scan, ScanDirection dir);
static bool
_bt_readnextpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir);
static bool
_bt_parallel_readpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir);
static Buffer
_bt_walk_left(Relation rel, Buffer buf, Snapshot snapshot);
static bool
_bt_endpoint(IndexScanDesc scan, ScanDirection dir);
static inline void
_bt_initialize_more_data(BTScanOpaque so, ScanDirection dir);

   
                                 
   
                                                                              
                                                                              
                                                                              
                                                                            
   
                                                                             
                                                                        
                                                                             
                                                                          
                
   
static void
_bt_drop_lock_and_maybe_pin(IndexScanDesc scan, BTScanPos sp)
{
  LockBuffer(sp->buf, BUFFER_LOCK_UNLOCK);

  if (IsMVCCSnapshot(scan->xs_snapshot) && RelationNeedsWAL(scan->indexRelation) && !scan->xs_want_itup)
  {
    ReleaseBuffer(sp->buf);
    sp->buf = InvalidBuffer;
  }
}

   
                                                             
                                                              
   
                                                                        
                                                         
   
                                                                         
                                                                     
                                                   
   
                                                                            
                                                                       
                                                                         
   
                                                                               
                                                                               
                                                                           
                                                                             
                                       
   
BTStack
_bt_search(Relation rel, BTScanInsert key, Buffer *bufP, int access, Snapshot snapshot)
{
  BTStack stack_in = NULL;
  int page_access = BT_READ;

                                       
  *bufP = _bt_getroot(rel, access);

                                                                        
  if (!BufferIsValid(*bufP))
  {
    return (BTStack)NULL;
  }

                                                          
  for (;;)
  {
    Page page;
    BTPageOpaque opaque;
    OffsetNumber offnum;
    ItemId itemid;
    IndexTuple itup;
    BlockNumber blkno;
    BlockNumber par_blkno;
    BTStack new_stack;

       
                                                                         
                                                                       
                                                
       
                                                                          
                                                                      
                                                                          
                                                                           
                                                                         
                                                                          
       
    *bufP = _bt_moveright(rel, key, *bufP, (access == BT_WRITE), stack_in, page_access, snapshot);

                                            
    page = BufferGetPage(*bufP);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    if (P_ISLEAF(opaque))
    {
      break;
    }

       
                                                                         
                               
       
    offnum = _bt_binsrch(rel, key, *bufP);
    itemid = PageGetItemId(page, offnum);
    itup = (IndexTuple)PageGetItem(page, itemid);
    blkno = BTreeInnerTupleGetDownLink(itup);
    par_blkno = BufferGetBlockNumber(*bufP);

       
                                                                       
                                                                        
                                                                          
                                                                         
                                                                         
                                                                           
                                                                           
                                                                       
                                                                          
                                                                           
                                                                      
               
       
    new_stack = (BTStack)palloc(sizeof(BTStackData));
    new_stack->bts_blkno = par_blkno;
    new_stack->bts_offset = offnum;
    new_stack->bts_btentry = blkno;
    new_stack->bts_parent = stack_in;

       
                                                                           
                                                                       
                                                                     
       
    if (opaque->btpo.level == 1 && access == BT_WRITE)
    {
      page_access = BT_WRITE;
    }

                                                                         
    *bufP = _bt_relandgetbuf(rel, *bufP, blkno, page_access);

                                            
    stack_in = new_stack;
  }

     
                                                                           
                                                                             
                                                          
     
  if (access == BT_WRITE && page_access == BT_READ)
  {
                                                 
    LockBuffer(*bufP, BUFFER_LOCK_UNLOCK);
    LockBuffer(*bufP, BT_WRITE);

       
                                                                           
                                                                         
                                                                        
                                                                  
                                           
       
    *bufP = _bt_moveright(rel, key, *bufP, true, stack_in, BT_WRITE, snapshot);
  }

  return stack_in;
}

   
                                                            
   
                                                                 
                                                                  
                                                                   
                                                                   
                                   
   
                                                                    
                                                                       
                                                            
                                                                      
                  
   
                                                                             
                              
   
                                                                            
                                                                             
                              
   
                                                                         
                                                                          
                                                                          
                                                             
   
                                                                           
                                                                           
                                                                          
   
                                                                            
                                                                       
                                                                         
   
Buffer
_bt_moveright(Relation rel, BTScanInsert key, Buffer buf, bool forupdate, BTStack stack, int access, Snapshot snapshot)
{
  Page page;
  BTPageOpaque opaque;
  int32 cmpval;

     
                                                                            
                                                                             
                                                                           
                                                                       
                                                                    
                                                 
     
                                                                            
                                                         
     
                                                                      
             
     
                                                                           
                
     
  cmpval = key->nextkey ? 0 : 1;

  for (;;)
  {
    page = BufferGetPage(buf);
    TestForOldSnapshot(snapshot, rel, page);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);

    if (P_RIGHTMOST(opaque))
    {
      break;
    }

       
                                                                
       
    if (forupdate && P_INCOMPLETE_SPLIT(opaque))
    {
      BlockNumber blkno = BufferGetBlockNumber(buf);

                                         
      if (access == BT_READ)
      {
        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
        LockBuffer(buf, BT_WRITE);
      }

      if (P_INCOMPLETE_SPLIT(opaque))
      {
        _bt_finish_split(rel, buf, stack);
      }
      else
      {
        _bt_relbuf(rel, buf);
      }

                                                               
      buf = _bt_getbuf(rel, blkno, access);
      continue;
    }

    if (P_IGNORE(opaque) || _bt_compare(rel, key, page, P_HIKEY) >= cmpval)
    {
                               
      buf = _bt_relandgetbuf(rel, buf, opaque->btpo_next, access);
      continue;
    }
    else
    {
      break;
    }
  }

  if (P_IGNORE(opaque))
  {
    elog(ERROR, "fell off the end of index \"%s\"", RelationGetRelationName(rel));
  }

  return buf;
}

   
                                                                       
   
                                                                       
                                                                     
                                                                              
                                                                          
   
                                                                          
                                                                            
                                                                            
                                                                           
                                                                          
                                                                            
                                                        
   
                                                                         
                                                                       
                  
   
static OffsetNumber
_bt_binsrch(Relation rel, BTScanInsert key, Buffer buf)
{
  Page page;
  BTPageOpaque opaque;
  OffsetNumber low, high;
  int32 result, cmpval;

                                                                          
  Assert(!key->nextkey || key->scantid == NULL);

  page = BufferGetPage(buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  low = P_FIRSTDATAKEY(opaque);
  high = PageGetMaxOffsetNumber(page);

     
                                                                             
                                                                      
                                                                             
                                                                        
                                                        
     
  if (unlikely(high < low))
  {
    return low;
  }

     
                                                                           
                                         
     
                                                                           
                                                                         
     
                                                                          
                                                                         
     
                                       
     
  high++;                                            

  cmpval = key->nextkey ? 0 : 1;                              

  while (high > low)
  {
    OffsetNumber mid = low + ((high - low) / 2);

                                                                 

    result = _bt_compare(rel, key, page, mid);

    if (result >= cmpval)
    {
      low = mid + 1;
    }
    else
    {
      high = mid;
    }
  }

     
                                                                         
                                     
     
                                                                         
                                                  
     
  if (P_ISLEAF(opaque))
  {
    return low;
  }

     
                                                                             
                                                                 
     
  Assert(low > P_FIRSTDATAKEY(opaque));

  return OffsetNumberPrev(low);
}

   
   
                                                                           
   
                                                                      
                                                                          
                                                                        
                                              
   
                                                                         
                                                                            
                                                                          
                                                                         
                                                                         
                              
   
                                                                           
                                      
   
OffsetNumber
_bt_binsrch_insert(Relation rel, BTInsertState insertstate)
{
  BTScanInsert key = insertstate->itup_key;
  Page page;
  BTPageOpaque opaque;
  OffsetNumber low, high, stricthigh;
  int32 result, cmpval;

  page = BufferGetPage(insertstate->buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  Assert(P_ISLEAF(opaque));
  Assert(!key->nextkey);

  if (!insertstate->bounds_valid)
  {
                                 
    low = P_FIRSTDATAKEY(opaque);
    high = PageGetMaxOffsetNumber(page);
  }
  else
  {
                                                                    
    low = insertstate->low;
    high = insertstate->stricthigh;
  }

                                                                         
  if (unlikely(high < low))
  {
                                   
    insertstate->low = InvalidOffsetNumber;
    insertstate->stricthigh = InvalidOffsetNumber;
    insertstate->bounds_valid = false;
    return low;
  }

     
                                                                           
                                      
     
                                                                             
                                                                             
                                                             
     
                                       
     
  if (!insertstate->bounds_valid)
  {
    high++;                                            
  }
  stricthigh = high;                                     

  cmpval = 1;                                

  while (high > low)
  {
    OffsetNumber mid = low + ((high - low) / 2);

                                                                 

    result = _bt_compare(rel, key, page, mid);

    if (result >= cmpval)
    {
      low = mid + 1;
    }
    else
    {
      high = mid;
      if (result != 0)
      {
        stricthigh = high;
      }
    }
  }

     
                                                                          
                                                                             
                                               
     
                                                                          
                                                                        
                                                                          
                    
     
  insertstate->low = low;
  insertstate->stricthigh = stricthigh;
  insertstate->bounds_valid = true;

  return low;
}

             
                                                                       
   
                                                          
   
                          
                                      
                                       
                                      
                                                                 
                                                                 
                                              
   
                                                                         
                                                                        
                                                                      
                                                                       
                                                                       
                                                                       
                                                       
             
   
int32
_bt_compare(Relation rel, BTScanInsert key, Page page, OffsetNumber offnum)
{
  TupleDesc itupdesc = RelationGetDescr(rel);
  BTPageOpaque opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  IndexTuple itup;
  ItemPointer heapTid;
  ScanKey scankey;
  int ncmpkey;
  int ntupatts;

  Assert(_bt_check_natts(rel, key->heapkeyspace, page, offnum));
  Assert(key->keysz <= IndexRelationGetNumberOfKeyAttributes(rel));
  Assert(key->heapkeyspace || key->scantid == NULL);

     
                                                                            
                         
     
  if (!P_ISLEAF(opaque) && offnum == P_FIRSTDATAKEY(opaque))
  {
    return 1;
  }

  itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offnum));
  ntupatts = BTreeTupleGetNAtts(itup, rel);

     
                                                                           
                                                                            
                                                                             
                                                                         
              
     
                                                                       
                                                                           
                 
     

  ncmpkey = Min(ntupatts, key->keysz);
  Assert(key->heapkeyspace || ncmpkey == key->keysz);
  scankey = key->scankeys;
  for (int i = 1; i <= ncmpkey; i++)
  {
    Datum datum;
    bool isNull;
    int32 result;

    datum = index_getattr(itup, scankey->sk_attno, itupdesc, &isNull);

                                                      
    if (scankey->sk_flags & SK_ISNULL)                  
    {
      if (isNull)
      {
        result = 0;                    
      }
      else if (scankey->sk_flags & SK_BT_NULLS_FIRST)
      {
        result = -1;                        
      }
      else
      {
        result = 1;                        
      }
    }
    else if (isNull)                                       
    {
      if (scankey->sk_flags & SK_BT_NULLS_FIRST)
      {
        result = 1;                        
      }
      else
      {
        result = -1;                        
      }
    }
    else
    {
         
                                                                        
                                                                  
                                                                 
                                                                         
                                                                         
                                                          
         
      result = DatumGetInt32(FunctionCall2Coll(&scankey->sk_func, scankey->sk_collation, datum, scankey->sk_argument));

      if (!(scankey->sk_flags & SK_BT_DESC))
      {
        INVERT_COMPARE_RESULT(result);
      }
    }

                                                        
    if (result != 0)
    {
      return result;
    }

    scankey++;
  }

     
                                                                         
                                                                             
                                                                    
     
                                                                      
                                                                     
                
     
  if (key->keysz > ntupatts)
  {
    return 1;
  }

     
                                                                          
                                                               
                             
     
  heapTid = BTreeTupleGetHeapTID(itup);
  if (key->scantid == NULL)
  {
       
                                                                      
                                                                          
                                                                        
                           
       
                                                                       
                                                                           
                                                                       
                                                                          
                                                                         
                                                                          
                                                                      
                                                                           
                                          
       
                                                                           
                                                                         
                                                                       
                                                                    
                                       
       
                                                                         
                                                                
                   
       
                                                                           
                                                                         
                                                                         
                                                                   
                                                                         
                              
       
    if (key->heapkeyspace && !key->pivotsearch && key->keysz == ntupatts && heapTid == NULL)
    {
      return 1;
    }

                                                          
    return 0;
  }

     
                                                                         
                                                                         
     
  Assert(key->keysz == IndexRelationGetNumberOfKeyAttributes(rel));
  if (heapTid == NULL)
  {
    return 1;
  }

  Assert(ntupatts >= IndexRelationGetNumberOfKeyAttributes(rel));
  return ItemPointerCompare(key->scantid, heapTid);
}

   
                                                 
   
                                                                 
                                                                    
                                                                     
                                                                          
                                                                     
                                                                        
                                                                      
                                                                         
   
                                                                         
                       
   
                                                                           
                                                                          
                                                                           
                                        
   
bool
_bt_first(IndexScanDesc scan, ScanDirection dir)
{
  Relation rel = scan->indexRelation;
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  Buffer buf;
  BTStack stack;
  OffsetNumber offnum;
  StrategyNumber strat;
  bool nextkey;
  bool goback;
  BTScanInsertData inskey;
  ScanKey startKeys[INDEX_MAX_KEYS];
  ScanKeyData notnullkeys[INDEX_MAX_KEYS];
  int keysCount = 0;
  int i;
  bool status = true;
  StrategyNumber strat_total;
  BTScanPosItem *currItem;
  BlockNumber blkno;

  Assert(!BTScanPosIsValid(so->currPos));

  pgstat_count_index_scan(rel);

     
                                                                           
                                                     
     
  _bt_preprocess_keys(scan);

     
                                                                         
                                                
     
  if (!so->qual_ok)
  {
                                                                      
    _bt_parallel_done(scan);
    return false;
  }

     
                                                                         
                                                                            
                                                                           
                                                                       
     
  if (scan->parallel_scan != NULL)
  {
    status = _bt_parallel_seize(scan, &blkno);
    if (!status)
    {
      return false;
    }
    else if (blkno == P_NONE)
    {
      _bt_parallel_done(scan);
      return false;
    }
    else if (blkno != InvalidBlockNumber)
    {
      if (!_bt_parallel_readpage(scan, blkno, dir))
      {
        return false;
      }
      goto readcomplete;
    }
  }

               
                                                                        
     
                                                                           
                                                                        
                                                                           
                                                                             
                                                                           
                                                                         
                                                                           
                                                  
     
                                                                          
                                                                        
                                                                        
                                                                      
                                                                     
                                                                          
                                                                      
     
                                                                          
                                                                        
                                                                     
                                                                           
                                                                           
                                                                            
                                                                            
                                                                
                    
     
                                                                       
                                                                         
                                                                           
                                                                             
                                                                            
                                                                           
                                                                
     
                                                                             
                                                                            
                                    
     
                                                                             
                                                               
               
     
  strat_total = BTEqualStrategyNumber;
  if (so->numberOfKeys > 0)
  {
    AttrNumber curattr;
    ScanKey chosen;
    ScanKey impliesNN;
    ScanKey cur;

       
                                                                          
                                                                       
                       
       
    curattr = 1;
    chosen = NULL;
                                                                      
    impliesNN = NULL;

       
                                                                       
                                                                       
                                                       
       
    for (cur = so->keyData, i = 0;; cur++, i++)
    {
      if (i >= so->numberOfKeys || cur->sk_attno != curattr)
      {
           
                                                                  
                                                                     
           
        if (chosen == NULL && impliesNN != NULL && ((impliesNN->sk_flags & SK_BT_NULLS_FIRST) ? ScanDirectionIsForward(dir) : ScanDirectionIsBackward(dir)))
        {
                                                               
          chosen = &notnullkeys[keysCount];
          ScanKeyEntryInitialize(chosen, (SK_SEARCHNOTNULL | SK_ISNULL | (impliesNN->sk_flags & (SK_BT_DESC | SK_BT_NULLS_FIRST))), curattr, ((impliesNN->sk_flags & SK_BT_NULLS_FIRST) ? BTGreaterStrategyNumber : BTLessStrategyNumber), InvalidOid, InvalidOid, InvalidOid, (Datum)0);
        }

           
                                                                     
                                                       
           
        if (chosen == NULL)
        {
          break;
        }
        startKeys[keysCount++] = chosen;

           
                                                                   
                
           
        strat = chosen->sk_strategy;
        if (strat != BTEqualStrategyNumber)
        {
          strat_total = strat;
          if (strat == BTGreaterStrategyNumber || strat == BTLessStrategyNumber)
          {
            break;
          }
        }

           
                                                                      
                                                                      
                            
           
        if (i >= so->numberOfKeys || cur->sk_attno != curattr + 1)
        {
          break;
        }

           
                                
           
        curattr = cur->sk_attno;
        chosen = NULL;
        impliesNN = NULL;
      }

         
                                                                   
         
                                                                
                                                                       
                                                                 
         
      switch (cur->sk_strategy)
      {
      case BTLessStrategyNumber:
      case BTLessEqualStrategyNumber:
        if (chosen == NULL)
        {
          if (ScanDirectionIsBackward(dir))
          {
            chosen = cur;
          }
          else
          {
            impliesNN = cur;
          }
        }
        break;
      case BTEqualStrategyNumber:
                                              
        chosen = cur;
        break;
      case BTGreaterEqualStrategyNumber:
      case BTGreaterStrategyNumber:
        if (chosen == NULL)
        {
          if (ScanDirectionIsForward(dir))
          {
            chosen = cur;
          }
          else
          {
            impliesNN = cur;
          }
        }
        break;
      }
    }
  }

     
                                                                           
                                                                            
            
     
  if (keysCount == 0)
  {
    bool match;

    match = _bt_endpoint(scan, dir);

    if (!match)
    {
                                                      
      _bt_parallel_done(scan);
    }

    return match;
  }

     
                                                                      
                                                                      
                                                                      
                                                                         
                                                                   
     
  Assert(keysCount <= INDEX_MAX_KEYS);
  for (i = 0; i < keysCount; i++)
  {
    ScanKey cur = startKeys[i];

    Assert(cur->sk_attno == i + 1);

    if (cur->sk_flags & SK_ROW_HEADER)
    {
         
                                                                      
         
                                                                       
                                                                         
                                                                       
                                                                       
                               
         
      ScanKey subkey = (ScanKey)DatumGetPointer(cur->sk_argument);

      Assert(subkey->sk_flags & SK_ROW_MEMBER);
      if (subkey->sk_flags & SK_ISNULL)
      {
        _bt_parallel_done(scan);
        return false;
      }
      memcpy(inskey.scankeys + i, subkey, sizeof(ScanKeyData));

         
                                                                        
                                                                      
                                                                    
                                                                        
                                                                     
                                                                       
                                                                         
                                                                         
                                                                       
                                                                      
                                                                         
                                                        
         
      if (i == keysCount - 1)
      {
        bool used_all_subkeys = false;

        Assert(!(subkey->sk_flags & SK_ROW_END));
        for (;;)
        {
          subkey++;
          Assert(subkey->sk_flags & SK_ROW_MEMBER);
          if (subkey->sk_attno != keysCount + 1)
          {
            break;                                    
          }
          if (subkey->sk_strategy != cur->sk_strategy)
          {
            break;                                    
          }
          if (subkey->sk_flags & SK_ISNULL)
          {
            break;                          
          }
          Assert(keysCount < INDEX_MAX_KEYS);
          memcpy(inskey.scankeys + keysCount, subkey, sizeof(ScanKeyData));
          keysCount++;
          if (subkey->sk_flags & SK_ROW_END)
          {
            used_all_subkeys = true;
            break;
          }
        }
        if (!used_all_subkeys)
        {
          switch (strat_total)
          {
          case BTLessStrategyNumber:
            strat_total = BTLessEqualStrategyNumber;
            break;
          case BTGreaterStrategyNumber:
            strat_total = BTGreaterEqualStrategyNumber;
            break;
          }
        }
        break;                           
      }
    }
    else
    {
         
                                                                       
                                                                    
                                                
         
                                                                        
                                                                       
                                                                         
                                                                         
                     
         
                                                                       
                                                                     
                        
         
      if (cur->sk_subtype == rel->rd_opcintype[i] || cur->sk_subtype == InvalidOid)
      {
        FmgrInfo *procinfo;

        procinfo = index_getprocinfo(rel, cur->sk_attno, BTORDER_PROC);
        ScanKeyEntryInitializeWithInfo(inskey.scankeys + i, cur->sk_flags, cur->sk_attno, InvalidStrategy, cur->sk_subtype, cur->sk_collation, procinfo, cur->sk_argument);
      }
      else
      {
        RegProcedure cmp_proc;

        cmp_proc = get_opfamily_proc(rel->rd_opfamily[i], rel->rd_opcintype[i], cur->sk_subtype, BTORDER_PROC);
        if (!RegProcedureIsValid(cmp_proc))
        {
          elog(ERROR, "missing support function %d(%u,%u) for attribute %d of index \"%s\"", BTORDER_PROC, rel->rd_opcintype[i], cur->sk_subtype, cur->sk_attno, RelationGetRelationName(rel));
        }
        ScanKeyEntryInitialize(inskey.scankeys + i, cur->sk_flags, cur->sk_attno, InvalidStrategy, cur->sk_subtype, cur->sk_collation, cmp_proc, cur->sk_argument);
      }
    }
  }

               
                                                                            
                                                                            
                 
     
                                                                          
                                                                      
                      
     
                                                                 
                                                                 
               
     
  switch (strat_total)
  {
  case BTLessStrategyNumber:

       
                                                                      
                                                                      
                                                                   
                  
       
    nextkey = false;
    goback = true;
    break;

  case BTLessEqualStrategyNumber:

       
                                                                     
                                                                       
                                                                   
                  
       
    nextkey = true;
    goback = true;
    break;

  case BTEqualStrategyNumber:

       
                                                                       
                           
       
    if (ScanDirectionIsBackward(dir))
    {
         
                                                                    
                                                   
         
      nextkey = true;
      goback = true;
    }
    else
    {
         
                                                                    
                                                   
         
      nextkey = false;
      goback = false;
    }
    break;

  case BTGreaterEqualStrategyNumber:

       
                                                                   
               
       
    nextkey = false;
    goback = false;
    break;

  case BTGreaterStrategyNumber:

       
                                                                  
               
       
    nextkey = true;
    goback = false;
    break;

  default:
                                                 
    elog(ERROR, "unrecognized strat_total: %d", (int)strat_total);
    return false;
  }

                                                      
  inskey.heapkeyspace = _bt_heapkeyspace(rel);
  inskey.anynullkeys = false;             
  inskey.nextkey = nextkey;
  inskey.pivotsearch = false;
  inskey.scantid = NULL;
  inskey.keysz = keysCount;

     
                                                                     
                                                 
     
  stack = _bt_search(rel, &inskey, &buf, BT_READ, scan->xs_snapshot);

                                              
  _bt_freestack(stack);

  if (!BufferIsValid(buf))
  {
       
                                                                        
                                             
       
    PredicateLockRelation(rel, scan->xs_snapshot);

       
                                                                      
                  
       
    _bt_parallel_done(scan);
    BTScanPosInvalidate(so->currPos);

    return false;
  }
  else
  {
    PredicateLockPage(rel, BufferGetBlockNumber(buf), scan->xs_snapshot);
  }

  _bt_initialize_more_data(so, dir);

                                                
  offnum = _bt_binsrch(rel, &inskey, buf);

     
                                                                             
                                                                            
                                                                             
                                
     
                                                                           
                                                                            
                                                                        
                                     
     
                                                                          
                                                                             
                                                                            
                                                                            
                                                                         
                           
     
  if (goback)
  {
    offnum = OffsetNumberPrev(offnum);
  }

                                                    
  Assert(!BTScanPosIsValid(so->currPos));
  so->currPos.buf = buf;

     
                                                    
     
  if (!_bt_readpage(scan, dir, offnum))
  {
       
                                                                          
                                                                        
       
    LockBuffer(so->currPos.buf, BUFFER_LOCK_UNLOCK);
    if (!_bt_steppage(scan, dir))
    {
      return false;
    }
  }
  else
  {
                                                               
    _bt_drop_lock_and_maybe_pin(scan, &so->currPos);
  }

readcomplete:
                                         
  currItem = &so->currPos.items[so->currPos.itemIndex];
  scan->xs_heaptid = currItem->heapTid;
  if (scan->xs_want_itup)
  {
    scan->xs_itup = (IndexTuple)(so->currTuples + currItem->tupleOffset);
  }

  return true;
}

   
                                              
   
                                                                          
                                                                           
                         
   
                                                                      
                                                                         
                                                        
   
                                                             
                                      
   
bool
_bt_next(IndexScanDesc scan, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  BTScanPosItem *currItem;

     
                                                                          
                                      
     
  if (ScanDirectionIsForward(dir))
  {
    if (++so->currPos.itemIndex > so->currPos.lastItem)
    {
      if (!_bt_steppage(scan, dir))
      {
        return false;
      }
    }
  }
  else
  {
    if (--so->currPos.itemIndex < so->currPos.firstItem)
    {
      if (!_bt_steppage(scan, dir))
      {
        return false;
      }
    }
  }

                                         
  currItem = &so->currPos.items[so->currPos.itemIndex];
  scan->xs_heaptid = currItem->heapTid;
  if (scan->xs_want_itup)
  {
    scan->xs_itup = (IndexTuple)(so->currTuples + currItem->tupleOffset);
  }

  return true;
}

   
                                                                        
   
                                                                               
                                                                             
                                                                         
                                  
   
                                                                           
                                                                               
                                                                              
                                                                            
   
                                                                              
                                                             
                                          
   
                                                                        
   
static bool
_bt_readpage(IndexScanDesc scan, ScanDirection dir, OffsetNumber offnum)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  Page page;
  BTPageOpaque opaque;
  OffsetNumber minoff;
  OffsetNumber maxoff;
  int itemIndex;
  bool continuescan;
  int indnatts;

     
                                                                             
                                                                 
     
  Assert(BufferIsValid(so->currPos.buf));

  page = BufferGetPage(so->currPos.buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

                                                       
  if (scan->parallel_scan)
  {
    if (ScanDirectionIsForward(dir))
    {
      _bt_parallel_release(scan, opaque->btpo_next);
    }
    else
    {
      _bt_parallel_release(scan, BufferGetBlockNumber(so->currPos.buf));
    }
  }

  continuescan = true;                         
  indnatts = IndexRelationGetNumberOfAttributes(scan->indexRelation);
  minoff = P_FIRSTDATAKEY(opaque);
  maxoff = PageGetMaxOffsetNumber(page);

     
                                                                             
                                                                             
     
  so->currPos.currPage = BufferGetBlockNumber(so->currPos.buf);

     
                                                                           
                                                                            
                                                                    
     
  so->currPos.lsn = BufferGetLSNAtomic(so->currPos.buf);

     
                                                                         
                                                                            
                                                                         
     
  so->currPos.nextPage = opaque->btpo_next;

                                           
  so->currPos.nextTupleOffset = 0;

     
                                                                             
           
     
  Assert(BTScanPosIsPinned(so->currPos));

  if (ScanDirectionIsForward(dir))
  {
                                         
    itemIndex = 0;

    offnum = Max(offnum, minoff);

    while (offnum <= maxoff)
    {
      ItemId iid = PageGetItemId(page, offnum);
      IndexTuple itup;

         
                                                                    
                                                      
         
      if (scan->ignore_killed_tuples && ItemIdIsDead(iid))
      {
        offnum = OffsetNumberNext(offnum);
        continue;
      }

      itup = (IndexTuple)PageGetItem(page, iid);

      if (_bt_checkkeys(scan, itup, indnatts, dir, &continuescan))
      {
                                                                  
        _bt_saveitem(so, itemIndex, offnum, itup);
        itemIndex++;
      }
                                                                        
      if (!continuescan)
      {
        break;
      }

      offnum = OffsetNumberNext(offnum);
    }

       
                                                                  
                                                           
       
                                                                           
                                                                        
                                                                           
                                                                        
                                                                     
               
       
    if (continuescan && !P_RIGHTMOST(opaque))
    {
      ItemId iid = PageGetItemId(page, P_HIKEY);
      IndexTuple itup = (IndexTuple)PageGetItem(page, iid);
      int truncatt;

      truncatt = BTreeTupleGetNAtts(itup, scan->indexRelation);
      _bt_checkkeys(scan, itup, truncatt, dir, &continuescan);
    }

    if (!continuescan)
    {
      so->currPos.moreRight = false;
    }

    Assert(itemIndex <= MaxIndexTuplesPerPage);
    so->currPos.firstItem = 0;
    so->currPos.lastItem = itemIndex - 1;
    so->currPos.itemIndex = 0;
  }
  else
  {
                                          
    itemIndex = MaxIndexTuplesPerPage;

    offnum = Min(offnum, maxoff);

    while (offnum >= minoff)
    {
      ItemId iid = PageGetItemId(page, offnum);
      IndexTuple itup;
      bool tuple_alive;
      bool passes_quals;

         
                                                                    
                                                                    
                                                                    
                                                                    
                                                                         
                                                                   
                                                                       
                                                             
         
      if (scan->ignore_killed_tuples && ItemIdIsDead(iid))
      {
        Assert(offnum >= P_FIRSTDATAKEY(opaque));
        if (offnum > P_FIRSTDATAKEY(opaque))
        {
          offnum = OffsetNumberPrev(offnum);
          continue;
        }

        tuple_alive = false;
      }
      else
      {
        tuple_alive = true;
      }

      itup = (IndexTuple)PageGetItem(page, iid);

      passes_quals = _bt_checkkeys(scan, itup, indnatts, dir, &continuescan);
      if (passes_quals && tuple_alive)
      {
                                                                  
        itemIndex--;
        _bt_saveitem(so, itemIndex, offnum, itup);
      }
      if (!continuescan)
      {
                                                      
        so->currPos.moreLeft = false;
        break;
      }

      offnum = OffsetNumberPrev(offnum);
    }

    Assert(itemIndex >= 0);
    so->currPos.firstItem = itemIndex;
    so->currPos.lastItem = MaxIndexTuplesPerPage - 1;
    so->currPos.itemIndex = MaxIndexTuplesPerPage - 1;
  }

  return (so->currPos.firstItem <= so->currPos.lastItem);
}

                                                          
static void
_bt_saveitem(BTScanOpaque so, int itemIndex, OffsetNumber offnum, IndexTuple itup)
{
  BTScanPosItem *currItem = &so->currPos.items[itemIndex];

  currItem->heapTid = itup->t_tid;
  currItem->indexOffset = offnum;
  if (so->currTuples)
  {
    Size itupsz = IndexTupleSize(itup);

    currItem->tupleOffset = so->currPos.nextTupleOffset;
    memcpy(so->currTuples + so->currPos.nextTupleOffset, itup, itupsz);
    so->currPos.nextTupleOffset += MAXALIGN(itupsz);
  }
}

   
                                                                      
   
                                                                              
                                                                            
                        
   
                                                                            
                                                                               
                                                          
   
static bool
_bt_steppage(IndexScanDesc scan, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  BlockNumber blkno = InvalidBlockNumber;
  bool status = true;

  Assert(BTScanPosIsValid(so->currPos));

                                                               
  if (so->numKilled > 0)
  {
    _bt_killitems(scan);
  }

     
                                                                           
                                  
     
  if (so->markItemIndex >= 0)
  {
                                                                  
    if (BTScanPosIsPinned(so->currPos))
    {
      IncrBufferRefCount(so->currPos.buf);
    }
    memcpy(&so->markPos, &so->currPos, offsetof(BTScanPosData, items[1]) + so->currPos.lastItem * sizeof(BTScanPosItem));
    if (so->markTuples)
    {
      memcpy(so->markTuples, so->currTuples, so->currPos.nextTupleOffset);
    }
    so->markPos.itemIndex = so->markItemIndex;
    so->markItemIndex = -1;
  }

  if (ScanDirectionIsForward(dir))
  {
                                               
    if (scan->parallel_scan != NULL)
    {
         
                                                                      
                                  
         
      status = _bt_parallel_seize(scan, &blkno);
      if (!status)
      {
                                                    
        BTScanPosUnpinIfPinned(so->currPos);
        BTScanPosInvalidate(so->currPos);
        return false;
      }
    }
    else
    {
                                                                    
      blkno = so->currPos.nextPage;
    }

                                           
    so->currPos.moreLeft = true;

                                                
    BTScanPosUnpinIfPinned(so->currPos);
  }
  else
  {
                                           
    so->currPos.moreRight = true;

    if (scan->parallel_scan != NULL)
    {
         
                                                                         
                                  
         
      status = _bt_parallel_seize(scan, &blkno);
      BTScanPosUnpinIfPinned(so->currPos);
      if (!status)
      {
        BTScanPosInvalidate(so->currPos);
        return false;
      }
    }
    else
    {
                                                                        
      blkno = so->currPos.currPage;
    }
  }

  if (!_bt_readnextpage(scan, blkno, dir))
  {
    return false;
  }

                                                             
  _bt_drop_lock_and_maybe_pin(scan, &so->currPos);

  return true;
}

   
                                                                       
   
                                                                         
                                                                       
                                                           
   
                                                                             
                                                                           
   
static bool
_bt_readnextpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  Relation rel;
  Page page;
  BTPageOpaque opaque;
  bool status = true;

  rel = scan->indexRelation;

  if (ScanDirectionIsForward(dir))
  {
    for (;;)
    {
         
                                                                    
                                                             
         
      if (blkno == P_NONE || !so->currPos.moreRight)
      {
        _bt_parallel_done(scan);
        BTScanPosInvalidate(so->currPos);
        return false;
      }
                                                                        
      CHECK_FOR_INTERRUPTS();
                               
      so->currPos.buf = _bt_getbuf(rel, blkno, BT_READ);
      page = BufferGetPage(so->currPos.buf);
      TestForOldSnapshot(scan->xs_snapshot, rel, page);
      opaque = (BTPageOpaque)PageGetSpecialPointer(page);
                                  
      if (!P_IGNORE(opaque))
      {
        PredicateLockPage(rel, blkno, scan->xs_snapshot);
                                                       
                                                                
        if (_bt_readpage(scan, dir, P_FIRSTDATAKEY(opaque)))
        {
          break;
        }
      }
      else if (scan->parallel_scan != NULL)
      {
                                                             
        _bt_parallel_release(scan, opaque->btpo_next);
      }

                            
      if (scan->parallel_scan != NULL)
      {
        _bt_relbuf(rel, so->currPos.buf);
        status = _bt_parallel_seize(scan, &blkno);
        if (!status)
        {
          BTScanPosInvalidate(so->currPos);
          return false;
        }
      }
      else
      {
        blkno = opaque->btpo_next;
        _bt_relbuf(rel, so->currPos.buf);
      }
    }
  }
  else
  {
       
                                                                     
                          
       
    if (so->currPos.currPage != blkno)
    {
      BTScanPosUnpinIfPinned(so->currPos);
      so->currPos.currPage = blkno;
    }

       
                                                                        
                                                                         
                                                                 
                                                                        
                                           
       
                                                                         
                                                                         
                                                                         
                                                                          
                                                                           
                                                                           
                                                                  
       
                                                                          
                                                                           
                                                                         
                                                                          
                                                                      
                
       
    if (BTScanPosIsPinned(so->currPos))
    {
      LockBuffer(so->currPos.buf, BT_READ);
    }
    else
    {
      so->currPos.buf = _bt_getbuf(rel, so->currPos.currPage, BT_READ);
    }

    for (;;)
    {
                                                                  
      if (!so->currPos.moreLeft)
      {
        _bt_relbuf(rel, so->currPos.buf);
        _bt_parallel_done(scan);
        BTScanPosInvalidate(so->currPos);
        return false;
      }

                                      
      so->currPos.buf = _bt_walk_left(rel, so->currPos.buf, scan->xs_snapshot);

                                                               
      if (so->currPos.buf == InvalidBuffer)
      {
        _bt_parallel_done(scan);
        BTScanPosInvalidate(so->currPos);
        return false;
      }

         
                                                                      
                                                                         
                              
         
      page = BufferGetPage(so->currPos.buf);
      TestForOldSnapshot(scan->xs_snapshot, rel, page);
      opaque = (BTPageOpaque)PageGetSpecialPointer(page);
      if (!P_IGNORE(opaque))
      {
        PredicateLockPage(rel, BufferGetBlockNumber(so->currPos.buf), scan->xs_snapshot);
                                                       
                                                               
        if (_bt_readpage(scan, dir, PageGetMaxOffsetNumber(page)))
        {
          break;
        }
      }
      else if (scan->parallel_scan != NULL)
      {
                                                             
        _bt_parallel_release(scan, BufferGetBlockNumber(so->currPos.buf));
      }

         
                                                                      
                                                                        
                                                                       
                                                                       
         
      if (scan->parallel_scan != NULL)
      {
        _bt_relbuf(rel, so->currPos.buf);
        status = _bt_parallel_seize(scan, &blkno);
        if (!status)
        {
          BTScanPosInvalidate(so->currPos);
          return false;
        }
        so->currPos.buf = _bt_getbuf(rel, blkno, BT_READ);
      }
    }
  }

  return true;
}

   
                                                                               
   
                                                                        
                     
   
static bool
_bt_parallel_readpage(IndexScanDesc scan, BlockNumber blkno, ScanDirection dir)
{
  BTScanOpaque so = (BTScanOpaque)scan->opaque;

  _bt_initialize_more_data(so, dir);

  if (!_bt_readnextpage(scan, blkno, dir))
  {
    return false;
  }

                                                             
  _bt_drop_lock_and_maybe_pin(scan, &so->currPos);

  return true;
}

   
                                                      
   
                                                                          
                                                                      
                           
   
                                                                          
                  
   
                                                                          
                                                                         
                            
   
static Buffer
_bt_walk_left(Relation rel, Buffer buf, Snapshot snapshot)
{
  Page page;
  BTPageOpaque opaque;

  page = BufferGetPage(buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  for (;;)
  {
    BlockNumber obknum;
    BlockNumber lblkno;
    BlockNumber blkno;
    int tries;

                                                                 
    if (P_LEFTMOST(opaque))
    {
      _bt_relbuf(rel, buf);
      break;
    }
                                                          
    obknum = BufferGetBlockNumber(buf);
                   
    blkno = lblkno = opaque->btpo_prev;
    _bt_relbuf(rel, buf);
                                                                      
    CHECK_FOR_INTERRUPTS();
    buf = _bt_getbuf(rel, blkno, BT_READ);
    page = BufferGetPage(buf);
    TestForOldSnapshot(snapshot, rel, page);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);

       
                                                                       
                                                                          
                                                                        
                                                                           
                                                                          
       
                                                                      
                                                                       
                                              
       
    tries = 0;
    for (;;)
    {
      if (!P_ISDELETED(opaque) && opaque->btpo_next == obknum)
      {
                                           
        return buf;
      }
      if (P_RIGHTMOST(opaque) || ++tries > 4)
      {
        break;
      }
      blkno = opaque->btpo_next;
      buf = _bt_relandgetbuf(rel, buf, blkno, BT_READ);
      page = BufferGetPage(buf);
      TestForOldSnapshot(snapshot, rel, page);
      opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    }

                                                      
    buf = _bt_relandgetbuf(rel, buf, obknum, BT_READ);
    page = BufferGetPage(buf);
    TestForOldSnapshot(snapshot, rel, page);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    if (P_ISDELETED(opaque))
    {
         
                                                                     
                                                                      
                                                                        
                     
         
      for (;;)
      {
        if (P_RIGHTMOST(opaque))
        {
          elog(ERROR, "fell off the end of index \"%s\"", RelationGetRelationName(rel));
        }
        blkno = opaque->btpo_next;
        buf = _bt_relandgetbuf(rel, buf, blkno, BT_READ);
        page = BufferGetPage(buf);
        TestForOldSnapshot(snapshot, rel, page);
        opaque = (BTPageOpaque)PageGetSpecialPointer(page);
        if (!P_ISDELETED(opaque))
        {
          break;
        }
      }

         
                                                                      
                                         
         
    }
    else
    {
         
                                                                        
                                                                       
                                                          
         
      if (opaque->btpo_prev == lblkno)
      {
        elog(ERROR, "could not find left sibling of block %u in index \"%s\"", obknum, RelationGetRelationName(rel));
      }
                                                   
    }
  }

  return InvalidBuffer;
}

   
                                                                           
   
                                                                          
                                                                
   
                                                  
   
Buffer
_bt_get_endpoint(Relation rel, uint32 level, bool rightmost, Snapshot snapshot)
{
  Buffer buf;
  Page page;
  BTPageOpaque opaque;
  OffsetNumber offnum;
  BlockNumber blkno;
  IndexTuple itup;

     
                                                                        
                                                                           
                                         
     
  if (level == 0)
  {
    buf = _bt_getroot(rel, BT_READ);
  }
  else
  {
    buf = _bt_gettrueroot(rel);
  }

  if (!BufferIsValid(buf))
  {
    return InvalidBuffer;
  }

  page = BufferGetPage(buf);
  TestForOldSnapshot(snapshot, rel, page);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  for (;;)
  {
       
                                                                      
                                                                       
                                                                         
                                           
       
    while (P_IGNORE(opaque) || (rightmost && !P_RIGHTMOST(opaque)))
    {
      blkno = opaque->btpo_next;
      if (blkno == P_NONE)
      {
        elog(ERROR, "fell off the end of index \"%s\"", RelationGetRelationName(rel));
      }
      buf = _bt_relandgetbuf(rel, buf, blkno, BT_READ);
      page = BufferGetPage(buf);
      TestForOldSnapshot(snapshot, rel, page);
      opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    }

               
    if (opaque->btpo.level == level)
    {
      break;
    }
    if (opaque->btpo.level < level)
    {
      elog(ERROR, "btree level %u not found in index \"%s\"", level, RelationGetRelationName(rel));
    }

                                                     
    if (rightmost)
    {
      offnum = PageGetMaxOffsetNumber(page);
    }
    else
    {
      offnum = P_FIRSTDATAKEY(opaque);
    }

    itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, offnum));
    blkno = BTreeInnerTupleGetDownLink(itup);

    buf = _bt_relandgetbuf(rel, buf, blkno, BT_READ);
    page = BufferGetPage(buf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  }

  return buf;
}

   
                                                                        
                                                         
   
                                                                      
                                                                      
                                                                      
                            
   
static bool
_bt_endpoint(IndexScanDesc scan, ScanDirection dir)
{
  Relation rel = scan->indexRelation;
  BTScanOpaque so = (BTScanOpaque)scan->opaque;
  Buffer buf;
  Page page;
  BTPageOpaque opaque;
  OffsetNumber start;
  BTScanPosItem *currItem;

     
                                                                             
                                                                          
                    
     
  buf = _bt_get_endpoint(rel, 0, ScanDirectionIsBackward(dir), scan->xs_snapshot);

  if (!BufferIsValid(buf))
  {
       
                                                                      
               
       
    PredicateLockRelation(rel, scan->xs_snapshot);
    BTScanPosInvalidate(so->currPos);
    return false;
  }

  PredicateLockPage(rel, BufferGetBlockNumber(buf), scan->xs_snapshot);
  page = BufferGetPage(buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  Assert(P_ISLEAF(opaque));

  if (ScanDirectionIsForward(dir))
  {
                                                             
                                     

    start = P_FIRSTDATAKEY(opaque);
  }
  else if (ScanDirectionIsBackward(dir))
  {
    Assert(P_RIGHTMOST(opaque));

    start = PageGetMaxOffsetNumber(page);
  }
  else
  {
    elog(ERROR, "invalid scan direction: %d", (int)dir);
    start = 0;                          
  }

                                            
  so->currPos.buf = buf;

  _bt_initialize_more_data(so, dir);

     
                                                    
     
  if (!_bt_readpage(scan, dir, start))
  {
       
                                                                          
                                                                        
       
    LockBuffer(so->currPos.buf, BUFFER_LOCK_UNLOCK);
    if (!_bt_steppage(scan, dir))
    {
      return false;
    }
  }
  else
  {
                                                               
    _bt_drop_lock_and_maybe_pin(scan, &so->currPos);
  }

                                         
  currItem = &so->currPos.items[so->currPos.itemIndex];
  scan->xs_heaptid = currItem->heapTid;
  if (scan->xs_want_itup)
  {
    scan->xs_itup = (IndexTuple)(so->currTuples + currItem->tupleOffset);
  }

  return true;
}

   
                                                                             
                      
   
static inline void
_bt_initialize_more_data(BTScanOpaque so, ScanDirection dir)
{
                                                                      
  if (ScanDirectionIsForward(dir))
  {
    so->currPos.moreLeft = false;
    so->currPos.moreRight = true;
  }
  else
  {
    so->currPos.moreLeft = true;
    so->currPos.moreRight = false;
  }
  so->numKilled = 0;                         
  so->markItemIndex = -1;            
}
