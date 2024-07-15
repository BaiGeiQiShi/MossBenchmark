                                                                            
   
                                                                                
   
                                                 
   
                                                                         
   
                  
                                            
                                                                            
   

#include "postgres_fe.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#include "access/xlog_internal.h"
#include "common/file_perm.h"
#include "common/file_utils.h"
#include "common/logging.h"
#include "common/string.h"
#include "fe_utils/string_utils.h"
#include "getopt_long.h"
#include "libpq-fe.h"
#include "pqexpbuffer.h"
#include "pgtar.h"
#include "pgtime.h"
#include "receivelog.h"
#include "replication/basebackup.h"
#include "streamutil.h"

#define ERRCODE_DATA_CORRUPTED "XX001"

typedef struct TablespaceListCell
{
  struct TablespaceListCell *next;
  char old_dir[MAXPGPATH];
  char new_dir[MAXPGPATH];
} TablespaceListCell;

typedef struct TablespaceList
{
  TablespaceListCell *head;
  TablespaceListCell *tail;
} TablespaceList;

   
                                                                          
                                              
   
#define MINIMUM_VERSION_FOR_PG_WAL 100000

   
                                                              
   
#define MINIMUM_VERSION_FOR_TEMP_SLOTS 100000

   
                                                                     
   
#define MINIMUM_VERSION_FOR_RECOVERY_GUC 120000

   
                                 
   
typedef enum
{
  NO_WAL,
  FETCH_WAL,
  STREAM_WAL
} IncludeWal;

                    
static char *basedir = NULL;
static TablespaceList tablespace_dirs = {NULL, NULL};
static char *xlog_dir = NULL;
static char format = 'p';                    
static char *label = "pg_basebackup base backup";
static bool noclean = false;
static bool checksum_failure = false;
static bool showprogress = false;
static int verbose = 0;
static int compresslevel = 0;
static IncludeWal includewal = STREAM_WAL;
static bool fastcheckpoint = false;
static bool writerecoveryconf = false;
static bool do_sync = true;
static int standby_message_timeout = 10 * 1000;                       
static pg_time_t last_progress_report = 0;
static int32 maxrate = 0;                          
static char *replication_slot = NULL;
static bool temp_replication_slot = true;
static bool create_slot = false;
static bool no_slot = false;
static bool verify_checksums = true;

static bool success = false;
static bool made_new_pgdata = false;
static bool found_existing_pgdata = false;
static bool made_new_xlogdir = false;
static bool found_existing_xlogdir = false;
static bool made_tablespace_dirs = false;
static bool found_tablespace_dirs = false;

                       
static uint64 totalsize;
static uint64 totaldone;
static int tablespacecount;

                                                              
#ifndef WIN32
static int bgpipe[2] = {-1, -1};
#endif

                             
static pid_t bgchild = -1;
static bool in_log_streamer = false;

                                                                  
static XLogRecPtr xlogendptr;

#ifndef WIN32
static int has_xlogendptr = 0;
#else
static volatile LONG has_xlogendptr = 0;
#endif

                                                    
static PQExpBuffer recoveryconfcontents = NULL;

                      
static void
usage(void);
static void
verify_dir_is_empty_or_create(char *dirname, bool *created, bool *found);
static void
progress_report(int tablespacenum, const char *filename, bool force, bool finished);

static void
ReceiveTarFile(PGconn *conn, PGresult *res, int rownum);
static void
ReceiveAndUnpackTarFile(PGconn *conn, PGresult *res, int rownum);
static void
GenerateRecoveryConf(PGconn *conn);
static void
WriteRecoveryConf(void);
static void
BaseBackup(void);

static bool
reached_end_position(XLogRecPtr segendpos, uint32 timeline, bool segment_finished);

static const char *
get_tablespace_mapping(const char *dir);
static void
tablespace_list_append(const char *arg);

static void
cleanup_directories_atexit(void)
{
  if (success || in_log_streamer)
  {
    return;
  }

  if (!noclean && !checksum_failure)
  {
    if (made_new_pgdata)
    {
      pg_log_info("removing data directory \"%s\"", basedir);
      if (!rmtree(basedir, true))
      {
        pg_log_error("failed to remove data directory");
      }
    }
    else if (found_existing_pgdata)
    {
      pg_log_info("removing contents of data directory \"%s\"", basedir);
      if (!rmtree(basedir, false))
      {
        pg_log_error("failed to remove contents of data directory");
      }
    }

    if (made_new_xlogdir)
    {
      pg_log_info("removing WAL directory \"%s\"", xlog_dir);
      if (!rmtree(xlog_dir, true))
      {
        pg_log_error("failed to remove WAL directory");
      }
    }
    else if (found_existing_xlogdir)
    {
      pg_log_info("removing contents of WAL directory \"%s\"", xlog_dir);
      if (!rmtree(xlog_dir, false))
      {
        pg_log_error("failed to remove contents of WAL directory");
      }
    }
  }
  else
  {
    if ((made_new_pgdata || found_existing_pgdata) && !checksum_failure)
    {
      pg_log_info("data directory \"%s\" not removed at user's request", basedir);
    }

    if (made_new_xlogdir || found_existing_xlogdir)
    {
      pg_log_info("WAL directory \"%s\" not removed at user's request", xlog_dir);
    }
  }

  if ((made_tablespace_dirs || found_tablespace_dirs) && !checksum_failure)
  {
    pg_log_info("changes to tablespace directories will not be undone");
  }
}

static void
disconnect_atexit(void)
{
  if (conn != NULL)
  {
    PQfinish(conn);
  }
}

#ifndef WIN32
   
                                                                         
                                                                       
                                                 
   
static void
kill_bgchild_atexit(void)
{
  if (bgchild > 0)
  {
    kill(bgchild, SIGTERM);
  }
}
#endif

   
                                                                            
         
   
static void
tablespace_list_append(const char *arg)
{
  TablespaceListCell *cell = (TablespaceListCell *)pg_malloc0(sizeof(TablespaceListCell));
  char *dst;
  char *dst_ptr;
  const char *arg_ptr;

  dst_ptr = dst = cell->old_dir;
  for (arg_ptr = arg; *arg_ptr; arg_ptr++)
  {
    if (dst_ptr - dst >= MAXPGPATH)
    {
      pg_log_error("directory name too long");
      exit(1);
    }

    if (*arg_ptr == '\\' && *(arg_ptr + 1) == '=')
      ;                                
    else if (*arg_ptr == '=' && (arg_ptr == arg || *(arg_ptr - 1) != '\\'))
    {
      if (*cell->new_dir)
      {
        pg_log_error("multiple \"=\" signs in tablespace mapping");
        exit(1);
      }
      else
      {
        dst = dst_ptr = cell->new_dir;
      }
    }
    else
    {
      *dst_ptr++ = *arg_ptr;
    }
  }

  if (!*cell->old_dir || !*cell->new_dir)
  {
    pg_log_error("invalid tablespace mapping format \"%s\", must be \"OLDDIR=NEWDIR\"", arg);
    exit(1);
  }

     
                                                                            
                                                                              
                                                                           
                                                                           
                                         
     
                                                                        
                                                                         
                                                                  
                                                                       
                                                                          
                                                                         
                                                              
     
  if (!is_nonwindows_absolute_path(cell->old_dir) && !is_windows_absolute_path(cell->old_dir))
  {
    pg_log_error("old directory is not an absolute path in tablespace mapping: %s", cell->old_dir);
    exit(1);
  }

  if (!is_absolute_path(cell->new_dir))
  {
    pg_log_error("new directory is not an absolute path in tablespace mapping: %s", cell->new_dir);
    exit(1);
  }

     
                                                                 
                                                                           
                                                             
     
  canonicalize_path(cell->old_dir);
  canonicalize_path(cell->new_dir);

  if (tablespace_dirs.tail)
  {
    tablespace_dirs.tail->next = cell;
  }
  else
  {
    tablespace_dirs.head = cell;
  }
  tablespace_dirs.tail = cell;
}

