                                                                            
   
        
                                                               
   
                                                                            
                                                                        
                                                                            
                                                                         
                                                                         
                       
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#include "miscadmin.h"
#include "access/xlogutils.h"
#include "access/xlog.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/bufmgr.h"
#include "storage/md.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"
#include "storage/sync.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "pg_trace.h"

   
                                                              
                                                                    
                                                                  
                                                                   
                                                                         
                                                                       
                                          
   
                                                                      
                        
                                                                     
                                                                          
                                                                     
                                                                         
                                                                          
                                                                            
                                                                        
                                                                             
                                                                          
                                                                         
                                                                       
                                                                      
              
   
                                                                        
                                                                           
                                                                        
                                                                          
                                                                       
                                                                            
                                                                       
                                                                        
                                                                 
   
                                                                     
   

typedef struct _MdfdVec
{
  File mdfd_vfd;                                        
  BlockNumber mdfd_segno;                             
} MdfdVec;

static MemoryContext MdCxt;                                      

                                                          
#define INIT_MD_FILETAG(a, xx_rnode, xx_forknum, xx_segno) (memset(&(a), 0, sizeof(FileTag)), (a).handler = SYNC_HANDLER_MD, (a).rnode = (xx_rnode), (a).forknum = (xx_forknum), (a).segno = (xx_segno))

                                            
                                    
#define EXTENSION_FAIL (1 << 0)
                                        
#define EXTENSION_RETURN_NULL (1 << 1)
                                   
#define EXTENSION_CREATE (1 << 2)
                                                   
#define EXTENSION_CREATE_RECOVERY (1 << 3)
   
                                                                      
                                                                          
                                                                             
                                                                      
                
   
#define EXTENSION_DONT_CHECK_SIZE (1 << 4)

                    
static void
mdunlinkfork(RelFileNodeBackend rnode, ForkNumber forkNum, bool isRedo);
static MdfdVec *
mdopen(SMgrRelation reln, ForkNumber forknum, int behavior);
static void
register_dirty_segment(SMgrRelation reln, ForkNumber forknum, MdfdVec *seg);
static void
register_unlink_segment(RelFileNodeBackend rnode, ForkNumber forknum, BlockNumber segno);
static void
register_forget_request(RelFileNodeBackend rnode, ForkNumber forknum, BlockNumber segno);
static void
_fdvec_resize(SMgrRelation reln, ForkNumber forknum, int nseg);
static char *
_mdfd_segpath(SMgrRelation reln, ForkNumber forknum, BlockNumber segno);
static MdfdVec *
_mdfd_openseg(SMgrRelation reln, ForkNumber forkno, BlockNumber segno, int oflags);
static MdfdVec *
_mdfd_getseg(SMgrRelation reln, ForkNumber forkno, BlockNumber blkno, bool skipFsync, int behavior);
static BlockNumber
_mdnblocks(SMgrRelation reln, ForkNumber forknum, MdfdVec *seg);

   
                                                                           
   
void
mdinit(void)
{
  MdCxt = AllocSetContextCreate(TopMemoryContext, "MdSmgr", ALLOCSET_DEFAULT_SIZES);
}

   
                                               
   
                                                                           
   
bool
mdexists(SMgrRelation reln, ForkNumber forkNum)
{
     
                                                                            
                         
     
  mdclose(reln, forkNum);

  return (mdopen(reln, forkNum, EXTENSION_RETURN_NULL) != NULL);
}

   
                                                         
   
                                                                   
   
void
mdcreate(SMgrRelation reln, ForkNumber forkNum, bool isRedo)
{
  MdfdVec *mdfd;
  char *path;
  File fd;

  if (isRedo && reln->md_num_open_segs[forkNum] > 0)
  {
    return;                                    
  }

  Assert(reln->md_num_open_segs[forkNum] == 0);

  path = relpath(reln->smgr_rnode, forkNum);

  fd = PathNameOpenFile(path, O_RDWR | O_CREAT | O_EXCL | PG_BINARY);

  if (fd < 0)
  {
    int save_errno = errno;

    if (isRedo)
    {
      fd = PathNameOpenFile(path, O_RDWR | PG_BINARY);
    }
    if (fd < 0)
    {
                                                                    
      errno = save_errno;
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", path)));
    }
  }

  pfree(path);

  _fdvec_resize(reln, forkNum, 1);
  mdfd = &reln->md_seg_fds[forkNum][0];
  mdfd->mdfd_vfd = fd;
  mdfd->mdfd_segno = 0;
}

   
                                    
   
                                                                               
                                                           
   
                                                                                
                        
   
                                                                             
                                                                                
                                                                          
                                                                       
                                                                     
                                                                       
                                                                             
                                                                            
                                                 
                                                                               
                                                                           
                                                                           
                                                                           
                                                                              
                                                                               
                                                                         
                  
   
                                                                               
                                                                              
                                                                              
                                                                       
                                        
   
                                                                           
                                                                         
                                                                      
                                                                           
                                                                          
   
                                                                             
                                                                            
                                                                     
                         
   
                                                                      
                                                                    
   
void
mdunlink(RelFileNodeBackend rnode, ForkNumber forkNum, bool isRedo)
{
                                
  if (forkNum == InvalidForkNumber)
  {
    for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
    {
      mdunlinkfork(rnode, forkNum, isRedo);
    }
  }
  else
  {
    mdunlinkfork(rnode, forkNum, isRedo);
  }
}

   
                                          
   
static int
do_truncate(const char *path)
{
  int save_errno;
  int ret;
  int fd;

                                                                   
  fd = OpenTransientFile(path, O_RDWR | PG_BINARY);
  if (fd >= 0)
  {
    ret = ftruncate(fd, 0);
    save_errno = errno;
    CloseTransientFile(fd);
    errno = save_errno;
  }
  else
  {
    ret = -1;
  }

                                                          
  if (ret < 0 && errno != ENOENT)
  {
    save_errno = errno;
    ereport(WARNING, (errcode_for_file_access(), errmsg("could not truncate file \"%s\": %m", path)));
    errno = save_errno;
  }

  return ret;
}

