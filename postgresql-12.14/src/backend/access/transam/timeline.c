                                                                            
   
              
                                                              
   
                                                                          
                                                                        
   
                                                                          
                                                                            
                              
   
                                                       
   
                                      
   
                                       
                                                                        
                                                                      
   
                                                                              
                                              
   
                                                                         
                                                                        
   
                                         
   
                                                                            
   

#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "access/timeline.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xlogdefs.h"
#include "pgstat.h"
#include "storage/fd.h"

   
                                                                         
                           
   
void
restoreTimeLineHistoryFiles(TimeLineID begin, TimeLineID end)
{
  char path[MAXPGPATH];
  char histfname[MAXFNAMELEN];
  TimeLineID tli;

  for (tli = begin; tli < end; tli++)
  {
    if (tli == 1)
    {
      continue;
    }

    TLHistoryFileName(histfname, tli);
    if (RestoreArchivedFile(path, histfname, "RECOVERYHISTORY", 0, false))
    {
      KeepFileRestoredFromArchive(path, histfname);
    }
  }
}

   
                                          
   
                                                                               
                                                                           
                                                                             
       
   
List *
readTimeLineHistory(TimeLineID targetTLI)
{
  List *result;
  char path[MAXPGPATH];
  char histfname[MAXFNAMELEN];
  FILE *fd;
  TimeLineHistoryEntry *entry;
  TimeLineID lasttli = 0;
  XLogRecPtr prevend;
  bool fromArchive = false;

                                                                    
  if (targetTLI == 1)
  {
    entry = (TimeLineHistoryEntry *)palloc(sizeof(TimeLineHistoryEntry));
    entry->tli = targetTLI;
    entry->begin = entry->end = InvalidXLogRecPtr;
    return list_make1(entry);
  }

  if (ArchiveRecoveryRequested)
  {
    TLHistoryFileName(histfname, targetTLI);
    fromArchive = RestoreArchivedFile(path, histfname, "RECOVERYHISTORY", 0, false);
  }
  else
  {
    TLHistoryFilePath(path, targetTLI);
  }

  fd = AllocateFile(path, "r");
  if (fd == NULL)
  {
    if (errno != ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
    }
                                         
    entry = (TimeLineHistoryEntry *)palloc(sizeof(TimeLineHistoryEntry));
    entry->tli = targetTLI;
    entry->begin = entry->end = InvalidXLogRecPtr;
    return list_make1(entry);
  }

  result = NIL;

     
                       
     
  prevend = InvalidXLogRecPtr;
  for (;;)
  {
    char fline[MAXPGPATH];
    char *res;
    char *ptr;
    TimeLineID tli;
    uint32 switchpoint_hi;
    uint32 switchpoint_lo;
    int nfields;

    pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_READ);
    res = fgets(fline, sizeof(fline), fd);
    pgstat_report_wait_end();
    if (res == NULL)
    {
      if (ferror(fd))
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
      }

      break;
    }

                                                         
    for (ptr = fline; *ptr; ptr++)
    {
      if (!isspace((unsigned char)*ptr))
      {
        break;
      }
    }
    if (*ptr == '\0' || *ptr == '#')
    {
      continue;
    }

    nfields = sscanf(fline, "%u\t%X/%X", &tli, &switchpoint_hi, &switchpoint_lo);

    if (nfields < 1)
    {
                                                               
      ereport(FATAL, (errmsg("syntax error in history file: %s", fline), errhint("Expected a numeric timeline ID.")));
    }
    if (nfields != 3)
    {
      ereport(FATAL, (errmsg("syntax error in history file: %s", fline), errhint("Expected a write-ahead log switchpoint location.")));
    }

    if (result && tli <= lasttli)
    {
      ereport(FATAL, (errmsg("invalid data in history file: %s", fline), errhint("Timeline IDs must be in increasing sequence.")));
    }

    lasttli = tli;

    entry = (TimeLineHistoryEntry *)palloc(sizeof(TimeLineHistoryEntry));
    entry->tli = tli;
    entry->begin = prevend;
    entry->end = ((uint64)(switchpoint_hi)) << 32 | (uint64)switchpoint_lo;
    prevend = entry->end;

                                           
    result = lcons(entry, result);

                                              
  }

  FreeFile(fd);

  if (result && targetTLI <= lasttli)
  {
    ereport(FATAL, (errmsg("invalid data in history file \"%s\"", path), errhint("Timeline IDs must be less than child timeline's ID.")));
  }

     
                                                                             
                          
     
  entry = (TimeLineHistoryEntry *)palloc(sizeof(TimeLineHistoryEntry));
  entry->tli = targetTLI;
  entry->begin = prevend;
  entry->end = InvalidXLogRecPtr;

  result = lcons(entry, result);

     
                                                                         
                       
     
  if (fromArchive)
  {
    KeepFileRestoredFromArchive(path, histfname);
  }

  return result;
}

   
                                                                          
   
bool
existsTimeLineHistory(TimeLineID probeTLI)
{
  char path[MAXPGPATH];
  char histfname[MAXFNAMELEN];
  FILE *fd;

                                                                    
  if (probeTLI == 1)
  {
    return false;
  }

  if (ArchiveRecoveryRequested)
  {
    TLHistoryFileName(histfname, probeTLI);
    RestoreArchivedFile(path, histfname, "RECOVERYHISTORY", 0, false);
  }
  else
  {
    TLHistoryFilePath(path, probeTLI);
  }

  fd = AllocateFile(path, "r");
  if (fd != NULL)
  {
    FreeFile(fd);
    return true;
  }
  else
  {
    if (errno != ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
    }
    return false;
  }
}

   
                                                                     
   
                                                                        
                                                                      
                                                
   
TimeLineID
findNewestTimeLine(TimeLineID startTLI)
{
  TimeLineID newestTLI;
  TimeLineID probeTLI;

     
                                                                          
                                                             
     
  newestTLI = startTLI;

  for (probeTLI = startTLI + 1;; probeTLI++)
  {
    if (existsTimeLineHistory(probeTLI))
    {
      newestTLI = probeTLI;                      
    }
    else
    {
                                            
      break;
    }
  }

  return newestTLI;
}

   
                                       
   
                                  
                                         
                                                                           
                                                                       
   
                                                                                
                                                                            
                           
   
