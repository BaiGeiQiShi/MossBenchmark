                                                                            
   
          
                                                                   
   
   
                                                                         
                                                                        
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/gist_private.h"
#include "access/gistscan.h"
#include "catalog/pg_collation.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "nodes/execnodes.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/memutils.h"
#include "utils/rel.h"

                                    
static void
gistfixsplit(GISTInsertState *state, GISTSTATE *giststate);
static bool
gistinserttuple(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple tuple, OffsetNumber oldoffnum);
static bool
gistinserttuples(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple *tuples, int ntup, OffsetNumber oldoffnum, Buffer leftchild, Buffer rightchild, bool unlockbuf, bool unlockleftchild);
static void
gistfinishsplit(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, List *splitinfo, bool releasebuf);
static void
gistprunepage(Relation rel, Page page, Buffer buffer, Relation heapRel);

#define ROTATEDIST(d)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    SplitedPageLayout *tmp = (SplitedPageLayout *)palloc0(sizeof(SplitedPageLayout));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    tmp->block.blkno = InvalidBlockNumber;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    tmp->buffer = InvalidBuffer;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    tmp->next = (d);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (d) = tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

   
                                                                              
                  
   
Datum
gisthandler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = 0;
  amroutine->amsupport = GISTNProcs;
  amroutine->amcanorder = false;
  amroutine->amcanorderbyop = true;
  amroutine->amcanbackward = false;
  amroutine->amcanunique = false;
  amroutine->amcanmulticol = true;
  amroutine->amoptionalkey = true;
  amroutine->amsearcharray = false;
  amroutine->amsearchnulls = true;
  amroutine->amstorage = true;
  amroutine->amclusterable = true;
  amroutine->ampredlocks = true;
  amroutine->amcanparallel = false;
  amroutine->amcaninclude = true;
  amroutine->amkeytype = InvalidOid;

  amroutine->ambuild = gistbuild;
  amroutine->ambuildempty = gistbuildempty;
  amroutine->aminsert = gistinsert;
  amroutine->ambulkdelete = gistbulkdelete;
  amroutine->amvacuumcleanup = gistvacuumcleanup;
  amroutine->amcanreturn = gistcanreturn;
  amroutine->amcostestimate = gistcostestimate;
  amroutine->amoptions = gistoptions;
  amroutine->amproperty = gistproperty;
  amroutine->ambuildphasename = NULL;
  amroutine->amvalidate = gistvalidate;
  amroutine->ambeginscan = gistbeginscan;
  amroutine->amrescan = gistrescan;
  amroutine->amgettuple = gistgettuple;
  amroutine->amgetbitmap = gistgetbitmap;
  amroutine->amendscan = gistendscan;
  amroutine->ammarkpos = NULL;
  amroutine->amrestrpos = NULL;
  amroutine->amestimateparallelscan = NULL;
  amroutine->aminitparallelscan = NULL;
  amroutine->amparallelrescan = NULL;

  PG_RETURN_POINTER(amroutine);
}

   
                                                                    
                                                               
                                                                 
                                                                    
                                                                 
                      
   
MemoryContext
createTempGistContext(void)
{
  return AllocSetContextCreate(CurrentMemoryContext, "GiST temporary context", ALLOCSET_DEFAULT_SIZES);
}

   
                                                                            
   
void
gistbuildempty(Relation index)
{
  Buffer buffer;

                                
  buffer = ReadBufferExtended(index, INIT_FORKNUM, P_NEW, RBM_NORMAL, NULL);
  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

                                  
  START_CRIT_SECTION();
  GISTInitBuffer(buffer, F_LEAF);
  MarkBufferDirty(buffer);
  log_newpage_buffer(buffer, true);
  END_CRIT_SECTION();

                                     
  UnlockReleaseBuffer(buffer);
}

   
                                                   
   
                                                                        
                                                                          
   
