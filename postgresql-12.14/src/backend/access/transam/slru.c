                                                                            
   
          
                                                         
   
                                                                       
                                                               
                                                                       
                                                                          
                                                                         
                                                                             
                                                                     
                                                                           
                                                                              
   
                                                                       
                                                                              
                                                                          
                                                                           
                                                             
   
                                                                           
                                                                         
                             
   
                                                                               
                                                                             
                                                                              
                                                                            
                                                                   
                                                                      
                                                                            
                                                                              
                                                       
   
                                                                          
                                                                            
                                             
   
   
                                                                         
                                                                        
   
                                     
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/slru.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/shmem.h"
#include "miscadmin.h"

#define SlruFileName(ctl, path, seg) snprintf(path, MAXPGPATH, "%s/%04X", (ctl)->Dir, seg)

   
                                                                         
                                                                          
                                                                        
                                                                             
                         
   
#define MAX_FLUSH_BUFFERS 16

typedef struct SlruFlushData
{
  int num_files;                                           
  int fd[MAX_FLUSH_BUFFERS];                    
  int segno[MAX_FLUSH_BUFFERS];                      
} SlruFlushData;

typedef struct SlruFlushData *SlruFlush;

   
                                                                               
                 
   
                                                                       
                                                                             
                                                                           
                                                                        
   
                                                                               
                                                                              
                                                                               
                                                                  
                                                                               
                                                                            
                                                                            
                                                                            
                                                                     
   
#define SlruRecentlyUsed(shared, slotno)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int new_lru_count = (shared)->cur_lru_count;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (new_lru_count != (shared)->page_lru_count[slotno])                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      (shared)->cur_lru_count = ++new_lru_count;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      (shared)->page_lru_count[slotno] = new_lru_count;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

                                      
typedef enum
{
  SLRU_OPEN_FAILED,
  SLRU_SEEK_FAILED,
  SLRU_READ_FAILED,
  SLRU_WRITE_FAILED,
  SLRU_FSYNC_FAILED,
  SLRU_CLOSE_FAILED
} SlruErrorCause;

static SlruErrorCause slru_errcause;
static int slru_errno;

static void
SimpleLruZeroLSNs(SlruCtl ctl, int slotno);
static void
SimpleLruWaitIO(SlruCtl ctl, int slotno);
static void
SlruInternalWritePage(SlruCtl ctl, int slotno, SlruFlush fdata);
static bool
SlruPhysicalReadPage(SlruCtl ctl, int pageno, int slotno);
static bool
SlruPhysicalWritePage(SlruCtl ctl, int pageno, int slotno, SlruFlush fdata);
static void
SlruReportIOError(SlruCtl ctl, int pageno, TransactionId xid);
static int
SlruSelectLRUPage(SlruCtl ctl, int pageno);

static bool
SlruScanDirCbDeleteCutoff(SlruCtl ctl, char *filename, int segpage, void *data);
static void
SlruInternalDeleteSegment(SlruCtl ctl, char *filename);

   
                                   
   

Size
SimpleLruShmemSize(int nslots, int nlsns)
{
  Size sz;

                                                           
  sz = MAXALIGN(sizeof(SlruSharedData));
  sz += MAXALIGN(nslots * sizeof(char *));                            
  sz += MAXALIGN(nslots * sizeof(SlruPageStatus));                    
  sz += MAXALIGN(nslots * sizeof(bool));                             
  sz += MAXALIGN(nslots * sizeof(int));                               
  sz += MAXALIGN(nslots * sizeof(int));                                  
  sz += MAXALIGN(nslots * sizeof(LWLockPadded));                       

  if (nlsns > 0)
  {
    sz += MAXALIGN(nslots * nlsns * sizeof(XLogRecPtr));                  
  }

  return BUFFERALIGN(sz) + BLCKSZ * nslots;
}

