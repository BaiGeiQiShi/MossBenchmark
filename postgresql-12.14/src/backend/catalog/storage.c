                                                                            
   
             
                                                               
   
                                                                         
                                                                        
   
   
                  
                                   
   
         
                                                                  
                                        
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"

#include "access/visibilitymap.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "storage/freespace.h"
#include "storage/proc.h"
#include "storage/smgr.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                       
                                                                       
                                                                       
                                                                   
                                                                  
                                                                       
                                                             
   
                                                                         
                                                                              
                                                                         
                                                                             
                  
   
                                                                            
                                                                        
                           
   

typedef struct PendingRelDelete
{
  RelFileNode relnode;                                                     
  BackendId backend;                                                     
  bool atCommit;                                                            
  int nestLevel;                                                    
  struct PendingRelDelete *next;                       
} PendingRelDelete;

static PendingRelDelete *pendingDeletes = NULL;                          

   
                         
                                            
   
                                                                       
                                                                     
                           
   
                                                                          
                                                               
   
SMgrRelation
RelationCreateStorage(RelFileNode rnode, char relpersistence)
{
  PendingRelDelete *pending;
  SMgrRelation srel;
  BackendId backend;
  bool needs_wal;

  switch (relpersistence)
  {
  case RELPERSISTENCE_TEMP:
    backend = BackendIdForTempRelations();
    needs_wal = false;
    break;
  case RELPERSISTENCE_UNLOGGED:
    backend = InvalidBackendId;
    needs_wal = false;
    break;
  case RELPERSISTENCE_PERMANENT:
    backend = InvalidBackendId;
    needs_wal = true;
    break;
  default:
    elog(ERROR, "invalid relpersistence: %c", relpersistence);
    return NULL;                       
  }

  srel = smgropen(rnode, backend);
  smgrcreate(srel, MAIN_FORKNUM, false);

  if (needs_wal)
  {
    log_smgrcreate(&srel->smgr_rnode.node, MAIN_FORKNUM);
  }

                                                                
  pending = (PendingRelDelete *)MemoryContextAlloc(TopMemoryContext, sizeof(PendingRelDelete));
  pending->relnode = rnode;
  pending->backend = backend;
  pending->atCommit = false;                      
  pending->nestLevel = GetCurrentTransactionNestLevel();
  pending->next = pendingDeletes;
  pendingDeletes = pending;

  return srel;
}

   
                                                            
   
void
log_smgrcreate(const RelFileNode *rnode, ForkNumber forkNum)
{
  xl_smgr_create xlrec;

     
                                                     
     
  xlrec.rnode = *rnode;
  xlrec.forkNum = forkNum;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, sizeof(xlrec));
  XLogInsert(RM_SMGR_ID, XLOG_SMGR_CREATE | XLR_SPECIAL_REL_UPDATE);
}

   
                       
                                                                  
   