static void
mdunlinkfork(RelFileNodeBackend rnode, ForkNumber forkNum, bool isRedo)
{
  char *path;
  int ret;

  path = relpath(rnode, forkNum);

     
                                           
     
  if (isRedo || forkNum != MAIN_FORKNUM || RelFileNodeBackendIsTemp(rnode))
  {
    if (!RelFileNodeBackendIsTemp(rnode))
    {
                                                                         
      ret = do_truncate(path);

                                                                  
      register_forget_request(rnode, forkNum, 0                );
    }
    else
    {
      ret = 0;
    }

                                                                         
    if (ret == 0 || errno != ENOENT)
    {
      ret = unlink(path);
      if (ret < 0 && errno != ENOENT)
      {
        ereport(WARNING, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
      }
    }
  }
  else
  {
                                                                       
    ret = do_truncate(path);

                                                        
    register_unlink_segment(rnode, forkNum, 0                );
  }

     
                                     
     
  if (ret >= 0)
  {
    char *segpath = (char *)palloc(strlen(path) + 12);
    BlockNumber segno;

       
                                                                         
                                                            
       
    for (segno = 1;; segno++)
    {
      sprintf(segpath, "%s.%u", path, segno);

      if (!RelFileNodeBackendIsTemp(rnode))
      {
           
                                                                   
                  
           
        if (do_truncate(segpath) < 0 && errno == ENOENT)
        {
          break;
        }

           
                                                                       
                          
           
        register_forget_request(rnode, forkNum, segno);
      }

      if (unlink(segpath) < 0)
      {
                                                          
        if (errno != ENOENT)
        {
          ereport(WARNING, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", segpath)));
        }
        break;
      }
    }
    pfree(segpath);
  }

  pfree(path);
}

   
                                                        
   
                                                                 
                                                                     
                                                                     
                                                                  
                                                                
   
void
mdextend(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer, bool skipFsync)
{
  off_t seekpos;
  int nbytes;
  MdfdVec *v;

                                                            
#ifdef CHECK_WRITE_VS_EXTEND
  Assert(blocknum >= mdnblocks(reln, forknum));
#endif

     
                                                                             
                                                                 
                                                                        
                                              
     
  if (blocknum == InvalidBlockNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("cannot extend file \"%s\" beyond %u blocks", relpath(reln->smgr_rnode, forknum), InvalidBlockNumber)));
  }

  v = _mdfd_getseg(reln, forknum, blocknum, skipFsync, EXTENSION_CREATE);

  seekpos = (off_t)BLCKSZ * (blocknum % ((BlockNumber)RELSEG_SIZE));

  Assert(seekpos < (off_t)BLCKSZ * RELSEG_SIZE);

  if ((nbytes = FileWrite(v->mdfd_vfd, buffer, BLCKSZ, seekpos, WAIT_EVENT_DATA_FILE_EXTEND)) != BLCKSZ)
  {
    if (nbytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not extend file \"%s\": %m", FilePathName(v->mdfd_vfd)), errhint("Check free disk space.")));
    }
                                             
    ereport(ERROR, (errcode(ERRCODE_DISK_FULL), errmsg("could not extend file \"%s\": wrote only %d of %d bytes at block %u", FilePathName(v->mdfd_vfd), nbytes, BLCKSZ, blocknum), errhint("Check free disk space.")));
  }

  if (!skipFsync && !SmgrIsTemp(reln))
  {
    register_dirty_segment(reln, forknum, v);
  }

  Assert(_mdnblocks(reln, forknum, v) <= ((BlockNumber)RELSEG_SIZE));
}

   
                                            
   
                                                                          
   
                                                                            
                                                                         
                                                                         
                                  
   
