                                                                            
   
               
                                                                
   
                                                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "pg_rewind.h"
#include "fetch.h"
#include "file_ops.h"
#include "filemap.h"

#include "access/timeline.h"
#include "access/xlog_internal.h"
#include "catalog/catversion.h"
#include "catalog/pg_control.h"
#include "common/controldata_utils.h"
#include "common/file_perm.h"
#include "common/file_utils.h"
#include "common/restricted_token.h"
#include "getopt_long.h"
#include "storage/bufpage.h"

static void
usage(const char *progname);

static void
createBackupLabel(XLogRecPtr startpoint, TimeLineID starttli, XLogRecPtr checkpointloc);

static void
digestControlFile(ControlFileData *ControlFile, char *source, size_t size);
static void
syncTargetDirectory(void);
static void
sanityChecks(void);
static void
findCommonAncestorTimeline(XLogRecPtr *recptr, int *tliIndex);

static ControlFileData ControlFile_target;
static ControlFileData ControlFile_source;

const char *progname;
int WalSegSz;

                           
char *datadir_target = NULL;
char *datadir_source = NULL;
char *connstr_source = NULL;

static bool debug = false;
bool showprogress = false;
bool dry_run = false;
bool do_sync = true;

                    
TimeLineHistoryEntry *targetHistory;
int targetNentries;

                       
uint64 fetch_size;
uint64 fetch_done;