#ifdef HAVE_LIBZ
static const char *
get_gz_error(gzFile gzf)
{
  int errnum;
  const char *errmsg;

  errmsg = gzerror(gzf, &errnum);
  if (errnum == Z_ERRNO)
  {
    return strerror(errno);
  }
  else
  {
    return errmsg;
  }
}
#endif

static void
usage(void)
{
  printf(_("%s takes a base backup of a running PostgreSQL server.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]...\n"), progname);
  printf(_("\nOptions controlling the output:\n"));
  printf(_("  -D, --pgdata=DIRECTORY receive base backup into directory\n"));
  printf(_("  -F, --format=p|t       output format (plain (default), tar)\n"));
  printf(_("  -r, --max-rate=RATE    maximum transfer rate to transfer data directory\n"
           "                         (in kB/s, or use suffix \"k\" or \"M\")\n"));
  printf(_("  -R, --write-recovery-conf\n"
           "                         write configuration for replication\n"));
  printf(_("  -T, --tablespace-mapping=OLDDIR=NEWDIR\n"
           "                         relocate tablespace in OLDDIR to NEWDIR\n"));
  printf(_("      --waldir=WALDIR    location for the write-ahead log directory\n"));
  printf(_("  -X, --wal-method=none|fetch|stream\n"
           "                         include required WAL files with specified method\n"));
  printf(_("  -z, --gzip             compress tar output\n"));
  printf(_("  -Z, --compress=0-9     compress tar output with given compression level\n"));
  printf(_("\nGeneral options:\n"));
  printf(_("  -c, --checkpoint=fast|spread\n"
           "                         set fast or spread checkpointing\n"));
  printf(_("  -C, --create-slot      create replication slot\n"));
  printf(_("  -l, --label=LABEL      set backup label\n"));
  printf(_("  -n, --no-clean         do not clean up after errors\n"));
  printf(_("  -N, --no-sync          do not wait for changes to be written safely to disk\n"));
  printf(_("  -P, --progress         show progress information\n"));
  printf(_("  -S, --slot=SLOTNAME    replication slot to use\n"));
  printf(_("  -v, --verbose          output verbose messages\n"));
  printf(_("  -V, --version          output version information, then exit\n"));
  printf(_("      --no-slot          prevent creation of temporary replication slot\n"));
  printf(_("      --no-verify-checksums\n"
           "                         do not verify checksums\n"));
  printf(_("  -?, --help             show this help, then exit\n"));
  printf(_("\nConnection options:\n"));
  printf(_("  -d, --dbname=CONNSTR   connection string\n"));
  printf(_("  -h, --host=HOSTNAME    database server host or socket directory\n"));
  printf(_("  -p, --port=PORT        database server port number\n"));
  printf(_("  -s, --status-interval=INTERVAL\n"
           "                         time between status packets sent to server (in seconds)\n"));
  printf(_("  -U, --username=NAME    connect as specified database user\n"));
  printf(_("  -w, --no-password      never prompt for password\n"));
  printf(_("  -W, --password         force password prompt (should happen automatically)\n"));
  printf(_("\nReport bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

   
                                                                 
                                                             
                                                                      
                       
                                                                        
                 
   
static bool
reached_end_position(XLogRecPtr segendpos, uint32 timeline, bool segment_finished)
{
  if (!has_xlogendptr)
  {
#ifndef WIN32
    fd_set fds;
    struct timeval tv;
    int r;

       
                                                                        
                      
       
    FD_ZERO(&fds);
    FD_SET(bgpipe[0], &fds);

    MemSet(&tv, 0, sizeof(tv));

    r = select(bgpipe[0] + 1, &fds, NULL, NULL, &tv);
    if (r == 1)
    {
      char xlogend[64];
      uint32 hi, lo;

      MemSet(xlogend, 0, sizeof(xlogend));
      r = read(bgpipe[0], xlogend, sizeof(xlogend) - 1);
      if (r < 0)
      {
        pg_log_error("could not read from ready pipe: %m");
        exit(1);
      }

      if (sscanf(xlogend, "%X/%X", &hi, &lo) != 2)
      {
        pg_log_error("could not parse write-ahead log location \"%s\"", xlogend);
        exit(1);
      }
      xlogendptr = ((uint64)hi) << 32 | lo;
      has_xlogendptr = 1;

         
                                                                  
                  
         
    }
    else
    {
         
                                                                  
                                                               
         
      return false;
    }
#else

       
                                                                          
                                                             
       
    return false;
#endif
  }

     
                                                                        
                                                  
     
  if (segendpos >= xlogendptr)
  {
    return true;
  }

     
                                                                          
                     
     
  return false;
}

typedef struct
{
  PGconn *bgconn;
  XLogRecPtr startptr;
  char xlog[MAXPGPATH];                                             
  char *sysidentifier;
  int timeline;
} logstreamer_param;

static int
LogStreamerMain(logstreamer_param *param)
{
  StreamCtl stream;

  in_log_streamer = true;

  MemSet(&stream, 0, sizeof(stream));
  stream.startpos = param->startptr;
  stream.timeline = param->timeline;
  stream.sysidentifier = param->sysidentifier;
  stream.stream_stop = reached_end_position;
#ifndef WIN32
  stream.stop_socket = bgpipe[0];
#else
  stream.stop_socket = PGINVALID_SOCKET;
#endif
  stream.standby_message_timeout = standby_message_timeout;
  stream.synchronous = false;
                                                              
  stream.do_sync = false;
  stream.mark_done = true;
  stream.partial_suffix = NULL;
  stream.replication_slot = replication_slot;

  if (format == 'p')
  {
    stream.walmethod = CreateWalDirectoryMethod(param->xlog, 0, stream.do_sync);
  }
  else
  {
    stream.walmethod = CreateWalTarMethod(param->xlog, compresslevel, stream.do_sync);
  }

  if (!ReceiveXlogStream(param->bgconn, &stream))
  {

       
                                                                           
                                                                        
            
       
    return 1;
  }

  if (!stream.walmethod->finish())
  {
    pg_log_error("could not finish writing WAL files: %m");
    return 1;
  }

  PQfinish(param->bgconn);

  if (format == 'p')
  {
    FreeWalDirectoryMethod();
  }
  else
  {
    FreeWalTarMethod();
  }
  pg_free(stream.walmethod);

  return 0;
}

   
                                                                     
                                                                        
                                                    
   
static void
StartLogStreamer(char *startpos, uint32 timeline, char *sysidentifier)
{
  logstreamer_param *param;
  uint32 hi, lo;
  char statusdir[MAXPGPATH];

  param = pg_malloc0(sizeof(logstreamer_param));
  param->timeline = timeline;
  param->sysidentifier = sysidentifier;

                                     
  if (sscanf(startpos, "%X/%X", &hi, &lo) != 2)
  {
    pg_log_error("could not parse write-ahead log location \"%s\"", startpos);
    exit(1);
  }
  param->startptr = ((uint64)hi) << 32 | lo;
                                          
  param->startptr -= XLogSegmentOffset(param->startptr, WalSegSz);

#ifndef WIN32
                                  
  if (pipe(bgpipe) < 0)
  {
    pg_log_error("could not create pipe for background process: %m");
    exit(1);
  }
#endif

                               
  param->bgconn = GetConnection();
  if (!param->bgconn)
  {
                                                          
    exit(1);
  }

                                                              
  snprintf(param->xlog, sizeof(param->xlog), "%s/%s", basedir, PQserverVersion(conn) < MINIMUM_VERSION_FOR_PG_WAL ? "pg_xlog" : "pg_wal");

                                                                      
  if (PQserverVersion(conn) < MINIMUM_VERSION_FOR_TEMP_SLOTS)
  {
    temp_replication_slot = false;
  }

     
                                          
     
  if (temp_replication_slot && !replication_slot)
  {
    replication_slot = psprintf("pg_basebackup_%d", (int)PQbackendPID(param->bgconn));
  }
  if (temp_replication_slot || create_slot)
  {
    if (!CreateReplicationSlot(param->bgconn, replication_slot, NULL, temp_replication_slot, true, true, false))
    {
      exit(1);
    }

    if (verbose)
    {
      if (temp_replication_slot)
      {
        pg_log_info("created temporary replication slot \"%s\"", replication_slot);
      }
      else
      {
        pg_log_info("created replication slot \"%s\"", replication_slot);
      }
    }
  }

  if (format == 'p')
  {
       
                                                                        
                                                                         
                                                                          
                                  
       
    snprintf(statusdir, sizeof(statusdir), "%s/%s/archive_status", basedir, PQserverVersion(conn) < MINIMUM_VERSION_FOR_PG_WAL ? "pg_xlog" : "pg_wal");

    if (pg_mkdir_p(statusdir, pg_dir_create_mode) != 0 && errno != EEXIST)
    {
      pg_log_error("could not create directory \"%s\": %m", statusdir);
      exit(1);
    }
  }

     
                                                                            
                                               
     
#ifndef WIN32
  bgchild = fork();
  if (bgchild == 0)
  {
                          
    exit(LogStreamerMain(param));
  }
  else if (bgchild < 0)
  {
    pg_log_error("could not create background process: %m");
    exit(1);
  }

     
                                                        
     
  atexit(kill_bgchild_atexit);
#else            
  bgchild = _beginthreadex(NULL, 0, (void *)LogStreamerMain, param, 0, NULL);
  if (bgchild == 0)
  {
    pg_log_error("could not create background thread: %m");
    exit(1);
  }
#endif
}

   
                                                                       
                                                                      
                                   
   
static void
verify_dir_is_empty_or_create(char *dirname, bool *created, bool *found)
{
  switch (pg_check_dir(dirname))
  {
  case 0:

       
                                 
       
    if (pg_mkdir_p(dirname, pg_dir_create_mode) == -1)
    {
      pg_log_error("could not create directory \"%s\": %m", dirname);
      exit(1);
    }
    if (created)
    {
      *created = true;
    }
    return;
  case 1:

       
                     
       
    if (found)
    {
      *found = true;
    }
    return;
  case 2:
  case 3:
  case 4:

       
                         
       
    pg_log_error("directory \"%s\" exists but is not empty", dirname);
    exit(1);
  case -1:

       
                      
       
    pg_log_error("could not access directory \"%s\": %m", dirname);
    exit(1);
  }
}

   
                                                                            
                                                 
   
                                                                           
                             
   
                                                                            
                              
   
static void
progress_report(int tablespacenum, const char *filename, bool force, bool finished)
{
  int percent;
  char totaldone_str[32];
  char totalsize_str[32];
  pg_time_t now;

  if (!showprogress)
  {
    return;
  }

  now = time(NULL);
  if (now == last_progress_report && !force && !finished)
  {
    return;                          
  }

  last_progress_report = now;
  percent = totalsize ? (int)((totaldone / 1024) * 100 / totalsize) : 0;

     
                                                                           
                                                                           
                                                                             
                                               
     
  if (percent > 100)
  {
    percent = 100;
  }
  if (totaldone / 1024 > totalsize)
  {
    totalsize = totaldone / 1024;
  }

     
                                                                 
                                                                           
                               
     
  snprintf(totaldone_str, sizeof(totaldone_str), INT64_FORMAT, totaldone / 1024);
  snprintf(totalsize_str, sizeof(totalsize_str), INT64_FORMAT, totalsize);

#define VERBOSE_FILENAME_LENGTH 35
  if (verbose)
  {
    if (!filename)
    {

         
                                                                    
               
         
      fprintf(stderr, ngettext("%*s/%s kB (100%%), %d/%d tablespace %*s", "%*s/%s kB (100%%), %d/%d tablespaces %*s", tablespacecount), (int)strlen(totalsize_str), totaldone_str, totalsize_str, tablespacenum, tablespacecount, VERBOSE_FILENAME_LENGTH + 5, "");
    }
    else
    {
      bool truncate = (strlen(filename) > VERBOSE_FILENAME_LENGTH);

      fprintf(stderr, ngettext("%*s/%s kB (%d%%), %d/%d tablespace (%s%-*.*s)", "%*s/%s kB (%d%%), %d/%d tablespaces (%s%-*.*s)", tablespacecount), (int)strlen(totalsize_str), totaldone_str, totalsize_str, percent, tablespacenum, tablespacecount,
                                                             
          truncate ? "..." : "", truncate ? VERBOSE_FILENAME_LENGTH - 3 : VERBOSE_FILENAME_LENGTH, truncate ? VERBOSE_FILENAME_LENGTH - 3 : VERBOSE_FILENAME_LENGTH,
                                                               
          truncate ? filename + strlen(filename) - VERBOSE_FILENAME_LENGTH + 3 : filename);
    }
  }
  else
  {
    fprintf(stderr, ngettext("%*s/%s kB (%d%%), %d/%d tablespace", "%*s/%s kB (%d%%), %d/%d tablespaces", tablespacecount), (int)strlen(totalsize_str), totaldone_str, totalsize_str, percent, tablespacenum, tablespacecount);
  }

     
                                                                         
          
     
  fputc((!finished && isatty(fileno(stderr))) ? '\r' : '\n', stderr);
}

static int32
parse_max_rate(char *src)
{
  double result;
  char *after_num;
  char *suffix = NULL;

  errno = 0;
  result = strtod(src, &after_num);
  if (src == after_num)
  {
    pg_log_error("transfer rate \"%s\" is not a valid value", src);
    exit(1);
  }
  if (errno != 0)
  {
    pg_log_error("invalid transfer rate \"%s\": %m", src);
    exit(1);
  }

  if (result <= 0)
  {
       
                                           
       
    pg_log_error("transfer rate must be greater than zero");
    exit(1);
  }

     
                                                                       
                             
     
  while (*after_num != '\0' && isspace((unsigned char)*after_num))
  {
    after_num++;
  }

  if (*after_num != '\0')
  {
    suffix = after_num;
    if (*after_num == 'k')
    {
                                          
      after_num++;
    }
    else if (*after_num == 'M')
    {
      after_num++;
      result *= 1024.0;
    }
  }

                                                 
  while (*after_num != '\0' && isspace((unsigned char)*after_num))
  {
    after_num++;
  }

  if (*after_num != '\0')
  {
    pg_log_error("invalid --max-rate unit: \"%s\"", suffix);
    exit(1);
  }

                      
  if ((uint64)result != (uint64)((uint32)result))
  {
    pg_log_error("transfer rate \"%s\" exceeds integer range", src);
    exit(1);
  }

     
                                                                       
                                                   
     
  if (result < MAX_RATE_LOWER || result > MAX_RATE_UPPER)
  {
    pg_log_error("transfer rate \"%s\" is out of range", src);
    exit(1);
  }

  return (int32)result;
}

   
                             
   
static void
writeTarData(
#ifdef HAVE_LIBZ
    gzFile ztarfile,
#endif
    FILE *tarfile, char *buf, int r, char *current_file)
{
#ifdef HAVE_LIBZ
  if (ztarfile != NULL)
  {
    errno = 0;
    if (gzwrite(ztarfile, buf, r) != r)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      pg_log_error("could not write to compressed file \"%s\": %s", current_file, get_gz_error(ztarfile));
      exit(1);
    }
  }
  else
#endif
  {
    errno = 0;
    if (fwrite(buf, r, 1, tarfile) != 1)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      pg_log_error("could not write to file \"%s\": %m", current_file);
      exit(1);
    }
  }
}

#ifdef HAVE_LIBZ
#define WRITE_TAR_DATA(buf, sz) writeTarData(ztarfile, tarfile, buf, sz, filename)
#else
#define WRITE_TAR_DATA(buf, sz) writeTarData(tarfile, buf, sz, filename)
#endif

   
                                                                          
                                                                       
                                                                   
   
                                                                            
                                                               
   
                                                                       
   
static void
ReceiveTarFile(PGconn *conn, PGresult *res, int rownum)
{
  char filename[MAXPGPATH];
  char *copybuf = NULL;
  FILE *tarfile = NULL;
  char tarhdr[512];
  bool basetablespace = PQgetisnull(res, rownum, 0);
  bool in_tarhdr = true;
  bool skip_file = false;
  bool is_recovery_guc_supported = true;
  bool is_postgresql_auto_conf = false;
  bool found_postgresql_auto_conf = false;
  int file_padding_len = 0;
  size_t tarhdrsz = 0;
  pgoff_t filesz = 0;

#ifdef HAVE_LIBZ
  gzFile ztarfile = NULL;
#endif

                                                                        
  if (PQserverVersion(conn) < MINIMUM_VERSION_FOR_RECOVERY_GUC)
  {
    is_recovery_guc_supported = false;
  }

  if (basetablespace)
  {
       
                        
       
    if (strcmp(basedir, "-") == 0)
    {
#ifdef WIN32
      _setmode(fileno(stdout), _O_BINARY);
#endif

#ifdef HAVE_LIBZ
      if (compresslevel != 0)
      {
        ztarfile = gzdopen(dup(fileno(stdout)), "wb");
        if (gzsetparams(ztarfile, compresslevel, Z_DEFAULT_STRATEGY) != Z_OK)
        {
          pg_log_error("could not set compression level %d: %s", compresslevel, get_gz_error(ztarfile));
          exit(1);
        }
      }
      else
#endif
        tarfile = stdout;
      strcpy(filename, "-");
    }
    else
    {
#ifdef HAVE_LIBZ
      if (compresslevel != 0)
      {
        snprintf(filename, sizeof(filename), "%s/base.tar.gz", basedir);
        ztarfile = gzopen(filename, "wb");
        if (gzsetparams(ztarfile, compresslevel, Z_DEFAULT_STRATEGY) != Z_OK)
        {
          pg_log_error("could not set compression level %d: %s", compresslevel, get_gz_error(ztarfile));
          exit(1);
        }
      }
      else
#endif
      {
        snprintf(filename, sizeof(filename), "%s/base.tar", basedir);
        tarfile = fopen(filename, "wb");
      }
    }
  }
  else
  {
       
                           
       
#ifdef HAVE_LIBZ
    if (compresslevel != 0)
    {
      snprintf(filename, sizeof(filename), "%s/%s.tar.gz", basedir, PQgetvalue(res, rownum, 0));
      ztarfile = gzopen(filename, "wb");
      if (gzsetparams(ztarfile, compresslevel, Z_DEFAULT_STRATEGY) != Z_OK)
      {
        pg_log_error("could not set compression level %d: %s", compresslevel, get_gz_error(ztarfile));
        exit(1);
      }
    }
    else
#endif
    {
      snprintf(filename, sizeof(filename), "%s/%s.tar", basedir, PQgetvalue(res, rownum, 0));
      tarfile = fopen(filename, "wb");
    }
  }

#ifdef HAVE_LIBZ
  if (compresslevel != 0)
  {
    if (!ztarfile)
    {
                                 
      pg_log_error("could not create compressed file \"%s\": %s", filename, get_gz_error(ztarfile));
      exit(1);
    }
  }
  else
#endif
  {
                                                                       
    if (!tarfile)
    {
      pg_log_error("could not create file \"%s\": %m", filename);
      exit(1);
    }
  }

     
                              
     
  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_COPY_OUT)
  {
    pg_log_error("could not get COPY data stream: %s", PQerrorMessage(conn));
    exit(1);
  }

  while (1)
  {
    int r;

    if (copybuf != NULL)
    {
      PQfreemem(copybuf);
      copybuf = NULL;
    }

    r = PQgetCopyData(conn, &copybuf, 0);
    if (r == -1)
    {
         
                                                                      
                                                                         
                                
         
                                                                       
                                                 
         
      char zerobuf[1024];

      MemSet(zerobuf, 0, sizeof(zerobuf));

      if (basetablespace && writerecoveryconf)
      {
        char header[512];

           
                                                                      
                                                                       
                                                                       
                                                                 
                      
           
        if (!found_postgresql_auto_conf || !is_recovery_guc_supported)
        {
          int padding;

          tarCreateHeader(header, is_recovery_guc_supported ? "postgresql.auto.conf" : "recovery.conf", NULL, recoveryconfcontents->len, pg_file_create_mode, 04000, 02000, time(NULL));

          padding = ((recoveryconfcontents->len + 511) & ~511) - recoveryconfcontents->len;

          WRITE_TAR_DATA(header, sizeof(header));
          WRITE_TAR_DATA(recoveryconfcontents->data, recoveryconfcontents->len);
          if (padding)
          {
            WRITE_TAR_DATA(zerobuf, padding);
          }
        }

           
                                                                       
                 
           
        if (is_recovery_guc_supported)
        {
          tarCreateHeader(header, "standby.signal", NULL, 0,                       
              pg_file_create_mode, 04000, 02000, time(NULL));

          WRITE_TAR_DATA(header, sizeof(header));

             
                                                                     
                                                                    
                                         
             
        }
      }

                                                   
      WRITE_TAR_DATA(zerobuf, sizeof(zerobuf));

#ifdef HAVE_LIBZ
      if (ztarfile != NULL)
      {
        errno = 0;                                       
        if (gzclose(ztarfile) != 0)
        {
          pg_log_error("could not close compressed file \"%s\": %m", filename);
          exit(1);
        }
      }
      else
#endif
      {
        if (strcmp(basedir, "-") != 0)
        {
          if (fclose(tarfile) != 0)
          {
            pg_log_error("could not close file \"%s\": %m", filename);
            exit(1);
          }
        }
      }

      break;
    }
    else if (r == -2)
    {
      pg_log_error("could not read COPY data: %s", PQerrorMessage(conn));
      exit(1);
    }

    if (!writerecoveryconf || !basetablespace)
    {
         
                                                                       
                                                                         
                             
         
      WRITE_TAR_DATA(copybuf, r);
    }
    else
    {
         
                                                                    
                                                                      
                                  
         
                                                                        
                                                                      
                                                                   
                                                                        
                                                      
         
      int rr = r;
      int pos = 0;

      while (rr > 0)
      {
        if (in_tarhdr)
        {
             
                                                                   
                                                 
             
          if (tarhdrsz < 512)
          {
               
                                                                 
                                                              
                                                                 
               
            int hdrleft;
            int bytes2copy;

            hdrleft = 512 - tarhdrsz;
            bytes2copy = (rr > hdrleft ? hdrleft : rr);

            memcpy(&tarhdr[tarhdrsz], copybuf + pos, bytes2copy);

            rr -= bytes2copy;
            pos += bytes2copy;
            tarhdrsz += bytes2copy;
          }
          else
          {
               
                                                                
                                                             
                                                                
                                                              
                                                                   
                                                         
               
            if (is_recovery_guc_supported)
            {
              skip_file = (strcmp(&tarhdr[0], "standby.signal") == 0);
              is_postgresql_auto_conf = (strcmp(&tarhdr[0], "postgresql.auto.conf") == 0);
            }
            else
            {
              skip_file = (strcmp(&tarhdr[0], "recovery.conf") == 0);
            }

            filesz = read_tar_number(&tarhdr[124], 12);
            file_padding_len = ((filesz + 511) & ~511) - filesz;

            if (is_recovery_guc_supported && is_postgresql_auto_conf && writerecoveryconf)
            {
                                      
              char header[512];

              tarCreateHeader(header, "postgresql.auto.conf", NULL, filesz + recoveryconfcontents->len, pg_file_create_mode, 04000, 02000, time(NULL));

              WRITE_TAR_DATA(header, sizeof(header));
            }
            else
            {
                                            
              filesz += file_padding_len;

              if (!skip_file)
              {
                   
                                                             
                                          
                   
                WRITE_TAR_DATA(tarhdr, 512);
              }
            }

                                                       
            in_tarhdr = false;
          }
        }
        else
        {
             
                                                 
             
          if (filesz > 0)
          {
               
                                                                
               
            int bytes2write;

            bytes2write = (filesz > rr ? rr : filesz);

            if (!skip_file)
            {
              WRITE_TAR_DATA(copybuf + pos, bytes2write);
            }

            rr -= bytes2write;
            pos += bytes2write;
            filesz -= bytes2write;
          }
          else if (is_recovery_guc_supported && is_postgresql_auto_conf && writerecoveryconf)
          {
                                                                
            int padding;
            int tailsize;

            tailsize = (512 - file_padding_len) + recoveryconfcontents->len;
            padding = ((tailsize + 511) & ~511) - tailsize;

            WRITE_TAR_DATA(recoveryconfcontents->data, recoveryconfcontents->len);

            if (padding)
            {
              char zerobuf[512];

              MemSet(zerobuf, 0, sizeof(zerobuf));
              WRITE_TAR_DATA(zerobuf, padding);
            }

                                            
            is_postgresql_auto_conf = false;
            skip_file = true;
            filesz += file_padding_len;

            found_postgresql_auto_conf = true;
          }
          else
          {
               
                                                                   
                                                                  
               
            in_tarhdr = true;
            skip_file = false;
            is_postgresql_auto_conf = false;
            tarhdrsz = 0;
            filesz = 0;
          }
        }
      }
    }
    totaldone += r;
    progress_report(rownum, filename, false, false);
  }                
  progress_report(rownum, filename, true, false);

  if (copybuf != NULL)
  {
    PQfreemem(copybuf);
  }

     
                                                                          
              
     
}

   
                                                                               
                         
   