static MdfdVec *
mdopen(SMgrRelation reln, ForkNumber forknum, int behavior)
{
  MdfdVec *mdfd;
  char *path;
  File fd;

                               
  if (reln->md_num_open_segs[forknum] > 0)
  {
    return &reln->md_seg_fds[forknum][0];
  }

  path = relpath(reln->smgr_rnode, forknum);

  fd = PathNameOpenFile(path, O_RDWR | PG_BINARY);

  if (fd < 0)
  {
    if ((behavior & EXTENSION_RETURN_NULL) && FILE_POSSIBLY_DELETED(errno))
    {
      pfree(path);
      return NULL;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

  pfree(path);

  _fdvec_resize(reln, forknum, 1);
  mdfd = &reln->md_seg_fds[forknum][0];
  mdfd->mdfd_vfd = fd;
  mdfd->mdfd_segno = 0;

  Assert(_mdnblocks(reln, forknum, mdfd) <= ((BlockNumber)RELSEG_SIZE));

  return mdfd;
}

   
                                                                          
   
void
mdclose(SMgrRelation reln, ForkNumber forknum)
{
  int nopensegs = reln->md_num_open_segs[forknum];

                                 
  if (nopensegs == 0)
  {
    return;
  }

                                            
  while (nopensegs > 0)
  {
    MdfdVec *v = &reln->md_seg_fds[forknum][nopensegs - 1];

    FileClose(v->mdfd_vfd);
    _fdvec_resize(reln, forknum, nopensegs - 1);
    nopensegs--;
  }
}

   
                                                                                   
   
void
mdprefetch(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum)
{
#ifdef USE_PREFETCH
  off_t seekpos;
  MdfdVec *v;

  v = _mdfd_getseg(reln, forknum, blocknum, false, EXTENSION_FAIL);

  seekpos = (off_t)BLCKSZ * (blocknum % ((BlockNumber)RELSEG_SIZE));

  Assert(seekpos < (off_t)BLCKSZ * RELSEG_SIZE);

  (void)FilePrefetch(v->mdfd_vfd, seekpos, BLCKSZ, WAIT_EVENT_DATA_FILE_PREFETCH);
#endif                   
}

   
                                                                    
   
                                                                            
                                                           
   
void
mdwriteback(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, BlockNumber nblocks)
{
     
                                                                           
                                                                         
     
  while (nblocks > 0)
  {
    BlockNumber nflush = nblocks;
    off_t seekpos;
    MdfdVec *v;
    int segnum_start, segnum_end;

    v = _mdfd_getseg(reln, forknum, blocknum, true               , EXTENSION_RETURN_NULL);

       
                                                                         
                                  
       
    if (!v)
    {
      return;
    }

                                                   
    segnum_start = blocknum / RELSEG_SIZE;

                                                                     
    segnum_end = (blocknum + nblocks - 1) / RELSEG_SIZE;
    if (segnum_start != segnum_end)
    {
      nflush = RELSEG_SIZE - (blocknum % ((BlockNumber)RELSEG_SIZE));
    }

    Assert(nflush >= 1);
    Assert(nflush <= nblocks);

    seekpos = (off_t)BLCKSZ * (blocknum % ((BlockNumber)RELSEG_SIZE));

    FileWriteback(v->mdfd_vfd, seekpos, (off_t)BLCKSZ * nflush, WAIT_EVENT_DATA_FILE_FLUSH);

    nblocks -= nflush;
    blocknum += nflush;
  }
}

   
                                                         
   
void
mdread(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer)
{
  off_t seekpos;
  int nbytes;
  MdfdVec *v;

  TRACE_POSTGRESQL_SMGR_MD_READ_START(forknum, blocknum, reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, reln->smgr_rnode.node.relNode, reln->smgr_rnode.backend);

  v = _mdfd_getseg(reln, forknum, blocknum, false, EXTENSION_FAIL | EXTENSION_CREATE_RECOVERY);

  seekpos = (off_t)BLCKSZ * (blocknum % ((BlockNumber)RELSEG_SIZE));

  Assert(seekpos < (off_t)BLCKSZ * RELSEG_SIZE);

  nbytes = FileRead(v->mdfd_vfd, buffer, BLCKSZ, seekpos, WAIT_EVENT_DATA_FILE_READ);

  TRACE_POSTGRESQL_SMGR_MD_READ_DONE(forknum, blocknum, reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, reln->smgr_rnode.node.relNode, reln->smgr_rnode.backend, nbytes, BLCKSZ);

  if (nbytes != BLCKSZ)
  {
    if (nbytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read block %u in file \"%s\": %m", blocknum, FilePathName(v->mdfd_vfd))));
    }

       
                                                                        
                                                                         
                                                                          
                                                                  
                                                                     
                                                     
       
    if (zero_damaged_pages || InRecovery)
    {
      MemSet(buffer, 0, BLCKSZ);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read block %u in file \"%s\": read only %d of %d bytes", blocknum, FilePathName(v->mdfd_vfd), nbytes, BLCKSZ)));
    }
  }
}

   
                                                                      
   
                                                                      
                                                                        
                    
   
void
mdwrite(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer, bool skipFsync)
{
  off_t seekpos;
  int nbytes;
  MdfdVec *v;

                                                            
#ifdef CHECK_WRITE_VS_EXTEND
  Assert(blocknum < mdnblocks(reln, forknum));
#endif

  TRACE_POSTGRESQL_SMGR_MD_WRITE_START(forknum, blocknum, reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, reln->smgr_rnode.node.relNode, reln->smgr_rnode.backend);

  v = _mdfd_getseg(reln, forknum, blocknum, skipFsync, EXTENSION_FAIL | EXTENSION_CREATE_RECOVERY);

  seekpos = (off_t)BLCKSZ * (blocknum % ((BlockNumber)RELSEG_SIZE));

  Assert(seekpos < (off_t)BLCKSZ * RELSEG_SIZE);

  nbytes = FileWrite(v->mdfd_vfd, buffer, BLCKSZ, seekpos, WAIT_EVENT_DATA_FILE_WRITE);

  TRACE_POSTGRESQL_SMGR_MD_WRITE_DONE(forknum, blocknum, reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, reln->smgr_rnode.node.relNode, reln->smgr_rnode.backend, nbytes, BLCKSZ);

  if (nbytes != BLCKSZ)
  {
    if (nbytes < 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write block %u in file \"%s\": %m", blocknum, FilePathName(v->mdfd_vfd))));
    }
                                             
    ereport(ERROR, (errcode(ERRCODE_DISK_FULL), errmsg("could not write block %u in file \"%s\": wrote only %d of %d bytes", blocknum, FilePathName(v->mdfd_vfd), nbytes, BLCKSZ), errhint("Check free disk space.")));
  }

  if (!skipFsync && !SmgrIsTemp(reln))
  {
    register_dirty_segment(reln, forknum, v);
  }
}

   
                                                                 
   
                                                                          
                                                                       
                                                                   
                              
   
