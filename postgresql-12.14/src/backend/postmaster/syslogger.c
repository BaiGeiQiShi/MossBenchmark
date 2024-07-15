                                                                            
   
               
   
                                                                          
                                                                       
                                                                 
                                                                        
                                                                  
                                                                 
                                                              
                                                            
   
                                                     
   
                                                                
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "common/file_perm.h"
#include "lib/stringinfo.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "pgstat.h"
#include "pgtime.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "storage/dsm.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pg_shmem.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "utils/timestamp.h"

   
                                                                              
                                                                             
                              
   
#define READ_BUF_SIZE (2 * PIPE_CHUNK_SIZE)

                                                        
#define LOGROTATE_SIGNAL_FILE "logrotate"

   
                                                                         
                                             
   
bool Logging_collector = false;
int Log_RotationAge = HOURS_PER_DAY * MINS_PER_HOUR;
int Log_RotationSize = 10 * 1024;
char *Log_directory = NULL;
char *Log_filename = NULL;
bool Log_truncate_on_rotation = false;
int Log_file_mode = S_IRUSR | S_IWUSR;

   
                                           
   
bool am_syslogger = false;

extern bool redirection_done;

   
                 
   
static pg_time_t next_rotation_time;
static bool pipe_eof_seen = false;
static bool rotation_disabled = false;
static FILE *syslogFile = NULL;
static FILE *csvlogFile = NULL;
NON_EXEC_STATIC pg_time_t first_syslogger_file_time = 0;
static char *last_file_name = NULL;
static char *last_csv_file_name = NULL;

   
                                                                
   
                                                                            
                                                                          
                                                                          
                                                                    
   
                                                                          
                                                                   
   
typedef struct
{
  int32 pid;                                      
  StringInfoData data;                                        
} save_buffer;

#define NBUFFER_LISTS 256
static List *buffer_lists[NBUFFER_LISTS];

                                                               
#ifndef WIN32
int syslogPipe[2] = {-1, -1};
#else
HANDLE syslogPipe[2] = {0, 0};
#endif

#ifdef WIN32
static HANDLE threadHandle = 0;
static CRITICAL_SECTION sysloggerSection;
#endif

   
                                                                       
   
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t rotation_requested = false;

                       
#ifdef EXEC_BACKEND
static pid_t
syslogger_forkexec(void);
static void
syslogger_parseArgs(int argc, char *argv[]);
#endif
NON_EXEC_STATIC void
SysLoggerMain(int argc, char *argv[]) pg_attribute_noreturn();
static void
process_pipe_input(char *logbuffer, int *bytes_in_logbuffer);
static void
flush_pipe_input(char *logbuffer, int *bytes_in_logbuffer);
static FILE *
logfile_open(const char *filename, const char *mode, bool allow_errors);

#ifdef WIN32
static unsigned int __stdcall pipeThread(void *arg);
#endif
static void
logfile_rotate(bool time_based_rotation, int size_rotation_for);
static char *
logfile_getname(pg_time_t timestamp, const char *suffix);
static void
set_next_rotation_time(void);
static void sigHupHandler(SIGNAL_ARGS);
static void sigUsr1Handler(SIGNAL_ARGS);
static void
update_metainfo_datafile(void);

   
                                          
                                                             
   
