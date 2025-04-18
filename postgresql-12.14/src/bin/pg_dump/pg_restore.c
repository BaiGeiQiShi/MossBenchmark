                                                                            
   
                
                                                                     
                                                               
              
   
                                               
                                     
                                          
                         
                             
             
              
                 
                
                         
   
                                                             
   
                                            
   
                                      
                                                       
                             
        
   
                                     
                                                               
                                   
   
                                                              
                        
   
   
                  
                                 
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "getopt_long.h"

#include "dumputils.h"
#include "parallel.h"
#include "pg_backup_utils.h"

static void
usage(const char *progname);

typedef struct option optType;

int
main(int argc, char **argv)
{
  RestoreOptions *opts;
  int c;
  int exit_code;
  int numWorkers = 1;
  Archive *AH;
  char *inputFileSpec;
  static int disable_triggers = 0;
  static int enable_row_security = 0;
  static int if_exists = 0;
  static int no_data_for_failed_tables = 0;
  static int outputNoTablespaces = 0;
  static int use_setsessauth = 0;
  static int no_comments = 0;
  static int no_publications = 0;
  static int no_security_labels = 0;
  static int no_subscriptions = 0;
  static int strict_names = 0;

  struct option cmdopts[] = {{"clean", 0, NULL, 'c'}, {"create", 0, NULL, 'C'}, {"data-only", 0, NULL, 'a'}, {"dbname", 1, NULL, 'd'}, {"exit-on-error", 0, NULL, 'e'}, {"exclude-schema", 1, NULL, 'N'}, {"file", 1, NULL, 'f'}, {"format", 1, NULL, 'F'}, {"function", 1, NULL, 'P'}, {"host", 1, NULL, 'h'}, {"index", 1, NULL, 'I'}, {"jobs", 1, NULL, 'j'}, {"list", 0, NULL, 'l'}, {"no-privileges", 0, NULL, 'x'}, {"no-acl", 0, NULL, 'x'}, {"no-owner", 0, NULL, 'O'}, {"no-reconnect", 0, NULL, 'R'}, {"port", 1, NULL, 'p'}, {"no-password", 0, NULL, 'w'}, {"password", 0, NULL, 'W'}, {"schema", 1, NULL, 'n'}, {"schema-only", 0, NULL, 's'}, {"superuser", 1, NULL, 'S'}, {"table", 1, NULL, 't'}, {"trigger", 1, NULL, 'T'}, {"use-list", 1, NULL, 'L'}, {"username", 1, NULL, 'U'}, {"verbose", 0, NULL, 'v'}, {"single-transaction", 0, NULL, '1'},

         
                                                                            
         
      {"disable-triggers", no_argument, &disable_triggers, 1}, {"enable-row-security", no_argument, &enable_row_security, 1}, {"if-exists", no_argument, &if_exists, 1}, {"no-data-for-failed-tables", no_argument, &no_data_for_failed_tables, 1}, {"no-tablespaces", no_argument, &outputNoTablespaces, 1}, {"role", required_argument, NULL, 2}, {"section", required_argument, NULL, 3}, {"strict-names", no_argument, &strict_names, 1}, {"use-set-session-authorization", no_argument, &use_setsessauth, 1}, {"no-comments", no_argument, &no_comments, 1}, {"no-publications", no_argument, &no_publications, 1}, {"no-security-labels", no_argument, &no_security_labels, 1}, {"no-subscriptions", no_argument, &no_subscriptions, 1},

      {NULL, 0, NULL, 0}};

  pg_logging_init(argv[0]);
  pg_logging_set_level(PG_LOG_WARNING);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_dump"));

  init_parallel_dump_utils();

  opts = NewRestoreOptions();

  progname = get_progname(argv[0]);

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      usage(progname);
      exit_nicely(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      puts("pg_restore (PostgreSQL) " PG_VERSION);
      exit_nicely(0);
    }
  }

  while ((c = getopt_long(argc, argv, "acCd:ef:F:h:I:j:lL:n:N:Op:P:RsS:t:T:U:vwWx1", cmdopts, NULL)) != -1)
  {
    switch (c)
    {
    case 'a':                     
      opts->dataOnly = 1;
      break;
    case 'c':                                                
      opts->dropSchema = 1;
      break;
    case 'C':
      opts->createDB = 1;
      break;
    case 'd':
      opts->cparams.dbname = pg_strdup(optarg);
      break;
    case 'e':
      opts->exit_on_error = true;
      break;
    case 'f':                       
      opts->filename = pg_strdup(optarg);
      break;
    case 'F':
      if (strlen(optarg) != 0)
      {
        opts->formatName = pg_strdup(optarg);
      }
      break;
    case 'h':
      if (strlen(optarg) != 0)
      {
        opts->cparams.pghost = pg_strdup(optarg);
      }
      break;

    case 'j':                             
      numWorkers = atoi(optarg);
      break;

    case 'l':                           
      opts->tocSummary = 1;
      break;

    case 'L':                                  
      opts->tocFile = pg_strdup(optarg);
      break;

    case 'n':                                     
      simple_string_list_append(&opts->schemaNames, optarg);
      break;

    case 'N':                                       
      simple_string_list_append(&opts->schemaExcludeNames, optarg);
      break;

    case 'O':
      opts->noOwner = 1;
      break;

    case 'p':
      if (strlen(optarg) != 0)
      {
        opts->cparams.pgport = pg_strdup(optarg);
      }
      break;
    case 'R':
                                                             
      break;
    case 'P':               
      opts->selTypes = 1;
      opts->selFunction = 1;
      simple_string_list_append(&opts->functionNames, optarg);
      break;
    case 'I':            
      opts->selTypes = 1;
      opts->selIndex = 1;
      simple_string_list_append(&opts->indexNames, optarg);
      break;
    case 'T':              
      opts->selTypes = 1;
      opts->selTrigger = 1;
      simple_string_list_append(&opts->triggerNames, optarg);
      break;
    case 's':                       
      opts->schemaOnly = 1;
      break;
    case 'S':                         
      if (strlen(optarg) != 0)
      {
        opts->superuser = pg_strdup(optarg);
      }
      break;
    case 't':                                   
      opts->selTypes = 1;
      opts->selTable = 1;
      simple_string_list_append(&opts->tableNames, optarg);
      break;

    case 'U':
      opts->cparams.username = pg_strdup(optarg);
      break;

    case 'v':              
      opts->verbose = 1;
      pg_logging_set_level(PG_LOG_INFO);
      break;

    case 'w':
      opts->cparams.promptPassword = TRI_NO;
      break;

    case 'W':
      opts->cparams.promptPassword = TRI_YES;
      break;

    case 'x':                    
      opts->aclsSkip = 1;
      break;

    case '1':                                           
      opts->single_txn = true;
      opts->exit_on_error = true;
      break;

    case 0:

         
                                                                  
         
      break;

    case 2:               
      opts->use_role = pg_strdup(optarg);
      break;

    case 3:              
      set_dump_section(optarg, &(opts->dumpSections));
      break;

    default:
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit_nicely(1);
    }
  }

                                       
  if (optind < argc)
  {
    inputFileSpec = argv[optind++];
  }
  else
  {
    inputFileSpec = NULL;
  }

                                        
  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

                                                                           
  if (!opts->cparams.dbname && !opts->filename && !opts->tocSummary)
  {
    pg_log_error("one of -d/--dbname and -f/--file must be specified");
    exit_nicely(1);
  }

                                                                  
  if (opts->cparams.dbname)
  {
    if (opts->filename)
    {
      pg_log_error("options -d/--dbname and -f/--file cannot be used together");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit_nicely(1);
    }
    opts->useDB = 1;
  }

  if (opts->dataOnly && opts->schemaOnly)
  {
    pg_log_error("options -s/--schema-only and -a/--data-only cannot be used together");
    exit_nicely(1);
  }

  if (opts->dataOnly && opts->dropSchema)
  {
    pg_log_error("options -c/--clean and -a/--data-only cannot be used together");
    exit_nicely(1);
  }

     
                                                                             
                          
     
  if (opts->createDB && opts->single_txn)
  {
    pg_log_error("options -C/--create and -1/--single-transaction cannot be used together");
    exit_nicely(1);
  }

  if (numWorkers <= 0)
  {
    pg_log_error("invalid number of parallel jobs");
    exit(1);
  }

                                 