bool
gistinsert(Relation r, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  GISTSTATE *giststate = (GISTSTATE *)indexInfo->ii_AmCache;
  IndexTuple itup;
  MemoryContext oldCxt;

                                                                  
  if (giststate == NULL)
  {
    oldCxt = MemoryContextSwitchTo(indexInfo->ii_Context);
    giststate = initGISTstate(r);
    giststate->tempCxt = createTempGistContext();
    indexInfo->ii_AmCache = (void *)giststate;
    MemoryContextSwitchTo(oldCxt);
  }

  oldCxt = MemoryContextSwitchTo(giststate->tempCxt);

  itup = gistFormTuple(giststate, r, values, isnull, true                              );
  itup->t_tid = *ht_ctid;

  gistdoinsert(r, itup, 0, giststate, heapRel, false);

               
  MemoryContextSwitchTo(oldCxt);
  MemoryContextReset(giststate->tempCxt);

  return false;
}

   
                                                                            
                                                                             
                                                   
   
                                                                         
                                                                                
                                                                    
   
                                                                         
                                                                             
                                                                              
                                                               
   
                                                                       
                                                                           
                                                                    
                                                                         
                                                                             
                                                                         
                                                                         
                                      
   
                                                                         
                                                                             
                                               
   
                                                            
   
bool
gistplacetopage(Relation rel, Size freespace, GISTSTATE *giststate, Buffer buffer, IndexTuple *itup, int ntup, OffsetNumber oldoffnum, BlockNumber *newblkno, Buffer leftchildbuf, List **splitinfo, bool markfollowright, Relation heapRel, bool is_build)
{
  BlockNumber blkno = BufferGetBlockNumber(buffer);
  Page page = BufferGetPage(buffer);
  bool is_leaf = (GistPageIsLeaf(page)) ? true : false;
  XLogRecPtr recptr;
  int i;
  bool is_split;

     
                                                                        
                                                                           
                                                                            
                                                                          
                                                                          
                                                                             
               
     
  if (GistFollowRight(page))
  {
    elog(ERROR, "concurrent GiST page split was incomplete");
  }

                                                    
  Assert(!GistPageIsDeleted(page));

  *splitinfo = NIL;

     
                                                                            
                                                                           
                                                                         
                      
     
                                                                          
                                                                            
                                                                            
     
  is_split = gistnospace(page, itup, ntup, oldoffnum, freespace);

     
                                                                        
                  
     
  if (is_split && GistPageIsLeaf(page) && GistPageHasGarbage(page))
  {
    gistprunepage(rel, page, buffer, heapRel);
    is_split = gistnospace(page, itup, ntup, oldoffnum, freespace);
  }

  if (is_split)
  {
                                
    IndexTuple *itvec;
    int tlen;
    SplitedPageLayout *dist = NULL, *ptr;
    BlockNumber oldrlink = InvalidBlockNumber;
    GistNSN oldnsn = 0;
    SplitedPageLayout rootpg;
    bool is_rootsplit;
    int npage;

    is_rootsplit = (blkno == GIST_ROOT_BLKNO);

       
                                                                           
                                               
       
    itvec = gistextractpage(page, &tlen);
    if (OffsetNumberIsValid(oldoffnum))
    {
                                                    
      int pos = oldoffnum - FirstOffsetNumber;

      tlen--;
      if (pos != tlen)
      {
        memmove(itvec + pos, itvec + pos + 1, sizeof(IndexTuple) * (tlen - pos));
      }
    }
    itvec = gistjoinvector(itvec, &tlen, itup, ntup);
    dist = gistSplit(rel, page, itvec, tlen, giststate);

       
                                                       
       
    npage = 0;
    for (ptr = dist; ptr; ptr = ptr->next)
    {
      npage++;
    }
                                                                    
    if (is_rootsplit)
    {
      npage++;
    }
    if (npage > GIST_MAX_SPLIT_PAGES)
    {
      elog(ERROR, "GiST page split into too many halves (%d, maximum %d)", npage, GIST_MAX_SPLIT_PAGES);
    }

       
                                                                       
                                                                           
                                               
       
                                                                       
                                                                  
                                         
       
    ptr = dist;
    if (!is_rootsplit)
    {
                                      
      oldrlink = GistPageGetOpaque(page)->rightlink;
      oldnsn = GistPageGetNSN(page);

      dist->buffer = buffer;
      dist->block.blkno = BufferGetBlockNumber(buffer);
      dist->page = PageGetTempPageCopySpecial(BufferGetPage(buffer));

                                         
      GistPageGetOpaque(dist->page)->flags = (is_leaf) ? F_LEAF : 0;

      ptr = ptr->next;
    }
    for (; ptr; ptr = ptr->next)
    {
                             
      ptr->buffer = gistNewBuffer(rel);
      GISTInitBuffer(ptr->buffer, (is_leaf) ? F_LEAF : 0);
      ptr->page = BufferGetPage(ptr->buffer);
      ptr->block.blkno = BufferGetBlockNumber(ptr->buffer);
      PredicateLockPageSplit(rel, BufferGetBlockNumber(buffer), BufferGetBlockNumber(ptr->buffer));
    }

       
                                                                          
                                
       
    for (ptr = dist; ptr; ptr = ptr->next)
    {
      ItemPointerSetBlockNumber(&(ptr->itup->t_tid), ptr->block.blkno);
      GistTupleSetValid(ptr->itup);
    }

       
                                                                        
                                                                          
                                                                           
       
    if (is_rootsplit)
    {
      IndexTuple *downlinks;
      int ndownlinks = 0;
      int i;

      rootpg.buffer = buffer;
      rootpg.page = PageGetTempPageCopySpecial(BufferGetPage(rootpg.buffer));
      GistPageGetOpaque(rootpg.page)->flags = 0;

                                                 
      for (ptr = dist; ptr; ptr = ptr->next)
      {
        ndownlinks++;
      }
      downlinks = palloc(sizeof(IndexTuple) * ndownlinks);
      for (i = 0, ptr = dist; ptr; ptr = ptr->next)
      {
        downlinks[i++] = ptr->itup;
      }

      rootpg.block.blkno = GIST_ROOT_BLKNO;
      rootpg.block.num = ndownlinks;
      rootpg.list = gistfillitupvec(downlinks, ndownlinks, &(rootpg.lenlist));
      rootpg.itup = NULL;

      rootpg.next = dist;
      dist = &rootpg;
    }
    else
    {
                                                       
      for (ptr = dist; ptr; ptr = ptr->next)
      {
        GISTPageSplitInfo *si = palloc(sizeof(GISTPageSplitInfo));

        si->buf = ptr->buffer;
        si->downlink = ptr->itup;
        *splitinfo = lappend(*splitinfo, si);
      }
    }

       
                                                                          
                                                   
       
    for (ptr = dist; ptr; ptr = ptr->next)
    {
      char *data = (char *)(ptr->list);

      for (i = 0; i < ptr->block.num; i++)
      {
        IndexTuple thistup = (IndexTuple)data;

        if (PageAddItem(ptr->page, (Item)data, IndexTupleSize(thistup), i + FirstOffsetNumber, false, false) == InvalidOffsetNumber)
        {
          elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(rel));
        }

           
                                                                       
                                         
           
        if (newblkno && ItemPointerEquals(&thistup->t_tid, &(*itup)->t_tid))
        {
          *newblkno = ptr->block.blkno;
        }

        data += IndexTupleSize(thistup);
      }

                             
      if (ptr->next && ptr->block.blkno != GIST_ROOT_BLKNO)
      {
        GistPageGetOpaque(ptr->page)->rightlink = ptr->next->block.blkno;
      }
      else
      {
        GistPageGetOpaque(ptr->page)->rightlink = oldrlink;
      }

         
                                                                    
                                                                      
                                                                       
                                                                        
                                                                     
                  
         
      if (ptr->next && !is_rootsplit && markfollowright)
      {
        GistMarkFollowRight(ptr->page);
      }
      else
      {
        GistClearFollowRight(ptr->page);
      }

         
                                                             
                                                                
                                                      
         
      GistPageSetNSN(ptr->page, oldnsn);
    }

       
                                                                    
                                                                     
                                                                      
       
    if (!is_build && RelationNeedsWAL(rel))
    {
      XLogEnsureRecordSpace(npage, 1 + npage * 2);
    }

    START_CRIT_SECTION();

       
                                                                          
                                              
       
    for (ptr = dist; ptr; ptr = ptr->next)
    {
      MarkBufferDirty(ptr->buffer);
    }
    if (BufferIsValid(leftchildbuf))
    {
      MarkBufferDirty(leftchildbuf);
    }

       
                                                                         
                                                        
       
    PageRestoreTempPage(dist->page, BufferGetPage(dist->buffer));
    dist->page = BufferGetPage(dist->buffer);

       
                             
       
                                                                        
                                                                         
                                                                    
                                                                          
                                                                          
                                                                  
       
    if (is_build)
    {
      recptr = GistBuildLSN;
    }
    else
    {
      if (RelationNeedsWAL(rel))
      {
        recptr = gistXLogSplit(is_leaf, dist, oldrlink, oldnsn, leftchildbuf, markfollowright);
      }
      else
      {
        recptr = gistGetFakeLSN(rel);
      }
    }

    for (ptr = dist; ptr; ptr = ptr->next)
    {
      PageSetLSN(ptr->page, recptr);
    }

       
                                                   
       
                                                                     
                                                                          
                                                                
       
    if (is_rootsplit)
    {
      for (ptr = dist->next; ptr; ptr = ptr->next)
      {
        UnlockReleaseBuffer(ptr->buffer);
      }
    }
  }
  else
  {
       
                                                     
       
    START_CRIT_SECTION();

       
                                                                     
                                                               
       
    if (OffsetNumberIsValid(oldoffnum))
    {
      if (ntup == 1)
      {
                                                                     
        if (!PageIndexTupleOverwrite(page, oldoffnum, (Item)*itup, IndexTupleSize(*itup)))
        {
          elog(ERROR, "failed to add item to index page in \"%s\"", RelationGetRelationName(rel));
        }
      }
      else
      {
                                                          
        PageIndexTupleDelete(page, oldoffnum);
        gistfillbuffer(page, itup, ntup, InvalidOffsetNumber);
      }
    }
    else
    {
                                                         
      gistfillbuffer(page, itup, ntup, InvalidOffsetNumber);
    }

    MarkBufferDirty(buffer);

    if (BufferIsValid(leftchildbuf))
    {
      MarkBufferDirty(leftchildbuf);
    }

    if (is_build)
    {
      recptr = GistBuildLSN;
    }
    else
    {
      if (RelationNeedsWAL(rel))
      {
        OffsetNumber ndeloffs = 0, deloffs[1];

        if (OffsetNumberIsValid(oldoffnum))
        {
          deloffs[0] = oldoffnum;
          ndeloffs = 1;
        }

        recptr = gistXLogUpdate(buffer, deloffs, ndeloffs, itup, ntup, leftchildbuf);
      }
      else
      {
        recptr = gistGetFakeLSN(rel);
      }
    }
    PageSetLSN(page, recptr);

    if (newblkno)
    {
      *newblkno = blkno;
    }
  }

     
                                                                     
                                                                             
                                                                        
                                      
     
                                                                          
                                                                           
                                                                          
                                                                           
                                                                          
                           
     
  if (BufferIsValid(leftchildbuf))
  {
    Page leftpg = BufferGetPage(leftchildbuf);

    GistPageSetNSN(leftpg, recptr);
    GistClearFollowRight(leftpg);

    PageSetLSN(leftpg, recptr);
  }

  END_CRIT_SECTION();

  return is_split;
}

   
                                                                      
                                                                       
                                                         
   
void
gistdoinsert(Relation r, IndexTuple itup, Size freespace, GISTSTATE *giststate, Relation heapRel, bool is_build)
{
  ItemId iid;
  IndexTuple idxtuple;
  GISTInsertStack firststack;
  GISTInsertStack *stack;
  GISTInsertState state;
  bool xlocked = false;

  memset(&state, 0, sizeof(GISTInsertState));
  state.freespace = freespace;
  state.r = r;
  state.heapRel = heapRel;
  state.is_build = is_build;

                           
  firststack.blkno = GIST_ROOT_BLKNO;
  firststack.lsn = 0;
  firststack.retry_from_parent = false;
  firststack.parent = NULL;
  firststack.downlinkoffnum = InvalidOffsetNumber;
  state.stack = stack = &firststack;

     
                                                                       
                                                                        
                                                                          
                   
     
  for (;;)
  {
       
                                                                          
                                                                         
                                                                       
                            
       
    while (stack->retry_from_parent)
    {
      if (xlocked)
      {
        LockBuffer(stack->buffer, GIST_UNLOCK);
      }
      xlocked = false;
      ReleaseBuffer(stack->buffer);
      state.stack = stack = stack->parent;
    }

    if (XLogRecPtrIsInvalid(stack->lsn))
    {
      stack->buffer = ReadBuffer(state.r, stack->blkno);
    }

       
                                                                          
                                                 
       
    if (!xlocked)
    {
      LockBuffer(stack->buffer, GIST_SHARE);
      gistcheckpage(state.r, stack->buffer);
    }

    stack->page = (Page)BufferGetPage(stack->buffer);
    stack->lsn = xlocked ? PageGetLSN(stack->page) : BufferGetLSNAtomic(stack->buffer);
    Assert(!RelationNeedsWAL(state.r) || !XLogRecPtrIsInvalid(stack->lsn));

       
                                                                         
                                                                           
                 
       
    if (GistFollowRight(stack->page))
    {
      if (!xlocked)
      {
        LockBuffer(stack->buffer, GIST_UNLOCK);
        LockBuffer(stack->buffer, GIST_EXCLUSIVE);
        xlocked = true;
                                                                   
        if (!GistFollowRight(stack->page))
        {
          continue;
        }
      }
      gistfixsplit(&state, giststate);

      UnlockReleaseBuffer(stack->buffer);
      xlocked = false;
      state.stack = stack = stack->parent;
      continue;
    }

    if ((stack->blkno != GIST_ROOT_BLKNO && stack->parent->lsn < GistPageGetNSN(stack->page)) || GistPageIsDeleted(stack->page))
    {
         
                                                                
                                                                      
                                                                     
                                  
         
      UnlockReleaseBuffer(stack->buffer);
      xlocked = false;
      state.stack = stack = stack->parent;
      continue;
    }

    if (!GistPageIsLeaf(stack->page))
    {
         
                                                                     
                                                                     
         
      BlockNumber childblkno;
      IndexTuple newtup;
      GISTInsertStack *item;
      OffsetNumber downlinkoffnum;

      downlinkoffnum = gistchoose(state.r, stack->page, itup, giststate);
      iid = PageGetItemId(stack->page, downlinkoffnum);
      idxtuple = (IndexTuple)PageGetItem(stack->page, iid);
      childblkno = ItemPointerGetBlockNumber(&(idxtuple->t_tid));

         
                                                                   
         
      if (GistTupleIsInvalid(idxtuple))
      {
        ereport(ERROR, (errmsg("index \"%s\" contains an inner tuple marked as invalid", RelationGetRelationName(r)), errdetail("This is caused by an incomplete page split at crash recovery before upgrading to PostgreSQL 9.1."), errhint("Please REINDEX it.")));
      }

         
                                                                  
                                                                         
         
      newtup = gistgetadjusted(state.r, idxtuple, itup, giststate);
      if (newtup)
      {
           
                                                                       
                                                   
           
        if (!xlocked)
        {
          LockBuffer(stack->buffer, GIST_UNLOCK);
          LockBuffer(stack->buffer, GIST_EXCLUSIVE);
          xlocked = true;
          stack->page = (Page)BufferGetPage(stack->buffer);

          if (PageGetLSN(stack->page) != stack->lsn)
          {
                                                                  
            continue;
          }
        }

           
                             
           
                                                                  
                                                                       
                                                                     
                                                                      
                                                                    
                  
           
        if (gistinserttuple(&state, stack, giststate, newtup, downlinkoffnum))
        {
             
                                                                     
                                                                 
                                                                 
                   
             
          if (stack->blkno != GIST_ROOT_BLKNO)
          {
            UnlockReleaseBuffer(stack->buffer);
            xlocked = false;
            state.stack = stack = stack->parent;
          }
          continue;
        }
      }
      LockBuffer(stack->buffer, GIST_UNLOCK);
      xlocked = false;

                                       
      item = (GISTInsertStack *)palloc0(sizeof(GISTInsertStack));
      item->blkno = childblkno;
      item->parent = stack;
      item->downlinkoffnum = downlinkoffnum;
      state.stack = stack = item;
    }
    else
    {
         
                                                                      
                                                                         
                                                                  
         

         
                                                                         
                                                 
         
      if (!xlocked)
      {
        LockBuffer(stack->buffer, GIST_UNLOCK);
        LockBuffer(stack->buffer, GIST_EXCLUSIVE);
        xlocked = true;
        stack->page = (Page)BufferGetPage(stack->buffer);
        stack->lsn = PageGetLSN(stack->page);

        if (stack->blkno == GIST_ROOT_BLKNO)
        {
             
                                                                    
                                                             
             
          if (!GistPageIsLeaf(stack->page))
          {
               
                                                                  
                                                 
               
            LockBuffer(stack->buffer, GIST_UNLOCK);
            xlocked = false;
            continue;
          }

             
                                                                 
                                                              
             
        }
        else if ((GistFollowRight(stack->page) || stack->parent->lsn < GistPageGetNSN(stack->page)) || GistPageIsDeleted(stack->page))
        {
             
                                                                
                                                   
             
          UnlockReleaseBuffer(stack->buffer);
          xlocked = false;
          state.stack = stack = stack->parent;
          continue;
        }
      }

                                                                         

      gistinserttuple(&state, stack, giststate, itup, InvalidOffsetNumber);
      LockBuffer(stack->buffer, GIST_UNLOCK);

                                                               
      for (; stack; stack = stack->parent)
      {
        ReleaseBuffer(stack->buffer);
      }
      break;
    }
  }
}

   
                                                                             
   
                                                                          
                                                                            
                           
   
                                                                   
   
static GISTInsertStack *
gistFindPath(Relation r, BlockNumber child, OffsetNumber *downlinkoffnum)
{
  Page page;
  Buffer buffer;
  OffsetNumber i, maxoff;
  ItemId iid;
  IndexTuple idxtuple;
  List *fifo;
  GISTInsertStack *top, *ptr;
  BlockNumber blkno;

  top = (GISTInsertStack *)palloc0(sizeof(GISTInsertStack));
  top->blkno = GIST_ROOT_BLKNO;
  top->downlinkoffnum = InvalidOffsetNumber;

  fifo = list_make1(top);
  while (fifo != NIL)
  {
                                
    top = linitial(fifo);
    fifo = list_delete_first(fifo);

    buffer = ReadBuffer(r, top->blkno);
    LockBuffer(buffer, GIST_SHARE);
    gistcheckpage(r, buffer);
    page = (Page)BufferGetPage(buffer);

    if (GistPageIsLeaf(page))
    {
         
                                                                       
                                                  
         
      UnlockReleaseBuffer(buffer);
      break;
    }

                                                     
    Assert(!GistPageIsDeleted(page));

    top->lsn = BufferGetLSNAtomic(buffer);

       
                                                                      
                                                   
       
    if (GistFollowRight(page))
    {
      elog(ERROR, "concurrent GiST page split was incomplete");
    }

    if (top->parent && top->parent->lsn < GistPageGetNSN(page) && GistPageGetOpaque(page)->rightlink != InvalidBlockNumber                   )
    {
         
                                                                     
                                                                       
                              
         
                                                                    
                                                                        
                                                                        
                                                                    
                                  
         
      ptr = (GISTInsertStack *)palloc0(sizeof(GISTInsertStack));
      ptr->blkno = GistPageGetOpaque(page)->rightlink;
      ptr->downlinkoffnum = InvalidOffsetNumber;
      ptr->parent = top->parent;

      fifo = lcons(ptr, fifo);
    }

    maxoff = PageGetMaxOffsetNumber(page);

    for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
    {
      iid = PageGetItemId(page, i);
      idxtuple = (IndexTuple)PageGetItem(page, iid);
      blkno = ItemPointerGetBlockNumber(&(idxtuple->t_tid));
      if (blkno == child)
      {
                       
        UnlockReleaseBuffer(buffer);
        *downlinkoffnum = i;
        return top;
      }
      else
      {
                                                                   
        ptr = (GISTInsertStack *)palloc0(sizeof(GISTInsertStack));
        ptr->blkno = blkno;
        ptr->downlinkoffnum = i;
        ptr->parent = top;

        fifo = lappend(fifo, ptr);
      }
    }

    UnlockReleaseBuffer(buffer);
  }

  elog(ERROR, "failed to re-find parent of a page in index \"%s\", block %u", RelationGetRelationName(r), child);
  return NULL;                          
}

   
                                                                        
                                                                      
                                                                 
   
static void
gistFindCorrectParent(Relation r, GISTInsertStack *child)
{
  GISTInsertStack *parent = child->parent;

  gistcheckpage(r, parent->buffer);
  parent->page = (Page)BufferGetPage(parent->buffer);

                                                                       
  if (child->downlinkoffnum == InvalidOffsetNumber || parent->lsn != PageGetLSN(parent->page))
  {
                                                                  
    OffsetNumber i, maxoff;
    ItemId iid;
    IndexTuple idxtuple;
    GISTInsertStack *ptr;

    while (true)
    {
      maxoff = PageGetMaxOffsetNumber(parent->page);
      for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
      {
        iid = PageGetItemId(parent->page, i);
        idxtuple = (IndexTuple)PageGetItem(parent->page, iid);
        if (ItemPointerGetBlockNumber(&(idxtuple->t_tid)) == child->blkno)
        {
                            
          child->downlinkoffnum = i;
          return;
        }
      }

      parent->blkno = GistPageGetOpaque(parent->page)->rightlink;
      UnlockReleaseBuffer(parent->buffer);
      if (parent->blkno == InvalidBlockNumber)
      {
           
                                                                       
                                             
           
        break;
      }
      parent->buffer = ReadBuffer(r, parent->blkno);
      LockBuffer(parent->buffer, GIST_EXCLUSIVE);
      gistcheckpage(r, parent->buffer);
      parent->page = (Page)BufferGetPage(parent->buffer);
    }

       
                                                                       
                                     
       

    ptr = child->parent->parent;                                   
                                            
    while (ptr)
    {
      ReleaseBuffer(ptr->buffer);
      ptr = ptr->parent;
    }

                           
    ptr = parent = gistFindPath(r, child->blkno, &child->downlinkoffnum);

                                                
                                                             
    while (ptr)
    {
      ptr->buffer = ReadBuffer(r, ptr->blkno);
      ptr->page = (Page)BufferGetPage(ptr->buffer);
      ptr = ptr->parent;
    }

                                               
    child->parent = parent;

                                                  
    LockBuffer(child->parent->buffer, GIST_EXCLUSIVE);
    gistFindCorrectParent(r, child);
  }

  return;
}

   
                                                  
   
static IndexTuple
gistformdownlink(Relation rel, Buffer buf, GISTSTATE *giststate, GISTInsertStack *stack)
{
  Page page = BufferGetPage(buf);
  OffsetNumber maxoff;
  OffsetNumber offset;
  IndexTuple downlink = NULL;

  maxoff = PageGetMaxOffsetNumber(page);
  for (offset = FirstOffsetNumber; offset <= maxoff; offset = OffsetNumberNext(offset))
  {
    IndexTuple ituple = (IndexTuple)PageGetItem(page, PageGetItemId(page, offset));

    if (downlink == NULL)
    {
      downlink = CopyIndexTuple(ituple);
    }
    else
    {
      IndexTuple newdownlink;

      newdownlink = gistgetadjusted(rel, downlink, ituple, giststate);
      if (newdownlink)
      {
        downlink = newdownlink;
      }
    }
  }

     
                                                                          
                                                                             
                                                                            
                                                                            
                                                                             
                                                                             
                                                                             
                       
     
  if (!downlink)
  {
    ItemId iid;

    LockBuffer(stack->parent->buffer, GIST_EXCLUSIVE);
    gistFindCorrectParent(rel, stack);
    iid = PageGetItemId(stack->parent->page, stack->downlinkoffnum);
    downlink = (IndexTuple)PageGetItem(stack->parent->page, iid);
    downlink = CopyIndexTuple(downlink);
    LockBuffer(stack->parent->buffer, GIST_UNLOCK);
  }

  ItemPointerSetBlockNumber(&(downlink->t_tid), BufferGetBlockNumber(buf));
  GistTupleSetValid(downlink);

  return downlink;
}

   
                                                        
   
static void
gistfixsplit(GISTInsertState *state, GISTSTATE *giststate)
{
  GISTInsertStack *stack = state->stack;
  Buffer buf;
  Page page;
  List *splitinfo = NIL;

  elog(LOG, "fixing incomplete split in index \"%s\", block %u", RelationGetRelationName(state->r), stack->blkno);

  Assert(GistFollowRight(stack->page));
  Assert(OffsetNumberIsValid(stack->downlinkoffnum));

  buf = stack->buffer;

     
                                                                          
                                   
     
  for (;;)
  {
    GISTPageSplitInfo *si = palloc(sizeof(GISTPageSplitInfo));
    IndexTuple downlink;

    page = BufferGetPage(buf);

                                                          
    downlink = gistformdownlink(state->r, buf, giststate, stack);

    si->buf = buf;
    si->downlink = downlink;

    splitinfo = lappend(splitinfo, si);

    if (GistFollowRight(page))
    {
                          
      buf = ReadBuffer(state->r, GistPageGetOpaque(page)->rightlink);
      LockBuffer(buf, GIST_EXCLUSIVE);
    }
    else
    {
      break;
    }
  }

                            
  gistfinishsplit(state, stack, giststate, splitinfo, false);
}

   
                                                                            
                                                                             
                                                                        
   
                                                                               
                                                                            
                                                                          
              
   