NON_EXEC_STATIC void
SysLoggerMain(int argc, char *argv[])
{
#ifndef WIN32
  char logbuffer[READ_BUF_SIZE];
  int bytes_in_logbuffer = 0;
#endif
  char *currentLogDir;
  char *currentLogFilename;
  int currentLogRotationAge;
  pg_time_t now;
  WaitEventSet *wes;

  now = MyStartTime;

#ifdef EXEC_BACKEND
  syslogger_parseArgs(argc, argv);
#endif                   

  am_syslogger = true;

  init_ps_display("logger", "", "", "");

     
                                                                          
                                                                     
                                                                          
                                                                           
                                                                   
     
  if (redirection_done)
  {
    int fd = open(DEVNULL, O_WRONLY, 0);

       
                                                                        
                                                                       
                                                                         
                                                            
       
                                                                         
                                                                          
                                                                        
                                                     
       
    close(fileno(stdout));
    close(fileno(stderr));
    if (fd != -1)
    {
      (void)dup2(fd, fileno(stdout));
      (void)dup2(fd, fileno(stderr));
      close(fd);
    }
  }

     
                                                                            
                                                               
                         
     
#ifdef WIN32
  else
  {
    _setmode(_fileno(stderr), _O_TEXT);
  }
#endif

     
                                                                          
                                                                             
                                             
     
#ifndef WIN32
  if (syslogPipe[1] >= 0)
  {
    close(syslogPipe[1]);
  }
  syslogPipe[1] = -1;
#else
  if (syslogPipe[1])
  {
    CloseHandle(syslogPipe[1]);
  }
  syslogPipe[1] = 0;
#endif

     
                                                                    
     
                                                                             
                                                                             
                        
     

  pqsignal(SIGHUP, sigHupHandler);                                   
  pqsignal(SIGINT, SIG_IGN);
  pqsignal(SIGTERM, SIG_IGN);
  pqsignal(SIGQUIT, SIG_IGN);
  pqsignal(SIGALRM, SIG_IGN);
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, sigUsr1Handler);                           
  pqsignal(SIGUSR2, SIG_IGN);

     
                                                                     
     
  pqsignal(SIGCHLD, SIG_DFL);

  PG_SETMASK(&UnBlockSig);

#ifdef WIN32
                                             
  InitializeCriticalSection(&sysloggerSection);
  EnterCriticalSection(&sysloggerSection);

  threadHandle = (HANDLE)_beginthreadex(NULL, 0, pipeThread, NULL, 0, NULL);
  if (threadHandle == 0)
  {
    elog(FATAL, "could not create syslogger data transfer thread: %m");
  }
#endif            

     
                                                                             
                                                                        
                                                         
     
  last_file_name = logfile_getname(first_syslogger_file_time, NULL);
  if (csvlogFile != NULL)
  {
    last_csv_file_name = logfile_getname(first_syslogger_file_time, ".csv");
  }

                                          
  currentLogDir = pstrdup(Log_directory);
  currentLogFilename = pstrdup(Log_filename);
  currentLogRotationAge = Log_RotationAge;
                                      
  set_next_rotation_time();
  update_metainfo_datafile();

     
                                                                            
                                                                             
                            
     
  whereToSendOutput = DestNone;

     
                                                                            
                                         
     
                                                                          
                                                                             
                                                                       
                                                                    
                                 
     
  wes = CreateWaitEventSet(CurrentMemoryContext, 2);
  AddWaitEventToSet(wes, WL_LATCH_SET, PGINVALID_SOCKET, MyLatch, NULL);
#ifndef WIN32
  AddWaitEventToSet(wes, WL_SOCKET_READABLE, syslogPipe[0], NULL, NULL);
#endif

                        
  for (;;)
  {
    bool time_based_rotation = false;
    int size_rotation_for = 0;
    long cur_timeout;
    WaitEvent event;

#ifndef WIN32
    int rc;
#endif

                                           
    ResetLatch(MyLatch);

       
                                                          
       
    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);

         
                                                                   
                                                                   
                                                  
         
      if (strcmp(Log_directory, currentLogDir) != 0)
      {
        pfree(currentLogDir);
        currentLogDir = pstrdup(Log_directory);
        rotation_requested = true;

           
                                                                    
           
        (void)MakePGDirectory(Log_directory);
      }
      if (strcmp(Log_filename, currentLogFilename) != 0)
      {
        pfree(currentLogFilename);
        currentLogFilename = pstrdup(Log_filename);
        rotation_requested = true;
      }

         
                                                                         
                                                          
         
      if (((Log_destination & LOG_DESTINATION_CSVLOG) != 0) != (csvlogFile != NULL))
      {
        rotation_requested = true;
      }

         
                                                                       
                                                 
         
      if (currentLogRotationAge != Log_RotationAge)
      {
        currentLogRotationAge = Log_RotationAge;
        set_next_rotation_time();
      }

         
                                                                    
                                                           
         
      if (rotation_disabled)
      {
        rotation_disabled = false;
        rotation_requested = true;
      }

         
                                                                         
                                                                       
                                                                        
         
      update_metainfo_datafile();
    }

    if (Log_RotationAge > 0 && !rotation_disabled)
    {
                                              
      now = (pg_time_t)time(NULL);
      if (now >= next_rotation_time)
      {
        rotation_requested = time_based_rotation = true;
      }
    }

    if (!rotation_requested && Log_RotationSize > 0 && !rotation_disabled)
    {
                                            
      if (ftell(syslogFile) >= Log_RotationSize * 1024L)
      {
        rotation_requested = true;
        size_rotation_for |= LOG_DESTINATION_STDERR;
      }
      if (csvlogFile != NULL && ftell(csvlogFile) >= Log_RotationSize * 1024L)
      {
        rotation_requested = true;
        size_rotation_for |= LOG_DESTINATION_CSVLOG;
      }
    }

    if (rotation_requested)
    {
         
                                                                        
                                                                
         
      if (!time_based_rotation && size_rotation_for == 0)
      {
        size_rotation_for = LOG_DESTINATION_STDERR | LOG_DESTINATION_CSVLOG;
      }
      logfile_rotate(time_based_rotation, size_rotation_for);
    }

       
                                                                      
                                                                      
                                                                         
                                                                   
                           
       
                                                                          
                                                                           
                                                                          
                                                      
       
    if (Log_RotationAge > 0 && !rotation_disabled)
    {
      pg_time_t delay;

      delay = next_rotation_time - now;
      if (delay > 0)
      {
        if (delay > INT_MAX / 1000)
        {
          delay = INT_MAX / 1000;
        }
        cur_timeout = delay * 1000L;           
      }
      else
      {
        cur_timeout = 0;
      }
    }
    else
    {
      cur_timeout = -1L;
    }

       
                                           
       
