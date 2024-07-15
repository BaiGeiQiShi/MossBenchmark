                                                                            
   
                
                                                                 
   
                                                                         
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "access/xlog_internal.h"                               
#include "catalog/pg_type.h"
#include "common/file_perm.h"
#include "lib/stringinfo.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "pgtar.h"
#include "pgstat.h"
#include "port.h"
#include "postmaster/syslogger.h"
#include "replication/basebackup.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/bufpage.h"
#include "storage/checksum.h"
#include "storage/dsm_impl.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/reinit.h"
#include "utils/builtins.h"
#include "utils/ps_status.h"
#include "utils/relcache.h"
#include "utils/timestamp.h"

typedef struct
{
  const char *label;
  bool progress;
  bool fastcheckpoint;
  bool nowait;
  bool includewal;
  uint32 maxrate;
  bool sendtblspcmapfile;
} basebackup_options;

static int64
sendDir(const char *path, int basepathlen, bool sizeonly, List *tablespaces, bool sendtblspclinks);
static bool
sendFile(const char *readfilename, const char *tarfilename, struct stat *statbuf, bool missing_ok, Oid dboid);
static void
sendFileWithContent(const char *filename, const char *content);
static int64
_tarWriteHeader(const char *filename, const char *linktarget, struct stat *statbuf, bool sizeonly);
static int64
_tarWriteDir(const char *pathbuf, int basepathlen, struct stat *statbuf, bool sizeonly);
static void
send_int8_string(StringInfoData *buf, int64 intval);
static void
SendBackupHeader(List *tablespaces);
static void
perform_base_backup(basebackup_options *opt);
static void
parse_basebackup_options(List *options, basebackup_options *opt);
static void
SendXlogRecPtrResult(XLogRecPtr ptr, TimeLineID tli);
static int
compareWalFileNames(const void *a, const void *b);
static void
throttle(size_t increment);
static bool
is_checksummed_file(const char *fullpath, const char *filename);

                                                                      
static bool backup_started_in_recovery = false;

                                                     
static char *statrelpath = NULL;

   
                                                                 
   
#define TAR_SEND_SIZE 32768

   
                                                                           
   
#define THROTTLING_FREQUENCY 8

   
                                                                             
                                                                         
                                                                     
   
#define CHECK_FREAD_ERROR(fp, filename)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (ferror(fp))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      ereport(ERROR, (errmsg("could not read from file \"%s\"", filename)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

                                                                    
static uint64 throttling_sample;

                                                                
static int64 throttling_counter;

                                                                    
static TimeOffset elapsed_min_unit;

                                          
static TimestampTz throttled_last;

                                                    
static XLogRecPtr startptr;

                                                           
static int64 total_checksum_failures;

                              
static bool noverify_checksums = false;

   
                                                                            
                                                                           
                                                                         
                                             
   
struct exclude_list_item
{
  const char *name;
  bool match_prefix;
};

   
                                                                            
                                                                              
                                                              
   
                                                                               
              
   
static const char *excludeDirContents[] = {
       
                                                                             
                                                                         
                      
       
    PG_STAT_TMP_DIR,

       
                                                                           
                                                                              
                                        
       
    "pg_replslot",

                                                                  
    PG_DYNSHMEM_DIR,

                                                            
    "pg_notify",

       
                                                                               
                                              
       
    "pg_serial",

                                                                            
    "pg_snapshots",

                                                            
    "pg_subtrans",

                     
    NULL};

   
                                        
   
static const struct exclude_list_item excludeFiles[] = {
                                        
    {PG_AUTOCONF_FILENAME ".tmp", false},

                                              
    {LOG_METAINFO_DATAFILE_TMP, false},

       
                                                                            
                        
       
    {RELCACHE_INIT_FILENAME, true},

       
                                                                         
                                                                               
                                                                              
                       
       
    {BACKUP_LABEL_FILE, false}, {TABLESPACE_MAP, false},

    {"postmaster.pid", false}, {"postmaster.opts", false},

                     
    {NULL, false}};

   
                                                    
   
                                                                   
             
   
static const struct exclude_list_item noChecksumFiles[] = {{"pg_control", false}, {"pg_filenode.map", false}, {"pg_internal.init", true}, {"PG_VERSION", false},
#ifdef EXEC_BACKEND
    {"config_exec_params", true},
#endif
    {NULL, false}};

   
                                                            
   
                                                                         
                                                        
   