void
SimpleLruInit(SlruCtl ctl, const char *name, int nslots, int nlsns, LWLock *ctllock, const char *subdir, int tranche_id)
{
  SlruShared shared;
  bool found;

  shared = (SlruShared)ShmemInitStruct(name, SimpleLruShmemSize(nslots, nlsns), &found);

  if (!IsUnderPostmaster)
  {
                                                 
    char *ptr;
    Size offset;
    int slotno;

    Assert(!found);

    memset(shared, 0, sizeof(SlruSharedData));

    shared->ControlLock = ctllock;

    shared->num_slots = nslots;
    shared->lsn_groups_per_page = nlsns;

    shared->cur_lru_count = 0;

                                                      

    ptr = (char *)shared;
    offset = MAXALIGN(sizeof(SlruSharedData));
    shared->page_buffer = (char **)(ptr + offset);
    offset += MAXALIGN(nslots * sizeof(char *));
    shared->page_status = (SlruPageStatus *)(ptr + offset);
    offset += MAXALIGN(nslots * sizeof(SlruPageStatus));
    shared->page_dirty = (bool *)(ptr + offset);
    offset += MAXALIGN(nslots * sizeof(bool));
    shared->page_number = (int *)(ptr + offset);
    offset += MAXALIGN(nslots * sizeof(int));
    shared->page_lru_count = (int *)(ptr + offset);
    offset += MAXALIGN(nslots * sizeof(int));

                            
    shared->buffer_locks = (LWLockPadded *)(ptr + offset);
    offset += MAXALIGN(nslots * sizeof(LWLockPadded));

    if (nlsns > 0)
    {
      shared->group_lsn = (XLogRecPtr *)(ptr + offset);
      offset += MAXALIGN(nslots * nlsns * sizeof(XLogRecPtr));
    }

    Assert(strlen(name) + 1 < SLRU_MAX_NAME_LENGTH);
    strlcpy(shared->lwlock_tranche_name, name, SLRU_MAX_NAME_LENGTH);
    shared->lwlock_tranche_id = tranche_id;

    ptr += BUFFERALIGN(offset);
    for (slotno = 0; slotno < nslots; slotno++)
    {
      LWLockInitialize(&shared->buffer_locks[slotno].lock, shared->lwlock_tranche_id);

      shared->page_buffer[slotno] = ptr;
      shared->page_status[slotno] = SLRU_PAGE_EMPTY;
      shared->page_dirty[slotno] = false;
      shared->page_lru_count[slotno] = 0;
      ptr += BLCKSZ;
    }

                                            
    Assert(ptr - (char *)shared <= SimpleLruShmemSize(nslots, nlsns));
  }
  else
  {
    Assert(found);
  }

                                                        
  LWLockRegisterTranche(shared->lwlock_tranche_id, shared->lwlock_tranche_name);

     
                                                                          
                                     
     
  ctl->shared = shared;
  ctl->do_fsync = true;                       
  StrNCpy(ctl->Dir, subdir, sizeof(ctl->Dir));
}

   
                                                  
   
                                                                   
                                                
   
                                                                 
   
int
SimpleLruZeroPage(SlruCtl ctl, int pageno)
{
  SlruShared shared = ctl->shared;
  int slotno;

                                                
  slotno = SlruSelectLRUPage(ctl, pageno);
  Assert(shared->page_status[slotno] == SLRU_PAGE_EMPTY || (shared->page_status[slotno] == SLRU_PAGE_VALID && !shared->page_dirty[slotno]) || shared->page_number[slotno] == pageno);

                                             
  shared->page_number[slotno] = pageno;
  shared->page_status[slotno] = SLRU_PAGE_VALID;
  shared->page_dirty[slotno] = true;
  SlruRecentlyUsed(shared, slotno);

                                
  MemSet(shared->page_buffer[slotno], 0, BLCKSZ);

                                              
  SimpleLruZeroLSNs(ctl, slotno);

                                                      
  shared->latest_page_number = pageno;

  return slotno;
}

   
                                                  
   
                                                                               
                                                                          
                                                                          
                                 
   
                                                         
   