#ifndef WIN32
    rc = WaitEventSetWait(wes, cur_timeout, &event, 1, WAIT_EVENT_SYSLOGGER_MAIN);

    if (rc == 1 && event.events == WL_SOCKET_READABLE)
    {
      int bytesRead;

      bytesRead = read(syslogPipe[0], logbuffer + bytes_in_logbuffer, sizeof(logbuffer) - bytes_in_logbuffer);
      if (bytesRead < 0)
      {
        if (errno != EINTR)
        {
          ereport(LOG, (errcode_for_socket_access(), errmsg("could not read from logger pipe: %m")));
        }
      }
      else if (bytesRead > 0)
      {
        bytes_in_logbuffer += bytesRead;
        process_pipe_input(logbuffer, &bytes_in_logbuffer);
        continue;
      }
      else
      {
           
                                                                    
                                                                       
                                                                    
                                                            
           
        pipe_eof_seen = true;

                                                            
        flush_pipe_input(logbuffer, &bytes_in_logbuffer);
      }
    }
#else             

       
                                                                        
                                                                        
                                
       
                                                                           
                                                                           
                                    
       
    LeaveCriticalSection(&sysloggerSection);

    (void)WaitEventSetWait(wes, cur_timeout, &event, 1, WAIT_EVENT_SYSLOGGER_MAIN);

    EnterCriticalSection(&sysloggerSection);
#endif            

    if (pipe_eof_seen)
    {
         
                                                                         
                                              
         
      ereport(DEBUG1, (errmsg("logger shutting down")));

         
                                                               
                                                                         
                                                                    
                                                                      
                                     
         
      proc_exit(0);
    }
  }
}

   
                                                          
   
int
SysLogger_Start(void)
{
  pid_t sysloggerPid;
  char *filename;

  if (!Logging_collector)
  {
    return 0;
  }

     
                                                                      
             
     
                                                                            
                                                                             
                      
     
                                                                         
                                                                           
                                               
     
#ifndef WIN32
  if (syslogPipe[0] < 0)
  {
    if (pipe(syslogPipe) < 0)
    {
      ereport(FATAL, (errcode_for_socket_access(), (errmsg("could not create pipe for syslog: %m"))));
    }
  }
#else
  if (!syslogPipe[0])
  {
    SECURITY_ATTRIBUTES sa;

    memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&syslogPipe[0], &syslogPipe[1], &sa, 32768))
    {
      ereport(FATAL, (errcode_for_file_access(), (errmsg("could not create pipe for syslog: %m"))));
    }
  }
