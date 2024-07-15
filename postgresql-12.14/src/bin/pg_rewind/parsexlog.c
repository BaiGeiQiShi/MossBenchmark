                                                                            
   
               
                                           
   
                                                                         
                                                                        
   
                                                                            
   

#include "postgres_fe.h"

#include <unistd.h>

#include "pg_rewind.h"
#include "filemap.h"

#include "access/rmgr.h"
#include "access/xlog_internal.h"
#include "access/xlogreader.h"
#include "catalog/pg_control.h"
#include "catalog/storage_xlog.h"
#include "commands/dbcommands_xlog.h"

   
                                                                           
                
   
#define PG_RMGR(symname, name, redo, desc, identify, startup, cleanup, mask) name,

static const char *RmgrNames[RM_MAX_ID + 1] = {
#include "access/rmgrlist.h"
};

static void
extractPageInfo(XLogReaderState *record);

static int xlogreadfd = -1;
static XLogSegNo xlogreadsegno = -1;
static char xlogfpath[MAXPGPATH];

typedef struct XLogPageReadPrivate
{
  const char *datadir;
  int tliIndex;
} XLogPageReadPrivate;

static int
SimpleXLogPageRead(XLogReaderState *xlogreader, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *readBuf, TimeLineID *pageTLI);

   
                                                                            
                                                                               
                                                                              
   
                                                                            
                                                 
   
void
extractPageMap(const char *datadir, XLogRecPtr startpoint, int tliIndex, XLogRecPtr endpoint)
{
  XLogRecord *record;
  XLogReaderState *xlogreader;
  char *errormsg;
  XLogPageReadPrivate private;

  private.datadir = datadir;
  private.tliIndex = tliIndex;
  xlogreader = XLogReaderAllocate(WalSegSz, &SimpleXLogPageRead, &private);
  if (xlogreader == NULL)
  {
    pg_fatal("out of memory");
  }

  do
  {
    record = XLogReadRecord(xlogreader, startpoint, &errormsg);

    if (record == NULL)
    {
      XLogRecPtr errptr;

      errptr = startpoint ? startpoint : xlogreader->EndRecPtr;

      if (errormsg)
      {
        pg_fatal("could not read WAL record at %X/%X: %s", (uint32)(errptr >> 32), (uint32)(errptr), errormsg);
      }
      else
      {
        pg_fatal("could not read WAL record at %X/%X", (uint32)(errptr >> 32), (uint32)(errptr));
      }
    }

    extractPageInfo(xlogreader);

    startpoint = InvalidXLogRecPtr;                                      

  } while (xlogreader->EndRecPtr < endpoint);

     
                                                                         
                
     
  if (xlogreader->EndRecPtr != endpoint)
  {
    pg_fatal("end pointer %X/%X is not a valid end point; expected %X/%X", (uint32)(endpoint >> 32), (uint32)(endpoint), (uint32)(xlogreader->EndRecPtr >> 32), (uint32)(xlogreader->EndRecPtr));
  }

  XLogReaderFree(xlogreader);
  if (xlogreadfd != -1)
  {
    close(xlogreadfd);
    xlogreadfd = -1;
  }
}

   
                                                                         
                                          
   