static void
usage(const char *progname)
{
  printf(_("%s resynchronizes a PostgreSQL cluster with another copy of the cluster.\n\n"), progname);
  printf(_("Usage:\n  %s [OPTION]...\n\n"), progname);
  printf(_("Options:\n"));
  printf(_("  -D, --target-pgdata=DIRECTORY  existing data directory to modify\n"));
  printf(_("      --source-pgdata=DIRECTORY  source data directory to synchronize with\n"));
  printf(_("      --source-server=CONNSTR    source server to synchronize with\n"));
  printf(_("  -n, --dry-run                  stop before modifying anything\n"));
  printf(_("  -N, --no-sync                  do not wait for changes to be written\n"
           "                                 safely to disk\n"));
  printf(_("  -P, --progress                 write progress messages\n"));
  printf(_("      --debug                    write a lot of debug messages\n"));
  printf(_("  -V, --version                  output version information, then exit\n"));
  printf(_("  -?, --help                     show this help, then exit\n"));
  printf(_("\nReport bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

int
main(int argc, char **argv)
{
  static struct option long_options[] = {{"help", no_argument, NULL, '?'}, {"target-pgdata", required_argument, NULL, 'D'}, {"source-pgdata", required_argument, NULL, 1}, {"source-server", required_argument, NULL, 2}, {"version", no_argument, NULL, 'V'}, {"dry-run", no_argument, NULL, 'n'}, {"no-sync", no_argument, NULL, 'N'}, {"progress", no_argument, NULL, 'P'}, {"debug", no_argument, NULL, 3}, {NULL, 0, NULL, 0}};
  int option_index;
  int c;
  XLogRecPtr divergerec;
  int lastcommontliIndex;
  XLogRecPtr chkptrec;
  TimeLineID chkpttli;
  XLogRecPtr chkptredo;
  XLogRecPtr target_wal_endrec;
  size_t size;
  char *buffer;
  bool rewind_needed;
  XLogRecPtr endrec;
  TimeLineID endtli;
  ControlFileData ControlFile_new;

  pg_logging_init(argv[0]);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_rewind"));
  progname = get_progname(argv[0]);

                                      
  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      usage(progname);
      exit(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      puts("pg_rewind (PostgreSQL) " PG_VERSION);
      exit(0);
    }
  }

  while ((c = getopt_long(argc, argv, "D:nNP", long_options, &option_index)) != -1)
  {
    switch (c)
    {
    case '?':
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);

    case 'P':
      showprogress = true;
      break;

    case 'n':
      dry_run = true;
      break;

    case 'N':
      do_sync = false;
      break;

    case 3:
      debug = true;
      pg_logging_set_level(PG_LOG_DEBUG);
      break;

    case 'D':                            
      datadir_target = pg_strdup(optarg);
      break;

    case 1:                      
      datadir_source = pg_strdup(optarg);
      break;
    case 2:                      
      connstr_source = pg_strdup(optarg);
      break;
    }
  }

  if (datadir_source == NULL && connstr_source == NULL)
  {
    pg_log_error("no source specified (--source-pgdata or --source-server)");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (datadir_source != NULL && connstr_source != NULL)
  {
    pg_log_error("only one of --source-pgdata or --source-server can be specified");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (datadir_target == NULL)
  {
    pg_log_error("no target data directory specified (--target-pgdata)");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

     
                                                                       
                                                                           
                                                                            
                         
     
#ifndef WIN32
  if (geteuid() == 0)
  {
    pg_log_error("cannot be executed by \"root\"");
    fprintf(stderr, _("You must run %s as the PostgreSQL superuser.\n"), progname);
    exit(1);
  }
#endif

  get_restricted_token();

                                            
  if (!GetDataDirectoryCreatePerm(datadir_target))
  {
    pg_log_error("could not read permissions of directory \"%s\": %m", datadir_target);
    exit(1);
  }

  umask(pg_mode_mask);

                                
  if (connstr_source)
  {
    libpqConnect(connstr_source);
  }

     
                                                                           
                                             
     
  buffer = slurpFile(datadir_target, "global/pg_control", &size);
  digestControlFile(&ControlFile_target, buffer, size);
  pg_free(buffer);

  buffer = fetchFile("global/pg_control", &size);
  digestControlFile(&ControlFile_source, buffer, size);
  pg_free(buffer);

  sanityChecks();

     
                                                                           
         
     
  if (ControlFile_target.checkPointCopy.ThisTimeLineID == ControlFile_source.checkPointCopy.ThisTimeLineID)
  {
    pg_log_info("source and target cluster are on the same timeline");
    rewind_needed = false;
    target_wal_endrec = 0;
  }
  else
  {
    XLogRecPtr chkptendrec;

    findCommonAncestorTimeline(&divergerec, &lastcommontliIndex);
    pg_log_info("servers diverged at WAL location %X/%X on timeline %u", (uint32)(divergerec >> 32), (uint32)divergerec, targetHistory[lastcommontliIndex].tli);

       
                                               
       
                                                           
                                                                          
                                                                      
                                                                           
       

                                                                        
    chkptendrec = readOneRecord(datadir_target, ControlFile_target.checkPoint, targetNentries - 1);

    if (ControlFile_target.minRecoveryPoint > chkptendrec)
    {
      target_wal_endrec = ControlFile_target.minRecoveryPoint;
    }
    else
    {
      target_wal_endrec = chkptendrec;
    }

       
                                                                     
                                                                           
                                           
       
    if (target_wal_endrec > divergerec)
    {
      rewind_needed = true;
    }
    else
    {
                                                                        
      Assert(target_wal_endrec == divergerec);

      rewind_needed = false;
    }
  }

  if (!rewind_needed)
  {
    pg_log_info("no rewind required");
    exit(0);
  }

  findLastCheckpoint(datadir_target, divergerec, lastcommontliIndex, &chkptrec, &chkpttli, &chkptredo);
  pg_log_info("rewinding from last common checkpoint at %X/%X on timeline %u", (uint32)(chkptrec >> 32), (uint32)chkptrec, chkpttli);

     
                                                                             
     
  filemap_create();
  if (showprogress)
  {
    pg_log_info("reading source file list");
  }
  fetchSourceFileList();
  if (showprogress)
  {
    pg_log_info("reading target file list");
  }
  traverse_datadir(datadir_target, &process_target_file);

     
                                                                           
                                                                          
               
     
  if (showprogress)
  {
    pg_log_info("reading WAL in target");
  }
  extractPageMap(datadir_target, chkptrec, lastcommontliIndex, target_wal_endrec);
  filemap_finalize();

  if (showprogress)
  {
    calculate_totals();
  }

                                                 
  if (debug)
  {
    print_filemap();
  }

     
                                                   
     
  if (showprogress)
  {
    pg_log_info("need to copy %lu MB (total source directory size is %lu MB)", (unsigned long)(filemap->fetch_size / (1024 * 1024)), (unsigned long)(filemap->total_size / (1024 * 1024)));

    fetch_size = filemap->fetch_size;
    fetch_done = 0;
  }

     
                                                                           
                                                                 
     

  executeFileMap();

  progress_report(true);

  if (showprogress)
  {
    pg_log_info("creating backup label and updating control file");
  }
  createBackupLabel(chkptredo, chkpttli, chkptrec);

     
                                                                     
                               
     
                                                                       
                                                                             
                                                                    
     
  memcpy(&ControlFile_new, &ControlFile_source, sizeof(ControlFileData));

  if (connstr_source)
  {
    endrec = libpqGetCurrentXlogInsertLocation();
    endtli = ControlFile_source.checkPointCopy.ThisTimeLineID;
  }
  else
  {
    endrec = ControlFile_source.checkPoint;
    endtli = ControlFile_source.checkPointCopy.ThisTimeLineID;
  }
  ControlFile_new.minRecoveryPoint = endrec;
  ControlFile_new.minRecoveryPointTLI = endtli;
  ControlFile_new.state = DB_IN_ARCHIVE_RECOVERY;
  if (!dry_run)
  {
    update_controlfile(datadir_target, &ControlFile_new, do_sync);
  }

  if (showprogress)
  {
    pg_log_info("syncing target data directory");
  }
  syncTargetDirectory();

  pg_log_info("Done!");

  return 0;
}

static void
sanityChecks(void)
{
                                                                 

                             
  if (ControlFile_target.system_identifier != ControlFile_source.system_identifier)
  {
    pg_fatal("source and target clusters are from different systems");
  }

                     
  if (ControlFile_target.pg_control_version != PG_CONTROL_VERSION || ControlFile_source.pg_control_version != PG_CONTROL_VERSION || ControlFile_target.catalog_version_no != CATALOG_VERSION_NO || ControlFile_source.catalog_version_no != CATALOG_VERSION_NO)
  {
    pg_fatal("clusters are not compatible with this version of pg_rewind");
  }

     
                                                                           
                                                                         
     
  if (ControlFile_target.data_checksum_version != PG_DATA_CHECKSUM_VERSION && !ControlFile_target.wal_log_hints)
  {
    pg_fatal("target server needs to use either data checksums or \"wal_log_hints = on\"");
  }

     
                                                                      
                                                                            
                                                                         
                                                         
     
  if (ControlFile_target.state != DB_SHUTDOWNED && ControlFile_target.state != DB_SHUTDOWNED_IN_RECOVERY)
  {
    pg_fatal("target server must be shut down cleanly");
  }

     
                                                                       
                                                                      
                                             
     
  if (datadir_source && ControlFile_source.state != DB_SHUTDOWNED && ControlFile_source.state != DB_SHUTDOWNED_IN_RECOVERY)
  {
    pg_fatal("source data directory must be shut down cleanly");
  }
}

   
                                                                             
   
                                                                          
                                           
   
                                                                            
                              
   
void
progress_report(bool finished)
{
  static pg_time_t last_progress_report = 0;
  int percent;
  char fetch_done_str[32];
  char fetch_size_str[32];
  pg_time_t now;

  if (!showprogress)
  {
    return;
  }

  now = time(NULL);
  if (now == last_progress_report && !finished)
  {
    return;                          
  }

  last_progress_report = now;
  percent = fetch_size ? (int)((fetch_done) * 100 / fetch_size) : 0;

     
                                                                           
                                                                           
                                                                             
                                               
     
  if (percent > 100)
  {
    percent = 100;
  }
  if (fetch_done > fetch_size)
  {
    fetch_size = fetch_done;
  }

     
                                                                 
                                                                           
                               
     
  snprintf(fetch_done_str, sizeof(fetch_done_str), INT64_FORMAT, fetch_done / 1024);
  snprintf(fetch_size_str, sizeof(fetch_size_str), INT64_FORMAT, fetch_size / 1024);

  fprintf(stderr, _("%*s/%s kB (%d%%) copied"), (int)strlen(fetch_size_str), fetch_done_str, fetch_size_str, percent);

     
                                                                         
          
     
  fputc((!finished && isatty(fileno(stderr))) ? '\r' : '\n', stderr);
}

   
                                                                        
                                                                         
                                                                       
   
static XLogRecPtr
MinXLogRecPtr(XLogRecPtr a, XLogRecPtr b)
{
  if (XLogRecPtrIsInvalid(a))
  {
    return b;
  }
  else if (XLogRecPtrIsInvalid(b))
  {
    return a;
  }
  else
  {
    return Min(a, b);
  }
}

   
                                                                        
                            
   
static TimeLineHistoryEntry *
getTimelineHistory(ControlFileData *controlFile, int *nentries)
{
  TimeLineHistoryEntry *history;
  TimeLineID tli;

  tli = controlFile->checkPointCopy.ThisTimeLineID;

     
                                                                           
                                                              
     
  if (tli == 1)
  {
    history = (TimeLineHistoryEntry *)pg_malloc(sizeof(TimeLineHistoryEntry));
    history->tli = tli;
    history->begin = history->end = InvalidXLogRecPtr;
    *nentries = 1;
  }
  else
  {
    char path[MAXPGPATH];
    char *histfile;

    TLHistoryFilePath(path, tli);

                                                  
    if (controlFile == &ControlFile_source)
    {
      histfile = fetchFile(path, NULL);
    }
    else if (controlFile == &ControlFile_target)
    {
      histfile = slurpFile(datadir_target, path, NULL);
    }
    else
    {
      pg_fatal("invalid control file");
    }

    history = rewind_parseTimeLineHistory(histfile, tli, nentries);
    pg_free(histfile);
  }

  if (debug)
  {
    int i;

    if (controlFile == &ControlFile_source)
    {
      pg_log_debug("Source timeline history:");
    }
    else if (controlFile == &ControlFile_target)
    {
      pg_log_debug("Target timeline history:");
    }
    else
    {
      Assert(false);
    }

       
                                          
       
    for (i = 0; i < targetNentries; i++)
    {
      TimeLineHistoryEntry *entry;

      entry = &history[i];
      pg_log_debug("%d: %X/%X - %X/%X", entry->tli, (uint32)(entry->begin >> 32), (uint32)(entry->begin), (uint32)(entry->end >> 32), (uint32)(entry->end));
    }
  }

  return history;
}

   
                                                                                
                                                                          
                                                                               
                                                                               
                                                                              
                                          
   
                                                                              
                                
   
static void
findCommonAncestorTimeline(XLogRecPtr *recptr, int *tliIndex)
{
  TimeLineHistoryEntry *sourceHistory;
  int sourceNentries;
  int i, n;

                                                     
  sourceHistory = getTimelineHistory(&ControlFile_source, &sourceNentries);
  targetHistory = getTimelineHistory(&ControlFile_target, &targetNentries);

     
                                                                          
                                                                      
                                                                        
                                                                           
                                                                            
                                                                             
     
  n = Min(sourceNentries, targetNentries);
  for (i = 0; i < n; i++)
  {
    if (sourceHistory[i].tli != targetHistory[i].tli || sourceHistory[i].begin != targetHistory[i].begin)
    {
      break;
    }
  }

  if (i > 0)
  {
    i--;
    *recptr = MinXLogRecPtr(sourceHistory[i].end, targetHistory[i].end);
    *tliIndex = i;

    pg_free(sourceHistory);
    return;
  }
  else
  {
    pg_fatal("could not find common ancestor of the source and target cluster's timelines");
  }
}

   
                                                                               
               
   
static void
createBackupLabel(XLogRecPtr startpoint, TimeLineID starttli, XLogRecPtr checkpointloc)
{
  XLogSegNo startsegno;
  time_t stamp_time;
  char strfbuf[128];
  char xlogfilename[MAXFNAMELEN];
  struct tm *tmp;
  char buf[1000];
  int len;

  XLByteToSeg(startpoint, startsegno, WalSegSz);
  XLogFileName(xlogfilename, starttli, startsegno, WalSegSz);

     
                                 
     
  stamp_time = time(NULL);
  tmp = localtime(&stamp_time);
  strftime(strfbuf, sizeof(strfbuf), "%Y-%m-%d %H:%M:%S %Z", tmp);

  len = snprintf(buf, sizeof(buf),
      "START WAL LOCATION: %X/%X (file %s)\n"
      "CHECKPOINT LOCATION: %X/%X\n"
      "BACKUP METHOD: pg_rewind\n"
      "BACKUP FROM: standby\n"
      "START TIME: %s\n",
                            
      (uint32)(startpoint >> 32), (uint32)startpoint, xlogfilename, (uint32)(checkpointloc >> 32), (uint32)checkpointloc, strfbuf);
  if (len >= sizeof(buf))
  {
    pg_fatal("backup label buffer too small");                       
  }

                                                   
  open_target_file("backup_label", true);                        
  write_target_range(buf, 0, len);
  close_target_file();
}

   
                             
   
static void
checkControlFile(ControlFileData *ControlFile)
{
  pg_crc32c crc;

                     
  INIT_CRC32C(crc);
  COMP_CRC32C(crc, (char *)ControlFile, offsetof(ControlFileData, crc));
  FIN_CRC32C(crc);

                             
  if (!EQ_CRC32C(crc, ControlFile->crc))
  {
    pg_fatal("unexpected control file CRC");
  }
}

   
                                                                                
   
static void
digestControlFile(ControlFileData *ControlFile, char *src, size_t size)
{
  if (size != PG_CONTROL_FILE_SIZE)
  {
    pg_fatal("unexpected control file size %d, expected %d", (int)size, PG_CONTROL_FILE_SIZE);
  }

  memcpy(ControlFile, src, sizeof(ControlFileData));

                                 
  WalSegSz = ControlFile->xlog_seg_size;

  if (!IsValidWalSegSize(WalSegSz))
  {
    pg_fatal(ngettext("WAL segment size must be a power of two between 1 MB and 1 GB, but the control file specifies %d byte", "WAL segment size must be a power of two between 1 MB and 1 GB, but the control file specifies %d bytes", WalSegSz), WalSegSz);
  }

                                         
  checkControlFile(ControlFile);
}

   
                                                                               
   
                                                                               
                                                                            
                                                                          
                                                                               
                                        
   
static void
syncTargetDirectory(void)
{
  if (!do_sync || dry_run)
  {
    return;
  }

  fsync_pgdata(datadir_target, PG_VERSION_NUM);
}
