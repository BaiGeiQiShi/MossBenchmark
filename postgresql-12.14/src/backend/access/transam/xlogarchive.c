                                                                            
   
                 
                                                                      
   
   
                                                                         
                                                                        
   
                                            
   
                                                                            
   

#include "postgres.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "miscadmin.h"
#include "postmaster/startup.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/pmsignal.h"

   
                                                                          
                                                                             
                                                                           
                
   
                                                                           
                                                                              
          
   
                                                                     
                                                                          
                                
   
                                                                              
                                                                             
                                                          
   
bool
RestoreArchivedFile(char *path, const char *xlogfname, const char *recovername, off_t expectedSize, bool cleanupEnabled)
{
  char xlogpath[MAXPGPATH];
  char xlogRestoreCmd[MAXPGPATH];
  char lastRestartPointFname[MAXPGPATH];
  char *dp;
  char *endp;
  const char *sp;
  int rc;
  struct stat stat_buf;
  XLogSegNo restartSegNo;
  XLogRecPtr restartRedoPtr;
  TimeLineID restartTli;

     
                                                                  
                                
     
  if (!ArchiveRecoveryRequested)
  {
    goto not_available;
  }

                                                              
  if (recoveryRestoreCommand == NULL || strcmp(recoveryRestoreCommand, "") == 0)
  {
    goto not_available;
  }

     
                                                                             
                                                                           
                                                                         
                                                                 
     
                                                                       
                                                                           
                                                                     
                                                                           
                                 
     
                                                                             
                                                                            
                                                                  
     
                                                                         
                                                                        
                                                                             
                                                             
                                                                           
                                               
     
  snprintf(xlogpath, MAXPGPATH, XLOGDIR "/%s", recovername);

     
                                                            
     
  if (stat(xlogpath, &stat_buf) != 0)
  {
    if (errno != ENOENT)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", xlogpath)));
    }
  }
  else
  {
    if (unlink(xlogpath) != 0)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", xlogpath)));
    }
  }

     
                                                                         
                                                                            
                                                       
     
                                                                     
                                                                         
                                                                        
                
     
                                                                          
                                                                            
                                                                             
                                                                        
                                                                          
                  
     
  if (cleanupEnabled)
  {
    GetOldestRestartPoint(&restartRedoPtr, &restartTli);
    XLByteToSeg(restartRedoPtr, restartSegNo, wal_segment_size);
    XLogFileName(lastRestartPointFname, restartTli, restartSegNo, wal_segment_size);
                                                                    
    Assert(strcmp(lastRestartPointFname, xlogfname) <= 0);
  }
  else
  {
    XLogFileName(lastRestartPointFname, 0, 0L, wal_segment_size);
  }

     
                                          
     
  dp = xlogRestoreCmd;
  endp = xlogRestoreCmd + MAXPGPATH - 1;
  *endp = '\0';

  for (sp = recoveryRestoreCommand; *sp; sp++)
  {
    if (*sp == '%')
    {
      switch (sp[1])
      {
      case 'p':
                                              
        sp++;
        StrNCpy(dp, xlogpath, endp - dp);
        make_native_path(dp);
        dp += strlen(dp);
        break;
      case 'f':
                                          
        sp++;
        StrNCpy(dp, xlogfname, endp - dp);
        dp += strlen(dp);
        break;
      case 'r':
                                               
        sp++;
        StrNCpy(dp, lastRestartPointFname, endp - dp);
        dp += strlen(dp);
        break;
      case '%':
                                      
        sp++;
        if (dp < endp)
        {
          *dp++ = *sp;
        }
        break;
      default:
                                                  
        if (dp < endp)
        {
          *dp++ = *sp;
        }
        break;
      }
    }
    else
    {
      if (dp < endp)
      {
        *dp++ = *sp;
      }
    }
  }
  *dp = '\0';

  ereport(DEBUG3, (errmsg_internal("executing restore command \"%s\"", xlogRestoreCmd)));

     
                                                                
     
  PreRestoreCommand();

     
                                                
     
  rc = system(xlogRestoreCmd);

  PostRestoreCommand();

  if (rc == 0)
  {
       
                                                                     
                                                  
       
    if (stat(xlogpath, &stat_buf) == 0)
    {
      if (expectedSize > 0 && stat_buf.st_size != expectedSize)
      {
        int elevel;

           
                                                                     
                                                                   
                   
           
                                                                     
                                                                     
                                                                   
                                                              
                                                                       
                               
           
        if (StandbyMode && stat_buf.st_size < expectedSize)
        {
          elevel = DEBUG1;
        }
        else
        {
          elevel = FATAL;
        }
        ereport(elevel, (errmsg("archive file \"%s\" has wrong size: %lu instead of %lu", xlogfname, (unsigned long)stat_buf.st_size, (unsigned long)expectedSize)));
        return false;
      }
      else
      {
        ereport(LOG, (errmsg("restored log file \"%s\" from archive", xlogfname)));
        strcpy(path, xlogpath);
        return true;
      }
    }
    else
    {
                       
      if (errno != ENOENT)
      {
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", xlogpath)));
      }
    }
  }

     
                                                                         
                                                                      
                                                                           
                                                                    
                                                 
     
                                                                         
                                                                             
                                                                       
                                                                      
                                                                         
                                                         
     
                                                                           
                                                                            
                                                                         
                                                                             
                                                                           
                                                                
                   
     
                                                                           
     
  if (wait_result_is_signal(rc, SIGTERM))
  {
    proc_exit(1);
  }

  ereport(wait_result_is_any_signal(rc, true) ? FATAL : DEBUG2, (errmsg("could not restore file \"%s\" from archive: %s", xlogfname, wait_result_to_str(rc))));