BlockNumber
mdnblocks(SMgrRelation reln, ForkNumber forknum)
{
  MdfdVec *v = mdopen(reln, forknum, EXTENSION_FAIL);
  BlockNumber nblocks;
  BlockNumber segno = 0;

                                           
  Assert(reln->md_num_open_segs[forknum] > 0);

     
                                                                           
                                                                           
                                                 
     
                                                                      
                                                                           
                                                                        
                                                                     
                                                                          
                                                                        
            
     
  segno = reln->md_num_open_segs[forknum] - 1;
  v = &reln->md_seg_fds[forknum][segno];

  for (;;)
  {
    nblocks = _mdnblocks(reln, forknum, v);
    if (nblocks > ((BlockNumber)RELSEG_SIZE))
    {
      elog(FATAL, "segment too big");
    }
    if (nblocks < ((BlockNumber)RELSEG_SIZE))
    {
      return (segno * ((BlockNumber)RELSEG_SIZE)) + nblocks;
    }

       
                                                               
       
    segno++;

       
                                                                           
                                                                        
                                                                       
                                                                        
                                         
       
    v = _mdfd_openseg(reln, forknum, segno, 0);
    if (v == NULL)
    {
      return segno * ((BlockNumber)RELSEG_SIZE);
    }
  }
}

   
                                                                    
   
void
mdtruncate(SMgrRelation reln, ForkNumber forknum, BlockNumber nblocks)
{
  BlockNumber curnblk;
  BlockNumber priorblocks;
  int curopensegs;

     
                                                                            
                                        
     
  curnblk = mdnblocks(reln, forknum);
  if (nblocks > curnblk)
  {
                                                          
    if (InRecovery)
    {
      return;
    }
    ereport(ERROR, (errmsg("could not truncate file \"%s\" to %u blocks: it's only %u blocks now", relpath(reln->smgr_rnode, forknum), nblocks, curnblk)));
  }
  if (nblocks == curnblk)
  {
    return;              
  }

     
                                                                            
                                                                          
     
  curopensegs = reln->md_num_open_segs[forknum];
  while (curopensegs > 0)
  {
    MdfdVec *v;

    priorblocks = (curopensegs - 1) * RELSEG_SIZE;

    v = &reln->md_seg_fds[forknum][curopensegs - 1];

    if (priorblocks > nblocks)
    {
         
                                                                        
                                                                      
         
      if (FileTruncate(v->mdfd_vfd, 0, WAIT_EVENT_DATA_FILE_TRUNCATE) < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not truncate file \"%s\": %m", FilePathName(v->mdfd_vfd))));
      }

      if (!SmgrIsTemp(reln))
      {
        register_dirty_segment(reln, forknum, v);
      }

                                         
      Assert(v != &reln->md_seg_fds[forknum][0]);

      FileClose(v->mdfd_vfd);
      _fdvec_resize(reln, forknum, curopensegs - 1);
    }
    else if (priorblocks + ((BlockNumber)RELSEG_SIZE) > nblocks)
    {
         
                                                                        
                                                                       
                                                                         
                                                                    
                   
         
      BlockNumber lastsegblocks = nblocks - priorblocks;

      if (FileTruncate(v->mdfd_vfd, (off_t)lastsegblocks * BLCKSZ, WAIT_EVENT_DATA_FILE_TRUNCATE) < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not truncate file \"%s\" to %u blocks: %m", FilePathName(v->mdfd_vfd), nblocks)));
      }
      if (!SmgrIsTemp(reln))
      {
        register_dirty_segment(reln, forknum, v);
      }
    }
    else
    {
         
                                                                       
                          
         
      break;
    }
    curopensegs--;
  }
}

   
                                                                   
   
                                                                       
                                                                      
   
