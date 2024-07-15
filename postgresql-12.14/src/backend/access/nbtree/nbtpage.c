                                                                            
   
             
                                                                       
             
   
                                                                         
                                                                        
   
   
                  
                                         
   
         
                                                                          
                                                                          
                                                                            
                                                                          
                                                       
   
                                                                            
   
#include "postgres.h"

#include "access/nbtree.h"
#include "access/nbtxlog.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "miscadmin.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/snapmgr.h"

static BTMetaPageData *
_bt_getmeta(Relation rel, Buffer metabuf);
static bool
_bt_mark_page_halfdead(Relation rel, Buffer leafbuf, BTStack stack);
static bool
_bt_unlink_halfdead_page(Relation rel, Buffer leafbuf, BlockNumber scanblkno, bool *rightsib_empty, TransactionId *oldestBtpoXact, uint32 *ndeleted);
static bool
_bt_lock_branch_parent(Relation rel, BlockNumber child, BTStack stack, Buffer *topparent, OffsetNumber *topoff, BlockNumber *target, BlockNumber *rightsib);
static void
_bt_log_reuse_page(Relation rel, BlockNumber blkno, TransactionId latestRemovedXid);

   
                                                                          
   
void
_bt_initmetapage(Page page, BlockNumber rootbknum, uint32 level)
{
  BTMetaPageData *metad;
  BTPageOpaque metaopaque;

  _bt_pageinit(page, BLCKSZ);

  metad = BTPageGetMeta(page);
  metad->btm_magic = BTREE_MAGIC;
  metad->btm_version = BTREE_VERSION;
  metad->btm_root = rootbknum;
  metad->btm_level = level;
  metad->btm_fastroot = rootbknum;
  metad->btm_fastlevel = level;
  metad->btm_oldest_btpo_xact = InvalidTransactionId;
  metad->btm_last_cleanup_num_heap_tuples = -1.0;

  metaopaque = (BTPageOpaque)PageGetSpecialPointer(page);
  metaopaque->btpo_flags = BTP_META;

     
                                                                         
                                                                          
               
     
  ((PageHeader)page)->pd_lower = ((char *)metad + sizeof(BTMetaPageData)) - (char *)page;
}

   
                                                                              
                                                                      
                                                                      
   
                                                                 
                                              
   
void
_bt_upgrademetapage(Page page)
{
  BTMetaPageData *metad;
  BTPageOpaque metaopaque PG_USED_FOR_ASSERTS_ONLY;

  metad = BTPageGetMeta(page);
  metaopaque = (BTPageOpaque)PageGetSpecialPointer(page);

                                                           
  Assert(metaopaque->btpo_flags & BTP_META);
  Assert(metad->btm_version < BTREE_NOVAC_VERSION);
  Assert(metad->btm_version >= BTREE_MIN_VERSION);

                                                                     
  metad->btm_version = BTREE_NOVAC_VERSION;
  metad->btm_oldest_btpo_xact = InvalidTransactionId;
  metad->btm_last_cleanup_num_heap_tuples = -1.0;

                                                            
  ((PageHeader)page)->pd_lower = ((char *)metad + sizeof(BTMetaPageData)) - (char *)page;
}

   
                                                                               
                           
   
                                                                            
                                                                               
                                                                             
   
static BTMetaPageData *
_bt_getmeta(Relation rel, Buffer metabuf)
{
  Page metapg;
  BTPageOpaque metaopaque;
  BTMetaPageData *metad;

  metapg = BufferGetPage(metabuf);
  metaopaque = (BTPageOpaque)PageGetSpecialPointer(metapg);
  metad = BTPageGetMeta(metapg);

                                 
  if (!P_ISMETA(metaopaque) || metad->btm_magic != BTREE_MAGIC)
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" is not a btree", RelationGetRelationName(rel))));
  }

  if (metad->btm_version < BTREE_MIN_VERSION || metad->btm_version > BTREE_VERSION)
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("version mismatch in index \"%s\": file version %d, "
                                                             "current version %d, minimal supported version %d",
                                                          RelationGetRelationName(rel), metad->btm_version, BTREE_VERSION, BTREE_MIN_VERSION)));
  }

  return metad;
}

   
                                                                           
                           
   
                                                                            
                                                                             
   
void
_bt_update_meta_cleanup_info(Relation rel, TransactionId oldestBtpoXact, float8 numHeapTuples)
{
  Buffer metabuf;
  Page metapg;
  BTMetaPageData *metad;
  bool needsRewrite = false;
  XLogRecPtr recptr;

                                                       
  metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_READ);
  metapg = BufferGetPage(metabuf);
  metad = BTPageGetMeta(metapg);

                                                         
  if (metad->btm_version < BTREE_NOVAC_VERSION)
  {
    needsRewrite = true;
  }
  else if (metad->btm_oldest_btpo_xact != oldestBtpoXact || metad->btm_last_cleanup_num_heap_tuples != numHeapTuples)
  {
    needsRewrite = true;
  }

  if (!needsRewrite)
  {
    _bt_relbuf(rel, metabuf);
    return;
  }

                                               
  LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
  LockBuffer(metabuf, BT_WRITE);

  START_CRIT_SECTION();

                                   
  if (metad->btm_version < BTREE_NOVAC_VERSION)
  {
    _bt_upgrademetapage(metapg);
  }

                                          
  metad->btm_oldest_btpo_xact = oldestBtpoXact;
  metad->btm_last_cleanup_num_heap_tuples = numHeapTuples;
  MarkBufferDirty(metabuf);

                                  
  if (RelationNeedsWAL(rel))
  {
    xl_btree_metadata md;

    XLogBeginInsert();
    XLogRegisterBuffer(0, metabuf, REGBUF_WILL_INIT | REGBUF_STANDARD);

    Assert(metad->btm_version >= BTREE_NOVAC_VERSION);
    md.version = metad->btm_version;
    md.root = metad->btm_root;
    md.level = metad->btm_level;
    md.fastroot = metad->btm_fastroot;
    md.fastlevel = metad->btm_fastlevel;
    md.oldest_btpo_xact = oldestBtpoXact;
    md.last_cleanup_num_heap_tuples = numHeapTuples;

    XLogRegisterBufData(0, (char *)&md, sizeof(xl_btree_metadata));

    recptr = XLogInsert(RM_BTREE_ID, XLOG_BTREE_META_CLEANUP);

    PageSetLSN(metapg, recptr);
  }

  END_CRIT_SECTION();
  _bt_relbuf(rel, metabuf);
}

   
                                                    
   
                                                                        
                                                                     
                                                                     
                                                                     
                                                            
   
                                                                     
                                                                  
                                                                
                                                                  
                                                                    
                                     
   
                                                                       
                                                                        
                                                                    
                                                                        
                                                                       
                                                                    
                                                                   
                                                   
   
                                                                   
                                                       
   