#endif

     
                                                        
     
  (void)MakePGDirectory(Log_directory);

     
                                                                            
                                                                            
                                                           
     
                                                                           
                                                                        
                                                                             
                                                                             
                                                                            
                            
     
  first_syslogger_file_time = time(NULL);

  filename = logfile_getname(first_syslogger_file_time, NULL);

  syslogFile = logfile_open(filename, "a", false);

  pfree(filename);

     
                                                                           
                                                                        
                                                             
     
  if (Log_destination & LOG_DESTINATION_CSVLOG)
  {
    filename = logfile_getname(first_syslogger_file_time, ".csv");

    csvlogFile = logfile_open(filename, "a", false);

    pfree(filename);
  }

#ifdef EXEC_BACKEND
  switch ((sysloggerPid = syslogger_forkexec()))
#else
  switch ((sysloggerPid = fork_process()))
#endif
  {
  case -1:
    ereport(LOG, (errmsg("could not fork system logger: %m")));
    return 0;

#ifndef EXEC_BACKEND
  case 0:
                                 
    InitPostmasterChild();

                                        
    ClosePostmasterPorts(true);

                                                                    
    dsm_detach_all();
    PGSharedMemoryDetach();

                     
    SysLoggerMain(0, NULL);
    break;
#endif

  default:
                                

                                                     
    if (!redirection_done)
    {
#ifdef WIN32
      int fd;
#endif

         
                                                                     
                                                                  
                                      
         
      ereport(LOG, (errmsg("redirecting log output to logging collector process"), errhint("Future log output will appear in directory \"%s\".", Log_directory)));

#ifndef WIN32
      fflush(stdout);
      if (dup2(syslogPipe[1], fileno(stdout)) < 0)
      {
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not redirect stdout: %m")));
      }
      fflush(stderr);
      if (dup2(syslogPipe[1], fileno(stderr)) < 0)
      {
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not redirect stderr: %m")));
      }
                                                           
      close(syslogPipe[1]);
      syslogPipe[1] = -1;
#else

         
                                                                     
                                                                   
                            
         
      fflush(stderr);
      fd = _open_osfhandle((intptr_t)syslogPipe[1], _O_APPEND | _O_BINARY);
      if (dup2(fd, _fileno(stderr)) < 0)
      {
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not redirect stderr: %m")));
      }
      close(fd);
      _setmode(_fileno(stderr), _O_BINARY);

         
                                                         
                                                                
                                               
         
      syslogPipe[1] = 0;
#endif
      redirection_done = true;
    }

                                                            
    fclose(syslogFile);
    syslogFile = NULL;
    if (csvlogFile != NULL)
    {
      fclose(csvlogFile);
      csvlogFile = NULL;
    }
    return (int)sysloggerPid;
  }

                                  
  return 0;
}