static void
perform_base_backup(basebackup_options *opt)
{
  TimeLineID starttli;
  XLogRecPtr endptr;
  TimeLineID endtli;
  StringInfo labelfile;
  StringInfo tblspc_map_file = NULL;
  int datadirpathlen;
  List *tablespaces = NIL;

  datadirpathlen = strlen(DataDir);

  backup_started_in_recovery = RecoveryInProgress();

  labelfile = makeStringInfo();
  tblspc_map_file = makeStringInfo();

  total_checksum_failures = 0;

  startptr = do_pg_start_backup(opt->label, opt->fastcheckpoint, &starttli, labelfile, &tablespaces, tblspc_map_file, opt->progress, opt->sendtblspcmapfile);

     
                                                                             
                                                                          
                                                                             
                                                                   
     

  PG_ENSURE_ERROR_CLEANUP(do_pg_abort_backup, BoolGetDatum(false));
  {
    ListCell *lc;
    tablespaceinfo *ti;

    SendXlogRecPtrResult(startptr, starttli);

       
                                                                        
                                                                          
       
    if (is_absolute_path(pgstat_stat_directory) && strncmp(pgstat_stat_directory, DataDir, datadirpathlen) == 0)
    {
      statrelpath = psprintf("./%s", pgstat_stat_directory + datadirpathlen + 1);
    }
    else if (strncmp(pgstat_stat_directory, "./", 2) != 0)
    {
      statrelpath = psprintf("./%s", pgstat_stat_directory);
    }
    else
    {
      statrelpath = pgstat_stat_directory;
    }

                                                      
    ti = palloc0(sizeof(tablespaceinfo));
    ti->size = opt->progress ? sendDir(".", 1, true, tablespaces, true) : -1;
    tablespaces = lappend(tablespaces, ti);

                                
    SendBackupHeader(tablespaces);

                                                                       
    if (opt->maxrate > 0)
    {
      throttling_sample = (int64)opt->maxrate * (int64)1024 / THROTTLING_FREQUENCY;

         
                                                                      
                      
         
      elapsed_min_unit = USECS_PER_SEC / THROTTLING_FREQUENCY;

                              
      throttling_counter = 0;

                                                            
      throttled_last = GetCurrentTimestamp();
    }
    else
    {
                               
      throttling_counter = -1;
    }

                                             
    foreach (lc, tablespaces)
    {
      tablespaceinfo *ti = (tablespaceinfo *)lfirst(lc);
      StringInfoData buf;

                                        
      pq_beginmessage(&buf, 'H');
      pq_sendbyte(&buf, 0);                      
      pq_sendint16(&buf, 0);            
      pq_endmessage(&buf);

      if (ti->path == NULL)
      {
        struct stat statbuf;

                                                                
        sendFileWithContent(BACKUP_LABEL_FILE, labelfile->data);

           
                                                                     
                      
           
        if (tblspc_map_file && opt->sendtblspcmapfile)
        {
          sendFileWithContent(TABLESPACE_MAP, tblspc_map_file->data);
          sendDir(".", 1, false, tablespaces, false);
        }
        else
        {
          sendDir(".", 1, false, tablespaces, true);
        }

                                                       
        if (lstat(XLOG_CONTROL_FILE, &statbuf) != 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", XLOG_CONTROL_FILE)));
        }
        sendFile(XLOG_CONTROL_FILE, XLOG_CONTROL_FILE, &statbuf, false, InvalidOid);
      }
      else
      {
        sendTablespace(ti->path, false);
      }

         
                                                                        
                                                                      
                                                                        
                                                        
         
      if (opt->includewal && ti->path == NULL)
      {
        Assert(lnext(lc) == NULL);
      }
      else
      {
        pq_putemptymessage('c');               
      }
    }

    endptr = do_pg_stop_backup(labelfile->data, !opt->nowait, &endtli);
  }
  PG_END_ENSURE_ERROR_CLEANUP(do_pg_abort_backup, BoolGetDatum(false));

  if (opt->includewal)
  {
       
                                                                     
                                 
       
    char pathbuf[MAXPGPATH];
    XLogSegNo segno;
    XLogSegNo startsegno;
    XLogSegNo endsegno;
    struct stat statbuf;
    List *historyFileList = NIL;
    List *walFileList = NIL;
    char **walFiles;
    int nWalFiles;
    char firstoff[MAXFNAMELEN];
    char lastoff[MAXFNAMELEN];
    DIR *dir;
    struct dirent *de;
    int i;
    ListCell *lc;
    TimeLineID tli;

       
                                                                     
                                                                           
                                                                         
                                                                           
                                                                        
                                                                         
                       
       
    XLByteToSeg(startptr, startsegno, wal_segment_size);
    XLogFileName(firstoff, ThisTimeLineID, startsegno, wal_segment_size);
    XLByteToPrevSeg(endptr, endsegno, wal_segment_size);
    XLogFileName(lastoff, ThisTimeLineID, endsegno, wal_segment_size);

    dir = AllocateDir("pg_wal");
    while ((de = ReadDir(dir, "pg_wal")) != NULL)
    {
                                                                    
      if (IsXLogFileName(de->d_name) && strcmp(de->d_name + 8, firstoff + 8) >= 0 && strcmp(de->d_name + 8, lastoff + 8) <= 0)
      {
        walFileList = lappend(walFileList, pstrdup(de->d_name));
      }
                                                      
      else if (IsTLHistoryFileName(de->d_name))
      {
        historyFileList = lappend(historyFileList, pstrdup(de->d_name));
      }
    }
    FreeDir(dir);

       
                                                                        
                          
       
    CheckXLogRemoved(startsegno, ThisTimeLineID);

       
                                                                           
                                                                        
                                                        
       
    nWalFiles = list_length(walFileList);
    walFiles = palloc(nWalFiles * sizeof(char *));
    i = 0;
    foreach (lc, walFileList)
    {
      walFiles[i++] = lfirst(lc);
    }
    qsort(walFiles, nWalFiles, sizeof(char *), compareWalFileNames);

       
                                                                           
                                           
       
    if (nWalFiles < 1)
    {
      ereport(ERROR, (errmsg("could not find any WAL files")));
    }

       
                                                                          
                                        
       
    XLogFromFileName(walFiles[0], &tli, &segno, wal_segment_size);
    if (segno != startsegno)
    {
      char startfname[MAXFNAMELEN];

      XLogFileName(startfname, ThisTimeLineID, startsegno, wal_segment_size);
      ereport(ERROR, (errmsg("could not find WAL file \"%s\"", startfname)));
    }
    for (i = 0; i < nWalFiles; i++)
    {
      XLogSegNo currsegno = segno;
      XLogSegNo nextsegno = segno + 1;

      XLogFromFileName(walFiles[i], &tli, &segno, wal_segment_size);
      if (!(nextsegno == segno || currsegno == segno))
      {
        char nextfname[MAXFNAMELEN];

        XLogFileName(nextfname, ThisTimeLineID, nextsegno, wal_segment_size);
        ereport(ERROR, (errmsg("could not find WAL file \"%s\"", nextfname)));
      }
    }
    if (segno != endsegno)
    {
      char endfname[MAXFNAMELEN];

      XLogFileName(endfname, ThisTimeLineID, endsegno, wal_segment_size);
      ereport(ERROR, (errmsg("could not find WAL file \"%s\"", endfname)));
    }

                                                             
    for (i = 0; i < nWalFiles; i++)
    {
      FILE *fp;
      char buf[TAR_SEND_SIZE];
      size_t cnt;
      pgoff_t len = 0;

      snprintf(pathbuf, MAXPGPATH, XLOGDIR "/%s", walFiles[i]);
      XLogFromFileName(walFiles[i], &tli, &segno, wal_segment_size);

      fp = AllocateFile(pathbuf, "rb");
      if (fp == NULL)
      {
        int save_errno = errno;

           
                                                                    
                                                                      
                          
           
        CheckXLogRemoved(segno, tli);

        errno = save_errno;
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", pathbuf)));
      }

      if (fstat(fileno(fp), &statbuf) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", pathbuf)));
      }
      if (statbuf.st_size != wal_segment_size)
      {
        CheckXLogRemoved(segno, tli);
        ereport(ERROR, (errcode_for_file_access(), errmsg("unexpected WAL file size \"%s\"", walFiles[i])));
      }

                                    
      _tarWriteHeader(pathbuf, NULL, &statbuf, false);

      while ((cnt = fread(buf, 1, Min(sizeof(buf), wal_segment_size - len), fp)) > 0)
      {
        CheckXLogRemoved(segno, tli);
                                                  
        if (pq_putmessage('d', buf, cnt))
        {
          ereport(ERROR, (errmsg("base backup could not send data, aborting backup")));
        }

        len += cnt;
        throttle(cnt);

        if (len == wal_segment_size)
        {
          break;
        }
      }

      CHECK_FREAD_ERROR(fp, pathbuf);

      if (len != wal_segment_size)
      {
        CheckXLogRemoved(segno, tli);
        ereport(ERROR, (errcode_for_file_access(), errmsg("unexpected WAL file size \"%s\"", walFiles[i])));
      }

                                                                         

      FreeFile(fp);

         
                                                                       
                                                             
                                                                      
                           
         
      StatusFilePath(pathbuf, walFiles[i], ".done");
      sendFileWithContent(pathbuf, "");
    }

       
                                                                         
                                                                          
                                                                          
                                                                          
                                                                         
                                                                       
                                        
       
    foreach (lc, historyFileList)
    {
      char *fname = lfirst(lc);

      snprintf(pathbuf, MAXPGPATH, XLOGDIR "/%s", fname);

      if (lstat(pathbuf, &statbuf) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", pathbuf)));
      }

      sendFile(pathbuf, pathbuf, &statbuf, false, InvalidOid);

                                                 
      StatusFilePath(pathbuf, fname, ".done");
      sendFileWithContent(pathbuf, "");
    }

                                                     
    pq_putemptymessage('c');
  }
  SendXlogRecPtrResult(endptr, endtli);

  if (total_checksum_failures)
  {
    if (total_checksum_failures > 1)
    {
      char buf[64];

      snprintf(buf, sizeof(buf), INT64_FORMAT, total_checksum_failures);

      ereport(WARNING, (errmsg("%s total checksum verification failures", buf)));
    }
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("checksum verification failure during base backup")));
  }
}

   
                                                                        
                                             
   