static void
SimpleLruZeroLSNs(SlruCtl ctl, int slotno)
{
  SlruShared shared = ctl->shared;

  if (shared->lsn_groups_per_page > 0)
  {
    MemSet(&shared->group_lsn[slotno * shared->lsn_groups_per_page], 0, shared->lsn_groups_per_page * sizeof(XLogRecPtr));
  }
}

   
                                                                     
                                                                        
                                                                   
   
                                                                 
   
static void
SimpleLruWaitIO(SlruCtl ctl, int slotno)
{
  SlruShared shared = ctl->shared;

                                
  LWLockRelease(shared->ControlLock);
  LWLockAcquire(&shared->buffer_locks[slotno].lock, LW_SHARED);
  LWLockRelease(&shared->buffer_locks[slotno].lock);
  LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

     
                                                                          
                                                                         
                                                                            
                                                                            
                                                                             
                                                            
     
  if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS || shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS)
  {
    if (LWLockConditionalAcquire(&shared->buffer_locks[slotno].lock, LW_SHARED))
    {
                                            
      if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS)
      {
        shared->page_status[slotno] = SLRU_PAGE_EMPTY;
      }
      else                        
      {
        shared->page_status[slotno] = SLRU_PAGE_VALID;
        shared->page_dirty[slotno] = true;
      }
      LWLockRelease(&shared->buffer_locks[slotno].lock);
    }
  }
}

   
                                                               
                                                                   
   
                                                                 
                                                                         
                                                                        
                                                                   
   
                                                                  
                                                                          
   
                                                                       
                                            
   
                                                                 
   
int
SimpleLruReadPage(SlruCtl ctl, int pageno, bool write_ok, TransactionId xid)
{
  SlruShared shared = ctl->shared;

                                                                         
  for (;;)
  {
    int slotno;
    bool ok;

                                                                    
    slotno = SlruSelectLRUPage(ctl, pageno);

                                         
    if (shared->page_number[slotno] == pageno && shared->page_status[slotno] != SLRU_PAGE_EMPTY)
    {
         
                                                                         
                                                                         
         
      if (shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS || (shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS && !write_ok))
      {
        SimpleLruWaitIO(ctl, slotno);
                                                    
        continue;
      }
                                        
      SlruRecentlyUsed(shared, slotno);
      return slotno;
    }

                                                               
    Assert(shared->page_status[slotno] == SLRU_PAGE_EMPTY || (shared->page_status[slotno] == SLRU_PAGE_VALID && !shared->page_dirty[slotno]));

                                 
    shared->page_number[slotno] = pageno;
    shared->page_status[slotno] = SLRU_PAGE_READ_IN_PROGRESS;
    shared->page_dirty[slotno] = false;

                                                                     
    LWLockAcquire(&shared->buffer_locks[slotno].lock, LW_EXCLUSIVE);

                                              
    LWLockRelease(shared->ControlLock);

                     
    ok = SlruPhysicalReadPage(ctl, pageno, slotno);

                                                          
    SimpleLruZeroLSNs(ctl, slotno);

                                                       
    LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

    Assert(shared->page_number[slotno] == pageno && shared->page_status[slotno] == SLRU_PAGE_READ_IN_PROGRESS && !shared->page_dirty[slotno]);

    shared->page_status[slotno] = ok ? SLRU_PAGE_VALID : SLRU_PAGE_EMPTY;

    LWLockRelease(&shared->buffer_locks[slotno].lock);

                                               
    if (!ok)
    {
      SlruReportIOError(ctl, pageno, xid);
    }

    SlruRecentlyUsed(shared, slotno);
    return slotno;
  }
}

   
                                                               
                                                                   
                                                             
   
                                                                  
                                                                          
   
                                                                       
                                            
   
                                                                     
                                                                   
   
int
SimpleLruReadPage_ReadOnly(SlruCtl ctl, int pageno, TransactionId xid)
{
  SlruShared shared = ctl->shared;
  int slotno;

                                                           
  LWLockAcquire(shared->ControlLock, LW_SHARED);

                                          
  for (slotno = 0; slotno < shared->num_slots; slotno++)
  {
    if (shared->page_number[slotno] == pageno && shared->page_status[slotno] != SLRU_PAGE_EMPTY && shared->page_status[slotno] != SLRU_PAGE_READ_IN_PROGRESS)
    {
                                                   
      SlruRecentlyUsed(shared, slotno);
      return slotno;
    }
  }

                                                                       
  LWLockRelease(shared->ControlLock);
  LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

  return SimpleLruReadPage(ctl, pageno, true, xid);
}

   
                                                    
                                                    
   
                                                                          
                                                                         
                                                                        
                                                      
   
                                                                 
   
static void
SlruInternalWritePage(SlruCtl ctl, int slotno, SlruFlush fdata)
{
  SlruShared shared = ctl->shared;
  int pageno = shared->page_number[slotno];
  bool ok;

                                                        
  while (shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS && shared->page_number[slotno] == pageno)
  {
    SimpleLruWaitIO(ctl, slotno);
  }

     
                                                                          
                                   
     
  if (!shared->page_dirty[slotno] || shared->page_status[slotno] != SLRU_PAGE_VALID || shared->page_number[slotno] != pageno)
  {
    return;
  }

     
                                                                            
                                                                      
     
  shared->page_status[slotno] = SLRU_PAGE_WRITE_IN_PROGRESS;
  shared->page_dirty[slotno] = false;

                                                                   
  LWLockAcquire(&shared->buffer_locks[slotno].lock, LW_EXCLUSIVE);

                                            
  LWLockRelease(shared->ControlLock);

                    
  ok = SlruPhysicalWritePage(ctl, pageno, slotno, fdata);

                                                                  
  if (!ok && fdata)
  {
    int i;

    for (i = 0; i < fdata->num_files; i++)
    {
      CloseTransientFile(fdata->fd[i]);
    }
  }

                                                     
  LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

  Assert(shared->page_number[slotno] == pageno && shared->page_status[slotno] == SLRU_PAGE_WRITE_IN_PROGRESS);

                                                        
  if (!ok)
  {
    shared->page_dirty[slotno] = true;
  }

  shared->page_status[slotno] = SLRU_PAGE_VALID;

  LWLockRelease(&shared->buffer_locks[slotno].lock);

                                             
  if (!ok)
  {
    SlruReportIOError(ctl, pageno, InvalidTransactionId);
  }
}

   
                                                           
                                       
   