void
mdimmedsync(SMgrRelation reln, ForkNumber forknum)
{
  int segno;

     
                                                                            
                                   
     
  mdnblocks(reln, forknum);

  segno = reln->md_num_open_segs[forknum];

  while (segno > 0)
  {
    MdfdVec *v = &reln->md_seg_fds[forknum][segno - 1];

    if (FileSync(v->mdfd_vfd, WAIT_EVENT_DATA_FILE_IMMEDIATE_SYNC) < 0)
    {
      ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", FilePathName(v->mdfd_vfd))));
    }
    segno--;
  }
}

   
                                                                        
   
                                                                       
                                                                         
                                                                          
                                                                      
                                        
   
static void
register_dirty_segment(SMgrRelation reln, ForkNumber forknum, MdfdVec *seg)
{
  FileTag tag;

  INIT_MD_FILETAG(tag, reln->smgr_rnode.node, forknum, seg->mdfd_segno);

                                              
  Assert(!SmgrIsTemp(reln));

  if (!RegisterSyncRequest(&tag, SYNC_REQUEST, false                   ))
  {
    ereport(DEBUG1, (errmsg("could not forward fsync request because request queue is full")));

    if (FileSync(seg->mdfd_vfd, WAIT_EVENT_DATA_FILE_SYNC) < 0)
    {
      ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", FilePathName(seg->mdfd_vfd))));
    }
  }
}

   
                                                                                    
   
static void
register_unlink_segment(RelFileNodeBackend rnode, ForkNumber forknum, BlockNumber segno)
{
  FileTag tag;

  INIT_MD_FILETAG(tag, rnode.node, forknum, segno);

                                                
  Assert(!RelFileNodeBackendIsTemp(rnode));

  RegisterSyncRequest(&tag, SYNC_UNLINK_REQUEST, true                   );
}

   
                                                                                
   
static void
register_forget_request(RelFileNodeBackend rnode, ForkNumber forknum, BlockNumber segno)
{
  FileTag tag;

  INIT_MD_FILETAG(tag, rnode.node, forknum, segno);

  RegisterSyncRequest(&tag, SYNC_FORGET_REQUEST, true                   );
}

   
                                                                        
   
void
ForgetDatabaseSyncRequests(Oid dbid)
{
  FileTag tag;
  RelFileNode rnode;

  rnode.dbNode = dbid;
  rnode.spcNode = 0;
  rnode.relNode = 0;

  INIT_MD_FILETAG(tag, rnode, InvalidForkNumber, InvalidBlockNumber);

  RegisterSyncRequest(&tag, SYNC_FILTER_REQUEST, true                   );
}

   
                                                          
   
void
DropRelationFiles(RelFileNode *delrels, int ndelrels, bool isRedo)
{
  SMgrRelation *srels;
  int i;

  srels = palloc(sizeof(SMgrRelation) * ndelrels);
  for (i = 0; i < ndelrels; i++)
  {
    SMgrRelation srel = smgropen(delrels[i], InvalidBackendId);

    if (isRedo)
    {
      ForkNumber fork;

      for (fork = 0; fork <= MAX_FORKNUM; fork++)
      {
        XLogDropRelation(delrels[i], fork);
      }
    }
    srels[i] = srel;
  }

  smgrdounlinkall(srels, ndelrels, isRedo);

  for (i = 0; i < ndelrels; i++)
  {
    smgrclose(srels[i]);
  }
  pfree(srels);
}

   
                                                            
   
static void
_fdvec_resize(SMgrRelation reln, ForkNumber forknum, int nseg)
{
  if (nseg == 0)
  {
    if (reln->md_num_open_segs[forknum] > 0)
    {
      pfree(reln->md_seg_fds[forknum]);
      reln->md_seg_fds[forknum] = NULL;
    }
  }
  else if (reln->md_num_open_segs[forknum] == 0)
  {
    reln->md_seg_fds[forknum] = MemoryContextAlloc(MdCxt, sizeof(MdfdVec) * nseg);
  }
  else
  {
       
                                                                    
                                                                          
                                                                           
                                     
       
    reln->md_seg_fds[forknum] = repalloc(reln->md_seg_fds[forknum], sizeof(MdfdVec) * nseg);
  }

  reln->md_num_open_segs[forknum] = nseg;
}

   
                                                                      
                                
   
