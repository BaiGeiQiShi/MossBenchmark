                                                                            
   
               
                                                           
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/nbtree.h"
#include "access/nbtxlog.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/smgr.h"

                                                                  
#define BTREE_FASTPATH_MIN_LEVEL 2

static Buffer
_bt_newroot(Relation rel, Buffer lbuf, Buffer rbuf);

static TransactionId
_bt_check_unique(Relation rel, BTInsertState insertstate, Relation heapRel, IndexUniqueCheck checkUnique, bool *is_unique, uint32 *speculativeToken);
static OffsetNumber
_bt_findinsertloc(Relation rel, BTInsertState insertstate, bool checkingunique, BTStack stack, Relation heapRel);
static void
_bt_stepright(Relation rel, BTInsertState insertstate, BTStack stack);
static void
_bt_insertonpg(Relation rel, BTScanInsert itup_key, Buffer buf, Buffer cbuf, BTStack stack, IndexTuple itup, OffsetNumber newitemoff, bool split_only_page);
static Buffer
_bt_split(Relation rel, BTScanInsert itup_key, Buffer buf, Buffer cbuf, OffsetNumber newitemoff, Size newitemsz, IndexTuple newitem);
static void
_bt_insert_parent(Relation rel, Buffer buf, Buffer rbuf, BTStack stack, bool is_root, bool is_only);
static bool
_bt_pgaddtup(Page page, Size itemsize, IndexTuple itup, OffsetNumber itup_off);
static void
_bt_vacuum_one_page(Relation rel, Buffer buffer, Relation heapRel);

   
                                                                           
   
                                                                      
                                                   
   
                                                                    
                                                           
                                                                
                                                                     
                           
   
                                                                   
                                                              
                                                                  
                                                                   
                                    
   
