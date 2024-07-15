                                                                            
   
          
                                                          
   
                                                                   
               
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "commands/tablespace.h"
#include "lib/ilist.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/md.h"
#include "storage/smgr.h"
#include "utils/hsearch.h"
#include "utils/inval.h"

   
                                                                       
                                                                           
                                                                           
                                                                        
                                                                          
                                                                          
                                                                          
                                                  
   
typedef struct f_smgr
{
  void (*smgr_init)(void);                      
  void (*smgr_shutdown)(void);                  
  void (*smgr_close)(SMgrRelation reln, ForkNumber forknum);
  void (*smgr_create)(SMgrRelation reln, ForkNumber forknum, bool isRedo);
  bool (*smgr_exists)(SMgrRelation reln, ForkNumber forknum);
  void (*smgr_unlink)(RelFileNodeBackend rnode, ForkNumber forknum, bool isRedo);
  void (*smgr_extend)(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer, bool skipFsync);
  void (*smgr_prefetch)(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum);
  void (*smgr_read)(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer);
  void (*smgr_write)(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer, bool skipFsync);
  void (*smgr_writeback)(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, BlockNumber nblocks);
  BlockNumber (*smgr_nblocks)(SMgrRelation reln, ForkNumber forknum);
  void (*smgr_truncate)(SMgrRelation reln, ForkNumber forknum, BlockNumber nblocks);
  void (*smgr_immedsync)(SMgrRelation reln, ForkNumber forknum);
} f_smgr;

static const f_smgr smgrsw[] = {
                       
    {
        .smgr_init = mdinit,
        .smgr_shutdown = NULL,
        .smgr_close = mdclose,
        .smgr_create = mdcreate,
        .smgr_exists = mdexists,
        .smgr_unlink = mdunlink,
        .smgr_extend = mdextend,
        .smgr_prefetch = mdprefetch,
        .smgr_read = mdread,
        .smgr_write = mdwrite,
        .smgr_writeback = mdwriteback,
        .smgr_nblocks = mdnblocks,
        .smgr_truncate = mdtruncate,
        .smgr_immedsync = mdimmedsync,
    }};

static const int NSmgr = lengthof(smgrsw);

   
                                                                             
                                                                               
   
static HTAB *SMgrRelationHash = NULL;

static dlist_head unowned_relns;

                               
static void
smgrshutdown(int code, Datum arg);

   
                                                                 
                      
   
                                                                         
                                                                           
                                                        
   
void
smgrinit(void)
{
  int i;

  for (i = 0; i < NSmgr; i++)
  {
    if (smgrsw[i].smgr_init)
    {
      smgrsw[i].smgr_init();
    }
  }

                                  
  on_proc_exit(smgrshutdown, 0);
}

   
                                                              
   
static void
smgrshutdown(int code, Datum arg)
{
  int i;

  for (i = 0; i < NSmgr; i++)
  {
    if (smgrsw[i].smgr_shutdown)
    {
      smgrsw[i].smgr_shutdown();
    }
  }
}

   
                                                                        
   
                                                                
   
SMgrRelation
smgropen(RelFileNode rnode, BackendId backend)
{
  RelFileNodeBackend brnode;
  SMgrRelation reln;
  bool found;

  if (SMgrRelationHash == NULL)
  {
                                                       
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(RelFileNodeBackend);
    ctl.entrysize = sizeof(SMgrRelationData);
    SMgrRelationHash = hash_create("smgr relation table", 400, &ctl, HASH_ELEM | HASH_BLOBS);
    dlist_init(&unowned_relns);
  }

                                  
  brnode.node = rnode;
  brnode.backend = backend;
  reln = (SMgrRelation)hash_search(SMgrRelationHash, (void *)&brnode, HASH_ENTER, &found);

                                           
  if (!found)
  {
    int forknum;

                                                      
    reln->smgr_owner = NULL;
    reln->smgr_targblock = InvalidBlockNumber;
    reln->smgr_fsm_nblocks = InvalidBlockNumber;
    reln->smgr_vm_nblocks = InvalidBlockNumber;
    reln->smgr_which = 0;                                   

                          
    for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
    {
      reln->md_num_open_segs[forknum] = 0;
    }

                             
    dlist_push_tail(&unowned_relns, &reln->node);
  }

  return reln;
}

   
                                                                                
   
                                                                             
                                               
   