void
SimpleLruWritePage(SlruCtl ctl, int slotno)
{
  SlruInternalWritePage(ctl, slotno, NULL);
}

   
                                                 
   
                                                                              
                                           
   
bool
SimpleLruDoesPhysicalPageExist(SlruCtl ctl, int pageno)
{
  int segno = pageno / SLRU_PAGES_PER_SEGMENT;
  int rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
  int offset = rpageno * BLCKSZ;
  char path[MAXPGPATH];
  int fd;
  bool result;
  off_t endpos;

  SlruFileName(ctl, path, segno);

  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
                                      
    if (errno == ENOENT)
    {
      return false;
    }

                               
    slru_errcause = SLRU_OPEN_FAILED;
    slru_errno = errno;
    SlruReportIOError(ctl, pageno, 0);
  }

  if ((endpos = lseek(fd, 0, SEEK_END)) < 0)
  {
    slru_errcause = SLRU_SEEK_FAILED;
    slru_errno = errno;
    SlruReportIOError(ctl, pageno, 0);
  }

  result = endpos >= (off_t)(offset + BLCKSZ);

  if (CloseTransientFile(fd))
  {
    slru_errcause = SLRU_CLOSE_FAILED;
    slru_errno = errno;
    return false;
  }

  return result;
}

   
                                                                    
   
                                                                           
                                                                           
                                                                      
   
                                                                     
                                                                       
   
static bool
SlruPhysicalReadPage(SlruCtl ctl, int pageno, int slotno)
{
  SlruShared shared = ctl->shared;
  int segno = pageno / SLRU_PAGES_PER_SEGMENT;
  int rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
  int offset = rpageno * BLCKSZ;
  char path[MAXPGPATH];
  int fd;

  SlruFileName(ctl, path, segno);

     
                                                                       
                                                                         
                                                                
                                                                          
                                                              
     
  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
    if (errno != ENOENT || !InRecovery)
    {
      slru_errcause = SLRU_OPEN_FAILED;
      slru_errno = errno;
      return false;
    }

    ereport(LOG, (errmsg("file \"%s\" doesn't exist, reading as zeroes", path)));
    MemSet(shared->page_buffer[slotno], 0, BLCKSZ);
    return true;
  }

  if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
  {
    slru_errcause = SLRU_SEEK_FAILED;
    slru_errno = errno;
    CloseTransientFile(fd);
    return false;
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_SLRU_READ);
  if (read(fd, shared->page_buffer[slotno], BLCKSZ) != BLCKSZ)
  {
    pgstat_report_wait_end();
    slru_errcause = SLRU_READ_FAILED;
    slru_errno = errno;
    CloseTransientFile(fd);
    return false;
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    slru_errcause = SLRU_CLOSE_FAILED;
    slru_errno = errno;
    return false;
  }

  return true;
}

   
                                               
   
                                                                           
                                                                           
                                                                      
   
                                                                     
                                                                     
                           
   
                                                                          
                   
   