bool
_bt_doinsert(Relation rel, IndexTuple itup, IndexUniqueCheck checkUnique, Relation heapRel)
{
  bool is_unique = false;
  BTInsertStateData insertstate;
  BTScanInsert itup_key;
  BTStack stack = NULL;
  Buffer buf;
  bool fastpath;
  bool checkingunique = (checkUnique != UNIQUE_CHECK_NO);

                                                                    
  itup_key = _bt_mkscankey(rel, itup);

  if (checkingunique)
  {
    if (!itup_key->anynullkeys)
    {
                                                                  
      itup_key->scantid = NULL;
    }
    else
    {
         
                                                                  
                                                                       
                                                                
         
                                                             
                                                                         
                                                            
         
      checkingunique = false;
                                                                   
      Assert(checkUnique != UNIQUE_CHECK_EXISTING);
      is_unique = true;
    }
  }

     
                                                                           
                                           
     
  insertstate.itup = itup;
                                                      
  insertstate.itemsz = MAXALIGN(IndexTupleSize(itup));
  insertstate.itup_key = itup_key;
  insertstate.bounds_valid = false;
  insertstate.buf = InvalidBuffer;

     
                                                                 
                                                                            
                                                                           
                                                                        
                                                                          
                                                                            
                                                                          
                                                                          
                                                                 
     
                                                                             
                                                                        
                                                                         
                                                                         
                                                                           
                                                                       
                                                                      
     
top:
  fastpath = false;
  if (RelationGetTargetBlock(rel) != InvalidBlockNumber)
  {
    Page page;
    BTPageOpaque lpageop;

       
                                                                           
                                                                          
                                                                         
                                                               
       
    buf = ReadBuffer(rel, RelationGetTargetBlock(rel));

    if (ConditionalLockBuffer(buf))
    {
      _bt_checkpage(rel, buf);

      page = BufferGetPage(buf);

      lpageop = (BTPageOpaque)PageGetSpecialPointer(page);

         
                                                                        
                                                                         
                                                                 
         
      if (P_ISLEAF(lpageop) && P_RIGHTMOST(lpageop) && !P_IGNORE(lpageop) && (PageGetFreeSpace(page) > insertstate.itemsz) && PageGetMaxOffsetNumber(page) >= P_FIRSTDATAKEY(lpageop) && _bt_compare(rel, itup_key, page, P_FIRSTDATAKEY(lpageop)) > 0)
      {
           
                                                                       
                                                    
           
        Assert(!P_INCOMPLETE_SPLIT(lpageop));
        fastpath = true;
      }
      else
      {
        _bt_relbuf(rel, buf);

           
                                                                    
                                                                      
                                          
           
        RelationSetTargetBlock(rel, InvalidBlockNumber);
      }
    }
    else
    {
      ReleaseBuffer(buf);

         
                                                                       
                                                                 
         
      RelationSetTargetBlock(rel, InvalidBlockNumber);
    }
  }

  if (!fastpath)
  {
       
                                                                    
                                                 
       
    stack = _bt_search(rel, itup_key, &buf, BT_WRITE, NULL);
  }

  insertstate.buf = buf;
  buf = InvalidBuffer;                                          

     
                                                                          
                
     
                                                                             
                                                                            
                                                                            
                                                                           
                                                                             
                                                                          
                                                                          
                                                                          
                                                                            
                                                                 
     
                                                                          
                                          
     
                                                                            
                                                                            
                        
     
  if (checkingunique)
  {
    TransactionId xwait;
    uint32 speculativeToken;

    xwait = _bt_check_unique(rel, &insertstate, heapRel, checkUnique, &is_unique, &speculativeToken);

    if (TransactionIdIsValid(xwait))
    {
                                              
      _bt_relbuf(rel, insertstate.buf);
      insertstate.buf = InvalidBuffer;

         
                                                                        
                                                                     
                                                      
         
      if (speculativeToken)
      {
        SpeculativeInsertionWait(xwait, speculativeToken);
      }
      else
      {
        XactLockTableWait(xwait, rel, &itup->t_tid, XLTW_InsertIndex);
      }

                         
      if (stack)
      {
        _bt_freestack(stack);
      }
      goto top;
    }

                                                                  
    if (itup_key->heapkeyspace)
    {
      itup_key->scantid = &itup->t_tid;
    }
  }

  if (checkUnique != UNIQUE_CHECK_EXISTING)
  {
    OffsetNumber newitemoff;

       
                                                                           
                                                                        
                                                                          
                                                                        
                                                                        
       
    CheckForSerializableConflictIn(rel, NULL, insertstate.buf);

       
                                                                       
                                                                           
                       
       
    newitemoff = _bt_findinsertloc(rel, &insertstate, checkingunique, stack, heapRel);
    _bt_insertonpg(rel, itup_key, insertstate.buf, InvalidBuffer, stack, itup, newitemoff, false);
  }
  else
  {
                                 
    _bt_relbuf(rel, insertstate.buf);
  }

               
  if (stack)
  {
    _bt_freestack(stack);
  }
  pfree(itup_key);

  return is_unique;
}

   
                                                                        
   
                                                                         
                                                                             
                                                                         
                                                                            
                                                                               
                                                                  
   
                                                                     
                                                                        
                                                                     
                                                   
   
                                                                         
                                                                     
         
   
                                                                            
                                                                           
                                      
   
static TransactionId
_bt_check_unique(Relation rel, BTInsertState insertstate, Relation heapRel, IndexUniqueCheck checkUnique, bool *is_unique, uint32 *speculativeToken)
{
  IndexTuple itup = insertstate->itup;
  BTScanInsert itup_key = insertstate->itup_key;
  SnapshotData SnapshotDirty;
  OffsetNumber offset;
  OffsetNumber maxoff;
  Page page;
  BTPageOpaque opaque;
  Buffer nbuf = InvalidBuffer;
  bool found = false;

                                               
  *is_unique = true;

  InitDirtySnapshot(SnapshotDirty);

  page = BufferGetPage(insertstate->buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  maxoff = PageGetMaxOffsetNumber(page);

     
                                             
     
                                                                           
                                                                            
     
  Assert(!insertstate->bounds_valid);
  offset = _bt_binsrch_insert(rel, insertstate);

     
                                                             
     
  Assert(!insertstate->bounds_valid || insertstate->low == offset);
  Assert(!itup_key->anynullkeys);
  Assert(itup_key->scantid == NULL);
  for (;;)
  {
    ItemId curitemid;
    IndexTuple curitup;
    BlockNumber nblkno;

       
                                                                      
                     
       
    if (offset <= maxoff)
    {
         
                                                                     
                                                              
                                                                         
                                                                 
                                                                     
                                    
         
                                                                
                                                                         
                                                                         
         
      if (nbuf == InvalidBuffer && offset == insertstate->stricthigh)
      {
        Assert(insertstate->bounds_valid);
        Assert(insertstate->low >= P_FIRSTDATAKEY(opaque));
        Assert(insertstate->low <= insertstate->stricthigh);
        Assert(_bt_compare(rel, itup_key, page, offset) < 0);
        break;
      }

      curitemid = PageGetItemId(page, offset);

         
                                                   
         
                                                                       
                                                                       
                                                                    
                                                                       
                                                                       
                                                                     
                                                                        
                 
         
      if (!ItemIdIsDead(curitemid))
      {
        ItemPointerData htid;
        bool all_dead;

        if (_bt_compare(rel, itup_key, page, offset) != 0)
        {
          break;                                      
        }

                                                     
        curitup = (IndexTuple)PageGetItem(page, curitemid);
        htid = curitup->t_tid;

           
                                                                     
                                                                      
                     
           
        if (checkUnique == UNIQUE_CHECK_EXISTING && ItemPointerCompare(&htid, &itup->t_tid) == 0)
        {
          found = true;
        }

           
                                                                  
                                                                       
                                                                     
                                             
           
        else if (table_index_fetch_tuple_check(heapRel, &htid, &SnapshotDirty, &all_dead))
        {
          TransactionId xwait;

             
                                                               
                                                                     
                                                                  
                                                                
                                                              
                     
             
          if (checkUnique == UNIQUE_CHECK_PARTIAL)
          {
            if (nbuf != InvalidBuffer)
            {
              _bt_relbuf(rel, nbuf);
            }
            *is_unique = false;
            return InvalidTransactionId;
          }

             
                                                                 
                                                        
             
          xwait = (TransactionIdIsValid(SnapshotDirty.xmin)) ? SnapshotDirty.xmin : SnapshotDirty.xmax;

          if (TransactionIdIsValid(xwait))
          {
            if (nbuf != InvalidBuffer)
            {
              _bt_relbuf(rel, nbuf);
            }
                                              
            *speculativeToken = SnapshotDirty.speculativeToken;
                                                         
            insertstate->bounds_valid = false;
            return xwait;
          }

             
                                                                
                                                                     
                                                                     
                                                                     
                                                         
             
                                                           
                                                                   
                                                                 
                                                                
                                                                   
                                                                     
                                                                     
                                                                     
                    
             
          htid = itup->t_tid;
          if (table_index_fetch_tuple_check(heapRel, &htid, SnapshotSelf, NULL))
          {
                                                 
          }
          else
          {
               
                                                              
                                  
               
            break;
          }

             
                                                                     
                                                                     
                                                                     
                                                           
                        
             
          CheckForSerializableConflictIn(rel, NULL, insertstate->buf);

             
                                                                     
                                                                   
                                                        
                                                                     
                                                                     
                              
             
          if (nbuf != InvalidBuffer)
          {
            _bt_relbuf(rel, nbuf);
          }
          _bt_relbuf(rel, insertstate->buf);
          insertstate->buf = InvalidBuffer;
          insertstate->bounds_valid = false;

          {
            Datum values[INDEX_MAX_KEYS];
            bool isnull[INDEX_MAX_KEYS];
            char *key_desc;

            index_deform_tuple(itup, RelationGetDescr(rel), values, isnull);

            key_desc = BuildIndexValueDescription(rel, values, isnull);

            ereport(ERROR, (errcode(ERRCODE_UNIQUE_VIOLATION), errmsg("duplicate key value violates unique constraint \"%s\"", RelationGetRelationName(rel)), key_desc ? errdetail("Key %s already exists.", key_desc) : 0, errtableconstraint(heapRel, RelationGetRelationName(rel))));
          }
        }
        else if (all_dead)
        {
             
                                                                   
                                                              
                     
             
          ItemIdMarkDead(curitemid);
          opaque->btpo_flags |= BTP_HAS_GARBAGE;

             
                                                               
                                                               
             
          if (nbuf != InvalidBuffer)
          {
            MarkBufferDirtyHint(nbuf, true);
          }
          else
          {
            MarkBufferDirtyHint(insertstate->buf, true);
          }
        }
      }
    }

       
                                                   
       
    if (offset < maxoff)
    {
      offset = OffsetNumberNext(offset);
    }
    else
    {
      int highkeycmp;

                                                                
      if (P_RIGHTMOST(opaque))
      {
        break;
      }
      highkeycmp = _bt_compare(rel, itup_key, page, P_HIKEY);
      Assert(highkeycmp <= 0);
      if (highkeycmp != 0)
      {
        break;
      }
                                                               
      for (;;)
      {
        nblkno = opaque->btpo_next;
        nbuf = _bt_relandgetbuf(rel, nbuf, nblkno, BT_READ);
        page = BufferGetPage(nbuf);
        opaque = (BTPageOpaque)PageGetSpecialPointer(page);
        if (!P_IGNORE(opaque))
        {
          break;
        }
        if (P_RIGHTMOST(opaque))
        {
          elog(ERROR, "fell off the end of index \"%s\"", RelationGetRelationName(rel));
        }
      }
      maxoff = PageGetMaxOffsetNumber(page);
      offset = P_FIRSTDATAKEY(opaque);
                                                 
    }
  }

     
                                                                          
                                                                         
                                             
     
  if (checkUnique == UNIQUE_CHECK_EXISTING && !found)
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("failed to re-find tuple within index \"%s\"", RelationGetRelationName(rel)), errhint("This may be because of a non-immutable index expression."), errtableconstraint(heapRel, RelationGetRelationName(rel))));
  }

  if (nbuf != InvalidBuffer)
  {
    _bt_relbuf(rel, nbuf);
  }

  return InvalidTransactionId;
}

   
                                                               
   
                                                                         
                                                          
   
                                                                       
                                                                         
                                                                         
                                                                        
                                            
   
                                                                         
                                                                       
                                                                       
                                                                          
                                                                        
                           
   
                                                                        
                                                                          
                                                                           
                                                             
   
                                                                      
                                                                          
                                
   
                                                                         
                                              
   
static OffsetNumber
_bt_findinsertloc(Relation rel, BTInsertState insertstate, bool checkingunique, BTStack stack, Relation heapRel)
{
  BTScanInsert itup_key = insertstate->itup_key;
  Page page = BufferGetPage(insertstate->buf);
  BTPageOpaque lpageop;

  lpageop = (BTPageOpaque)PageGetSpecialPointer(page);

                                       
  if (unlikely(insertstate->itemsz > BTMaxItemSize(page)))
  {
    _bt_check_third_page(rel, heapRel, itup_key->heapkeyspace, page, insertstate->itup);
  }

  Assert(P_ISLEAF(lpageop) && !P_INCOMPLETE_SPLIT(lpageop));
  Assert(!insertstate->bounds_valid || checkingunique);
  Assert(!itup_key->heapkeyspace || itup_key->scantid != NULL);
  Assert(itup_key->heapkeyspace || itup_key->scantid == NULL);

  if (itup_key->heapkeyspace)
  {
       
                                                                         
                                                                           
           
       
                                                                           
                                                                        
                                                                  
                                                                         
                                                                          
                                                                         
                                                                         
                              
       
    if (checkingunique)
    {
      for (;;)
      {
           
                                                   
           
                                                             
                                                                      
                                                                       
                                                                       
                                                                   
                                                       
           
        if (insertstate->bounds_valid && insertstate->low <= insertstate->stricthigh && insertstate->stricthigh <= PageGetMaxOffsetNumber(page))
        {
          break;
        }

                                                           
        if (P_RIGHTMOST(lpageop) || _bt_compare(rel, itup_key, page, P_HIKEY) <= 0)
        {
          break;
        }

        _bt_stepright(rel, insertstate, stack);
                                                     
        page = BufferGetPage(insertstate->buf);
        lpageop = (BTPageOpaque)PageGetSpecialPointer(page);
      }
    }

       
                                                                        
                             
       
    if (PageGetFreeSpace(page) < insertstate->itemsz && P_HAS_GARBAGE(lpageop))
    {
      _bt_vacuum_one_page(rel, insertstate->buf, heapRel);
      insertstate->bounds_valid = false;
    }
  }
  else
  {
                 
                                                                         
                                                                          
                                                                         
       
                                                                    
                                                                       
                                                                           
                                                                         
                                                                    
                                                                        
                      
       
                                    
                                                
                                                                   
                                    
                                                                      
                                                                         
                                                                          
                                                                      
                                                                           
                                                                      
                                                                          
                                                                           
                                                               
                 
       
    while (PageGetFreeSpace(page) < insertstate->itemsz)
    {
         
                                                                      
                                        
         
      if (P_HAS_GARBAGE(lpageop))
      {
        _bt_vacuum_one_page(rel, insertstate->buf, heapRel);
        insertstate->bounds_valid = false;

        if (PageGetFreeSpace(page) >= insertstate->itemsz)
        {
          break;                                   
        }
      }

         
                                                                
         
                                                                         
                                                                         
                                                                        
                                                                       
                                                                        
                             
         
      if (insertstate->bounds_valid && insertstate->low <= insertstate->stricthigh && insertstate->stricthigh <= PageGetMaxOffsetNumber(page))
      {
        break;
      }

      if (P_RIGHTMOST(lpageop) || _bt_compare(rel, itup_key, page, P_HIKEY) != 0 || random() <= (MAX_RANDOM_VALUE / 100))
      {
        break;
      }

      _bt_stepright(rel, insertstate, stack);
                                                   
      page = BufferGetPage(insertstate->buf);
      lpageop = (BTPageOpaque)PageGetSpecialPointer(page);
    }
  }

     
                                                                            
                                                                  
     
  Assert(P_RIGHTMOST(lpageop) || _bt_compare(rel, itup_key, page, P_HIKEY) <= 0);

  return _bt_binsrch_insert(rel, insertstate);
}

   
                                                       
   
                                                                          
                                                                           
                                                                              
                                                                              
                                          
   
                                                                            
            
   