#ifdef EXEC_BACKEND

   
                          
   
                                                                      
   
static pid_t
syslogger_forkexec(void)
{
  char *av[10];
  int ac = 0;
  char filenobuf[32];
  char csvfilenobuf[32];

  av[ac++] = "postgres";
  av[ac++] = "--forklog";
  av[ac++] = NULL;                                       

                                                                      
#ifndef WIN32
  if (syslogFile != NULL)
  {
    snprintf(filenobuf, sizeof(filenobuf), "%d", fileno(syslogFile));
  }
  else
  {
    strcpy(filenobuf, "-1");
  }
#else             
  if (syslogFile != NULL)
  {
    snprintf(filenobuf, sizeof(filenobuf), "%ld", (long)_get_osfhandle(_fileno(syslogFile)));
  }
  else
  {
    strcpy(filenobuf, "0");
  }
#endif            
  av[ac++] = filenobuf;

#ifndef WIN32
  if (csvlogFile != NULL)
  {
    snprintf(csvfilenobuf, sizeof(csvfilenobuf), "%d", fileno(csvlogFile));
  }
  else
  {
    strcpy(csvfilenobuf, "-1");
  }
#else             
  if (csvlogFile != NULL)
  {
    snprintf(csvfilenobuf, sizeof(csvfilenobuf), "%ld", (long)_get_osfhandle(_fileno(csvlogFile)));
  }
  else
  {
    strcpy(csvfilenobuf, "0");
  }
#endif            
  av[ac++] = csvfilenobuf;

  av[ac] = NULL;
  Assert(ac < lengthof(av));

  return postmaster_forkexec(ac, av);
}

   
                           
   
                                                               
   
static void
syslogger_parseArgs(int argc, char *argv[])
{
  int fd;

  Assert(argc == 5);
  argv += 3;

     
                                                                           
     
                                                                            
                                                                         
                                                                            
     
#ifndef WIN32
  fd = atoi(*argv++);
  if (fd != -1)
  {
    syslogFile = fdopen(fd, "a");
    setvbuf(syslogFile, NULL, PG_IOLBF, 0);
  }
  fd = atoi(*argv++);
  if (fd != -1)
  {
    csvlogFile = fdopen(fd, "a");
    setvbuf(csvlogFile, NULL, PG_IOLBF, 0);
  }
#else             
  fd = atoi(*argv++);
  if (fd != 0)
  {
    fd = _open_osfhandle(fd, _O_APPEND | _O_TEXT);
    if (fd > 0)
    {
      syslogFile = fdopen(fd, "a");
      setvbuf(syslogFile, NULL, PG_IOLBF, 0);
    }
  }
  fd = atoi(*argv++);
  if (fd != 0)
  {
    fd = _open_osfhandle(fd, _O_APPEND | _O_TEXT);
    if (fd > 0)
    {
      csvlogFile = fdopen(fd, "a");
      setvbuf(csvlogFile, NULL, PG_IOLBF, 0);
    }
  }
#endif            
}
#endif                   

                                    
                           
                                    
   

   
                                                     
   
                                                                             
                                                                              
   
                                                                               
                                                                           
                                                                               
                                                                            
   
                                       
                                                                       
                                                                            
   
                                                                              
                                                                               
            
   
                                                                             
                                                                           
                                                  
   
static void
process_pipe_input(char *logbuffer, int *bytes_in_logbuffer)
{
  char *cursor = logbuffer;
  int count = *bytes_in_logbuffer;
  int dest = LOG_DESTINATION_STDERR;

                                                          
  while (count >= (int)(offsetof(PipeProtoHeader, data) + 1))
  {
    PipeProtoHeader p;
    int chunklen;

                                    
    memcpy(&p, cursor, offsetof(PipeProtoHeader, data));
    if (p.nuls[0] == '\0' && p.nuls[1] == '\0' && p.len > 0 && p.len <= PIPE_MAX_PAYLOAD && p.pid != 0 && (p.is_last == 't' || p.is_last == 'f' || p.is_last == 'T' || p.is_last == 'F'))
    {
      List *buffer_list;
      ListCell *cell;
      save_buffer *existing_slot = NULL, *free_slot = NULL;
      StringInfo str;

      chunklen = PIPE_HEADER_SIZE + p.len;

                                                                 
      if (count < chunklen)
      {
        break;
      }

      dest = (p.is_last == 'T' || p.is_last == 'F') ? LOG_DESTINATION_CSVLOG : LOG_DESTINATION_STDERR;

                                                          
      buffer_list = buffer_lists[p.pid % NBUFFER_LISTS];
      foreach (cell, buffer_list)
      {
        save_buffer *buf = (save_buffer *)lfirst(cell);

        if (buf->pid == p.pid)
        {
          existing_slot = buf;
          break;
        }
        if (buf->pid == 0 && free_slot == NULL)
        {
          free_slot = buf;
        }
      }

      if (p.is_last == 'f' || p.is_last == 'F')
      {
           
                                                               
           
        if (existing_slot != NULL)
        {
                                                       
          str = &(existing_slot->data);
          appendBinaryStringInfo(str, cursor + PIPE_HEADER_SIZE, p.len);
        }
        else
        {
                                                            
          if (free_slot == NULL)
          {
               
                                                                  
                                                                
               
            free_slot = palloc(sizeof(save_buffer));
            buffer_list = lappend(buffer_list, free_slot);
            buffer_lists[p.pid % NBUFFER_LISTS] = buffer_list;
          }
          free_slot->pid = p.pid;
          str = &(free_slot->data);
          initStringInfo(str);
          appendBinaryStringInfo(str, cursor + PIPE_HEADER_SIZE, p.len);
        }
      }
      else
      {
           
                                                                      
                                                 
           
        if (existing_slot != NULL)
        {
          str = &(existing_slot->data);
          appendBinaryStringInfo(str, cursor + PIPE_HEADER_SIZE, p.len);
          write_syslogger_file(str->data, str->len, dest);
                                                                  
          existing_slot->pid = 0;
          pfree(str->data);
        }
        else
        {
                                                           
          write_syslogger_file(cursor + PIPE_HEADER_SIZE, p.len, dest);
        }
      }

                                          
      cursor += chunklen;
      count -= chunklen;
    }
    else
    {
                                     

         
                                                                       
                                                                      
                                                                        
                                                                        
                                                                     
                                                                       
                                           
         
      for (chunklen = 1; chunklen < count; chunklen++)
      {
        if (cursor[chunklen] == '\0')
        {
          break;
        }
      }
                                                          
      write_syslogger_file(cursor, chunklen, LOG_DESTINATION_STDERR);
      cursor += chunklen;
      count -= chunklen;
    }
  }

                                                                            
  if (count > 0 && cursor != logbuffer)
  {
    memmove(logbuffer, cursor, count);
  }
  *bytes_in_logbuffer = count;
}

   
                               
   
                                                                           
                                                                             
   