static const char *
get_tablespace_mapping(const char *dir)
{
  TablespaceListCell *cell;
  char canon_dir[MAXPGPATH];

                                                    
  strlcpy(canon_dir, dir, sizeof(canon_dir));
  canonicalize_path(canon_dir);

  for (cell = tablespace_dirs.head; cell; cell = cell->next)
  {
    if (strcmp(canon_dir, cell->old_dir) == 0)
    {
      return cell->new_dir;
    }
  }

  return dir;
}

   
                                                                             
                                                                    
                                                            
   
                                                                          
                                                                            
                                        
   
static void
ReceiveAndUnpackTarFile(PGconn *conn, PGresult *res, int rownum)
{
  char current_path[MAXPGPATH];
  char filename[MAXPGPATH];
  const char *mapped_tblspc_path;
  pgoff_t current_len_left = 0;
  int current_padding = 0;
  bool basetablespace;
  char *copybuf = NULL;
  FILE *file = NULL;

  basetablespace = PQgetisnull(res, rownum, 0);
  if (basetablespace)
  {
    strlcpy(current_path, basedir, sizeof(current_path));
  }
  else
  {
    strlcpy(current_path, get_tablespace_mapping(PQgetvalue(res, rownum, 1)), sizeof(current_path));
  }

     
                       
     
  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_COPY_OUT)
  {
    pg_log_error("could not get COPY data stream: %s", PQerrorMessage(conn));
    exit(1);
  }

  while (1)
  {
    int r;

    if (copybuf != NULL)
    {
      PQfreemem(copybuf);
      copybuf = NULL;
    }

    r = PQgetCopyData(conn, &copybuf, 0);

    if (r == -1)
    {
         
                      
         
      if (file)
      {
        fclose(file);
      }

      break;
    }
    else if (r == -2)
    {
      pg_log_error("could not read COPY data: %s", PQerrorMessage(conn));
      exit(1);
    }

    if (file == NULL)
    {
      int filemode;

         
                                                                    
         
      if (r != 512)
      {
        pg_log_error("invalid tar block header size: %d", r);
        exit(1);
      }
      totaldone += 512;

      current_len_left = read_tar_number(&copybuf[124], 12);

                                       
      filemode = read_tar_number(&copybuf[100], 8);

         
                                              
         
      current_padding = ((current_len_left + 511) & ~511) - current_len_left;

         
                                                          
         
      snprintf(filename, sizeof(filename), "%s/%s", current_path, copybuf);
      if (filename[strlen(filename) - 1] == '/')
      {
           
                                                                   
           
        if (copybuf[156] == '5')
        {
             
                       
             
          filename[strlen(filename) - 1] = '\0';                            
          if (mkdir(filename, pg_dir_create_mode) != 0)
          {
               
                                                                  
                                                           
                                                              
                                                               
                                                              
                                                                   
                                                
               
            if (!((pg_str_endswith(filename, "/pg_wal") || pg_str_endswith(filename, "/pg_xlog") || pg_str_endswith(filename, "/archive_status")) && errno == EEXIST))
            {
              pg_log_error("could not create directory \"%s\": %m", filename);
              exit(1);
            }
          }
#ifndef WIN32
          if (chmod(filename, (mode_t)filemode))
          {
            pg_log_error("could not set permissions on directory \"%s\": %m", filename);
          }
#endif
        }
        else if (copybuf[156] == '2')
        {
             
                           
             
                                                                    
                                                                    
                                                                   
                                                                 
                                                                    
                                                                    
                                                                   
                                
             
          filename[strlen(filename) - 1] = '\0';                            

          mapped_tblspc_path = get_tablespace_mapping(&copybuf[157]);
          if (symlink(mapped_tblspc_path, filename) != 0)
          {
            pg_log_error("could not create symbolic link from \"%s\" to \"%s\": %m", filename, mapped_tblspc_path);
            exit(1);
          }
        }
        else
        {
          pg_log_error("unrecognized link indicator \"%c\"", copybuf[156]);
          exit(1);
        }
        continue;                                
      }

         
                      
         
      file = fopen(filename, "wb");
      if (!file)
      {
        pg_log_error("could not create file \"%s\": %m", filename);
        exit(1);
      }

#ifndef WIN32
      if (chmod(filename, (mode_t)filemode))
      {
        pg_log_error("could not set permissions on file \"%s\": %m", filename);
      }
#endif

      if (current_len_left == 0)
      {
           
                                                                  
           
        fclose(file);
        file = NULL;
        continue;
      }
    }               
    else
    {
         
                                            
         
      if (current_len_left == 0 && r == current_padding)
      {
           
                                                                   
                                                                
           
        fclose(file);
        file = NULL;
        totaldone += r;
        continue;
      }

      errno = 0;
      if (fwrite(copybuf, r, 1, file) != 1)
      {
                                                                        
        if (errno == 0)
        {
          errno = ENOSPC;
        }
        pg_log_error("could not write to file \"%s\": %m", filename);
        exit(1);
      }
      totaldone += r;
      progress_report(rownum, filename, false, false);

      current_len_left -= r;
      if (current_len_left == 0 && current_padding == 0)
      {
           
                                                                  
                                                                
                   
           
        fclose(file);
        file = NULL;
        continue;
      }
    }                                       
  }                                
  progress_report(rownum, filename, true, false);

  if (file != NULL)
  {
    pg_log_error("COPY stream ended before last file was finished");
    exit(1);
  }

  if (copybuf != NULL)
  {
    PQfreemem(copybuf);
  }

  if (basetablespace && writerecoveryconf)
  {
    WriteRecoveryConf();
  }

     
                                                                           
          
     
}

   
                                                                         
                         
   