static char *
_mdfd_segpath(SMgrRelation reln, ForkNumber forknum, BlockNumber segno)
{
  char *path, *fullpath;

  path = relpath(reln->smgr_rnode, forknum);

  if (segno > 0)
  {
    fullpath = psprintf("%s.%u", path, segno);
    pfree(path);
  }
  else
  {
    fullpath = path;
  }

  return fullpath;
}

   
                                               
                                                               
   
static MdfdVec *
_mdfd_openseg(SMgrRelation reln, ForkNumber forknum, BlockNumber segno, int oflags)
{
  MdfdVec *v;
  int fd;
  char *fullpath;

  fullpath = _mdfd_segpath(reln, forknum, segno);

                     
  fd = PathNameOpenFile(fullpath, O_RDWR | PG_BINARY | oflags);

  pfree(fullpath);

  if (fd < 0)
  {
    return NULL;
  }

  if (segno <= reln->md_num_open_segs[forknum])
  {
    _fdvec_resize(reln, forknum, segno + 1);
  }

                      
  v = &reln->md_seg_fds[forknum][segno];
  v->mdfd_vfd = fd;
  v->mdfd_segno = segno;

  Assert(_mdnblocks(reln, forknum, v) <= ((BlockNumber)RELSEG_SIZE));

                
  return v;
}

   
                                                                  
                     
   
                                                                        
                                                                          
                          
   