static void
_bt_stepright(Relation rel, BTInsertState insertstate, BTStack stack)
{
  Page page;
  BTPageOpaque lpageop;
  Buffer rbuf;
  BlockNumber rblkno;

  page = BufferGetPage(insertstate->buf);
  lpageop = (BTPageOpaque)PageGetSpecialPointer(page);

  rbuf = InvalidBuffer;
  rblkno = lpageop->btpo_next;
  for (;;)
  {
    rbuf = _bt_relandgetbuf(rel, rbuf, rblkno, BT_WRITE);
    page = BufferGetPage(rbuf);
    lpageop = (BTPageOpaque)PageGetSpecialPointer(page);

       
                                                                         
                                                                        
                                                                        
                                           
       
    if (P_INCOMPLETE_SPLIT(lpageop))
    {
      _bt_finish_split(rel, rbuf, stack);
      rbuf = InvalidBuffer;
      continue;
    }

    if (!P_IGNORE(lpageop))
    {
      break;
    }
    if (P_RIGHTMOST(lpageop))
    {
      elog(ERROR, "fell off the end of index \"%s\"", RelationGetRelationName(rel));
    }

    rblkno = lpageop->btpo_next;
  }
                                                        
  _bt_relbuf(rel, insertstate->buf);
  insertstate->buf = rbuf;
  insertstate->bounds_valid = false;
}

             
                                                                         
   
                                                        
   
                                                                   
                                                                
                         
                           
                                                                    
                                                                
                                                             
                                                                
                                  
                                                                   
   
                                                                 
                                                                           
                                                                  
   
                                                                       
                                                                      
                                                                   
                                                                         
                                                                    
                                                         
             
   