Buffer
_bt_getroot(Relation rel, int access)
{
  Buffer metabuf;
  Buffer rootbuf;
  Page rootpage;
  BTPageOpaque rootopaque;
  BlockNumber rootblkno;
  uint32 rootlevel;
  BTMetaPageData *metad;

     
                                                                        
                                                                        
                                                             
     
  if (rel->rd_amcache != NULL)
  {
    metad = (BTMetaPageData *)rel->rd_amcache;
                                                          
    Assert(metad->btm_magic == BTREE_MAGIC);
    Assert(metad->btm_version >= BTREE_MIN_VERSION);
    Assert(metad->btm_version <= BTREE_VERSION);
    Assert(metad->btm_root != P_NONE);

    rootblkno = metad->btm_fastroot;
    Assert(rootblkno != P_NONE);
    rootlevel = metad->btm_fastlevel;

    rootbuf = _bt_getbuf(rel, rootblkno, BT_READ);
    rootpage = BufferGetPage(rootbuf);
    rootopaque = (BTPageOpaque)PageGetSpecialPointer(rootpage);

       
                                                                        
                                                                         
                                                                         
                                                                           
                                                
       
    if (!P_IGNORE(rootopaque) && rootopaque->btpo.level == rootlevel && P_LEFTMOST(rootopaque) && P_RIGHTMOST(rootopaque))
    {
                                              
      return rootbuf;
    }
    _bt_relbuf(rel, rootbuf);
                                       
    if (rel->rd_amcache)
    {
      pfree(rel->rd_amcache);
    }
    rel->rd_amcache = NULL;
  }

  metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_READ);
  metad = _bt_getmeta(rel, metabuf);

                                              
  if (metad->btm_root == P_NONE)
  {
    Page metapg;

                                                                        
    if (access == BT_READ)
    {
      _bt_relbuf(rel, metabuf);
      return InvalidBuffer;
    }

                                                 
    LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
    LockBuffer(metabuf, BT_WRITE);

       
                                                                        
                                                                          
                                  
       
    if (metad->btm_root != P_NONE)
    {
         
                                                                         
                                                                       
                                                                         
                                 
         
      _bt_relbuf(rel, metabuf);
      return _bt_getroot(rel, access);
    }

       
                                                                           
                                                                          
                                   
       
    rootbuf = _bt_getbuf(rel, P_NEW, BT_WRITE);
    rootblkno = BufferGetBlockNumber(rootbuf);
    rootpage = BufferGetPage(rootbuf);
    rootopaque = (BTPageOpaque)PageGetSpecialPointer(rootpage);
    rootopaque->btpo_prev = rootopaque->btpo_next = P_NONE;
    rootopaque->btpo_flags = (BTP_LEAF | BTP_ROOT);
    rootopaque->btpo.level = 0;
    rootopaque->btpo_cycleid = 0;
                                           
    metapg = BufferGetPage(metabuf);

                                             
    START_CRIT_SECTION();

                                    
    if (metad->btm_version < BTREE_NOVAC_VERSION)
    {
      _bt_upgrademetapage(metapg);
    }

    metad->btm_root = rootblkno;
    metad->btm_level = 0;
    metad->btm_fastroot = rootblkno;
    metad->btm_fastlevel = 0;
    metad->btm_oldest_btpo_xact = InvalidTransactionId;
    metad->btm_last_cleanup_num_heap_tuples = -1.0;

    MarkBufferDirty(rootbuf);
    MarkBufferDirty(metabuf);

                    
    if (RelationNeedsWAL(rel))
    {
      xl_btree_newroot xlrec;
      XLogRecPtr recptr;
      xl_btree_metadata md;

      XLogBeginInsert();
      XLogRegisterBuffer(0, rootbuf, REGBUF_WILL_INIT);
      XLogRegisterBuffer(2, metabuf, REGBUF_WILL_INIT | REGBUF_STANDARD);

      Assert(metad->btm_version >= BTREE_NOVAC_VERSION);
      md.version = metad->btm_version;
      md.root = rootblkno;
      md.level = 0;
      md.fastroot = rootblkno;
      md.fastlevel = 0;
      md.oldest_btpo_xact = InvalidTransactionId;
      md.last_cleanup_num_heap_tuples = -1.0;

      XLogRegisterBufData(2, (char *)&md, sizeof(xl_btree_metadata));

      xlrec.rootblk = rootblkno;
      xlrec.level = 0;

      XLogRegisterData((char *)&xlrec, SizeOfBtreeNewroot);

      recptr = XLogInsert(RM_BTREE_ID, XLOG_BTREE_NEWROOT);

      PageSetLSN(rootpage, recptr);
      PageSetLSN(metapg, recptr);
    }

    END_CRIT_SECTION();

       
                                                                         
                                                                          
                                   
       
    LockBuffer(rootbuf, BUFFER_LOCK_UNLOCK);
    LockBuffer(rootbuf, BT_READ);

                                                                       
    _bt_relbuf(rel, metabuf);
  }
  else
  {
    rootblkno = metad->btm_fastroot;
    Assert(rootblkno != P_NONE);
    rootlevel = metad->btm_fastlevel;

       
                                             
       
    rel->rd_amcache = MemoryContextAlloc(rel->rd_indexcxt, sizeof(BTMetaPageData));
    memcpy(rel->rd_amcache, metad, sizeof(BTMetaPageData));

       
                                                                      
                             
       
    rootbuf = metabuf;

    for (;;)
    {
      rootbuf = _bt_relandgetbuf(rel, rootbuf, rootblkno, BT_READ);
      rootpage = BufferGetPage(rootbuf);
      rootopaque = (BTPageOpaque)PageGetSpecialPointer(rootpage);

      if (!P_IGNORE(rootopaque))
      {
        break;
      }

                                                
      if (P_RIGHTMOST(rootopaque))
      {
        elog(ERROR, "no live root page found in index \"%s\"", RelationGetRelationName(rel));
      }
      rootblkno = rootopaque->btpo_next;
    }

                                                       
    if (rootopaque->btpo.level != rootlevel)
    {
      elog(ERROR, "root page %u of index \"%s\" has level %u, expected %u", rootblkno, RelationGetRelationName(rel), rootopaque->btpo.level, rootlevel);
    }
  }

     
                                                                            
                                                           
     
  return rootbuf;
}

   
                                                             
   
                                                                  
                                                         
   
                                                                              
                                                                            
                                                                              
                                                                           
                                                                           
                                                                              
                                                                              
   