static bool
SlruPhysicalWritePage(SlruCtl ctl, int pageno, int slotno, SlruFlush fdata)
{
  SlruShared shared = ctl->shared;
  int segno = pageno / SLRU_PAGES_PER_SEGMENT;
  int rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
  int offset = rpageno * BLCKSZ;
  char path[MAXPGPATH];
  int fd = -1;

     
                                                                             
                                                                            
                                                                
     
  if (shared->group_lsn != NULL)
  {
       
                                                                         
                                                                       
                                                                           
                                                             
                                 
       
    XLogRecPtr max_lsn;
    int lsnindex, lsnoff;

    lsnindex = slotno * shared->lsn_groups_per_page;
    max_lsn = shared->group_lsn[lsnindex++];
    for (lsnoff = 1; lsnoff < shared->lsn_groups_per_page; lsnoff++)
    {
      XLogRecPtr this_lsn = shared->group_lsn[lsnindex++];

      if (max_lsn < this_lsn)
      {
        max_lsn = this_lsn;
      }
    }

    if (!XLogRecPtrIsInvalid(max_lsn))
    {
         
                                                                   
                                                                      
                                                                  
                                              
         
      START_CRIT_SECTION();
      XLogFlush(max_lsn);
      END_CRIT_SECTION();
    }
  }

     
                                                                
     
  if (fdata)
  {
    int i;

    for (i = 0; i < fdata->num_files; i++)
    {
      if (fdata->segno[i] == segno)
      {
        fd = fdata->fd[i];
        break;
      }
    }
  }

  if (fd < 0)
  {
       
                                                                      
                                                                          
                                                                         
                                                                 
                                                                    
                                                                      
                                                                         
                                                                       
                                                                          
                                                                
                                                            
       
                                                                           
                                                                        
                                                          
       
    SlruFileName(ctl, path, segno);
    fd = OpenTransientFile(path, O_RDWR | O_CREAT | PG_BINARY);
    if (fd < 0)
    {
      slru_errcause = SLRU_OPEN_FAILED;
      slru_errno = errno;
      return false;
    }

    if (fdata)
    {
      if (fdata->num_files < MAX_FLUSH_BUFFERS)
      {
        fdata->fd[fdata->num_files] = fd;
        fdata->segno[fdata->num_files] = segno;
        fdata->num_files++;
      }
      else
      {
           
                                                                   
                                                           
           
        fdata = NULL;
      }
    }
  }

  if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
  {
    slru_errcause = SLRU_SEEK_FAILED;
    slru_errno = errno;
    if (!fdata)
    {
      CloseTransientFile(fd);
    }
    return false;
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_SLRU_WRITE);
  if (write(fd, shared->page_buffer[slotno], BLCKSZ) != BLCKSZ)
  {
    pgstat_report_wait_end();
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    slru_errcause = SLRU_WRITE_FAILED;
    slru_errno = errno;
    if (!fdata)
    {
      CloseTransientFile(fd);
    }
    return false;
  }
  pgstat_report_wait_end();

     
                                                                      
                                                            
     
  if (!fdata)
  {
    pgstat_report_wait_start(WAIT_EVENT_SLRU_SYNC);
    if (ctl->do_fsync && pg_fsync(fd))
    {
      pgstat_report_wait_end();
      slru_errcause = SLRU_FSYNC_FAILED;
      slru_errno = errno;
      CloseTransientFile(fd);
      return false;
    }
    pgstat_report_wait_end();

    if (CloseTransientFile(fd))
    {
      slru_errcause = SLRU_CLOSE_FAILED;
      slru_errno = errno;
      return false;
    }
  }

  return true;
}

   
                                                                    
                                                                            
   