static void
_bt_insertonpg(Relation rel, BTScanInsert itup_key, Buffer buf, Buffer cbuf, BTStack stack, IndexTuple itup, OffsetNumber newitemoff, bool split_only_page)
{
  Page page;
  BTPageOpaque lpageop;
  Size itemsz;

  page = BufferGetPage(buf);
  lpageop = (BTPageOpaque)PageGetSpecialPointer(page);

                                                                    
  Assert(P_ISLEAF(lpageop) == !BufferIsValid(cbuf));
                                                        
  Assert(!P_ISLEAF(lpageop) || BTreeTupleGetNAtts(itup, rel) == IndexRelationGetNumberOfAttributes(rel));
  Assert(P_ISLEAF(lpageop) || BTreeTupleGetNAtts(itup, rel) <= IndexRelationGetNumberOfKeyAttributes(rel));

                                                                    
  if (P_INCOMPLETE_SPLIT(lpageop))
  {
    elog(ERROR, "cannot insert to incompletely split page %u", BufferGetBlockNumber(buf));
  }

  itemsz = IndexTupleSize(itup);
  itemsz = MAXALIGN(itemsz);                                             
                                                        

     
                                                         
     
                                                                            
                                                                          
                                                     
     
  if (PageGetFreeSpace(page) < itemsz)
  {
    bool is_root = P_ISROOT(lpageop);
    bool is_only = P_LEFTMOST(lpageop) && P_RIGHTMOST(lpageop);
    Buffer rbuf;

       
                                                                       
                                                                         
                                                                          
                                                                        
                                                                       
                                                                     
                                                                    
                                                                          
                                          
       
                                                                     
                                                                        
                                                            
       
    Assert(!(P_ISLEAF(lpageop) && BlockNumberIsValid(RelationGetTargetBlock(rel))));

                                                     
    rbuf = _bt_split(rel, itup_key, buf, cbuf, newitemoff, itemsz, itup);
    PredicateLockPageSplit(rel, BufferGetBlockNumber(buf), BufferGetBlockNumber(rbuf));

                 
                
       
                                           
                                                 
                                                           
                                                              
                                                             
                                                         
       
                                                                         
                                                                        
                                                                     
                                                                      
                                                                         
             
                 
       
    _bt_insert_parent(rel, buf, rbuf, stack, is_root, is_only);
  }
  else
  {
    Buffer metabuf = InvalidBuffer;
    Page metapg = NULL;
    BTMetaPageData *metad = NULL;
    OffsetNumber itup_off;
    BlockNumber itup_blkno;
    BlockNumber cachedBlock = InvalidBlockNumber;

    itup_off = newitemoff;
    itup_blkno = BufferGetBlockNumber(buf);

       
                                                                        
                                                                          
                                                                          
                                                                          
                                                         
       
    if (split_only_page)
    {
      Assert(!P_ISLEAF(lpageop));

      metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_WRITE);
      metapg = BufferGetPage(metabuf);
      metad = BTPageGetMeta(metapg);

      if (metad->btm_fastlevel >= lpageop->btpo.level)
      {
                              
        _bt_relbuf(rel, metabuf);
        metabuf = InvalidBuffer;
      }
    }

       
                                                                          
                                                                          
                                                                     
                                                                           
                                                                       
               
       
    if (!P_ISLEAF(lpageop) && newitemoff == P_FIRSTDATAKEY(lpageop))
    {
      elog(ERROR, "cannot insert second negative infinity item in block %u of index \"%s\"", itup_blkno, RelationGetRelationName(rel));
    }

                                                                    
    START_CRIT_SECTION();

    if (!_bt_pgaddtup(page, itemsz, itup, newitemoff))
    {
      elog(PANIC, "failed to add new item to block %u in index \"%s\"", itup_blkno, RelationGetRelationName(rel));
    }

    MarkBufferDirty(buf);

    if (BufferIsValid(metabuf))
    {
                                       
      if (metad->btm_version < BTREE_NOVAC_VERSION)
      {
        _bt_upgrademetapage(metapg);
      }
      metad->btm_fastroot = itup_blkno;
      metad->btm_fastlevel = lpageop->btpo.level;
      MarkBufferDirty(metabuf);
    }

                                                                      
    if (BufferIsValid(cbuf))
    {
      Page cpage = BufferGetPage(cbuf);
      BTPageOpaque cpageop = (BTPageOpaque)PageGetSpecialPointer(cpage);

      Assert(P_INCOMPLETE_SPLIT(cpageop));
      cpageop->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;
      MarkBufferDirty(cbuf);
    }

       
                                                                          
                                                                          
                                                                           
                     
       
    if (P_RIGHTMOST(lpageop) && P_ISLEAF(lpageop) && !P_ISROOT(lpageop))
    {
      cachedBlock = BufferGetBlockNumber(buf);
    }

                    
    if (RelationNeedsWAL(rel))
    {
      xl_btree_insert xlrec;
      xl_btree_metadata xlmeta;
      uint8 xlinfo;
      XLogRecPtr recptr;

      xlrec.offnum = itup_off;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, SizeOfBtreeInsert);

      if (P_ISLEAF(lpageop))
      {
        xlinfo = XLOG_BTREE_INSERT_LEAF;
      }
      else
      {
           
                                                                   
                    
           
        XLogRegisterBuffer(1, cbuf, REGBUF_STANDARD);

        xlinfo = XLOG_BTREE_INSERT_UPPER;
      }

      if (BufferIsValid(metabuf))
      {
        Assert(metad->btm_version >= BTREE_NOVAC_VERSION);
        xlmeta.version = metad->btm_version;
        xlmeta.root = metad->btm_root;
        xlmeta.level = metad->btm_level;
        xlmeta.fastroot = metad->btm_fastroot;
        xlmeta.fastlevel = metad->btm_fastlevel;
        xlmeta.oldest_btpo_xact = metad->btm_oldest_btpo_xact;
        xlmeta.last_cleanup_num_heap_tuples = metad->btm_last_cleanup_num_heap_tuples;

        XLogRegisterBuffer(2, metabuf, REGBUF_WILL_INIT | REGBUF_STANDARD);
        XLogRegisterBufData(2, (char *)&xlmeta, sizeof(xl_btree_metadata));

        xlinfo = XLOG_BTREE_INSERT_META;
      }

      XLogRegisterBuffer(0, buf, REGBUF_STANDARD);
      XLogRegisterBufData(0, (char *)itup, IndexTupleSize(itup));

      recptr = XLogInsert(RM_BTREE_ID, xlinfo);

      if (BufferIsValid(metabuf))
      {
        PageSetLSN(metapg, recptr);
      }
      if (BufferIsValid(cbuf))
      {
        PageSetLSN(BufferGetPage(cbuf), recptr);
      }

      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();

                         
    if (BufferIsValid(metabuf))
    {
      _bt_relbuf(rel, metabuf);
    }
    if (BufferIsValid(cbuf))
    {
      _bt_relbuf(rel, cbuf);
    }
    _bt_relbuf(rel, buf);

       
                                                                           
                                                                          
                                                                       
                                                                          
                                
       
                                                                          
                                                                         
                                                                          
                                                                        
           
       
    if (BlockNumberIsValid(cachedBlock) && _bt_getrootheight(rel) >= BTREE_FASTPATH_MIN_LEVEL)
    {
      RelationSetTargetBlock(rel, cachedBlock);
    }
  }
}

   
                                             
   
                                                                        
                                                                     
                                                
   
                                                                   
                                                                     
                                                                      
                                                                  
                        
   
                                                                   
                                            
   