Buffer
_bt_gettrueroot(Relation rel)
{
  Buffer metabuf;
  Page metapg;
  BTPageOpaque metaopaque;
  Buffer rootbuf;
  Page rootpage;
  BTPageOpaque rootopaque;
  BlockNumber rootblkno;
  uint32 rootlevel;
  BTMetaPageData *metad;

     
                                                                           
                                                                            
                                                                            
                                              
     
  if (rel->rd_amcache)
  {
    pfree(rel->rd_amcache);
  }
  rel->rd_amcache = NULL;

  metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_READ);
  metapg = BufferGetPage(metabuf);
  metaopaque = (BTPageOpaque)PageGetSpecialPointer(metapg);
  metad = BTPageGetMeta(metapg);

  if (!P_ISMETA(metaopaque) || metad->btm_magic != BTREE_MAGIC)
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" is not a btree", RelationGetRelationName(rel))));
  }

  if (metad->btm_version < BTREE_MIN_VERSION || metad->btm_version > BTREE_VERSION)
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("version mismatch in index \"%s\": file version %d, "
                                                             "current version %d, minimal supported version %d",
                                                          RelationGetRelationName(rel), metad->btm_version, BTREE_VERSION, BTREE_MIN_VERSION)));
  }

                                             
  if (metad->btm_root == P_NONE)
  {
    _bt_relbuf(rel, metabuf);
    return InvalidBuffer;
  }

  rootblkno = metad->btm_root;
  rootlevel = metad->btm_level;

     
                                                                    
                           
     
  rootbuf = metabuf;

  for (;;)
  {
    rootbuf = _bt_relandgetbuf(rel, rootbuf, rootblkno, BT_READ);
    rootpage = BufferGetPage(rootbuf);
    rootopaque = (BTPageOpaque)PageGetSpecialPointer(rootpage);

    if (!P_IGNORE(rootopaque))
    {
      break;
    }

                                              
    if (P_RIGHTMOST(rootopaque))
    {
      elog(ERROR, "no live root page found in index \"%s\"", RelationGetRelationName(rel));
    }
    rootblkno = rootopaque->btpo_next;
  }

                                                     
  if (rootopaque->btpo.level != rootlevel)
  {
    elog(ERROR, "root page %u of index \"%s\" has level %u, expected %u", rootblkno, RelationGetRelationName(rel), rootopaque->btpo.level, rootlevel);
  }

  return rootbuf;
}

   
                                                                   
   
                                                                       
                                                                           
                                     
   
                                                                          
                                                                        
                                           
   
int
_bt_getrootheight(Relation rel)
{
  BTMetaPageData *metad;

  if (rel->rd_amcache == NULL)
  {
    Buffer metabuf;

    metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_READ);
    metad = _bt_getmeta(rel, metabuf);

       
                                                                         
                                                                          
                                                                         
       
    if (metad->btm_root == P_NONE)
    {
      _bt_relbuf(rel, metabuf);
      return 0;
    }

       
                                             
       
    rel->rd_amcache = MemoryContextAlloc(rel->rd_indexcxt, sizeof(BTMetaPageData));
    memcpy(rel->rd_amcache, metad, sizeof(BTMetaPageData));
    _bt_relbuf(rel, metabuf);
  }

                       
  metad = (BTMetaPageData *)rel->rd_amcache;
                                                        
  Assert(metad->btm_magic == BTREE_MAGIC);
  Assert(metad->btm_version >= BTREE_MIN_VERSION);
  Assert(metad->btm_version <= BTREE_VERSION);
  Assert(metad->btm_fastroot != P_NONE);

  return metad->btm_fastlevel;
}

   
                                                             
   
                                                                       
                                                                        
                                                                           
                                                                       
                                                           
   
bool
_bt_heapkeyspace(Relation rel)
{
  BTMetaPageData *metad;

  if (rel->rd_amcache == NULL)
  {
    Buffer metabuf;

    metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_READ);
    metad = _bt_getmeta(rel, metabuf);

       
                                                                         
                                                                         
                                       
       
    if (metad->btm_root == P_NONE)
    {
      uint32 btm_version = metad->btm_version;

      _bt_relbuf(rel, metabuf);
      return btm_version > BTREE_NOVAC_VERSION;
    }

       
                                             
       
                                                                        
                                                                           
                                                                         
                                                                    
                 
       
    rel->rd_amcache = MemoryContextAlloc(rel->rd_indexcxt, sizeof(BTMetaPageData));
    memcpy(rel->rd_amcache, metad, sizeof(BTMetaPageData));
    _bt_relbuf(rel, metabuf);
  }

                       
  metad = (BTMetaPageData *)rel->rd_amcache;
                                                        
  Assert(metad->btm_magic == BTREE_MAGIC);
  Assert(metad->btm_version >= BTREE_MIN_VERSION);
  Assert(metad->btm_version <= BTREE_VERSION);
  Assert(metad->btm_fastroot != P_NONE);

  return metad->btm_version > BTREE_NOVAC_VERSION;
}

   
                                                                  
   
void
_bt_checkpage(Relation rel, Buffer buf)
{
  Page page = BufferGetPage(buf);

     
                                                           
                                                                         
                                                                         
                    
     
  if (PageIsNew(page))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains unexpected zero page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
  }

     
                                                          
     
  if (PageGetSpecialSize(page) != MAXALIGN(sizeof(BTPageOpaqueData)))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains corrupted page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
  }
}

   
                                         
   
static void
_bt_log_reuse_page(Relation rel, BlockNumber blkno, TransactionId latestRemovedXid)
{
  xl_btree_reuse_page xlrec_reuse;

     
                                                                          
                                                                             
                                     
     

                  
  xlrec_reuse.node = rel->rd_node;
  xlrec_reuse.block = blkno;
  xlrec_reuse.latestRemovedXid = latestRemovedXid;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec_reuse, SizeOfBtreeReusePage);

  XLogInsert(RM_BTREE_ID, XLOG_BTREE_REUSE_PAGE);
}

   
                                                                   
   
                                                                     
                                             
   
                                                                  
                                                                  
                                                             
                                                                   
   
Buffer
_bt_getbuf(Relation rel, BlockNumber blkno, int access)
{
  Buffer buf;

  if (blkno != P_NEW)
  {
                                                
    buf = ReadBuffer(rel, blkno);
    LockBuffer(buf, access);
    _bt_checkpage(rel, buf);
  }
  else
  {
    bool needLock;
    Page page;

    Assert(access == BT_WRITE);

       
                                                     
       
                                                                           
                                                                         
                                                                         
                                                  
       
                                                                          
                                                                         
                                                                          
                                                                         
                                                                          
                                                                           
                                                                           
                                                                           
                                      
       
                                                                     
                                                                        
                                                                        
                                                                         
                                                  
       
    for (;;)
    {
      blkno = GetFreeIndexPage(rel);
      if (blkno == InvalidBlockNumber)
      {
        break;
      }
      buf = ReadBuffer(rel, blkno);
      if (ConditionalLockBuffer(buf))
      {
        page = BufferGetPage(buf);
        if (_bt_page_recyclable(page))
        {
             
                                                                    
                                                                    
                                                                   
                                                                   
                                                                    
                                                                     
                     
             
          if (XLogStandbyInfoActive() && RelationNeedsWAL(rel) && !PageIsNew(page))
          {
            BTPageOpaque opaque = (BTPageOpaque)PageGetSpecialPointer(page);

            _bt_log_reuse_page(rel, blkno, opaque->btpo.xact);
          }

                                                              
          _bt_pageinit(page, BufferGetPageSize(buf));
          return buf;
        }
        elog(DEBUG2, "FSM returned nonrecyclable page");
        _bt_relbuf(rel, buf);
      }
      else
      {
        elog(DEBUG2, "FSM returned nonlockable page");
                                                 
        ReleaseBuffer(buf);
      }
    }

       
                                        
       
                                                                           
                                                                       
                                                                      
                                                  
       
    needLock = !RELATION_IS_LOCAL(rel);

    if (needLock)
    {
      LockRelationForExtension(rel, ExclusiveLock);
    }

    buf = ReadBuffer(rel, P_NEW);

                                         
    LockBuffer(buf, BT_WRITE);

       
                                                                        
                                                                        
                                                                          
                                                                
       
    if (needLock)
    {
      UnlockRelationForExtension(rel, ExclusiveLock);
    }

                                                     
    page = BufferGetPage(buf);
    Assert(PageIsNew(page));
    _bt_pageinit(page, BufferGetPageSize(buf));
  }

                                           
  return buf;
}

   
                                                                      
   
                                                                     
                                                                          
                                                                          
            
   
                                                                          
                                                                         
                                                                              
                                                                  
   
Buffer
_bt_relandgetbuf(Relation rel, Buffer obuf, BlockNumber blkno, int access)
{
  Buffer buf;

  Assert(blkno != P_NEW);
  if (BufferIsValid(obuf))
  {
    LockBuffer(obuf, BUFFER_LOCK_UNLOCK);
  }
  buf = ReleaseAndReadBuffer(obuf, rel, blkno);
  LockBuffer(buf, access);
  _bt_checkpage(rel, buf);
  return buf;
}

   
                                            
   
                                             
   
void
_bt_relbuf(Relation rel, Buffer buf)
{
  UnlockReleaseBuffer(buf);
}

   
                                            
   
                                                                   
                                
   