static void
SlruReportIOError(SlruCtl ctl, int pageno, TransactionId xid)
{
  int segno = pageno / SLRU_PAGES_PER_SEGMENT;
  int rpageno = pageno % SLRU_PAGES_PER_SEGMENT;
  int offset = rpageno * BLCKSZ;
  char path[MAXPGPATH];

  SlruFileName(ctl, path, segno);
  errno = slru_errno;
  switch (slru_errcause)
  {
  case SLRU_OPEN_FAILED:
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not access status of transaction %u", xid), errdetail("Could not open file \"%s\": %m.", path)));
    break;
  case SLRU_SEEK_FAILED:
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not access status of transaction %u", xid), errdetail("Could not seek in file \"%s\" to offset %u: %m.", path, offset)));
    break;
  case SLRU_READ_FAILED:
    if (errno)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not access status of transaction %u", xid), errdetail("Could not read from file \"%s\" at offset %u: %m.", path, offset)));
    }
    else
    {
      ereport(ERROR, (errmsg("could not access status of transaction %u", xid), errdetail("Could not read from file \"%s\" at offset %u: read too few bytes.", path, offset)));
    }
    break;
  case SLRU_WRITE_FAILED:
    if (errno)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not access status of transaction %u", xid), errdetail("Could not write to file \"%s\" at offset %u: %m.", path, offset)));
    }
    else
    {
      ereport(ERROR, (errmsg("could not access status of transaction %u", xid), errdetail("Could not write to file \"%s\" at offset %u: wrote too few bytes.", path, offset)));
    }
    break;
  case SLRU_FSYNC_FAILED:
    ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not access status of transaction %u", xid), errdetail("Could not fsync file \"%s\": %m.", path)));
    break;
  case SLRU_CLOSE_FAILED:
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not access status of transaction %u", xid), errdetail("Could not close file \"%s\": %m.", path)));
    break;
  default:
                                  
    elog(ERROR, "unrecognized SimpleLru error cause: %d", (int)slru_errcause);
    break;
  }
}

   
                                                       
   
                                                                    
                                                                      
                                                                       
                                                                       
                                                                         
                                                                        
              
   
                                                                 
   
static int
SlruSelectLRUPage(SlruCtl ctl, int pageno)
{
  SlruShared shared = ctl->shared;

                                            
  for (;;)
  {
    int slotno;
    int cur_count;
    int bestvalidslot = 0;                          
    int best_valid_delta = -1;
    int best_valid_page_number = 0;                          
    int bestinvalidslot = 0;                                 
    int best_invalid_delta = -1;
    int best_invalid_page_number = 0;                          

                                                   
    for (slotno = 0; slotno < shared->num_slots; slotno++)
    {
      if (shared->page_number[slotno] == pageno && shared->page_status[slotno] != SLRU_PAGE_EMPTY)
      {
        return slotno;
      }
    }

       
                                                                      
                                                                         
                                                              
                                                                       
                                                                       
                                                                           
                                                                         
                                                                         
                                                                           
             
       
                                                                       
                                                                  
                                                         
                                                                        
                                                                         
                                        
       
                                                                       
                                                                    
                                                                        
                                                                       
                                                                         
                                                                        
                                               
       
    cur_count = (shared->cur_lru_count)++;
    for (slotno = 0; slotno < shared->num_slots; slotno++)
    {
      int this_delta;
      int this_page_number;

      if (shared->page_status[slotno] == SLRU_PAGE_EMPTY)
      {
        return slotno;
      }
      this_delta = cur_count - shared->page_lru_count[slotno];
      if (this_delta < 0)
      {
           
                                                                 
                                                                   
                                                                  
                                                                    
                                  
           
        shared->page_lru_count[slotno] = cur_count;
        this_delta = 0;
      }
      this_page_number = shared->page_number[slotno];
      if (this_page_number == shared->latest_page_number)
      {
        continue;
      }
      if (shared->page_status[slotno] == SLRU_PAGE_VALID)
      {
        if (this_delta > best_valid_delta || (this_delta == best_valid_delta && ctl->PagePrecedes(this_page_number, best_valid_page_number)))
        {
          bestvalidslot = slotno;
          best_valid_delta = this_delta;
          best_valid_page_number = this_page_number;
        }
      }
      else
      {
        if (this_delta > best_invalid_delta || (this_delta == best_invalid_delta && ctl->PagePrecedes(this_page_number, best_invalid_page_number)))
        {
          bestinvalidslot = slotno;
          best_invalid_delta = this_delta;
          best_invalid_page_number = this_page_number;
        }
      }
    }

       
                                                                         
                                                                    
                                                                         
                                                                          
                                                                
       
    if (best_valid_delta < 0)
    {
      SimpleLruWaitIO(ctl, bestinvalidslot);
      continue;
    }

       
                                                 
       
    if (!shared->page_dirty[bestvalidslot])
    {
      return bestvalidslot;
    }

       
                       
       
    SlruInternalWritePage(ctl, bestvalidslot, NULL);

       
                                                                        
                                                                           
                 
       
  }
}

   
                                                                    
   
void
SimpleLruFlush(SlruCtl ctl, bool allow_redirtied)
{
  SlruShared shared = ctl->shared;
  SlruFlushData fdata;
  int slotno;
  int pageno = 0;
  int i;
  bool ok;

     
                                
     
  fdata.num_files = 0;

  LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

  for (slotno = 0; slotno < shared->num_slots; slotno++)
  {
    SlruInternalWritePage(ctl, slotno, &fdata);

       
                                                                         
                                                                    
                              
       
    Assert(allow_redirtied || shared->page_status[slotno] == SLRU_PAGE_EMPTY || (shared->page_status[slotno] == SLRU_PAGE_VALID && !shared->page_dirty[slotno]));
  }

  LWLockRelease(shared->ControlLock);

     
                                                  
     
  ok = true;
  for (i = 0; i < fdata.num_files; i++)
  {
    pgstat_report_wait_start(WAIT_EVENT_SLRU_FLUSH_SYNC);
    if (ctl->do_fsync && pg_fsync(fdata.fd[i]))
    {
      slru_errcause = SLRU_FSYNC_FAILED;
      slru_errno = errno;
      pageno = fdata.segno[i] * SLRU_PAGES_PER_SEGMENT;
      ok = false;
    }
    pgstat_report_wait_end();

    if (CloseTransientFile(fdata.fd[i]))
    {
      slru_errcause = SLRU_CLOSE_FAILED;
      slru_errno = errno;
      pageno = fdata.segno[i] * SLRU_PAGES_PER_SEGMENT;
      ok = false;
    }
  }
  if (!ok)
  {
    SlruReportIOError(ctl, pageno, InvalidTransactionId);
  }

                                                                
  if (ctl->do_fsync)
  {
    fsync_fname(ctl->Dir, true);
  }
}

   
                                                                     
   
                                                                              
                                                                               
                                                                           
                                                                        
                                                                            
                                                                              
                                              
   
void
SimpleLruTruncate(SlruCtl ctl, int cutoffPage)
{
  SlruShared shared = ctl->shared;
  int slotno;

     
                                                                           
                                                                            
                                                                          
                                                       
     
  LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);