#ifdef WIN32
  if (numWorkers > MAXIMUM_WAIT_OBJECTS)
  {
    pg_log_error("maximum number of parallel jobs is %d", MAXIMUM_WAIT_OBJECTS);
    exit(1);
  }
#endif

                                                          
  if (opts->single_txn && numWorkers > 1)
  {
    pg_log_error("cannot specify both --single-transaction and multiple jobs");
    exit_nicely(1);
  }

  opts->disable_triggers = disable_triggers;
  opts->enable_row_security = enable_row_security;
  opts->noDataForFailedTables = no_data_for_failed_tables;
  opts->noTablespace = outputNoTablespaces;
  opts->use_setsessauth = use_setsessauth;
  opts->no_comments = no_comments;
  opts->no_publications = no_publications;
  opts->no_security_labels = no_security_labels;
  opts->no_subscriptions = no_subscriptions;

  if (if_exists && !opts->dropSchema)
  {
    pg_log_error("option --if-exists requires option -c/--clean");
    exit_nicely(1);
  }
  opts->if_exists = if_exists;
  opts->strict_names = strict_names;

  if (opts->formatName)
  {
    switch (opts->formatName[0])
    {
    case 'c':
    case 'C':
      opts->format = archCustom;
      break;

    case 'd':
    case 'D':
      opts->format = archDirectory;
      break;

    case 't':
    case 'T':
      opts->format = archTar;
      break;

    default:
      pg_log_error("unrecognized archive format \"%s\"; please specify \"c\", \"d\", or \"t\"", opts->formatName);
      exit_nicely(1);
    }
  }

  AH = OpenArchive(inputFileSpec, opts->format);

  SetArchiveOptions(AH, NULL, opts);

     
                                                                            
                                                                            
                                                                 
     
  on_exit_close_archive(AH);

                                             
  AH->verbose = opts->verbose;

     
                                                                             
     
  AH->exit_on_error = opts->exit_on_error;

  if (opts->tocFile)
  {
    SortTocFromFile(AH);
  }

  AH->numWorkers = numWorkers;

  if (opts->tocSummary)
  {
    PrintTOCSummary(AH);
  }
  else
  {
    ProcessArchiveRestoreOptions(AH);
    RestoreArchive(AH);
  }

                                               
  if (AH->n_errors)
  {
    pg_log_warning("errors ignored on restore: %d", AH->n_errors);
  }

                                        
  exit_code = AH->n_errors ? 1 : 0;

  CloseArchive(AH);

  return exit_code;
}