void
_bt_pageinit(Page page, Size size)
{
  PageInit(page, size, sizeof(BTPageOpaqueData));
}

   
                                                            
   
                                                                      
                                                                            
                                                                           
                                                                              
                 
   
bool
_bt_page_recyclable(Page page)
{
  BTPageOpaque opaque;

     
                                                                             
                                                                            
                                                                             
                                  
     
  if (PageIsNew(page))
  {
    return true;
  }

     
                                                                     
                       
     
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  if (P_ISDELETED(opaque) && TransactionIdPrecedes(opaque->btpo.xact, RecentGlobalXmin))
  {
    return true;
  }
  return false;
}

   
                                                   
   
                                                                          
                                                                          
                                   
   
                                                                          
                                                                           
   
                                                                         
                                                                        
                                                                       
                                                                          
                                                                     
                                                                             
                                                                          
                                                                     
                           
   
void
_bt_delitems_vacuum(Relation rel, Buffer buf, OffsetNumber *itemnos, int nitems, BlockNumber lastBlockVacuumed)
{
  Page page = BufferGetPage(buf);
  BTPageOpaque opaque;

                                                  
  START_CRIT_SECTION();

                    
  if (nitems > 0)
  {
    PageIndexMultiDelete(page, itemnos, nitems);
  }

     
                                                                         
                                           
     
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  opaque->btpo_cycleid = 0;

     
                                                                     
                                                                             
                                                                             
                                                                         
                                                          
     
  opaque->btpo_flags &= ~BTP_HAS_GARBAGE;

  MarkBufferDirty(buf);

                  
  if (RelationNeedsWAL(rel))
  {
    XLogRecPtr recptr;
    xl_btree_vacuum xlrec_vacuum;

    xlrec_vacuum.lastBlockVacuumed = lastBlockVacuumed;

    XLogBeginInsert();
    XLogRegisterBuffer(0, buf, REGBUF_STANDARD);
    XLogRegisterData((char *)&xlrec_vacuum, SizeOfBtreeVacuum);

       
                                                                          
                                                                       
                               
       
    if (nitems > 0)
    {
      XLogRegisterBufData(0, (char *)itemnos, nitems * sizeof(OffsetNumber));
    }

    recptr = XLogInsert(RM_BTREE_ID, XLOG_BTREE_VACUUM);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();
}

   
                                                                
   
                                              
   
                                                                          
                                                                           
   
                                                                            
                                                                          
                                     
   
void
_bt_delitems_delete(Relation rel, Buffer buf, OffsetNumber *itemnos, int nitems, Relation heapRel)
{
  Page page = BufferGetPage(buf);
  BTPageOpaque opaque;
  TransactionId latestRemovedXid = InvalidTransactionId;

                                                          
  Assert(nitems > 0);

  if (XLogStandbyInfoActive() && RelationNeedsWAL(rel))
  {
    latestRemovedXid = index_compute_xid_horizon_for_tuples(rel, heapRel, buf, itemnos, nitems);
  }

                                                  
  START_CRIT_SECTION();

                    
  PageIndexMultiDelete(page, itemnos, nitems);

     
                                                                          
                                           
     

     
                                                                     
                                                                             
                                                                             
                                                                         
                                                          
     
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  opaque->btpo_flags &= ~BTP_HAS_GARBAGE;

  MarkBufferDirty(buf);

                  
  if (RelationNeedsWAL(rel))
  {
    XLogRecPtr recptr;
    xl_btree_delete xlrec_delete;

    xlrec_delete.latestRemovedXid = latestRemovedXid;
    xlrec_delete.nitems = nitems;

    XLogBeginInsert();
    XLogRegisterBuffer(0, buf, REGBUF_STANDARD);
    XLogRegisterData((char *)&xlrec_delete, SizeOfBtreeDelete);

       
                                                                          
                                                                     
               
       
    XLogRegisterData((char *)itemnos, nitems * sizeof(OffsetNumber));

    recptr = XLogInsert(RM_BTREE_ID, XLOG_BTREE_DELETE);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();
}

   
                                                                
   
static bool
_bt_is_page_halfdead(Relation rel, BlockNumber blk)
{
  Buffer buf;
  Page page;
  BTPageOpaque opaque;
  bool result;

  buf = _bt_getbuf(rel, blk, BT_READ);
  page = BufferGetPage(buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  result = P_ISHALFDEAD(opaque);
  _bt_relbuf(rel, buf);

  return result;
}

   
                                                                            
                                                                           
                                                                               
                                                                            
   
                                                                      
                                                                               
                                                                           
                                                                               
                                                                               
                                                                               
                  
   
                                                                             
                                                                              
                                                                            
                                                                      
                                                                           
                                                                       
                                                                            
   
                                                                              
                                                                      
                                                                           
                                                                           
         
   
static bool
_bt_lock_branch_parent(Relation rel, BlockNumber child, BTStack stack, Buffer *topparent, OffsetNumber *topoff, BlockNumber *target, BlockNumber *rightsib)
{
  BlockNumber parent;
  OffsetNumber poffset, maxoff;
  Buffer pbuf;
  Page page;
  BTPageOpaque opaque;
  BlockNumber leftsib;

     
                                                                            
                                                                    
                                                                          
                                                               
     
  stack->bts_btentry = child;
  pbuf = _bt_getstackbuf(rel, stack);
  if (pbuf == InvalidBuffer)
  {
       
                                                                          
                                                                      
                                                                      
                                                    
       
                                                                         
                                                                          
                                                                        
                                                                           
                                
       
    ereport(LOG, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg_internal("failed to re-find parent key in index \"%s\" for deletion target page %u", RelationGetRelationName(rel), child)));
    return false;
  }
  parent = stack->bts_blkno;
  poffset = stack->bts_offset;

  page = BufferGetPage(pbuf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  maxoff = PageGetMaxOffsetNumber(page);

     
                                                                       
                                              
     
  if (poffset >= maxoff)
  {
                                 
    if (poffset == P_FIRSTDATAKEY(opaque))
    {
         
                                                                       
                                                                      
                                                
         
      if (P_RIGHTMOST(opaque) || P_ISROOT(opaque) || P_INCOMPLETE_SPLIT(opaque))
      {
        _bt_relbuf(rel, pbuf);
        return false;
      }

      *target = parent;
      *rightsib = opaque->btpo_next;
      leftsib = opaque->btpo_prev;

      _bt_relbuf(rel, pbuf);

         
                                                                        
                                                                       
                                                                   
                                                  
         
      if (leftsib != P_NONE)
      {
        Buffer lbuf;
        Page lpage;
        BTPageOpaque lopaque;

        lbuf = _bt_getbuf(rel, leftsib, BT_READ);
        lpage = BufferGetPage(lbuf);
        lopaque = (BTPageOpaque)PageGetSpecialPointer(lpage);

           
                                                                   
                                                                       
                                                                      
                                                                  
                                                        
           
        if (lopaque->btpo_next == parent && P_INCOMPLETE_SPLIT(lopaque))
        {
          _bt_relbuf(rel, lbuf);
          return false;
        }
        _bt_relbuf(rel, lbuf);
      }

      return _bt_lock_branch_parent(rel, parent, stack->bts_parent, topparent, topoff, target, rightsib);
    }
    else
    {
                            
      _bt_relbuf(rel, pbuf);
      return false;
    }
  }
  else
  {
                                                
    *topparent = pbuf;
    *topoff = poffset;
    return true;
  }
}

   
                                                                           
   
                                                                             
                                                                             
                                                                             
                                                                             
                                                                    
   
                                                                               
                                                                             
                                                                         
                                                                              
                                                                             
                                                                     
   
                                                                         
                                                                           
                                                                            
                                                                          
            
   
                                                                        
                                                                              
                                 
   
                                                                       
                                                                        
               
   
uint32
_bt_pagedel(Relation rel, Buffer leafbuf, TransactionId *oldestBtpoXact)
{
  uint32 ndeleted = 0;
  BlockNumber rightsib;
  bool rightsib_empty;
  Page page;
  BTPageOpaque opaque;

     
                                                                          
                                                                 
     
  BlockNumber scanblkno = BufferGetBlockNumber(leafbuf);

     
                                                                           
                                                                   
                               
     
                                                                      
                                                                         
                                                               
     
  BTStack stack = NULL;

  for (;;)
  {
    page = BufferGetPage(leafbuf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);

       
                                                                           
                                                        
       
                                                                          
                                                                        
                                                                       
                                                                       
                         
       
    Assert(!P_ISDELETED(opaque));
    if (!P_ISLEAF(opaque) || P_ISDELETED(opaque))
    {
         
                                                                        
                                                                        
                                                                         
                                                                     
                                                                         
                                                                     
                                                                       
                                                                         
                      
         
      if (P_ISHALFDEAD(opaque))
      {
        ereport(LOG, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains a half-dead internal page", RelationGetRelationName(rel)), errhint("This can be caused by an interrupted VACUUM in version 9.3 or older, before upgrade. Please REINDEX it.")));
      }

      if (P_ISDELETED(opaque))
      {
        ereport(LOG, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg_internal("found deleted block %u while following right link from block %u in index \"%s\"", BufferGetBlockNumber(leafbuf), scanblkno, RelationGetRelationName(rel))));
      }

      _bt_relbuf(rel, leafbuf);
      return ndeleted;
    }

       
                                                                         
                                                                           
                                                               
       
                                                                          
                                                                         
                                             
       
                                                                         
                                                                        
                                                                        
                                                                        
                                                                           
                                                                          
                                                                           
                                            
       
    if (P_RIGHTMOST(opaque) || P_ISROOT(opaque) || P_FIRSTDATAKEY(opaque) <= PageGetMaxOffsetNumber(page) || P_INCOMPLETE_SPLIT(opaque))
    {
                                                        
      Assert(!P_ISHALFDEAD(opaque));

      _bt_relbuf(rel, leafbuf);
      return ndeleted;
    }

       
                                                                       
                                                                           
                     
       
    if (!P_ISHALFDEAD(opaque))
    {
         
                                                                       
                                                                      
                                                                     
                                                                        
                                                                         
         
                                                                     
                              
         
      if (!stack)
      {
        BTScanInsert itup_key;
        ItemId itemid;
        IndexTuple targetkey;
        Buffer lbuf;
        BlockNumber leftsib;

        itemid = PageGetItemId(page, P_HIKEY);
        targetkey = CopyIndexTuple((IndexTuple)PageGetItem(page, itemid));

        leftsib = opaque->btpo_prev;

           
                                                                   
                                 
           
        LockBuffer(leafbuf, BUFFER_LOCK_UNLOCK);

           
                                                                      
                                                                 
                                                               
                                                             
           
        if (!P_LEFTMOST(opaque))
        {
          BTPageOpaque lopaque;
          Page lpage;

          lbuf = _bt_getbuf(rel, leftsib, BT_READ);
          lpage = BufferGetPage(lbuf);
          lopaque = (BTPageOpaque)PageGetSpecialPointer(lpage);

             
                                                                    
                                                                
                                                                 
                                                                     
                                      
             
          if (lopaque->btpo_next == BufferGetBlockNumber(leafbuf) && P_INCOMPLETE_SPLIT(lopaque))
          {
            ReleaseBuffer(leafbuf);
            _bt_relbuf(rel, lbuf);
            return ndeleted;
          }
          _bt_relbuf(rel, lbuf);
        }

                                                                        
        itup_key = _bt_mkscankey(rel, targetkey);
                                                                      
        itup_key->pivotsearch = true;
        stack = _bt_search(rel, itup_key, &lbuf, BT_READ, NULL);
                                                         
        _bt_relbuf(rel, lbuf);

           
                                                                  
                                                                  
                                                               
                                         
           
        LockBuffer(leafbuf, BT_WRITE);
        continue;
      }

         
                                                                     
                                                                 
                                                                      
                                                                         
                                 
         
      Assert(P_ISLEAF(opaque) && !P_IGNORE(opaque));
      if (!_bt_mark_page_halfdead(rel, leafbuf, stack))
      {
        _bt_relbuf(rel, leafbuf);
        return ndeleted;
      }
    }

       
                                                       
                                                                          
                                                                        
       
                                                                        
                                                                  
       
    rightsib_empty = false;
    Assert(P_ISLEAF(opaque) && P_ISHALFDEAD(opaque));
    while (P_ISHALFDEAD(opaque))
    {
                                                            
      if (!_bt_unlink_halfdead_page(rel, leafbuf, scanblkno, &rightsib_empty, oldestBtpoXact, &ndeleted))
      {
                                                              
        return ndeleted;
      }
    }

    Assert(P_ISLEAF(opaque) && P_ISDELETED(opaque));
    Assert(TransactionIdFollowsOrEquals(opaque->btpo.xact, *oldestBtpoXact));

    rightsib = opaque->btpo_next;

    _bt_relbuf(rel, leafbuf);

       
                                                                     
                                        
       
    CHECK_FOR_INTERRUPTS();

       
                                                                         
                                                                          
                                                                     
                                                                          
                                                                       
                                                                     
                                                                 
       
    if (!rightsib_empty)
    {
      break;
    }

    leafbuf = _bt_getbuf(rel, rightsib, BT_WRITE);
  }

  return ndeleted;
}

   
                                                                        
                                                              
   
static bool
_bt_mark_page_halfdead(Relation rel, Buffer leafbuf, BTStack stack)
{
  BlockNumber leafblkno;
  BlockNumber leafrightsib;
  BlockNumber target;
  BlockNumber rightsib;
  ItemId itemid;
  Page page;
  BTPageOpaque opaque;
  Buffer topparent;
  OffsetNumber topoff;
  OffsetNumber nextoffset;
  IndexTuple itup;
  IndexTupleData trunctuple;

  page = BufferGetPage(leafbuf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  Assert(!P_RIGHTMOST(opaque) && !P_ISROOT(opaque) && !P_ISDELETED(opaque) && !P_ISHALFDEAD(opaque) && P_ISLEAF(opaque) && P_FIRSTDATAKEY(opaque) > PageGetMaxOffsetNumber(page));

     
                                    
     
  leafblkno = BufferGetBlockNumber(leafbuf);
  leafrightsib = opaque->btpo_next;

     
                                                                             
                                                                         
                                                                           
                                                                      
                                                                      
                                                                         
                                  
     
  if (_bt_is_page_halfdead(rel, leafrightsib))
  {
    elog(DEBUG1, "could not delete page %u because its right sibling %u is half-dead", leafblkno, leafrightsib);
    return false;
  }

     
                                                                          
                                                                             
                                                                          
                                                                          
                                                             
     
                                                                             
                                                                         
                                                                       
                                                                        
                                                                   
                                                                        
                                                                           
                                                                          
                                                                 
     
  rightsib = leafrightsib;
  target = leafblkno;
  if (!_bt_lock_branch_parent(rel, leafblkno, stack, &topparent, &topoff, &target, &rightsib))
  {
    return false;
  }

     
                                                                            
                                                                            
                                                                      
                                                     
     
                                                           
                                                                       
                                                                          
                                
     
  page = BufferGetPage(topparent);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

#ifdef USE_ASSERT_CHECKING
  itemid = PageGetItemId(page, topoff);
  itup = (IndexTuple)PageGetItem(page, itemid);
  Assert(BTreeInnerTupleGetDownLink(itup) == target);
#endif

  nextoffset = OffsetNumberNext(topoff);
  itemid = PageGetItemId(page, nextoffset);
  itup = (IndexTuple)PageGetItem(page, itemid);
  if (BTreeInnerTupleGetDownLink(itup) != rightsib)
  {
    elog(ERROR, "right sibling %u of block %u is not next child %u of block %u in index \"%s\"", rightsib, target, BTreeInnerTupleGetDownLink(itup), BufferGetBlockNumber(topparent), RelationGetRelationName(rel));
  }

     
                                                                           
                    
     
  PredicateLockPageCombine(rel, leafblkno, leafrightsib);

                                                  
  START_CRIT_SECTION();

     
                                                                        
                                                                           
                                                                             
                                
     
  page = BufferGetPage(topparent);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  itemid = PageGetItemId(page, topoff);
  itup = (IndexTuple)PageGetItem(page, itemid);
  BTreeInnerTupleSetDownLink(itup, rightsib);

  nextoffset = OffsetNumberNext(topoff);
  PageIndexTupleDelete(page, nextoffset);

     
                                                                         
                                                                            
                               
     
  page = BufferGetPage(leafbuf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  opaque->btpo_flags |= BTP_HALF_DEAD;

  PageIndexTupleDelete(page, P_HIKEY);
  Assert(PageGetMaxOffsetNumber(page) == 0);
  MemSet(&trunctuple, 0, sizeof(IndexTupleData));
  trunctuple.t_info = sizeof(IndexTupleData);
  if (target != leafblkno)
  {
    BTreeTupleSetTopParent(&trunctuple, target);
  }
  else
  {
    BTreeTupleSetTopParent(&trunctuple, InvalidBlockNumber);
  }

  if (PageAddItem(page, (Item)&trunctuple, sizeof(IndexTupleData), P_HIKEY, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR, "could not add dummy high key to half-dead page");
  }

                                                 
  MarkBufferDirty(topparent);
  MarkBufferDirty(leafbuf);

                  
  if (RelationNeedsWAL(rel))
  {
    xl_btree_mark_page_halfdead xlrec;
    XLogRecPtr recptr;

    xlrec.poffset = topoff;
    xlrec.leafblk = leafblkno;
    if (target != leafblkno)
    {
      xlrec.topparent = target;
    }
    else
    {
      xlrec.topparent = InvalidBlockNumber;
    }

    XLogBeginInsert();
    XLogRegisterBuffer(0, leafbuf, REGBUF_WILL_INIT);
    XLogRegisterBuffer(1, topparent, REGBUF_STANDARD);

    page = BufferGetPage(leafbuf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    xlrec.leftblk = opaque->btpo_prev;
    xlrec.rightblk = opaque->btpo_next;

    XLogRegisterData((char *)&xlrec, SizeOfBtreeMarkPageHalfDead);

    recptr = XLogInsert(RM_BTREE_ID, XLOG_BTREE_MARK_PAGE_HALFDEAD);

    page = BufferGetPage(topparent);
    PageSetLSN(page, recptr);
    page = BufferGetPage(leafbuf);
    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  _bt_relbuf(rel, topparent);
  return true;
}

   
                                                                   
   
                                                                             
                                                                            
                                                                          
                         
   
                                                                             
                                                                             
                                                                             
                                                                            
                                                                            
                                                                              
                
   
                                                                         
                                                                               
                                                                           
                                                                           
          
   
                                                                              
                                                                           
                                                                           
                                                             
   
static bool
_bt_unlink_halfdead_page(Relation rel, Buffer leafbuf, BlockNumber scanblkno, bool *rightsib_empty, TransactionId *oldestBtpoXact, uint32 *ndeleted)
{
  BlockNumber leafblkno = BufferGetBlockNumber(leafbuf);
  BlockNumber leafleftsib;
  BlockNumber leafrightsib;
  BlockNumber target;
  BlockNumber leftsib;
  BlockNumber rightsib;
  Buffer lbuf = InvalidBuffer;
  Buffer buf;
  Buffer rbuf;
  Buffer metabuf = InvalidBuffer;
  Page metapg = NULL;
  BTMetaPageData *metad = NULL;
  ItemId itemid;
  Page page;
  BTPageOpaque opaque;
  bool rightsib_is_rightmost;
  int targetlevel;
  IndexTuple leafhikey;
  BlockNumber nextchild;

  page = BufferGetPage(leafbuf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

  Assert(P_ISLEAF(opaque) && P_ISHALFDEAD(opaque));

     
                                                    
     
  itemid = PageGetItemId(page, P_HIKEY);
  leafhikey = (IndexTuple)PageGetItem(page, itemid);
  leafleftsib = opaque->btpo_prev;
  leafrightsib = opaque->btpo_next;

  LockBuffer(leafbuf, BUFFER_LOCK_UNLOCK);

     
                                                                   
                                      
     
  CHECK_FOR_INTERRUPTS();

     
                                                                       
                                                                            
                                                                         
                              
     
  target = BTreeTupleGetTopParent(leafhikey);

  if (target != InvalidBlockNumber)
  {
    Assert(target != leafblkno);

                                                                     
    buf = _bt_getbuf(rel, target, BT_READ);
    page = BufferGetPage(buf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    leftsib = opaque->btpo_prev;
    targetlevel = opaque->btpo.level;

       
                                                                        
                      
       
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
  }
  else
  {
    target = leafblkno;

    buf = leafbuf;
    leftsib = leafleftsib;
    targetlevel = 0;
  }

     
                                                                        
                                                                          
     
                                                                          
                                                                          
                                                                            
                                                                             
                                                                         
                                                                          
                                                                      
             
     
  if (target != leafblkno)
  {
    LockBuffer(leafbuf, BT_WRITE);
  }
  if (leftsib != P_NONE)
  {
    lbuf = _bt_getbuf(rel, leftsib, BT_WRITE);
    page = BufferGetPage(lbuf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    while (P_ISDELETED(opaque) || opaque->btpo_next != target)
    {
                               
      leftsib = opaque->btpo_next;
      _bt_relbuf(rel, lbuf);

         
                                                                         
                                                               
                                                                  
                                             
         

      if (leftsib == P_NONE)
      {
        elog(LOG, "no left sibling (concurrent deletion?) of block %u in \"%s\"", target, RelationGetRelationName(rel));
        if (target != leafblkno)
        {
                                                                     
          ReleaseBuffer(buf);
          _bt_relbuf(rel, leafbuf);
        }
        else
        {
                                             
          ReleaseBuffer(leafbuf);
        }
        return false;
      }
      lbuf = _bt_getbuf(rel, leftsib, BT_WRITE);
      page = BufferGetPage(lbuf);
      opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    }
  }
  else
  {
    lbuf = InvalidBuffer;
  }

     
                                                                             
                                                                            
           
     
  LockBuffer(buf, BT_WRITE);
  page = BufferGetPage(buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);

     
                                                                             
                                                                             
                                                
     
  if (P_RIGHTMOST(opaque) || P_ISROOT(opaque) || P_ISDELETED(opaque))
  {
    elog(ERROR, "half-dead page changed status unexpectedly in block %u of index \"%s\"", target, RelationGetRelationName(rel));
  }
  if (opaque->btpo_prev != leftsib)
  {
    elog(ERROR, "left link changed unexpectedly in block %u of index \"%s\"", target, RelationGetRelationName(rel));
  }

  if (target == leafblkno)
  {
    if (P_FIRSTDATAKEY(opaque) <= PageGetMaxOffsetNumber(page) || !P_ISLEAF(opaque) || !P_ISHALFDEAD(opaque))
    {
      elog(ERROR, "half-dead page changed status unexpectedly in block %u of index \"%s\"", target, RelationGetRelationName(rel));
    }
    nextchild = InvalidBlockNumber;
  }
  else
  {
    if (P_FIRSTDATAKEY(opaque) != PageGetMaxOffsetNumber(page) || P_ISLEAF(opaque))
    {
      elog(ERROR, "half-dead page changed status unexpectedly in block %u of index \"%s\"", target, RelationGetRelationName(rel));
    }

                                                              
    itemid = PageGetItemId(page, P_FIRSTDATAKEY(opaque));
    nextchild = BTreeInnerTupleGetDownLink((IndexTuple)PageGetItem(page, itemid));
    if (nextchild == leafblkno)
    {
      nextchild = InvalidBlockNumber;
    }
  }

     
                                                      
     
  rightsib = opaque->btpo_next;
  rbuf = _bt_getbuf(rel, rightsib, BT_WRITE);
  page = BufferGetPage(rbuf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  if (opaque->btpo_prev != target)
  {
    elog(ERROR,
        "right sibling's left-link doesn't match: "
        "block %u links to %u instead of expected %u in index \"%s\"",
        rightsib, opaque->btpo_prev, target, RelationGetRelationName(rel));
  }
  rightsib_is_rightmost = P_RIGHTMOST(opaque);
  *rightsib_empty = (P_FIRSTDATAKEY(opaque) > PageGetMaxOffsetNumber(page));

     
                                                                          
                                                                             
                                                                             
                                                                     
     
                                                                             
                                                          
     
                                                                            
                    
     
  if (leftsib == P_NONE && rightsib_is_rightmost)
  {
    page = BufferGetPage(rbuf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    if (P_RIGHTMOST(opaque))
    {
                                                           
      metabuf = _bt_getbuf(rel, BTREE_METAPAGE, BT_WRITE);
      metapg = BufferGetPage(metabuf);
      metad = BTPageGetMeta(metapg);

         
                                                                      
                                                                     
                                           
         
      if (metad->btm_fastlevel > targetlevel + 1)
      {
                              
        _bt_relbuf(rel, metabuf);
        metabuf = InvalidBuffer;
      }
    }
  }

     
                                       
     

                                                  
  START_CRIT_SECTION();

     
                                                                          
                                                                          
                                       
     
  if (BufferIsValid(lbuf))
  {
    page = BufferGetPage(lbuf);
    opaque = (BTPageOpaque)PageGetSpecialPointer(page);
    Assert(opaque->btpo_next == target);
    opaque->btpo_next = rightsib;
  }
  page = BufferGetPage(rbuf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  Assert(opaque->btpo_prev == target);
  opaque->btpo_prev = leftsib;

     
                                                                           
                                                                         
             
     
  if (target != leafblkno)
  {
    BTreeTupleSetTopParent(leafhikey, nextchild);
  }

     
                                                                        
                                                                           
                                                                          
                                                                        
                                                                            
                                                                             
                                                                             
                   
     
  page = BufferGetPage(buf);
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  Assert(P_ISHALFDEAD(opaque) || !P_ISLEAF(opaque));
  opaque->btpo_flags &= ~BTP_HALF_DEAD;
  opaque->btpo_flags |= BTP_DELETED;
  opaque->btpo.xact = ReadNewTransactionId();

                                          
  if (BufferIsValid(metabuf))
  {
                                    
    if (metad->btm_version < BTREE_NOVAC_VERSION)
    {
      _bt_upgrademetapage(metapg);
    }
    metad->btm_fastroot = rightsib;
    metad->btm_fastlevel = targetlevel;
    MarkBufferDirty(metabuf);
  }

                                                 
  MarkBufferDirty(rbuf);
  MarkBufferDirty(buf);
  if (BufferIsValid(lbuf))
  {
    MarkBufferDirty(lbuf);
  }
  if (target != leafblkno)
  {
    MarkBufferDirty(leafbuf);
  }

                  
  if (RelationNeedsWAL(rel))
  {
    xl_btree_unlink_page xlrec;
    xl_btree_metadata xlmeta;
    uint8 xlinfo;
    XLogRecPtr recptr;

    XLogBeginInsert();

    XLogRegisterBuffer(0, buf, REGBUF_WILL_INIT);
    if (BufferIsValid(lbuf))
    {
      XLogRegisterBuffer(1, lbuf, REGBUF_STANDARD);
    }
    XLogRegisterBuffer(2, rbuf, REGBUF_STANDARD);
    if (target != leafblkno)
    {
      XLogRegisterBuffer(3, leafbuf, REGBUF_WILL_INIT);
    }

                                           
    xlrec.leftsib = leftsib;
    xlrec.rightsib = rightsib;
    xlrec.btpo_xact = opaque->btpo.xact;

                                                                           
    xlrec.leafleftsib = leafleftsib;
    xlrec.leafrightsib = leafrightsib;
    xlrec.topparent = nextchild;

    XLogRegisterData((char *)&xlrec, SizeOfBtreeUnlinkPage);

    if (BufferIsValid(metabuf))
    {
      XLogRegisterBuffer(4, metabuf, REGBUF_WILL_INIT | REGBUF_STANDARD);

      Assert(metad->btm_version >= BTREE_NOVAC_VERSION);
      xlmeta.version = metad->btm_version;
      xlmeta.root = metad->btm_root;
      xlmeta.level = metad->btm_level;
      xlmeta.fastroot = metad->btm_fastroot;
      xlmeta.fastlevel = metad->btm_fastlevel;
      xlmeta.oldest_btpo_xact = metad->btm_oldest_btpo_xact;
      xlmeta.last_cleanup_num_heap_tuples = metad->btm_last_cleanup_num_heap_tuples;

      XLogRegisterBufData(4, (char *)&xlmeta, sizeof(xl_btree_metadata));
      xlinfo = XLOG_BTREE_UNLINK_PAGE_META;
    }
    else
    {
      xlinfo = XLOG_BTREE_UNLINK_PAGE;
    }

    recptr = XLogInsert(RM_BTREE_ID, xlinfo);

    if (BufferIsValid(metabuf))
    {
      PageSetLSN(metapg, recptr);
    }
    page = BufferGetPage(rbuf);
    PageSetLSN(page, recptr);
    page = BufferGetPage(buf);
    PageSetLSN(page, recptr);
    if (BufferIsValid(lbuf))
    {
      page = BufferGetPage(lbuf);
      PageSetLSN(page, recptr);
    }
    if (target != leafblkno)
    {
      page = BufferGetPage(leafbuf);
      PageSetLSN(page, recptr);
    }
  }

  END_CRIT_SECTION();

                        
  if (BufferIsValid(metabuf))
  {
    _bt_relbuf(rel, metabuf);
  }

                        
  if (BufferIsValid(lbuf))
  {
    _bt_relbuf(rel, lbuf);
  }
  _bt_relbuf(rel, rbuf);

  if (!TransactionIdIsValid(*oldestBtpoXact) || TransactionIdPrecedes(opaque->btpo.xact, *oldestBtpoXact))
  {
    *oldestBtpoXact = opaque->btpo.xact;
  }

     
                                                                           
                                                                     
                       
     
  if (target <= scanblkno)
  {
    (*ndeleted)++;
  }

     
                                                                           
                  
     
  if (target != leafblkno)
  {
    _bt_relbuf(rel, buf);
  }

  return true;
}