restart:;

     
                                                                        
                                                             
     
  if (ctl->PagePrecedes(shared->latest_page_number, cutoffPage))
  {
    LWLockRelease(shared->ControlLock);
    ereport(LOG, (errmsg("could not truncate directory \"%s\": apparent wraparound", ctl->Dir)));
    return;
  }

  for (slotno = 0; slotno < shared->num_slots; slotno++)
  {
    if (shared->page_status[slotno] == SLRU_PAGE_EMPTY)
    {
      continue;
    }
    if (!ctl->PagePrecedes(shared->page_number[slotno], cutoffPage))
    {
      continue;
    }

       
                                                                     
       
    if (shared->page_status[slotno] == SLRU_PAGE_VALID && !shared->page_dirty[slotno])
    {
      shared->page_status[slotno] = SLRU_PAGE_EMPTY;
      continue;
    }

       
                                                                        
                                                                          
                                                                       
                                                                
                                                                         
                                                                       
                                                                         
                            
       
    if (shared->page_status[slotno] == SLRU_PAGE_VALID)
    {
      SlruInternalWritePage(ctl, slotno, NULL);
    }
    else
    {
      SimpleLruWaitIO(ctl, slotno);
    }
    goto restart;
  }

  LWLockRelease(shared->ControlLock);

                                            
  (void)SlruScanDirectory(ctl, SlruScanDirCbDeleteCutoff, &cutoffPage);
}

   
                                                                  
   
                                                                               
                                                                             
   
static void
SlruInternalDeleteSegment(SlruCtl ctl, char *filename)
{
  char path[MAXPGPATH];

  snprintf(path, MAXPGPATH, "%s/%s", ctl->Dir, filename);
  ereport(DEBUG2, (errmsg("removing file \"%s\"", path)));
  unlink(path);
}

   
                                                                        
   
void
SlruDeleteSegment(SlruCtl ctl, int segno)
{
  SlruShared shared = ctl->shared;
  int slotno;
  char path[MAXPGPATH];
  bool did_write;

                                                                  
  LWLockAcquire(shared->ControlLock, LW_EXCLUSIVE);
restart:
  did_write = false;
  for (slotno = 0; slotno < shared->num_slots; slotno++)
  {
    int pagesegno = shared->page_number[slotno] / SLRU_PAGES_PER_SEGMENT;

    if (shared->page_status[slotno] == SLRU_PAGE_EMPTY)
    {
      continue;
    }

                                           
    if (pagesegno != segno)
    {
      continue;
    }

                                                                       
    if (shared->page_status[slotno] == SLRU_PAGE_VALID && !shared->page_dirty[slotno])
    {
      shared->page_status[slotno] = SLRU_PAGE_EMPTY;
      continue;
    }

                                           
    if (shared->page_status[slotno] == SLRU_PAGE_VALID)
    {
      SlruInternalWritePage(ctl, slotno, NULL);
    }
    else
    {
      SimpleLruWaitIO(ctl, slotno);
    }

    did_write = true;
  }

     
                                                                         
                                                 
     
  if (did_write)
  {
    goto restart;
  }

  snprintf(path, MAXPGPATH, "%s/%04X", ctl->Dir, segno);
  ereport(DEBUG2, (errmsg("removing file \"%s\"", path)));
  unlink(path);

  LWLockRelease(shared->ControlLock);
}

   
                                                  
   
                                                                              
                                                                             
                                                                        
                                   
   
                                      
                                                                       
                                                                           
                                                                              
   
static bool
SlruMayDeleteSegment(SlruCtl ctl, int segpage, int cutoffPage)
{
  int seg_last_page = segpage + SLRU_PAGES_PER_SEGMENT - 1;

  Assert(segpage % SLRU_PAGES_PER_SEGMENT == 0);

  return (ctl->PagePrecedes(segpage, cutoffPage) && ctl->PagePrecedes(seg_last_page, cutoffPage));
}