void
RelationDropStorage(Relation rel)
{
  PendingRelDelete *pending;

                                                                 
  pending = (PendingRelDelete *)MemoryContextAlloc(TopMemoryContext, sizeof(PendingRelDelete));
  pending->relnode = rel->rd_node;
  pending->backend = rel->rd_backend;
  pending->atCommit = true;                       
  pending->nestLevel = GetCurrentTransactionNestLevel();
  pending->next = pendingDeletes;
  pendingDeletes = pending;

     
                                                                           
                                                                           
                                                                            
                                                                    
                                                                             
                                                                           
                                         
     

  RelationCloseSmgr(rel);
}

   
                           
                                                    
   
                                                                        
                                                                           
                                                                  
                                                                     
                                                                        
                                                                             
                                    
   
                                                                               
                                             
   
                                                                    
   
void
RelationPreserveStorage(RelFileNode rnode, bool atCommit)
{
  PendingRelDelete *pending;
  PendingRelDelete *prev;
  PendingRelDelete *next;

  prev = NULL;
  for (pending = pendingDeletes; pending != NULL; pending = next)
  {
    next = pending->next;
    if (RelFileNodeEquals(rnode, pending->relnode) && pending->atCommit == atCommit)
    {
                                        
      if (prev)
      {
        prev->next = next;
      }
      else
      {
        pendingDeletes = next;
      }
      pfree(pending);
                                
    }
    else
    {
                                           
      prev = pending;
    }
  }
}

   
                    
                                                                      
   
                                                                          
            
   
void
RelationTruncate(Relation rel, BlockNumber nblocks)
{
  bool fsm;
  bool vm;
  SMgrRelation reln;

     
                                                                          
                                                         
     
  reln = RelationGetSmgr(rel);
  reln->smgr_targblock = InvalidBlockNumber;
  reln->smgr_fsm_nblocks = InvalidBlockNumber;
  reln->smgr_vm_nblocks = InvalidBlockNumber;

                                           
  fsm = smgrexists(reln, FSM_FORKNUM);
  if (fsm)
  {
    FreeSpaceMapTruncateRel(rel, nblocks);
  }

                                                     
  vm = smgrexists(RelationGetSmgr(rel), VISIBILITYMAP_FORKNUM);
  if (vm)
  {
    visibilitymap_truncate(rel, nblocks);
  }

     
                                                                            
                     
     
                                                                     
                                                                        
                                                                           
                                                                      
                                                                     
                                                                         
                                                                          
                     
     
  Assert(!MyProc->delayChkptEnd);
  MyProc->delayChkptEnd = true;

     
                                                                       
                                                                       
                                                                         
                                                                        
                                                                       
                                                                         
                    
     
  if (RelationNeedsWAL(rel))
  {
       
                                                         
       
    XLogRecPtr lsn;
    xl_smgr_truncate xlrec;

    xlrec.blkno = nblocks;
    xlrec.rnode = rel->rd_node;
    xlrec.flags = SMGR_TRUNCATE_ALL;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, sizeof(xlrec));

    lsn = XLogInsert(RM_SMGR_ID, XLOG_SMGR_TRUNCATE | XLR_SPECIAL_REL_UPDATE);

       
                                                                          
                                                                         
                                                                         
                                                                        
                                                        
       
    if (fsm || vm)
    {
      XLogFlush(lsn);
    }
  }

     
                                                                            
                                                                      
                                  
     
  smgrtruncate(RelationGetSmgr(rel), MAIN_FORKNUM, nblocks);

                                                                    
  MyProc->delayChkptEnd = false;
}

   
                                       
   
                                                                             
                                                                   
                                   
   
                                                                  
                                                    
                                                                          
                                                                              
                                          
   
void
RelationCopyStorage(SMgrRelation src, SMgrRelation dst, ForkNumber forkNum, char relpersistence)
{
  PGAlignedBlock buf;
  Page page;
  bool use_wal;
  bool copying_initfork;
  BlockNumber nblocks;
  BlockNumber blkno;

  page = (Page)buf.data;

     
                                                                       
                                                                            
                                    
     
  copying_initfork = relpersistence == RELPERSISTENCE_UNLOGGED && forkNum == INIT_FORKNUM;

     
                                                                          
                                            
     
  use_wal = XLogIsNeeded() && (relpersistence == RELPERSISTENCE_PERMANENT || copying_initfork);

  nblocks = smgrnblocks(src, forkNum);

  for (blkno = 0; blkno < nblocks; blkno++)
  {
                                                                     
    CHECK_FOR_INTERRUPTS();

    smgrread(src, forkNum, blkno, buf.data);

    if (!PageIsVerifiedExtended(page, blkno, PIV_LOG_WARNING | PIV_REPORT_STAT))
    {
         
                                                                        
                                                                      
                                                                 
                                                                        
                                                      
         
      char *relpath = relpathbackend(src->smgr_rnode.node, src->smgr_rnode.backend, forkNum);

      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("invalid page in block %u of relation %s", blkno, relpath)));
    }

       
                                                                           
                                                                          
              
       
    if (use_wal)
    {
      log_newpage(&dst->smgr_rnode.node, forkNum, blkno, page, false);
    }

    PageSetChecksumInplace(page, blkno);

       
                                                                         
                                                                           
                                           
       
    smgrextend(dst, forkNum, blkno, buf.data, true);
  }

     
                                                                           
                                                                      
                                                                          
              
     
                                                                           
                                                                          
                                                                             
                                                                             
                                                                             
                                                                       
                                                                             
                                                                  
     
  if (relpersistence == RELPERSISTENCE_PERMANENT || copying_initfork)
  {
    smgrimmedsync(dst, forkNum);
  }
}

   
                                                                           
   
                                                                        
                        
   
                                                                            
                                                                            
                                                                         
                                           
   
void
smgrDoPendingDeletes(bool isCommit)
{
  int nestLevel = GetCurrentTransactionNestLevel();
  PendingRelDelete *pending;
  PendingRelDelete *prev;
  PendingRelDelete *next;
  int nrels = 0, i = 0, maxrels = 0;
  SMgrRelation *srels = NULL;

  prev = NULL;
  for (pending = pendingDeletes; pending != NULL; pending = next)
  {
    next = pending->next;
    if (pending->nestLevel < nestLevel)
    {
                                                           
      prev = pending;
    }
    else
    {
                                                                 
      if (prev)
      {
        prev->next = next;
      }
      else
      {
        pendingDeletes = next;
      }
                                     
      if (pending->atCommit == isCommit)
      {
        SMgrRelation srel;

        srel = smgropen(pending->relnode, pending->backend);

                                                                 
        if (maxrels == 0)
        {
          maxrels = 8;
          srels = palloc(sizeof(SMgrRelation) * maxrels);
        }
        else if (maxrels <= nrels)
        {
          maxrels *= 2;
          srels = repalloc(srels, sizeof(SMgrRelation) * maxrels);
        }

        srels[nrels++] = srel;
      }
                                               
      pfree(pending);
                                
    }
  }

  if (nrels > 0)
  {
    smgrdounlinkall(srels, nrels, false);

    for (i = 0; i < nrels; i++)
    {
      smgrclose(srels[i]);
    }

    pfree(srels);
  }
}

   
                                                                              
   
                                                                          
                                                                     
                                                                 
   
                                                                               
                                                                             
                                                                              
                                                                             
                                                                            
                            
   
                                                                          
                                
   
int
smgrGetPendingDeletes(bool forCommit, RelFileNode **ptr)
{
  int nestLevel = GetCurrentTransactionNestLevel();
  int nrels;
  RelFileNode *rptr;
  PendingRelDelete *pending;

  nrels = 0;
  for (pending = pendingDeletes; pending != NULL; pending = pending->next)
  {
    if (pending->nestLevel >= nestLevel && pending->atCommit == forCommit && pending->backend == InvalidBackendId)
    {
      nrels++;
    }
  }
  if (nrels == 0)
  {
    *ptr = NULL;
    return 0;
  }
  rptr = (RelFileNode *)palloc(nrels * sizeof(RelFileNode));
  *ptr = rptr;
  for (pending = pendingDeletes; pending != NULL; pending = pending->next)
  {
    if (pending->nestLevel >= nestLevel && pending->atCommit == forCommit && pending->backend == InvalidBackendId)
    {
      *rptr = pending->relnode;
      rptr++;
    }
  }
  return nrels;
}

   
                                                           
   
                                                                           
                                                                       
                                                
   
void
PostPrepare_smgr(void)
{
  PendingRelDelete *pending;
  PendingRelDelete *next;

  for (pending = pendingDeletes; pending != NULL; pending = next)
  {
    next = pending->next;
    pendingDeletes = next;
                                             
    pfree(pending);
  }
}

   
                                                              
   
                                                                             
   
void
AtSubCommit_smgr(void)
{
  int nestLevel = GetCurrentTransactionNestLevel();
  PendingRelDelete *pending;

  for (pending = pendingDeletes; pending != NULL; pending = pending->next)
  {
    if (pending->nestLevel >= nestLevel)
    {
      pending->nestLevel = nestLevel - 1;
    }
  }
}

   
                                                            
   
                                                                
                                                                    
                                   
   
void
AtSubAbort_smgr(void)
{
  smgrDoPendingDeletes(false);
}