static char *
escape_quotes(const char *src)
{
  char *result = escape_single_quotes_ascii(src);

  if (!result)
  {
    pg_log_error("out of memory");
    exit(1);
  }
  return result;
}

   
                                                             
   
static void
GenerateRecoveryConf(PGconn *conn)
{
  PQconninfoOption *connOptions;
  PQconninfoOption *option;
  PQExpBufferData conninfo_buf;
  char *escaped;

  recoveryconfcontents = createPQExpBuffer();
  if (!recoveryconfcontents)
  {
    pg_log_error("out of memory");
    exit(1);
  }

     
                                                                            
                                                            
     
  if (PQserverVersion(conn) < MINIMUM_VERSION_FOR_RECOVERY_GUC)
  {
    appendPQExpBufferStr(recoveryconfcontents, "standby_mode = 'on'\n");
  }

  connOptions = PQconninfo(conn);
  if (connOptions == NULL)
  {
    pg_log_error("out of memory");
    exit(1);
  }

  initPQExpBuffer(&conninfo_buf);
  for (option = connOptions; option && option->keyword; option++)
  {
                                                                   
    if (strcmp(option->keyword, "replication") == 0 || strcmp(option->keyword, "dbname") == 0 || strcmp(option->keyword, "fallback_application_name") == 0 || (option->val == NULL) || (option->val != NULL && option->val[0] == '\0'))
    {
      continue;
    }

                                              
    if (conninfo_buf.len != 0)
    {
      appendPQExpBufferChar(&conninfo_buf, ' ');
    }

       
                                                                        
                            
       
    appendPQExpBuffer(&conninfo_buf, "%s=", option->keyword);
    appendConnStrVal(&conninfo_buf, option->val);
  }

     
                                                                             
                                                                            
                    
     
  escaped = escape_quotes(conninfo_buf.data);
  appendPQExpBuffer(recoveryconfcontents, "primary_conninfo = '%s'\n", escaped);
  free(escaped);

  if (replication_slot)
  {
                                                                      
    appendPQExpBuffer(recoveryconfcontents, "primary_slot_name = '%s'\n", replication_slot);
  }

  if (PQExpBufferBroken(recoveryconfcontents) || PQExpBufferDataBroken(conninfo_buf))
  {
    pg_log_error("out of memory");
    exit(1);
  }

  termPQExpBuffer(&conninfo_buf);

  PQconninfoFree(connOptions);
}

   
                                                                         
                                                                       
                                                                     
                                                                      
                                              
   
static void
WriteRecoveryConf(void)
{
  char filename[MAXPGPATH];
  FILE *cf;
  bool is_recovery_guc_supported = true;

  if (PQserverVersion(conn) < MINIMUM_VERSION_FOR_RECOVERY_GUC)
  {
    is_recovery_guc_supported = false;
  }

  snprintf(filename, MAXPGPATH, "%s/%s", basedir, is_recovery_guc_supported ? "postgresql.auto.conf" : "recovery.conf");

  cf = fopen(filename, is_recovery_guc_supported ? "a" : "w");
  if (cf == NULL)
  {
    pg_log_error("could not open file \"%s\": %m", filename);
    exit(1);
  }

  if (fwrite(recoveryconfcontents->data, recoveryconfcontents->len, 1, cf) != 1)
  {
    pg_log_error("could not write to file \"%s\": %m", filename);
    exit(1);
  }

  fclose(cf);

  if (is_recovery_guc_supported)
  {
    snprintf(filename, MAXPGPATH, "%s/%s", basedir, "standby.signal");
    cf = fopen(filename, "w");
    if (cf == NULL)
    {
      pg_log_error("could not create file \"%s\": %m", filename);
      exit(1);
    }

    fclose(cf);
  }
}