not_available:

     
                                                                             
                                                                   
     
                                                                            
                                              
     
  snprintf(path, MAXPGPATH, XLOGDIR "/%s", xlogfname);
  return false;
}

   
                                                                 
   
                                                                     
                                                                      
                                                                         
                                                    
   
                                                                                
   
void
ExecuteRecoveryCommand(const char *command, const char *commandName, bool failOnSignal)
{
  char xlogRecoveryCmd[MAXPGPATH];
  char lastRestartPointFname[MAXPGPATH];
  char *dp;
  char *endp;
  const char *sp;
  int rc;
  XLogSegNo restartSegNo;
  XLogRecPtr restartRedoPtr;
  TimeLineID restartTli;

  Assert(command && commandName);

     
                                                                         
                                                                            
                                                       
     
  GetOldestRestartPoint(&restartRedoPtr, &restartTli);
  XLByteToSeg(restartRedoPtr, restartSegNo, wal_segment_size);
  XLogFileName(lastRestartPointFname, restartTli, restartSegNo, wal_segment_size);

     
                                          
     
  dp = xlogRecoveryCmd;
  endp = xlogRecoveryCmd + MAXPGPATH - 1;
  *endp = '\0';

  for (sp = command; *sp; sp++)
  {
    if (*sp == '%')
    {
      switch (sp[1])
      {
      case 'r':
                                               
        sp++;
        StrNCpy(dp, lastRestartPointFname, endp - dp);
        dp += strlen(dp);
        break;
      case '%':
                                      
        sp++;
        if (dp < endp)
        {
          *dp++ = *sp;
        }
        break;
      default:
                                                  
        if (dp < endp)
        {
          *dp++ = *sp;
        }
        break;
      }
    }
    else
    {
      if (dp < endp)
      {
        *dp++ = *sp;
      }
    }
  }
  *dp = '\0';

  ereport(DEBUG3, (errmsg_internal("executing %s \"%s\"", commandName, command)));

     
                                     
     
  rc = system(xlogRecoveryCmd);
  if (rc != 0)
  {
       
                                                                           
                                                               
       
    ereport((failOnSignal && wait_result_is_any_signal(rc, true)) ? FATAL : WARNING,
                 
                                                                                 
                                                                              
                                                       
        (errmsg("%s \"%s\": %s", commandName, command, wait_result_to_str(rc))));
  }
}

   
                                                                           
                                                                         
                                                                       
   
void
KeepFileRestoredFromArchive(const char *path, const char *xlogfname)
{
  char xlogfpath[MAXPGPATH];
  bool reload = false;
  struct stat statbuf;

  snprintf(xlogfpath, MAXPGPATH, XLOGDIR "/%s", xlogfname);

  if (stat(xlogfpath, &statbuf) == 0)
  {
    char oldpath[MAXPGPATH];

#ifdef WIN32
    static unsigned int deletedcounter = 1;

       
                                                                          
                                                                         
                                                                          
                                                                       
                                                                       
                                                                          
                                                                          
                                                                           
       
    snprintf(oldpath, MAXPGPATH, "%s.deleted%u", xlogfpath, deletedcounter++);
    if (rename(xlogfpath, oldpath) != 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", xlogfpath, oldpath)));
    }
#else
                                                    
    strlcpy(oldpath, xlogfpath, MAXPGPATH);
#endif
    if (unlink(oldpath) != 0)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", xlogfpath)));
    }
    reload = true;
  }

  durable_rename(path, xlogfpath, ERROR);

     
                                                                           
                           
     
  if (XLogArchiveMode != ARCHIVE_MODE_ALWAYS)
  {
    XLogArchiveForceDone(xlogfname);
  }
  else
  {
    XLogArchiveNotify(xlogfname);
  }

     
                                                                             
                                                                            
                                                                           
                                                                            
                              
     
  if (reload)
  {
    WalSndRqstFileReload();
  }

     
                                                                            
                                                                            
             
     
  WalSndWakeup();
}

   
                     
   
                                       
   
                                                                           
                                                                 
                                                                            
                                                                  
   