void
writeTimeLineHistory(TimeLineID newTLI, TimeLineID parentTLI, XLogRecPtr switchpoint, char *reason)
{
  char path[MAXPGPATH];
  char tmppath[MAXPGPATH];
  char histfname[MAXFNAMELEN];
  char buffer[BLCKSZ];
  int srcfd;
  int fd;
  int nbytes;

  Assert(newTLI > parentTLI);                                   

     
                                  
     
  snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int)getpid());

  unlink(tmppath);

                                                                            
  fd = OpenTransientFile(tmppath, O_RDWR | O_CREAT | O_EXCL);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmppath)));
  }

     
                                                               
     
  if (ArchiveRecoveryRequested)
  {
    TLHistoryFileName(histfname, parentTLI);
    RestoreArchivedFile(path, histfname, "RECOVERYHISTORY", 0, false);
  }
  else
  {
    TLHistoryFilePath(path, parentTLI);
  }

  srcfd = OpenTransientFile(path, O_RDONLY);
  if (srcfd < 0)
  {
    if (errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
    }
                                                    
  }
  else
  {
    for (;;)
    {
      errno = 0;
      pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_READ);
      nbytes = (int)read(srcfd, buffer, sizeof(buffer));
      pgstat_report_wait_end();
      if (nbytes < 0 || errno != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
      }
      if (nbytes == 0)
      {
        break;
      }
      errno = 0;
      pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_WRITE);
      if ((int)write(fd, buffer, nbytes) != nbytes)
      {
        int save_errno = errno;

           
                                                                  
                 
           
        unlink(tmppath);

           
                                                                      
           
        errno = save_errno ? save_errno : ENOSPC;

        ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
      }
      pgstat_report_wait_end();
    }

    if (CloseTransientFile(srcfd))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
    }
  }

     
                                                              
     
                                                                            
                                         
     
  snprintf(buffer, sizeof(buffer), "%s%u\t%X/%X\t%s\n", (srcfd < 0) ? "" : "\n", parentTLI, (uint32)(switchpoint >> 32), (uint32)(switchpoint), reason);

  nbytes = strlen(buffer);
  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_WRITE);
  if ((int)write(fd, buffer, nbytes) != nbytes)
  {
    int save_errno = errno;

       
                                                                    
       
    unlink(tmppath);
                                                                    
    errno = save_errno ? save_errno : ENOSPC;

    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
  }

     
                                                                         
     
  TLHistoryFilePath(path, newTLI);

     
                                                                            
                                                            
     
  durable_link_or_rename(tmppath, path, ERROR);

                                                     
  if (XLogArchivingActive())
  {
    TLHistoryFileName(histfname, newTLI);
    XLogArchiveNotify(histfname);
  }
}

   
                                                          
   
                                                                            
                                                                              
                                    
   
void
writeTimeLineHistoryFile(TimeLineID tli, char *content, int size)
{
  char path[MAXPGPATH];
  char tmppath[MAXPGPATH];
  int fd;

     
                                  
     
  snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int)getpid());

  unlink(tmppath);

                                                                            
  fd = OpenTransientFile(tmppath, O_RDWR | O_CREAT | O_EXCL);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", tmppath)));
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_FILE_WRITE);
  if ((int)write(fd, content, size) != size)
  {
    int save_errno = errno;

       
                                                                    
       
    unlink(tmppath);
                                                                    
    errno = save_errno ? save_errno : ENOSPC;

    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  pgstat_report_wait_start(WAIT_EVENT_TIMELINE_HISTORY_FILE_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
  }

     
                                                                         
     
  TLHistoryFilePath(path, tli);

     
                                                                            
                                                            
     
  durable_link_or_rename(tmppath, path, ERROR);
}

   
                                                                    
   
bool
tliInHistory(TimeLineID tli, List *expectedTLEs)
{
  ListCell *cell;

  foreach (cell, expectedTLEs)
  {
    if (((TimeLineHistoryEntry *)lfirst(cell))->tli == tli)
    {
      return true;
    }
  }

  return false;
}

   
                                                                           
                               
   