static void
BaseBackup(void)
{
  PGresult *res;
  char *sysidentifier;
  TimeLineID latesttli;
  TimeLineID starttli;
  char *basebkp;
  char escaped_label[MAXPGPATH];
  char *maxrate_clause = NULL;
  int i;
  char xlogstart[64];
  char xlogend[64];
  int minServerMajor, maxServerMajor;
  int serverVersion, serverMajor;

  Assert(conn != NULL);

     
                                                                            
                                             
     
  minServerMajor = 901;
  maxServerMajor = PG_VERSION_NUM / 100;
  serverVersion = PQserverVersion(conn);
  serverMajor = serverVersion / 100;
  if (serverMajor < minServerMajor || serverMajor > maxServerMajor)
  {
    const char *serverver = PQparameterStatus(conn, "server_version");

    pg_log_error("incompatible server version %s", serverver ? serverver : "'unknown'");
    exit(1);
  }

     
                                                                       
                      
     
  if (includewal == STREAM_WAL && !CheckServerVersionForStreaming(conn))
  {
       
                                                                          
                                           
       
    pg_log_info("HINT: use -X none or -X fetch to disable log streaming");
    exit(1);
  }

     
                                                       
     
  if (writerecoveryconf)
  {
    GenerateRecoveryConf(conn);
  }

     
                                                    
     
  if (!RunIdentifySystem(conn, &sysidentifier, &latesttli, NULL, NULL))
  {
    exit(1);
  }

     
                             
     
  PQescapeStringConn(conn, escaped_label, label, sizeof(escaped_label), &i);

  if (maxrate > 0)
  {
    maxrate_clause = psprintf("MAX_RATE %u", maxrate);
  }

  if (verbose)
  {
    pg_log_info("initiating base backup, waiting for checkpoint to complete");
  }

  if (showprogress && !verbose)
  {
    fprintf(stderr, "waiting for checkpoint");
    if (isatty(fileno(stderr)))
    {
      fprintf(stderr, "\r");
    }
    else
    {
      fprintf(stderr, "\n");
    }
  }

  basebkp = psprintf("BASE_BACKUP LABEL '%s' %s %s %s %s %s %s %s", escaped_label, showprogress ? "PROGRESS" : "", includewal == FETCH_WAL ? "WAL" : "", fastcheckpoint ? "FAST" : "", includewal == NO_WAL ? "" : "NOWAIT", maxrate_clause ? maxrate_clause : "", format == 't' ? "TABLESPACE_MAP" : "", verify_checksums ? "" : "NOVERIFY_CHECKSUMS");

  if (PQsendQuery(conn, basebkp) == 0)
  {
    pg_log_error("could not send replication command \"%s\": %s", "BASE_BACKUP", PQerrorMessage(conn));
    exit(1);
  }

     
                                   
     
  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("could not initiate base backup: %s", PQerrorMessage(conn));
    exit(1);
  }
  if (PQntuples(res) != 1)
  {
    pg_log_error("server returned unexpected response to BASE_BACKUP command; got %d rows and %d fields, expected %d rows and %d fields", PQntuples(res), PQnfields(res), 1, 2);
    exit(1);
  }

  strlcpy(xlogstart, PQgetvalue(res, 0, 0), sizeof(xlogstart));

  if (verbose)
  {
    pg_log_info("checkpoint completed");
  }

     
                                                                            
                                                             
                      
     
  if (PQnfields(res) >= 2)
  {
    starttli = atoi(PQgetvalue(res, 0, 1));
  }
  else
  {
    starttli = latesttli;
  }
  PQclear(res);
  MemSet(xlogend, 0, sizeof(xlogend));

  if (verbose && includewal != NO_WAL)
  {
    pg_log_info("write-ahead log start point: %s on timeline %u", xlogstart, starttli);
  }

     
                    
     
  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("could not get backup header: %s", PQerrorMessage(conn));
    exit(1);
  }
  if (PQntuples(res) < 1)
  {
    pg_log_error("no data returned from server");
    exit(1);
  }

     
                                                   
     
  totalsize = totaldone = 0;
  tablespacecount = PQntuples(res);
  for (i = 0; i < PQntuples(res); i++)
  {
    totalsize += atol(PQgetvalue(res, i, 2));

       
                                                                      
                                                                           
                              
       
    if (format == 'p' && !PQgetisnull(res, i, 1))
    {
      char *path = unconstify(char *, get_tablespace_mapping(PQgetvalue(res, i, 1)));

      verify_dir_is_empty_or_create(path, &made_tablespace_dirs, &found_tablespace_dirs);
    }
  }

     
                                                         
     
  if (format == 't' && strcmp(basedir, "-") == 0 && PQntuples(res) > 1)
  {
    pg_log_error("can only write single tablespace to stdout, database has %d", PQntuples(res));
    exit(1);
  }

     
                                                                         
                                       
     
  if (includewal == STREAM_WAL)
  {
    if (verbose)
    {
      pg_log_info("starting background WAL receiver");
    }
    StartLogStreamer(xlogstart, starttli, sysidentifier);
  }

     
                            
     
  for (i = 0; i < PQntuples(res); i++)
  {
    if (format == 't')
    {
      ReceiveTarFile(conn, res, i);
    }
    else
    {
      ReceiveAndUnpackTarFile(conn, res, i);
    }
  }                                

  if (showprogress)
  {
    progress_report(PQntuples(res), NULL, true, true);
  }

  PQclear(res);

     
                           
     
  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("could not get write-ahead log end position from server: %s", PQerrorMessage(conn));
    exit(1);
  }
  if (PQntuples(res) != 1)
  {
    pg_log_error("no write-ahead log end position returned from server");
    exit(1);
  }
  strlcpy(xlogend, PQgetvalue(res, 0, 0), sizeof(xlogend));
  if (verbose && includewal != NO_WAL)
  {
    pg_log_info("write-ahead log end point: %s", xlogend);
  }
  PQclear(res);

  res = PQgetResult(conn);
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);

    if (sqlstate && strcmp(sqlstate, ERRCODE_DATA_CORRUPTED) == 0)
    {
      pg_log_error("checksum error occurred");
      checksum_failure = true;
    }
    else
    {
      pg_log_error("final receive failed: %s", PQerrorMessage(conn));
    }
    exit(1);
  }

  if (bgchild > 0)
  {
#ifndef WIN32
    int status;
    pid_t r;
#else
    DWORD status;

       
                                                                      
                                             
       
    intptr_t bgchild_handle = bgchild;
    uint32 hi, lo;
#endif

    if (verbose)
    {
      pg_log_info("waiting for background process to finish streaming ...");
    }

#ifndef WIN32
    if (write(bgpipe[1], xlogend, strlen(xlogend)) != strlen(xlogend))
    {
      pg_log_info("could not send command to background pipe: %m");
      exit(1);
    }

                                                      
    r = waitpid(bgchild, &status, 0);
    if (r == (pid_t)-1)
    {
      pg_log_error("could not wait for child process: %m");
      exit(1);
    }
    if (r != bgchild)
    {
      pg_log_error("child %d died, expected %d", (int)r, (int)bgchild);
      exit(1);
    }
    if (status != 0)
    {
      pg_log_error("%s", wait_result_to_str(status));
      exit(1);
    }
                                       
#else            

       
                                                                           
                                                                       
                   
       
    if (sscanf(xlogend, "%X/%X", &hi, &lo) != 2)
    {
      pg_log_error("could not parse write-ahead log location \"%s\"", xlogend);
      exit(1);
    }
    xlogendptr = ((uint64)hi) << 32 | lo;
    InterlockedIncrement(&has_xlogendptr);

                                           
    if (WaitForSingleObjectEx((HANDLE)bgchild_handle, INFINITE, FALSE) != WAIT_OBJECT_0)
    {
      _dosmaperr(GetLastError());
      pg_log_error("could not wait for child thread: %m");
      exit(1);
    }
    if (GetExitCodeThread((HANDLE)bgchild_handle, &status) == 0)
    {
      _dosmaperr(GetLastError());
      pg_log_error("could not get child thread exit status: %m");
      exit(1);
    }
    if (status != 0)
    {
      pg_log_error("child thread exited with error %u", (unsigned int)status);
      exit(1);
    }
                                      
#endif
  }

                                            
  destroyPQExpBuffer(recoveryconfcontents);

     
                                                                        
     
  PQclear(res);
  PQfinish(conn);
  conn = NULL;

     
                                                                           
                                                                             
                                                                         
                                                                        
                                      
     
  if (do_sync)
  {
    if (verbose)
    {
      pg_log_info("syncing data to disk ...");
    }
    if (format == 't')
    {
      if (strcmp(basedir, "-") != 0)
      {
        (void)fsync_dir_recurse(basedir);
      }
    }
    else
    {
      (void)fsync_pgdata(basedir, serverVersion);
    }
  }

  if (verbose)
  {
    pg_log_info("base backup completed");
  }
}