void
smgrsetowner(SMgrRelation *owner, SMgrRelation reln)
{
                                                                             
  Assert(owner != NULL);

     
                                                                            
                                                                      
                                                                     
                                         
     
                                                                         
                                     
     
  if (reln->smgr_owner)
  {
    *(reln->smgr_owner) = NULL;
  }
  else
  {
    dlist_delete(&reln->node);
  }

                                                 
  reln->smgr_owner = owner;
  *owner = reln;
}

   
                                                                             
                        
   
void
smgrclearowner(SMgrRelation *owner, SMgrRelation reln)
{
                                                                       
  if (reln->smgr_owner != owner)
  {
    return;
  }

                                   
  *owner = NULL;

                                        
  reln->smgr_owner = NULL;

                                        
  dlist_push_tail(&unowned_relns, &reln->node);
}

   
                                                              
   
bool
smgrexists(SMgrRelation reln, ForkNumber forknum)
{
  return smgrsw[reln->smgr_which].smgr_exists(reln, forknum);
}

   
                                                           
   
void
smgrclose(SMgrRelation reln)
{
  SMgrRelation *owner;
  ForkNumber forknum;

  for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
  {
    smgrsw[reln->smgr_which].smgr_close(reln, forknum);
  }

  owner = reln->smgr_owner;

  if (!owner)
  {
    dlist_delete(&reln->node);
  }

  if (hash_search(SMgrRelationHash, (void *)&(reln->smgr_rnode), HASH_REMOVE, NULL) == NULL)
  {
    elog(ERROR, "SMgrRelation hashtable corrupted");
  }

     
                                                                            
                                                                             
     
  if (owner)
  {
    *owner = NULL;
  }
}

   
                                                              
   
void
smgrcloseall(void)
{
  HASH_SEQ_STATUS status;
  SMgrRelation reln;

                                             
  if (SMgrRelationHash == NULL)
  {
    return;
  }

  hash_seq_init(&status, SMgrRelationHash);

  while ((reln = (SMgrRelation)hash_seq_search(&status)) != NULL)
  {
    smgrclose(reln);
  }
}

   
                                                                       
                         
   
                                                                          
                                                                      
                              
   
void
smgrclosenode(RelFileNodeBackend rnode)
{
  SMgrRelation reln;

                                             
  if (SMgrRelationHash == NULL)
  {
    return;
  }

  reln = (SMgrRelation)hash_search(SMgrRelationHash, (void *)&rnode, HASH_FIND, NULL);
  if (reln != NULL)
  {
    smgrclose(reln);
  }
}

   
                                          
   
                                                                   
                                                                 
                   
   
                                                                   
                                                     
   
void
smgrcreate(SMgrRelation reln, ForkNumber forknum, bool isRedo)
{
     
                                                                          
                                      
     
  if (isRedo && reln->md_num_open_segs[forknum] > 0)
  {
    return;
  }

     
                                                                       
                                                                
     
                                                                            
                                                                           
                                                                            
                                                                 
     
  TablespaceCreateDbspace(reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, isRedo);

  smgrsw[reln->smgr_which].smgr_create(reln, forknum, isRedo);
}

   
                                                                 
   
                                                                       
                                                                           
   
                                                                        
             
   
                                                                      
                                                                     
   
void
smgrdounlink(SMgrRelation reln, bool isRedo)
{
  RelFileNodeBackend rnode = reln->smgr_rnode;
  int which = reln->smgr_which;
  ForkNumber forknum;

                                     
  for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
  {
    smgrsw[which].smgr_close(reln, forknum);
  }

     
                                                                          
                                                        
     
  DropRelFileNodesAllBuffers(&rnode, 1);

     
                                                                             
                                                                        
                                                                         
              
     

     
                                                                      
                                                                             
                                                                           
                                                                           
                                                                          
                   
     
  CacheInvalidateSmgr(rnode);

     
                                  
     
                                                                        
                                                                         
           
     
  smgrsw[which].smgr_unlink(rnode, InvalidForkNumber, isRedo);
}

   
                                                                            
   
                                                                       
                                                                          
            
   
                                                                        
             
   
                                                                           
                                                                
   
void
smgrdounlinkall(SMgrRelation *rels, int nrels, bool isRedo)
{
  int i = 0;
  RelFileNodeBackend *rnodes;
  ForkNumber forknum;

  if (nrels == 0)
  {
    return;
  }

     
                                                                           
                                                         
     
  rnodes = palloc(sizeof(RelFileNodeBackend) * nrels);
  for (i = 0; i < nrels; i++)
  {
    RelFileNodeBackend rnode = rels[i]->smgr_rnode;
    int which = rels[i]->smgr_which;

    rnodes[i] = rnode;

                                       
    for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
    {
      smgrsw[which].smgr_close(rels[i], forknum);
    }
  }

     
                                                                           
                                                        
     
  DropRelFileNodesAllBuffers(rnodes, nrels);

     
                                                                          
                                                       
     

     
                                                                      
                                                                          
                                                                        
                                                                            
                                                                       
                              
     
  for (i = 0; i < nrels; i++)
  {
    CacheInvalidateSmgr(rnodes[i]);
  }

     
                                  
     
                                                                        
                                                                         
           
     

  for (i = 0; i < nrels; i++)
  {
    int which = rels[i]->smgr_which;

    for (forknum = 0; forknum <= MAX_FORKNUM; forknum++)
    {
      smgrsw[which].smgr_unlink(rnodes[i], forknum, isRedo);
    }
  }

  pfree(rnodes);
}

   
                                                                    
   
                                                                        
                                                                          
            
   
                                                                     
             
   