static bool
gistinserttuple(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple tuple, OffsetNumber oldoffnum)
{
  return gistinserttuples(state, stack, giststate, &tuple, 1, oldoffnum, InvalidBuffer, InvalidBuffer, false, false);
}

                    
                                                                           
                                                                                
                                                                              
             
   
                                                                        
                                                                            
                                                                        
                              
   
                                                                           
                                                                             
                                                                       
                                                 
   
                                                                            
                                  
                                                                             
                     
                                                       
   
                                                                         
                                                                        
                                                             
   
static bool
gistinserttuples(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, IndexTuple *tuples, int ntup, OffsetNumber oldoffnum, Buffer leftchild, Buffer rightchild, bool unlockbuf, bool unlockleftchild)
{
  List *splitinfo;
  bool is_split;

     
                                                                       
                                         
     
  CheckForSerializableConflictIn(state->r, NULL, stack->buffer);

                                                                        
  is_split = gistplacetopage(state->r, state->freespace, giststate, stack->buffer, tuples, ntup, oldoffnum, NULL, leftchild, &splitinfo, true, state->heapRel, state->is_build);

     
                                                                          
                                                                      
             
     
  if (BufferIsValid(rightchild))
  {
    UnlockReleaseBuffer(rightchild);
  }
  if (BufferIsValid(leftchild) && unlockleftchild)
  {
    LockBuffer(leftchild, GIST_UNLOCK);
  }

     
                                                                           
                                                                    
                                                                       
                                                 
     
  if (splitinfo)
  {
    gistfinishsplit(state, stack, giststate, splitinfo, unlockbuf);
  }
  else if (unlockbuf)
  {
    LockBuffer(stack->buffer, GIST_UNLOCK);
  }

  return is_split;
}

   
                                                                            
                                                                         
                       
   
                                                                            
                                                                              
                                                                         
   
static void
gistfinishsplit(GISTInsertState *state, GISTInsertStack *stack, GISTSTATE *giststate, List *splitinfo, bool unlockbuf)
{
  ListCell *lc;
  List *reversed;
  GISTPageSplitInfo *right;
  GISTPageSplitInfo *left;
  IndexTuple tuples[2];

                                                   
  Assert(list_length(splitinfo) >= 2);

     
                                                                            
                                                                           
                                                                         
                                                                            
                                                      
     

                                                                   
  reversed = NIL;
  foreach (lc, splitinfo)
  {
    reversed = lcons(lfirst(lc), reversed);
  }

  LockBuffer(stack->parent->buffer, GIST_EXCLUSIVE);

     
                                                                           
                             
     
  while (list_length(reversed) > 2)
  {
    right = (GISTPageSplitInfo *)linitial(reversed);
    left = (GISTPageSplitInfo *)lsecond(reversed);

    gistFindCorrectParent(state->r, stack);
    if (gistinserttuples(state, stack->parent, giststate, &right->downlink, 1, InvalidOffsetNumber, left->buf, right->buf, false, false))
    {
         
                                                                   
                     
         
      stack->downlinkoffnum = InvalidOffsetNumber;
    }
                                                             
    reversed = list_delete_first(reversed);
  }

  right = (GISTPageSplitInfo *)linitial(reversed);
  left = (GISTPageSplitInfo *)lsecond(reversed);

     
                                                                         
                                                                        
                             
     
  tuples[0] = left->downlink;
  tuples[1] = right->downlink;
  gistFindCorrectParent(state->r, stack);
  if (gistinserttuples(state, stack->parent, giststate, tuples, 2, stack->downlinkoffnum, left->buf, right->buf, true,                    
          unlockbuf                                                                                                                                                   
          ))
  {
       
                                                                    
       
    stack->downlinkoffnum = InvalidOffsetNumber;
  }

  Assert(left->buf == stack->buffer);

     
                                                                      
                                                                         
                                                                          
                                                                           
                                                                            
                                                                        
                
     
                                                                        
                                                                             
                                                                       
                                                                           
                  
     
  stack->retry_from_parent = true;
}

   
                                                         
                                                                    
                                                         
   
SplitedPageLayout *
gistSplit(Relation r, Page page, IndexTuple *itup,                                
    int len, GISTSTATE *giststate)
{
  IndexTuple *lvectup, *rvectup;
  GistSplitVector v;
  int i;
  SplitedPageLayout *res = NULL;

                                                                         
  check_stack_depth();

                                                   
  Assert(len > 0);

     
                                                                          
           
     
  if (len == 1)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row size %zu exceeds maximum %zu for index \"%s\"", IndexTupleSize(itup[0]), GiSTPageSize, RelationGetRelationName(r))));
  }

  memset(v.spl_lisnull, true, sizeof(bool) * giststate->nonLeafTupdesc->natts);
  memset(v.spl_risnull, true, sizeof(bool) * giststate->nonLeafTupdesc->natts);
  gistSplitByKey(r, page, itup, len, giststate, &v, 0);

                                  
  lvectup = (IndexTuple *)palloc(sizeof(IndexTuple) * (len + 1));
  rvectup = (IndexTuple *)palloc(sizeof(IndexTuple) * (len + 1));

  for (i = 0; i < v.splitVector.spl_nleft; i++)
  {
    lvectup[i] = itup[v.splitVector.spl_left[i] - 1];
  }

  for (i = 0; i < v.splitVector.spl_nright; i++)
  {
    rvectup[i] = itup[v.splitVector.spl_right[i] - 1];
  }

                                                   
  if (!gistfitpage(rvectup, v.splitVector.spl_nright))
  {
    res = gistSplit(r, page, rvectup, v.splitVector.spl_nright, giststate);
  }
  else
  {
    ROTATEDIST(res);
    res->block.num = v.splitVector.spl_nright;
    res->list = gistfillitupvec(rvectup, v.splitVector.spl_nright, &(res->lenlist));
    res->itup = gistFormTuple(giststate, r, v.spl_rattr, v.spl_risnull, false);
  }

  if (!gistfitpage(lvectup, v.splitVector.spl_nleft))
  {
    SplitedPageLayout *resptr, *subres;

    resptr = subres = gistSplit(r, page, lvectup, v.splitVector.spl_nleft, giststate);

                                
    while (resptr->next)
    {
      resptr = resptr->next;
    }

    resptr->next = res;
    res = subres;
  }
  else
  {
    ROTATEDIST(res);
    res->block.num = v.splitVector.spl_nleft;
    res->list = gistfillitupvec(lvectup, v.splitVector.spl_nleft, &(res->lenlist));
    res->itup = gistFormTuple(giststate, r, v.spl_lattr, v.spl_lisnull, false);
  }

  return res;
}

   
                                                                   
   
GISTSTATE *
initGISTstate(Relation index)
{
  GISTSTATE *giststate;
  MemoryContext scanCxt;
  MemoryContext oldCxt;
  int i;

                                                              
  if (index->rd_att->natts > INDEX_MAX_KEYS)
  {
    elog(ERROR, "numberOfAttributes %d > %d", index->rd_att->natts, INDEX_MAX_KEYS);
  }

                                                              
  scanCxt = AllocSetContextCreate(CurrentMemoryContext, "GiST scan context", ALLOCSET_DEFAULT_SIZES);
  oldCxt = MemoryContextSwitchTo(scanCxt);

                                        
  giststate = (GISTSTATE *)palloc(sizeof(GISTSTATE));

  giststate->scanCxt = scanCxt;
  giststate->tempCxt = scanCxt;                                        
  giststate->leafTupdesc = index->rd_att;

     
                                                                            
                             
     
                                                                       
                                                                          
                                                                             
                                                                        
                                       
     
  giststate->nonLeafTupdesc = CreateTupleDescCopyConstr(index->rd_att);
  giststate->nonLeafTupdesc->natts = IndexRelationGetNumberOfKeyAttributes(index);

  for (i = 0; i < IndexRelationGetNumberOfKeyAttributes(index); i++)
  {
    fmgr_info_copy(&(giststate->consistentFn[i]), index_getprocinfo(index, i + 1, GIST_CONSISTENT_PROC), scanCxt);
    fmgr_info_copy(&(giststate->unionFn[i]), index_getprocinfo(index, i + 1, GIST_UNION_PROC), scanCxt);

                                                                 
    if (OidIsValid(index_getprocid(index, i + 1, GIST_COMPRESS_PROC)))
    {
      fmgr_info_copy(&(giststate->compressFn[i]), index_getprocinfo(index, i + 1, GIST_COMPRESS_PROC), scanCxt);
    }
    else
    {
      giststate->compressFn[i].fn_oid = InvalidOid;
    }

                                                                   
    if (OidIsValid(index_getprocid(index, i + 1, GIST_DECOMPRESS_PROC)))
    {
      fmgr_info_copy(&(giststate->decompressFn[i]), index_getprocinfo(index, i + 1, GIST_DECOMPRESS_PROC), scanCxt);
    }
    else
    {
      giststate->decompressFn[i].fn_oid = InvalidOid;
    }

    fmgr_info_copy(&(giststate->penaltyFn[i]), index_getprocinfo(index, i + 1, GIST_PENALTY_PROC), scanCxt);
    fmgr_info_copy(&(giststate->picksplitFn[i]), index_getprocinfo(index, i + 1, GIST_PICKSPLIT_PROC), scanCxt);
    fmgr_info_copy(&(giststate->equalFn[i]), index_getprocinfo(index, i + 1, GIST_EQUAL_PROC), scanCxt);

                                                                 
    if (OidIsValid(index_getprocid(index, i + 1, GIST_DISTANCE_PROC)))
    {
      fmgr_info_copy(&(giststate->distanceFn[i]), index_getprocinfo(index, i + 1, GIST_DISTANCE_PROC), scanCxt);
    }
    else
    {
      giststate->distanceFn[i].fn_oid = InvalidOid;
    }

                                                              
    if (OidIsValid(index_getprocid(index, i + 1, GIST_FETCH_PROC)))
    {
      fmgr_info_copy(&(giststate->fetchFn[i]), index_getprocinfo(index, i + 1, GIST_FETCH_PROC), scanCxt);
    }
    else
    {
      giststate->fetchFn[i].fn_oid = InvalidOid;
    }

       
                                                                           
                                                                           
                                                                        
                                                                    
                                                                  
                                                              
                                                                        
                                                                           
                                                                        
       
    if (OidIsValid(index->rd_indcollation[i]))
    {
      giststate->supportCollation[i] = index->rd_indcollation[i];
    }
    else
    {
      giststate->supportCollation[i] = DEFAULT_COLLATION_OID;
    }
  }

                                                     
  for (; i < index->rd_att->natts; i++)
  {
    giststate->consistentFn[i].fn_oid = InvalidOid;
    giststate->unionFn[i].fn_oid = InvalidOid;
    giststate->compressFn[i].fn_oid = InvalidOid;
    giststate->decompressFn[i].fn_oid = InvalidOid;
    giststate->penaltyFn[i].fn_oid = InvalidOid;
    giststate->picksplitFn[i].fn_oid = InvalidOid;
    giststate->equalFn[i].fn_oid = InvalidOid;
    giststate->distanceFn[i].fn_oid = InvalidOid;
    giststate->fetchFn[i].fn_oid = InvalidOid;
    giststate->supportCollation[i] = InvalidOid;
  }

  MemoryContextSwitchTo(oldCxt);

  return giststate;
}