static Buffer
_bt_split(Relation rel, BTScanInsert itup_key, Buffer buf, Buffer cbuf, OffsetNumber newitemoff, Size newitemsz, IndexTuple newitem)
{
  Buffer rbuf;
  Page origpage;
  Page leftpage, rightpage;
  BlockNumber origpagenumber, rightpagenumber;
  BTPageOpaque ropaque, lopaque, oopaque;
  Buffer sbuf = InvalidBuffer;
  Page spage = NULL;
  BTPageOpaque sopaque = NULL;
  Size itemsz;
  ItemId itemid;
  IndexTuple item;
  OffsetNumber leftoff, rightoff;
  OffsetNumber firstright;
  OffsetNumber maxoff;
  OffsetNumber i;
  bool newitemonleft, isleaf;
  IndexTuple lefthikey;
  int indnatts = IndexRelationGetNumberOfAttributes(rel);
  int indnkeyatts = IndexRelationGetNumberOfKeyAttributes(rel);

     
                                                                         
                                                                           
                                                                            
                             
     
                                                                          
                                                                           
                                                                         
                                                                          
                                                                           
                     
     
  origpage = BufferGetPage(buf);
  oopaque = (BTPageOpaque)PageGetSpecialPointer(origpage);
  origpagenumber = BufferGetBlockNumber(buf);

     
                                          
     
                                                                       
                                                                       
                                                                       
     
                                                                             
                                                                      
                                                                  
                                                                          
                                                                            
                                                                           
                                             
     
  firstright = _bt_findsplitloc(rel, origpage, newitemoff, newitemsz, newitem, &newitemonleft);

                                         
  leftpage = PageGetTempPage(origpage);
  _bt_pageinit(leftpage, BufferGetPageSize(buf));
  lopaque = (BTPageOpaque)PageGetSpecialPointer(leftpage);

     
                                                                            
                            
     
  lopaque->btpo_flags = oopaque->btpo_flags;
  lopaque->btpo_flags &= ~(BTP_ROOT | BTP_SPLIT_END | BTP_HAS_GARBAGE);
                                                                          
  lopaque->btpo_flags |= BTP_INCOMPLETE_SPLIT;
  lopaque->btpo_prev = oopaque->btpo_prev;
                                                        
  lopaque->btpo.level = oopaque->btpo.level;
                                                           

     
                                                                       
                                                                        
                                                           
     
  PageSetLSN(leftpage, PageGetLSN(origpage));
  isleaf = P_ISLEAF(oopaque);

     
                                                                             
                                                                             
                 
     
                                                                          
                                                                             
                                                                           
                                                                         
                                                                             
                                                                             
                                                                             
                                                                           
                                                                         
                                                                          
                                                                        
           
     
                                                                        
                                                                         
                                                                             
                                                                         
                                                                             
                                                         
     
  if (!newitemonleft && newitemoff == firstright)
  {
                                                        
    itemsz = newitemsz;
    item = newitem;
  }
  else
  {
                                                                     
    itemid = PageGetItemId(origpage, firstright);
    itemsz = ItemIdGetLength(itemid);
    item = (IndexTuple)PageGetItem(origpage, itemid);
  }

     
                                                                       
                                                                             
                                                                        
                                                                        
                                                                         
                
     
  if (isleaf && (itup_key->heapkeyspace || indnatts != indnkeyatts))
  {
    IndexTuple lastleft;

       
                                                                          
                                                                          
                                                             
       
    if (newitemonleft && newitemoff == firstright)
    {
                                                        
      lastleft = newitem;
    }
    else
    {
      OffsetNumber lastleftoff;

                                                                     
      lastleftoff = OffsetNumberPrev(firstright);
      Assert(lastleftoff >= P_FIRSTDATAKEY(oopaque));
      itemid = PageGetItemId(origpage, lastleftoff);
      lastleft = (IndexTuple)PageGetItem(origpage, itemid);
    }

    Assert(lastleft != item);
    lefthikey = _bt_truncate(rel, lastleft, item, itup_key);
    itemsz = IndexTupleSize(lefthikey);
    itemsz = MAXALIGN(itemsz);
  }
  else
  {
    lefthikey = item;
  }

     
                                  
     
  leftoff = P_HIKEY;

  Assert(BTreeTupleGetNAtts(lefthikey, rel) > 0);
  Assert(BTreeTupleGetNAtts(lefthikey, rel) <= indnkeyatts);
  if (PageAddItem(leftpage, (Item)lefthikey, itemsz, leftoff, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR,
        "failed to add hikey to the left sibling"
        " while splitting block %u of index \"%s\"",
        origpagenumber, RelationGetRelationName(rel));
  }
  leftoff = OffsetNumberNext(leftoff);
               
  if (lefthikey != item)
  {
    pfree(lefthikey);
  }

     
                                                                          
                                                                      
                                                                      
                                                                            
                                                                        
     
                                                                             
                                                                        
                                                                             
                                                                            
                                        
     
  rbuf = _bt_getbuf(rel, P_NEW, BT_WRITE);
  rightpage = BufferGetPage(rbuf);
  rightpagenumber = BufferGetBlockNumber(rbuf);
                                               
  ropaque = (BTPageOpaque)PageGetSpecialPointer(rightpage);

     
                                                                            
                                                                            
             
     
  lopaque->btpo_next = rightpagenumber;
  lopaque->btpo_cycleid = _bt_vacuum_cycleid(rel);

     
                                                                             
                            
     
  ropaque->btpo_flags = oopaque->btpo_flags;
  ropaque->btpo_flags &= ~(BTP_ROOT | BTP_SPLIT_END | BTP_HAS_GARBAGE);
  ropaque->btpo_prev = origpagenumber;
  ropaque->btpo_next = oopaque->btpo_next;
  ropaque->btpo.level = oopaque->btpo.level;
  ropaque->btpo_cycleid = lopaque->btpo_cycleid;

     
                                                    
     
                                                                           
                                                                     
               
     
  rightoff = P_HIKEY;

  if (!P_RIGHTMOST(oopaque))
  {
    itemid = PageGetItemId(origpage, P_HIKEY);
    itemsz = ItemIdGetLength(itemid);
    item = (IndexTuple)PageGetItem(origpage, itemid);
    Assert(BTreeTupleGetNAtts(item, rel) > 0);
    Assert(BTreeTupleGetNAtts(item, rel) <= indnkeyatts);
    if (PageAddItem(rightpage, (Item)item, itemsz, rightoff, false, false) == InvalidOffsetNumber)
    {
      memset(rightpage, 0, BufferGetPageSize(rbuf));
      elog(ERROR,
          "failed to add hikey to the right sibling"
          " while splitting block %u of index \"%s\"",
          origpagenumber, RelationGetRelationName(rel));
    }
    rightoff = OffsetNumberNext(rightoff);
  }

     
                                                                          
                                                                       
     
                                                                           
                                                   
     
  maxoff = PageGetMaxOffsetNumber(origpage);

  for (i = P_FIRSTDATAKEY(oopaque); i <= maxoff; i = OffsetNumberNext(i))
  {
    itemid = PageGetItemId(origpage, i);
    itemsz = ItemIdGetLength(itemid);
    item = (IndexTuple)PageGetItem(origpage, itemid);

                                               
    if (i == newitemoff)
    {
      if (newitemonleft)
      {
        Assert(newitemoff <= firstright);
        if (!_bt_pgaddtup(leftpage, newitemsz, newitem, leftoff))
        {
          memset(rightpage, 0, BufferGetPageSize(rbuf));
          elog(ERROR,
              "failed to add new item to the left sibling"
              " while splitting block %u of index \"%s\"",
              origpagenumber, RelationGetRelationName(rel));
        }
        leftoff = OffsetNumberNext(leftoff);
      }
      else
      {
        Assert(newitemoff >= firstright);
        if (!_bt_pgaddtup(rightpage, newitemsz, newitem, rightoff))
        {
          memset(rightpage, 0, BufferGetPageSize(rbuf));
          elog(ERROR,
              "failed to add new item to the right sibling"
              " while splitting block %u of index \"%s\"",
              origpagenumber, RelationGetRelationName(rel));
        }
        rightoff = OffsetNumberNext(rightoff);
      }
    }

                                        
    if (i < firstright)
    {
      if (!_bt_pgaddtup(leftpage, itemsz, item, leftoff))
      {
        memset(rightpage, 0, BufferGetPageSize(rbuf));
        elog(ERROR,
            "failed to add old item to the left sibling"
            " while splitting block %u of index \"%s\"",
            origpagenumber, RelationGetRelationName(rel));
      }
      leftoff = OffsetNumberNext(leftoff);
    }
    else
    {
      if (!_bt_pgaddtup(rightpage, itemsz, item, rightoff))
      {
        memset(rightpage, 0, BufferGetPageSize(rbuf));
        elog(ERROR,
            "failed to add old item to the right sibling"
            " while splitting block %u of index \"%s\"",
            origpagenumber, RelationGetRelationName(rel));
      }
      rightoff = OffsetNumberNext(rightoff);
    }
  }

                                                          
  if (i <= newitemoff)
  {
       
                                                                           
                                                                          
                                   
       
    Assert(!newitemonleft);
    if (!_bt_pgaddtup(rightpage, newitemsz, newitem, rightoff))
    {
      memset(rightpage, 0, BufferGetPageSize(rbuf));
      elog(ERROR,
          "failed to add new item to the right sibling"
          " while splitting block %u of index \"%s\"",
          origpagenumber, RelationGetRelationName(rel));
    }
    rightoff = OffsetNumberNext(rightoff);
  }

     
                                                                         
                                                                        
                                                                             
                                                                    
                
     
  if (!P_RIGHTMOST(oopaque))
  {
    sbuf = _bt_getbuf(rel, oopaque->btpo_next, BT_WRITE);
    spage = BufferGetPage(sbuf);
    sopaque = (BTPageOpaque)PageGetSpecialPointer(spage);
    if (sopaque->btpo_prev != origpagenumber)
    {
      memset(rightpage, 0, BufferGetPageSize(rbuf));
      elog(ERROR,
          "right sibling's left-link doesn't match: "
          "block %u links to %u instead of expected %u in index \"%s\"",
          oopaque->btpo_next, sopaque->btpo_prev, origpagenumber, RelationGetRelationName(rel));
    }

       
                                                                       
                                                                       
                                                                       
                                                                           
                                                                       
                                                                        
                                                                        
                                                                        
                                                                         
                                                                        
                           
       
    if (sopaque->btpo_cycleid != ropaque->btpo_cycleid)
    {
      ropaque->btpo_flags |= BTP_SPLIT_END;
    }
  }

     
                                                                           
                         
     
                                                                            
                                                                         
                                                              
     
  START_CRIT_SECTION();

     
                                                                             
                                                                         
                                                                          
                                                                             
                                                                       
                                                                            
                                                                       
                                                                             
                                                                
     
                                                                          
                                                    
     
  PageRestoreTempPage(leftpage, origpage);
                                                     

  MarkBufferDirty(buf);
  MarkBufferDirty(rbuf);

  if (!P_RIGHTMOST(ropaque))
  {
    sopaque->btpo_prev = rightpagenumber;
    MarkBufferDirty(sbuf);
  }

     
                                                                             
              
     
  if (!isleaf)
  {
    Page cpage = BufferGetPage(cbuf);
    BTPageOpaque cpageop = (BTPageOpaque)PageGetSpecialPointer(cpage);

    cpageop->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;
    MarkBufferDirty(cbuf);
  }

                  
  if (RelationNeedsWAL(rel))
  {
    xl_btree_split xlrec;
    uint8 xlinfo;
    XLogRecPtr recptr;

    xlrec.level = ropaque->btpo.level;
    xlrec.firstright = firstright;
    xlrec.newitemoff = newitemoff;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfBtreeSplit);

    XLogRegisterBuffer(0, buf, REGBUF_STANDARD);
    XLogRegisterBuffer(1, rbuf, REGBUF_WILL_INIT);
                                                                        
    if (!P_RIGHTMOST(ropaque))
    {
      XLogRegisterBuffer(2, sbuf, REGBUF_STANDARD);
    }
    if (BufferIsValid(cbuf))
    {
      XLogRegisterBuffer(3, cbuf, REGBUF_STANDARD);
    }

       
                                                                         
                                                                     
                                                                          
                                                                          
                                                                         
                                                                      
                                             
       
    if (newitemonleft)
    {
      XLogRegisterBufData(0, (char *)newitem, MAXALIGN(newitemsz));
    }

                                          
    itemid = PageGetItemId(origpage, P_HIKEY);
    item = (IndexTuple)PageGetItem(origpage, itemid);
    XLogRegisterBufData(0, (char *)item, MAXALIGN(IndexTupleSize(item)));

       
                                                                      
                                                                    
       
                                                                          
                                                                 
                                                                        
                                                                        
                           
       
    XLogRegisterBufData(1, (char *)rightpage + ((PageHeader)rightpage)->pd_upper, ((PageHeader)rightpage)->pd_special - ((PageHeader)rightpage)->pd_upper);

    xlinfo = newitemonleft ? XLOG_BTREE_SPLIT_L : XLOG_BTREE_SPLIT_R;
    recptr = XLogInsert(RM_BTREE_ID, xlinfo);

    PageSetLSN(origpage, recptr);
    PageSetLSN(rightpage, recptr);
    if (!P_RIGHTMOST(ropaque))
    {
      PageSetLSN(spage, recptr);
    }
    if (!isleaf)
    {
      PageSetLSN(BufferGetPage(cbuf), recptr);
    }
  }

  END_CRIT_SECTION();

                                     
  if (!P_RIGHTMOST(ropaque))
  {
    _bt_relbuf(rel, sbuf);
  }

                         
  if (!isleaf)
  {
    _bt_relbuf(rel, cbuf);
  }

                    
  return rbuf;
}

   
                                                                         
   
                                                                       
                                                                     
                                                                       
                                                                            
                                                                           
                                                                       
                                                                          
   
                                                                            
                                                                        
                                    
                                                                            
   