static int
compareWalFileNames(const void *a, const void *b)
{
  char *fna = *((char **)a);
  char *fnb = *((char **)b);

  return strcmp(fna + 8, fnb + 8);
}

   
                                                           
   
static void
parse_basebackup_options(List *options, basebackup_options *opt)
{
  ListCell *lopt;
  bool o_label = false;
  bool o_progress = false;
  bool o_fast = false;
  bool o_nowait = false;
  bool o_wal = false;
  bool o_maxrate = false;
  bool o_tablespace_map = false;
  bool o_noverify_checksums = false;

  MemSet(opt, 0, sizeof(*opt));
  foreach (lopt, options)
  {
    DefElem *defel = (DefElem *)lfirst(lopt);

    if (strcmp(defel->defname, "label") == 0)
    {
      if (o_label)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      opt->label = strVal(defel->arg);
      o_label = true;
    }
    else if (strcmp(defel->defname, "progress") == 0)
    {
      if (o_progress)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      opt->progress = true;
      o_progress = true;
    }
    else if (strcmp(defel->defname, "fast") == 0)
    {
      if (o_fast)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      opt->fastcheckpoint = true;
      o_fast = true;
    }
    else if (strcmp(defel->defname, "nowait") == 0)
    {
      if (o_nowait)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      opt->nowait = true;
      o_nowait = true;
    }
    else if (strcmp(defel->defname, "wal") == 0)
    {
      if (o_wal)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      opt->includewal = true;
      o_wal = true;
    }
    else if (strcmp(defel->defname, "max_rate") == 0)
    {
      long maxrate;

      if (o_maxrate)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }

      maxrate = intVal(defel->arg);
      if (maxrate < MAX_RATE_LOWER || maxrate > MAX_RATE_UPPER)
      {
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("%d is outside the valid range for parameter \"%s\" (%d .. %d)", (int)maxrate, "MAX_RATE", MAX_RATE_LOWER, MAX_RATE_UPPER)));
      }

      opt->maxrate = (uint32)maxrate;
      o_maxrate = true;
    }
    else if (strcmp(defel->defname, "tablespace_map") == 0)
    {
      if (o_tablespace_map)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      opt->sendtblspcmapfile = true;
      o_tablespace_map = true;
    }
    else if (strcmp(defel->defname, "noverify_checksums") == 0)
    {
      if (o_noverify_checksums)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("duplicate option \"%s\"", defel->defname)));
      }
      noverify_checksums = true;
      o_noverify_checksums = true;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }
  if (opt->label == NULL)
  {
    opt->label = "base backup";
  }
}

   
                                                   
   
                                                                            
                                                                            
                                               
   
void
SendBaseBackup(BaseBackupCmd *cmd)
{
  basebackup_options opt;
  SessionBackupState status = get_backup_status();

  if (status == SESSION_BACKUP_NON_EXCLUSIVE)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("a backup is already in progress in this session")));
  }

  parse_basebackup_options(cmd->options, &opt);

  WalSndSetState(WALSNDSTATE_BACKUP);

  if (update_process_title)
  {
    char activitymsg[50];

    snprintf(activitymsg, sizeof(activitymsg), "sending backup \"%s\"", opt.label);
    set_ps_display(activitymsg, false);
  }

  perform_base_backup(&opt);
}