void
freeGISTstate(GISTSTATE *giststate)
{
                                             
  MemoryContextDelete(giststate->scanCxt);
}

   
                                                                       
                                                       
   
static void
gistprunepage(Relation rel, Page page, Buffer buffer, Relation heapRel)
{
  OffsetNumber deletable[MaxIndexTuplesPerPage];
  int ndeletable = 0;
  OffsetNumber offnum, maxoff;
  TransactionId latestRemovedXid = InvalidTransactionId;

  Assert(GistPageIsLeaf(page));

     
                                                                           
                    
     
  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemId = PageGetItemId(page, offnum);

    if (ItemIdIsDead(itemId))
    {
      deletable[ndeletable++] = offnum;
    }
  }

  if (XLogStandbyInfoActive() && RelationNeedsWAL(rel))
  {
    latestRemovedXid = index_compute_xid_horizon_for_tuples(rel, heapRel, buffer, deletable, ndeletable);
  }

  if (ndeletable > 0)
  {
    START_CRIT_SECTION();

    PageIndexMultiDelete(page, deletable, ndeletable);

       
                                                                       
                                                                           
                                                                         
                                                                           
                                                                    
       
    GistClearPageHasGarbage(page);

    MarkBufferDirty(buffer);

                    
    if (RelationNeedsWAL(rel))
    {
      XLogRecPtr recptr;

      recptr = gistXLogDelete(buffer, deletable, ndeletable, latestRemovedXid);

      PageSetLSN(page, recptr);
    }
    else
    {
      PageSetLSN(page, gistGetFakeLSN(rel));
    }

    END_CRIT_SECTION();
  }

     
                                                                
                                                                          
                                                                          
               
     
}