static void
_bt_insert_parent(Relation rel, Buffer buf, Buffer rbuf, BTStack stack, bool is_root, bool is_only)
{
     
                                                                             
                                                                         
                                                                            
                                                                     
                                                                             
                                                                            
                                                   
     
                                                                          
                                                                           
                
     
  if (is_root)
  {
    Buffer rootbuf;

    Assert(stack == NULL);
    Assert(is_only);
                                                        
    rootbuf = _bt_newroot(rel, buf, rbuf);
                                   
    _bt_relbuf(rel, rootbuf);
    _bt_relbuf(rel, rbuf);
    _bt_relbuf(rel, buf);
  }
  else
  {
    BlockNumber bknum = BufferGetBlockNumber(buf);
    BlockNumber rbknum = BufferGetBlockNumber(rbuf);
    Page page = BufferGetPage(buf);
    IndexTuple new_item;
    BTStackData fakestack;
    IndexTuple ritem;
    Buffer pbuf;

    if (stack == NULL)
    {
      BTPageOpaque lpageop;

      elog(DEBUG2, "concurrent ROOT page split");
      lpageop = (BTPageOpaque)PageGetSpecialPointer(page);
                                                       
      pbuf = _bt_get_endpoint(rel, lpageop->btpo.level + 1, false, NULL);
                                                     
      stack = &fakestack;
      stack->bts_blkno = BufferGetBlockNumber(pbuf);
      stack->bts_offset = InvalidOffsetNumber;
      stack->bts_btentry = InvalidBlockNumber;
      stack->bts_parent = NULL;
      _bt_relbuf(rel, pbuf);
    }

                                                                         
    ritem = (IndexTuple)PageGetItem(page, PageGetItemId(page, P_HIKEY));

                                                               
    new_item = CopyIndexTuple(ritem);
    BTreeInnerTupleSetDownLink(new_item, rbknum);

       
                                                 
       
                                                                           
                                                                        
                                                                         
                                                                       
                                
       
    stack->bts_btentry = bknum;
    pbuf = _bt_getstackbuf(rel, stack);

       
                                                                          
                            
       
    _bt_relbuf(rel, rbuf);

    if (pbuf == InvalidBuffer)
    {
      elog(ERROR, "failed to re-find parent key in index \"%s\" for split pages %u/%u", RelationGetRelationName(rel), bknum, rbknum);
    }

                                       
    _bt_insertonpg(rel, NULL, pbuf, buf, stack->bts_parent, new_item, stack->bts_offset + 1, is_only);

                 
    pfree(new_item);
  }
}

   
                                                    
   
                                                                         
                                                                        
                                                             
   
                                                                           
                 
   