void
smgr_redo(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                  
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_SMGR_CREATE)
  {
    xl_smgr_create *xlrec = (xl_smgr_create *)XLogRecGetData(record);
    SMgrRelation reln;

    reln = smgropen(xlrec->rnode, InvalidBackendId);
    smgrcreate(reln, xlrec->forkNum, true);
  }
  else if (info == XLOG_SMGR_TRUNCATE)
  {
    xl_smgr_truncate *xlrec = (xl_smgr_truncate *)XLogRecGetData(record);
    SMgrRelation reln;
    Relation rel;

    reln = smgropen(xlrec->rnode, InvalidBackendId);

       
                                                                         
                                                                   
                                                                           
                                                  
       
    smgrcreate(reln, MAIN_FORKNUM, true);

       
                                                                          
                                                                         
                                                                      
                                                                      
                                                                           
                                                                      
       
                                                                           
                                                                           
                                                                          
                                                                          
                                                                       
                                         
       
    XLogFlush(lsn);

    if ((xlrec->flags & SMGR_TRUNCATE_HEAP) != 0)
    {
      smgrtruncate(reln, MAIN_FORKNUM, xlrec->blkno);

                                          
      XLogTruncateRelation(xlrec->rnode, MAIN_FORKNUM, xlrec->blkno);
    }

                                 
    rel = CreateFakeRelcacheEntry(xlrec->rnode);

    if ((xlrec->flags & SMGR_TRUNCATE_FSM) != 0 && smgrexists(reln, FSM_FORKNUM))
    {
      FreeSpaceMapTruncateRel(rel, xlrec->blkno);
    }
    if ((xlrec->flags & SMGR_TRUNCATE_VM) != 0 && smgrexists(reln, VISIBILITYMAP_FORKNUM))
    {
      visibilitymap_truncate(rel, xlrec->blkno);
    }

    FreeFakeRelcacheEntry(rel);
  }
  else
  {
    elog(PANIC, "smgr_redo: unknown op code %u", info);
  }
}