XLogRecPtr
readOneRecord(const char *datadir, XLogRecPtr ptr, int tliIndex)
{
  XLogRecord *record;
  XLogReaderState *xlogreader;
  char *errormsg;
  XLogPageReadPrivate private;
  XLogRecPtr endptr;

  private.datadir = datadir;
  private.tliIndex = tliIndex;
  xlogreader = XLogReaderAllocate(WalSegSz, &SimpleXLogPageRead, &private);
  if (xlogreader == NULL)
  {
    pg_fatal("out of memory");
  }

  record = XLogReadRecord(xlogreader, ptr, &errormsg);
  if (record == NULL)
  {
    if (errormsg)
    {
      pg_fatal("could not read WAL record at %X/%X: %s", (uint32)(ptr >> 32), (uint32)(ptr), errormsg);
    }
    else
    {
      pg_fatal("could not read WAL record at %X/%X", (uint32)(ptr >> 32), (uint32)(ptr));
    }
  }
  endptr = xlogreader->EndRecPtr;

  XLogReaderFree(xlogreader);
  if (xlogreadfd != -1)
  {
    close(xlogreadfd);
    xlogreadfd = -1;
  }

  return endptr;
}

   
                                                              
   
void
findLastCheckpoint(const char *datadir, XLogRecPtr forkptr, int tliIndex, XLogRecPtr *lastchkptrec, TimeLineID *lastchkpttli, XLogRecPtr *lastchkptredo)
{
                                                      
  XLogRecord *record;
  XLogRecPtr searchptr;
  XLogReaderState *xlogreader;
  char *errormsg;
  XLogPageReadPrivate private;

     
                                                                         
                                                                       
                                                                           
                                                  
     
  if (forkptr % XLOG_BLCKSZ == 0)
  {
    if (XLogSegmentOffset(forkptr, WalSegSz) == 0)
    {
      forkptr += SizeOfXLogLongPHD;
    }
    else
    {
      forkptr += SizeOfXLogShortPHD;
    }
  }

  private.datadir = datadir;
  private.tliIndex = tliIndex;
  xlogreader = XLogReaderAllocate(WalSegSz, &SimpleXLogPageRead, &private);
  if (xlogreader == NULL)
  {
    pg_fatal("out of memory");
  }

  searchptr = forkptr;
  for (;;)
  {
    uint8 info;

    record = XLogReadRecord(xlogreader, searchptr, &errormsg);

    if (record == NULL)
    {
      if (errormsg)
      {
        pg_fatal("could not find previous WAL record at %X/%X: %s", (uint32)(searchptr >> 32), (uint32)(searchptr), errormsg);
      }
      else
      {
        pg_fatal("could not find previous WAL record at %X/%X", (uint32)(searchptr >> 32), (uint32)(searchptr));
      }
    }

       
                                                                           
                                                                         
                                                         
       
    info = XLogRecGetInfo(xlogreader) & ~XLR_INFO_MASK;
    if (searchptr < forkptr && XLogRecGetRmid(xlogreader) == RM_XLOG_ID && (info == XLOG_CHECKPOINT_SHUTDOWN || info == XLOG_CHECKPOINT_ONLINE))
    {
      CheckPoint checkPoint;

      memcpy(&checkPoint, XLogRecGetData(xlogreader), sizeof(CheckPoint));
      *lastchkptrec = searchptr;
      *lastchkpttli = checkPoint.ThisTimeLineID;
      *lastchkptredo = checkPoint.redo;
      break;
    }

                                            
    searchptr = record->xl_prev;
  }

  XLogReaderFree(xlogreader);
  if (xlogreadfd != -1)
  {
    close(xlogreadfd);
    xlogreadfd = -1;
  }
}

                                                      
static int
SimpleXLogPageRead(XLogReaderState *xlogreader, XLogRecPtr targetPagePtr, int reqLen, XLogRecPtr targetRecPtr, char *readBuf, TimeLineID *pageTLI)
{
  XLogPageReadPrivate *private = (XLogPageReadPrivate *)xlogreader->private_data;
  uint32 targetPageOff;
  XLogRecPtr targetSegEnd;
  XLogSegNo targetSegNo;
  int r;

  XLByteToSeg(targetPagePtr, targetSegNo, WalSegSz);
  XLogSegNoOffsetToRecPtr(targetSegNo + 1, 0, WalSegSz, targetSegEnd);
  targetPageOff = XLogSegmentOffset(targetPagePtr, WalSegSz);

     
                                                                            
                                       
     
  if (xlogreadfd >= 0 && !XLByteInSeg(targetPagePtr, xlogreadsegno, WalSegSz))
  {
    close(xlogreadfd);
    xlogreadfd = -1;
  }

  XLByteToSeg(targetPagePtr, xlogreadsegno, WalSegSz);

  if (xlogreadfd < 0)
  {
    char xlogfname[MAXFNAMELEN];

       
                                                                           
                                                                         
                                                                           
                    
       
    while (private->tliIndex < targetNentries - 1 && targetHistory[private->tliIndex].end < targetSegEnd)
    private->tliIndex++;
    while (private->tliIndex > 0 && targetHistory[private->tliIndex].begin >= targetSegEnd)
    private->tliIndex--;

    XLogFileName(xlogfname, targetHistory[private->tliIndex].tli, xlogreadsegno, WalSegSz);

    snprintf(xlogfpath, MAXPGPATH, "%s/" XLOGDIR "/%s", private->datadir, xlogfname);

    xlogreadfd = open(xlogfpath, O_RDONLY | PG_BINARY, 0);

    if (xlogreadfd < 0)
    {
      pg_log_error("could not open file \"%s\": %m", xlogfpath);
      return -1;
    }
  }

     
                                                    
     
  Assert(xlogreadfd != -1);

                               
  if (lseek(xlogreadfd, (off_t)targetPageOff, SEEK_SET) < 0)
  {
    pg_log_error("could not seek in file \"%s\": %m", xlogfpath);
    return -1;
  }

  r = read(xlogreadfd, readBuf, XLOG_BLCKSZ);
  if (r != XLOG_BLCKSZ)
  {
    if (r < 0)
    {
      pg_log_error("could not read file \"%s\": %m", xlogfpath);
    }
    else
    {
      pg_log_error("could not read file \"%s\": read %d of %zu", xlogfpath, r, (Size)XLOG_BLCKSZ);
    }

    return -1;
  }

  Assert(targetSegNo == xlogreadsegno);

  *pageTLI = targetHistory[private->tliIndex].tli;
  return XLOG_BLCKSZ;
}

   
                                                                    
   
static void
extractPageInfo(XLogReaderState *record)
{
  int block_id;
  RmgrId rmid = XLogRecGetRmid(record);
  uint8 info = XLogRecGetInfo(record);
  uint8 rminfo = info & ~XLR_INFO_MASK;

                                                       

  if (rmid == RM_DBASE_ID && rminfo == XLOG_DBASE_CREATE)
  {
       
                                                                       
                                                                      
                                                                           
                                                                         
                                                                        
                                                                
                                                              
       
  }
  else if (rmid == RM_DBASE_ID && rminfo == XLOG_DBASE_DROP)
  {
       
                                                                        
                                                                           
                                                    
       
  }
  else if (rmid == RM_SMGR_ID && rminfo == XLOG_SMGR_CREATE)
  {
       
                                                                     
                                                                         
                                                                        
                                 
       
  }
  else if (rmid == RM_SMGR_ID && rminfo == XLOG_SMGR_TRUNCATE)
  {
       
                                                                       
                                                                     
                      
       
  }
  else if (info & XLR_SPECIAL_REL_UPDATE)
  {
       
                                                                          
                                                                      
                          
       
    pg_fatal("WAL record modifies a relation, but record type is not recognized: "
             "lsn: %X/%X, rmgr: %s, info: %02X",
        (uint32)(record->ReadRecPtr >> 32), (uint32)(record->ReadRecPtr), RmgrNames[rmid], info);
  }

  for (block_id = 0; block_id <= record->max_block_id; block_id++)
  {
    RelFileNode rnode;
    ForkNumber forknum;
    BlockNumber blkno;

    if (!XLogRecGetBlockTag(record, block_id, &rnode, &forknum, &blkno))
    {
      continue;
    }

                                                                     
    if (forknum != MAIN_FORKNUM)
    {
      continue;
    }

    process_block_change(forknum, rnode, blkno);
  }
}