void
_bt_finish_split(Relation rel, Buffer lbuf, BTStack stack)
{
  Page lpage = BufferGetPage(lbuf);
  BTPageOpaque lpageop = (BTPageOpaque)PageGetSpecialPointer(lpage);
  Buffer rbuf;
  Page rpage;
  BTPageOpaque rpageop;
  bool was_root;
  bool was_only;

  Assert(P_INCOMPLETE_SPLIT(lpageop));

                                                        
  rbuf = _bt_getbuf(rel, lpageop->btpo_next, BT_WRITE);
  rpage = BufferGetPage(rbuf);
  rpageop = (BTPageOpaque)PageGetSpecialPointer(rpage);

                                   
  if (!stack)
  {
    Buffer metabuf;
    Page metapg;
    BTMetaPageData *metad;

                                      
    metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_WRITE);
    metapg = BufferGetPage(metabuf);
    metad = BTPageGetMeta(metapg);

    was_root = (metad->btm_root == BufferGetBlockNumber(lbuf));

    _bt_relbuf(rel, metabuf);
  }
  else
  {
    was_root = false;
  }

                                                         
  was_only = (P_LEFTMOST(lpageop) && P_RIGHTMOST(rpageop));

  elog(DEBUG1, "finishing incomplete split of %u/%u", BufferGetBlockNumber(lbuf), BufferGetBlockNumber(rbuf));

  _bt_insert_parent(rel, lbuf, rbuf, stack, was_root, was_only);
}

   
                                                                          
                                          
   
                                                                        
                                                                         
                                                                       
                                                                      
   
                                               
   
                                                                    
                         
   