static void
flush_pipe_input(char *logbuffer, int *bytes_in_logbuffer)
{
  int i;

                                             
  for (i = 0; i < NBUFFER_LISTS; i++)
  {
    List *list = buffer_lists[i];
    ListCell *cell;

    foreach (cell, list)
    {
      save_buffer *buf = (save_buffer *)lfirst(cell);

      if (buf->pid != 0)
      {
        StringInfo str = &(buf->data);

        write_syslogger_file(str->data, str->len, LOG_DESTINATION_STDERR);
                                                                
        buf->pid = 0;
        pfree(str->data);
      }
    }
  }

     
                                                                        
                                                       
     
  if (*bytes_in_logbuffer > 0)
  {
    write_syslogger_file(logbuffer, *bytes_in_logbuffer, LOG_DESTINATION_STDERR);
  }
  *bytes_in_logbuffer = 0;
}

                                    
                     
                                    
   

   
                                            
   
                                                                          
                                                                         
                                                             
   
void
write_syslogger_file(const char *buffer, int count, int destination)
{
  int rc;
  FILE *logfile;

     
                                                                            
                                                                           
                                                                            
                                                                           
                                                                            
                                                                         
                                                                      
     
                                                                             
                                              
     
  logfile = (destination == LOG_DESTINATION_CSVLOG && csvlogFile != NULL) ? csvlogFile : syslogFile;

  rc = fwrite(buffer, 1, count, logfile);

     
                                                                         
                                                                         
                                                                            
                                                                          
     
  if (rc != count)
  {
    write_stderr("could not write to log file: %s\n", strerror(errno));
  }
}

