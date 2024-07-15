   
                       
   
                                                                        
                 
   
                                                 
   
#include "postgres_fe.h"

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

#include "pg_getopt.h"

#include "common/logging.h"

#include "access/xlog_internal.h"

const char *progname;

                          
bool dryrun = false;                                                     
char *additional_ext = NULL;                                         

char *archiveLocation;                                                      
char *restartWALFileName;                                                                   
char exclusiveCleanupFileName[MAXFNAMELEN];                            
                                                                      

                                                                         
   
                           
   
                                                                         
   
                                                                 
                                                                
                                                                 
                                                       
                                                                          
                                                                 
                                                
   

   
                                                                           
   
                                                               
   
static void
Initialize(void)
{
     
                                                                           
                                 
     
  struct stat stat_buf;

  if (stat(archiveLocation, &stat_buf) != 0 || !S_ISDIR(stat_buf.st_mode))
  {
    pg_log_error("archive location \"%s\" does not exist", archiveLocation);
    exit(2);
  }
}

static void
TrimExtension(char *filename, char *extension)
{
  int flen;
  int elen;

  if (extension == NULL)
  {
    return;
  }

  elen = strlen(extension);
  flen = strlen(filename);

  if (flen > elen && strcmp(filename + flen - elen, extension) == 0)
  {
    filename[flen - elen] = '\0';
  }
}

static void
CleanupPriorWALFiles(void)
{
  int rc;
  DIR *xldir;
  struct dirent *xlde;
  char walfile[MAXPGPATH];

  if ((xldir = opendir(archiveLocation)) != NULL)
  {
    while (errno = 0, (xlde = readdir(xldir)) != NULL)
    {
         
                                                                      
                                                                         
                                                           
         
      strlcpy(walfile, xlde->d_name, MAXPGPATH);
      TrimExtension(walfile, additional_ext);

         
                                                                        
                                                                        
                                                                       
                                                                     
                                                                         
                           
         
                                                                      
                                                                         
                                                                       
                                                                 
         
      if ((IsXLogFileName(walfile) || IsPartialXLogFileName(walfile)) && strcmp(walfile + 8, exclusiveCleanupFileName + 8) < 0)
      {
        char WALFilePath[MAXPGPATH * 2];                  
                                                                

           
                                                               
                                                                     
                         
           
        snprintf(WALFilePath, sizeof(WALFilePath), "%s/%s", archiveLocation, xlde->d_name);

        if (dryrun)
        {
             
                                                                     
                                                                  
                                                               
             
          printf("%s\n", WALFilePath);
          pg_log_debug("file \"%s\" would be removed", WALFilePath);
          continue;
        }

        pg_log_debug("removing file \"%s\"", WALFilePath);

        rc = unlink(WALFilePath);
        if (rc != 0)
        {
          pg_log_error("could not remove file \"%s\": %m", WALFilePath);
          break;
        }
      }
    }

    if (errno)
    {
      pg_log_error("could not read archive location \"%s\": %m", archiveLocation);
    }
    if (closedir(xldir))
    {
      pg_log_error("could not close archive location \"%s\": %m", archiveLocation);
    }
  }
  else
  {
    pg_log_error("could not open archive location \"%s\": %m", archiveLocation);
  }
}

   
                              
   
                                                                       
                                        
   
static void
SetWALFileNameForCleanup(void)
{
  bool fnameOK = false;

  TrimExtension(restartWALFileName, additional_ext);

     
                                                                            
                                                                            
                                                                            
                                          
                                                        
                               
     
  if (IsXLogFileName(restartWALFileName))
  {
    strcpy(exclusiveCleanupFileName, restartWALFileName);
    fnameOK = true;
  }
  else if (IsPartialXLogFileName(restartWALFileName))
  {
    int args;
    uint32 tli = 1, log = 0, seg = 0;

    args = sscanf(restartWALFileName, "%08X%08X%08X.partial", &tli, &log, &seg);
    if (args == 3)
    {
      fnameOK = true;

         
                                                                      
                      
         
      XLogFileNameById(exclusiveCleanupFileName, tli, log, seg);
    }
  }
  else if (IsBackupHistoryFileName(restartWALFileName))
  {
    int args;
    uint32 tli = 1, log = 0, seg = 0, offset = 0;

    args = sscanf(restartWALFileName, "%08X%08X%08X.%08X.backup", &tli, &log, &seg, &offset);
    if (args == 4)
    {
      fnameOK = true;

         
                                                                      
                      
         
      XLogFileNameById(exclusiveCleanupFileName, tli, log, seg);
    }
  }

  if (!fnameOK)
  {
    pg_log_error("invalid file name argument");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(2);
  }
}

                                                                         
                                  
                                                                         
   

static void
usage(void)
{
  printf(_("%s removes older WAL files from PostgreSQL archives.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]... ARCHIVELOCATION OLDESTKEPTWALFILE\n"), progname);
  printf(_("\nOptions:\n"));
  printf(_("  -d             generate debug output (verbose mode)\n"));
  printf(_("  -n             dry run, show the names of the files that would be removed\n"));
  printf(_("  -V, --version  output version information, then exit\n"));
  printf(_("  -x EXT         clean up files if they have this extension\n"));
  printf(_("  -?, --help     show this help, then exit\n"));
  printf(_("\n"
           "For use as archive_cleanup_command in postgresql.conf:\n"
           "  archive_cleanup_command = 'pg_archivecleanup [OPTION]... ARCHIVELOCATION %%r'\n"
           "e.g.\n"
           "  archive_cleanup_command = 'pg_archivecleanup /mnt/server/archiverdir %%r'\n"));
  printf(_("\n"
           "Or for use as a standalone archive cleaner:\n"
           "e.g.\n"
           "  pg_archivecleanup /mnt/server/archiverdir 000000010000000000000010.00000020.backup\n"));
  printf(_("\nReport bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

                                                              
int
main(int argc, char **argv)
{
  int c;

  pg_logging_init(argv[0]);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_archivecleanup"));
  progname = get_progname(argv[0]);

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      usage();
      exit(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      puts("pg_archivecleanup (PostgreSQL) " PG_VERSION);
      exit(0);
    }
  }

  while ((c = getopt(argc, argv, "x:dn")) != -1)
  {
    switch (c)
    {
    case 'd':                 
      pg_logging_set_level(PG_LOG_DEBUG);
      break;
    case 'n':                   
      dryrun = true;
      break;
    case 'x':
      additional_ext = pg_strdup(optarg);                        
                                                                   
      break;
    default:
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(2);
      break;
    }
  }

     
                                                                    
                                                                            
                                                                         
                                                                        
           
     
  if (optind < argc)
  {
    archiveLocation = argv[optind];
    optind++;
  }
  else
  {
    pg_log_error("must specify archive location");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(2);
  }

  if (optind < argc)
  {
    restartWALFileName = argv[optind];
    optind++;
  }
  else
  {
    pg_log_error("must specify oldest kept WAL file");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(2);
  }

  if (optind < argc)
  {
    pg_log_error("too many command-line arguments");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(2);
  }

     
                                                                
     
  Initialize();

     
                                                                  
     
  SetWALFileNameForCleanup();

  pg_log_debug("keeping WAL file \"%s/%s\" and later", archiveLocation, exclusiveCleanupFileName);

     
                                         
     
  CleanupPriorWALFiles();

  exit(0);
}