Buffer
_bt_getstackbuf(Relation rel, BTStack stack)
{
  BlockNumber blkno;
  OffsetNumber start;

  blkno = stack->bts_blkno;
  start = stack->bts_offset;

  for (;;)
  {
    Buffer buf;
    Page page;
    BTPageOpaque opaque;

    buf = _bt_getbuf(rel, blkno, BT_WRITE);
    page = BufferGetPage(buf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);

    if (P_INCOMPLETE_SPLIT(opaque))
    {
      _bt_finish_split(rel, buf, stack->bts_parent);
      continue;
    }

    if (!P_IGNORE(opaque))
    {
      OffsetNumber offnum, minoff, maxoff;
      ItemId itemid;
      IndexTuple item;

      minoff = P_FIRSTDATAKEY(opaque);
      maxoff = PageGetMaxOffsetNumber(page);

         
                                                                       
                                                                       
                                        
         
      if (start < minoff)
      {
        start = minoff;
      }

         
                                                                     
                                               
         
      if (start > maxoff)
      {
        start = OffsetNumberNext(maxoff);
      }

         
                                                                     
                                                                      
                                                         
         
      for (offnum = start; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
      {
        itemid = PageGetItemId(page, offnum);
        item = (IndexTuple)PageGetItem(page, itemid);

        if (BTreeInnerTupleGetDownLink(item) == stack->bts_btentry)
        {
                                                            
          stack->bts_blkno = blkno;
          stack->bts_offset = offnum;
          return buf;
        }
      }

      for (offnum = OffsetNumberPrev(start); offnum >= minoff; offnum = OffsetNumberPrev(offnum))
      {
        itemid = PageGetItemId(page, offnum);
        item = (IndexTuple)PageGetItem(page, itemid);

        if (BTreeInnerTupleGetDownLink(item) == stack->bts_btentry)
        {
                                                            
          stack->bts_blkno = blkno;
          stack->bts_offset = offnum;
          return buf;
        }
      }
    }

       
                                                                 
       
    if (P_RIGHTMOST(opaque))
    {
      _bt_relbuf(rel, buf);
      return InvalidBuffer;
    }
    blkno = opaque->btpo_next;
    start = InvalidOffsetNumber;
    _bt_relbuf(rel, buf);
  }
}

   
                                                          
   
                                                                     
                                                                       
                                                                         
                                                                       
                                                                         
                                                                       
                                                                       
           
   
                                                                     
                                                                 
                                                                 
                                                                        
                          
   
static Buffer
_bt_newroot(Relation rel, Buffer lbuf, Buffer rbuf)
{
  Buffer rootbuf;
  Page lpage, rootpage;
  BlockNumber lbkno, rbkno;
  BlockNumber rootblknum;
  BTPageOpaque rootopaque;
  BTPageOpaque lopaque;
  ItemId itemid;
  IndexTuple item;
  IndexTuple left_item;
  Size left_item_sz;
  IndexTuple right_item;
  Size right_item_sz;
  Buffer metabuf;
  Page metapg;
  BTMetaPageData *metad;

  lbkno = BufferGetBlockNumber(lbuf);
  rbkno = BufferGetBlockNumber(rbuf);
  lpage = BufferGetPage(lbuf);
  lopaque = (BTPageOpaque)PageGetSpecialPointer(lpage);

                           
  rootbuf = _bt_getbuf(rel, P_NEW, BT_WRITE);
  rootpage = BufferGetPage(rootbuf);
  rootblknum = BufferGetBlockNumber(rootbuf);

                                    
  metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_WRITE);
  metapg = BufferGetPage(metabuf);
  metad = BTPageGetMeta(metapg);

     
                                                                            
                                                                         
                                                       
     
  left_item_sz = sizeof(IndexTupleData);
  left_item = (IndexTuple)palloc(left_item_sz);
  left_item->t_info = left_item_sz;
  BTreeInnerTupleSetDownLink(left_item, lbkno);
  BTreeTupleSetNAtts(left_item, 0);

     
                                                                           
                                               
     
  itemid = PageGetItemId(lpage, P_HIKEY);
  right_item_sz = ItemIdGetLength(itemid);
  item = (IndexTuple)PageGetItem(lpage, itemid);
  right_item = CopyIndexTuple(item);
  BTreeInnerTupleSetDownLink(right_item, rbkno);

                                                             
  START_CRIT_SECTION();

                                  
  if (metad->btm_version < BTREE_NOVAC_VERSION)
  {
    _bt_upgrademetapage(metapg);
  }

                              
  rootopaque = (BTPageOpaque)PageGetSpecialPointer(rootpage);
  rootopaque->btpo_prev = rootopaque->btpo_next = P_NONE;
  rootopaque->btpo_flags = BTP_ROOT;
  rootopaque->btpo.level = ((BTPageOpaque)PageGetSpecialPointer(lpage))->btpo.level + 1;
  rootopaque->btpo_cycleid = 0;

                            
  metad->btm_root = rootblknum;
  metad->btm_level = rootopaque->btpo.level;
  metad->btm_fastroot = rootblknum;
  metad->btm_fastlevel = rootopaque->btpo.level;

     
                                                                            
                                                                          
                                                              
     
                                                                        
                                    
     
  Assert(BTreeTupleGetNAtts(left_item, rel) == 0);
  if (PageAddItem(rootpage, (Item)left_item, left_item_sz, P_HIKEY, false, false) == InvalidOffsetNumber)
  {
    elog(PANIC,
        "failed to add leftkey to new root page"
        " while splitting block %u of index \"%s\"",
        BufferGetBlockNumber(lbuf), RelationGetRelationName(rel));
  }

     
                                                           
     
  Assert(BTreeTupleGetNAtts(right_item, rel) > 0);
  Assert(BTreeTupleGetNAtts(right_item, rel) <= IndexRelationGetNumberOfKeyAttributes(rel));
  if (PageAddItem(rootpage, (Item)right_item, right_item_sz, P_FIRSTKEY, false, false) == InvalidOffsetNumber)
  {
    elog(PANIC,
        "failed to add rightkey to new root page"
        " while splitting block %u of index \"%s\"",
        BufferGetBlockNumber(lbuf), RelationGetRelationName(rel));
  }

                                                         
  Assert(P_INCOMPLETE_SPLIT(lopaque));
  lopaque->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;
  MarkBufferDirty(lbuf);

  MarkBufferDirty(rootbuf);
  MarkBufferDirty(metabuf);

                  
  if (RelationNeedsWAL(rel))
  {
    xl_btree_newroot xlrec;
    XLogRecPtr recptr;
    xl_btree_metadata md;

    xlrec.rootblk = rootblknum;
    xlrec.level = metad->btm_level;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfBtreeNewroot);

    XLogRegisterBuffer(0, rootbuf, REGBUF_WILL_INIT);
    XLogRegisterBuffer(1, lbuf, REGBUF_STANDARD);
    XLogRegisterBuffer(2, metabuf, REGBUF_WILL_INIT | REGBUF_STANDARD);

    Assert(metad->btm_version >= BTREE_NOVAC_VERSION);
    md.version = metad->btm_version;
    md.root = rootblknum;
    md.level = metad->btm_level;
    md.fastroot = rootblknum;
    md.fastlevel = metad->btm_level;
    md.oldest_btpo_xact = metad->btm_oldest_btpo_xact;
    md.last_cleanup_num_heap_tuples = metad->btm_last_cleanup_num_heap_tuples;

    XLogRegisterBufData(2, (char *)&md, sizeof(xl_btree_metadata));

       
                                                                          
                                  
       
    XLogRegisterBufData(0, (char *)rootpage + ((PageHeader)rootpage)->pd_upper, ((PageHeader)rootpage)->pd_special - ((PageHeader)rootpage)->pd_upper);

    recptr = XLogInsert(RM_BTREE_ID, XLOG_BTREE_NEWROOT);

    PageSetLSN(lpage, recptr);
    PageSetLSN(rootpage, recptr);
    PageSetLSN(metapg, recptr);
  }

  END_CRIT_SECTION();

                          
  _bt_relbuf(rel, metabuf);

  pfree(left_item);
  pfree(right_item);

  return rootbuf;
}

   
                                                                    
   
                                                                   
                                                                   
                                                                     
                                   
   
                                                                         
                                                                        
                                                                      
                                                                       
                                                                   
                                               
   
static bool
_bt_pgaddtup(Page page, Size itemsize, IndexTuple itup, OffsetNumber itup_off)
{
  BTPageOpaque opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  IndexTupleData trunctuple;

  if (!P_ISLEAF(opaque) && itup_off == P_FIRSTDATAKEY(opaque))
  {
    trunctuple = *itup;
    trunctuple.t_info = sizeof(IndexTupleData);
                                                   
    BTreeTupleSetNAtts(&trunctuple, 0);
    itup = &trunctuple;
    itemsize = sizeof(IndexTupleData);
  }

  if (PageAddItem(page, (Item)itup, itemsize, itup_off, false, false) == InvalidOffsetNumber)
  {
    return false;
  }

  return true;
}

   
                                                     
   
                                                                       
                                                                       
                                                       
   
static void
_bt_vacuum_one_page(Relation rel, Buffer buffer, Relation heapRel)
{
  OffsetNumber deletable[MaxOffsetNumber];
  int ndeletable = 0;
  OffsetNumber offnum, minoff, maxoff;
  Page page = BufferGetPage(buffer);
  BTPageOpaque opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  Assert(P_ISLEAF(opaque));

     
                                                                           
                    
     
  minoff = P_FIRSTDATAKEY(opaque);
  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = minoff; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemId = PageGetItemId(page, offnum);

    if (ItemIdIsDead(itemId))
    {
      deletable[ndeletable++] = offnum;
    }
  }

  if (ndeletable > 0)
  {
    _bt_delitems_delete(rel, buffer, deletable, ndeletable, heapRel);
  }

     
                                                                
                                                                            
                                                                          
               
     
}