static void
usage(const char *progname)
{
  printf(_("%s restores a PostgreSQL database from an archive created by pg_dump.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]... [FILE]\n"), progname);

  printf(_("\nGeneral options:\n"));
  printf(_("  -d, --dbname=NAME        connect to database name\n"));
  printf(_("  -f, --file=FILENAME      output file name (- for stdout)\n"));
  printf(_("  -F, --format=c|d|t       backup file format (should be automatic)\n"));
  printf(_("  -l, --list               print summarized TOC of the archive\n"));
  printf(_("  -v, --verbose            verbose mode\n"));
  printf(_("  -V, --version            output version information, then exit\n"));
  printf(_("  -?, --help               show this help, then exit\n"));

  printf(_("\nOptions controlling the restore:\n"));
  printf(_("  -a, --data-only              restore only the data, no schema\n"));
  printf(_("  -c, --clean                  clean (drop) database objects before recreating\n"));
  printf(_("  -C, --create                 create the target database\n"));
  printf(_("  -e, --exit-on-error          exit on error, default is to continue\n"));
  printf(_("  -I, --index=NAME             restore named index\n"));
  printf(_("  -j, --jobs=NUM               use this many parallel jobs to restore\n"));
  printf(_("  -L, --use-list=FILENAME      use table of contents from this file for\n"
           "                               selecting/ordering output\n"));
  printf(_("  -n, --schema=NAME            restore only objects in this schema\n"));
  printf(_("  -N, --exclude-schema=NAME    do not restore objects in this schema\n"));
  printf(_("  -O, --no-owner               skip restoration of object ownership\n"));
  printf(_("  -P, --function=NAME(args)    restore named function\n"));
  printf(_("  -s, --schema-only            restore only the schema, no data\n"));
  printf(_("  -S, --superuser=NAME         superuser user name to use for disabling triggers\n"));
  printf(_("  -t, --table=NAME             restore named relation (table, view, etc.)\n"));
  printf(_("  -T, --trigger=NAME           restore named trigger\n"));
  printf(_("  -x, --no-privileges          skip restoration of access privileges (grant/revoke)\n"));
  printf(_("  -1, --single-transaction     restore as a single transaction\n"));
  printf(_("  --disable-triggers           disable triggers during data-only restore\n"));
  printf(_("  --enable-row-security        enable row security\n"));
  printf(_("  --if-exists                  use IF EXISTS when dropping objects\n"));
  printf(_("  --no-comments                do not restore comments\n"));
  printf(_("  --no-data-for-failed-tables  do not restore data of tables that could not be\n"
           "                               created\n"));
  printf(_("  --no-publications            do not restore publications\n"));
  printf(_("  --no-security-labels         do not restore security labels\n"));
  printf(_("  --no-subscriptions           do not restore subscriptions\n"));
  printf(_("  --no-tablespaces             do not restore tablespace assignments\n"));
  printf(_("  --section=SECTION            restore named section (pre-data, data, or post-data)\n"));
  printf(_("  --strict-names               require table and/or schema include patterns to\n"
           "                               match at least one entity each\n"));
  printf(_("  --use-set-session-authorization\n"
           "                               use SET SESSION AUTHORIZATION commands instead of\n"
           "                               ALTER OWNER commands to set ownership\n"));

  printf(_("\nConnection options:\n"));
  printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
  printf(_("  -p, --port=PORT          database server port number\n"));
  printf(_("  -U, --username=NAME      connect as specified database user\n"));
  printf(_("  -w, --no-password        never prompt for password\n"));
  printf(_("  -W, --password           force password prompt (should happen automatically)\n"));
  printf(_("  --role=ROLENAME          do SET ROLE before restore\n"));

  printf(_("\n"
           "The options -I, -n, -N, -P, -t, -T, and --section can be combined and specified\n"
           "multiple times to select multiple objects.\n"));
  printf(_("\nIf no input file name is supplied, then standard input is used.\n\n"));
  printf(_("Report bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}