#ifdef USE_ASSERT_CHECKING
static void
SlruPagePrecedesTestOffset(SlruCtl ctl, int per_page, uint32 offset)
{
  TransactionId lhs, rhs;
  int newestPage, oldestPage;
  TransactionId newestXact, oldestXact;

     
                                                                          
                                                                            
                                                                            
                      
     
  lhs = per_page + offset;                                               
  rhs = lhs + (1U << 31);
  Assert(TransactionIdPrecedes(lhs, rhs));
  Assert(TransactionIdPrecedes(rhs, lhs));
  Assert(!TransactionIdPrecedes(lhs - 1, rhs));
  Assert(TransactionIdPrecedes(rhs, lhs - 1));
  Assert(TransactionIdPrecedes(lhs + 1, rhs));
  Assert(!TransactionIdPrecedes(rhs, lhs + 1));
  Assert(!TransactionIdFollowsOrEquals(lhs, rhs));
  Assert(!TransactionIdFollowsOrEquals(rhs, lhs));
  Assert(!ctl->PagePrecedes(lhs / per_page, lhs / per_page));
  Assert(!ctl->PagePrecedes(lhs / per_page, rhs / per_page));
  Assert(!ctl->PagePrecedes(rhs / per_page, lhs / per_page));
  Assert(!ctl->PagePrecedes((lhs - per_page) / per_page, rhs / per_page));
  Assert(ctl->PagePrecedes(rhs / per_page, (lhs - 3 * per_page) / per_page));
  Assert(ctl->PagePrecedes(rhs / per_page, (lhs - 2 * per_page) / per_page));
  Assert(ctl->PagePrecedes(rhs / per_page, (lhs - 1 * per_page) / per_page) || (1U << 31) % per_page != 0);                                 
  Assert(ctl->PagePrecedes((lhs + 1 * per_page) / per_page, rhs / per_page) || (1U << 31) % per_page != 0);
  Assert(ctl->PagePrecedes((lhs + 2 * per_page) / per_page, rhs / per_page));
  Assert(ctl->PagePrecedes((lhs + 3 * per_page) / per_page, rhs / per_page));
  Assert(!ctl->PagePrecedes(rhs / per_page, (lhs + per_page) / per_page));

     
                                                                            
                                                                        
                          
     
  newestPage = 2 * SLRU_PAGES_PER_SEGMENT - 1;
  newestXact = newestPage * per_page + offset;
  Assert(newestXact / per_page == newestPage);
  oldestXact = newestXact + 1;
  oldestXact -= 1U << 31;
  oldestPage = oldestXact / per_page;
  Assert(!SlruMayDeleteSegment(ctl, (newestPage - newestPage % SLRU_PAGES_PER_SEGMENT), oldestPage));

     
                                                                            
                                                                         
                          
     
  newestPage = SLRU_PAGES_PER_SEGMENT;
  newestXact = newestPage * per_page + offset;
  Assert(newestXact / per_page == newestPage);
  oldestXact = newestXact + 1;
  oldestXact -= 1U << 31;
  oldestPage = oldestXact / per_page;
  Assert(!SlruMayDeleteSegment(ctl, (newestPage - newestPage % SLRU_PAGES_PER_SEGMENT), oldestPage));
}

   
                                      
   
                                                                             
                                                                              
                                                                
                                                                             
                          
   
void
SlruPagePrecedesUnitTests(SlruCtl ctl, int per_page)
{
                                                      
  SlruPagePrecedesTestOffset(ctl, per_page, 0);
  SlruPagePrecedesTestOffset(ctl, per_page, per_page / 2);
  SlruPagePrecedesTestOffset(ctl, per_page, per_page - 1);
}
#endif

   
                              
                                                                          
                                              
   
bool
SlruScanDirCbReportPresence(SlruCtl ctl, char *filename, int segpage, void *data)
{
  int cutoffPage = *(int *)data;

  if (SlruMayDeleteSegment(ctl, segpage, cutoffPage))
  {
    return true;                                        
  }

  return false;                 
}

   
                               
                                                                         
   
static bool
SlruScanDirCbDeleteCutoff(SlruCtl ctl, char *filename, int segpage, void *data)
{
  int cutoffPage = *(int *)data;

  if (SlruMayDeleteSegment(ctl, segpage, cutoffPage))
  {
    SlruInternalDeleteSegment(ctl, filename);
  }

  return false;                 
}

   
                               
                                        
   
bool
SlruScanDirCbDeleteAll(SlruCtl ctl, char *filename, int segpage, void *data)
{
  SlruInternalDeleteSegment(ctl, filename);

  return false;                 
}

   
                                                                               
   
                                                                             
                                  
   
                                                                                
                                                                              
                                                                                
                  
   
                                                                               
   
                                    
   
bool
SlruScanDirectory(SlruCtl ctl, SlruScanCallback callback, void *data)
{
  bool retval = false;
  DIR *cldir;
  struct dirent *clde;
  int segno;
  int segpage;

  cldir = AllocateDir(ctl->Dir);
  while ((clde = ReadDir(cldir, ctl->Dir)) != NULL)
  {
    size_t len;

    len = strlen(clde->d_name);

    if ((len == 4 || len == 5 || len == 6) && strspn(clde->d_name, "0123456789ABCDEF") == len)
    {
      segno = (int)strtol(clde->d_name, NULL, 16);
      segpage = segno * SLRU_PAGES_PER_SEGMENT;

      elog(DEBUG2, "SlruScanDirectory invoking callback on %s/%s", ctl->Dir, clde->d_name);
      retval = callback(ctl, clde->d_name, segpage, data);
      if (retval)
      {
        break;
      }
    }
  }
  FreeDir(cldir);

  return retval;
}
