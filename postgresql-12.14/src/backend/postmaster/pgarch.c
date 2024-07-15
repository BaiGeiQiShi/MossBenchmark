                                                                            
   
            
   
                           
   
                                                        
   
                                                
   
                                                     
                                                           
                                                     
   
                                                      
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/fork_process.h"
#include "postmaster/pgarch.h"
#include "postmaster/postmaster.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "utils/guc.h"
#include "utils/ps_status.h"

              
                      
              
   
#define PGARCH_AUTOWAKE_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  60                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                
#define PGARCH_RESTART_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  10                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                       

   
                                                                      
         
   
#define NUM_ARCHIVE_RETRIES 3

   
                                                                  
                               
   
#define NUM_ORPHAN_CLEANUP_RETRIES 3

              
              
              
   
static time_t last_pgarch_start_time;
static time_t last_sigterm_time = 0;

   
                                                                       
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t got_SIGTERM = false;
static volatile sig_atomic_t wakened = false;
static volatile sig_atomic_t ready_to_stop = false;

              
                                       
              
   
#ifdef EXEC_BACKEND
static pid_t
pgarch_forkexec(void);
#endif

NON_EXEC_STATIC void
PgArchiverMain(int argc, char *argv[]) pg_attribute_noreturn();
static void pgarch_exit(SIGNAL_ARGS);
static void ArchSigHupHandler(SIGNAL_ARGS);
static void ArchSigTermHandler(SIGNAL_ARGS);
static void pgarch_waken(SIGNAL_ARGS);
static void pgarch_waken_stop(SIGNAL_ARGS);
static void
pgarch_MainLoop(void);
static void
pgarch_ArchiverCopyLoop(void);
static bool
pgarch_archiveXlog(char *xlog);
static bool
pgarch_readyXlog(char *xlog);
static void
pgarch_archiveDone(char *xlog);

                                                                
                                                  
                                                                
   

   
                
   
                                                                   
                                                       
   
                                               
   
                                                                         
   
int
pgarch_start(void)
{
  time_t curtime;
  pid_t pgArchPid;

     
                                      
     
  if (!XLogArchivingActive())
  {
    return 0;
  }

     
                                                                         
                                                                             
                                                                            
                                                                 
     
  curtime = time(NULL);
  if ((unsigned int)(curtime - last_pgarch_start_time) < (unsigned int)PGARCH_RESTART_INTERVAL)
  {
    return 0;
  }
  last_pgarch_start_time = curtime;

#ifdef EXEC_BACKEND
  switch ((pgArchPid = pgarch_forkexec()))
#else
  switch ((pgArchPid = fork_process()))
#endif
  {
  case -1:
    ereport(LOG, (errmsg("could not fork archiver: %m")));
    return 0;

#ifndef EXEC_BACKEND
  case 0:
                                 
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(false);

                                                                    
    dsm_detach_all();
    PGSharedMemoryDetach();

    PgArchiverMain(0, NULL);
    break;
#endif

  default:
    return (int)pgArchPid;
  }

                          
  return 0;
}

                                                                
                                             
                                                                
   

#ifdef EXEC_BACKEND

   
                       
   
                                                                  
   
static pid_t
pgarch_forkexec(void)
{
  char *av[10];
  int ac = 0;

  av[ac++] = "postgres";

  av[ac++] = "--forkarch";

  av[ac++] = NULL;                                       

  av[ac] = NULL;
  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}
#endif                   

   
                  
   
                                                                           
                                                
   
NON_EXEC_STATIC void
PgArchiverMain(int argc, char *argv[])
{
     
                                                                        
                                                                
     
  pqsignal(SIGHUP, ArchSigHupHandler);
  pqsignal(SIGINT, SIG_IGN);
  pqsignal(SIGTERM, ArchSigTermHandler);
  pqsignal(SIGQUIT, pgarch_exit);
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, pgarch_waken);
  pqsignal(SIGUSR2, pgarch_waken_stop);
                                                                       
  pqsignal(SIGCHLD, SIG_DFL);
  PG_SETMASK(&UnBlockSig);

     
                            
     
  init_ps_display("archiver", "", "", "");

  pgarch_MainLoop();

  exit(0);
}

                                                 
static void
pgarch_exit(SIGNAL_ARGS)
{
     
                                                                            
                                                                           
                      
     
                                                                        
                                                                            
                                                                      
     
  _exit(2);
}

                                                
static void
ArchSigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

                                                               
  got_SIGHUP = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                 
static void
ArchSigTermHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                                                         
                                                                           
                                                                          
                       
     
  got_SIGTERM = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                 