#ifdef WIN32

   
                                                                        
   
                                                                            
                                                                               
                                                     
   
static unsigned int __stdcall pipeThread(void *arg)
{
  char logbuffer[READ_BUF_SIZE];
  int bytes_in_logbuffer = 0;

  for (;;)
  {
    DWORD bytesRead;
    BOOL result;

    result = ReadFile(syslogPipe[0], logbuffer + bytes_in_logbuffer, sizeof(logbuffer) - bytes_in_logbuffer, &bytesRead, 0);

       
                                                                     
                                                                  
                                                                        
                
       
    EnterCriticalSection(&sysloggerSection);
    if (!result)
    {
      DWORD error = GetLastError();

      if (error == ERROR_HANDLE_EOF || error == ERROR_BROKEN_PIPE)
      {
        break;
      }
      _dosmaperr(error);
      ereport(LOG, (errcode_for_file_access(), errmsg("could not read from logger pipe: %m")));
    }
    else if (bytesRead > 0)
    {
      bytes_in_logbuffer += bytesRead;
      process_pipe_input(logbuffer, &bytes_in_logbuffer);
    }

       
                                                                          
                     
       
    if (Log_RotationSize > 0)
    {
      if (ftell(syslogFile) >= Log_RotationSize * 1024L || (csvlogFile != NULL && ftell(csvlogFile) >= Log_RotationSize * 1024L))
      {
        SetLatch(MyLatch);
      }
    }
    LeaveCriticalSection(&sysloggerSection);
  }

                                                           
  pipe_eof_seen = true;

                                                      
  flush_pipe_input(logbuffer, &bytes_in_logbuffer);

                                                               
  SetLatch(MyLatch);

  LeaveCriticalSection(&sysloggerSection);
  _endthread();
  return 0;
}
#endif            

   
                                                                     
   
                                                                         
                                                     
                                           
   
static FILE *
logfile_open(const char *filename, const char *mode, bool allow_errors)
{
  FILE *fh;
  mode_t oumask;

     
                                                                             
                                              
     
  oumask = umask((mode_t)((~(Log_file_mode | S_IWUSR)) & (S_IRWXU | S_IRWXG | S_IRWXO)));
  fh = fopen(filename, mode);
  umask(oumask);

  if (fh)
  {
    setvbuf(fh, NULL, PG_IOLBF, 0);

#ifdef WIN32
                                          
    _setmode(_fileno(fh), _O_TEXT);
#endif
  }
  else
  {
    int save_errno = errno;

    ereport(allow_errors ? LOG : FATAL, (errcode_for_file_access(), errmsg("could not open log file \"%s\": %m", filename)));
    errno = save_errno;
  }

  return fh;
}

   
                            
   
static void
logfile_rotate(bool time_based_rotation, int size_rotation_for)
{
  char *filename;
  char *csvfilename = NULL;
  pg_time_t fntime;
  FILE *fh;

  rotation_requested = false;

     
                                                                            
                                                                             
                                                          
     
  if (time_based_rotation)
  {
    fntime = next_rotation_time;
  }
  else
  {
    fntime = time(NULL);
  }
  filename = logfile_getname(fntime, NULL);
  if (Log_destination & LOG_DESTINATION_CSVLOG)
  {
    csvfilename = logfile_getname(fntime, ".csv");
  }

     
                                                                     
                                                                        
                                                                            
                                                          
     
                                                                           
     
  if (time_based_rotation || (size_rotation_for & LOG_DESTINATION_STDERR))
  {
    if (Log_truncate_on_rotation && time_based_rotation && last_file_name != NULL && strcmp(filename, last_file_name) != 0)
    {
      fh = logfile_open(filename, "w", true);
    }
    else
    {
      fh = logfile_open(filename, "a", true);
    }

    if (!fh)
    {
         
                                                                     
                                                                  
                                                                         
                                 
         
      if (errno != ENFILE && errno != EMFILE)
      {
        ereport(LOG, (errmsg("disabling automatic rotation (use SIGHUP to re-enable)")));
        rotation_disabled = true;
      }

      if (filename)
      {
        pfree(filename);
      }
      if (csvfilename)
      {
        pfree(csvfilename);
      }
      return;
    }

    fclose(syslogFile);
    syslogFile = fh;

                                                                  
    if (last_file_name != NULL)
    {
      pfree(last_file_name);
    }
    last_file_name = filename;
    filename = NULL;
  }

     
                                                                           
                                                                             
                                                                        
                                                                          
                                                 
     
  if ((Log_destination & LOG_DESTINATION_CSVLOG) && (csvlogFile == NULL || time_based_rotation || (size_rotation_for & LOG_DESTINATION_CSVLOG)))
  {
    if (Log_truncate_on_rotation && time_based_rotation && last_csv_file_name != NULL && strcmp(csvfilename, last_csv_file_name) != 0)
    {
      fh = logfile_open(csvfilename, "w", true);
    }
    else
    {
      fh = logfile_open(csvfilename, "a", true);
    }

    if (!fh)
    {
         
                                                                     
                                                                  
                                                                         
                                 
         
      if (errno != ENFILE && errno != EMFILE)
      {
        ereport(LOG, (errmsg("disabling automatic rotation (use SIGHUP to re-enable)")));
        rotation_disabled = true;
      }

      if (filename)
      {
        pfree(filename);
      }
      if (csvfilename)
      {
        pfree(csvfilename);
      }
      return;
    }

    if (csvlogFile != NULL)
    {
      fclose(csvlogFile);
    }
    csvlogFile = fh;

                                                                  
    if (last_csv_file_name != NULL)
    {
      pfree(last_csv_file_name);
    }
    last_csv_file_name = csvfilename;
    csvfilename = NULL;
  }
  else if (!(Log_destination & LOG_DESTINATION_CSVLOG) && csvlogFile != NULL)
  {
                                                           
    fclose(csvlogFile);
    csvlogFile = NULL;
    if (last_csv_file_name != NULL)
    {
      pfree(last_csv_file_name);
    }
    last_csv_file_name = NULL;
  }

  if (filename)
  {
    pfree(filename);
  }
  if (csvfilename)
  {
    pfree(csvfilename);
  }

  update_metainfo_datafile();

  set_next_rotation_time();
}

   
                                                      
   
                                                                     
                               
   
                       
   