void
XLogArchiveNotify(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];
  FILE *fd;

                                                          
  StatusFilePath(archiveStatusPath, xlog, ".ready");
  fd = AllocateFile(archiveStatusPath, "w");
  if (fd == NULL)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not create archive status file \"%s\": %m", archiveStatusPath)));
    return;
  }
  if (FreeFile(fd))
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write archive status file \"%s\": %m", archiveStatusPath)));
    return;
  }

                                                     
  if (IsUnderPostmaster)
  {
    SendPostmasterSignal(PMSIGNAL_WAKEN_ARCHIVER);
  }
}

   
                                                                                 
   
void
XLogArchiveNotifySeg(XLogSegNo segno)
{
  char xlog[MAXFNAMELEN];

  XLogFileName(xlog, ThisTimeLineID, segno, wal_segment_size);
  XLogArchiveNotify(xlog);
}

   
                        
   
                                                                              
                                                                        
                  
   
void
XLogArchiveForceDone(const char *xlog)
{
  char archiveReady[MAXPGPATH];
  char archiveDone[MAXPGPATH];
  struct stat stat_buf;
  FILE *fd;

                                  
  StatusFilePath(archiveDone, xlog, ".done");
  if (stat(archiveDone, &stat_buf) == 0)
  {
    return;
  }

                                            
  StatusFilePath(archiveReady, xlog, ".ready");
  if (stat(archiveReady, &stat_buf) == 0)
  {
    (void)durable_rename(archiveReady, archiveDone, WARNING);
    return;
  }

                                                         
  fd = AllocateFile(archiveDone, "w");
  if (fd == NULL)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not create archive status file \"%s\": %m", archiveDone)));
    return;
  }
  if (FreeFile(fd))
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not write archive status file \"%s\": %m", archiveDone)));
    return;
  }
}

   
                        
   
                                                                             
                                                                              
                                                                              
          
   
                                                                         
                                                                 
   
                                                                          
                                                                         
   
bool
XLogArchiveCheckDone(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];
  struct stat stat_buf;

                                                              
  if (!XLogArchivingActive())
  {
    return true;
  }

     
                                                                           
               
     
  if (!XLogArchivingAlways() && GetRecoveryState() == RECOVERY_STATE_ARCHIVE)
  {
    return true;
  }

     
                                                                        
                                                                          
                      
     

                                                                     
  StatusFilePath(archiveStatusPath, xlog, ".done");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

                                                                      
  StatusFilePath(archiveStatusPath, xlog, ".ready");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return false;
  }

                                                                   
  StatusFilePath(archiveStatusPath, xlog, ".done");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

                                         
  XLogArchiveNotify(xlog);
  return false;
}

   
                     
   
                                                             
                                                                        
                                                                        
                                                                           
                                                                           
                                                                 
   
bool
XLogArchiveIsBusy(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];
  struct stat stat_buf;

                                                                     
  StatusFilePath(archiveStatusPath, xlog, ".done");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return false;
  }

                                                                      
  StatusFilePath(archiveStatusPath, xlog, ".ready");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

                                                                   
  StatusFilePath(archiveStatusPath, xlog, ".done");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return false;
  }

     
                                                                        
                                                                           
                         
     
  snprintf(archiveStatusPath, MAXPGPATH, XLOGDIR "/%s", xlog);
  if (stat(archiveStatusPath, &stat_buf) != 0 && errno == ENOENT)
  {
    return false;
  }

  return true;
}

   
                            
   
                                                                    
                                                                        
                                                   
   
                                                                          
                                                                            
                                                
   
bool
XLogArchiveIsReadyOrDone(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];
  struct stat stat_buf;

                                                                     
  StatusFilePath(archiveStatusPath, xlog, ".done");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

                                                                      
  StatusFilePath(archiveStatusPath, xlog, ".ready");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

                                                                   
  StatusFilePath(archiveStatusPath, xlog, ".done");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

  return false;
}

   
                      
   
                                                                             
         
   
bool
XLogArchiveIsReady(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];
  struct stat stat_buf;

  StatusFilePath(archiveStatusPath, xlog, ".ready");
  if (stat(archiveStatusPath, &stat_buf) == 0)
  {
    return true;
  }

  return false;
}

   
                      
   
                                                                      
   
void
XLogArchiveCleanup(const char *xlog)
{
  char archiveStatusPath[MAXPGPATH];

                             
  StatusFilePath(archiveStatusPath, xlog, ".done");
  unlink(archiveStatusPath);
                                         

                                                                      
  StatusFilePath(archiveStatusPath, xlog, ".ready");
  unlink(archiveStatusPath);
                                         
}