static void
pgarch_waken(SIGNAL_ARGS)
{
  int save_errno = errno;

                                              
  wakened = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                 
static void
pgarch_waken_stop(SIGNAL_ARGS)
{
  int save_errno = errno;

                                                             
  ready_to_stop = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                   
   
                          
   
static void
pgarch_MainLoop(void)
{
  pg_time_t last_copy_time = 0;
  bool time_to_stop;

     
                                                                    
                                                                           
                                                                      
                                
     
  wakened = true;

     
                                                                             
                                                                         
                                                             
     
  do
  {
    ResetLatch(MyLatch);

                                                                      
    time_to_stop = ready_to_stop;

                                 
    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
    }

       
                                                                          
                                                                    
                                                                      
                                                                          
                                                               
       
    if (got_SIGTERM)
    {
      time_t curtime = time(NULL);

      if (last_sigterm_time == 0)
      {
        last_sigterm_time = curtime;
      }
      else if ((unsigned int)(curtime - last_sigterm_time) >= (unsigned int)60)
      {
        break;
      }
    }

                                
    if (wakened || time_to_stop)
    {
      wakened = false;
      pgarch_ArchiverCopyLoop();
      last_copy_time = time(NULL);
    }

       
                                                                      
                                                                       
                              
       
    if (!time_to_stop)                                       
    {
      pg_time_t curtime = (pg_time_t)time(NULL);
      int timeout;

      timeout = PGARCH_AUTOWAKE_INTERVAL - (curtime - last_copy_time);
      if (timeout > 0)
      {
        int rc;

        rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, timeout * 1000L, WAIT_EVENT_ARCHIVER_MAIN);
        if (rc & WL_TIMEOUT)
        {
          wakened = true;
        }
        if (rc & WL_POSTMASTER_DEATH)
        {
          time_to_stop = true;
        }
      }
      else
      {
        wakened = true;
      }
    }

       
                                                                         
                                                                    
                
       
  } while (!time_to_stop);
}

   
                           
   
                                               
   
static void
pgarch_ArchiverCopyLoop(void)
{
  char xlog[MAX_XFN_CHARS + 1];

     
                                                                      
                                                                             
                                                                            
                                                 
     
  while (pgarch_readyXlog(xlog))
  {
    int failures = 0;
    int failures_orphan = 0;

    for (;;)
    {
      struct stat stat_buf;
      char pathname[MAXPGPATH];

         
                                                                   
                                                                      
                                                                        
                                                                    
                                                 
         
      if (got_SIGTERM || !PostmasterIsAlive())
      {
        return;
      }

         
                                                                     
                                                                        
                                               
         
      if (got_SIGHUP)
      {
        got_SIGHUP = false;
        ProcessConfigFile(PGC_SIGHUP);
      }

                                               
      if (!XLogArchiveCommandSet())
      {
        ereport(WARNING, (errmsg("archive_mode enabled, yet archive_command is not set")));
        return;
      }

         
                                                                         
                                                                         
                                                                    
                                                                        
                                                                       
                                                               
                     
         
      snprintf(pathname, MAXPGPATH, XLOGDIR "/%s", xlog);
      if (stat(pathname, &stat_buf) != 0 && errno == ENOENT)
      {
        char xlogready[MAXPGPATH];

        StatusFilePath(xlogready, xlog, ".ready");
        if (unlink(xlogready) == 0)
        {
          ereport(WARNING, (errmsg("removed orphan archive status file \"%s\"", xlogready)));

                                                           
          break;
        }

        if (++failures_orphan >= NUM_ORPHAN_CLEANUP_RETRIES)
        {
          ereport(WARNING, (errmsg("removal of orphan archive status file \"%s\" failed too many times, will try again later", xlogready)));

                                                      
          return;
        }

                                        
        pg_usleep(1000000L);
        continue;
      }

      if (pgarch_archiveXlog(xlog))
      {
                        
        pgarch_archiveDone(xlog);

           
                                                                      
                    
           
        pgstat_send_archiver(xlog, false);

        break;                              
      }
      else
      {
           
                                                                   
                   
           
        pgstat_send_archiver(xlog, true);

        if (++failures >= NUM_ARCHIVE_RETRIES)
        {
          ereport(WARNING, (errmsg("archiving write-ahead log file \"%s\" failed too many times, will try again later", xlog)));
          return;                                
        }
        pg_usleep(1000000L);                                 
      }
    }
  }
}

   
                      
   
                                                                       
   
                              
   
static bool
pgarch_archiveXlog(char *xlog)
{
  char xlogarchcmd[MAXPGPATH];
  char pathname[MAXPGPATH];
  char activitymsg[MAXFNAMELEN + 16];
  char *dp;
  char *endp;
  const char *sp;
  int rc;

  snprintf(pathname, MAXPGPATH, XLOGDIR "/%s", xlog);

     
                                          
     
  dp = xlogarchcmd;
  endp = xlogarchcmd + MAXPGPATH - 1;
  *endp = '\0';

  for (sp = XLogArchiveCommand; *sp; sp++)
  {
    if (*sp == '%')
    {
      switch (sp[1])
      {
      case 'p':
                                              
        sp++;
        strlcpy(dp, pathname, endp - dp);
        make_native_path(dp);
        dp += strlen(dp);
        break;
      case 'f':
                                         
        sp++;
        strlcpy(dp, xlog, endp - dp);
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

  ereport(DEBUG3, (errmsg_internal("executing archive command \"%s\"", xlogarchcmd)));

                                             
  snprintf(activitymsg, sizeof(activitymsg), "archiving %s", xlog);
  set_ps_display(activitymsg, false);

  rc = system(xlogarchcmd);
  if (rc != 0)
  {
       
                                                                          
                                                                           
                                                                        
                                                                         
                                                                       
                                                                
       
    int lev = wait_result_is_any_signal(rc, true) ? FATAL : LOG;

    if (WIFEXITED(rc))
    {
      ereport(lev, (errmsg("archive command failed with exit code %d", WEXITSTATUS(rc)), errdetail("The failed archive command was: %s", xlogarchcmd)));
    }
    else if (WIFSIGNALED(rc))
    {
#if defined(WIN32)
      ereport(lev, (errmsg("archive command was terminated by exception 0x%X", WTERMSIG(rc)), errhint("See C include file \"ntstatus.h\" for a description of the hexadecimal value."), errdetail("The failed archive command was: %s", xlogarchcmd)));
#else
      ereport(lev, (errmsg("archive command was terminated by signal %d: %s", WTERMSIG(rc), pg_strsignal(WTERMSIG(rc))), errdetail("The failed archive command was: %s", xlogarchcmd)));
#endif
    }
    else
    {
      ereport(lev, (errmsg("archive command exited with unrecognized status %d", rc), errdetail("The failed archive command was: %s", xlogarchcmd)));
    }

    snprintf(activitymsg, sizeof(activitymsg), "failed on %s", xlog);
    set_ps_display(activitymsg, false);

    return false;
  }
  elog(DEBUG1, "archived write-ahead log file \"%s\"", xlog);

  snprintf(activitymsg, sizeof(activitymsg), "last was %s", xlog);
  set_ps_display(activitymsg, false);

  return true;
}

   
                    
   
                                                                       
                                                                     
                                                                  
                                                               
                                                       
   
                                                                       
                                                     
                                                                      
                                                                
                                   
   
                                                                             
                                                                             
                                                                          
                                                                        
                                                                             
             
   
static bool
pgarch_readyXlog(char *xlog)
{
     
                                                                             
                                                                          
                                                                           
                      
     
  char XLogArchiveStatusDir[MAXPGPATH];
  DIR *rldir;
  struct dirent *rlde;
  bool found = false;
  bool historyFound = false;

  snprintf(XLogArchiveStatusDir, MAXPGPATH, XLOGDIR "/archive_status");
  rldir = AllocateDir(XLogArchiveStatusDir);

  while ((rlde = ReadDir(rldir, XLogArchiveStatusDir)) != NULL)
  {
    int basenamelen = (int)strlen(rlde->d_name) - 6;
    char basename[MAX_XFN_CHARS + 1];
    bool ishistory;

                                                             
    if (basenamelen < MIN_XFN_CHARS || basenamelen > MAX_XFN_CHARS)
    {
      continue;
    }

                                                   
    if (strspn(rlde->d_name, VALID_XFN_CHARS) < basenamelen)
    {
      continue;
    }

                                                  
    if (strcmp(rlde->d_name + basenamelen, ".ready") != 0)
    {
      continue;
    }

                                 
    memcpy(basename, rlde->d_name, basenamelen);
    basename[basenamelen] = '\0';

                                 
    ishistory = IsTLHistoryFileName(basename);

       
                                                                    
                                                                      
                                                                           
                                                                           
                                                                      
                          
       
    if (!found || (ishistory && !historyFound))
    {
      strcpy(xlog, basename);
      found = true;
      historyFound = ishistory;
    }
    else if (ishistory || !historyFound)
    {
      if (strcmp(basename, xlog) < 0)
      {
        strcpy(xlog, basename);
      }
    }
  }
  FreeDir(rldir);

  return found;
}

   
                      
   
                                                                       
                                                                      
                                                                         
                                           
   
static void
pgarch_archiveDone(char *xlog)
{
  char rlogready[MAXPGPATH];
  char rlogdone[MAXPGPATH];

  StatusFilePath(rlogready, xlog, ".ready");
  StatusFilePath(rlogdone, xlog, ".done");
  (void)durable_rename(rlogready, rlogdone, WARNING);
}