void
smgrdounlinkfork(SMgrRelation reln, ForkNumber forknum, bool isRedo)
{
  RelFileNodeBackend rnode = reln->smgr_rnode;
  int which = reln->smgr_which;

                                    
  smgrsw[which].smgr_close(reln, forknum);

     
                                                                           
                                                   
     
  DropRelFileNodeBuffers(rnode, forknum, 0);

     
                                                                             
                                                                        
                                                                         
              
     

     
                                                                      
                                                                             
                                                                           
                                                                           
                                                                          
                   
     
  CacheInvalidateSmgr(rnode);

     
                                  
     
                                                                        
                                                                         
           
     
  smgrsw[which].smgr_unlink(rnode, forknum, isRedo);
}

   
                                              
   
                                                                   
                                                                     
                                                                     
                                                                  
                                                                
   
void
smgrextend(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer, bool skipFsync)
{
  smgrsw[reln->smgr_which].smgr_extend(reln, forknum, blocknum, buffer, skipFsync);
}

   
                                                                                      
   
void
smgrprefetch(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum)
{
  smgrsw[reln->smgr_which].smgr_prefetch(reln, forknum, blocknum);
}

   
                                                                           
                
   
                                                               
                                                                        
                                                      
   
void
smgrread(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer)
{
  smgrsw[reln->smgr_which].smgr_read(reln, forknum, blocknum, buffer);
}

   
                                                 
   
                                                                      
                                                                        
                      
   
                                                                    
                                                                
                                                                           
   
                                                                      
                                                                        
                          
   
void
smgrwrite(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer, bool skipFsync)
{
  smgrsw[reln->smgr_which].smgr_write(reln, forknum, blocknum, buffer, skipFsync);
}

   
                                                                         
                  
   
void
smgrwriteback(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, BlockNumber nblocks)
{
  smgrsw[reln->smgr_which].smgr_writeback(reln, forknum, blocknum, nblocks);
}

   
                                                          
                           
   
BlockNumber
smgrnblocks(SMgrRelation reln, ForkNumber forknum)
{
  return smgrsw[reln->smgr_which].smgr_nblocks(reln, forknum);
}

   
                                                                        
                   
   
                                                                     
   
void
smgrtruncate(SMgrRelation reln, ForkNumber forknum, BlockNumber nblocks)
{
     
                                                                            
                                                             
     
  DropRelFileNodeBuffers(reln->smgr_rnode, forknum, nblocks);

     
                                                                           
                                                                         
                                                                        
                                                                         
                                                           
                                                                           
                                                                            
                                                                       
     
  CacheInvalidateSmgr(reln->smgr_rnode);

     
                        
     
  smgrsw[reln->smgr_which].smgr_truncate(reln, forknum, nblocks);
}

   
                                                                      
   
                                                                      
                  
   
                                                                  
                                                                    
                                                                          
                                                                  
                                                                     
                                                                         
                                                                       
                                                                      
                                          
   
                                                                  
                        
   
                                                                      
                                                                   
                                               
   
void
smgrimmedsync(SMgrRelation reln, ForkNumber forknum)
{
  smgrsw[reln->smgr_which].smgr_immedsync(reln, forknum);
}

   
                 
   
                                                                         
                                                                             
   
                                                                         
                                                                          
                                                                           
                                                                           
                                                               
   
void
AtEOXact_SMgr(void)
{
  dlist_mutable_iter iter;

     
                                                                           
                        
     
  dlist_foreach_modify(iter, &unowned_relns)
  {
    SMgrRelation rel = dlist_container(SMgrRelationData, node, iter.cur);

    Assert(rel->smgr_owner == NULL);

    smgrclose(rel);
  }
}