static void
send_int8_string(StringInfoData *buf, int64 intval)
{
  char is[32];

  sprintf(is, INT64_FORMAT, intval);
  pq_sendint32(buf, strlen(is));
  pq_sendbytes(buf, is, strlen(is));
}

static void
SendBackupHeader(List *tablespaces)
{
  StringInfoData buf;
  ListCell *lc;

                                                    
  pq_beginmessage(&buf, 'T');                     
  pq_sendint16(&buf, 3);                    

                            
  pq_sendstring(&buf, "spcoid");
  pq_sendint32(&buf, 0);                     
  pq_sendint16(&buf, 0);                  
  pq_sendint32(&buf, OIDOID);               
  pq_sendint16(&buf, 4);                  
  pq_sendint32(&buf, 0);                  
  pq_sendint16(&buf, 0);                       

                              
  pq_sendstring(&buf, "spclocation");
  pq_sendint32(&buf, 0);
  pq_sendint16(&buf, 0);
  pq_sendint32(&buf, TEXTOID);
  pq_sendint16(&buf, -1);
  pq_sendint32(&buf, 0);
  pq_sendint16(&buf, 0);

                          
  pq_sendstring(&buf, "size");
  pq_sendint32(&buf, 0);
  pq_sendint16(&buf, 0);
  pq_sendint32(&buf, INT8OID);
  pq_sendint16(&buf, 8);
  pq_sendint32(&buf, 0);
  pq_sendint16(&buf, 0);
  pq_endmessage(&buf);

  foreach (lc, tablespaces)
  {
    tablespaceinfo *ti = lfirst(lc);

                                  
    pq_beginmessage(&buf, 'D');
    pq_sendint16(&buf, 3);                        
    if (ti->path == NULL)
    {
      pq_sendint32(&buf, -1);                           
      pq_sendint32(&buf, -1);
    }
    else
    {
      Size len;

      len = strlen(ti->oid);
      pq_sendint32(&buf, len);
      pq_sendbytes(&buf, ti->oid, len);

      len = strlen(ti->path);
      pq_sendint32(&buf, len);
      pq_sendbytes(&buf, ti->path, len);
    }
    if (ti->size >= 0)
    {
      send_int8_string(&buf, ti->size / 1024);
    }
    else
    {
      pq_sendint32(&buf, -1);           
    }

    pq_endmessage(&buf);
  }

                                      
  pq_puttextmessage('C', "SELECT");
}

   
                                                    
                                      
   