static MdfdVec *
_mdfd_getseg(SMgrRelation reln, ForkNumber forknum, BlockNumber blkno, bool skipFsync, int behavior)
{
  MdfdVec *v;
  BlockNumber targetseg;
  BlockNumber nextsegno;

                                                                      
  Assert(behavior & (EXTENSION_FAIL | EXTENSION_CREATE | EXTENSION_RETURN_NULL));

  targetseg = blkno / ((BlockNumber)RELSEG_SIZE);

                                                     
  if (targetseg < reln->md_num_open_segs[forknum])
  {
    v = &reln->md_seg_fds[forknum][targetseg];
    return v;
  }

     
                                                                       
                                                                      
                                                                  
                                                                             
                             
     
  if (reln->md_num_open_segs[forknum] > 0)
  {
    v = &reln->md_seg_fds[forknum][reln->md_num_open_segs[forknum] - 1];
  }
  else
  {
    v = mdopen(reln, forknum, behavior);
    if (!v)
    {
      return NULL;                                          
    }
  }

  for (nextsegno = reln->md_num_open_segs[forknum]; nextsegno <= targetseg; nextsegno++)
  {
    BlockNumber nblocks = _mdnblocks(reln, forknum, v);
    int flags = 0;

    Assert(nextsegno == v->mdfd_segno + 1);

    if (nblocks > ((BlockNumber)RELSEG_SIZE))
    {
      elog(FATAL, "segment too big");
    }

    if ((behavior & EXTENSION_CREATE) || (InRecovery && (behavior & EXTENSION_CREATE_RECOVERY)))
    {
         
                                                                        
                                                                     
                                                                     
                                                                  
                                                                     
                                                                        
                                             
                                                                     
                                                                   
         
                                                                         
                                                               
                                                                    
                                                                   
                                                                         
         
      if (nblocks < ((BlockNumber)RELSEG_SIZE))
      {
        char *zerobuf = palloc0(BLCKSZ);

        mdextend(reln, forknum, nextsegno * ((BlockNumber)RELSEG_SIZE) - 1, zerobuf, skipFsync);
        pfree(zerobuf);
      }
      flags = O_CREAT;
    }
    else if (!(behavior & EXTENSION_DONT_CHECK_SIZE) && nblocks < ((BlockNumber)RELSEG_SIZE))
    {
         
                                                               
                                                                     
                                                                        
                  
         
      if (behavior & EXTENSION_RETURN_NULL)
      {
           
                                                                   
                                                                
                                                                  
                                                             
           
        errno = ENOENT;
        return NULL;
      }

      ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" (target block %u): previous segment is only %u blocks", _mdfd_segpath(reln, forknum, nextsegno), blkno, nblocks)));
    }

    v = _mdfd_openseg(reln, forknum, nextsegno, flags);

    if (v == NULL)
    {
      if ((behavior & EXTENSION_RETURN_NULL) && FILE_POSSIBLY_DELETED(errno))
      {
        return NULL;
      }
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" (target block %u): %m", _mdfd_segpath(reln, forknum, nextsegno), blkno)));
    }
  }

  return v;
}

   
                                                      
   
static BlockNumber
_mdnblocks(SMgrRelation reln, ForkNumber forknum, MdfdVec *seg)
{
  off_t len;

  len = FileSize(seg->mdfd_vfd);
  if (len < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek to end of file \"%s\": %m", FilePathName(seg->mdfd_vfd))));
  }
                                                                       
  return (BlockNumber)(len / BLCKSZ);
}

   
                                                                         
                                                      
   
                                                       
   