TimeLineID
tliOfPointInHistory(XLogRecPtr ptr, List *history)
{
  ListCell *cell;

  foreach (cell, history)
  {
    TimeLineHistoryEntry *tle = (TimeLineHistoryEntry *)lfirst(cell);

    if ((XLogRecPtrIsInvalid(tle->begin) || tle->begin <= ptr) && (XLogRecPtrIsInvalid(tle->end) || ptr < tle->end))
    {
                    
      return tle->tli;
    }
  }

                         
  elog(ERROR, "timeline history was not contiguous");
  return 0;                          
}

   
                                                                          
                                                                            
                                                                             
                                                                  
   
XLogRecPtr
tliSwitchPoint(TimeLineID tli, List *history, TimeLineID *nextTLI)
{
  ListCell *cell;

  if (nextTLI)
  {
    *nextTLI = 0;
  }
  foreach (cell, history)
  {
    TimeLineHistoryEntry *tle = (TimeLineHistoryEntry *)lfirst(cell);

    if (tle->tli == tli)
    {
      return tle->end;
    }
    if (nextTLI)
    {
      *nextTLI = tle->tli;
    }
  }

  ereport(ERROR, (errmsg("requested timeline %u is not in this server's history", tli)));
  return InvalidXLogRecPtr;                          
}