static void
SendXlogRecPtrResult(XLogRecPtr ptr, TimeLineID tli)
{
  StringInfoData buf;
  char str[MAXFNAMELEN];
  Size len;

  pq_beginmessage(&buf, 'T');                     
  pq_sendint16(&buf, 2);                    

                     
  pq_sendstring(&buf, "recptr");
  pq_sendint32(&buf, 0);                      
  pq_sendint16(&buf, 0);                   
  pq_sendint32(&buf, TEXTOID);               
  pq_sendint16(&buf, -1);
  pq_sendint32(&buf, 0);
  pq_sendint16(&buf, 0);

  pq_sendstring(&buf, "tli");
  pq_sendint32(&buf, 0);                
  pq_sendint16(&buf, 0);             

     
                                                                            
                                                                   
     
  pq_sendint32(&buf, INT8OID);               
  pq_sendint16(&buf, -1);
  pq_sendint32(&buf, 0);
  pq_sendint16(&buf, 0);
  pq_endmessage(&buf);

                
  pq_beginmessage(&buf, 'D');
  pq_sendint16(&buf, 2);                        

  len = snprintf(str, sizeof(str), "%X/%X", (uint32)(ptr >> 32), (uint32)ptr);
  pq_sendint32(&buf, len);
  pq_sendbytes(&buf, str, len);

  len = snprintf(str, sizeof(str), "%u", tli);
  pq_sendint32(&buf, len);
  pq_sendbytes(&buf, str, len);

  pq_endmessage(&buf);

                                      
  pq_puttextmessage('C', "SELECT");
}

   
                                                                       
   
static void
sendFileWithContent(const char *filename, const char *content)
{
  struct stat statbuf;
  int pad, len;

  len = strlen(content);

     
                                                                          
              
     
                                                       
#ifdef WIN32
  statbuf.st_uid = 0;
  statbuf.st_gid = 0;
#else
  statbuf.st_uid = geteuid();
  statbuf.st_gid = getegid();
#endif
  statbuf.st_mtime = time(NULL);
  statbuf.st_mode = pg_file_create_mode;
  statbuf.st_size = len;

  _tarWriteHeader(filename, NULL, &statbuf, false);
                                               
  pq_putmessage('d', content, len);

                                                             
  pad = ((len + 511) & ~511) - len;
  if (pad > 0)
  {
    char buf[512];

    MemSet(buf, 0, pad);
    pq_putmessage('d', buf, pad);
  }
}

   
                                                                           
                                                                               
                                          
   
                                                        
   
int64
sendTablespace(char *path, bool sizeonly)
{
  int64 size;
  char pathbuf[MAXPGPATH];
  struct stat statbuf;

     
                                                                           
                                                     
     
  snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, TABLESPACE_VERSION_DIRECTORY);

     
                                                                       
            
     
  if (lstat(pathbuf, &statbuf) != 0)
  {
    if (errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file or directory \"%s\": %m", pathbuf)));
    }

                                                                    
    return 0;
  }

  size = _tarWriteHeader(TABLESPACE_VERSION_DIRECTORY, NULL, &statbuf, sizeonly);

                                                              
  size += sendDir(pathbuf, strlen(path), sizeonly, NIL, true);

  return size;
}

   
                                                                           
                                                                               
                              
   
                                                                   
                                                           
   
                                                          
                                                         
                                                             
   