int
main(int argc, char **argv)
{
  static struct option long_options[] = {{"help", no_argument, NULL, '?'}, {"version", no_argument, NULL, 'V'}, {"pgdata", required_argument, NULL, 'D'}, {"format", required_argument, NULL, 'F'}, {"checkpoint", required_argument, NULL, 'c'}, {"create-slot", no_argument, NULL, 'C'}, {"max-rate", required_argument, NULL, 'r'}, {"write-recovery-conf", no_argument, NULL, 'R'}, {"slot", required_argument, NULL, 'S'}, {"tablespace-mapping", required_argument, NULL, 'T'}, {"wal-method", required_argument, NULL, 'X'}, {"gzip", no_argument, NULL, 'z'}, {"compress", required_argument, NULL, 'Z'}, {"label", required_argument, NULL, 'l'}, {"no-clean", no_argument, NULL, 'n'}, {"no-sync", no_argument, NULL, 'N'}, {"dbname", required_argument, NULL, 'd'}, {"host", required_argument, NULL, 'h'}, {"port", required_argument, NULL, 'p'}, {"username", required_argument, NULL, 'U'}, {"no-password", no_argument, NULL, 'w'}, {"password", no_argument, NULL, 'W'},
      {"status-interval", required_argument, NULL, 's'}, {"verbose", no_argument, NULL, 'v'}, {"progress", no_argument, NULL, 'P'}, {"waldir", required_argument, NULL, 1}, {"no-slot", no_argument, NULL, 2}, {"no-verify-checksums", no_argument, NULL, 3}, {NULL, 0, NULL, 0}};
  int c;

  int option_index;

  pg_logging_init(argv[0]);
  progname = get_progname(argv[0]);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_basebackup"));

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      usage();
      exit(0);
    }
    else if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)
    {
      puts("pg_basebackup (PostgreSQL) " PG_VERSION);
      exit(0);
    }
  }

  atexit(cleanup_directories_atexit);

  while ((c = getopt_long(argc, argv, "CD:F:r:RS:T:X:l:nNzZ:d:c:h:p:U:s:wWkvP", long_options, &option_index)) != -1)
  {
    switch (c)
    {
    case 'C':
      create_slot = true;
      break;
    case 'D':
      basedir = pg_strdup(optarg);
      break;
    case 'F':
      if (strcmp(optarg, "p") == 0 || strcmp(optarg, "plain") == 0)
      {
        format = 'p';
      }
      else if (strcmp(optarg, "t") == 0 || strcmp(optarg, "tar") == 0)
      {
        format = 't';
      }
      else
      {
        pg_log_error("invalid output format \"%s\", must be \"plain\" or \"tar\"", optarg);
        exit(1);
      }
      break;
    case 'r':
      maxrate = parse_max_rate(optarg);
      break;
    case 'R':
      writerecoveryconf = true;
      break;
    case 'S':

         
                                                                
               
         
      replication_slot = pg_strdup(optarg);
      temp_replication_slot = false;
      break;
    case 2:
      no_slot = true;
      break;
    case 'T':
      tablespace_list_append(optarg);
      break;
    case 'X':
      if (strcmp(optarg, "n") == 0 || strcmp(optarg, "none") == 0)
      {
        includewal = NO_WAL;
      }
      else if (strcmp(optarg, "f") == 0 || strcmp(optarg, "fetch") == 0)
      {
        includewal = FETCH_WAL;
      }
      else if (strcmp(optarg, "s") == 0 || strcmp(optarg, "stream") == 0)
      {
        includewal = STREAM_WAL;
      }
      else
      {
        pg_log_error("invalid wal-method option \"%s\", must be \"fetch\", \"stream\", or \"none\"", optarg);
        exit(1);
      }
      break;
    case 1:
      xlog_dir = pg_strdup(optarg);
      break;
    case 'l':
      label = pg_strdup(optarg);
      break;
    case 'n':
      noclean = true;
      break;
    case 'N':
      do_sync = false;
      break;
    case 'z':
#ifdef HAVE_LIBZ
      compresslevel = Z_DEFAULT_COMPRESSION;
#else
      compresslevel = 1;                             
#endif
      break;
    case 'Z':
      compresslevel = atoi(optarg);
      if (compresslevel < 0 || compresslevel > 9)
      {
        pg_log_error("invalid compression level \"%s\"", optarg);
        exit(1);
      }
      break;
    case 'c':
      if (pg_strcasecmp(optarg, "fast") == 0)
      {
        fastcheckpoint = true;
      }
      else if (pg_strcasecmp(optarg, "spread") == 0)
      {
        fastcheckpoint = false;
      }
      else
      {
        pg_log_error("invalid checkpoint argument \"%s\", must be \"fast\" or \"spread\"", optarg);
        exit(1);
      }
      break;
    case 'd':
      connection_string = pg_strdup(optarg);
      break;
    case 'h':
      dbhost = pg_strdup(optarg);
      break;
    case 'p':
      dbport = pg_strdup(optarg);
      break;
    case 'U':
      dbuser = pg_strdup(optarg);
      break;
    case 'w':
      dbgetpassword = -1;
      break;
    case 'W':
      dbgetpassword = 1;
      break;
    case 's':
      standby_message_timeout = atoi(optarg) * 1000;
      if (standby_message_timeout < 0)
      {
        pg_log_error("invalid status interval \"%s\"", optarg);
        exit(1);
      }
      break;
    case 'v':
      verbose++;
      break;
    case 'P':
      showprogress = true;
      break;
    case 3:
      verify_checksums = false;
      break;
    default:

         
                                                 
         
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
  }

     
                               
     
  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

     
                        
     
  if (basedir == NULL)
  {
    pg_log_error("no target directory specified");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

     
                                  
     
  if (format == 'p' && compresslevel != 0)
  {
    pg_log_error("only tar mode backups can be compressed");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (format == 't' && includewal == STREAM_WAL && strcmp(basedir, "-") == 0)
  {
    pg_log_error("cannot stream write-ahead logs in tar mode to stdout");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (replication_slot && includewal != STREAM_WAL)
  {
    pg_log_error("replication slots can only be used with WAL streaming");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (no_slot)
  {
    if (replication_slot)
    {
      pg_log_error("--no-slot cannot be used with slot name");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
    temp_replication_slot = false;
  }

  if (create_slot)
  {
    if (!replication_slot)
    {
      pg_log_error("%s needs a slot to be specified using --slot", "--create-slot");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }

    if (no_slot)
    {
      pg_log_error("--create-slot and --no-slot are incompatible options");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
  }

  if (xlog_dir)
  {
    if (format != 'p')
    {
      pg_log_error("WAL directory location can only be specified in plain mode");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }

                                                           
    canonicalize_path(xlog_dir);
    if (!is_absolute_path(xlog_dir))
    {
      pg_log_error("WAL directory location must be an absolute path");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
  }

#ifndef HAVE_LIBZ
  if (compresslevel != 0)
  {
    pg_log_error("this build does not support compression");
    exit(1);
  }
#endif

                                                
  conn = GetConnection();
  if (!conn)
  {
                                                          
    exit(1);
  }
  atexit(disconnect_atexit);

     
                                                                   
                                                                    
     
                                                                      
                                                                     
                                                                             
     
  umask(pg_mode_mask);

     
                                                                          
                                                                        
                                      
     
  if (format == 'p' || strcmp(basedir, "-") != 0)
  {
    verify_dir_is_empty_or_create(basedir, &made_new_pgdata, &found_existing_pgdata);
  }

                                                   
  if (!RetrieveWalSegSize(conn))
  {
    exit(1);
  }

                                          
  if (xlog_dir)
  {
    char *linkloc;

    verify_dir_is_empty_or_create(xlog_dir, &made_new_xlogdir, &found_existing_xlogdir);

       
                                                                          
                                              
       
    linkloc = psprintf("%s/%s", basedir, PQserverVersion(conn) < MINIMUM_VERSION_FOR_PG_WAL ? "pg_xlog" : "pg_wal");

#ifdef HAVE_SYMLINK
    if (symlink(xlog_dir, linkloc) != 0)
    {
      pg_log_error("could not create symbolic link \"%s\": %m", linkloc);
      exit(1);
    }
#else
    pg_log_error("symlinks are not supported on this platform");
    exit(1);
#endif
    free(linkloc);
  }

  BaseBackup();

  success = true;
  return 0;
}