int
mdsyncfiletag(const FileTag *ftag, char *path)
{
  SMgrRelation reln = smgropen(ftag->rnode, InvalidBackendId);
  File file;
  bool need_to_close;
  int result, save_errno;

                                                                 
  if (ftag->segno < reln->md_num_open_segs[ftag->forknum])
  {
    file = reln->md_seg_fds[ftag->forknum][ftag->segno].mdfd_vfd;
    strlcpy(path, FilePathName(file), MAXPGPATH);
    need_to_close = false;
  }
  else
  {
    char *p;

    p = _mdfd_segpath(reln, ftag->forknum, ftag->segno);
    strlcpy(path, p, MAXPGPATH);
    pfree(p);

    file = PathNameOpenFile(path, O_RDWR | PG_BINARY);
    if (file < 0)
    {
      return -1;
    }
    need_to_close = true;
  }

                      
  result = FileSync(file, WAIT_EVENT_DATA_FILE_SYNC);
  save_errno = errno;

  if (need_to_close)
  {
    FileClose(file);
  }

  errno = save_errno;
  return result;
}

   
                                                                   
                                                      
   
                                                       
   
int
mdunlinkfiletag(const FileTag *ftag, char *path)
{
  char *p;

                         
  p = relpathperm(ftag->rnode, MAIN_FORKNUM);
  strlcpy(path, p, MAXPGPATH);
  pfree(p);

                               
  return unlink(path);
}

   
                                                                           
                                                                       
                                                
   
bool
mdfiletagmatches(const FileTag *ftag, const FileTag *candidate)
{
     
                                                                        
                                                                         
                                                                             
                                                                          
     
  return ftag->rnode.dbNode == candidate->rnode.dbNode;
}