static int64
sendDir(const char *path, int basepathlen, bool sizeonly, List *tablespaces, bool sendtblspclinks)
{
  DIR *dir;
  struct dirent *de;
  char pathbuf[MAXPGPATH * 2];
  struct stat statbuf;
  int64 size = 0;
  const char *lastDir;                                        
  bool isDbDir = false;                                             

     
                                                                            
                
     
                                                                            
                           
     
  lastDir = last_dir_separator(path);

                                                                   
  if (lastDir != NULL && strspn(lastDir + 1, "0123456789") == strlen(lastDir + 1))
  {
                                                          
    int parentPathLen = lastDir - path;

       
                                                                      
                                                  
       
    if (strncmp(path, "./base", parentPathLen) == 0 || (parentPathLen >= (sizeof(TABLESPACE_VERSION_DIRECTORY) - 1) && strncmp(lastDir - (sizeof(TABLESPACE_VERSION_DIRECTORY) - 1), TABLESPACE_VERSION_DIRECTORY, sizeof(TABLESPACE_VERSION_DIRECTORY) - 1) == 0))
    {
      isDbDir = true;
    }
  }

  dir = AllocateDir(path);
  while ((de = ReadDir(dir, path)) != NULL)
  {
    int excludeIdx;
    bool excludeFound;
    ForkNumber relForkNum;                                         
    int relOidChars;                                                   

                            
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
    {
      continue;
    }

                              
    if (strncmp(de->d_name, PG_TEMP_FILE_PREFIX, strlen(PG_TEMP_FILE_PREFIX)) == 0)
    {
      continue;
    }

       
                                                                          
                                                                  
                                                                      
                                                                    
                                                                        
                                                                 
       
    CHECK_FOR_INTERRUPTS();
    if (RecoveryInProgress() != backup_started_in_recovery)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("the standby was promoted during online backup"),
                         errhint("This means that the backup being taken is corrupt "
                                 "and should not be used. "
                                 "Try taking another online backup.")));
    }

                                                
    excludeFound = false;
    for (excludeIdx = 0; excludeFiles[excludeIdx].name != NULL; excludeIdx++)
    {
      int cmplen = strlen(excludeFiles[excludeIdx].name);

      if (!excludeFiles[excludeIdx].match_prefix)
      {
        cmplen++;
      }
      if (strncmp(de->d_name, excludeFiles[excludeIdx].name, cmplen) == 0)
      {
        elog(DEBUG1, "file \"%s\" excluded from backup", de->d_name);
        excludeFound = true;
        break;
      }
    }

    if (excludeFound)
    {
      continue;
    }

                                                                    
    if (isDbDir && parse_filename_for_nontemp_relation(de->d_name, &relOidChars, &relForkNum))
    {
                                    
      if (relForkNum != INIT_FORKNUM)
      {
        char initForkFile[MAXPGPATH];
        char relOid[OIDCHARS + 1];

           
                                                                     
                                                               
           
        memcpy(relOid, de->d_name, relOidChars);
        relOid[relOidChars] = '\0';
        snprintf(initForkFile, sizeof(initForkFile), "%s/%s_init", path, relOid);

        if (lstat(initForkFile, &statbuf) == 0)
        {
          elog(DEBUG2, "unlogged relation file \"%s\" excluded from backup", de->d_name);

          continue;
        }
      }
    }

                                     
    if (isDbDir && looks_like_temp_rel_name(de->d_name))
    {
      elog(DEBUG2, "temporary relation file \"%s\" excluded from backup", de->d_name);

      continue;
    }

    snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, de->d_name);

                                                 
    if (strcmp(pathbuf, "./global/pg_control") == 0)
    {
      continue;
    }

    if (lstat(pathbuf, &statbuf) != 0)
    {
      if (errno != ENOENT)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file or directory \"%s\": %m", pathbuf)));
      }

                                                                    
      continue;
    }

                                                                
    excludeFound = false;
    for (excludeIdx = 0; excludeDirContents[excludeIdx] != NULL; excludeIdx++)
    {
      if (strcmp(de->d_name, excludeDirContents[excludeIdx]) == 0)
      {
        elog(DEBUG1, "contents of directory \"%s\" excluded from backup", de->d_name);
        size += _tarWriteDir(pathbuf, basepathlen, &statbuf, sizeonly);
        excludeFound = true;
        break;
      }
    }

    if (excludeFound)
    {
      continue;
    }

       
                                                                         
                                                                       
       
    if (statrelpath != NULL && strcmp(pathbuf, statrelpath) == 0)
    {
      elog(DEBUG1, "contents of directory \"%s\" excluded from backup", statrelpath);
      size += _tarWriteDir(pathbuf, basepathlen, &statbuf, sizeonly);
      continue;
    }

       
                                                                        
                                                                           
                                 
       
    if (strcmp(pathbuf, "./pg_wal") == 0)
    {
                                                                  
      size += _tarWriteDir(pathbuf, basepathlen, &statbuf, sizeonly);

         
                                                                  
                                  
         
      size += _tarWriteHeader("./pg_wal/archive_status", NULL, &statbuf, sizeonly);

      continue;                                
    }

                                                
    if (strcmp(path, "./pg_tblspc") == 0 &&
#ifndef WIN32
        S_ISLNK(statbuf.st_mode)
#else
        pgwin32_is_junction(pathbuf)
#endif
    )
    {
#if defined(HAVE_READLINK) || defined(WIN32)
      char linkpath[MAXPGPATH];
      int rllen;

      rllen = readlink(pathbuf, linkpath, sizeof(linkpath));
      if (rllen < 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not read symbolic link \"%s\": %m", pathbuf)));
      }
      if (rllen >= sizeof(linkpath))
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("symbolic link \"%s\" target is too long", pathbuf)));
      }
      linkpath[rllen] = '\0';

      size += _tarWriteHeader(pathbuf + basepathlen + 1, linkpath, &statbuf, sizeonly);
#else

         
                                                                        
                                                                      
                                         
         
      ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tablespaces are not supported on this platform")));
      continue;
#endif                    
    }
    else if (S_ISDIR(statbuf.st_mode))
    {
      bool skip_this_dir = false;
      ListCell *lc;

         
                                                                   
                            
         
      size += _tarWriteHeader(pathbuf + basepathlen + 1, NULL, &statbuf, sizeonly);

         
                                                                       
                                                            
         
      foreach (lc, tablespaces)
      {
        tablespaceinfo *ti = (tablespaceinfo *)lfirst(lc);

           
                                                                       
                                                                      
                 
           
                                                                 
           
        if (ti->rpath && strcmp(ti->rpath, pathbuf + 2) == 0)
        {
          skip_this_dir = true;
          break;
        }
      }

         
                                                                     
         
      if (strcmp(pathbuf, "./pg_tblspc") == 0 && !sendtblspclinks)
      {
        skip_this_dir = true;
      }

      if (!skip_this_dir)
      {
        size += sendDir(pathbuf, basepathlen, sizeonly, tablespaces, sendtblspclinks);
      }
    }
    else if (S_ISREG(statbuf.st_mode))
    {
      bool sent = false;

      if (!sizeonly)
      {
        sent = sendFile(pathbuf, pathbuf + basepathlen + 1, &statbuf, true, isDbDir ? atooid(lastDir + 1) : InvalidOid);
      }

      if (sent || sizeonly)
      {
                                                   
        size += ((statbuf.st_size + 511) & ~511);
        size += 512;                                     
      }
    }
    else
    {
      ereport(WARNING, (errmsg("skipping special file \"%s\"", pathbuf)));
    }
  }
  FreeDir(dir);
  return size;
}

   
                                                       
                                                         
                                                           
                                                
   