static char *
logfile_getname(pg_time_t timestamp, const char *suffix)
{
  char *filename;
  int len;

  filename = palloc(MAXPGPATH);

  snprintf(filename, MAXPGPATH, "%s/", Log_directory);

  len = strlen(filename);

                                                
  pg_strftime(filename + len, MAXPGPATH - len, Log_filename, pg_localtime(&timestamp, log_timezone));

  if (suffix != NULL)
  {
    len = strlen(filename);
    if (len > 4 && (strcmp(filename + (len - 4), ".log") == 0))
    {
      len -= 4;
    }
    strlcpy(filename + len, suffix, MAXPGPATH - len);
  }

  return filename;
}

   
                                                                              
   
static void
set_next_rotation_time(void)
{
  pg_time_t now;
  struct pg_tm *tm;
  int rotinterval;

                                                        
  if (Log_RotationAge <= 0)
  {
    return;
  }

     
                                                                       
                                                                             
                                                                           
          
     
  rotinterval = Log_RotationAge * SECS_PER_MINUTE;                         
  now = (pg_time_t)time(NULL);
  tm = pg_localtime(&now, log_timezone);
  now += tm->tm_gmtoff;
  now -= now % rotinterval;
  now += rotinterval;
  now -= tm->tm_gmtoff;
  next_rotation_time = now;
}

   
                                                                               
                                                                            
                                                                         
                                                                      
                                                                         
                                                                   
   
static void
update_metainfo_datafile(void)
{
  FILE *fh;
  mode_t oumask;

  if (!(Log_destination & LOG_DESTINATION_STDERR) && !(Log_destination & LOG_DESTINATION_CSVLOG))
  {
    if (unlink(LOG_METAINFO_DATAFILE) < 0 && errno != ENOENT)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", LOG_METAINFO_DATAFILE)));
    }
    return;
  }

                                                                       
  oumask = umask(pg_mode_mask);
  fh = fopen(LOG_METAINFO_DATAFILE_TMP, "w");
  umask(oumask);

  if (fh)
  {
    setvbuf(fh, NULL, PG_IOLBF, 0);

#ifdef WIN32
                                          
    _setmode(_fileno(fh), _O_TEXT);
#endif
  }
  else
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", LOG_METAINFO_DATAFILE_TMP)));
    return;
  }

  if (last_file_name && (Log_destination & LOG_DESTINATION_STDERR))
  {
    if (fprintf(fh, "stderr %s\n", last_file_name) < 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", LOG_METAINFO_DATAFILE_TMP)));
      fclose(fh);
      return;
    }
  }

  if (last_csv_file_name && (Log_destination & LOG_DESTINATION_CSVLOG))
  {
    if (fprintf(fh, "csvlog %s\n", last_csv_file_name) < 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", LOG_METAINFO_DATAFILE_TMP)));
      fclose(fh);
      return;
    }
  }
  fclose(fh);

  if (rename(LOG_METAINFO_DATAFILE_TMP, LOG_METAINFO_DATAFILE) != 0)
  {
    ereport(LOG, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", LOG_METAINFO_DATAFILE_TMP, LOG_METAINFO_DATAFILE)));
  }
}

                                    
                            
                                    
   

   
                                                                  
                                                 
   
bool
CheckLogrotateSignal(void)
{
  struct stat stat_buf;

  if (stat(LOGROTATE_SIGNAL_FILE, &stat_buf) == 0)
  {
    return true;
  }

  return false;
}

   
                                                     
   
void
RemoveLogrotateSignalFiles(void)
{
  unlink(LOGROTATE_SIGNAL_FILE);
}

                                            
static void
sigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

                                         
static void
sigUsr1Handler(SIGNAL_ARGS)
{
  int save_errno = errno;

  rotation_requested = true;
  SetLatch(MyLatch);

  errno = save_errno;
}