static bool
is_checksummed_file(const char *fullpath, const char *filename)
{
                                              
  if (strncmp(fullpath, "./global/", 9) == 0 || strncmp(fullpath, "./base/", 7) == 0 || strncmp(fullpath, "/", 1) == 0)
  {
    int excludeIdx;

                                                        
    for (excludeIdx = 0; noChecksumFiles[excludeIdx].name != NULL; excludeIdx++)
    {
      int cmplen = strlen(noChecksumFiles[excludeIdx].name);

      if (!noChecksumFiles[excludeIdx].match_prefix)
      {
        cmplen++;
      }
      if (strncmp(filename, noChecksumFiles[excludeIdx].name, cmplen) == 0)
      {
        return false;
      }
    }

    return true;
  }
  else
  {
    return false;
  }
}

       
                                          
   
                                                                    
   

   
                                                           
   
                                                                              
   
                                                                                  
                                             
   
                                                                          
                               
   
static bool
sendFile(const char *readfilename, const char *tarfilename, struct stat *statbuf, bool missing_ok, Oid dboid)
{
  FILE *fp;
  BlockNumber blkno = 0;
  bool block_retry = false;
  char buf[TAR_SEND_SIZE];
  uint16 checksum;
  int checksum_failures = 0;
  off_t cnt;
  int i;
  pgoff_t len = 0;
  char *page;
  size_t pad;
  PageHeader phdr;
  int segmentno = 0;
  char *segmentpath;
  bool verify_checksum = false;

  fp = AllocateFile(readfilename, "rb");
  if (fp == NULL)
  {
    if (errno == ENOENT && missing_ok)
    {
      return false;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", readfilename)));
  }

  _tarWriteHeader(tarfilename, NULL, statbuf, false);

  if (!noverify_checksums && DataChecksumsEnabled())
  {
    char *filename;

       
                                                                   
                                                                  
                                 
       
    filename = last_dir_separator(readfilename) + 1;

    if (is_checksummed_file(readfilename, filename))
    {
      verify_checksum = true;

         
                                                                         
                                               
         
      segmentpath = strstr(filename, ".");
      if (segmentpath != NULL)
      {
        segmentno = atoi(segmentpath + 1);
        if (segmentno == 0)
        {
          ereport(ERROR, (errmsg("invalid segment number %d in file \"%s\"", segmentno, filename)));
        }
      }
    }
  }

  while ((cnt = fread(buf, 1, Min(sizeof(buf), statbuf->st_size - len), fp)) > 0)
  {
       
                                                                         
                                                          
                                                                          
                     
       
    Assert(TAR_SEND_SIZE % BLCKSZ == 0);

    if (verify_checksum && (cnt % BLCKSZ != 0))
    {
      ereport(WARNING, (errmsg("could not verify checksum in file \"%s\", block "
                               "%d: read buffer size %d and page size %d "
                               "differ",
                           readfilename, blkno, (int)cnt, BLCKSZ)));
      verify_checksum = false;
    }

    if (verify_checksum)
    {
      for (i = 0; i < cnt / BLCKSZ; i++)
      {
        page = buf + BLCKSZ * i;

           
                                                                   
                                                                     
                                                                     
                                                                      
                                                                    
                                      
           
        if (!PageIsNew(page) && PageGetLSN(page) < startptr)
        {
          checksum = pg_checksum_page((char *)page, blkno + segmentno * RELSEG_SIZE);
          phdr = (PageHeader)page;
          if (phdr->pd_checksum != checksum)
          {
               
                                                           
                                                              
                                                                   
                                                                  
                                                               
                                                                 
                                                                
                        
               
            if (block_retry == false)
            {
                                           
              if (fseek(fp, -(cnt - BLCKSZ * i), SEEK_CUR) == -1)
              {
                ereport(ERROR, (errcode_for_file_access(), errmsg("could not fseek in file \"%s\": %m", readfilename)));
              }

              if (fread(buf + BLCKSZ * i, 1, BLCKSZ, fp) != BLCKSZ)
              {
                   
                                                       
                                                               
                                                               
                                                              
                                                             
                                          
                   
                if (feof(fp))
                {
                  cnt = BLCKSZ * i;
                  break;
                }

                ereport(ERROR, (errcode_for_file_access(), errmsg("could not reread block %d of file \"%s\": %m", blkno, readfilename)));
              }

              if (fseek(fp, cnt - BLCKSZ * i - BLCKSZ, SEEK_CUR) == -1)
              {
                ereport(ERROR, (errcode_for_file_access(), errmsg("could not fseek in file \"%s\": %m", readfilename)));
              }

                                                             
              block_retry = true;

                                                          
              i--;
              continue;
            }

            checksum_failures++;

            if (checksum_failures <= 5)
            {
              ereport(WARNING, (errmsg("checksum verification failed in "
                                       "file \"%s\", block %d: calculated "
                                       "%X but expected %X",
                                   readfilename, blkno, checksum, phdr->pd_checksum)));
            }
            if (checksum_failures == 5)
            {
              ereport(WARNING, (errmsg("further checksum verification "
                                       "failures in file \"%s\" will not "
                                       "be reported",
                                   readfilename)));
            }
          }
        }
        block_retry = false;
        blkno++;
      }
    }

                                              
    if (pq_putmessage('d', buf, cnt))
    {
      ereport(ERROR, (errmsg("base backup could not send data, aborting backup")));
    }

    len += cnt;
    throttle(cnt);

    if (feof(fp) || len >= statbuf->st_size)
    {
         
                                                                  
                                                                         
                                                                  
         
      break;
    }
  }

  CHECK_FREAD_ERROR(fp, readfilename);

                                                                             
  if (len < statbuf->st_size)
  {
    MemSet(buf, 0, sizeof(buf));
    while (len < statbuf->st_size)
    {
      cnt = Min(sizeof(buf), statbuf->st_size - len);
      pq_putmessage('d', buf, cnt);
      len += cnt;
      throttle(cnt);
    }
  }

     
                                                                        
                                                      
     
  pad = ((len + 511) & ~511) - len;
  if (pad > 0)
  {
    MemSet(buf, 0, pad);
    pq_putmessage('d', buf, pad);
  }

  FreeFile(fp);

  if (checksum_failures > 1)
  {
    ereport(WARNING, (errmsg_plural("file \"%s\" has a total of %d checksum verification failure", "file \"%s\" has a total of %d checksum verification failures", checksum_failures, readfilename, checksum_failures)));

    pgstat_report_checksum_failures_in_db(dboid, checksum_failures);
  }

  total_checksum_failures += checksum_failures;

  return true;
}

static int64
_tarWriteHeader(const char *filename, const char *linktarget, struct stat *statbuf, bool sizeonly)
{
  char h[512];
  enum tarError rc;

  if (!sizeonly)
  {
    rc = tarCreateHeader(h, filename, linktarget, statbuf->st_size, statbuf->st_mode, statbuf->st_uid, statbuf->st_gid, statbuf->st_mtime);

    switch (rc)
    {
    case TAR_OK:
      break;
    case TAR_NAME_TOO_LONG:
      ereport(ERROR, (errmsg("file name too long for tar format: \"%s\"", filename)));
      break;
    case TAR_SYMLINK_TOO_LONG:
      ereport(ERROR, (errmsg("symbolic link target too long for tar format: "
                             "file name \"%s\", target \"%s\"",
                         filename, linktarget)));
      break;
    default:
      elog(ERROR, "unrecognized tar error: %d", rc);
    }

    pq_putmessage('d', h, sizeof(h));
  }

  return sizeof(h);
}

   
                                                                             
                                   
   
static int64
_tarWriteDir(const char *pathbuf, int basepathlen, struct stat *statbuf, bool sizeonly)
{
                                                  
#ifndef WIN32
  if (S_ISLNK(statbuf->st_mode))
#else
  if (pgwin32_is_junction(pathbuf))
#endif
    statbuf->st_mode = S_IFDIR | pg_dir_create_mode;

  return _tarWriteHeader(pathbuf + basepathlen + 1, NULL, statbuf, sizeonly);
}

   
                                                                        
                                                                        
         
   
static void
throttle(size_t increment)
{
  TimeOffset elapsed_min;

  if (throttling_counter < 0)
  {
    return;
  }

  throttling_counter += increment;
  if (throttling_counter < throttling_sample)
  {
    return;
  }

                                                     
  elapsed_min = elapsed_min_unit * (throttling_counter / throttling_sample);

     
                                                                         
                                                                 
     
  for (;;)
  {
    TimeOffset elapsed, sleep;
    int wait_result;

                                                                         
    elapsed = GetCurrentTimestamp() - throttled_last;

                                                           
    sleep = elapsed_min - elapsed;
    if (sleep <= 0)
    {
      break;
    }

    ResetLatch(MyLatch);

                                                                       
    CHECK_FOR_INTERRUPTS();

       
                                                                        
                                                                 
       
    wait_result = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, (long)(sleep / 1000), WAIT_EVENT_BASE_BACKUP_THROTTLE);

    if (wait_result & WL_LATCH_SET)
    {
      CHECK_FOR_INTERRUPTS();
    }

                       
    if (wait_result & WL_TIMEOUT)
    {
      break;
    }
  }

     
                                                                            
                                                                             
     
  throttling_counter %= throttling_sample;

     
                                                                         
                 
     
  throttled_last = GetCurrentTimestamp();
}
